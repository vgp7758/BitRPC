/*
 * 跨语言共享内存测试 - C#示例
 * 测试与C++和Python的跨语言通信
 */

using System;
using System.Collections.Generic;
using System.Threading;
using BitRPC.SharedMemory;

// 测试数据结构
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TestData
{
    public int Id;
    public double Value;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
    public string Message;
    public ulong Timestamp;
}

public class CrossLanguageTestProducer
{
    private readonly string name;
    private volatile bool running;
    private SharedMemoryProducer producer;
    private Thread sendThread;

    public CrossLanguageTestProducer(string name = "CrossLangTest")
    {
        this.name = name;
        this.running = false;
    }

    ~CrossLanguageTestProducer()
    {
        Stop();
    }

    public bool Start()
    {
        if (running) return true;

        try
        {
            // 创建生产者
            producer = SharedMemoryFactory.CreateProducer(name, 1024 * 1024); // 1MB buffer
            if (producer == null)
            {
                Console.WriteLine("✗ Failed to create producer");
                return false;
            }

            Console.WriteLine("✓ C# Producer connected to shared memory: " + name);
            running = true;

            // 启动发送线程
            sendThread = new Thread(SendLoop);
            sendThread.IsBackground = true;
            sendThread.Start();

            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"✗ Error starting producer: {ex.Message}");
            return false;
        }
    }

    public void Stop()
    {
        if (!running) return;

        running = false;

        if (sendThread != null && sendThread.IsAlive)
        {
            sendThread.Join();
        }

        producer?.Dispose();
        Console.WriteLine("✓ C# Producer stopped");
    }

    public bool IsRunning => running;

    private void SendLoop()
    {
        int counter = 0;

        while (running)
        {
            try
            {
                // 发送文本消息
                string textMessage = $"Hello from C#! Message #{counter}";
                if (producer.SendString(textMessage))
                {
                    Console.WriteLine($"Sent text: {textMessage}");
                }

                Thread.Sleep(100);

                // 发送结构化数据
                var data = new TestData
                {
                    Id = counter,
                    Value = 2.71828 * counter,
                    Message = $"C# Data #{counter}",
                    Timestamp = (ulong)DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()
                };

                var message = new SharedMemoryMessage(MessageType.Data, StructureToBytes(data));
                message.SetFlag(MessageFlags.Urgent);

                if (producer.SendMessage(message))
                {
                    Console.WriteLine($"Sent structured data: id={data.Id}, value={data.Value}");
                }

                // 发送心跳
                if (counter % 10 == 0)
                {
                    if (producer.SendMessage(MessageType.Heartbeat, Array.Empty<byte>()))
                    {
                        Console.WriteLine("Sent heartbeat");
                    }
                }

                counter++;

                // 等待一段时间
                Thread.Sleep(500);

            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in send loop: {ex.Message}");
                Thread.Sleep(1000);
            }
        }
    }

    private static byte[] StructureToBytes<T>(T structure) where T : struct
    {
        int size = Marshal.SizeOf<T>();
        byte[] arr = new byte[size];
        IntPtr ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(structure, ptr, true);
            Marshal.Copy(ptr, arr, 0, size);
            return arr;
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }
}

public class CrossLanguageTestConsumer
{
    private readonly string name;
    private volatile bool running;
    private SharedMemoryConsumer consumer;
    private Thread receiveThread;

    public CrossLanguageTestConsumer(string name = "CrossLangTest")
    {
        this.name = name;
        this.running = false;
    }

    ~CrossLanguageTestConsumer()
    {
        Stop();
    }

