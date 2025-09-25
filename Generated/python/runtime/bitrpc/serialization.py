import struct
from typing import Any, TypeVar, Generic, Dict, List, Callable, Optional, Union
from dataclasses import dataclass, field
from datetime import datetime
from abc import ABC, abstractmethod

T = TypeVar('T')

class StreamWriter:
    """Binary stream writer for serialization"""

    def __init__(self):
        self.buffer = bytearray()

    def write_int32(self, value: int):
        """Write a 32-bit integer"""
        self.buffer.extend(struct.pack('i', value))

    def write_int64(self, value: int):
        """Write a 64-bit integer"""
        self.buffer.extend(struct.pack('q', value))

    def write_uint32(self, value: int):
        """Write a 32-bit unsigned integer"""
        self.buffer.extend(struct.pack('I', value))

    def write_float(self, value: float):
        """Write a 32-bit float"""
        self.buffer.extend(struct.pack('f', value))

    def write_double(self, value: float):
        """Write a 64-bit double"""
        self.buffer.extend(struct.pack('d', value))

    def write_bool(self, value: bool):
        """Write a boolean value"""
        self.write_int32(1 if value else 0)

    def write_string(self, value: str):
        """Write a string value"""
        if value is None:
            self.write_int32(-1)
        elif value == "":
            self.write_int32(0)
        else:
            encoded = value.encode('utf-8')
            self.write_int32(len(encoded))
            self.buffer.extend(encoded)

    def write_bytes(self, value: bytes):
        """Write bytes value"""
        if value is None:
            self.write_int32(-1)
        else:
            self.write_int32(len(value))
            self.buffer.extend(value)

    def write_list(self, value: List, write_func: Callable):
        """Write a list using the provided write function"""
        if value is None:
            self.write_int32(-1)
        else:
            self.write_int32(len(value))
            for item in value:
                write_func(item)

    def write_datetime(self, value: datetime):
        """Write a datetime value"""
        if value is None:
            self.write_int64(-1)
        else:
            # Convert to Unix timestamp (milliseconds)
            timestamp = int(value.timestamp() * 1000)
            self.write_int64(timestamp)

    def to_bytes(self) -> bytes:
        """Get the written bytes"""
        return bytes(self.buffer)

