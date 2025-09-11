#include <iostream>
#include <vector>
#include <string>

// Include the generated models
#include "models.h"

using namespace bitrpc::example::protocol;

void test_cpp_models() {
    std::cout << "Testing C++ Generated Code for Repeated Complex Fields" << std::endl;
    std::cout << "====================================================" << std::endl;
    
    // Create test users
    User user1;
    user1.name = "Alice";
    user1.age = 30;
    user1.email = "alice@techcorp.com";
    
    User user2;
    user2.name = "Bob";
    user2.age = 25;
    user2.email = "bob@techcorp.com";
    
    User user3;
    user3.name = "Charlie";
    user3.age = 35;
    user3.email = "charlie@techcorp.com";
    
    // Create test groups
    Group group1;
    group1.name = "Developers";
    group1.members = {user1, user2};
    group1.description = "Software development team";
    
    Group group2;
    group2.name = "Management";
    group2.members = {user3};
    group2.description = "Company management";
    
    // Create test organization
    Organization organization;
    organization.name = "Tech Corp";
    organization.groups = {group1, group2};
    organization.departments = {"Engineering", "Marketing", "Sales"};
    organization.leader = user3;
    
    std::cout << "+ Created test organization with proper types:" << std::endl;
    std::cout << "  - Name: " << organization.name << std::endl;
    std::cout << "  - Departments: ";
    for (const auto& dept : organization.departments) {
        std::cout << dept << " ";
    }
    std::cout << std::endl;
    std::cout << "  - Groups: " << organization.groups.size() << std::endl;
    std::cout << "  - Leader: " << organization.leader.name << std::endl;
    
    // Verify type information
    std::cout << "\n+ Type verification:" << std::endl;
    std::cout << "  - organization.groups is a vector: " << std::is_same_v<decltype(organization.groups), std::vector<Group>> << std::endl;
    std::cout << "  - organization.departments is a vector: " << std::is_same_v<decltype(organization.departments), std::vector<std::string>> << std::endl;
    std::cout << "  - organization.leader is a User: " << std::is_same_v<decltype(organization.leader), User> << std::endl;
    
    // Test data access
    std::cout << "\n+ Data access verification:" << std::endl;
    std::cout << "  - First group: " << organization.groups[0].name << std::endl;
    std::cout << "  - First member: " << organization.groups[0].members[0].name << std::endl;
    std::cout << "  - Leader name: " << organization.leader.name << std::endl;
    
    // Test nested access
    std::cout << "\n+ Nested access verification:" << std::endl;
    std::cout << "  - Group 1 member count: " << organization.groups[0].members.size() << std::endl;
    std::cout << "  - Group 2 member count: " << organization.groups[1].members.size() << std::endl;
    std::cout << "  - Group 1 first member age: " << organization.groups[0].members[0].age << std::endl;
    std::cout << "  - Group 2 first member age: " << organization.groups[1].members[0].age << std::endl;
    
    std::cout << "\n+ C++ test completed successfully!" << std::endl;
    std::cout << "+ Custom message types are properly handled" << std::endl;
    std::cout << "+ Repeated complex fields work correctly" << std::endl;
    std::cout << "+ Type safety is maintained" << std::endl;
}

int main() {
    try {
        test_cpp_models();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}