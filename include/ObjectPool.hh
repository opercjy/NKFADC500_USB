#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>

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

#pragma pack(push, 1) // 패딩 바이트 강제 제거
struct LiveMonitorPacket {
    uint32_t num_events;           
    uint32_t samples_per_ch;       
    uint32_t total_acquired_events;
    uint32_t queue_size;           
    uint32_t pool_free_size;       
    uint32_t padding;              
    double last_waveform[4][4096]; 
    double charge_array[4][MAX_MONITOR_EVENTS]; 
};
#pragma pack(pop)

// =========================================================
// 1. Raw USB Bulk 데이터 구조체 (수정됨)
// =========================================================
struct alignas(CACHE_LINE_SIZE) DataBlock {
    // 💡 [버그 수정] Bulk 데이터는 ReadDataWorker 단독 사용이므로 참조 카운팅 불필요
    std::size_t valid_size{0};
    uint8_t data[BULK_READ_SIZE];
};

// =========================================================
// 2. 모니터링 래퍼 구조체 
// =========================================================
struct alignas(CACHE_LINE_SIZE) EventBlock {
    alignas(CACHE_LINE_SIZE) std::atomic<int> ref_count{0};
    LiveMonitorPacket payload;

    inline void Retain(int count) { ref_count.store(count, std::memory_order_relaxed); }
    inline bool Release() { return ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1; }
    inline void Clear() { payload.num_events = 0; }
};

// =========================================================
// 3. Lock-Free Pipeline
// =========================================================
class LockFreePipeline {
public:
    static constexpr std::size_t BULK_POOL_SIZE = 256;  
    static constexpr std::size_t EVENT_POOL_SIZE = 1024; 

    LockFreePipeline() : bulk_pool_(BULK_POOL_SIZE), event_pool_(EVENT_POOL_SIZE) {
        for (std::size_t i = 0; i < BULK_POOL_SIZE; ++i) bulk_free_queue_.bounded_push(&bulk_pool_[i]);
        for (std::size_t i = 0; i < EVENT_POOL_SIZE; ++i) event_free_queue_.bounded_push(&event_pool_[i]);
    }

    // 💡 [버그 수정] 조건 검사 없이 valid_size 초기화 후 즉각 반환 보장
    inline DataBlock* AcquireFreeBulk() {
        DataBlock* block = nullptr; bulk_free_queue_.pop(block); return block;
    }
    inline void ReturnToFreeBulk(DataBlock* block) {
        if (block) { 
            block->valid_size = 0; 
            while (!bulk_free_queue_.bounded_push(block)) CPU_RELAX(); 
        }
    }

    inline EventBlock* AcquireFreeEvent() {
        EventBlock* ev = nullptr; event_free_queue_.pop(ev); return ev;
    }
    inline void ReturnToFreeEvent(EventBlock* ev) {
        if (ev && ev->Release()) { ev->Clear(); while (!event_free_queue_.bounded_push(ev)) CPU_RELAX(); }
    }

    inline void PushToZmq(EventBlock* ev) {
        if (!zmq_queue_.push(ev)) ReturnToFreeEvent(ev); 
    }
    inline EventBlock* AcquireForZmq() {
        EventBlock* ev = nullptr; zmq_queue_.pop(ev); return ev;
    }

    inline std::size_t GetEventFreeSize() const { return EVENT_POOL_SIZE - zmq_queue_.read_available(); }
    inline std::size_t GetZmqQueueSize() const { return zmq_queue_.read_available(); }

private:
    std::vector<DataBlock> bulk_pool_;
    std::vector<EventBlock> event_pool_;
    boost::lockfree::queue<DataBlock*, boost::lockfree::capacity<BULK_POOL_SIZE>> bulk_free_queue_;
    boost::lockfree::queue<EventBlock*, boost::lockfree::capacity<EVENT_POOL_SIZE>> event_free_queue_;
    boost::lockfree::spsc_queue<EventBlock*, boost::lockfree::capacity<EVENT_POOL_SIZE>> zmq_queue_;
};

extern LockFreePipeline g_pipeline;
