#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
STL Visualizer Node - Placeholder Implementation
This node visualizes STL robustness, constraints, and violations in RViz

Phase 2: STL Integration - Work in Progress
"""

import rospy
from std_msgs.msg import Float32, Bool, ColorRGBA
from geometry_msgs.msg import Point
from visualization_msgs.msg import Marker, MarkerArray
from geometry_msgs.msg import PoseWithCovarianceStamped
import numpy as np

class STLVisualizer:
    def __init__(self):
        rospy.init_node('stl_visualizer', anonymous=False)

        # Parameters
        self.visualization_frame = rospy.get_param('~visualization_frame', 'map')
        self.marker_scale = rospy.get_param('~robustness_marker_scale', 0.5)

        # Color parameters
        self.violation_color_r = rospy.get_param('~violation_marker_color_r', 1.0)
        self.violation_color_g = rospy.get_param('~violation_marker_color_g', 0.0)
        self.violation_color_b = rospy.get_param('~violation_marker_color_b', 0.0)

        # State variables
        self.robustness = 1.0
        self.budget = 1.0
        self.violation = False
        self.robot_position = Point()

        # Subscribers
        rospy.Subscriber('/stl/robustness', Float32, self.robustness_callback)
        rospy.Subscriber('/stl/budget', Float32, self.budget_callback)
        rospy.Subscriber('/stl/violation', Bool, self.violation_callback)
        rospy.Subscriber('/amcl_pose', PoseWithCovarianceStamped, self.pose_callback)

        # Publishers
        self.marker_pub = rospy.Publisher('/stl_visualization', MarkerArray, queue_size=10)

        # Timer for visualization updates
        self.timer = rospy.Timer(rospy.Duration(0.5), self.update_visualization)

        rospy.loginfo("STL Visualizer Node started (Placeholder Implementation)")

    def robustness_callback(self, msg):
        """Callback for robustness updates"""
        self.robustness = msg.data

    def budget_callback(self, msg):
        """Callback for budget updates"""
        self.budget = msg.data

    def violation_callback(self, msg):
        """Callback for violation updates"""
        self.violation = msg.data

    def pose_callback(self, msg):
        """Callback for robot pose updates"""
        self.robot_position = msg.pose.pose.position

    def get_robustness_color(self):
        """Get color based on robustness value"""
        # Green for positive, red for negative
        color = ColorRGBA()
        if self.robustness >= 0.0:
            # Scale from yellow to green based on robustness
            color.r = max(0.0, 1.0 - self.robustness)
            color.g = 1.0
            color.b = 0.0
        else:
            # Red for negative robustness
            color.r = 1.0
            color.g = 0.0
            color.b = 0.0

        color.a = 0.8  # Alpha
        return color

    def update_visualization(self, event):
        """Update visualization markers"""
        marker_array = MarkerArray()

        # Create robustness indicator marker
        robustness_marker = Marker()
        robustness_marker.header.frame_id = self.visualization_frame
        robustness_marker.header.stamp = rospy.Time.now()
        robustness_marker.ns = "stl_robustness"
        robustness_marker.id = 0
        robustness_marker.type = Marker.SPHERE
        robustness_marker.action = Marker.ADD

        # Position marker above robot
        robustness_marker.pose.position.x = self.robot_position.x
        robustness_marker.pose.position.y = self.robot_position.y
        robustness_marker.pose.position.z = 1.0
        robustness_marker.pose.orientation.w = 1.0

        # Scale based on robustness magnitude
        scale = min(1.0, abs(self.robustness) * self.marker_scale)
        robustness_marker.scale.x = scale
        robustness_marker.scale.y = scale
        robustness_marker.scale.z = scale

        # Color based on robustness
        robustness_marker.color = self.get_robustness_color()

        marker_array.markers.append(robustness_marker)

        # Create budget indicator marker
        budget_marker = Marker()
        budget_marker.header.frame_id = self.visualization_frame
        budget_marker.header.stamp = rospy.Time.now()
        budget_marker.ns = "stl_budget"
        budget_marker.id = 1
        budget_marker.type = Marker.TEXT_VIEW_FACING
        budget_marker.action = Marker.ADD

        budget_marker.pose.position.x = self.robot_position.x
        budget_marker.pose.position.y = self.robot_position.y
        budget_marker.pose.position.z = 1.5
        budget_marker.pose.orientation.w = 1.0

        budget_marker.text = f"R:{self.robustness:.2f}\nB:{self.budget:.2f}"
        budget_marker.scale.z = 0.2  # Text height

        budget_marker.color.r = 1.0
        budget_marker.color.g = 1.0
        budget_marker.color.b = 1.0
        budget_marker.color.a = 0.8

        marker_array.markers.append(budget_marker)

        # Create violation warning marker
        if self.violation:
            violation_marker = Marker()
            violation_marker.header.frame_id = self.visualization_frame
            violation_marker.header.stamp = rospy.Time.now()
            violation_marker.ns = "stl_violation"
            violation_marker.id = 2
            violation_marker.type = Marker.CYLINDER
            violation_marker.action = Marker.ADD

            violation_marker.pose.position.x = self.robot_position.x
            violation_marker.pose.position.y = self.robot_position.y
            violation_marker.pose.position.z = 0.5
            violation_marker.pose.orientation.w = 1.0

            violation_marker.scale.x = 1.0
            violation_marker.scale.y = 1.0
            violation_marker.scale.z = 0.1

            violation_marker.color.r = self.violation_color_r
            violation_marker.color.g = self.violation_color_g
            violation_marker.color.b = self.violation_color_b
            violation_marker.color.a = 0.5

            marker_array.markers.append(violation_marker)

        # Publish markers
        self.marker_pub.publish(marker_array)

    def run(self):
        rospy.spin()

if __name__ == '__main__':
    try:
        visualizer = STLVisualizer()
        visualizer.run()
    except rospy.ROSInterruptException:
        pass