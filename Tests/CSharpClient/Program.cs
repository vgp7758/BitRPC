using System;
using System.Threading.Tasks;
using Example.Protocol;
using Example.Protocol.Client;
using BitRPC.Client;
using BitRPC.Serialization;

namespace TestClient
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("BitRPC Test Client");
            Console.WriteLine("==================");
            
            // Register serializers
            var serializer = BufferSerializer.Instance;
            Example.Protocol.Serialization.SerializerRegistry.RegisterSerializers(serializer);
            
            // Create client
            var rpcClient = new BitRPC.Client.TcpRpcClient("localhost", 8080);
            var client = new UserServiceClient(rpcClient);
            
            try
            {
                Console.WriteLine("Connecting to server...");
                await client.ConnectAsync();
                Console.WriteLine("Connected successfully!");
                
                // Test login
                Console.WriteLine("\nTesting Login...");
                var loginRequest = new LoginRequest
                {
                    username = "testuser",
                    password = "testpass"
                };
                
                var loginResponse = await client.LoginAsync(loginRequest);
                Console.WriteLine($"Login result: {loginResponse.success}");
                Console.WriteLine($"Token: {loginResponse.token}");
                if (loginResponse.user != null)
                {
                    var user = (User)loginResponse.user;
                    Console.WriteLine($"User ID: {user.user_id}");
                    Console.WriteLine($"Username: {user.username}");
                    Console.WriteLine($"Email: {user.email}");
                    Console.WriteLine($"Roles: {string.Join(", ", user.roles)}");
                    Console.WriteLine($"Active: {user.is_active}");
                }
                if (!string.IsNullOrEmpty(loginResponse.error_message))
                {
                    Console.WriteLine($"Error: {loginResponse.error_message}");
                }
                
                // Test get user
                Console.WriteLine("\nTesting GetUser...");
                var getUserRequest = new GetUserRequest
                {
                    user_id = 123
                };
                
                var getUserResponse = await client.GetUserAsync(getUserRequest);
                Console.WriteLine($"GetUser result: {getUserResponse.found}");
                if (getUserResponse.user != null)
                {
                    var user = (User)getUserResponse.user;
                    Console.WriteLine($"User ID: {user.user_id}");
                    Console.WriteLine($"Username: {user.username}");
                    Console.WriteLine($"Email: {user.email}");
                    Console.WriteLine($"Roles: {string.Join(", ", user.roles)}");
                    Console.WriteLine($"Active: {user.is_active}");
                }
                
                Console.WriteLine("\nAll tests completed successfully!");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
                Console.WriteLine($"Stack trace: {ex.StackTrace}");
            }
            finally
            {
                await rpcClient.DisconnectAsync();
                Console.WriteLine("Disconnected from server.");
            }
        }
    }
}