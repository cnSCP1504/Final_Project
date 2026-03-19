/**
 * @file dr_tightening_node.cpp
 * @brief Implementation of DR tightening ROS node
 */

#include "dr_tightening/dr_tightening_node.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

namespace dr_tightening {

// ============================================================================
// NavigationSafetyFunction Implementation
// ============================================================================

NavigationSafetyFunction::NavigationSafetyFunction(
  const std::vector<Eigen::Vector2d>& obstacle_positions,
  double safety_buffer)
  : obstacle_positions_(obstacle_positions), safety_buffer_(safety_buffer) {
}

double NavigationSafetyFunction::evaluate(const Eigen::VectorXd& state) const {
  if (obstacle_positions_.empty()) {
    // No obstacles: return large positive value (always safe)
    return 1000.0;
  }

  if (state.size() < 2) {
    ROS_WARN("State dimension too small for obstacle checking");
    return 0.0;
  }

  Eigen::Vector2d position(state[0], state[1]);
  double min_distance = findNearestObstacleDistance(position);

  return min_distance - safety_buffer_;
}

Eigen::VectorXd NavigationSafetyFunction::gradient(const Eigen::VectorXd& state) const {
  if (state.size() < 2) {
    return Eigen::VectorXd::Zero(3);
  }

  Eigen::Vector2d position(state[0], state[1]);
  Eigen::VectorXd grad = Eigen::VectorXd::Zero(state.size());

  // Find nearest obstacle
  if (obstacle_positions_.empty()) {
    return grad;  // No gradient if no obstacles
  }

  // Compute gradient toward nearest obstacle
  Eigen::Vector2d nearest_obstacle = obstacle_positions_[0];
  double min_dist = (position - nearest_obstacle).norm();

  for (const auto& obstacle : obstacle_positions_) {
    double dist = (position - obstacle).norm();
    if (dist < min_dist) {
      min_dist = dist;
      nearest_obstacle = obstacle;
    }
  }

  // Gradient is unit vector pointing away from obstacle
  Eigen::Vector2d diff = position - nearest_obstacle;
  double dist = diff.norm();

  if (dist > 1e-6) {
    grad[0] = diff[0] / dist;
    grad[1] = diff[1] / dist;
  }

  return grad;
}

double NavigationSafetyFunction::estimateLipschitz(
  const Eigen::Vector2d& center,
  double radius) const {
  // For distance-based safety function, Lipschitz constant is 1.0
  // (gradient is unit vector)
  return 1.0;
}

void NavigationSafetyFunction::addObstacle(const Eigen::Vector2d& position) {
  obstacle_positions_.push_back(position);
}

void NavigationSafetyFunction::clearObstacles() {
  obstacle_positions_.clear();
}

size_t NavigationSafetyFunction::getObstacleCount() const {
  return obstacle_positions_.size();
}

double NavigationSafetyFunction::findNearestObstacleDistance(
  const Eigen::Vector2d& position) const {
  double min_dist = std::numeric_limits<double>::max();

  for (const auto& obstacle : obstacle_positions_) {
    double dist = (position - obstacle).norm();
    min_dist = std::min(min_dist, dist);
  }

  return min_dist;
}

// ============================================================================
// DRTighteningNode Implementation
// ============================================================================

DRTighteningNode::DRTighteningNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh)
  : nh_(nh), private_nh_(private_nh),
    have_state_(false), have_cmd_vel_(false), data_count_(0) {
}

DRTighteningNode::~DRTighteningNode() {
  if (csv_file_.is_open()) {
    csv_file_.close();
  }
}

