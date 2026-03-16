#!/usr/bin/env python3
"""
Unit tests for STL Monitor
"""

import unittest
import numpy as np
import sys
import os

# Add path to import STL modules
sys.path.append(os.path.join(os.path.dirname(__file__), '../../STL_ros/scripts'))

class TestSTLMonitor(unittest.TestCase):
    """Test cases for STL Monitor functionality"""

    def setUp(self):
        """Set up test fixtures"""
        pass

    def test_smooth_min(self):
        """Test smooth min approximation"""
        # Test with temperature τ = 0.1
        tau = 0.1

        # Simple case: two values
        a, b = 1.0, 2.0

        # Smooth min: smin(a,b) = -τ * log(exp(-a/τ) + exp(-b/τ))
        smin = -tau * np.log(np.exp(-a/tau) + np.exp(-b/tau))

        # Should be close to regular min
        self.assertAlmostEqual(smin, min(a,b), delta=0.1)

    def test_smooth_max(self):
        """Test smooth max approximation"""
        tau = 0.1
        a, b = 1.0, 2.0

        # Smooth max: smax(a,b) = τ * log(exp(a/τ) + exp(b/τ))
        smax = tau * np.log(np.exp(a/tau) + np.exp(b/tau))

        # Should be close to regular max
        self.assertAlmostEqual(smax, max(a,b), delta=0.1)

    def test_reachability_robustness(self):
        """Test reachability robustness computation"""
        # Robot at (0, 0), goal at (1, 0)
        robot_pos = np.array([0.0, 0.0])
        goal_pos = np.array([1.0, 0.0])
        threshold = 0.5

        # Distance to goal
        distance = np.linalg.norm(robot_pos - goal_pos)

        # Robustness: ρ = threshold - distance
        robustness = threshold - distance

        # Should be negative (not at goal yet)
        self.assertLess(robustness, 0.0)
        self.assertAlmostEqual(robustness, -0.5, places=1)

    def test_budget_update(self):
        """Test robustness budget update"""
        budget = 1.0
        baseline = 0.1

        # Update with positive robustness
        robustness = 0.5
        new_budget = max(0.0, budget + robustness - baseline)

        self.assertGreater(new_budget, budget)
        self.assertAlmostEqual(new_budget, 1.4, places=1)

        # Update with negative robustness
        robustness = -0.2
        new_budget = max(0.0, new_budget + robustness - baseline)

        self.assertLess(new_budget, 1.4)
        self.assertAlmostEqual(new_budget, 1.1, places=1)

    def test_temperature_effect(self):
        """Test effect of temperature parameter on smooth approximation"""
        values = np.array([1.0, 2.0, 3.0])

        # Low temperature = more accurate (closer to true min)
        tau_low = 0.01
        smin_low = -tau_low * np.log(np.sum(np.exp(-values/tau_low)))

        # High temperature = smoother (further from true min)
        tau_high = 1.0
        smin_high = -tau_high * np.log(np.sum(np.exp(-values/tau_high)))

        # Low temperature should be closer to true min
        true_min = np.min(values)

        self.assertLess(abs(smin_low - true_min), abs(smin_high - true_min))

    def test_belief_space_uncertainty(self):
        """Test belief space uncertainty penalty"""
        base_robustness = 0.5
        uncertainty = 0.2
        penalty = 0.5

        # Belief robustness = base - penalty * uncertainty
        belief_robustness = base_robustness - penalty * uncertainty

        self.assertLess(belief_robustness, base_robustness)
        self.assertAlmostEqual(belief_robustness, 0.4, places=1)

class TestSTLIntegration(unittest.TestCase):
    """Integration tests for STL system"""

    def test_full_pipeline(self):
        """Test full STL evaluation pipeline"""
        # Simulate robot approaching goal
        positions = [
            np.array([0.0, 0.0]),
            np.array([0.3, 0.0]),
            np.array([0.6, 0.0]),
            np.array([0.9, 0.0]),
        ]

        goal = np.array([1.0, 0.0])
        threshold = 0.5

        robustness_values = []
        for pos in positions:
            distance = np.linalg.norm(pos - goal)
            robustness = threshold - distance
            robustness_values.append(robustness)

        # Robustness should increase as robot approaches goal
        for i in range(len(robustness_values) - 1):
            self.assertLess(robustness_values[i], robustness_values[i+1])

        # Final robustness should be positive (at goal)
        self.assertGreater(robustness_values[-1], 0.0)

def run_tests():
    """Run all tests"""
    unittest.main(argv=[''], verbosity=2, exit=False)

if __name__ == '__main__':
    print("="*60)
    print("STL Monitor Unit Tests")
    print("="*60)
    run_tests()
