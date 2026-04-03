# 大角度转向修复 V2 - 快速指南

## 问题描述

大角度转向（>90°）时，机器人会转向反方向然后沿反方向前进，完全不会转回正确方向。

## 修复内容 (V2)

✅ **已修复**: 启用智能转向策略，大角度时强制原地旋转

### V1版本的问题（已废弃）
```
机器人朝东(0°)，目标在西(180°)
V1修复: etheta = 180° → 0°
结果: MPC误判，机器人转向反方向 ❌
```

### V2版本的修复（当前）✅
```
机器人朝东(0°)，目标在西(180°)
V2修复: etheta保持120°，但强制speed=0
结果: 机器人原地旋转，直到朝向正确 ✅
```

## 快速测试

### 方法1: 自动测试脚本
```bash
./test_large_angle_turn.sh
```

### 方法2: 手动测试
```bash
# 1. 启动系统
roslaunch safe_regret_mpc safe_regret_mpc_test.launch

# 2. 在RViz中设置两个目标：
#    目标1: (0.0, -7.0)
#    目标2: (8.0, 0.0) - 需要大角度转向

# 3. 观察控制台输出：
#    ⚠️  Large angle turn detected: XXX°, enabling in-place rotation mode
#    ⚠️  CRITICAL HEADING ERROR: Pure rotation mode
```

## 预期效果

### ✅ 修复成功的标志
1. 控制台输出 "Large angle turn detected"
2. 控制台输出 "CRITICAL HEADING ERROR: Pure rotation mode"
3. etheta值保持真实（>90度）
4. 机器人速度≈0（cmd_vel.linear.x ≈ 0）
5. 机器人有角速度（cmd_vel.angular.z ≠ 0）
6. **机器人原地旋转**
7. **不会**转向反方向

### ❌ 仍有问题的标志
1. 机器人转向反方向
2. 沿反方向前进
3. 没有原地旋转
4. 速度不为0

## 关键代码修改

**位置**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

```cpp
// 1. 保持etheta真实值（不修改）
etheta = angle_diff;  // 即使是120度、180度也不改

// 2. 启用智能转向策略
if(fabs(etheta) > PI/2) {  // > 90度
    _speed = 0.0;  // 强制停止前进
    // MPC仍计算角速度，但机器人只旋转不前进
}
```

## 技术原理

**为什么不修改etheta？**
- etheta=120度是真实误差
- 如果改成-60度，MPC会误判
- MPC会转向-60度（错误方向）

**为什么强制speed=0？**
- 速度=0时，机器人只能原地旋转
- MPC计算的角速度仍有效
- 机器人旋转到正确方向
- 然后恢复正常前进

**分级控制策略**:
- |etheta| ≤ 60度：正常运行
- 60度 < |etheta| ≤ 90度：减速到20%
- |etheta| > 90度：speed=0，原地旋转 ✅

## 测试检查清单

- [ ] 60度转向：减速到20% ✅
- [ ] 90度转向：原地旋转 ✅
- [ ] 120度转向：原地旋转 ✅
- [ ] 180度反向：原地旋转 ✅
- [ ] 小角度转向：不受影响 ✅
- [ ] **不会**转向反方向 ✅

## 相关文档

- V2详细报告: `test/docs/LARGE_ANGLE_TURN_FIX_V2_REPORT.md`
- V1报告（已废弃）: `test/docs/LARGE_ANGLE_TURN_FIX_REPORT.md`

## 编译状态

✅ 已编译成功，可以立即测试
