cmake_minimum_required(VERSION 3.0.2)
project(stl_monitor)

## Find catkin and required components
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
  tf
  message_generation
)

## Find Eigen3
find_package(Eigen3 REQUIRED)

## Generate messages
add_message_files(
  FILES
  STLFormula.msg
  STLRobustness.msg
  STLBudget.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
)

## Catkin package configuration
catkin_package(
  CATKIN_DEPENDS roscpp rospy std_msgs geometry_msgs nav_msgs sensor_msgs tf message_runtime
)

## Install Python scripts
catkin_install_python(PROGRAMS
  src/stl_monitor_node.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Install launch files and params
install(DIRECTORY launch params cfg
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
  USE_SOURCE_PERMISSIONS
)

## Install test scripts
install(DIRECTORY test
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/test
  USE_SOURCE_PERMISSIONS
)

## Install scripts
install(DIRECTORY scripts
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/scripts
  USE_SOURCE_PERMISSIONS
)
