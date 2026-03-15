#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
STL_ros Integration Test
Tests STL_ros package communication and data flow
"""

import sys
import time
import rospy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Path
from stl_ros.msg import STLRobustness, STLBudget

class STLIntegrationTest:
    def __init__(self):
        rospy.init_node('stl_integration_test', anonymous=True)

        # Test counters
        self.received_robustness = False
        self.received_budget = False
        self.published_belief = False
        self.published_trajectory = False

        # Publishers
        self.belief_pub = rospy.Publisher('/stl_monitor/belief', PoseStamped, queue_size=10)
        self.traj_pub = rospy.Publisher('/stl_monitor/mpc_trajectory', Path, queue_size=10)

        # Subscribers
        self.robustness_sub = rospy.Subscriber('/stl_monitor/robustness', STLRobustness, self.robustness_callback)
        self.budget_sub = rospy.Subscriber('/stl_monitor/budget', STLBudget, self.budget_callback)

    def robustness_callback(self, msg):
        self.received_robustness = True
        rospy.loginfo("Received robustness: %.3f (formula: %s, satisfied: %s)",
                     msg.robustness, msg.formula_name, msg.satisfied)

    def budget_callback(self, msg):
        self.received_budget = True
        rospy.loginfo("Received budget: %.3f (feasible: %s)",
                     msg.current_budget, msg.feasible)

    def publish_test_data(self):
        rospy.loginfo("Publishing test data...")

        # Publish test belief
        belief_msg = PoseStamped()
        belief_msg.header.stamp = rospy.Time.now()
        belief_msg.header.frame_id = 'map'
        belief_msg.pose.position.x = 5.0
        belief_msg.pose.position.y = 5.0
        belief_msg.pose.position.z = 0.0
        belief_msg.pose.orientation.w = 1.0

        self.belief_pub.publish(belief_msg)
        self.published_belief = True
        rospy.loginfo("Published belief state")

        # Publish test trajectory
        traj_msg = Path()
        traj_msg.header.stamp = rospy.Time.now()
        traj_msg.header.frame_id = 'map'

        for i in range(5):
            pose = PoseStamped()
            pose.header = traj_msg.header
            pose.pose.position.x = 5.0 + i * 0.1
            pose.pose.position.y = 5.0 + i * 0.1
            pose.pose.position.z = 0.0
            pose.pose.orientation.w = 1.0
            traj_msg.poses.append(pose)

        self.traj_pub.publish(traj_msg)
        self.published_trajectory = True
        rospy.loginfo("Published MPC trajectory with %d poses", len(traj_msg.poses))

    def run_test(self, timeout=10.0):
        rospy.loginfo("Starting STL integration test...")

        # Give publishers time to register
        time.sleep(1.0)

        # Publish test data
        self.publish_test_data()

        # Wait for responses
        start_time = time.time()
        rate = rospy.Rate(10)  # 10 Hz

        while not rospy.is_shutdown() and (time.time() - start_time) < timeout:
            if self.received_robustness and self.received_budget:
                rospy.loginfo("✓ Test PASSED: All communication working!")
                return True
            rate.sleep()

        # Report results
        rospy.logwarn("Test completed with partial results:")
        rospy.logwarn(f"  Published belief: {self.published_belief}")
        rospy.logwarn(f"  Published trajectory: {self.published_trajectory}")
        rospy.logwarn(f"  Received robustness: {self.received_robustness}")
        rospy.logwarn(f"  Received budget: {self.received_budget}")

        return self.received_robustness and self.received_budget

def main():
    try:
        tester = STLIntegrationTest()
        success = tester.run_test(timeout=15.0)

        if success:
            rospy.loginfo("=" * 50)
            rospy.loginfo("STL_INTEGRATION_TEST: PASSED ✓")
            rospy.loginfo("=" * 50)
            sys.exit(0)
        else:
            rospy.logerr("=" * 50)
            rospy.logerr("STL_INTEGRATION_TEST: FAILED ✗")
            rospy.logerr("=" * 50)
            sys.exit(1)

    except rospy.ROSInterruptException:
        rospy.loginfo("Test interrupted by user")
        sys.exit(0)
    except Exception as e:
        rospy.logerr(f"Test failed with exception: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
