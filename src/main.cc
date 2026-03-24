#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

extern "C" {
#include "NoticeKFADC500USB.h"
#include "nkusb.h"
#include "usb3com.h"
}

#include "ConfigParser.hh"
#include "ObjectPool.hh"
#include "ZmqPublisher.hh"
#include "ReadDataWorker.hh"

std::atomic<bool> g_app_running(true);

void SigIntHandler(int /*signum*/) {
    std::cout << "\n[SYSTEM:WARN] Interrupt signal (Ctrl+C) received. Initiating graceful shutdown...\n";
    g_app_running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string config_file = "";
    std::string out_file = "kfadc500_data.dat";
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
            default:
                std::cerr << "Usage: " << argv[0] << " -f <config> [-o <out>] [-n <events>] [-t <seconds>]\n";
                return 1;
        }
    }

    if (config_file.empty()) {
        std::cerr << "[SYSTEM:FATAL] Config file is missing! Usage: -f <config.txt>\n";
        return 1;
    }

    KFADC500_Config config;
    if (!ConfigParser::Parse(config_file, config)) {
        std::cerr << "[SYSTEM:FATAL] Failed to parse config file: " << config_file << "\n";
        return 1;
    }
    
    auto t_start = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    std::cout << "\n\033[1;36m╔════════════════ RUN CONFIGURATION ════════════════╗\033[0m\n";
    std::cout << "\033[1;36m║\033[0m Start Time   : " << std::put_time(std::localtime(&t_start), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "\033[1;36m║\033[0m Config File  : " << config_file << "\n";
    std::cout << "\033[1;36m║\033[0m Target File  : " << out_file << "\n";
    std::cout << "\033[1;36m║\033[0m Target Limit : " << (preset_time > 0 ? std::to_string(preset_time) + " sec " : "") 
                                               << (preset_events > 0 ? std::to_string(preset_events) + " evts" : "Infinite") << "\n";
    std::cout << "\033[1;36m╚═══════════════════════════════════════════════════╝\033[0m\n";

    if (libusb_init(NULL) < 0) return 1;

    libusb_device_handle* devh = libusb_open_device_with_vid_pid(NULL, 0x04B4, 0x00F1);
    if (!devh) {
        std::cerr << "[SYSTEM:FATAL] Cannot open USB Device. Check permissions.\n";
        libusb_exit(NULL);
        return 1;
    }
    if (libusb_kernel_driver_active(devh, 0) == 1) libusb_detach_kernel_driver(devh, 0);
    libusb_claim_interface(devh, 0);

    NoticeKFADC500USB_reset(sid);
    NoticeKFADC500USB_write_RL(sid, config.record_length);
    NoticeKFADC500USB_write_TLT(sid, config.trigger_lut);
    NoticeKFADC500USB_write_AMODE(sid, config.filter);
    
    for (int ch = 0; ch < 4; ++ch) {
        NoticeKFADC500USB_write_THR(sid, ch+1, config.threshold[ch]);
        NoticeKFADC500USB_write_DACOFF(sid, ch+1, config.offset[ch]);
        NoticeKFADC500USB_write_DLY(sid, ch+1, config.delay[ch]);
        NoticeKFADC500USB_write_POL(sid, ch+1, config.polarity[ch]);
    }

    ObjectPool mem_pool(1000); 
    DataQueue data_queue;
    ZmqPublisher zmq_pub("tcp://*:5555", &data_queue);
    ReadDataWorker usb_worker(devh, &mem_pool, &data_queue, out_file, preset_events, preset_time);

    auto timer_start = std::chrono::steady_clock::now();

    zmq_pub.Start();
    usb_worker.Start();
    NoticeKFADC500USB_start(sid);

    while (g_app_running && usb_worker.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    NoticeKFADC500USB_stop(sid);
    usb_worker.Stop();
    zmq_pub.Stop();

    auto timer_end = std::chrono::steady_clock::now();
    double total_sec = std::chrono::duration<double>(timer_end - timer_start).count();

    std::cout << "\n\033[1;32m╔════════════════ ACQUISITION SUMMARY ══════════════╗\033[0m\n";
    std::cout << "\033[1;32m║\033[0m \033[1;37mTotal Elapsed Time : \033[1;33m" << std::fixed << std::setprecision(2) << total_sec << " sec\033[0m\n";
    std::cout << "\033[1;32m║\033[0m \033[1;37mData File Saved to : \033[1;36m" << out_file << "\033[0m\n";
    std::cout << "\033[1;32m╚═══════════════════════════════════════════════════╝\033[0m\n";

    libusb_release_interface(devh, 0);
    libusb_close(devh);
    libusb_exit(NULL);

    return 0;
}