#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstdint>

constexpr size_t BULK_READ_SIZE = 1048576; 
constexpr size_t EVENT_ALIGNMENT = 512;    

class ObjectPool; 

struct DataBlock {
    uint8_t data[BULK_READ_SIZE];
    size_t valid_size;
    ObjectPool* origin_pool; 
};

class ObjectPool {
public:
    ObjectPool(size_t pool_size) {
        for (size_t i = 0; i < pool_size; i++) {
            DataBlock* block = new DataBlock();
            block->origin_pool = this;
            block->valid_size = 0;
            free_queue_.push(block);
            all_blocks_.push_back(block);
        }
    }

    ~ObjectPool() {
        for (auto block : all_blocks_) delete block;
    }

    DataBlock* Acquire(bool& is_running) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this, &is_running] { return !free_queue_.empty() || !is_running; });
        if (!is_running && free_queue_.empty()) return nullptr;
        
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

private:
    std::queue<DataBlock*> free_queue_;
    std::vector<DataBlock*> all_blocks_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class DataQueue {
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