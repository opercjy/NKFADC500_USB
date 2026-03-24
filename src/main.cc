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

std::atomic<bool> g_app_running(true);

void SigIntHandler(int /*signum*/) {
    std::cout << "\n\033[1;31m[SYSTEM:WARN] Interrupt signal (Ctrl+C) received. Initiating graceful shutdown...\033[0m\n";
    g_app_running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string config_file = "";
    std::string out_file = "data/kfadc500_data.dat"; 
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
        std::cout << "\033[1;33m[SYSTEM:INFO] Created data directory: " << dir_path << "\033[0m\n";
    }

    KFADC500_Config config;
    if (!ConfigParser::Parse(config_file, config)) return 1;

    auto now = std::chrono::system_clock::now();
    std::time_t start_time = std::chrono::system_clock::to_time_t(now);

    std::cout << "\n\033[1;36m============================================================\033[0m\n";
    std::cout << "\033[1;36m [ KFADC500 DAQ Initialization Summary ] \033[0m\n";
    std::cout << "\033[1;36m============================================================\033[0m\n";
    std::cout << "  - Config File  : " << config_file << "\n";
    std::cout << "  - Output File  : " << out_file << "\n";
    std::cout << "  - Target Event : " << (preset_events > 0 ? std::to_string(preset_events) : "Infinite") << "\n";
    std::cout << "  - Record Len   : " << config.record_length << " (" << config.record_length * 512 << " Bytes/Event)\n";
    std::cout << "  - Trigger LUT  : 0x" << std::hex << std::uppercase << config.trigger_lut << std::nouppercase << std::dec << "\n";
    std::cout << "  --------------------------------------------------------\n";
    std::cout << "   [CH] | POLARITY | THRESHOLD | DELAY | DACOFFSET \n";
    std::cout << "  --------------------------------------------------------\n";
    for(int i = 0; i < 4; i++) {
        std::cout << "   [" << i+1 << "] | " 
                  << std::setw(8) << (config.polarity[i] == 0 ? "0 (NEG)" : "1 (POS)") << " | " 
                  << std::setw(9) << config.threshold[i] << " | " 
                  << std::setw(5) << config.delay[i] << " | " 
                  << std::setw(9) << config.offset[i] << "\n";
    }
    std::cout << "  --------------------------------------------------------\n";
    std::cout << "  - Start Time   : " << std::ctime(&start_time); 
    std::cout << "\033[1;36m============================================================\033[0m\n\n";

    USB3Init();

    // ====================================================================
    // 💡 [SET PHASE] : 하드웨어 크래시 및 Ctrl+C 즉시 반응 방어 로직 적용
    // ====================================================================
    std::cout << "\033[1;32m>>> STARTING SET PHASE <<<\033[0m\n";
    
    if (g_app_running) {
        KFADC500open(sid);
        KFADC500write_RM(sid, 1, 1, 0, 0);
        KFADC500reset(sid);
        KFADC500write_DRAMON(sid, 1);
        KFADC500calibrate(sid);
    }

    if (g_app_running) {
        KFADC500write_AMODE(sid, config.filter);
        KFADC500write_RL(sid, config.record_length);
        KFADC500write_TLT(sid, config.trigger_lut, 0); 
        KFADC500write_TOW(sid, 1000);
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running) break; // 즉각 중단
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
    
    if (g_app_running) std::cout << "[SYSTEM:INFO] Waiting 200ms for Analog Baseline Settling...\n";
    
    // 💡 Sleep을 잘게 쪼개어 대기 중에도 Ctrl+C가 눌리면 즉각 빠져나오게 구성
    for (int i = 0; i < 20; ++i) {
        if (!g_app_running) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running) break;
        KFADC500measure_PED(sid, ch);
    }

    for (int i = 0; i < 20; ++i) {
        if (!g_app_running) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int ch = 1; ch <= 4; ++ch) {
        if (!g_app_running) break;
        std::cout << "[DAQ:INFO] CH" << ch << " Settled Pedestal: " << KFADC500read_PED(sid, ch) << "\n";
    }

    if (g_app_running) {
        std::cout << "\033[1;33m[SYSTEM:INFO] Closing device to simulate set/run separation...\033[0m\n";
        KFADC500close(sid);
    }

    for (int i = 0; i < 200; ++i) { // 2초 대기
        if (!g_app_running) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 💡 초기화 도중 Ctrl+C가 눌렸다면 프로그램 완전 종료 (진입 차단)
    if (!g_app_running) {
        std::cout << "\n\033[1;31m[SYSTEM:WARN] DAQ Initialization Aborted. Exiting safely...\033[0m\n";
        USB3Exit();
        return 0;
    }


    // ====================================================================
    // [RUN PHASE] : 정상적으로 설정이 완료된 경우에만 진입
    // ====================================================================
    std::cout << "\n\033[1;32m>>> STARTING RUN PHASE <<<\033[0m\n";
    KFADC500open(sid); 
    KFADC500reset(sid); 

    ObjectPool mem_pool(1000); 
    DataQueue data_queue;
    ZmqPublisher zmq_pub("tcp://*:5555", &data_queue);
    ReadDataWorker usb_worker(sid, &mem_pool, &data_queue, out_file, config.record_length, preset_events, preset_time);

    auto timer_start = std::chrono::steady_clock::now();

    zmq_pub.Start();
    usb_worker.Start();
    
    KFADC500start(sid); 
    std::cout << "\033[1;32m[SYSTEM:INFO] Trigger FSM Armed. Ready for Physical Pulses.\033[0m\n";

    // 메인 루프: Ctrl+C가 눌리기 전까지 대기
    while (g_app_running && usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[SYSTEM:INFO] Shutting down DAQ gracefully...\n";
    
    usb_worker.Stop();
    zmq_pub.Stop();
    KFADC500stop(sid);

    auto timer_end = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(timer_end - timer_start).count();
    
    auto end_now = std::chrono::system_clock::now();
    std::time_t end_time_t = std::chrono::system_clock::to_time_t(end_now);

    int final_events = usb_worker.GetTotalAcquiredEvents();
    double avg_trigger_rate = (total_sec > 0.0) ? (final_events / total_sec) : 0.0;
    size_t final_bytes = usb_worker.GetTotalAcquiredBytes();
    double final_mb = final_bytes / (1024.0 * 1024.0);

    std::cout << "\n\033[1;32m================ ACQUISITION SUMMARY ================\033[0m\n";
    std::cout << " End Time           : " << std::ctime(&end_time_t);
    std::cout << " Total Elapsed Time : \033[1;33m" << std::fixed << std::setprecision(2) << total_sec << " sec\033[0m\n";
    std::cout << " Total Events       : \033[1;36m" << final_events << " Events\033[0m\n";
    std::cout << " Total Data Size    : \033[1;36m" << std::fixed << std::setprecision(2) << final_mb << " MB\033[0m\n";
    std::cout << " Avg Trigger Rate   : \033[1;35m" << std::fixed << std::setprecision(1) << avg_trigger_rate << " Hz\033[0m\n";
    std::cout << " Data File Saved to : \033[1;36m" << out_file << "\033[0m\n";
    std::cout << "\033[1;32m=====================================================\033[0m\n";

    KFADC500close(sid);
    USB3Exit();

    return 0;
}