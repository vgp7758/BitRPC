#!/usr/bin/env python3
"""
Simple test for Python serialization
"""

import sys
import os

# Add the generated Python code to path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python'))

# Import basic serialization components
from bitrpc.serialization import StreamWriter, StreamReader, BufferSerializer
from data.models import LoginRequest, LoginResponse
from serialization.registry import register_serializers


def test_login_request_serialization():
    """Test LoginRequest serialization"""
    print("Testing LoginRequest serialization...")
    
    # Initialize serializer
    serializer = BufferSerializer.instance()
    register_serializers(serializer)
    
    # Create test request
    request = LoginRequest(username="testuser", password="testpass")
    print(f"Original request: username='{request.username}', password='{request.password}'")
    
    # Serialize
    writer = StreamWriter()
    writer.write_object(request)
    data = writer.to_array()
    print(f"Serialized data: {data.hex()}")
    
    # Deserialize
    reader = StreamReader(data)
    deserialized = reader.read_object()
    print(f"Deserialized request: username='{deserialized.username}', password='{deserialized.password}'")
    
    # Verify
    assert deserialized.username == request.username
    assert deserialized.password == request.password
    print("✅ LoginRequest serialization test passed!")


def test_login_response_serialization():
    """Test LoginResponse serialization"""
    print("\nTesting LoginResponse serialization...")
    
    # Initialize serializer
    serializer = BufferSerializer.instance()
    register_serializers(serializer)
    
    # Create test response
    response = LoginResponse(success=True, token="test-token-123", error_message="")
    print(f"Original response: success={response.success}, token='{response.token}', error='{response.error_message}'")
    
    # Serialize
    writer = StreamWriter()
    writer.write_object(response)
    data = writer.to_array()
    print(f"Serialized data: {data.hex()}")
    
    # Deserialize
    reader = StreamReader(data)
    deserialized = reader.read_object()
    print(f"Deserialized response: success={deserialized.success}, token='{deserialized.token}', error='{deserialized.error_message}'")
    
    # Verify
    assert deserialized.success == response.success
    assert deserialized.token == response.token
    assert deserialized.error_message == response.error_message
    print("✅ LoginResponse serialization test passed!")


def test_bitmask():
    """Test BitMask functionality"""
    print("\nTesting BitMask...")
    
    from bitrpc.serialization import BitMask, StreamWriter, StreamReader
    
    # Create and test bitmask
    mask = BitMask()
    mask.set_bit(0, True)   # username present
    mask.set_bit(1, True)   # password present
    mask.set_bit(2, False)  # some other field absent
    
    print(f"Bit 0: {mask.get_bit(0)}")
    print(f"Bit 1: {mask.get_bit(1)}")
    print(f"Bit 2: {mask.get_bit(2)}")
    
    # Serialize and deserialize
    writer = StreamWriter()
    mask.write(writer)
    data = writer.to_array()
    
    reader = StreamReader(data)
    new_mask = BitMask()
    new_mask.read(reader)
    
    print(f"Deserialized Bit 0: {new_mask.get_bit(0)}")
    print(f"Deserialized Bit 1: {new_mask.get_bit(1)}")
    print(f"Deserialized Bit 2: {new_mask.get_bit(2)}")
    
    # Verify
    assert new_mask.get_bit(0) == mask.get_bit(0)
    assert new_mask.get_bit(1) == mask.get_bit(1)
    assert new_mask.get_bit(2) == mask.get_bit(2)
    print("✅ BitMask test passed!")


if __name__ == "__main__":
    print("BitRPC Python Serialization Test")
    print("================================")
    
    try:
        test_bitmask()
        test_login_request_serialization()
        test_login_response_serialization()
        print("\n🎉 All serialization tests passed!")
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()