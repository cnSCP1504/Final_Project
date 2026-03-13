#!/bin/bash

# 深度诊断脚本 - 机器人完全不动问题

echo "=== Tube MPC 深度诊断工具 ==="
echo ""

# 1. 检查关键文件和配置
echo "1. 检查可执行文件和配置..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

EXE_PATH="/home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node"
if [ -f "$EXE_PATH" ]; then
    echo "✅ 可执行文件存在: $EXE_PATH"
    ls -lh "$EXE_PATH"
else
    echo "❌ 可执行文件不存在: $EXE_PATH"
    echo "需要运行: catkin_make"
fi

# 检查启动文件
echo ""
echo "检查启动文件..."
find /home/dixon/Final_Project/catkin/src/tube_mpc_ros -name "*.launch" -type f

# 2. 检查参数文件
echo ""
echo "2. 检查关键参数..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

PARAM_FILE="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/params/tube_mpc_params.yaml"
if [ -f "$PARAM_FILE" ]; then
    echo "✅ 参数文件存在"
    echo ""
    echo "关键参数值:"
    grep -E "max_speed|mpc_ref_vel|mpc_w_vel|controller_freq" "$PARAM_FILE" | sed 's/^/  /'
else
    echo "❌ 参数文件不存在: $PARAM_FILE"
fi

# 3. 检查代码逻辑
echo ""
echo "3. 检查代码中的关键逻辑..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

CODE_FILE="/home/dixon/Final_Project/catkin/src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp"
if [ -f "$CODE_FILE" ]; then
    echo "检查控制条件..."
    grep -n "if(_goal_received && !_goal_reached && _path_computed" "$CODE_FILE" | head -1

    echo ""
    echo "检查速度发布..."
    grep -n "_pub_twist.publish" "$CODE_FILE" | head -1

    echo ""
    echo "检查pub_twist_flag..."
    grep -n "_pub_twist_flag" "$CODE_FILE" | head -3

    echo ""
    echo "检查速度赋值逻辑..."
    grep -A2 "_speed = v + _throttle" "$CODE_FILE" | head -5
fi

echo ""
echo "4. 创建调试启动文件..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 创建一个调试启动文件
cat > "/tmp/tube_mpc_debug.launch" << 'EOF'
<?xml version="1.0"?>
<launch>
  <!-- 启用详细日志 -->
  <env name="ROSCONSOLE_CONFIG_FILE" value="/tmp/rosconsole.config"/>

  <!-- Tube MPC Navigation -->
  <include file="$(find tube_mpc_ros)/launch/tube_mpc_navigation.launch">
    <arg name="debug_info" value="true"/>
    <arg name="controller_freq" value="10"/>
  </include>

  <!-- 额外的话题监控节点 -->
  <node name="topic_monitor" pkg="rostopic" type="rostopic" output="screen"
        args="echo /cmd_vel" />

</launch>
EOF

echo "✓ 已创建调试启动文件: /tmp/tube_mpc_debug.launch"

echo ""
echo "5. 可能的问题和建议..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo "可能的问题:"
echo "  1. ❓ 路径规划未启动 - 检查/move_base是否运行"
echo "  2. ❓ 全局路径未生成 - 检查是否设置了目标点"
echo "  3. ❓ 控制器未启动 - 检查tube_TubeMPC_Node是否运行"
echo "  4. ❓ 话题名称不匹配 - 检查/cmd_vel话题"
echo "  5. ❓ 控制器频率问题 - 检查controller_freq设置"
echo "  6. ❓ 条件未满足 - 检查goal/path状态"

echo ""
echo "6. 诊断步骤..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "请按顺序执行以下检查:"
echo ""
echo "A. 启动系统时检查:"
echo "   1. 查看所有节点: rosnode list"
echo "   2. 查看所有话题: rostopic list"
echo "   3. 检查节点图: rosrun rqt_graph rqt_graph"
echo ""
echo "B. 设置目标后检查:"
echo "   1. 检查目标是否接收: rostopic echo /move_base_simple/goal"
echo "   2. 检查全局路径: rostopic echo /move_base/GlobalPlanner/plan"
echo "   3. 检查局部路径: rostopic echo /mpc_reference"
echo "   4. 检查控制命令: rostopic echo /cmd_vel"
echo ""
echo "C. 实时监控:"
echo "   1. 速度: rostopic hz /cmd_vel"
echo "   2. 路径: rostopic hz /mpc_reference"
echo "   3. 轨迹: rostopic hz /mpc_trajectory"
echo ""
echo "7. 快速修复建议..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "如果路径存在但机器人不动，可能是:"
echo "  → 控制器参数问题"
echo "  → 速度计算问题"
echo "  → 话题发布问题"
echo ""
echo "如果路径不存在，可能是:"
echo "  → 全局规划器问题"
echo "  → 地图配置问题"
echo "  → 目标设置问题"

# 创建诊断日志脚本
cat > "/tmp/diagnose_robot.sh" << 'EOF'
#!/bin/bash
echo "=== 机器人不动实时诊断 ==="
echo ""
echo "节点状态:"
rosnode list 2>/dev/null | grep -v "rosout\|/" || echo "  ❌ ROS未运行"
echo ""
echo "话题状态:"
rostopic list 2>/dev/null | grep -E "cmd_vel|mpc|goal|plan" | head -10 || echo "  ❌ 话题不可用"
echo ""
echo "控制命令 (最近5个):"
timeout 2 rostopic echo /cmd_vel -n 5 2>/dev/null || echo "  ❌ /cmd_vel 无数据"
echo ""
echo "参考路径 (最近5个):"
timeout 2 rostopic echo /mpc_reference -n 5 2>/dev/null || echo "  ❌ /mpc_reference 无数据"
echo ""
echo "全局路径 (最近5个):"
timeout 2 rostopic echo /move_base/GlobalPlanner/plan -n 5 2>/dev/null || echo "  ❌ 全局路径无数据"
EOF

chmod +x "/tmp/diagnose_robot.sh"
echo "✓ 已创建实时诊断脚本: /tmp/diagnose_robot.sh"

echo ""
echo "8. 创建强制测试节点..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

cat > "/tmp/test_cmd_vel_publisher.py" << 'EOF'
#!/usr/bin/env python3
import rospy
from geometry_msgs.msg import Twist

def test_publisher():
    rospy.init_node('test_cmd_vel_publisher')
    pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)

    rate = rospy.Rate(10)  # 10Hz
    twist = Twist()

    print("开始发布测试速度命令...")
    print("linear.x = 0.5, angular.z = 0.0")
    print("按Ctrl+C停止")

    twist.linear.x = 0.5
    twist.angular.z = 0.0

    try:
        while not rospy.is_shutdown():
            pub.publish(twist)
            rate.sleep()
    except rospy.ROSInterruptException:
        print("\n停止发布")

if __name__ == '__main__':
    test_publisher()
EOF

chmod +x "/tmp/test_cmd_vel_publisher.py"
echo "✓ 已创建测试发布节点: /tmp/test_cmd_vel_publisher.py"

echo ""
echo "✅ 诊断完成！"
echo ""
echo "📋 建议的下一步操作:"
echo ""
echo "1. 启动导航系统（如未启动）"
echo "2. 运行实时诊断: /tmp/diagnose_robot.sh"
echo "3. 检查具体哪一步出了问题"
echo "4. 根据诊断结果采取相应措施"
