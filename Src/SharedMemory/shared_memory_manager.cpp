#include "shared_memory_manager.h"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace bitrpc {
namespace shared_memory {

// SharedMemoryMessage实现
std::atomic<uint32_t> SharedMemoryMessage::next_id_(1);

SharedMemoryMessage::SharedMemoryMessage() {
    std::memset(&header_, 0, sizeof(header_));
}

SharedMemoryMessage::SharedMemoryMessage(MessageType type, const void* data, size_t size) {
    header_.message_id = next_id_.fetch_add(1);
    header_.message_type = static_cast<uint32_t>(type);
    header_.payload_size = static_cast<uint32_t>(size);
    header_.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header_.flags = 0;

    if (data && size > 0) {
        payload_.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    }
}

void SharedMemoryMessage::set_payload(const void* data, size_t size) {
    header_.payload_size = static_cast<uint32_t>(size);
    if (data && size > 0) {
        payload_.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    } else {
        payload_.clear();
    }
}

std::vector<uint8_t> SharedMemoryMessage::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(sizeof(MessageHeader) + payload_.size());

    // 写入头部
    const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header_);
    data.insert(data.end(), header_ptr, header_ptr + sizeof(MessageHeader));

    // 写入负载
    data.insert(data.end(), payload_.begin(), payload_.end());

    return data;
}

bool SharedMemoryMessage::deserialize(const uint8_t* data, size_t size) {
    if (size < sizeof(MessageHeader)) {
        return false;
    }

    // 读取头部
    std::memcpy(&header_, data, sizeof(MessageHeader));

    // 验证头部
    if (header_.payload_size > size - sizeof(MessageHeader)) {
        return false;
    }

    // 读取负载
    payload_.assign(data + sizeof(MessageHeader), data + size);

    return true;
}

// SharedMemoryManager实现
SharedMemoryManager::SharedMemoryManager(const Config& config) : config_(config) {
}

SharedMemoryManager::~SharedMemoryManager() {
    stop();
}

bool SharedMemoryManager::start_producer() {
    if (running_) {
        return false;
    }

    // 创建环形缓冲区
    ring_buffer_ = std::make_unique<RingBuffer>(RingBuffer::Config(config_.instance_name));
    if (!ring_buffer_->create(RingBuffer::CreateMode::CREATE_OR_OPEN)) {
        return false;
    }

    running_ = true;
    is_producer_ = true;

    // 启动工作线程
    worker_thread_ = std::make_unique<std::thread>(&SharedMemoryManager::worker_thread, this);
    heartbeat_thread_ = std::make_unique<std::thread>(&SharedMemoryManager::heartbeat_thread, this);

    return true;
}

bool SharedMemoryManager::start_consumer() {
    if (running_) {
        return false;
    }

    // 创建环形缓冲区
    ring_buffer_ = std::make_unique<RingBuffer>(RingBuffer::Config(config_.instance_name));
    if (!ring_buffer_->create(RingBuffer::CreateMode::OPEN_ONLY)) {
        return false;
    }

    running_ = true;
    is_consumer_ = true;

    // 启动工作线程
    worker_thread_ = std::make_unique<std::thread>(&SharedMemoryManager::worker_thread, this);
    heartbeat_thread_ = std::make_unique<std::thread>(&SharedMemoryManager::heartbeat_thread, this);

    return true;
}

void SharedMemoryManager::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    heartbeat_active_ = false;

    // 停止环形缓冲区
    if (ring_buffer_) {
        ring_buffer_->close();
    }

    // 等待线程结束
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }

    ring_buffer_.reset();
    is_producer_ = false;
    is_consumer_ = false;
}

bool SharedMemoryManager::send_message(const SharedMemoryMessage& message) {
    if (!running_ || !ring_buffer_) {
        return false;
    }

    // 验证消息
    if (!validate_message(message)) {
        return false;
    }

    // 序列化消息
    auto serialized = serialize_message(message);
    if (serialized.empty()) {
        return false;
    }

    // 检查消息大小限制
    if (serialized.size() > config_.max_message_size) {
        return false;
    }

    // 写入环形缓冲区
    bool success = ring_buffer_->write(serialized.data(), serialized.size());
    if (success) {
        update_statistics(true, serialized.size());
    }

    return success;
}

