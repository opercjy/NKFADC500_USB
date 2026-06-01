#include "ZmqPublisher.hh"
#include "ObjectPool.hh"
#include <iostream>

extern LockFreePipeline g_pipeline;
extern std::atomic<bool> g_system_running;

ZmqPublisher::ZmqPublisher(const std::string& endpoint, DataQueue* /*legacy_queue*/)
    : endpoint_(endpoint), is_running_(false) {
    zmq_ctx_ = zmq_ctx_new();
    zmq_pub_ = zmq_socket(zmq_ctx_, ZMQ_PUB);
    
    // [Backpressure 제어] 수신자(GUI)가 느려 네트워크 버퍼가 포화될 경우,
    // 데이터를 버림(Drop)으로써 DAQ 코어의 메모리 고갈 및 데드락을 원천 차단.
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
    if (pub_thread_.joinable()) {
        pub_thread_.join();
    }
}

void ZmqPublisher::PublishLoop() {
    while (g_system_running.load(std::memory_order_acquire)) {
        
        // 1. Ready Queue에서 포인터 획득 (Wait-Free)
        EventBlock* ev = g_pipeline.AcquireForZmq();
        if (!ev) {
            CPU_RELAX();
            continue;
        }

        zmq_msg_t msg;
        // 2. [Zero-Copy] OS 버퍼 복사 없이 포인터를 직접 바인딩.
        // 전송이 완료되거나 파기될 때 내부적으로 FreeZmqMessage가 호출됨.
        zmq_msg_init_data(&msg, ev, sizeof(EventBlock), FreeZmqMessage, ev);

        int rc = zmq_msg_send(&msg, zmq_pub_, ZMQ_DONTWAIT);
        
        if (rc == -1) {
            // High Water Mark(HWM) 도달 또는 소켓 에러 시 메시지 파기 및 소유권 즉각 반환
            zmq_msg_close(&msg); 
        }
    }
}

// ZMQ C-API 하위 레벨 콜백 함수
void ZmqPublisher::FreeZmqMessage(void* /*data*/, void* hint) {
    EventBlock* ev = static_cast<EventBlock*>(hint);
    // [Ownership Transfer] 라이프사이클의 끝. 
    // ZMQ I/O가 끝났으므로 참조 카운터를 차감하고 조건 충족 시 Free 큐로 반환.
    g_pipeline.ReturnToFreeEvent(ev);
}
