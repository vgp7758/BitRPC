import asyncio
import struct
import socket
from typing import Any, TypeVar, Generic, Callable, Optional
from abc import ABC, abstractmethod
from .serialization import BufferSerializer, StreamWriter, StreamReader

T = TypeVar('T')
R = TypeVar('R')

class IRpcClient(ABC):
    """Interface for RPC clients"""

    @abstractmethod
    async def call_async(self, method: str, request: Any) -> Any:
        """Call a remote method"""
        pass

    @abstractmethod
    async def connect_async(self) -> None:
        """Connect to the RPC server"""
        pass

    @abstractmethod
    async def disconnect_async(self) -> None:
        """Disconnect from the RPC server"""
        pass

    @property
    @abstractmethod
    def is_connected(self) -> bool:
        """Check if client is connected"""
        pass

class TcpRpcClient(IRpcClient):
    """TCP-based RPC client implementation"""

    def __init__(self, host: str, port: int, serializer: Optional[BufferSerializer] = None):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self._serializer = serializer or BufferSerializer()
        self._connected = False

    async def connect_async(self):
        """Connect to the RPC server"""
        if self._connected:
            return

        self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
        self._connected = True

    async def disconnect_async(self):
        """Disconnect from the RPC server"""
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()
        self.reader = None
        self.writer = None
        self._connected = False

    async def call_async(self, method: str, request: Any) -> Any:
        """Call a remote method"""
        if not self._connected:
            raise ConnectionError("Client is not connected")

        try:
            # Serialize the request
            request_writer = StreamWriter()
            self._serializer.serialize(request, request_writer)
            request_data = request_writer.to_bytes()

            # Create the message: method_length + method_data + request_length + request_data
            method_data = method.encode('utf-8')
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
            response_reader = StreamReader(response_data)
            response = self._serializer.deserialize(response_reader)

            return response

        except Exception as e:
            raise RuntimeError(f"RPC call failed: {e}")

    @property
    def is_connected(self) -> bool:
        """Check if client is connected"""
        return self._connected

class BaseClient:
    """Base class for service clients"""

    def __init__(self, client: IRpcClient):
        self._client = client

    async def connect_async(self):
        """Connect to the RPC server"""
        await self._client.connect_async()

    async def disconnect_async(self):
        """Disconnect from the RPC server"""
        await self._client.disconnect_async()

    @property
    def is_connected(self) -> bool:
        """Check if client is connected"""
        return self._client.is_connected

    async def call_async(self, method: str, request: Any) -> Any:
        """Call a remote method"""
        return await self._client.call_async(method, request)

    async def call_typed_async(self, method: str, request: T, response_type: type[R]) -> R:
        """Call a remote method with type safety"""
        response = await self.call_async(method, request)
        return response  # Type casting would happen here in a more typed language

class RpcClientFactory:
    """Factory for creating RPC clients"""

    @staticmethod
    def create_tcp_client(host: str, port: int, serializer: Optional[BufferSerializer] = None) -> TcpRpcClient:
        """Create a TCP RPC client"""
        return TcpRpcClient(host, port, serializer)

    @staticmethod
    def create_base_client(client: IRpcClient) -> BaseClient:
        """Create a base client wrapper"""
        return BaseClient(client)