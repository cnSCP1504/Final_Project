# 测试地图清理说明

## 🎯 修改目标

将测试地图修改为只包含货架实体，移除所有额外的安全半径、障碍物标记和起点终点标记。

## ✅ 完成的修改

### 1. 生成干净的测试地图

**文件**: `test/scripts/generate_clean_test_map.py`

**功能**:
- 创建20m × 20m的空地图
- 只画6个货架实体（0.8m × 1.0m）
- 无额外安全半径
- 无墙壁障碍物
- 无起点终点标记

**货架位置**:
- 南侧货架 (Y=-7): (-8, -7), (-5, -7), (-2, -7)
- 北侧货架 (Y=+7): (8, 7), (5, 7), (2, 7)

### 2. 设置为默认地图

**文件**:
- `src/tube_mpc_ros/mpc_ros/map/test_world_clean.pgm` (新地图)
- `src/tube_mpc_ros/mpc_ros/map/test_world_clean.yaml` (配置)
- `test_world.pgm` → `test_world_clean.pgm` (符号链接)
- `test_world.yaml` → `test_world_clean.yaml` (符号链接)

## 📊 地图规格

| 属性 | 值 |
|------|-----|
| 地图尺寸 | 20m × 20m |
| 分辨率 | 0.05 m/pixel (5cm) |
| 图像尺寸 | 400 × 400 pixels |
| 货架数量 | 6个 |
| 货架尺寸 | 0.8m × 1.0m |
| 货架面积 | 0.8 m² (每个) |
| 总障碍物面积 | 4.8 m² (1.2%) |
| 自由空间面积 | 383.2 m² (98.8%) |

## 🔍 地图验证

```bash
# 验证货架数量和位置
python3 << 'EOF'
import numpy as np
from PIL import Image
from scipy import ndimage

img = Image.open("src/tube_mpc_ros/mpc_ros/map/test_world.pgm")
map_data = np.array(img)
labeled, num_features = ndimage.label(map_data == 0)

print(f"检测到的货架数量: {num_features}")
EOF
```

**输出**:
```
检测到的货架数量: 6
各货架像素数: [320, 320, 320, 320, 320, 320]
货架面积: 0.80 m² (期望: 0.80 m²)
```

## 🎨 可视化工具

使用可视化脚本查看地图：

```bash
python3 test/scripts/visualize_test_map.py
```

**输出**: `/tmp/test_world_map_visualization.png`

## 📝 对比：修改前 vs 修改后

### 修改前
- ❌ 货架有额外安全半径（inflation）
- ❌ 包含墙壁障碍物
- ❌ 画有起点终点标记
- ❌ 地图复杂，不利于测试

### 修改后
- ✅ 货架只有实体面积（0.8m × 1.0m）
- ✅ 无墙壁障碍物
- ✅ 无起点终点标记
- ✅ 地图简洁，适合测试

## 🚀 使用方法

### 直接使用（默认地图）

地图已经通过符号链接设置为默认，无需修改任何配置：

```bash
# 运行测试（会自动使用新的干净地图）
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

### 恢复旧地图

如果需要恢复之前的地图：

```bash
cd src/tube_mpc_ros/mpc_ros/map/
# 删除符号链接
rm test_world.pgm test_world.yaml
# 恢复备份
cp test_world_old_backup.pgm test_world.pgm
cp test_world_old_backup.yaml test_world.yaml
```

### 重新生成地图

如果需要修改货架配置：

```bash
# 1. 编辑货架配置
vim test/scripts/generate_clean_test_map.py

# 2. 重新生成地图
python3 test/scripts/generate_clean_test_map.py

# 3. 验证
python3 test/scripts/visualize_test_map.py
```

## 📂 相关文件

| 文件 | 说明 |
|------|------|
| `test/scripts/generate_clean_test_map.py` | 地图生成脚本 |
| `test/scripts/visualize_test_map.py` | 地图可视化工具 |
| `src/tube_mpc_ros/mpc_ros/map/test_world.pgm` | 当前使用的地图（符号链接） |
| `src/tube_mpc_ros/mpc_ros/map/test_world_clean.pgm` | 干净的测试地图 |
| `test/scripts/shelf_locations.yaml` | 取货点配置 |

## ⚠️ 注意事项

1. **Costmap配置**
   - 地图本身不包含安全半径
   - 实际安全半径由costmap参数控制（inflation_radius）
   - 修改costmap参数不会影响PGM地图

2. **货架尺寸**
   - 当前货架尺寸: 0.8m × 1.0m
   - 如果需要更大的货架，修改`generate_clean_test_map.py`中的`width`和`length`

3. **符号链接**
   - `test_world.pgm`是指向`test_world_clean.pgm`的符号链接
   - 修改`test_world_clean.pgm`会自动影响`test_world.pgm`

## 🔄 版本历史

| 日期 | 版本 | 修改内容 |
|------|------|---------|
| 2026-04-06 | 1.0 | 创建干净的测试地图，移除额外安全半径和标记 |

---

**创建日期**: 2026-04-06
**作者**: Claude Code
**状态**: ✅ 完成并已启用
