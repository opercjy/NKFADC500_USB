#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>

class ReadDataWorker {
public:
    // 💡 [패치] Lock-Free 파이프라인 전역 객체 사용으로 인해, 풀/큐 포인터는 무시(void*)됩니다.
    ReadDataWorker(int sid, void* dummy1, void* dummy2, 
                   const std::string& out_file, int record_length, 
                   int preset_events, int preset_time);
    ~ReadDataWorker();

    void Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(std::memory_order_acquire); }
    int GetTotalAcquiredEvents() const { return total_events_.load(std::memory_order_relaxed); }
    size_t GetTotalAcquiredBytes() const { return total_bytes_.load(std::memory_order_relaxed); }

private:
    void ReadLoop();

    int sid_;
    std::string out_file_;
    int record_length_;
    int preset_events_;
    int preset_time_;

    std::atomic<bool> is_running_;
    std::thread worker_thread_;
    
    std::atomic<int> total_events_;
    std::atomic<size_t> total_bytes_;

    std::vector<uint8_t> residual_buffer_;
};
