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
      record_length_(record_length), preset_events_(preset_events), preset_time_(preset_time), total_acquired_events_(0) 
{}

ReadDataWorker::~ReadDataWorker() { Stop(); }

void ReadDataWorker::Start() {
    is_running_ = true;
    write_thread_ = std::thread(&ReadDataWorker::FileWriterLoop, this);
    acq_thread_ = std::thread(&ReadDataWorker::AcquisitionLoop, this);
    std::cout << "[DAQ:INFO] Acquisition & Writer Threads Started.\n";
}

void ReadDataWorker::Stop() {
    is_running_ = false;
    if (pool_) pool_->WakeUpAll(); 
    if (queue_) queue_->WakeUpAll();
    
    if (acq_thread_.joinable()) acq_thread_.join();
    if (write_thread_.joinable()) write_thread_.join();
    std::cout << "[DAQ:INFO] Worker Threads Safely Terminated.\n";
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
            
            // 💡 [UX 패치] \r(캐리지 리턴)과 \033[K(줄 끝까지 지우기)를 조합하여 터미널 한 줄에 덮어쓰기!
            printf("\r\033[K\033[1;36m[MONITOR]\033[0m \033[1;33mTime: %5.1fs\033[0m | \033[1;32mEvents: %-8d\033[0m | \033[1;35mRate: %-8.1f Hz\033[0m | \033[1;34mQ: %-3zu\033[0m | \033[1;31mPool: %-3zu\033[0m", 
                   elapsed_sec, current_events, rate, queue_->Size(), pool_->FreeSize());
            fflush(stdout);
            
            last_events = current_events; last_print_time = now;
        }

        int bcount_kb = KFADC500read_BCOUNT(sid_);
        if (bcount_kb <= 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue; 
        }

        int max_kb = BULK_READ_SIZE / 1024;
        int read_kb = (bcount_kb > max_kb) ? max_kb : bcount_kb;

        DataBlock* block = pool_->Acquire(is_running_);
        if (!block) continue;

        KFADC500read_DATA(sid_, read_kb, reinterpret_cast<char*>(block->data));

        block->valid_size = read_kb * 1024;
        queue_->Push(block); 

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