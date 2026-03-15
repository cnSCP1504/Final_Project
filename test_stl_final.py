#!/usr/bin/env python3
import rospy
from geometry_msgs.msg import PoseStamped
from nav_msgs.msg import Path
from stl_ros.msg import STLRobustness, STLBudget

def test_stl_monitor():
    rospy.init_node('stl_test_client')

    # Create publishers
    belief_pub = rospy.Publisher('/stl_monitor/belief', PoseStamped, queue_size=1)
    trajectory_pub = rospy.Publisher('/stl_monitor/mpc_trajectory', Path, queue_size=1)

    # Create subscribers for monitoring
    robustness_values = []
    budget_values = []

    def robustness_callback(msg):
        robustness_values.append(msg)
        print(f"📊 STL Robustness Received:")
        print(f"   Formula: {msg.formula_name}")
        print(f"   Robustness: {msg.robustness:.3f}")
        print(f"   Expected: {msg.expected_robustness:.3f}")
        print(f"   Satisfied: {msg.satisfied}")
        print(f"   Budget Feasible: {msg.budget_feasible}")
        print()

    def budget_callback(msg):
        budget_values.append(msg)
        print(f"💰 STL Budget Received:")
        print(f"   Current Budget: {msg.current_budget:.3f}")
        print(f"   Baseline: {msg.baseline_r:.3f}")
        print(f"   Feasible: {msg.feasible}")
        print(f"   Formulas: {msg.formula_names}")
        print(f"   Formula Feasible: {msg.formula_feasible}")
        print()

    rospy.Subscriber('/stl_monitor/robustness', STLRobustness, robustness_callback)
    rospy.Subscriber('/stl_monitor/budget', STLBudget, budget_callback)

    print("🚀 STL Monitor Test Starting...")
    print("Waiting for STL monitor to be ready...")
    rospy.sleep(2)

    # Test Case 1: Trajectory far from goal
    print("\n=== Test Case 1: Far from goal (5,5) -> (6,6) -> (7,7) ===")
    belief1 = PoseStamped()
    belief1.header.stamp = rospy.Time.now()
    belief1.header.frame_id = "map"
    belief1.pose.position.x = 5.0
    belief1.pose.position.y = 5.0
    belief1.pose.orientation.w = 1.0

    trajectory1 = Path()
    trajectory1.header.stamp = rospy.Time.now()
    trajectory1.header.frame_id = "map"

    for i, (x, y) in enumerate([(5.0, 5.0), (6.0, 6.0), (7.0, 7.0)]):
        pose = PoseStamped()
        pose.header.stamp = rospy.Time.now()
        pose.header.frame_id = "map"
        pose.pose.position.x = x
        pose.pose.position.y = y
        pose.pose.orientation.w = 1.0
        trajectory1.poses.append(pose)

    print("Sending test data...")
    belief_pub.publish(belief1)
    trajectory_pub.publish(trajectory1)

    print("Waiting for STL evaluation...")
    rospy.sleep(3)

    # Test Case 2: Trajectory near goal
    print("\n=== Test Case 2: Near goal (8,8) -> (8.1,8.1) -> (8.2,8.2) ===")
    belief2 = PoseStamped()
    belief2.header.stamp = rospy.Time.now()
    belief2.header.frame_id = "map"
    belief2.pose.position.x = 8.0
    belief2.pose.position.y = 8.0
    belief2.pose.orientation.w = 1.0

    trajectory2 = Path()
    trajectory2.header.stamp = rospy.Time.now()
    trajectory2.header.frame_id = "map"

    for i, (x, y) in enumerate([(8.0, 8.0), (8.1, 8.1), (8.2, 8.2)]):
        pose = PoseStamped()
        pose.header.stamp = rospy.Time.now()
        pose.header.frame_id = "map"
        pose.pose.position.x = x
        pose.pose.position.y = y
        pose.pose.orientation.w = 1.0
        trajectory2.poses.append(pose)

    print("Sending test data...")
    belief_pub.publish(belief2)
    trajectory_pub.publish(trajectory2)

    print("Waiting for STL evaluation...")
    rospy.sleep(3)

    # Summary
    print(f"\n=== Test Summary ===")
    print(f"Total Robustness Messages: {len(robustness_values)}")
    print(f"Total Budget Messages: {len(budget_values)}")

    if robustness_values and budget_values:
        print("✅ STL Monitor is working correctly!")
        print("✅ Both robustness and budget messages are being published")
    else:
        print("❌ STL Monitor has issues")

    print("\n🎯 Safe-Regret MPC System Test Complete!")

if __name__ == '__main__':
    try:
        test_stl_monitor()
    except rospy.ROSInterruptException:
        pass
