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

std::vector<uint8_t> TcpRpcClient::call(const std::string& method, const std::vector<uint8_t>& request) {
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

// TcpRpcClientAsync implementation
TcpRpcClientAsync::TcpRpcClientAsync() : socket_(nullptr), connected_(false) {
#ifdef _WIN32
    initialize_network();
#endif
}

TcpRpcClientAsync::~TcpRpcClientAsync() {
    disconnect();
#ifdef _WIN32
    cleanup_network();
#endif
}

void TcpRpcClientAsync::connect(const std::string& host, int port) {
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
    host_ = host;
    port_ = port;
}

void TcpRpcClientAsync::disconnect() {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (connected_ && socket_) {
        SOCKET sock = reinterpret_cast<SOCKET>(socket_);
        closesocket(sock);
        socket_ = nullptr;
        connected_ = false;
    }
}

bool TcpRpcClientAsync::is_connected() const {
    return connected_;
}

std::future<std::vector<uint8_t>> TcpRpcClientAsync::call_async(const std::string& method, const std::vector<uint8_t>& request) {
    return std::async(std::launch::async, [this, method, request]() {
        return make_rpc_call(method, request);
    });
}

std::shared_ptr<StreamResponseReader> TcpRpcClientAsync::stream_async(const std::string& method, const std::vector<uint8_t>& request) {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (!connected_ || !socket_) {
        throw std::runtime_error("Not connected to server");
    }

    // Send stream request
    send_stream_request(method, request);

    // Create stream reader - pass the socket and serializer
    auto& serializer = BufferSerializer::instance();
    auto reader = std::make_shared<TcpStreamResponseReader>(socket_, 0, serializer);

    return reader;
}


void TcpRpcClientAsync::send_stream_request(const std::string& method, const std::vector<uint8_t>& request) {
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Send stream indicator (special method name)
    std::string stream_method = "STREAM:" + method;
    uint32_t method_length = static_cast<uint32_t>(stream_method.length());
    send(sock, reinterpret_cast<const char*>(&method_length), sizeof(method_length), 0);
    send(sock, stream_method.c_str(), static_cast<int>(stream_method.length()), 0);

    // Send request length and request data
    uint32_t request_length = static_cast<uint32_t>(request.size());
    send(sock, reinterpret_cast<const char*>(&request_length), sizeof(request_length), 0);
    send(sock, reinterpret_cast<const char*>(request.data()), static_cast<int>(request.size()), 0);
}

void TcpRpcClientAsync::initialize_network() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif
}

void TcpRpcClientAsync::cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

std::shared_ptr<TcpRpcClientAsync> RpcClientFactory::create_tcp_client_async(const std::string& host, int port) {
    auto client = std::make_shared<TcpRpcClientAsync>();
    client->connect(host, port);
    return client;
}

// TcpStreamResponseReader implementation
TcpStreamResponseReader::TcpStreamResponseReader(void* socket, int response_type_hash, BufferSerializer& serializer)
    : socket_(socket), response_type_hash_(response_type_hash), serializer_(serializer),
      stream_ended_(false), has_error_(false), connection_closed_(false) {
}

TcpStreamResponseReader::~TcpStreamResponseReader() {
    close();
}

std::vector<uint8_t> TcpStreamResponseReader::read_next() {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (has_error_) {
        throw std::runtime_error(error_message_);
    }

    if (stream_ended_ || connection_closed_) {
        return {};
    }

    std::vector<uint8_t> frame_data;
    if (!read_next_frame(frame_data)) {
        return {};
    }

    // Check for special markers
    if (frame_data.size() == sizeof(uint32_t)) {
        uint32_t marker = *reinterpret_cast<uint32_t*>(frame_data.data());
        if (marker == NetworkFrameHandler::STREAM_END_MARKER) {
            stream_ended_ = true;
            return {};
        } else if (marker == NetworkFrameHandler::ERROR_MARKER) {
            has_error_ = true;
            error_message_ = "Stream error from server";
            return {};
        }
    }

    // For now, return the raw frame data
    // In a real implementation, this would deserialize the data
    return frame_data;
}