class StreamReader:
    """Binary stream reader for deserialization"""

    def __init__(self, data: bytes):
        self.data = data
        self.position = 0

    def read_int32(self) -> int:
        """Read a 32-bit integer"""
        if self.position + 4 > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = struct.unpack('i', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value

    def read_int64(self) -> int:
        """Read a 64-bit integer"""
        if self.position + 8 > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = struct.unpack('q', self.data[self.position:self.position+8])[0]
        self.position += 8
        return value

    def read_uint32(self) -> int:
        """Read a 32-bit unsigned integer"""
        if self.position + 4 > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = struct.unpack('I', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value

    def read_float(self) -> float:
        """Read a 32-bit float"""
        if self.position + 4 > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = struct.unpack('f', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value

    def read_double(self) -> float:
        """Read a 64-bit double"""
        if self.position + 8 > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = struct.unpack('d', self.data[self.position:self.position+8])[0]
        self.position += 8
        return value

    def read_bool(self) -> bool:
        """Read a boolean value"""
        return self.read_int32() != 0

    def read_string(self) -> str:
        """Read a string value"""
        length = self.read_int32()
        if length == -1:
            return None
        if length == 0:
            return ""
        if self.position + length > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = self.data[self.position:self.position+length].decode('utf-8')
        self.position += length
        return value

    def read_bytes(self) -> bytes:
        """Read bytes value"""
        length = self.read_int32()
        if length == -1:
            return None
        if self.position + length > len(self.data):
            raise ValueError("Unexpected end of stream")
        value = self.data[self.position:self.position+length]
        self.position += length
        return value

    def read_list(self, read_func: Callable) -> List:
        """Read a list using the provided read function"""
        length = self.read_int32()
        if length == -1:
            return None
        result = []
        for _ in range(length):
            result.append(read_func())
        return result

    def read_datetime(self) -> datetime:
        """Read a datetime value"""
        timestamp = self.read_int64()
        if timestamp == -1:
            return None
        # Convert from Unix timestamp (milliseconds)
        return datetime.fromtimestamp(timestamp / 1000)

class BitMask:
    """Bitmask for field presence tracking"""

    def __init__(self, size: int = 1):
        self.masks = [0] * size

    def set_bit(self, index: int, value: bool):
        """Set a bit at the given index"""
        mask_index = index // 32
        bit_index = index % 32

        if mask_index >= len(self.masks):
            self.masks.extend([0] * (mask_index - len(self.masks) + 1))

        if value:
            self.masks[mask_index] |= (1 << bit_index)
        else:
            self.masks[mask_index] &= ~(1 << bit_index)

    def get_bit(self, index: int) -> bool:
        """Get a bit at the given index"""
        mask_index = index // 32
        bit_index = index % 32

        if mask_index >= len(self.masks):
            return False

        return (self.masks[mask_index] & (1 << bit_index)) != 0

    def write(self, writer: StreamWriter):
        """Write the bitmask to a stream"""
        writer.write_int32(len(self.masks))
        for mask in self.masks:
            writer.write_uint32(mask)

    def read(self, reader: StreamReader):
        """Read the bitmask from a stream"""
        size = reader.read_int32()
        self.masks = []
        for _ in range(size):
            self.masks.append(reader.read_uint32())

    @property
    def size(self) -> int:
        """Get the size of the bitmask"""
        return len(self.masks)

class ITypeHandler(ABC):
    """Interface for type handlers"""

    @property
    @abstractmethod
    def hash_code(self) -> int:
        """Get the hash code for the type"""
        pass

    @abstractmethod
    def write(self, obj: Any, writer: StreamWriter) -> None:
        """Write an object to the stream"""
        pass

    @abstractmethod
    def read(self, reader: StreamReader) -> Any:
        """Read an object from the stream"""
        pass

class BaseTypeHandler(ITypeHandler):
    """Base implementation for type handlers"""

    def __init__(self, type_name: str, hash_code: int):
        self.type_name = type_name
        self._hash_code = hash_code

    @property
    def hash_code(self) -> int:
        return self._hash_code

class Int32TypeHandler(BaseTypeHandler):
    """Handler for int32 values"""

    def __init__(self):
        super().__init__("int32", self._compute_hash("int32"))

    def write(self, obj: int, writer: StreamWriter) -> None:
        writer.write_int32(obj)

    def read(self, reader: StreamReader) -> int:
        return reader.read_int32()

    @staticmethod
    def _compute_hash(type_name: str) -> int:
        """Compute a stable hash for the type name"""
        # Simple hash implementation - in production use a more robust hash
        return hash(type_name) & 0xFFFFFFFF

class Int64TypeHandler(BaseTypeHandler):
    """Handler for int64 values"""

    def __init__(self):
        super().__init__("int64", self._compute_hash("int64"))

    def write(self, obj: int, writer: StreamWriter) -> None:
        writer.write_int64(obj)

    def read(self, reader: StreamReader) -> int:
        return reader.read_int64()

    @staticmethod
    def _compute_hash(type_name: str) -> int:
        return hash(type_name) & 0xFFFFFFFF

class StringTypeHandler(BaseTypeHandler):
    """Handler for string values"""

    def __init__(self):
        super().__init__("string", self._compute_hash("string"))

    def write(self, obj: str, writer: StreamWriter) -> None:
        writer.write_string(obj)

    def read(self, reader: StreamReader) -> str:
        return reader.read_string()

    @staticmethod
    def _compute_hash(type_name: str) -> int:
        return hash(type_name) & 0xFFFFFFFF

class BoolTypeHandler(BaseTypeHandler):
    """Handler for boolean values"""

    def __init__(self):
        super().__init__("bool", self._compute_hash("bool"))

    def write(self, obj: bool, writer: StreamWriter) -> None:
        writer.write_bool(obj)

    def read(self, reader: StreamReader) -> bool:
        return reader.read_bool()

    @staticmethod
    def _compute_hash(type_name: str) -> int:
        return hash(type_name) & 0xFFFFFFFF

class DateTimeTypeHandler(BaseTypeHandler):
    """Handler for datetime values"""

    def __init__(self):
        super().__init__("datetime", self._compute_hash("datetime"))

    def write(self, obj: datetime, writer: StreamWriter) -> None:
        writer.write_datetime(obj)

    def read(self, reader: StreamReader) -> datetime:
        return reader.read_datetime()

    @staticmethod
    def _compute_hash(type_name: str) -> int:
        return hash(type_name) & 0xFFFFFFFF

class BufferSerializer:
    """Buffer serializer with type registration"""

    def __init__(self):
        self._handlers: Dict[int, ITypeHandler] = {}
        self._initialize_default_handlers()

    def _initialize_default_handlers(self):
        """Initialize default type handlers"""
        self.register_handler(Int32TypeHandler())
        self.register_handler(Int64TypeHandler())
        self.register_handler(StringTypeHandler())
        self.register_handler(BoolTypeHandler())
        self.register_handler(DateTimeTypeHandler())

    def register_handler(self, handler: ITypeHandler):
        """Register a type handler"""
        self._handlers[handler.hash_code] = handler

    def get_handler(self, hash_code: int) -> Optional[ITypeHandler]:
        """Get a type handler by hash code"""
        return self._handlers.get(hash_code)

    def serialize(self, obj: Any, writer: StreamWriter):
        """Serialize an object to the stream"""
        if obj is None:
            writer.write_int32(-1)  # Null marker
            return

        obj_type = type(obj)
        type_name = obj_type.__name__

        # For now, handle basic types directly
        if isinstance(obj, int):
            if obj.bit_length() <= 31:
                writer.write_int32(1)  # Type code for int32
                writer.write_int32(obj)
            else:
                writer.write_int32(2)  # Type code for int64
                writer.write_int64(obj)
        elif isinstance(obj, str):
            writer.write_int32(3)  # Type code for string
            writer.write_string(obj)
        elif isinstance(obj, bool):
            writer.write_int32(4)  # Type code for bool
            writer.write_bool(obj)
        elif isinstance(obj, datetime):
            writer.write_int32(5)  # Type code for datetime
            writer.write_datetime(obj)
        elif isinstance(obj, list):
            writer.write_int32(6)  # Type code for list
            writer.write_int32(len(obj))
            for item in obj:
                self.serialize(item, writer)
        else:
            # Fallback for complex objects (dataclass or custom objects)
            writer.write_int32(99)  # Complex object marker
            self._serialize_complex_object(obj, writer)

    def _serialize_complex_object(self, obj: Any, writer: StreamWriter):
        """Serialize a complex object using reflection"""
        if hasattr(obj, '__dataclass_fields__'):
            # Handle dataclass
            fields = obj.__dataclass_fields__
            writer.write_int32(len(fields))

            for field_name, field_info in fields.items():
                field_value = getattr(obj, field_name)
                writer.write_string(field_name)
                self.serialize(field_value, writer)
        else:
            # Handle as dictionary
            if hasattr(obj, '__dict__'):
                obj_dict = obj.__dict__
            else:
                raise ValueError(f"Cannot serialize object of type {type(obj)}")

            writer.write_int32(len(obj_dict))
            for key, value in obj_dict.items():
                writer.write_string(key)
                self.serialize(value, writer)

    def deserialize(self, reader: StreamReader) -> Any:
        """Deserialize an object from the stream"""
        type_code = reader.read_int32()

        if type_code == -1:
            return None  # Null value
        elif type_code == 1:
            return reader.read_int32()  # int32
        elif type_code == 2:
            return reader.read_int64()  # int64
        elif type_code == 3:
            return reader.read_string()  # string
        elif type_code == 4:
            return reader.read_bool()  # bool
        elif type_code == 5:
            return reader.read_datetime()  # datetime
        elif type_code == 6:
            # List
            length = reader.read_int32()
            return [self.deserialize(reader) for _ in range(length)]
        elif type_code == 99:
            # Complex object
            return self._deserialize_complex_object(reader)
        else:
            raise ValueError(f"Unknown type code: {type_code}")

    def _deserialize_complex_object(self, reader: StreamReader) -> Any:
        """Deserialize a complex object"""
        field_count = reader.read_int32()
        fields = {}

        for _ in range(field_count):
            field_name = reader.read_string()
            field_value = self.deserialize(reader)
            fields[field_name] = field_value

        # For now, return as dictionary
        # In a full implementation, this would reconstruct the original object type
        return fields