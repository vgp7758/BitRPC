#!/usr/bin/env python3
"""
BitRPC Python Test Client
Tests communication with C# server using generated protocol code
"""

import asyncio
import struct
import socket
from typing import Any, Optional
from dataclasses import dataclass, field
from datetime import datetime

# Import generated models
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python'))

from data.models import LoginRequest, LoginResponse, GetUserRequest, GetUserResponse, User


class BitRpcClient:
    """Simple TCP client for BitRPC protocol"""
    
    def __init__(self, host: str = 'localhost', port: int = 8080):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self.connected = False
    
    async def connect(self):
        """Connect to the RPC server"""
        try:
            self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
            self.connected = True
            print(f"Connected to {self.host}:{self.port}")
        except Exception as e:
            print(f"Failed to connect: {e}")
            raise
    
    async def disconnect(self):
        """Disconnect from the server"""
        if self.writer:
            self.writer.close()
            await self.writer.wait_closed()
            self.connected = False
            print("Disconnected from server")
    
    async def call_method(self, method_name: str, request_data: bytes) -> bytes:
        """Call an RPC method and return response"""
        if not self.connected:
            raise RuntimeError("Not connected to server")
        
        try:
            # Write method name
            method_bytes = method_name.encode('utf-8')
            method_length = struct.pack('<I', len(method_bytes))
            
            # Create full message: method_length + method_name + request_data
            full_message = method_length + method_bytes + request_data
            
            # Send message with length prefix
            message_length = struct.pack('<I', len(full_message))
            self.writer.write(message_length)
            self.writer.write(full_message)
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
            
            return response_data
            
        except Exception as e:
            print(f"RPC call failed: {e}")
            raise
    
    def serialize_request(self, request: Any) -> bytes:
        """Simple serialization for request objects"""
        # This is a simplified serialization - in real implementation,
        # this would use the generated serializers
        if isinstance(request, LoginRequest):
            # Create simple binary format for LoginRequest
            data = bytearray()
            
            # Username
            username_bytes = request.username.encode('utf-8')
            data.extend(struct.pack('<I', len(username_bytes)))
            data.extend(username_bytes)
            
            # Password
            password_bytes = request.password.encode('utf-8')
            data.extend(struct.pack('<I', len(password_bytes)))
            data.extend(password_bytes)
            
            return bytes(data)
        
        elif isinstance(request, GetUserRequest):
            # Create simple binary format for GetUserRequest
            return struct.pack('<q', request.user_id)
        
        else:
            raise ValueError(f"Unknown request type: {type(request)}")
    
    def deserialize_response(self, response_data: bytes, response_type: type) -> Any:
        """Simple deserialization for response objects"""
        # This is a simplified deserialization - in real implementation,
        # this would use the generated deserializers
        if response_type == LoginResponse:
            if len(response_data) < 1:
                return LoginResponse()
            
            pos = 0
            response = LoginResponse()
            
            # Success flag
            response.success = bool(response_data[pos])
            pos += 1
            
            # User (simplified - just skip for now)
            response.user = None
            
            # Token
            if pos < len(response_data):
                token_length = struct.unpack('<I', response_data[pos:pos+4])[0]
                pos += 4
                if pos + token_length <= len(response_data):
                    response.token = response_data[pos:pos+token_length].decode('utf-8')
                    pos += token_length
            
            # Error message
            if pos < len(response_data):
                error_length = struct.unpack('<I', response_data[pos:pos+4])[0]
                pos += 4
                if pos + error_length <= len(response_data):
                    response.error_message = response_data[pos:pos+error_length].decode('utf-8')
            
            return response
        
        elif response_type == GetUserResponse:
            if len(response_data) < 1:
                return GetUserResponse()
            
            pos = 0
            response = GetUserResponse()
            
            # Found flag
            response.found = bool(response_data[pos])
            pos += 1
            
            # User (simplified - just skip for now)
            response.user = None
            
            return response
        
        else:
            raise ValueError(f"Unknown response type: {response_type}")


class UserServiceClient:
    """Client for UserService RPC calls"""
    
    def __init__(self, rpc_client: BitRpcClient):
        self.rpc_client = rpc_client
    
    async def login(self, username: str, password: str) -> LoginResponse:
        """Call Login method"""
        request = LoginRequest(username=username, password=password)
        request_data = self.rpc_client.serialize_request(request)
        
        response_data = await self.rpc_client.call_method("UserService.Login", request_data)
        return self.rpc_client.deserialize_response(response_data, LoginResponse)
    
    async def get_user(self, user_id: int) -> GetUserResponse:
        """Call GetUser method"""
        request = GetUserRequest(user_id=user_id)
        request_data = self.rpc_client.serialize_request(request)
        
        response_data = await self.rpc_client.call_method("UserService.GetUser", request_data)
        return self.rpc_client.deserialize_response(response_data, GetUserResponse)


def print_user(user: User):
    """Print user details"""
    if user:
        print(f"User Details:")
        print(f"  ID: {user.user_id}")
        print(f"  Username: {user.username}")
        print(f"  Email: {user.email}")
        print(f"  Active: {user.is_active}")
        print(f"  Roles: {', '.join(user.roles)}")
        print(f"  Created: {user.created_at}")


async def main():
    """Main test function"""
    print("BitRPC Python Test Client")
    print("=========================")
    
    rpc_client = BitRpcClient('localhost', 8080)
    user_client = UserServiceClient(rpc_client)
    
    try:
        # Connect to server
        print("Connecting to server...")
        await rpc_client.connect()
        
        # Test 1: Login with admin credentials
        print("\n=== Test 1: Admin Login ===")
        try:
            login_response = await user_client.login("admin", "password")
            print(f"Login result: {login_response.success}")
            print(f"Token: {login_response.token}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
            if login_response.user:
                print_user(login_response.user)
        except Exception as e:
            print(f"Login test failed: {e}")
        
        # Test 2: Login with user credentials
        print("\n=== Test 2: User Login ===")
        try:
            login_response = await user_client.login("user", "password")
            print(f"Login result: {login_response.success}")
            print(f"Token: {login_response.token}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
            if login_response.user:
                print_user(login_response.user)
        except Exception as e:
            print(f"Login test failed: {e}")
        
        # Test 3: Failed login
        print("\n=== Test 3: Failed Login ===")
        try:
            login_response = await user_client.login("wrong", "credentials")
            print(f"Login result: {login_response.success}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
        except Exception as e:
            print(f"Failed login test: {e}")
        
        # Test 4: Get user by ID
        print("\n=== Test 4: Get User by ID ===")
        try:
            get_user_response = await user_client.get_user(1)
            print(f"GetUser result: {get_user_response.found}")
            if get_user_response.user:
                print_user(get_user_response.user)
        except Exception as e:
            print(f"GetUser test failed: {e}")
        
        # Test 5: Get non-existent user
        print("\n=== Test 5: Get Non-existent User ===")
        try:
            get_user_response = await user_client.get_user(999)
            print(f"GetUser result: {get_user_response.found}")
            if get_user_response.user:
                print_user(get_user_response.user)
        except Exception as e:
            print(f"GetUser test failed: {e}")
        
        print("\nAll tests completed!")
        
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        await rpc_client.disconnect()


if __name__ == "__main__":
    asyncio.run(main())