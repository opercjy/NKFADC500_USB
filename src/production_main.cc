#include <iostream>
#include <chrono>
#include <unistd.h>
#include <string>
#include <TApplication.h>
#include "RootProducer.hh"

int main(int argc, char** argv) {
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

    std::cout << "\n\033[1;36m================================================\033[0m\n";
    std::cout << "\033[1;36m [ KFADC500 ROOT Physics Production ] \033[0m\n";
    std::cout << "\033[1;36m================================================\033[0m\n";
    std::cout << "  - Input       : " << input_file << "\n";
    std::cout << "  - Output      : " << output_file << "\n";
    std::cout << "  - Mode        : " << (display_mode ? "\033[1;35mINTERACTIVE DISPLAY (No Saving)\033[0m" : "\033[1;32mBATCH PRODUCTION (ROOT TTree)\033[0m") << "\n";
    std::cout << "  - Waveform(-w): " << (save_waveform ? "ON" : "OFF") << "\n";
    std::cout << "================================================\n\n";

    auto start_time = std::chrono::steady_clock::now();

    RootProducer producer(input_file, output_file, save_waveform, display_mode);

    // 💡 패치: TApplication을 포인터(Heap)로 생성합니다.
    TApplication* app = nullptr;

    if (display_mode) {
        int temp_argc = 1; 
        char* temp_argv[] = { argv[0] };
        app = new TApplication("app", &temp_argc, temp_argv);
        producer.RunDisplayMode();
    } else {
        producer.RunBatchMode();
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "\033[1;33m[SYSTEM:INFO] Process Completed in " << elapsed << " seconds.\033[0m\n\n";

    // 💡 주의: delete app; 를 절대 호출하지 마십시오! 
    // ROOT 프레임워크는 프로세스 종료 시점에 OS가 자원을 회수하도록 내버려두는 것이 가장 안전합니다.
    return 0;
}