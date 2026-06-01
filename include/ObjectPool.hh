#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>

// 플랫폼 맞춤형 하드웨어 레벨 경량 백오프 (OS 개입 없는 스핀 대기)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    #include <immintrin.h>
    #define CPU_RELAX() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define CPU_RELAX() __asm__ volatile("yield" ::: "memory")
#else
    #include <thread>
    #define CPU_RELAX() std::this_thread::yield()
#endif

constexpr std::size_t CACHE_LINE_SIZE = 64;

// 1. 이벤트 구조체 (False Sharing 원천 차단)
struct alignas(CACHE_LINE_SIZE) Event {
    // 라이프사이클 관리용 참조 카운터.
    // ZMQ 컨슈머가 ref_count를 깎을 때, ROOT 컨슈머가 payload를 읽는 캐시가 무효화되지 않도록 독자적 캐시 라인에 격리.
    alignas(CACHE_LINE_SIZE) std::atomic<int> ref_count{0};
    
    // 메타데이터
    uint64_t timestamp{0};
    uint32_t trigger_number{0};
    std::size_t data_size{0};
    
    // 동적 할당을 없애기 위한 하드웨어 최대 지원 샘플 사이즈의 정적 배열
    static constexpr std::size_t MAX_SAMPLES = 4096;
    alignas(CACHE_LINE_SIZE) uint16_t adc_data[MAX_SAMPLES];

    inline void Retain(int count) {
        ref_count.store(count, std::memory_order_relaxed);
    }

    // 자신이 마지막 소유자라면 true 반환
    inline bool Release() {
        // memory_order_acq_rel를 통해 다른 코어의 메모리 읽기/쓰기 가시성을 완벽 보장
        return ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }
};

class LockFreePipeline {
public:
    static constexpr std::size_t POOL_SIZE = 8192; // 2의 거듭제곱(Lock-free 비트마스킹 최적화)

    LockFreePipeline() : pool_(POOL_SIZE) {
        // [Zero-Allocation] 기동 시 1회 정적 할당된 주소들을 Free 큐에 삽입
        for (std::size_t i = 0; i < POOL_SIZE; ++i) {
            free_queue_.bounded_push(&pool_[i]);
        }
    }

    LockFreePipeline(const LockFreePipeline&) = delete;
    LockFreePipeline& operator=(const LockFreePipeline&) = delete;

    // ================= [ Producer API ] =================
    inline Event* AcquireFree() {
        Event* ev = nullptr;
        free_queue_.pop(ev); // 대기열 고갈 시 즉각 nullptr 반환 (Wait-free)
        return ev; 
    }

    inline void PushToRoot(Event* ev) {
        // [Backpressure] 소비자 큐가 꽉 찼으면 데드락 방지 및 컨텍스트 스위칭 없는 Spin-wait
        while (!root_queue_.push(ev)) { CPU_RELAX(); }
    }

    inline void PushToZmq(Event* ev) {
        while (!zmq_queue_.push(ev)) { CPU_RELAX(); }
    }

    // ================= [ Consumer API ] =================
    inline Event* AcquireForRoot() {
        Event* ev = nullptr;
        root_queue_.pop(ev);
        return ev;
    }

    inline Event* AcquireForZmq() {
        Event* ev = nullptr;
        zmq_queue_.pop(ev);
        return ev;
    }

    inline void ReturnToFree(Event* ev) {
        if (ev->Release()) {
            // 소유권이 완전히 소멸된 마지막 컨슈머가 Free 큐로 환원
            while (!free_queue_.bounded_push(ev)) { CPU_RELAX(); }
        }
    }

private:
    std::vector<Event> pool_;

    // [MPMC Queue] 다수 소비자(Root, Zmq)가 빈 버퍼 반환, 1 생산자 획득
    // bounded_push 사용 시 내부 노드 할당 없음
    boost::lockfree::queue<Event*, boost::lockfree::capacity<POOL_SIZE>> free_queue_;

    // [SPSC Queue] 단일 생산자 -> 단일 소비자 파이프라인 (극한의 Lock-Free 성능)
    boost::lockfree::spsc_queue<Event*, boost::lockfree::capacity<POOL_SIZE>> root_queue_;
    boost::lockfree::spsc_queue<Event*, boost::lockfree::capacity<POOL_SIZE>> zmq_queue_;
};

// Global Pipeline Instance 선언
extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;
