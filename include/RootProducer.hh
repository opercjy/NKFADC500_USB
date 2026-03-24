#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ROOT Headers
#include <TFile.h>
#include <TTree.h>

class RootProducer {
public:
    RootProducer(const std::string& output_root_file);
    ~RootProducer();

    // 💡 1개의 물리 이벤트를 파싱하여 TTree에 Fill 하는 코어 함수
    // raw_data: 1개 이벤트 분량의 바이너리 데이터 포인터
    // num_samples: 해당 이벤트의 총 샘플 수 (Record Length)
    void ProcessEvent(uint32_t event_id, const uint16_t* raw_data, size_t num_samples);

    void Close();

private:
    TFile* root_file_;
    TTree* fadc_tree_;

    // 💡 TTree Branches (물리학자들에게 제공될 직관적인 변수들)
    uint32_t b_event_id;
    double   b_baseline;     // 파형 앞단의 평균 기저대역 전압(ADC)
    double   b_amplitude;    // 최대 Peak 전압(ADC)
    double   b_charge;       // Baseline이 보정된 적분 전하량 (QDC)
    
    // 파형 시계열 데이터를 가변 길이 배열로 저장 (GUI 렌더링 및 심층 분석용)
    std::vector<uint16_t> b_waveform; 

    // 내부 분석 로직
    void CalculatePhysicsQuantities();
};