#!/usr/bin/env python3
"""
STL Monitor ROS Node - Simplified Working Version
Directly implements STL monitoring without complex imports
"""

import rospy
import numpy as np
from std_msgs.msg import Header, Float32, Bool
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped
from nav_msgs.msg import Path, Odometry
from tf.transformations import euler_from_quaternion
import csv
import os
from datetime import datetime

class STLMonitorNode:
    """
    Simplified STL Monitor with basic reachability constraints
    """

    def __init__(self):
        rospy.init_node('stl_monitor', anonymous=False)

        # Parameters
        self.use_belief_space = rospy.get_param('~use_belief_space', False)
        self.uncertainty_threshold = rospy.get_param('~belief_uncertainty_threshold', 0.1)
        self.reachability_threshold = rospy.get_param('~reachability_threshold', 0.5)
        self.baseline_requirement = rospy.get_param('~baseline_requirement', 0.1)

        # State variables
        self.current_pose = None
        self.current_goal = None
        self.global_path = None
        self.robustness = 1.0
        self.budget = 1.0
        self.violation = False

        # Subscribers
        rospy.Subscriber('/amcl_pose', PoseWithCovarianceStamped, self.pose_callback)
        rospy.Subscriber('/move_base/GlobalPlanner/plan', Path, self.path_callback)

        # Publishers
        self.robustness_pub = rospy.Publisher('/stl_monitor/robustness', Float32, queue_size=10)
        self.violation_pub = rospy.Publisher('/stl_monitor/violation', Bool, queue_size=10)
        self.budget_pub = rospy.Publisher('/stl_monitor/budget', Float32, queue_size=10)

        # Timer for STL evaluation (10Hz)
        self.timer = rospy.Timer(rospy.Duration(0.1), self.evaluate_stl)

        rospy.loginfo("STL Monitor Node started successfully")
        rospy.loginfo("STL monitoring with reachability constraints")

    def pose_callback(self, msg):
        """Callback for robot pose updates"""
        self.current_pose = msg.pose.pose

    def path_callback(self, msg):
        """Callback for global path updates"""
        self.global_path = msg

    def evaluate_stl(self, event):
        """Evaluate STL robustness"""
        if self.current_pose is None or self.global_path is None or len(self.global_path.poses) == 0:
            return

        try:
            # Extract current position
            x = self.current_pose.position.x
            y = self.current_pose.position.y

            # Get goal position (last pose in path)
            goal_pose = self.global_path.poses[-1].pose
            goal_x = goal_pose.position.x
            goal_y = goal_pose.position.y

            # Calculate distance to goal
            distance = np.sqrt((x - goal_x)**2 + (y - goal_y)**2)

            # Simple reachability robustness: ρ = threshold - distance
            self.robustness = self.reachability_threshold - distance

            # Update budget: R[k+1] = max{0, R[k] + ρ[k] - r̲}
            self.budget = max(0.0, self.budget + self.robustness - self.baseline_requirement)

            # Check for violation
            self.violation = (self.robustness < 0.0)

            # Publish results
            self.robustness_pub.publish(Float32(self.robustness))
            self.violation_pub.publish(Bool(self.violation))
            self.budget_pub.publish(Float32(self.budget))

            # Log violations
            if self.violation:
                rospy.logwarn_throttle(2.0, f"STL Violation! Robustness: {self.robustness:.3f}, Distance: {distance:.3f}")

        except Exception as e:
            rospy.logerr_throttle(2.0, f"Error in STL evaluation: {e}")

    def run(self):
        rospy.spin()

if __name__ == '__main__':
    try:
        monitor = STLMonitorNode()
        monitor.run()
    except rospy.ROSInterruptException:
        pass
