# 测试脚本 "all" 模式功能说明

**日期**: 2026-04-09
**功能**: 添加 `--model all` 选项，自动测试所有模型并保存数据

---

## 🎯 功能描述

新增 `--model all` 模式，可以自动循环测试所有4个模型（tube_mpc, stl, dr, safe_regret），每个模型都使用指定的货架数量进行测试。

### 测试的模型列表
1. **tube_mpc** - 基础Tube MPC控制器
2. **stl** - STL增强的Tube MPC
3. **dr** - 分布式鲁棒（DR）增强的Tube MPC
4. **safe_regret** - 完整的Safe-Regret MPC（STL + DR + Reference Planner）

---

## 📝 使用方法

### 完整测试（所有模型 × 10个货架）
```bash
# 测试所有模型，每个模型测试10个货架（默认）
python3 test/scripts/run_automated_test.py --model all

# 等价于
python3 test/scripts/run_automated_test.py --model all --shelves 10
```

**测试数量**: 4个模型 × 10个货架 = **40次测试**

**预计时间**:
- 有Gazebo+RViz: 约 40 × 3分钟 = **2小时**
- 无Gazebo+RViz: 约 40 × 2分钟 = **1.3小时**

### 快速测试（所有模型 × 1个货架）
```bash
# 快速验证所有模型是否正常工作
python3 test/scripts/run_automated_test.py --model all --shelves 1 --no-viz
```

**测试数量**: 4个模型 × 1个货架 = **4次测试**

**预计时间**: 约 4 × 2分钟 = **8分钟**

### 自定义货架数量
```bash
# 测试所有模型，每个模型测试5个货架
python3 test/scripts/run_automated_test.py --model all --shelves 5
```

**测试数量**: 4个模型 × 5个货架 = **20次测试**

---

## 📊 数据保存结构

### 结果目录结构
```
test_results/
├── tube_mpc_YYYYMMDD_HHMMSS/
│   ├── test_01_shelf_01/
│   ├── test_02_shelf_02/
│   ├── ...
│   └── test_10_shelf_10/
│   └── final_summary.txt
├── stl_YYYYMMDD_HHMMSS/
│   ├── test_01_shelf_01/
│   ├── ...
│   └── final_summary.txt
├── dr_YYYYMMDD_HHMMSS/
│   ├── ...
│   └── final_summary.txt
└── safe_regret_YYYYMMDD_HHMMSS/
    ├── ...
    └── final_summary.txt
```

### 每个模型的测试结果
每个模型的测试结果独立保存在各自的目录中：
- **目录命名**: `{model_name}_{timestamp}`
- **测试子目录**: `test_XX_shelf_XX/`
- **指标数据**: `metrics.json`
- **测试摘要**: `test_summary.txt`
- **日志文件**: `logs/launch.log`

---

## 🔧 实现细节

### 代码修改位置

**文件**: `test/scripts/run_automated_test.py`

**修改1**: 添加'all'选项（第2182-2185行）
```python
parser.add_argument('--model',
                   choices=['tube_mpc', 'stl', 'dr', 'safe_regret', 'all'],
                   default='tube_mpc',
                   help='模型类型 (默认: tube_mpc, "all"测试所有模型)')
```

**修改2**: 处理'all'模式（第2243-2303行）
```python
if args.model == 'all':
    all_models = ['tube_mpc', 'stl', 'dr', 'safe_regret']

    # 循环测试所有模型
    for model in all_models:
        # 创建独立的参数副本
        model_args = copy.deepcopy(args)
        model_args.model = model

        # 运行测试
        runner = AutomatedTestRunner(model_args)
        success = runner.run_tests()

        # 清理ROS进程
        runner.cleanup_ros_processes()

        # 等待间隔时间
        time.sleep(args.interval)
```

### 关键特性

1. **独立结果目录**:
   - 每个模型的测试结果保存在独立的时间戳目录中
   - 避免不同模型的数据混淆

2. **完整的数据收集**:
   - 每个模型都收集完整的manuscript metrics
   - 包括STL robustness, DR margins, tracking error等

3. **自动清理**:
   - 每个模型测试完成后自动清理ROS进程
   - 避免进程冲突和资源泄漏

