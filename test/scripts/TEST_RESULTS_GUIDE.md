# 测试记录保存位置指南

## 📁 默认保存位置

### 主目录

```
/home/dixon/Final_Project/catkin/test_results/
```

### 目录命名规则

每个测试会话都有一个独立的时间戳目录：

```
<模型名称>_<年月日>_<时分秒>

例如：
- safe_regret_20260406_200458  (safe_regret模型，2026年4月6日 20:04:58)
- tube_mpc_20260406_024509     (tube_mpc模型，2026年4月6日 02:45:09)
```

### 自定义保存位置

可以通过命令行参数 `--results-dir` 自定义保存位置：

```bash
python3 test/scripts/run_automated_test.py \
    --model tube_mpc \
    --shelves 3 \
    --results-dir /path/to/custom/location
```

## 📂 目录结构

### 完整目录树

```
test_results/
└── safe_regret_20260406_200458/          # 测试会话目录
    ├── final_report.txt                    # 最终测试报告
    ├── test_01_shelf_01/                   # 测试1目录
    │   ├── logs/                           # 日志文件
    │   │   └── launch.log                  # ROS launch日志（~1MB）
    │   ├── rosbag/                         # ROS数据包（如果启用）
    │   ├── metrics.json                    # 测试指标数据（~12KB）
    │   └── test_summary.txt                # 测试摘要（~1KB）
    ├── test_02_shelf_02/                   # 测试2目录
    │   ├── (同上)
    ├── test_03_shelf_03/                   # 测试3目录
    │   ├── (同上)
    └── ...                                  # 更多测试
```

## 📄 文件内容详解

### 1. final_report.txt

**位置**: `<测试会话目录>/final_report.txt`

**内容**:
- 测试时间
- 模型配置
- 货架配置
- 测试结果统计（成功/失败/超时）
- 结果目录路径

**示例**:
```
======================================================================
      自动化Baseline测试最终报告
   Automated Baseline Testing Final Report
======================================================================

测试时间 / Test Time: 2026-04-06 20:17:44

模型配置 / Model Configuration:
  - 模型类型 / Model Type: safe_regret
  - 描述 / Description: Safe-Regret MPC统一框架

结果 / Results:
  - 总测试数 / Total Tests: 5
  - 成功 / Successful: 5
  - 失败 / Failed: 0
  - 超时 / Timeout: 0
  - ROS崩溃 / ROS Crashed: 0

结果目录 / Results Directory:
  /home/dixon/Final_Project/catkin/test_results/safe_regret_20260406_200458

======================================================================
```

---

### 2. test_summary.txt

**位置**: `<测试会话目录>/test_XX_shelf_XX/test_summary.txt`

**内容**:
- 测试时间
- 模型配置
- 货架信息
- 测试结果（SUCCESS/FAILED/TIMEOUT/ERROR/ROS_DIED）
- 如果是ROS崩溃，包含退出码和运行时间

**示例**:
```
======================================================================
           测试摘要 / Test Summary
======================================================================

测试时间 / Test Time: 2026-04-06 20:05:17

模型配置 / Model Configuration:
  - 模型类型 / Model Type: safe_regret
  - 货架ID / Shelf ID: 1
  - 货架名称 / Shelf Name: Shelf S1

测试结果 / Test Result: SUCCESS
```

---

### 3. metrics.json

**位置**: `<测试会话目录>/test_XX_shelf_XX/metrics.json`

**内容**: 完整的测试指标数据（JSON格式）

**字段说明**:
```json
{
  "test_start_time": "2026-04-06 20:05:17",           // 测试开始时间
  "test_end_time": "2026-04-06 20:06:35",             // 测试结束时间
  "test_status": "SUCCESS",                           // 测试状态
  "total_goals": 2,                                   // 总目标数
  "goals_reached": [                                  // 已到达目标列表
    {
      "goal_index": 0,                                // 目标索引
      "goal_position": {"x": -6.5, "y": -7.0, "yaw": 0.0},  // 目标位置
      "robot_position": {"x": -6.67, "y": -6.53, "yaw": -1.51}, // 机器人位置
      "distance": 0.50,                               // 到达时的距离
      "time_elapsed": 14.52,                          // 用时（秒）
      "timestamp": "2026-04-06 20:05:31"             // 时间戳
    },
    // ... 更多目标
  ],
  "position_history": [                               // 位置历史（每秒一次）
    {
      "time": 1.01,                                  // 时间（秒）
      "x": -6.79,                                    // X坐标
      "y": -3.21,                                    // Y坐标
      "yaw": -1.51                                   // 朝向（弧度）
    },
    // ... 更多位置记录
  ],
  "ros_died": false                                   // ROS是否崩溃
}
```

**用途**:
- 分析机器人轨迹
- 计算平均速度
- 检查目标到达精度
- 绘制位置-时间曲线

---

### 4. launch.log

**位置**: `<测试会话目录>/test_XX_shelf_XX/logs/launch.log`

**内容**: ROS launch文件的完整输出日志

**大小**: 通常 1MB - 5MB

**用途**:
- 调试ROS节点问题
- 查看MPC求解器状态
- 检查错误和警告信息
- 分析系统性能

**关键信息**:
- move_base路径规划状态
- MPC求解结果
- 目标发布和到达
- 错误和异常信息

---

### 5. rosbag/ (目录)

**位置**: `<测试会话目录>/test_XX_shelf_XX/rosbag/`

**内容**: ROS数据包文件（.bag格式）

**说明**:
- 目前为空目录
- 如果启用rosbag录制，会保存完整的ROS话题数据
- 可用于回放和分析

