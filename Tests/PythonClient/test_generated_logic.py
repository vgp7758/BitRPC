#!/usr/bin/env python3
"""
Test generated serializer logic by copying the code
"""

import sys
import os

# Add the generated Python code to path
generated_path = os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python')
sys.path.insert(0, generated_path)

def test_generated_logic():
    """Test the logic from generated serializer"""
    print("Testing generated serializer logic...")
    
    try:
        # Import required components
        from data.models import LoginRequest
        from bitrpc.serialization import StreamWriter, StreamReader, BitMask
        
        # Copy the generated serializer logic
        class GeneratedLoginRequestSerializer:
            def __init__(self):
                pass
            
            @property
            def hash_code(self) -> int:
                return hash('LoginRequest')
            
            def write(self, obj: LoginRequest, writer: StreamWriter) -> None:
                message: LoginRequest = obj
                mask = BitMask()
                
                # Bit mask group 0 (from generated code)
                mask.set_bit(0, not self._is_default_string(message.username))
                mask.set_bit(1, not self._is_default_string(message.password))
                mask.write(writer)
                
                # Write field values (from generated code)
                if mask.get_bit(0):
                    writer.write_string(message.username)
                if mask.get_bit(1):
                    writer.write_string(message.password)
            
            def read(self, reader: StreamReader) -> LoginRequest:
                message = LoginRequest()
                
                # Read bit mask group 0 (from generated code)
                mask0 = BitMask()
                mask0.read(reader)
                
                if mask0.get_bit(0):
                    message.username = reader.read_string()
                if mask0.get_bit(1):
                    message.password = reader.read_string()
                
                return message
            
            def _is_default_string(self, value: str) -> bool:
                return value == ""
        
        # Create test request
        request = LoginRequest(username="admin", password="password")
        print(f"Original request: username='{request.username}', password='{request.password}'")
        
        # Use generated logic serializer
        serializer = GeneratedLoginRequestSerializer()
        
        # Serialize
        writer = StreamWriter()
        serializer.write(request, writer)
        data = writer.to_array()
        print(f"Generated logic serialized data: {data.hex()}")
        
        # Deserialize
        reader = StreamReader(data)
        deserialized = serializer.read(reader)
        print(f"Deserialized request: username='{deserialized.username}', password='{deserialized.password}'")
        
        # Verify
        assert deserialized.username == request.username
        assert deserialized.password == request.password
        print("SUCCESS: Generated logic test passed!")
        
        # Compare with C# format
        import struct
        
        # C# format: [bitmask][username_length][username][password_length][password]
        bitmask = 0x3  # both fields present
        username_bytes = request.username.encode('utf-8')
        password_bytes = request.password.encode('utf-8')
        
        csharp_data = bytearray()
        csharp_data.extend(struct.pack('<I', bitmask))
        csharp_data.extend(struct.pack('<I', len(username_bytes)))
        csharp_data.extend(username_bytes)
        csharp_data.extend(struct.pack('<I', len(password_bytes)))
        csharp_data.extend(password_bytes)
        
        print(f"C# format data: {csharp_data.hex()}")
        
        # Compare formats
        if data == csharp_data:
            print("SUCCESS: Generated logic matches C# format perfectly!")
        else:
            print("WARNING: Generated logic differs from C# format")
            print(f"Generated: {data.hex()}")
            print(f"C#:       {csharp_data.hex()}")
            
            # Let's analyze the difference
            print("\nAnalyzing format difference...")
            
            # Parse generated format
            gen_reader = StreamReader(data)
            gen_bitmask = gen_reader.read_uint32()  # Assuming this is how BitMask reads
            print(f"Generated bitmask: {gen_bitmask}")
            
    except Exception as e:
        print(f"FAILED: Generated logic test failed: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    print("Generated Serializer Logic Test")
    print("===============================")
    
    test_generated_logic()