using System;
using System.Diagnostics;
using BitRPC.Serialization;
using Example.Protocol;
using Example.Protocol.Serialization;

namespace ComplexMessageDebugTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Debug Test for Complex Message Serialization");
            Console.WriteLine("==============================================");
            
            // Create a simple test message
            var message = new ComplexMessage();
            message.field1 = "First";
            message.field10 = "Tenth";
            
            Console.WriteLine($"Original field1: '{message.field1}'");
            Console.WriteLine($"Original field10: '{message.field10}'");
            
            // Test IsDefault method
            var serializer = new ComplexMessageSerializer();
            var isDefaultMethod = serializer.GetType().GetMethod("IsDefault", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            
            if (isDefaultMethod != null)
            {
                var isField1Default = (bool)isDefaultMethod.Invoke(serializer, new object[] { message.field1 });
                var isField10Default = (bool)isDefaultMethod.Invoke(serializer, new object[] { message.field10 });
                
                Console.WriteLine($"Is field1 default: {isField1Default}");
                Console.WriteLine($"Is field10 default: {isField10Default}");
            }
            
            // Test serialization
            var writer = new BitRPC.Serialization.StreamWriter();
            serializer.Write(message, writer);
            var data = writer.ToArray();
            
            Console.WriteLine($"Serialized data length: {data.Length}");
            Console.WriteLine($"Serialized data bytes: {BitConverter.ToString(data)}");
            
            // Test deserialization
            var reader = new BitRPC.Serialization.StreamReader(data);
            var deserialized = (ComplexMessage)serializer.Read(reader);
            
            Console.WriteLine($"Deserialized field1: '{deserialized.field1}'");
            Console.WriteLine($"Deserialized field10: '{deserialized.field10}'");
            
            // Test bit mask directly
            Console.WriteLine("\nTesting bit mask directly...");
            var mask = BitMaskPool.Get(2);
            mask.SetBit(0, !string.IsNullOrEmpty(message.field1));
            mask.SetBit(9, !string.IsNullOrEmpty(message.field10));
            
            Console.WriteLine($"Bit 0 (field1): {mask.GetBit(0)}");
            Console.WriteLine($"Bit 9 (field10): {mask.GetBit(9)}");
            
            var testWriter = new BitRPC.Serialization.StreamWriter();
            mask.Write(testWriter);
            var maskData = testWriter.ToArray();
            Console.WriteLine($"Mask data bytes: {BitConverter.ToString(maskData)}");
            
            var testReader = new BitRPC.Serialization.StreamReader(maskData);
            var testMask = BitMaskPool.Get(2);
            testMask.Read(testReader);
            
            Console.WriteLine($"Read mask bit 0: {testMask.GetBit(0)}");
            Console.WriteLine($"Read mask bit 9: {testMask.GetBit(9)}");
        }
    }
}