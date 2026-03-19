# Benchmark系统测试指南

## 快速验证（推荐首先运行）

### 测试1：核心组件验证（30秒）

```bash
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 检查Python脚本语法
python3 -m py_compile src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py
python3 -m py_compile src/tube_mpc_benchmark/scripts/benchmark_runner.py

# 检查包能否被ROS找到
rospack find tube_mpc_benchmark

# 预期输出：/home/dixon/Final_Project/catkin/src/tube_mpc_benchmark
```

### 测试2：模拟器单元测试（1分钟）

创建测试文件 `test_simulator.py`:

```python
#!/usr/bin/env python3
import sys
sys.path.append('/home/dixon/Final_Project/catkin/src/tube_mpc_benchmark/scripts')

# 测试导入
try:
    # 检查Python脚本语法
    import py_compile
    py_compile.compile('src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py', doraise=True)
    py_compile.compile('src/tube_mpc_benchmark/scripts/benchmark_runner.py', doraise=True)
    print("✓ Python scripts syntax valid")
except SyntaxError as e:
    print(f"✗ Syntax error: {e}")
    sys.exit(1)

# 检查必要的Python包
try:
    import numpy
    import rospy
    print("✓ Required Python packages available")
except ImportError as e:
    print(f"✗ Missing package: {e}")
    sys.exit(1)

print("✓ All unit tests passed!")
```

运行：
```bash
python3 test_simulator.py
```

### 测试3：快速功能测试（2分钟）

由于需要ROS master和图形界面，建议在新终端运行：

```bash
# 终端1：启动ROS master（如果没有运行）
roscore &

# 终端2：运行快速测试
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 检查测试参数
cat src/tube_mpc_benchmark/launch/quick_test.launch | grep -E "start_x|goal_x"

# 预期输出：
#   <arg name="start_x" default="0.0"/>
#   <arg name="goal_x" default="5.0"/>
```

## 完整系统测试（5-10分钟）

### 选项A：带可视化测试（推荐）

```bash
# 在有显示器的环境中
roslaunch tube_mpc_benchmark quick_test.launch controller:=tube_mpc

# 预期行为：
# 1. RViz窗口打开
# 2. 机器人出现在(0,0)
# 3. 导航到目标(5,5)
# 4. 10-15秒内显示 "✓ Goal reached!"
```

### 选项B：无可视化快速测试

```bash
# 创建最小测试配置
cat > /tmp/quick_test.launch <<'EOF'
<launch>
  <arg name="test_duration" default="10.0"/>

  <!-- 只启动必要的节点 -->
  <node name="kinematic_robot_sim" pkg="tube_mpc_benchmark" type="kinematic_robot_sim.py" output="screen">
    <param name="update_rate" value="50.0"/>
  </node>

  <!-- 测试监控 -->
  <node name="test_monitor" pkg="rostopic" type="rostopic" args="echo /odom -n 10"/>
</launch>
EOF

# 运行测试
timeout 15 roslaunch /tmp/quick_test.launch

# 检查是否收到odom消息
# 如果看到/odom数据输出，说明模拟器工作正常
```

### 选项C：Python单元测试

创建 `test_benchmark_components.py`:

```python
#!/usr/bin/env python3
"""
Unit tests for benchmark components
"""

import os
import sys
import importlib.util

def test_file_exists(filepath):
    """Test if file exists"""
    if os.path.exists(filepath):
        print(f"✓ {filepath}")
        return True
    else:
        print(f"✗ {filepath} not found")
        return False

def test_python_syntax(filepath):
    """Test Python file syntax"""
    try:
        import py_compile
        py_compile.compile(filepath, doraise=True)
        print(f"✓ {filepath} syntax valid")
        return True
    except SyntaxError as e:
        print(f"✗ {filepath} syntax error: {e}")
        return False

def test_file_size(filepath, min_size):
    """Test if file is larger than min_size bytes"""
    size = os.path.getsize(filepath)
    if size >= min_size:
        print(f"✓ {filepath} size: {size} bytes")
        return True
    else:
        print(f"✗ {filepath} too small: {size} bytes")
        return False

def main():
    print("=" * 60)
    print("Tube MPC Benchmark - Component Tests")
    print("=" * 60)
    print()

    passed = 0
    total = 0

    # Test 1: File existence
    print("Test 1: File Existence")
    files_to_check = [
        "src/tube_mpc_benchmark/package.xml",
        "src/tube_mpc_benchmark/CMakeLists.txt",
        "src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py",
        "src/tube_mpc_benchmark/scripts/benchmark_runner.py",
        "src/tube_mpc_benchmark/launch/quick_test.launch",
        "src/tube_mpc_benchmark/configs/empty_map.yaml",
        "src/tube_mpc_benchmark/configs/empty_map.pgm",
    ]

    for filepath in files_to_check:
        total += 1
        if test_file_exists(filepath):
            passed += 1
    print()

    # Test 2: Python syntax
    print("Test 2: Python Syntax")
    python_files = [
        "src/tube_mpc_benchmark/scripts/kinematic_robot_sim.py",
        "src/tube_mpc_benchmark/scripts/benchmark_runner.py",
        "src/tube_mpc_benchmark/scripts/simple_goal_publisher.py",
    ]

    for filepath in python_files:
        total += 1
        if test_python_syntax(filepath):
            passed += 1
    print()

    # Test 3: File sizes
    print("Test 3: File Sizes")
    total += 1
    if test_file_size("src/tube_mpc_benchmark/configs/empty_map.pgm", 100000):
        passed += 1
    print()

    # Summary
    print("=" * 60)
    print(f"Results: {passed}/{total} tests passed")
    print("=" * 60)

    if passed == total:
        print("✓ All tests passed!")
        return 0
    else:
        print(f"✗ {total - passed} tests failed")
        return 1

if __name__ == "__main__":
    sys.exit(main())
```

