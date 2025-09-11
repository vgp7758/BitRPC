#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <typeinfo>
#include <typeindex>

namespace bitrpc {

class StreamReader;
class StreamWriter;

// Type handler interface
class TypeHandler {
public:
    virtual ~TypeHandler() = default;
    virtual int hash_code() const = 0;
    virtual void write(const void* obj, StreamWriter& writer) const = 0;
    virtual void* read(StreamReader& reader) const = 0;
};

// Buffer serializer interface
class BufferSerializer {
public:
    static BufferSerializer& instance();
    
    template<typename T>
    void register_handler(std::shared_ptr<TypeHandler> handler) {
        register_handler_impl(typeid(T).hash_code(), handler);
    }
    
    TypeHandler* get_handler(size_t type_hash) const;
    void register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler);

private:
    BufferSerializer() = default;
    std::unordered_map<size_t, std::shared_ptr<TypeHandler>> handlers_;
};

// Bit mask implementation
class BitMask {
public:
    BitMask() = default;
    explicit BitMask(int size);
    
    void set_bit(int index, bool value);
    bool get_bit(int index) const;
    
    void write(StreamWriter& writer) const;
    void read(StreamReader& reader);

private:
    std::vector<uint32_t> masks_;
};

// Stream writer for serialization
class StreamWriter {
public:
    StreamWriter();
    
    void write_int32(int32_t value);
    void write_int64(int64_t value);
    void write_uint32(uint32_t value);
    void write_float(float value);
    void write_double(double value);
    void write_bool(bool value);
    void write_string(const std::string& value);
    void write_bytes(const std::vector<uint8_t>& bytes);
    
    template<typename T>
    void write_vector(const std::vector<T>& vec, std::function<void(const T&)> write_func);
    
    void write_object(const void* obj, size_t type_hash);
    
    std::vector<uint8_t> to_array() const;

private:
    std::vector<uint8_t> buffer_;
    size_t position_;
};

// Stream reader for deserialization
class StreamReader {
public:
    explicit StreamReader(const std::vector<uint8_t>& data);
    
    int32_t read_int32();
    int64_t read_int64();
    uint32_t read_uint32();
    float read_float();
    double read_double();
    bool read_bool();
    std::string read_string();
    std::vector<uint8_t> read_bytes();
    
    template<typename T>
    std::vector<T> read_vector(std::function<T()> read_func);
    
    void* read_object();

private:
    std::vector<uint8_t> data_;
    size_t position_;
};

// RPC Client interface
class RpcClient {
public:
    virtual ~RpcClient() = default;
    virtual void connect(const std::string& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual std::vector<uint8_t> call(const std::string& method, const std::vector<uint8_t>& request) = 0;
};

// TCP RPC Client implementation
class TcpRpcClient : public RpcClient {
public:
    TcpRpcClient();
    ~TcpRpcClient() override;
    
    void connect(const std::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    std::vector<uint8_t> call(const std::string& method, const std::vector<uint8_t>& request) override;

private:
    void* socket_; // Platform-specific socket handle
    bool connected_;
};

// RPC Server interface
class RpcServer {
public:
    virtual ~RpcServer() = default;
    virtual void start(int port) = 0;
    virtual void stop() = 0;
};

// Base service class
class BaseService {
public:
    virtual ~BaseService() = default;
    virtual void* call_method(const std::string& method_name, void* request) = 0;
};

// Template implementations
template<typename T>
void StreamWriter::write_vector(const std::vector<T>& vec, std::function<void(const T&)> write_func) {
    write_int32(static_cast<int32_t>(vec.size()));
    for (const auto& item : vec) {
        write_func(item);
    }
}

template<typename T>
std::vector<T> StreamReader::read_vector(std::function<T()> read_func) {
    int32_t size = read_int32();
    std::vector<T> result;
    result.reserve(size);
    for (int32_t i = 0; i < size; ++i) {
        result.push_back(read_func());
    }
    return result;
}

} // namespace bitrpc