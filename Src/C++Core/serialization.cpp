#include "serialization.h"
#include <stdexcept>
#include <chrono>
#include <ctime>

namespace bitrpc {

BufferSerializer& BufferSerializer::instance() {
    static BufferSerializer instance;
    return instance;
}

TypeHandler* BufferSerializer::get_handler(size_t type_hash) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_.find(type_hash);
    return (it != handlers_.end()) ? it->second.get() : nullptr;
}

TypeHandler* BufferSerializer::get_handler_by_hash_code(int hash_code) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = handlers_by_hash_code_.find(hash_code);
    return (it != handlers_by_hash_code_.end()) ? it->second.get() : nullptr;
}

void BufferSerializer::register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[type_hash] = handler;
    handlers_by_hash_code_[handler->hash_code()] = handler;
}

void BufferSerializer::init_handlers() {
    // Register built-in type handlers using singleton instances
    register_handler_impl(typeid(int32_t).hash_code(), std::shared_ptr<TypeHandler>(&Int32Handler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(int64_t).hash_code(), std::shared_ptr<TypeHandler>(&Int64Handler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(float).hash_code(), std::shared_ptr<TypeHandler>(&FloatHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(double).hash_code(), std::shared_ptr<TypeHandler>(&DoubleHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(bool).hash_code(), std::shared_ptr<TypeHandler>(&BoolHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(std::string).hash_code(), std::shared_ptr<TypeHandler>(&StringHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(std::vector<uint8_t>).hash_code(), std::shared_ptr<TypeHandler>(&BytesHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(std::chrono::system_clock::time_point).hash_code(), std::shared_ptr<TypeHandler>(&DateTimeHandler::instance(), [](TypeHandler*){}));
    register_handler_impl(typeid(Vector3).hash_code(), std::shared_ptr<TypeHandler>(&Vector3Handler::instance(), [](TypeHandler*){}));
}

void BufferSerializer::serialize_impl(const void* obj, StreamWriter& writer, size_t type_hash) {
    auto handler = get_handler(type_hash);
    if (!handler) {
        throw std::runtime_error("No serializer registered for type");
    }
    handler->write(obj, writer);
}

void* BufferSerializer::deserialize_impl(StreamReader& reader) {
    // For now, implement a simple version that just returns nullptr
    // In a full implementation, this would need to read type information and deserialize accordingly
    return nullptr;
}

// Int32Handler implementation
void Int32Handler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const int32_t*>(obj);
    writer.write_int32(*value);
}

void* Int32Handler::read(StreamReader& reader) const {
    auto value = reader.read_int32();
    return new int32_t(value);
}

// Int64Handler implementation
void Int64Handler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const int64_t*>(obj);
    writer.write_int64(*value);
}

void* Int64Handler::read(StreamReader& reader) const {
    auto value = reader.read_int64();
    return new int64_t(value);
}

// FloatHandler implementation
void FloatHandler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const float*>(obj);
    writer.write_float(*value);
}

void* FloatHandler::read(StreamReader& reader) const {
    auto value = reader.read_float();
    return new float(value);
}

// DoubleHandler implementation
void DoubleHandler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const double*>(obj);
    writer.write_double(*value);
}

void* DoubleHandler::read(StreamReader& reader) const {
    auto value = reader.read_double();
    return new double(value);
}

// BoolHandler implementation
void BoolHandler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const bool*>(obj);
    writer.write_bool(*value);
}

void* BoolHandler::read(StreamReader& reader) const {
    auto value = reader.read_bool();
    return new bool(value);
}

// StringHandler implementation
void StringHandler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const std::string*>(obj);
    writer.write_string(*value);
}

void* StringHandler::read(StreamReader& reader) const {
    auto value = reader.read_string();
    return new std::string(std::move(value));
}

// BytesHandler implementation
void BytesHandler::write(const void* obj, StreamWriter& writer) const {
    auto value = static_cast<const std::vector<uint8_t>*>(obj);
    writer.write_bytes(*value);
}

void* BytesHandler::read(StreamReader& reader) const {
    auto value = reader.read_bytes();
    return new std::vector<uint8_t>(std::move(value));
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

// StreamWriter additional methods
void StreamWriter::write_datetime(const std::chrono::system_clock::time_point& time) {
    auto timestamp = std::chrono::system_clock::to_time_t(time);
    write_int64(static_cast<int64_t>(timestamp));
}

void StreamWriter::write_vector3(const Vector3& vec) {
    write_float(vec.x);
    write_float(vec.y);
    write_float(vec.z);
}

void StreamWriter::write_optional(const std::optional<std::string>& value) {
    if (value.has_value()) {
        write_int32(1);
        write_string(*value);
    } else {
        write_int32(0);
    }
}

// StreamReader additional methods
std::chrono::system_clock::time_point StreamReader::read_datetime() {
    auto timestamp = read_int64();
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
}

Vector3 StreamReader::read_vector3() {
    Vector3 vec;
    vec.x = read_float();
    vec.y = read_float();
    vec.z = read_float();
    return vec;
}

std::optional<std::string> StreamReader::read_optional_string() {
    int32_t has_value = read_int32();
    if (has_value) {
        return read_string();
    } else {
        return std::nullopt;
    }
}

// TypeHandler convenience static method implementations
bool TypeHandler::is_default_datetime(const std::chrono::system_clock::time_point& value) {
    return value == std::chrono::system_clock::time_point{};
}

bool TypeHandler::is_default_vector3(const Vector3& value) {
    return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

// Base TypeHandler is_default implementations
bool Int32Handler::is_default(const void* obj) const {
    auto value = static_cast<const int32_t*>(obj);
    return *value == 0;
}

bool Int64Handler::is_default(const void* obj) const {
    auto value = static_cast<const int64_t*>(obj);
    return *value == 0;
}

bool FloatHandler::is_default(const void* obj) const {
    auto value = static_cast<const float*>(obj);
    return *value == 0.0f;
}

bool DoubleHandler::is_default(const void* obj) const {
    auto value = static_cast<const double*>(obj);
    return *value == 0.0;
}

bool BoolHandler::is_default(const void* obj) const {
    auto value = static_cast<const bool*>(obj);
    return *value == false;
}

bool StringHandler::is_default(const void* obj) const {
    auto value = static_cast<const std::string*>(obj);
    return value->empty();
}

bool BytesHandler::is_default(const void* obj) const {
    auto value = static_cast<const std::vector<uint8_t>*>(obj);
    return value->empty();
}

bool DateTimeHandler::is_default(const void* obj) const {
    auto value = static_cast<const std::chrono::system_clock::time_point*>(obj);
    return *value == std::chrono::system_clock::time_point{};
}

bool Vector3Handler::is_default(const void* obj) const {
    auto value = static_cast<const Vector3*>(obj);
    return value->x == 0.0f && value->y == 0.0f && value->z == 0.0f;
}

} // namespace bitrpc