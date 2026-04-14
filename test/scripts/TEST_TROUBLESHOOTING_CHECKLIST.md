# 自动化测试问题检查清单

**日期**: 2026-04-07
**目的**: 验证STL修复后自动化测试系统是否正常工作

---

## ✅ 预检清单（运行测试前）

### 1. 编译状态
```bash
# 检查C++ STL节点是否编译
ls -l /home/dixon/Final_Project/catkin/devel/lib/stl_monitor/stl_monitor_node_cpp
# 预期: 文件存在，可执行
```
- [ ] ✅ 文件存在
- [ ] ✅ 可执行权限

### 2. Launch配置
```bash
# 检查launch文件配置
grep "stl_monitor_node" /home/dixon/Final_Project/catkin/src/safe_regret_mpc/launch/safe_regret_mpc_test.launch
# 预期: type="stl_monitor_node_cpp" (不是.py)
grep "use_belief_space" /home/dixon/Final_Project/catkin/src/safe_regret_mpc/launch/safe_regret_mpc_test.launch
# 预期: value="true"
```
- [ ] ✅ 使用C++节点
- [ ] ✅ Belief space已启用

### 3. 话题映射
```bash
# 检查话题remap
grep "remap.*stl_robustness" /home/dixon/Final_Project/catkin/src/safe_regret_mpc/launch/safe_regret_mpc_test.launch
# 预期: <remap from="stl_robustness" to="/stl_monitor/robustness"/>
```
- [ ] ✅ Remap配置正确

### 4. 测试脚本权限
```bash
ls -l /home/dixon/Final_Project/catkin/test/scripts/*.py | grep -E "test_automated|manuscript_metrics"
# 预期: 可执行权限 (-rwxrwxrwx)
```
- [ ] ✅ 可执行权限

---

## 🧪 测试执行步骤

### 步骤1：清理环境
```bash
cd /home/dixon/Final_Project/catkin
killall -9 roslaunch rosmaster roscore gzserver gzclient python
sleep 2
```
- [ ] ✅ 所有ROS进程已清理

### 步骤2：运行快速数据流测试
```bash
# Terminal 1: 启动系统（最小配置）
roslaunch safe_regret_mpc safe_regret_mpc_test.launch \
    enable_stl:=true \
    enable_dr:=false \
    use_gazebo:=false &

# 等待10秒让系统初始化
sleep 10

# Terminal 2: 运行数据流测试
./test/scripts/test_stl_data_flow.py
```

**预期结果**:
- [ ] ✅ 收集到>100个robustness样本
- [ ] ✅ 唯一值>10（不是只有2个）
- [ ] ✅ Budget数据正常
- [ ] ✅ 无ROS错误

**如果失败**:
- ❌ 只有2个唯一值 → 检查`use_belief_space=true`
- ❌ 没有数据 → 检查STL节点是否运行
- ❌ ROS错误 → 检查话题名称和类型

### 步骤3：检查话题数据
```bash
# 在新terminal中
rostopic echo /stl_monitor/robustness -n 5
```

**预期输出**:
```
data: -2.3456
---
data: -2.1234
---
data: -1.9876
---
data: -1.8765
---
data: -1.7654
---
```
- [ ] ✅ 连续变化的值（不是只有-6.67和-15.49）

### 步骤4：运行完整自动化测试
```bash
# 清理环境
killall -9 roslaunch rosmaster roscore gzserver gzclient python
sleep 2

# 运行测试（1个货架，无可视化）
./test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 1 \
    --no-viz
```

**监控要点**:
- [ ] ✅ 系统正常启动（无crash）
- [ ] ✅ 机器人开始移动
- [ ] ✅ 测试脚本显示收集metrics
- [ ] ✅ STL robustness数据连续变化
- [ ] ✅ 测试完成后生成结果文件夹

---

## 🚨 常见问题和解决方案

### 问题1：STL节点崩溃

**症状**:
```
[stl_monitor] process has died
```

**诊断**:
```bash
# 查看日志
rosnode list
# 如果/stl_monitor不在列表中，说明节点崩溃

# 查看错误
rosservice call /stl_monitor/get_loggers
```

**可能原因**:
1. 编译问题 → 重新编译`catkin_make --only-pkg-with-deps stl_monitor`
2. 缺少依赖 → 检查CMakeLists.txt
3. 参数错误 → 检查launch文件配置

**解决方案**:
```bash
# 重新编译
cd /home/dixon/Final_Project/catkin
catkin_make --only-pkg-with-deps stl_monitor
source devel/setup.bash

# 重新运行测试
```

---

### 问题2：Robustness只有2个值

**症状**:
```
Unique values: 2
Values: -6.6718, -15.4923
```

**原因**: 仍在使用Python简化版（`use_belief_space=false`）

**解决方案**:
```xml
<!-- 编辑launch文件 -->
<param name="use_belief_space" value="true"/>  <!-- 必须是true -->
<node type="stl_monitor_node_cpp"/>  <!-- 必须是_cpp -->
```

---

### 问题3：测试脚本无法订阅STL话题

**症状**:
```
⚠️ 无法订阅STL话题
```

**诊断**:
```bash
# 检查话题是否存在
rostopic list | grep stl_monitor

# 检查话题类型
rostopic info /stl_monitor/robustness
```

