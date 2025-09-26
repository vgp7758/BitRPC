#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <typeinfo>
#include <chrono>
#include <optional>
#include "rpc_core.h"

namespace bitrpc {

// Enhanced Buffer Serializer with better type support
class EnhancedBufferSerializer {
public:
    static EnhancedBufferSerializer& instance();

    template<typename T>
    void register_handler(std::shared_ptr<TypeHandler> handler) {
        register_handler_impl(typeid(T).hash_code(), handler);
    }

    template<typename T>
    void serialize(const T* obj, StreamWriter& writer) {
        serialize_impl(obj, writer, typeid(T).hash_code());
    }

    template<typename T>
    std::unique_ptr<T> deserialize(StreamReader& reader) {
        void* obj = deserialize_impl(reader);
        return std::unique_ptr<T>(static_cast<T*>(obj));
    }

    TypeHandler* get_handler(size_t type_hash) const;
    TypeHandler* get_handler_by_hash_code(int hash_code) const;
    void register_handler_impl(size_t type_hash, std::shared_ptr<TypeHandler> handler);
    void init_handlers();

private:
    EnhancedBufferSerializer() = default;
    std::unordered_map<size_t, std::shared_ptr<TypeHandler>> handlers_;
    std::unordered_map<int, std::shared_ptr<TypeHandler>> handlers_by_hash_code_;
    std::mutex handlers_mutex_;

    void serialize_impl(const void* obj, StreamWriter& writer, size_t type_hash);
    void* deserialize_impl(StreamReader& reader);
};

// Enhanced Type Handlers
class DateTimeHandler : public TypeHandler {
public:
    int hash_code() const override { return 201; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class Vector3Handler : public TypeHandler {
public:
    int hash_code() const override { return 202; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

// Struct type handler for custom message types
template<typename T>
class StructTypeHandler : public TypeHandler {
public:
    int hash_code() const override { return T::type_hash(); }

    void write(const void* obj, StreamWriter& writer) const override {
        const T* typed_obj = static_cast<const T*>(obj);
        T::serialize(typed_obj, writer);
    }

    void* read(StreamReader& reader) const override {
        return T::deserialize(reader);
    }
};

// Helper for registering struct handlers
template<typename T>
void register_struct_handler(EnhancedBufferSerializer& serializer) {
    serializer.register_handler<T>(std::make_shared<StructTypeHandler<T>>());
}

// Enhanced Stream Writer with more data types
class EnhancedStreamWriter : public StreamWriter {
public:
    EnhancedStreamWriter() : StreamWriter() {}

    // Additional write methods
    void write_datetime(const std::chrono::system_clock::time_point& time);
    void write_vector3(const Vector3& vec);
    void write_optional(const std::optional<std::string>& value);

    // Enhanced vector writing with custom serializers
    template<typename T>
    void write_vector_typed(const std::vector<T>& vec,
                           std::function<void(EnhancedStreamWriter&, const T&)> write_func) {
        write_int32(static_cast<int32_t>(vec.size()));
        for (const auto& item : vec) {
            write_func(*this, item);
        }
    }
};

// Enhanced Stream Reader with more data types
class EnhancedStreamReader : public StreamReader {
public:
    explicit EnhancedStreamReader(const std::vector<uint8_t>& data) : StreamReader(data) {}

    // Additional read methods
    std::chrono::system_clock::time_point read_datetime();
    Vector3 read_vector3();
    std::optional<std::string> read_optional_string();

    // Enhanced vector reading with custom deserializers
    template<typename T>
    std::vector<T> read_vector_typed(std::function<T(EnhancedStreamReader&)> read_func) {
        int32_t size = read_int32();
        std::vector<T> result;
        result.reserve(size);
        for (int32_t i = 0; i < size; ++i) {
            result.push_back(read_func(*this));
        }
        return result;
    }
};

// Vector3 struct
struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vector3& other) const {
        return !(*this == other);
    }
};

// Global serializer instance
inline EnhancedBufferSerializer& get_serializer() {
    return EnhancedBufferSerializer::instance();
}

} // namespace bitrpc