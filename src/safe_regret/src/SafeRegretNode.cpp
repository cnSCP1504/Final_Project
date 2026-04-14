/**
 * @file SafeRegretNode.cpp
 * @brief Implementation of Safe-Regret MPC ROS node
 */

#include "safe_regret/SafeRegretNode.hpp"
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace safe_regret {

// ============================================================================
// Constructor and Destructor
// ============================================================================

SafeRegretNode::SafeRegretNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh)
  : nh_(nh),
    private_nh_(private_nh),
    current_step_(0),
    latest_tracking_error_(3),
    latest_stl_robustness_(0.0),
    latest_stl_budget_(0.0),
    received_tracking_error_(false),
    received_mpc_trajectory_(false),
    received_stl_data_(false),
    received_dr_margins_(false),
    received_odom_(false),
    received_goal_(false) {

  // Initialize belief state (3D: x, y, theta)
  current_belief_.mean = Eigen::VectorXd::Zero(3);
  current_belief_.covariance = 0.1 * Eigen::MatrixXd::Identity(3, 3);
}

SafeRegretNode::~SafeRegretNode() {
  if (log_file_.is_open()) {
    log_file_.close();
  }
}

// ============================================================================
// Initialization
// ============================================================================

bool SafeRegretNode::initialize() {
  ROS_INFO("==============================================");
  ROS_INFO("Safe-Regret MPC Node - Phase 4 Integration");
  ROS_INFO("==============================================");

  // Load parameters
  if (!loadParameters()) {
    ROS_ERROR("Failed to load parameters");
    return false;
  }

  // Setup ROS connections
  setupROSConnections();

  // Initialize core components
  initializeCoreComponents();

  // Open log file if enabled
  if (params_.enable_regret_logging) {
    log_file_.open(params_.regret_log_file);
    if (log_file_.is_open()) {
      // Write CSV header
      log_file_ << "step,safe_regret,dynamic_regret,tracking_contribution,"
                << "nominal_contribution,tightening_contribution,sublinear_check\n";
      ROS_INFO("Regret logging enabled: %s", params_.regret_log_file.c_str());
    } else {
      ROS_WARN("Failed to open log file: %s", params_.regret_log_file.c_str());
    }
  }

  ROS_INFO("==============================================");
  ROS_INFO("Safe-Regret MPC Node Initialized");
  ROS_INFO("  Subscribing to:");
  ROS_INFO("    - /tube_mpc/tracking_error");
  ROS_INFO("    - /mpc_trajectory");
  ROS_INFO("    - /stl_robustness");
  ROS_INFO("    - /stl_budget");
  ROS_INFO("    - /dr_margins");
  ROS_INFO("    - /odom");
  ROS_INFO("    - /move_base_simple/goal");
  ROS_INFO("  Publishing to:");
  ROS_INFO("    - %s", params_.reference_trajectory_topic.c_str());
  ROS_INFO("    - %s", params_.regret_metrics_topic.c_str());
  ROS_INFO("    - %s", params_.feasibility_status_topic.c_str());
  ROS_INFO("==============================================");

  return true;
}

void SafeRegretNode::spin() {
  ros::spin();
}

bool SafeRegretNode::isHealthy() const {
  return true;  // TODO: Add health checks
}

// ============================================================================
// Parameter Loading
// ============================================================================

