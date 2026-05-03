#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <TFile.h>
#include <TTree.h>

constexpr int MAX_MONITOR_EVENTS = 2000;

struct LiveMonitorPacket {
    uint32_t num_events;           
    uint32_t samples_per_ch;       
    uint32_t total_acquired_events;
    uint32_t queue_size;           
    uint32_t pool_free_size;       
    uint32_t padding;              

    double last_waveform[4][4096]; 
    double charge_array[4][MAX_MONITOR_EVENTS]; 
};

class RootProducer {
public:
    RootProducer(const std::string& input_file, const std::string& output_file, bool save_waveform, bool display_mode);
    ~RootProducer();

    void RunBatchMode(std::atomic<bool>& is_running);
    void RunDisplayMode(std::atomic<bool>& is_running);

    int GetTotalEvents() const { return event_id_; }
    long long GetTotalBytes() const { return total_bytes_processed_; }

    void ProcessOnlineEvent(const uint16_t* raw_event, int samples_per_ch);
    void GetLatestPacket(LiveMonitorPacket& packet);
    void ClearPacket();
    int GetOnlineEventCount() const { return current_packet_.num_events; }

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

    // 💡 [UX 개선] 배열 지양, ROOT TBrowser 친화적인 독립 벡터 (파형)
    std::vector<double> wave_ch0_;
    std::vector<double> wave_ch1_;
    std::vector<double> wave_ch2_;
    std::vector<double> wave_ch3_;

    // 💡 [UX 개선] 배열 지양, ROOT TBrowser 친화적인 독립 변수 (페데스탈, 전하, 피크)
    double ped_ch0_, ped_ch1_, ped_ch2_, ped_ch3_;
    double charge_ch0_, charge_ch1_, charge_ch2_, charge_ch3_;
    double peak_ch0_, peak_ch1_, peak_ch2_, peak_ch3_;

    LiveMonitorPacket current_packet_;
};