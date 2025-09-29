#pragma once

#include "ring_buffer.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <string>

namespace bitrpc {
namespace shared_memory {

// 消息头结构
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t message_id{0};     // 消息ID
    uint32_t message_type{0};   // 消息类型
    uint32_t payload_size{0};   // 负载大小
    uint64_t timestamp{0};      // 时间戳
    uint8_t flags{0};           // 标志位
    uint8_t reserved[3];        // 保留
};
#pragma pack(pop)

// 消息类型枚举
enum class MessageType : uint32_t {
    DATA = 1,
    CONTROL = 2,
    HEARTBEAT = 3,
    ERROR = 4,
    CUSTOM_MIN = 1000
};

// 消息标志位
enum class MessageFlags : uint8_t {
    NONE = 0,
    URGENT = 0x01,
    COMPRESSED = 0x02,
    ENCRYPTED = 0x04,
    LAST_FRAGMENT = 0x08
};

// 共享内存消息
class SharedMemoryMessage {
public:
    SharedMemoryMessage();
    SharedMemoryMessage(MessageType type, const void* data, size_t size);
    ~SharedMemoryMessage() = default;

    // 消息访问器
    MessageType get_type() const { return static_cast<MessageType>(header_.message_type); }
    uint32_t get_id() const { return header_.message_id; }
    uint64_t get_timestamp() const { return header_.timestamp; }
    uint32_t get_payload_size() const { return header_.payload_size; }
    const uint8_t* get_payload() const { return payload_.data(); }
    uint8_t* get_mutable_payload() { return payload_.data(); }

    // 消息修改
    void set_type(MessageType type) { header_.message_type = static_cast<uint32_t>(type); }
    void set_payload(const void* data, size_t size);
    void set_flag(MessageFlags flag) { header_.flags |= static_cast<uint8_t>(flag); }
    bool has_flag(MessageFlags flag) const { return (header_.flags & static_cast<uint8_t>(flag)) != 0; }

    // 序列化/反序列化
    std::vector<uint8_t> serialize() const;
    bool deserialize(const uint8_t* data, size_t size);

    // 工具方法
    size_t total_size() const { return sizeof(MessageHeader) + header_.payload_size; }
    bool is_valid() const { return header_.message_id != 0; }

private:
    MessageHeader header_;
    std::vector<uint8_t> payload_;
    static std::atomic<uint32_t> next_id_;
};

// 共享内存管理器
class SharedMemoryManager {
public:
    // 配置
    struct Config {
        size_t buffer_size{1024 * 1024};  // 缓冲区大小
        size_t max_message_size{64 * 1024};  // 最大消息大小
        std::string instance_name;     // 实例名称
        bool auto_cleanup{true};       // 自动清理
        int heartbeat_interval_ms{1000};  // 心跳间隔

        Config(const std::string& name = "BitRPC_SharedMemory")
            : instance_name(name) {}
    };

    // 消息处理器
    using MessageHandler = std::function<bool(const SharedMemoryMessage&, SharedMemoryMessage&)>;

    explicit SharedMemoryManager(const Config& config = Config{});
    ~SharedMemoryManager();

    // 禁用拷贝
    SharedMemoryManager(const SharedMemoryManager&) = delete;
    SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;

    // 生命周期管理
    bool start_producer();
    bool start_consumer();
    void stop();

    // 消息发送/接收
    bool send_message(const SharedMemoryMessage& message);
    bool send_message(MessageType type, const void* data, size_t size);
    bool receive_message(SharedMemoryMessage& message, int timeout_ms = -1);
    bool peek_message(SharedMemoryMessage& message) const;

    // 批量操作
    size_t send_messages(const std::vector<SharedMemoryMessage>& messages);
    size_t receive_messages(std::vector<SharedMemoryMessage>& messages, size_t max_count, int timeout_ms = -1);

    // 消息处理器注册
    void register_handler(MessageType type, MessageHandler handler);
    void unregister_handler(MessageType type);

    // 状态查询
    bool is_running() const { return running_; }
    bool is_producer() const { return is_producer_; }
    bool is_consumer() const { return is_consumer_; }
    size_t get_pending_count() const { return pending_count_; }
    size_t get_buffer_usage() const { return buffer_usage_; }

    // 统计信息
    struct Statistics {
        uint64_t messages_sent{0};
        uint64_t messages_received{0};
        uint64_t bytes_sent{0};
        uint64_t bytes_received{0};
        uint64_t errors{0};
        double avg_message_size{0.0};
    };

    const Statistics& get_statistics() const { return stats_; }
    void reset_statistics();

    // 心跳和健康检查
    bool send_heartbeat();
    bool wait_for_heartbeat(int timeout_ms = 2000);

    // 缓冲区管理
    size_t get_free_space() const;
    size_t get_used_space() const;
    bool clear_buffer();

private:
    // 内部方法
    void worker_thread();
    void heartbeat_thread();
    bool process_message(const SharedMemoryMessage& message);
    void update_statistics(bool sent, size_t bytes);

    // 消息处理
    bool validate_message(const SharedMemoryMessage& message) const;
    std::vector<uint8_t> serialize_message(const SharedMemoryMessage& message) const;
    bool deserialize_message(const uint8_t* data, size_t size, SharedMemoryMessage& message) const;

private:
    Config config_;
    std::unique_ptr<RingBuffer> ring_buffer_;
    std::atomic<bool> running_{false};
    std::atomic<bool> is_producer_{false};
    std::atomic<bool> is_consumer_{false};

    // 线程
    std::unique_ptr<std::thread> worker_thread_;
    std::unique_ptr<std::thread> heartbeat_thread_;

    // 消息处理器
    std::unordered_map<MessageType, MessageHandler> handlers_;
    std::mutex handlers_mutex_;

    // 同步
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    std::atomic<size_t> pending_count_{0};
    std::atomic<size_t> buffer_usage_{0};

    // 心跳
    std::atomic<uint64_t> last_heartbeat_{0};
    std::atomic<bool> heartbeat_active_{false};
};

// 多实例管理器
class SharedMemoryMultiInstanceManager {
public:
    using InstanceManagerPtr = std::shared_ptr<SharedMemoryManager>;

    // 注册/注销实例
    static bool register_instance(const std::string& name, InstanceManagerPtr manager);
    static bool unregister_instance(const std::string& name);
    static InstanceManagerPtr get_instance(const std::string& name);

    // 全局操作
    static void stop_all_instances();
    static std::vector<std::string> get_instance_names();
    static bool is_instance_running(const std::string& name);

private:
    static std::unordered_map<std::string, InstanceManagerPtr> instances_;
    static std::mutex instances_mutex_;
};

// 便捷工厂函数
inline std::shared_ptr<SharedMemoryManager> create_producer_manager(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto config = SharedMemoryManager::Config(name);
    config.buffer_size = buffer_size;
    auto manager = std::make_shared<SharedMemoryManager>(config);

    if (manager->start_producer()) {
        SharedMemoryMultiInstanceManager::register_instance(name, manager);
        return manager;
    }

    return nullptr;
}

inline std::shared_ptr<SharedMemoryManager> create_consumer_manager(
    const std::string& name, size_t buffer_size = 1024 * 1024) {
    auto config = SharedMemoryManager::Config(name);
    config.buffer_size = buffer_size;
    auto manager = std::make_shared<SharedMemoryManager>(config);

    if (manager->start_consumer()) {
        SharedMemoryMultiInstanceManager::register_instance(name, manager);
        return manager;
    }

    return nullptr;
}

} // namespace shared_memory
} // namespace bitrpc