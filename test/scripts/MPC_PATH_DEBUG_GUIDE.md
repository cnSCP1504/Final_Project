# MPC纯基准测试 - 路径接收问题调试

**日期**: 2026-04-15
**状态**: ✅ 参数已正确设置，但路径仍未接收到

---

## 当前状态

### 参数已正确设置
```
* /nav_mpc/global_path_topic: /move_base/Global...  ✅
* /nav_mpc/MPCPlannerROS/global_path_topic: /move_base/Global...  ✅
```

### 问题依然存在
- `Goal Received :goalCB!` - 目标已接收 ✅
- `Failed to path generation` - 路径生成失败 ❌

---

## 问题分析

### 路径生成失败的原因

查看 `navMPCNode.cpp` 第289-302行：

```cpp
if(odom_path.poses.size() >= 6 )
{
    _odom_path = odom_path; // Path waypoints in odom frame
    _path_computed = true;
    // publish odom path
    odom_path.header.frame_id = _odom_frame;
    odom_path.header.stamp = ros::Time::now();
    _pub_odompath.publish(odom_path);
}
else
{
    cout << "Failed to path generation" << endl;
    _waypointsDist = -1;
}
```

**关键发现**：路径点数量 < 6 导致失败！

### 可能的原因

1. **GlobalPlanner没有发布路径**
   - move_base可能没有成功规划路径
   - GlobalPlanner话题可能没有数据

2. **TF变换失败**
   - 路径坐标转换时TF异常
   - 被catch块捕获，但没有详细日志

3. **路径下采样问题**
   - `_downSampling`计算可能有问题
   - 导致有效点数不足

---

## 调试方案

### 方案1：添加详细日志

修改 `navMPCNode.cpp` 的 `pathCB` 函数，添加调试输出：

```cpp
void MPCNode::pathCB(const nav_msgs::Path::ConstPtr& pathMsg)
{
    ROS_INFO("Path received: %zu poses", pathMsg->poses.size());  // 添加

    if(_goal_received && !_goal_reached)
    {
        nav_msgs::Path odom_path = nav_msgs::Path();
        try
        {
            double total_length = 0.0;
            int sampling = _downSampling;

            if(_waypointsDist <=0.0)
            {
                double dx = pathMsg->poses[1].pose.position.x - pathMsg->poses[0].pose.position.x;
                double dy = pathMsg->poses[1].pose.position.y - pathMsg->poses[0].pose.position.y;
                _waypointsDist = sqrt(dx*dx + dy*dy);
                _downSampling = int(_pathLength/10.0/_waypointsDist);
                ROS_INFO("waypointsDist: %.3f, downSampling: %d", _waypointsDist, _downSampling);  // 添加
            }

            // Cut and downsampling the path
            for(int i =0; i< pathMsg->poses.size(); i++)
            {
                if(total_length > _pathLength)
                    break;

                if(sampling == _downSampling)
                {
                    geometry_msgs::PoseStamped tempPose;
                    _tf_listener.transformPose(_odom_frame, ros::Time(0) , pathMsg->poses[i], _map_frame, tempPose);
                    odom_path.poses.push_back(tempPose);
                    sampling = 0;
                }
                total_length = total_length + _waypointsDist;
                sampling = sampling + 1;
            }

            ROS_INFO("Generated %zu odom_path poses", odom_path.poses.size());  // 添加

            if(odom_path.poses.size() >= 6 )
            {
                // ... success
            }
            else
            {
                ROS_ERROR("Failed to path generation: only %zu poses (< 6)", odom_path.poses.size());  // 修改
                _waypointsDist = -1;
            }
        }
        catch(tf::TransformException &ex)
        {
            ROS_ERROR("TF Exception: %s", ex.what());  // 添加详细错误
            ros::Duration(1.0).sleep();
        }
    }
}
```

### 方案2：检查GlobalPlanner是否发布路径

运行测试时检查：
```bash
rostopic echo /move_base/GlobalPlanner/plan --noarr
```

### 方案3：简化问题

直接运行 `mpc_ros` 包的原始launch文件，看是否正常工作：
```bash
roslaunch mpc_ros nav_gazebo.launch controller:=mpc
```

---

## 推荐行动

**最简单的方法**：检查 `mpc_ros` 原始包是否正常工作

1. 停止当前测试
2. 直接运行 `roslaunch mpc_ros nav_gazebo.launch controller:=mpc`
3. 发送一个目标点
4. 查看是否正常移动

如果原始包正常，说明问题出在 `mpc_baseline_test.launch` 配置；
如果原始包也不正常，说明问题出在 `mpc_ros` 包本身或环境配置。
