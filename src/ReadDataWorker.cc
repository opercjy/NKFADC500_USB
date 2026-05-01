#include "ReadDataWorker.hh"
#include "RootProducer.hh"
#include <iostream>
#include <fstream>
#include <chrono>

extern "C" {
#include "NoticeKFADC500USB.h"
}

ReadDataWorker::ReadDataWorker(int sid, ObjectPool* pool, DataQueue* queue, 
                               const std::string& out_file, int record_length, 
                               int preset_events, int preset_time)
    : sid_(sid), mem_pool_(pool), data_queue_(queue), out_file_(out_file),
      record_length_(record_length), preset_events_(preset_events), preset_time_(preset_time),
      is_running_(false), total_events_(0), total_bytes_(0) {}

ReadDataWorker::~ReadDataWorker() { Stop(); }

void ReadDataWorker::Start() {
    if (is_running_) return;
    is_running_ = true;
    total_events_ = 0;
    total_bytes_ = 0;
    residual_buffer_.clear(); 
    worker_thread_ = std::thread(&ReadDataWorker::ReadLoop, this);
}

void ReadDataWorker::Stop() {
    is_running_ = false;
    if (worker_thread_.joinable()) worker_thread_.join();
}

void ReadDataWorker::ReadLoop() {
    std::ofstream fout(out_file_, std::ios::binary | std::ios::app);
    if (!fout.is_open()) {
        std::cerr << "\033[1;31m[DAQ:ERROR] Cannot open output file: " << out_file_ << "\033[0m\n";
        is_running_ = false;
        return;
    }

    fout.write(reinterpret_cast<const char*>(&record_length_), sizeof(int));
    fout.write(reinterpret_cast<const char*>(&preset_events_), sizeof(int));
    total_bytes_ += 8;

    RootProducer monitor_core("dummy", "dummy", false, true);
    monitor_core.ClearPacket();

    auto start_time = std::chrono::steady_clock::now();
    auto last_zmq_time = start_time;
    
    int event_size = record_length_ * 512; 
    int samples_per_ch = (event_size - 32) / 8; 
    
    int read_kbytes = 16; 
    int read_bytes = read_kbytes * 1024; 

    residual_buffer_.reserve(read_bytes * 2); 

    while (is_running_) {
        if (preset_events_ > 0 && total_events_ >= preset_events_) break;
        if (preset_time_ > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= preset_time_) break;
        }

        int bcount = KFADC500read_BCOUNT(sid_);
        if (bcount >= read_kbytes) {
            DataBlock* block = mem_pool_->Acquire(is_running_);
            if (!block) break;

            KFADC500read_DATA(sid_, read_kbytes, reinterpret_cast<char*>(block->data));
            block->valid_size = read_bytes;

            fout.write(reinterpret_cast<const char*>(block->data), block->valid_size);
            total_bytes_ += block->valid_size;
            
            residual_buffer_.insert(residual_buffer_.end(), block->data, block->data + block->valid_size);
            
            size_t offset = 0;
            int num_events_processed = 0;
            
            while (offset + event_size <= residual_buffer_.size()) {
                const uint16_t* evt_data = reinterpret_cast<const uint16_t*>(residual_buffer_.data() + offset);
                
                monitor_core.ProcessOnlineEvent(evt_data, samples_per_ch);
                offset += event_size;
                num_events_processed++;
                total_events_++;

                if (monitor_core.GetOnlineEventCount() >= 2000) {
                    DataBlock* monitor_block = mem_pool_->Acquire(is_running_);
                    if (monitor_block) {
                        auto* pkt = reinterpret_cast<LiveMonitorPacket*>(monitor_block->data);
                        monitor_core.GetLatestPacket(*pkt);
                        
                        // 💡 [텔레메트리 기록] 상태값을 ZMQ 헤더에 담습니다.
                        pkt->total_acquired_events = total_events_.load();
                        pkt->queue_size = data_queue_->Size();
                        pkt->pool_free_size = mem_pool_->FreeSize();

                        monitor_block->valid_size = sizeof(LiveMonitorPacket);
                        data_queue_->Push(monitor_block);
                    }
                    monitor_core.ClearPacket();
                    last_zmq_time = std::chrono::steady_clock::now();
                }
            }

            if (offset > 0) {
                residual_buffer_.erase(residual_buffer_.begin(), residual_buffer_.begin() + offset);
            }

            mem_pool_->Release(block);

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_zmq_time).count() >= 100) {
                if (monitor_core.GetOnlineEventCount() > 0) {
                    DataBlock* monitor_block = mem_pool_->Acquire(is_running_);
                    if (monitor_block) {
                        auto* pkt = reinterpret_cast<LiveMonitorPacket*>(monitor_block->data);
                        monitor_core.GetLatestPacket(*pkt);
                        
                        pkt->total_acquired_events = total_events_.load();
                        pkt->queue_size = data_queue_->Size();
                        pkt->pool_free_size = mem_pool_->FreeSize();

                        monitor_block->valid_size = sizeof(LiveMonitorPacket);
                        data_queue_->Push(monitor_block);
                    }
                    monitor_core.ClearPacket();
                }
                last_zmq_time = now;
            }

        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    fout.close();
    is_running_ = false; // 💡 [핵심 버그 수정] 설정된 시간이 지나 스레드 종료 시, main의 무한루프를 풀어줌!
}