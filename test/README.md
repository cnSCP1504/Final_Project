# Test Directory - 整理后的测试文件结构

这个目录包含了所有测试相关的文件，按功能分类组织，保持项目根目录整洁。

## 📁 目录结构

### 📊 `analysis/` - 分析工具和数据
- `simple_analyze.py` - 简单的数据分析脚本
- `test_viz.png` - 测试可视化截图

### 📁 `archive/` - 历史文档
存放已完成的项目里程碑和修复文档：
- `BUILD_SUCCESS.md` - 构建成功记录
- `CRITICAL_ISSUE_FOUND.md` - 关键问题发现记录
- `FINAL_FIX_SUMMARY.md` - 最终修复总结
- `GITHUB_SAVE_COMPLETE.md` - GitHub保存完成记录
- `GIT_PUSH_SUCCESS.md` - Git推送成功记录
- `METRICS_IMPLEMENTATION_*.md` - 指标实现文档
- `PAPER_ANALYSIS_SUMMARY.md` - 论文分析总结
- `PHASE2_COMPLETION_SUMMARY.md` - Phase 2完成总结
- `ROBOT_MOTION_FIX_*.md` - 机器人运动修复文档
- `ROOT_CAUSE_FIX_COMPLETE.md` - 根因修复完成记录
- `SMART_TURNING_FIX.md` - 智能转向修复文档

### 🧪 `benchmark/` - 基准测试
- 原来的 `src/tube_mpc_benchmark/` 目录内容
- Tube MPC性能基准测试代码和数据

### 📖 `docs/` - 测试文档和指南
测试相关的文档和使用指南：
- `BENCHMARK_TEST_GUIDE.md` - 基准测试指南
- `GIT_UPLOAD_REPORT.md` - Git上传报告
- `NAVIGATION_TEST_REPORT.md` - 导航测试报告
- `TEST_RESULTS.md` - 测试结果总结
- `TUBE_MPC_NAV_TEST_REPORT.md` - Tube MPC导航测试报告
- `VISUALIZATION_SYSTEM_SUMMARY.md` - 可视化系统总结
- `VISUALIZATION_TEST_REPORT.md` - 可视化测试报告

### 🔧 `scripts/` - 测试脚本
所有测试和验证脚本：

#### Tube MPC测试
- `demo_visualization.sh` - 可视化演示
- `quick_tube_mpc_test.sh` - 快速Tube MPC测试
- `run_simple_test.sh` - 运行简单测试
- `run_tube_mpc_test.sh` - 运行Tube MPC测试
- `simple_tube_mpc_nav_test.sh` - 简单导航测试
- `test_simulator_movement.sh` - 仿真器运动测试

#### 参数调优脚本
- `auto_tune_mpc.sh` - 自动参数调优
- `guided_tuning.sh` - 引导式调优
- `one_click_tune.sh` - 一键调优
- `quick_fix_mpc.sh` - 快速MPC修复
- `start_tuning.sh` - 启动调优
- `switch_mpc_params.sh` - 切换MPC参数
- `turning_optimizer.sh` - 转向优化

#### 验证和诊断脚本
- `verify_benchmark.sh` - 验证基准测试
- `verify_fix.sh` - 验证修复
- `verify_simulator_data.sh` - 验证仿真数据
- `verify_visualization.sh` - 验证可视化
- `debug_tubempc.sh` - Tube MPC调试
- `diagnose_motion.sh` - 运动诊断

#### 测试工具
- `test_benchmark.sh` - 基准测试
- `test_cmd_vel.sh` - 速度命令测试
- `test_robot_motion.sh` - 机器人运动测试
- `test_viz_simple.sh` - 简单可视化测试
- `auto_test_and_tune.sh` - 自动测试和调优
- `test_and_monitor.sh` - 测试和监控
- `upgrade_to_new_ipopt.sh` - 升级Ipopt求解器

## 📋 使用指南

### 快速测试
```bash
# 快速Tube MPC测试
./test/scripts/quick_tube_mpc_test.sh

# 简单导航测试
./test/scripts/simple_tube_mpc_nav_test.sh
```

### 参数调优
```bash
# 一键调优
./test/scripts/one_click_tune.sh

# 切换参数配置
./test/scripts/switch_mpc_params.sh
```

### 验证系统
```bash
# 验证可视化
./test/scripts/verify_visualization.sh

# 验证仿真数据
./test/scripts/verify_simulator_data.sh
```

### 数据分析
```bash
# 分析测试数据
./test/analysis/simple_analyze.py
```

## 🎯 核心文件位置

项目核心文档保留在根目录：
- `CLAUDE.md` - 项目总体指导文档
- `PROJECT_ROADMAP.md` - 项目路线图
- `QUICK_START.md` - 快速开始指南

## 📝 说明

- 所有测试文件已按功能分类整理
- 历史文档归档到 `archive/` 目录
- 测试脚本集中到 `scripts/` 目录便于查找
- 分析工具和数据放在 `analysis/` 目录
- 保持根目录简洁，只保留核心文档

## 🔗 相关文档

- 主项目指南: `../CLAUDE.md`
- 项目路线图: `../PROJECT_ROADMAP.md`
- 快速开始: `../QUICK_START.md`
- Tube MPC可视化: `../src/tube_mpc_ros/TUBE_MPC_VISUALIZATION_GUIDE.md`

---

**最后更新**: 2026-03-18
**整理目的**: 保持项目根目录整洁，便于维护和查找文件
