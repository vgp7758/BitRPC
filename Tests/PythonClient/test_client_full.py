#!/usr/bin/env python3
"""
BitRPC Python Test Client with Full Protocol Support
Tests communication with C# server using proper BitRPC serialization
"""

import asyncio
import sys
import os

# Add the generated Python code to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python'))

# Import generated models and services
from data.models import LoginRequest, LoginResponse, GetUserRequest, GetUserResponse, User
from client.userservice_client import UserServiceClient
from serialization.registry import register_serializers
from bitrpc.client import TcpRpcClient
from bitrpc.serialization import BufferSerializer


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
    print("BitRPC Python Test Client (Full Protocol)")
    print("=======================================")
    
    try:
        # Initialize protocol serializers
        print("Initializing protocol serializers...")
        serializer = BufferSerializer.instance()
        register_serializers(serializer)
        
        # Create RPC client
        rpc_client = TcpRpcClient('localhost', 8080)
        user_client = UserServiceClient(rpc_client)
        
        # Connect to server
        print("Connecting to server...")
        await user_client.connect_async()
        print("Connected successfully!")
        
        # Test 1: Login with admin credentials
        print("\n=== Test 1: Admin Login ===")
        try:
            login_request = LoginRequest(username="admin", password="password")
            login_response = await user_client.Login_async(login_request)
            print(f"Login result: {login_response.success}")
            print(f"Token: {login_response.token}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
            if login_response.user:
                print_user(login_response.user)
        except Exception as e:
            print(f"Admin login test failed: {e}")
            import traceback
            traceback.print_exc()
        
        # Test 2: Login with user credentials
        print("\n=== Test 2: User Login ===")
        try:
            login_request = LoginRequest(username="user", password="password")
            login_response = await user_client.Login_async(login_request)
            print(f"Login result: {login_response.success}")
            print(f"Token: {login_response.token}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
            if login_response.user:
                print_user(login_response.user)
        except Exception as e:
            print(f"User login test failed: {e}")
            import traceback
            traceback.print_exc()
        
        # Test 3: Failed login
        print("\n=== Test 3: Failed Login ===")
        try:
            login_request = LoginRequest(username="wrong", password="credentials")
            login_response = await user_client.Login_async(login_request)
            print(f"Login result: {login_response.success}")
            if login_response.error_message:
                print(f"Error: {login_response.error_message}")
        except Exception as e:
            print(f"Failed login test: {e}")
            import traceback
            traceback.print_exc()
        
        # Test 4: Get user by ID
        print("\n=== Test 4: Get User by ID ===")
        try:
            get_user_request = GetUserRequest(user_id=1)
            get_user_response = await user_client.GetUser_async(get_user_request)
            print(f"GetUser result: {get_user_response.found}")
            if get_user_response.user:
                print_user(get_user_response.user)
        except Exception as e:
            print(f"GetUser test failed: {e}")
            import traceback
            traceback.print_exc()
        
        # Test 5: Get non-existent user
        print("\n=== Test 5: Get Non-existent User ===")
        try:
            get_user_request = GetUserRequest(user_id=999)
            get_user_response = await user_client.GetUser_async(get_user_request)
            print(f"GetUser result: {get_user_response.found}")
            if get_user_response.user:
                print_user(get_user_response.user)
        except Exception as e:
            print(f"GetUser test failed: {e}")
            import traceback
            traceback.print_exc()
        
        print("\nAll tests completed!")
        
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        try:
            await user_client.disconnect_async()
        except:
            pass


if __name__ == "__main__":
    asyncio.run(main())