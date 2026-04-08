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

## Build C++ library for STL monitoring
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
  include
)

add_library(${PROJECT_NAME}_lib
  src/BeliefSpaceEvaluator.cpp
  src/SmoothRobustness.cpp
  src/STLParser.cpp
  src/RobustnessBudget.cpp
)

add_dependencies(${PROJECT_NAME}_lib
  ${catkin_EXPORTED_TARGETS}
)

target_link_libraries(${PROJECT_NAME}_lib
  ${catkin_LIBRARIES}
)

## Build C++ ROS node
add_executable(stl_monitor_node_cpp src/stl_monitor_node.cpp)
target_link_libraries(stl_monitor_node_cpp
  ${PROJECT_NAME}_lib
  ${catkin_LIBRARIES}
)

## Install Python scripts (keep for backward compatibility)
catkin_install_python(PROGRAMS
  src/stl_monitor_node.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Install C++ node
install(TARGETS stl_monitor_node_cpp
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
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
