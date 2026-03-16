#!/usr/bin/env python3
"""
STL Monitor ROS Node - Standalone Version
No external dependencies for testing
"""

import rospy
import numpy as np
from std_msgs.msg import Float32, Bool
from geometry_msgs.msg import PoseWithCovarianceStamped
from nav_msgs.msg import Path, Odometry
from tf.transformations import euler_from_quaternion
import csv
import os
from datetime import datetime

class STLMonitorNode:
    """
    STL Monitor with:
    - Reachability robustness computation
    - Robustness budget mechanism
    - ROS integration
    - Data logging
    """

    def __init__(self):
        rospy.init_node('stl_monitor', anonymous=False)

        # Parameters
        self.temperature = rospy.get_param('~temperature', 0.1)
        self.use_belief_space = rospy.get_param('~use_belief_space', False)
        self.reachability_threshold = rospy.get_param('~reachability_threshold', 0.5)
        self.baseline_requirement = rospy.get_param('~baseline_requirement', 0.1)
        self.update_frequency = rospy.get_param('~update_frequency', 10.0)

        # State variables
        self.current_pose = None
        self.global_path = None
        self.robustness = 1.0
        self.budget = 1.0
        self.violation = False
        self.belief_covariance = np.eye(2) * 0.1

        # Statistics
        self.evaluation_count = 0
        self.total_evaluation_time = 0.0
        self.robustness_history = []
        self.budget_history = []

        # Data logging
        self.log_enabled = rospy.get_param('~log_enabled', True)
        self.log_file = rospy.get_param('~log_file', '/tmp/stl_monitor_data.csv')
        self.log_frequency = rospy.get_param('~log_frequency', 10.0)

        if self.log_enabled:
            self.init_logging()

        # Subscribers
        rospy.Subscriber('/amcl_pose', PoseWithCovarianceStamped, self.pose_callback)
        rospy.Subscriber('/move_base/GlobalPlanner/plan', Path, self.path_callback)

        if self.use_belief_space:
            rospy.Subscriber('/ekf/odom', Odometry, self.odom_callback)

        # Publishers
        self.robustness_pub = rospy.Publisher('/stl/robustness', Float32, queue_size=10)
        self.violation_pub = rospy.Publisher('/stl/violation', Bool, queue_size=10)
        self.budget_pub = rospy.Publisher('/stl/budget', Float32, queue_size=10)

        # Timer for STL evaluation
        self.timer = rospy.Timer(rospy.Duration(1.0 / self.update_frequency), self.evaluate_stl)

        rospy.loginfo("="*60)
        rospy.loginfo("STL Monitor Node Started")
        rospy.loginfo("="*60)
        rospy.loginfo(f"Temperature: {self.temperature}")
        rospy.loginfo(f"Reachability Threshold: {self.reachability_threshold}")
        rospy.loginfo(f"Belief Space: {self.use_belief_space}")
        rospy.loginfo(f"Update Frequency: {self.update_frequency} Hz")
        rospy.loginfo(f"Data Logging: {self.log_enabled}")
        rospy.loginfo("="*60)

    def pose_callback(self, msg):
        """Callback for robot pose updates"""
        self.current_pose = msg.pose.pose

    def path_callback(self, msg):
        """Callback for global path updates"""
        self.global_path = msg

    def odom_callback(self, msg):
        """Callback for odometry with covariance (belief space)"""
        if msg.pose.covariance:
            self.belief_covariance = np.array(msg.pose.covariance).reshape(6, 6)[:2, :2]

    def evaluate_stl(self, event):
        """Evaluate STL robustness"""
        start_time = rospy.Time.now()

        if self.current_pose is None or self.global_path is None:
            return

        if len(self.global_path.poses) == 0:
            return

        try:
            # Compute robustness
            self.robustness = self.compute_robustness()

            # Update budget: R[k+1] = max{0, R[k] + ρ[k] - r̲}
            self.budget = max(0.0, self.budget + self.robustness - self.baseline_requirement)

            # Check violation
            self.violation = (self.robustness < 0.0)

            # Publish results
            self.robustness_pub.publish(Float32(self.robustness))
            self.violation_pub.publish(Bool(self.violation))
            self.budget_pub.publish(Float32(self.budget))

            # Update statistics
            self.evaluation_count += 1
            elapsed = (rospy.Time.now() - start_time).to_sec()
            self.total_evaluation_time += elapsed

            self.robustness_history.append(self.robustness)
            self.budget_history.append(self.budget)

            # Log data periodically
            if self.log_enabled and self.evaluation_count % int(self.log_frequency) == 0:
                self.log_data()

            # Print statistics periodically
            if self.evaluation_count % 100 == 0:
                self.print_statistics()

            # Log violations
            if self.violation:
                rospy.logwarn(f"STL Violation! Robustness: {self.robustness:.3f}, Budget: {self.budget:.3f}")

        except Exception as e:
            rospy.logerr(f"Error in STL evaluation: {e}")

    def compute_robustness(self):
        """Compute reachability robustness"""
        # Extract robot state
        x = self.current_pose.position.x
        y = self.current_pose.position.y
        quaternion = [
            self.current_pose.orientation.x,
            self.current_pose.orientation.y,
            self.current_pose.orientation.z,
            self.current_pose.orientation.w
        ]
        theta = euler_from_quaternion(quaternion)[2]

        # Get goal position (last pose in path)
        goal_pose = self.global_path.poses[-1].pose
        goal_x = goal_pose.position.x
        goal_y = goal_pose.position.y

        # Calculate distance to goal
        distance = np.sqrt((x - goal_x)**2 + (y - goal_y)**2)

        # Base robustness: ρ = threshold - distance
        base_robustness = self.reachability_threshold - distance

        # Apply belief space adjustment if enabled
        if self.use_belief_space:
            uncertainty = np.sqrt(np.trace(self.belief_covariance))
            uncertainty_penalty = rospy.get_param('~uncertainty_penalty', 0.5)
            belief_robustness = base_robustness - uncertainty_penalty * uncertainty
        else:
            belief_robustness = base_robustness

        return belief_robustness

    def init_logging(self):
        """Initialize CSV logging"""
        try:
            file_exists = os.path.isfile(self.log_file)

            with open(self.log_file, 'a') as f:
                writer = csv.writer(f)

                if not file_exists:
                    # Write header
                    writer.writerow([
                        'timestamp',
                        'robustness',
                        'budget',
                        'violation',
                        'robot_x',
                        'robot_y',
                        'robot_theta',
                        'goal_distance',
                        'evaluation_time_ms'
                    ])

            rospy.loginfo(f"Logging to: {self.log_file}")

        except Exception as e:
            rospy.logerr(f"Failed to initialize logging: {e}")
            self.log_enabled = False

    def log_data(self):
        """Log current data to CSV"""
        if not self.log_enabled:
            return

        try:
            if self.current_pose is None or self.global_path is None:
                return

            # Extract state
            x = self.current_pose.position.x
            y = self.current_pose.position.y
            quaternion = [
                self.current_pose.orientation.x,
                self.current_pose.orientation.y,
                self.current_pose.orientation.z,
                self.current_pose.orientation.w
            ]
            theta = euler_from_quaternion(quaternion)[2]

            # Goal distance
            if len(self.global_path.poses) > 0:
                goal_pose = self.global_path.poses[-1].pose
                goal_x = goal_pose.position.x
                goal_y = goal_pose.position.y
                goal_dist = np.sqrt((x - goal_x)**2 + (y - goal_y)**2)
            else:
                goal_dist = 0.0

            # Average evaluation time
            if self.evaluation_count > 0:
                avg_time = (self.total_evaluation_time / self.evaluation_count) * 1000  # ms
            else:
                avg_time = 0.0

            # Write row
            with open(self.log_file, 'a') as f:
                writer = csv.writer(f)
                writer.writerow([
                    datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f'),
                    f'{self.robustness:.6f}',
                    f'{self.budget:.6f}',
                    1 if self.violation else 0,
                    f'{x:.6f}',
                    f'{y:.6f}',
                    f'{theta:.6f}',
                    f'{goal_dist:.6f}',
                    f'{avg_time:.3f}'
                ])

        except Exception as e:
            rospy.logerr(f"Logging error: {e}")

    def print_statistics(self):
        """Print performance statistics"""
        if self.evaluation_count == 0:
            return

        avg_time = (self.total_evaluation_time / self.evaluation_count) * 1000  # ms
        freq = 1.0 / (self.total_evaluation_time / self.evaluation_count) if self.total_evaluation_time > 0 else 0.0

        min_rob = min(self.robustness_history) if self.robustness_history else 0.0
        max_rob = max(self.robustness_history) if self.robustness_history else 0.0
        avg_rob = np.mean(self.robustness_history) if self.robustness_history else 0.0

        min_bud = min(self.budget_history) if self.budget_history else 0.0
        max_bud = max(self.budget_history) if self.budget_history else 0.0

        rospy.loginfo("="*60)
        rospy.loginfo("STL Monitor Statistics")
        rospy.loginfo("="*60)
        rospy.loginfo(f"Evaluations: {self.evaluation_count}")
        rospy.loginfo(f"Average Time: {avg_time:.3f} ms ({freq:.1f} Hz)")
        rospy.loginfo(f"Robustness: min={min_rob:.3f}, max={max_rob:.3f}, avg={avg_rob:.3f}")
        rospy.loginfo(f"Budget:      min={min_bud:.3f}, max={max_bud:.3f}, cur={self.budget:.3f}")
        rospy.loginfo("="*60)

    def shutdown(self):
        """Clean shutdown"""
        rospy.loginfo("STL Monitor shutting down...")
        if self.log_enabled:
            self.print_statistics()
        rospy.loginfo(f"Data logged to: {self.log_file}")

    def run(self):
        """Run the node"""
        rospy.on_shutdown(self.shutdown)
        rospy.spin()

def main():
    try:
        monitor = STLMonitorNode()
        monitor.run()
    except rospy.ROSInterruptException:
        pass

if __name__ == '__main__':
    main()
