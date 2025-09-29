"""
BitRPC Shared Memory Python API
跨进程共享内存通信的Python封装
"""

import ctypes
import threading
import time
from typing import Optional, List, Tuple, Callable, Any, Union
from enum import IntEnum
from dataclasses import dataclass
import struct

# 加载共享库
try:
    # Windows
    _lib = ctypes.CDLL("./SharedMemoryNative.dll")
except:
    try:
        # Linux/Mac
        _lib = ctypes.CDLL("./libSharedMemoryNative.so")
    except:
        raise RuntimeError("Cannot load shared memory native library")

# 定义消息类型
class MessageType(IntEnum):
    DATA = 1
    CONTROL = 2
    HEARTBEAT = 3
    ERROR = 4
    CUSTOM_MIN = 1000

# 定义消息标志
class MessageFlags(IntEnum):
    NONE = 0
    URGENT = 0x01
    COMPRESSED = 0x02
    ENCRYPTED = 0x04
    LAST_FRAGMENT = 0x08

# 消息头结构
@dataclass
class MessageHeader:
    message_id: int = 0
    message_type: int = 0
    payload_size: int = 0
    timestamp: int = 0
    flags: int = 0

    def pack(self) -> bytes:
        """打包消息头为字节"""
        return struct.pack('<IIIIBxxx',
                          self.message_id,
                          self.message_type,
                          self.payload_size,
                          self.timestamp,
                          self.flags)

    @classmethod
    def unpack(cls, data: bytes) -> 'MessageHeader':
        """从字节解包消息头"""
        if len(data) < 16:  # 消息头固定16字节
            raise ValueError("Invalid message header data")

        message_id, message_type, payload_size, timestamp, flags = struct.unpack('<IIIIBxxx', data[:16])
        return cls(message_id, message_type, payload_size, timestamp, flags)

# 共享内存消息类
class SharedMemoryMessage:
    _next_id = 1
    _id_lock = threading.Lock()

    def __init__(self, message_type: MessageType = MessageType.DATA, payload: bytes = b''):
        with self._id_lock:
            self.message_id = SharedMemoryMessage._next_id
            SharedMemoryMessage._next_id += 1

        self.message_type = message_type
        self.payload = payload or b''
        self.timestamp = int(time.time() * 1000)  # 毫秒时间戳
        self.flags = MessageFlags.NONE

    def serialize(self) -> bytes:
        """序列化消息为字节"""
        header = MessageHeader(
            message_id=self.message_id,
            message_type=self.message_type,
            payload_size=len(self.payload),
            timestamp=self.timestamp,
            flags=self.flags
        )
        return header.pack() + self.payload

    @classmethod
    def deserialize(cls, data: bytes) -> 'SharedMemoryMessage':
        """从字节反序列化消息"""
        if len(data) < 16:
            raise ValueError("Invalid message data")

        header = MessageHeader.unpack(data)

        if header.payload_size > len(data) - 16:
            raise ValueError("Payload size mismatch")

        payload = data[16:16+header.payload_size]

        message = cls()
        message.message_id = header.message_id
        message.message_type = header.message_type
        message.payload = payload
        message.timestamp = header.timestamp
        message.flags = header.flags

        return message

    def has_flag(self, flag: MessageFlags) -> bool:
        """检查是否有指定标志"""
        return (self.flags & flag) != 0

    def set_flag(self, flag: MessageFlags):
        """设置标志"""
        self.flags |= flag

    @property
    def total_size(self) -> int:
        """消息总大小"""
        return 16 + len(self.payload)