bool SafeRegretNode::loadParameters() {
  // Regret analysis parameters
  private_nh_.param("lipschitz_constant", params_.lipschitz_constant, 1.0);
  private_nh_.param("infeasibility_penalty", params_.infeasibility_penalty, 1000.0);
  private_nh_.param("tracking_error_constant", params_.tracking_error_constant, 1.0);

  // Load LQR gain matrix (2x3 for 2 inputs, 3 states)
  std::vector<double> gain_matrix;
  if (private_nh_.getParam("lqr_gain", gain_matrix)) {
    if (gain_matrix.size() == 6) {  // 2x3
      params_.lqr_gain = Eigen::MatrixXd::Map(gain_matrix.data(), 2, 3);
    } else {
      ROS_WARN("Invalid LQR gain size, using zero matrix");
      params_.lqr_gain = Eigen::MatrixXd::Zero(2, 3);
    }
  } else {
    params_.lqr_gain = Eigen::MatrixXd::Zero(2, 3);
  }

  // Reference planner parameters
  private_nh_.param("no_regret_algorithm", params_.no_regret_algorithm, std::string("omd"));
  private_nh_.param("learning_rate", params_.learning_rate, 0.1);
  private_nh_.param("planning_horizon", params_.planning_horizon, 20);
  private_nh_.param("update_frequency", params_.update_frequency, 10.0);

  // Feasibility constraints
  private_nh_.param("max_velocity", params_.max_velocity, 2.0);
  private_nh_.param("max_curvature", params_.max_curvature, 1.0);
  private_nh_.param("tube_radius", params_.tube_radius, 0.5);

  // STL parameters
  private_nh_.param("stl_formula_type", params_.stl_formula_type, std::string("reachability"));
  private_nh_.param("stl_time_horizon", params_.stl_time_horizon, 10.0);
  private_nh_.param("robustness_weight", params_.robustness_weight, 10.0);
  private_nh_.param("robustness_budget_baseline", params_.robustness_budget_baseline, 0.1);

  // Logging
  private_nh_.param("enable_regret_logging", params_.enable_regret_logging, true);
  private_nh_.param("regret_log_file", params_.regret_log_file,
           std::string("/tmp/safe_regret_data.csv"));

  // Topics
  private_nh_.param("reference_trajectory_topic", params_.reference_trajectory_topic,
           std::string("/safe_regret/reference_trajectory"));
  private_nh_.param("regret_metrics_topic", params_.regret_metrics_topic,
           std::string("/safe_regret/regret_metrics"));
  private_nh_.param("feasibility_status_topic", params_.feasibility_status_topic,
           std::string("/safe_regret/feasibility_status"));

  return true;
}

void SafeRegretNode::setupROSConnections() {
  // Subscribers from Phase 1-3
  sub_tracking_error_ = nh_.subscribe(
    "/tube_mpc/tracking_error", 1,
    &SafeRegretNode::trackingErrorCallback, this);

  sub_mpc_trajectory_ = nh_.subscribe(
    "/mpc_trajectory", 1,
    &SafeRegretNode::mpcTrajectoryCallback, this);

  sub_stl_robustness_ = nh_.subscribe(
    "/stl_robustness", 1,
    &SafeRegretNode::stlRobustnessCallback, this);

  sub_stl_budget_ = nh_.subscribe(
    "/stl_budget", 1,
    &SafeRegretNode::stlBudgetCallback, this);

  sub_dr_margins_ = nh_.subscribe(
    "/dr_margins", 1,
    &SafeRegretNode::drMarginsCallback, this);

  sub_odom_ = nh_.subscribe(
    "/odom", 1,
    &SafeRegretNode::odomCallback, this);

  // Goal subscriber (from move_base or user input)
  sub_goal_ = nh_.subscribe(
    "/move_base_simple/goal", 1,
    &SafeRegretNode::goalCallback, this);

  // Publishers
  pub_reference_trajectory_ = nh_.advertise<nav_msgs::Path>(
    params_.reference_trajectory_topic, 1);

  pub_regret_metrics_ = nh_.advertise<std_msgs::Float64MultiArray>(
    params_.regret_metrics_topic, 1);

  pub_feasibility_status_ = nh_.advertise<std_msgs::Float64MultiArray>(
    params_.feasibility_status_topic, 1);

  // Planning timer
  double period = 1.0 / params_.update_frequency;
  planning_timer_ = nh_.createTimer(
    ros::Duration(period),
    &SafeRegretNode::planningTimerCallback, this);

  ROS_INFO("ROS connections created");
}

