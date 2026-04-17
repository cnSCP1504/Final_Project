# MPC Pure Baseline Launch File Issue Analysis

**Date**: 2026-04-15
**Issue**: Pure MPC baseline test - GlobalPlanner not publishing paths to `/move_base/GlobalPlanner/plan`

---

## Problem Summary

The pure MPC model test fails because:
1. `nav_mpc` receives goal from `/move_base_simple/goal` ✅
2. `nav_mpc` subscribes to `/move_base/GlobalPlanner/plan` for path ✅
3. **move_base's GlobalPlanner does NOT publish paths** ❌

Result: `nav_mpc` shows "Waiting for global path msgs" and robot doesn't move.

---

## Root Cause Analysis

### Key Finding: tube_mpc DOES work with the same setup!

Comparing tube_mpc test logs vs mpc test logs:

**tube_mpc (SUCCESS)**:
```
[INFO] Path transformation successful: 9 points transformed from map to odom
[INFO] Goal Received :goalCB!
[WARN] Path has only 3 point(s), distance to goal: 2.20 m. Continuing control...
```

**mpc (FAILURE)**:
```
[ERROR] Client [/nav_mpc] wants topic /move_base_simple/goal to have datatype/md5sum [geometry_msgs/PoseStamped],
but our version has [move_base_msgs/MoveBaseActionGoal]. Dropping connection.
[INFO] Goal Received :goalCB!
(No path messages)
```

### The Real Issue: nav_mpc vs tube_mpc Code Difference

**tube_mpc_ros/src/navMPCNode.cpp** (lines 135-136):
```cpp
pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
pn.param<std::string>("goal_topic", _goal_topic, "/move_base_simple/goal");
```

**mpc_ros/src/navMPCNode.cpp** (lines 135-136):
```cpp
pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
pn.param<std::string>("goal_topic", _goal_topic, "/move_base_simple/goal");
```

**Both use the same defaults!** The difference is:

1. **tube_mpc launch files** have a remap:
   ```xml
   <remap from="/move_base/TrajectoryPlannerROS/global_plan" to="/move_base/GlobalPlanner/plan"/>
   ```

2. **mpc launch file** sets parameter:
   ```xml
   <param name="global_path_topic" value="/move_base/GlobalPlanner/plan"/>
   ```

Both should work... but there's still the goal topic type mismatch issue!

### The Goal Topic Type Mismatch

```
[ERROR] Client [/nav_mpc] wants topic /move_base_simple/goal to have datatype/md5sum [geometry_msgs/PoseStamped],
but our version has [move_base_msgs/MoveBaseActionGoal]. Dropping connection.
```

This error appears in mpc test but NOT in tube_mpc test!

**Why?** Let me check if there's a difference in how move_base handles this...

---

## Solution

The issue is that move_base is trying to subscribe to `/move_base_simple/goal` with `MoveBaseActionGoal` type, but `auto_nav_goal.py` publishes `PoseStamped`.

**However**, this error alone shouldn't prevent GlobalPlanner from working, because:
1. The goal is still received by `nav_mpc` (goalCB is triggered)
2. tube_mpc has the same setup but works

**The real difference might be timing or move_base initialization.**

Let me try a simpler approach: Use the tube_mpc_ros launch file structure directly.
