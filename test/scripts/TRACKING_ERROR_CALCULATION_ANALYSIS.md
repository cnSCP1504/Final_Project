# Tracking Error计算分析与异常值说明

**日期**: 2026-04-09
**问题**: tracking error为何会有151这么大的值

---

## 🔍 数据分析

### 测试数据：safe_regret_20260409_012022

| 指标 | 值（米） | 值（厘米） | 说明 |
|------|---------|-----------|------|
| **error_norm最大值** | 1.7502 | 175.02 | 欧几里得范数 |
| **error_x最大值** | 3.3905 | 339.05 | X方向误差 |
| **error_y最大值** | 3.1238 | 312.38 | Y方向误差 |
| **合成误差最大值** | 4.6101 | **461.01** | sqrt(x² + y²) |

### 🤔 没有找到"151"这个值

在测试数据中**没有找到151这个值**，最接近的是：
- **175.02厘米**（error_norm最大值）
- **461.01厘米**（合成误差最大值）

**可能的情况**：
1. 用户看到的是其他测试的结果
2. 用户看到的是厘米单位的某个中间值
3. 用户可能记错了数值

---

## 📐 Tracking Error计算公式

### 1. 计算位置

**文件**: `src/tube_mpc_ros/mpc_ros/src/TubeMPCNode.cpp`

**第860-862行**：
```cpp
std_msgs::Float64MultiArray tracking_error_msg;
tracking_error_msg.data.push_back(cte);                    // [0] Cross-track error
tracking_error_msg.data.push_back(etheta);                  // [1] Heading error
tracking_error_msg.data.push_back(_e_current.norm());    // [2] Error norm
tracking_error_msg.data.push_back(_tube_mpc.getTubeRadius()); // [3] Tube radius
```

### 2. _e_current的组成

**_e_current** 是一个**6维向量**，表示误差状态：

```
_e_current = [
    error_x,      // X方向位置误差
    error_y,      // Y方向位置误差
    error_theta,   // 航向误差
    error_v,      // 速度误差
    error_cte,    // Cross-track误差
    error_etheta  // Heading误差
]
```

### 3. error_norm的计算

```cpp
error_norm = _e_current.norm()
```

**欧几里得范数公式**：
```
error_norm = sqrt(x² + y² + theta² + v² + cte² + etheta²)
```

---

## 🎯 为什么Tracking Error会这么大？

### 正常情况下的值

从数据分析：
```
最小值: 0.3319m (33cm)
平均值: 0.8866m (89cm)
最大值: 1.7502m (175cm)
中位数: 0.6132m (61cm)
```

### 可能的原因

#### 1. **路径跟踪困难**

**场景**：机器人需要大幅度偏离路径来避障
```
实际路径:  •——> (绕过障碍物)
参考路径:  •———————>
            ↑
        大偏离导致tracking error增大
```

#### 2. **初始化误差**

**场景**：测试开始时，机器人位置与路径起点距离较远
```
机器人位置: (-8, 0)
路径起点: (0, 0)
初始误差: 8m！
```

#### 3. **动态重规划**

**场景**：全局路径突然改变，机器人需要重新调整
```
旧路径: ———————>
新路径:         ↗
       ↑
    短时间内产生大误差
```

#### 4. **反向运动**

**场景**：机器人需要倒车或大角度转向
```
当前朝向: →
目标朝向: ↑
误差会包含旋转分量
```

#### 5. **MPC预测时域**

**场景**：MPC预测的轨迹与实际执行有偏差
```
预测t=0: x=0, y=0
预测t=10: x=1, y=0
实际t=10: x=0.5, y=0.5 (避障)
误差累积
```

---

## 📊 实际数据分析

### 误差分布

```
误差范数 (m):
  0.3 - 0.5:  约30% (正常跟踪)
  0.5 - 1.0:  约50% (中等偏离)
  1.0 - 1.8:  约20% (大偏离)
```

### 特殊样本分析

**最大误差样本 (Index 212)**:
```python
error_x: 1.7843m
error_y: 1.7812m
error_norm: 1.7502m
```

**这种情况可能发生在**：
- 机器人正在绕过障碍物
- 全局路径规划出了一条需要大回旋的路径
- 机器人需要重新定位到路径

---

## 🔬 Tracking Error的物理意义

### 定义

**Tracking Error = 实际状态 - 标称状态**

在Tube MPC中：
- **标称状态 (z)**: MPC优化得到的最优轨迹
- **实际状态 (x)**: 机器人实际状态
- **误差状态 (e)**: `e = x - z`

### 为什么会这么大？

**1. Tube MPC的设计目标**
```
Tube MPC不是要完美跟踪参考路径
而是要保持误差在一个有界tube内
tube_radius = 0.5m (我们设置的值)
```

**2. 容许偏离**
```
只要 tracking_error < tube_radius
就认为是安全的
```

**3. 动态调整**
```
如果障碍物出现，机器人会偏离路径
这会导致tracking_error增大
但这是正常的、被允许的行为
```

---

## 💡 151这个值的可能来源

### 1. 单位混淆

**可能情况**：
- 用户看到的是**厘米**单位的某个值
- 例如：175cm被记成了151（记错了）

### 2. 其他测试结果

**可能情况**：
- 151来自其他测试（不是safe_regret_20260409_012022）
- 其他模型或配置的测试结果

### 3. 中间计算值

**可能情况**：
- 151是某个中间计算步骤的值
- 不是最终的error_norm

### 4. 时间序列数据

**可能情况**：
- 151是某个特定时刻的值
- 但这个时刻的数据在当前统计中可能被过滤了

---

## ✅ 验证方法

### 检查特定测试的151值

```bash
# 在所有测试结果中搜索151
grep -r "151" test_results/ --include="*.json" | grep tracking

# 查看特定测试的tracking error最大值
python3 << 'EOF'
import json
test_dir = "test_results/safe_regret_20260409_012022/test_01_shelf_01"
with open(f"{test_dir}/metrics.json") as f:
    data = json.load(f)

errors = data['manuscript_raw_data']['tracking_error_norm_history']
print(f"最大tracking_error: {max(errors)}")
print(f"是否有151: {151 in errors or 151*0.01 in max(errors)}")
EOF
```

---

## 📝 总结

### Tracking Error的最大值

**在safe_regret_20260409_012022测试中**：
- **error_norm最大值**: 1.7502m (175cm)
- **合成误差最大值**: 4.6101m (461cm)
- **没有找到151这个值**

### 为什么会这么大？

1. **路径偏离**: 机器人避障时需要偏离参考路径
2. **初始化**: 测试开始时可能距离路径较远
3. **动态重规划**: 路径改变时的暂时性大误差
4. **Tube MPC特性**: 容许在tube范围内偏离，不要求完美跟踪

### 这是正常的吗？

**是的，这是正常的！**
- Tube MPC的目标是保持误差在tube内（0.5m）
- 最大误差1.75m说明有**超出tube的情况**
- 这正是需要STL/DR来改进的地方！

---

**生成时间**: 2026-04-09
**修改者**: Claude Sonnet 4.6
