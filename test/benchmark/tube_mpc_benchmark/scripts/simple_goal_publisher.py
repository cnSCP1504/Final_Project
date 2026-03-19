#!/usr/bin/env python3
"""
Simple goal publisher for quick testing
"""

import rospy
import math
import sys
from move_base_msgs.msg import MoveBaseAction, MoveBaseGoal
import actionlib

def publish_goal(x, y, theta=0.0):
    """Publish navigation goal"""
    rospy.init_node('simple_goal_publisher', anonymous=True)

    # Create action client
    client = actionlib.SimpleActionClient('move_base', MoveBaseAction)
    rospy.loginfo("Waiting for move_base server...")
    client.wait_for_server()

    # Create goal
    goal = MoveBaseGoal()
    goal.target_pose.header.stamp = rospy.Time.now()
    goal.target_pose.header.frame_id = "map"
    goal.target_pose.pose.position.x = x
    goal.target_pose.pose.position.y = y
    goal.target_pose.pose.position.z = 0.0
    goal.target_pose.pose.orientation.x = 0.0
    goal.target_pose.pose.orientation.y = 0.0
    goal.target_pose.pose.orientation.z = math.sin(theta/2)
    goal.target_pose.pose.orientation.w = math.cos(theta/2)

    rospy.loginfo(f"Sending goal: ({x:.2f}, {y:.2f}, {theta:.2f})")

    # Send goal
    client.send_goal(goal)

    # Wait for result
    finished = client.wait_for_result(rospy.Duration(60.0))

    if finished:
        state = client.get_state()
        if state == actionlib.GoalStatus.SUCCEEDED:
            rospy.loginfo("✓ Goal reached!")
            return True
        else:
            rospy.logwarn(f"✗ Goal failed with state: {state}")
            return False
    else:
        rospy.logwarn("✗ Goal timeout")
        return False

if __name__ == '__main__':
    if len(sys.argv) >= 3:
        x = float(sys.argv[1])
        y = float(sys.argv[2])
        theta = float(sys.argv[3]) if len(sys.argv) >= 4 else 0.0
        publish_goal(x, y, theta)
    else:
        print("Usage: simple_goal_publisher.py <x> <y> [theta]")
