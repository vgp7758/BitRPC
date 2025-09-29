#include "shared_memory_api.h"
#include <cstring>
#include <mutex>

namespace bitrpc {
namespace shared_memory {

// 全局错误状态
static std::string last_error;
static std::mutex error_mutex;

void RB_SetLastError(const char* error) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = error ? error : "";
}

const char* RB_GetLastError() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error.c_str();
}

// C风格API实现
RingBufferHandle RB_CreateProducer(const char* name, size_t buffer_size) {
    if (!name) {
        RB_SetLastError("Invalid name parameter");
        return nullptr;
    }

    try {
        auto buffer = std::make_unique<RingBuffer>(RingBuffer::Config(name));
        if (!buffer->create(RingBuffer::CreateMode::CREATE_OR_OPEN)) {
            RB_SetLastError("Failed to create ring buffer");
            return nullptr;
        }
        return buffer.release();
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return nullptr;
    }
}

RingBufferHandle RB_CreateConsumer(const char* name, size_t buffer_size) {
    if (!name) {
        RB_SetLastError("Invalid name parameter");
        return nullptr;
    }

    try {
        auto buffer = std::make_unique<RingBuffer>(RingBuffer::Config(name));
        if (!buffer->create(RingBuffer::CreateMode::OPEN_ONLY)) {
            RB_SetLastError("Failed to open ring buffer");
            return nullptr;
        }
        return buffer.release();
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return nullptr;
    }
}

void RB_Close(RingBufferHandle handle) {
    if (handle) {
        auto* buffer = static_cast<RingBuffer*>(handle);
        delete buffer;
    }
}

