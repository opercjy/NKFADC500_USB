#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <zmq.h>
#include "ObjectPool.hh"

class ZmqPublisher {
public:
    ZmqPublisher(const std::string& bind_address, DataQueue* data_queue);
    ~ZmqPublisher();

    void Start();
    void Stop();

private:
    void PublishLoop();

    std::string address_;
    DataQueue* data_queue_;
    std::atomic<bool> is_running_;
    std::thread pub_thread_;

    void* zmq_context_;
    void* zmq_socket_;
};