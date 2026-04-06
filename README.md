# Safe-Regret MPC 项目

基于时序逻辑、分布鲁棒性和遗憾分析的统一安全机器人控制框架。

## 📁 项目结构

```
catkin/
├── CLAUDE.md                              # Claude Code 使用指南 ⭐
├── PROJECT_ROADMAP.md                     # 项目路线图 ⭐
├── README.md                              # 本文件
│
├── src/                                   # 源代码
│   ├── tube_mpc_ros/                     # Tube MPC 实现
│   ├── safe_regret_mpc/                  # Safe-Regret MPC 统一框架
│   ├── stl_monitor/                      # STL 监控模块
│   ├── dr_tightening/                    # DR 约束收紧模块
│   └── reference_planner/                # 参考规划器
│
├── test/                                  # 测试和文档 ⭐
│   ├── scripts/                          # 测试脚本
│   ├── docs/                             # 文档
│   ├── reports/                          # 报告
│   └── diagnostics/                      # 诊断工具
│
└── latex/                                # 论文源文件
    └── manuscript.tex                    # Safe-Regret MPC 论文
```

## 🚀 快速开始

### 1. 构建项目
```bash
cd /home/dixon/Final_Project/catkin
catkin_make
source devel/setup.bash
```

### 2. 运行测试
```bash
# 清理之前的ROS进程
killall -9 roslaunch rosmaster roscore gzserver gzclient python

# 启动物流任务测试
roslaunch safe_regret_mpc safe_regret_mpc_test.launch
```

### 3. 运行自动化测试
```bash
# 物流任务批量测试
./test/scripts/root_tests/test_logistics_task.sh

# 原地旋转测试
./test/scripts/root_tests/test_large_angle_turn.sh
```

## 📚 核心文档

### 必读文档
1. **CLAUDE.md** - Claude Code 使用指南和项目说明
2. **PROJECT_ROADMAP.md** - 项目实现路线图和进度
3. **test/README.md** - 测试系统说明

### 最新更新
1. **物流任务系统**:
   - `test/docs/guides/LOGISTICS_TASK_GUIDE.md` - 使用指南
   - `test/docs/LOGISTICS_TASK_UPDATE_SUMMARY.md` - 更新总结

2. **角速度修复**:
   - `test/reports/fixes/ANGULAR_VELOCITY_FIX_FINAL.md` - 最新修复

3. **文件整理**:
   - `FILE_ORGANIZATION_COMPLETE.md` - 整理总结
   - `test/ROOT_FILES_ORGANIZATION.md` - 文件索引

## 🧪 测试系统

所有测试文件已整理到 `test/` 文件夹：

### 测试脚本
- 位置: `test/scripts/root_tests/`
- 示例: `test_logistics_task.sh`, `test_large_angle_turn.sh`

### 测试报告
- 位置: `test/reports/`
- 子目录: `fixes/`, `tests/`, `phase2/`, `phase3/`

### 文档
- 位置: `test/docs/`
- 子目录: `guides/`, `summaries/`

### 诊断工具
- 位置: `test/diagnostics/root_scripts/`

详细索引请查看: `test/ROOT_FILES_ORGANIZATION.md`

## 📊 项目状态

### 实现阶段
- ✅ Phase 1: Tube MPC (Complete)
- ✅ Phase 2: STL Integration (Complete)
- ✅ Phase 3: DR Constraints (Complete)
- ✅ Phase 4: Regret Analysis (Complete)
- ✅ Phase 5: System Integration (Complete)
- ✅ Phase 6: Experimental Validation (In Progress)

### 最新功能
- ✅ 物流任务模式（取货点 → 卸货点）
- ✅ 原地旋转角速度固定
- ✅ 地图障碍物缩小工具
- ✅ 自动化测试脚本

## 🔧 常用命令

### 清理ROS进程
```bash
killall -9 roslaunch rosmaster roscore gzserver gzclient python
```

### 查看节点图
```bash
rosrun rqt_graph rqt_graph
```

### 查看话题
```bash
rostopic list
rostopic echo /cmd_vel
rostopic echo /odom
```

### 查看TF树
```bash
rosrun tf view_frames
```

## 📖 论文

- **位置**: `latex/manuscript.tex`
- **标题**: "Safe-Regret MPC for Temporal-Logic Tasks in Stochastic, Partially Observable Robots"
- **状态**: 完整初稿 (1372行)

## 🤝 贡献指南

### 添加新测试
1. 将测试脚本放入 `test/scripts/root_tests/`
2. 添加文档到 `test/docs/guides/`
3. 运行 `test/ORGANIZE_ROOT_FILES.sh` 更新索引

### 添加新功能
1. 更新 `PROJECT_ROADMAP.md`
2. 编写单元测试
3. 更新文档

### 报告问题
1. 查看现有修复报告: `test/reports/fixes/`
2. 创建新的修复报告
3. 提交issue或PR

## 📞 技术支持

### 文档
- 项目说明: `CLAUDE.md`
- 测试指南: `test/README.md`
- 文件索引: `test/ROOT_FILES_ORGANIZATION.md`

### 诊断工具
- 角度振荡诊断: `test/diagnostics/root_scripts/diagnose_etheta_oscillation.sh`
- Gazebo相机: `test/diagnostics/root_scripts/get_gazebo_camera_pose.sh`

## 📝 许可证

本研究项目基于以下论文：
- "Safe-Regret MPC for Temporal-Logic Tasks in Stochastic, Partially Observable Robots"

---

**最后更新**: 2026-04-05
**版本**: 1.0
**状态**: 活跃开发中
