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
    return methods_.find(method_name) != methods_.end();
}

bool BaseService::has_async_method(const std::string& method_name) const {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    return async_methods_.find(method_name) != async_methods_.end();
}

bool BaseService::has_stream_method(const std::string& method_name) const {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    return stream_methods_.find(method_name) != stream_methods_.end();
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

std::shared_ptr<StreamResponseReader> BaseService::call_stream_method(const std::string& method_name,
                                                                     const std::vector<uint8_t>& request_bytes) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    auto it = stream_methods_.find(method_name);
    if (it != stream_methods_.end()) {
        // We pass bytes pointer to registered wrapper which deserializes
        return it->second(const_cast<void*>(static_cast<const void*>(&request_bytes)));
    }
    throw std::runtime_error("Stream method not found: " + method_name);
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

static bool recv_all_helper(SOCKET sock, char* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        int r = recv(sock, buf + off, static_cast<int>(len - off), 0);
        if (r <= 0) return false;
        off += r;
    }
    return true;
}

static bool is_printable_ascii(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

void TcpRpcServer::handle_client(void* client_socket) {
    SOCKET sock = reinterpret_cast<SOCKET>(client_socket);

    try {
        while (is_running_) {
            // Payload: either [payload_len][method_len][method][request]
            // or [payload_len][method (ASCII)][request]
            uint32_t payload_length = 0;
            if (!recv_all_helper(sock, reinterpret_cast<char*>(&payload_length), sizeof(payload_length))) break;
            if (payload_length == 0) continue;

            std::vector<uint8_t> payload(payload_length);
            if (!recv_all_helper(sock, reinterpret_cast<char*>(payload.data()), payload.size())) break;

            std::string method_name;
            std::vector<uint8_t> request_bytes;

            // Try format A: explicit method length prefix inside payload
            if (payload.size() >= 4) {
                uint32_t mlen = *reinterpret_cast<const uint32_t*>(payload.data());
                if (mlen > 0 && 4 + mlen <= payload.size() && is_printable_ascii(payload.data() + 4, mlen)) {
                    method_name.assign(reinterpret_cast<const char*>(payload.data() + 4), mlen);
                    request_bytes.assign(payload.begin() + 4 + mlen, payload.end());
                }
            }

            // Fallback format B: ASCII prefix until first non-printable
            if (method_name.empty()) {
                size_t i = 0;
                while (i < payload.size()) {
                    unsigned char c = payload[i];
                    if (c < 32 || c > 126) break;
                    ++i;
                }
                method_name.assign(reinterpret_cast<const char*>(payload.data()), i);
                request_bytes.assign(payload.begin() + i, payload.end());
            }

            auto method_pair = parse_method_name(method_name);
            auto& service_name = method_pair.first;
            auto& method = method_pair.second;
            auto service = service_manager_->get_service(service_name);

            if (!service) {
                std::cerr << "Service not found: " << service_name << std::endl;
                // Respond with empty
                uint32_t response_length = 0;
                send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                continue;
            }

            try {
                if (service->has_stream_method(method)) {
                    // Handle streaming using the service-provided StreamResponseReader
                    auto reader = service->call_stream_method(method, request_bytes);
                    if (!reader) {
                        uint32_t zero = 0; // end-of-stream
                        send(sock, reinterpret_cast<const char*>(&zero), sizeof(zero), 0);
                        continue;
                    }

                    while (reader->has_more()) {
                        auto frame = reader->read_next();
                        uint32_t flen = static_cast<uint32_t>(frame.size());
                        // write frame
                        send(sock, reinterpret_cast<const char*>(&flen), sizeof(flen), 0);
                        if (flen) {
                            size_t off = 0; while (off < frame.size()) {
                                int s = send(sock, reinterpret_cast<const char*>(frame.data() + off),
                                              static_cast<int>(frame.size() - off), 0);
                                if (s <= 0) { break; }
                                off += s;
                            }
                        }
                        if (flen == 0) break; // end marker by reader
                    }
                    // Explicit end marker (0) to align with client
                    uint32_t zero = 0; send(sock, reinterpret_cast<const char*>(&zero), sizeof(zero), 0);
                    continue;
                }

                if (service->has_method(method)) {
                    // Synchronous method wrapper (already serializes response with type hash)
                    void* response = service->call_method(method, static_cast<void*>(&request_bytes));

                    if (response) {
                        auto response_vector = static_cast<std::vector<uint8_t>*>(response);
                        uint32_t response_length = static_cast<uint32_t>(response_vector->size());
                        send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                        if (response_length > 0) {
                            send(sock, reinterpret_cast<const char*>(response_vector->data()), response_length, 0);
                        }
                        delete response_vector;
                    } else {
                        uint32_t response_length = 0;
                        send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                    }
                    continue;
                }

                if (service->has_async_method(method)) {
                    // Async method
                    void* dummy = static_cast<void*>(&request_bytes);
                    auto future_response = service->call_method_async(method, dummy);
                    auto response_ptr = future_response.get();
                    if (response_ptr) {
                        auto response_vector = static_cast<std::vector<uint8_t>*>(response_ptr);
                        uint32_t response_length = static_cast<uint32_t>(response_vector->size());
                        send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                        if (response_length > 0) {
                            send(sock, reinterpret_cast<const char*>(response_vector->data()), response_length, 0);
                        }
                        delete response_vector;
                    } else {
                        uint32_t response_length = 0;
                        send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
                    }
                    continue;
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