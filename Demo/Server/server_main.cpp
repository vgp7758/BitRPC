#include "../cpp/include/models.h"
#include "../cpp/include/testservice_service_base.h"
#include "../cpp/include/protocol_factory.h"
#include "../cpp/runtime/client.h"
#include "../cpp/runtime/server.h"
#include "../cpp/runtime/serialization.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <map>

using namespace bitrpc;
using namespace bitrpc::test::protocol;

class TestServiceImpl : public TestServiceServiceBase {
public:
    TestServiceImpl() {
        // Initialize some test users
        users_[1] = UserInfo();
        users_[1].user_id = 1;
        users_[1].username = "admin";
        users_[1].email = "admin@test.com";
        users_[1].roles = {"admin"};
        users_[1].is_active = true;
        users_[1].created_at = std::chrono::system_clock::now();

        users_[2] = UserInfo();
        users_[2].user_id = 2;
        users_[2].username = "user1";
        users_[2].email = "user1@test.com";
        users_[2].roles = {"user"};
        users_[2].is_active = true;
        users_[2].created_at = std::chrono::system_clock::now();

        users_[3] = UserInfo();
        users_[3].user_id = 3;
        users_[3].username = "user2";
        users_[3].email = "user2@test.com";
        users_[3].roles = {"user"};
        users_[3].is_active = false;
        users_[3].created_at = std::chrono::system_clock::now();
    }

    std::future<LoginResponse> LoginAsync_impl(const LoginRequest& request) override {
        return std::async(std::launch::async, [this, request]() {
            LoginResponse response;

            std::cout << "Login attempt for user: " << request.username << std::endl;

            // Simple authentication logic
            if (request.username == "admin" && request.password == "admin123") {
                response.success = true;
                response.user = users_[1];
                response.token = "admin-token-12345";
                response.error_message = "";
            } else if (request.username == "user1" && request.password == "user123") {
                response.success = true;
                response.user = users_[2];
                response.token = "user1-token-67890";
                response.error_message = "";
            } else {
                response.success = false;
                response.token = "";
                response.error_message = "Invalid username or password";
            }

            std::cout << "Login " << (response.success ? "successful" : "failed") << " for user: " << request.username << std::endl;
            return response;
        });
    }

    std::future<GetUserResponse> GetUserAsync_impl(const GetUserRequest& request) override {
        return std::async(std::launch::async, [this, request]() {
            GetUserResponse response;

            std::cout << "GetUser request for ID: " << request.user_id << std::endl;

            auto it = users_.find(request.user_id);
            if (it != users_.end()) {
                response.user = it->second;
                response.found = true;
                std::cout << "User found: " << response.user.username << std::endl;
            } else {
                response.found = false;
                std::cout << "User not found for ID: " << request.user_id << std::endl;
            }

            return response;
        });
    }

    std::future<EchoResponse> EchoAsync_impl(const EchoRequest& request) override {
        return std::async(std::launch::async, [this, request]() {
            EchoResponse response;

            response.message = request.message;
            response.timestamp = request.timestamp;

            // Get current server time
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string time_str = std::ctime(&time_t);
            // Remove newline from ctime output
            if (!time_str.empty()) {
                time_str.pop_back();
            }
            response.server_time = time_str;

            std::cout << "Echo: " << request.message << " at " << request.timestamp << std::endl;
            std::cout << "Response sent at: " << response.server_time << std::endl;

            return response;
        });
    }

private:
    std::map<int64_t, UserInfo> users_;
};

int main() {
    std::cout << "=== BitRPC C++ Test Server ===" << std::endl;

    try {
        // Initialize the protocol
        ProtocolFactory::initialize();

        // Create RPC server
        TcpRpcServer server;

        // Create and register service
        auto service = std::make_shared<TestServiceImpl>();
        server.register_service(service);

        // Start server on port 8080
        int port = 8080;
        std::cout << "Starting server on port " << port << "..." << std::endl;
        server.start(port);

        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;

        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}