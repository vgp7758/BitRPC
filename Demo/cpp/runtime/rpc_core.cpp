#include "rpc_core.h"
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <chrono>
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

// BufferSerializer implementation
BufferSerializer& BufferSerializer::instance() {
    static BufferSerializer instance;
    static bool initialized = false;
    if (!initialized) {
        instance.init_handlers();
        initialized = true;
    }
    return instance;
}

void BufferSerializer::register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler) {
    handlers_[type_hash] = handler;
    handlers_by_hash_code_[handler->hash_code()] = handler;
}

TypeHandler* BufferSerializer::get_handler(size_t type_hash) const {
    auto it = handlers_.find(type_hash);
    return it != handlers_.end() ? it->second.get() : nullptr;
}

TypeHandler* BufferSerializer::get_handler_by_hash_code(int hash_code) const {
    auto it = handlers_by_hash_code_.find(hash_code);
    return it != handlers_by_hash_code_.end() ? it->second.get() : nullptr;
}

void BufferSerializer::init_handlers() {
    register_handler<int32_t>(std::make_shared<Int32Handler>());
    register_handler<int64_t>(std::make_shared<Int64Handler>());
    register_handler<float>(std::make_shared<FloatHandler>());
    register_handler<double>(std::make_shared<DoubleHandler>());
    register_handler<bool>(std::make_shared<BoolHandler>());
    register_handler<std::string>(std::make_shared<StringHandler>());
    register_handler<std::vector<uint8_t>>(std::make_shared<BytesHandler>());
}

// BitMask implementation
BitMask::BitMask() : masks_(1, 0) {}
BitMask::BitMask(int size) : masks_(size, 0) {}

void BitMask::clear() {
    std::fill(masks_.begin(), masks_.end(), 0);
}

void BitMask::set_bit(int index, bool value) {
    int mask_index = index / 32;
    int bit_index = index % 32;
    
    if (mask_index >= static_cast<int>(masks_.size())) {
        masks_.resize(mask_index + 1, 0);
    }
    
    if (value) {
        masks_[mask_index] |= (1u << bit_index);
    } else {
        masks_[mask_index] &= ~(1u << bit_index);
    }
}

bool BitMask::get_bit(int index) const {
    int mask_index = index / 32;
    int bit_index = index % 32;
    
    if (mask_index >= static_cast<int>(masks_.size())) {
        return false;
    }
    
    return (masks_[mask_index] & (1u << bit_index)) != 0;
}

void BitMask::write(StreamWriter& writer) const {
    writer.write_int32(static_cast<int32_t>(masks_.size()));
    for (uint32_t mask : masks_) {
        writer.write_uint32(mask);
    }
}

void BitMask::read(StreamReader& reader) {
    int size = reader.read_int32();
    masks_.resize(size);
    for (int i = 0; i < size; ++i) {
        masks_[i] = reader.read_uint32();
    }
}

// StreamWriter implementation
StreamWriter::StreamWriter() : position_(0) {}

void StreamWriter::write_int32(int32_t value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value), 
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void StreamWriter::write_int64(int64_t value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value), 
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void StreamWriter::write_uint32(uint32_t value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value), 
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void StreamWriter::write_float(float value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value), 
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void StreamWriter::write_double(double value) {
    buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value), 
                   reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

void StreamWriter::write_bool(bool value) {
    write_int32(value ? 1 : 0);
}

void StreamWriter::write_string(const std::string& value) {
    if (value.empty()) {
        write_int32(-1);
    } else {
        write_int32(static_cast<int32_t>(value.size()));
        buffer_.insert(buffer_.end(), value.begin(), value.end());
    }
}

