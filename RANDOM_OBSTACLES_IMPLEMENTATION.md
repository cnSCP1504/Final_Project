# 随机障碍物动态刷新系统 - 实现文档

**日期**: 2026-04-05
**功能**: 地图中央障碍物随机刷新，自动避开目标点周围0.6m范围

---

## 📋 功能概述

### 核心功能
1. **动态随机刷新**：地图中央的三个障碍物位置随机刷新
2. **智能避让**：障碍物永远不会出现在目标点周围0.6m范围内
3. **目标跟随**：当目标点改变时，立即重新计算障碍物位置
4. **定时刷新**：每5秒自动刷新一次障碍物位置（可配置）

### 障碍物列表
- `central_obstacle_1`: 圆柱形障碍物 (半径0.8m)
- `central_obstacle_2`: 方块障碍物 (1.5m x 1.5m)
- `central_obstacle_3`: 方块障碍物 (1.5m x 1.5m)

---

## 🔧 实现细节

### 文件清单

| 文件 | 说明 |
|------|------|
| `src/safe_regret_mpc/scripts/random_obstacles.py` | 随机障碍物管理器节点 |
| `src/safe_regret_mpc/launch/safe_regret_mpc_test.launch` | 更新：添加random_obstacles节点 |
| `test_random_obstacles.sh` | 功能测试脚本 |
| `RANDOM_OBSTACLES_IMPLEMENTATION.md` | 本文档 |

### 核心算法

#### 1. 目标点监听
```python
def goal_callback(self, msg):
    """监听目标点变化"""
    self.current_goal_x = msg.pose.position.x
    self.current_goal_y = msg.pose.position.y
    rospy.loginfo(f"目标点更新: ({self.current_goal_x:.2f}, {self.current_goal_y:.2f})")
    # 目标点变化时立即刷新障碍物位置
    self.randomize_obstacles()
```

**工作流程**：
- 订阅 `/move_base_simple/goal` 话题
- 提取目标点 (x, y) 坐标
- 立即触发障碍物重新随机化

#### 2. 安全位置检查
```python
def is_safe_position(self, x, y):
    """检查位置是否安全（不在目标点周围的安全半径内）"""
    dx = x - self.current_goal_x
    dy = y - self.current_goal_y
    distance = math.sqrt(dx*dx + dy*dy)
    return distance > self.safety_radius  # safety_radius = 0.6m
```

**安全保证**：
- 计算障碍物到目标点的欧氏距离
- 距离必须 > 0.6m 才被认为是安全的
- 否则重新生成位置

#### 3. 随机位置生成
```python
def generate_random_position(self, obstacle_name):
    """为障碍物生成随机位置，避开目标点周围区域"""
    max_attempts = 100  # 最大尝试次数

    for attempt in range(max_attempts):
        # 在地图范围内生成随机位置
        x = random.uniform(-10 + 0.75, 10 - 0.75)
        y = random.uniform(-10 + 0.75, 10 - 0.75)

        # 检查安全性
        if self.is_safe_position(x, y):
            # 额外检查：避免离墙壁太近
            if (-9 < x < 9 and -9 < y < 9):
                return x, y

    # 如果找不到，返回默认位置
    return -self.current_goal_x, -self.current_goal_y
```

**算法特点**：
- 最多尝试100次寻找安全位置
- 考虑地图边界（-10到10）
- 考虑墙壁边距（1m）
- 失败时返回对称位置

#### 4. Gazebo模型更新
```python
def randomize_obstacles(self):
    """随机刷新所有障碍物位置"""
    for obstacle_name in ['central_obstacle_1', 'central_obstacle_2', 'central_obstacle_3']:
        # 生成新的随机位置
        new_x, new_y = self.generate_random_position(obstacle_name)

        # 创建模型状态消息
        model_state = ModelState()
        model_state.model_name = obstacle_name
        model_state.pose.position.x = new_x
        model_state.pose.position.y = new_y
        model_state.pose.position.z = 0.5

        # 通过Gazebo服务更新位置
        self.set_model_state(model_state)
```

**更新机制**：
- 使用Gazebo的 `set_model_state` 服务
- 保持障碍物高度不变（z=0.5m）
- 实时更新到仿真环境

---

## 🎛️ 配置参数

### ROS参数

| 参数名 | 默认值 | 说明 |
|--------|--------|------|
| `safety_radius` | 0.6 | 目标点安全半径（米） |
| `refresh_interval` | 5.0 | 障碍物刷新间隔（秒） |

### 修改参数

**方法1：修改launch文件**
```xml
<node name="random_obstacles" pkg="safe_regret_mpc" type="random_obstacles.py">
    <param name="safety_radius" value="1.0"/>      <!-- 改为1m安全半径 -->
    <param name="refresh_interval" value="10.0"/>  <!-- 改为10秒刷新 -->
</node>
```

**方法2：命令行参数**
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    safety_radius:=1.0 \
    refresh_interval:=10.0
```

---

## 🧪 测试验证

### 运行测试脚本
```bash
./test_random_obstacles.sh
```

### 手动测试步骤

#### 1. 启动系统
```bash
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

#### 2. 观察日志输出
```
============================================================
随机障碍物管理器启动
============================================================
安全半径: 0.6 m
刷新间隔: 5.0 s
初始目标点: (8.00, 0.00)
============================================================
✓ Gazebo服务已连接

============================================================
开始随机刷新障碍物位置...
当前目标点: (8.00, 0.00)
安全半径: 0.6 m
============================================================
✓ central_obstacle_1: (-3.45, 2.78) | 距离目标: 11.78 m
✓ central_obstacle_2: (1.23, -4.56) | 距离目标: 7.01 m
✓ central_obstacle_3: (-1.89, 3.21) | 距离目标: 10.12 m
============================================================
障碍物刷新完成！
```

