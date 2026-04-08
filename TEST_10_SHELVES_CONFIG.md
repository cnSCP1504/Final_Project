# 10个取货点配置说明

**更新日期**: 2026-04-07
**配置文件**: `test/scripts/shelf_locations.yaml`
**测试脚本**: `test/scripts/run_automated_test.py`

---

## 📍 取货点布局

### 地图范围: 20m × 20m
- **X轴**: -10m 到 +10m
- **Y轴**: -10m 到 +10m
- **起点/装货区**: (-8.0, 0.0)
- **终点/卸货区**: (8.0, 0.0)

### 取货点分布

```
北侧取货区 (Y = +7.0m)
┌────────────────────────────────────────────────────┐
│                                                    │
│  shelf_06     shelf_07     shelf_08     shelf_09  │  shelf_10
│  (-6.5, 7.0)  (-3.5, 7.0)  (0.0, 7.0)   (3.5, 7.0)  (6.5, 7.0)
│     📦          📦          📦          📦          📦
│                                                    │
├────────────────────────────────────────────────────┤
│                    中间区域                         │
│          起点(-8,0)  →  终点(8,0)                  │
├────────────────────────────────────────────────────┤
│                                                    │
│  shelf_01     shelf_02     shelf_03     shelf_04  │  shelf_05
│  (-6.5, -7.0) (-3.5, -7.0) (0.0, -7.0)  (3.5, -7.0) (6.5, -7.0)
│     📦          📦          📦          📦          📦
│                                                    │
└────────────────────────────────────────────────────┘
南侧取货区 (Y = -7.0m)
```

---

## 📋 详细坐标列表

| # | ID | 名称 | X坐标 | Y坐标 | 描述 |
|---|----|----|-------|-------|------|
| 1 | shelf_01 | 货架01 - 南侧西 | -6.5 | -7.0 | 南侧货架1和2之间的取货点 |
| 2 | shelf_02 | 货架02 - 南侧中东 | -3.5 | -7.0 | 南侧货架2和3之间的取货点 |
| 3 | shelf_03 | 货架03 - 南侧东 | 0.0 | -7.0 | 南侧货架3右侧的取货点 |
| 4 | shelf_04 | 货架04 - 南侧东延伸 | 3.5 | -7.0 | 南侧更东侧的取货点 |
| 5 | shelf_05 | 货架05 - 南侧最东 | 6.5 | -7.0 | 南侧最东侧的取货点 |
| 6 | shelf_06 | 货架06 - 北侧西 | -6.5 | **+7.0** | 北侧西侧的取货点（对应shelf_01的Y镜像） |
| 7 | shelf_07 | 货架07 - 北侧中 | -3.5 | **+7.0** | 北侧中部的取货点（对应shelf_02的Y镜像） |
| 8 | shelf_08 | 货架08 - 北侧中 | 0.0 | **+7.0** | 北侧中部的取货点（对应shelf_03的Y镜像） |
| 9 | shelf_09 | 货架09 - 北侧东 | 3.5 | **+7.0** | 北侧东侧的取货点（对应shelf_04的Y镜像） |
| 10 | shelf_10 | 货架10 - 北侧最东 | 6.5 | **+7.0** | 北侧最东侧的取货点（对应shelf_05的Y镜像） |

**统计**:
- 南侧取货点 (Y=-7.0): 5个
- 北侧取货点 (Y=+7.0): 5个
- **总计**: 10个

---

## 🚀 使用方法

### 方法1: 使用快速测试脚本

```bash
# 测试所有模式（各10个取货点）
./test/scripts/test_all_10_shelves.sh tube_mpc
./test/scripts/test_all_10_shelves.sh stl
./test/scripts/test_all_10_shelves.sh dr
./test/scripts/test_all_10_shelves.sh safe_regret
```

### 方法2: 直接使用Python脚本

```bash
# 测试所有10个取货点
python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 10 \
    --timeout 240

# 测试前5个取货点（南侧）
python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 5 \
    --timeout 240

# 测试单个取货点
python3 test/scripts/run_automated_test.py \
    --model tube_mpc \
    --shelves 1 \
    --timeout 180
```

