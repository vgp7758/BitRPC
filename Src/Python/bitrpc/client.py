import asyncio
import struct
import socket
from typing import Any, TypeVar, Generic

T = TypeVar('T')

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
        # For simplicity, we're using a basic approach here
        # In a real implementation, you would use the BitRPC serialization
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