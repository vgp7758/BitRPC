using System;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using Example.Protocol;
using Example.Protocol.Server;
using BitRPC.Server;
using BitRPC.Serialization;

namespace TestServer
{
    public class UserServiceImplementation : UserServiceServiceBase
    {
        private readonly Dictionary<long, User> _users = new Dictionary<long, User>();

        public UserServiceImplementation()
        {
            // Add some sample users
            _users[1] = new User
            {
                user_id = 1,
                username = "john_doe",
                email = "john@example.com",
                roles = new List<string> { "user", "admin" },
                is_active = true,
                created_at = DateTime.Now
            };

            _users[2] = new User
            {
                user_id = 2,
                username = "jane_smith",
                email = "jane@example.com",
                roles = new List<string> { "user" },
                is_active = true,
                created_at = DateTime.Now
            };
        }

        public override Task<LoginResponse> LoginAsync(LoginRequest request)
        {
            Console.WriteLine($"Login request received for user: {request.username}");
            
            var response = new LoginResponse
            {
                success = false,
                error_message = "Invalid credentials"
            };

            // Simple authentication logic
            if (request.username == "admin" && request.password == "password")
            {
                response.success = true;
                response.user = _users[1];
                response.token = "fake-jwt-token-12345";
                response.error_message = "";
            }
            else if (request.username == "user" && request.password == "password")
            {
                response.success = true;
                response.user = _users[2];
                response.token = "fake-jwt-token-67890";
                response.error_message = "";
            }

            Console.WriteLine($"Login response: success={response.success}");
            return Task.FromResult(response);
        }

        public override Task<GetUserResponse> GetUserAsync(GetUserRequest request)
        {
            Console.WriteLine($"GetUser request received for user_id: {request.user_id}");
            
            var response = new GetUserResponse
            {
                found = false
            };

            if (_users.TryGetValue(request.user_id, out var user))
            {
                response.found = true;
                response.user = user;
                Console.WriteLine($"User found: {user.username}");
            }
            else
            {
                Console.WriteLine($"User not found: {request.user_id}");
            }

            return Task.FromResult(response);
        }
    }

    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("BitRPC Test Server");
            Console.WriteLine("===================");

            // Initialize the protocol
            Example.Protocol.Factory.ProtocolFactory.Initialize();

            // Create and start the server
            var server = new RpcServer(8080);
            server.RegisterService("UserService", new UserServiceImplementation());

            Console.WriteLine("Starting server on port 8080...");
            server.Start();

            Console.WriteLine("Server is running. Press Ctrl+C to stop...");
            
            // Keep server running until cancelled
            var cts = new CancellationTokenSource();
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                cts.Cancel();
            };
            
            try
            {
                await Task.Delay(Timeout.InfiniteTimeSpan, cts.Token);
            }
            catch (TaskCanceledException)
            {
                // Normal cancellation
            }

            Console.WriteLine("Stopping server...");
            server.Stop();
            Console.WriteLine("Server stopped.");
        }
    }
}