#pragma once
#include <thread>
#include <atomic>
#include <string>
#include <zmq.h>

class ZmqPublisher {
public:
    // 💡 [패치] 레거시 큐 포인터 인자를 무시(void*)하도록 수정
    ZmqPublisher(const std::string& endpoint, void* dummy);
    ~ZmqPublisher();

    void Start();
    void Stop();

private:
    void PublishLoop();
    
    static void FreeZmqMessage(void* data, void* hint);

    void* zmq_ctx_;
    void* zmq_pub_;
    std::string endpoint_;

    std::atomic<bool> is_running_;
    std::thread pub_thread_;
};
