#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>
#include "serialization.h"

namespace bitrpc {

// Forward declarations
class ServiceManager;

// Unified RPC Server interface
class IRpcServer {
public:
    virtual ~IRpcServer() = default;

    // Start methods
    virtual void start(int port) = 0;
    virtual void start_async(const std::string& host, int port) = 0;

    // Server control
    virtual void stop() = 0;
    virtual bool is_running() const = 0;

    // Service management
    virtual ServiceManager& service_manager() = 0;
};

// Legacy interface for backward compatibility
using RpcServer = IRpcServer;

// Method registration helper
template<typename TRequest, typename TResponse>
using ServiceMethod = std::function<TResponse*(const TRequest*)>;

// Base service class with method registration
class BaseService {
public:
    explicit BaseService(const std::string& name);
    virtual ~BaseService() = default;

    std::string service_name() const { return name_; }
    virtual bool has_method(const std::string& method_name) const;
    virtual bool has_async_method(const std::string& method_name) const;
    virtual void* call_method(const std::string& method_name, void* request);
    virtual std::future<void*> call_method_async(const std::string& method_name, void* request);

    // Streaming support
    virtual bool has_stream_method(const std::string& method_name) const;
    virtual std::shared_ptr<StreamResponseReader> call_stream_method(const std::string& method_name,
                                                                     const std::vector<uint8_t>& request_bytes);

protected:
    template<typename TRequest, typename TResponse>
    void register_method(const std::string& method_name, ServiceMethod<TRequest, TResponse> method);

    template<typename TRequest, typename TResponse>
    void register_async_method(const std::string& method_name,
                               std::function<std::future<TResponse>(const TRequest&)> method);

    template<typename TRequest>
    void register_stream_method(const std::string& method_name,
                               std::function<std::shared_ptr<StreamResponseReader>(const TRequest&)> method);

protected:
    std::string name_;
    std::unordered_map<std::string, std::function<void*(void*)>> methods_;
    std::unordered_map<std::string, std::function<std::future<void*>(void*)>> async_methods_;
    std::unordered_map<std::string, std::function<std::shared_ptr<StreamResponseReader>(void*)>> stream_methods_;
    mutable std::mutex methods_mutex_;
};

// Service Manager for managing multiple services
class ServiceManager {
public:
    ServiceManager();
    ~ServiceManager();

    void register_service(std::shared_ptr<BaseService> service);
    void unregister_service(const std::string& service_name);
    std::shared_ptr<BaseService> get_service(const std::string& service_name) const;
    bool has_service(const std::string& service_name) const;
    std::vector<std::string> get_service_names() const;

private:
    std::unordered_map<std::string, std::shared_ptr<BaseService>> services_;
    mutable std::mutex services_mutex_;
};

// Unified TCP RPC Server implementation
class TcpRpcServer : public IRpcServer {
public:
    TcpRpcServer();
    ~TcpRpcServer() override;

    void start(int port) override;
    void start_async(const std::string& host, int port) override;
    void stop() override;
    bool is_running() const override;
    ServiceManager& service_manager() override;

private:
    std::shared_ptr<ServiceManager> service_manager_;
    void* server_socket_;
    std::atomic<bool> is_running_;
    std::vector<std::thread> client_threads_;
    std::thread accept_thread_;
    std::mutex server_mutex_;

    void accept_connections();
    void handle_client(void* client_socket);
    std::pair<std::string, std::string> parse_method_name(const std::string& method);
    void initialize_network();
    void cleanup_network();
};


// Template implementations
// Requests arrive as raw bytes (without type hash). We deserialize here and serialize responses
// with type hash using StreamWriter::write_object for client compatibility.
template<typename TRequest, typename TResponse>
void BaseService::register_method(const std::string& method_name, ServiceMethod<TRequest, TResponse> method) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    methods_[method_name] = [method](void* request) -> void* {
        // request is std::vector<uint8_t>*
        auto req_bytes = static_cast<const std::vector<uint8_t>*>(request);
        StreamReader reader(*req_bytes);
        auto* handler = BufferSerializer::instance().get_handler(typeid(TRequest).hash_code());
        if (!handler) {
            throw std::runtime_error("No serializer for request type");
        }
        std::unique_ptr<TRequest> req(static_cast<TRequest*>(handler->read(reader)));
        // Invoke user method
        std::unique_ptr<TResponse> resp_ptr(method(req.get()));
        // Serialize with type hash
        StreamWriter writer;
        writer.write_object(resp_ptr.get(), typeid(TResponse).hash_code());
        return new std::vector<uint8_t>(writer.to_array());
    };
}

template<typename TRequest, typename TResponse>
void BaseService::register_async_method(const std::string& method_name,
                                        std::function<std::future<TResponse>(const TRequest&)> method) {
    std::lock_guard<std::mutex> lock(methods_mutex_);

    async_methods_[method_name] = [method](void* request) -> std::future<void*> {
        auto req_bytes = static_cast<const std::vector<uint8_t>*>(request);
        StreamReader reader(*req_bytes);
        auto* handler = BufferSerializer::instance().get_handler(typeid(TRequest).hash_code());
        if (!handler) {
            throw std::runtime_error("No serializer for request type");
        }
        std::unique_ptr<TRequest> req(static_cast<TRequest*>(handler->read(reader)));

        auto future_result = method(*req);
        return std::async(std::launch::async, [future_result = std::move(future_result)]() mutable -> void* {
            auto result = future_result.get(); // by value
            StreamWriter writer;
            writer.write_object(&result, typeid(TResponse).hash_code());
            return new std::vector<uint8_t>(writer.to_array());
        });
    };
}

template<typename TRequest>
void BaseService::register_stream_method(const std::string& method_name,
                                        std::function<std::shared_ptr<StreamResponseReader>(const TRequest&)> method) {
    std::lock_guard<std::mutex> lock(methods_mutex_);

    stream_methods_[method_name] = [method](void* request) -> std::shared_ptr<StreamResponseReader> {
        auto req_bytes = static_cast<const std::vector<uint8_t>*>(request);
        StreamReader reader(*req_bytes);
        auto* handler = BufferSerializer::instance().get_handler(typeid(TRequest).hash_code());
        if (!handler) {
            throw std::runtime_error("No serializer for request type");
        }
        std::unique_ptr<TRequest> req(static_cast<TRequest*>(handler->read(reader)));
        return method(*req);
    };
}

} // namespace bitrpc