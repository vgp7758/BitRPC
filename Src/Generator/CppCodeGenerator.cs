using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using BitRPC.Protocol.Parser;

namespace BitRPC.Protocol.Generator
{
    public class CppCodeGenerator : BaseCodeGenerator
    {
        public CppCodeGenerator() : base("Templates/Cpp")
        {
        }

        public override void Generate(ProtocolDefinition definition, GenerationOptions options)
        {
            var baseDir = GetOutputPath(options);

            if (options.GenerateSerialization)
            {
                GenerateDataStructures(definition, options, baseDir);
                GenerateSerializationCode(definition, options, baseDir);
            }

            if (options.GenerateClientServer)
            {
                GenerateClientCode(definition, options, baseDir);
                GenerateServerCode(definition, options, baseDir);
            }

            if (options.GenerateFactories)
            {
                GenerateFactoryCode(definition, options, baseDir);
            }

            GenerateCMakeFile(definition, options, baseDir);
        }

        // Stable 32-bit FNV-1a hash (same algorithm as C# generator) for cross-language stability
        private static int ComputeStableHash(string text)
        {
            unchecked
            {
                const uint fnvOffset = 2166136261;
                const uint fnvPrime = 16777619;
                uint hash = fnvOffset;
                foreach (var c in text)
                {
                    hash ^= c;
                    hash *= fnvPrime;
                }
                return (int)hash;
            }
        }

        private void GenerateDataStructures(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var includeDir = Path.Combine(baseDir, "include");
            var sourceDir = Path.Combine(baseDir, "src");
            EnsureDirectoryExists(includeDir);
            EnsureDirectoryExists(sourceDir);

            var headerPath = Path.Combine(includeDir, "models.h");
            var sourcePath = Path.Combine(sourceDir, "models.cpp");

            var headerContent = GenerateModelsHeader(definition, options);
            var sourceContent = GenerateModelsSource(definition, options);

            File.WriteAllText(headerPath, headerContent);
            File.WriteAllText(sourcePath, sourceContent);
        }

        private string GenerateModelsHeader(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("models.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();
            sb.AppendLine("#include <vector>");
            sb.AppendLine("#include <string>");
            sb.AppendLine("#include <optional>");
            sb.AppendLine("#include <chrono>");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();

            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"struct {message.Name} {{");
                sb.AppendLine();

                foreach (var field in message.Fields)
                {
                    sb.AppendLine($"    {GetCppType(field)} {field.Name};");
                }

                sb.AppendLine();
                sb.AppendLine($"    {message.Name}();");
                sb.AppendLine("};");
                sb.AppendLine();
            }

