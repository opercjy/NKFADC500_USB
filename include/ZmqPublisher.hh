#pragma once
#include <thread>
#include <atomic>
#include <string>
#include <zmq.h>
#include "ObjectPool.hh"

class ZmqPublisher {
public:
    ZmqPublisher(const std::string& endpoint, DataQueue* data_queue);
    ~ZmqPublisher();

    void Start();
    void Stop();

private:
    void PublishLoop();
    static void FreeZmqMessage(void* data, void* hint);

    void* zmq_ctx_;
    void* zmq_pub_;
    std::string endpoint_;
    DataQueue* data_queue_;

    std::atomic<bool> is_running_;
    std::thread pub_thread_;
};