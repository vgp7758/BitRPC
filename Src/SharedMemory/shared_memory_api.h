#pragma once

#include "shared_memory_manager.h"
#include <memory>
#include <string>
#include <vector>

namespace bitrpc {
namespace shared_memory {

// C风格API - 用于跨语言调用
extern "C" {
    // 环形缓冲区API
    typedef void* RingBufferHandle;

    RingBufferHandle RB_CreateProducer(const char* name, size_t buffer_size);
    RingBufferHandle RB_CreateConsumer(const char* name, size_t buffer_size);
    void RB_Close(RingBufferHandle handle);

    int RB_Write(RingBufferHandle handle, const void* data, size_t size);
    int RB_Read(RingBufferHandle handle, void* buffer, size_t buffer_size, size_t* bytes_read);
    int RB_GetFreeSpace(RingBufferHandle handle);
    int RB_GetUsedSpace(RingBufferHandle handle);
    int RB_IsConnected(RingBufferHandle handle);

    // 共享内存管理器API
    typedef void* SharedMemoryManagerHandle;

    SharedMemoryManagerHandle SMM_CreateProducer(const char* name, size_t buffer_size);
    SharedMemoryManagerHandle SMM_CreateConsumer(const char* name, size_t buffer_size);
    void SMM_Destroy(SharedMemoryManagerHandle handle);

    int SMM_SendMessage(SharedMemoryManagerHandle handle, int message_type, const void* data, size_t size);
    int SMM_ReceiveMessage(SharedMemoryManagerHandle handle, void* buffer, size_t buffer_size, size_t* bytes_read, int timeout_ms);
    int SMM_IsRunning(SharedMemoryManagerHandle handle);

    // 工具函数
    void RB_SetLastError(const char* error);
    const char* RB_GetLastError();
}

// C++封装类
class SharedMemoryProducer {
public:
    explicit SharedMemoryProducer(const std::string& name, size_t buffer_size = 1024 * 1024);
    ~SharedMemoryProducer();

    // 禁用拷贝
    SharedMemoryProducer(const SharedMemoryProducer&) = delete;
    SharedMemoryProducer& operator=(const SharedMemoryProducer&) = delete;

    // 基本操作
    bool connect();
    void disconnect();
    bool is_connected() const;

    // 数据发送
    bool send(const void* data, size_t size);
    bool send(const std::vector<uint8_t>& data);
    bool send_string(const std::string& str);

    // 消息发送
    bool send_message(const SharedMemoryMessage& message);
    bool send_message(MessageType type, const void* data, size_t size);

    // 批量操作
    size_t send_batch(const std::vector<std::vector<uint8_t>>& data_batch);
    size_t send_message_batch(const std::vector<SharedMemoryMessage>& messages);

    // 状态查询
    size_t get_free_space() const;
    size_t get_used_space() const;
    size_t get_capacity() const;
    bool is_empty() const;
    bool is_full() const;

    // 统计信息
    const SharedMemoryManager::Statistics& get_statistics() const;
    void reset_statistics();

    // 心跳
    bool send_heartbeat();

    // 错误处理
    std::string get_last_error() const;
    void clear_error();

private:
    std::string name_;
    size_t buffer_size_;
    std::unique_ptr<SharedMemoryManager> manager_;
    mutable std::string last_error_;

    void set_error(const std::string& error) const;
};

class SharedMemoryConsumer {
public:
    explicit SharedMemoryConsumer(const std::string& name, size_t buffer_size = 1024 * 1024);
    ~SharedMemoryConsumer();

    // 禁用拷贝
    SharedMemoryConsumer(const SharedMemoryConsumer&) = delete;
    SharedMemoryConsumer& operator=(const SharedMemoryConsumer&) = delete;

    // 基本操作
    bool connect();
    void disconnect();
    bool is_connected() const;

    // 数据接收
    bool receive(std::vector<uint8_t>& data, int timeout_ms = -1);
    bool receive_string(std::string& str, int timeout_ms = -1);
    bool peek(std::vector<uint8_t>& data) const;

    // 消息接收
    bool receive_message(SharedMemoryMessage& message, int timeout_ms = -1);
    bool peek_message(SharedMemoryMessage& message) const;

    // 批量操作
    size_t receive_batch(std::vector<std::vector<uint8_t>>& data_batch, size_t max_count, int timeout_ms = -1);
    size_t receive_message_batch(std::vector<SharedMemoryMessage>& messages, size_t max_count, int timeout_ms = -1);

