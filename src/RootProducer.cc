#include "RootProducer.hh"
#include "ObjectPool.hh"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <TSystem.h>
#include <iomanip>
#include <TCanvas.h>
#include <TH1D.h>
#include <cstring> 

// 글로벌 락프리 파이프라인 참조
extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;

RootProducer::RootProducer(const std::string& input_file, const std::string& output_file, bool save_waveform, bool display_mode)
    : in_filename_(input_file), out_filename_(output_file), 
      save_waveform_(save_waveform), display_mode_(display_mode),
      root_file_(nullptr), tree_(nullptr), event_id_(0), record_length_(0), preset_events_(0), total_bytes_processed_(0) {
    
    if (!display_mode_) {
        root_file_ = new TFile(out_filename_.c_str(), "RECREATE");
        root_file_->cd();
        tree_ = new TTree("kfadc_tree", "Notice KFADC500 Physics Data");

        tree_->Branch("event_id", &event_id_, "event_id/I");
        tree_->Branch("record_length", &record_length_, "record_length/I");
        
        tree_->Branch("pedestal_ch0", &ped_ch0_, "pedestal_ch0/D");
        tree_->Branch("pedestal_ch1", &ped_ch1_, "pedestal_ch1/D");
        tree_->Branch("pedestal_ch2", &ped_ch2_, "pedestal_ch2/D");
        tree_->Branch("pedestal_ch3", &ped_ch3_, "pedestal_ch3/D");

        tree_->Branch("charge_ch0", &charge_ch0_, "charge_ch0/D");
        tree_->Branch("charge_ch1", &charge_ch1_, "charge_ch1/D");
        tree_->Branch("charge_ch2", &charge_ch2_, "charge_ch2/D");
        tree_->Branch("charge_ch3", &charge_ch3_, "charge_ch3/D");

        tree_->Branch("peak_ch0", &peak_ch0_, "peak_ch0/D");
        tree_->Branch("peak_ch1", &peak_ch1_, "peak_ch1/D");
        tree_->Branch("peak_ch2", &peak_ch2_, "peak_ch2/D");
        tree_->Branch("peak_ch3", &peak_ch3_, "peak_ch3/D");
        
        if (save_waveform_) {
            tree_->Branch("wave_ch0", &wave_ch0_);
            tree_->Branch("wave_ch1", &wave_ch1_);
            tree_->Branch("wave_ch2", &wave_ch2_);
            tree_->Branch("wave_ch3", &wave_ch3_);
        }
    }
}

RootProducer::~RootProducer() {
    if (root_file_) {
        root_file_->cd();
        if (tree_) tree_->Write();
        root_file_->Close();
        delete root_file_;
    }
}

// ===========================================================================
// [신규] 락프리 온라인 데이터 수집 모드 (Zero-Copy)
// ===========================================================================
void RootProducer::RunOnlineMode() {
    // 디스크 I/O 병목 방지를 위한 대용량 AutoSave 설정 (약 100MB)
    if (tree_) tree_->SetAutoSave(104857600); 

    while (g_system_running.load(std::memory_order_acquire)) {
        
        // 1. Ready 큐에서 이벤트 블록 획득 (Mutex 락 대기 없음)
        EventBlock* ev = g_pipeline.AcquireForRoot();
        if (!ev) {
            CPU_RELAX(); // 큐가 비어있으면 초경량 스핀
            continue;
        }

        // 2. ROOT Tree 변수 바인딩
        event_id_ = ev->total_acquired_events;
        record_length_ = ev->samples_per_ch * 8 + 32;

        if (ev->samples_per_ch > 0) {
            // ReadDataWorker가 연산해둔 전하량 데이터를 그대로 매핑
            charge_ch0_ = ev->charge_array[0][0];
            charge_ch1_ = ev->charge_array[1][0];
            charge_ch2_ = ev->charge_array[2][0];
            charge_ch3_ = ev->charge_array[3][0];
            
            // 온라인 모드에서는 연산 오버헤드 최소화를 위해 peak/pedestal을 간소화 (또는 필요시 ReadDataWorker에서 계산 추가)
            peak_ch0_ = peak_ch1_ = peak_ch2_ = peak_ch3_ = 0.0;
            ped_ch0_ = ped_ch1_ = ped_ch2_ = ped_ch3_ = 0.0;

            if (save_waveform_) {
                // std::vector 내부 할당자만 사용하여 고속 복사
                wave_ch0_.assign(ev->last_waveform[0], ev->last_waveform[0] + ev->samples_per_ch);
                wave_ch1_.assign(ev->last_waveform[1], ev->last_waveform[1] + ev->samples_per_ch);
                wave_ch2_.assign(ev->last_waveform[2], ev->last_waveform[2] + ev->samples_per_ch);
                wave_ch3_.assign(ev->last_waveform[3], ev->last_waveform[3] + ev->samples_per_ch);
            }

            tree_->Fill();
        }

        // 3. 소유권 반환 (마지막 컨슈머라면 Free 큐로 환원)
        g_pipeline.ReturnToFreeEvent(ev);
    }
}

