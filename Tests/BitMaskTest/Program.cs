using System;
using System.Diagnostics;
using BitRPC.Serialization;

namespace BitMaskTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("BitMask Test");
            Console.WriteLine("============");
            
            // Test BitMask directly
            var mask = BitMaskPool.Get(1);
            
            // Set bit 0 to true, bit 1 to false
            mask.SetBit(0, true);
            mask.SetBit(1, false);
            
            Console.WriteLine($"Bit 0: {mask.GetBit(0)}");
            Console.WriteLine($"Bit 1: {mask.GetBit(1)}");
            
            // Write to stream
            var writer = new BitRPC.Serialization.StreamWriter();
            mask.Write(writer);
            var data = writer.ToArray();
            
            Console.WriteLine($"Mask data: {BitConverter.ToString(data)}");
            Console.WriteLine($"Mask value: {BitConverter.ToUInt32(data, 0)}");
            
            // Read back
            var reader = new BitRPC.Serialization.StreamReader(data);
            var readMask = BitMaskPool.Get(1);
            readMask.Read(reader);
            
            Console.WriteLine($"Read Bit 0: {readMask.GetBit(0)}");
            Console.WriteLine($"Read Bit 1: {readMask.GetBit(1)}");
        }
    }
}