int RB_Write(RingBufferHandle handle, const void* data, size_t size) {
    if (!handle || !data || size == 0) {
        RB_SetLastError("Invalid parameters");
        return 0;
    }

    try {
        auto* buffer = static_cast<RingBuffer*>(handle);
        return buffer->write(data, size) ? 1 : 0;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

int RB_Read(RingBufferHandle handle, void* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!handle || !buffer || buffer_size == 0 || !bytes_read) {
        RB_SetLastError("Invalid parameters");
        return 0;
    }

    try {
        auto* ring_buffer = static_cast<RingBuffer*>(handle);
        size_t read = 0;
        bool success = ring_buffer->read(buffer, buffer_size, read);
        *bytes_read = read;
        return success ? 1 : 0;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

int RB_GetFreeSpace(RingBufferHandle handle) {
    if (!handle) {
        RB_SetLastError("Invalid handle");
        return -1;
    }

    try {
        auto* buffer = static_cast<RingBuffer*>(handle);
        return static_cast<int>(buffer->get_free_space());
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return -1;
    }
}

int RB_GetUsedSpace(RingBufferHandle handle) {
    if (!handle) {
        RB_SetLastError("Invalid handle");
        return -1;
    }

    try {
        auto* buffer = static_cast<RingBuffer*>(handle);
        return static_cast<int>(buffer->get_used_space());
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return -1;
    }
}

int RB_IsConnected(RingBufferHandle handle) {
    if (!handle) {
        return 0;
    }

    try {
        auto* buffer = static_cast<RingBuffer*>(handle);
        return buffer->is_connected() ? 1 : 0;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

// 共享内存管理器C API
SharedMemoryManagerHandle SMM_CreateProducer(const char* name, size_t buffer_size) {
    if (!name) {
        RB_SetLastError("Invalid name parameter");
        return nullptr;
    }

    try {
        auto config = SharedMemoryManager::Config(name);
        config.buffer_size = buffer_size;
        auto manager = std::make_unique<SharedMemoryManager>(config);
        if (!manager->start_producer()) {
            RB_SetLastError("Failed to start producer");
            return nullptr;
        }
        return manager.release();
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return nullptr;
    }
}

SharedMemoryManagerHandle SMM_CreateConsumer(const char* name, size_t buffer_size) {
    if (!name) {
        RB_SetLastError("Invalid name parameter");
        return nullptr;
    }

    try {
        auto config = SharedMemoryManager::Config(name);
        config.buffer_size = buffer_size;
        auto manager = std::make_unique<SharedMemoryManager>(config);
        if (!manager->start_consumer()) {
            RB_SetLastError("Failed to start consumer");
            return nullptr;
        }
        return manager.release();
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return nullptr;
    }
}

void SMM_Destroy(SharedMemoryManagerHandle handle) {
    if (handle) {
        auto* manager = static_cast<SharedMemoryManager*>(handle);
        manager->stop();
        delete manager;
    }
}

int SMM_SendMessage(SharedMemoryManagerHandle handle, int message_type, const void* data, size_t size) {
    if (!handle || message_type <= 0) {
        RB_SetLastError("Invalid parameters");
        return 0;
    }

    try {
        auto* manager = static_cast<SharedMemoryManager*>(handle);
        return manager->send_message(static_cast<MessageType>(message_type), data, size) ? 1 : 0;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

int SMM_ReceiveMessage(SharedMemoryManagerHandle handle, void* buffer, size_t buffer_size, size_t* bytes_read, int timeout_ms) {
    if (!handle || !buffer || buffer_size == 0 || !bytes_read) {
        RB_SetLastError("Invalid parameters");
        return 0;
    }

    try {
        auto* manager = static_cast<SharedMemoryManager*>(handle);
        SharedMemoryMessage message;
        if (!manager->receive_message(message, timeout_ms)) {
            return 0;
        }

        size_t message_size = message.get_payload_size();
        if (message_size > buffer_size) {
            RB_SetLastError("Buffer too small");
            return 0;
        }

        std::memcpy(buffer, message.get_payload(), message_size);
        *bytes_read = message_size;
        return 1;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

int SMM_IsRunning(SharedMemoryManagerHandle handle) {
    if (!handle) {
        return 0;
    }

    try {
        auto* manager = static_cast<SharedMemoryManager*>(handle);
        return manager->is_running() ? 1 : 0;
    } catch (const std::exception& e) {
        RB_SetLastError(e.what());
        return 0;
    }
}

// C++封装类实现
SharedMemoryProducer::SharedMemoryProducer(const std::string& name, size_t buffer_size)
    : name_(name), buffer_size_(buffer_size) {
}

SharedMemoryProducer::~SharedMemoryProducer() {
    disconnect();
}

bool SharedMemoryProducer::connect() {
    if (manager_) {
        return true;
    }

    try {
        auto config = SharedMemoryManager::Config(name_);
        config.buffer_size = buffer_size_;
        manager_ = std::make_unique<SharedMemoryManager>(config);

        if (!manager_->start_producer()) {
            set_error("Failed to start producer");
            return false;
        }

        clear_error();
        return true;
    } catch (const std::exception& e) {
        set_error(e.what());
        return false;
    }
}

void SharedMemoryProducer::disconnect() {
    if (manager_) {
        manager_->stop();
        manager_.reset();
    }
}

bool SharedMemoryProducer::is_connected() const {
    return manager_ && manager_->is_running();
}

bool SharedMemoryProducer::send(const void* data, size_t size) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->send_message(MessageType::DATA, data, size);
}

bool SharedMemoryProducer::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

bool SharedMemoryProducer::send_string(const std::string& str) {
    return send(str.data(), str.size());
}

bool SharedMemoryProducer::send_message(const SharedMemoryMessage& message) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->send_message(message);
}

bool SharedMemoryProducer::send_message(MessageType type, const void* data, size_t size) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->send_message(type, data, size);
}

size_t SharedMemoryProducer::send_batch(const std::vector<std::vector<uint8_t>>& data_batch) {
    if (!is_connected()) {
        set_error("Not connected");
        return 0;
    }

    size_t sent_count = 0;
    for (const auto& data : data_batch) {
        if (send(data)) {
            sent_count++;
        } else {
            break;
        }
    }

    return sent_count;
}

size_t SharedMemoryProducer::send_message_batch(const std::vector<SharedMemoryMessage>& messages) {
    if (!is_connected()) {
        set_error("Not connected");
        return 0;
    }

    return manager_->send_messages(messages);
}

size_t SharedMemoryProducer::get_free_space() const {
    return is_connected() ? manager_->get_free_space() : 0;
}

size_t SharedMemoryProducer::get_used_space() const {
    return is_connected() ? manager_->get_used_space() : 0;
}

size_t SharedMemoryProducer::get_capacity() const {
    return buffer_size_;
}

bool SharedMemoryProducer::is_empty() const {
    return get_used_space() == 0;
}

bool SharedMemoryProducer::is_full() const {
    return get_free_space() == 0;
}

const SharedMemoryManager::Statistics& SharedMemoryProducer::get_statistics() const {
    static SharedMemoryManager::Statistics empty_stats;
    return is_connected() ? manager_->get_statistics() : empty_stats;
}

void SharedMemoryProducer::reset_statistics() {
    if (is_connected()) {
        manager_->reset_statistics();
    }
}

bool SharedMemoryProducer::send_heartbeat() {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->send_heartbeat();
}

std::string SharedMemoryProducer::get_last_error() const {
    return last_error_;
}

void SharedMemoryProducer::clear_error() {
    last_error_.clear();
}

void SharedMemoryProducer::set_error(const std::string& error) const {
    last_error_ = error;
}

// SharedMemoryConsumer实现
SharedMemoryConsumer::SharedMemoryConsumer(const std::string& name, size_t buffer_size)
    : name_(name), buffer_size_(buffer_size) {
}

SharedMemoryConsumer::~SharedMemoryConsumer() {
    disconnect();
}

bool SharedMemoryConsumer::connect() {
    if (manager_) {
        return true;
    }

    try {
        auto config = SharedMemoryManager::Config(name_);
        config.buffer_size = buffer_size_;
        manager_ = std::make_unique<SharedMemoryManager>(config);

        if (!manager_->start_consumer()) {
            set_error("Failed to start consumer");
            return false;
        }

        clear_error();
        return true;
    } catch (const std::exception& e) {
        set_error(e.what());
        return false;
    }
}

void SharedMemoryConsumer::disconnect() {
    if (manager_) {
        manager_->stop();
        manager_.reset();
    }
}

bool SharedMemoryConsumer::is_connected() const {
    return manager_ && manager_->is_running();
}

bool SharedMemoryConsumer::receive(std::vector<uint8_t>& data, int timeout_ms) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    SharedMemoryMessage message;
    if (manager_->receive_message(message, timeout_ms)) {
        data.assign(message.get_payload(), message.get_payload() + message.get_payload_size());
        return true;
    }

    return false;
}

bool SharedMemoryConsumer::receive_string(std::string& str, int timeout_ms) {
    std::vector<uint8_t> data;
    if (receive(data, timeout_ms)) {
        str.assign(data.begin(), data.end());
        return true;
    }
    return false;
}

bool SharedMemoryConsumer::peek(std::vector<uint8_t>& data) const {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    SharedMemoryMessage message;
    if (manager_->peek_message(message)) {
        data.assign(message.get_payload(), message.get_payload() + message.get_payload_size());
        return true;
    }

    return false;
}

bool SharedMemoryConsumer::receive_message(SharedMemoryMessage& message, int timeout_ms) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->receive_message(message, timeout_ms);
}

bool SharedMemoryConsumer::peek_message(SharedMemoryMessage& message) const {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->peek_message(message);
}

size_t SharedMemoryConsumer::receive_batch(std::vector<std::vector<uint8_t>>& data_batch, size_t max_count, int timeout_ms) {
    if (!is_connected()) {
        set_error("Not connected");
        return 0;
    }

    std::vector<SharedMemoryMessage> messages;
    size_t count = manager_->receive_messages(messages, max_count, timeout_ms);

    data_batch.reserve(count);
    for (const auto& message : messages) {
        data_batch.emplace_back(message.get_payload(), message.get_payload() + message.get_payload_size());
    }

    return count;
}

size_t SharedMemoryConsumer::receive_message_batch(std::vector<SharedMemoryMessage>& messages, size_t max_count, int timeout_ms) {
    if (!is_connected()) {
        set_error("Not connected");
        return 0;
    }

    return manager_->receive_messages(messages, max_count, timeout_ms);
}

void SharedMemoryConsumer::register_handler(MessageType type, std::function<bool(const SharedMemoryMessage&)> handler) {
    if (is_connected()) {
        auto wrapped_handler = [handler](const SharedMemoryMessage& msg, SharedMemoryMessage& response) -> bool {
            return handler(msg);
        };
        manager_->register_handler(type, wrapped_handler);
    }
}

void SharedMemoryConsumer::unregister_handler(MessageType type) {
    if (is_connected()) {
        manager_->unregister_handler(type);
    }
}

size_t SharedMemoryConsumer::get_free_space() const {
    return is_connected() ? manager_->get_free_space() : 0;
}

size_t SharedMemoryConsumer::get_used_space() const {
    return is_connected() ? manager_->get_used_space() : 0;
}

size_t SharedMemoryConsumer::get_capacity() const {
    return buffer_size_;
}

bool SharedMemoryConsumer::is_empty() const {
    return get_used_space() == 0;
}

bool SharedMemoryConsumer::is_full() const {
    return get_free_space() == 0;
}

const SharedMemoryManager::Statistics& SharedMemoryConsumer::get_statistics() const {
    static SharedMemoryManager::Statistics empty_stats;
    return is_connected() ? manager_->get_statistics() : empty_stats;
}

void SharedMemoryConsumer::reset_statistics() {
    if (is_connected()) {
        manager_->reset_statistics();
    }
}

bool SharedMemoryConsumer::wait_for_heartbeat(int timeout_ms) {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->wait_for_heartbeat(timeout_ms);
}

uint64_t SharedMemoryConsumer::get_last_heartbeat_time() const {
    return 0; // 简化实现
}

bool SharedMemoryConsumer::clear_buffer() {
    if (!is_connected()) {
        set_error("Not connected");
        return false;
    }

    return manager_->clear_buffer();
}

std::string SharedMemoryConsumer::get_last_error() const {
    return last_error_;
}

void SharedMemoryConsumer::clear_error() {
    last_error_.clear();
}

void SharedMemoryConsumer::set_error(const std::string& error) const {
    last_error_ = error;
}

// 全局管理函数
void cleanup_all_shared_memory() {
    SharedMemoryMultiInstanceManager::stop_all_instances();
}

std::vector<std::string> get_active_shared_memory_instances() {
    return SharedMemoryMultiInstanceManager::get_instance_names();
}

} // namespace shared_memory
} // namespace bitrpc