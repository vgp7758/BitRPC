using System;
using System.Collections.Generic;
using System.IO;
using System.Collections.Concurrent; // 新增

namespace BitRPC.Serialization
{
    public interface ITypeHandler
    {
        int HashCode { get; }
        void Write(object obj, StreamWriter writer);
        object Read(StreamReader reader);
    }

    public interface IBufferSerializer
    {
        void RegisterHandler<T>(ITypeHandler handler);
        ITypeHandler GetHandler(Type type);
        void InitHandlers();
    }

    public class BitMask
    {
        private uint[] _masks;
        private int _size;

        public BitMask() : this(1)
        {
        }

        public BitMask(int size)
        {
            _size = size;
            _masks = new uint[size];
        }

        /// <summary>
        /// 获取BitMask的大小
        /// </summary>
        public int Size => _size;

        public void SetBit(int index, bool value)
        {
            int maskIndex = index / 32;
            int bitIndex = index % 32;

            if (maskIndex >= _size)
            {
                Array.Resize(ref _masks, maskIndex + 1);
                _size = maskIndex + 1;
            }

            if (value)
            {
                _masks[maskIndex] |= (1u << bitIndex);
            }
            else
            {
                _masks[maskIndex] &= ~(1u << bitIndex);
            }
        }

        public bool GetBit(int index)
        {
            int maskIndex = index / 32;
            int bitIndex = index % 32;

            if (maskIndex >= _size)
            {
                return false;
            }

            return (_masks[maskIndex] & (1u << bitIndex)) != 0;
        }

        public void Write(StreamWriter writer)
        {
            writer.WriteInt32(_size);
            for (int i = 0; i < _size; i++)
            {
                writer.WriteUInt32(_masks[i]);
            }
        }

        public void Read(StreamReader reader)
        {
            for (int i = 0; i < _size; i++)
            {
                _masks[i] = reader.ReadUInt32();
            }
        }

        /// <summary>
        /// 清空BitMask，重置所有位为0
        /// </summary>
        public void Clear()
        {
            for (int i = 0; i < _size; i++)
            {
                _masks[i] = 0;
            }
        }
    }

    public partial class StreamWriter
    {
        private readonly MemoryStream _stream;
        private readonly BinaryWriter _writer;

        public StreamWriter()
        {
            _stream = new MemoryStream();
            _writer = new BinaryWriter(_stream);
        }

        public void WriteInt32(int value)
        {
            _writer.Write(value);
        }

        public void WriteInt64(long value)
        {
            _writer.Write(value);
        }

        public void WriteUInt32(uint value)
        {
            _writer.Write(value);
        }

        public void WriteFloat(float value)
        {
            _writer.Write(value);
        }

        public void WriteDouble(double value)
        {
            _writer.Write(value);
        }

        public void WriteBool(bool value)
        {
            _writer.Write(value);
        }

        public void WriteString(string value)
        {
            if (string.IsNullOrEmpty(value))
            {
                WriteInt32(-1);
            }
            else
            {
                var bytes = System.Text.Encoding.UTF8.GetBytes(value);
                WriteInt32(bytes.Length);
                _writer.Write(bytes);
            }
        }

        public void WriteBytes(byte[] bytes)
        {
            WriteInt32(bytes.Length);
            _writer.Write(bytes);
        }

        public void WriteList<T>(List<T> list, Action<T> writeAction)
        {
            WriteInt32(list.Count);
            foreach (var item in list)
            {
                writeAction(item);
            }
        }

        public void WriteList<T>(List<T> list, Action<T, StreamWriter> writeAction)
        {
            WriteInt32(list.Count);
            foreach (var item in list)
            {
                writeAction(item, this);
            }
        }

        public void WriteObject(object obj)
        {
            if (obj == null)
            {
                WriteInt32(-1);
                return;
            }

            var serializer = BufferSerializer.Instance;
            var handler = serializer.GetHandler(obj.GetType());
            if (handler != null)
            {
                WriteInt32(handler.HashCode);
                handler.Write(obj, this);
            }
            else
            {
                WriteInt32(-1);
            }
        }

