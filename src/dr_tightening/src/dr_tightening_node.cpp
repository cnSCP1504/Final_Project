/**
 * @file dr_tightening_node.cpp
 * @brief Implementation of DR tightening ROS node
 */

#include "dr_tightening/dr_tightening_node.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <nav_msgs/OccupancyGrid.h>  // ✅ 方案B: Costmap支持

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
  // ✅ 方案B: 使用costmap作为安全函数
  if (state.size() < 2) {
    return 0.0;
  }

  Eigen::Vector2d position(state[0], state[1]);

  // ✅ 优先使用costmap
  if (hasCostmap()) {
    static int debug_count = 0;
    if (debug_count++ < 5) {  // 只打印5次
      double costmap_value = getCostmapValue(position[0], position[1]);
      std::cout << "[DEBUG] Using COSTMAP safety function" << std::endl;
      std::cout << "  position: [" << position[0] << ", " << position[1] << "]" << std::endl;
      std::cout << "  costmap_value: " << costmap_value << std::endl;
    }

    double costmap_value = getCostmapValue(position[0], position[1]);

    // h(x) = 1.0 - (costmap_value / 254.0)
    // costmap_value = 0 (自由空间) -> h(x) = 1.0 (安全)
    // costmap_value = 254 (障碍物) -> h(x) = 0.0 (危险)
    double safety_value = 1.0 - (costmap_value / 254.0);

    return safety_value - safety_buffer_;
  }

  // Fallback: 如果没有costmap，使用障碍物列表
  if (obstacle_positions_.empty()) {
    // ✅ 使用状态边界约束
    // 地图边界: X ∈ [-10, 10], Y ∈ [-10, 10]
    double x_max = 10.0;
    double y_max = 10.0;

    // h(x) = 1.0 - (x²/x_max² + y²/y_max²)
    double normalized_x = position[0] / x_max;
    double normalized_y = position[1] / y_max;
    double safety_value = 1.0 - (normalized_x * normalized_x +
                                  normalized_y * normalized_y);

    static int debug_count = 0;
    if (debug_count++ < 5) {
      std::cout << "[DEBUG] Using STATE BOUND fallback (no costmap, no obstacles)" << std::endl;
      std::cout << "  position: [" << position[0] << ", " << position[1] << "]" << std::endl;
      std::cout << "  safety_value: " << safety_value << std::endl;
    }

    return safety_value - safety_buffer_;
  }

  // 使用障碍物列表
  double min_distance = findNearestObstacleDistance(position);
  return min_distance - safety_buffer_;
}

