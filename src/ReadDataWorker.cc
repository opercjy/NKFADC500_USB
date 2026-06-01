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
    g_system_running.store(true, std::memory_order_release);
    total_events_.store(0, std::memory_order_relaxed);
    total_bytes_.store(0, std::memory_order_relaxed);
    residual_buffer_.clear(); 
    worker_thread_ = std::thread(&ReadDataWorker::ReadLoop, this);
}

void ReadDataWorker::Stop() {
    is_running_.store(false, std::memory_order_release);
    g_system_running.store(false, std::memory_order_release);
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

    // 모니터링용 활성 이벤트 블록 할당
    EventBlock* mon_ev = g_pipeline.AcquireFreeEvent();
    if (mon_ev) std::memset(&mon_ev->payload, 0, sizeof(LiveMonitorPacket));

    while (is_running_.load(std::memory_order_acquire)) {
        if (preset_events_ > 0 && total_events_.load(std::memory_order_relaxed) >= preset_events_) break;
        if (preset_time_ > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= preset_time_) break;
        }

        int bcount = KFADC500read_BCOUNT(sid_);
        if (bcount >= read_kbytes) {
            DataBlock* bulk = g_pipeline.AcquireFreeBulk();
            if (!bulk) { CPU_RELAX(); continue; }

            // 💡 [핵심] 순수 바이너리를 오직 .dat 에만 기록
            KFADC500read_DATA(sid_, read_kbytes, reinterpret_cast<char*>(bulk->data));
            bulk->valid_size = read_bytes;
            fout.write(reinterpret_cast<const char*>(bulk->data), bulk->valid_size);
            total_bytes_.fetch_add(bulk->valid_size, std::memory_order_relaxed);
            
            residual_buffer_.insert(residual_buffer_.end(), bulk->data, bulk->data + bulk->valid_size);
            size_t offset = 0;
            
            // GUI 모니터링을 위한 백그라운드 데이터 파싱
            while (offset + event_size <= residual_buffer_.size()) {
                const uint8_t* evt_bytes = residual_buffer_.data() + offset;
                
                int current_evt_num = total_events_.fetch_add(1, std::memory_order_relaxed);

                if (mon_ev && mon_ev->payload.num_events < MAX_MONITOR_EVENTS) {
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
                }
                offset += event_size;
            }

            g_pipeline.ReturnToFreeBulk(bulk);
            if (offset > 0) residual_buffer_.erase(residual_buffer_.begin(), residual_buffer_.begin() + offset);

            // 💡 [배치 송출] 데이터가 2000개 모이거나 100ms 가 지나면 한 번에 ZMQ 송출
            auto now = std::chrono::steady_clock::now();
            if (mon_ev && (mon_ev->payload.num_events >= MAX_MONITOR_EVENTS || 
                std::chrono::duration_cast<std::chrono::milliseconds>(now - last_zmq_time).count() >= 100)) {
                
                if (mon_ev->payload.num_events > 0) {
                    mon_ev->payload.total_acquired_events = total_events_.load(std::memory_order_relaxed);
                    mon_ev->payload.queue_size = g_pipeline.GetZmqQueueSize();
                    mon_ev->payload.pool_free_size = g_pipeline.GetEventFreeSize();

                    mon_ev->Retain(1); // 소비자(ZMQ) 1명
                    g_pipeline.PushToZmq(mon_ev);
                }
                
                // 새로운 빈 껍데기 획득
                mon_ev = g_pipeline.AcquireFreeEvent();
                if (mon_ev) std::memset(&mon_ev->payload, 0, sizeof(LiveMonitorPacket));
                last_zmq_time = now;
            }

        } else {
            CPU_RELAX();
        }
    }

    if (mon_ev) g_pipeline.ReturnToFreeEvent(mon_ev); // 종료 시 잔여 껍데기 반환
    fout.close();
    is_running_.store(false, std::memory_order_release);
}