4. **进度跟踪**:
   - 显示当前测试进度（1/4, 2/4, 3/4, 4/4）
   - 实时显示测试结果汇总

5. **错误处理**:
   - 单个模型失败不影响其他模型的测试
   - 记录每个模型的测试状态（SUCCESS/FAILED/ERROR/INTERRUPTED）

---

## 📈 测试结果汇总

### 控制台输出示例
```
======================================================================
🚀 ALL MODE: 测试所有模型
======================================================================
模型列表: tube_mpc, stl, dr, safe_regret
每个模型测试货架数: 10
总测试数: 4 × 10 = 40
======================================================================

======================================================================
📊 开始测试模型: tube_mpc (1/4)
======================================================================
[测试进行中...]

⏳ 等待 5 秒后开始下一个模型测试...

======================================================================
📊 开始测试模型: stl (2/4)
======================================================================
[测试进行中...]

...

======================================================================
📈 ALL MODE 测试结果汇总
======================================================================
  ✅ tube_mpc: SUCCESS
  ✅ stl: SUCCESS
  ✅ dr: SUCCESS
  ✅ safe_regret: SUCCESS
======================================================================

总体结果: ✅ 全部成功
======================================================================
```

### 最终报告
每个模型测试完成后都会生成：
- ✅ `final_summary.txt` - 测试摘要
- ✅ `metrics.json` - 完整指标数据
- ✅ `manuscript_metrics_report.txt` - Manuscript指标报告
- ✅ `figures/` - 可视化图表

---

## ⚙️ 高级用法

### 调整模型间隔时间
```bash
# 默认间隔5秒，可以自定义
python3 test/scripts/run_automated_test.py --model all --interval 10
```

### 组合其他参数
```bash
# 无头模式 + 快速测试
python3 test/scripts/run_automated_test.py --model all --shelves 1 --headless

# 自定义超时时间
python3 test/scripts/run_automated_test.py --model all --shelves 5 --timeout 300

# 自定义结果目录
python3 test/scripts/run_automated_test.py --model all --results-dir /path/to/results
```

---

## 🎯 典型使用场景

### 场景1: 完整性能测试
```bash
# 测试所有模型的完整性能（10个货架）
python3 test/scripts/run_automated_test.py --model all --shelves 10 --no-viz
```

**目的**: 收集所有模型的完整性能数据，用于论文实验对比

### 场景2: 快速验证
```bash
# 快速验证所有模型是否正常工作
python3 test/scripts/run_automated_test.py --model all --shelves 1 --no-viz
```

**目的**: 在修改代码后快速验证所有模型是否正常

### 场景3: 定期回归测试
```bash
# 定期运行完整测试套件
python3 test/scripts/run_automated_test.py --model all --shelves 10 --no-viz
```

**目的**: 确保代码修改没有破坏现有功能

---

## 📝 注意事项

1. **测试时间**:
   - 完整测试（4模型 × 10货架）可能需要**2小时**
   - 建议使用`--no-viz`或`--headless`加快测试速度

2. **磁盘空间**:
   - 每个模型的测试结果约100-200MB
   - 完整测试可能需要**800MB-1GB**磁盘空间

3. **资源清理**:
   - 每个模型测试后会自动清理ROS进程
   - 如果手动中断（Ctrl+C），也会自动清理

4. **失败处理**:
   - 单个模型失败不会中断整个测试流程
   - 最终汇总会显示哪些模型成功/失败

---

## ✅ 验证方法

运行测试后检查：
```bash
# 1. 检查是否生成了4个模型的结果目录
ls -lt test_results/ | head -5

# 2. 检查每个模型的结果完整性
for model in tube_mpc stl dr safe_regret; do
    echo "=== $model ==="
    ls -d test_results/${model}_*/
done

# 3. 检查测试摘要
for model in tube_mpc stl dr safe_regret; do
    echo "=== $model ==="
    cat test_results/${model}_*/final_summary.txt
done
```

---

**生成时间**: 2026-04-09
**修改文件**: `test/scripts/run_automated_test.py`
**新功能**: `--model all` 模式
