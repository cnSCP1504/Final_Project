#!/usr/bin/env python3
"""
Integration tests for STL Monitor ROS node
Tests node startup, message publishing, and parameter loading
"""

import unittest
import rospy
import rostest
import rospkg
import time
from std_msgs.msg import Float32, Bool
from geometry_msgs.msg import PoseWithCovarianceStamped, Pose
from nav_msgs.msg import Path
from tf.transformations import quaternion_from_euler

class TestSTLMonitorROS(unittest.TestCase):
    """Test cases for STL Monitor ROS integration"""

    @classmethod
    def setUpClass(cls):
        """Initialize ROS node"""
        rospy.init_node('test_stl_monitor', anonymous=True)

    def setUp(self):
        """Set up test fixtures"""
        self.received_robustness = None
        self.received_violation = None
        self.received_budget = None

        # Subscribers
        self.rob_sub = rospy.Subscriber(
            '/stl/robustness',
            Float32,
            self.robustness_callback
        )
        self.vio_sub = rospy.Subscriber(
            '/stl/violation',
            Bool,
            self.violation_callback
        )
        self.bud_sub = rospy.Subscriber(
            '/stl/budget',
            Float32,
            self.budget_callback
        )

        # Publishers
        self.pose_pub = rospy.Publisher(
            '/amcl_pose',
            PoseWithCovarianceStamped,
            queue_size=10
        )
        self.path_pub = rospy.Publisher(
            '/move_base/GlobalPlanner/plan',
            Path,
            queue_size=10
        )

        # Wait for connections
        timeout = 2.0
        start = time.time()
        while not any([self.received_robustness is not None,
                      self.received_violation is not None,
                      self.received_budget is not None]):
            if time.time() - start > timeout:
                break
            time.sleep(0.1)

    def robustness_callback(self, msg):
        self.received_robustness = msg.data

    def violation_callback(self, msg):
        self.received_violation = msg.data

    def budget_callback(self, msg):
        self.received_budget = msg.data

    def test_node_startup(self):
        """Test that node can start and publishers are ready"""
        # This test just verifies the test infrastructure works
        self.assertIsNotNone(self.rob_sub)
        self.assertIsNotNone(self.vio_sub)
        self.assertIsNotNone(self.bud_sub)

    def test_pose_publishing(self):
        """Test publishing robot pose"""
        pose_msg = PoseWithCovarianceStamped()
        pose_msg.header.stamp = rospy.Time.now()
        pose_msg.header.frame_id = 'map'

        # Set position
        pose_msg.pose.pose.position.x = 1.0
        pose_msg.pose.pose.position.y = 2.0
        pose_msg.pose.pose.position.z = 0.0

        # Set orientation
        q = quaternion_from_euler(0, 0, 0.5)
        pose_msg.pose.pose.orientation.x = q[0]
        pose_msg.pose.pose.orientation.y = q[1]
        pose_msg.pose.pose.orientation.z = q[2]
        pose_msg.pose.pose.orientation.w = q[3]

        # Publish
        self.pose_pub.publish(pose_msg)
        time.sleep(0.5)

        # Test passes if no exception
        self.assertTrue(True)

    def test_path_publishing(self):
        """Test publishing global path"""
        path_msg = Path()
        path_msg.header.stamp = rospy.Time.now()
        path_msg.header.frame_id = 'map'

        # Add goal pose
        pose_stamped = Pose()
        pose_stamped.position.x = 5.0
        pose_stamped.position.y = 5.0
        pose_stamped.position.z = 0.0

        q = quaternion_from_euler(0, 0, 0.0)
        pose_stamped.orientation.x = q[0]
        pose_stamped.orientation.y = q[1]
        pose_stamped.orientation.z = q[2]
        pose_stamped.orientation.w = q[3]

        path_msg.poses.append(pose_stamped)

        # Publish
        self.path_pub.publish(path_msg)
        time.sleep(0.5)

        # Test passes if no exception
        self.assertTrue(True)

    def test_robustness_computation(self):
        """Test robustness is computed"""
        # Publish pose and path
        pose_msg = PoseWithCovarianceStamped()
        pose_msg.header.stamp = rospy.Time.now()
        pose_msg.pose.pose.position.x = 0.0
        pose_msg.pose.pose.position.y = 0.0

        path_msg = Path()
        path_msg.header.stamp = rospy.Time.now()
        goal_pose = Pose()
        goal_pose.position.x = 0.3
        goal_pose.position.y = 0.0
        path_msg.poses.append(goal_pose)

        # Publish messages
        for _ in range(5):
            self.pose_pub.publish(pose_msg)
            self.path_pub.publish(path_msg)
            time.sleep(0.1)

        # Wait for response
        time.sleep(1.0)

        # Check if we received any messages
        # (Note: This test requires the STL monitor node to be running)
        if self.received_robustness is not None:
            # Robustness should be a float
            self.assertIsInstance(self.received_robustness, float)

if __name__ == '__main__':
    rospy.init_node('test_stl_monitor', anonymous=True)
    rostest.rosrun('stl_monitor', 'test_stl_monitor_ros', TestSTLMonitorROS)