**启用方法**（需要修改launch文件）:
```xml
<node name="rosbag_record" pkg="rosbag" type="record"
      args="-O $(find test_results)/test_XX/rosbag/all_topics.bash
            -a" />
```

## 🔍 查看测试记录

### 快速查看最新测试

```bash
# 查看最新的测试会话目录
cd /home/dixon/Final_Project/catkin/test_results
ls -lt | head -5

# 查看最新的测试摘要
cat $(ls -t */final_report.txt | head -1)

# 查看最新测试的metrics
cat $(ls -t */test_01_shelf_01/metrics.json | head -1) | jq '.'
```

### 查看特定测试的结果

```bash
# 列出某个测试会话的所有测试
ls test_results/safe_regret_20260406_200458/

# 查看测试2的摘要
cat test_results/safe_regret_20260406_200458/test_02_shelf_02/test_summary.txt

# 查看测试2的指标（格式化JSON）
cat test_results/safe_regret_20260406_200458/test_02_shelf_02/metrics.json | jq '.'
```

### 查看launch日志

```bash
# 查看实时日志（tail）
tail -f test_results/safe_regret_20260406_200458/test_01_shelf_01/logs/launch.log

# 查看最后100行
tail -n 100 test_results/safe_regret_20260406_200458/test_01_shelf_01/logs/launch.log

# 搜索错误信息
grep -i "error\|failed" test_results/safe_regret_20260406_200458/test_01_shelf_01/logs/launch.log
```

## 📊 数据分析

### 使用Python分析metrics.json

```python
import json
import matplotlib.pyplot as plt

# 读取metrics
with open('test_results/safe_regret_20260406_200458/test_01_shelf_01/metrics.json') as f:
    metrics = json.load(f)

# 提取位置历史
positions = metrics['position_history']
times = [p['time'] for p in positions]
x_coords = [p['x'] for p in positions]
y_coords = [p['y'] for p in positions]

# 绘制轨迹
plt.figure(figsize=(10, 8))
plt.plot(x_coords, y_coords, 'b-', label='Robot Trajectory')
plt.xlabel('X (m)')
plt.ylabel('Y (m)')
plt.title('Robot Navigation Trajectory')
plt.grid(True)
plt.axis('equal')
plt.legend()
plt.savefig('trajectory.png')
plt.show()
```

### 计算性能指标

```python
# 计算平均速度
goals = metrics['goals_reached']
total_distance = 0
total_time = 0

for i in range(len(goals)):
    if i == 0:
        # 从起点到第一个目标
        continue
    else:
        # 目标之间的距离
        prev_goal = goals[i-1]['goal_position']
        curr_goal = goals[i]['goal_position']
        dist = ((curr_goal['x'] - prev_goal['x'])**2 +
                (curr_goal['y'] - prev_goal['y'])**2)**0.5
        total_distance += dist

avg_speed = total_distance / metrics['total_time']
print(f"平均速度: {avg_speed:.2f} m/s")
```

## 📈 测试历史管理

### 查看所有测试历史

```bash
# 按时间排序
ls -lt test_results/

# 按模型分组
ls -l test_results/ | grep safe_regret
ls -l test_results/ | grep tube_mpc

# 统计测试次数
ls -1 test_results/ | wc -l
```

### 清理旧测试记录

```bash
# 删除7天前的测试记录
find test_results/ -maxdepth 1 -type d -mtime +7 -exec rm -rf {} \;

# 只保留最新的10次测试
cd test_results/
ls -t | tail -n +11 | xargs rm -rf
```

### 归档测试记录

```bash
# 创建归档目录
mkdir -p test_results_archive/2026_04

# 移动特定月份的测试记录
mv test_results/safe_regret_202604* test_results_archive/2026_04/
mv test_results/tube_mpc_202604* test_results_archive/2026_04/
```

## 💾 磁盘空间管理

### 查看磁盘占用

```bash
# 查看test_results目录大小
du -sh test_results/

# 查看每个测试会话的大小
du -sh test_results/* | sort -h

# 查看最大的文件
find test_results/ -type f -exec du -h {} + | sort -rh | head -20
```

### 预估磁盘空间

**单次测试占用空间**（估算）:
- launch.log: 1-5 MB
- metrics.json: 10-50 KB
- test_summary.txt: 1 KB
- rosbag: 0-100 MB（如果启用）
- **总计**: 约 2-10 MB/测试

**3次测试会话**: 约 6-30 MB
**100次测试会话**: 约 200 MB - 1 GB

## 🎯 最佳实践

1. **定期清理**: 删除旧的测试记录，节省磁盘空间
2. **重要测试备份**: 将关键测试结果复制到其他位置
3. **使用有意义的目录名**: 可以手动重命名测试目录
4. **记录测试配置**: 在测试目录中添加README.md说明测试目的
5. **版本控制**: 将测试结果的分析脚本纳入版本控制

## 📝 相关文档

- `test/scripts/run_automated_test.py` - 测试脚本源代码
- `CLAUDE.md` - 项目总体说明
- `test/scripts/TEST_HANG_FIX.md` - 测试挂起修复文档

## 🔗 快速链接

- 查看最新测试: `cd /home/dixon/Final_Project/catkin/test_results && ls -lt | head -2`
- 查看所有测试: `ls -lh /home/dixon/Final_Project/catkin/test_results/`
- 磁盘占用: `du -sh /home/dixon/Final_Project/catkin/test_results/`

---

**更新时间**: 2026-04-06
**版本**: 1.0