        public byte[] ToArray()
        {
            _writer.Flush();
            return _stream.ToArray();
        }
    }

    public partial class StreamReader
    {
        private readonly MemoryStream _stream;
        private readonly BinaryReader _reader;

        public StreamReader(byte[] data)
        {
            _stream = new MemoryStream(data);
            _reader = new BinaryReader(_stream);
        }

        public int ReadInt32()
        {
            return _reader.ReadInt32();
        }

        public long ReadInt64()
        {
            return _reader.ReadInt64();
        }

        public uint ReadUInt32()
        {
            return _reader.ReadUInt32();
        }

        public float ReadFloat()
        {
            return _reader.ReadSingle();
        }

        public double ReadDouble()
        {
            return _reader.ReadDouble();
        }

        public bool ReadBool()
        {
            return _reader.ReadBoolean();
        }

        public string ReadString()
        {
            var length = ReadInt32();
            if (length == -1)
            {
                return null;
            }
            var bytes = _reader.ReadBytes(length);
            return System.Text.Encoding.UTF8.GetString(bytes);
        }

        public byte[] ReadBytes()
        {
            var length = ReadInt32();
            return _reader.ReadBytes(length);
        }

        public List<T> ReadList<T>(Func<T> readFunc)
        {
            var count = ReadInt32();
            var list = new List<T>(count);
            for (int i = 0; i < count; i++)
            {
                list.Add(readFunc());
            }
            return list;
        }

        public List<T> ReadList<T>(Func<StreamReader, T> readFunc)
        {
            var count = ReadInt32();
            var list = new List<T>(count);
            for (int i = 0; i < count; i++)
            {
                list.Add(readFunc(this));
            }
            return list;
        }

        public object ReadObject()
        {
            var hashCode = ReadInt32();
            if (hashCode == -1)
            {
                return null;
            }

            var serializer = BufferSerializer.Instance;
            var handler = serializer.GetHandlerByHashCode(hashCode);
            if (handler != null)
            {
                return handler.Read(this);
            }
            return null;
        }
    }

    public class BufferSerializer : IBufferSerializer
    {
        public static readonly BufferSerializer Instance = new BufferSerializer();

        private readonly Dictionary<Type, ITypeHandler> _handlers;
        private readonly Dictionary<int, ITypeHandler> _handlersByHashCode;

        private BufferSerializer()
        {
            _handlers = new Dictionary<Type, ITypeHandler>();
            _handlersByHashCode = new Dictionary<int, ITypeHandler>();
            InitHandlers();
        }

        public void RegisterHandler<T>(ITypeHandler handler)
        {
            var type = typeof(T);
            _handlers[type] = handler;
            _handlersByHashCode[handler.HashCode] = handler;
        }

        public ITypeHandler GetHandler(Type type)
        {
            if (_handlers.TryGetValue(type, out var handler))
            {
                return handler;
            }
            return null;
        }

        public ITypeHandler GetHandlerByHashCode(int hashCode)
        {
            if (_handlersByHashCode.TryGetValue(hashCode, out var handler))
            {
                return handler;
            }
            return null;
        }

        public void InitHandlers()
        {
            RegisterHandler<int>(new Int32Handler());
            RegisterHandler<long>(new Int64Handler());
            RegisterHandler<float>(new FloatHandler());
            RegisterHandler<double>(new DoubleHandler());
            RegisterHandler<bool>(new BoolHandler());
            RegisterHandler<string>(new StringHandler());
            RegisterHandler<byte[]>(new BytesHandler());
        }

        public byte[] Serialize<T>(T obj)
        {
            var writer = new StreamWriter();
            var handler = GetHandler(typeof(T));
            if (handler != null)
            {
                handler.Write(obj, writer);
            }
            return writer.ToArray();
        }

        public T Deserialize<T>(byte[] data)
        {
            var reader = new StreamReader(data);
            var handler = GetHandler(typeof(T));
            if (handler != null)
            {
                return (T)handler.Read(reader);
            }
            return default(T);
        }
    }

