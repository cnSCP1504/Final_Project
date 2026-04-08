/**
 * @file dr_tightening_node.hpp
 * @brief ROS node for distributionally robust chance constraint tightening
 *
 * This node collects residuals from Tube MPC, computes DR margins online,
 * and publishes constraint tightening for the MPC optimizer.
 *
 * Subscribes to:
 *   - /tube_mpc/tracking_error (tracking error/residuals)
 *   - /mpc_trajectory (MPC predicted trajectory)
 *   - /odom (robot odometry)
 *
 * Publishes to:
 *   - /dr_margins (constraint tightening margins)
 *   - /dr_margins_debug (debug visualization)
 *   - /dr_statistics (statistics for monitoring)
 */

#ifndef DR_TIGHTENING_DR_TIGHTENING_NODE_HPP
#define DR_TIGHTENING_DR_TIGHTENING_NODE_HPP

#include <ros/ros.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Float64.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TwistStamped.h>
#include <nav_msgs/Path.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <nav_msgs/Odometry.h>
#include <fstream>

#include "dr_tightening/ResidualCollector.hpp"
#include "dr_tightening/AmbiguityCalibrator.hpp"
#include "dr_tightening/TighteningComputer.hpp"
#include "dr_tightening/SafetyLinearization.hpp"
#include "dr_tightening/TerminalSetCalculator.hpp"

#include <Eigen/Dense>
#include <deque>
#include <string>
#include <map>

namespace dr_tightening {

/**
 * @brief Safety function for navigation (obstacle avoidance)
 */
class NavigationSafetyFunction {
public:
  /**
   * @brief Constructor
   * @param obstacle_positions List of obstacle positions (x, y)
   * @param safety_buffer Safety buffer distance around obstacles
   */
  NavigationSafetyFunction(
    const std::vector<Eigen::Vector2d>& obstacle_positions,
    double safety_buffer = 1.0);

  /**
   * @brief Evaluate safety function h(x)
   * @param state Robot state [x, y, theta]
   * @return h(x) = distance to nearest obstacle - buffer
   */
  double evaluate(const Eigen::VectorXd& state) const;

  /**
   * @brief Compute gradient ∇h(x)
   * @param state Robot state
   * @return Gradient vector
   */
  Eigen::VectorXd gradient(const Eigen::VectorXd& state) const;

  /**
   * @brief Estimate Lipschitz constant
   * @param center Center position
   * @param radius Search radius
   * @return Estimated Lipschitz constant
   */
  double estimateLipschitz(const Eigen::Vector2d& center, double radius) const;

  /**
   * @brief Add obstacle
   * @param position Obstacle position (x, y)
   */
  void addObstacle(const Eigen::Vector2d& position);

  /**
   * @brief Clear all obstacles
   */
  void clearObstacles();

  /**
   * @brief Get number of obstacles
   * @return Number of obstacles
   */
  size_t getObstacleCount() const;

private:
  std::vector<Eigen::Vector2d> obstacle_positions_;
  double safety_buffer_;

  /**
   * @brief Find nearest obstacle distance
   * @param position Query position
   * @return Distance to nearest obstacle
   */
  double findNearestObstacleDistance(const Eigen::Vector2d& position) const;
};

/**
 * @brief Main ROS node for DR tightening
 */
class DRTighteningNode {
public:
  /**
   * @brief Constructor
   * @param nh Node handle
   * @param private_nh Private node handle for parameters
   */
  DRTighteningNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh);

  /**
   * @brief Destructor
   */
  ~DRTighteningNode();

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
   * @brief Get node status
   * @return True if node is healthy
   */
  bool isHealthy() const;

private:
  // ROS interfaces
  ros::NodeHandle nh_;
  ros::NodeHandle private_nh_;

  // Subscribers
  ros::Subscriber sub_tracking_error_;
  ros::Subscriber sub_mpc_trajectory_;
  ros::Subscriber sub_odom_;
  ros::Subscriber sub_cmd_vel_;

  // Publishers
  ros::Publisher pub_margins_;
  ros::Publisher pub_margins_debug_;
  ros::Publisher pub_margins_debug_marker_;
  ros::Publisher pub_statistics_;
  ros::Publisher pub_viz_markers_;
  ros::Publisher pub_safety_function_;

  // Parameters
  struct Parameters {
    // Residual collection
    int residual_window_size;
    int residual_dimension;
    int min_residuals_for_update;  // ✅ 添加：最小残差要求

