#include "ReadDataWorker.hh"
#include "RootProducer.hh"
#include <iostream>
#include <fstream>
#include <chrono>

extern "C" {
#include "NoticeKFADC500USB.h"
}

// ==============================================================================
// 글로벌 락프리 파이프라인 및 시스템 상태 변수 인스턴스화
// ==============================================================================
LockFreePipeline g_pipeline;
std::atomic<bool> g_system_running{false};

ReadDataWorker::ReadDataWorker(int sid, ObjectPool* pool, DataQueue* queue, 
                               const std::string& out_file, int record_length, 
                               int preset_events, int preset_time)
    : sid_(sid), mem_pool_(pool), data_queue_(queue), out_file_(out_file),
      record_length_(record_length), preset_events_(preset_events), preset_time_(preset_time),
      is_running_(false), total_events_(0), total_bytes_(0) {
    // 기존의 mem_pool_, data_queue_ 포인터는 더 이상 사용되지 않으나, 
    // 레거시 인터페이스 호환성을 위해 남겨둡니다. 실제 I/O는 g_pipeline을 통합니다.
}

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

    // 파일 헤더 기록
    fout.write(reinterpret_cast<const char*>(&record_length_), sizeof(int));
    fout.write(reinterpret_cast<const char*>(&preset_events_), sizeof(int));
    total_bytes_.fetch_add(8, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();
    
    const int event_size = record_length_ * 512; 
    const int samples_per_ch = (event_size - 32) / 8; 
    const int read_kbytes = 16; 
    const int read_bytes = read_kbytes * 1024; 

    residual_buffer_.reserve(read_bytes * 2); 

    // 하드웨어 처리 상수
    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;

    while (is_running_.load(std::memory_order_acquire)) {
        
        // 1. 제한 조건 검사
        if (preset_events_ > 0 && total_events_.load(std::memory_order_relaxed) >= preset_events_) break;
        if (preset_time_ > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= preset_time_) break;
        }

        // 2. 하드웨어 버퍼 상태 폴링
        int bcount = KFADC500read_BCOUNT(sid_);
        if (bcount >= read_kbytes) {
            
            // [Zero-Allocation] 벌크 단위 데이터 블록 획득
            DataBlock* bulk = g_pipeline.AcquireFreeBulk();
            if (!bulk) {
                // 소비자 처리 지연으로 가용 메모리가 없음. 커널 Sleep 없이 하드웨어 파이프라인 정지.
                CPU_RELAX(); 
                continue;
            }

            // 하드웨어 DMA 데이터 적재 및 디스크 Write
            KFADC500read_DATA(sid_, read_kbytes, reinterpret_cast<char*>(bulk->data));
            bulk->valid_size = read_bytes;
            fout.write(reinterpret_cast<const char*>(bulk->data), bulk->valid_size);
            total_bytes_.fetch_add(bulk->valid_size, std::memory_order_relaxed);
            
            // 스트림 정합성을 위한 잔여 버퍼 처리
            residual_buffer_.insert(residual_buffer_.end(), bulk->data, bulk->data + bulk->valid_size);
            size_t offset = 0;
            
            // 3. 이벤트 단위 파싱 및 파이프라인 송출
            while (offset + event_size <= residual_buffer_.size()) {
                
                // [Zero-Allocation] 파싱 완료된 데이터를 담을 이벤트 블록 획득
                EventBlock* ev = g_pipeline.AcquireFreeEvent();
                if (!ev) {
                    CPU_RELAX(); 
                    continue; 
                }

                const uint8_t* evt_bytes = residual_buffer_.data() + offset;
                ev->samples_per_ch = samples_per_ch;

                // [Inline Processing] 메인 스레드에서 직접 기초 물리량 연산 수행
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
                        if (i < 4096) ev->last_waveform[ch][i] = inverted_adc;
                    }
                    
                    for (int i = samples_per_ch; i < 4096; ++i) {
                        ev->last_waveform[ch][i] = 0.0;
                    }
                    ev->charge_array[ch][0] = ch_charge;
                }

                // 텔레메트리 메타데이터 주입
                ev->num_events = 1;
                ev->total_acquired_events = total_events_.fetch_add(1, std::memory_order_relaxed) + 1;
                ev->queue_size = g_pipeline.GetRootQueueSize();
                ev->pool_free_size = g_pipeline.GetEventFreeSize();

                // 4. [소유권 이전] Root, Zmq 두 소비자가 이 메모리를 참조함을 명시
                ev->Retain(2);

                // 5. [Lock-Free Push] 각 소비자 큐로 포인터 복사 송출
                g_pipeline.PushToRoot(ev);
                g_pipeline.PushToZmq(ev);

                offset += event_size;
            }

            // 처리 완료된 벌크 블록은 즉시 Pool로 반환
            g_pipeline.ReturnToFreeBulk(bulk);

            if (offset > 0) {
                residual_buffer_.erase(residual_buffer_.begin(), residual_buffer_.begin() + offset);
            }

        } else {
            // 하드웨어 FIFO에 충분한 데이터가 쌓이지 않음. 커널 락을 피하기 위한 초경량 스핀.
            CPU_RELAX();
        }
    }

    fout.close();
    is_running_.store(false, std::memory_order_release);
}
