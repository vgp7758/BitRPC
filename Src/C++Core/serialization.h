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

namespace bitrpc {

    // Forward declarations
    class StreamReader;
    class StreamWriter;
    template<typename T>
    class StructTypeHandler;

    // nullopt equivalent (must be declared before optional)
    struct nullopt_t {
        explicit nullopt_t() {}
    };
    extern const nullopt_t nullopt;

    // C++11 compatible optional replacement
    template<typename T>
    class optional {
    public:
        optional() : has_value_(false) {}
        optional(const T& value) : has_value_(true), value_(value) {}
        optional(const nullopt_t&) : has_value_(false) {}

        bool has_value() const { return has_value_; }
        const T& value() const { return value_; }
        T& value() { return value_; }

    private:
        bool has_value_;
        T value_;
    };

    template<typename T>
    optional<T> make_optional(const T& value) {
        return optional<T>(value);
    }

    // For string optional specifically
    typedef optional<std::string> optional_string;

    // Custom deleter for shared_ptr to avoid lambda (C++11 compatibility)
    template<typename T>
    class SharedPtrDeleter {
    public:
        void operator()(T* ptr) const {
            // Do nothing - we don't want to delete singleton instances
            (void)ptr; // Suppress unused parameter warning
        }
    };

    // C++11 compatible make_unique implementation (default constructor version)
    template<typename T>
    std::unique_ptr<T> make_unique() {
        return std::unique_ptr<T>(new T());
    }

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
        void write_optional(const optional_string& value);

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
        optional_string read_optional_string();

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

        // Check if more data is available
        bool has_more_data() const { return position_ < data_.size(); }
        size_t available_data() const { return data_.size() - position_; }

    private:
        std::vector<uint8_t> data_;
        size_t position_;
    };

    // Streaming reader interface for server-side streaming responses
    class StreamResponseReader {
    public:
        virtual ~StreamResponseReader() = default;

        // Read next item from stream, returns empty vector if stream ended
        virtual std::vector<uint8_t> read_next() = 0;

        // Check if stream has more data
        virtual bool has_more() const = 0;

        // Close the stream
        virtual void close() = 0;

        // Get error state
        virtual bool has_error() const = 0;
        virtual std::string get_error_message() const = 0;
    };


    // Stream response writer interface for server-side streaming
    class StreamResponseWriter {
    public:
        virtual ~StreamResponseWriter() = default;

        // Write an item to the stream
        virtual bool write(const void* item) = 0;

        // Check if writer is still valid
        virtual bool is_valid() const = 0;

        // Close the stream gracefully
        virtual void close() = 0;

        // Get error state
        virtual bool has_error() const = 0;
        virtual std::string get_error_message() const = 0;
    };

    // Template helper for typed stream writing
    template<typename T>
    class TypedStreamResponseWriter : public StreamResponseWriter {
    public:
        explicit TypedStreamResponseWriter(std::unique_ptr<StreamResponseWriter> base_writer)
            : base_writer_(std::move(base_writer)) {
        }

        bool write(const T& item) {
            return base_writer_->write(&item);
        }

        bool is_valid() const override { return base_writer_->is_valid(); }
        void close() override { base_writer_->close(); }
        bool has_error() const override { return base_writer_->has_error(); }
        std::string get_error_message() const override { return base_writer_->get_error_message(); }

        bool write(const void* item) override { return base_writer_->write(item); }

    private:
        std::unique_ptr<StreamResponseWriter> base_writer_;
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
            register_handler<T>(std::shared_ptr<TypeHandler>(&StructTypeHandler<T>::instance(), SharedPtrDeleter<TypeHandler>()));
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
        void serialize_impl(const void* obj, StreamWriter& writer, size_t type_hash);
        void* deserialize_impl(StreamReader& reader);

    private:
        BufferSerializer() = default;
        std::unordered_map<size_t, std::shared_ptr<TypeHandler>> handlers_;
        std::unordered_map<int, std::shared_ptr<TypeHandler>> handlers_by_hash_code_;
        mutable std::mutex handlers_mutex_;
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

    // Template helper for typed stream reading
    template<typename T>
    class TypedStreamResponseReader : public StreamResponseReader {
    public:
        explicit TypedStreamResponseReader(std::unique_ptr<StreamResponseReader> base_reader)
            : base_reader_(std::move(base_reader)) {
        }

        std::unique_ptr<T> read_next_typed() {
            auto data = base_reader_->read_next();
            if (data.empty()) {
                return nullptr;
            }
            StreamReader reader(data);
            auto obj = BufferSerializer::instance().deserialize<T>(reader);
            return obj;
        }

        bool has_more() const override { return base_reader_->has_more(); }
        void close() override { base_reader_->close(); }
        bool has_error() const override { return base_reader_->has_error(); }
        std::string get_error_message() const override { return base_reader_->get_error_message(); }

        std::vector<uint8_t> read_next() override { return base_reader_->read_next(); }

    private:
        std::unique_ptr<StreamResponseReader> base_reader_;
    };

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