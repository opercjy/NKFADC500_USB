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
    
    // 메인 스레드에서 실행 상태를 확인하기 위함
    bool IsRunning() const { return is_running_.load(); }
    
    // 💡 [신규 추가] 획득한 총 이벤트 수 반환 (종료 서머리용)
    int GetTotalAcquiredEvents() const { return total_acquired_events_.load(); }

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

    std::thread acq_thread_;
    std::thread write_thread_;
};