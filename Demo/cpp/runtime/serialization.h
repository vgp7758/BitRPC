#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <typeinfo>
#include <mutex>
#include <optional>

namespace bitrpc {

// Forward declarations
class StreamReader;
class StreamWriter;

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

// Type handler interface
class TypeHandler {
public:
    virtual ~TypeHandler() = default;
    virtual int hash_code() const = 0;
    virtual void write(const void* obj, StreamWriter& writer) const = 0;
    virtual void* read(StreamReader& reader) const = 0;
};

// Bit mask implementation
class BitMask {
public:
    BitMask();
    explicit BitMask(int size);

    int size() const { return static_cast<int>(masks_.size()); }
    void set_bit(int index, bool value);
    bool get_bit(int index) const;
    void clear();

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

    // Enhanced write methods
    void write_datetime(const std::chrono::system_clock::time_point& time);
    void write_vector3(const Vector3& vec);
    void write_optional(const std::optional<std::string>& value);

    template<typename T>
    void write_vector(const std::vector<T>& vec, std::function<void(const T&)> write_func);

    // Enhanced vector writing with custom serializers
    template<typename T>
    void write_vector_typed(const std::vector<T>& vec,
                           std::function<void(StreamWriter&, const T&)> write_func) {
        write_int32(static_cast<int32_t>(vec.size()));
        for (const auto& item : vec) {
            write_func(*this, item);
        }
    }

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

    // Enhanced read methods
    std::chrono::system_clock::time_point read_datetime();
    Vector3 read_vector3();
    std::optional<std::string> read_optional_string();

    template<typename T>
    std::vector<T> read_vector(std::function<T()> read_func);

    // Enhanced vector reading with custom deserializers
    template<typename T>
    std::vector<T> read_vector_typed(std::function<T(StreamReader&)> read_func) {
        int32_t size = read_int32();
        std::vector<T> result;
        result.reserve(size);
        for (int32_t i = 0; i < size; ++i) {
            result.push_back(read_func(*this));
        }
        return result;
    }

    void* read_object();

private:
    std::vector<uint8_t> data_;
    size_t position_;
};

// Buffer serializer interface with enhanced features
class BufferSerializer {
public:
    static BufferSerializer& instance();

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
    BufferSerializer() = default;
    std::unordered_map<size_t, std::shared_ptr<TypeHandler>> handlers_;
    std::unordered_map<int, std::shared_ptr<TypeHandler>> handlers_by_hash_code_;
    std::mutex handlers_mutex_;

    void serialize_impl(const void* obj, StreamWriter& writer, size_t type_hash);
    void* deserialize_impl(StreamReader& reader);
};

// Type handlers
class Int32Handler : public TypeHandler {
public:
    int hash_code() const override { return 101; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class Int64Handler : public TypeHandler {
public:
    int hash_code() const override { return 102; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class FloatHandler : public TypeHandler {
public:
    int hash_code() const override { return 103; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class DoubleHandler : public TypeHandler {
public:
    int hash_code() const override { return 104; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class BoolHandler : public TypeHandler {
public:
    int hash_code() const override { return 105; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class StringHandler : public TypeHandler {
public:
    int hash_code() const override { return 106; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
};

class BytesHandler : public TypeHandler {
public:
    int hash_code() const override { return 107; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
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
void register_struct_handler(BufferSerializer& serializer) {
    serializer.register_handler<T>(std::make_shared<StructTypeHandler<T>>());
}

// Global serializer instance
inline BufferSerializer& get_serializer() {
    return BufferSerializer::instance();
}

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