void SafeRegretNode::initializeCoreComponents() {
  // Determine algorithm type
  NoRegretAlgorithm algorithm;
  if (params_.no_regret_algorithm == "omd") {
    algorithm = NoRegretAlgorithm::ONLINE_MIRROR_DESCENT;
  } else if (params_.no_regret_algorithm == "ftrl") {
    algorithm = NoRegretAlgorithm::FOLLOW_THE_REGULARIZED_LEADER;
  } else {
    algorithm = NoRegretAlgorithm::MULTIPlicative_WEIGHTS;
  }

  // Create regret analyzer
  regret_analyzer_ = std::make_unique<RegretAnalyzer>(
    3,  // state_dim (x, y, theta)
    2,  // input_dim (v, omega)
    params_.lipschitz_constant,
    params_.lqr_gain,
    params_.infeasibility_penalty
  );

  // Create reference planner
  reference_planner_ = std::make_unique<ReferencePlanner>(
    3,  // state_dim
    2,  // input_dim
    algorithm
  );

  reference_planner_->setLearningRate(params_.learning_rate);

  // Create feasibility checker
  feasibility_checker_ = std::make_unique<FeasibilityChecker>();

  ROS_INFO("Core components initialized");
  ROS_INFO("  Algorithm: %s", params_.no_regret_algorithm.c_str());
  ROS_INFO("  Learning rate: %.3f", params_.learning_rate);
  ROS_INFO("  Planning horizon: %d", params_.planning_horizon);
}

// ============================================================================
// Callbacks
// ============================================================================

void SafeRegretNode::trackingErrorCallback(const std_msgs::Float64MultiArray::ConstPtr& msg) {
  if (msg->data.size() >= 3) {
    latest_tracking_error_[0] = msg->data[0];  // cte (cross-track error)
    latest_tracking_error_[1] = msg->data[1];  // etheta (heading error)
    latest_tracking_error_[2] = msg->data[2];  // error_norm
    received_tracking_error_ = true;
  }
}

void SafeRegretNode::mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg) {
  latest_mpc_trajectory_ = *msg;
  received_mpc_trajectory_ = true;
}

void SafeRegretNode::stlRobustnessCallback(const std_msgs::Float64::ConstPtr& msg) {
  latest_stl_robustness_ = msg->data;
  received_stl_data_ = true;
}

void SafeRegretNode::stlBudgetCallback(const std_msgs::Float64::ConstPtr& msg) {
  latest_stl_budget_ = msg->data;
  received_stl_data_ = true;
}

void SafeRegretNode::drMarginsCallback(const std_msgs::Float64MultiArray::ConstPtr& msg) {
  latest_dr_margins_.clear();
  for (const auto& val : msg->data) {
    latest_dr_margins_.push_back(val);
  }
  received_dr_margins_ = true;
}

void SafeRegretNode::odomCallback(const nav_msgs::Odometry::ConstPtr& msg) {
  latest_odom_ = *msg;
  received_odom_ = true;

  // Update belief state mean from odometry
  current_belief_.mean[0] = msg->pose.pose.position.x;
  current_belief_.mean[1] = msg->pose.pose.position.y;

  // Extract yaw angle
  tf2::Quaternion q(
    msg->pose.pose.orientation.x,
    msg->pose.pose.orientation.y,
    msg->pose.pose.orientation.z,
    msg->pose.pose.orientation.w);
  tf2::Matrix3x3 m(q);
  double roll, pitch, yaw;
  m.getRPY(roll, pitch, yaw);
  current_belief_.mean[2] = yaw;
}

void SafeRegretNode::goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg) {
  current_goal_ = *msg;
  received_goal_ = true;

  // Update reference planner with new goal
  if (reference_planner_) {
#ifdef HAVE_OMPL
    reference_planner_->setGoal(
      msg->pose.position.x,
      msg->pose.position.y
    );
    ROS_INFO("ReferencePlanner: Goal set to (%.2f, %.2f)",
             msg->pose.position.x, msg->pose.position.y);
#else
    ROS_INFO("Goal received but OMPL not available");
#endif
  }
}

// ============================================================================
// Main Planning Loop
// ============================================================================

void SafeRegretNode::planningTimerCallback(const ros::TimerEvent& event) {
  // Check if we have received data from all phases
  if (!received_odom_) {
    ROS_WARN_THROTTLE(5.0, "Waiting for odometry data...");
    return;
  }

  // Update belief state and STL specification
  updateBeliefState();
  updateSTLSpecification();

  // Generate reference trajectory
  current_reference_ = generateReferenceTrajectory();

  // Check feasibility
  checkFeasibilityAndPublish();

  // Update regret analysis
  if (received_tracking_error_ && received_mpc_trajectory_) {
    updateRegretAnalysis();
  }

  // Publish reference trajectory
  publishReferenceTrajectory(current_reference_);

  current_step_++;
}

