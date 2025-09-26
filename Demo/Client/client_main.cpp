#include "../cpp/include/models.h"
#include "../cpp/include/testservice_client.h"
#include "../cpp/include/protocol_factory.h"
#include "../cpp/runtime/rpc_core.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

using namespace bitrpc;
using namespace bitrpc::test::protocol;

int main() {
    std::cout << "=== BitRPC C++ Test Client ===" << std::endl;

    try {
        // Initialize the protocol
        ProtocolFactory::initialize();

        // Create TCP client
        TcpRpcClient client;
        client.connect("localhost", 8080);

        std::cout << "Connected to server. Starting tests..." << std::endl;

        // Test 1: Echo
        std::cout << "\n1. Testing Echo method..." << std::endl;
        try {
            EchoRequest echoRequest;
            echoRequest.message = "Hello from C++ client!";
            echoRequest.timestamp = (int32_t)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Serialize request
            StreamWriter writer;
            writer.write_string(echoRequest.message);
            writer.write_int32(echoRequest.timestamp);
            auto requestData = writer.to_array();

            // Call RPC method
            auto responseData = client.call("TestService.Echo", requestData);

            // Deserialize response
            StreamReader reader(responseData);
            EchoResponse echoResponse;
            echoResponse.message = reader.read_string();
            echoResponse.timestamp = reader.read_int32();
            echoResponse.server_time = reader.read_string();

            std::cout << "   Echo: " << echoResponse.message << std::endl;
            std::cout << "   Timestamp: " << echoResponse.timestamp << std::endl;
            std::cout << "   Server time: " << echoResponse.server_time << std::endl;
            std::cout << "   ✓ Echo test passed" << std::endl;
        } catch (const std::exception& ex) {
            std::cout << "   ✗ Echo test failed: " << ex.what() << std::endl;
        }

        // Test 2: Login with valid credentials
        std::cout << "\n2. Testing Login with valid credentials..." << std::endl;
        try {
            LoginRequest loginRequest;
            loginRequest.username = "admin";
            loginRequest.password = "admin123";

            // Serialize request
            StreamWriter writer;
            writer.write_string(loginRequest.username);
            writer.write_string(loginRequest.password);
            auto requestData = writer.to_array();

            // Call RPC method
            auto responseData = client.call("TestService.Login", requestData);

            // Deserialize response
            StreamReader reader(responseData);
            LoginResponse loginResponse;
            loginResponse.success = reader.read_bool();

            if (loginResponse.success) {
                // Read user info
                loginResponse.user.user_id = reader.read_int64();
                loginResponse.user.username = reader.read_string();
                loginResponse.user.email = reader.read_string();

                // Read roles
                int32_t roleCount = reader.read_int32();
                for (int i = 0; i < roleCount; i++) {
                    loginResponse.user.roles.push_back(reader.read_string());
                }

                loginResponse.user.is_active = reader.read_bool();
                loginResponse.token = reader.read_string();
            } else {
                loginResponse.error_message = reader.read_string();
            }

            std::cout << "   Success: " << (loginResponse.success ? "true" : "false") << std::endl;
            if (loginResponse.success) {
                std::cout << "   User ID: " << loginResponse.user.user_id << std::endl;
                std::cout << "   Username: " << loginResponse.user.username << std::endl;
                std::cout << "   Token: " << loginResponse.token << std::endl;
            } else {
                std::cout << "   Error: " << loginResponse.error_message << std::endl;
            }
            std::cout << "   ✓ Login test passed" << std::endl;
        } catch (const std::exception& ex) {
            std::cout << "   ✗ Login test failed: " << ex.what() << std::endl;
        }

        // Test 3: GetUser
        std::cout << "\n3. Testing GetUser method..." << std::endl;
        try {
            GetUserRequest getUserRequest;
            getUserRequest.user_id = 1;

            // Serialize request
            StreamWriter writer;
            writer.write_int64(getUserRequest.user_id);
            auto requestData = writer.to_array();

            // Call RPC method
            auto responseData = client.call("TestService.GetUser", requestData);

            // Deserialize response
            StreamReader reader(responseData);
            GetUserResponse getUserResponse;
            getUserResponse.found = reader.read_bool();

            if (getUserResponse.found) {
                getUserResponse.user.user_id = reader.read_int64();
                getUserResponse.user.username = reader.read_string();
                getUserResponse.user.email = reader.read_string();

                // Read roles
                int32_t roleCount = reader.read_int32();
                for (int i = 0; i < roleCount; i++) {
                    getUserResponse.user.roles.push_back(reader.read_string());
                }

                getUserResponse.user.is_active = reader.read_bool();
            }

            std::cout << "   Found: " << (getUserResponse.found ? "true" : "false") << std::endl;
            if (getUserResponse.found) {
                std::cout << "   User ID: " << getUserResponse.user.user_id << std::endl;
                std::cout << "   Username: " << getUserResponse.user.username << std::endl;
                std::cout << "   Email: " << getUserResponse.user.email << std::endl;
                std::cout << "   Roles: ";
                for (const auto& role : getUserResponse.user.roles) {
                    std::cout << role << " ";
                }
                std::cout << std::endl;
                std::cout << "   Active: " << (getUserResponse.user.is_active ? "true" : "false") << std::endl;
            }
            std::cout << "   ✓ GetUser test passed" << std::endl;
        } catch (const std::exception& ex) {
            std::cout << "   ✗ GetUser test failed: " << ex.what() << std::endl;
        }

        std::cout << "\n=== All tests completed ===" << std::endl;

        client.disconnect();

    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}