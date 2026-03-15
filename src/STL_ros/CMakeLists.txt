cmake_minimum_required(VERSION 3.0.2)
project(stl_ros)

## Find catkin and required components
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
  message_generation
  message_runtime
  dynamic_reconfigure
)

## Python setup
catkin_python_setup()

## Generate messages
add_message_files(
  FILES
  STLRobustness.msg
  STLBudget.msg
  STLFormula.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
  nav_msgs
)

## Generate dynamic reconfigure configurations
generate_dynamic_reconfigure_options(
  cfg/STLMonitorParams.cfg
)

## Catkin package configuration
catkin_package(
  CATKIN_DEPENDS
    rospy
    std_msgs
    geometry_msgs
    nav_msgs
    sensor_msgs
    message_runtime
    dynamic_reconfigure
)

## Install Python scripts
catkin_install_python(PROGRAMS
  scripts/stl_monitor.py
  scripts/stl_visualizer.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Install launch files
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  FILES_MATCHING PATTERN "*.launch"
)

## Install parameter files
install(DIRECTORY params/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/params
  FILES_MATCHING PATTERN "*.yaml" PATTERN "*.txt"
)

## Install headers (for future C++ implementation)
install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)