# 定义C函数接口
class NativeMethods:
    # 环形缓冲区API
    _lib.RB_CreateProducer.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    _lib.RB_CreateProducer.restype = ctypes.c_void_p

    _lib.RB_CreateConsumer.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    _lib.RB_CreateConsumer.restype = ctypes.c_void_p

    _lib.RB_Close.argtypes = [ctypes.c_void_p]
    _lib.RB_Close.restype = None

    _lib.RB_Write.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
    _lib.RB_Write.restype = ctypes.c_int

    _lib.RB_Read.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    _lib.RB_Read.restype = ctypes.c_int

    _lib.RB_GetFreeSpace.argtypes = [ctypes.c_void_p]
    _lib.RB_GetFreeSpace.restype = ctypes.c_int

    _lib.RB_GetUsedSpace.argtypes = [ctypes.c_void_p]
    _lib.RB_GetUsedSpace.restype = ctypes.c_int

    _lib.RB_IsConnected.argtypes = [ctypes.c_void_p]
    _lib.RB_IsConnected.restype = ctypes.c_int

    # 共享内存管理器API
    _lib.SMM_CreateProducer.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    _lib.SMM_CreateProducer.restype = ctypes.c_void_p

    _lib.SMM_CreateConsumer.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    _lib.SMM_CreateConsumer.restype = ctypes.c_void_p

    _lib.SMM_Destroy.argtypes = [ctypes.c_void_p]
    _lib.SMM_Destroy.restype = None

    _lib.SMM_SendMessage.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t]
    _lib.SMM_SendMessage.restype = ctypes.c_int

    _lib.SMM_ReceiveMessage.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t), ctypes.c_int]
    _lib.SMM_ReceiveMessage.restype = ctypes.c_int

    _lib.SMM_IsRunning.argtypes = [ctypes.c_void_p]
    _lib.SMM_IsRunning.restype = ctypes.c_int

    # 错误处理
    _lib.RB_SetLastError.argtypes = [ctypes.c_char_p]
    _lib.RB_SetLastError.restype = None

    _lib.RB_GetLastError.argtypes = []
    _lib.RB_GetLastError.restype = ctypes.c_char_p

    @staticmethod
    def get_last_error() -> str:
        """获取最后错误信息"""
        error_ptr = _lib.RB_GetLastError()
        if error_ptr:
            return error_ptr.decode('utf-8')
        return ""

# 基础环形缓冲区类
class RingBuffer:
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        self.name = name.encode('utf-8')
        self.buffer_size = buffer_size
        self._handle = None
        self._disposed = False

    def __del__(self):
        self.dispose()

    def dispose(self):
        """释放资源"""
        if not self._disposed and self._handle:
            NativeMethods._lib.RB_Close(self._handle)
            self._handle = None
            self._disposed = True

    @property
    def is_connected(self) -> bool:
        """是否已连接"""
        if self._disposed or not self._handle:
            return False
        return NativeMethods._lib.RB_IsConnected(self._handle) != 0

    @property
    def free_space(self) -> int:
        """可用空间"""
        if not self.is_connected:
            return 0
        return NativeMethods._lib.RB_GetFreeSpace(self._handle)

    @property
    def used_space(self) -> int:
        """已用空间"""
        if not self.is_connected:
            return 0
        return NativeMethods._lib.RB_GetUsedSpace(self._handle)

    @property
    def buffer_size_total(self) -> int:
        """总缓冲区大小"""
        return self.buffer_size

    @property
    def is_empty(self) -> bool:
        """是否为空"""
        return self.used_space == 0

    @property
    def is_full(self) -> bool:
        """是否已满"""
        return self.free_space == 0

    def get_last_error(self) -> str:
        """获取最后错误"""
        return NativeMethods.get_last_error()

# 共享内存生产者
class SharedMemoryProducer(RingBuffer):
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        super().__init__(name, buffer_size)

    def connect(self) -> bool:
        """连接到共享内存"""
        if self.is_connected:
            return True

        self._handle = NativeMethods._lib.RB_CreateProducer(self.name, self.buffer_size)
        return self.is_connected

    def send(self, data: bytes) -> bool:
        """发送数据"""
        if not self.is_connected or not data:
            return False

        data_ptr = ctypes.cast(data, ctypes.c_void_p)
        result = NativeMethods._lib.RB_Write(self._handle, data_ptr, len(data))
        return result != 0

    def send_string(self, text: str) -> bool:
        """发送字符串"""
        if not text:
            return False
        return self.send(text.encode('utf-8'))

    def send_message(self, message: SharedMemoryMessage) -> bool:
        """发送消息"""
        if not self.is_connected or not message:
            return False
        data = message.serialize()
        return self.send(data)

    def send_message_data(self, message_type: MessageType, data: bytes) -> bool:
        """发送消息数据"""
        message = SharedMemoryMessage(message_type, data)
        return self.send_message(message)

    def send_batch(self, data_list: List[bytes]) -> int:
        """批量发送数据"""
        if not self.is_connected or not data_list:
            return 0

        sent_count = 0
        for data in data_list:
            if self.send(data):
                sent_count += 1
            else:
                break
        return sent_count

