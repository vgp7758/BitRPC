"""
跨语言共享内存测试 - Python示例
测试与C++和C#的跨语言通信
"""

import sys
import time
import threading
import struct
import argparse
from typing import Optional, Dict, Any
from dataclasses import dataclass

# 导入共享内存API
try:
    from shared_memory_api import (
        SharedMemoryFactory, SharedMemoryProducer, SharedMemoryConsumer,
        SharedMemoryMessage, SharedMemoryManager, MessageType, MessageFlags,
        create_producer, create_consumer, create_manager
    )
except ImportError:
    print("无法导入共享内存API，请确保库已正确安装")
    sys.exit(1)

# 测试数据结构
@dataclass
class TestData:
    id: int = 0
    value: float = 0.0
    message: str = ""
    timestamp: int = 0

    def pack(self) -> bytes:
        """将结构打包为字节"""
        # 打包格式: i (int), d (double), 64s (64字节字符串), Q (ulong)
        message_bytes = self.message.encode('utf-8').ljust(64, b'\x00')
        return struct.pack('<id64sQ', self.id, self.value, message_bytes, self.timestamp)

    @classmethod
    def unpack(cls, data: bytes) -> 'TestData':
        """从字节解包结构"""
        id_val, value, message_bytes, timestamp = struct.unpack('<id64sQ', data)
        message = message_bytes.decode('utf-8').rstrip('\x00')
        return cls(id=id_val, value=value, message=message, timestamp=timestamp)

class CrossLanguageTestProducer:
    def __init__(self, name: str = "CrossLangTest"):
        self.name = name
        self.running = False
        self.producer: Optional[SharedMemoryProducer] = None
        self.send_thread: Optional[threading.Thread] = None

    def start(self) -> bool:
        """启动生产者"""
        if self.running:
            return True

        try:
            # 创建生产者
            self.producer = create_producer(self.name, 1024 * 1024)  # 1MB buffer
            if not self.producer:
                print("✗ Failed to create producer")
                return False

            print(f"✓ Python Producer connected to shared memory: {self.name}")
            self.running = True

            # 启动发送线程
            self.send_thread = threading.Thread(target=self.send_loop)
            self.send_thread.daemon = True
            self.send_thread.start()

            return True

        except Exception as e:
            print(f"✗ Error starting producer: {e}")
            return False

    def stop(self):
        """停止生产者"""
        if not self.running:
            return

        self.running = False

        if self.send_thread and self.send_thread.is_alive():
            self.send_thread.join(timeout=5)

        if self.producer:
            self.producer.dispose()
            self.producer = None

        print("✓ Python Producer stopped")

    def is_running(self) -> bool:
        return self.running

    def send_loop(self):
        """发送循环"""
        counter = 0

        while self.running:
            try:
                # 发送文本消息
                text_message = f"Hello from Python! Message #{counter}"
                if self.producer.send_string(text_message):
                    print(f"Sent text: {text_message}")

                time.sleep(0.1)

                # 发送结构化数据
                data = TestData(
                    id=counter,
                    value=1.41421 * counter,
                    message=f"Python Data #{counter}",
                    timestamp=int(time.time() * 1000)
                )

                message = SharedMemoryMessage(MessageType.DATA, data.pack())
                message.set_flag(MessageFlags.URGENT)

                if self.producer.send_message(message):
                    print(f"Sent structured data: id={data.id}, value={data.value}")

                # 发送心跳
                if counter % 10 == 0:
                    if self.producer.send_message_data(MessageType.HEARTBEAT, b''):
                        print("Sent heartbeat")

                counter += 1

                # 等待一段时间
                time.sleep(0.5)

            except Exception as e:
                print(f"Error in send loop: {e}")
                time.sleep(1)

