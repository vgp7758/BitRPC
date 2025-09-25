using System;
using System.Threading.Tasks;
using BitRPC.Client;
using Test.Protocol.Client;
using Test.Protocol;
using Test.Protocol.Serialization; // 初始化序列化注册

namespace BitRPC.Demo.Client
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("=== BitRPC C# Test Client ===");

            try
            {
                // 初始化协议序列化
                Test.Protocol.Factory.ProtocolFactory.Initialize();

                // 创建 TCP 客户端
                var tcpClient = new BitRPC.Client.TcpRpcClient("localhost", 8080);
                await tcpClient.ConnectAsync();

                // 创建服务客户端
                var testClient = new TestServiceClient(tcpClient);

                Console.WriteLine("Connected to server. Starting tests...\n");

                // 1. Echo test
                Console.WriteLine("1. Testing Echo method...");
                try
                {
                    var echoRequest = new EchoRequest
                    {
                        message = "Hello from C# client!",
                        timestamp = DateTime.Now.Ticks
                    };

                    var echoResponse = await testClient.EchoAsync(echoRequest);
                    Console.WriteLine($"   Sent: {echoRequest.message}");
                    Console.WriteLine($"   Received: {echoResponse.message}");
                    Console.WriteLine($"   Server time: {echoResponse.server_time}");
                    Console.WriteLine("   ✓ Echo test passed\n");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"   ✗ Echo test failed: {ex.Message}\n");
                }

                // 2. Login 有效凭据
                Console.WriteLine("2. Testing Login with valid credentials...");
                try
                {
                    var loginRequest = new LoginRequest
                    {
                        username = "admin",
                        password = "admin123"
                    };

                    var loginResponse = await testClient.LoginAsync(loginRequest);
                    Console.WriteLine($"   Success: {loginResponse.success}");
                    Console.WriteLine($"   User ID: {loginResponse.user?.user_id}");
                    Console.WriteLine($"   Username: {loginResponse.user?.username}");
                    Console.WriteLine($"   Token: {loginResponse.token}");
                    Console.WriteLine("   ✓ Login test passed\n");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"   ✗ Login test failed: {ex.Message}\n");
                }

                // 3. Login 无效凭据
                Console.WriteLine("3. Testing Login with invalid credentials...");
                try
                {
                    var invalidLoginRequest = new LoginRequest
                    {
                        username = "invalid",
                        password = "wrong"
                    };

                    var invalidLoginResponse = await testClient.LoginAsync(invalidLoginRequest);
                    Console.WriteLine($"   Success: {invalidLoginResponse.success}");
                    Console.WriteLine($"   Error: {invalidLoginResponse.error_message}");
                    Console.WriteLine("   ✓ Invalid login test passed\n");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"   ✗ Invalid login test failed: {ex.Message}\n");
                }

                // 4. GetUser
                Console.WriteLine("4. Testing GetUser method...");
                try
                {
                    var getUserRequest = new GetUserRequest { user_id = 1 };
                    var getUserResponse = await testClient.GetUserAsync(getUserRequest);
                    Console.WriteLine($"   Found: {getUserResponse.found}");
                    if (getUserResponse.found && getUserResponse.user != null)
                    {
                        Console.WriteLine($"   User ID: {getUserResponse.user.user_id}");
                        Console.WriteLine($"   Username: {getUserResponse.user.username}");
                        Console.WriteLine($"   Email: {getUserResponse.user.email}");
                        Console.WriteLine($"   Roles: {string.Join(", ", getUserResponse.user.roles)}");
                        Console.WriteLine($"   Active: {getUserResponse.user.is_active}");
                    }
                    Console.WriteLine("   ✓ GetUser test passed\n");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"   ✗ GetUser test failed: {ex.Message}\n");
                }

                Console.WriteLine("=== All tests completed ===");
                Console.WriteLine("\nPress any key to exit...");
                Console.ReadKey();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Client error: {ex.Message}");
                Console.WriteLine("Press any key to exit...");
                Console.ReadKey();
            }
        }
    }
}