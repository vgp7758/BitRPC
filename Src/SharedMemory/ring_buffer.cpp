#include "ring_buffer.h"
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/syscall.h>
#endif

namespace bitrpc {
namespace shared_memory {

// Windows实现
#ifdef _WIN32
class WindowsEvent : public CrossProcessEvent {
public:
    WindowsEvent(const std::string& name, bool manual_reset = false, bool initial_state = false) {
        handle_ = CreateEventA(nullptr, manual_reset, initial_state, name.c_str());
        if (handle_ == nullptr) {
            throw std::runtime_error("Failed to create event: " + std::to_string(GetLastError()));
        }
    }

    ~WindowsEvent() override { close(); }

    bool signal() override {
        return SetEvent(handle_) != 0;
    }

    bool wait(int timeout_ms = -1) override {
        DWORD timeout = (timeout_ms < 0) ? INFINITE : timeout_ms;
        return WaitForSingleObject(handle_, timeout) == WAIT_OBJECT_0;
    }

    bool reset() override {
        return ResetEvent(handle_) != 0;
    }

    void close() override {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

private:
    HANDLE handle_{nullptr};
};

// Linux实现
#else
class LinuxEvent : public CrossProcessEvent {
public:
    LinuxEvent(const std::string& name) : name_(name) {
        std::string sem_name = "/" + name;
        semaphore_ = sem_open(sem_name.c_str(), O_CREAT, 0666, 0);
        if (semaphore_ == SEM_FAILED) {
            throw std::runtime_error("Failed to create semaphore");
        }
    }

    ~LinuxEvent() override { close(); }

    bool signal() override {
        return sem_post(semaphore_) == 0;
    }

