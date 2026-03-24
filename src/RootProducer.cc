#include "RootProducer.hh"
#include <iostream>
#include <numeric>
#include <algorithm>

RootProducer::RootProducer(const std::string& output_root_file) {
    // 💡 압축률이 뛰어난 ROOT 파일 생성
    root_file_ = new TFile(output_root_file.c_str(), "RECREATE");
    fadc_tree_ = new TTree("FADC", "KFADC500 Offline Production Tree");

    // 💡 브랜치(Branch) 생성 및 메모리 주소 바인딩
    fadc_tree_->Branch("EventID", &b_event_id, "EventID/i");
    fadc_tree_->Branch("Baseline", &b_baseline, "Baseline/D");
    fadc_tree_->Branch("Amplitude", &b_amplitude, "Amplitude/D");
    fadc_tree_->Branch("Charge", &b_charge, "Charge/D");
    
    // std::vector는 ROOT가 자동으로 가변 배열 브랜치로 인식하여 처리함
    fadc_tree_->Branch("Waveform", &b_waveform);
}

RootProducer::~RootProducer() {
    if (root_file_ && root_file_->IsOpen()) {
        Close();
    }
}

void RootProducer::ProcessEvent(uint32_t event_id, const uint16_t* raw_data, size_t num_samples) {
    b_event_id = event_id;
    
    // 1. 파형 데이터 복사 (벡터 크기 사전 할당으로 오버헤드 최소화)
    b_waveform.assign(raw_data, raw_data + num_samples);

    // 2. 1차 물리량 계산 (QDC, Peak, Baseline)
    CalculatePhysicsQuantities();

    // 3. TTree에 1개 이벤트 기록 (Basket에 담김)
    fadc_tree_->Fill();
}

void RootProducer::CalculatePhysicsQuantities() {
    if (b_waveform.empty()) return;

    size_t length = b_waveform.size();
    
    // [1] Baseline 계산: 파형의 첫 20 샘플(또는 10%)의 평균을 기저대역으로 가정
    size_t bl_window = std::min((size_t)20, length / 10);
    if (bl_window == 0) bl_window = 1;
    
    double sum_bl = 0;
    for (size_t i = 0; i < bl_window; ++i) {
        sum_bl += b_waveform[i];
    }
    b_baseline = sum_bl / (double)bl_window;

    // [2] Peak Amplitude 계산 및 [3] Charge (적분량) 계산
    b_amplitude = 0.0;
    b_charge = 0.0;

    for (size_t i = 0; i < length; ++i) {
        // 음의 극성(Negative Polarity - 보통 PMT 펄스)을 기준으로 Baseline에서 아래로 떨어지는 펄스를 양수화
        // 양의 극성(Positive) 장비라면 부호를 반대로 처리
        double corrected_adc = b_baseline - (double)b_waveform[i]; 
        
        if (corrected_adc > b_amplitude) {
            b_amplitude = corrected_adc; // Max Peak 탐색
        }
        
        // 간단한 펄스 적분 (Threshold 없이 전체 적분, 필요시 Threshold 로직 추가 가능)
        if (corrected_adc > 0) {
            b_charge += corrected_adc;
        }
    }
}

void RootProducer::Close() {
    if (root_file_) {
        root_file_->cd();
        fadc_tree_->Write(); // 메모리 Basket에 남아있는 데이터를 디스크로 Flush
        root_file_->Close();
        std::cout << "[PROD:INFO] ROOT File generated successfully. Tree Entries: " 
                  << fadc_tree_->GetEntries() << "\n";
        delete root_file_;
        root_file_ = nullptr;
    }
}