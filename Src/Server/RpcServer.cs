using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Threading.Tasks;
using BitRPC.Serialization;
using System.Collections.Concurrent;

namespace BitRPC.Server
{
    public abstract class BaseService
    {
        private readonly Dictionary<string, Func<object, object>> _methods;
        private readonly Dictionary<string, Func<object, IAsyncEnumerable<object>>> _serverStreamMethods;
        public abstract string ServiceName { get; }

        protected BaseService()
        {
            _methods = new Dictionary<string, Func<object, object>>();
            _serverStreamMethods = new Dictionary<string, Func<object, IAsyncEnumerable<object>>>();
            RegisterMethods();
        }

        protected abstract void RegisterMethods();

        protected void RegisterMethod<TRequest, TResponse>(string methodName, Func<TRequest, TResponse> method)
        {
            _methods[methodName] = request => method((TRequest)request);
        }

        protected void RegisterMethod<TRequest, TResponse>(string methodName, Func<TRequest, Task<TResponse>> method)
        {
            _methods[methodName] = request => method((TRequest)request).GetAwaiter().GetResult();
        }

        protected void RegisterServerStreamMethod<TRequest, TResponse>(string methodName, Func<TRequest, IAsyncEnumerable<TResponse>> method)
        {
            // Fix for CS1621: Move yield return out of lambda, use a local function
            _serverStreamMethods[methodName] = req => ServerStreamWrapper(method, (TRequest)req);
        }

        // Helper method to wrap the async enumerable and cast items to object
        private async IAsyncEnumerable<object> ServerStreamWrapper<TRequest, TResponse>(Func<TRequest, IAsyncEnumerable<TResponse>> method, TRequest req)
        {
            await foreach (var item in method(req))
            {
                // Fix for CS8600/CS8603: Ensure item is not null before returning
                if (item is not null)
                    yield return item!;
            }
        }

        public object CallMethod(string methodName, object request)
        {
            if (_methods.TryGetValue(methodName, out var method))
            {
                return method(request);
            }
            throw new InvalidOperationException($"Method '{methodName}' not found");
        }

        public bool IsServerStream(string methodName) => _serverStreamMethods.ContainsKey(methodName);

        public IAsyncEnumerable<object> CallServerStream(string methodName, object request)
        {
            if (_serverStreamMethods.TryGetValue(methodName, out var method))
            {
                return method(request);
            }
            throw new InvalidOperationException($"Streaming method '{methodName}' not found");
        }

        public bool HasMethod(string methodName)
        {
            return _methods.ContainsKey(methodName) || _serverStreamMethods.ContainsKey(methodName);
        }
    }

    public class RpcServer
    {
        private readonly int _port;
        private readonly Dictionary<string, BaseService> _services;
        private System.Net.Sockets.TcpListener _listener;
        private bool _isRunning;
        private readonly List<System.Threading.Tasks.Task> _clientTasks;

        public RpcServer(int port)
        {
            _port = port;
            _services = new Dictionary<string, BaseService>();
            _clientTasks = new List<System.Threading.Tasks.Task>();
        }

        public RpcServer RegisterService(BaseService service)
        {
            _services[service.ServiceName] = service;
            return this;
        }

        public void Start()
        {
            _listener = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Any, _port);
            _listener.Start();
            _isRunning = true;

            System.Threading.Tasks.Task.Run(async () =>
            {
                while (_isRunning)
                {
                    var client = await _listener.AcceptTcpClientAsync();
                    var clientTask = HandleClientAsync(client);
                    _clientTasks.Add(clientTask);
                }
            });
        }

        public void Stop()
        {
            _isRunning = false;
            _listener?.Stop();
            
            System.Threading.Tasks.Task.WhenAll(_clientTasks);
        }

        private async System.Threading.Tasks.Task HandleClientAsync(System.Net.Sockets.TcpClient client)
        {
            try
            {
                using (var stream = client.GetStream())
                {
                    while (client.Connected && _isRunning)
                    {
                        var lengthBytes = new byte[4];
                        int read = await stream.ReadAsync(lengthBytes, 0, 4);
                        if (read == 0) break; // connection closed
                        if (read != 4) throw new IOException("Incomplete frame length");
                        var length = BitConverter.ToInt32(lengthBytes, 0);
                        var data = new byte[length];
                        int offset = 0;
                        while (offset < length)
                        {
                            int r = await stream.ReadAsync(data, offset, length - offset);
                            if (r <= 0) throw new IOException("Connection closed");
                            offset += r;
                        }
                        await ProcessRequestAsync(data, stream);
                    }
                }
            }
            catch (Exception)
            {
            }
            finally
            {
                client.Close();
            }
        }

        private async Task ProcessRequestAsync(byte[] data, NetworkStream stream)
        {
            try
            {
                var reader = new BitRPC.Serialization.StreamReader(data);
                var methodName = reader.ReadString();
                var serviceName = ExtractServiceName(methodName);
                var operationName = ExtractOperationName(methodName);

                if (_services.TryGetValue(serviceName, out var service))
                {
                    var request = reader.ReadObject();

                    if (service.IsServerStream(operationName))
                    {
                        await foreach (var item in service.CallServerStream(operationName, request))
                        {
                            var writer = new BitRPC.Serialization.StreamWriter();
                            writer.WriteObject(item);
                            var frame = writer.ToArray();
                            var lenBytes = BitConverter.GetBytes(frame.Length);
                            await stream.WriteAsync(lenBytes, 0, 4);
                            await stream.WriteAsync(frame, 0, frame.Length);
                            await stream.FlushAsync();
                        }
                        // end marker
                        await stream.WriteAsync(BitConverter.GetBytes(0), 0, 4);
                        await stream.FlushAsync();
                        return;
                    }
                    else
                    {
                        var response = service.CallMethod(operationName, request);
                        var writer = new BitRPC.Serialization.StreamWriter();
                        writer.WriteObject(response);
                        var responseBytes = writer.ToArray();
                        var responseLengthBytes = BitConverter.GetBytes(responseBytes.Length);
                        await stream.WriteAsync(responseLengthBytes, 0, responseLengthBytes.Length);
                        await stream.WriteAsync(responseBytes, 0, responseBytes.Length);
                        await stream.FlushAsync();
                        return;
                    }
                }

                throw new InvalidOperationException($"Service '{serviceName}' not found");
            }
            catch (Exception ex)
            {
                var writer = new BitRPC.Serialization.StreamWriter();
                writer.WriteObject(ex);
                var responseBytes = writer.ToArray();
                var responseLengthBytes = BitConverter.GetBytes(responseBytes.Length);
                await stream.WriteAsync(responseLengthBytes, 0, responseLengthBytes.Length);
                await stream.WriteAsync(responseBytes, 0, responseBytes.Length);
                await stream.FlushAsync();
            }
        }

        private string ExtractServiceName(string methodName)
        {
            var dotIndex = methodName.IndexOf('.');
            return dotIndex > 0 ? methodName.Substring(0, dotIndex) : methodName;
        }

        private string ExtractOperationName(string methodName)
        {
            var dotIndex = methodName.IndexOf('.');
            return dotIndex > 0 ? methodName.Substring(dotIndex + 1) : methodName;
        }
    }
}