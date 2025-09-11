"""
Default type handlers for BitRPC Python serialization
"""

from typing import Any
from . import TypeHandler, StreamWriter, StreamReader


class Int32Handler(TypeHandler):
    """Handler for 32-bit integers"""
    
    @property
    def hash_code(self) -> int:
        return hash('int32')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_int32(int(obj))
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_int32()


class Int64Handler(TypeHandler):
    """Handler for 64-bit integers"""
    
    @property
    def hash_code(self) -> int:
        return hash('int64')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_int64(int(obj))
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_int64()


class FloatHandler(TypeHandler):
    """Handler for 32-bit floats"""
    
    @property
    def hash_code(self) -> int:
        return hash('float')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_float(float(obj))
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_float()


class DoubleHandler(TypeHandler):
    """Handler for 64-bit doubles"""
    
    @property
    def hash_code(self) -> int:
        return hash('double')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_double(float(obj))
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_double()


class BoolHandler(TypeHandler):
    """Handler for booleans"""
    
    @property
    def hash_code(self) -> int:
        return hash('bool')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_bool(bool(obj))
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_bool()


class StringHandler(TypeHandler):
    """Handler for strings"""
    
    @property
    def hash_code(self) -> int:
        return hash('string')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_string(str(obj) if obj is not None else "")
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_string()


class BytesHandler(TypeHandler):
    """Handler for bytes"""
    
    @property
    def hash_code(self) -> int:
        return hash('bytes')
    
    def write(self, obj: Any, writer: StreamWriter) -> None:
        writer.write_bytes(obj if obj is not None else b"")
    
    def read(self, reader: StreamReader) -> Any:
        return reader.read_bytes()