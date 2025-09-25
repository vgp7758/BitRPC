#pragma once

#include <memory>
#include <future>
#include <string>
#include <unordered_map>
#include <mutex>
#include "rpc_core.h"

namespace bitrpc {

// RPC Client interface for async operations
class IRpcClient {
public:
    virtual ~IRpcClient() = default;
    virtual std::future<std::vector<uint8_t>> call_async(const std::string& method, const std::vector<uint8_t>& request) = 0;
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

private:
    std::shared_ptr<IRpcClient> client_;
    std::mutex mutex_;
};

// TCP RPC Client implementation with async support
class TcpRpcClient : public IRpcClient {
public:
    TcpRpcClient();
    ~TcpRpcClient() override;

    void connect(const std::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    std::future<std::vector<uint8_t>> call_async(const std::string& method, const std::vector<uint8_t>& request) override;

private:
    void* socket_;
    bool connected_;
    std::mutex socket_mutex_;

    void initialize_network();
    void cleanup_network();
    std::vector<uint8_t> make_rpc_call(const std::string& method, const std::vector<uint8_t>& request);
};

// RPC Client Factory
class RpcClientFactory {
public:
    static std::shared_ptr<IRpcClient> create_tcp_client(const std::string& host, int port);
    static std::shared_ptr<TcpRpcClient> create_tcp_client_native(const std::string& host, int port);
};

// Template implementations
template<typename TRequest, typename TResponse>
std::future<TResponse> BaseClient::call_async(const std::string& method, const TRequest& request) {
    // Serialize request
    auto& serializer = BufferSerializer::instance();
    StreamWriter writer;
    serializer.serialize(&request, writer, typeid(TRequest).hash_code());
    auto request_data = writer.to_array();

    // Make async call
    auto future = client_->call_async(method, request_data);

    // Transform future to return TResponse
    return std::async(std::launch::async, [future, &serializer]() -> TResponse {
        auto response_data = future.get();
        StreamReader reader(response_data);
        auto response_ptr = static_cast<TResponse*>(serializer.deserialize(reader));
        return std::move(*response_ptr);
    });
}

} // namespace bitrpc