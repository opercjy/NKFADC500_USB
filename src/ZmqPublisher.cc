#include "ZmqPublisher.hh"
#include <iostream>

ZmqPublisher::ZmqPublisher(const std::string& endpoint, DataQueue* data_queue)
    : endpoint_(endpoint), data_queue_(data_queue), is_running_(false) {
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
    is_running_ = true;
    pub_thread_ = std::thread(&ZmqPublisher::PublishLoop, this);
    std::cout << "[ZMQ:INFO] Publisher Started on " << endpoint_ << "\n";
}

void ZmqPublisher::Stop() {
    is_running_ = false;
    if (data_queue_) data_queue_->WakeUpAll();
    
    // 💡 중복 실행 방어
    if (pub_thread_.joinable()) {
        pub_thread_.join();
        std::cout << "[ZMQ:INFO] Publisher Terminated.\n";
    }
}

void ZmqPublisher::PublishLoop() {
    while (is_running_) {
        DataBlock* block = data_queue_->Pop(is_running_);
        if (!block || block->valid_size == 0) {
            if (block) block->origin_pool->Release(block);
            continue;
        }

        zmq_msg_t msg;
        zmq_msg_init_data(&msg, block->data, block->valid_size, FreeZmqMessage, block);
        int rc = zmq_msg_send(&msg, zmq_pub_, ZMQ_DONTWAIT);

        if (rc == -1) {
            zmq_msg_close(&msg);
        }
    }
}

void ZmqPublisher::FreeZmqMessage(void* data, void* hint) {
    DataBlock* block = static_cast<DataBlock*>(hint);
    if (block && block->origin_pool) {
        block->origin_pool->Release(block);
    }
}