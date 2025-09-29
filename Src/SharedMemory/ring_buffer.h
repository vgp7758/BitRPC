#pragma once

#include <cstdint>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#endif

namespace bitrpc {
namespace shared_memory {

// 环形缓冲区头部元数据
#pragma pack(push, 1)  // 确保紧凑内存布局
struct RingBufferHeader {
    std::atomic<uint64_t> write_pos{0};    // 写位置
    std::atomic<uint64_t> read_pos{0};     // 读位置
    uint64_t buffer_size{0};               // 缓冲区大小
    uint32_t magic_number{0};              // 魔数，用于验证
    uint32_t version{1};                   // 版本号
    uint8_t initialized{0};               // 初始化标志
    uint8_t padding[7];                   // 对齐填充
};
#pragma pack(pop)

// 跨进程事件通知接口
class CrossProcessEvent {
public:
    virtual ~CrossProcessEvent() = default;
    virtual bool signal() = 0;
    virtual bool wait(int timeout_ms = -1) = 0;
    virtual bool reset() = 0;
    virtual void close() = 0;
};

// SPSC环形缓冲区核心类
class RingBuffer {
public:
    // 配置参数
    struct Config {
        size_t buffer_size{1024 * 1024};  // 默认1MB
        bool enable_events{true};         // 启用事件通知
        std::string name;                 // 缓冲区名称（用于跨进程标识）

        Config(const std::string& buffer_name = "BitRPC_RingBuffer")
            : name(buffer_name) {}
    };

    // 创建选项
    enum class CreateMode {
        CREATE_OR_OPEN,   // 创建或打开已存在的
        CREATE_ONLY,       // 仅创建新的
        OPEN_ONLY          // 仅打开已存在的
    };

    explicit RingBuffer(const Config& config = Config{});
    ~RingBuffer();

    // 禁用拷贝
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // 初始化/清理
    bool create(CreateMode mode = CreateMode::CREATE_OR_OPEN);
    void close();

    // 生产者接口
    bool write(const void* data, size_t size);
    bool write_atomic(const void* data, size_t size);  // 原子写入（全部成功或全部失败）
    size_t get_free_space() const;
    size_t get_capacity() const;

    // 消费者接口
    bool read(void* buffer, size_t buffer_size, size_t& bytes_read);
    bool peek(void* buffer, size_t buffer_size, size_t& bytes_read) const;  // 查看但不移动读指针
    bool skip(size_t bytes);  // 跳过指定字节数
    size_t get_used_space() const;

    // 状态查询
    bool is_connected() const;
    bool is_empty() const;
    bool is_full() const;
    std::string get_name() const { return config_.name; }

    // 等待通知（用于消费者等待数据）
    bool wait_for_data(int timeout_ms = -1);
    bool notify_data_ready();  // 生产者通知数据就绪

private:
    // 内部方法
    bool allocate_memory();
    bool create_shared_objects();
    void cleanup();
    bool validate_header() const;
    void update_header();

    // 内存操作
    uint8_t* get_buffer_ptr() const;
    size_t get_data_offset() const;

    // 位置计算
    uint64_t get_write_position() const;
    uint64_t get_read_position() const;
    void set_write_position(uint64_t pos);
    void set_read_position(uint64_t pos);

    // 内存屏障
    void acquire_barrier();
    void release_barrier();

private:
    Config config_;
    bool initialized_{false};

    // 共享内存句柄
#ifdef _WIN32
    HANDLE file_handle_{nullptr};
    HANDLE file_mapping_{nullptr};
#else
    int file_descriptor_{-1};
#endif

    // 内存映射
    void* mapped_memory_{nullptr};
    size_t mapped_size_{0};

    // 头部和缓冲区指针
    RingBufferHeader* header_{nullptr};
    uint8_t* buffer_{nullptr};

    // 跨进程同步事件
    std::unique_ptr<CrossProcessEvent> data_ready_event_;
    std::unique_ptr<CrossProcessEvent> space_available_event_;

    // 常量
    static constexpr uint32_t MAGIC_NUMBER = 0x42525446;  // "BRTF"
    static constexpr size_t HEADER_SIZE = sizeof(RingBufferHeader);
    static constexpr size_t ALIGNMENT = 64;  // 缓存行对齐
};

// 工厂方法创建环形缓冲区
class RingBufferFactory {
public:
    static std::unique_ptr<RingBuffer> create_producer(const std::string& name, size_t buffer_size = 1024 * 1024);
    static std::unique_ptr<RingBuffer> create_consumer(const std::string& name, size_t buffer_size = 1024 * 1024);
    static bool remove_ring_buffer(const std::string& name);
};

// 数据序列化助手
template<typename T>
bool write_data(RingBuffer& buffer, const T& data) {
    return buffer.write(&data, sizeof(T));
}

template<typename T>
bool read_data(RingBuffer& buffer, T& data) {
    size_t bytes_read = 0;
    return buffer.read(&data, sizeof(T), bytes_read) && bytes_read == sizeof(T));
}

} // namespace shared_memory
} // namespace bitrpc