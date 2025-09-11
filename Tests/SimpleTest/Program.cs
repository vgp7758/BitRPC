using System;
using System.Diagnostics;
using BitRPC.Serialization;
using Example.Protocol;
using Example.Protocol.Serialization;

namespace SimpleTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Simple Test for Complex Message Serialization");
            Console.WriteLine("==============================================");
            
            // Create a simple test message
            var message = new ComplexMessage();
            message.field1 = "First";
            // field2 should be string.Empty (default)
            message.field10 = "Tenth";
            
            Console.WriteLine($"Original field1: '{message.field1}'");
            Console.WriteLine($"Original field2: '{message.field2}'");
            Console.WriteLine($"Original field10: '{message.field10}'");
            
            // Test IsDefault method
            var serializer = new ComplexMessageSerializer();
            var isDefaultMethod = serializer.GetType().GetMethod("IsDefault", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            
            if (isDefaultMethod != null)
            {
                var isField1Default = (bool)isDefaultMethod.Invoke(serializer, new object[] { message.field1 });
                var isField2Default = (bool)isDefaultMethod.Invoke(serializer, new object[] { message.field2 });
                var isField10Default = (bool)isDefaultMethod.Invoke(serializer, new object[] { message.field10 });
                
                Console.WriteLine($"Is field1 default: {isField1Default}");
                Console.WriteLine($"Is field2 default: {isField2Default}");
                Console.WriteLine($"Is field10 default: {isField10Default}");
            }
            
            // Test serialization
            var writer = new BitRPC.Serialization.StreamWriter();
            serializer.Write(message, writer);
            var data = writer.ToArray();
            
            Console.WriteLine($"Serialized data length: {data.Length}");
            Console.WriteLine($"Serialized data bytes: {BitConverter.ToString(data)}");
            
            // Let's manually decode the bitmask
            var bitmask = BitConverter.ToUInt32(data, 0);
            Console.WriteLine($"Bitmask: {bitmask} (0x{bitmask:X8})");
            for (int i = 0; i < 32; i++)
            {
                if ((bitmask & (1 << i)) != 0)
                {
                    Console.WriteLine($"Bit {i} is set");
                }
            }
        }
    }
}