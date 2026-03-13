# TubeMPC_Node 崩溃问题 - 系统环境诊断报告

## 🔍 问题分析总结

**问题**: TubeMPC_Node 进程崩溃，退出代码 -11 (SIGSEGV 段错误)
**触发时机**: 设置导航目标后立即崩溃
**持续时间**: 恢复代码版本后问题依然存在

## 🎯 根本原因 - 系统环境问题

### 关键发现

#### 1. **Ipopt库版本冲突** (主要问题)

**今天(2026-03-10 00:41)的系统变更记录**:
```
Mar 10 00:41:44 sudo[27545]: make install
Mar 10 00:41:58 sudo[28262]: ln -s /usr/local/lib/libipopt.so.3 /usr/lib/libipopt.so.3
```

**当前系统Ipopt库状态**:
```
/lib/libipopt.so.1 -> libipopt.so.1.9.9 (coinor版本 3.11.9, 系统包管理器安装)
/lib/libipopt.so.3 -> /usr/local/lib/libipopt.so.3.14.20 (自定义编译版本, 今天安装)
/usr/local/lib/libipopt.so.3 -> libipopt.so.3.14.20 (新版本, 今天00:41安装)
/usr/local/lib/libipopt.so.3.14.17 (旧版本, Mar 6编译)
```

**版本冲突分析**:
- **程序链接**: `libipopt.so.1` (版本 3.11.9)
- **新安装**: `libipopt.so.3` (版本 3.14.20)
- **ABI不兼容**: 两个主要版本间的ABI变化导致运行时崩溃

#### 2. **相关依赖库冲突**

**MUMPS求解器库冲突**:
```
Mar 10 00:41:56 sudo[28257]: ln -s /usr/local/lib/libcoinmumps.so.3 /usr/lib/libcoinmumps.so.3
Mar 10 41:56 sudo[28260]: ln -s /usr/local/lib/libcoinhsl.so.2 /usr/lib/libcoinhsl.so.2
```

**运行时依赖检查**:
```
libdmumps_seq-5.2.1.so => /lib/x86_64-linux-gnu/libdmumps_seq-5.2.1.so
libmumps_common_seq-5.2.1.so => /lib/x86_64-linux-gnu/libmumps_common_seq-5.2.1.so
```

**问题**: 系统MUMPS库(5.2.1)与新版Ipopt可能不兼容

#### 3. **CMakeLists配置问题**

**问题配置**:
```cmake
include_directories(/usr/include)
link_directories(/usr/lib)
TARGET_LINK_LIBRARIES(tube_TubeMPC_Node ipopt ${catkin_LIBRARIES})
```

**问题描述**:
- 硬编码`/usr/lib`搜索路径
- 未指定Ipopt的完整路径
- 链接器可能找到错误的Ipopt版本

## 🔧 问题形成的完整过程

### 时间线:
1. **2026-03-06**: 安装coinor-libipopt系统包 (版本3.11.9)
2. **2026-03-06 02:33**: 编译项目，链接到系统Ipopt库
3. **2026-03-10 00:41**: 手动编译安装新版Ipopt (版本3.14.20)
4. **2026-03-10 00:41**: 创建系统级软链接 `/usr/lib/libipopt.so.3`
5. **2026-03-10 现在**: 程序运行时出现段错误

### 崩溃机制:
```
编译时: 链接 libipopt.so.1 (3.11.9) → ABI兼容
运行时: 混合使用 libipopt.so.1 (3.11.9) 和 libipopt.so.3 (3.14.20)
结果: 内存布局不一致 → 段错误(-11)
```

## 🛠️ 修复方案

### 方案1: 移除新版Ipopt干扰 (推荐)

```bash
# 1. 移除今天创建的软链接
sudo rm /lib/libipopt.so.3
sudo rm /lib/libcoinmumps.so.3
sudo rm /lib/libcoinhsl.so.2

# 2. 重新编译项目
cd /home/dixon/Final_Project/catkin
catkin_make clean
catkin_make

# 3. 测试
roslaunch tube_mpc_ros tube_mpc_navigation.launch
```

### 方案2: 强制使用项目源码中的Ipopt

```bash
# 1. 修改CMakeLists.txt，指定项目Ipopt路径
include_directories(/home/dixon/Final_Project/catkin/src/Ipopt/build/include)
link_directories(/home/dixon/Final_Project/catkin/src/Ipopt/build/src/.libs)

# 2. 重新编译
catkin_make clean
catkin_make
```

### 方案3: 统一使用新版Ipopt

```bash
# 1. 为新版Ipopt创建兼容软链接
cd /lib
sudo ln -sf libipopt.so.3.14.20 libipopt.so.1

# 2. 更新动态链接库缓存
sudo ldconfig

# 3. 重新编译所有相关组件
catkin_make clean
catkin_make
```

## 🔬 诊断建议

### 验证修复是否成功:

1. **检查库依赖一致性**:
```bash
ldd /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node | grep ipopt
```

2. **运行时调试**:
```bash
gdb -ex "run" -ex "bt" /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node
```

3. **监控库加载**:
```bash
LD_DEBUG=libs roslaunch tube_mpc_ros tube_mpc_navigation.launch 2>&1 | grep ipopt
```

## 📊 系统环境状态

### 当前环境:
- **OS**: Ubuntu 20.04.6 LTS
- **Kernel**: Linux 5.15.0-139-generic
- **ROS**: Noetic
- **编译器**: GCC with C++11 support
- **可用内存**: 5.5GB / 7.7GB
- **磁盘空间**: 9.9GB可用

### 关键库版本:
- **系统Ipopt**: 3.11.9 (coinor)
- **自定义Ipopt**: 3.14.20 (今天安装)
- **libstdc++**: GLIBCXX_3.4.28
- **MUMPS**: 5.2.1

## ⚠️ 预防措施

1. **避免混合使用不同版本的数值计算库**
2. **使用容器化或虚拟环境隔离开发环境**
3. **建立依赖库版本管理机制**
4. **记录所有系统级库安装操作**
5. **使用LD_LIBRARY_PATH明确指定库搜索路径**

## 🎯 下一步行动

**推荐使用方案1**，因为它:
- ✅ 最简单直接
- ✅ 不修改构建系统
- ✅ 回到已知正常工作的状态
- ✅ 风险最低

---

**诊断完成时间**: 2026-03-10
**问题严重性**: 高 (影响核心功能)
**修复难度**: 中等
**推荐方案**: 方案1 (移除新版Ipopt干扰)