# 共享内存消费者
class SharedMemoryConsumer(RingBuffer):
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        super().__init__(name, buffer_size)

    def connect(self) -> bool:
        """连接到共享内存"""
        if self.is_connected:
            return True

        self._handle = NativeMethods._lib.RB_CreateConsumer(self.name, self.buffer_size)
        return self.is_connected

    def receive(self, timeout_ms: int = -1) -> Optional[bytes]:
        """接收数据"""
        if not self.is_connected:
            return None

        if self.is_empty:
            return None

        # 创建缓冲区
        buffer_size = self.buffer_size_total
        buffer = ctypes.create_string_buffer(buffer_size)
        bytes_read = ctypes.c_size_t()

        result = NativeMethods._lib.RB_Read(
            self._handle,
            buffer,
            buffer_size,
            ctypes.byref(bytes_read)
        )

        if result == 0 or bytes_read.value == 0:
            return None

        return buffer.raw[:bytes_read.value]

    def receive_string(self, timeout_ms: int = -1) -> Optional[str]:
        """接收字符串"""
        data = self.receive(timeout_ms)
        if data:
            return data.decode('utf-8')
        return None

    def receive_message(self, timeout_ms: int = -1) -> Optional[SharedMemoryMessage]:
        """接收消息"""
        data = self.receive(timeout_ms)
        if data:
            try:
                return SharedMemoryMessage.deserialize(data)
            except ValueError:
                return None
        return None

    def peek(self) -> Optional[bytes]:
        """查看数据但不移动指针"""
        # 简化的实现，实际可能需要更复杂的逻辑
        return self.receive(0)

    def receive_batch(self, max_count: int, timeout_ms: int = -1) -> List[bytes]:
        """批量接收数据"""
        if not self.is_connected or max_count <= 0:
            return []

        result = []
        start_time = time.time()

        for i in range(max_count):
            # 计算剩余超时时间
            remaining_timeout = timeout_ms
            if timeout_ms > 0:
                elapsed = int((time.time() - start_time) * 1000)
                remaining_timeout = timeout_ms - elapsed
                if remaining_timeout <= 0:
                    break

            data = self.receive(remaining_timeout)
            if data:
                result.append(data)
            else:
                break

        return result

# 高级共享内存管理器
class SharedMemoryManager:
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        self.name = name.encode('utf-8')
        self.buffer_size = buffer_size
        self._handle = None
        self._disposed = False
        self._is_producer = False

    def __del__(self):
        self.dispose()

    def dispose(self):
        """释放资源"""
        if not self._disposed and self._handle:
            NativeMethods._lib.SMM_Destroy(self._handle)
            self._handle = None
            self._disposed = True

    def start_producer(self) -> bool:
        """启动生产者"""
        if self.is_running:
            return True

        self._handle = NativeMethods._lib.SMM_CreateProducer(self.name, self.buffer_size)
        self._is_producer = True
        return self.is_running

    def start_consumer(self) -> bool:
        """启动消费者"""
        if self.is_running:
            return True

        self._handle = NativeMethods._lib.SMM_CreateConsumer(self.name, self.buffer_size)
        self._is_producer = False
        return self.is_running

    def stop(self):
        """停止"""
        self.dispose()

    @property
    def is_running(self) -> bool:
        """是否在运行"""
        if self._disposed or not self._handle:
            return False
        return NativeMethods._lib.SMM_IsRunning(self._handle) != 0

    @property
    def is_producer(self) -> bool:
        """是否为生产者"""
        return self._is_producer

    @property
    def is_consumer(self) -> bool:
        """是否为消费者"""
        return not self._is_producer

    def send_message(self, message: SharedMemoryMessage) -> bool:
        """发送消息"""
        if not self.is_running or not message:
            return False

        data = message.serialize()
        data_ptr = ctypes.cast(data, ctypes.c_void_p)
        result = NativeMethods._lib.SMM_SendMessage(
            self._handle,
            message.message_type,
            data_ptr,
            len(data)
        )
        return result != 0

    def send_message_data(self, message_type: MessageType, data: bytes) -> bool:
        """发送消息数据"""
        message = SharedMemoryMessage(message_type, data)
        return self.send_message(message)

    def receive_message(self, timeout_ms: int = -1) -> Optional[SharedMemoryMessage]:
        """接收消息"""
        if not self.is_running:
            return None

        # 创建缓冲区
        buffer_size = self.buffer_size
        buffer = ctypes.create_string_buffer(buffer_size)
        bytes_read = ctypes.c_size_t()

        result = NativeMethods._lib.SMM_ReceiveMessage(
            self._handle,
            buffer,
            buffer_size,
            ctypes.byref(bytes_read),
            timeout_ms
        )

        if result == 0 or bytes_read.value == 0:
            return None

        message_data = buffer.raw[:bytes_read.value]
        try:
            return SharedMemoryMessage.deserialize(message_data)
        except ValueError:
            return None

    def send_heartbeat(self) -> bool:
        """发送心跳"""
        return self.send_message_data(MessageType.HEARTBEAT, b'')

    def get_last_error(self) -> str:
        """获取最后错误"""
        return NativeMethods.get_last_error()

