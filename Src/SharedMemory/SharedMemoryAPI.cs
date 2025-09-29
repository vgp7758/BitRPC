using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace BitRPC.SharedMemory
{
    // 消息类型枚举
    public enum MessageType : uint
    {
        Data = 1,
        Control = 2,
        Heartbeat = 3,
        Error = 4,
        CustomMin = 1000
    }

    // 消息标志位
    [Flags]
    public enum MessageFlags : byte
    {
        None = 0,
        Urgent = 0x01,
        Compressed = 0x02,
        Encrypted = 0x04,
        LastFragment = 0x08
    }

    // 消息头结构
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MessageHeader
    {
        public uint MessageId;
        public uint MessageType;
        public uint PayloadSize;
        public ulong Timestamp;
        public byte Flags;
        private readonly byte Reserved1;
        private readonly byte Reserved2;
        private readonly byte Reserved3;
    }

    // 共享内存消息
    public class SharedMemoryMessage
    {
        private static uint nextId = 1;

        public uint MessageId { get; private set; }
        public MessageType MessageType { get; set; }
        public byte[] Payload { get; set; }
        public ulong Timestamp { get; private set; }
        public MessageFlags Flags { get; set; }

        public SharedMemoryMessage()
        {
            MessageId = nextId++;
            Timestamp = (ulong)DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
            Flags = MessageFlags.None;
        }

        public SharedMemoryMessage(MessageType type, byte[] payload) : this()
        {
            MessageType = type;
            Payload = payload ?? Array.Empty<byte>();
        }

        public byte[] Serialize()
        {
            var header = new MessageHeader
            {
                MessageId = MessageId,
                MessageType = (uint)MessageType,
                PayloadSize = (uint)(Payload?.Length ?? 0),
                Timestamp = Timestamp,
                Flags = (byte)Flags
            };

            byte[] headerBytes = StructureToBytes(header);
            byte[] result = new byte[headerBytes.Length + (Payload?.Length ?? 0)];

            Buffer.BlockCopy(headerBytes, 0, result, 0, headerBytes.Length);
            if (Payload != null && Payload.Length > 0)
            {
                Buffer.BlockCopy(Payload, 0, result, headerBytes.Length, Payload.Length);
            }

            return result;
        }

        public bool Deserialize(byte[] data)
        {
            if (data == null || data.Length < Marshal.SizeOf<MessageHeader>())
                return false;

            var header = BytesToStructure<MessageHeader>(data);

            if (header.PayloadSize > (uint)(data.Length - Marshal.SizeOf<MessageHeader>()))
                return false;

            MessageId = header.MessageId;
            MessageType = (MessageType)header.MessageType;
            Timestamp = header.Timestamp;
            Flags = (MessageFlags)header.Flags;

            Payload = new byte[header.PayloadSize];
            Buffer.BlockCopy(data, Marshal.SizeOf<MessageHeader>(), Payload, 0, (int)header.PayloadSize);

            return true;
        }

        public bool HasFlag(MessageFlags flag)
        {
            return (Flags & flag) != 0;
        }

        public void SetFlag(MessageFlags flag)
        {
            Flags |= flag;
        }

        public int TotalSize => Marshal.SizeOf<MessageHeader>() + (Payload?.Length ?? 0);

        private static byte[] StructureToBytes<T>(T structure) where T : struct
        {
            int size = Marshal.SizeOf<T>();
            byte[] arr = new byte[size];
            IntPtr ptr = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(structure, ptr, true);
            Marshal.Copy(ptr, arr, 0, size);
            Marshal.FreeHGlobal(ptr);
            return arr;
        }

        private static T BytesToStructure<T>(byte[] bytes) where T : struct
        {
            IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, ptr, bytes.Length);
            T result = Marshal.PtrToStructure<T>(ptr);
            Marshal.FreeHGlobal(ptr);
            return result;
        }
    }

    // P/Invoke声明
    public static class NativeMethods
    {
        private const string SharedMemoryLib = "SharedMemoryNative";

        // 环形缓冲区API
        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr RB_CreateProducer(string name, ulong bufferSize);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr RB_CreateConsumer(string name, ulong bufferSize);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RB_Close(IntPtr handle);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RB_Write(IntPtr handle, byte[] data, ulong size);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RB_Read(IntPtr handle, byte[] buffer, ulong bufferSize, out ulong bytesRead);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RB_GetFreeSpace(IntPtr handle);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RB_GetUsedSpace(IntPtr handle);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int RB_IsConnected(IntPtr handle);

        // 共享内存管理器API
        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr SMM_CreateProducer(string name, ulong bufferSize);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr SMM_CreateConsumer(string name, ulong bufferSize);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SMM_Destroy(IntPtr handle);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SMM_SendMessage(IntPtr handle, int messageType, byte[] data, ulong size);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SMM_ReceiveMessage(IntPtr handle, byte[] buffer, ulong bufferSize, out ulong bytesRead, int timeoutMs);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern int SMM_IsRunning(IntPtr handle);

        // 错误处理
        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RB_SetLastError(string error);

        [DllImport(SharedMemoryLib, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private static extern string RB_GetLastError();

        public static string GetLastError()
        {
            return RB_GetLastError();
        }
    }

    // 基础环形缓冲区类
    public class RingBuffer : IDisposable
    {
        protected IntPtr handle;
        protected bool disposed;
        protected readonly string name;
        protected readonly ulong bufferSize;

        protected RingBuffer(string name, ulong bufferSize)
        {
            this.name = name;
            this.bufferSize = bufferSize;
        }

        ~RingBuffer()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    NativeMethods.RB_Close(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        public virtual bool IsConnected => !disposed && handle != IntPtr.Zero && NativeMethods.RB_IsConnected(handle) != 0;
        public virtual ulong FreeSpace => IsConnected ? (ulong)NativeMethods.RB_GetFreeSpace(handle) : 0;
        public virtual ulong UsedSpace => IsConnected ? (ulong)NativeMethods.RB_GetUsedSpace(handle) : 0;
        public virtual ulong BufferSize => bufferSize;
        public virtual bool IsEmpty => UsedSpace == 0;
        public virtual bool IsFull => FreeSpace == 0;

        public string GetLastError()
        {
            return NativeMethods.GetLastError();
        }
    }

    // 共享内存生产者
    public class SharedMemoryProducer : RingBuffer
    {
        public SharedMemoryProducer(string name, ulong bufferSize = 1024 * 1024)
            : base(name, bufferSize)
        {
        }

        public bool Connect()
        {
            if (IsConnected) return true;

            handle = NativeMethods.RB_CreateProducer(name, bufferSize);
            return IsConnected;
        }

        public bool Send(byte[] data)
        {
            if (!IsConnected || data == null)
                return false;

            return NativeMethods.RB_Write(handle, data, (ulong)data.Length) != 0;
        }

        public bool SendString(string str)
        {
            if (string.IsNullOrEmpty(str))
                return false;

            byte[] data = Encoding.UTF8.GetBytes(str);
            return Send(data);
        }

        public bool SendMessage(SharedMemoryMessage message)
        {
            if (!IsConnected || message == null)
                return false;

            byte[] data = message.Serialize();
            return Send(data);
        }

        public bool SendMessage(MessageType type, byte[] data)
        {
            var message = new SharedMemoryMessage(type, data);
            return SendMessage(message);
        }

        public int SendBatch(IEnumerable<byte[]> dataBatch)
        {
            if (!IsConnected || dataBatch == null)
                return 0;

            int sentCount = 0;
            foreach (byte[] data in dataBatch)
            {
                if (Send(data))
                    sentCount++;
                else
                    break;
            }
            return sentCount;
        }
    }

    // 共享内存消费者
    public class SharedMemoryConsumer : RingBuffer
    {
        public SharedMemoryConsumer(string name, ulong bufferSize = 1024 * 1024)
            : base(name, bufferSize)
        {
        }

        public bool Connect()
        {
            if (IsConnected) return true;

            handle = NativeMethods.RB_CreateConsumer(name, bufferSize);
            return IsConnected;
        }

        public bool Receive(out byte[] data, int timeoutMs = -1)
        {
            data = null;

            if (!IsConnected)
                return false;

            // 首先检查是否有数据
            if (IsEmpty)
                return false;

            // 读取数据
            byte[] buffer = new byte[BufferSize];
            if (NativeMethods.RB_Read(handle, buffer, (ulong)buffer.Length, out ulong bytesRead) == 0)
                return false;

            if (bytesRead == 0)
                return false;

            data = new byte[bytesRead];
            Buffer.BlockCopy(buffer, 0, data, 0, (int)bytesRead);
            return true;
        }

        public bool ReceiveString(out string str, int timeoutMs = -1)
        {
            str = null;

            if (Receive(out byte[] data, timeoutMs))
            {
                str = Encoding.UTF8.GetString(data);
                return true;
            }

            return false;
        }

        public bool ReceiveMessage(out SharedMemoryMessage message, int timeoutMs = -1)
        {
            message = null;

            if (!Receive(out byte[] data, timeoutMs))
                return false;

            message = new SharedMemoryMessage();
            return message.Deserialize(data);
        }

        public bool Peek(out byte[] data)
        {
            data = null;

            if (!IsConnected || IsEmpty)
                return false;

            // 简化的peek实现 - 在实际应用中可能需要更复杂的逻辑
            return Receive(out data, 0);
        }

        public int ReceiveBatch(IList<byte[]> dataBatch, int maxCount, int timeoutMs = -1)
        {
            if (!IsConnected || dataBatch == null || maxCount <= 0)
                return 0;

            int receivedCount = 0;
            var startTime = DateTime.UtcNow;

            for (int i = 0; i < maxCount; i++)
            {
                // 计算剩余超时时间
                int remainingTimeout = timeoutMs;
                if (timeoutMs > 0)
                {
                    var elapsed = (int)(DateTime.UtcNow - startTime).TotalMilliseconds;
                    remainingTimeout = timeoutMs - elapsed;
                    if (remainingTimeout <= 0)
                        break;
                }

                if (Receive(out byte[] data, remainingTimeout))
                {
                    dataBatch.Add(data);
                    receivedCount++;
                }
                else
                {
                    break;
                }
            }

            return receivedCount;
        }
    }

    // 高级共享内存管理器
    public class SharedMemoryManager : IDisposable
    {
        private IntPtr handle;
        private bool disposed;
        private readonly string name;
        private readonly ulong bufferSize;
        private bool isProducer;

        public SharedMemoryManager(string name, ulong bufferSize = 1024 * 1024)
        {
            this.name = name;
            this.bufferSize = bufferSize;
        }

        ~SharedMemoryManager()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposed)
            {
                if (handle != IntPtr.Zero)
                {
                    NativeMethods.SMM_Destroy(handle);
                    handle = IntPtr.Zero;
                }
                disposed = true;
            }
        }

        public bool StartProducer()
        {
            if (IsRunning) return true;

            handle = NativeMethods.SMM_CreateProducer(name, bufferSize);
            isProducer = true;
            return IsRunning;
        }

        public bool StartConsumer()
        {
            if (IsRunning) return true;

            handle = NativeMethods.SMM_CreateConsumer(name, bufferSize);
            isProducer = false;
            return IsRunning;
        }

        public void Stop()
        {
            Dispose(true);
        }

        public bool IsRunning => !disposed && handle != IntPtr.Zero && NativeMethods.SMM_IsRunning(handle) != 0;
        public bool IsProducer => isProducer;
        public bool IsConsumer => !isProducer;

        public bool SendMessage(SharedMemoryMessage message)
        {
            if (!IsRunning || message == null)
                return false;

            byte[] data = message.Serialize();
            return NativeMethods.SMM_SendMessage(handle, (int)message.MessageType, data, (ulong)data.Length) != 0;
        }

        public bool SendMessage(MessageType type, byte[] data)
        {
            var message = new SharedMemoryMessage(type, data);
            return SendMessage(message);
        }

        public bool ReceiveMessage(out SharedMemoryMessage message, int timeoutMs = -1)
        {
            message = null;

            if (!IsRunning)
                return false;

            byte[] buffer = new byte[bufferSize];
            if (NativeMethods.SMM_ReceiveMessage(handle, buffer, (ulong)buffer.Length, out ulong bytesRead, timeoutMs) == 0)
                return false;

            if (bytesRead == 0)
                return false;

            byte[] messageData = new byte[bytesRead];
            Buffer.BlockCopy(buffer, 0, messageData, 0, (int)bytesRead);

            message = new SharedMemoryMessage();
            return message.Deserialize(messageData);
        }

        public bool SendHeartbeat()
        {
            return SendMessage(MessageType.Heartbeat, Array.Empty<byte>());
        }

        public string GetLastError()
        {
            return NativeMethods.GetLastError();
        }
    }

    // 工厂方法
    public static class SharedMemoryFactory
    {
        public static SharedMemoryProducer CreateProducer(string name, ulong bufferSize = 1024 * 1024)
        {
            var producer = new SharedMemoryProducer(name, bufferSize);
            return producer.Connect() ? producer : null;
        }

        public static SharedMemoryConsumer CreateConsumer(string name, ulong bufferSize = 1024 * 1024)
        {
            var consumer = new SharedMemoryConsumer(name, bufferSize);
            return consumer.Connect() ? consumer : null;
        }

        public static SharedMemoryManager CreateManager(string name, bool asProducer, ulong bufferSize = 1024 * 1024)
        {
            var manager = new SharedMemoryManager(name, bufferSize);
            bool success = asProducer ? manager.StartProducer() : manager.StartConsumer();
            return success ? manager : null;
        }
    }

    // 泛型封装
    public class TypedSharedMemoryProducer<T> : IDisposable where T : struct
    {
        private readonly SharedMemoryProducer producer;

        public TypedSharedMemoryProducer(string name, ulong bufferSize = 1024 * 1024)
        {
            producer = new SharedMemoryProducer(name, bufferSize);
        }

        public bool Connect()
        {
            return producer.Connect();
        }

        public bool Send(T data)
        {
            byte[] bytes = StructureToBytes(data);
            return producer.Send(bytes);
        }

        public bool SendList(IEnumerable<T> items)
        {
            var list = items.ToList();
            byte[] bytes = new byte[list.Count * Marshal.SizeOf<T>()];
            int offset = 0;
            foreach (T item in list)
            {
                byte[] itemBytes = StructureToBytes(item);
                Buffer.BlockCopy(itemBytes, 0, bytes, offset, itemBytes.Length);
                offset += itemBytes.Length;
            }
            return producer.Send(bytes);
        }

        public void Dispose()
        {
            producer.Dispose();
        }

        private static byte[] StructureToBytes(T structure)
        {
            int size = Marshal.SizeOf<T>();
            byte[] arr = new byte[size];
            IntPtr ptr = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(structure, ptr, true);
            Marshal.Copy(ptr, arr, 0, size);
            Marshal.FreeHGlobal(ptr);
            return arr;
        }
    }

    public class TypedSharedMemoryConsumer<T> : IDisposable where T : struct
    {
        private readonly SharedMemoryConsumer consumer;

        public TypedSharedMemoryConsumer(string name, ulong bufferSize = 1024 * 1024)
        {
            consumer = new SharedMemoryConsumer(name, bufferSize);
        }

        public bool Connect()
        {
            return consumer.Connect();
        }

        public bool Receive(out T data)
        {
            data = default(T);

            if (!consumer.Receive(out byte[] bytes))
                return false;

            if (bytes.Length != Marshal.SizeOf<T>())
                return false;

            data = BytesToStructure<T>(bytes);
            return true;
        }

        public bool ReceiveList(out List<T> items)
        {
            items = new List<T>();

            if (!consumer.Receive(out byte[] bytes))
                return false;

            int itemSize = Marshal.SizeOf<T>();
            if (bytes.Length % itemSize != 0)
                return false;

            int count = bytes.Length / itemSize;
            for (int i = 0; i < count; i++)
            {
                byte[] itemBytes = new byte[itemSize];
                Buffer.BlockCopy(bytes, i * itemSize, itemBytes, 0, itemSize);
                items.Add(BytesToStructure<T>(itemBytes));
            }

            return true;
        }

        public void Dispose()
        {
            consumer.Dispose();
        }

        private static T BytesToStructure(byte[] bytes)
        {
            IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, ptr, bytes.Length);
            T result = Marshal.PtrToStructure<T>(ptr);
            Marshal.FreeHGlobal(ptr);
            return result;
        }
    }
}