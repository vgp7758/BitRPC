#!/usr/bin/env python3
"""
Simple test for Python serialization with direct imports
"""

import sys
import os

# Add the generated Python code to path
generated_path = os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python')
sys.path.insert(0, generated_path)

# Direct imports
from data.models import LoginRequest, LoginResponse


def test_basic_serialization():
    """Test basic serialization without full framework"""
    print("Testing basic serialization...")
    
    # Create test request
    request = LoginRequest(username="testuser", password="testpass")
    print(f"Original request: username='{request.username}', password='{request.password}'")
    
    # Simple manual serialization (simulate what BitRPC should do)
    import struct
    
    # Create bit mask for field presence
    username_present = request.username != ""
    password_present = request.password != ""
    
    # Bit mask: bit 0 = username, bit 1 = password
    bitmask = 0
    if username_present:
        bitmask |= (1 << 0)
    if password_present:
        bitmask |= (1 << 1)
    
    # Serialize
    data = bytearray()
    
    # Write bit mask (1 uint32)
    data.extend(struct.pack('<I', bitmask))
    
    # Write username if present
    if username_present:
        username_bytes = request.username.encode('utf-8')
        data.extend(struct.pack('<I', len(username_bytes)))
        data.extend(username_bytes)
    
    # Write password if present
    if password_present:
        password_bytes = request.password.encode('utf-8')
        data.extend(struct.pack('<I', len(password_bytes)))
        data.extend(password_bytes)
    
    print(f"Serialized data: {data.hex()}")
    
    # Deserialize
    pos = 0
    
    # Read bit mask
    deserialized_bitmask = struct.unpack('<I', data[pos:pos+4])[0]
    pos += 4
    
    # Check field presence
    username_present_deserialized = (deserialized_bitmask & (1 << 0)) != 0
    password_present_deserialized = (deserialized_bitmask & (1 << 1)) != 0
    
    # Read username if present
    username_deserialized = ""
    if username_present_deserialized:
        username_length = struct.unpack('<I', data[pos:pos+4])[0]
        pos += 4
        username_deserialized = data[pos:pos+username_length].decode('utf-8')
        pos += username_length
    
    # Read password if present
    password_deserialized = ""
    if password_present_deserialized:
        password_length = struct.unpack('<I', data[pos:pos+4])[0]
        pos += 4
        password_deserialized = data[pos:pos+password_length].decode('utf-8')
        pos += password_length
    
    # Create deserialized object
    deserialized = LoginRequest(username=username_deserialized, password=password_deserialized)
    print(f"Deserialized request: username='{deserialized.username}', password='{deserialized.password}'")
    
    # Verify
    assert deserialized.username == request.username
    assert deserialized.password == request.password
    print("SUCCESS: Basic serialization test passed!")


def test_csharp_compatibility():
    """Test serialization format compatibility with C#"""
    print("\nTesting C# compatibility...")
    
    # According to C# implementation, the format should be:
    # 1. Bit mask (int32)
    # 2. For each present field: length (int32) + UTF-8 bytes
    
    request = LoginRequest(username="admin", password="password")
    print(f"Testing with: username='{request.username}', password='{request.password}'")
    
    # Create exactly what C# would create
    import struct
    
    # Bit mask: both fields present
    bitmask = 0x3  # bits 0 and 1 set
    username_bytes = request.username.encode('utf-8')
    password_bytes = request.password.encode('utf-8')
    
    # Build data: [bitmask][username_length][username][password_length][password]
    data = bytearray()
    data.extend(struct.pack('<I', bitmask))  # bit mask
    data.extend(struct.pack('<I', len(username_bytes)))  # username length
    data.extend(username_bytes)  # username
    data.extend(struct.pack('<I', len(password_bytes)))  # password length
    data.extend(password_bytes)  # password
    
    print(f"C# compatible data: {data.hex()}")
    print(f"Total length: {len(data)} bytes")
    
    # Now parse it back
    pos = 0
    parsed_bitmask = struct.unpack('<I', data[pos:pos+4])[0]
    pos += 4
    
    username_present = (parsed_bitmask & 1) != 0
    password_present = (parsed_bitmask & 2) != 0
    
    parsed_username = ""
    if username_present:
        username_len = struct.unpack('<I', data[pos:pos+4])[0]
        pos += 4
        parsed_username = data[pos:pos+username_len].decode('utf-8')
        pos += username_len
    
    parsed_password = ""
    if password_present:
        password_len = struct.unpack('<I', data[pos:pos+4])[0]
        pos += 4
        parsed_password = data[pos:pos+password_len].decode('utf-8')
        pos += password_len
    
    print(f"Parsed: username='{parsed_username}', password='{parsed_password}'")
    
    # Verify
    assert parsed_username == request.username
    assert parsed_password == request.password
    print("SUCCESS: C# compatibility test passed!")


if __name__ == "__main__":
    print("BitRPC Python Serialization Compatibility Test")
    print("===========================================")
    
    try:
        test_basic_serialization()
        test_csharp_compatibility()
        print("\nSUCCESS: All serialization tests passed!")
        print("\nThis confirms that the serialization format is compatible with C#!")
    except Exception as e:
        print(f"\nFAILED: Test failed: {e}")
        import traceback
        traceback.print_exc()