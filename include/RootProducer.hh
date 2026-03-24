#pragma once
#include <string>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TH1D.h>

class RootProducer {
public:
    RootProducer(const std::string& input_file, const std::string& output_file, bool save_waveform, bool display_mode);
    ~RootProducer();

    // 💡 양립 불가한 두 개의 독립 프로세스
    void RunBatchMode();
    void RunDisplayMode();

private:
    std::string in_filename_;
    std::string out_filename_;
    bool save_waveform_;
    bool display_mode_;
    
    TFile* root_file_;
    TTree* tree_;

    int event_id_;
    int record_length_;
    int preset_events_;
    
    // 💡 물리 분석 변수
    double pedestal_[4]; 
    double charge_[4];   
    double peak_[4];     

    std::vector<unsigned short> raw_event_data_; 
};