    // Risk parameters
    double total_risk_delta;
    int mpc_horizon;

    // Risk allocation strategy
    std::string risk_allocation_strategy;

    // Ambiguity calibration
    std::string ambiguity_strategy;
    double confidence_level;

    // Tube parameters
    double tube_radius;
    double lipschitz_constant;

    // Safety function
    double safety_buffer;
    double obstacle_influence_radius;

    // Performance
    double update_rate;
    bool enable_visualization;
    bool enable_debug_output;

    // File logging
    bool log_to_csv;
    std::string csv_output_path;

    // Parameters() : ResidualCollector(200),
    //   total_risk_delta(0.1),
    //   mpc_horizon(20),
    //   risk_allocation_strategy("uniform"),
    //   ambiguity_strategy("wasserstein_ball"),
    //   confidence_level(0.95),
    //   tube_radius(0.5),
    //   lipschitz_constant(1.0),
    //   safety_buffer(1.0),
    //   obstacle_influence_radius(5.0),
    //   update_rate(10.0),
    //   enable_visualization(true),
    //   enable_debug_output(false),
    //   log_to_csv(false),
    //   csv_output_path("/tmp/dr_margins.csv")
    // {}
  } params_;

  // Core modules
  std::unique_ptr<ResidualCollector> residual_collector_;
  std::unique_ptr<AmbiguityCalibrator> ambiguity_calibrator_;
  std::unique_ptr<TighteningComputer> tightening_computer_;
  std::unique_ptr<SafetyLinearization> safety_linearization_;
  std::unique_ptr<NavigationSafetyFunction> safety_function_;
  std::unique_ptr<TerminalSetCalculator> terminal_set_calculator_;  // P1-1: Terminal set

  // Terminal set (P1-1)
  bool enable_terminal_set_;
  bool terminal_set_computed_;
  double terminal_set_update_frequency_;
  ros::Time last_terminal_set_update_;

  // State tracking
  Eigen::VectorXd current_state_;
  geometry_msgs::TwistStamped current_cmd_vel_;
  bool have_state_;
  bool have_cmd_vel_;

  // Data logging
  std::ofstream csv_file_;
  int data_count_;

  // Timers
  ros::Timer update_timer_;
  ros::Time last_update_time_;

  // Callbacks
  void trackingErrorCallback(const std_msgs::Float64MultiArray::ConstPtr& msg);
  void mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg);
  void odomCallback(const nav_msgs::Odometry::ConstPtr& msg);
  void cmdVelCallback(const geometry_msgs::Twist::ConstPtr& msg);

  // Timer callback
  void updateCallback(const ros::TimerEvent& event);

  // Core computation
  bool computeDRTightening(
    const Eigen::VectorXd& nominal_state,
    const Eigen::VectorXd& gradient,
    double& margin_out);

  // P1-1: Terminal set computation
  bool computeTerminalSet();
  void publishTerminalSet();
  bool verifyRecursiveFeasibility();

  // Publishing
  void publishMargins(
    const std::vector<double>& margins,
    const std::vector<double>& risk_allocations);

  void publishDebugInfo(
    const std::vector<double>& margins,
    const std::vector<double>& risk_allocations,
    const Eigen::VectorXd& stats);

  void publishDebugMarker(
    const std::vector<double>& margins,
    const std::vector<double>& risk_allocations);

  void publishStatistics();

  void publishVisualization(
    const std::vector<double>& margins,
    const nav_msgs::Path::ConstPtr& trajectory);

  // Utility functions
  RiskAllocation parseRiskAllocation(const std::string& strategy) const;
  AmbiguityStrategy parseAmbiguityStrategy(const std::string& strategy) const;
  bool loadParameters();
  bool createConnections();
  void setupSafetyFunction();
  void openLogFile();

  // Helper functions
  std::vector<double> allocateRisk(double total_risk, int horizon) const;
  Eigen::VectorXd computeStatistics() const;
  void logToCSV(const std::vector<double>& margins, const std::vector<double>& risks);

  // Constants
  // static constexpr double MIN_RESIDUALS_FOR_UPDATE = 50;  // ❌ 删除：改用params_.min_residuals_for_update
  static constexpr double DEFAULT_LIPSCHITZ = 1.0;
  static constexpr double DEFAULT_TUBE_RADIUS = 0.5;
};

} // namespace dr_tightening

#endif // DR_TIGHTENING_DR_TIGHTENING_NODE_HPP
