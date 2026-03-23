/**
 * @file safe_regret_node_main.cpp
 * @brief Main entry point for Safe-Regret MPC ROS node
 */

#include "safe_regret/SafeRegretNode.hpp"
#include <signal.h>
#include <ros/ros.h>
#include <memory>

namespace safe_regret {

std::unique_ptr<SafeRegretNode> g_node;

void signalHandler(int signum) {
  ROS_INFO("Interrupt signal (%d) received, shutting down...", signum);
  if (g_node) {
    g_node.reset();
  }
  ros::shutdown();
}

} // namespace safe_regret

int main(int argc, char** argv) {
  ros::init(argc, argv, "safe_regret_node", ros::init_options::NoSigintHandler);

  // Register signal handler
  signal(SIGINT, safe_regret::signalHandler);
  signal(SIGTERM, safe_regret::signalHandler);

  ros::NodeHandle nh;
  ros::NodeHandle private_nh("~");

  try {
    // Create node
    safe_regret::g_node = std::make_unique<safe_regret::SafeRegretNode>(nh, private_nh);

    // Initialize
    if (!safe_regret::g_node->initialize()) {
      ROS_ERROR("Failed to initialize Safe-Regret node");
      return 1;
    }

    // Spin
    safe_regret::g_node->spin();

    ROS_INFO("Safe-Regret node terminated successfully");
    return 0;

  } catch (const std::exception& e) {
    ROS_ERROR("Exception in Safe-Regret node: %s", e.what());
    return 1;
  }
}