    public static partial class Types
    {
        public static bool IsDefault(int value) => value == 0;
        public static bool IsDefault(long value) => value == 0L;
        public static bool IsDefault(float value) => value == 0f;
        public static bool IsDefault(double value) => value == 0.0;
        public static bool IsDefault(bool value) => value == false;
        public static bool IsDefault(string value) => string.IsNullOrEmpty(value);
        public static bool IsDefault(byte[] value) => value == null || value.Length == 0;
        public static bool IsDefault(DateTime value) => value == default;
        public static bool IsDefault<T>(List<T> value) => value == null || value.Count == 0;
        public static bool IsDefault(object value) => value == null;
    }

    #region Primitive Type Handlers
    public class Int32Handler : ITypeHandler
    {
        public int HashCode => 101;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteInt32((int)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadInt32();
        }

        public static int ReadStatic(StreamReader reader)
        {
            return reader.ReadInt32();
        }

        public static void WriteStatic(int value, StreamWriter writer)
        {
            writer.WriteInt32(value);
        }
    }

    public class Int64Handler : ITypeHandler
    {
        public int HashCode => 102;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteInt64((long)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadInt64();
        }

        public static long ReadStatic(StreamReader reader)
        {
            return reader.ReadInt64();
        }

        public static void WriteStatic(long value, StreamWriter writer)
        {
            writer.WriteInt64(value);
        }
    }

    public class FloatHandler : ITypeHandler
    {
        public int HashCode => 103;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteFloat((float)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadFloat();
        }

        public static float ReadStatic(StreamReader reader)
        {
            return reader.ReadFloat();
        }

        public static void WriteStatic(float value, StreamWriter writer)
        {
            writer.WriteFloat(value);
        }
    }

    public class DoubleHandler : ITypeHandler
    {
        public int HashCode => 104;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteDouble((double)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadDouble();
        }

        public static double ReadStatic(StreamReader reader)
        {
            return reader.ReadDouble();
        }

        public static void WriteStatic(double value, StreamWriter writer)
        {
            writer.WriteDouble(value);
        }
    }

    public class BoolHandler : ITypeHandler
    {
        public int HashCode => 105;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteBool((bool)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadBool();
        }

        public static bool ReadStatic(StreamReader reader)
        {
            return reader.ReadBool();
        }

        public static void WriteStatic(bool value, StreamWriter writer)
        {
            writer.WriteBool(value);
        }
    }

    public class StringHandler : ITypeHandler
    {
        public int HashCode => 106;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteString((string)obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadString();
        }

        public static string ReadStatic(StreamReader reader)
        {
            return reader.ReadString();
        }

        public static void WriteStatic(string value, StreamWriter writer)
        {
            writer.WriteString(value);
        }
    }

    public class BytesHandler : ITypeHandler
    {
        public int HashCode => 107;

        public void Write(object obj, StreamWriter writer)
        {
            writer.WriteBytes((byte[])obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadBytes();
        }

        public static byte[] ReadStatic(StreamReader reader)
        {
            return reader.ReadBytes();
        }

        public static void WriteStatic(byte[] value, StreamWriter writer)
        {
            writer.WriteBytes(value);
        }
    }

    public class AutoHandler : ITypeHandler
    {
        public int HashCode => 150;

        public void Write(object obj,StreamWriter writer)
        {
            writer.WriteObject(obj);
        }

        public object Read(StreamReader reader)
        {
            return reader.ReadObject();
        }

        public static object ReadStatic(StreamReader reader)
        {
            return reader.ReadObject();
        }

        public static void WriteStatic(object obj, StreamWriter writer)
        {
            writer.WriteObject(obj);
        }
    }

    #endregion

    public static class BitMaskPool
    {
        private static readonly ConcurrentBag<BitMask> _pool = new ConcurrentBag<BitMask>();

        public static BitMask Get(int size)
        {
            if (_pool.TryTake(out var mask))
            {
                // 如果池中对象容量不足则重新创建
                if (mask.Size < size)
                {
                    mask = new BitMask(size);
                }
                else
                {
                    mask.Clear();
                }
                return mask;
            }
            return new BitMask(size);
        }

        public static void Return(BitMask mask)
        {
            if (mask != null)
            {
                _pool.Add(mask);
            }
        }
    }
}