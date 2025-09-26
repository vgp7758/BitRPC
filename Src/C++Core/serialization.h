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

    // Static is_default method to be implemented by each concrete handler
    virtual bool is_default(const void* obj) const = 0;

    // Convenience static methods for built-in types
    static bool is_default_int32(const int32_t& value) { return value == 0; }
    static bool is_default_int64(const int64_t& value) { return value == 0; }
    static bool is_default_float(const float& value) { return value == 0.0f; }
    static bool is_default_double(const double& value) { return value == 0.0; }
    static bool is_default_bool(const bool& value) { return value == false; }
    static bool is_default_string(const std::string& value) { return value.empty(); }
    static bool is_default_bytes(const std::vector<uint8_t>& value) { return value.empty(); }
    static bool is_default_datetime(const std::chrono::system_clock::time_point& value);
    static bool is_default_vector3(const Vector3& value);
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

    // Helper method for struct registration (aligns with C# pattern)
    template<typename T>
    void register_struct_handler() {
        register_handler<T>(std::shared_ptr<TypeHandler>(&StructTypeHandler<T>::instance(), [](TypeHandler*){}));
    }

    // C#-style serialization methods for convenience
    template<typename T>
    std::vector<uint8_t> serialize(const T& obj) {
        StreamWriter writer;
        serialize(&obj, writer);
        return writer.to_array();
    }

    template<typename T>
    T deserialize(const std::vector<uint8_t>& data) {
        StreamReader reader(data);
        auto result = deserialize<T>(reader);
        return std::move(*result);
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
    bool is_default(const void* obj) const override;

    // Singleton instance
    static Int32Handler& instance() {
        static Int32Handler instance;
        return instance;
    }

private:
    Int32Handler() = default;
};

class Int64Handler : public TypeHandler {
public:
    int hash_code() const override { return 102; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static Int64Handler& instance() {
        static Int64Handler instance;
        return instance;
    }

private:
    Int64Handler() = default;
};

class FloatHandler : public TypeHandler {
public:
    int hash_code() const override { return 103; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static FloatHandler& instance() {
        static FloatHandler instance;
        return instance;
    }

private:
    FloatHandler() = default;
};

class DoubleHandler : public TypeHandler {
public:
    int hash_code() const override { return 104; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static DoubleHandler& instance() {
        static DoubleHandler instance;
        return instance;
    }

private:
    DoubleHandler() = default;
};

class BoolHandler : public TypeHandler {
public:
    int hash_code() const override { return 105; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static BoolHandler& instance() {
        static BoolHandler instance;
        return instance;
    }

private:
    BoolHandler() = default;
};

class StringHandler : public TypeHandler {
public:
    int hash_code() const override { return 106; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static StringHandler& instance() {
        static StringHandler instance;
        return instance;
    }

private:
    StringHandler() = default;
};

class BytesHandler : public TypeHandler {
public:
    int hash_code() const override { return 107; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static BytesHandler& instance() {
        static BytesHandler instance;
        return instance;
    }

private:
    BytesHandler() = default;
};

// Enhanced Type Handlers
class DateTimeHandler : public TypeHandler {
public:
    int hash_code() const override { return 201; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static DateTimeHandler& instance() {
        static DateTimeHandler instance;
        return instance;
    }

private:
    DateTimeHandler() = default;
};

class Vector3Handler : public TypeHandler {
public:
    int hash_code() const override { return 202; }
    void write(const void* obj, StreamWriter& writer) const override;
    void* read(StreamReader& reader) const override;
    bool is_default(const void* obj) const override;

    // Singleton instance
    static Vector3Handler& instance() {
        static Vector3Handler instance;
        return instance;
    }

private:
    Vector3Handler() = default;
};

// Struct type handler for custom message types
template<typename T>
class StructTypeHandler : public TypeHandler {
public:
    int hash_code() const override {
        // Use typeid hash for consistency with other types
        return static_cast<int>(typeid(T).hash_code());
    }

    void write(const void* obj, StreamWriter& writer) const override {
        const T* typed_obj = static_cast<const T*>(obj);
        T::serialize(typed_obj, writer);
    }

    void* read(StreamReader& reader) const override {
        return T::deserialize(reader);
    }

    bool is_default(const void* obj) const override {
        const T* typed_obj = static_cast<const T*>(obj);
        return *typed_obj == T{};  // Compare with default-constructed value
    }

    // Singleton instance
    static StructTypeHandler& instance() {
        static StructTypeHandler instance;
        return instance;
    }

private:
    StructTypeHandler() = default;
};


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