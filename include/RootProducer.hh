#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <TFile.h>
#include <TTree.h>

class RootProducer {
public:
    RootProducer(const std::string& input_file, const std::string& output_file, bool save_waveform, bool display_mode);
    ~RootProducer();

    // 💡 [패치] 잔여 온라인 함수 완전히 제거, 오직 오프라인 기능만 수행
    void RunBatchMode(std::atomic<bool>& is_running);
    void RunDisplayMode(std::atomic<bool>& is_running);

    int GetTotalEvents() const { return event_id_; }
    long long GetTotalBytes() const { return total_bytes_processed_; }

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
    long long total_bytes_processed_;

    std::vector<double> wave_ch0_, wave_ch1_, wave_ch2_, wave_ch3_;
    double ped_ch0_, ped_ch1_, ped_ch2_, ped_ch3_;
    double charge_ch0_, charge_ch1_, charge_ch2_, charge_ch3_;
    double peak_ch0_, peak_ch1_, peak_ch2_, peak_ch3_;
};