class CrossLanguageTestConsumer:
    def __init__(self, name: str = "CrossLangTest"):
        self.name = name
        self.running = False
        self.consumer: Optional[SharedMemoryConsumer] = None
        self.receive_thread: Optional[threading.Thread] = None
        self.message_handlers: Dict[MessageType, callable] = {}

    def start(self) -> bool:
        """启动消费者"""
        if self.running:
            return True

        try:
            # 创建消费者
            self.consumer = create_consumer(self.name, 1024 * 1024)  # 1MB buffer
            if not self.consumer:
                print("✗ Failed to create consumer")
                return False

            # 注册消息处理器
            self.register_message_handler(MessageType.DATA, self.handle_data_message)
            self.register_message_handler(MessageType.HEARTBEAT, self.handle_heartbeat_message)
            self.register_message_handler(MessageType.CONTROL, self.handle_control_message)

            print(f"✓ Python Consumer connected to shared memory: {self.name}")
            self.running = True

            # 启动接收线程
            self.receive_thread = threading.Thread(target=self.receive_loop)
            self.receive_thread.daemon = True
            self.receive_thread.start()

            return True

        except Exception as e:
            print(f"✗ Error starting consumer: {e}")
            return False

    def stop(self):
        """停止消费者"""
        if not self.running:
            return

        self.running = False

        if self.receive_thread and self.receive_thread.is_alive():
            self.receive_thread.join(timeout=5)

        if self.consumer:
            self.consumer.dispose()
            self.consumer = None

        print("✓ Python Consumer stopped")

    def is_running(self) -> bool:
        return self.running

    def register_message_handler(self, message_type: MessageType, handler: callable):
        """注册消息处理器"""
        self.message_handlers[message_type] = handler

    def handle_data_message(self, message: SharedMemoryMessage) -> bool:
        """处理数据消息"""
        print(f"Received data message: {message.payload_size} bytes")

        # 尝试解析为TestData结构
        if message.payload_size == struct.calcsize('<id64sQ'):
            try:
                data = TestData.unpack(message.payload)
                print(f"  Parsed data: id={data.id}, value={data.value}, "
                      f"message='{data.message}', timestamp={data.timestamp}")
            except Exception as e:
                print(f"  Failed to parse TestData: {e}")

        return True

    def handle_heartbeat_message(self, message: SharedMemoryMessage) -> bool:
        """处理心跳消息"""
        print("Received heartbeat from producer")
        return True

    def handle_control_message(self, message: SharedMemoryMessage) -> bool:
        """处理控制消息"""
        try:
            text = message.payload.decode('utf-8')
            print(f"Received control message: {text}")
        except:
            print("Received control message with non-UTF8 payload")
        return True

    def receive_loop(self):
        """接收循环"""
        while self.running:
            try:
                # 尝试接收消息
                message = self.consumer.receive_message(100)  # 100ms timeout
                if message:
                    handler = self.message_handlers.get(message.message_type)
                    if handler:
                        handler(message)
                    else:
                        print(f"Received unhandled message type: {message.message_type}")
                    continue

                # 尝试接收字符串
                text = self.consumer.receive_string(100)
                if text is not None:
                    print(f"Received string: {text}")
                    continue

                # 尝试接收原始数据
                data = self.consumer.receive(100)
                if data is not None:
                    print(f"Received raw data: {len(data)} bytes")
                    continue

                # 如果没有数据，短暂休眠
                time.sleep(0.05)

            except Exception as e:
                print(f"Error in receive loop: {e}")
                time.sleep(1)

def main():
    """主函数"""
    print("BitRPC Cross-Language Shared Memory Test (Python)")
    print("===============================================")

    parser = argparse.ArgumentParser(description='Cross-language shared memory test')
    parser.add_argument('--mode', choices=['producer', 'consumer'], default='producer',
                       help='Set mode (producer/consumer)')
    parser.add_argument('--name', default='CrossLangTest',
                       help='Set shared memory name')
    parser.add_argument('--help', '-h', action='store_true',
                       help='Show this help message')

    args = parser.parse_args()

    if args.help:
        parser.print_help()
        return

    print(f"Mode: {args.mode}")
    print(f"Shared Memory Name: {args.name}")
    print()

    try:
        if args.mode == 'producer':
            producer = CrossLanguageTestProducer(args.name)
            if not producer.start():
                print("Failed to start producer")
                sys.exit(1)

            print("Producer running. Press Ctrl+C to stop...")

            # 等待用户中断
            try:
                while producer.is_running():
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\nReceived interrupt signal, stopping...")
            finally:
                producer.stop()

        elif args.mode == 'consumer':
            consumer = CrossLanguageTestConsumer(args.name)
            if not consumer.start():
                print("Failed to start consumer")
                sys.exit(1)

            print("Consumer running. Press Ctrl+C to stop...")

            # 等待用户中断
            try:
                while consumer.is_running():
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\nReceived interrupt signal, stopping...")
            finally:
                consumer.stop()

        else:
            print(f"Invalid mode: {args.mode}")
            print("Supported modes: producer, consumer")
            sys.exit(1)

    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

    print("Test completed successfully!")

if __name__ == "__main__":
    main()