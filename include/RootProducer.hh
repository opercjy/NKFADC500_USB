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

    void RunBatchMode(std::atomic<bool>& is_running);
    void RunDisplayMode(std::atomic<bool>& is_running);

    // 💡 최종 요약 출력을 위한 Getter
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
    long long total_bytes_processed_; // 💡 클래스 멤버로 격상

    std::vector<uint16_t> raw_event_data_;
    double pedestal_[4];
    double charge_[4];
    double peak_[4];
};