    // 消息处理
    void register_handler(MessageType type, std::function<bool(const SharedMemoryMessage&)> handler);
    void unregister_handler(MessageType type);

    // 状态查询
    size_t get_free_space() const;
    size_t get_used_space() const;
    size_t get_capacity() const;
    bool is_empty() const;
    bool is_full() const;

    // 统计信息
    const SharedMemoryManager::Statistics& get_statistics() const;
    void reset_statistics();

    // 心跳和健康检查
    bool wait_for_heartbeat(int timeout_ms = 2000);
    uint64_t get_last_heartbeat_time() const;

    // 缓冲区管理
    bool clear_buffer();

    // 错误处理
    std::string get_last_error() const;
    void clear_error();

private:
    std::string name_;
    size_t buffer_size_;
    std::unique_ptr<SharedMemoryManager> manager_;
    mutable std::string last_error_;

    void set_error(const std::string& error) const;
};

// 高级封装 - 支持模板化数据类型
template<typename T>
class TypedSharedMemoryProducer {
public:
    TypedSharedMemoryProducer(const std::string& name, size_t buffer_size = 1024 * 1024)
        : producer_(name, buffer_size) {}

    bool connect() { return producer_.connect(); }
    void disconnect() { producer_.disconnect(); }
    bool is_connected() const { return producer_.is_connected(); }

    bool send_typed(const T& data) {
        return send_struct(&data, sizeof(T));
    }

    bool send_struct(const void* data, size_t size) {
        SharedMemoryMessage message(MessageType::DATA, data, size);
        return producer_.send_message(message);
    }

    bool send_vector(const std::vector<T>& vec) {
        return producer_.send(reinterpret_cast<const void*>(vec.data()), vec.size() * sizeof(T));
    }

    size_t get_free_space() const { return producer_.get_free_space(); }
    size_t get_used_space() const { return producer_.get_used_space(); }

private:
    SharedMemoryProducer producer_;
};

template<typename T>
class TypedSharedMemoryConsumer {
public:
    TypedSharedMemoryConsumer(const std::string& name, size_t buffer_size = 1024 * 1024)
        : consumer_(name, buffer_size) {}

    bool connect() { return consumer_.connect(); }
    void disconnect() { consumer_.disconnect(); }
    bool is_connected() const { return consumer_.is_connected(); }

    bool receive_typed(T& data, int timeout_ms = -1) {
        std::vector<uint8_t> buffer(sizeof(T));
        if (consumer_.receive(buffer, timeout_ms)) {
            if (buffer.size() == sizeof(T)) {
                std::memcpy(&data, buffer.data(), sizeof(T));
                return true;
            }
        }
        return false;
    }

    bool receive_vector(std::vector<T>& vec, int timeout_ms = -1) {
        std::vector<uint8_t> buffer;
        if (consumer_.receive(buffer, timeout_ms)) {
            if (buffer.size() % sizeof(T) == 0) {
                vec.resize(buffer.size() / sizeof(T));
                std::memcpy(vec.data(), buffer.data(), buffer.size());
                return true;
            }
        }
        return false;
    }

    size_t get_free_space() const { return consumer_.get_free_space(); }
    size_t get_used_space() const { return consumer_.get_used_space(); }

private:
    SharedMemoryConsumer consumer_;
};

// 便捷工厂函数
inline std::unique_ptr<SharedMemoryProducer> create_producer(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto producer = std::make_unique<SharedMemoryProducer>(name, buffer_size);
    if (producer->connect()) {
        return producer;
    }
    return nullptr;
}

inline std::unique_ptr<SharedMemoryConsumer> create_consumer(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto consumer = std::make_unique<SharedMemoryConsumer>(name, buffer_size);
    if (consumer->connect()) {
        return consumer;
    }
    return nullptr;
}

template<typename T>
inline std::unique_ptr<TypedSharedMemoryProducer<T>> create_typed_producer(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto producer = std::make_unique<TypedSharedMemoryProducer<T>>(name, buffer_size);
    if (producer->connect()) {
        return producer;
    }
    return nullptr;
}

template<typename T>
inline std::unique_ptr<TypedSharedMemoryConsumer<T>> create_typed_consumer(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto consumer = std::make_unique<TypedSharedMemoryConsumer<T>>(name, buffer_size);
    if (consumer->connect()) {
        return consumer;
    }
    return nullptr;
}

// 全局管理函数
void cleanup_all_shared_memory();
std::vector<std::string> get_active_shared_memory_instances();

} // namespace shared_memory
} // namespace bitrpc