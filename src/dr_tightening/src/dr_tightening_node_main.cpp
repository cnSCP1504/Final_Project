/**
 * @file dr_tightening_node_main.cpp
 * @brief Main entry point for DR tightening ROS node
 */

#include "dr_tightening/dr_tightening_node.hpp"
#include <signal.h>
#include <memory>

using namespace dr_tightening;

std::unique_ptr<DRTighteningNode> g_node;

void signalHandler(int signum) {
  ROS_INFO("Shutting down DR Tightening Node");
  if (g_node) {
    // Node will be destroyed automatically
  }
  ros::shutdown();
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "dr_tightening_node");

  // Setup signal handler for graceful shutdown
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  ros::NodeHandle nh;
  ros::NodeHandle private_nh("~");

  // Create node
  g_node = std::make_unique<DRTighteningNode>(nh, private_nh);

  // Initialize
  if (!g_node->initialize()) {
    ROS_ERROR("Failed to initialize DR Tightening Node");
    return 1;
  }

  ROS_INFO("DR Tightening Node starting...");

  // Spin
  g_node->spin();

  return 0;
}
