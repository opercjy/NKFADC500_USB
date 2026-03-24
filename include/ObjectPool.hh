#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

// 💡 하드웨어 USB 고속 전송을 위한 1MB 단위 블록 및 정렬 상수
constexpr size_t BULK_READ_SIZE = 1048576; 
constexpr size_t EVENT_ALIGNMENT = 512;    

class ObjectPool; 

// 💡 1MB 데이터를 담는 껍데기 (Zero-Copy 직결용)
struct DataBlock {
    uint8_t data[BULK_READ_SIZE];
    size_t valid_size;
    ObjectPool* origin_pool; 
};

// =========================================================
// 메모리 할당/해제 부하를 없애는 Object Pool
// =========================================================
class ObjectPool {
public:
    ObjectPool(size_t pool_size) {
        for (size_t i = 0; i < pool_size; i++) {
            DataBlock* block = new DataBlock();
            block->origin_pool = this;
            block->valid_size = 0;
            free_queue_.push(block);
            all_blocks_.push_back(block); // 메모리 누수 방지용 추적 벡터
        }
    }

    ~ObjectPool() {
        for (auto block : all_blocks_) delete block;
    }

    DataBlock* Acquire(std::atomic<bool>& is_running) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this, &is_running] { return !free_queue_.empty() || !is_running.load(); });
        if (!is_running.load() && free_queue_.empty()) return nullptr;
        
        DataBlock* block = free_queue_.front();
        free_queue_.pop();
        return block;
    }

    void Release(DataBlock* block) {
        if (!block) return;
        std::lock_guard<std::mutex> lock(mutex_);
        block->valid_size = 0;
        free_queue_.push(block);
        cond_.notify_one();
    }

    void WakeUpAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        cond_.notify_all();
    }

    // 💡 (신규 추가) 터미널 UI 모니터링을 위한 잔여 여유 풀 사이즈 반환
    size_t FreeSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return free_queue_.size();
    }

private:
    std::queue<DataBlock*> free_queue_;
    std::vector<DataBlock*> all_blocks_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

// =========================================================
// 스레드 간 데이터 전달을 위한 안전한 Queue
// =========================================================
class DataQueue {
public:
    void Push(DataBlock* block) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(block);
        cond_.notify_one();
    }

    DataBlock* Pop(std::atomic<bool>& is_running) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this, &is_running] { return !queue_.empty() || !is_running.load(); });
        if (!is_running.load() && queue_.empty()) return nullptr;
        
        DataBlock* block = queue_.front();
        queue_.pop();
        return block;
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void WakeUpAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        cond_.notify_all();
    }

private:
    std::queue<DataBlock*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};