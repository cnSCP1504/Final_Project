#!/usr/bin/env python3
"""
Lightweight Kinematic Robot Simulator
Fast, physics-free simulation for rapid MPC testing (100x faster than Gazebo)
"""

import rospy
import math
import time
from geometry_msgs.msg import Twist, TransformStamped, Quaternion
from nav_msgs.msg import Odometry
from std_msgs.msg import Bool, Float64MultiArray
from tf2_ros import TransformBroadcaster
from sensor_msgs.msg import LaserScan
import numpy as np

class KinematicRobotSim:
    """
    Differential drive kinematic model (no physics)
    State: [x, y, theta]
    Control: [linear_vel, angular_vel]
    """
    def __init__(self):
        rospy.init_node('kinematic_robot_sim', anonymous=False)

        # Parameters
        self.update_rate = rospy.get_param('~update_rate', 50.0)  # Hz
        self.dt = 1.0 / self.update_rate

        # Robot state
        self.x = rospy.get_param('~initial_x', 0.0)
        self.y = rospy.get_param('~initial_y', 0.0)
        self.theta = rospy.get_param('~initial_theta', 0.0)
        self.linear_vel = 0.0
        self.angular_vel = 0.0

        # Robot dimensions
        self.robot_radius = rospy.get_param('~robot_radius', 0.2)

        # Noise parameters (simulate sensor uncertainty)
        self.odom_noise_linear = rospy.get_param('~odom_noise_linear', 0.001)
        self.odom_noise_angular = rospy.get_param('~odom_noise_angular', 0.001)

        # Test control
        self.test_active = False
        self.start_time = None
        self.elapsed_time = 0.0

        # TF broadcaster
        self.tf_broadcaster = TransformBroadcaster()

        # Publishers
        self.odom_pub = rospy.Publisher('/odom', Odometry, queue_size=10)
        self.laser_pub = rospy.Publisher('/scan', LaserScan, queue_size=10)
        self.test_status_pub = rospy.Publisher('/test_status', Float64MultiArray, queue_size=10)
        self.goal_reached_pub = rospy.Publisher('/goal_reached', Bool, queue_size=10)

        # Subscribers
        self.cmd_vel_sub = rospy.Subscriber('/cmd_vel', Twist, self.cmd_vel_callback)
        self.test_control_sub = rospy.Subscriber('/test_control', Float64MultiArray, self.test_control_callback)

        # Timer
        self.timer = rospy.Timer(rospy.Duration(self.dt), self.update)

        rospy.loginfo("Kinematic Robot Simulator Started")
        rospy.loginfo(f"Initial pose: x={self.x:.2f}, y={self.y:.2f}, theta={self.theta:.2f}")

        # Performance tracking
        self.total_distance = 0.0

    def cmd_vel_callback(self, msg):
        """Store velocity commands"""
        if self.test_active:
            self.linear_vel = msg.linear.x
            self.angular_vel = msg.angular.z

    def test_control_callback(self, msg):
        """
        Handle test control commands
        msg.data[0]: command type (0=reset, 1=start, 2=stop, 3=set_pose)
        msg.data[1:]: parameters (x, y, theta for set_pose)
        """
        cmd = int(msg.data[0])

        if cmd == 0:  # Reset
            self.reset_robot()
        elif cmd == 1:  # Start test
            self.start_test()
        elif cmd == 2:  # Stop test
            self.stop_test()
        elif cmd == 3:  # Set pose
            if len(msg.data) >= 4:
                self.set_pose(msg.data[1], msg.data[2], msg.data[3])

    def reset_robot(self):
        """Reset robot to initial pose"""
        self.x = rospy.get_param('~initial_x', 0.0)
        self.y = rospy.get_param('~initial_y', 0.0)
        self.theta = rospy.get_param('~initial_theta', 0.0)
        self.linear_vel = 0.0
        self.angular_vel = 0.0
        self.total_distance = 0.0
        rospy.loginfo(f"Robot reset to: ({self.x:.2f}, {self.y:.2f}, {self.theta:.2f})")

    def set_pose(self, x, y, theta):
        """Set robot to specific pose"""
        self.x = x
        self.y = y
        self.theta = theta
        self.linear_vel = 0.0
        self.angular_vel = 0.0
        rospy.loginfo(f"Robot set to: ({self.x:.2f}, {self.y:.2f}, {self.theta:.2f})")

    def start_test(self):
        """Start test timing"""
        self.test_active = True
        self.start_time = time.time()
        self.elapsed_time = 0.0
        rospy.loginfo("Test started")

    def stop_test(self):
        """Stop test timing"""
        self.test_active = False
        self.linear_vel = 0.0
        self.angular_vel = 0.0
        rospy.loginfo(f"Test stopped. Duration: {self.elapsed_time:.2f}s")

    def update(self, event):
        """Update robot state using kinematic model"""
        # Differential drive kinematics
        dx = self.linear_vel * math.cos(self.theta) * self.dt
        dy = self.linear_vel * math.sin(self.theta) * self.dt
        dtheta = self.angular_vel * self.dt

        # Add noise (simulate sensor uncertainty)
        dx += np.random.normal(0, self.odom_noise_linear * abs(dx))
        dy += np.random.normal(0, self.odom_noise_linear * abs(dy))
        dtheta += np.random.normal(0, self.odom_noise_angular * abs(dtheta))

        self.x += dx
        self.y += dy
        self.theta += dtheta

        # Track distance
        step_distance = math.sqrt(dx**2 + dy**2)
        self.total_distance += step_distance

        # Update test time
        if self.test_active and self.start_time:
            self.elapsed_time = time.time() - self.start_time

        # Publish messages
        self.publish_odometry()
        self.publish_tf()
        self.publish_laser_scan()
        self.publish_test_status()

    def publish_odometry(self):
        """Publish odometry message"""
        odom = Odometry()
        odom.header.stamp = rospy.Time.now()
        odom.header.frame_id = "odom"
        odom.child_frame_id = "base_link"

        # Position
        odom.pose.pose.position.x = self.x
        odom.pose.pose.position.y = self.y
        odom.pose.pose.position.z = 0.0

        # Orientation
        odom.pose.pose.orientation = self.yaw_to_quaternion(self.theta)

        # Velocity
        odom.twist.twist.linear.x = self.linear_vel
        odom.twist.twist.angular.z = self.angular_vel

        # Covariance
        odom.pose.covariance = [0.0] * 36
        odom.pose.covariance[0] = 0.0001
        odom.pose.covariance[7] = 0.0001
        odom.pose.covariance[35] = 0.0001

        self.odom_pub.publish(odom)

    def publish_tf(self):
        """Publish transform from odom to base_link"""
        t = TransformStamped()
        t.header.stamp = rospy.Time.now()
        t.header.frame_id = "odom"
        t.child_frame_id = "base_link"

        t.transform.translation.x = self.x
        t.transform.translation.y = self.y
        t.transform.translation.z = 0.0
        t.transform.rotation = self.yaw_to_quaternion(self.theta)

        self.tf_broadcaster.sendTransform(t)

    def publish_laser_scan(self):
        """Publish fake laser scan (all rays at max range for empty map)"""
        scan = LaserScan()
        scan.header.stamp = rospy.Time.now()
        scan.header.frame_id = "laser_link"

        scan.angle_min = -math.pi
        scan.angle_max = math.pi
        scan.angle_increment = math.pi / 180.0
        scan.range_min = 0.1
        scan.range_max = 10.0

        num_readings = int((scan.angle_max - scan.angle_min) / scan.angle_increment) + 1
        scan.ranges = [scan.range_max] * num_readings

        self.laser_pub.publish(scan)

    def publish_test_status(self):
        """Publish test status for performance analysis"""
        status = Float64MultiArray()
        status.data = [
            self.x,
            self.y,
            self.theta,
            self.linear_vel,
            self.angular_vel,
            self.elapsed_time,
            self.total_distance
        ]
        self.test_status_pub.publish(status)

    @staticmethod
    def yaw_to_quaternion(yaw):
        """Convert yaw angle to quaternion"""
        quat = Quaternion()
        quat.x = 0.0
        quat.y = 0.0
        quat.z = math.sin(yaw / 2.0)
        quat.w = math.cos(yaw / 2.0)
        return quat

if __name__ == '__main__':
    try:
        sim = KinematicRobotSim()
        rospy.spin()
    except rospy.ROSInterruptException:
        pass
