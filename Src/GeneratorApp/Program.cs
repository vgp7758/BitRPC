using System;
using System.Collections.Generic;
using System.IO;
using BitRPC.Protocol.Generator;
using BitRPC.Protocol.Parser;

namespace BitRPC.GeneratorApp
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("BitRPC Protocol Generator");
            Console.WriteLine("===========================");

            if (args.Length < 1)
            {
                Console.WriteLine("Usage: BitRPC.GeneratorApp <protocol-file> [output-directory]");
                return;
            }

            var protocolFile = args[0];
            var outputDir = args.Length > 1 ? args[1] : "Generated";

            if (!File.Exists(protocolFile))
            {
                Console.WriteLine($"Error: Protocol file '{protocolFile}' not found.");
                return;
            }

            try
            {
                var generator = new ProtocolGenerator();
                
                var csharpOptions = new GenerationOptions
                {
                    Language = TargetLanguage.CSharp,
                    OutputDirectory = Path.Combine(outputDir, "csharp"),
                    Namespace = "Example.Protocol",
                    GenerateSerialization = true,
                    GenerateClientServer = true,
                    GenerateFactories = true
                };

                var pythonOptions = new GenerationOptions
                {
                    Language = TargetLanguage.Python,
                    OutputDirectory = Path.Combine(outputDir, "python"),
                    Namespace = "example.protocol",
                    GenerateSerialization = true,
                    GenerateClientServer = true,
                    GenerateFactories = true
                };

                var cppOptions = new GenerationOptions
                {
                    Language = TargetLanguage.Cpp,
                    OutputDirectory = Path.Combine(outputDir, "cpp"),
                    Namespace = "example.protocol",
                    GenerateSerialization = true,
                    GenerateClientServer = true,
                    GenerateFactories = true
                };

                generator.GenerateMultiple(protocolFile, new List<GenerationOptions>
                {
                    csharpOptions,
                    pythonOptions,
                    cppOptions
                });

                Console.WriteLine($"Successfully generated code for '{protocolFile}'");
                Console.WriteLine($"Output directory: {outputDir}");
                Console.WriteLine("Generated languages: C#, Python, C++");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
                Console.WriteLine(ex.StackTrace);
            }
        }
    }
}