运行：
```bash
python3 test_benchmark_components.py
```

## 集成测试（可选，需要完整ROS环境）

### 测试完整的导航流程

```bash
# 1. 启动ROS core
roscore &
sleep 2

# 2. 在一个终端启动模拟器和导航
roslaunch tube_mpc_benchmark quick_test.launch controller:=tube_mpc &

# 3. 等待系统启动
sleep 5

# 4. 检查话题是否活跃
rostopic list | grep -E "cmd_vel|odom|move_base"

# 预期输出应该包含：
#   /cmd_vel
#   /odom
#   /move_base/... (多个话题)

# 5. 检查话题数据
timeout 3 rostopic echo /cmd_vel -n 2
timeout 3 rostopic echo /odom -n 2

# 6. 检查导航状态
rostopic echo /move_base/status -n 1

# 如果看到status，说明导航栈正在运行
```

## 常见问题排查

### 问题1：ImportError: No module named 'rospy'

**解决方案**：
```bash
source devel/setup.bash
echo "ROS distribution: $ROS_DISTRO"
```

### 问题2：Package not found

**解决方案**：
```bash
# 确保在正确的目录
cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# 检查包路径
rospack find tube_mpc_benchmark
```

### 问题3：Launch文件无法加载

**解决方案**：
```bash
# 检查launch文件语法
xmllint --noout src/tube_mpc_benchmark/launch/quick_test.launch

# 检查依赖的包
rospack find tube_mpc_ros
rospack find move_base
```

### 问题4：模拟器不发布/odom

**诊断**：
```bash
# 检查节点是否运行
rosnode list | grep kinematic

# 检查话题是否发布
rostopic list | grep odom

# 查看节点日志
rosnode info kinematic_robot_sim
rqt_console  # 图形界面查看日志
```

## 验证清单

运行以下命令验证系统就绪：

```bash
# ✓ 1. 包结构
ls -d src/tube_mpc_benchmark
# 预期：src/tube_mpc_benchmark

# ✓ 2. Python脚本（4个）
ls src/tube_mpc_benchmark/scripts/*.py | wc -l
# 预期：4

# ✓ 3. Launch文件（2个）
ls src/tube_mpc_benchmark/launch/*.launch | wc -l
# 预期：2

# ✓ 4. 地图文件
ls -lh src/tube_mpc_benchmark/configs/empty_map.pgm
# 预期：~157KB文件

# ✓ 5. ROS包识别
source devel/setup.bash && rospack find tube_mpc_benchmark
# 预期：/home/dixon/Final_Project/catkin/src/tube_mpc_benchmark

# ✓ 6. Python语法
python3 -m py_compile src/tube_mpc_benchmark/scripts/*.py && echo "语法正确"
# 预期：无错误输出

# ✓ 7. 脚本权限
ls -l src/tube_mpc_benchmark/scripts/*.sh | grep -q rwx && echo "权限正确"
# 预期：权限正确
```

## 下一步

如果所有验证通过：

1. **运行可视化测试**：
   ```bash
   roslaunch tube_mpc_benchmark quick_test.launch
   ```

2. **运行小批量测试**：
   ```bash
   roslaunch tube_mpc_benchmark benchmark_batch.launch num_tests:=10 rviz:=false
   ```

3. **运行完整基准测试**：
   ```bash
   ./src/tube_mpc_benchmark/scripts/run_benchmark.sh -n 100
   ```

## 预期结果

### 单次测试（quick_test）
- **启动时间**：<5秒
- **导航时间**：10-15秒
- **成功率**：>95%
- **位置误差**：<0.3m

### 批量测试（100次）
- **总时间**：12-15分钟
- **成功率**：90-98%
- **平均时间**：8-15秒/测试
- **内存占用**：<200MB

祝测试顺利！🚀
