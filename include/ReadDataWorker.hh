#pragma once
#include <string>
#include <atomic>
#include <thread>
#include "ObjectPool.hh"

class ReadDataWorker {
public:
    ReadDataWorker(int sid, ObjectPool* pool, DataQueue* queue, 
                   const std::string& out_filename, int record_length, int preset_events, int preset_time);
    ~ReadDataWorker();

    void Start();
    void Stop();
    
    bool IsRunning() const { return is_running_.load(); }
    int GetTotalAcquiredEvents() const { return total_acquired_events_.load(); }
    
    // 신규 추가: 획득한 총 데이터 크기(바이트) 반환
    size_t GetTotalAcquiredBytes() const { return total_acquired_bytes_.load(); }

private:
    void AcquisitionLoop();
    void FileWriterLoop();

    int sid_;
    std::string outfile_;
    std::atomic<bool> is_running_;
    
    ObjectPool* pool_;
    DataQueue* queue_;
    
    int record_length_;
    int preset_events_;
    int preset_time_;
    
    std::atomic<int> total_acquired_events_;
    std::atomic<size_t> total_acquired_bytes_; // 신규 추가

    std::thread acq_thread_;
    std::thread write_thread_;
};