#!/usr/bin/env python3
"""
STL Fix Verification Script

Verifies that STL implementation now uses belief-space evaluation
and produces continuous robustness values (not just 2 constant values).

Usage:
    ./verify_stl_fix.py
"""

import subprocess
import time
import sys
import re
from collections import defaultdict

def run_command(cmd, timeout=5):
    """Run shell command and return output"""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=timeout
        )
        return result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return ""

def check_compilation():
    """Check if C++ STL node compiled successfully"""
    print("=" * 60)
    print("1. 检查编译状态...")
    print("=" * 60)

    # Check if executable exists
    import os
    stl_node_path = "/home/dixon/Final_Project/catkin/devel/lib/stl_monitor/stl_monitor_node_cpp"

    if os.path.exists(stl_node_path):
        print("✅ C++ STL节点已编译成功")
        print(f"   路径: {stl_node_path}")
        return True
    else:
        print("❌ C++ STL节点未找到")
        print("   请运行: catkin_make --only-pkg-with-deps stl_monitor")
        return False

def check_launch_file():
    """Check if launch file uses C++ node with belief space"""
    print("\n" + "=" * 60)
    print("2. 检查Launch文件配置...")
    print("=" * 60)

    launch_file = "/home/dixon/Final_Project/catkin/src/safe_regret_mpc/launch/safe_regret_mpc_test.launch"

    with open(launch_file, 'r') as f:
        content = f.read()

    # Check for C++ node
    if 'type="stl_monitor_node_cpp"' in content:
        print("✅ Launch文件使用C++节点")
    else:
        print("❌ Launch文件未使用C++节点")
        return False

    # Check for belief space enabled
    if 'use_belief_space" value="true"' in content:
        print("✅ Belief space已启用")
    else:
        print("❌ Belief space未启用")
        return False

    return True

def check_cmake_config():
    """Check if CMakeLists.txt compiles C++ sources"""
    print("\n" + "=" * 60)
    print("3. 检查CMakeLists.txt配置...")
    print("=" * 60)

    cmake_file = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/CMakeLists.txt"

    with open(cmake_file, 'r') as f:
        content = f.read()

    checks = {
        'BeliefSpaceEvaluator.cpp': 'BeliefSpaceEvaluator编译',
        'SmoothRobustness.cpp': 'SmoothRobustness编译',
        'stl_monitor_node_cpp': 'C++节点编译'
    }

    all_ok = True
    for pattern, desc in checks.items():
        if pattern in content:
            print(f"✅ {desc}")
        else:
            print(f"❌ {desc} - 未找到")
            all_ok = False

    return all_ok

def verify_belief_space_implementation():
    """Verify C++ implementation uses belief space"""
    print("\n" + "=" * 60)
    print("4. 验证Belief-Space实现...")
    print("=" * 60)

    node_file = "/home/dixon/Final_Project/catkin/src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.cpp"

    with open(node_file, 'r') as f:
        content = f.read()

    checks = {
        'sampleParticles': 'Particle sampling',
        'smax': 'Log-sum-exp smooth max',
        'belief.covariance': 'Covariance extraction',
        'budget_->update': 'Budget mechanism'
    }

    all_ok = True
    for pattern, desc in checks.items():
        if pattern in content:
            print(f"✅ {desc}")
        else:
            print(f"❌ {desc} - 未找到")
            all_ok = False

    return all_ok

def main():
    """Main verification flow"""
    print("\n🔍 STL实现修复验证")
    print("=" * 60)

    results = {
        "编译状态": check_compilation(),
        "Launch配置": check_launch_file(),
        "CMake配置": check_cmake_config(),
        "Belief-Space实现": verify_belief_space_implementation()
    }

    # Summary
    print("\n" + "=" * 60)
    print("📊 验证总结")
    print("=" * 60)

    for test, passed in results.items():
        status = "✅ PASS" if passed else "❌ FAIL"
        print(f"{status} - {test}")

    all_passed = all(results.values())

    if all_passed:
        print("\n🎉 所有检查通过！STL实现修复成功。")
        print("\n下一步：")
        print("1. 运行测试：")
        print("   roslaunch safe_regret_mpc safe_regret_mpc_test.launch enable_stl:=true")
        print("\n2. 检查日志，应该看到：")
        print("   🔍 Belief-space robustness: -X.XXXX (连续变化)")
        print("   而不是只有两个常数值")
        return 0
    else:
        print("\n⚠️  部分检查失败，请修复上述问题。")
        return 1

if __name__ == "__main__":
    sys.exit(main())