#### 3. 验证安全性
- 检查日志中的"距离目标"值
- **必须**始终 > 0.60 m
- 如果 ≤ 0.60 m，说明功能异常

#### 4. 测试目标点变化
```bash
# 发送新的目标点
rostopic pub /move_base_simple/goal geometry_msgs/PoseStamped "header:
  frame_id: 'map'
pose:
  position:
    x: -8.0
    y: 0.0
    z: 0.0
  orientation:
    x: 0.0
    y: 0.0
    z: 0.0
    w: 1.0" --once
```

**预期行为**：
1. 日志显示"目标点更新: (-8.00, 0.00)"
2. 立即触发障碍物刷新
3. 新的障碍物位置都距离 (-8, 0) > 0.6m

---

## 📊 系统架构

### 数据流
```
auto_nav_goal.py
  ↓ 发布目标点
/move_base_simple/goal
  ↓ 订阅
random_obstacles.py
  ↓ 计算安全位置
/gazebo/set_model_state
  ↓ 更新模型
Gazebo仿真环境
```

### 节点关系
```
┌─────────────────────┐
│  auto_nav_goal.py   │
│  (发布目标点)        │
└──────────┬──────────┘
           │ /move_base_simple/goal
           ↓
┌─────────────────────┐
│ random_obstacles.py │
│ (监听目标点)         │
│                     │
│ 1. 读取目标点坐标    │
│ 2. 生成安全位置      │
│ 3. 更新障碍物        │
└──────────┬──────────┘
           │ /gazebo/set_model_state
           ↓
┌─────────────────────┐
│     Gazebo          │
│  (更新障碍物位置)    │
└─────────────────────┘
```

---

## 🎯 使用场景

### 场景1：固定目标测试
```bash
# 目标点固定在 (8, 0)
# 障碍物每5秒刷新一次
# 始终避开 (8, 0) 周围0.6m范围
```

### 场景2：动态目标测试
```python
# auto_nav_goal.py定义多个目标点
goals = [
    (0.0, -7.0, 0.0, "目标1"),
    (8.0, 0.0, 0.0, "目标2"),
    (-8.0, 0.0, 0.0, "目标3"),
]
# 每次目标改变，障碍物立即重新随机化
```

### 场景3：自定义安全半径
```xml
<!-- 增大安全半径到1m -->
<param name="safety_radius" value="1.0"/>
```

---

## ⚠️ 注意事项

### 1. Gazebo服务依赖
- 节点启动后会等待Gazebo服务可用
- 如果Gazebo未启动，节点会一直等待
- 确保 `use_gazebo:=true` 时才使用此功能

### 2. 启动顺序
- `random_obstacles.py` 延迟10秒启动（`launch-prefix`）
- 确保Gazebo完全加载后再启动障碍物管理器
- 避免服务未就绪的错误

### 3. 性能考虑
- 默认刷新间隔：5秒
- 过于频繁的刷新会影响仿真性能
- 建议范围：3-10秒

### 4. 地图边界
- 当前地图大小：20m x 20m
- 坐标范围：-10 到 10
- 障碍物不会生成在墙壁1m范围内

---

## 🔍 故障排查

### 问题1：障碍物不移动
**症状**：障碍物位置始终不变

**可能原因**：
1. Gazebo服务未启动
2. 模型名称不匹配
3. 权限问题

**解决方案**：
```bash
# 检查Gazebo服务
rosservice list | grep gazebo

# 检查模型是否存在
rosservice call /gazebo/get_model_state "model_name: 'central_obstacle_1'"

# 查看错误日志
rosnode info random_obstacles
```

### 问题2：障碍物出现在目标点附近
**症状**：障碍物距离目标点 < 0.6m

**可能原因**：
1. safety_radius参数设置错误
2. 随机生成算法失败

**解决方案**：
```bash
# 检查参数
rosparam get /random_obstacles/safety_radius

# 增大安全半径
<param name="safety_radius" value="1.0"/>
```

### 问题3：目标点变化时障碍物不刷新
**症状**：发送新目标后，障碍物位置不变

**可能原因**：
1. `/move_base_simple/goal` 话题未连接
2. 回调函数未触发

**解决方案**：
```bash
# 检查话题连接
rostopic info /move_base_simple/goal

# 手动发送目标测试
rostopic pub /move_base_simple/goal ...
```

---

## 📈 未来改进

### 可能的增强功能
1. **障碍物形状随机化**：不只是位置，形状也可以随机
2. **动态数量**：随机生成3-5个障碍物
3. **高度变化**：障碍物高度也可以随机
4. **移动障碍物**：障碍物可以在地图上缓慢移动
5. **可视化**：在RViz中显示障碍物的安全范围

---

## 📝 修改记录

| 日期 | 版本 | 修改内容 |
|------|------|---------|
| 2026-04-05 | 1.0 | 初始实现 |
|  |  | - 实现基本随机刷新功能 |
|  |  | - 实现目标点安全避让 |
|  |  | - 实现动态目标跟随 |

---

**状态**: ✅ **实现完成，已集成到safe_regret_mpc_test.launch**

**测试**: 运行 `./test_random_obstacles.sh` 进行验证