// ===========================================================================
// 대화형 디스플레이 모드 (오프라인 뷰어 - 기존 로직 100% 보존)
// ===========================================================================
void RootProducer::RunDisplayMode(std::atomic<bool>& is_running) {
    std::ifstream infile(in_filename_, std::ios::binary);
    if (!infile.is_open()) return;

    infile.read(reinterpret_cast<char*>(&record_length_), sizeof(int));
    infile.read(reinterpret_cast<char*>(&preset_events_), sizeof(int));

    infile.seekg(0, std::ios::end);
    long long file_size = infile.tellg();
    int event_size_bytes = record_length_ * 512;
    int samples_per_ch = (event_size_bytes - 32) / 8;
    
    int total_events = (file_size - 8) / event_size_bytes; 
    if (total_events <= 0) return;

    std::vector<uint8_t> raw_buffer(event_size_bytes);

    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;   

    TCanvas* c1 = new TCanvas("c1", "KFADC500 Waveform Viewer", 1200, 800);
    c1->Divide(2, 2);
    TH1D* h_wave[4];
    for (int ch = 0; ch < 4; ++ch) {
        h_wave[ch] = new TH1D(Form("h_wave_ch%d", ch), Form("Channel %d (Inverted);Sample Index;ADC", ch), samples_per_ch, 0, samples_per_ch);
        h_wave[ch]->SetLineColor(kBlue + ch);
        h_wave[ch]->SetStats(0); 
    }

    int current_id = 0;
    std::string input;

    while (is_running.load()) {
        if (current_id < 0) current_id = 0;
        if (current_id >= total_events) current_id = total_events - 1;

        infile.seekg(8 + (long long)current_id * event_size_bytes, std::ios::beg);
        infile.read(reinterpret_cast<char*>(raw_buffer.data()), event_size_bytes);
        
        const uint8_t* evt_bytes = raw_buffer.data();

        for (int ch = 0; ch < 4; ++ch) { h_wave[ch]->Reset(); }

        for (int ch = 0; ch < 4; ++ch) {
            double ch_pedestal = 0.0;
            int ped_start = std::min(PED_START, samples_per_ch);
            int ped_end = std::min(PED_END, samples_per_ch);
            int num_ped = ped_end - ped_start;
            
            for (int i = ped_start; i < ped_end; ++i) {
                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + i * 8 + ch * 2);
                ch_pedestal += adc;
            }
            if (num_ped > 0) ch_pedestal /= num_ped;

            for (int i = SKIP_BINS; i < samples_per_ch; ++i) {
                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + i * 8 + ch * 2);
                double inverted_adc = ch_pedestal - adc;
                h_wave[ch]->SetBinContent(i + 1, inverted_adc); 
            }
        }

        for (int ch = 0; ch < 4; ++ch) {
            c1->cd(ch + 1);
            h_wave[ch]->Draw("HIST");
        }
        c1->Update();
        gSystem->ProcessEvents();

        std::cout << "\r\033[K\033[1;33m[DISPLAY]\033[0m Event \033[1;32m" << current_id << "\033[0m / " << total_events - 1 
                  << " | [\033[1;36mn\033[0m]ext, [\033[1;36mp\033[0m]rev, [\033[1;36mj <id>\033[0m]ump, [\033[1;31mq\033[0m]uit : " << std::flush;
        std::getline(std::cin, input);
        
        if (input == "q" || input == "Q") break;
        else if (input == "p" || input == "P") current_id--;
        else if (input == "n" || input == "N" || input.empty()) current_id++;
        else if (input.rfind("j ", 0) == 0 || input.rfind("J ", 0) == 0) {
            try { current_id = std::stoi(input.substr(2)); }
            catch (...) { std::cout << "Invalid jump format. Use 'j <id>'\n"; }
        }
    }
    delete c1;
}

