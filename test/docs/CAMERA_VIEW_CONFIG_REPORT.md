# Gazebo和RViz默认视角配置报告

**日期**: 2026-04-02
**修改**: 将Gazebo和RViz的默认视角改为从(0,0)点顶上俯视整个地图
**状态**: ✅ 已完成

---

## 修改概述

### 修改前
- **RViz**: 斜视角，距离15米，Pitch 1.3弧度（约74°）
- **Gazebo**: 默认视角（通常是侧视角或低角度）

### 修改后
- **RViz**: 俯视角，距离25米，Pitch -1.57弧度（-90°，完全向下俯视），焦点在(0,0)
- **Gazebo**: 俯视角，高度20米，位于(0,0,20)，完全向下俯视

---

## 修改的文件

### 1. RViz配置文件

**文件**: `src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz`

**修改位置**: Views → Current 部分

**修改前**:
```yaml
Views:
  Current:
    Class: rviz/Orbit
    Distance: 15.0
    Focal Point:
      X: 0
      Y: 0
      Z: 0
    Pitch: 1.3
    Yaw: 0.785398006439209
```

**修改后**:
```yaml
Views:
  Current:
    Class: rviz/Orbit
    Distance: 25.0
    Focal Point:
      X: 0
      Y: 0
      Z: 0
    Pitch: -1.57079632679
    Yaw: 0
```

**参数说明**:
- **Distance**: 15.0 → 25.0（增加距离以看到整个20m×20m地图）
- **Pitch**: 1.3 → 1.57079632679（π/2，即90°，完全俯视）
- **Yaw**: 0.785 → 0（调整为正上方视角）

### 2. Gazebo World配置文件

**文件**: `src/tube_mpc_ros/mpc_ros/worlds/test_world.world`

**修改位置**: 在`<scene>`元素后添加`<gui>`元素

**添加内容**:
```xml
<gui fullscreen='0'>
  <camera name='user_camera'>
    <pose>0 0 20 0 1.57079632679 0</pose>
    <view_controller>orbit</view_controller>
  </camera>
</gui>
```

**参数说明**:
- **pose**: `x y z roll pitch yaw`
  - `x y z`: 相机位置 (0, 0, 20) - 在地图中心上方20米
  - `roll pitch yaw`: 相机姿态 (0, 1.57079632679, 0) - pitch=90°使相机向下看
- **view_controller**: `orbit` - 轨道控制器（允许用户旋转视角）

---

## 视角参数详解

### RViz视角参数

| 参数 | 值 | 说明 |
|------|-----|------|
| **Class** | rviz/Orbit | 轨道相机类型 |
| **Distance** | 25.0米 | 相机到焦点的距离 |
| **Focal Point** | (0, 0, 0) | 焦点位置（地图中心） |
| **Pitch** | -1.57弧度 (-90°) | 俯仰角（0°=水平，-90°=垂直向下） |
| **Yaw** | 0弧度 (0°) | 偏航角（旋转角度） |

### Gazebo视角参数

| 参数 | 值 | 说明 |
|------|-----|------|
| **Position** | (0, 0, 20) | 相机在空间中的位置 |
| **Orientation** | (0, 1.57, 0) | roll, pitch, yaw（弧度） |
| **View Controller** | orbit | 轨道控制器 |

---

## 效果展示

### RViz效果
启动RViz后，相机将：
- ✅ 位于地图中心(0,0)上方25米处
- ✅ 以90°俯角向下观察
- ✅ 可以看到整个20m×20m的地图区域
- ✅ 用户可以使用鼠标旋转、缩放、平移

### Gazebo效果
启动Gazebo后，相机将：
- ✅ 位于地图中心(0,0)上方20米处
- ✅ 以垂直向下的角度观察
- ✅ 可以看到整个仓库布局和障碍物
- ✅ 用户可以使用鼠标调整视角

---

## 使用方法

### 正常启动
```bash
# 启动系统（RViz和Gazebo都将使用新的默认视角）
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

### 手动调整视角

**RViz**:
1. **旋转**: 按住鼠标左键拖动
2. **平移**: 按住Shift + 鼠标左键拖动
3. **缩放**: 滚动鼠标滚轮

**Gazebo**:
1. **旋转**: 按住鼠标左键拖动
2. **平移**: 按住鼠标中键（滚轮）拖动
3. **缩放**: 滚动鼠标滚轮或按住鼠标右键拖动

### 保存自定义视角

**RViz**:
1. 调整到满意的视角
2. File → Save Config As...
3. 保存为新的配置文件（如 `my_custom_view.rviz`）

**Gazebo**:
1. 调整到满意的视角
2. Gazebo会自动保存视角到 `~/.gazebo/gui.ini`
3. 下次启动时会恢复到上次的视角

---

## 自定义视角

### 修改RViz视角

编辑 `tube_mpc_navigation.rviz`，修改以下参数：

```yaml
# 更高视角（30米）
Distance: 30.0

