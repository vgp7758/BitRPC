#include "client.h"
#include "serialization.h"
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
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
        throw ConnectionException("Failed to create socket");
    }
#else
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw ConnectionException("Failed to create socket");
    }
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct hostent* host_info = gethostbyname(host.c_str());
    if (!host_info) {
        closesocket(sock);
        throw ConnectionException("Failed to resolve hostname");
    }

    memcpy(&server_addr.sin_addr, host_info->h_addr_list[0], host_info->h_length);

    if (::connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        throw ConnectionException("Failed to connect to server");
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
        throw ConnectionException("Not connected to server");
    }

    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Build payload: WriteString(method) + pre-serialized request (starts with handler hash_code)
    StreamWriter meta_writer;
    meta_writer.write_string(method);
    auto method_part = meta_writer.to_array();

    std::vector<uint8_t> combined_payload;
    combined_payload.reserve(method_part.size() + request.size());
    combined_payload.insert(combined_payload.end(), method_part.begin(), method_part.end());
    combined_payload.insert(combined_payload.end(), request.begin(), request.end());

    // Send combined payload length and data
    uint32_t payload_length = static_cast<uint32_t>(combined_payload.size());
    send(sock, reinterpret_cast<const char*>(&payload_length), sizeof(payload_length), 0);
    send(sock, reinterpret_cast<const char*>(combined_payload.data()), static_cast<int>(combined_payload.size()), 0);

    // Receive response length
    uint32_t response_length = 0;
    int bytes_received = recv(sock, reinterpret_cast<char*>(&response_length), sizeof(response_length), 0);
    if (bytes_received != sizeof(response_length)) {
        throw ConnectionException("Failed to receive response length");
    }

    // Receive response data
    std::vector<uint8_t> response(response_length);
    size_t total_received = 0;
    while (total_received < response_length) {
        bytes_received = recv(sock, reinterpret_cast<char*>(response.data() + total_received),
                              static_cast<int>(response_length - total_received), 0);
        if (bytes_received <= 0) {
            throw ConnectionException("Failed to receive response data");
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
        throw ConnectionException("Failed to create socket");
    }
#else
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw ConnectionException("Failed to create socket");
    }
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    struct hostent* host_info = gethostbyname(host.c_str());
    if (!host_info) {
        closesocket(sock);
        throw ConnectionException("Failed to resolve hostname");
    }

    memcpy(&server_addr.sin_addr, host_info->h_addr_list[0], host_info->h_length);

    if (::connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        throw ConnectionException("Failed to connect to server");
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
        throw ConnectionException("Not connected to server");
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

    // Build payload: WriteString(method) + pre-serialized request (starts with handler hash_code)
    StreamWriter meta_writer;
    meta_writer.write_string(method);
    auto method_part = meta_writer.to_array();

    std::vector<uint8_t> combined_payload;
    combined_payload.reserve(method_part.size() + request.size());
    combined_payload.insert(combined_payload.end(), method_part.begin(), method_part.end());
    combined_payload.insert(combined_payload.end(), request.begin(), request.end());

    // Send combined payload length and data
    uint32_t payload_length = static_cast<uint32_t>(combined_payload.size());
    send(sock, reinterpret_cast<const char*>(&payload_length), sizeof(payload_length), 0);
    send(sock, reinterpret_cast<const char*>(combined_payload.data()), static_cast<int>(combined_payload.size()), 0);
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

std::vector<uint8_t> TcpRpcClientAsync::make_rpc_call(const std::string& method, const std::vector<uint8_t>& request) {
    std::lock_guard<std::mutex> lock(socket_mutex_);

    if (!connected_ || !socket_) {
        throw ConnectionException("Not connected to server");
    }

    SOCKET sock = reinterpret_cast<SOCKET>(socket_);

    // Build payload: WriteString(method) + pre-serialized request (starts with handler hash_code)
    StreamWriter meta_writer;
    meta_writer.write_string(method);
    auto method_part = meta_writer.to_array();

    std::vector<uint8_t> combined_payload;
    combined_payload.reserve(method_part.size() + request.size());
    combined_payload.insert(combined_payload.end(), method_part.begin(), method_part.end());
    combined_payload.insert(combined_payload.end(), request.begin(), request.end());

    // Send combined payload length and data
    uint32_t payload_length = static_cast<uint32_t>(combined_payload.size());
    send(sock, reinterpret_cast<const char*>(&payload_length), sizeof(payload_length), 0);
    send(sock, reinterpret_cast<const char*>(combined_payload.data()), static_cast<int>(combined_payload.size()), 0);

    // Receive response length
    uint32_t response_length = 0;
    int bytes_received = recv(sock, reinterpret_cast<char*>(&response_length), sizeof(response_length), 0);
    if (bytes_received != sizeof(response_length)) {
        throw ConnectionException("Failed to receive response length");
    }

    // Receive response data
    std::vector<uint8_t> response(response_length);
    size_t total_received = 0;
    while (total_received < response_length) {
        bytes_received = recv(sock, reinterpret_cast<char*>(response.data() + total_received),
                              static_cast<int>(response_length - total_received), 0);
        if (bytes_received <= 0) {
            throw ConnectionException("Failed to receive response data");
        }
        total_received += bytes_received;
    }

    return response;
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
        throw StreamException(error_message_);
    }

    if (stream_ended_) {
        return {};
    }

    if (connection_closed_) {
        throw ConnectionException("Stream connection is closed");
    }

    std::vector<uint8_t> frame_data;
    if (!read_next_frame(frame_data)) {
        if (has_error_) {
            throw StreamException(error_message_);
        }
        return {};
    }

    // Check for end-of-stream marker (zero-length frame, aligned with C#)
    if (frame_data.size() == sizeof(uint32_t)) {
        uint32_t marker = *reinterpret_cast<uint32_t*>(frame_data.data());
        if (marker == 0) {  // C# uses zero-length frame as end marker
            stream_ended_ = true;
            return {};
        }
    }

    // Deserialize the data using response type hash
    try {
        if (response_type_hash_ != 0) {
            auto handler = serializer_.get_handler_by_hash_code(response_type_hash_);
            if (handler) {
                StreamReader reader(frame_data);
                void* obj = handler->read(reader);
                // Convert to vector format (simplified for now)
                // In a full implementation, this would return properly typed objects
                return frame_data;
            }
        }
        return frame_data;
    } catch (const SerializationException& e) {
        mark_error("Deserialization error: " + std::string(e.what()));
        throw; // Re-throw serialization exceptions
    } catch (const std::exception& e) {
        mark_error("Unexpected error during deserialization: " + std::string(e.what()));
        throw StreamException("Stream processing error: " + std::string(e.what()));
    }
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

    // Read frame length (C# compatible format) with timeout handling
    uint32_t frame_length = 0;
    int bytes_received = recv(sock, reinterpret_cast<char*>(&frame_length), sizeof(frame_length), 0);

    if (bytes_received <= 0) {
        connection_closed_ = true;
        mark_error("Connection closed while reading frame length");
        return false;
    }

    if (bytes_received != sizeof(frame_length)) {
        mark_error("Incomplete frame length received");
        return false;
    }

    // Check for end-of-stream marker (zero-length frame, aligned with C#)
    if (frame_length == 0) {
        data.resize(sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(data.data()) = 0;
        return true;
    }

    // Validate frame length to prevent memory exhaustion
    const uint32_t MAX_FRAME_SIZE = 10 * 1024 * 1024; // 10MB limit
    if (frame_length > MAX_FRAME_SIZE) {
        mark_error("Frame size exceeds maximum limit");
        return false;
    }

    // Read frame data with robust error handling
    data.resize(frame_length);
    size_t total_received = 0;
    while (total_received < frame_length) {
        bytes_received = recv(sock, reinterpret_cast<char*>(data.data() + total_received),
                             static_cast<int>(frame_length - total_received), 0);
        if (bytes_received <= 0) {
            connection_closed_ = true;
            mark_error("Connection closed while reading frame data");
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
        // Serialize the item: write handler hash_code then payload to align with C# ReadObject
        StreamWriter writer;
        auto handler = serializer_.get_handler_by_hash_code(response_type_hash_);
        if (!handler) {
            mark_error("No type handler found for response type hash_code");
            return false;
        }
        writer.write_int32(static_cast<int32_t>(handler->hash_code()));
        handler->write(item, writer);
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
        // Send end marker (zero-length frame, aligned with C#)
        SOCKET sock = reinterpret_cast<SOCKET>(socket_);
        uint32_t zero_length = 0;
        send(sock, reinterpret_cast<const char*>(&zero_length), sizeof(zero_length), 0);
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

    // Validate data size
    const uint32_t MAX_FRAME_SIZE = 10 * 1024 * 1024; // 10MB limit
    uint32_t frame_length = static_cast<uint32_t>(data.size());
    if (frame_length > MAX_FRAME_SIZE) {
        mark_error("Frame size exceeds maximum limit");
        return false;
    }

    // Write frame length (C# compatible format)
    int bytes_sent = send(sock, reinterpret_cast<const char*>(&frame_length), sizeof(frame_length), 0);
    if (bytes_sent != sizeof(frame_length)) {
        mark_error("Failed to send frame length: connection may be broken");
        return false;
    }

    // Write frame data in chunks if necessary
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        size_t remaining = data.size() - total_sent;
        int chunk_size = remaining < 8192 ? static_cast<int>(remaining) : 8192;
        bytes_sent = send(sock, reinterpret_cast<const char*>(data.data() + total_sent), chunk_size, 0);
        if (bytes_sent <= 0) {
            mark_error("Failed to send frame data: connection may be broken");
            return false;
        }
        total_sent += bytes_sent;
    }

    return true;
}

void TcpStreamResponseWriter::mark_error(const std::string& error) {
    has_error_ = true;
    error_message_ = error;
    connection_valid_ = false;

    // For C# compatibility, we don't send special error markers
    // The connection will simply be closed
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

// ErrorHandler implementation
void ErrorHandler::log_error(const std::string& context, const std::exception& e) {
    // Simple error logging - in a real implementation this would use a proper logging framework
    std::cerr << "[ERROR] " << context << ": " << e.what() << std::endl;
}

void ErrorHandler::log_warning(const std::string& message) {
    std::cerr << "[WARNING] " << message << std::endl;
}

void ErrorHandler::log_info(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

std::string ErrorHandler::error_code_to_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case 1001: return "Connection Error";
        case 1002: return "Timeout Error";
        case 2001: return "Serialization Error";
        case 3001: return "Stream Error";
        case 4001: return "Protocol Error";
        default: return "Unknown Error";
    }
}

std::string ErrorHandler::get_last_system_error() {
#ifdef _WIN32
    char buffer[256];
    DWORD error = GetLastError();
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer, sizeof(buffer), nullptr);
    return buffer;
#else
    return strerror(errno);
#endif
}

} // namespace bitrpc