void SafeRegretNode::updateBeliefState() {
  // Update covariance based on tracking error
  if (received_tracking_error_) {
    double error_norm = latest_tracking_error_.norm();

    // Simple model: increase covariance with tracking error
    double uncertainty = 0.1 + 0.5 * error_norm;
    current_belief_.covariance(0, 0) = uncertainty;
    current_belief_.covariance(1, 1) = uncertainty;
    current_belief_.covariance(2, 2) = 0.1 * uncertainty;
  }
}

void SafeRegretNode::updateSTLSpecification() {
  // Create STL specification based on type
  if (params_.stl_formula_type == "reachability") {
    current_stl_spec_ = STLSpecification::createReachability(
      "goal", params_.stl_time_horizon);
  } else if (params_.stl_formula_type == "safety") {
    current_stl_spec_ = STLSpecification::createSafety(
      "safe", params_.stl_time_horizon);
  } else {
    // Default: reachability
    current_stl_spec_ = STLSpecification::createReachability(
      "goal", params_.stl_time_horizon);
  }
}

ReferenceTrajectory SafeRegretNode::generateReferenceTrajectory() {
  // Plan reference trajectory using abstract planner
  ReferenceTrajectory reference = reference_planner_->planReference(
    current_belief_,
    current_stl_spec_,
    params_.planning_horizon
  );

  // Update learning algorithm with executed reference
  double reference_cost = 0.0;
  for (const auto& pt : reference.points) {
    reference_cost += pt.state.squaredNorm() + pt.input.squaredNorm();
  }

  reference_planner_->updateLearning(reference, reference_cost);

  return reference;
}

void SafeRegretNode::updateRegretAnalysis() {
  // Extract states from MPC trajectory
  if (latest_mpc_trajectory_.poses.size() < 2) {
    return;
  }

  // Get actual and nominal states
  Eigen::VectorXd actual_state = poseToVector(latest_mpc_trajectory_.poses[0].pose);
  Eigen::VectorXd actual_input = Eigen::VectorXd::Zero(2);

  // Nominal state (actual - tracking error)
  Eigen::VectorXd nominal_state = actual_state - latest_tracking_error_;
  Eigen::VectorXd nominal_input = actual_input;

  // Reference state (from reference planner)
  Eigen::VectorXd reference_state = actual_state;  // Use actual as reference
  Eigen::VectorXd reference_input = actual_input;

  // Update regret analyzer
  regret_analyzer_->updateStep(
    actual_state,
    actual_input,
    nominal_state,
    nominal_input,
    latest_tracking_error_,
    reference_state,
    reference_input,
    nullptr,  // No oracle available
    true,     // Assume feasible
    0.0      // Tightening slack (simplified)
  );

  // Compute cumulative regrets every 10 steps
  if (current_step_ % 10 == 0) {
    current_metrics_ = regret_analyzer_->computeCumulativeRegrets();
    publishRegretMetrics(current_metrics_);
    logRegretData(current_metrics_);

    // Print summary
    if (current_step_ % 50 == 0) {
      current_metrics_.printSummary();
    }
  }
}

void SafeRegretNode::checkFeasibilityAndPublish() {
  // Define state and input bounds
  Eigen::VectorXd state_lower(3), state_upper(3);
  state_lower << -10.0, -10.0, -M_PI;
  state_upper << 10.0, 10.0, M_PI;

  Eigen::VectorXd input_lower(2), input_upper(2);
  input_lower << -params_.max_velocity, -1.0;
  input_upper << params_.max_velocity, 1.0;

  // Check feasibility
  std::pair<bool, std::string> feasibility_result = feasibility_checker_->checkFeasibility(
    current_reference_,
    params_.max_velocity,
    params_.max_curvature,
    params_.tube_radius,
    state_lower,
    state_upper,
    input_lower,
    input_upper
  );

  bool is_feasible = feasibility_result.first;
  std::string reason = feasibility_result.second;

  publishFeasibilityStatus(is_feasible, reason);

  if (!is_feasible) {
    ROS_WARN_THROTTLE(5.0, "Reference trajectory infeasible: %s", reason.c_str());

    // Sanitize trajectory
    current_reference_ = feasibility_checker_->sanitizeInfeasible(
      current_reference_,
      params_.max_velocity,
      params_.max_curvature,
      params_.tube_radius
    );
  }
}