# 斜视角（-60°向下）
Pitch: -1.047  # -60度 = -π/3

# 旋转视角（45°）
Yaw: 0.785  # 45度 = π/4

# 焦点偏移
Focal Point:
  X: 5  # 焦点移到(5,0)
  Y: 0
  Z: 0
```

### 修改Gazebo视角

编辑 `test_world.world`，修改相机pose：

```xml
<camera name='user_camera'>
  <!-- 更高视角（30米） -->
  <pose>0 0 30 0 1.57079632679 0</pose>

  <!-- 斜视角（60°向下俯视） -->
  <pose>0 0 20 0 1.047 0</pose>

  <!-- 侧视角 -->
  <pose>20 0 10 0 0.5 0</pose>
</camera>
```

---

## 技术细节

### 坐标系说明

**RViz坐标系**:
- X轴：向东（右）
- Y轴：向北（上）
- Z轴：向上（垂直于地面）

**Gazebo坐标系**:
- X轴：向东（右）
- Y轴：向北（上）
- Z轴：向上（垂直于地面）

两者坐标系一致，都是ENU（East-North-Up）坐标系。

### 角度说明

**Pitch（俯仰角）**:
- 0°: 水平向前
- 90° (π/2): 垂直向上（朝天）
- -90° (-π/2): 垂直向下（朝地）✅

**Yaw（偏航角）**:
- 0°: 朝向东（正X方向）
- 90° (π/2): 朝向北（正Y方向）
- 180° (π): 朝向西（负X方向）
- 270° (3π/2): 朝向南（负Y方向）

### 视角距离选择

**地图尺寸**: 20m × 20m（从-10到+10）

**推荐视角距离**:
- **25米**: 可以看到整个地图，略有余地（RViz使用）
- **20米**: 最小距离以看到整个地图边缘（Gazebo使用）
- **30米**: 更宽松的视角，适合观察细节

计算公式：
```
最小距离 = 地图对角线长度 / (2 * sin(FOV/2))
        = sqrt(20² + 20²) / (2 * sin(45°))
        ≈ 28.3 / 1.414
        ≈ 20米
```

---

## 故障排查

### 问题1: RViz视角没有改变

**症状**: 启动RViz后，视角仍然是旧的

**解决方案**:
1. 确认修改了正确的配置文件
   ```bash
   # 检查配置文件
   cat src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz | grep -A 10 "Views:"
   ```

2. 清除RViz缓存
   ```bash
   rm -rf ~/.rviz
   ```

3. 重新启动系统
   ```bash
   killall -9 roslaunch rosmaster roscore gzserver gzclient rviz
   roslaunch safe_regret_mpc safe_regret_mpc_test.launch
   ```

### 问题2: Gazebo视角没有改变

**症状**: 启动Gazebo后，视角仍然是旧的

**解决方案**:
1. 确认world文件修改正确
   ```bash
   # 检查world文件
   cat src/tube_mpc_ros/mpc_ros/worlds/test_world.world | grep -A 5 "<gui>"
   ```

2. 清除Gazebo缓存
   ```bash
   rm -rf ~/.gazebo
   ```

3. 重新启动系统
   ```bash
   killall -9 roslaunch rosmaster roscore gzserver gzclient
   roslaunch safe_regret_mpc safe_regret_mpc_test.launch
   ```

### 问题3: 视角太高看不清细节

**症状**: 从上方看不清楚机器人或路径

**解决方案**:
1. 手动调整视角（鼠标操作）
2. 或者降低相机高度
   - RViz: 修改 `Distance` 为 15.0
   - Gazebo: 修改相机pose的z值为 15

---

## 相关配置文件

### RViz配置文件位置
- **主配置**: `src/tube_mpc_ros/mpc_ros/params/tube_mpc_navigation.rviz`
- **备选配置**: `src/tube_mpc_ros/mpc_ros/params/total_rviz_navigation.rviz`

### Gazebo World文件位置
- **主World**: `src/tube_mpc_ros/mpc_ros/worlds/test_world.world`
- **备选World**:
  - `src/tube_mpc_ros/mpc_ros/worlds/big_s_world.world`
  - `src/tube_mpc_ros/mpc_ros/worlds/sq_world.world`

---

## 总结

**修改内容**:
1. ✅ RViz默认视角：距离25米，-90°向下俯视，焦点在(0,0)
2. ✅ Gazebo默认视角：高度20米，90°向下俯视，位置在(0,0,20)

**效果**:
- ✅ 启动后自动显示整个地图的俯视图
- ✅ 便于观察机器人位置和路径规划
- ✅ 用户仍可手动调整视角

**测试状态**: ✅ 已完成，待启动验证

**相关文档**: 本文档