# 工厂方法
class SharedMemoryFactory:
    @staticmethod
    def create_producer(name: str, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryProducer]:
        """创建生产者"""
        producer = SharedMemoryProducer(name, buffer_size)
        if producer.connect():
            return producer
        return None

    @staticmethod
    def create_consumer(name: str, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryConsumer]:
        """创建消费者"""
        consumer = SharedMemoryConsumer(name, buffer_size)
        if consumer.connect():
            return consumer
        return None

    @staticmethod
    def create_manager(name: str, as_producer: bool = True, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryManager]:
        """创建管理器"""
        manager = SharedMemoryManager(name, buffer_size)
        if as_producer:
            if manager.start_producer():
                return manager
        else:
            if manager.start_consumer():
                return manager
        return None

# 泛型封装
class TypedSharedMemoryProducer:
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        self._producer = SharedMemoryProducer(name, buffer_size)

    def connect(self) -> bool:
        """连接"""
        return self._producer.connect()

    def send_typed(self, data: Any) -> bool:
        """发送类型化数据"""
        try:
            # 尝试将数据转换为字节
            if isinstance(data, (int, float, str)):
                data_bytes = str(data).encode('utf-8')
            elif isinstance(data, bytes):
                data_bytes = data
            else:
                # 对于复杂对象，使用pickle序列化
                import pickle
                data_bytes = pickle.dumps(data)

            return self._producer.send(data_bytes)
        except Exception as e:
            print(f"Error sending typed data: {e}")
            return False

    def send_list(self, data_list: List[Any]) -> int:
        """发送列表数据"""
        sent_count = 0
        for data in data_list:
            if self.send_typed(data):
                sent_count += 1
            else:
                break
        return sent_count

class TypedSharedMemoryConsumer:
    def __init__(self, name: str, buffer_size: int = 1024 * 1024):
        self._consumer = SharedMemoryConsumer(name, buffer_size)

    def connect(self) -> bool:
        """连接"""
        return self._consumer.connect()

    def receive_typed(self, timeout_ms: int = -1) -> Optional[Any]:
        """接收类型化数据"""
        data = self._consumer.receive(timeout_ms)
        if data:
            try:
                # 尝试解析为基本类型
                return data.decode('utf-8')
            except:
                # 如果不是字符串，尝试pickle反序列化
                try:
                    import pickle
                    return pickle.loads(data)
                except:
                    return data
        return None

    def receive_list(self, max_count: int, timeout_ms: int = -1) -> List[Any]:
        """接收列表数据"""
        raw_list = self._consumer.receive_batch(max_count, timeout_ms)
        result = []
        for data in raw_list:
            try:
                result.append(data.decode('utf-8'))
            except:
                try:
                    import pickle
                    result.append(pickle.loads(data))
                except:
                    result.append(data)
        return result

# 便利函数
def create_producer(name: str, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryProducer]:
    """创建生产者的便利函数"""
    return SharedMemoryFactory.create_producer(name, buffer_size)

def create_consumer(name: str, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryConsumer]:
    """创建消费者的便利函数"""
    return SharedMemoryFactory.create_consumer(name, buffer_size)

def create_manager(name: str, as_producer: bool = True, buffer_size: int = 1024 * 1024) -> Optional[SharedMemoryManager]:
    """创建管理器的便利函数"""
    return SharedMemoryFactory.create_manager(name, as_producer, buffer_size)

def cleanup_all():
    """清理所有共享内存资源"""
    # 这里可以添加全局清理逻辑
    pass

def get_active_instances() -> List[str]:
    """获取活跃实例列表"""
    # 这里可以添加获取实例列表的逻辑
    return []

# 示例用法
if __name__ == "__main__":
    print("BitRPC Shared Memory Python API")
    print("=" * 40)

    # 测试基本功能
    try:
        # 创建生产者和消费者
        producer = create_producer("test_buffer", 65536)
        consumer = create_consumer("test_buffer", 65536)

        if producer and consumer:
            print("✓ 创建生产者和消费者成功")

            # 发送测试数据
            test_data = b"Hello, Shared Memory!"
            if producer.send(test_data):
                print("✓ 发送数据成功")

                # 接收数据
                received = consumer.receive()
                if received == test_data:
                    print("✓ 接收数据成功")
                    print(f"  接收到的数据: {received.decode('utf-8')}")
                else:
                    print("✗ 接收数据失败")
            else:
                print("✗ 发送数据失败")
        else:
            print("✗ 创建生产者和消费者失败")

    except Exception as e:
        print(f"测试失败: {e}")
    finally:
        print("测试完成")