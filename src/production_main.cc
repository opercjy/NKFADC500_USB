#include <iostream>
#include <chrono>
#include <unistd.h>
#include <string>
#include <csignal>
#include <atomic>
#include <iomanip>
#include <ctime>
#include <TApplication.h>
#include "RootProducer.hh"

std::atomic<bool> g_prod_running(true);

void SigIntHandler(int /*signum*/) {
    std::cout << "\n\033[1;31m[SYSTEM:WARN] Interrupt signal (Ctrl+C) received. Saving ROOT file safely...\033[0m\n";
    g_prod_running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string input_file = "";
    std::string output_file = "";
    bool save_waveform = false; 
    bool display_mode = false;  

    int opt;
    while ((opt = getopt(argc, argv, "i:o:wd")) != -1) {
        switch (opt) {
            case 'i': input_file = optarg; break;
            case 'o': output_file = optarg; break;
            case 'w': save_waveform = true; break;
            case 'd': display_mode = true; break;
            default:
                std::cerr << "Usage: " << argv[0] << " [input] [-o output] [-w] [-d]\n";
                return 1;
        }
    }

    if (input_file.empty() && optind < argc) {
        input_file = argv[optind];
    }

    if (input_file.empty()) {
        std::cerr << "\033[1;31m[SYSTEM:ERROR] Input data file is required.\033[0m\n";
        std::cerr << "Usage: " << argv[0] << " <input_file.dat> [-o output.root] [-w] [-d]\n";
        return 1;
    }

    if (output_file.empty()) {
        size_t dot_pos = input_file.find_last_of('.');
        output_file = (dot_pos != std::string::npos) ? input_file.substr(0, dot_pos) + ".root" : input_file + ".root";
    }

    auto start_sys = std::chrono::system_clock::now();
    std::time_t start_time_t = std::chrono::system_clock::to_time_t(start_sys);
    auto start_steady = std::chrono::steady_clock::now();

    std::cout << "\n\033[1;36m============================================================\033[0m\n";
    std::cout << "\033[1;36m [ KFADC500 ROOT Physics Production ] \033[0m\n";
    std::cout << "\033[1;36m============================================================\033[0m\n";
    std::cout << "  - Input File  : " << input_file << "\n";
    std::cout << "  - Output File : " << output_file << "\n";
    std::cout << "  - Config Mode : " << (display_mode ? "\033[1;35mINTERACTIVE DISPLAY (No Saving)\033[0m" : "\033[1;32mBATCH PRODUCTION (ROOT TTree)\033[0m") << "\n";
    std::cout << "  - Waveform(-w): " << (save_waveform ? "ON (Heavy & Slow)" : "OFF (Fast Physics Only)") << "\n";
    std::cout << "  - Start Time  : " << std::ctime(&start_time_t);
    std::cout << "\033[1;36m============================================================\033[0m\n\n";

    RootProducer producer(input_file, output_file, save_waveform, display_mode);
    TApplication* app = nullptr;

    if (display_mode) {
        int temp_argc = 1; 
        char* temp_argv[] = { argv[0] };
        app = new TApplication("app", &temp_argc, temp_argv);
        producer.RunDisplayMode(g_prod_running);
    } else {
        producer.RunBatchMode(g_prod_running);
    }

    auto end_steady = std::chrono::steady_clock::now();
    auto end_sys = std::chrono::system_clock::now();
    std::time_t end_time_t = std::chrono::system_clock::to_time_t(end_sys);
    double elapsed_sec = std::chrono::duration<double>(end_steady - start_steady).count();

    // 💡 BATCH 모드일 때만 종합 성능 서머리 출력
    if (!display_mode) {
        int final_events = producer.GetTotalEvents();
        double final_mb = producer.GetTotalBytes() / (1024.0 * 1024.0);
        double speed_mb = (elapsed_sec > 0.0) ? final_mb / elapsed_sec : 0.0;
        double speed_evt = (elapsed_sec > 0.0) ? final_events / elapsed_sec : 0.0;

        std::cout << "\n\033[1;32m================ PRODUCTION SUMMARY ================\033[0m\n";
        std::cout << " End Time           : " << std::ctime(&end_time_t);
        std::cout << " Total Elapsed Time : \033[1;33m" << std::fixed << std::setprecision(2) << elapsed_sec << " sec\033[0m\n";
        std::cout << " Processed Events   : \033[1;36m" << final_events << " Events\033[0m\n";
        std::cout << " Processed Data     : \033[1;36m" << std::fixed << std::setprecision(2) << final_mb << " MB\033[0m\n";
        std::cout << " Conversion Speed   : \033[1;35m" << std::fixed << std::setprecision(1) << speed_mb << " MB/s (" << std::fixed << std::setprecision(0) << speed_evt << " Evt/s)\033[0m\n";
        std::cout << " Output ROOT File   : \033[1;36m" << output_file << "\033[0m\n";
        std::cout << "\033[1;32m====================================================\033[0m\n";
    } else {
        std::cout << "\n\033[1;33m[SYSTEM:INFO] Display Mode Closed.\033[0m\n";
    }

    return 0;
}