#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <filesystem>

extern "C" {
#include "NoticeKFADC500USB.h"
}

#include "ConfigParser.hh"
#include "ObjectPool.hh"
#include "ZmqPublisher.hh"
#include "ReadDataWorker.hh"
#include "RootProducer.hh"

// ==============================================================================
// 💡 [Global Architecture] 락프리 파이프라인 및 생명주기 제어 플래그 정의
// ==============================================================================
LockFreePipeline g_pipeline;
std::atomic<bool> g_system_running{false};
std::atomic<bool> g_app_running{true};

// 인터럽트 시그널 핸들러 (Ctrl+C)
void SigIntHandler(int /*signum*/) {
    std::cout << "\n\033[1;31m[SYSTEM:WARN] Interrupt signal received. Initiating graceful shutdown...\033[0m\n";
    g_app_running.store(false, std::memory_order_release);
}

int main(int argc, char** argv) {
    // OS 파이프 버퍼링 비활성화 (로그 즉각 출력 보장)
    setvbuf(stdout, NULL, _IONBF, 0);

    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string config_file = "";
    std::string out_file = "data/run_001.dat"; 
    int preset_events = 0;
    int preset_time = 0; 
    int sid = 0;

    int opt;
    while ((opt = getopt(argc, argv, "f:o:n:t:s:")) != -1) {
        switch (opt) {
            case 'f': config_file = optarg; break;
            case 'o': out_file = optarg; break;
            case 'n': preset_events = std::stoi(optarg); break;
            case 't': preset_time = std::stoi(optarg); break;
            case 's': sid = std::stoi(optarg); break;
        }
    }

    if (config_file.empty()) {
        std::cerr << "Usage: " << argv[0] << " -f <config_file> [-o output.dat] [-n events] [-t seconds]\n";
        return 1;
    }

    // 데이터 저장 디렉토리 검증 및 생성
    std::filesystem::path out_path(out_file);
    std::filesystem::path dir_path = out_path.parent_path();
    if (!dir_path.empty() && !std::filesystem::exists(dir_path)) {
        std::filesystem::create_directories(dir_path);
    }

    // 설정 파싱
    KFADC500_Config config;
    if (!ConfigParser::Parse(config_file, config)) return 1;

    // ROOT 출력 파일명 자동 생성 (.dat -> .root)
    std::string root_out_file = out_file;
    size_t dot_pos = root_out_file.find_last_of('.');
    if (dot_pos != std::string::npos) {
        root_out_file.replace(dot_pos, std::string::npos, ".root");
    } else {
        root_out_file += ".root";
    }

    auto now = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(now);

    std::cout << "\n\033[1;36m============================================================\033[0m\n";
    std::cout << "\033[1;36m [ KFADC500 DAQ Lock-Free Initialization ] \033[0m\n";
    std::cout << "\033[1;36m============================================================\033[0m\n";
    std::cout << "  - Config File  : " << config_file << "\n";
    std::cout << "  - RAW Out File : " << out_file << "\n";
    std::cout << "  - ROOT Out File: " << root_out_file << "\n";
    std::cout << "  - Target Event : " << (preset_events > 0 ? std::to_string(preset_events) : "Infinite") << "\n";
    std::cout << "  - Record Len   : " << config.record_length << " (" << config.record_length * 512 << " Bytes/Event)\n";
    std::cout << "  - Start Time   : " << std::ctime(&start_time); 
    std::cout << "\033[1;36m============================================================\033[0m\n\n";

    USB3Init();

    std::cout << "\033[1;32m>>> STARTING HARDWARE SET PHASE <<<\033[0m\n";
    
    if (g_app_running.load()) {
        KFADC500open(sid);
        KFADC500write_RM(sid, 1, 1, 0, 0);
        KFADC500reset(sid);
        KFADC500write_DRAMON(sid, 1);
        KFADC500calibrate(sid);
    }

    if (g_app_running.load()) {
        KFADC500write_AMODE(sid, config.filter);
        KFADC500write_RL(sid, config.record_length);
        KFADC500write_TLT(sid, config.trigger_lut, 0); 
        KFADC500write_TOW(sid, 1000);
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running.load()) break; 
        int idx = ch - 1;
        KFADC500write_DACOFF(sid, ch, config.offset[idx]);
        KFADC500write_DLY(sid, ch, config.delay[idx]);
        KFADC500write_POL(sid, ch, config.polarity[idx]);
        KFADC500write_THR(sid, ch, config.threshold[idx]); 
        KFADC500write_TM(sid, ch, config.pulse_width_en, config.pulse_count_en); 
        KFADC500write_PCT(sid, ch, config.pulse_count_thr);
        KFADC500write_PCI(sid, ch, config.pulse_count_int);
        KFADC500write_PWT(sid, ch, config.pulse_width_thr);
        KFADC500write_DT(sid, ch, config.deadtime);
        KFADC500write_CW(sid, ch, config.coincidence_width);
    }
    
    if (g_app_running.load()) {
        std::cout << "[SYSTEM:INFO] Waiting 200ms for Analog Baseline Settling...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running.load()) break;
        KFADC500measure_PED(sid, ch);
    }

    if (g_app_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        for (int ch = 1; ch <= 4; ++ch) {
            std::cout << "[DAQ:INFO] CH" << ch << " Settled Pedestal: " << KFADC500read_PED(sid, ch) << "\n";
        }
        std::cout << "\033[1;33m[SYSTEM:INFO] Flushing initial pipelines...\033[0m\n";
        KFADC500close(sid);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (!g_app_running.load()) {
        std::cout << "\n\033[1;31m[SYSTEM:WARN] DAQ Initialization Aborted. Exiting safely...\033[0m\n";
        USB3Exit();
        return 0;
    }

    // ==============================================================================
    // [핵심] 락프리 다중 스레드 파이프라인 오케스트레이션
    // ==============================================================================
    std::cout << "\n\033[1;32m>>> STARTING PARALLEL RUN PHASE <<<\033[0m\n";
    KFADC500open(sid); 
    KFADC500reset(sid); 

    // 글로벌 시스템 플래그 ON (메모리 배리어를 통한 전역 가시성 확보)
    g_system_running.store(true, std::memory_order_release);

    // 생산자 및 소비자 객체 인스턴스화
    ReadDataWorker usb_worker(sid, nullptr, nullptr, out_file, config.record_length, preset_events, preset_time);
    ZmqPublisher zmq_pub("tcp://*:5555", nullptr);
    
    // ROOT 생산자: 온라인 모드(디스플레이 false, 파형저장 true 설정)
    bool save_full_waveform = true; 
    RootProducer online_root_producer("", root_out_file, save_full_waveform, false);

    // 소비자 스레드 선 기동 (생산된 데이터를 즉각 수용할 수 있도록 대기)
    zmq_pub.Start();
    std::thread root_thread(&RootProducer::RunOnlineMode, &online_root_producer);

    // 생산자 스레드 기동
    usb_worker.Start();
    
    // 모든 소프트웨어 버퍼가 준비된 후 최후에 하드웨어 트리거 래치 개방
    KFADC500start(sid); 
    std::cout << "\033[1;32m[SYSTEM:INFO] Trigger FSM Armed. DAQ Core Running at Zero-Deadtime.\033[0m\n";

    auto timer_start = std::chrono::steady_clock::now();

    // 메인 스레드는 상태 감시 역할만 수행
    while (g_app_running.load(std::memory_order_acquire) && usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ==============================================================================
    // [핵심] Graceful Shutdown (우아한 종료) 시퀀스
    // ==============================================================================
    std::cout << "\n\033[1;33m[SYSTEM:INFO] Stopping Hardware Trigger (Draining FIFO)...\033[0m\n";
    KFADC500stop(sid);
    
    // 하드웨어 버퍼 내부의 잔여 데이터가 파이프라인으로 모두 밀려나오도록 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[SYSTEM:INFO] Initiating Graceful Shutdown for Lock-Free Pipelines...\n";
    
    // 워커들에게 루프 종료 시그널 전달 (메모리 가시성 확보)
    g_system_running.store(false, std::memory_order_release);
    
    // 스레드 회수 (내부 큐가 비워질 때까지 안전하게 대기 후 종료됨)
    usb_worker.Stop();
    zmq_pub.Stop();
    if (root_thread.joinable()) {
        root_thread.join();
        std::cout << "[ROOT:INFO] Online ROOT TTree gracefully saved and closed.\n";
    }

    auto timer_end = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(timer_end - timer_start).count();
    
    int final_events = usb_worker.GetTotalAcquiredEvents();
    double avg_trigger_rate = (total_sec > 0.0) ? (final_events / total_sec) : 0.0;
    size_t final_bytes = usb_worker.GetTotalAcquiredBytes();
    double final_mb = final_bytes / (1024.0 * 1024.0);

    std::cout << "\n\033[1;32m================ ACQUISITION SUMMARY ================\033[0m\n";
    std::cout << " Total Elapsed Time : \033[1;33m" << std::fixed << std::setprecision(2) << total_sec << " sec\033[0m\n";
    std::cout << " Total Events       : \033[1;36m" << final_events << " Events\033[0m\n";
    std::cout << " Total RAW Data     : \033[1;36m" << std::fixed << std::setprecision(2) << final_mb << " MB\033[0m\n";
    std::cout << " Avg Trigger Rate   : \033[1;35m" << std::fixed << std::setprecision(1) << avg_trigger_rate << " Hz\033[0m\n";
    std::cout << " RAW File Saved to  : \033[1;36m" << out_file << "\033[0m\n";
    std::cout << " ROOT File Saved to : \033[1;36m" << root_out_file << "\033[0m\n";
    std::cout << "\033[1;32m=====================================================\033[0m\n";

    KFADC500close(sid);
    USB3Exit();

    return 0;
}