bool TcpStreamResponseReader::has_more() const {
    return !stream_ended_ && !connection_closed_ && !has_error_;
}

void TcpStreamResponseReader::close() {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    stream_ended_ = true;
}

bool TcpStreamResponseReader::has_error() const {
    return has_error_;
}

std::string TcpStreamResponseReader::get_error_message() const {
    return error_message_;
}

bool TcpStreamResponseReader::read_next_frame(std::vector<uint8_t>& data) {
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Read frame length
    uint32_t frame_length = 0;
    int bytes_received = recv(sock, reinterpret_cast<char*>(&frame_length), sizeof(frame_length), 0);

    if (bytes_received <= 0) {
        connection_closed_ = true;
        return false;
    }

    if (bytes_received != sizeof(frame_length)) {
        mark_error("Failed to read frame length");
        return false;
    }

    // Check for special markers
    if (frame_length == NetworkFrameHandler::STREAM_END_MARKER ||
        frame_length == NetworkFrameHandler::ERROR_MARKER) {
        data.resize(sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(data.data()) = frame_length;
        return true;
    }

    // Read frame data
    data.resize(frame_length);
    size_t total_received = 0;
    while (total_received < frame_length) {
        bytes_received = recv(sock, reinterpret_cast<char*>(data.data() + total_received),
                             static_cast<int>(frame_length - total_received), 0);
        if (bytes_received <= 0) {
            connection_closed_ = true;
            return false;
        }
        total_received += bytes_received;
    }

    return true;
}

void TcpStreamResponseReader::mark_error(const std::string& error) {
    has_error_ = true;
    error_message_ = error;
    stream_ended_ = true;
}

// TcpStreamResponseWriter implementation
TcpStreamResponseWriter::TcpStreamResponseWriter(void* socket, int response_type_hash, BufferSerializer& serializer)
    : socket_(socket), response_type_hash_(response_type_hash), serializer_(serializer),
      stream_ended_(false), has_error_(false), connection_valid_(true) {
}

TcpStreamResponseWriter::~TcpStreamResponseWriter() {
    close();
}

bool TcpStreamResponseWriter::write(const void* item) {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (has_error_ || !connection_valid_ || stream_ended_) {
        return false;
    }

    try {
        // Serialize the item
        StreamWriter writer;
        serializer_.serialize(item, writer);
        auto data = writer.to_array();

        // Write the frame
        return write_frame(data);
    } catch (const std::exception& e) {
        mark_error("Serialization error: " + std::string(e.what()));
        return false;
    }
}

bool TcpStreamResponseWriter::is_valid() const {
    return connection_valid_ && !has_error_ && !stream_ended_;
}

void TcpStreamResponseWriter::close() {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (connection_valid_ && !stream_ended_) {
        // Send end marker
        NetworkFrameHandler::write_end_marker(socket_);
        stream_ended_ = true;
    }
}

bool TcpStreamResponseWriter::has_error() const {
    return has_error_;
}

std::string TcpStreamResponseWriter::get_error_message() const {
    return error_message_;
}

bool TcpStreamResponseWriter::write_frame(const std::vector<uint8_t>& data) {
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Write frame length
    uint32_t frame_length = static_cast<uint32_t>(data.size());
    int bytes_sent = send(sock, reinterpret_cast<const char*>(&frame_length), sizeof(frame_length), 0);
    if (bytes_sent != sizeof(frame_length)) {
        mark_error("Failed to send frame length");
        return false;
    }

    // Write frame data
    bytes_sent = send(sock, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
    if (bytes_sent != static_cast<int>(data.size())) {
        mark_error("Failed to send frame data");
        return false;
    }

    return true;
}

void TcpStreamResponseWriter::mark_error(const std::string& error) {
    has_error_ = true;
    error_message_ = error;
    connection_valid_ = false;

    // Try to send error marker
    if (socket_) {
        NetworkFrameHandler::write_error_marker(socket_, error);
    }
}

std::shared_ptr<IRpcClient> RpcClientFactory::create_tcp_client(const std::string& host, int port) {
    auto tcp_client = std::make_shared<TcpRpcClientAsync>();
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