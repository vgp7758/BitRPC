import asyncio
import struct
import socket
from typing import Any, TypeVar, Generic, Dict, List, Callable
from dataclasses import dataclass, field
from datetime import datetime
import pickle

T = TypeVar('T')

class StreamWriter:
    def __init__(self):
        self.buffer = bytearray()
        
    def write_int32(self, value: int):
        self.buffer.extend(struct.pack('i', value))
        
    def write_int64(self, value: int):
        self.buffer.extend(struct.pack('q', value))
        
    def write_uint32(self, value: int):
        self.buffer.extend(struct.pack('I', value))
        
    def write_float(self, value: float):
        self.buffer.extend(struct.pack('f', value))
        
    def write_double(self, value: float):
        self.buffer.extend(struct.pack('d', value))
        
    def write_bool(self, value: bool):
        self.write_int32(1 if value else 0)
        
    def write_string(self, value: str):
        if value is None or value == "":
            self.write_int32(-1)
        else:
            encoded = value.encode('utf-8')
            self.write_int32(len(encoded))
            self.buffer.extend(encoded)
            
    def write_bytes(self, value: bytes):
        if value is None:
            self.write_int32(-1)
        else:
            self.write_int32(len(value))
            self.buffer.extend(value)
            
    def write_list(self, value: List, write_func: Callable):
        if value is None:
            self.write_int32(-1)
        else:
            self.write_int32(len(value))
            for item in value:
                write_func(item)
                
    def write_object(self, obj: Any):
        if obj is None:
            self.write_int32(-1)
            return
            
        # For simplicity, we're using pickle for object serialization
        # In a real implementation, you would use BitRPC's serialization
        data = pickle.dumps(obj)
        self.write_int32(len(data))
        self.buffer.extend(data)
        
    def to_bytes(self) -> bytes:
        return bytes(self.buffer)

class StreamReader:
    def __init__(self, data: bytes):
        self.data = data
        self.position = 0
        
    def read_int32(self) -> int:
        if self.position + 4 > len(self.data):
            raise Exception("Unexpected end of stream")
        value = struct.unpack('i', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value
        
    def read_int64(self) -> int:
        if self.position + 8 > len(self.data):
            raise Exception("Unexpected end of stream")
        value = struct.unpack('q', self.data[self.position:self.position+8])[0]
        self.position += 8
        return value
        
    def read_uint32(self) -> int:
        if self.position + 4 > len(self.data):
            raise Exception("Unexpected end of stream")
        value = struct.unpack('I', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value
        
    def read_float(self) -> float:
        if self.position + 4 > len(self.data):
            raise Exception("Unexpected end of stream")
        value = struct.unpack('f', self.data[self.position:self.position+4])[0]
        self.position += 4
        return value
        
    def read_double(self) -> float:
        if self.position + 8 > len(self.data):
            raise Exception("Unexpected end of stream")
        value = struct.unpack('d', self.data[self.position:self.position+8])[0]
        self.position += 8
        return value
        
    def read_bool(self) -> bool:
        return self.read_int32() != 0
        
    def read_string(self) -> str:
        length = self.read_int32()
        if length == -1:
            return ""
        if self.position + length > len(self.data):
            raise Exception("Unexpected end of stream")
        value = self.data[self.position:self.position+length].decode('utf-8')
        self.position += length
        return value
        
    def read_bytes(self) -> bytes:
        length = self.read_int32()
        if length == -1:
            return None
        if self.position + length > len(self.data):
            raise Exception("Unexpected end of stream")
        value = self.data[self.position:self.position+length]
        self.position += length
        return value
        
    def read_list(self, read_func: Callable) -> List:
        length = self.read_int32()
        if length == -1:
            return None
        result = []
        for _ in range(length):
            result.append(read_func())
        return result
        
    def read_object(self) -> Any:
        length = self.read_int32()
        if length == -1:
            return None
        if self.position + length > len(self.data):
            raise Exception("Unexpected end of stream")
        data = self.data[self.position:self.position+length]
        self.position += length
        # For simplicity, we're using pickle for object deserialization
        # In a real implementation, you would use BitRPC's deserialization
        return pickle.loads(data)

class BitMask:
    def __init__(self, size: int = 1):
        self.masks = [0] * size
        
    def set_bit(self, index: int, value: bool):
        mask_index = index // 32
        bit_index = index % 32
        
        if mask_index >= len(self.masks):
            self.masks.extend([0] * (mask_index - len(self.masks) + 1))
            
        if value:
            self.masks[mask_index] |= (1 << bit_index)
        else:
            self.masks[mask_index] &= ~(1 << bit_index)
            
    def get_bit(self, index: int) -> bool:
        mask_index = index // 32
        bit_index = index % 32
        
        if mask_index >= len(self.masks):
            return False
            
        return (self.masks[mask_index] & (1 << bit_index)) != 0
        
    def write(self, writer: StreamWriter):
        writer.write_int32(len(self.masks))
        for mask in self.masks:
            writer.write_uint32(mask)
            
    def read(self, reader: StreamReader):
        size = reader.read_int32()
        self.masks = []
        for _ in range(size):
            self.masks.append(reader.read_uint32())

class TypeHandler:
    @property
    def hash_code(self) -> int:
        raise NotImplementedError()
        
    def write(self, obj: Any, writer: StreamWriter) -> None:
        raise NotImplementedError()
        
    def read(self, reader: StreamReader) -> Any:
        raise NotImplementedError()

class BufferSerializer:
    _instance = None
    _handlers: Dict[int, TypeHandler] = {}
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(BufferSerializer, cls).__new__(cls)
        return cls._instance
        
    @classmethod
    def instance(cls):
        return cls()
        
    def register_handler(self, type_hash: int, handler: TypeHandler):
        self._handlers[type_hash] = handler
        
    def get_handler(self, type_hash: int) -> TypeHandler:
        return self._handlers.get(type_hash)

class TcpRpcClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        
    async def connect_async(self):
        """Connect to the RPC server"""
        self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
        
    async def disconnect_async(self):
        """Disconnect from the RPC server"""
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()
            
    async def call_async(self, method: str, request: Any) -> Any:
        """Call a remote method"""
        if not self.writer:
            raise Exception("Client is not connected")
            
        # Serialize the request
        serializer = BufferSerializer.instance()
        handler = serializer.get_handler(hash(type(request).__name__))
        
        writer = StreamWriter()
        if handler:
            writer.write_string(method)
            handler.write(request, writer)
        else:
            # Fallback to pickle for unknown types
            import pickle
            request_data = pickle.dumps(request)
            method_data = method.encode('utf-8')
            
            # Create the message: method_length + method_data + request_length + request_data
            message = (
                struct.pack('I', len(method_data)) +
                method_data +
                struct.pack('I', len(request_data)) +
                request_data
            )
            
            # Send the message
            self.writer.write(struct.pack('I', len(message)))
            self.writer.write(message)
            await self.writer.drain()
            
            # Read the response
            response_length_data = await self.reader.readexactly(4)
            response_length = struct.unpack('I', response_length_data)[0]
            response_data = await self.reader.readexactly(response_length)
            
            # Deserialize the response
            # For simplicity, we're using a basic approach here
            # In a real implementation, you would use the BitRPC serialization
            response = pickle.loads(response_data)
            return response

class BaseClient:
    def __init__(self, client: TcpRpcClient):
        self._client = client
        
    async def connect_async(self):
        await self._client.connect_async()
        
    async def disconnect_async(self):
        await self._client.disconnect_async()