    bool wait(int timeout_ms = -1) override {
        if (timeout_ms < 0) {
            return sem_wait(semaphore_) == 0;
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        ts.tv_sec += timeout_ms / 1000 + ts.tv_nsec / 1000000000;
        ts.tv_nsec %= 1000000000;

        return sem_timedwait(semaphore_, &ts) == 0;
    }

    bool reset() override {
        // 对于信号量，reset没有意义，清空所有等待
        while (sem_trywait(semaphore_) == 0) {
            // 继续清空
        }
        return true;
    }

    void close() override {
        if (semaphore_ != SEM_FAILED) {
            sem_close(semaphore_);
            sem_unlink(("/" + name_).c_str());
            semaphore_ = SEM_FAILED;
        }
    }

private:
    sem_t* semaphore_{SEM_FAILED};
    std::string name_;
};
#endif

// RingBuffer实现
RingBuffer::RingBuffer(const Config& config) : config_(config) {
}

RingBuffer::~RingBuffer() {
    close();
}

bool RingBuffer::create(CreateMode mode) {
    if (initialized_) {
        return true;
    }

    try {
        // 分配共享内存
        if (!allocate_memory()) {
            return false;
        }

        // 创建同步对象
        if (config_.enable_events && !create_shared_objects()) {
            return false;
        }

        // 初始化头部
        if (mode == CreateMode::CREATE_ONLY ||
            (mode == CreateMode::CREATE_OR_OPEN && header_->magic_number != MAGIC_NUMBER)) {

            header_->magic_number = MAGIC_NUMBER;
            header_->version = 1;
            header_->buffer_size = config_.buffer_size;
            header_->write_pos.store(0);
            header_->read_pos.store(0);
            header_->initialized = 1;
        }

        // 验证头部
        if (!validate_header()) {
            close();
            return false;
        }

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "RingBuffer creation failed: " << e.what() << std::endl;
        close();
        return false;
    }
}

void RingBuffer::close() {
    if (data_ready_event_) {
        data_ready_event_->close();
        data_ready_event_.reset();
    }

    if (space_available_event_) {
        space_available_event_->close();
        space_available_event_.reset();
    }

    if (mapped_memory_ != nullptr) {
#ifdef _WIN32
        UnmapViewOfFile(mapped_memory_);
        if (file_mapping_ != nullptr) {
            CloseHandle(file_mapping_);
        }
        if (file_handle_ != nullptr) {
            CloseHandle(file_handle_);
        }
#else
        munmap(mapped_memory_, mapped_size_);
        if (file_descriptor_ != -1) {
            ::close(file_descriptor_);
        }
#endif
        mapped_memory_ = nullptr;
    }

    header_ = nullptr;
    buffer_ = nullptr;
    initialized_ = false;
}

bool RingBuffer::write(const void* data, size_t size) {
    if (!initialized_ || data == nullptr || size == 0) {
        return false;
    }

    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();

    // 计算可用空间
    size_t free_space = config_.buffer_size - (write_pos - read_pos);
    if (size > free_space) {
        return false;
    }

    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint64_t buffer_size = config_.buffer_size;

    // 处理环形缓冲区的回绕
    size_t first_chunk = std::min(size, buffer_size - (write_pos % buffer_size));
    size_t second_chunk = size - first_chunk;

    // 写入第一部分
    uint8_t* dst = buffer_ + (write_pos % buffer_size);
    std::memcpy(dst, src, first_chunk);

    // 写入第二部分（如果有）
    if (second_chunk > 0) {
        dst = buffer_;
        std::memcpy(dst, src + first_chunk, second_chunk);
    }

    // 更新写位置
    set_write_position(write_pos + size);
    release_barrier();

    // 通知消费者
    if (data_ready_event_) {
        data_ready_event_->signal();
    }

    return true;
}

bool RingBuffer::write_atomic(const void* data, size_t size) {
    // 检查是否有足够的连续空间
    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();
    size_t free_space = config_.buffer_size - (write_pos - read_pos);

    if (size > free_space) {
        return false;
    }

    // 对于原子写入，确保不跨越缓冲区边界
    uint64_t buffer_size = config_.buffer_size;
    uint64_t write_offset = write_pos % buffer_size;

    if (write_offset + size <= buffer_size) {
        // 可以一次性写入
        std::memcpy(buffer_ + write_offset, data, size);
    } else {
        // 需要回绕，不是连续的，无法原子写入
        return false;
    }

    set_write_position(write_pos + size);
    release_barrier();

    if (data_ready_event_) {
        data_ready_event_->signal();
    }

    return true;
}

bool RingBuffer::read(void* buffer, size_t buffer_size, size_t& bytes_read) {
    if (!initialized_ || buffer == nullptr || buffer_size == 0) {
        bytes_read = 0;
        return false;
    }

    acquire_barrier();
    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();

    // 计算可用数据
    size_t available = write_pos - read_pos;
    if (available == 0) {
        bytes_read = 0;
        return true;  // 缓冲区为空
    }

    // 限制读取量
    size_t to_read = std::min(available, buffer_size);
    uint64_t buffer_size = config_.buffer_size;

    // 处理环形缓冲区的回绕
    uint8_t* dst = static_cast<uint8_t*>(buffer);
    size_t first_chunk = std::min(to_read, buffer_size - (read_pos % buffer_size));
    size_t second_chunk = to_read - first_chunk;

    // 读取第一部分
    const uint8_t* src = buffer_ + (read_pos % buffer_size);
    std::memcpy(dst, src, first_chunk);

    // 读取第二部分（如果有）
    if (second_chunk > 0) {
        src = buffer_;
        std::memcpy(dst + first_chunk, src, second_chunk);
    }

    // 更新读位置
    set_read_position(read_pos + to_read);
    bytes_read = to_read;

    // 通知生产者
    if (space_available_event_) {
        space_available_event_->signal();
    }

    return true;
}

bool RingBuffer::peek(void* buffer, size_t buffer_size, size_t& bytes_read) const {
    if (!initialized_ || buffer == nullptr || buffer_size == 0) {
        bytes_read = 0;
        return false;
    }

    acquire_barrier();
    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();

    size_t available = write_pos - read_pos;
    if (available == 0) {
        bytes_read = 0;
        return true;
    }

    size_t to_read = std::min(available, buffer_size);
    uint64_t buffer_size = config_.buffer_size;

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    size_t first_chunk = std::min(to_read, buffer_size - (read_pos % buffer_size));
    size_t second_chunk = to_read - first_chunk;

    const uint8_t* src = buffer_ + (read_pos % buffer_size);
    std::memcpy(dst, src, first_chunk);

    if (second_chunk > 0) {
        src = buffer_;
        std::memcpy(dst + first_chunk, src, second_chunk);
    }

    bytes_read = to_read;
    return true;
}

bool RingBuffer::skip(size_t bytes) {
    if (!initialized_) {
        return false;
    }

    acquire_barrier();
    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();

    size_t available = write_pos - read_pos;
    if (bytes > available) {
        return false;
    }

    set_read_position(read_pos + bytes);

    if (space_available_event_) {
        space_available_event_->signal();
    }

    return true;
}

size_t RingBuffer::get_free_space() const {
    if (!initialized_) {
        return 0;
    }

    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();
    return config_.buffer_size - (write_pos - read_pos);
}

size_t RingBuffer::get_used_space() const {
    if (!initialized_) {
        return 0;
    }

    acquire_barrier();
    uint64_t write_pos = get_write_position();
    uint64_t read_pos = get_read_position();
    return write_pos - read_pos;
}

size_t RingBuffer::get_capacity() const {
    return config_.buffer_size;
}

bool RingBuffer::is_connected() const {
    return initialized_;
}

bool RingBuffer::is_empty() const {
    return get_used_space() == 0;
}

bool RingBuffer::is_full() const {
    return get_free_space() == 0;
}

bool RingBuffer::wait_for_data(int timeout_ms) {
    if (!initialized_ || !data_ready_event_) {
        return false;
    }

    // 首先检查是否有数据
    if (!is_empty()) {
        return true;
    }

    // 等待事件通知
    return data_ready_event_->wait(timeout_ms);
}

bool RingBuffer::notify_data_ready() {
    if (!initialized_ || !data_ready_event_) {
        return false;
    }

    return data_ready_event_->signal();
}

// 私有方法实现
bool RingBuffer::allocate_memory() {
    size_t total_size = HEADER_SIZE + config_.buffer_size;

    // 对齐到页面大小
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size_t page_size = si.dwPageSize;
#else
    size_t page_size = sysconf(_SC_PAGESIZE);
#endif
    mapped_size_ = ((total_size + page_size - 1) / page_size) * page_size;

#ifdef _WIN32
    // Windows共享内存实现
    std::string mapping_name = "Local\\" + config_.name;

    file_mapping_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(mapped_size_),
        mapping_name.c_str()
    );

