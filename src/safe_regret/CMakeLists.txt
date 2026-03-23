cmake_minimum_required(VERSION 3.0.2)
project(safe_regret)

# Find dependencies
find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  geometry_msgs
  nav_msgs
  tf2_ros
  tf2
)

# OMPL dependency (optional)
find_package(ompl QUIET)
find_package(ompl QUIET COMPONENTS geometric)

if(ompl_FOUND)
  message(STATUS "OMPL found - building with OMPL support")
  add_definitions(-DHAVE_OMPL)
else()
  message(STATUS "OMPL not found - building without OMPL support (fallback implementations)")
endif()

find_package(Eigen3 REQUIRED)

# Catkin package configuration
catkin_package(
  INCLUDE_DIRS include ${EIGEN3_INCLUDE_DIRS}
  LIBRARIES safe_regret_core
  CATKIN_DEPENDS roscpp std_msgs geometry_msgs nav_msgs
)

# ============================================================================
# Library
# ============================================================================

include_directories(
  include
  ${EIGEN3_INCLUDE_DIRS}
  ${catkin_INCLUDE_DIRS}
)

# Build library with or without OMPL support
if(ompl_FOUND)
  add_library(${PROJECT_NAME}_core
    src/RegretMetrics.cpp
    src/RegretAnalyzer.cpp
    src/ReferencePlanner.cpp
    src/TopologicalAbstraction.cpp
    src/OMPLPathPlanner.cpp
    src/BeliefSpaceOptimizer.cpp
    src/ActiveInformation.cpp
  )

  target_link_libraries(${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
    ${ompl_LIBRARIES}
    ${ompl_geometric_LIBRARIES}
  )

  include_directories(${ompl_INCLUDE_DIRS})
  message(STATUS "Building with OMPL support")
else()
  add_library(${PROJECT_NAME}_core
    src/RegretMetrics.cpp
    src/RegretAnalyzer.cpp
    src/ReferencePlanner.cpp
    src/TopologicalAbstraction.cpp
    src/OMPLPathPlannerFallback.cpp
    src/BeliefSpaceOptimizer.cpp
    src/ActiveInformation.cpp
  )

  target_link_libraries(${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )
  message(STATUS "Building with fallback geometric planning (no OMPL)")
endif()

add_dependencies(${PROJECT_NAME}_core ${catkin_EXPORTED_TARGETS})

# ============================================================================
# Tests
# ============================================================================

if(CATKIN_ENABLE_TESTING)
  find_package(rostest REQUIRED)

  # Core regret computation tests
  add_executable(test_regret_core test/test_regret_core.cpp)

  target_link_libraries(test_regret_core
    ${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )

  add_dependencies(test_regret_core ${catkin_EXPORTED_TARGETS})

  # Add tests to CTest
  add_test(NAME test_regret_core COMMAND test_regret_core)

  # Topological abstraction tests
  add_executable(test_topological_abstraction test/test_topological_abstraction.cpp)

  target_link_libraries(test_topological_abstraction
    ${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )

  add_dependencies(test_topological_abstraction ${catkin_EXPORTED_TARGETS})

  add_test(NAME test_topological_abstraction COMMAND test_topological_abstraction)

  # OMPL integration tests (only if OMPL is available)
  if(ompl_FOUND)
    add_executable(test_ompl_planner test/test_ompl_planner.cpp)

    target_link_libraries(test_ompl_planner
      ${PROJECT_NAME}_core
      ${catkin_LIBRARIES}
    )

    add_dependencies(test_ompl_planner ${catkin_EXPORTED_TARGETS})

    add_test(NAME test_ompl_planner COMMAND test_ompl_planner)
  endif()

  # OMPL integration with ReferencePlanner tests
  add_executable(test_ompl_integration test/test_ompl_integration.cpp)

  target_link_libraries(test_ompl_integration
    ${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )

  add_dependencies(test_ompl_integration ${catkin_EXPORTED_TARGETS})

  add_test(NAME test_ompl_integration COMMAND test_ompl_integration)

  # Belief-space optimizer tests
  add_executable(test_belief_optimizer test/test_belief_optimizer.cpp)

  target_link_libraries(test_belief_optimizer
    ${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )

  add_dependencies(test_belief_optimizer ${catkin_EXPORTED_TARGETS})

  add_test(NAME test_belief_optimizer COMMAND test_belief_optimizer)

  # Active information acquisition tests
  add_executable(test_active_information test/test_active_information.cpp)

  target_link_libraries(test_active_information
    ${PROJECT_NAME}_core
    ${catkin_LIBRARIES}
  )

  add_dependencies(test_active_information ${catkin_EXPORTED_TARGETS})

  add_test(NAME test_active_information COMMAND test_active_information)
endif()

# ============================================================================
# ROS Node
# ============================================================================

add_executable(safe_regret_node src/safe_regret_node_main.cpp src/SafeRegretNode.cpp)

target_link_libraries(safe_regret_node
  ${PROJECT_NAME}_core
  ${catkin_LIBRARIES}
)

add_dependencies(safe_regret_node ${catkin_EXPORTED_TARGETS})

# ============================================================================
# Install
# ============================================================================

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)

install(DIRECTORY params launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
