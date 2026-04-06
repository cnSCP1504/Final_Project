cmake_minimum_required(VERSION 3.5)
project(tube_mpc_ros)

## Compile as C++11, supported in ROS Kinetic and newer
add_definitions(-std=c++11 -O3)

# options for build configuration
option(BUILD_EXAMPLE "Whether or not building the CppAD & Ipopt example" OFF) 

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  costmap_2d
  dynamic_reconfigure
  geometry_msgs
  nav_msgs
  #ackermann_msgs
  pluginlib
  message_generation
  move_base
  base_local_planner
  roscpp
  rospy
  std_msgs
  tf
  visualization_msgs
)

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories( ${catkin_INCLUDE_DIRS} include )
include_directories(/usr/include)
link_directories(/usr/lib)


# dynamic reconfigure
generate_dynamic_reconfigure_options(
    cfg/MPCPlanner.cfg
)


catkin_package(
   INCLUDE_DIRS include
   LIBRARIES tube_mpc_lib
   CATKIN_DEPENDS costmap_2d dynamic_reconfigure geometry_msgs move_base roscpp rospy std_msgs tf visualization_msgs pluginlib
)

###########
## Build ##
###########

# General MPC_Node 
ADD_EXECUTABLE( tube_MPC_Node src/MPC.cpp src/MPC_Node.cpp )
TARGET_LINK_LIBRARIES(tube_MPC_Node ipopt ${catkin_LIBRARIES} )

# Total navigation with MPC_Node
ADD_EXECUTABLE( tube_nav_mpc src/navMPC.cpp src/navMPCNode.cpp )
TARGET_LINK_LIBRARIES(tube_nav_mpc ipopt ${catkin_LIBRARIES} )

# Tube MPC Node
ADD_EXECUTABLE( tube_TubeMPC_Node src/TubeMPC.cpp src/TubeMPCNode.cpp src/MetricsCollector.cpp )
TARGET_LINK_LIBRARIES(tube_TubeMPC_Node ipopt ${catkin_LIBRARIES} )

# Local planner with MPC_Node for tracking
ADD_EXECUTABLE( tube_tracking_reference_trajectory src/trackRefTraj.cpp src/trackRefTrajNode.cpp )
TARGET_LINK_LIBRARIES(tube_tracking_reference_trajectory ipopt ${catkin_LIBRARIES} )

# Pure Pursuit Node
add_executable(tube_Pure_Pursuit src/Pure_Pursuit.cpp)
target_link_libraries(tube_Pure_Pursuit ${catkin_LIBRARIES})

# Own Global planner for tracking desired trajectory
add_library(tube_global_planner_lib src/global_planner/global_planner.cpp)
TARGET_LINK_LIBRARIES(tube_global_planner_lib ipopt ${catkin_LIBRARIES} )

# MPC Local planner plugin
add_library(tube_mpc_lib src/mpc_plannner_ros.cpp src/mpc_plannner.cpp src/TubeMPCMetricsCollector.cpp)
TARGET_LINK_LIBRARIES(tube_mpc_lib ipopt ${catkin_LIBRARIES} )

#############
## Example ##
#############
if(BUILD_EXAMPLE)
    ADD_EXECUTABLE( CppAD_started example/CppAD_started.cpp )
    TARGET_LINK_LIBRARIES(CppAD_started)
    
    ADD_EXECUTABLE( CppAD_Ipopt example/CppAD_Ipopt.cpp )
    TARGET_LINK_LIBRARIES(CppAD_Ipopt ipopt)
endif(BUILD_EXAMPLE)