    if (file_mapping_ == nullptr) {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            // 尝试打开已存在的映射
            file_mapping_ = OpenFileMappingA(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                mapping_name.c_str()
            );
        }

        if (file_mapping_ == nullptr) {
            return false;
        }
    }

    mapped_memory_ = MapViewOfFile(
        file_mapping_,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        mapped_size_
    );

#else
    // Linux共享内存实现
    std::string shm_name = "/BitRPC_" + config_.name;

    file_descriptor_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (file_descriptor_ == -1) {
        return false;
    }

    // 设置共享内存大小
    if (ftruncate(file_descriptor_, mapped_size_) == -1) {
        return false;
    }

    mapped_memory_ = mmap(
        nullptr,
        mapped_size_,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        file_descriptor_,
        0
    );

    if (mapped_memory_ == MAP_FAILED) {
        mapped_memory_ = nullptr;
        return false;
    }
#endif

    if (mapped_memory_ == nullptr) {
        return false;
    }

    // 设置头部和缓冲区指针
    header_ = static_cast<RingBufferHeader*>(mapped_memory_);
    buffer_ = static_cast<uint8_t*>(mapped_memory_) + HEADER_SIZE;

    return true;
}

bool RingBuffer::create_shared_objects() {
    try {
        std::string data_ready_name = config_.name + "_data_ready";
        std::string space_available_name = config_.name + "_space_available";

#ifdef _WIN32
        data_ready_event_ = std::make_unique<WindowsEvent>(data_ready_name);
        space_available_event_ = std::make_unique<WindowsEvent>(space_available_name);
#else
        data_ready_event_ = std::make_unique<LinuxEvent>(data_ready_name);
        space_available_event_ = std::make_unique<LinuxEvent>(space_available_name);
#endif

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create shared objects: " << e.what() << std::endl;
        return false;
    }
}

void RingBuffer::cleanup() {
    // 清理共享对象的实现...
}

bool RingBuffer::validate_header() const {
    if (!header_) {
        return false;
    }

    return header_->magic_number == MAGIC_NUMBER &&
           header_->version == 1 &&
           header_->buffer_size == config_.buffer_size &&
           header_->initialized == 1;
}

void RingBuffer::update_header() {
    if (header_) {
        header_->version = 1;
        header_->initialized = 1;
    }
}

uint8_t* RingBuffer::get_buffer_ptr() const {
    return buffer_;
}

size_t RingBuffer::get_data_offset() const {
    return HEADER_SIZE;
}

uint64_t RingBuffer::get_write_position() const {
    return header_ ? header_->write_pos.load(std::memory_order_acquire) : 0;
}

uint64_t RingBuffer::get_read_position() const {
    return header_ ? header_->read_pos.load(std::memory_order_acquire) : 0;
}

void RingBuffer::set_write_position(uint64_t pos) {
    if (header_) {
        header_->write_pos.store(pos, std::memory_order_release);
    }
}

void RingBuffer::set_read_position(uint64_t pos) {
    if (header_) {
        header_->read_pos.store(pos, std::memory_order_release);
    }
}

void RingBuffer::acquire_barrier() {
    std::atomic_thread_fence(std::memory_order_acquire);
}

void RingBuffer::release_barrier() {
    std::atomic_thread_fence(std::memory_order_release);
}

// RingBufferFactory实现
std::unique_ptr<RingBuffer> RingBufferFactory::create_producer(const std::string& name, size_t buffer_size) {
    auto config = RingBuffer::Config(name);
    config.buffer_size = buffer_size;
    auto buffer = std::make_unique<RingBuffer>(config);

    if (!buffer->create(RingBuffer::CreateMode::CREATE_OR_OPEN)) {
        return nullptr;
    }

    return buffer;
}

std::unique_ptr<RingBuffer> RingBufferFactory::create_consumer(const std::string& name, size_t buffer_size) {
    auto config = RingBuffer::Config(name);
    config.buffer_size = buffer_size;
    auto buffer = std::make_unique<RingBuffer>(config);

    if (!buffer->create(RingBuffer::CreateMode::OPEN_ONLY)) {
        return nullptr;
    }

    return buffer;
}

bool RingBufferFactory::remove_ring_buffer(const std::string& name) {
#ifdef _WIN32
    // Windows下不需要显式删除，会自动清理
    return true;
#else
    std::string shm_name = "/BitRPC_" + name;
    return shm_unlink(shm_name.c_str()) == 0;
#endif
}

} // namespace shared_memory
} // namespace bitrpc