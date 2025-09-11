"""
BitRPC Python Serialization Core Library
Provides core serialization functionality for cross-language RPC communication
"""

import struct
from typing import Any, Dict, Type, Optional, List, Callable
from abc import ABC, abstractmethod


class BitMask:
    """Bit mask for efficient field presence detection"""
    
    def __init__(self, size: int = 1):
        self._size = size
        self._masks = [0] * size
    
    def set_bit(self, index: int, value: bool) -> None:
        """Set a bit at the given index"""
        mask_index = index // 32
        bit_index = index % 32
        
        if mask_index >= self._size:
            # Extend the mask array
            self._masks.extend([0] * (mask_index - self._size + 1))
            self._size = mask_index + 1
        
        if value:
            self._masks[mask_index] |= (1 << bit_index)
    
    def get_bit(self, index: int) -> bool:
        """Get a bit at the given index"""
        mask_index = index // 32
        bit_index = index % 32
        
        if mask_index >= self._size:
            return False
        
        return (self._masks[mask_index] & (1 << bit_index)) != 0
    
    def write(self, writer: 'StreamWriter') -> None:
        """Write bit mask to writer"""
        # Write only the first mask (uint32) to match C# format
        if self._size > 0:
            writer.write_uint32(self._masks[0])
        else:
            writer.write_uint32(0)
    
    def read(self, reader: 'StreamReader') -> None:
        """Read bit mask from reader"""
        # Read only the first mask (uint32) to match C# format
        self._size = 1
        self._masks = [reader.read_uint32()]


class StreamWriter:
    """Binary stream writer for BitRPC serialization"""
    
    def __init__(self):
        self._buffer = bytearray()
    
    def write_int32(self, value: int) -> None:
        """Write 32-bit integer"""
        self._buffer.extend(struct.pack('<i', value))
    
    def write_int64(self, value: int) -> None:
        """Write 64-bit integer"""
        self._buffer.extend(struct.pack('<q', value))
    
    def write_uint32(self, value: int) -> None:
        """Write 32-bit unsigned integer"""
        self._buffer.extend(struct.pack('<I', value))
    
    def write_float(self, value: float) -> None:
        """Write 32-bit float"""
        self._buffer.extend(struct.pack('<f', value))
    
    def write_double(self, value: float) -> None:
        """Write 64-bit double"""
        self._buffer.extend(struct.pack('<d', value))
    
    def write_bool(self, value: bool) -> None:
        """Write boolean"""
        self.write_int32(1 if value else 0)
    
    def write_string(self, value: str) -> None:
        """Write string"""
        if value is None:
            self.write_int32(-1)
        else:
            encoded = value.encode('utf-8')
            self.write_int32(len(encoded))
            self._buffer.extend(encoded)
    
    def write_bytes(self, value: bytes) -> None:
        """Write bytes"""
        if value is None:
            self.write_int32(-1)
        else:
            self.write_int32(len(value))
            self._buffer.extend(value)
    
    def write_list(self, items: List[Any], write_func: Callable[[Any], None]) -> None:
        """Write list of items"""
        self.write_int32(len(items))
        for item in items:
            write_func(item)
    
    def write_object(self, obj: Any) -> None:
        """Write object using registered serializer"""
        if obj is None:
            self.write_int32(-1)
            return
        
        serializer = BufferSerializer.instance()
        handler = serializer.get_handler(type(obj))
        if handler is not None:
            self.write_int32(handler.hash_code)
            handler.write(obj, self)
        else:
            self.write_int32(-1)
    
    def to_array(self) -> bytes:
        """Get the written data as bytes"""
        return bytes(self._buffer)


