#include "serialization.h"
#include <stdexcept>
#include <chrono>
#include <ctime>

namespace bitrpc {

EnhancedBufferSerializer& EnhancedBufferSerializer::instance() {
    static EnhancedBufferSerializer instance;
    return instance;
}

TypeHandler* EnhancedBufferSerializer::get_handler(size_t type_hash) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(type_hash);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

TypeHandler* EnhancedBufferSerializer::get_handler_by_hash_code(int hash_code) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_by_hash_code_.find(hash_code);
    return (it != handlers_by_hash_code_.end()) ? it->second.get() : nullptr;
}

void EnhancedBufferSerializer::register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[type_hash] = handler;
    handlers_by_hash_code_[handler->hash_code()] = handler;
}

void EnhancedBufferSerializer::init_handlers() {
    // Register built-in type handlers
    register_handler_impl(typeid(int32_t).hash_code(), std::make_shared<Int32Handler>());
    register_handler_impl(typeid(int64_t).hash_code(), std::make_shared<Int64Handler>());
    register_handler_impl(typeid(float).hash_code(), std::make_shared<FloatHandler>());
    register_handler_impl(typeid(double).hash_code(), std::make_shared<DoubleHandler>());
    register_handler_impl(typeid(bool).hash_code(), std::make_shared<BoolHandler>());
    register_handler_impl(typeid(std::string).hash_code(), std::make_shared<StringHandler>());
    register_handler_impl(typeid(std::vector<uint8_t>).hash_code(), std::make_shared<BytesHandler>());
    register_handler_impl(typeid(std::chrono::system_clock::time_point).hash_code(), std::make_shared<DateTimeHandler>());
    register_handler_impl(typeid(Vector3).hash_code(), std::make_shared<Vector3Handler>());
}

void EnhancedBufferSerializer::serialize_impl(const void* obj, StreamWriter& writer, size_t type_hash) {
    auto handler = get_handler(type_hash);
    if (!handler) {
        throw std::runtime_error("No serializer registered for type");
    }
    handler->write(obj, writer);
}

void* EnhancedBufferSerializer::deserialize_impl(StreamReader& reader) {
    // For now, implement a simple version that just returns nullptr
    // In a full implementation, this would need to read type information and deserialize accordingly
    return nullptr;
}

// DateTimeHandler implementation
void DateTimeHandler::write(const void* obj, StreamWriter& writer) const {
    auto time_point = static_cast<const std::chrono::system_clock::time_point*>(obj);
    auto timestamp = std::chrono::system_clock::to_time_t(*time_point);
    writer.write_int64(static_cast<int64_t>(timestamp));
}

void* DateTimeHandler::read(StreamReader& reader) const {
    auto timestamp = reader.read_int64();
    auto time_point = std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
    return new std::chrono::system_clock::time_point(time_point);
}

// Vector3Handler implementation
void Vector3Handler::write(const void* obj, StreamWriter& writer) const {
    auto vec = static_cast<const Vector3*>(obj);
    writer.write_float(vec->x);
    writer.write_float(vec->y);
    writer.write_float(vec->z);
}

void* Vector3Handler::read(StreamReader& reader) const {
    auto vec = new Vector3();
    vec->x = reader.read_float();
    vec->y = reader.read_float();
    vec->z = reader.read_float();
    return vec;
}

// EnhancedStreamWriter additional methods
void EnhancedStreamWriter::write_datetime(const std::chrono::system_clock::time_point& time) {
    auto timestamp = std::chrono::system_clock::to_time_t(time);
    write_int64(static_cast<int64_t>(timestamp));
}

void EnhancedStreamWriter::write_vector3(const Vector3& vec) {
    write_float(vec.x);
    write_float(vec.y);
    write_float(vec.z);
}

void EnhancedStreamWriter::write_optional(const std::optional<std::string>& value) {
    if (value.has_value()) {
        write_int32(1);
        write_string(*value);
    } else {
        write_int32(0);
    }
}

// EnhancedStreamReader additional methods
std::chrono::system_clock::time_point EnhancedStreamReader::read_datetime() {
    auto timestamp = read_int64();
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
}

Vector3 EnhancedStreamReader::read_vector3() {
    Vector3 vec;
    vec.x = read_float();
    vec.y = read_float();
    vec.z = read_float();
    return vec;
}

std::optional<std::string> EnhancedStreamReader::read_optional_string() {
    int32_t has_value = read_int32();
    if (has_value) {
        return read_string();
    } else {
        return std::nullopt;
    }
}

} // namespace bitrpc