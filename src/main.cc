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
    libusb_device_handle* nkusb_get_device_handle(int sid);
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
    std::cout << "\033[1;32m>>> STARTING HARDWARE SET PHASE <<<\033[0m\n";
    
    if (KFADC500open(sid) < 0) {
        std::cerr << "\033[1;31m[SYSTEM:ERROR] Failed to open device. Check USB connection.\033[0m\n";
        USB3Exit();
        return 1;
    }

    if (g_app_running.load()) {
        // =========================================================================
        // 💡 [사용자 피드백 반영] 하드웨어 친화적 상태 소독 (Gentle Sanitization)
        // 무리한 EP0 제어를 삭제하고, 정규 명령어를 통한 안전한 리셋과 충분한 대기 시간을 부여합니다.
        // =========================================================================
        std::cout << "\033[1;33m[SYSTEM:INFO] Executing hardware state sanitization...\033[0m\n";
        
        // 1. DAQ가 이전에 켜져있었다면 하드웨어 레벨에서 즉각 중지
        KFADC500stop(sid); 
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // FIFO 비우기 및 명령 소화 대기

        // 2. 파이프라인 내부에 고여있던 쓰레기(Zombie) 데이터 뽑아내기
        libusb_device_handle* devh = nkusb_get_device_handle(sid);
        if (devh) {
            int transferred = 0;
            unsigned char garbage[16384];
            // 10ms 타임아웃으로 읽히는 데이터가 없을 때까지 부드럽게 빼냅니다.
            while (libusb_bulk_transfer(devh, 0x82, garbage, sizeof(garbage), &transferred, 10) == 0) {}
        }

        // 3. FADC 내부 레지스터 및 상태머신 강제 초기화 (Hard Reset)
        KFADC500reset(sid);   
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 칩셋 안정화 대기
        
        std::cout << "\033[1;32m[SYSTEM:INFO] Hardware Sanitization Complete.\033[0m\n";
        // =========================================================================

        KFADC500write_RM(sid, 1, 1, 0, 0);
        KFADC500reset(sid);
        KFADC500write_DRAMON(sid, 1);
        KFADC500calibrate(sid);
        
        // 💡 내부 캘리브레이션 후 잠시 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
        std::cout << "[SYSTEM:INFO] Waiting 500ms for Analog Baseline Settling...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 💡 아날로그 회로 안정화 시간을 길게 확보
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
        std::cout << "\033[1;33m[SYSTEM:INFO] Initial pipelines settled.\033[0m\n";
    }

    if (!g_app_running.load()) {
        std::cout << "\n\033[1;31m[SYSTEM:WARN] DAQ Initialization Aborted. Exiting safely...\033[0m\n";
        KFADC500close(sid);
        USB3Exit();
        return 0;
    }

    std::cout << "\n\033[1;32m>>> STARTING PARALLEL RUN PHASE <<<\033[0m\n";
    KFADC500reset(sid); 

    g_system_running.store(true, std::memory_order_release);

    ReadDataWorker usb_worker(sid, nullptr, nullptr, out_file, config.record_length, preset_events, preset_time);
    ZmqPublisher zmq_pub("tcp://*:5555", nullptr);
    
    zmq_pub.Start();
    usb_worker.Start();
    
    // 💡 [사용자 피드백 반영] 데이터 획득(Start) 직전에 장치가 완벽히 숨을 고를 수 있도록 1초 대기
    std::cout << "[SYSTEM:INFO] Preparing hardware for acquisition trigger...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    KFADC500start(sid); 
    std::cout << "\033[1;32m[SYSTEM:INFO] Trigger FSM Armed. DAQ Core Running at Zero-Deadtime.\033[0m\n";

    auto timer_start = std::chrono::steady_clock::now();

    // 메인 루프 대기 (워커 스레드가 살아있는 동안)
    while (g_app_running.load(std::memory_order_acquire) && usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n\033[1;33m[SYSTEM:INFO] Stopping Hardware Trigger (Draining FIFO)...\033[0m\n";
    
    // 강제 종료 시, ReadDataWorker.cc 내부에서 KFADC500stop을 호출하도록 권한을 완전히 이양했으므로
    // 메인 스레드는 조용히 워커가 완전히 종료될 때까지 기다리기만 합니다.
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

    // 모든 작업이 안전하게 종료된 후 닫기
    KFADC500close(sid);
    USB3Exit();

    return 0;
}
