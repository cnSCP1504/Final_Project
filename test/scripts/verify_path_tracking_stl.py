#!/usr/bin/env python3
"""
Path Tracking STL验证脚本
验证STL监控节点正确计算到路径的最短距离
"""

import rospy
import numpy as np
from nav_msgs.msg import Path
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped
from std_msgs.msg import Float32
import time

class PathTrackingSTLVerifier:
    def __init__(self):
        rospy.init_node('path_tracking_stl_verifier', anonymous=True)

        # 测试场景：创建一个简单的L形路径
        self.test_path = Path()
        self.test_path.header.frame_id = "map"
        self.test_path.header.stamp = rospy.Time.now()

        # 创建路径点：从(0,0)到(5,0)到(5,5)
        path_points = [
            (0.0, 0.0, 0.0),
            (2.0, 0.0, 0.0),
            (4.0, 0.0, 0.0),
            (5.0, 0.0, 0.0),
            (5.0, 2.0, 0.0),
            (5.0, 4.0, 0.0),
            (5.0, 5.0, 0.0),
        ]

        for x, y, yaw in path_points:
            pose = PoseStamped()
            pose.header = self.test_path.header
            pose.pose.position.x = x
            pose.pose.position.y = y
            pose.pose.position.z = 0.0
            pose.pose.orientation.w = 1.0
            self.test_path.poses.append(pose)

        # Publishers
        self.path_pub = rospy.Publisher('/move_base/GlobalPlanner/plan', Path, queue_size=10, latch=True)
        self.pose_pub = rospy.Publisher('/amcl_pose', PoseWithCovarianceStamped, queue_size=10)

        # Subscribers
        self.robustness = None
        rospy.Subscriber('/stl_monitor/robustness', Float32, self.robustness_callback)

        print("✅ Path Tracking STL验证器初始化完成")
        print(f"   路径点数: {len(self.test_path.poses)}")
        print(f"   路径形状: L形 (0,0)→(5,0)→(5,5)")

    def robustness_callback(self, msg):
        self.robustness = msg.data

    def publish_test_path(self):
        """发布测试路径"""
        self.path_pub.publish(self.test_path)
        print("✅ 测试路径已发布")

    def test_point_on_path(self, x, y, expected_distance, tolerance=0.1):
        """测试特定点的距离"""
        print(f"\n📍 测试点: ({x}, {y})")
        print(f"   预期到路径距离: {expected_distance:.3f}m (容差±{tolerance}m)")

        # 发布机器人位置
        pose = PoseWithCovarianceStamped()
        pose.header.stamp = rospy.Time.now()
        pose.header.frame_id = "map"
        pose.pose.pose.position.x = x
        pose.pose.pose.position.y = y
        pose.pose.pose.position.z = 0.0
        pose.pose.pose.orientation.w = 1.0
        pose.pose.covariance = [0.01] * 36  # 小的协方差

        self.pose_pub.publish(pose)

        # 等待STL计算
        timeout = 2.0
        start_time = time.time()
        self.robustness = None

        while self.robustness is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)

        if self.robustness is not None:
            # robustness = threshold - distance
            # distance = threshold - robustness
            threshold = 0.5  # 默认阈值
            actual_distance = threshold - self.robustness

            print(f"   实际到路径距离: {actual_distance:.3f}m")
            print(f"   STL Robustness: {self.robustness:.4f}")

            error = abs(actual_distance - expected_distance)
            if error <= tolerance:
                print(f"   ✅ PASS - 误差: {error:.4f}m")
                return True
            else:
                print(f"   ❌ FAIL - 误差: {error:.4f}m > {tolerance}m")
                return False
        else:
            print(f"   ❌ FAIL - 未收到robustness数据（超时{timeout}s）")
            return False

    def run_verification(self):
        """运行完整验证测试"""
        print("\n" + "="*60)
        print("Path Tracking STL验证测试")
        print("="*60)

        # 发布路径
        self.publish_test_path()
        time.sleep(0.5)

        # 测试用例
        test_cases = [
            # (x, y, expected_distance, description)
            (0.0, 0.0, 0.0, "路径起点（在路径上）"),
            (2.5, 0.0, 0.0, "第一段中点（在路径上）"),
            (5.0, 2.5, 0.0, "第二段中点（在路径上）"),
            (5.0, 5.0, 0.0, "路径终点（在路径上）"),

            (2.5, 0.3, 0.3, "第一段附近（偏移0.3m）"),
            (5.0, 2.5, 0.0, "转角处（在路径上）"),
            (5.3, 2.5, 0.3, "第二段附近（偏移0.3m）"),

            (2.5, 1.0, 1.0, "第一段远处（偏移1.0m）"),
            (6.0, 2.5, 1.0, "第二段远处（偏移1.0m）"),

            (2.5, -0.5, 0.5, "第一段外侧（偏移0.5m）"),
        ]

        results = []
        for x, y, expected_dist, desc in test_cases:
            print(f"\n{'='*60}")
            print(f"测试: {desc}")
            print(f"{'='*60}")
            result = self.test_point_on_path(x, y, expected_dist, tolerance=0.15)
            results.append((desc, result))
            time.sleep(0.3)

        # 汇总结果
        print(f"\n{'='*60}")
        print("验证结果汇总")
        print(f"{'='*60}")

        passed = sum(1 for _, r in results if r)
        total = len(results)

        for desc, result in results:
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{status} - {desc}")

        print(f"\n总计: {passed}/{total} 通过")

        if passed == total:
            print("\n🎉 所有测试通过！Path Tracking STL实现正确！")
            return True
        else:
            print(f"\n⚠️  {total - passed} 个测试失败")
            return False


if __name__ == '__main__':
    try:
        verifier = PathTrackingSTLVerifier()
        time.sleep(1.0)  # 等待节点启动
        success = verifier.run_verification()

        if success:
            exit(0)
        else:
            exit(1)

    except rospy.ROSInterruptException:
        pass
    except Exception as e:
        print(f"❌ 错误: {e}")
        import traceback
        traceback.print_exc()
        exit(1)