bool SharedMemoryManager::send_message(MessageType type, const void* data, size_t size) {
    SharedMemoryMessage message(type, data, size);
    return send_message(message);
}

bool SharedMemoryManager::receive_message(SharedMemoryMessage& message, int timeout_ms) {
    if (!running_ || !ring_buffer_) {
        return false;
    }

    // 等待数据
    if (!ring_buffer_->wait_for_data(timeout_ms)) {
        return false;
    }

    // 读取数据
    std::vector<uint8_t> buffer(config_.max_message_size);
    size_t bytes_read = 0;

    if (!ring_buffer_->peek(buffer.data(), buffer.size(), bytes_read)) {
        return false;
    }

    if (bytes_read == 0) {
        return false;
    }

    // 反序列化消息
    if (!deserialize_message(buffer.data(), bytes_read, message)) {
        return false;
    }

    // 跳过已读取的数据
    ring_buffer_->skip(bytes_read);

    update_statistics(false, bytes_read);

    // 处理消息
    if (is_consumer_) {
        process_message(message);
    }

    return true;
}

bool SharedMemoryManager::peek_message(SharedMemoryMessage& message) const {
    if (!running_ || !ring_buffer_) {
        return false;
    }

    // 查看数据
    std::vector<uint8_t> buffer(config_.max_message_size);
    size_t bytes_read = 0;

    if (!ring_buffer_->peek(buffer.data(), buffer.size(), bytes_read)) {
        return false;
    }

    if (bytes_read == 0) {
        return false;
    }

    return deserialize_message(buffer.data(), bytes_read, message);
}

size_t SharedMemoryManager::send_messages(const std::vector<SharedMemoryMessage>& messages) {
    if (!running_ || !ring_buffer_ || messages.empty()) {
        return 0;
    }

    size_t sent_count = 0;
    for (const auto& message : messages) {
        if (send_message(message)) {
            sent_count++;
        } else {
            break;  // 发送失败，停止发送
        }
    }

    return sent_count;
}

size_t SharedMemoryManager::receive_messages(std::vector<SharedMemoryMessage>& messages, size_t max_count, int timeout_ms) {
    if (!running_ || !ring_buffer_ || max_count == 0) {
        return 0;
    }

    messages.clear();
    messages.reserve(max_count);

    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < max_count; ++i) {
        // 计算剩余超时时间
        int remaining_timeout = timeout_ms;
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            remaining_timeout = timeout_ms - static_cast<int>(elapsed);
            if (remaining_timeout <= 0) {
                break;
            }
        }

        SharedMemoryMessage message;
        if (receive_message(message, remaining_timeout)) {
            messages.push_back(std::move(message));
        } else {
            break;
        }
    }

    return messages.size();
}

void SharedMemoryManager::register_handler(MessageType type, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[type] = handler;
}

void SharedMemoryManager::unregister_handler(MessageType type) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_.erase(type);
}

size_t SharedMemoryManager::get_free_space() const {
    return ring_buffer_ ? ring_buffer_->get_free_space() : 0;
}

size_t SharedMemoryManager::get_used_space() const {
    return ring_buffer_ ? ring_buffer_->get_used_space() : 0;
}

bool SharedMemoryManager::clear_buffer() {
    // 清理缓冲区的实现较为复杂，需要双方协调
    // 这里简单实现为重新创建缓冲区
    if (!ring_buffer_) {
        return false;
    }

    ring_buffer_->close();
    ring_buffer_ = std::make_unique<RingBuffer>(RingBuffer::Config(config_.instance_name));
    return ring_buffer_->create(RingBuffer::CreateMode::CREATE_OR_OPEN);
}

bool SharedMemoryManager::send_heartbeat() {
    return send_message(MessageType::HEARTBEAT, nullptr, 0);
}

