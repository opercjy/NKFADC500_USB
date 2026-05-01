#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>
#include "ObjectPool.hh"

class ReadDataWorker {
public:
    ReadDataWorker(int sid, ObjectPool* pool, DataQueue* queue, 
                   const std::string& out_file, int record_length, 
                   int preset_events, int preset_time);
    ~ReadDataWorker();

    void Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }
    int GetTotalAcquiredEvents() const { return total_events_.load(); } // 💡 Atomic Read
    size_t GetTotalAcquiredBytes() const { return total_bytes_.load(); }

private:
    void ReadLoop();

    int sid_;
    ObjectPool* mem_pool_;
    DataQueue* data_queue_;
    std::string out_file_;
    int record_length_;
    int preset_events_;
    int preset_time_;

    std::atomic<bool> is_running_;
    std::thread worker_thread_;
    
    // 💡 [버그 수정] 메인 스레드에서 Rate 계산 시 값이 멈추는 현상 방지
    std::atomic<int> total_events_;
    std::atomic<size_t> total_bytes_;

    std::vector<uint8_t> residual_buffer_;
};