### 方法3: 无头模式（无可视化）

```bash
# 快速测试（无Gazebo和RViz）
python3 test/scripts/run_automated_test.py \
    --model safe_regret \
    --shelves 10 \
    --headless \
    --timeout 240
```

---

## ⏱️ 时间估算

### 单个取货点测试时间
- **Tube MPC**: 约60-90秒
- **STL模式**: 约90-120秒
- **DR模式**: 约90-120秒
- **Safe-Regret**: 约90-120秒

### 总测试时间（10个取货点）
- **Tube MPC**: 约15-25分钟
- **STL模式**: 约20-30分钟
- **DR模式**: 约20-30分钟
- **Safe-Regret**: 约20-30分钟

### 所有模式总时间
- **4种模式 × 10个取货点**: 约**1.5-2小时**

---

## 📊 测试数据

### 每个取货点收集的数据

| 数据类型 | Tube MPC | STL | DR | Safe-Regret |
|---------|----------|-----|----|----|
| MPC求解时间 | ✅ | ✅ | ✅ | ✅ |
| MPC可行性 | ✅ | ✅ | ✅ | ✅ |
| 跟踪误差 | ✅ | ✅ | ✅ | ✅ |
| STL robustness | ❌ | ✅ | ❌ | ✅ |
| STL budget | ❌ | ✅ | ❌ | ✅ |
| DR margins | ❌ | ❌ | ✅ | ✅ |
| 安全违背率 | ✅ | ✅ | ✅ | ✅ |
| Regret metrics | ❌ | ❌ | ❌ | ✅ |

### 预期数据量（10个取货点）

- **MPC数据**: 约6000-9000个样本点
- **STL数据**: 约4000-6000个样本点（STL和Safe-Regret模式）
- **DR数据**: 约20000-30000个样本点（DR和Safe-Regret模式）
- **总数据文件大小**: 约10-50MB

---

## ✅ 验证测试

### 配置验证

```bash
# 验证配置文件加载
python3 << 'EOF'
import yaml
with open('/home/dixon/Final_Project/catkin/test/scripts/shelf_locations.yaml') as f:
    config = yaml.safe_load(f)

shelves = config['shelves']
print(f"✅ 总取货点数量: {len(shelves)}")
print(f"✅ 南侧取货点: {sum(1 for s in shelves if s['y'] == -7.0)} 个")
print(f"✅ 北侧取货点: {sum(1 for s in shelves if s['y'] == 7.0)} 个")
EOF
```

### 快速单点测试

```bash
# 测试第一个取货点（验证配置）
python3 test/scripts/run_automated_test.py \
    --model tube_mpc \
    --shelves 1 \
    --timeout 120 \
    --visualization
```

---

## 📝 修改记录

### 2026-04-07 - 初始配置
- ✅ 添加5个北侧取货点（shelf_06到shelf_10）
- ✅ 更新测试脚本支持1-10个取货点
- ✅ 创建快速测试脚本 `test_all_10_shelves.sh`
- ✅ 验证配置文件加载正常

### 修改的文件
1. `test/scripts/shelf_locations.yaml` - 添加5个北侧取货点
2. `test/scripts/run_automated_test.py` - 更新参数范围（1-10）
3. `test/scripts/test_all_10_shelves.sh` - 新建快速测试脚本

---

## 🎯 测试目标

### 主要目标
1. ✅ 验证所有模式在不同位置的导航性能
2. ✅ 收取不同位置的STL robustness数据
3. ✅ 测试DR约束在不同场景下的有效性
4. ✅ 评估Safe-Regret MPC在复杂任务中的表现

### 预期结果
- 每个模式完成10个取货点的导航任务
- 收集完整的性能指标数据
- 验证系统在地图不同区域的鲁棒性
- 对比4种模式在不同位置的性能差异

---

**状态**: ✅ **配置完成，可以开始测试**
**建议**: 先运行1-2个取货点验证配置，然后再运行完整10个取货点测试
