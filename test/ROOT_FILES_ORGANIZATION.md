# 根目录测试文件整理索引

## 整理时间
2026-04-05

## 文件分类

### 1. 测试脚本 -> test/scripts/root_tests/
所有从根目录移动来的测试脚本。

### 2. 诊断脚本 -> test/diagnostics/root_scripts/
用于诊断和调试的脚本。

### 3. 修复报告 -> test/reports/fixes/
各类bug修复和改进的详细报告。

### 4. 测试报告 -> test/reports/tests/
功能测试和集成测试的报告。

### 5. 指南文档 -> test/docs/guides/
系统使用和配置的指南。

### 6. 总结文档 -> test/docs/
实现总结、技术文档等。

## 查找文件

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

## 运行测试

### 运行根目录测试脚本
```bash
# 物流任务测试
test/scripts/root_tests/test_logistics_task.sh

# 原地旋转测试
test/scripts/root_tests/test_strict_rotation_mode.sh

# 大角度转向测试
test/scripts/root_tests/test_large_angle_turn.sh
```

### 查看文档
```bash
# 查看物流任务指南
cat test/docs/guides/LOGISTICS_TASK_GUIDE.md

# 查看最新修复报告
ls -lt test/reports/fixes/ | head -5
```
