using System;
using System.Diagnostics;
using BitRPC.Serialization;
using Example.Protocol;
using Example.Protocol.Serialization;

namespace ComplexMessageSerializationTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Complex Message Serialization Test with Size-Based Object Pool");
            Console.WriteLine("==============================================================");
            
            // Test data
            var testData = new string[] {
                "Hello", "World", "BitRPC", "Serialization", "Test",
                "Field1", "Field2", "Field3", "Field4", "Field5",
                "Field6", "Field7", "Field8", "Field9", "Field10",
                "Field11", "Field12", "Field13", "Field14", "Field15",
                "Field16", "Field17", "Field18", "Field19", "Field20",
                "Field21", "Field22", "Field23", "Field24", "Field25",
                "Field26", "Field27", "Field28", "Field29", "Field30",
                "Field31", "Field32", "Field33", "Field34", "Field35",
                "Field36", "Field37", "Field38", "Field39", "Field40"
            };
            
            const int iterations = 1000;
            var stopwatch = Stopwatch.StartNew();
            
            // Test serialization and deserialization
            for (int i = 0; i < iterations; i++)
            {
                // Create complex message with all fields populated
                var originalMessage = new ComplexMessage();
                for (int j = 0; j < testData.Length; j++)
                {
                    var field = originalMessage.GetType().GetField($"field{j + 1}");
                    if (field != null)
                    {
                        field.SetValue(originalMessage, testData[j] + $"_iteration_{i}");
                    }
                }
                
                // Serialize
                var writer = new BitRPC.Serialization.StreamWriter();
                var serializer = new ComplexMessageSerializer();
                serializer.Write(originalMessage, writer);
                var data = writer.ToArray();
                
                // Deserialize
                var reader = new BitRPC.Serialization.StreamReader(data);
                var deserializedMessage = (ComplexMessage)serializer.Read(reader);
                
                // Verify all fields
                for (int j = 0; j < testData.Length; j++)
                {
                    var field = deserializedMessage.GetType().GetField($"field{j + 1}");
                    if (field != null)
                    {
                        var expectedValue = testData[j] + $"_iteration_{i}";
                        var actualValue = field.GetValue(deserializedMessage) as string;
                        if (actualValue != expectedValue)
                        {
                            throw new Exception($"Field field{j + 1} mismatch: expected '{expectedValue}', got '{actualValue}'");
                        }
                    }
                }
            }
            
            stopwatch.Stop();
            Console.WriteLine($"✅ {iterations} iterations completed successfully in {stopwatch.ElapsedMilliseconds}ms");
            Console.WriteLine($"✅ Average time per iteration: {stopwatch.ElapsedMilliseconds / (double)iterations:F2}ms");
            
            // Show object pool statistics
            Console.WriteLine("\n📊 Object Pool Statistics:");
            var stats = BitMaskPool.GetStats();
            foreach (var kvp in stats)
            {
                Console.WriteLine($"   Size {kvp.Key}: Count={kvp.Value.Count}, MaxSize={kvp.Value.MaxSize}");
            }
            
            // Test with sparse fields (only some fields set)
            Console.WriteLine("\n🧪 Testing sparse field serialization...");
            var sparseMessage = new ComplexMessage();
            // Set only specific fields, others should remain empty (default)
            sparseMessage.field1 = "First";
            sparseMessage.field10 = "Tenth";
            sparseMessage.field20 = "Twentieth";
            sparseMessage.field30 = "Thirtieth";
            sparseMessage.field40 = "Fortieth";
            
            var sparseWriter = new BitRPC.Serialization.StreamWriter();
            var sparseSerializer = new ComplexMessageSerializer();
            sparseSerializer.Write(sparseMessage, sparseWriter);
            var sparseData = sparseWriter.ToArray();
            
            var sparseReader = new BitRPC.Serialization.StreamReader(sparseData);
            var sparseDeserialized = (ComplexMessage)sparseSerializer.Read(sparseReader);
            
            // Verify sparse fields
            Console.WriteLine($"field1: '{sparseDeserialized.field1}' (expected: 'First')");
            Console.WriteLine($"field10: '{sparseDeserialized.field10}' (expected: 'Tenth')");
            Console.WriteLine($"field20: '{sparseDeserialized.field20}' (expected: 'Twentieth')");
            Console.WriteLine($"field30: '{sparseDeserialized.field30}' (expected: 'Thirtieth')");
            Console.WriteLine($"field40: '{sparseDeserialized.field40}' (expected: 'Fortieth')");
            
            if (sparseDeserialized.field1 != "First") throw new Exception($"Sparse field1 failed: expected 'First', got '{sparseDeserialized.field1}'");
            if (sparseDeserialized.field10 != "Tenth") throw new Exception($"Sparse field10 failed: expected 'Tenth', got '{sparseDeserialized.field10}'");
            if (sparseDeserialized.field20 != "Twentieth") throw new Exception($"Sparse field20 failed: expected 'Twentieth', got '{sparseDeserialized.field20}'");
            if (sparseDeserialized.field30 != "Thirtieth") throw new Exception($"Sparse field30 failed: expected 'Thirtieth', got '{sparseDeserialized.field30}'");
            if (sparseDeserialized.field40 != "Fortieth") throw new Exception($"Sparse field40 failed: expected 'Fortieth', got '{sparseDeserialized.field40}'");
            
            // Verify other fields are empty
            for (int j = 2; j <= 40; j++)
            {
                if (j != 10 && j != 20 && j != 30 && j != 40)
                {
                    var field = sparseDeserialized.GetType().GetField($"field{j}");
                    if (field != null)
                    {
                        var value = field.GetValue(sparseDeserialized) as string;
                        if (value != string.Empty)
                        {
                            throw new Exception($"Sparse field{j} should be empty but got '{value}'");
                        }
                    }
                }
            }
            
            Console.WriteLine("✅ Sparse field serialization test passed!");
            Console.WriteLine("✅ All complex message serialization tests passed!");
            
            // Show final pool statistics
            Console.WriteLine("\n📊 Final Object Pool Statistics:");
            var finalStats = BitMaskPool.GetStats();
            foreach (var kvp in finalStats)
            {
                Console.WriteLine($"   Size {kvp.Key}: Count={kvp.Value.Count}, MaxSize={kvp.Value.MaxSize}");
            }
        }
    }
}