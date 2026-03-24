#include "ZmqPublisher.hh"
#include <iostream>

// 💡 [핵심] ZMQ Zero-Copy 자동 반납 콜백 함수
// zmq_msg_send가 완전히 비동기로 끝난 직후 백그라운드에서 자동 호출됩니다.
void ZmqFreeCallback(void* data, void* hint) {
    DataBlock* block = static_cast<DataBlock*>(hint);
    if (block && block->origin_pool) {
        block->origin_pool->Release(block); // 런타임 new/delete 없이 고향 풀로 100% 반납!
    }
}

ZmqPublisher::ZmqPublisher(const std::string& bind_address, DataQueue* data_queue)
    : address_(bind_address), data_queue_(data_queue), is_running_(false) {
    
    zmq_context_ = zmq_ctx_new();
    zmq_socket_ = zmq_socket(zmq_context_, ZMQ_PUB);
    
    // 💡 Backpressure 제어 (HWM 설정)
    // 파이썬 GUI가 파형 렌더링하느라 느려질 경우, ZMQ 소켓 버퍼가 무한정 커지는 것을 방지.
    // 100개 이상 밀리면 오래된 패킷을 알아서 Drop 하여 C++ 메모리 폭발 방지 (Conflate의 기초)
    int hwm = 100;
    zmq_setsockopt(zmq_socket_, ZMQ_SNDHWM, &hwm, sizeof(hwm));
}

ZmqPublisher::~ZmqPublisher() {
    Stop();
    zmq_close(zmq_socket_);
    zmq_ctx_destroy(zmq_context_);
}

void ZmqPublisher::Start() {
    int rc = zmq_bind(zmq_socket_, address_.c_str());
    if (rc != 0) {
        std::cerr << "[ZMQ:FATAL] Failed to bind ZMQ socket on " << address_ << "\n";
        return;
    }
    
    is_running_ = true;
    pub_thread_ = std::thread(&ZmqPublisher::PublishLoop, this);
    std::cout << "[ZMQ:INFO] ZmqPublisher started successfully. Binding to " << address_ << "\n";
}

void ZmqPublisher::Stop() {
    is_running_ = false;
    // data_queue 블로킹 해제는 ReadDataWorker나 main에서 nullptr Push로 처리됨
    if (pub_thread_.joinable()) pub_thread_.join();
    std::cout << "[ZMQ:INFO] ZmqPublisher cleanly stopped.\n";
}

void ZmqPublisher::PublishLoop() {
    while (is_running_) {
        bool running_state = is_running_.load();
        DataBlock* block = data_queue_->Pop(running_state);
        
        if (!block) continue; // 종료 시그널
        
        if (block->valid_size == 0) {
            block->origin_pool->Release(block);
            continue;
        }

        zmq_msg_t msg;
        // 💡 [마법의 1줄] 직렬화(Serialization/Memcpy) 절대 금지! 
        // 물리적 메모리(block->data)를 그대로 바인딩하고, 완료 시 ZmqFreeCallback 트리거.
        zmq_msg_init_data(&msg, block->data, block->valid_size, ZmqFreeCallback, block);

        // ZMQ_DONTWAIT: 소켓이 꽉 차면 대기하지 않고 즉시 에러 리턴 (논블로킹)
        int rc = zmq_msg_send(&msg, zmq_socket_, ZMQ_DONTWAIT);
        
        if (rc == -1) {
            // HWM(High Water Mark) 도달 시 전송 실패 처리.
            // zmq_msg_close를 호출하면 전송하지 않더라도 ZmqFreeCallback이 즉시 발동하여 풀로 안전하게 반환됨.
            zmq_msg_close(&msg);
            std::cerr << "[ZMQ:WARN] Network bottleneck! Dropping packet to prevent OOM.\n";
        }
    }
}