void StreamWriter::write_bytes(const std::vector<uint8_t>& bytes) {
    write_int32(static_cast<int32_t>(bytes.size()));
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

void StreamWriter::write_object(const void* obj, size_t type_hash) {
    if (!obj) {
        write_int32(-1);
        return;
    }
    
    auto& serializer = BufferSerializer::instance();
    auto handler = serializer.get_handler(type_hash);
    if (handler) {
        write_int32(handler->hash_code());
        handler->write(obj, *this);
    } else {
        write_int32(-1);
    }
}

std::vector<uint8_t> StreamWriter::to_array() const {
    return buffer_;
}

// StreamReader implementation
StreamReader::StreamReader(const std::vector<uint8_t>& data) : data_(data), position_(0) {}

int32_t StreamReader::read_int32() {
    if (position_ + sizeof(int32_t) > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    int32_t value = *reinterpret_cast<const int32_t*>(data_.data() + position_);
    position_ += sizeof(int32_t);
    return value;
}

int64_t StreamReader::read_int64() {
    if (position_ + sizeof(int64_t) > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    int64_t value = *reinterpret_cast<const int64_t*>(data_.data() + position_);
    position_ += sizeof(int64_t);
    return value;
}

uint32_t StreamReader::read_uint32() {
    if (position_ + sizeof(uint32_t) > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    uint32_t value = *reinterpret_cast<const uint32_t*>(data_.data() + position_);
    position_ += sizeof(uint32_t);
    return value;
}

float StreamReader::read_float() {
    if (position_ + sizeof(float) > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    float value = *reinterpret_cast<const float*>(data_.data() + position_);
    position_ += sizeof(float);
    return value;
}

double StreamReader::read_double() {
    if (position_ + sizeof(double) > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    double value = *reinterpret_cast<const double*>(data_.data() + position_);
    position_ += sizeof(double);
    return value;
}

bool StreamReader::read_bool() {
    return read_int32() != 0;
}

std::string StreamReader::read_string() {
    int32_t length = read_int32();
    if (length == -1) {
        return "";
    }
    if (length == 0) {
        return std::string();
    }
    if (position_ + length > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    std::string result(data_.data() + position_, data_.data() + position_ + length);
    position_ += length;
    return result;
}

std::vector<uint8_t> StreamReader::read_bytes() {
    int32_t length = read_int32();
    if (position_ + length > data_.size()) {
        throw std::runtime_error("Unexpected end of stream");
    }
    std::vector<uint8_t> result(data_.data() + position_, data_.data() + position_ + length);
    position_ += length;
    return result;
}

void* StreamReader::read_object() {
    int32_t hash_code = read_int32();
    if (hash_code == -1) {
        return nullptr;
    }
    
    auto& serializer = BufferSerializer::instance();
    auto handler = serializer.get_handler(hash_code);
    if (handler) {
        return handler->read(*this);
    }
    
    return nullptr;
}

// TcpRpcClient implementation
TcpRpcClient::TcpRpcClient() : socket_(reinterpret_cast<void*>(INVALID_SOCKET)), connected_(false) {
    initialize_network();
}

TcpRpcClient::~TcpRpcClient() {
    disconnect();
    cleanup_network();
}

void TcpRpcClient::connect(const std::string& host, int port) {
    disconnect();

    socket_ = reinterpret_cast<void*>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (reinterpret_cast<SOCKET>(socket_) == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        // Try to resolve hostname
        addrinfo hints{}, *result;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) {
            closesocket(reinterpret_cast<SOCKET>(socket_));
            socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
            throw std::runtime_error("Failed to resolve host: " + host);
        }

        sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(result->ai_addr);
        server_addr.sin_addr = addr_in->sin_addr;
        freeaddrinfo(result);
    }

    if (::connect(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(reinterpret_cast<SOCKET>(socket_));
        socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
        throw std::runtime_error("Failed to connect to server");
    }

    connected_ = true;
}

void TcpRpcClient::disconnect() {
    if (socket_ != reinterpret_cast<void*>(INVALID_SOCKET)) {
        closesocket(reinterpret_cast<SOCKET>(socket_));
        socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
    }
    connected_ = false;
}

bool TcpRpcClient::is_connected() const {
    return connected_ && socket_ != reinterpret_cast<void*>(INVALID_SOCKET);
}

std::vector<uint8_t> TcpRpcClient::call(const std::string& method, const std::vector<uint8_t>& request) {
    if (!is_connected()) {
        throw std::runtime_error("Client is not connected");
    }

    // Send request
    StreamWriter writer;
    writer.write_string(method);
    writer.write_bytes(request);
    auto data = writer.to_array();

    uint32_t length = static_cast<uint32_t>(data.size());
    if (send(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<const char*>(&length), sizeof(length), 0) == SOCKET_ERROR) {
        throw std::runtime_error("Failed to send request length");
    }

    if (send(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<const char*>(data.data()), data.size(), 0) == SOCKET_ERROR) {
        throw std::runtime_error("Failed to send request data");
    }

    // Read response length
    uint32_t response_length;
    if (recv(reinterpret_cast<SOCKET>(socket_), reinterpret_cast<char*>(&response_length), sizeof(response_length), 0) != sizeof(response_length)) {
        throw std::runtime_error("Failed to read response length");
    }

    // Read response data
    std::vector<uint8_t> response_data(response_length);
    size_t total_received = 0;
    while (total_received < response_length) {
        int bytes_received = recv(reinterpret_cast<SOCKET>(socket_), 
                                 reinterpret_cast<char*>(response_data.data() + total_received),
                                 response_length - total_received, 0);
        if (bytes_received <= 0) {
            throw std::runtime_error("Failed to read response data");
        }
        total_received += bytes_received;
    }

    return response_data;
}

// TypeHandler implementations
void Int32Handler::write(const void* obj, StreamWriter& writer) const {
    writer.write_int32(*static_cast<const int32_t*>(obj));
}

void* Int32Handler::read(StreamReader& reader) const {
    int32_t* value = new int32_t();
    *value = reader.read_int32();
    return value;
}

void Int64Handler::write(const void* obj, StreamWriter& writer) const {
    writer.write_int64(*static_cast<const int64_t*>(obj));
}

void* Int64Handler::read(StreamReader& reader) const {
    int64_t* value = new int64_t();
    *value = reader.read_int64();
    return value;
}

void FloatHandler::write(const void* obj, StreamWriter& writer) const {
    writer.write_float(*static_cast<const float*>(obj));
}

void* FloatHandler::read(StreamReader& reader) const {
    float* value = new float();
    *value = reader.read_float();
    return value;
}

void DoubleHandler::write(const void* obj, StreamWriter& writer) const {
    writer.write_double(*static_cast<const double*>(obj));
}

void* DoubleHandler::read(StreamReader& reader) const {
    double* value = new double();
    *value = reader.read_double();
    return value;
}

void BoolHandler::write(const void* obj, StreamWriter& writer) const {
    writer.write_bool(*static_cast<const bool*>(obj));
}

void* BoolHandler::read(StreamReader& reader) const {
    bool* value = new bool();
    *value = reader.read_bool();
    return value;
}

void StringHandler::write(const void* obj, StreamWriter& writer) const {
    const std::string* str = static_cast<const std::string*>(obj);
    writer.write_string(*str);
}

void* StringHandler::read(StreamReader& reader) const {
    std::string* str = new std::string();
    *str = reader.read_string();
    return str;
}

void BytesHandler::write(const void* obj, StreamWriter& writer) const {
    const std::vector<uint8_t>* bytes = static_cast<const std::vector<uint8_t>*>(obj);
    writer.write_bytes(*bytes);
}

void* BytesHandler::read(StreamReader& reader) const {
    std::vector<uint8_t>* bytes = new std::vector<uint8_t>();
    *bytes = reader.read_bytes();
    return bytes;
}

// ServiceBase implementation
ServiceBase::ServiceBase(const std::string& name) : name_(name) {}

bool ServiceBase::has_method(const std::string& method_name) const {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    return methods_.find(method_name) != methods_.end();
}

void* ServiceBase::call_method(const std::string& method_name, void* request) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    auto it = methods_.find(method_name);
    if (it != methods_.end()) {
        return it->second(request);
    }
    throw std::runtime_error("Method '" + method_name + "' not found");
}

// TcpRpcServer implementation
TcpRpcServer::TcpRpcServer() : server_socket_(reinterpret_cast<void*>(INVALID_SOCKET)), is_running_(false) {
    initialize_network();
}

TcpRpcServer::~TcpRpcServer() {
    stop();
    cleanup_network();
}

void TcpRpcServer::register_service(std::shared_ptr<BaseService> service) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    services_[service->service_name()] = service;
}

void TcpRpcServer::start(int port) {
    if (is_running_) {
        return;
    }

    server_socket_ = reinterpret_cast<void*>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (reinterpret_cast<SOCKET>(server_socket_) == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create server socket");
    }

    int opt = 1;
    setsockopt(reinterpret_cast<SOCKET>(server_socket_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(reinterpret_cast<SOCKET>(server_socket_), reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(reinterpret_cast<SOCKET>(server_socket_));
        server_socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
        throw std::runtime_error("Failed to bind server socket");
    }

    if (listen(reinterpret_cast<SOCKET>(server_socket_), SOMAXCONN) == SOCKET_ERROR) {
        closesocket(reinterpret_cast<SOCKET>(server_socket_));
        server_socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
        throw std::runtime_error("Failed to listen on server socket");
    }

    is_running_ = true;

    // Start accepting connections in a separate thread
    std::thread([this, port]() {
        std::cout << "Server started on port " << port << std::endl;
        while (is_running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            SOCKET client_socket = accept(reinterpret_cast<SOCKET>(server_socket_), reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_socket == INVALID_SOCKET) {
                if (is_running_) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                }
                continue;
            }

            // Handle client in a separate thread
            client_threads_.emplace_back([this, client_socket]() {
                handle_client(reinterpret_cast<void*>(client_socket));
            });
        }
    }).detach();
}

void TcpRpcServer::stop() {
    if (!is_running_) {
        return;
    }

    is_running_ = false;

    if (server_socket_ != reinterpret_cast<void*>(INVALID_SOCKET)) {
        closesocket(reinterpret_cast<SOCKET>(server_socket_));
        server_socket_ = reinterpret_cast<void*>(INVALID_SOCKET);
    }

    // Wait for all client threads to finish
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
}

void TcpRpcServer::handle_client(void* client_socket) {
    SOCKET sock = reinterpret_cast<SOCKET>(client_socket);

    try {
        while (is_running_) {
            // Read request length
            uint32_t length;
            int bytes_read = recv(sock, reinterpret_cast<char*>(&length), sizeof(length), 0);
            if (bytes_read != sizeof(length)) {
                break; // Connection closed or error
            }

            // Read request data
            std::vector<uint8_t> request_data(length);
            size_t total_read = 0;
            while (total_read < length) {
                bytes_read = recv(sock, reinterpret_cast<char*>(request_data.data() + total_read),
                                 length - total_read, 0);
                if (bytes_read <= 0) {
                    break;
                }
                total_read += bytes_read;
            }

            if (total_read != length) {
                break;
            }

            // Process request
            StreamReader reader(request_data);
            std::string method_name = reader.read_string();
            auto request = reader.read_bytes();

            // Parse method name
            auto [service_name, operation_name] = parse_method_name(method_name);

            // Find service
            std::shared_ptr<BaseService> service;
            {
                std::lock_guard<std::mutex> lock(services_mutex_);
                auto it = services_.find(service_name);
                if (it != services_.end()) {
                    service = it->second;
                }
            }

            StreamWriter response_writer;

            if (service && service->has_method(operation_name)) {
                try {
                    // Call method (simplified - in real implementation you'd need proper deserialization)
                    auto response = service->call_method(operation_name, &request);
                    response_writer.write_bytes(std::vector<uint8_t>{1}); // Success indicator
                    // In real implementation, serialize and write the actual response
                } catch (const std::exception& e) {
                    response_writer.write_bytes(std::vector<uint8_t>{0}); // Error indicator
                    response_writer.write_string(e.what());
                }
            } else {
                response_writer.write_bytes(std::vector<uint8_t>{0}); // Error indicator
                response_writer.write_string("Service or method not found");
            }

            // Send response
            auto response_data = response_writer.to_array();
            uint32_t response_length = static_cast<uint32_t>(response_data.size());

            send(sock, reinterpret_cast<const char*>(&response_length), sizeof(response_length), 0);
            send(sock, reinterpret_cast<const char*>(response_data.data()), response_data.size(), 0);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling client: " << e.what() << std::endl;
    }

    closesocket(sock);
}

std::pair<std::string, std::string> TcpRpcServer::parse_method_name(const std::string& method) {
    size_t dot_pos = method.find('.');
    if (dot_pos != std::string::npos) {
        return {method.substr(0, dot_pos), method.substr(dot_pos + 1)};
    }
    return {method, ""};
}

void TcpRpcServer::initialize_network() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void TcpRpcServer::cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpRpcClient::initialize_network() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

void TcpRpcClient::cleanup_network() {
#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace bitrpc