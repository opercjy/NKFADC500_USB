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

#include <libusb.h>
extern "C" {
#include "NoticeKFADC500USB.h"
}

#include "ConfigParser.hh"
#include "ObjectPool.hh"
#include "ZmqPublisher.hh"
#include "ReadDataWorker.hh"

LockFreePipeline g_pipeline;
std::atomic<bool> g_system_running{false};
std::atomic<bool> g_app_running{true};

void SigIntHandler(int /*signum*/) {
    std::cout << "\n\033[1;31m[SYSTEM:WARN] Interrupt signal received. Initiating graceful shutdown...\033[0m\n";
    g_app_running.store(false, std::memory_order_release);
}

int main(int argc, char** argv) {
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

    std::filesystem::path out_path(out_file);
    std::filesystem::path dir_path = out_path.parent_path();
    if (!dir_path.empty() && !std::filesystem::exists(dir_path)) {
        std::filesystem::create_directories(dir_path);
    }

    KFADC500_Config config;
    if (!ConfigParser::Parse(config_file, config)) return 1;

    // 인터럽트 가능한 대기 함수 (데드락 방지)
    auto WaitInterruptible = [](int milliseconds) {
        int chunks = milliseconds / 10;
        for (int i = 0; i < chunks; ++i) {
            if (!g_app_running.load(std::memory_order_relaxed)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    auto now = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(now);

    std::cout << "\n\033[1;36m============================================================\033[0m\n";
    std::cout << "\033[1;36m [ KFADC500 DAQ Lock-Free Initialization ] \033[0m\n";
    std::cout << "\033[1;36m============================================================\033[0m\n";
    std::cout << "  - Config File  : " << config_file << "\n";
    std::cout << "  - RAW Out File : " << out_file << "\n";
    std::cout << "  - Target Event : " << (preset_events > 0 ? std::to_string(preset_events) : "Infinite") << "\n";
    std::cout << "  - Record Len   : " << config.record_length << " (" << config.record_length * 512 << " Bytes/Event)\n";
    std::cout << "  - Start Time   : " << std::ctime(&start_time); 
    std::cout << "\033[1;36m============================================================\033[0m\n\n";

    USB3Init();

    // =========================================================================
    // 1. 하드웨어 설정 (SET) 페이즈
    // =========================================================================
    std::cout << "\033[1;32m>>> STARTING HARDWARE SET PHASE <<<\033[0m\n";
    
    if (KFADC500open(sid) < 0) {
        std::cerr << "\033[1;31m[SYSTEM:ERROR] Failed to open device.\033[0m\n";
        USB3Exit();
        return 1;
    }

    if (g_app_running.load(std::memory_order_acquire)) {
        KFADC500write_RM(sid, 1, 1, 0, 0);
        KFADC500reset(sid);
        KFADC500write_DRAMON(sid, 1);
        KFADC500calibrate(sid);
        WaitInterruptible(200);
    }

    if (g_app_running.load(std::memory_order_acquire)) {
        KFADC500write_AMODE(sid, config.filter);
        KFADC500write_RL(sid, config.record_length);
        KFADC500write_TLT(sid, config.trigger_lut, 0); 
        KFADC500write_TOW(sid, 1000);
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running.load(std::memory_order_acquire)) break; 
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
    
    if (g_app_running.load(std::memory_order_acquire)) {
        std::cout << "[SYSTEM:INFO] Waiting 200ms for Analog Baseline Settling...\n";
        WaitInterruptible(200); 
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running.load(std::memory_order_acquire)) break;
        KFADC500measure_PED(sid, ch);
    }

    if (g_app_running.load(std::memory_order_acquire)) {
        WaitInterruptible(200);
        for (int ch = 1; ch <= 4; ++ch) {
            std::cout << "[DAQ:INFO] CH" << ch << " Settled Pedestal: " << KFADC500read_PED(sid, ch) << "\n";
        }
    }

    // =========================================================================
    // 2. 검증된 하드웨어 세션 분리 (Mid-Close) 및 안정화 대기
    // =========================================================================
    if (g_app_running.load(std::memory_order_acquire)) {
        std::cout << "\033[1;33m[SYSTEM:INFO] Closing device to simulate set/run separation...\033[0m\n";
        KFADC500close(sid);
    }

    // 💡 [핵심 복원] 하드웨어가 내부 FIFO를 비우고 세션을 온전히 정리할 수 있도록 1초(1000ms) 대기
    std::cout << "[SYSTEM:INFO] Waiting 1000ms for Hardware Endpoint Flush...\n";
    WaitInterruptible(1000);

    if (!g_app_running.load(std::memory_order_acquire)) {
        std::cout << "\n\033[1;31m[SYSTEM:WARN] DAQ Initialization Aborted. Exiting safely...\033[0m\n";
        USB3Exit();
        return 0;
    }

    // =========================================================================
    // 3. 획득 (RUN) 페이즈 및 락프리 엔진 기동
    // =========================================================================
    std::cout << "\n\033[1;32m>>> STARTING PARALLEL RUN PHASE <<<\033[0m\n";
    
    if (KFADC500open(sid) < 0) {
        std::cerr << "\033[1;31m[SYSTEM:ERROR] Failed to re-open device for RUN phase.\033[0m\n";
        USB3Exit();
        return 1;
    }
    
    KFADC500reset(sid); 

    g_system_running.store(true, std::memory_order_release);

    ReadDataWorker usb_worker(sid, nullptr, nullptr, out_file, config.record_length, preset_events, preset_time);
    ZmqPublisher zmq_pub("tcp://*:5555", nullptr);
    
    zmq_pub.Start();
    usb_worker.Start();
    
    KFADC500start(sid); 
    std::cout << "\033[1;32m[SYSTEM:INFO] Trigger FSM Armed. DAQ Core Running at Zero-Deadtime.\033[0m\n";

    auto timer_start = std::chrono::steady_clock::now();

    // 메인 스레드는 강제 종료 시그널이 인가되지 않고, 워커가 살아있는 동안 조용히 대기
    while (g_app_running.load(std::memory_order_acquire) && usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n\033[1;33m[SYSTEM:INFO] Stopping Hardware Trigger (Draining FIFO)...\033[0m\n";
    
    // 워커가 스스로 FIFO를 완벽히 배수(Drain)하고 종료할 때까지 대기
    while (usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[SYSTEM:INFO] Worker drained successfully. Shutting down Lock-Free Pipelines...\n";
    g_system_running.store(false, std::memory_order_release);
    
    zmq_pub.Stop();

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
    std::cout << "\033[1;32m=====================================================\033[0m\n";

    // 💡 [안전 보장] 모든 작업이 끝난 후 최종적으로 장치를 닫음
    KFADC500close(sid);
    USB3Exit();

    return 0;
}