Eigen::VectorXd NavigationSafetyFunction::gradient(const Eigen::VectorXd& state) const {
  if (state.size() < 2) {
    return Eigen::VectorXd::Zero(state.size());
  }

  Eigen::VectorXd grad = Eigen::VectorXd::Zero(state.size());

  // ✅ 方案B: 使用costmap梯度（数值梯度）
  if (hasCostmap()) {
    double epsilon = 0.05;  // 5cm步长

    // 计算数值梯度
    double f_x_plus = getCostmapValue(state[0] + epsilon, state[1]);
    double f_x_minus = getCostmapValue(state[0] - epsilon, state[1]);
    double f_y_plus = getCostmapValue(state[0], state[1] + epsilon);
    double f_y_minus = getCostmapValue(state[0], state[1] - epsilon);

    // ∇h(x) = -∇(costmap/254) = -(1/254)∇costmap
    grad[0] = -(f_x_plus - f_x_minus) / (2.0 * epsilon * 254.0);
    grad[1] = -(f_y_plus - f_y_minus) / (2.0 * epsilon * 254.0);

    // ✅ FIX: 如果costmap梯度太小（自由空间），使用状态边界约束的梯度
    double grad_norm = std::sqrt(grad[0]*grad[0] + grad[1]*grad[1]);
    if (grad_norm < 1e-6) {
      // 使用状态边界约束的梯度作为fallback
      double x_max = 10.0;
      double y_max = 10.0;

      // ∇h(x) = [-2x/x_max², -2y/y_max²]
      grad[0] = -2.0 * state[0] / (x_max * x_max);
      grad[1] = -2.0 * state[1] / (y_max * y_max);

      // ✅ DEBUG: 只打印前几次
      static int debug_count = 0;
      if (debug_count < 3) {
        std::cout << "[DEBUG] gradient fallback (costmap gradient ~0):" << std::endl;
        std::cout << "  position: [" << state[0] << ", " << state[1] << "]" << std::endl;
        std::cout << "  fallback gradient: [" << grad[0] << ", " << grad[1] << "]" << std::endl;
        debug_count++;
      }
    }

    return grad;
  }

  // Fallback: 使用状态边界约束的梯度
  if (obstacle_positions_.empty()) {
    double x_max = 10.0;
    double y_max = 10.0;

    // ∇h(x) = [-2x/x_max², -2y/y_max²]
    grad[0] = -2.0 * state[0] / (x_max * x_max);
    grad[1] = -2.0 * state[1] / (y_max * y_max);

    return grad;
  }

  // 使用障碍物列表的梯度
  Eigen::Vector2d position(state[0], state[1]);
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

// ✅ 方案B: Costmap支持实现
void NavigationSafetyFunction::setCostmap(
    const nav_msgs::OccupancyGrid::ConstPtr& costmap) {
  costmap_ = costmap;
}

double NavigationSafetyFunction::getCostmapValue(double x, double y) const {
  if (!costmap_) {
    return 0.0;  // 没有costmap数据时返回安全值（自由空间）
  }

  // 将世界坐标转换为costmap坐标
  double resolution = costmap_->info.resolution;
  double origin_x = costmap_->info.origin.position.x;
  double origin_y = costmap_->info.origin.position.y;

  int map_x = static_cast<int>((x - origin_x) / resolution);
  int map_y = static_cast<int>((y - origin_y) / resolution);

  // 检查边界
  if (map_x < 0 || map_x >= static_cast<int>(costmap_->info.width) ||
      map_y < 0 || map_y >= static_cast<int>(costmap_->info.height)) {
    return 50.0;  // 超出costmap范围，返回未知值
  }

  int index = map_y * costmap_->info.width + map_x;
  int8_t cost_value = costmap_->data[index];

  // costmap: -1=未知, 0=自由, 1-254=障碍物概率, 254=障碍物
  if (cost_value == -1) {
    return 50.0;  // 未知区域
  }

  return static_cast<double>(cost_value);
}

bool NavigationSafetyFunction::hasCostmap() const {
  return costmap_ != nullptr;
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
  return residual_collector_->getWindowSize() >= params_.min_residuals_for_update;
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

// ✅ 方案B: Costmap回调
void DRTighteningNode::costmapCallback(
  const nav_msgs::OccupancyGrid::ConstPtr& msg) {
  // 更新safety_function的costmap数据
  safety_function_->setCostmap(msg);

  static int log_count = 0;
  if (log_count++ < 3) {  // 只打印3次
    ROS_INFO("📍 [COSTMAP] Received costmap: %d x %d, resolution: %.3f m",
             msg->info.width, msg->info.height, msg->info.resolution);
    ROS_INFO("   Origin: (%.2f, %.2f)",
             msg->info.origin.position.x, msg->info.origin.position.y);
  }
}

void DRTighteningNode::updateCallback(const ros::TimerEvent& event) {
  if (!have_state_ || !have_cmd_vel_) {
    ROS_WARN_ONCE("Waiting for state and command velocity");
    return;
  }

  // Check if we have enough residuals
  if (residual_collector_->getWindowSize() < params_.min_residuals_for_update) {
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

      // Publish debug marker for RViz display
      publishDebugMarker(margins, risk_allocations);
    }

    // Log to CSV
    if (params_.log_to_csv) {
      logToCSV(margins, risk_allocations);
    }

    data_count_++;
  }

  // Publish statistics
  publishStatistics();

  // P1-1: Compute and publish terminal set (at lower frequency)
  if (enable_terminal_set_) {
    ros::Time now = ros::Time::now();
    double time_since_last_update = (now - last_terminal_set_update_).toSec();

    // Update at specified frequency (default 0.2 Hz = every 5 seconds)
    if (time_since_last_update >= (1.0 / terminal_set_update_frequency_)) {
      computeTerminalSet();
    }
  }
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

void DRTighteningNode::publishDebugMarker(
  const std::vector<double>& margins,
  const std::vector<double>& risk_allocations) {

  // Create a marker to visualize DR margins in RViz
  visualization_msgs::Marker marker;

  marker.header.frame_id = "map";
  marker.header.stamp = ros::Time::now();
  marker.ns = "dr_tightening";
  marker.id = 0;
  marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
  marker.action = visualization_msgs::Marker::ADD;

  // Position marker at robot location
  marker.pose.position.x = current_state_[0];
  marker.pose.position.y = current_state_[1];
  marker.pose.position.z = 2.0;  // Above the robot
  marker.pose.orientation.w = 1.0;

  // Marker appearance
  marker.scale.z = 0.3;  // Text height
  marker.color.r = 1.0;
  marker.color.g = 0.5;
  marker.color.b = 0.0;
  marker.color.a = 1.0;

  // Create text showing margin info
  std::ostringstream text;
  text << "DR Margins:\n";
  if (margins.size() > 0) {
    text << "Step 0: " << std::fixed << std::setprecision(3) << margins[0];
    if (margins.size() > 1) {
      text << "\nStep N: " << margins[margins.size()-1];
    }
  }
  if (risk_allocations.size() > 0) {
    text << "\nRisk: " << std::fixed << std::setprecision(3) << risk_allocations[0];
  }
  marker.text = text.str();

  // Publish marker
  pub_margins_debug_marker_.publish(marker);
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
  private_nh_.param("min_residuals_for_update", params_.min_residuals_for_update, 50);  // ✅ 添加：读取最小残差参数
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

  // P1-1: Create terminal set calculator
  terminal_set_calculator_ = std::make_unique<TerminalSetCalculator>();

  // Setup system dynamics for terminal set (2D unicycle model)
  TerminalSetCalculator::SystemDynamics dynamics;
  dynamics.state_dim = 2;
  dynamics.input_dim = 2;
  dynamics.dist_dim = 2;

  // Simple 2D unicycle model: x+ = x + v*cos(θ)*dt, y+ = y + v*sin(θ)*dt
  dynamics.A = Eigen::MatrixXd::Identity(2, 2);  // Simplified
  dynamics.B = Eigen::MatrixXd::Identity(2, 2);
  dynamics.G = Eigen::MatrixXd::Identity(2, 2) * 0.1;  // Disturbance scaling

  terminal_set_calculator_->setSystemDynamics(dynamics);

  // Setup constraints
  TerminalSetCalculator::ConstraintParams constraints;
  int state_dim = 2;
  constraints.x_min = Eigen::VectorXd::Zero(state_dim);
  constraints.x_max = Eigen::VectorXd::Ones(state_dim) * 10.0;  // 10m workspace
  constraints.u_min = Eigen::VectorXd::Zero(2);
  constraints.u_max = Eigen::VectorXd::Ones(2) * 0.5;  // 0.5 m/s, 0.5 rad/s
  constraints.w_bound = Eigen::VectorXd::Ones(2) * 0.1;  // Disturbance bound
  constraints.safety_margin = 0.5;  // Additional safety margin

  terminal_set_calculator_->setConstraints(constraints);

  // Setup terminal set parameters
  TerminalSetCalculator::TerminalSetParams terminal_params;
  terminal_params.max_iterations = 50;
  terminal_params.use_dr_tightening = true;
  terminal_params.risk_delta = params_.total_risk_delta;

  terminal_set_calculator_->setTerminalParams(terminal_params);

  // Get terminal set enable parameter
  private_nh_.param("enable_terminal_set", enable_terminal_set_, false);
  private_nh_.param("terminal_set_update_frequency", terminal_set_update_frequency_, 0.2);

  terminal_set_computed_ = false;

  ROS_INFO("P1-1: Terminal Set Calculator initialized");
  ROS_INFO("  Enable terminal set: %s", enable_terminal_set_ ? "YES" : "NO");
  ROS_INFO("  Update frequency: %.2f Hz", terminal_set_update_frequency_);

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

  // ✅ 方案B: 订阅costmap
  sub_costmap_ = nh_.subscribe(
    "/move_base/global_costmap/costmap", 1,
    &DRTighteningNode::costmapCallback, this);

  // Setup publishers
  pub_margins_ = nh_.advertise<std_msgs::Float64MultiArray>(
    "/dr_margins", 1);

  pub_margins_debug_ = nh_.advertise<std_msgs::Float64MultiArray>(
    "/dr_margins_debug", 1);

  pub_margins_debug_marker_ = nh_.advertise<visualization_msgs::Marker>(
    "/dr_margins_debug_viz", 1);

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

// ============================================================================
// P1-1: Terminal Set Computation (Theorem 4.5 - Recursive Feasibility)
// ============================================================================

bool DRTighteningNode::computeTerminalSet() {
  /**
   * Compute terminal set for recursive feasibility (Theorem 4.5)
   *
   * The terminal set 𝒳_f must be DR control-invariant:
   * - For all x ∈ 𝒳_f, there exists u such that f(x,u,w) ∈ 𝒳_f for all w ∈ 𝒲
   * - 𝒳_f satisfies tightened constraints: h(z) ≥ σ + η_ℰ
   */

  if (!enable_terminal_set_ || !terminal_set_calculator_) {
    return false;
  }

  ROS_INFO("Computing terminal set for recursive feasibility...");

  // Get current DR margin from tightening computer
  double current_dr_margin = 0.0;

  // Compute terminal set with DR tightening
  TerminalSetCalculator::TerminalSet terminal_set =
    terminal_set_calculator_->computeTerminalSet(current_dr_margin);

  if (terminal_set.is_empty) {
    ROS_ERROR("Failed to compute terminal set (empty set)");
    return false;
  }

  // Verify recursive feasibility
  bool recursive_feasible = terminal_set_calculator_->verifyRecursiveFeasibility(terminal_set);

  if (recursive_feasible) {
    ROS_INFO("✓ Recursive feasibility verified (Theorem 4.5)");
    terminal_set_computed_ = true;
    last_terminal_set_update_ = ros::Time::now();

    // Log terminal set bounds
    auto bounds = terminal_set_calculator_->getBounds(terminal_set);
    ROS_INFO("Terminal set bounds:");
    for (size_t i = 0; i < bounds.size(); ++i) {
      ROS_INFO("  Dimension %zu: [%.2f, %.2f]",
               i, bounds[i].first, bounds[i].second);
    }

    // Publish terminal set
    publishTerminalSet();

  } else {
    ROS_WARN("⚠ Terminal set fails recursive feasibility check");
    ROS_WARN("  This may affect long-term stability");
    ROS_WARN("  Consider reducing workspace or increasing safety margins");
  }

  return recursive_feasible;
}

void DRTighteningNode::publishTerminalSet() {
  /**
   * Publish terminal set for MPC integration
   *
   * Message format: [x_center, y_center, theta_center, v_center, cte_center, etheta_center, radius]
   */

  if (!terminal_set_computed_ || !terminal_set_calculator_) {
    return;
  }

  // Get current terminal set (recompute if needed)
  double current_dr_margin = 0.0;  // Would get from current margins
  auto terminal_set = terminal_set_calculator_->computeTerminalSet(current_dr_margin);

  // Publish in format expected by TubeMPCNode
  // Format: [x_center, y_center, theta_center, v_center, cte_center, etheta_center, radius]
  std_msgs::Float64MultiArray terminal_msg;

  // Terminal set center (6D state)
  for (int i = 0; i < terminal_set.center.size(); ++i) {
    terminal_msg.data.push_back(terminal_set.center(i));
  }

  // Terminal set radius (computed from bounds for simplicity)
  auto bounds = terminal_set_calculator_->getBounds(terminal_set);
  if (!bounds.empty()) {
    // Use average of x and y bounds as radius
    double x_range = bounds[0].second - bounds[0].first;
    double y_range = bounds[1].second - bounds[1].first;
    double radius = std::sqrt(x_range * x_range + y_range * y_range) / 2.0;
    terminal_msg.data.push_back(radius);
  } else {
    terminal_msg.data.push_back(1.0);  // Default radius
  }

  // Publish to topic (create new publisher if needed)
  static ros::Publisher pub_terminal_set =
    nh_.advertise<std_msgs::Float64MultiArray>("/dr_terminal_set", 1, true);

  pub_terminal_set.publish(terminal_msg);

  ROS_DEBUG("Published terminal set: center=[%.2f,%.2f,%.2f], radius=%.2f",
            terminal_set.center(0), terminal_set.center(1), terminal_set.center(2),
            terminal_msg.data[6]);
}

bool DRTighteningNode::verifyRecursiveFeasibility() {
  /**
   * Verify recursive feasibility condition (Theorem 4.5)
   *
   * Checks if the current DR tightening allows for long-term feasible operation
   */

  if (!enable_terminal_set_ || !terminal_set_computed_) {
    ROS_WARN("Terminal set not computed, cannot verify recursive feasibility");
    return false;
  }

  // Recompute terminal set and verify
  double current_dr_margin = 0.0;
  auto terminal_set = terminal_set_calculator_->computeTerminalSet(current_dr_margin);

  bool feasible = terminal_set_calculator_->verifyRecursiveFeasibility(terminal_set);

  static int verify_count = 0;
  if (verify_count++ % 10 == 0) {  // Log every 10th check
    ROS_INFO("Recursive feasibility check #%d: %s",
             verify_count, feasible ? "PASS" : "FAIL");
  }

  return feasible;
}

} // namespace dr_tightening
