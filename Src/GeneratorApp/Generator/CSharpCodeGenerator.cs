using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using BitRPC.Protocol.Parser;

namespace BitRPC.Protocol.Generator
{
    public class CSharpCodeGenerator : BaseCodeGenerator
    {
        public CSharpCodeGenerator() : base("Templates/CSharp")
        {
        }

        public override void Generate(ProtocolDefinition definition, GenerationOptions options)
        {
            var namespacePath = GetNamespacePath(options.Namespace);
            var baseDir = GetOutputPath(options, namespacePath);

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
        }

        // Stable 32-bit FNV-1a hash for type names (same across processes)
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
            var dataDir = Path.Combine(baseDir, "Data");
            EnsureDirectoryExists(dataDir);

            foreach (var message in definition.Messages)
            {
                var filePath = Path.Combine(dataDir, $"{message.Name}.cs");
                var content = GenerateMessageClass(message, options);
                File.WriteAllText(filePath, content);
            }
        }

        private string GenerateMessageClass(ProtocolMessage message, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{message.Name}.cs", options));
            sb.AppendLine($"using System;");
            sb.AppendLine($"using System.Collections.Generic;");
            sb.AppendLine($"using BitRPC.Serialization;");
            sb.AppendLine();
            
            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"namespace {options.Namespace}");
                sb.AppendLine("{");
            }

            sb.AppendLine($"    public partial class {message.Name}");
            sb.AppendLine("    {");

            foreach (var field in message.Fields)
            {
                sb.AppendLine($"        public {GetCSharpType(field)} {field.Name} {{ get; set; }}");
            }

            sb.AppendLine();
            sb.AppendLine($"        public {message.Name}()");
            sb.AppendLine("        {");

            foreach (var field in message.Fields)
            {
                var defaultValue = GetDefaultValue(field);
                if (defaultValue != null)
                {
                    sb.AppendLine($"            {field.Name} = {defaultValue};");
                }
            }

            sb.AppendLine("        }");

            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }

            return sb.ToString();
        }

        private void GenerateSerializationCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var serializationDir = Path.Combine(baseDir, "Serialization");
            EnsureDirectoryExists(serializationDir);

            foreach (var message in definition.Messages)
            {
                var filePath = Path.Combine(serializationDir, $"{message.Name}Handler.cs");
                var content = GenerateMessageSerializer(message, options);
                File.WriteAllText(filePath, content);
            }

            GenerateSerializerRegistry(definition, options, serializationDir);
        }

        private string GenerateMessageSerializer(ProtocolMessage message, GenerationOptions options)
        {
            var sb = new StringBuilder();
            var fieldGroups = message.Fields.Select((f, i) => new { Field = f, Index = f.Id - 1 })
                                           .GroupBy(x => x.Index / 32)
                                           .ToList();
            
            int stableHash = ComputeStableHash(message.Name); // pre-compute stable hash
            sb.AppendLine(GenerateFileHeader($"{message.Name}Handler.cs", options));
            sb.AppendLine($"using System;");
            sb.AppendLine($"using System.IO;");
            sb.AppendLine($"using BitRPC.Serialization;");
            sb.AppendLine("using System.Collections.Generic;");
            sb.AppendLine("using static BitRPC.Serialization.Types;");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"using {options.Namespace};");
                sb.AppendLine($"using {options.Namespace}.Serialization;");
                sb.AppendLine();
                sb.AppendLine($"namespace {options.Namespace}.Serialization");
                sb.AppendLine("{");
            }

            sb.AppendLine($"    public class {message.Name}Handler : ITypeHandler");
            sb.AppendLine("    {");
            sb.AppendLine($"        public int HashCode => {stableHash};");
            sb.AppendLine();
            sb.AppendLine("        public void Write(object obj, BitRPC.Serialization.StreamWriter writer)");
            sb.AppendLine("        {");
            sb.AppendLine($"            var message = ({message.Name})obj;");
            sb.AppendLine($"            BitRPC.Serialization.BitMask mask = null;");
            sb.AppendLine($"            try");
            sb.AppendLine($"            {{");
            sb.AppendLine($"                mask = BitRPC.Serialization.BitMaskPool.Get({fieldGroups.Count});");

            for (int group = 0; group < fieldGroups.Count; group++)
            {
                var fields = fieldGroups[group].ToList();
                sb.AppendLine($"                // Bit mask group {group}");
                foreach (var fieldInfo in fields)
                {
                    var field = fieldInfo.Field;
                    sb.AppendLine($"                mask.SetBit({fieldInfo.Index}, !IsDefault(message.{field.Name}),{group});");
                }
            }
            sb.AppendLine($"                mask.Write(writer,{fieldGroups.Count});");
            sb.AppendLine();
            sb.AppendLine("                // Write field values");
            foreach (var field in message.Fields)
            {
                int index = field.Id - 1;
                int group = index / 32;
                int bitPosition = index % 32;
                sb.AppendLine($"                if (mask.GetBit({bitPosition}, {group})) {GenerateWriteField(field)}");
                //sb.AppendLine($"                {GenerateWriteField(field)}");
            }

            sb.AppendLine("            }");
            sb.AppendLine("            finally");
            sb.AppendLine("            {");
            sb.AppendLine("                if (mask != null)");
            sb.AppendLine("                {");
            sb.AppendLine("                    BitRPC.Serialization.BitMaskPool.Return(mask);");
            sb.AppendLine("                }");
            sb.AppendLine("            }");
            sb.AppendLine("        }");
            sb.AppendLine();
            sb.AppendLine("        public object Read(BitRPC.Serialization.StreamReader reader)");
            sb.AppendLine("        {");
            sb.AppendLine($"            var message = new {message.Name}();");
            sb.AppendLine();

            sb.AppendLine($"            BitRPC.Serialization.BitMask mask = null;");
            sb.AppendLine($"            try");
            sb.AppendLine($"            {{");
            sb.AppendLine($"                mask = BitRPC.Serialization.BitMaskPool.Get({fieldGroups.Count});");
            sb.AppendLine($"                mask.Read(reader, {fieldGroups.Count});");

            foreach (var field in message.Fields)
            {
                int index = field.Id - 1;
                int group = index / 32;
                int bitPosition = index % 32;
                sb.AppendLine($"                if (mask.GetBit({bitPosition}, {group})) {GenerateReadField(field)}");
            }

            sb.AppendLine("                return message;");
            sb.AppendLine("            }");
            sb.AppendLine("            finally");
            sb.AppendLine("            {");
            sb.AppendLine("                if (mask != null) BitRPC.Serialization.BitMaskPool.Return(mask);");
            sb.AppendLine("            }");
            sb.AppendLine("        }");
            sb.AppendLine();

            // add constructor to initialize static instance
            sb.AppendLine($"        public {message.Name}Handler()" + "{ _instance = this; }");
            sb.AppendLine($"        public static {message.Name}Handler _instance{{ get; private set; }}");
            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }
            sb.AppendLine();
            sb.AppendLine("namespace BitRPC.Serialization{");
            sb.AppendLine("    public static partial class Types{");
            sb.AppendLine($"        public static bool IsDefault({message.Name} message)"+ "{");
            sb.AppendLine("            if (message == null) return true;");
            foreach (var field in message.Fields)
            {
                sb.AppendLine($"            if (!IsDefault(message.{field.Name})) return false;");
            }
            sb.AppendLine("            return true;");
            sb.AppendLine("        }");
            //sb.AppendLine($"        public static void Write(this StreamWriter writer, {message.Name} message) => {message.Name}Handler.WriteStatic(message, writer);");
            sb.AppendLine("    }");

            // streamwriter partial class
            sb.AppendLine("    public partial class StreamWriter{");
            sb.AppendLine($"        public void Write{message.Name}({message.Name} message) => {message.Name}Handler._instance.Write(message, this);");
            sb.AppendLine("    }");

            // streamreader partial class
            sb.AppendLine("    public partial class StreamReader{");
            sb.AppendLine($"        public {message.Name} Read{message.Name}() => ({message.Name}){message.Name}Handler._instance.Read(this);");
            sb.AppendLine("    }");

            sb.AppendLine("}");

            return sb.ToString();
        }

        private void GenerateSerializerRegistry(ProtocolDefinition definition, GenerationOptions options, string serializationDir)
        {
            var filePath = Path.Combine(serializationDir, "SerializerRegistry.cs");
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader("SerializerRegistry.cs", options));
            sb.AppendLine("using System;");
            sb.AppendLine("using BitRPC.Serialization;");
            sb.AppendLine();
            
            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"namespace {options.Namespace}.Serialization");
                sb.AppendLine("{");
            }

            sb.AppendLine("    public static class SerializerRegistry");
            sb.AppendLine("    {");
            sb.AppendLine("        public static void RegisterSerializers()");
            sb.AppendLine("        {");
            sb.AppendLine("            var serializer = BufferSerializer.Instance;");

            foreach (var message in definition.Messages)
            {
                sb.AppendLine($"            serializer.RegisterHandler<{message.Name}>(new {message.Name}Handler());");
            }

            sb.AppendLine("        }");
            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }

            File.WriteAllText(filePath, sb.ToString());
        }

        private void GenerateClientCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var clientDir = Path.Combine(baseDir, "Client");
            EnsureDirectoryExists(clientDir);

            foreach (var service in definition.Services)
            {
                var filePath = Path.Combine(clientDir, $"{service.Name}Client.cs");
                var content = GenerateServiceClient(service, options);
                File.WriteAllText(filePath, content);
            }
        }

        private string GenerateServiceClient(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}Client.cs", options));
            sb.AppendLine("using System;");
            sb.AppendLine("using System.Threading.Tasks;");
            sb.AppendLine("using System.Collections.Generic;");
            sb.AppendLine("using BitRPC.Client;");
            sb.AppendLine();
            
            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"namespace {options.Namespace}.Client");
                sb.AppendLine("{");
            }

            sb.AppendLine($"    public class {service.Name}Client : BaseClient");
            sb.AppendLine("    {");
            sb.AppendLine($"        public {service.Name}Client(IRpcClient client) : base(client)");
            sb.AppendLine("        {");
            sb.AppendLine("        }");
            sb.AppendLine();

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"        public async IAsyncEnumerable<{method.ResponseType}> {method.Name}StreamAsync({method.RequestType} request)");
                    sb.AppendLine("        {");
                    sb.AppendLine($"            await foreach (var item in StreamAsync<{method.RequestType}, {method.ResponseType}>(\"{service.Name}.{method.Name}\", request))");
                    sb.AppendLine("            {");
                    sb.AppendLine("                yield return item;");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                else
                {
                    sb.AppendLine($"        public async Task<{method.ResponseType}> {method.Name}Async({method.RequestType} request)");
                    sb.AppendLine("        {");
                    sb.AppendLine($"            return await CallAsync<{method.RequestType}, {method.ResponseType}>(\"{service.Name}.{method.Name}\", request);");
                    sb.AppendLine("        }");
                }
            }

            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }

            return sb.ToString();
        }

        private void GenerateServerCode(ProtocolDefinition definition, GenerationOptions options, string baseDir)
        {
            var serverDir = Path.Combine(baseDir, "Server");
            EnsureDirectoryExists(serverDir);

            foreach (var service in definition.Services)
            {
                var filePath = Path.Combine(serverDir, $"I{service.Name}Service.cs");
                var content = GenerateServiceInterface(service, options);
                File.WriteAllText(filePath, content);

                var implFilePath = Path.Combine(serverDir, $"{service.Name}ServiceBase.cs");
                var implContent = GenerateServiceBase(service, options);
                File.WriteAllText(implFilePath, implContent);
            }
        }

        private string GenerateServiceInterface(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"I{service.Name}Service.cs", options));
            sb.AppendLine("using System;");
            sb.AppendLine("using System.Threading.Tasks;");
            sb.AppendLine("using System.Collections.Generic;");
            sb.AppendLine();
            
            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"namespace {options.Namespace}.Server");
                sb.AppendLine("{");
            }

            sb.AppendLine($"    public interface I{service.Name}Service");
            sb.AppendLine("    {");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"        IAsyncEnumerable<{method.ResponseType}> {method.Name}StreamAsync({method.RequestType} request);");
                }
                else
                {
                    sb.AppendLine($"        Task<{method.ResponseType}> {method.Name}Async({method.RequestType} request);");
                }
            }

            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }

            return sb.ToString();
        }

        private string GenerateServiceBase(ProtocolService service, GenerationOptions options)
        {
            var sb = new StringBuilder();
            sb.AppendLine(GenerateFileHeader($"{service.Name}ServiceBase.cs", options));
            sb.AppendLine("using System;");
            sb.AppendLine("using System.Threading.Tasks;");
            sb.AppendLine("using System.Collections.Generic;");
            sb.AppendLine("using BitRPC.Server;");
            sb.AppendLine();
            
            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine($"namespace {options.Namespace}.Server");
                sb.AppendLine("{");
            }

            sb.AppendLine($"    public abstract class {service.Name}ServiceBase : BaseService, I{service.Name}Service");
            sb.AppendLine("    {");
            sb.AppendLine($"        public override string ServiceName => \"{service.Name}\";");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"        public abstract IAsyncEnumerable<{method.ResponseType}> {method.Name}StreamAsync({method.RequestType} request);");
                }
                else
                {
                    sb.AppendLine($"        public abstract Task<{method.ResponseType}> {method.Name}Async({method.RequestType} request);");
                }
            }

            sb.AppendLine();
            sb.AppendLine("        protected override void RegisterMethods()");
            sb.AppendLine("        {");

            foreach (var method in service.Methods)
            {
                if (method.ResponseStream)
                {
                    sb.AppendLine($"            RegisterServerStreamMethod<{method.RequestType}, {method.ResponseType}>(\"{method.Name}\", {method.Name}StreamAsync);");
                }
                else
                {
                    sb.AppendLine($"            RegisterMethod<{method.RequestType}, {method.ResponseType}>(\"{method.Name}\", {method.Name}Async);");
                }
            }

            sb.AppendLine("        }");
            sb.AppendLine("    }");

            if (!string.IsNullOrEmpty(options.Namespace))
            {
                sb.AppendLine("}");
            }

            return sb.ToString();
        }

        private string GetCSharpTypeName(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "int",
                FieldType.Int64 => "long",
                FieldType.Float => "float",
                FieldType.Double => "double",
                FieldType.Bool => "bool",
                FieldType.String => "string",
                FieldType.Vector3 => "Vector3",
                FieldType.DateTime => "DateTime",
                _ => "object"
            };
        }

        private string GetCSharpType(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                return $"List<{GetCSharpTypeNameForField(field)}>"; // incorrect placeholder removed later
            }
            return GetCSharpTypeNameForField(field);
        }

        private string GetCSharpTypeNameForField(ProtocolField field)
        {
            if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
            {
                return field.CustomType;
            }
            return GetCSharpTypeName(field.Type);
        }

        private string GetDefaultValue(ProtocolField field)
        {
            if (field.IsRepeated) return $"new List<{GetCSharpTypeNameForField(field)}>()";

            return GetDefaultValueForType(field.Type);
        }

        private string GetDefaultValueForType(FieldType type)
        {
            return type switch
            {
                FieldType.Int32 => "0",
                FieldType.Int64 => "0L",
                FieldType.Float => "0.0f",
                FieldType.Double => "0.0",
                FieldType.Bool => "false",
                FieldType.String => "string.Empty",
                _ => "default"
            };
        }

        private string GenerateWriteField(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                return $"writer.WriteList(message.{field.Name}, writer.Write{GetTypeName(field)});";
            }

            return $"writer.Write{GetTypeName(field)}(message.{field.Name});";
        }

        private string GetTypeName(ProtocolField field)
        {
            if (field.Type == FieldType.Struct && !string.IsNullOrEmpty(field.CustomType))
            {
                return $"{field.CustomType}";
            }
            return $"{field.Type}";
        }

        private string GenerateReadField(ProtocolField field)
        {
            if (field.IsRepeated)
            {
                return $"message.{field.Name} = reader.ReadList(reader.Read{GetTypeName(field)});";
            }

            return $"message.{field.Name} = reader.Read{GetTypeName(field)}();";
        }
    }
}