    public bool Start()
    {
        if (running) return true;

        try
        {
            // 创建消费者
            consumer = SharedMemoryFactory.CreateConsumer(name, 1024 * 1024); // 1MB buffer
            if (consumer == null)
            {
                Console.WriteLine("✗ Failed to create consumer");
                return false;
            }

            Console.WriteLine("✓ C# Consumer connected to shared memory: " + name);
            running = true;

            // 启动接收线程
            receiveThread = new Thread(ReceiveLoop);
            receiveThread.IsBackground = true;
            receiveThread.Start();

            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"✗ Error starting consumer: {ex.Message}");
            return false;
        }
    }

    public void Stop()
    {
        if (!running) return;

        running = false;

        if (receiveThread != null && receiveThread.IsAlive)
        {
            receiveThread.Join();
        }

        consumer?.Dispose();
        Console.WriteLine("✓ C# Consumer stopped");
    }

    public bool IsRunning => running;

    private void ReceiveLoop()
    {
        while (running)
        {
            try
            {
                // 尝试接收消息
                if (consumer.ReceiveMessage(out var message, 100))
                {
                    Console.WriteLine($"Received message: type={message.MessageType}, size={message.Payload.Length} bytes");

                    if (message.MessageType == MessageType.Data && message.Payload.Length == Marshal.SizeOf<TestData>())
                    {
                        var data = BytesToStructure<TestData>(message.Payload);
                        Console.WriteLine($"  Parsed data: id={data.Id}, value={data.Value}, message={data.Message}, timestamp={data.Timestamp}");
                    }
                    else if (message.MessageType == MessageType.Heartbeat)
                    {
                        Console.WriteLine("  Received heartbeat from producer");
                    }
                    else if (message.MessageType == MessageType.Control)
                    {
                        string text = System.Text.Encoding.UTF8.GetString(message.Payload);
                        Console.WriteLine($"  Received control message: {text}");
                    }

                    continue;
                }

                // 尝试接收字符串
                if (consumer.ReceiveString(out var text, 100))
                {
                    Console.WriteLine($"Received string: {text}");
                    continue;
                }

                // 尝试接收原始数据
                if (consumer.Receive(out var data, 100))
                {
                    Console.WriteLine($"Received raw data: {data.Length} bytes");
                    continue;
                }

                // 如果没有数据，短暂休眠
                Thread.Sleep(50);

            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in receive loop: {ex.Message}");
                Thread.Sleep(1000);
            }
        }
    }

    private static T BytesToStructure<T>(byte[] bytes) where T : struct
    {
        IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
        try
        {
            Marshal.Copy(bytes, 0, ptr, bytes.Length);
            return Marshal.PtrToStructure<T>(ptr);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }
}

public class Program
{
    public static void Main(string[] args)
    {
        Console.WriteLine("BitRPC Cross-Language Shared Memory Test (C#)");
        Console.WriteLine("============================================");

        string mode = "producer"; // 默认为生产者
        string name = "CrossLangTest";

        // 解析命令行参数
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--mode" && i + 1 < args.Length)
            {
                mode = args[i + 1];
                i++;
            }
            else if (args[i] == "--name" && i + 1 < args.Length)
            {
                name = args[i + 1];
                i++;
            }
            else if (args[i] == "--help" || args[i] == "-h")
            {
                Console.WriteLine("Usage: Program.exe [options]");
                Console.WriteLine("Options:");
                Console.WriteLine("  --mode MODE    Set mode (producer/consumer)");
                Console.WriteLine("  --name NAME    Set shared memory name");
                Console.WriteLine("  --help, -h     Show this help");
                return;
            }
        }

        Console.WriteLine($"Mode: {mode}");
        Console.WriteLine($"Shared Memory Name: {name}");
        Console.WriteLine();

        try
        {
            if (mode == "producer")
            {
                using (var producer = new CrossLanguageTestProducer(name))
                {
                    if (!producer.Start())
                    {
                        Console.WriteLine("Failed to start producer");
                        return;
                    }

                    Console.WriteLine("Producer running. Press Ctrl+C to stop...");

                    // 等待用户中断
                    while (producer.IsRunning)
                    {
                        Thread.Sleep(1000);
                    }
                }
            }
            else if (mode == "consumer")
            {
                using (var consumer = new CrossLanguageTestConsumer(name))
                {
                    if (!consumer.Start())
                    {
                        Console.WriteLine("Failed to start consumer");
                        return;
                    }

                    Console.WriteLine("Consumer running. Press Ctrl+C to stop...");

                    // 等待用户中断
                    while (consumer.IsRunning)
                    {
                        Thread.Sleep(1000);
                    }
                }
            }
            else
            {
                Console.WriteLine($"Invalid mode: {mode}");
                Console.WriteLine("Supported modes: producer, consumer");
                return;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error: {ex.Message}");
            Console.WriteLine($"Stack trace: {ex.StackTrace}");
            return;
        }

        Console.WriteLine("Test completed successfully!");
    }
}