// ===========================================================================
// 물리 TTree 변환 모드 (Batch Mode - 기존 로직 100% 보존)
// ===========================================================================
void RootProducer::RunBatchMode(std::atomic<bool>& is_running) {
    std::ifstream infile(in_filename_, std::ios::binary);
    if (!infile.is_open()) return;

    infile.read(reinterpret_cast<char*>(&record_length_), sizeof(int));
    infile.read(reinterpret_cast<char*>(&preset_events_), sizeof(int));

    int event_size_bytes = record_length_ * 512;
    if (event_size_bytes <= 0) return;

    int samples_per_ch = (event_size_bytes - 32) / 8;
    std::vector<uint8_t> raw_buffer(event_size_bytes);

    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;   

    total_bytes_processed_ = 8; 
    event_id_ = 0;

    while (is_running.load()) {
        infile.read(reinterpret_cast<char*>(raw_buffer.data()), event_size_bytes);
        int bytes_read = infile.gcount();
        if (bytes_read < event_size_bytes) break;
        
        total_bytes_processed_ += bytes_read;
        const uint8_t* evt_bytes = raw_buffer.data();

        if (save_waveform_) {
            wave_ch0_.clear(); wave_ch0_.reserve(samples_per_ch);
            wave_ch1_.clear(); wave_ch1_.reserve(samples_per_ch);
            wave_ch2_.clear(); wave_ch2_.reserve(samples_per_ch);
            wave_ch3_.clear(); wave_ch3_.reserve(samples_per_ch);
        }

        ped_ch0_ = 0; ped_ch1_ = 0; ped_ch2_ = 0; ped_ch3_ = 0;
        charge_ch0_ = 0; charge_ch1_ = 0; charge_ch2_ = 0; charge_ch3_ = 0;
        peak_ch0_ = -9999; peak_ch1_ = -9999; peak_ch2_ = -9999; peak_ch3_ = -9999;

        for (int ch = 0; ch < 4; ++ch) {
            double current_ped = 0.0;
            int ped_start = std::min(PED_START, samples_per_ch);
            int ped_end = std::min(PED_END, samples_per_ch);
            int num_ped = ped_end - ped_start;
            
            for (int i = ped_start; i < ped_end; ++i) {
                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + i * 8 + ch * 2);
                current_ped += adc;
            }
            if (num_ped > 0) current_ped /= num_ped;

            double current_charge = 0.0;
            double current_peak = -9999.0;

            for (int i = 0; i < samples_per_ch; ++i) {
                uint16_t adc = *reinterpret_cast<const uint16_t*>(evt_bytes + 32 + i * 8 + ch * 2);
                double inverted_adc = current_ped - adc;

                if (i >= SKIP_BINS) {
                    current_charge += inverted_adc;
                    if (inverted_adc > current_peak) current_peak = inverted_adc;
                }

                if (save_waveform_) {
                    if (ch == 0) wave_ch0_.push_back(inverted_adc);
                    else if (ch == 1) wave_ch1_.push_back(inverted_adc);
                    else if (ch == 2) wave_ch2_.push_back(inverted_adc);
                    else if (ch == 3) wave_ch3_.push_back(inverted_adc);
                }
            }

            if (ch == 0) { ped_ch0_ = current_ped; charge_ch0_ = current_charge; peak_ch0_ = current_peak; }
            else if (ch == 1) { ped_ch1_ = current_ped; charge_ch1_ = current_charge; peak_ch1_ = current_peak; }
            else if (ch == 2) { ped_ch2_ = current_ped; charge_ch2_ = current_charge; peak_ch2_ = current_peak; }
            else if (ch == 3) { ped_ch3_ = current_ped; charge_ch3_ = current_charge; peak_ch3_ = current_peak; }
        }

        tree_->Fill();
        event_id_++;

        if (event_id_ % 5000 == 0) {
            std::cout << "\r\033[K\033[1;34m[PROD:INFO]\033[0m Processing... \033[1;32m" << event_id_ << "\033[0m events saved." << std::flush;
        }
    }
}
