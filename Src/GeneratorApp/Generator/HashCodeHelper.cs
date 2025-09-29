using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Security.Cryptography;

namespace BitRPC.Protocol.Generator
{
    public static class HashCodeHelper
    {
        /// <summary>
        /// 计算类型名称的统一32位哈希码
        /// 使用 FNV-1a 算法确保跨语言一致性
        /// </summary>
        public static int ComputeHashCode(string typeName)
        {
            // FNV-1a hash algorithm
            const uint fnvPrime = 16777619;
            const uint fnvOffsetBasis = 2166136261;

            uint hash = fnvOffsetBasis;

            // 使用 UTF-8 字节序列确保跨语言一致性
            byte[] bytes = Encoding.UTF8.GetBytes(typeName);

            foreach (byte b in bytes)
            {
                hash ^= b;
                hash *= fnvPrime;
            }

            return (int)(hash & 0x7FFFFFFF); // 确保是正32位整数
        }

        /// <summary>
        /// 获取预计算的常见类型哈希码
        /// </summary>
        public static Dictionary<string, int> GetPrecomputedHashCodes()
        {
            return new Dictionary<string, int>
            {
                { "Vector3", ComputeHashCode("Vector3") },
                { "Rotation3", ComputeHashCode("Rotation3") },
                { "Scale3", ComputeHashCode("Scale3") },
                { "Transform", ComputeHashCode("Transform") },
                { "SceneObject", ComputeHashCode("SceneObject") },
                { "SceneUpdateRequest", ComputeHashCode("SceneUpdateRequest") },
                { "SceneUpdateResponse", ComputeHashCode("SceneUpdateResponse") },
                { "GetSceneStateRequest", ComputeHashCode("GetSceneStateRequest") },
                { "GetSceneStateResponse", ComputeHashCode("GetSceneStateResponse") },
                { "TransformEvent", ComputeHashCode("TransformEvent") },
                { "BatchTransformEvents", ComputeHashCode("BatchTransformEvents") },
                { "EventAckResponse", ComputeHashCode("EventAckResponse") }
            };
        }
    }
}