bool SharedMemoryManager::wait_for_heartbeat(int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        // 检查是否有最近的心跳
        if (last_heartbeat_.load() > 0) {
            auto last_heartbeat_time = std::chrono::milliseconds(last_heartbeat_.load());
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());
            auto elapsed = now - last_heartbeat_time;

            if (elapsed.count() < timeout_ms) {
                return true;
            }
        }

        // 检查超时
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed >= timeout_ms) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SharedMemoryManager::reset_statistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics{};
}

// 私有方法实现
void SharedMemoryManager::worker_thread() {
    if (is_producer_) {
        // 生产者工作线程主要负责监控和清理
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else if (is_consumer_) {
        // 消费者工作线程处理消息
        while (running_) {
            SharedMemoryMessage message;
            if (receive_message(message, 100)) {
                // 消息已在receive_message中处理
            }
        }
    }
}

void SharedMemoryManager::heartbeat_thread() {
    heartbeat_active_ = true;

    while (running_ && heartbeat_active_) {
        if (is_producer_) {
            send_heartbeat();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
    }
}

bool SharedMemoryManager::process_message(const SharedMemoryMessage& message) {
    // 更新心跳时间戳
    if (message.get_type() == MessageType::HEARTBEAT) {
        last_heartbeat_.store(message.get_timestamp());
        return true;
    }

    // 查找注册的处理器
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(message.get_type());
    if (it != handlers_.end()) {
        SharedMemoryMessage response;
        try {
            return it->second(message, response);
        } catch (const std::exception& e) {
            std::cerr << "Message handler error: " << e.what() << std::endl;
            return false;
        }
    }

    return true;  // 没有处理器也认为是成功的
}

void SharedMemoryManager::update_statistics(bool sent, size_t bytes) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (sent) {
        stats_.messages_sent++;
        stats_.bytes_sent += bytes;
    } else {
        stats_.messages_received++;
        stats_.bytes_received += bytes;
    }

    // 计算平均消息大小
    uint64_t total_messages = stats_.messages_sent + stats_.messages_received;
    if (total_messages > 0) {
        stats_.avg_message_size = static_cast<double>(stats_.bytes_sent + stats_.bytes_received) / total_messages;
    }

    // 更新缓冲区使用情况
    buffer_usage_.store(get_used_space());
}

bool SharedMemoryManager::validate_message(const SharedMemoryMessage& message) const {
    if (!message.is_valid()) {
        return false;
    }

    if (message.get_payload_size() > config_.max_message_size) {
        return false;
    }

    return true;
}

std::vector<uint8_t> SharedMemoryManager::serialize_message(const SharedMemoryMessage& message) const {
    return message.serialize();
}

bool SharedMemoryManager::deserialize_message(const uint8_t* data, size_t size, SharedMemoryMessage& message) const {
    return message.deserialize(data, size);
}

// SharedMemoryMultiInstanceManager实现
std::unordered_map<std::string, SharedMemoryMultiInstanceManager::InstanceManagerPtr>
    SharedMemoryMultiInstanceManager::instances_;
std::mutex SharedMemoryMultiInstanceManager::instances_mutex_;

bool SharedMemoryMultiInstanceManager::register_instance(const std::string& name, InstanceManagerPtr manager) {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    instances_[name] = manager;
    return true;
}

bool SharedMemoryMultiInstanceManager::unregister_instance(const std::string& name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    return instances_.erase(name) > 0;
}

SharedMemoryMultiInstanceManager::InstanceManagerPtr
SharedMemoryMultiInstanceManager::get_instance(const std::string& name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(name);
    return (it != instances_.end()) ? it->second : nullptr;
}

void SharedMemoryMultiInstanceManager::stop_all_instances() {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    for (auto& pair : instances_) {
        pair.second->stop();
    }
    instances_.clear();
}

std::vector<std::string> SharedMemoryMultiInstanceManager::get_instance_names() {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    std::vector<std::string> names;
    names.reserve(instances_.size());

    for (const auto& pair : instances_) {
        names.push_back(pair.first);
    }

    return names;
}

bool SharedMemoryMultiInstanceManager::is_instance_running(const std::string& name) {
    auto instance = get_instance(name);
    return instance && instance->is_running();
}

} // namespace shared_memory
} // namespace bitrpc