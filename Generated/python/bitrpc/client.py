"""
BitRPC Python Client Core Library
Provides base client functionality for RPC communication
"""

import asyncio
import struct
from typing import Any, Optional
from .serialization import StreamWriter, StreamReader, BufferSerializer


class BaseClient:
    """Base class for RPC clients"""
    
    def __init__(self, rpc_client):
        self._rpc_client = rpc_client
    
    async def call_async(self, method: str, request: Any) -> Any:
        """Call RPC method asynchronously"""
        return await self._rpc_client.call_async(method, request)
    
    async def connect_async(self) -> None:
        """Connect to server"""
        await self._rpc_client.connect_async()
    
    async def disconnect_async(self) -> None:
        """Disconnect from server"""
        await self._rpc_client.disconnect_async()
    
    @property
    def is_connected(self) -> bool:
        """Check if connected"""
        return self._rpc_client.is_connected


class TcpRpcClient:
    """TCP RPC client implementation"""
    
    def __init__(self, host: str = 'localhost', port: int = 8080):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self._connected = False
    
    @property
    def is_connected(self) -> bool:
        """Check if connected"""
        return self._connected
    
    async def connect_async(self) -> None:
        """Connect to RPC server"""
        try:
            self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
            self._connected = True
        except Exception as e:
            raise ConnectionError(f"Failed to connect to {self.host}:{self.port}: {e}")
    
    async def disconnect_async(self) -> None:
        """Disconnect from server"""
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()
            self._connected = False
    
    async def call_async(self, method: str, request: Any) -> Any:
        """Call RPC method"""
        if not self._connected:
            raise RuntimeError("Not connected to server")
        
        try:
            # Serialize the request
            writer = StreamWriter()
            writer.write_string(method)
            writer.write_object(request)
            request_data = writer.to_array()
            
            # Send request with length prefix
            request_length = struct.pack('<I', len(request_data))
            self.writer.write(request_length)
            self.writer.write(request_data)
            await self.writer.drain()
            
            # Read response length
            response_length_bytes = await self.reader.read(4)
            if len(response_length_bytes) != 4:
                raise RuntimeError("Connection closed while reading response length")
            
            response_length = struct.unpack('<I', response_length_bytes)[0]
            
            # Read response data
            response_data = await self.reader.read(response_length)
            if len(response_data) != response_length:
                raise RuntimeError("Connection closed while reading response data")
            
            # Deserialize response
            reader = StreamReader(response_data)
            return reader.read_object()
            
        except Exception as e:
            raise RuntimeError(f"RPC call failed: {e}")