#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <chrono>
#include <libusb-1.0/libusb.h>
#include "ObjectPool.hh"

class ReadDataWorker {
public:
    ReadDataWorker(libusb_device_handle* dev_handle, 
                   ObjectPool* pool, 
                   DataQueue* queue, 
                   const std::string& out_filename, 
                   int preset_events = 0, 
                   int preset_time = 0);
    ~ReadDataWorker();

    void Start();
    void Stop();
    bool IsRunning() const { return is_running_.load(); }

private:
    void AcquisitionLoop();
    void FileWriterLoop();
    void ExecuteSelfHealing(int usb_status);

    libusb_device_handle* dev_handle_;
    std::string outfile_;
    std::atomic<bool> is_running_;
    
    std::thread acq_thread_;
    std::thread write_thread_;

    ObjectPool* pool_;
    DataQueue* queue_;

    int preset_events_;
    int preset_time_;
    std::atomic<int> total_acquired_events_;
};