bool DRTighteningNode::initialize() {
  // Load parameters
  if (!loadParameters()) {
    ROS_ERROR("Failed to load parameters");
    return false;
  }

  // Setup safety function
  setupSafetyFunction();

  // Create ROS connections
  if (!createConnections()) {
    ROS_ERROR("Failed to create ROS connections");
    return false;
  }

  // Open CSV log file if enabled
  openLogFile();

  // Setup update timer
  double update_period = 1.0 / params_.update_rate;
  update_timer_ = nh_.createTimer(
    ros::Duration(update_period),
    &DRTighteningNode::updateCallback,
    this);

  ROS_INFO("DR Tightening Node initialized");
  ROS_INFO("  Window size: %d", params_.residual_window_size);
  ROS_INFO("  Total risk δ: %.3f", params_.total_risk_delta);
  ROS_INFO("  MPC horizon: %d", params_.mpc_horizon);
  ROS_INFO("  Update rate: %.1f Hz", params_.update_rate);

  return true;
}

void DRTighteningNode::spin() {
  ros::spin();
}

bool DRTighteningNode::isHealthy() const {
  return residual_collector_->getWindowSize() >= MIN_RESIDUALS_FOR_UPDATE;
}

// ============================================================================
// Callbacks
// ============================================================================

void DRTighteningNode::trackingErrorCallback(
  const std_msgs::Float64MultiArray::ConstPtr& msg) {
  // Extract residual from tracking error
  // Assuming msg->data contains [cte, etheta, velocity_error, ...]

  if (msg->data.size() < static_cast<size_t>(params_.residual_dimension)) {
    ROS_WARN_THROTTLE(1.0, "Tracking error dimension mismatch: expected %d, got %zu",
                      params_.residual_dimension, msg->data.size());
    return;
  }

  // Convert to Eigen vector
  Eigen::VectorXd residual(params_.residual_dimension);
  for (int i = 0; i < params_.residual_dimension; ++i) {
    residual[i] = msg->data[i];
  }

  // Add to collector
  residual_collector_->addResidual(residual);

  if (params_.enable_debug_output) {
    ROS_DEBUG("Added residual, window size: %d",
              residual_collector_->getWindowSize());
  }
}

void DRTighteningNode::mpcTrajectoryCallback(
  const nav_msgs::Path::ConstPtr& msg) {
  // Store latest MPC trajectory for safety evaluation
  // Could be used to evaluate safety along entire predicted path

  if (params_.enable_debug_output) {
    ROS_DEBUG("Received MPC trajectory with %zu poses", msg->poses.size());
  }
}

void DRTighteningNode::odomCallback(const nav_msgs::Odometry::ConstPtr& msg) {
  // Extract robot state from odometry
  current_state_ = Eigen::VectorXd::Zero(3);
  current_state_[0] = msg->pose.pose.position.x;
  current_state_[1] = msg->pose.pose.position.y;

  // Extract yaw from quaternion
  tf2::Quaternion q;
  tf2::fromMsg(msg->pose.pose.orientation, q);
  double yaw, pitch, roll;
  tf2::Matrix3x3(q).getEulerYPR(yaw, pitch, roll);
  current_state_[2] = yaw;

  have_state_ = true;
}

void DRTighteningNode::cmdVelCallback(
  const geometry_msgs::Twist::ConstPtr& msg) {
  current_cmd_vel_.header.stamp = ros::Time::now();
  current_cmd_vel_.twist = *msg;
  have_cmd_vel_ = true;
}

void DRTighteningNode::updateCallback(const ros::TimerEvent& event) {
  if (!have_state_ || !have_cmd_vel_) {
    ROS_WARN_ONCE("Waiting for state and command velocity");
    return;
  }

  // Check if we have enough residuals
  if (residual_collector_->getWindowSize() < MIN_RESIDUALS_FOR_UPDATE) {
    return;
  }

  // Compute DR tightening margins
  std::vector<double> margins(params_.mpc_horizon + 1);
  std::vector<double> risk_allocations(params_.mpc_horizon + 1);

  // Linearize safety function at current state
  auto linearization_result = safety_linearization_->linearize(current_state_);

  // Compute margins for each time step
  bool success = true;
  for (int t = 0; t <= params_.mpc_horizon; ++t) {
    if (!computeDRTightening(current_state_,
                             linearization_result.gradient,
                             margins[t])) {
      success = false;
      break;
    }
  }

  if (success) {
    // Get risk allocation
    risk_allocations = tightening_computer_->allocateRisk(
      params_.total_risk_delta,
      params_.mpc_horizon,
      parseRiskAllocation(params_.risk_allocation_strategy));

    // Publish results
    publishMargins(margins, risk_allocations);

    if (params_.enable_debug_output) {
      publishDebugInfo(margins, risk_allocations, computeStatistics());
    }

    if (params_.enable_visualization) {
      // Get current trajectory (if available)
      nav_msgs::Path::ConstPtr trajectory;  // Would need to store this
      publishVisualization(margins, trajectory);
    }

    // Log to CSV
    if (params_.log_to_csv) {
      logToCSV(margins, risk_allocations);
    }

    data_count_++;
  }

  // Publish statistics
  publishStatistics();
}

