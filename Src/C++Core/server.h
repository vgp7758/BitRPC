#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "rpc_core.h"

namespace bitrpc {

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

// RPC Server interface
class IRpcServer {
public:
    virtual ~IRpcServer() = default;
    virtual void start_async(const std::string& host, int port) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual ServiceManager& service_manager() = 0;
};

// Enhanced TCP RPC Server with async support
class TcpRpcServer : public IRpcServer {
public:
    TcpRpcServer();
    ~TcpRpcServer() override;

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

// Enhanced Base Service with better async support
class EnhancedBaseService : public BaseService {
public:
    EnhancedBaseService(const std::string& name);
    virtual ~EnhancedBaseService() = default;

    // Override for async method handling
    virtual std::future<void*> call_method_async(const std::string& method_name, void* request);

protected:
    // Helper for registering async methods
    template<typename TRequest, typename TResponse>
    void register_async_method(const std::string& method_name,
                              std::function<std::future<TResponse>(const TRequest&)> method);

private:
    std::unordered_map<std::string, std::function<std::future<void*>(void*)>> async_methods_;
    std::mutex async_methods_mutex_;
};

// Template implementations
template<typename TRequest, typename TResponse>
void EnhancedBaseService::register_async_method(const std::string& method_name,
                                               std::function<std::future<TResponse>(const TRequest&)> method) {
    std::lock_guard<std::mutex> lock(async_methods_mutex_);

    async_methods_[method_name] = [method](void* request) -> std::future<void*> {
        auto typed_request = static_cast<const TRequest*>(request);
        auto future_result = method(*typed_request);

        return std::async(std::launch::async, [future_result]() -> void* {
            auto result = future_result.get();
            return new TResponse(std::move(result));
        });
    };
}

} // namespace bitrpc