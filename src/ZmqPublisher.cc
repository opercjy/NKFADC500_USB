#include "ZmqPublisher.hh"
#include <iostream>

ZmqPublisher::ZmqPublisher(const std::string& endpoint, DataQueue* data_queue)
    : endpoint_(endpoint), data_queue_(data_queue), is_running_(false) {
    zmq_ctx_ = zmq_ctx_new();
    zmq_pub_ = zmq_socket(zmq_ctx_, ZMQ_PUB);
    
    // 라이브 모니터링 큐가 꽉 차서 시스템이 뻗는 것을 방지하는 HWM 설정
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
    if (is_running_) return;
    is_running_ = true;
    pub_thread_ = std::thread(&ZmqPublisher::PublishLoop, this);
    std::cout << "[ZMQ:INFO] Publisher Started on " << endpoint_ << "\n";
}

void ZmqPublisher::Stop() {
    is_running_ = false;
    if (data_queue_) data_queue_->WakeUpAll(); // Blocking 해제
    
    if (pub_thread_.joinable()) {
        pub_thread_.join();
        std::cout << "[ZMQ:INFO] Publisher Terminated.\n";
    }
}

void ZmqPublisher::PublishLoop() {
    while (is_running_) {
        // 워커가 채워준 큐에서 데이터를 하나 꺼냄
        DataBlock* block = data_queue_->Pop(is_running_);
        if (!block) continue;
        
        if (block->valid_size == 0) {
            block->origin_pool->Release(block);
            continue;
        }

        zmq_msg_t msg;
        // 💡 [Zero-Copy 핵심] 데이터를 복사하지 않고 포인터만 ZMQ에 넘김
        // 전송이 끝나거나 큐가 꽉 차서 버려질 때 FreeZmqMessage 콜백이 자동으로 호출됨
        zmq_msg_init_data(&msg, block->data, block->valid_size, FreeZmqMessage, block);
        int rc = zmq_msg_send(&msg, zmq_pub_, ZMQ_DONTWAIT);

        if (rc == -1) {
            // 클라이언트(Python GUI)가 죽었거나 큐가 꽉 찬 경우 강제 반환 처리
            zmq_msg_close(&msg); 
        }
    }
}

void ZmqPublisher::FreeZmqMessage(void* data, void* hint) {
    // 💡 ZMQ 전송이 완전히 끝난 후, 이 블록을 원래의 Object Pool로 되돌려줌
    DataBlock* block = static_cast<DataBlock*>(hint);
    if (block && block->origin_pool) {
        block->origin_pool->Release(block);
    }
}