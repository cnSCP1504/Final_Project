# Test 文件夹补充说明

## 🎯 根目录文件整理（2026-04-05）

所有根目录下的测试文件已成功整理到test文件夹的相应子目录中。

## 📊 整理统计

- **总文件数**: 51个
- **测试脚本**: 19个 → `test/scripts/root_tests/`
- **诊断脚本**: 2个 → `test/diagnostics/root_scripts/`
- **修复报告**: 12个 → `test/reports/fixes/`
- **测试报告**: 5个 → `test/reports/tests/`
- **指南文档**: 4个 → `test/docs/guides/`
- **其他文档**: 9个 → `test/docs/`

## 📁 新增目录结构

### 1. test/scripts/root_tests/
从根目录移动的测试脚本：
- `test_logistics_task.sh` - 物流任务测试 ⭐
- `test_large_angle_turn.sh` - 大角度转向测试
- `test_in_place_rotation_simple.sh` - 原地旋转测试
- `test_random_obstacles.sh` - 随机障碍物测试
- 其他15个测试脚本...

### 2. test/diagnostics/root_scripts/
从根目录移动的诊断脚本：
- `diagnose_etheta_oscillation.sh` - 角度振荡诊断
- `get_gazebo_camera_pose.sh` - Gazebo相机位姿获取

### 3. test/reports/fixes/
从根目录移动的修复报告：
- `ANGULAR_VELOCITY_FIX_FINAL.md` - 最新角速度修复 ⭐
- `DR_CONSTRAINT_FIX_REPORT.md` - DR约束修复
- `ROTATION_DIRECTION_LOCK_FIX_REPORT.md` - 旋转方向锁定
- 其他9个修复报告...

### 4. test/reports/tests/
从根目录移动的测试报告：
- `INTEGRATION_TEST_REPORT_FINAL.md` - 集成测试最终报告
- `METRICS_COLLECTION_TEST_REPORT.md` - 指标收集测试
- 其他3个测试报告...

### 5. test/docs/guides/
从根目录移动的指南文档：
- `LOGISTICS_TASK_GUIDE.md` - 物流任务使用指南 ⭐
- `INTEGRATION_TESTING_GUIDE.md` - 集成测试指南
- `METRICS_COLLECTION_GUIDE.md` - 指标收集指南
- `TUBE_MPC_METRICS_GUIDE.md` - Tube MPC指标指南

### 6. test/docs/
从根目录移动的其他文档：
- `LOGISTICS_TASK_UPDATE_SUMMARY.md` - 物流任务更新总结
- `METRICS_SYSTEM_IMPLEMENTATION_SUMMARY.md` - 指标系统实现
- 其他7个文档...

## 🚀 快速使用

### 运行从根目录移动的测试

```bash
# 物流任务测试（重要）
./test/scripts/root_tests/test_logistics_task.sh

# 原地旋转测试
./test/scripts/root_tests/test_in_place_rotation_simple.sh

# 大角度转向测试
./test/scripts/root_tests/test_large_angle_turn.sh

# 随机障碍物测试
./test/scripts/root_tests/test_random_obstacles.sh
```

### 查看从根目录移动的文档

```bash
# 查看物流任务指南
cat test/docs/guides/LOGISTICS_TASK_GUIDE.md

# 查看最新修复报告
ls -lt test/reports/fixes/ | head -5

# 查看文件索引
cat test/ROOT_FILES_ORGANIZATION.md
```

## 📝 重点文件

### 必读文档
1. **物流任务系统**:
   - `test/docs/guides/LOGISTICS_TASK_GUIDE.md`
   - `test/docs/LOGISTICS_TASK_UPDATE_SUMMARY.md`

2. **最新修复**:
   - `test/reports/fixes/ANGULAR_VELOCITY_FIX_FINAL.md`

3. **文件索引**:
   - `test/ROOT_FILES_ORGANIZATION.md`
   - `FILE_ORGANIZATION_COMPLETE.md` (根目录)

## 🔍 文件查找

### 查找所有测试脚本
```bash
find test/scripts/root_tests/ -name "*.sh"
```

### 查找所有修复报告
```bash
find test/reports/fixes/ -name "*.md"
```

### 查找所有指南
```bash
find test/docs/guides/ -name "*.md"
```

### 查找特定主题的文件
```bash
# 查找所有关于rotation的文件
find test/ -name "*rotation*" -o -name "*ROTATION*"

# 查找所有关于fix的报告
find test/reports/fixes/ -name "*.md"
```

## 📖 维护说明

### 添加新的测试脚本
```bash
# 将新脚本移动到合适的位置
mv your_new_test.sh test/scripts/root_tests/

# 更新文件索引
./test/ORGANIZE_ROOT_FILES.sh
```

### 添加新的修复报告
```bash
# 将新报告移动到fixes目录
mv your_fix_report.md test/reports/fixes/

# 更新文件索引
./test/ORGANIZE_ROOT_FILES.sh
```

### 添加新的指南文档
```bash
# 将新指南移动到guides目录
mv your_guide.md test/docs/guides/

# 更新文件索引
./test/ORGANIZE_ROOT_FILES.sh
```

## 🔄 更新索引

如果添加了新文件，运行整理脚本更新索引：
```bash
./test/ORGANIZE_ROOT_FILES.sh
```

## 📌 相关文档

- **项目根目录**: `README.md`
- **测试系统**: `test/README.md`
- **文件索引**: `test/ROOT_FILES_ORGANIZATION.md`
- **整理总结**: `FILE_ORGANIZATION_COMPLETE.md`

---

**整理时间**: 2026-04-05
**整理脚本**: `test/ORGANIZE_ROOT_FILES.sh`
**状态**: ✅ 完成