class StreamReader:
    """Binary stream reader for BitRPC serialization"""
    
    def __init__(self, data: bytes):
        self._data = data
        self._position = 0
    
    def read_int32(self) -> int:
        """Read 32-bit integer"""
        if self._position + 4 > len(self._data):
            raise ValueError("Insufficient data for int32")
        value = struct.unpack('<i', self._data[self._position:self._position + 4])[0]
        self._position += 4
        return value
    
    def read_int64(self) -> int:
        """Read 64-bit integer"""
        if self._position + 8 > len(self._data):
            raise ValueError("Insufficient data for int64")
        value = struct.unpack('<q', self._data[self._position:self._position + 8])[0]
        self._position += 8
        return value
    
    def read_uint32(self) -> int:
        """Read 32-bit unsigned integer"""
        if self._position + 4 > len(self._data):
            raise ValueError("Insufficient data for uint32")
        value = struct.unpack('<I', self._data[self._position:self._position + 4])[0]
        self._position += 4
        return value
    
    def read_float(self) -> float:
        """Read 32-bit float"""
        if self._position + 4 > len(self._data):
            raise ValueError("Insufficient data for float")
        value = struct.unpack('<f', self._data[self._position:self._position + 4])[0]
        self._position += 4
        return value
    
    def read_double(self) -> float:
        """Read 64-bit double"""
        if self._position + 8 > len(self._data):
            raise ValueError("Insufficient data for double")
        value = struct.unpack('<d', self._data[self._position:self._position + 8])[0]
        self._position += 8
        return value
    
    def read_bool(self) -> bool:
        """Read boolean"""
        return self.read_int32() != 0
    
    def read_string(self) -> str:
        """Read string"""
        length = self.read_int32()
        if length == -1:
            return ""
        if self._position + length > len(self._data):
            raise ValueError("Insufficient data for string")
        value = self._data[self._position:self._position + length].decode('utf-8')
        self._position += length
        return value
    
    def read_bytes(self) -> bytes:
        """Read bytes"""
        length = self.read_int32()
        if length == -1:
            return b""
        if self._position + length > len(self._data):
            raise ValueError("Insufficient data for bytes")
        value = self._data[self._position:self._position + length]
        self._position += length
        return value
    
    def read_list(self, read_func: Callable[[], Any]) -> List[Any]:
        """Read list of items"""
        count = self.read_int32()
        return [read_func() for _ in range(count)]
    
    def read_object(self) -> Any:
        """Read object using registered serializer"""
        hash_code = self.read_int32()
        if hash_code == -1:
            return None
        
        serializer = BufferSerializer.instance()
        handler = serializer.get_handler_by_hash(hash_code)
        if handler is not None:
            return handler.read(self)
        else:
            raise ValueError(f"No serializer found for hash code: {hash_code}")


class TypeHandler(ABC):
    """Abstract base class for type handlers"""
    
    @property
    @abstractmethod
    def hash_code(self) -> int:
        """Get hash code for the type"""
        pass
    
    @abstractmethod
    def write(self, obj: Any, writer: StreamWriter) -> None:
        """Write object to stream"""
        pass
    
    @abstractmethod
    def read(self, reader: StreamReader) -> Any:
        """Read object from stream"""
        pass


class BufferSerializer:
    """Singleton buffer serializer"""
    
    _instance = None
    
    def __init__(self):
        self._handlers: Dict[int, TypeHandler] = {}
        self._type_to_hash: Dict[Type, int] = {}
        self._initialize_default_handlers()
    
    @classmethod
    def instance(cls) -> 'BufferSerializer':
        """Get singleton instance"""
        if cls._instance is None:
            cls._instance = cls()
        return cls._instance
    
    def register_handler(self, type_class: Type, handler: TypeHandler) -> None:
        """Register a type handler"""
        hash_code = handler.hash_code
        self._handlers[hash_code] = handler
        self._type_to_hash[type_class] = hash_code
    
    def get_handler(self, type_class: Type) -> Optional[TypeHandler]:
        """Get handler for type"""
        hash_code = self._type_to_hash.get(type_class)
        if hash_code is not None:
            return self._handlers.get(hash_code)
        return None
    
    def get_handler_by_hash(self, hash_code: int) -> Optional[TypeHandler]:
        """Get handler by hash code"""
        return self._handlers.get(hash_code)
    
    def _initialize_default_handlers(self) -> None:
        """Initialize default type handlers"""
        from .handlers import Int32Handler, Int64Handler, FloatHandler, DoubleHandler, BoolHandler, StringHandler
        
        self.register_handler(int, Int32Handler())
        self.register_handler(type(int(0)), Int32Handler())  # For int subclasses
        self.register_handler(type(0), Int32Handler())
        
        # Note: Python doesn't distinguish between int32 and int64 at runtime level
        # For large integers, we'll use int64
        self.register_handler(type(0).__class__, Int64Handler())
        
        self.register_handler(float, FloatHandler())
        self.register_handler(type(0.0), FloatHandler())
        
        self.register_handler(bool, BoolHandler())
        self.register_handler(type(True), BoolHandler())
        
        self.register_handler(str, StringHandler())
        self.register_handler(type(""), StringHandler())