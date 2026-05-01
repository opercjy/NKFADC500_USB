#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <TFile.h>
#include <TTree.h>

constexpr int MAX_MONITOR_EVENTS = 2000;

// 💡 파이썬 GUI로 전송할 요약 패킷 (헤더 추가)
struct LiveMonitorPacket {
    // [ 메타데이터 텔레메트리 헤더 (24 Bytes) ]
    uint32_t num_events;           // 패킷에 포함된 실제 이벤트 갯수
    uint32_t samples_per_ch;       // 동적 RL 대응: 현재 채널당 샘플 수
    uint32_t total_acquired_events;// 전체 누적 이벤트
    uint32_t queue_size;           // 현재 큐 사이즈
    uint32_t pool_free_size;       // 현재 풀 여유 사이즈
    uint32_t padding;              // 64-bit alignment 패딩

    double last_waveform[4][4096]; // 파형 데이터
    double charge_array[4][MAX_MONITOR_EVENTS]; // 전하량 데이터
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

    std::vector<uint16_t> raw_event_data_;
    double pedestal_[4];
    double charge_[4];
    double peak_[4];

    LiveMonitorPacket current_packet_;
};