// ============================================================================
// Core Computation
// ============================================================================

bool DRTighteningNode::computeDRTightening(
  const Eigen::VectorXd& nominal_state,
  const Eigen::VectorXd& gradient,
  double& margin_out) {

  // Evaluate safety function
  double safety_value = safety_function_->evaluate(nominal_state);

  // Compute margin using TighteningComputer
  double tube_offset = tightening_computer_->computeTubeOffset(
    params_.lipschitz_constant,
    params_.tube_radius);

  margin_out = tightening_computer_->computeChebyshevMargin(
    nominal_state,
    safety_value,
    gradient,
    *residual_collector_,
    params_.tube_radius,
    params_.lipschitz_constant,
    0);  // time_step = 0 (current)

  return true;
}

// ============================================================================
// Publishing
// ============================================================================

void DRTighteningNode::publishMargins(
  const std::vector<double>& margins,
  const std::vector<double>& risk_allocations) {

  std_msgs::Float64MultiArray margin_msg;
  margin_msg.data = margins;

  pub_margins_.publish(margin_msg);
}

void DRTighteningNode::publishDebugInfo(
  const std::vector<double>& margins,
  const std::vector<double>& risk_allocations,
  const Eigen::VectorXd& stats) {

  std_msgs::Float64MultiArray debug_msg;

  // Pack debug info:
  // [margins..., risk_allocations..., mean..., std..., tube_radius]

  debug_msg.data = margins;
  debug_msg.data.insert(debug_msg.data.end(), risk_allocations.begin(),
                       risk_allocations.end());

  // Add statistics
  if (stats.size() > 0) {
    debug_msg.data.insert(debug_msg.data.end(), stats.data(),
                         stats.data() + stats.size());
  }

  // Add tube radius
  debug_msg.data.push_back(params_.tube_radius);

  pub_margins_debug_.publish(debug_msg);
}

void DRTighteningNode::publishStatistics() {
  std_msgs::Float64MultiArray stats_msg;

  // Statistics format:
  // [window_size, current_time, margin_0, margin_N, ..., num_obstacles]

  stats_msg.data.push_back(residual_collector_->getWindowSize());
  stats_msg.data.push_back(ros::Time::now().toSec());
  stats_msg.data.push_back(safety_function_->getObstacleCount());

  pub_statistics_.publish(stats_msg);
}

void DRTighteningNode::publishVisualization(
  const std::vector<double>& margins,
  const nav_msgs::Path::ConstPtr& trajectory) {

  // Create visualization markers for safety margins
  visualization_msgs::MarkerArray markers;

  // Marker for current safety margin
  visualization_msgs::Marker margin_marker;
  margin_marker.header.frame_id = "map";
  margin_marker.header.stamp = ros::Time::now();
  margin_marker.ns = "dr_margins";
  margin_marker.id = 0;
  margin_marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
  margin_marker.action = visualization_msgs::Marker::ADD;
  margin_marker.pose.position.x = current_state_[0];
  margin_marker.pose.position.y = current_state_[1];
  margin_marker.pose.position.z = 1.0;
  margin_marker.scale.z = 0.3;
  margin_marker.color.r = 1.0;
  margin_marker.color.g = 0.0;
  margin_marker.color.b = 0.0;
  margin_marker.color.a = 1.0;
  margin_marker.text = "Margin: " + std::to_string(margins[0]);

  markers.markers.push_back(margin_marker);

  pub_viz_markers_.publish(markers);
}

