#!/bin/bash

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║       Tube MPC Simulator - Movement Test                  ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Start simulator in background
echo "→ Starting kinematic simulator..."
roslaunch tube_mpc_benchmark simple_test.launch test_duration:=10 > /tmp/sim_test.log 2>&1 &
SIM_PID=$!
sleep 3

# Activate test
echo "→ Activating test..."
python3 <<PYTHON_SCRIPT
import rospy
from std_msgs.msg import Float64MultiArray
rospy.init_node('activator', anonymous=True)
pub = rospy.Publisher('/test_control', Float64MultiArray, queue_size=10)
rospy.sleep(1.0)
msg = Float64MultiArray()
msg.data = [1.0]
pub.publish(msg)
print("✓ Test activated")
rospy.sleep(1.0)
PYTHON_SCRIPT

sleep 2

# Publish velocity commands
echo "→ Publishing velocity commands (v=0.3 m/s, 5 seconds)..."
python3 <<PYTHON_SCRIPT
import rospy
from geometry_msgs.msg import Twist
rospy.init_node('vel_cmd', anonymous=True)
pub = rospy.Publisher('/cmd_vel', Twist, queue_size=10)
rospy.sleep(1.0)
for i in range(50):  # 5 seconds at 10 Hz
    msg = Twist()
    msg.linear.x = 0.3
    msg.angular.z = 0.0
    pub.publish(msg)
    rospy.sleep(0.1)
print("✓ Velocity commands sent")
PYTHON_SCRIPT

# Wait for robot to move
echo "→ Waiting for robot to move..."
sleep 3

# Check odometry
echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                  Odometry Results                          ║"
echo "╚════════════════════════════════════════════════════════════╝"
timeout 2 rostopic echo /odom -n 1 2>/dev/null | grep -A 3 "position:" || echo "No odom data"

# Cleanup
kill -9 $SIM_PID 2>/dev/null
pkill -9 -f simple_test 2>/dev/null
pkill -9 -f kinematic_robot_sim 2>/dev/null

echo ""
echo "✓ Test completed"
echo ""
echo "Expected: Robot should have moved forward by ~1.5m"
echo "(0.3 m/s × 5 seconds = 1.5 meters)"
