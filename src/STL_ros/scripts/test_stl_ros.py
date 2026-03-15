#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Simple integration test for STL_ros package
Tests basic functionality without requiring full system
"""

import sys
import time
import signal
import rospy
from std_msgs.msg import Header
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Path
from stl_ros.msg import STLRobustness, STLBudget

class STLTester:
    def __init__(self):
        rospy.init_node('stl_tester', anonymous=True)

        # Track test results
        self.received_robustness = False
        self.received_budget = False
        self.test_passed = False

        # Subscribers
        rospy.Subscriber('/stl_monitor/robustness', STLRobustness, self.robustness_callback)
        rospy.Subscriber('/stl_monitor/budget', STLBudget, self.budget_callback)

        # Publishers for test input
        self.belief_pub = rospy.Publisher('/stl_monitor/belief', PoseStamped, queue_size=10)
        self.traj_pub = rospy.Publisher('/stl_monitor/mpc_trajectory', Path, queue_size=10)

    def robustness_callback(self, msg):
        rospy.loginfo("Received robustness message for formula: %s", msg.formula_name)
        self.received_robustness = True
        self.check_test_completion()

    def budget_callback(self, msg):
        rospy.loginfo("Received budget message: %.2f (feasible: %s)",
                     msg.current_budget, msg.feasible)
        self.received_budget = True
        self.check_test_completion()

    def check_test_completion(self):
        if self.received_robustness and self.received_budget:
            rospy.loginfo("✓ Test PASSED: STL monitor is responding correctly")
            self.test_passed = True

    def publish_test_data(self):
        """Publish test data to trigger STL monitor"""
        rospy.loginfo("Publishing test data...")

        # Create test belief message
        belief_msg = PoseStamped()
        belief_msg.header = Header()
        belief_msg.header.stamp = rospy.Time.now()
        belief_msg.header.frame_id = 'map'
        belief_msg.pose.position.x = 5.0
        belief_msg.pose.position.y = 5.0
        belief_msg.pose.position.z = 0.0
        belief_msg.pose.orientation.w = 1.0

        # Create test trajectory message
        traj_msg = Path()
        traj_msg.header = Header()
        traj_msg.header.stamp = rospy.Time.now()
        traj_msg.header.frame_id = 'map'

        for i in range(5):
            pose = PoseStamped()
            pose.header = traj_msg.header
            pose.pose.position.x = 5.0 + i * 0.1
            pose.pose.position.y = 5.0 + i * 0.1
            pose.pose.orientation.w = 1.0
            traj_msg.poses.append(pose)

        # Publish messages
        self.belief_pub.publish(belief_msg)
        self.traj_pub.publish(traj_msg)

    def run_test(self, timeout=5.0):
        """Run the test with timeout"""
        rospy.loginfo("Starting STL_ros integration test...")
        rospy.loginfo("Waiting for STL monitor to start...")

        # Give monitor time to start
        time.sleep(1.0)

        # Publish test data
        self.publish_test_data()

        # Wait for responses
        start_time = time.time()
        rate = rospy.Rate(10)  # 10 Hz

        while not rospy.is_shutdown() and not self.test_passed:
            if time.time() - start_time > timeout:
                rospy.logwarn("Test timeout - no response from STL monitor")
                break
            rate.sleep()

        return self.test_passed

def signal_handler(sig, frame):
    rospy.loginfo("Test interrupted")
    sys.exit(0)

def main():
    signal.signal(signal.SIGINT, signal_handler)

    try:
        tester = STLTester()

        if tester.run_test(timeout=10.0):
            rospy.loginfo("=" * 50)
            rospy.loginfo("STL_ros INTEGRATION TEST: PASSED ✓")
            rospy.loginfo("=" * 50)
            sys.exit(0)
        else:
            rospy.logerr("=" * 50)
            rospy.logerr("STL_ros INTEGRATION TEST: FAILED ✗")
            rospy.logerr("=" * 50)
            sys.exit(1)

    except Exception as e:
        rospy.logerr("Test failed with exception: %s", str(e))
        sys.exit(1)

if __name__ == '__main__':
    main()
