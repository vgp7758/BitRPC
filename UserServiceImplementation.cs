using System;
using System.Threading.Tasks;
using Example.Protocol;
using Example.Protocol.Server;

namespace BitRPC.Test
{
    public class UserServiceImplementation : UserServiceServiceBase
    {
        public override async Task<LoginResponse> LoginAsync(LoginRequest request)
        {
            Console.WriteLine($"Login request received for user: {request.username}");
            
            // Simple authentication logic for demo purposes
            if (request.username == "admin" && request.password == "password")
            {
                var user = new User
                {
                    user_id = 1,
                    username = "admin",
                    email = "admin@example.com",
                    is_active = true,
                    created_at = DateTime.Now
                };
                
                return new LoginResponse
                {
                    success = true,
                    user = user,
                    token = "sample_token_12345"
                };
            }
            else
            {
                return new LoginResponse
                {
                    success = false,
                    error_message = "Invalid username or password"
                };
            }
        }

        public override async Task<GetUserResponse> GetUserAsync(GetUserRequest request)
        {
            Console.WriteLine($"Get user request received for user ID: {request.user_id}");
            
            // Simple user lookup logic for demo purposes
            if (request.user_id == 1)
            {
                var user = new User
                {
                    user_id = 1,
                    username = "admin",
                    email = "admin@example.com",
                    is_active = true,
                    created_at = DateTime.Now
                };
                
                return new GetUserResponse
                {
                    user = user,
                    found = true
                };
            }
            else
            {
                return new GetUserResponse
                {
                    found = false
                };
            }
        }
    }
}