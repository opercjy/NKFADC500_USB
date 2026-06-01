#include "ZmqPublisher.hh"
#include "ObjectPool.hh"
#include <iostream>

extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;

ZmqPublisher::ZmqPublisher(const std::string& endpoint, void* /*dummy*/)
    : endpoint_(endpoint), is_running_(false) {
    zmq_ctx_ = zmq_ctx_new();
    zmq_pub_ = zmq_socket(zmq_ctx_, ZMQ_PUB);
    
    int hwm = 1000;
    zmq_setsockopt(zmq_pub_, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_bind(zmq_pub_, endpoint_.c_str());
}

ZmqPublisher::~ZmqPublisher() {
    Stop();
    zmq_close(zmq_pub_);
    zmq_ctx_destroy(zmq_ctx_);
}

void ZmqPublisher::Start() {
    if (is_running_.load(std::memory_order_acquire)) return;
    is_running_.store(true, std::memory_order_release);
    pub_thread_ = std::thread(&ZmqPublisher::PublishLoop, this);
}

void ZmqPublisher::Stop() {
    is_running_.store(false, std::memory_order_release);
    if (pub_thread_.joinable()) pub_thread_.join();
}

void ZmqPublisher::PublishLoop() {
    while (g_system_running.load(std::memory_order_acquire)) {
        
        EventBlock* ev = g_pipeline.AcquireForZmq();
        if (!ev) {
            CPU_RELAX();
            continue;
        }

        zmq_msg_t msg;
        // 💡 [버그 수정] 파이썬 GUI가 100% 동일하게 해석할 수 있도록,
        // EventBlock의 맨 앞부분(std::atomic 헤더)을 제외한 순수 payload 메모리만 전송합니다.
        zmq_msg_init_data(&msg, &ev->payload, sizeof(LiveMonitorPacket), FreeZmqMessage, ev);
        int rc = zmq_msg_send(&msg, zmq_pub_, ZMQ_DONTWAIT);

        if (rc == -1) {
            zmq_msg_close(&msg); 
        }
    }
}

void ZmqPublisher::FreeZmqMessage(void* /*data*/, void* hint) {
    EventBlock* ev = static_cast<EventBlock*>(hint);
    g_pipeline.ReturnToFreeEvent(ev);
}
