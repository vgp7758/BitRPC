import asyncio
import struct
import json
from typing import Any, Dict, Callable, Optional, List, TypeVar, Generic
from abc import ABC, abstractmethod
from .serialization import BufferSerializer, StreamWriter, StreamReader
from .client import IRpcClient

T = TypeVar('T')
R = TypeVar('R')

class RpcServer:
    """TCP-based RPC server implementation"""

    def __init__(self, host: str, port: int, serializer: Optional[BufferSerializer] = None):
        self.host = host
        self.port = port
        self._serializer = serializer or BufferSerializer()
        self._services: Dict[str, 'BaseService'] = {}
        self._server: Optional[asyncio.Server] = None
        self._running = False

    async def start_async(self):
        """Start the RPC server"""
        if self._running:
            return

        self._server = await asyncio.start_server(
            self.handle_client, self.host, self.port
        )
        self._running = True
        print(f"RPC server started on {self.host}:{self.port}")

    async def stop_async(self):
        """Stop the RPC server"""
        if self._server:
            self._server.close()
            await self._server.wait_closed()
        self._running = False
        print("RPC server stopped")

    async def wait_async(self):
        """Wait for server to complete"""
        if self._server:
            async with self._server:
                await self._server.serve_forever()

    def register_service(self, service: 'BaseService'):
        """Register a service with the server"""
        service_name = service.service_name
        if not service_name:
            raise ValueError("Service must have a service_name")

        self._services[service_name] = service
        print(f"Registered service: {service_name}")

    async def handle_client(self, reader, writer):
        """Handle incoming client connections"""
        try:
            while True:
                # Read message length
                length_data = await reader.read(4)
                if not length_data:
                    break

                message_length = struct.unpack('I', length_data)[0]

                # Read message content
                message_data = await reader.read(message_length)
                if not message_data:
                    break

                # Parse method name and request data
                method_length = struct.unpack('I', message_data[:4])[0]
                method_data = message_data[4:4+method_length]
                request_length = struct.unpack('I', message_data[4+method_length:8+method_length])[0]
                request_data = message_data[8+method_length:8+method_length+request_length]

                # Extract service and method names
                method_name = method_data.decode('utf-8')
                if '.' not in method_name:
                    raise ValueError(f"Invalid method format: {method_name}")

                service_name, method_key = method_name.split('.', 1)

                # Find the service
                service = self._services.get(service_name)
                if not service:
                    error_response = {"error": f"Service '{service_name}' not found"}
                    await self.send_response(writer, error_response)
                    continue

                # Deserialize request
                try:
                    request_reader = StreamReader(request_data)
                    request = self._serializer.deserialize(request_reader)
                except Exception as e:
                    error_response = {"error": f"Request deserialization failed: {str(e)}"}
                    await self.send_response(writer, error_response)
                    continue

                # Handle the request
                try:
                    response = await service.handle_request_async(method_key, request)
                    await self.send_response(writer, response)
                except Exception as e:
                    error_response = {"error": f"Method execution failed: {str(e)}"}
                    await self.send_response(writer, error_response)

        except Exception as e:
            print(f"Error handling client: {e}")
        finally:
            writer.close()
            await writer.wait_closed()

    async def send_response(self, writer, response: Any):
        """Send response to client"""
        try:
            # Serialize response
            response_writer = StreamWriter()
            self._serializer.serialize(response, response_writer)
            response_data = response_writer.to_bytes()

            # Send response length and data
            writer.write(struct.pack('I', len(response_data)))
            writer.write(response_data)
            await writer.drain()
        except Exception as e:
            print(f"Error sending response: {e}")

    @property
    def is_running(self) -> bool:
        """Check if server is running"""
        return self._running

class BaseService(ABC):
    """Base class for RPC services"""

    def __init__(self):
        self._methods: Dict[str, Callable] = {}
        self._register_methods()

    @property
    @abstractmethod
    def service_name(self) -> str:
        """Get the service name"""
        pass

    @abstractmethod
    def _register_methods(self):
        """Register service methods"""
        pass

    def register_method(self, method_name: str, method_func: Callable):
        """Register a service method"""
        self._methods[method_name] = method_func

    async def handle_request_async(self, method_name: str, request: Any) -> Any:
        """Handle a service request"""
        method = self._methods.get(method_name)
        if not method:
            raise ValueError(f"Method '{method_name}' not found")

        if asyncio.iscoroutinefunction(method):
            return await method(request)
        else:
            return method(request)

    def get_method_names(self) -> List[str]:
        """Get list of registered method names"""
        return list(self._methods.keys())

    def has_method(self, method_name: str) -> bool:
        """Check if method is registered"""
        return method_name in self._methods

class ServiceManager:
    """Manager for RPC services"""

    def __init__(self):
        self._services: Dict[str, BaseService] = {}

    def register_service(self, service: BaseService):
        """Register a service"""
        service_name = service.service_name
        if service_name in self._services:
            raise ValueError(f"Service '{service_name}' already registered")

        self._services[service_name] = service
        print(f"Service registered: {service_name}")

    def unregister_service(self, service_name: str):
        """Unregister a service"""
        if service_name in self._services:
            del self._services[service_name]
            print(f"Service unregistered: {service_name}")

    def get_service(self, service_name: str) -> Optional[BaseService]:
        """Get a service by name"""
        return self._services.get(service_name)

    def get_service_names(self) -> List[str]:
        """Get list of registered service names"""
        return list(self._services.keys())

    def has_service(self, service_name: str) -> bool:
        """Check if service is registered"""
        return service_name in self._services

# Utility classes for type safety
class ServiceMethod:
    """Represents a service method"""

    def __init__(self, name: str, request_type: type, response_type: type, func: Callable):
        self.name = name
        self.request_type = request_type
        self.response_type = response_type
        self.func = func

class TypedServiceBase(BaseService):
    """Base class for typed services"""

    def __init__(self):
        super().__init__()
        self._typed_methods: Dict[str, ServiceMethod] = {}

    def register_typed_method(self, name: str, request_type: type, response_type: type, func: Callable):
        """Register a typed service method"""
        method = ServiceMethod(name, request_type, response_type, func)
        self._typed_methods[name] = method
        self.register_method(name, func)

    def get_typed_method(self, name: str) -> Optional[ServiceMethod]:
        """Get a typed method by name"""
        return self._typed_methods.get(name)

    def get_typed_method_names(self) -> List[str]:
        """Get list of typed method names"""
        return list(self._typed_methods.keys())