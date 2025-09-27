using System;
using System.Collections.Generic;
using System.IO;
using Newtonsoft.Json;
using BitRPC.Protocol.Generator;
using BitRPC.Protocol.Parser;

namespace BitRPC.GeneratorApp
{
    public class LanguageConfig
    {
        public string Name { get; set; }
        public bool Enabled { get; set; }
        public string Namespace { get; set; }
        public string RuntimePath { get; set; }
        public string OutputDirectory { get; set; }
        public Dictionary<string, object> SpecificOptions { get; set; } = new Dictionary<string, object>();
    }

    public class GeneratorConfig
    {
        public string ProtocolFile { get; set; }
        public List<LanguageConfig> Languages { get; set; }
    }

    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("BitRPC Protocol Generator");
            Console.WriteLine("===========================");

            string configPath = "generator-config.json";
            if (args.Length > 0)
            {
                configPath = args[0];
            }

            if (!File.Exists(configPath))
            {
                Console.WriteLine($"Error: Config file '{configPath}' not found.");
                return;
            }

            try
            {
                var configContent = File.ReadAllText(configPath);
                var config = JsonConvert.DeserializeObject<GeneratorConfig>(configContent);

#pragma warning disable CS8602 // 解引用可能出现空引用。
                if (!File.Exists(config.ProtocolFile))
                {
                    Console.WriteLine($"Error: Protocol file '{config.ProtocolFile}' not found.");
                    return;
                }
#pragma warning restore CS8602 // 解引用可能出现空引用。

                var generator = new ProtocolGenerator();
                
                var optionsList = new List<GenerationOptions>();
                
                foreach (var lang in config.Languages)
                {
                    if (lang.Enabled)
                    {
                        var outputDir = lang.OutputDirectory;
                        var options = new GenerationOptions
                        {
                            Language = (TargetLanguage)Enum.Parse(typeof(TargetLanguage), lang.Name, true),
                            OutputDirectory = outputDir,
                            Namespace = lang.Namespace,
                            GenerateSerialization = true,
                            GenerateClientServer = true,
                            GenerateFactories = true,
                            LanguageSpecificOptions = lang.SpecificOptions ?? new Dictionary<string, object>()
                        };

                        // Ensure runtime is copied if configured
                        if (!string.IsNullOrWhiteSpace(lang.RuntimePath) && Directory.Exists(lang.RuntimePath))
                        {
                            // Runtime subdirectory under the language output, default to "runtime"
                            var runtimeSubdir = GetRuntimeSubdir(options);
                            var destRuntimeDir = Path.Combine(outputDir, runtimeSubdir);

                            Console.WriteLine($"Copying runtime for {lang.Name} from '{lang.RuntimePath}' to '{destRuntimeDir}'...");
                            CopyDirectory(lang.RuntimePath, destRuntimeDir);

                            TryEnsurePythonRuntimePackageInit(destRuntimeDir);

                            // Set language-specific hints so codegen can reference runtime relatively
                            switch (options.Language)
                            {
                                case TargetLanguage.Python:
                                {
                                    var py = GetOrCreateChildOptions(options.LanguageSpecificOptions, "Python");
                                    // Use relative imports within generated package
                                    py["UseRelativeImports"] = true;
                                    // RuntimeImportBase like ".runtime.bitrpc" so generators can import from it
                                    py["RuntimeImportBase"] = $".{runtimeSubdir}.bitrpc";
                                    break;
                                }
                                case TargetLanguage.Cpp:
                                {
                                    var cpp = GetOrCreateChildOptions(options.LanguageSpecificOptions, "Cpp");
                                    // Allow generator to add include_directories for runtime headers
                                    cpp["RuntimeIncludeDir"] = runtimeSubdir; // relative to base output dir
                                    break;
                                }
                            }
                        }
                        
                        optionsList.Add(options);
                    }
                }

                generator.GenerateMultiple(config.ProtocolFile, optionsList);

                Console.WriteLine($"Successfully generated code for '{config.ProtocolFile}'");
                
                var generatedLanguages = new List<string>();
                foreach (var lang in config.Languages)
                {
                    if (lang.Enabled)
                    {
                        generatedLanguages.Add(lang.Name);
                    }
                }
                Console.WriteLine($"Generated languages: {string.Join(", ", generatedLanguages)}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
                Console.WriteLine(ex.StackTrace);
            }
        }

        private static string GetRuntimeSubdir(GenerationOptions options)
        {
            // Allow override via LanguageSpecificOptions: { "RuntimeSubdir": "xyz" } or language-scoped
            if (options.LanguageSpecificOptions != null)
            {
                if (options.LanguageSpecificOptions.TryGetValue("RuntimeSubdir", out var genericSubdirObj) && genericSubdirObj is string genericSubdir && !string.IsNullOrWhiteSpace(genericSubdir))
                {
                    return genericSubdir;
                }

                // Language scoped
                string scope = options.Language.ToString();
                if (options.LanguageSpecificOptions.TryGetValue(scope, out var childObj) && childObj is Dictionary<string, object> childDict)
                {
                    if (childDict.TryGetValue("RuntimeSubdir", out var subdirObj) && subdirObj is string subdir && !string.IsNullOrWhiteSpace(subdir))
                    {
                        return subdir;
                    }
                }
            }

            return "runtime";
        }

        private static Dictionary<string, object> GetOrCreateChildOptions(Dictionary<string, object> root, string key)
        {
            if (!root.TryGetValue(key, out var child) || child is not Dictionary<string, object> dict)
            {
                dict = new Dictionary<string, object>();
                root[key] = dict;
            }
            return dict;
        }

        private static void CopyDirectory(string sourceDir, string destinationDir)
        {
            if (!Directory.Exists(destinationDir))
            {
                Directory.CreateDirectory(destinationDir);
            }

            foreach (var file in Directory.GetFiles(sourceDir))
            {
                var destFileName = Path.Combine(destinationDir, Path.GetFileName(file));
                File.Copy(file, destFileName, true);
            }

            foreach (var dir in Directory.GetDirectories(sourceDir))
            {
                var destSubDir = Path.Combine(destinationDir, Path.GetFileName(dir));
                CopyDirectory(dir, destSubDir);
            }
        }

        private static void TryEnsurePythonRuntimePackageInit(string destRuntimeDir)
        {
            try
            {
                // Ensure the runtime directory is a Python package for relative imports
                var initPath = Path.Combine(destRuntimeDir, "__init__.py");
                if (!File.Exists(initPath))
                {
                    File.WriteAllText(initPath, "# Runtime package for generated BitRPC code\n");
                }
            }
            catch
            {
                // Ignore non-critical errors while creating __init__.py
            }
        }
    }
}