#include "rpc_core.h"
#include <iostream>
#include <memory>

using namespace bitrpc;

// Test service implementation
class TestService : public ServiceBase {
public:
    TestService() : ServiceBase("TestService") {
        // Register test methods
        register_method<std::string, std::string>("echo", [this](const std::string* req) -> std::string* {
            return new std::string("Echo: " + *req);
        });

        register_method<int32_t, int32_t>("double", [this](const int32_t* req) -> int32_t* {
            return new int32_t(*req * 2);
        });
    }
};

int main() {
    std::cout << "=== BitRPC C++ Runtime Library Test ===" << std::endl;

    try {
        // Test serialization
        std::cout << "\n1. Testing serialization..." << std::endl;

        // Test string serialization
        std::string test_str = "Hello, BitRPC!";
        StreamWriter writer;
        writer.write_string(test_str);
        auto data = writer.to_array();

        StreamReader reader(data);
        std::string result = reader.read_string();

        std::cout << "Original: " << test_str << std::endl;
        std::cout << "Deserialized: " << result << std::endl;
        std::cout << "String test: " << (test_str == result ? "PASSED" : "FAILED") << std::endl;

        // Test integer serialization
        int32_t test_int = 42;
        StreamWriter int_writer;
        int_writer.write_int32(test_int);
        auto int_data = int_writer.to_array();

        StreamReader int_reader(int_data);
        int32_t int_result = int_reader.read_int32();

        std::cout << "Original int: " << test_int << std::endl;
        std::cout << "Deserialized int: " << int_result << std::endl;
        std::cout << "Int32 test: " << (test_int == int_result ? "PASSED" : "FAILED") << std::endl;

        // Test BitMask
        std::cout << "\n2. Testing BitMask..." << std::endl;
        BitMask mask;
        mask.set_bit(1, true);
        mask.set_bit(3, true);
        mask.set_bit(32, true);

        std::cout << "Bit 1: " << mask.get_bit(1) << std::endl;
        std::cout << "Bit 3: " << mask.get_bit(3) << std::endl;
        std::cout << "Bit 32: " << mask.get_bit(32) << std::endl;
        std::cout << "Bit 2: " << mask.get_bit(2) << std::endl;

        // Test BufferSerializer
        std::cout << "\n3. Testing BufferSerializer..." << std::endl;
        auto& serializer = BufferSerializer::instance();

        // Test type handlers
        int32_t value = 123;
        auto handler = serializer.get_handler(typeid(int32_t).hash_code());
        if (handler) {
            std::cout << "Int32 handler found with hash code: " << handler->hash_code() << std::endl;
        } else {
            std::cout << "Int32 handler not found!" << std::endl;
        }

        // Test service registration
        std::cout << "\n4. Testing service registration..." << std::endl;
        auto service = std::make_shared<TestService>();

        std::cout << "Service name: " << service->service_name() << std::endl;
        std::cout << "Has 'echo' method: " << service->has_method("echo") << std::endl;
        std::cout << "Has 'double' method: " << service->has_method("double") << std::endl;
        std::cout << "Has 'nonexistent' method: " << service->has_method("nonexistent") << std::endl;

        // Test method calling
        std::string echo_req = "Test message";
        auto echo_resp = service->call_method("echo", &echo_req);
        if (echo_resp) {
            std::string* echo_result = static_cast<std::string*>(echo_resp);
            std::cout << "Echo response: " << *echo_result << std::endl;
            delete echo_result;
        }

        int32_t double_req = 21;
        auto double_resp = service->call_method("double", &double_req);
        if (double_resp) {
            int32_t* double_result = static_cast<int32_t*>(double_resp);
            std::cout << "Double response: " << *double_result << std::endl;
            delete double_result;
        }

        // Test network (server only, no client connection test)
        std::cout << "\n5. Testing network setup..." << std::endl;
        TcpRpcServer server;
        std::cout << "TcpRpcServer created successfully" << std::endl;

        TcpRpcClient client;
        std::cout << "TcpRpcClient created successfully" << std::endl;

        std::cout << "\n=== All tests completed successfully! ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}