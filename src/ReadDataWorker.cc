#include "ReadDataWorker.hh"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cstring>

constexpr int EP_BULK_IN = 0x82;
constexpr unsigned int USB_TIMEOUT_MS = 100;

ReadDataWorker::ReadDataWorker(libusb_device_handle* dev_handle, ObjectPool* pool, DataQueue* queue, 
                               const std::string& out_filename, int preset_events, int preset_time)
    : dev_handle_(dev_handle), pool_(pool), queue_(queue), outfile_(out_filename), 
      is_running_(false), preset_events_(preset_events), preset_time_(preset_time), total_acquired_events_(0) {}

ReadDataWorker::~ReadDataWorker() {
    Stop();
}

void ReadDataWorker::Start() {
    is_running_ = true;
    write_thread_ = std::thread(&ReadDataWorker::FileWriterLoop, this);
    acq_thread_ = std::thread(&ReadDataWorker::AcquisitionLoop, this);
    std::cout << "[DAQ:INFO] Acquisition & Writer Threads Started.\n";
}

void ReadDataWorker::Stop() {
    is_running_ = false;
    if (pool_) pool_->Release(nullptr); 
    if (queue_) queue_->Push(nullptr);
    
    if (acq_thread_.joinable()) acq_thread_.join();
    if (write_thread_.joinable()) write_thread_.join();
    std::cout << "[DAQ:INFO] Worker Threads Safely Terminated.\n";
}

void ReadDataWorker::AcquisitionLoop() {
    uint8_t residual_buffer[EVENT_ALIGNMENT] = {0};
    size_t residual_size = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto last_print_time = start_time;
    int last_events = 0;

    std::cout << "\n\033[1;32m[SYSTEM] DAQ Pipeline Activated. Starting Data Acquisition...\033[0m\n";

    while (is_running_) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
        
        if (preset_time_ > 0 && elapsed_sec >= preset_time_) {
            std::cout << "\n\033[1;33m[DAQ:INFO] Preset Time Limit Reached (" << preset_time_ << "s). Auto-Stopping...\033[0m\n";
            is_running_ = false;
            break;
        }

        double print_dt = std::chrono::duration<double>(now - last_print_time).count();
        if (print_dt >= 0.5) {
            int current_events = total_acquired_events_.load();
            double rate = (current_events - last_events) / print_dt;
            
            printf("\r\033[1;36m[MONITOR]\033[0m \033[1;33mTime: %5.1fs\033[0m | \033[1;32mEvents: %-8d\033[0m | \033[1;35mRate: %-8.1f Hz\033[0m | Q: %-3zu", 
                   elapsed_sec, current_events, rate, queue_->Size());
            fflush(stdout);

            last_events = current_events;
            last_print_time = now;
        }

        bool running_state = is_running_.load();
        DataBlock* block = pool_->Acquire(running_state);
        if (!block) continue;

        if (residual_size > 0) std::memcpy(block->data, residual_buffer, residual_size);

        int transferred = 0;
        int status = libusb_bulk_transfer(dev_handle_, EP_BULK_IN, block->data + residual_size, BULK_READ_SIZE - residual_size, &transferred, USB_TIMEOUT_MS);

        if (status == LIBUSB_SUCCESS || (status == LIBUSB_ERROR_TIMEOUT && transferred > 0)) {
            size_t total_bytes = residual_size + transferred;
            size_t valid_bytes = (total_bytes / EVENT_ALIGNMENT) * EVENT_ALIGNMENT;
            size_t torn_bytes = total_bytes - valid_bytes;

            if (torn_bytes > 0) std::memcpy(residual_buffer, block->data + valid_bytes, torn_bytes);
            residual_size = torn_bytes;

            if (valid_bytes > 0) {
                block->valid_size = valid_bytes;
                queue_->Push(block); 

                int events_in_this_block = valid_bytes / EVENT_ALIGNMENT;
                total_acquired_events_ += events_in_this_block;
                
                if (preset_events_ > 0 && total_acquired_events_ >= preset_events_) {
                    std::cout << "\n\033[1;33m[DAQ:INFO] Preset Event Limit Reached (" << preset_events_ << "). Auto-Stopping...\033[0m\n";
                    is_running_ = false;
                }
            } else {
                pool_->Release(block);
            }
        } 
        else if (status == LIBUSB_ERROR_TIMEOUT && transferred == 0) {
            pool_->Release(block); 
        } 
        else {
            pool_->Release(block);
            ExecuteSelfHealing(status);
        }
    }
}

void ReadDataWorker::FileWriterLoop() {
    std::ofstream fout(outfile_, std::ios::out | std::ios::binary);
    if (!fout.is_open()) {
        std::cerr << "[DAQ:ERROR] Failed to open output file: " << outfile_ << "\n";
        return;
    }

    size_t total_written = 0;
    while (is_running_ || queue_->Size() > 0) {
        bool running_state = is_running_.load();
        DataBlock* block = queue_->Pop(running_state);
        
        if (block && block->valid_size > 0) {
            fout.write(reinterpret_cast<char*>(block->data), block->valid_size);
            total_written += block->valid_size;
            pool_->Release(block); 
        }
    }
    
    fout.close();
    std::cout << "\n[DAQ:INFO] File closed. Total Written: " << (total_written / 1024 / 1024) << " MB\n";
}

void ReadDataWorker::ExecuteSelfHealing(int usb_status) {
    if (usb_status == LIBUSB_ERROR_NO_DEVICE) {
        std::cerr << "\n[FATAL] Device Disconnected!\n";
        is_running_ = false;
        return;
    }
    if (usb_status == LIBUSB_ERROR_PIPE) {
        std::cerr << "\n[RECOVERY] Clearing Endpoint Halt...\n";
        libusb_clear_halt(dev_handle_, EP_BULK_IN);
    }
}