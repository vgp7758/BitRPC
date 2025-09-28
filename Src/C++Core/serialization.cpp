#include "serialization.h"
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <cstring>
#include <algorithm>

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

// StreamWriter core implementation
StreamWriter::StreamWriter() : position_(0) {}

void StreamWriter::write_int32(int32_t value) {
    buffer_.resize(buffer_.size() + sizeof(int32_t));
    std::memcpy(buffer_.data() + position_, &value, sizeof(int32_t));
    position_ += sizeof(int32_t);
}

void StreamWriter::write_int64(int64_t value) {
    buffer_.resize(buffer_.size() + sizeof(int64_t));
    std::memcpy(buffer_.data() + position_, &value, sizeof(int64_t));
    position_ += sizeof(int64_t);
}

void StreamWriter::write_uint32(uint32_t value) {
    buffer_.resize(buffer_.size() + sizeof(uint32_t));
    std::memcpy(buffer_.data() + position_, &value, sizeof(uint32_t));
    position_ += sizeof(uint32_t);
}

void StreamWriter::write_float(float value) {
    buffer_.resize(buffer_.size() + sizeof(float));
    std::memcpy(buffer_.data() + position_, &value, sizeof(float));
    position_ += sizeof(float);
}

void StreamWriter::write_double(double value) {
    buffer_.resize(buffer_.size() + sizeof(double));
    std::memcpy(buffer_.data() + position_, &value, sizeof(double));
    position_ += sizeof(double);
}

void StreamWriter::write_bool(bool value) {
    buffer_.resize(buffer_.size() + sizeof(bool));
    std::memcpy(buffer_.data() + position_, &value, sizeof(bool));
    position_ += sizeof(bool);
}

void StreamWriter::write_string(const std::string& value) {
    write_int32(static_cast<int32_t>(value.size()));
    buffer_.resize(buffer_.size() + value.size());
    std::memcpy(buffer_.data() + position_, value.data(), value.size());
    position_ += value.size();
}

void StreamWriter::write_bytes(const std::vector<uint8_t>& bytes) {
    write_int32(static_cast<int32_t>(bytes.size()));
    buffer_.resize(buffer_.size() + bytes.size());
    std::memcpy(buffer_.data() + position_, bytes.data(), bytes.size());
    position_ += bytes.size();
}

void StreamWriter::write_object(const void* obj, size_t type_hash) {
    BufferSerializer::instance().serialize_impl(obj, *this, type_hash);
}

std::vector<uint8_t> StreamWriter::to_array() const {
    return buffer_;
}

// StreamReader core implementation
StreamReader::StreamReader(const std::vector<uint8_t>& data) : data_(data), position_(0) {}

int32_t StreamReader::read_int32() {
    if (position_ + sizeof(int32_t) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    int32_t value;
    std::memcpy(&value, data_.data() + position_, sizeof(int32_t));
    position_ += sizeof(int32_t);
    return value;
}

int64_t StreamReader::read_int64() {
    if (position_ + sizeof(int64_t) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    int64_t value;
    std::memcpy(&value, data_.data() + position_, sizeof(int64_t));
    position_ += sizeof(int64_t);
    return value;
}

uint32_t StreamReader::read_uint32() {
    if (position_ + sizeof(uint32_t) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    uint32_t value;
    std::memcpy(&value, data_.data() + position_, sizeof(uint32_t));
    position_ += sizeof(uint32_t);
    return value;
}

float StreamReader::read_float() {
    if (position_ + sizeof(float) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    float value;
    std::memcpy(&value, data_.data() + position_, sizeof(float));
    position_ += sizeof(float);
    return value;
}

double StreamReader::read_double() {
    if (position_ + sizeof(double) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    double value;
    std::memcpy(&value, data_.data() + position_, sizeof(double));
    position_ += sizeof(double);
    return value;
}

bool StreamReader::read_bool() {
    if (position_ + sizeof(bool) > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    bool value;
    std::memcpy(&value, data_.data() + position_, sizeof(bool));
    position_ += sizeof(bool);
    return value;
}

std::string StreamReader::read_string() {
    int32_t length = read_int32();
    if (position_ + length > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    std::string value(data_.data() + position_, data_.data() + position_ + length);
    position_ += length;
    return value;
}

std::vector<uint8_t> StreamReader::read_bytes() {
    int32_t length = read_int32();
    if (position_ + length > data_.size()) {
        throw std::runtime_error("Buffer underflow");
    }
    std::vector<uint8_t> value(data_.data() + position_, data_.data() + position_ + length);
    position_ += length;
    return value;
}

void* StreamReader::read_object() {
    // For now, return nullptr - in a full implementation this would read type info
    return nullptr;
}

// BitMask implementation
BitMask::BitMask() = default;

BitMask::BitMask(int size) {
    masks_.resize((size + 31) / 32, 0);
}

void BitMask::set_bit(int index, bool value) {
    int mask_index = index / 32;
    int bit_index = index % 32;

    if (mask_index >= static_cast<int>(masks_.size())) {
        masks_.resize(mask_index + 1, 0);
    }

    if (value) {
        masks_[mask_index] |= (1 << bit_index);
    } else {
        masks_[mask_index] &= ~(1 << bit_index);
    }
}

bool BitMask::get_bit(int index) const {
    int mask_index = index / 32;
    int bit_index = index % 32;

    if (mask_index >= static_cast<int>(masks_.size())) {
        return false;
    }

    return (masks_[mask_index] & (1 << bit_index)) != 0;
}

void BitMask::clear() {
    std::fill(masks_.begin(), masks_.end(), 0);
}

void BitMask::write(StreamWriter& writer) const {
    writer.write_int32(static_cast<int32_t>(masks_.size()));
    for (uint32_t mask : masks_) {
        writer.write_uint32(mask);
    }
}

void BitMask::read(StreamReader& reader) {
    int32_t size = reader.read_int32();
    masks_.resize(size);
    for (int32_t i = 0; i < size; ++i) {
        masks_[i] = reader.read_uint32();
    }
}

} // namespace bitrpc