#!/usr/bin/env python3
"""
Manual validation of C++ generated code for cross-language communication
"""

def validate_cpp_code():
    """Validate that C++ generated code has correct types and structure"""
    
    print("Manual C++ Code Validation")
    print("=" * 30)
    
    # Read header file
    with open("D:/Projects/BitRPC/BitRPC/Generated/cpp/include/models.h", "r") as f:
        header_content = f.read()
    
    # Read implementation file
    with open("D:/Projects/BitRPC/BitRPC/Generated/cpp/src/models.cpp", "r") as f:
        impl_content = f.read()
    
    # Check for proper type usage
    checks = [
        ("User struct definition", "struct User {"),
        ("Group struct definition", "struct Group {"),
        ("Organization struct definition", "struct Organization {"),
        ("User members in Group", "std::vector<User> members;"),
        ("Group groups in Organization", "std::vector<Group> groups;"),
        ("User leader in Organization", "User leader;"),
        ("String departments in Organization", "std::vector<std::string> departments;"),
        ("Proper namespace", "namespace bitrpc {"),
        ("Proper nested namespace", "namespace example::protocol {"),
        ("User constructor", "User::User()"),
        ("Group constructor", "Group::Group()"),
        ("Organization constructor", "Organization::Organization()"),
    ]
    
    all_passed = True
    for check_name, pattern in checks:
        if pattern in header_content or pattern in impl_content:
            print(f"+ {check_name}: Found")
        else:
            print(f"- {check_name}: Missing")
            all_passed = False
    
    # Check for incorrect types that should have been fixed
    bad_patterns = [
        ("void* instead of User", "void* leader;"),
        ("void* instead of User in vector", "std::vector<void*>"),
        ("object instead of User", "object leader;"),
        ("Any instead of User", "Any leader;"),
    ]
    
    print("\nChecking for fixed type issues:")
    for check_name, pattern in bad_patterns:
        if pattern in header_content or pattern in impl_content:
            print(f"- {check_name}: Still present - this is a bug!")
            all_passed = False
        else:
            print(f"+ {check_name}: Correctly fixed")
    
    print(f"\nOverall validation: {'PASSED' if all_passed else 'FAILED'}")
    return all_passed

if __name__ == "__main__":
    validate_cpp_code()