#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstring>

constexpr size_t BULK_READ_SIZE = 1048576; // 1MB 
constexpr size_t EVENT_ALIGNMENT = 512;    // 물리 이벤트 바이트 (예시)

// 💡 Zero-Malloc을 위한 데이터 블록 단위
struct DataBlock {
    uint8_t data[BULK_READ_SIZE];
    size_t valid_size;
};

// 💡 Thread-safe 큐 및 풀 관리자
class ThreadSafeQueue {
public:
    void Push(DataBlock* block) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(block);
        cond_.notify_one();
    }

    DataBlock* Pop(bool& is_running) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this, &is_running] { return !queue_.empty() || !is_running; });
        if (!is_running && queue_.empty()) return nullptr;
        
        DataBlock* block = queue_.front();
        queue_.pop();
        return block;
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<DataBlock*> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};