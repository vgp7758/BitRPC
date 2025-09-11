import asyncio
import sys
import os

# Add the generated Python code to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'Generated', 'python'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'Src', 'Python'))

from bitrpc.client import TcpRpcClient, BaseClient
from client.userservice_client import UserServiceClient
from data.models import LoginRequest, GetUserRequest

async def main():
    # Create RPC client
    client = TcpRpcClient("localhost", 8080)
    
    # Create service client
    user_service = UserServiceClient(client)
    
    try:
        # Connect to the server
        await client.connect_async()
        print("Connected to the server")
        
        # Test Login method
        print("\n--- Testing Login ---")
        login_request = LoginRequest()
        login_request.username = "admin"
        login_request.password = "password"
        
        login_response = await user_service.Login_async(login_request)
        print(f"Login success: {login_response.success}")
        if login_response.success:
            print(f"User ID: {login_response.user.user_id}")
            print(f"Username: {login_response.user.username}")
            print(f"Token: {login_response.token}")
        else:
            print(f"Login error: {login_response.error_message}")
        
        # Test GetUser method
        print("\n--- Testing GetUser ---")
        get_user_request = GetUserRequest()
        get_user_request.user_id = 1
        
        get_user_response = await user_service.GetUser_async(get_user_request)
        print(f"User found: {get_user_response.found}")
        if get_user_response.found:
            print(f"User ID: {get_user_response.user.user_id}")
            print(f"Username: {get_user_response.user.username}")
            print(f"Email: {get_user_response.user.email}")
        
        # Test with invalid credentials
        print("\n--- Testing Login with invalid credentials ---")
        login_request2 = LoginRequest()
        login_request2.username = "invalid"
        login_request2.password = "wrong"
        
        login_response2 = await user_service.Login_async(login_request2)
        print(f"Login success: {login_response2.success}")
        if not login_response2.success:
            print(f"Login error: {login_response2.error_message}")
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Close the connection
        await client.disconnect_async()
        print("Connection closed")

if __name__ == "__main__":
    asyncio.run(main())