void SafeRegretNode::publishReferenceTrajectory(const ReferenceTrajectory& reference) {
  nav_msgs::Path path_msg = referenceToPathMsg(reference);
  path_msg.header.stamp = ros::Time::now();
  path_msg.header.frame_id = "map";

  pub_reference_trajectory_.publish(path_msg);
}

void SafeRegretNode::publishRegretMetrics(const RegretMetrics& metrics) {
  std_msgs::Float64MultiArray msg;

  msg.data.push_back(metrics.safe_regret);
  msg.data.push_back(metrics.dynamic_regret);
  msg.data.push_back(metrics.tracking_contribution);
  msg.data.push_back(metrics.nominal_contribution);
  msg.data.push_back(metrics.tightening_contribution);
  msg.data.push_back(metrics.sublinear_check);
  msg.data.push_back(static_cast<double>(metrics.horizon_T));

  pub_regret_metrics_.publish(msg);
}

void SafeRegretNode::publishFeasibilityStatus(bool is_feasible, const std::string& message) {
  std_msgs::Float64MultiArray msg;

  msg.data.push_back(is_feasible ? 1.0 : 0.0);
  msg.data.push_back(message.empty() ? 0.0 : 1.0);  // Has message

  pub_feasibility_status_.publish(msg);
}

void SafeRegretNode::logRegretData(const RegretMetrics& metrics) {
  if (log_file_.is_open()) {
    log_file_ << current_step_ << ","
              << metrics.safe_regret << ","
              << metrics.dynamic_regret << ","
              << metrics.tracking_contribution << ","
              << metrics.nominal_contribution << ","
              << metrics.tightening_contribution << ","
              << metrics.sublinear_check << "\n";
  }
}

// ============================================================================
// Helper Methods
// ============================================================================

Eigen::VectorXd SafeRegretNode::poseToVector(const geometry_msgs::Pose& pose) const {
  Eigen::VectorXd vec(3);

  vec[0] = pose.position.x;
  vec[1] = pose.position.y;

  // Extract yaw angle
  tf2::Quaternion q(
    pose.orientation.x,
    pose.orientation.y,
    pose.orientation.z,
    pose.orientation.w);
  tf2::Matrix3x3 m(q);
  double roll, pitch, yaw;
  m.getRPY(roll, pitch, yaw);
  vec[2] = yaw;

  return vec;
}

geometry_msgs::Pose SafeRegretNode::vectorToPose(const Eigen::VectorXd& vec) const {
  geometry_msgs::Pose pose;

  pose.position.x = vec[0];
  pose.position.y = vec[1];
  pose.position.z = 0.0;

  // Set yaw angle
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, vec[2]);
  pose.orientation.x = q.x();
  pose.orientation.y = q.y();
  pose.orientation.z = q.z();
  pose.orientation.w = q.w();

  return pose;
}

nav_msgs::Path SafeRegretNode::referenceToPathMsg(const ReferenceTrajectory& reference) const {
  nav_msgs::Path path_msg;

  for (const auto& pt : reference.points) {
    geometry_msgs::PoseStamped pose_stamped;
    pose_stamped.pose = vectorToPose(pt.state);
    pose_stamped.header.stamp = ros::Time(pt.timestamp);
    pose_stamped.header.frame_id = "map";
    path_msg.poses.push_back(pose_stamped);
  }

  return path_msg;
}

std::vector<double> SafeRegretNode::computeNominalInputs(const ReferenceTrajectory& reference) const {
  std::vector<double> inputs;

  for (const auto& pt : reference.points) {
    inputs.push_back(pt.input[0]);  // Linear velocity
    inputs.push_back(pt.input[1]);  // Angular velocity
  }

  return inputs;
}

} // namespace safe_regret
