#include "rpc_core.h"
#include <iostream>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <chrono>

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
    return instance;
}

void BufferSerializer::register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler) {
    handlers_[type_hash] = handler;
}

TypeHandler* BufferSerializer::get_handler(size_t type_hash) const {
    auto it = handlers_.find(type_hash);
    return it != handlers_.end() ? it->second.get() : nullptr;
}

// BitMask implementation
BitMask::BitMask(int size) : masks_(size, 0) {}

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
TcpRpcClient::TcpRpcClient() : socket_(INVALID_SOCKET), connected_(false) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

TcpRpcClient::~TcpRpcClient() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpRpcClient::connect(const std::string& host, int port) {
    disconnect();
    
    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    if (getaddrinfo(host.c_str(), port_str, &hints, &result) != 0) {
        throw std::runtime_error("Failed to resolve host");
    }
    
    socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_ == INVALID_SOCKET) {
        freeaddrinfo(result);
        throw std::runtime_error("Failed to create socket");
    }
    
    if (::connect(socket_, result->ai_addr, result->ai_addrlen) == SOCKET_ERROR) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        freeaddrinfo(result);
        throw std::runtime_error("Failed to connect to server");
    }
    
    freeaddrinfo(result);
    connected_ = true;
}

void TcpRpcClient::disconnect() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    connected_ = false;
}

bool TcpRpcClient::is_connected() const {
    return connected_;
}

std::vector<uint8_t> TcpRpcClient::call(const std::string& method, const std::vector<uint8_t>& request) {
    if (!connected_) {
        throw std::runtime_error("Client is not connected");
    }
    
    // Write method name and request
    bitrpc::StreamWriter writer;
    writer.write_string(method);
    writer.write_bytes(request);
    
    auto data = writer.to_array();
    
    // Send length prefix
    uint32_t length = static_cast<uint32_t>(data.size());
    if (send(socket_, reinterpret_cast<const char*>(&length), sizeof(length), 0) != sizeof(length)) {
        throw std::runtime_error("Failed to send length");
    }
    
    // Send data
    if (send(socket_, reinterpret_cast<const char*>(data.data()), data.size(), 0) != static_cast<int>(data.size())) {
        throw std::runtime_error("Failed to send data");
    }
    
    // Read response length
    uint32_t response_length;
    if (recv(socket_, reinterpret_cast<char*>(&response_length), sizeof(response_length), 0) != sizeof(response_length)) {
        throw std::runtime_error("Failed to read response length");
    }
    
    // Read response data
    std::vector<uint8_t> response(response_length);
    size_t total_read = 0;
    while (total_read < response_length) {
        int bytes_read = recv(socket_, reinterpret_cast<char*>(response.data() + total_read), 
                               response_length - total_read, 0);
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read response data");
        }
        total_read += bytes_read;
    }
    
    return response;
}

} // namespace bitrpc