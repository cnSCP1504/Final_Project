#!/usr/bin/env python3
"""Activate the simulator test"""
import rospy
from std_msgs.msg import Float64MultiArray

rospy.init_node('test_activator', anonymous=True)
pub = rospy.Publisher('/test_control', Float64MultiArray, queue_size=10)

# Wait for simulator to be ready
rospy.sleep(2.0)

# Send start command (1.0 = start)
msg = Float64MultiArray()
msg.data = [1.0]  # Start test
pub.publish(msg)

print("✓ Test activated")
rospy.sleep(1.0)
