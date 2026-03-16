#!/usr/bin/env python3
"""
STL Monitor ROS Node - Main implementation
Integrates STL parsing, robustness computation, and budget management
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

# Import STL modules
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../STL_ros/scripts'))
from stl_monitor import STLMonitor as BaseMonitor

class STLMonitorNode(BaseMonitor):
    """
    Enhanced STL Monitor with:
    - Belief space evaluation
    - Smooth robustness computation
    - Robustness budget mechanism
    - ROS integration
    """

    def __init__(self):
        # Initialize base class
        super(STLMonitorNode, self).__init__()

        # Enhanced parameters
        self.temperature = rospy.get_param('~temperature', 0.1)
        self.use_belief_space = rospy.get_param('~use_belief_space', False)
        self.num_particles = rospy.get_param('~num_particles', 100)
        self.prediction_horizon = rospy.get_param('~prediction_horizon', 2.0)
        self.prediction_dt = rospy.get_param('~prediction_dt', 0.1)

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
            self.init_logging_()

        # Additional subscribers for belief space
        if self.use_belief_space:
            rospy.Subscriber('/ekf/odom', Odometry, self.odom_callback)
            self.belief_covariance = np.eye(3) * 0.1

        # Enhanced timer with statistics
        self.timer.shutdown()
        self.timer = rospy.Timer(
            rospy.Duration(1.0 / rospy.get_param('~update_frequency', 10.0)),
            self.evaluate_stl_enhanced
        )

        rospy.loginfo("="*60)
        rospy.loginfo("STL Monitor Node Started - Enhanced Version")
        rospy.loginfo("="*60)
        rospy.loginfo(f"Temperature: {self.temperature}")
        rospy.loginfo(f"Belief Space: {self.use_belief_space}")
        rospy.loginfo(f"Num Particles: {self.num_particles}")
        rospy.loginfo(f"Prediction Horizon: {self.prediction_horizon}s")
        rospy.loginfo(f"Data Logging: {self.log_enabled} -> {self.log_file}")
        rospy.loginfo("="*60)

    def odom_callback(self, msg):
        """Callback for odometry with covariance (belief space)"""
        # Extract position covariance
        if msg.pose.covariance:
            self.belief_covariance = np.array(msg.pose.covariance).reshape(6, 6)[:2, :2]

    def evaluate_stl_enhanced(self, event):
        """Enhanced STL evaluation with statistics and logging"""
        start_time = rospy.Time.now()

        if self.current_pose is None or self.global_path is None:
            return

        try:
            # Compute robustness
            robustness = self.compute_robustness_enhanced()

            # Update budget
            baseline = rospy.get_param('~baseline_requirement', 0.1)
            self.budget = max(0.0, self.budget + robustness - baseline)

            # Check violation
            self.violation = (robustness < 0.0)

            # Publish results
            self.publish_results()

            # Update statistics
            self.evaluation_count += 1
            elapsed = (rospy.Time.now() - start_time).to_sec()
            self.total_evaluation_time += elapsed

            self.robustness_history.append(robustness)
            self.budget_history.append(self.budget)

            # Log data periodically
            if self.log_enabled and self.evaluation_count % int(self.log_frequency) == 0:
                self.log_data()

            # Print statistics periodically
            if self.evaluation_count % 100 == 0:
                self.print_statistics()

        except Exception as e:
            rospy.logerr(f"Error in enhanced STL evaluation: {e}")

    def compute_robustness_enhanced(self):
        """Compute enhanced robustness with smooth approximation"""
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

        # Get goal
        if len(self.global_path.poses) == 0:
            return 0.0

        goal_pose = self.global_path.poses[-1].pose
        goal_x = goal_pose.position.x
        goal_y = goal_pose.position.y

        # Compute distance to goal
        distance = np.sqrt((x - goal_x)**2 + (y - goal_y)**2)

        # Base robustness: reachability
        threshold = rospy.get_param('~reachability_threshold', 0.5)
        base_robustness = threshold - distance

        # Apply smooth min/max for temporal logic
        if self.use_belief_space:
            # Belief space adjustment
            uncertainty = np.sqrt(np.trace(self.belief_covariance))
            uncertainty_penalty = rospy.get_param('~uncertainty_penalty', 0.5)
            belief_robustness = base_robustness - uncertainty_penalty * uncertainty
        else:
            belief_robustness = base_robustness

        # Apply temperature-based smoothing (log-sum-exp approximation)
        # For a single value, just return it
        # For multiple predicates, would use: smin/max(x) = ±τ * log(Σ exp(∓x_i/τ))

        return belief_robustness

    def publish_results(self):
        """Publish STL evaluation results"""
        # Publish robustness
        rob_msg = Float32()
        rob_msg.data = float(self.robustness)
        self.robustness_pub.publish(rob_msg)

        # Publish violation
        vio_msg = Bool()
        vio_msg.data = self.violation
        self.violation_pub.publish(vio_msg)

        # Publish budget
        bud_msg = Float32()
        bud_msg.data = float(self.budget)
        self.budget_pub.publish(bud_msg)

    def init_logging_(self):
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
            avg_time = (self.total_evaluation_time / self.evaluation_count) * 1000  # ms

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
        freq = 1.0 / (self.total_evaluation_time / self.evaluation_count)

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
        rospy.loginfo(f"Violations:  {int(self.budget)}")
        rospy.loginfo("="*60)

    def shutdown(self):
        """Clean shutdown"""
        rospy.loginfo("STL Monitor shutting down...")
        if self.log_enabled:
            self.print_statistics()
        rospy.loginfo(f"Data logged to: {self.log_file}")

def main():
    rospy.init_node('stl_monitor_node', anonymous=False)

    try:
        monitor = STLMonitorNode()
        rospy.on_shutdown(monitor.shutdown)
        monitor.run()

    except rospy.ROSInterruptException:
        pass

if __name__ == '__main__':
    main()
