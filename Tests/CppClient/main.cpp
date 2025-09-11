#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

// Include generated protocol headers
#include "generated/models.h"
#include "generated/userservice_client.h"
#include "generated/serializer_registry.h"
#include "generated/protocol_factory.h"

// Include RPC core
#include "rpc_core.h"

using namespace bitrpc::example::protocol;

class UserServiceClient {
private:
    std::unique_ptr<bitrpc::TcpRpcClient> rpc_client_;
    
public:
    UserServiceClient() : rpc_client_(std::make_unique<bitrpc::TcpRpcClient>()) {}
    
    void connect(const std::string& host, int port) {
        rpc_client_->connect(host, port);
        std::cout << "Connected to server at " << host << ":" << port << std::endl;
    }
    
    void disconnect() {
        rpc_client_->disconnect();
        std::cout << "Disconnected from server" << std::endl;
    }
    
    bool login(const std::string& username, const std::string& password, User& out_user, std::string& out_token) {
        try {
            LoginRequest request;
            request.username = username;
            request.password = password;
            
            std::cout << "Attempting login for user: " << username << std::endl;
            
            // Serialize request
            bitrpc::StreamWriter writer;
            writer.write_object(&request, typeid(LoginRequest).hash_code());
            auto request_data = writer.to_array();
            
            // Call RPC method
            auto response_data = rpc_client_->call("UserService.Login", request_data);
            
            // Deserialize response
            bitrpc::StreamReader reader(response_data);
            auto response_ptr = std::unique_ptr<LoginResponse>(static_cast<LoginResponse*>(reader.read_object()));
            
            if (response_ptr) {
                std::cout << "Login response: success=" << response_ptr->success 
                         << ", error='" << response_ptr->error_message << "'" << std::endl;
                
                if (response_ptr->success && response_ptr->user) {
                    out_user = *response_ptr->user;
                    out_token = response_ptr->token;
                    return true;
                }
            }
            
            return false;
        } catch (const std::exception& e) {
            std::cerr << "Login failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool getUser(int64_t user_id, User& out_user) {
        try {
            GetUserRequest request;
            request.user_id = user_id;
            
            std::cout << "Requesting user info for ID: " << user_id << std::endl;
            
            // Serialize request
            bitrpc::StreamWriter writer;
            writer.write_object(&request, typeid(GetUserRequest).hash_code());
            auto request_data = writer.to_array();
            
            // Call RPC method
            auto response_data = rpc_client_->call("UserService.GetUser", request_data);
            
            // Deserialize response
            bitrpc::StreamReader reader(response_data);
            auto response_ptr = std::unique_ptr<GetUserResponse>(static_cast<GetUserResponse*>(reader.read_object()));
            
            if (response_ptr) {
                std::cout << "GetUser response: found=" << response_ptr->found << std::endl;
                
                if (response_ptr->found && response_ptr->user) {
                    out_user = *response_ptr->user;
                    return true;
                }
            }
            
            return false;
        } catch (const std::exception& e) {
            std::cerr << "GetUser failed: " << e.what() << std::endl;
            return false;
        }
    }
};

void printUser(const User& user) {
    std::cout << "User Details:" << std::endl;
    std::cout << "  ID: " << user.user_id << std::endl;
    std::cout << "  Username: " << user.username << std::endl;
    std::cout << "  Email: " << user.email << std::endl;
    std::cout << "  Active: " << (user.is_active ? "Yes" : "No") << std::endl;
    std::cout << "  Roles: ";
    for (const auto& role : user.roles) {
        std::cout << role << " ";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "BitRPC C++ Test Client" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        // Initialize protocol
        std::cout << "Initializing protocol..." << std::endl;
        bitrpc::example::protocol::ProtocolFactory::initialize();
        
        // Create client
        UserServiceClient client;
        
        // Connect to server
        std::cout << "Connecting to server..." << std::endl;
        client.connect("127.0.0.1", 8080);
        
        // Test 1: Successful admin login
        std::cout << "\n=== Test 1: Admin Login ===" << std::endl;
        User admin_user;
        std::string admin_token;
        if (client.login("admin", "password", admin_user, admin_token)) {
            std::cout << "Admin login successful!" << std::endl;
            printUser(admin_user);
            std::cout << "Token: " << admin_token << std::endl;
        } else {
            std::cout << "Admin login failed" << std::endl;
        }
        
        // Test 2: Successful user login
        std::cout << "\n=== Test 2: User Login ===" << std::endl;
        User regular_user;
        std::string user_token;
        if (client.login("user", "password", regular_user, user_token)) {
            std::cout << "User login successful!" << std::endl;
            printUser(regular_user);
            std::cout << "Token: " << user_token << std::endl;
        } else {
            std::cout << "User login failed" << std::endl;
        }
        
        // Test 3: Failed login
        std::cout << "\n=== Test 3: Failed Login ===" << std::endl;
        User dummy_user;
        std::string dummy_token;
        if (client.login("wrong", "credentials", dummy_user, dummy_token)) {
            std::cout << "This should not happen - login should have failed" << std::endl;
        } else {
            std::cout << "Failed login as expected" << std::endl;
        }
        
        // Test 4: Get user by ID
        std::cout << "\n=== Test 4: Get User by ID ===" << std::endl;
        User fetched_user;
        if (client.getUser(1, fetched_user)) {
            std::cout << "Successfully fetched user:" << std::endl;
            printUser(fetched_user);
        } else {
            std::cout << "Failed to fetch user" << std::endl;
        }
        
        // Test 5: Get non-existent user
        std::cout << "\n=== Test 5: Get Non-existent User ===" << std::endl;
        User nonexistent_user;
        if (client.getUser(999, nonexistent_user)) {
            std::cout << "This should not happen - user should not exist" << std::endl;
        } else {
            std::cout << "Correctly reported that user does not exist" << std::endl;
        }
        
        // Disconnect
        std::cout << "\nDisconnecting..." << std::endl;
        client.disconnect();
        
        std::cout << "\nAll tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}