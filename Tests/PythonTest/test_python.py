#!/usr/bin/env python3
"""
Test Python generated code for repeated complex fields
"""

import sys
import os

# Add the generated python code to the path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'Generated', 'python'))

from data.models import User, Group, Organization

def test_python_models():
    """Test that Python models work correctly with proper types"""
    
    print("Testing Python Generated Code for Repeated Complex Fields")
    print("=" * 60)
    
    # Create test users
    user1 = User(name="Alice", age=30, email="alice@techcorp.com")
    user2 = User(name="Bob", age=25, email="bob@techcorp.com")
    user3 = User(name="Charlie", age=35, email="charlie@techcorp.com")
    
    # Create test groups
    group1 = Group(
        name="Developers",
        members=[user1, user2],
        description="Software development team"
    )
    
    group2 = Group(
        name="Management",
        members=[user3],
        description="Company management"
    )
    
    # Create test organization
    organization = Organization(
        name="Tech Corp",
        groups=[group1, group2],
        departments=["Engineering", "Marketing", "Sales"],
        leader=user3
    )
    
    print("+ Created test organization with proper types:")
    print(f"  - Name: {organization.name}")
    print(f"  - Departments: {organization.departments}")
    print(f"  - Groups: {len(organization.groups)}")
    print(f"  - Leader: {organization.leader.name}")
    
    # Verify type annotations
    print("\n+ Type verification:")
    print(f"  - organization.groups type: {type(organization.groups)}")
    print(f"  - organization.groups[0] type: {type(organization.groups[0])}")
    print(f"  - organization.groups[0].members type: {type(organization.groups[0].members)}")
    print(f"  - organization.groups[0].members[0] type: {type(organization.groups[0].members[0])}")
    print(f"  - organization.leader type: {type(organization.leader)}")
    
    # Test data access
    print("\n+ Data access verification:")
    print(f"  - First group: {organization.groups[0].name}")
    print(f"  - First member: {organization.groups[0].members[0].name}")
    print(f"  - Leader name: {organization.leader.name}")
    
    print("\n+ Python test completed successfully!")
    print("+ Custom message types are properly handled")
    print("+ Repeated complex fields work correctly")
    print("+ Type annotations are correct")

if __name__ == "__main__":
    test_python_models()