// ============================================================================
// Utility Functions
// ============================================================================

RiskAllocation DRTighteningNode::parseRiskAllocation(
  const std::string& strategy) const {

  if (strategy == "uniform") {
    return RiskAllocation::UNIFORM;
  } else if (strategy == "deadline_weighted") {
    return RiskAllocation::DEADLINE_WEIGHED;
  } else if (strategy == "inverse_square") {
    return RiskAllocation::INVERSE_SQUARE;
  } else {
    ROS_WARN("Unknown risk allocation strategy: %s, using uniform",
             strategy.c_str());
    return RiskAllocation::UNIFORM;
  }
}

AmbiguityStrategy DRTighteningNode::parseAmbiguityStrategy(
  const std::string& strategy) const {

  if (strategy == "wasserstein_ball") {
    return AmbiguityStrategy::WATERSTEIN_BALL;
  } else if (strategy == "empirical_gaussian") {
    return AmbiguityStrategy::EMPIRICAL_GAUSSIAN;
  } else if (strategy == "kl_divergence") {
    return AmbiguityStrategy::KL_DIVERGENCE;
  } else {
    ROS_WARN("Unknown ambiguity strategy: %s, using wasserstein_ball",
             strategy.c_str());
    return AmbiguityStrategy::WATERSTEIN_BALL;
  }
}

bool DRTighteningNode::loadParameters() {
  // Get parameters from server
  private_nh_.param("residual_window_size", params_.residual_window_size, 200);
  private_nh_.param("residual_dimension", params_.residual_dimension, 3);
  private_nh_.param("total_risk_delta", params_.total_risk_delta, 0.1);
  private_nh_.param("mpc_horizon", params_.mpc_horizon, 20);
  private_nh_.param("risk_allocation_strategy", params_.risk_allocation_strategy,
                   std::string("uniform"));
  private_nh_.param("ambiguity_strategy", params_.ambiguity_strategy,
                   std::string("wasserstein_ball"));
  private_nh_.param("confidence_level", params_.confidence_level, 0.95);
  private_nh_.param("tube_radius", params_.tube_radius, 0.5);
  private_nh_.param("lipschitz_constant", params_.lipschitz_constant, 1.0);
  private_nh_.param("safety_buffer", params_.safety_buffer, 1.0);
  private_nh_.param("obstacle_influence_radius", params_.obstacle_influence_radius, 5.0);
  private_nh_.param("update_rate", params_.update_rate, 10.0);
  private_nh_.param("enable_visualization", params_.enable_visualization, true);
  private_nh_.param("enable_debug_output", params_.enable_debug_output, false);
  private_nh_.param("log_to_csv", params_.log_to_csv, false);
  private_nh_.param("csv_output_path", params_.csv_output_path,
                   std::string("/tmp/dr_margins.csv"));

  // Validate parameters
  if (params_.total_risk_delta <= 0.0 || params_.total_risk_delta >= 1.0) {
    ROS_ERROR("Invalid total_risk_delta: %.3f (must be in (0,1))",
              params_.total_risk_delta);
    return false;
  }

  if (params_.residual_window_size < 10) {
    ROS_WARN("Residual window size very small: %d", params_.residual_window_size);
  }

  if (params_.update_rate <= 0.0 || params_.update_rate > 100.0) {
    ROS_ERROR("Invalid update_rate: %.2f", params_.update_rate);
    return false;
  }

  return true;
}

