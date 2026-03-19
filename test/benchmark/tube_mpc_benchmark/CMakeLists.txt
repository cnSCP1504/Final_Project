cmake_minimum_required(VERSION 3.0.2)
project(tube_mpc_benchmark)

## Find catkin and required components
find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
  move_base_msgs
  actionlib
  map_server
  amcl
  move_base
)

## Python support for analysis scripts
find_package(catkin REQUIRED COMPONENTS rospy)

###################################
## Catkin specific configuration ##
###################################
catkin_package(
  CATKIN_DEPENDS
    rospy
    std_msgs
    geometry_msgs
    nav_msgs
    sensor_msgs
    tf2
    tf2_ros
    tf2_geometry_msgs
    move_base_msgs
    actionlib
)

###########
## Build ##
###########

# This is a Python-only package, no C++ compilation needed

# catkin_python_setup() was deprecated, use CATKIN_SKIP_INSTALL_SPACE_FOR_PYTHON

#############
## Install ##
#############

## Install Python scripts
install(PROGRAMS
  scripts/kinematic_robot_sim.py
  scripts/benchmark_runner.py
  scripts/simple_goal_publisher.py
  scripts/generate_empty_map.py
  scripts/run_benchmark.sh
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Install launch files
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  FILES_MATCHING PATTERN "*.launch"
)

## Install config files
install(DIRECTORY configs/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/configs
  FILES_MATCHING PATTERN "*.yaml" PATTERN "*.pgm"
)

## Install launch files
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  FILES_MATCHING PATTERN "*.launch"
)

## Install config files
install(DIRECTORY configs/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/configs
  FILES_MATCHING PATTERN "*.yaml"
)

## Install Python scripts
install(DIRECTORY scripts/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/scripts
  USE_SOURCE_PERMISSIONS
)

#############
## Testing ##
#############

# Python-based testing, no C++ tests needed
