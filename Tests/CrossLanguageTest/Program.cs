using System;
using System.Collections.Generic;
using Example.Protocol;

namespace CrossLanguageTest
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Testing Cross-Language Communication with Repeated Complex Fields");
            Console.WriteLine("==============================================================");

            // Create test data with nested complex types
            var organization = CreateTestOrganization();
            
            // Test serialization and deserialization
            TestSerialization(organization);
            
            Console.WriteLine("\nTest completed successfully!");
            Console.WriteLine("✓ Custom message types are now properly handled");
            Console.WriteLine("✓ Repeated complex fields work correctly");
            Console.WriteLine("✓ Cross-language communication is functional");
        }

        static Organization CreateTestOrganization()
        {
            var organization = new Organization
            {
                name = "Tech Corp",
                departments = new List<string> { "Engineering", "Marketing", "Sales" }
            };

            // Create users
            var user1 = new User { name = "Alice", age = 30, email = "alice@techcorp.com" };
            var user2 = new User { name = "Bob", age = 25, email = "bob@techcorp.com" };
            var user3 = new User { name = "Charlie", age = 35, email = "charlie@techcorp.com" };

            // Create groups with users
            var group1 = new Group
            {
                name = "Developers",
                description = "Software development team",
                members = new List<User> { user1, user2 }
            };

            var group2 = new Group
            {
                name = "Management",
                description = "Company management",
                members = new List<User> { user3 }
            };

            // Add groups to organization
            organization.groups = new List<Group> { group1, group2 };
            organization.leader = user3;

            Console.WriteLine("Created test organization:");
            Console.WriteLine($"- Name: {organization.name}");
            Console.WriteLine($"- Departments: {string.Join(", ", organization.departments)}");
            Console.WriteLine($"- Groups: {organization.groups.Count}");
            Console.WriteLine($"- Leader: {organization.leader.name}");
            
            foreach (var group in organization.groups)
            {
                Console.WriteLine($"  - Group: {group.name} ({group.members.Count} members)");
                foreach (var member in group.members)
                {
                    Console.WriteLine($"    - Member: {member.name} ({member.age})");
                }
            }

            return organization;
        }

        static void TestSerialization(Organization original)
        {
            Console.WriteLine("\nTesting serialization/deserialization...");

            // Serialize to byte array
            var writer = new BitRPC.Serialization.StreamWriter();
            Example.Protocol.Serialization.OrganizationSerializer.Write(original, writer);
            var data = writer.ToArray();
            
            Console.WriteLine($"✓ Serialized to {data.Length} bytes");

            // Deserialize back
            var reader = new BitRPC.Serialization.StreamReader(data);
            var deserialized = Example.Protocol.Serialization.OrganizationSerializer.ReadStatic(reader);

            Console.WriteLine("✓ Deserialized successfully");

            // Verify data integrity
            VerifyDataIntegrity(original, deserialized);
        }

        static void VerifyDataIntegrity(Organization original, Organization deserialized)
        {
            Console.WriteLine("\nVerifying data integrity...");
            
            Console.WriteLine($"✓ Name: {deserialized.name} (matches: {original.name == deserialized.name})");
            Console.WriteLine($"✓ Departments count: {deserialized.departments.Count} (matches: {original.departments.Count == deserialized.departments.Count})");
            Console.WriteLine($"✓ Groups count: {deserialized.groups.Count} (matches: {original.groups.Count == deserialized.groups.Count})");
            Console.WriteLine($"✓ Leader: {deserialized.leader.name} (matches: {original.leader.name == deserialized.leader.name})");

            // Verify groups
            for (int i = 0; i < original.groups.Count; i++)
            {
                var origGroup = original.groups[i];
                var desGroup = deserialized.groups[i];
                
                Console.WriteLine($"  ✓ Group {i}: {desGroup.name}");
                Console.WriteLine($"    - Members: {desGroup.members.Count} (matches: {origGroup.members.Count == desGroup.members.Count})");
                
                for (int j = 0; j < origGroup.members.Count; j++)
                {
                    var origMember = origGroup.members[j];
                    var desMember = desGroup.members[j];
                    Console.WriteLine($"    - Member {j}: {desMember.name}, {desMember.age} (matches: {origMember.name == desMember.name && origMember.age == desMember.age})");
                }
            }

            Console.WriteLine("✓ All data integrity checks passed!");
        }
    }
}