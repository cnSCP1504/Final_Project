# 自动导航目标功能说明

## 概述
为tube_mpc_navigation.launch添加了自动设置2D导航目标的功能。默认目标坐标为（3.0，-7.0，0）。

## 文件位置
- **Python脚本**: `tube_mpc_ros/script/auto_nav_goal.py`
- **Launch文件**: `tube_mpc_ros/launch/tube_mpc_navigation.launch`

## 功能特性
1. **自动发送导航目标**: 在launch文件启动后自动发送目标位置
2. **可配置目标坐标**: 通过ROS参数或修改脚本自定义目标位置
3. **等待move_base准备就绪**: 确保导航系统完全启动后再发送目标
4. **详细日志输出**: 显示目标发送状态和坐标信息

## 默认目标坐标
- **X坐标**: 3.0 米
- **Y坐标**: -7.0 米
- **Yaw角度**: 0.0 弧度 (0度)

## 使用方法

### 1. 启动导航（使用默认目标）
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```
启动后会自动发送目标到（3.0，-7.0，0）。

### 2. 自定义目标坐标

#### 方法1: 修改launch文件
编辑 `tube_mpc_navigation.launch` 文件中的参数：
```xml
<node name="auto_nav_goal" pkg="tube_mpc_ros" type="auto_nav_goal.py" output="screen">
    <param name="goal_x" value="5.0"/>    <!-- 修改X坐标 -->
    <param name="goal_y" value="3.0"/>    <!-- 修改Y坐标 -->
    <param name="goal_yaw" value="1.57"/> <!-- 修改Yaw角度(弧度) -->
</node>
```

#### 方法2: 直接修改脚本
编辑 `auto_nav_goal.py` 文件底部的默认值：
```python
if __name__ == '__main__':
    try:
        goal_x = rospy.get_param('~goal_x', 5.0)    # 新的默认X坐标
        goal_y = rospy.get_param('~goal_y', 3.0)    # 新的默认Y坐标
        goal_yaw = rospy.get_param('~goal_yaw', 1.57)  # 新的默认Yaw角度
```

#### 方法3: 命令行参数
```bash
roslaunch tube_mpc_ros tube_mpc_navigation.launch goal_x:=5.0 goal_y:=3.0 goal_yaw:=1.57
```

### 3. 禁用自动导航目标
如果不想自动发送导航目标，可以从launch文件中注释掉或删除以下部分：
```xml
<!--  ************** Auto Navigation Goal **************  -->
<node name="auto_nav_goal" pkg="tube_mpc_ros" type="auto_nav_goal.py" output="screen" launch-prefix="bash -c 'sleep 5; $0 $@'">
    <param name="goal_x" value="3.0"/>
    <param name="goal_y" value="-7.0"/>
    <param name="goal_yaw" value="0.0"/>
</node>
```

## 技术细节

### 发送话题
- **话题名称**: `/move_base_simple/goal`
- **消息类型**: `geometry_msgs/PoseStamped`
- **坐标系**: `map`

### 延迟启动
- 使用5秒延迟确保move_base完全启动
- 可通过修改launch文件中的`sleep 5`调整延迟时间

### 坐标系说明
- **X坐标**: 前方（米）
- **Y坐标**: 左侧（米）
- **Yaw角度**: 逆时针旋转（弧度）
  - 0.0 = 正前方
  - 1.57 = 左转90度
  - -1.57 = 右转90度
  - 3.14 = 后方

## 故障排除

### 问题1: 目标未发送
- 检查move_base是否正常启动
- 查看日志输出中的错误信息
- 确认`/move_base_simple/goal`话题存在

### 问题2: 机器人无法到达目标
- 检查地图是否正确加载
- 确认目标位置在可行区域内
- 查看move_base的规划路径

### 问题3: 脚本无法执行
- 确保脚本有执行权限: `chmod +x script/auto_nav_goal.py`
- 检查Python环境: `python3 --version`
- 重新编译工作空间: `catkin_make`

## 集成测试
测试自动导航功能：
```bash
# 终端1: 启动导航
roslaunch tube_mpc_ros tube_mpc_navigation.launch

# 终端2: 监听目标话题
rostopic echo /move_base_simple/goal

# 终端3: 查看机器人状态
rostopic echo /amcl_pose
```

## 版本信息
- **创建日期**: 2026-03-10
- **ROS版本**: ROS Noetic
- **Python版本**: Python 3.8+
