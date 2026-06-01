#include "ReadDataWorker.hh"
#include "ObjectPool.hh"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>

extern "C" {
#include "NoticeKFADC500USB.h"
}

extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;
extern std::atomic<bool> g_app_running; 

ReadDataWorker::ReadDataWorker(int sid, void*, void*, 
                               const std::string& out_file, int record_length, 
                               int preset_events, int preset_time)
    : sid_(sid), out_file_(out_file), record_length_(record_length), 
      preset_events_(preset_events), preset_time_(preset_time),
      is_running_(false), total_events_(0), total_bytes_(0) {}

ReadDataWorker::~ReadDataWorker() { Stop(); }

void ReadDataWorker::Start() {
    if (is_running_.load(std::memory_order_acquire)) return;
    is_running_.store(true, std::memory_order_release);
    total_events_.store(0, std::memory_order_relaxed);
    total_bytes_.store(0, std::memory_order_relaxed);
    residual_buffer_.clear(); 
    worker_thread_ = std::thread(&ReadDataWorker::ReadLoop, this);
}

void ReadDataWorker::Stop() {
    if (worker_thread_.joinable()) worker_thread_.join();
}

void ReadDataWorker::ReadLoop() {
    std::ofstream fout(out_file_, std::ios::binary | std::ios::app);
    if (!fout.is_open()) {
        std::cerr << "\033[1;31m[DAQ:ERROR] Cannot open output file: " << out_file_ << "\033[0m\n";
        is_running_.store(false, std::memory_order_release);
        return;
    }

    fout.write(reinterpret_cast<const char*>(&record_length_), sizeof(int));
    fout.write(reinterpret_cast<const char*>(&preset_events_), sizeof(int));
    total_bytes_.fetch_add(8, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();
    auto last_zmq_time = start_time;
    
    const int event_size = record_length_ * 512; 
    const int samples_per_ch = (event_size - 32) / 8; 
    const int read_kbytes = 16; 
    const int read_bytes = read_kbytes * 1024; 

    residual_buffer_.reserve(read_bytes * 2); 

    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;

    EventBlock* mon_ev = nullptr;
    bool stop_issued = false; 

    while (is_running_.load(std::memory_order_acquire)) {
        
        // 1. 외부 강제 종료 시그널 감지 시 즉시 H/W 래치 잠금
        if (!g_app_running.load(std::memory_order_acquire) && !stop_issued) {
            std::cout << "\n\033[1;33m[DAQ:INFO] Stop signal received. Halting hardware trigger safely...\033[0m\n";
            KFADC500stop(sid_);
            stop_issued = true;
        }

        // 2. 프리셋 목표 달성 검증
        if (!stop_issued) {
            if (preset_events_ > 0 && total_events_.load(std::memory_order_relaxed) >= preset_events_) {
                KFADC500stop(sid_);
                stop_issued = true;
            } else if (preset_time_ > 0) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= preset_time_) {
                    KFADC500stop(sid_);
                    stop_issued = true;
                }
            }
        }

        int bcount = KFADC500read_BCOUNT(sid_);
        
        // 하드웨어에 16KB 이상의 데이터가 찼을 때만 Bulk Read 수행
        if (bcount >= read_kbytes) {
            DataBlock* bulk = g_pipeline.AcquireFreeBulk();
            if (bulk) {
                KFADC500read_DATA(sid_, read_kbytes, reinterpret_cast<char*>(bulk->data));
                bulk->valid_size = read_bytes;
                fout.write(reinterpret_cast<const char*>(bulk->data), bulk->valid_size);
                total_bytes_.fetch_add(bulk->valid_size, std::memory_order_relaxed);
                
                residual_buffer_.insert(residual_buffer_.end(), bulk->data, bulk->data + bulk->valid_size);
                size_t offset = 0;
                
                while (offset + event_size <= residual_buffer_.size()) {
                    if (!mon_ev) {
                        mon_ev = g_pipeline.AcquireFreeEvent();
                        if (mon_ev) std::memset(&mon_ev->payload, 0, sizeof(LiveMonitorPacket));
                    }

                    if (mon_ev && mon_ev->payload.num_events < MAX_MONITOR_EVENTS) {
                        const uint8_t* evt_bytes = residual_buffer_.data() + offset;
                        mon_ev->payload.samples_per_ch = samples_per_ch;
                        int idx = mon_ev->payload.num_events;

                        for (int ch = 0; ch < 4; ++ch) {
                            double ped = 0.0;
                            int ped_start = std::min(PED_START, samples_per_ch);
                            int ped_end = std::min(PED_END, samples_per_ch);
                            int num_ped = ped_end - ped_start;
                            
                            for (int i = ped_start; i < ped_end; ++i) {
                                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + (i * 8) + (ch * 2));
                                ped += adc;
                            }
                            if (num_ped > 0) ped /= num_ped;

                            double ch_charge = 0;
                            for (int i = SKIP_BINS; i < samples_per_ch; ++i) {
                                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + (i * 8) + (ch * 2));
                                double inverted_adc = ped - adc;
                                ch_charge += inverted_adc;
                                if (i < 4096) mon_ev->payload.last_waveform[ch][i] = inverted_adc;
                            }
                            for (int i = samples_per_ch; i < 4096; ++i) mon_ev->payload.last_waveform[ch][i] = 0.0;
                            mon_ev->payload.charge_array[ch][idx] = ch_charge;
                        }
                        mon_ev->payload.num_events++;
                        total_events_.fetch_add(1, std::memory_order_relaxed);
                    }
                    offset += event_size;
                }

                g_pipeline.ReturnToFreeBulk(bulk);
                if (offset > 0) residual_buffer_.erase(residual_buffer_.begin(), residual_buffer_.begin() + offset);
            }
        } else {
            // 정지 명령이 내려졌고 더 이상 읽어올 16KB 블록이 없다면 루프 탈출
            if (stop_issued) {
                std::cout << "[DAQ:INFO] Hardware FIFO drained completely.\n";
                break; 
            }
            CPU_RELAX();
        }

        // ZMQ 모니터링 주기적(100ms) 전송 로직
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_zmq_time).count() >= 100) {
            if (!mon_ev) {
                mon_ev = g_pipeline.AcquireFreeEvent();
                if (mon_ev) std::memset(&mon_ev->payload, 0, sizeof(LiveMonitorPacket));
            }
            if (mon_ev) {
                mon_ev->payload.total_acquired_events = total_events_.load(std::memory_order_relaxed);
                mon_ev->payload.queue_size = g_pipeline.GetZmqQueueSize();
                mon_ev->payload.pool_free_size = g_pipeline.GetEventFreeSize();

                mon_ev->Retain(1);
                g_pipeline.PushToZmq(mon_ev);
                mon_ev = nullptr; 
            }
            last_zmq_time = now;
        }
    }

    if (mon_ev) g_pipeline.ReturnToFreeEvent(mon_ev); 
    fout.close();

    // 💡 [치명적 오타 수정] std::memory_order_release 로 정상화 완료
    is_running_.store(false, std::memory_order_release);
}
