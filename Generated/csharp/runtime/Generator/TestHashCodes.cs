using System;
using System.Collections.Generic;

namespace BitRPC.Protocol.Generator
{
    class TestHashCodes
    {
        static void Main(string[] args)
        {
            Console.WriteLine("BitRPC Hash Code Test");
            Console.WriteLine("=====================");

            var types = new List<string>
            {
                "Vector3",
                "Rotation3",
                "Scale3",
                "Transform",
                "SceneObject",
                "SceneUpdateRequest",
                "SceneUpdateResponse",
                "GetSceneStateRequest",
                "GetSceneStateResponse",
                "TransformEvent",
                "BatchTransformEvents",
                "EventAckResponse"
            };

            Console.WriteLine("New hash codes (FNV-1a):");
            foreach (var type in types)
            {
                int hash = HashCodeHelper.ComputeHashCode(type);
                Console.WriteLine($"{type,-25} = {hash,10} (0x{hash:X8})");
            }

            Console.WriteLine("\nComparison with C# GetHashCode():");
            foreach (var type in types)
            {
                int oldHash = type.GetHashCode();
                int newHash = HashCodeHelper.ComputeHashCode(type);
                Console.WriteLine($"{type,-25} = Old: {oldHash,10}, New: {newHash,10}");
            }
        }
    }
}