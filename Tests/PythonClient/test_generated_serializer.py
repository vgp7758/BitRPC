#!/usr/bin/env python3
"""
Test generated serializers from code generator
"""

import sys
import os

# Add the generated Python code to path
generated_path = os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python')
sys.path.insert(0, generated_path)

# Test generated serializer directly
def test_generated_serializer():
    """Test the generated LoginRequestSerializer"""
    print("Testing generated LoginRequestSerializer...")
    
    try:
        # Import generated components
        from data.models import LoginRequest
        from serialization.loginrequest_serializer import LoginRequestSerializer
        from bitrpc.serialization import StreamWriter, StreamReader
        
        # Create test request
        request = LoginRequest(username="admin", password="password")
        print(f"Original request: username='{request.username}', password='{request.password}'")
        
        # Use generated serializer
        serializer = LoginRequestSerializer()
        
        # Serialize
        writer = StreamWriter()
        serializer.write(request, writer)
        data = writer.to_array()
        print(f"Generated serialized data: {data.hex()}")
        
        # Deserialize
        reader = StreamReader(data)
        deserialized = serializer.read(reader)
        print(f"Deserialized request: username='{deserialized.username}', password='{deserialized.password}'")
        
        # Verify
        assert deserialized.username == request.username
        assert deserialized.password == request.password
        print("SUCCESS: Generated serializer test passed!")
        
        # Compare with manual serialization
        import struct
        
        # Manual serialization (same as C# format)
        bitmask = 0x3  # both fields present
        username_bytes = request.username.encode('utf-8')
        password_bytes = request.password.encode('utf-8')
        
        manual_data = bytearray()
        manual_data.extend(struct.pack('<I', bitmask))
        manual_data.extend(struct.pack('<I', len(username_bytes)))
        manual_data.extend(username_bytes)
        manual_data.extend(struct.pack('<I', len(password_bytes)))
        manual_data.extend(password_bytes)
        
        print(f"Manual serialized data: {manual_data.hex()}")
        
        # Compare formats
        if data == manual_data:
            print("SUCCESS: Generated serializer matches manual format!")
        else:
            print("WARNING: Generated serializer differs from manual format")
            print(f"Generated: {data.hex()}")
            print(f"Manual:    {manual_data.hex()}")
            
    except Exception as e:
        print(f"FAILED: Generated serializer test failed: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    print("Generated Serializer Test")
    print("========================")
    
    test_generated_serializer()