cmake_minimum_required(VERSION 3.0.2)
project(dr_tightening)

## Find catkin and required components
find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  geometry_msgs
  nav_msgs
  cmake_modules
)

## Find Eigen3 (standalone, not from catkin)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

## catkin package configuration
catkin_package(
  INCLUDE_DIRS include ${EIGEN3_INCLUDE_DIRS}
  LIBRARIES dr_tightening
  CATKIN_DEPENDS roscpp std_msgs geometry_msgs nav_msgs
)

## Include directories
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
  include
)

## Library
add_library(${PROJECT_NAME}
  src/ResidualCollector.cpp
  src/AmbiguityCalibrator.cpp
  src/TighteningComputer.cpp
  src/SafetyLinearization.cpp
  src/TerminalSetCalculator.cpp
)

target_link_libraries(${PROJECT_NAME}
  ${catkin_LIBRARIES}
)

## ROS Node
add_executable(dr_tightening_node
  src/dr_tightening_node_main.cpp
  src/dr_tightening_node.cpp
)

target_link_libraries(dr_tightening_node
  ${PROJECT_NAME}
  ${catkin_LIBRARIES}
  ${catkin_LIBRARIES}
)

## Add tf2 dependency for node
find_package(tf2 REQUIRED)

target_include_directories(dr_tightening_node PRIVATE
  ${tf2_INCLUDE_DIRS}
)

target_link_libraries(dr_tightening_node
  ${tf2_LIBRARIES}
)

## Test executables
add_executable(dr_tightening_test
  test/test_dr_formulas.cpp
)

target_link_libraries(dr_tightening_test
  ${PROJECT_NAME}
  ${catkin_LIBRARIES}
)

add_executable(dr_tightening_comprehensive_test
  test/test_comprehensive.cpp
)

target_link_libraries(dr_tightening_comprehensive_test
  ${PROJECT_NAME}
  ${catkin_LIBRARIES}
)

add_executable(test_dimension_mismatch
  test/test_dimension_mismatch.cpp
)

target_link_libraries(test_dimension_mismatch
  ${PROJECT_NAME}
  ${catkin_LIBRARIES}
)

add_executable(test_stress
  test/test_stress.cpp
)

target_link_libraries(test_stress
  ${PROJECT_NAME}
  ${catkin_LIBRARIES}
)

## Install targets
install(TARGETS ${PROJECT_NAME} dr_tightening_test dr_tightening_comprehensive_test test_dimension_mismatch test_stress
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)

install(DIRECTORY params launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
