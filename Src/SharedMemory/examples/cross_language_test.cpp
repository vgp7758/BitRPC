/*
 * 跨语言共享内存测试 - C++生产者示例
 * 测试与C#和Python的跨语言通信
 */

#include "../shared_memory_api.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using namespace bitrpc::shared_memory;

// 测试数据结构
#pragma pack(push, 1)
struct TestData {
    int id;
    double value;
    char message[64];
    uint64_t timestamp;
};
#pragma pack(pop)

class CrossLanguageTestProducer {
public:
    CrossLanguageTestProducer(const std::string& name = "CrossLangTest")
        : name_(name), running_(false) {
    }

    ~CrossLanguageTestProducer() {
        stop();
    }

    bool start() {
        if (running_) {
            return true;
        }

        // 创建生产者
        producer_ = create_producer(name_, 1024 * 1024); // 1MB buffer
        if (!producer_) {
            std::cerr << "Failed to create producer" << std::endl;
            return false;
        }

        std::cout << "✓ C++ Producer connected to shared memory: " << name_ << std::endl;
        running_ = true;

        // 启动发送线程
        send_thread_ = std::make_unique<std::thread>(&CrossLanguageTestProducer::send_loop, this);

        return true;
    }

    void stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        if (send_thread_ && send_thread_->joinable()) {
            send_thread_->join();
        }

        producer_.reset();
        std::cout << "✓ C++ Producer stopped" << std::endl;
    }

    bool is_running() const {
        return running_;
    }

private:
    void send_loop() {
        int counter = 0;

        while (running_) {
            try {
                // 发送文本消息
                std::string text_message = "Hello from C++! Message #" + std::to_string(counter);
                if (producer_->send_string(text_message)) {
                    std::cout << "Sent text: " << text_message << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // 发送结构化数据
                TestData data;
                data.id = counter;
                data.value = 3.14159 * counter;
                snprintf(data.message, sizeof(data.message), "C++ Data #%d", counter);
                data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                SharedMemoryMessage message(MessageType::DATA, &data, sizeof(TestData));
                message.set_flag(MessageFlags::URGENT);

                if (producer_->send_message(message)) {
                    std::cout << "Sent structured data: id=" << data.id
                              << ", value=" << data.value << std::endl;
                }

                // 发送心跳
                if (counter % 10 == 0) {
                    producer_->send_heartbeat();
                    std::cout << "Sent heartbeat" << std::endl;
                }

                counter++;

                // 等待一段时间
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

            } catch (const std::exception& e) {
                std::cerr << "Error in send loop: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

private:
    std::string name_;
    std::atomic<bool> running_;
    std::unique_ptr<SharedMemoryProducer> producer_;
    std::unique_ptr<std::thread> send_thread_;
};

class CrossLanguageTestConsumer {
public:
    CrossLanguageTestConsumer(const std::string& name = "CrossLangTest")
        : name_(name), running_(false) {
    }

    ~CrossLanguageTestConsumer() {
        stop();
    }

    bool start() {
        if (running_) {
            return true;
        }

        // 创建消费者
        consumer_ = create_consumer(name_, 1024 * 1024); // 1MB buffer
        if (!consumer_) {
            std::cerr << "Failed to create consumer" << std::endl;
            return false;
        }

        // 注册消息处理器
        consumer_->register_handler(MessageType::DATA,
            [this](const SharedMemoryMessage& msg) -> bool {
                std::cout << "Received data message: " << msg.get_payload_size() << " bytes" << std::endl;

                if (msg.get_payload_size() == sizeof(TestData)) {
                    const TestData* data = reinterpret_cast<const TestData*>(msg.get_payload());
                    std::cout << "  Parsed data: id=" << data->id
                              << ", value=" << data->value
                              << ", message=" << data->message
                              << ", timestamp=" << data->timestamp << std::endl;
                }

                return true;
            });

        consumer_->register_handler(MessageType::HEARTBEAT,
            [this](const SharedMemoryMessage& msg) -> bool {
                std::cout << "Received heartbeat from producer" << std::endl;
                return true;
            });

        std::cout << "✓ C++ Consumer connected to shared memory: " << name_ << std::endl;
        running_ = true;

        // 启动接收线程
        receive_thread_ = std::make_unique<std::thread>(&CrossLanguageTestConsumer::receive_loop, this);

        return true;
    }

    void stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        if (receive_thread_ && receive_thread_->joinable()) {
            receive_thread_->join();
        }

        consumer_.reset();
        std::cout << "✓ C++ Consumer stopped" << std::endl;
    }

    bool is_running() const {
        return running_;
    }

private:
    void receive_loop() {
        while (running_) {
            try {
                // 尝试接收消息
                SharedMemoryMessage message;
                if (consumer_->receive_message(message, 100)) {
                    // 消息已经在处理器中处理
                    continue;
                }

                // 尝试接收字符串
                std::string text;
                if (consumer_->receive_string(text, 100)) {
                    std::cout << "Received string: " << text << std::endl;
                    continue;
                }

                // 尝试接收原始数据
                std::vector<uint8_t> data;
                if (consumer_->receive(data, 100)) {
                    std::cout << "Received raw data: " << data.size() << " bytes" << std::endl;
                    continue;
                }

                // 如果没有数据，短暂休眠
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

            } catch (const std::exception& e) {
                std::cerr << "Error in receive loop: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

private:
    std::string name_;
    std::atomic<bool> running_;
    std::unique_ptr<SharedMemoryConsumer> consumer_;
    std::unique_ptr<std::thread> receive_thread_;
};

int main(int argc, char* argv[]) {
    std::cout << "BitRPC Cross-Language Shared Memory Test (C++)" << std::endl;
    std::cout << "=============================================" << std::endl;

    std::string mode = "producer"; // 默认为生产者
    std::string name = "CrossLangTest";

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--mode" && i + 1 < argc) {
            mode = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "--name" && i + 1 < argc) {
            name = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --mode MODE    Set mode (producer/consumer)" << std::endl;
            std::cout << "  --name NAME    Set shared memory name" << std::endl;
            std::cout << "  --help, -h     Show this help" << std::endl;
            return 0;
        }
    }

    std::cout << "Mode: " << mode << std::endl;
    std::cout << "Shared Memory Name: " << name << std::endl;
    std::cout << std::endl;

    try {
        if (mode == "producer") {
            CrossLanguageTestProducer producer(name);
            if (!producer.start()) {
                std::cerr << "Failed to start producer" << std::endl;
                return 1;
            }

            std::cout << "Producer running. Press Ctrl+C to stop..." << std::endl;

            // 等待用户中断
            while (producer.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } else if (mode == "consumer") {
            CrossLanguageTestConsumer consumer(name);
            if (!consumer.start()) {
                std::cerr << "Failed to start consumer" << std::endl;
                return 1;
            }

            std::cout << "Consumer running. Press Ctrl+C to stop..." << std::endl;

            // 等待用户中断
            while (consumer.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } else {
            std::cerr << "Invalid mode: " << mode << std::endl;
            std::cerr << "Supported modes: producer, consumer" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}