            // Declarations of is_default_* (pointer & reference overloads) for all messages
            sb.AppendLine("// is_default declarations for generated messages");
            foreach (var message in definition.Messages)
            {
                var lower = message.Name.ToLower();
                sb.AppendLine($"bool is_default_{lower}(const {message.Name}* value);");
                sb.AppendLine($"bool is_default_{lower}(const {message.Name}& value);");
            }
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateModelsSource(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("models.cpp", options));
            sb.AppendLine($"#include \"../include/models.h\"");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();

            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"{message.Name}::{message.Name}() {{");
                foreach (var field in message.Fields)
                {
                    var defaultValue = GetCppDefaultValue(field);
                    if (!string.IsNullOrEmpty(defaultValue))
                    {
                        sb.AppendLine($"    {field.Name} = {defaultValue};");
                    }
                }
                sb.AppendLine("}");
                sb.AppendLine();
            }

            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private void GenerateSerializationCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var includeDir = Path.Combine(baseDir, "include");
            var sourceDir = Path.Combine(baseDir, "src");
            EnsureDirectoryExists(includeDir);
            EnsureDirectoryExists(sourceDir);

            foreach (var message in definition.Messages)
            {
                var headerPath = Path.Combine(includeDir, $"{message.Name.ToLower()}_serializer.h");
                var sourcePath = Path.Combine(sourceDir, $"{message.Name.ToLower()}_serializer.cpp");

                var headerContent = GenerateMessageSerializerHeader(message, options);
                var sourceContent = GenerateMessageSerializerSource(message, options);

                File.WriteAllText(headerPath, headerContent);
                File.WriteAllText(sourcePath, sourceContent);
            }

            GenerateSerializerRegistry(definition, options, includeDir, sourceDir);
        }

        private string GenerateMessageSerializerHeader(ProtocolMessage message, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{message.Name}_serializer.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();
            var runtimeInclude = GetRuntimeInclude(options);
            sb.AppendLine("#include \"./serializer_registry.h\"");
            sb.AppendLine("#include \"./models.h\"");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            var lowerName = message.Name.ToLower();
            // Only rely on declarations from models.h
            sb.AppendLine($"class {message.Name}Serializer : public TypeHandler {{");
            sb.AppendLine("public:");
            sb.AppendLine("    int hash_code() const override;");
            sb.AppendLine("    void write(const void* obj, StreamWriter& writer) const override;");
            sb.AppendLine("    void* read(StreamReader& reader) const override;");
            sb.AppendLine("    bool is_default(const void* obj) const override { return is_default_" + lowerName + "(static_cast<const " + message.Name + "*>(obj)); }");
            sb.AppendLine();
            sb.AppendLine("    static " + message.Name + "Serializer& instance() { static " + message.Name + "Serializer inst; return inst; }");
            sb.AppendLine();
            sb.AppendLine("    static void serialize(const " + message.Name + "& obj, StreamWriter& writer);");
            sb.AppendLine("    static std::unique_ptr<" + message.Name + "> deserialize(StreamReader& reader);");
            sb.AppendLine("};");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");
            return sb.ToString();
        }

        private string GenerateMessageSerializerSource(ProtocolMessage message, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{message.Name}_serializer.cpp", options));
            sb.AppendLine("#include \"../include/" + message.Name.ToLower() + "_serializer.h\"");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            var stableHash = ComputeStableHash(message.Name);
            sb.AppendLine("int " + message.Name + "Serializer::hash_code() const { return " + stableHash + "; }");
            sb.AppendLine();
            // Implement is_default_* functions here
            var lower = message.Name.ToLower();
            sb.AppendLine("bool is_default_" + lower + "(const " + message.Name + "* value) {");
            sb.AppendLine("    if (value == nullptr) return true;");
            sb.AppendLine("    const auto& obj = *value;");
            foreach (var field in message.Fields)
            {
                if (field.IsRepeated)
                {
                    sb.AppendLine("    if (!obj." + field.Name + ".empty()) return false;");
                }
                else if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
                {
                    sb.AppendLine("    if (!is_default_" + field.CustomType.ToLower() + "(&obj." + field.Name + ")) return false;");
                }
                else
                {
                    var defVal = GetCppDefaultValueForType(field.Type);
                    sb.AppendLine("    if (obj." + field.Name + " != " + defVal + ") return false;");
                }
            }
            sb.AppendLine("    return true;");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("bool is_default_" + lower + "(const " + message.Name + "& value) { return is_default_" + lower + "(&value); }");
            sb.AppendLine();
            // Field groups
            var fieldGroups = message.Fields.Select(f => new { Field = f, Index = f.Id - 1 }).GroupBy(x => x.Index / 32).ToList();
            int groupCount = fieldGroups.Count;
            sb.AppendLine("void " + message.Name + "Serializer::write(const void* obj, StreamWriter& writer) const {");
            sb.AppendLine("    const auto& obj_ref = *static_cast<const " + message.Name + "*>(obj);");
            for (int g = 0; g < groupCount; g++) sb.AppendLine("    uint32_t mask" + g + " = 0;");
            foreach (var grp in fieldGroups.Select((grp, gi) => new { grp, gi }))
            {
                foreach (var fi in grp.grp)
                {
                    int bitPos = fi.Index % 32;
                    var field = fi.Field;
                    if (field.IsRepeated)
                    {
                        sb.AppendLine("    if (!obj_ref." + field.Name + ".empty()) mask" + grp.gi + " |= (1u << " + bitPos + ");");
                    }
                    else if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
                    {
                        sb.AppendLine("    if (!is_default_" + field.CustomType.ToLower() + "(&obj_ref." + field.Name + ")) mask" + grp.gi + " |= (1u << " + bitPos + ");");
                    }
                    else
                    {
                        var defVal = GetCppDefaultValueForType(field.Type);
                        sb.AppendLine("    if (!(obj_ref." + field.Name + " == " + defVal + ")) mask" + grp.gi + " |= (1u << " + bitPos + ");");
                    }
                }
            }
            for (int g = 0; g < groupCount; g++) sb.AppendLine("    writer.write_uint32(mask" + g + ");");
            foreach (var field in message.Fields)
            {
                int index = field.Id - 1; int group = index / 32; int bitPos = index % 32;
                sb.AppendLine("    if (mask" + group + " & (1u << " + bitPos + ")) { " + GenerateCppWriteField(field) + " }");
            }
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("void* " + message.Name + "Serializer::read(StreamReader& reader) const {");
            sb.AppendLine("    auto obj_ptr = std::make_unique<" + message.Name + ">();");
            for (int g = 0; g < groupCount; g++) sb.AppendLine("    uint32_t mask" + g + " = reader.read_uint32();");
            foreach (var field in message.Fields)
            {
                int index = field.Id - 1; int group = index / 32; int bitPos = index % 32;
                sb.AppendLine("    if (mask" + group + " & (1u << " + bitPos + ")) { " + GenerateCppReadField(field) + " }");
            }
            sb.AppendLine("    return obj_ptr.release();");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("void " + message.Name + "Serializer::serialize(const " + message.Name + "& obj, StreamWriter& writer) { instance().write(&obj, writer); }");
            sb.AppendLine("std::unique_ptr<" + message.Name + "> " + message.Name + "Serializer::deserialize(StreamReader& reader) { auto obj_ptr = std::unique_ptr<" + message.Name + ">(static_cast<" + message.Name + "*>(instance().read(reader))); return obj_ptr; }");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");
            return sb.ToString();
        }

        private void GenerateSerializerRegistry(ProtocolDefinition definition, GenerationOptions options, string includeDir, string sourceDir)
        {
            var headerPath = Path.Combine(includeDir, "serializer_registry.h");
            var sourcePath = Path.Combine(sourceDir, "serializer_registry.cpp");

            var headerContent = GenerateSerializerRegistryHeader(definition, options);
            var sourceContent = GenerateSerializerRegistrySource(definition, options);

            File.WriteAllText(headerPath, headerContent);
            File.WriteAllText(sourcePath, sourceContent);
        }

        private string GenerateSerializerRegistryHeader(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("serializer_registry.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();

            sb.AppendLine($"#include \"../runtime/serialization.h\"");
            
            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"#include \"./{message.Name.ToLower()}_serializer.h\"");
            }
            sb.AppendLine();

            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine("class BufferSerializer;");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine("void register_serializers(BufferSerializer& serializer);");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateSerializerRegistrySource(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("serializer_registry.cpp", options));
            var runtimeInclude = GetRuntimeInclude(options);
            sb.AppendLine($"#include \"../include/serializer_registry.h\"");
            sb.AppendLine();

            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine("void register_serializers(BufferSerializer& serializer) {");

            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"    serializer.register_handler<{message.Name}>(std::shared_ptr<TypeHandler>(&{message.Name}Serializer::instance(), [](TypeHandler*){{}}));");
            }

            sb.AppendLine("}");
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private void GenerateClientCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var includeDir = Path.Combine(baseDir, "include");
            var sourceDir = Path.Combine(baseDir, "src");
            EnsureDirectoryExists(includeDir);
            EnsureDirectoryExists(sourceDir);

            foreach (var service in definition.Services)
            {
                var headerPath = Path.Combine(includeDir, $"{service.Name.ToLower()}_client.h");
                var sourcePath = Path.Combine(sourceDir, $"{service.Name.ToLower()}_client.cpp");

                var headerContent = GenerateServiceClientHeader(service, options);
                var sourceContent = GenerateServiceClientSource(service, options);

                File.WriteAllText(headerPath, headerContent);
                File.WriteAllText(sourcePath, sourceContent);
            }
        }

        private string GenerateServiceClientHeader(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}_client.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();
            var runtimeInclude = GetRuntimeInclude(options);
            sb.AppendLine($"#include \"../runtime/client.h\"");
            sb.AppendLine("#include \"./models.h\"");
            sb.AppendLine("#include <future>");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine($"class {service.Name}Client : public BaseClient {{");
            sb.AppendLine("public:");
            sb.AppendLine($"    explicit {service.Name}Client(std::shared_ptr<IRpcClient> client);");
            sb.AppendLine();

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"    std::shared_ptr<StreamResponseReader> {method.Name}StreamAsync(const {method.RequestType}& request);");
                }
                else
                {
                    sb.AppendLine($"    std::future<{method.ResponseType}> {method.Name}Async(const {method.RequestType}& request);");
                }
            }

            sb.AppendLine();
            sb.AppendLine($"    static std::shared_ptr<{service.Name}Client> create_tcp_client(const std::string& host, int port);");

            sb.AppendLine("};");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateServiceClientSource(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}_client.cpp", options));
            sb.AppendLine($"#include \"../include/{service.Name.ToLower()}_client.h\"");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine($"{service.Name}Client::{service.Name}Client(std::shared_ptr<IRpcClient> client)");
            sb.AppendLine("    : BaseClient(client) {}");
            sb.AppendLine();

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"std::shared_ptr<StreamResponseReader> {service.Name}Client::{method.Name}StreamAsync(const {method.RequestType}& request) {{");
                    sb.AppendLine($"    return stream_async<{method.RequestType}>(\"{service.Name}.{method.Name}\", request);");
                    sb.AppendLine("}");
                    sb.AppendLine();
                }
                else
                {
                    sb.AppendLine($"std::future<{method.ResponseType}> {service.Name}Client::{method.Name}Async(const {method.RequestType}& request) {{");
                    sb.AppendLine($"    return call_async<{method.RequestType}, {method.ResponseType}>(\"{service.Name}.{method.Name}\", request);");
                    sb.AppendLine("}");
                    sb.AppendLine();
                }
            }

            sb.AppendLine($"std::shared_ptr<{service.Name}Client> {service.Name}Client::create_tcp_client(const std::string& host, int port) {{");
            sb.AppendLine($"    auto tcp_client = RpcClientFactory::create_tcp_client(host, port);");
            sb.AppendLine($"    return std::make_shared<{service.Name}Client>(tcp_client);");
            sb.AppendLine("}");

            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private void GenerateServerCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var includeDir = Path.Combine(baseDir, "include");
            var sourceDir = Path.Combine(baseDir, "src");
            EnsureDirectoryExists(includeDir);
            EnsureDirectoryExists(sourceDir);

            foreach (var service in definition.Services)
            {
                var interfacePath = Path.Combine(includeDir, $"i{service.Name.ToLower()}_service.h");
                var basePath = Path.Combine(includeDir, $"{service.Name.ToLower()}_service_base.h");
                var baseSourcePath = Path.Combine(sourceDir, $"{service.Name.ToLower()}_service_base.cpp");

                var interfaceContent = GenerateServiceInterface(service, options);
                var baseContent = GenerateServiceBase(service, options);
                var baseSourceContent = GenerateServiceBaseSource(service, options);

                File.WriteAllText(interfacePath, interfaceContent);
                File.WriteAllText(basePath, baseContent);
                File.WriteAllText(baseSourcePath, baseSourceContent);
            }
        }

        private string GenerateServiceInterface(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"i{service.Name}_service.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();
            sb.AppendLine("#include \"./models.h\"");
            sb.AppendLine("#include <future>");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine($"class I{service.Name}Service {{");
            sb.AppendLine("public:");
            sb.AppendLine($"    virtual ~I{service.Name}Service() = default;");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"    virtual std::shared_ptr<StreamResponseReader> {method.Name}StreamAsync(const {method.RequestType}& request) = 0;");
                }
                else
                {
                    sb.AppendLine($"    virtual std::future<{method.ResponseType}> {method.Name}Async(const {method.RequestType}& request) = 0;");
                }
            }

            sb.AppendLine("};");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateServiceBase(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}_service_base.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();

            var runtimeInclude = GetRuntimeInclude(options);
            sb.AppendLine($"#include \"../runtime/server.h\"");
            sb.AppendLine($"#include \"./models.h\"");
            sb.AppendLine($"#include \"./i{service.Name.ToLower()}_service.h\"");
            sb.AppendLine();

            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();

            sb.AppendLine($"class {service.Name}ServiceBase : public BaseService, public I{service.Name}Service {{");
            sb.AppendLine("public:");
            sb.AppendLine($"    {service.Name}ServiceBase();");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"    std::shared_ptr<StreamResponseReader> {method.Name}StreamAsync(const {method.RequestType}& request) override;");
                }
                else
                {
                    sb.AppendLine($"    std::future<{method.ResponseType}> {method.Name}Async(const {method.RequestType}& request) override;");
                }
            }

            //sb.AppendLine($"    static void register_with_manager(ServiceManager& manager);");
            sb.AppendLine();
            sb.AppendLine("protected:");
            sb.AppendLine("    void register_methods();");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"    virtual std::shared_ptr<StreamResponseReader> {method.Name}StreamAsync_impl(const {method.RequestType}& request) = 0;");
                }
                else
                {
                    sb.AppendLine($"    virtual std::future<{method.ResponseType}> {method.Name}Async_impl(const {method.RequestType}& request) = 0;");
                }
            }

            sb.AppendLine("};");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateServiceBaseSource(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}_service_base.cpp", options));
            sb.AppendLine($"#include \"../include/{service.Name.ToLower()}_service_base.h\"");

            // Add includes for all message serializers
            foreach (var method in service.Methods)
            {
                sb.AppendLine($"#include \"../include/{method.RequestType.ToLower()}_serializer.h\"");
                sb.AppendLine($"#include \"../include/{method.ResponseType.ToLower()}_serializer.h\"");
            }
            sb.AppendLine();

            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine($"{service.Name}ServiceBase::{service.Name}ServiceBase() : BaseService(\"{service.Name}\") {{");
            sb.AppendLine("    register_methods();");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine($"void {service.Name}ServiceBase::register_methods() {{");

            foreach (var method in service.Methods)
            {
                var requestType = method.RequestType;
                var responseType = method.ResponseType;
                if (method.ResponseStream)
                {
                    sb.AppendLine($"    register_stream_method<{requestType}>(\"{method.Name}\", [this](const {method.RequestType}& request) {{");
                    sb.AppendLine($"        return {method.Name}StreamAsync_impl(request);");
                    sb.AppendLine("    });");
                }
                else
                {
                    // Register async method that properly handles typed requests and responses
                    sb.AppendLine($"    register_async_method<{requestType}, {responseType}>(\"{method.Name}\", [this](const {method.RequestType}& request) {{");
                    sb.AppendLine($"        return {method.Name}Async_impl(request);");
                    sb.AppendLine("    });");
                }
            }

            sb.AppendLine("}");
            sb.AppendLine();

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"std::shared_ptr<StreamResponseReader> {service.Name}ServiceBase::{method.Name}StreamAsync(const {method.RequestType}& request) {{");
                    sb.AppendLine($"    return {method.Name}StreamAsync_impl(request);");
                    sb.AppendLine("}");
                    sb.AppendLine();
                }
                else
                {
                    sb.AppendLine($"std::future<{method.ResponseType}> {service.Name}ServiceBase::{method.Name}Async(const {method.RequestType}& request) {{");
                    sb.AppendLine($"    return {method.Name}Async_impl(request);");
                    sb.AppendLine("}");
                    sb.AppendLine();
                }
            }

            //sb.AppendLine($"void {service.Name}ServiceBase::register_with_manager(ServiceManager& manager) {{");
            //sb.AppendLine($"    manager.register_service(instance);");
            //sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private void GenerateFactoryCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var includeDir = Path.Combine(baseDir, "include");
            var sourceDir = Path.Combine(baseDir, "src");
            EnsureDirectoryExists(includeDir);
            EnsureDirectoryExists(sourceDir);

            var headerPath = Path.Combine(includeDir, "protocol_factory.h");
            var sourcePath = Path.Combine(sourceDir, "protocol_factory.cpp");
            
            var headerContent = GenerateProtocolFactory(definition, options);
            var sourceContent = GenerateProtocolFactorySource(definition, options);
            
            File.WriteAllText(headerPath, headerContent);
            File.WriteAllText(sourcePath, sourceContent);
        }

        private string GenerateProtocolFactory(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("protocol_factory.h", options));
            sb.AppendLine("#pragma once");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine("class BufferSerializer;");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine("class ProtocolFactory {");
            sb.AppendLine("public:");
            sb.AppendLine("    static void initialize();");
            sb.AppendLine("};");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private string GenerateProtocolFactorySource(ProtocolDefinition definition, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("protocol_factory.cpp", options));
            sb.AppendLine($"#include \"./include/protocol_factory.h\"");
            sb.AppendLine($"#include \"./include/serializer_registry.h\"");
            sb.AppendLine($"#include \"./runtime/serialization.h\"");
            sb.AppendLine();
            sb.AppendLine("namespace bitrpc {");
            sb.AppendLine($"namespace {GetCppNamespace(options.Namespace)} {{");
            sb.AppendLine();
            sb.AppendLine("void ProtocolFactory::initialize() {");
            sb.AppendLine("    auto& serializer = BufferSerializer::instance();");
            sb.AppendLine("    register_serializers(serializer);");
            sb.AppendLine("}");
            sb.AppendLine();
            sb.AppendLine("}} // namespace bitrpc");

            return sb.ToString();
        }

        private void GenerateCMakeFile(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var filePath = Path.Combine(baseDir, "CMakeLists.txt");
            var sb = new StringBuilder();
            sb.AppendLine("cmake_minimum_required(VERSION 3.10)");
            sb.AppendLine($"project({options.Namespace}Protocol)");
            sb.AppendLine();
            sb.AppendLine("set(CMAKE_CXX_STANDARD 17)");
            sb.AppendLine("set(CMAKE_CXX_STANDARD_REQUIRED ON)");
            sb.AppendLine();
            sb.AppendLine("include_directories(include)");
            var runtimeIncludeDir = GetRuntimeIncludeDirOption(options);
            if (!string.IsNullOrWhiteSpace(runtimeIncludeDir))
            {
                sb.AppendLine("include_directories(\"${CMAKE_CURRENT_LIST_DIR}/" + runtimeIncludeDir + "\")");
            }
            sb.AppendLine();
            sb.AppendLine("set(SOURCES");
            sb.AppendLine("    src/models.cpp");
            
            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"    src/{message.Name.ToLower()}_serializer.cpp");
            }
            
            sb.AppendLine("    src/serializer_registry.cpp");
            
            foreach (var service in definition.Services)
            {
                sb.AppendLine($"    src/{service.Name.ToLower()}_client.cpp");
                sb.AppendLine($"    src/{service.Name.ToLower()}_service_base.cpp");
            }
            
            sb.AppendLine(")");
            sb.AppendLine();
            sb.AppendLine("add_library(${PROJECT_NAME} STATIC ${SOURCES})");
            sb.AppendLine();
            sb.AppendLine("target_link_libraries(${PROJECT_NAME}");
            sb.AppendLine("    bitrpc");
            sb.AppendLine(")");

            File.WriteAllText(filePath, sb.ToString());
        }

        private string GetCppNamespace(string ns)
        {
            return string.IsNullOrEmpty(ns) ? "generated" : ns.Replace(".", "::");
        }

        private string GetCppIncludePath(string ns)
        {
            return string.IsNullOrEmpty(ns) ? "generated" : ns.Replace("::", "/").Replace(".", "/");
        }

        private string GetCppType(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                return $"std::vector<{GetCppTypeNameForField(field)}>`".Replace("`", string.Empty);
            }

            return GetCppTypeNameForField(field);
        }

        private string GetCppTypeName(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "int32_t",
                FieldType.Int64 => "int64_t",
                FieldType.Float => "float",
                FieldType.Double => "double",
                FieldType.Bool => "bool",
                FieldType.String => "std::string",
                FieldType.Vector3 => "Vector3",
                FieldType.DateTime => "std::chrono::system_clock::time_point",
                _ => "void*"
            };
        }

        private string GetCppTypeNameForField(ProtocolField field)
        {
            if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
            {
                return field.CustomType;
            }
            return GetCppTypeName(field.Type);
        }

        private string GetCppDefaultValue(ProtocolField field)
        {
            if (field.IsRepeated) return "{}";
            return GetCppDefaultValueForType(field.Type);
        }

        private string GetCppDefaultValueForType(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "0",
                FieldType.Int64 => "0",
                FieldType.Float => "0.0f",
                FieldType.Double => "0.0",
                FieldType.Bool => "false",
                FieldType.String => "\"\"",
                FieldType.Vector3 => "Vector3(0.0f, 0.0f, 0.0f)",
                FieldType.DateTime => "std::chrono::system_clock::time_point()",
                _ => ""
            };
        }

        private string GenerateCppWriteField(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                var elemType = GetCppTypeNameForField(field);
                return $"writer.write_vector<{elemType}>(obj_ref.{field.Name}, [&writer](const {elemType}& x) {{ {GenerateCppWriteValueForField(field, "x")}; }});";
            }

            return $"{GenerateCppWriteValueForField(field, $"obj_ref.{field.Name}")};";
        }

        private string GenerateCppReadField(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                var elemType = GetCppTypeNameForField(field);
                return $"obj_ptr->{field.Name} = reader.read_vector<{elemType}>([&reader]() {{ return {GenerateCppReadValueForField(field)}; }});";
            }

            return $"obj_ptr->{field.Name} = {GenerateCppReadValueForField(field)};";
        }

        private string GenerateCppWriteValueForField(ProtocolField field, string value)
        {
            if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
            {
                return $"{field.CustomType}Serializer::serialize({value}, writer)";
            }

            // For built-in types, use the singleton handler
            var handlerName = GetSingletonHandlerForType(field.Type);
            return $"{handlerName}.write(&{value}, writer)";
        }

        private string GenerateCppReadValueForField(ProtocolField field)
        {
            if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
            {
                return $"*{field.CustomType}Serializer::deserialize(reader)";
            }

            // For built-in types, use the singleton handler
            var handlerName = GetSingletonHandlerForType(field.Type);
            return $"*static_cast<{GetCppTypeName(field.Type)}*>({handlerName}.read(reader))";
        }

        private string GetSingletonHandlerForType(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "Int32Handler::instance()",
                FieldType.Int64 => "Int64Handler::instance()",
                FieldType.Float => "FloatHandler::instance()",
                FieldType.Double => "DoubleHandler::instance()",
                FieldType.Bool => "BoolHandler::instance()",
                FieldType.String => "StringHandler::instance()",
                FieldType.DateTime => "DateTimeHandler::instance()",
                FieldType.Vector3 => "Vector3Handler::instance()",
                _ => throw new NotSupportedException($"Unsupported field type: {type}")
            };
        }

        private string GetSingletonHandlerNameForType(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "Int32Handler",
                FieldType.Int64 => "Int64Handler",
                FieldType.Float => "FloatHandler",
                FieldType.Double => "DoubleHandler",
                FieldType.Bool => "BoolHandler",
                FieldType.String => "StringHandler",
                FieldType.DateTime => "DateTimeHandler",
                FieldType.Vector3 => "Vector3Handler",
                _ => throw new NotSupportedException($"Unsupported field type: {type}")
            };
        }

        private string GetRuntimeInclude(GenerationOptions options)
        {
            // returns include prefix used in #include <...> e.g. "bitrpc" or "runtime/bitrpc"
            if (options.LanguageSpecificOptions.TryGetValue("Cpp", out var cppOptions) &&
                cppOptions is Dictionary<string, object> cppDict)
            {
                if (cppDict.TryGetValue("RuntimeIncludePrefix", out var prefixObj) && prefixObj is string prefix && !string.IsNullOrWhiteSpace(prefix))
                {
                    return prefix;
                }
            }

            // For generated code, use relative path to runtime directory
            return ".";
        }

        private string GetRuntimeIncludeDirOption(GenerationOptions options)
        {
            if (options.LanguageSpecificOptions.TryGetValue("Cpp", out var cppOptions) &&
                cppOptions is Dictionary<string, object> cppDict)
            {
                if (cppDict.TryGetValue("RuntimeIncludeDir", out var dirObj) && dirObj is string dir && !string.IsNullOrWhiteSpace(dir))
                {
                    return dir;
                }
            }
            return "runtime";
        }
    }
}