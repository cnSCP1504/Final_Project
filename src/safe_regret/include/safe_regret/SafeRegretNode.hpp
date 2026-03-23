/**
 * @file SafeRegretNode.hpp
 * @brief ROS node for Safe-Regret MPC reference planner
 *
 * This node integrates Phase 1 (Tube MPC), Phase 2 (STL Monitor), and Phase 3 (DR Tightening)
 * to generate reference trajectories with no-regret guarantees.
 *
 * Subscribes to:
 *   - /tube_mpc/tracking_error (from Phase 1)
 *   - /mpc_trajectory (from Phase 1)
 *   - /stl_robustness (from Phase 2)
 *   - /stl_budget (from Phase 2)
 *   - /dr_margins (from Phase 3)
 *   - /odom (robot odometry)
 *
 * Publishes to:
 *   - /safe_regret/reference_trajectory (reference for Tube MPC)
 *   - /safe_regret/regret_metrics (monitoring)
 *   - /safe_regret/feasibility_status (feasibility checking)
 */

#pragma once

#include <ros/ros.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Float64.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>

#include "safe_regret/RegretAnalyzer.hpp"
#include "safe_regret/ReferencePlanner.hpp"
#include "safe_regret/RegretMetrics.hpp"

#include <Eigen/Dense>
#include <memory>
#include <fstream>

namespace safe_regret {

/**
 * @brief Node parameters
 */
struct SafeRegretNodeParams {
  // Regret analysis parameters
  double lipschitz_constant;
  Eigen::MatrixXd lqr_gain;
  double infeasibility_penalty;
  double tracking_error_constant;

  // Reference planner parameters
  std::string no_regret_algorithm;
  double learning_rate;
  int planning_horizon;
  double update_frequency;

  // Feasibility constraints (Theorem 4.8)
  double max_velocity;
  double max_curvature;
  double tube_radius;

  // STL parameters
  std::string stl_formula_type;
  double stl_time_horizon;
  double robustness_weight;
  double robustness_budget_baseline;

  // Logging
  bool enable_regret_logging;
  std::string regret_log_file;

  // Topics
  std::string reference_trajectory_topic;
  std::string regret_metrics_topic;
  std::string feasibility_status_topic;

  SafeRegretNodeParams()
    : lipschitz_constant(1.0),
      infeasibility_penalty(1000.0),
      tracking_error_constant(1.0),
      no_regret_algorithm("omd"),
      learning_rate(0.1),
      planning_horizon(20),
      update_frequency(10.0),
      max_velocity(2.0),
      max_curvature(1.0),
      tube_radius(0.5),
      stl_formula_type("reachability"),
      stl_time_horizon(10.0),
      robustness_weight(10.0),
      robustness_budget_baseline(0.1),
      enable_regret_logging(true),
      regret_log_file("/tmp/safe_regret_data.csv"),
      reference_trajectory_topic("/safe_regret/reference_trajectory"),
      regret_metrics_topic("/safe_regret/regret_metrics"),
      feasibility_status_topic("/safe_regret/feasibility_status") {}
};

/**
 * @brief Main ROS node for Safe-Regret MPC
 *
 * Integrates all phases and provides reference trajectory with regret guarantees
 */
class SafeRegretNode {
public:
  /**
   * @brief Constructor
   * @param nh Node handle
   * @param private_nh Private node handle for parameters
   */
  SafeRegretNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh);

  /**
   * @brief Destructor
   */
  ~SafeRegretNode();

  /**
   * @brief Initialize the node
   * @return True if successful
   */
  bool initialize();

  /**
   * @brief Spin the node (main loop)
   */
  void spin();

  /**
   * @brief Get node health status
   */
  bool isHealthy() const;

private:
  // ROS interfaces
  ros::NodeHandle nh_;
  ros::NodeHandle private_nh_;

  // Subscribers (from Phase 1-3)
  ros::Subscriber sub_tracking_error_;     // From Phase 1 (Tube MPC)
  ros::Subscriber sub_mpc_trajectory_;     // From Phase 1
  ros::Subscriber sub_stl_robustness_;     // From Phase 2 (STL Monitor)
  ros::Subscriber sub_stl_budget_;         // From Phase 2
  ros::Subscriber sub_dr_margins_;         // From Phase 3 (DR Tightening)
  ros::Subscriber sub_odom_;               // Robot odometry

  // Publishers (to Phase 1-3 and monitoring)
  ros::Publisher pub_reference_trajectory_; // Reference to Tube MPC
  ros::Publisher pub_regret_metrics_;      // Monitoring
  ros::Publisher pub_feasibility_status_;  // Feasibility checking

  // Timers
  ros::Timer planning_timer_;

  // Core components
  SafeRegretNodeParams params_;
  std::unique_ptr<RegretAnalyzer> regret_analyzer_;
  std::unique_ptr<ReferencePlanner> reference_planner_;
  std::unique_ptr<FeasibilityChecker> feasibility_checker_;

  // State
  int current_step_;
  BeliefState current_belief_;
  STLSpecification current_stl_spec_;
  ReferenceTrajectory current_reference_;
  RegretMetrics current_metrics_;

  // Data from Phase 1-3
  Eigen::VectorXd latest_tracking_error_;
  nav_msgs::Path latest_mpc_trajectory_;
  double latest_stl_robustness_;
  double latest_stl_budget_;
  std::vector<double> latest_dr_margins_;
  nav_msgs::Odometry latest_odom_;

  // Flags
  bool received_tracking_error_;
  bool received_mpc_trajectory_;
  bool received_stl_data_;
  bool received_dr_margins_;
  bool received_odom_;

  // Logging
  std::ofstream log_file_;

  // Callbacks
  void trackingErrorCallback(const std_msgs::Float64MultiArray::ConstPtr& msg);
  void mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg);
  void stlRobustnessCallback(const std_msgs::Float64::ConstPtr& msg);
  void stlBudgetCallback(const std_msgs::Float64::ConstPtr& msg);
  void drMarginsCallback(const std_msgs::Float64MultiArray::ConstPtr& msg);
  void odomCallback(const nav_msgs::Odometry::ConstPtr& msg);

  // Timer callback
  void planningTimerCallback(const ros::TimerEvent& event);

  // Core methods
  bool loadParameters();
  void setupROSConnections();
  void initializeCoreComponents();
  void updateBeliefState();
  void updateSTLSpecification();
  ReferenceTrajectory generateReferenceTrajectory();
  void updateRegretAnalysis();
  void checkFeasibilityAndPublish();
  void publishReferenceTrajectory(const ReferenceTrajectory& reference);
  void publishRegretMetrics(const RegretMetrics& metrics);
  void publishFeasibilityStatus(bool is_feasible, const std::string& message);
  void logRegretData(const RegretMetrics& metrics);

  // Helper methods
  Eigen::VectorXd poseToVector(const geometry_msgs::Pose& pose) const;
  geometry_msgs::Pose vectorToPose(const Eigen::VectorXd& vec) const;
  nav_msgs::Path referenceToPathMsg(const ReferenceTrajectory& reference) const;
  std::vector<double> computeNominalInputs(const ReferenceTrajectory& reference) const;
};

} // namespace safe_regret
