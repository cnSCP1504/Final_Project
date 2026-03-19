#!/usr/bin/env python3
"""
Automated Batch Benchmark Runner
Runs multiple navigation tests with random scenarios and generates performance reports
"""

import rospy
import math
import random
import time
import json
import csv
import os
from datetime import datetime
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped
from std_msgs.msg import Float64MultiArray
from nav_msgs.msg import Path
from move_base_msgs.msg import MoveBaseAction, MoveBaseGoal
import actionlib

class BenchmarkRunner:
    """
    Automated batch testing for Tube MPC navigation
    """
    def __init__(self):
        rospy.init_node('benchmark_runner', anonymous=False)

        # Load parameters
        self.config_file = rospy.get_param('~config_file', '')
        self.num_tests = rospy.get_param('~num_tests', 100)
        self.map_size = rospy.get_param('~map_size', 20.0)  # meters
        self.min_goal_distance = rospy.get_param('~min_goal_distance', 5.0)  # meters
        self.max_goal_distance = rospy.get_param('~max_goal_distance', 15.0)  # meters
        self.goal_tolerance = rospy.get_param('~goal_tolerance', 0.3)  # meters
        self.timeout = rospy.get_param('~timeout', 60.0)  # seconds per test

        # Results storage
        self.results = []
        self.test_number = 0
        self.output_dir = rospy.get_param('~output_dir', '/tmp/tube_mpc_benchmark')
        os.makedirs(self.output_dir, exist_ok=True)

        # Control interface
        self.test_control_pub = rospy.Publisher('/test_control', Float64MultiArray, queue_size=10)

        # MoveBase client
        self.mb_client = actionlib.SimpleActionClient('move_base', MoveBaseAction)
        rospy.loginfo("Waiting for move_base action server...")
        self.mb_client.wait_for_server()

        # Subscribers for monitoring
        self.test_status_sub = rospy.Subscriber('/test_status', Float64MultiArray, self.test_status_callback)

        # Current test state
        self.current_test_data = None
        self.test_start_time = None
        self.test_active = False

        rospy.loginfo("Benchmark Runner Initialized")
        rospy.loginfo(f"Will run {self.num_tests} tests")
        rospy.loginfo(f"Map size: {self.map_size}x{self.map_size}m")

    def test_status_callback(self, msg):
        """Monitor test status"""
        if self.test_active:
            self.current_test_data = {
                'x': msg.data[0],
                'y': msg.data[1],
                'theta': msg.data[2],
                'linear_vel': msg.data[3],
                'angular_vel': msg.data[4],
                'elapsed_time': msg.data[5],
                'distance': msg.data[6]
            }

    def generate_random_pose(self):
        """Generate random pose within map bounds"""
        x = random.uniform(-self.map_size/2, self.map_size/2)
        y = random.uniform(-self.map_size/2, self.map_size/2)
        theta = random.uniform(-math.pi, math.pi)
        return x, y, theta

    def generate_goal_at_distance(self, start_x, start_y):
        """Generate goal at specified distance from start"""
        # Random direction
        angle = random.uniform(0, 2*math.pi)

        # Random distance within range
        distance = random.uniform(self.min_goal_distance, self.max_goal_distance)

        # Calculate goal position
        goal_x = start_x + distance * math.cos(angle)
        goal_y = start_y + distance * math.sin(angle)

        # Ensure goal is within map bounds
        goal_x = max(-self.map_size/2, min(self.map_size/2, goal_x))
        goal_y = max(-self.map_size/2, min(self.map_size/2, goal_y))

        theta = random.uniform(-math.pi, math.pi)

        return goal_x, goal_y, theta

    def set_robot_pose(self, x, y, theta):
        """Set robot initial pose"""
        # Send test control command (3 = set_pose)
        msg = Float64MultiArray()
        msg.data = [3.0, float(x), float(y), float(theta)]
        self.test_control_pub.publish(msg)
        time.sleep(0.5)  # Wait for sim to update

    def reset_robot(self):
        """Reset robot"""
        msg = Float64MultiArray()
        msg.data = [0.0]  # 0 = reset
        self.test_control_pub.publish(msg)
        time.sleep(0.5)

    def run_single_test(self):
        """Run a single navigation test"""
        self.test_number += 1
        test_info = {
            'test_number': self.test_number,
            'timestamp': datetime.now().isoformat()
        }

        rospy.loginfo(f"\n{'='*60}")
        rospy.loginfo(f"Test {self.test_number}/{self.num_tests}")
        rospy.loginfo(f"{'='*60}")

        # Generate random start and goal
        start_x, start_y, start_theta = self.generate_random_pose()
        goal_x, goal_y, goal_theta = self.generate_goal_at_distance(start_x, start_y)

        test_info.update({
            'start_x': start_x,
            'start_y': start_y,
            'start_theta': start_theta,
            'goal_x': goal_x,
            'goal_y': goal_y,
            'goal_theta': goal_theta,
            'goal_distance': math.sqrt((goal_x-start_x)**2 + (goal_y-start_y)**2)
        })

        rospy.loginfo(f"Start: ({start_x:.2f}, {start_y:.2f}, {start_theta:.2f})")
        rospy.loginfo(f"Goal:  ({goal_x:.2f}, {goal_y:.2f}, {goal_theta:.2f})")
        rospy.loginfo(f"Distance: {test_info['goal_distance']:.2f}m")

        # Reset and set pose
        self.reset_robot()
        self.set_robot_pose(start_x, start_y, start_theta)

        # Set initial pose for AMCL
        initial_pose = PoseWithCovarianceStamped()
        initial_pose.header.stamp = rospy.Time.now()
        initial_pose.header.frame_id = "map"
        initial_pose.pose.pose.position.x = start_x
        initial_pose.pose.pose.position.y = start_y
        initial_pose.pose.pose.position.z = 0.0
        initial_pose.pose.pose.orientation.x = 0.0
        initial_pose.pose.pose.orientation.y = 0.0
        initial_pose.pose.pose.orientation.z = math.sin(start_theta/2)
        initial_pose.pose.pose.orientation.w = math.cos(start_theta/2)

        # Publish initial pose estimate
        initial_pose_pub = rospy.Publisher('/initialpose', PoseWithCovarianceStamped, queue_size=10)
        time.sleep(0.5)
        initial_pose_pub.publish(initial_pose)
        time.sleep(1.0)

        # Create goal
        goal = MoveBaseGoal()
        goal.target_pose.header.stamp = rospy.Time.now()
        goal.target_pose.header.frame_id = "map"
        goal.target_pose.pose.position.x = goal_x
        goal.target_pose.pose.position.y = goal_y
        goal.target_pose.pose.position.z = 0.0
        goal.target_pose.pose.orientation.x = 0.0
        goal.target_pose.pose.orientation.y = 0.0
        goal.target_pose.pose.orientation.z = math.sin(goal_theta/2)
        goal.target_pose.pose.orientation.w = math.cos(goal_theta/2)

        # Start test
        start_msg = Float64MultiArray()
        start_msg.data = [1.0]  # 1 = start
        self.test_control_pub.publish(start_msg)

        # Send goal and track
        self.mb_client.send_goal(goal)
        self.test_active = True
        self.test_start_time = time.time()

        # Wait for result
        finished_before_timeout = self.mb_client.wait_for_result(rospy.Duration(self.timeout))

        # Stop test
        self.test_active = False
        stop_msg = Float64MultiArray()
        stop_msg.data = [2.0]  # 2 = stop
        self.test_control_pub.publish(stop_msg)

        # Record results
        elapsed_time = time.time() - self.test_start_time
        test_info['elapsed_time'] = elapsed_time

        if finished_before_timeout:
            state = self.mb_client.get_state()
            if state == actionlib.GoalStatus.SUCCEEDED:
                # Check final position accuracy
                if self.current_test_data:
                    final_x = self.current_test_data['x']
                    final_y = self.current_test_data['y']
                    position_error = math.sqrt((final_x - goal_x)**2 + (final_y - goal_y)**2)

                    test_info['success'] = position_error < self.goal_tolerance
                    test_info['position_error'] = position_error
                    test_info['final_x'] = final_x
                    test_info['final_y'] = final_y
                    test_info['distance_traveled'] = self.current_test_data['distance']

                    rospy.loginfo(f"✓ Goal reached! Error: {position_error:.3f}m, Time: {elapsed_time:.2f}s")
                else:
                    test_info['success'] = True
                    test_info['position_error'] = 0.0
                    rospy.loginfo(f"✓ Goal reached! Time: {elapsed_time:.2f}s")
            else:
                test_info['success'] = False
                test_info['status'] = f"Failed with state: {state}"
                rospy.logwarn(f"✗ Goal failed with state: {state}")
        else:
            test_info['success'] = False
            test_info['status'] = "Timeout"
            rospy.logwarn(f"✗ Goal timeout after {self.timeout}s")

        # Cancel any lingering goals
        self.mb_client.cancel_goal()

        self.results.append(test_info)

        # Wait between tests
        time.sleep(1.0)

        return test_info['success']

    def run_batch(self):
        """Run full batch of tests"""
        rospy.loginfo(f"\nStarting batch test: {self.num_tests} tests")
        rospy.loginfo(f"Output directory: {self.output_dir}")

        start_time = time.time()

        for i in range(self.num_tests):
            try:
                self.run_single_test()
            except Exception as e:
                rospy.logerr(f"Error in test {self.test_number}: {e}")
                continue

        total_time = time.time() - start_time

        # Generate report
        self.generate_report(total_time)

    def generate_report(self, total_time):
        """Generate performance report"""
        rospy.loginfo(f"\n{'='*60}")
        rospy.loginfo("BENCHMARK RESULTS")
        rospy.loginfo(f"{'='*60}")

        # Calculate statistics
        successful_tests = [r for r in self.results if r.get('success', False)]
        success_rate = len(successful_tests) / len(self.results) * 100 if self.results else 0

        if successful_tests:
            avg_time = sum(r['elapsed_time'] for r in successful_tests) / len(successful_tests)
            avg_error = sum(r.get('position_error', 0) for r in successful_tests) / len(successful_tests)
            avg_distance = sum(r.get('distance_traveled', 0) for r in successful_tests) / len(successful_tests)
        else:
            avg_time = avg_error = avg_distance = 0

        rospy.loginfo(f"Total tests: {len(self.results)}")
        rospy.loginfo(f"Successful: {len(successful_tests)} ({success_rate:.1f}%)")
        rospy.loginfo(f"Avg completion time: {avg_time:.2f}s")
        rospy.loginfo(f"Avg position error: {avg_error:.3f}m")
        rospy.loginfo(f"Avg distance traveled: {avg_distance:.2f}m")
        rospy.loginfo(f"Total benchmark time: {total_time:.1f}s ({total_time/60:.1f}min)")
        rospy.loginfo(f"Avg time per test: {total_time/len(self.results):.2f}s")

        # Save to CSV
        csv_file = os.path.join(self.output_dir, 'benchmark_results.csv')
        with open(csv_file, 'w', newline='') as f:
            if self.results:
                writer = csv.DictWriter(f, fieldnames=self.results[0].keys())
                writer.writeheader()
                writer.writerows(self.results)
        rospy.loginfo(f"\nResults saved to: {csv_file}")

        # Save summary JSON
        summary = {
            'timestamp': datetime.now().isoformat(),
            'total_tests': len(self.results),
            'successful_tests': len(successful_tests),
            'success_rate': success_rate,
            'avg_completion_time': avg_time,
            'avg_position_error': avg_error,
            'avg_distance_traveled': avg_distance,
            'total_benchmark_time': total_time
        }

        json_file = os.path.join(self.output_dir, 'benchmark_summary.json')
        with open(json_file, 'w') as f:
            json.dump(summary, f, indent=2)
        rospy.loginfo(f"Summary saved to: {json_file}")

if __name__ == '__main__':
    try:
        runner = BenchmarkRunner()
        runner.run_batch()
        rospy.loginfo("\n✓ Benchmark complete!")
    except rospy.ROSInterruptException:
        pass
    except Exception as e:
        rospy.logerr(f"Benchmark failed: {e}")
        import traceback
        traceback.print_exc()
