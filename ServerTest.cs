using System;
using System.Net;
using System.Net.Sockets;
using System.Threading.Tasks;

namespace BitRPC.Test
{
    class Program
    {
        static async Task Main(string[] args)
        {
            // Create RPC server on port 8080
            var server = new BitRPC.Server.RpcServer(8080);
            
            // Register our service implementation
            server.RegisterService("UserService", new UserServiceImplementation());
            
            // Start the server
            server.Start();
            
            Console.WriteLine("UserService server started on port 8080. Press any key to stop...");
            
            // Wait for a key press or exit after 60 seconds
            var cts = new CancellationTokenSource();
            
            // Run the server for 60 seconds or until a key is pressed
            try
            {
                await Task.Delay(60000, cts.Token);
            }
            catch (OperationCanceledException)
            {
                // Expected when cancelled
            }
            
            Console.WriteLine("Stopping server...");
            
            // Stop the server
            server.Stop();
        }
    }
}