**可能原因**:
1. STL节点未启动 → 检查`enable_stl:=true`
2. 话题名称不匹配 → 检查remap配置
3. 权限问题 → 检查ROS_MASTER_URI

---

### 问题4：数据收集不完整

**症状**:
```
STL robustness: 0 samples
Budget: 0 samples
```

**诊断**:
```bash
# 检查发布频率
rostopic hz /stl_monitor/robustness
# 预期: average rate: 10.0

# 检查订阅者数量
rostopic info /stl_monitor/robustness
# 预期: Subscribers: 至少1个（safe_regret_mpc或测试脚本）
```

**解决方案**:
1. 确认机器人有goal和global plan
2. 检查amcl定位是否工作
3. 验证测试脚本在robot移动时运行

---

### 问题5：CSV数据文件未生成

**症状**:
```
⚠️ No metrics data to save!
```

**诊断**:
```bash
# 检查输出目录
ls -la /home/dixon/Final_Project/catkin/test_results/

# 检查测试脚本日志
grep "manuscript_metrics" /home/dixon/Final_Project/catkin/test_results/safe_regret_*/test.log
```

**可能原因**:
1. 输出目录权限 → `chmod +x test_results`
2. 磁盘空间 → `df -h`
3. 测试时间太短 → 增加测试时间

---

## 📊 数据完整性验证

### 测试完成后验证

```bash
# 找到最新的测试结果
cd /home/dixon/Final_Project/catkin/test_results
latest_dir=$(ls -td -- safe_regret_* | head -1)
cd $latest_dir

# 检查生成的文件
ls -lh
```

**预期文件**:
- [ ] ✅ `test_summary.json` - 测试摘要
- [ ] ✅ `manuscript_metrics.csv` - 论文指标数据
- [ ] ✅ `test.log` - 完整日志
- [ ] ✅ `stl_robustness_timeseries.csv` - STL时间序列（如果启用STL）

### 验证数据质量

```python
# 快速检查脚本
import pandas as pd
import numpy as np

# 读取STL数据
df = pd.read_csv('manuscript_metrics.csv')

# 检查robustness列
if 'stl_robustness' in df.columns:
    robustness = df['stl_robustness'].dropna()
    print(f"Total samples: {len(robustness)}")
    print(f"Unique values: {robustness.round(4).nunique()}")
    print(f"Min: {robustness.min():.4f}")
    print(f"Max: {robustness.max():.4f}")
    print(f"Mean: {robustness.mean():.4f}")

    # 关键检查
    if robustness.round(4).nunique() > 10:
        print("✅ PASS: Continuous robustness values detected")
    else:
        print("❌ FAIL: Only discrete values (simplified implementation)")
else:
    print("❌ FAIL: No STL robustness data collected")
```

---

## 🔧 调试技巧

### 实时监控数据流

```bash
# Terminal 1: 监控STL robustness
watch -n 1 'rostopic echo /stl_monitor/robustness -n 1'

# Terminal 2: 监控测试脚本日志
tail -f test_results/safe_regret_*/test.log | grep "STL\|robustness"

# Terminal 3: 监控节点状态
watch -n 5 'rosnode list | grep stl'
```

### 检查数据收集速率

```bash
# 每10秒统计一次收集的数据量
watch -n 10 'grep "Collected.*snapshots" test_results/safe_regret_*/test.log | tail -1'
```

### 查看错误汇总

```bash
# 提取所有错误和警告
grep -E "ERROR|WARN|FAIL" test_results/safe_regret_*/test.log | sort | uniq -c
```

---

## ✅ 最终检查清单

测试完成后，确认以下所有项：

### 编译和配置
- [ ] ✅ C++ STL节点编译成功
- [ ] ✅ Launch文件使用C++节点
- [ ] ✅ Belief space已启用
- [ ] ✅ 话题remap正确

### 运行时状态
- [ ] ✅ STL节点正常运行
- [ ] ✅ 话题发布正常
- [ ] ✅ 机器人可以移动
- [ ] ✅ Global planner生成路径

### 数据收集
- [ ] ✅ 收集到>100个robustness样本
- [ ] ✅ Robustness值连续变化（>10个唯一值）
- [ ] ✅ Budget数据正常更新
- [ ] ✅ 无ROS订阅错误

### 输出文件
- [ ] ✅ 测试摘要JSON生成
- [ ] ✅ Manuscript metrics CSV生成
- [ ] ✅ STL时间序列数据生成
- [ ] ✅ 测试日志完整

### 数据质量
- [ ] ✅ Robustness范围合理（-10到0之间）
- [ ] ✅ 无异常常数值（如NaN, Inf）
- [ ] ✅ 时间戳连续无中断
- [ ] ✅ 满足率计算正确

---

## 📞 遇到问题时

1. **查看本文档**对应问题的解决方案
2. **运行快速验证** `./test/scripts/test_stl_data_flow.py`
3. **检查日志文件** `test_results/*/test.log`
4. **查看节点状态** `rosnode list`, `rostopic list`
5. **验证话题数据** `rostopic echo /stl_monitor/robustness`

---

**文档版本**: 1.0
**最后更新**: 2026-04-07
**维护者**: Claude Code
