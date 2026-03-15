#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
STL Monitor Python Node
Provides ROS interface for STL monitoring and evaluation
"""

import rospy
import math
import numpy as np
from std_msgs.msg import Header
from stl_ros.msg import STLRobustness, STLBudget, STLFormula
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Path

# Ensure numpy is available
try:
    import numpy as np
except ImportError:
    rospy.logerr("numpy not available, installing...")
    import subprocess
    subprocess.check_call(['pip3', 'install', 'numpy'])
    import numpy as np


class STLMonitorNode:
    """STL Monitor ROS Node"""

    def __init__(self):
        rospy.init_node('stl_monitor', anonymous=True)

        # Parameters
        self.prediction_horizon = rospy.get_param('~prediction_horizon', 20)
        self.temperature_tau = rospy.get_param('~temperature_tau', 0.05)
        self.num_monte_carlo_samples = rospy.get_param('~num_monte_carlo_samples', 100)
        self.baseline_robustness_r = rospy.get_param('~baseline_robustness_r', 0.1)
        self.stl_weight_lambda = rospy.get_param('~stl_weight_lambda', 1.0)
        self.enable_visualization = rospy.get_param('~enable_visualization', True)

        # State
        self.current_belief = None  # Current belief position
        self.current_trajectory = None  # Current MPC trajectory
        self.robustness_budget = 1.0  # Initial budget R_k

        # Publishers
        self.robustness_pub = rospy.Publisher('~robustness', STLRobustness, queue_size=10)
        self.budget_pub = rospy.Publisher('~budget', STLBudget, queue_size=10)

        # Subscribers
        rospy.Subscriber('~belief', PoseStamped, self.belief_callback)
        rospy.Subscriber('~mpc_trajectory', Path, self.trajectory_callback)

        rospy.loginfo("STL Monitor Node initialized")
        rospy.loginfo("Parameters: horizon=%d, tau=%.3f, samples=%d",
                     self.prediction_horizon, self.temperature_tau, self.num_monte_carlo_samples)

    def belief_callback(self, msg):
        """Handle belief state updates"""
        self.current_belief = msg
        rospy.loginfo("Received belief update at (%.2f, %.2f)",
                     msg.pose.position.x, msg.pose.position.y)
        self.evaluate_stl_formulas()

    def trajectory_callback(self, msg):
        """Handle MPC trajectory updates"""
        self.current_trajectory = msg
        rospy.loginfo("Received MPC trajectory with %d poses", len(msg.poses))
        self.evaluate_stl_formulas()

    def smooth_max(self, values):
        """Compute smooth maximum using log-sum-exp (smax_τ)"""
        if not values:
            return float('-inf')

        # Scale values to prevent overflow
        max_val = max(values)
        scaled_values = [v / self.temperature_tau for v in values]
        max_scaled = max_val / self.temperature_tau

        # smax_τ(z) = τ * log(Σᵢ e^(zᵢ/τ))
        try:
            log_sum_exp = max_scaled + math.log(sum(math.exp(v - max_scaled) for v in scaled_values))
            return self.temperature_tau * log_sum_exp
        except (OverflowError, ValueError):
            return max_val

    def smooth_min(self, values):
        """Compute smooth minimum (smin_τ = -smax_τ(-values))"""
        if not values:
            return float('inf')
        neg_values = [-v for v in values]
        return -self.smooth_max(neg_values)

    def evaluate_stay_in_bounds(self, trajectory):
        """Evaluate 'stay in bounds' STL formula"""
        if not trajectory or len(trajectory.poses) == 0:
            return 0.0

        # Formula: always[0,T](0 < x < 10 && 0 < y < 10)
        # Robustness: min_t(min(x_t, 10-x_t, y_t, 10-y_t))
        min_robustness = float('inf')

        for pose in trajectory.poses:
            x, y = pose.pose.position.x, pose.pose.position.y
            # Distance to boundaries
            dist_x_min = x - 0.0
            dist_x_max = 10.0 - x
            dist_y_min = y - 0.0
            dist_y_max = 10.0 - y

            # Minimum distance to any boundary
            pose_robustness = min(dist_x_min, dist_x_max, dist_y_min, dist_y_max)
            min_robustness = min(min_robustness, pose_robustness)

        return min_robustness if min_robustness != float('inf') else 0.0

    def evaluate_reach_goal(self, trajectory):
        """Evaluate 'reach goal' STL formula"""
        if not trajectory or len(trajectory.poses) == 0:
            return 0.0

        # Formula: eventually[0,T](at_goal)
        # Goal position: (8.0, 8.0), radius: 0.5
        goal_x, goal_y = 8.0, 8.0
        goal_radius = 0.5

        # Robustness: max_t(goal_radius - distance_to_goal)
        max_robustness = float('-inf')

        for pose in trajectory.poses:
            x, y = pose.pose.position.x, pose.pose.position.y
            dist_to_goal = math.sqrt((x - goal_x)**2 + (y - goal_y)**2)
            pose_robustness = goal_radius - dist_to_goal
            max_robustness = max(max_robustness, pose_robustness)

        return max_robustness if max_robustness != float('-inf') else 0.0

    def compute_expected_robustness(self, trajectory):
        """Compute expected robustness using Monte Carlo sampling"""
        # E[ρ^soft(φ)] ≈ (1/M) Σ ρ^soft(φ; x^(m))
        total_robustness = 0.0

        for _ in range(self.num_monte_carlo_samples):
            # Add Gaussian noise to trajectory (belief space sampling)
            noisy_trajectory = self.add_noise_to_trajectory(trajectory)

            # Evaluate STL formulas on noisy trajectory
            stay_robustness = self.evaluate_stay_in_bounds(noisy_trajectory)
            reach_robustness = self.evaluate_reach_goal(noisy_trajectory)

            # Combined robustness (smooth min of formulas)
            combined = self.smooth_min([stay_robustness, reach_robustness])
            total_robustness += combined

        expected_robustness = total_robustness / self.num_monte_carlo_samples
        return expected_robustness

    def add_noise_to_trajectory(self, trajectory):
        """Add Gaussian noise to trajectory for Monte Carlo sampling"""
        if not trajectory:
            return trajectory

        # Create a copy with added noise
        noisy_trajectory = Path()
        noisy_trajectory.header = trajectory.header

        for pose in trajectory.poses:
            noisy_pose = PoseStamped()
            noisy_pose.header = pose.header

            # Add Gaussian noise (σ = 0.1)
            noise_x = np.random.normal(0, 0.1)
            noise_y = np.random.normal(0, 0.1)

            noisy_pose.pose.position.x = pose.pose.position.x + noise_x
            noisy_pose.pose.position.y = pose.pose.position.y + noise_y
            noisy_pose.pose.position.z = pose.pose.position.z
            noisy_pose.pose.orientation = pose.pose.orientation

            noisy_trajectory.poses.append(noisy_pose)

        return noisy_trajectory

    def evaluate_stl_formulas(self):
        """Evaluate all STL formulas and update budget"""
        if not self.current_trajectory:
            return

        # Compute expected robustness: E[ρ^soft(φ)]
        expected_robustness = self.compute_expected_robustness(self.current_trajectory)

        # Evaluate individual formulas
        stay_robustness = self.evaluate_stay_in_bounds(self.current_trajectory)
        reach_robustness = self.evaluate_reach_goal(self.current_trajectory)

        # Update budget: R_{k+1} = max{0, R_k + ρ̃_k - r̲}
        budget_update = self.robustness_budget + expected_robustness - self.baseline_robustness_r
        self.robustness_budget = max(0.0, budget_update)

        # Check feasibility
        feasible = self.robustness_budget >= 0.0

        # Publish robustness results
        self.publish_robustness("stay_in_bounds", stay_robustness, expected_robustness, feasible)
        self.publish_robustness("reach_goal", reach_robustness, expected_robustness, feasible)

        # Publish budget status
        self.publish_budget(self.robustness_budget, self.baseline_robustness_r, feasible)

        rospy.loginfo("STL Evaluation: ρ̃=%.3f, R=%.3f, feasible=%s",
                     expected_robustness, self.robustness_budget, feasible)

    def publish_robustness(self, formula_name, robustness_value, expected_robustness, feasible):
        """Publish STL robustness message"""
        msg = STLRobustness()
        msg.header = Header()
        msg.header.stamp = rospy.Time.now()
        msg.formula_name = formula_name
        msg.robustness = robustness_value
        msg.expected_robustness = expected_robustness
        msg.satisfied = robustness_value > 0
        msg.budget_feasible = feasible
        self.robustness_pub.publish(msg)

    def publish_budget(self, current_budget, baseline_r, feasible):
        """Publish budget status message"""
        msg = STLBudget()
        msg.header = Header()
        msg.header.stamp = rospy.Time.now()
        msg.current_budget = current_budget
        msg.baseline_r = baseline_r
        msg.feasible = feasible  # Overall feasibility
        msg.formula_names = ["stay_in_bounds", "reach_goal"]
        msg.budgets = [current_budget, current_budget]
        msg.formula_feasible = [feasible, feasible]  # Per-formula feasibility
        msg.min_budget = 0.0
        msg.max_budget = max(10.0, current_budget)
        msg.mean_budget = current_budget
        msg.violation_count = 0 if feasible else 1
        msg.predicted_budgets = [current_budget] * 5  # 5-step prediction
        msg.confidence_bounds = [0.1] * 5
        msg.update_frequency_hz = 10.0
        self.budget_pub.publish(msg)


def main():
    """Main entry point"""
    try:
        node = STLMonitorNode()
        rospy.spin()
    except rospy.ROSInterruptException:
        pass


if __name__ == '__main__':
    main()
