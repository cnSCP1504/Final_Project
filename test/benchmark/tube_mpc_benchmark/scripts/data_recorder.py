#!/usr/bin/env python3
"""
Data recorder for Tube MPC benchmark tests
Records all relevant navigation data to CSV for post-processing
"""

import rospy
import csv
import numpy as np
from nav_msgs.msg import Odometry, Path
from geometry_msgs.msg import Twist
from std_msgs.msg import Float64, Float64MultiArray
from move_base_msgs.msg import MoveBaseActionGoal
import os
from datetime import datetime

class NavigationDataRecorder:
    def __init__(self):
        rospy.init_node('navigation_data_recorder', anonymous=True)

        # Parameters
        self.output_dir = rospy.get_param('~output_dir', '/tmp/tube_mpc_data')
        self.record_rate = rospy.get_param('~record_rate', 10.0)  # Hz

        # Create output directory
        os.makedirs(self.output_dir, exist_ok=True)

        # Generate filename with timestamp
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.csv_file = os.path.join(self.output_dir, f'navigation_data_{timestamp}.csv')

        # Data storage
        self.current_data = {}
        self.recording = False
        self.start_time = None
        self.goal = None

        # Setup CSV file
        self.setup_csv()

        # Subscribers
        rospy.Subscriber('/odom', Odometry, self.odom_callback)
        rospy.Subscriber('/cmd_vel', Twist, self.cmd_vel_callback)
        rospy.Subscriber('/global_path', Path, self.path_callback)
        rospy.Subscriber('/mpc_metrics/tracking_error', Float64, self.tracking_error_callback)
        rospy.Subscriber('/mpc_metrics/solve_time_ms', Float64, self.solve_time_callback)
        rospy.Subscriber('/mpc_metrics/empirical_risk', Float64, self.empirical_risk_callback)
        rospy.Subscriber('/mpc_metrics/feasibility_rate', Float64, self.feasibility_rate_callback)
        rospy.Subscriber('/test_status', Float64MultiArray, self.test_status_callback)
        rospy.Subscriber('/move_base/goal', MoveBaseActionGoal, self.goal_callback)

        # Recording timer
        self.timer = rospy.Timer(rospy.Duration(1.0 / self.record_rate), self.record_callback)

        rospy.loginfo(f"Navigation Data Recorder started")
        rospy.loginfo(f"Output file: {self.csv_file}")

    def setup_csv(self):
        """Setup CSV file with headers"""
        self.fieldnames = [
            'timestamp',
            'elapsed_time',
            'x', 'y', 'z',
            'roll', 'pitch', 'yaw',
            'linear_vel', 'angular_vel',
            'goal_x', 'goal_y', 'goal_yaw',
            'distance_to_goal',
            'tracking_error',
            'solve_time_ms',
            'empirical_risk',
            'feasibility_rate',
            'path_length'
        ]

        with open(self.csv_file, 'w', newline='') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=self.fieldnames)
            writer.writeheader()

    def odom_callback(self, msg):
        """Handle odometry data"""
        self.current_data['x'] = msg.pose.pose.position.x
        self.current_data['y'] = msg.pose.pose.position.y
        self.current_data['z'] = msg.pose.pose.position.z

        # Convert quaternion to euler
        import tf.transformations as tf
        quaternion = (
            msg.pose.pose.orientation.x,
            msg.pose.pose.orientation.y,
            msg.pose.pose.orientation.z,
            msg.pose.pose.orientation.w
        )
        euler = tf.euler_from_quaternion(quaternion)
        self.current_data['roll'] = euler[0]
        self.current_data['pitch'] = euler[1]
        self.current_data['yaw'] = euler[2]

        # Start recording on first odom message
        if not self.recording:
            self.recording = True
            self.start_time = rospy.Time.now()
            rospy.loginfo("Recording started")

    def cmd_vel_callback(self, msg):
        """Handle velocity commands"""
        self.current_data['linear_vel'] = msg.linear.x
        self.current_data['angular_vel'] = msg.angular.z

    def path_callback(self, msg):
        """Handle global path"""
        if len(msg.poses) > 0:
            # Calculate path length
            path_length = 0.0
            for i in range(1, len(msg.poses)):
                p1 = msg.poses[i-1].pose.position
                p2 = msg.poses[i].pose.position
                path_length += np.sqrt((p2.x - p1.x)**2 + (p2.y - p1.y)**2)

            self.current_data['path_length'] = path_length

    def tracking_error_callback(self, msg):
        """Handle tracking error"""
        self.current_data['tracking_error'] = msg.data

    def solve_time_callback(self, msg):
        """Handle MPC solve time"""
        self.current_data['solve_time_ms'] = msg.data

    def empirical_risk_callback(self, msg):
        """Handle empirical risk"""
        self.current_data['empirical_risk'] = msg.data

    def feasibility_rate_callback(self, msg):
        """Handle feasibility rate"""
        self.current_data['feasibility_rate'] = msg.data

    def test_status_callback(self, msg):
        """Handle test status updates"""
        if len(msg.data) >= 7:
            # Test status provides current position and time
            self.current_data['x'] = msg.data[0]
            self.current_data['y'] = msg.data[1]
            self.current_data['yaw'] = msg.data[2]
            self.current_data['linear_vel'] = msg.data[3]
            self.current_data['angular_vel'] = msg.data[4]
            self.current_data['elapsed_time'] = msg.data[5]

    def goal_callback(self, msg):
        """Handle navigation goal"""
        self.goal = {
            'x': msg.goal.target_pose.pose.position.x,
            'y': msg.goal.target_pose.pose.position.y,
            'yaw': 0  # Simplified
        }
        rospy.loginfo(f"Goal received: ({self.goal['x']}, {self.goal['y']})")

    def record_callback(self, event):
        """Periodic recording callback"""
        if not self.recording or not self.current_data:
            return

        # Calculate elapsed time
        if self.start_time:
            elapsed = (rospy.Time.now() - self.start_time).to_sec()
            self.current_data['elapsed_time'] = elapsed
        else:
            self.current_data['elapsed_time'] = 0

        # Add timestamp
        self.current_data['timestamp'] = rospy.Time.now().to_sec()

        # Add goal information
        if self.goal:
            self.current_data['goal_x'] = self.goal['x']
            self.current_data['goal_y'] = self.goal['y']
            self.current_data['goal_yaw'] = self.goal['yaw']

            # Calculate distance to goal
            if 'x' in self.current_data and 'y' in self.current_data:
                dx = self.goal['x'] - self.current_data['x']
                dy = self.goal['y'] - self.current_data['y']
                self.current_data['distance_to_goal'] = np.sqrt(dx**2 + dy**2)

        # Fill missing fields
        for field in self.fieldnames:
            if field not in self.current_data:
                self.current_data[field] = None

        # Write to CSV
        try:
            with open(self.csv_file, 'a', newline='') as csvfile:
                writer = csv.DictWriter(csvfile, fieldnames=self.fieldnames)
                writer.writerow(self.current_data)

            rospy.loginfo_throttle(1.0, f"Recorded data point: t={self.current_data['elapsed_time']:.1f}s")

        except Exception as e:
            rospy.logwarn(f"Failed to write data: {e}")

if __name__ == '__main__':
    try:
        recorder = NavigationDataRecorder()
        rospy.spin()
    except rospy.ROSInterruptException:
        rospy.loginfo("Data recorder stopped")
