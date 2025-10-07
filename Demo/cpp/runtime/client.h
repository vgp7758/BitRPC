#pragma once

#include <memory>
#include <future>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cstdint>
#include <functional>
#include <typeinfo>
#include <stdexcept>
#include "serialization.h"

namespace bitrpc {

// Custom RPC exceptions
class RpcException : public std::runtime_error {
public:
    RpcException(const std::string& message) : std::runtime_error(message) {}
    virtual ~RpcException() = default;
    virtual int error_code() const { return 0; }
};

class ConnectionException : public RpcException {
public:
    ConnectionException(const std::string& message) : RpcException(message) {}
    int error_code() const override { return 1001; }
};

class TimeoutException : public RpcException {
public:
    TimeoutException(const std::string& message) : RpcException(message) {}
    int error_code() const override { return 1002; }
};

class SerializationException : public RpcException {
public:
    SerializationException(const std::string& message) : RpcException(message) {}
    int error_code() const override { return 2001; }
};

class StreamException : public RpcException {
public:
    StreamException(const std::string& message) : RpcException(message) {}
    int error_code() const override { return 3001; }
};

class ProtocolException : public RpcException {
public:
    ProtocolException(const std::string& message) : RpcException(message) {}
    int error_code() const override { return 4001; }
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

// RPC Client interface for async operations
class IRpcClient {
public:
    virtual ~IRpcClient() = default;
    virtual std::future<std::vector<uint8_t>> call_async(const std::string& method, const std::vector<uint8_t>& request) = 0;
    virtual std::shared_ptr<StreamResponseReader> stream_async(const std::string& method, const std::vector<uint8_t>& request) = 0;
    virtual void connect(const std::string& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
};

// Base client class for generated service clients
class BaseClient {
public:
    explicit BaseClient(std::shared_ptr<IRpcClient> client);
    virtual ~BaseClient() = default;

protected:
    template<typename TRequest, typename TResponse>
    std::future<TResponse> call_async(const std::string& method, const TRequest& request);

    template<typename TRequest>
    std::shared_ptr<StreamResponseReader> stream_async(const std::string& method, const TRequest& request);

private:
    std::shared_ptr<IRpcClient> client_;
    std::mutex mutex_;
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
#ifdef _WIN32
    void* socket_; // Platform-specific socket handle
#else
    int socket_; // Unix socket handle
#endif
    bool connected_;
    std::mutex socket_mutex_;

    void initialize_network();
    void cleanup_network();
};

// TCP RPC Client implementation with async support
class TcpRpcClientAsync : public IRpcClient {
public:
    TcpRpcClientAsync();
    ~TcpRpcClientAsync() override;

    void connect(const std::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    std::future<std::vector<uint8_t>> call_async(const std::string& method, const std::vector<uint8_t>& request) override;
    std::shared_ptr<StreamResponseReader> stream_async(const std::string& method, const std::vector<uint8_t>& request) override;

private:
    void* socket_;
    bool connected_;
    std::mutex socket_mutex_;
    std::string host_;
    int port_;

    void initialize_network();
    void cleanup_network();
    std::vector<uint8_t> make_rpc_call(const std::string& method, const std::vector<uint8_t>& request);
    void send_stream_request(const std::string& method, const std::vector<uint8_t>& request);
};

// TCP Stream Response Reader implementation
class TcpStreamResponseReader : public StreamResponseReader {
public:
    TcpStreamResponseReader(void* socket, int response_type_hash, BufferSerializer& serializer);
    ~TcpStreamResponseReader() override;

    std::vector<uint8_t> read_next() override;
    bool has_more() const override;
    void close() override;
    bool has_error() const override;
    std::string get_error_message() const override;

private:
    void* socket_;
    int response_type_hash_;
    BufferSerializer& serializer_;
    mutable std::mutex socket_mutex_;
    bool stream_ended_;
    bool has_error_;
    std::string error_message_;
    bool connection_closed_;

    bool read_next_frame(std::vector<uint8_t>& data);
    void mark_error(const std::string& error);
};

// TCP Stream Response Writer implementation
class TcpStreamResponseWriter : public StreamResponseWriter {
public:
    TcpStreamResponseWriter(void* socket, int response_type_hash, BufferSerializer& serializer);
    ~TcpStreamResponseWriter() override;

    bool write(const void* item) override;
    bool is_valid() const override;
    void close() override;
    bool has_error() const override;
    std::string get_error_message() const override;

private:
    void* socket_;
    int response_type_hash_;
    BufferSerializer& serializer_;
    mutable std::mutex socket_mutex_;
    bool stream_ended_;
    bool has_error_;
    std::string error_message_;
    bool connection_valid_;

    bool write_frame(const std::vector<uint8_t>& data);
    void mark_error(const std::string& error);
};

// Error handler utility
class ErrorHandler {
public:
    static void log_error(const std::string& context, const std::exception& e);
    static void log_warning(const std::string& message);
    static void log_info(const std::string& message);

    // Convert error code to human readable string
    static std::string error_code_to_string(int error_code);

    // Get last system error message
    static std::string get_last_system_error();
};

// RPC Client Factory
class RpcClientFactory {
public:
    static std::shared_ptr<IRpcClient> create_tcp_client(const std::string& host, int port);
    static std::shared_ptr<TcpRpcClient> create_tcp_client_native(const std::string& host, int port);
    static std::shared_ptr<TcpRpcClientAsync> create_tcp_client_async(const std::string& host, int port);
};

// Template implementations
template<typename TRequest, typename TResponse>
std::future<TResponse> BaseClient::call_async(const std::string& method, const TRequest& request) {
    // Serialize request
    auto& serializer = BufferSerializer::instance();
    StreamWriter writer;
    serializer.serialize(&request, writer);
    auto request_data = writer.to_array();

    // Make async call
    auto future = client_->call_async(method, request_data);

    // Transform future to return TResponse
    return std::async(std::launch::async, [future = std::move(future), &serializer]() mutable -> TResponse {
        auto response_data = future.get();
        StreamReader reader(response_data);
        auto response_ptr = serializer.deserialize<TResponse>(reader);
        return std::move(*response_ptr);
    });
}

template<typename TRequest>
std::shared_ptr<StreamResponseReader> BaseClient::stream_async(const std::string& method, const TRequest& request) {
    // Serialize request
    auto& serializer = BufferSerializer::instance();
    StreamWriter writer;
    serializer.serialize(&request, writer);
    auto request_data = writer.to_array();

    // Start streaming call
    return client_->stream_async(method, request_data);
}

} // namespace bitrpc