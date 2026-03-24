cmake_minimum_required(VERSION 3.0.2)
project(safe_regret_mpc)

## Find catkin and required components
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
  visualization_msgs
  tf
  message_generation
  pluginlib
)

## Find system dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(Eigen3 REQUIRED)
find_package(PkgConfig REQUIRED)

## Find CppAD
pkg_check_modules(CPPAD REQUIRED cppad)

## Find Ipopt
pkg_check_modules(IPOPT REQUIRED ipopt)

## Python setup (not needed - C++ only package)
# catkin_python_setup()  # Removed: no Python files in this package

## Generate messages
add_message_files(
  FILES
  SafeRegretState.msg
  SafeRegretMetrics.msg
  STLRobustness.msg
  DRMargins.msg
  TerminalSet.msg
)

add_service_files(
  FILES
  SetSTLSpecification.srv
  GetSystemStatus.srv
  ResetRegretTracker.srv
)

generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
  nav_msgs
)

## Catkin package configuration
catkin_package(
  INCLUDE_DIRS include
  LIBRARIES safe_regret_mpc_lib
  CATKIN_DEPENDS
    roscpp
    rospy
    std_msgs
    geometry_msgs
    nav_msgs
    sensor_msgs
    visualization_msgs
    tf
    message_runtime
    pluginlib
  DEPENDS
    CPPAD
    IPOPT
)

## Include directories
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
  ${CPPAD_INCLUDE_DIRS}
  ${IPOPT_INCLUDE_DIRS}
  include
  ${catkin_DEVEL_PREFIX}/include
)

## Link directories
link_directories(
  ${CPPAD_LIBRARY_DIRS}
  ${IPOPT_LIBRARY_DIRS}
)

## Build Safe-Regret MPC library
add_library(${PROJECT_NAME}_lib
  src/SafeRegretMPC.cpp
  src/SafeRegretMPCSolver.cpp
  src/STLConstraintIntegrator.cpp
  src/DRConstraintIntegrator.cpp
  src/TerminalSetIntegrator.cpp
  src/RegretTracker.cpp
  src/PerformanceMonitor.cpp
)

target_link_libraries(${PROJECT_NAME}_lib
  ${catkin_LIBRARIES}
  ${CPPAD_LIBRARIES}
  ${IPOPT_LIBRARIES}
)

add_dependencies(${PROJECT_NAME}_lib
  ${catkin_EXPORTED_TARGETS}
  ${PROJECT_NAME}_generate_messages_cpp
)

## Build Safe-Regret MPC node
add_executable(safe_regret_mpc_node
  src/safe_regret_mpc_node.cpp
)

target_link_libraries(safe_regret_mpc_node
  ${PROJECT_NAME}_lib
  ${catkin_LIBRARIES}
)

add_dependencies(safe_regret_mpc_node
  ${PROJECT_NAME}_generate_messages_cpp
)

## Install targets
install(TARGETS ${PROJECT_NAME}_lib safe_regret_mpc_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

## Install launch files
install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch
  FILES_MATCHING PATTERN "*.launch"
)

## Install params files
install(DIRECTORY params/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/params
  FILES_MATCHING PATTERN "*.yaml"
)

## Install rviz files
install(DIRECTORY rviz/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/rviz
  FILES_MATCHING PATTERN "*.rviz"
)

## Testing (disabled for now - no test files)
# if(CATKIN_ENABLE_TESTING)
#   find_package(rostest REQUIRED)
#   # Add tests here when ready
# endif()

## Print configuration
message(STATUS "Safe-Regret MPC Configuration:")
message(STATUS "  Eigen3: ${EIGEN3_INCLUDE_DIRS}")
message(STATUS "  CppAD: ${CPPAD_INCLUDE_DIRS}")
message(STATUS "  Ipopt: ${IPOPT_INCLUDE_DIRS}")
