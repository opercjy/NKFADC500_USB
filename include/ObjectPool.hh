#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>

// ---------------------------------------------------------
// 하드웨어 아키텍처별 초경량 백오프 힌트 매크로
// 커널 개입(Context Switch) 없이 CPU 파이프라인만 일시정지
// ---------------------------------------------------------
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
constexpr std::size_t BULK_READ_SIZE = 1048576; // 1MB USB Bulk
constexpr std::size_t MAX_MONITOR_EVENTS = 2000;

// ---------------------------------------------------------
// 1. Raw USB Bulk 데이터 구조체 (Zero-Copy DMA용)
// ---------------------------------------------------------
struct alignas(CACHE_LINE_SIZE) DataBlock {
    alignas(CACHE_LINE_SIZE) std::atomic<int> ref_count{0};
    std::size_t valid_size{0};
    uint8_t data[BULK_READ_SIZE];

    inline void Retain(int count) {
        ref_count.store(count, std::memory_order_relaxed);
    }

    inline bool Release() {
        return ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }
};

// ---------------------------------------------------------
// 2. 파싱된 물리 이벤트 구조체 (Root / Zmq 전송용)
// ---------------------------------------------------------
struct alignas(CACHE_LINE_SIZE) EventBlock {
    alignas(CACHE_LINE_SIZE) std::atomic<int> ref_count{0};
    
    // ZMQ 텔레메트리 및 ROOT 변환용 메타데이터
    uint32_t num_events{0};           
    uint32_t samples_per_ch{0};       
    uint32_t total_acquired_events{0};
    uint32_t queue_size{0};           
    uint32_t pool_free_size{0};       
    
    // 하드웨어 한계치 정적 배열 (동적할당 철폐)
    double last_waveform[4][4096]; 
    double charge_array[4][MAX_MONITOR_EVENTS];

    inline void Retain(int count) {
        ref_count.store(count, std::memory_order_relaxed);
    }

    inline bool Release() {
        return ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    inline void Clear() {
        num_events = 0;
        samples_per_ch = 0;
    }
};

// ---------------------------------------------------------
// 3. 중앙 집중형 Lock-Free 메모리 파이프라인 관리자
// ---------------------------------------------------------
class LockFreePipeline {
public:
    static constexpr std::size_t BULK_POOL_SIZE = 256;  // 1MB * 256 = 256MB RAM 예약
    static constexpr std::size_t EVENT_POOL_SIZE = 1024; // 파싱 이벤트용 메모리 예약

    LockFreePipeline() : bulk_pool_(BULK_POOL_SIZE), event_pool_(EVENT_POOL_SIZE) {
        // [A Priori Provisioning] 기동 시 정적 할당된 포인터들을 Free 큐에 모두 삽입
        for (std::size_t i = 0; i < BULK_POOL_SIZE; ++i) {
            bulk_free_queue_.bounded_push(&bulk_pool_[i]);
        }
        for (std::size_t i = 0; i < EVENT_POOL_SIZE; ++i) {
            event_free_queue_.bounded_push(&event_pool_[i]);
        }
    }

    LockFreePipeline(const LockFreePipeline&) = delete;
    LockFreePipeline& operator=(const LockFreePipeline&) = delete;

    // ================= [ Bulk Block API (USB Read 용) ] =================
    inline DataBlock* AcquireFreeBulk() {
        DataBlock* block = nullptr;
        bulk_free_queue_.pop(block);
        return block;
    }

    inline void ReturnToFreeBulk(DataBlock* block) {
        if (block && block->Release()) {
            block->valid_size = 0;
            while (!bulk_free_queue_.bounded_push(block)) { CPU_RELAX(); }
        }
    }

    // ================= [ Event Block API (ROOT / ZMQ 용) ] =================
    inline EventBlock* AcquireFreeEvent() {
        EventBlock* ev = nullptr;
        event_free_queue_.pop(ev);
        return ev;
    }

    inline void ReturnToFreeEvent(EventBlock* ev) {
        if (ev && ev->Release()) {
            ev->Clear();
            while (!event_free_queue_.bounded_push(ev)) { CPU_RELAX(); }
        }
    }

    // [ SPSC Queues - 생산자 Push ]
    inline void PushToRoot(EventBlock* ev) {
        while (!root_queue_.push(ev)) { CPU_RELAX(); }
    }

    inline void PushToZmq(EventBlock* ev) {
        // ZMQ는 최신 상태 모니터링이 목적이므로, 큐가 꽉 찼다면 Spin-wait 하지 않고 버림 (Backpressure 방지)
        if (!zmq_queue_.push(ev)) {
            // Push 실패 시 자신이 책임지고 참조 카운트 감소
            ReturnToFreeEvent(ev); 
        }
    }

    // [ SPSC Queues - 소비자 Pop ]
    inline EventBlock* AcquireForRoot() {
        EventBlock* ev = nullptr;
        root_queue_.pop(ev);
        return ev;
    }

    inline EventBlock* AcquireForZmq() {
        EventBlock* ev = nullptr;
        zmq_queue_.pop(ev);
        return ev;
    }

    // 모니터링용 유틸리티
    inline std::size_t GetEventFreeSize() const {
        // Lock-free queue의 size() 추정치 (정확성보다는 모니터링 목적)
        return EVENT_POOL_SIZE - root_queue_.read_available() - zmq_queue_.read_available();
    }
    
    inline std::size_t GetRootQueueSize() const {
        return root_queue_.read_available();
    }

private:
    // 실제 물리 메모리를 점유하는 컨테이너 (정적 할당)
    std::vector<DataBlock> bulk_pool_;
    std::vector<EventBlock> event_pool_;

    // MPMC (다중 생산/소비 가능 - Free 반환용)
    boost::lockfree::queue<DataBlock*, boost::lockfree::capacity<BULK_POOL_SIZE>> bulk_free_queue_;
    boost::lockfree::queue<EventBlock*, boost::lockfree::capacity<EVENT_POOL_SIZE>> event_free_queue_;

    // SPSC (단일 생산/단일 소비 - 극한의 파이프라인 성능용)
    boost::lockfree::spsc_queue<EventBlock*, boost::lockfree::capacity<EVENT_POOL_SIZE>> root_queue_;
    boost::lockfree::spsc_queue<EventBlock*, boost::lockfree::capacity<EVENT_POOL_SIZE>> zmq_queue_;
};

// 글로벌 파이프라인 인스턴스 (main.cc에서 정의 예정)
extern LockFreePipeline g_pipeline;
