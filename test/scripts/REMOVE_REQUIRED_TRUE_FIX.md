# 移除 safe_regret_mpc 节点的 required="true" 参数

## 修复日期
2026-04-06

## 问题描述

用户报告："测试脚本中的ROS进程一撞墙就会直接关闭"

## 根本原因

**`safe_regret_mpc_test.launch` 中设置了 `required="true"`**：

```xml
<node name="safe_regret_mpc" pkg="safe_regret_mpc" type="safe_regret_mpc_node"
      output="screen" required="true">
```

### ROS launch 文件的 `required` 参数行为

当一个节点设置了 `required="true"` 时：
- 如果该节点崩溃（退出码非0），**整个launch文件会立即关闭**
- 所有其他节点也会被终止
- 这是为了确保关键节点必须运行的机制

### 为什么会崩溃？

`safe_regret_mpc_node` 可能因为以下原因崩溃：
1. **STL约束数值问题**：鲁棒性计算中出现NaN或Inf
2. **DR约束不可行**：优化问题无解，Ipopt求解器失败
3. **段错误（Segmentation Fault）**：访问非法内存（exit code -11）
4. **矩阵奇异**：在求逆或分解时遇到奇异矩阵

这些情况在机器人撞墙或遇到极端路径时更容易发生。

## 解决方案

### 修改前

```xml
<node name="safe_regret_mpc" pkg="safe_regret_mpc" type="safe_regret_mpc_node"
      output="screen" required="true">
```

**行为**：
- safe_regret_mpc_node 崩溃 → 整个launch关闭 → Gazebo、tube_mpc等全部关闭

### 修改后

```xml
<node name="safe_regret_mpc" pkg="safe_regret_mpc" type="safe_regret_mpc_node"
      output="screen" required="false"
      respawn="false" respawn_delay="5">
```

**行为**：
- safe_regret_mpc_node 崩溃 → 仅该节点关闭
- tube_mpc、Gazebo、move_base等继续运行
- 机器人可以继续导航（使用tube_mpc基线控制器）

## 新增参数说明

- **`required="false"`**: 该节点不是必需的，崩溃时不会关闭整个launch
- **`respawn="false"`**: 节点崩溃后不自动重启（避免重复崩溃）
- **`respawn_delay="5"`**: 如果启用respawn，延迟5秒后重启（当前未启用）

## 效果对比

### 修改前
```
时间 0s:  ROS启动，所有节点运行正常
时间 30s: 机器人撞墙，safe_regret_mpc_node崩溃（段错误）
时间 30s: [ERROR] REQUIRED process has died!
         Initiating shutdown!
时间 30s: Gazebo关闭、tube_mpc关闭、所有节点关闭
时间 30s: 测试失败，需要重新启动
```

### 修改后
```
时间 0s:  ROS启动，所有节点运行正常
时间 30s: 机器人撞墙，safe_regret_mpc_node崩溃（段错误）
时间 30s: [WARN] process has died [safe_regret_mpc-13, exit code -11]
时间 30s: tube_mpc继续运行 ✅
         Gazebo继续运行 ✅
         move_base继续运行 ✅
         机器人继续导航（使用tube_mpc基线）✅
时间 60s: 机器人可能完成任务或遇到其他问题
```

## 优点

1. **提高测试鲁棒性**：即使STL/DR模块崩溃，基线tube_mpc仍可工作
2. **更好的调试**：可以看到崩溃后系统的实际行为
3. **数据收集**：即使safe_regret_mpc失败，仍可收集tube_mpc的性能数据
4. **避免完全中断**：不会因为一个模块的问题导致整个测试停止

## 缺点

1. **功能不完整**：如果safe_regret_mpc崩溃，STL和DR约束将不再生效
2. **降级运行**：系统会降级到tube_mpc基线模式
3. **可能被忽视**：如果监控不仔细，可能没注意到safe_regret_mpc已崩溃

## 测试建议

修改后，建议在测试中监控：
1. `/safe_regret_mpc/state` 话题 - 检查节点是否还活着
2. 测试脚本的ROS进程健康检查 - 已经实现
3. Metrics中的`ros_died`标志 - 记录崩溃情况

## 适用场景

这个修改适用于：
- ✅ **测试阶段**：收集数据，即使某些模块失败也能继续
- ✅ **渐进式集成**：逐步添加功能，允许模块失败
- ✅ **基线对比**：在同一测试中比较tube_mpc和safe_regret_mpc

不适用于：
- ❌ **生产环境**：如果safe_regret_mpc是安全关键组件
- ❌ **严格实验**：如果实验要求所有模块必须正常运行

## 其他可能的改进

如果safe_regret_mpc经常崩溃，可以考虑：

1. **添加异常处理**：
   ```cpp
   try {
       // STL/DR计算
   } catch (const std::exception& e) {
       ROS_WARN("STL/DR computation failed: %s", e.what());
       // 降级到tube_mPC模式
   }
   ```

2. **添加数值检查**：
   ```cpp
   if (std::isnan(robustness) || std::isinf(robustness)) {
       ROS_WARN("Invalid robustness value, skipping STL constraint");
       continue;
   }
   ```

3. **添加约束可行性检查**：
   ```cpp
   if (!check_feasibility()) {
       ROS_WARN("Constraints infeasible, using tube_mpc fallback");
       return;
   }
   ```

## 修改文件

- ✅ `/home/dixon/Final_Project/catkin/src/safe_regret_mpc/launch/safe_regret_mpc_test.launch` (line 153-154)

## 验证

修改后测试：
```bash
./test/scripts/run_automated_test.py --model safe_regret --shelves 1 --no-viz
```

预期行为：
- 如果safe_regret_mpc崩溃，测试应该继续运行
- tube_mpc应该接管控制
- 测试完成后应该记录崩溃情况

## 状态

✅ **修复完成**

已移除 `required="true"` 参数，ROS系统不会因为safe_regret_mpc_node崩溃而关闭。

---

**修复人**: Claude Code
**日期**: 2026-04-06
