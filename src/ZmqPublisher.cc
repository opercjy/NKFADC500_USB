#include "ZmqPublisher.hh"
#include <iostream>

extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;

ZmqPublisher::ZmqPublisher(const std::string& endpoint, DataQueue* /*legacy*/)
    : endpoint_(endpoint), is_running_(false) {
    zmq_ctx_ = zmq_ctx_new();
    zmq_pub_ = zmq_socket(zmq_ctx_, ZMQ_PUB);
    
    // 라이브 모니터링 큐가 꽉 차서 시스템이 뻗는 것을 방지하는 HWM 설정
    // ZMQ 내부 버퍼 포화 시 이벤트를 버림(Drop)으로써 생산자(DAQ) 백프레셔 방어
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
    std::cout << "[ZMQ:INFO] Zero-Copy Publisher Started on " << endpoint_ << "\n";
}

void ZmqPublisher::Stop() {
    is_running_.store(false, std::memory_order_release);
    if (pub_thread_.joinable()) {
        pub_thread_.join();
        std::cout << "[ZMQ:INFO] Publisher Terminated.\n";
    }
}

void ZmqPublisher::PublishLoop() {
    while (g_system_running.load(std::memory_order_acquire)) {
        
        // 1. Ready Queue에서 포인터 획득 (Wait-free)
        EventBlock* ev = g_pipeline.AcquireForZmq();
        if (!ev) {
            CPU_RELAX();
            continue;
        }

        zmq_msg_t msg;
        // 2. [Zero-Copy] 데이터 복사 없이 포인터를 ZMQ로 직접 넘김
        // 전송이 끝나거나 파기될 때 FreeZmqMessage 콜백이 자동 호출됨
        zmq_msg_init_data(&msg, ev, sizeof(EventBlock), FreeZmqMessage, ev);
        
        int rc = zmq_msg_send(&msg, zmq_pub_, ZMQ_DONTWAIT);

        if (rc == -1) {
            // HWM 도달 시 메시지 강제 파기(Drop) 및 콜백 트리거
            zmq_msg_close(&msg); 
        }
    }
}

// ZMQ 내부 콜백 - 전송 라이프사이클 종료 시 호출됨
void ZmqPublisher::FreeZmqMessage(void* /*data*/, void* hint) {
    EventBlock* ev = static_cast<EventBlock*>(hint);
    // 소유권을 차감하고 0이 되면 Free 큐로 반환 (RootProducer와 경쟁 없음)
    g_pipeline.ReturnToFreeEvent(ev);
}
