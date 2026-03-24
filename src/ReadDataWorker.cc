#include "ReadDataWorker.hh"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <thread>

extern "C" {
#include "NoticeKFADC500USB.h"
}

ReadDataWorker::ReadDataWorker(int sid, ObjectPool* pool, DataQueue* queue, 
                               const std::string& out_filename, int record_length, int preset_events, int preset_time)
    : sid_(sid), outfile_(out_filename), is_running_(false), pool_(pool), queue_(queue), 
      record_length_(record_length), preset_events_(preset_events), preset_time_(preset_time), 
      total_acquired_events_(0), total_acquired_bytes_(0) 
{}

ReadDataWorker::~ReadDataWorker() { Stop(); }

void ReadDataWorker::Start() {
    is_running_ = true;
    write_thread_ = std::thread(&ReadDataWorker::FileWriterLoop, this);
    acq_thread_ = std::thread(&ReadDataWorker::AcquisitionLoop, this);
    std::cout << "[DAQ:INFO] Acquisition & Writer Threads Started.\n";
}

void ReadDataWorker::Stop() {
    if (!is_running_) return; 
    is_running_ = false; // 루프 중단 시그널 발생
    
    if (pool_) pool_->WakeUpAll(); 
    if (queue_) queue_->WakeUpAll();
    
    bool thread_joined = false;
    if (acq_thread_.joinable()) { acq_thread_.join(); thread_joined = true; }
    if (write_thread_.joinable()) { write_thread_.join(); thread_joined = true; }
    
    if (thread_joined) {
        std::cout << "[DAQ:INFO] Worker Threads Safely Terminated.\n";
    }
}

void ReadDataWorker::AcquisitionLoop() {
    auto start_time = std::chrono::steady_clock::now();
    auto last_print_time = start_time;
    int last_events = 0;
    int event_size = record_length_ * 512;

    std::cout << "\n\033[1;32m[SYSTEM] DAQ Pipeline Activated. Waiting for Hardware Trigger...\033[0m\n\n";

    while (is_running_) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
        
        if (preset_time_ > 0 && elapsed_sec >= preset_time_) {
            std::cout << "\n\033[1;33m[DAQ:INFO] Preset Time Limit Reached (" << preset_time_ << "s).\033[0m\n";
            is_running_ = false; break;
        }

        double print_dt = std::chrono::duration<double>(now - last_print_time).count();
        if (print_dt >= 0.5) {
            int current_events = total_acquired_events_.load();
            double rate = (current_events - last_events) / print_dt;
            
            printf("\r\033[K\033[1;36m[MONITOR]\033[0m \033[1;33m%.1fs\033[0m | \033[1;32mEvt: %d\033[0m | \033[1;35m%.1f Hz\033[0m | \033[1;34mQ: %zu\033[0m | \033[1;31mPool: %zu\033[0m", 
                   elapsed_sec, current_events, rate, queue_->Size(), pool_->FreeSize());
            fflush(stdout);
            
            last_events = current_events; last_print_time = now;
        }

        if (!is_running_) break; // 하드웨어 읽기 전 방어

        int bcount_kb = KFADC500read_BCOUNT(sid_);
        if (bcount_kb <= 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue; 
        }

        if (!is_running_) break; // 블록 획득 전 방어

        int max_kb = BULK_READ_SIZE / 1024;
        int read_kb = (bcount_kb > max_kb) ? max_kb : bcount_kb;

        DataBlock* block = pool_->Acquire(is_running_);
        if (!block) continue;

        if (!is_running_) { // 진짜 데이터 읽기 직전 최후 방어
            pool_->Release(block);
            break;
        }

        KFADC500read_DATA(sid_, read_kb, reinterpret_cast<char*>(block->data));

        block->valid_size = read_kb * 1024;
        queue_->Push(block); 

        total_acquired_bytes_ += block->valid_size;

        if (event_size > 0) {
            total_acquired_events_ += (block->valid_size / event_size);
        }
        
        if (preset_events_ > 0 && total_acquired_events_ >= preset_events_) {
            std::cout << "\n\033[1;33m[DAQ:INFO] Preset Event Limit Reached (" << preset_events_ << ").\033[0m\n";
            is_running_ = false;
        }
    }
}

void ReadDataWorker::FileWriterLoop() {
    std::ofstream fout(outfile_, std::ios::out | std::ios::binary);
    if (!fout.is_open()) return;

    fout.write(reinterpret_cast<const char*>(&record_length_), 4);
    fout.write(reinterpret_cast<const char*>(&preset_events_), 4);

    size_t total_written = 8;
    while (is_running_ || queue_->Size() > 0) {
        DataBlock* block = queue_->Pop(is_running_);
        if (block && block->valid_size > 0) {
            fout.write(reinterpret_cast<char*>(block->data), block->valid_size);
            total_written += block->valid_size;
            pool_->Release(block); 
        }
    }
    fout.close();
}