bool DRTighteningNode::createConnections() {
  // Create core modules
  residual_collector_ = std::make_unique<ResidualCollector>(
    params_.residual_window_size);

  ambiguity_calibrator_ = std::make_unique<AmbiguityCalibrator>(
    parseAmbiguityStrategy(params_.ambiguity_strategy),
    params_.confidence_level);

  tightening_computer_ = std::make_unique<TighteningComputer>(
    params_.total_risk_delta,
    params_.mpc_horizon,
    parseRiskAllocation(params_.risk_allocation_strategy));

  // Setup safety function
  auto safety_func = [this](const Eigen::VectorXd& x) -> double {
    return this->safety_function_->evaluate(x);
  };

  auto safety_grad = [this](const Eigen::VectorXd& x) -> Eigen::VectorXd {
    return this->safety_function_->gradient(x);
  };

  safety_linearization_ = std::make_unique<SafetyLinearization>(
    safety_func, safety_grad);

  // Setup subscribers
  sub_tracking_error_ = nh_.subscribe(
    "/tube_mpc/tracking_error", 1,
    &DRTighteningNode::trackingErrorCallback, this);

  sub_mpc_trajectory_ = nh_.subscribe(
    "/mpc_trajectory", 1,
    &DRTighteningNode::mpcTrajectoryCallback, this);

  sub_odom_ = nh_.subscribe(
    "/odom", 1,
    &DRTighteningNode::odomCallback, this);

  sub_cmd_vel_ = nh_.subscribe(
    "/cmd_vel", 1,
    &DRTighteningNode::cmdVelCallback, this);

  // Setup publishers
  pub_margins_ = nh_.advertise<std_msgs::Float64MultiArray>(
    "/dr_margins", 1);

  pub_margins_debug_ = nh_.advertise<std_msgs::Float64MultiArray>(
    "/dr_margins_debug", 1);

  pub_statistics_ = nh_.advertise<std_msgs::Float64MultiArray>(
    "/dr_statistics", 1);

  pub_viz_markers_ = nh_.advertise<visualization_msgs::MarkerArray>(
    "/dr_visualization", 1);

  ROS_INFO("ROS connections created");
  ROS_INFO("  Subscribing to: /tube_mpc/tracking_error, /mpc_trajectory, /odom, /cmd_vel");
  ROS_INFO("  Publishing to: /dr_margins, /dr_margins_debug, /dr_statistics");

  return true;
}

void DRTighteningNode::setupSafetyFunction() {
  // Initialize with no obstacles (can be added dynamically)
  safety_function_ = std::make_unique<NavigationSafetyFunction>(
    std::vector<Eigen::Vector2d>(),  // No obstacles initially
    params_.safety_buffer);

  ROS_INFO("Safety function initialized with buffer: %.2f", params_.safety_buffer);
}

void DRTighteningNode::openLogFile() {
  if (!params_.log_to_csv) {
    return;
  }

  csv_file_.open(params_.csv_output_path);
  if (!csv_file_.is_open()) {
    ROS_ERROR("Failed to open CSV log file: %s",
              params_.csv_output_path.c_str());
    return;
  }

  // Write header
  csv_file_ << "timestamp,window_size";
  for (int i = 0; i <= params_.mpc_horizon; ++i) {
    csv_file_ << ",margin_" << i;
  }
  for (int i = 0; i <= params_.mpc_horizon; ++i) {
    csv_file_ << ",risk_" << i;
  }
  csv_file_ << std::endl;

  ROS_INFO("Logging DR margins to: %s", params_.csv_output_path.c_str());
}

Eigen::VectorXd DRTighteningNode::computeStatistics() const {
  Eigen::VectorXd stats = residual_collector_->getMean();

  // Add additional statistics if needed
  // For now, just return mean

  return stats;
}

void DRTighteningNode::logToCSV(
  const std::vector<double>& margins,
  const std::vector<double>& risks) {

  if (!csv_file_.is_open()) {
    return;
  }

  csv_file_ << ros::Time::now().toSec() << ","
            << residual_collector_->getWindowSize();

  for (double margin : margins) {
    csv_file_ << "," << margin;
  }

  for (double risk : risks) {
    csv_file_ << "," << risk;
  }

  csv_file_ << std::endl;
}

} // namespace dr_tightening
