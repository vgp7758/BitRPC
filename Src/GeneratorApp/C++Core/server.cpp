#include "server.h"
#include "serialization.h"
#include <stdexcept>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace bitrpc {

ServiceManager::ServiceManager() = default;
ServiceManager::~ServiceManager() = default;

void ServiceManager::register_service(std::shared_ptr<BaseService> service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[service->service_name()] = service;
}

void ServiceManager::unregister_service(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_.erase(service_name);
}

std::shared_ptr<BaseService> ServiceManager::get_service(const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = services_.find(service_name);
    return (it != services_.end()) ? it->second : nullptr;
}

bool ServiceManager::has_service(const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    return services_.find(service_name) != services_.end();
}

std::vector<std::string> ServiceManager::get_service_names() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    std::vector<std::string> names;
    names.reserve(services_.size());
    for (const auto& pair : services_) {
        names.push_back(pair.first);
    }
    return names;
}

BaseService::BaseService(const std::string& name) : name_(name) {}

bool BaseService::has_method(const std::string& method_name) const {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    return methods_.find(method_name) != methods_.end() ||
           async_methods_.find(method_name) != async_methods_.end();
}

void* BaseService::call_method(const std::string& method_name, void* request) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    auto it = methods_.find(method_name);
    if (it != methods_.end()) {
        return it->second(request);
    }
    throw std::runtime_error("Method not found: " + method_name);
}

std::future<void*> BaseService::call_method_async(const std::string& method_name, void* request) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    auto it = async_methods_.find(method_name);
    if (it != async_methods_.end()) {
        return it->second(request);
    }
    throw std::runtime_error("Async method not found: " + method_name);
}

TcpRpcServer::TcpRpcServer()
    : service_manager_(std::make_shared<ServiceManager>()),
      server_socket_(nullptr),
      is_running_(false) {
#ifdef _WIN32
    initialize_network();
#endif
}

TcpRpcServer::~TcpRpcServer() {
    stop();
#ifdef _WIN32
    cleanup_network();
#endif
}

void TcpRpcServer::start(int port) {
    start_async("0.0.0.0", port);
}

void TcpRpcServer::start_async(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(server_mutex_);

    if (is_running_) {
        return;
    }

#ifdef _WIN32
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create server socket");
    }
#else
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        throw std::runtime_error("Failed to create server socket");
    }
#endif

    // Set socket options
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(server_sock);
        throw std::runtime_error("Failed to bind server socket");
    }

    if (listen(server_sock, SOMAXCONN) != 0) {
        closesocket(server_sock);
        throw std::runtime_error("Failed to listen on server socket");
    }

    server_socket_ = reinterpret_cast<void*>(static_cast<intptr_t>(server_sock));
    is_running_ = true;

    // Start accept thread
    accept_thread_ = std::thread([this]() { accept_connections(); });
}

void TcpRpcServer::stop() {
    std::lock_guard<std::mutex> lock(server_mutex_);

    if (!is_running_) {
        return;
    }

    is_running_ = false;

    // Close server socket to stop accepting connections
    if (server_socket_) {
        SOCKET server_sock = reinterpret_cast<SOCKET>(server_socket_);
        closesocket(server_sock);
        server_socket_ = nullptr;
    }

    // Wait for accept thread to finish
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Wait for all client threads to finish
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.detach(); // Don't wait, just detach
        }
    }
    client_threads_.clear();
}

bool TcpRpcServer::is_running() const {
    return is_running_;
}

ServiceManager& TcpRpcServer::service_manager() {
    return *service_manager_;
}

void TcpRpcServer::accept_connections() {
    while (is_running_) {
        SOCKET server_sock = reinterpret_cast<SOCKET>(server_socket_);
        if (!server_sock) break;

        struct sockaddr_in client_addr;
#ifdef _WIN32
        int client_addr_len = sizeof(client_addr);
#else
        socklen_t client_addr_len = sizeof(client_addr);
#endif

        SOCKET client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            if (is_running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }

        // Start client handler thread
        client_threads_.emplace_back([this, client_sock]() {
            handle_client(reinterpret_cast<void*>(static_cast<intptr_t>(client_sock)));
        });
    }
}

void TcpRpcServer::handle_client(void* client_socket) {
    SOCKET sock = reinterpret_cast<SOCKET>(client_socket);

    try {
        while (is_running_) {
            // Receive method name length
            uint32_t method_length = 0;
            int bytes_received = recv(sock, reinterpret_cast<char*>(&method_length), sizeof(method_length), 0);
            if (bytes_received != sizeof(method_length)) break;

            // Receive method name
            std::vector<char> method_buffer(method_length + 1);
            bytes_received = recv(sock, method_buffer.data(), method_length, 0);
            if (bytes_received != method_length) break;
            method_buffer[method_length] = '\0';
            std::string method_name(method_buffer.data());

            // Receive request length
            uint32_t request_length = 0;
            bytes_received = recv(sock, reinterpret_cast<char*>(&request_length), sizeof(request_length), 0);
            if (bytes_received != sizeof(request_length)) break;

            // Receive request data
            std::vector<uint8_t> request_data(request_length);
            bytes_received = recv(sock, reinterpret_cast<char*>(request_data.data()), request_length, 0);
            if (bytes_received != request_length) break;

            // Parse method name and call service
            auto [service_name, method] = parse_method_name(method_name);
            auto service = service_manager_->get_service(service_name);

            if (!service) {
                std::cerr << "Service not found: " << service_name << std::endl;
                continue;
            }

            try {
                // Deserialize request
                StreamReader reader(request_data);
                void* response = service->call_method(method, &request_data[0]);

                // Serialize response (simplified - should use proper serialization)
                if (response) {
                    // For now, just send a success indicator
                    uint32_t response_length = 4;
                    uint32_t success = 1;
                    send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                    send(sock, reinterpret_cast<const char*>(&success), sizeof(success), 0);
                } else {
                    uint32_t response_length = 0;
                    send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error handling RPC call: " << e.what() << std::endl;
                uint32_t response_length = 0;
                send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in client handler: " << e.what() << std::endl;
    }

    closesocket(sock);
}

std::pair<std::string, std::string> TcpRpcServer::parse_method_name(const std::string& method) {
    size_t dot_pos = method.find('.');
    if (dot_pos == std::string::npos) {
        return {method, ""};
    }
    return {method.substr(0, dot_pos), method.substr(dot_pos + 1)};
}

#ifdef _WIN32
void TcpRpcServer::initialize_network() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
}

void TcpRpcServer::cleanup_network() {
    WSACleanup();
}
#endif

} // namespace bitrpc