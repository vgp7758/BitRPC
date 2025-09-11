using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using BitMask = BitRPC.Serialization.BitMask;
using BitRPC.Serialization;

namespace ObjectPoolPerformanceTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("BitRPC Object Pool Performance Test");
            Console.WriteLine("===================================");
            
            const int iterations = 100000;
            
            // Test without object pool
            var stopwatch = Stopwatch.StartNew();
            for (int i = 0; i < iterations; i++)
            {
                var mask = new BitMask();
                mask.SetBit(0, true);
                mask.SetBit(1, false);
                // Simulate serialization
                var writer = new BitRPC.Serialization.StreamWriter();
                mask.Write(writer);
                var data = writer.ToArray();
                
                // Simulate deserialization
                var reader = new BitRPC.Serialization.StreamReader(data);
                var readMask = new BitMask();
                readMask.Read(reader);
            }
            stopwatch.Stop();
            Console.WriteLine($"Without object pool: {stopwatch.ElapsedMilliseconds}ms for {iterations} iterations");
            
            // Test with object pool
            stopwatch.Restart();
            for (int i = 0; i < iterations; i++)
            {
                BitMask mask = null;
                BitMask readMask = null;
                try
                {
                    mask = BitMaskPool.Get(1); // size=1 for single bitmask
                    mask.SetBit(0, true);
                    mask.SetBit(1, false);
                    // Simulate serialization
                    var writer = new BitRPC.Serialization.StreamWriter();
                    mask.Write(writer);
                    var data = writer.ToArray();
                    
                    // Simulate deserialization
                    var reader = new BitRPC.Serialization.StreamReader(data);
                    readMask = BitMaskPool.Get(1); // size=1 for single bitmask
                    readMask.Read(reader);
                }
                finally
                {
                    if (mask != null) BitMaskPool.Return(mask);
                    if (readMask != null) BitMaskPool.Return(readMask);
                }
            }
            stopwatch.Stop();
            Console.WriteLine($"With object pool: {stopwatch.ElapsedMilliseconds}ms for {iterations} iterations");
            
            // Show pool statistics
            var stats = BitMaskPool.GetStats();
            foreach (var kvp in stats)
            {
                Console.WriteLine($"Pool Size {kvp.Key}: Count={kvp.Value.Count}, MaxSize={kvp.Value.MaxSize}");
            }
            
            Console.WriteLine("\nTest completed successfully!");
        }
    }
}