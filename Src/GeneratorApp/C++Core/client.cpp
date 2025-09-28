#include "client.h"
#include "serialization.h"
#include <stdexcept>
#include <iostream>

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

BaseClient::BaseClient(std::shared_ptr<IRpcClient> client) : client_(client) {}

TcpRpcClient::TcpRpcClient() : socket_(nullptr), connected_(false) {
#ifdef _WIN32
    initialize_network();
#endif
}

TcpRpcClient::~TcpRpcClient() {
    disconnect();
#ifdef _WIN32
    cleanup_network();
#endif
}

void TcpRpcClient::connect(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (connected_) {
        disconnect();
    }

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket");
    }
#else
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
    }
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct hostent* host_info = gethostbyname(host.c_str());
    if (!host_info) {
        closesocket(sock);
        throw std::runtime_error("Failed to resolve hostname");
    }

    memcpy(&server_addr.sin_addr, host_info->h_addr_list[0], host_info->h_length);

    if (::connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        throw std::runtime_error("Failed to connect to server");
    }

    socket_ = reinterpret_cast<void*>(static_cast<intptr_t>(sock));
    connected_ = true;
}

void TcpRpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (connected_ && socket_) {
        SOCKET sock = reinterpret_cast<SOCKET>(socket_);
        closesocket(sock);
        socket_ = nullptr;
        connected_ = false;
    }
}

bool TcpRpcClient::is_connected() const {
    return connected_;
}

std::future<std::vector<uint8_t>> TcpRpcClient::call_async(const std::string& method, const std::vector<uint8_t>& request) {
    return std::async(std::launch::async, [this, method, request]() {
        return make_rpc_call(method, request);
    });
}

std::vector<uint8_t> TcpRpcClient::make_rpc_call(const std::string& method, const std::vector<uint8_t>& request) {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (!connected_ || !socket_) {
        throw std::runtime_error("Not connected to server");
    }

    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Send method name length and method name
    uint32_t method_length = static_cast<uint32_t>(method.length());
    send(sock, reinterpret_cast<const char*>(&method_length), sizeof(method_length), 0);
    send(sock, method.c_str(), static_cast<int>(method.length()), 0);

    // Send request length and request data
    uint32_t request_length = static_cast<uint32_t>(request.size());
    send(sock, reinterpret_cast<const char*>(&request_length), sizeof(request_length), 0);
    send(sock, reinterpret_cast<const char*>(request.data()), static_cast<int>(request.size()), 0);

    // Receive response length
    uint32_t response_length = 0;
    int bytes_received = recv(sock, reinterpret_cast<char*>(&response_length), sizeof(response_length), 0);
    if (bytes_received != sizeof(response_length)) {
        throw std::runtime_error("Failed to receive response length");
    }

    // Receive response data
    std::vector<uint8_t> response(response_length);
    size_t total_received = 0;
    while (total_received < response_length) {
        bytes_received = recv(sock, reinterpret_cast<char*>(response.data() + total_received),
                              static_cast<int>(response_length - total_received), 0);
        if (bytes_received <= 0) {
            throw std::runtime_error("Failed to receive response data");
        }
        total_received += bytes_received;
    }

    return response;
}

std::shared_ptr<IRpcClient> RpcClientFactory::create_tcp_client(const std::string& host, int port) {
    auto tcp_client = std::make_shared<TcpRpcClient>();
    tcp_client->connect(host, port);
    return tcp_client;
}

std::shared_ptr<TcpRpcClient> RpcClientFactory::create_tcp_client_native(const std::string& host, int port) {
    auto client = std::make_shared<TcpRpcClient>();
    client->connect(host, port);
    return client;
}

#ifdef _WIN32
void TcpRpcClient::initialize_network() {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
}

void TcpRpcClient::cleanup_network() {
    WSACleanup();
}
#endif

} // namespace bitrpc