#include "STL_ros/STLROSInterface.h"
#include <stl_ros/STLRobustness.h>
#include <stl_ros/STLBudget.h>
#include <sstream>
#include <vector>

namespace stl_ros {

STLROSInterface::STLROSInterface(ros::NodeHandle& nh, const STLMonitor::Config& config)
    : nh_(nh), private_nh_("~"), config_(config) {

    monitor_ = std::make_unique<STLMonitor>(config);

    control_frequency_ = 10.0;  // Default 10 Hz
    enable_visualization_ = true;
}

bool STLROSInterface::initialize() {
    try {
        // Get parameters
        private_nh_.param("control_frequency", control_frequency_, 10.0);
        private_nh_.param("enable_visualization", enable_visualization_, true);

        // Initialize subscribers
        belief_sub_ = nh_.subscribe("~belief", 1, &STLROSInterface::beliefCallback, this);
        mpc_trajectory_sub_ = nh_.subscribe("~mpc_trajectory", 1, &STLROSInterface::mpcTrajectoryCallback, this);
        robot_state_sub_ = nh_.subscribe("~robot_state", 1, &STLROSInterface::robotStateCallback, this);

        // Initialize publishers
        robustness_pub_ = nh_.advertise<stl_ros::STLRobustness>("~robustness", 10);
        budget_pub_ = nh_.advertise<stl_ros::STLBudget>("~budget", 10);
        formula_pub_ = nh_.advertise<stl_ros::STLFormula>("~formula", 10);

        if (enable_visualization_) {
            visualization_pub_ = nh_.advertise<visualization_msgs::Marker>("~visualization", 10);
        }

        // Initialize timer
        ros::Duration timer_period(1.0 / control_frequency_);
        evaluation_timer_ = nh_.createTimer(timer_period, &STLROSInterface::evaluationTimerCallback, this);

        // Initialize dynamic reconfigure
        reconfigure_server_ = std::make_unique<ReconfigureServer>(
            private_nh_,
            boost::bind(&STLROSInterface::reconfigureCallback, this, _1, _2));

        ROS_INFO("STL ROS Interface initialized");
        return true;

    } catch (const std::exception& e) {
        ROS_ERROR("Failed to initialize STL ROS Interface: %s", e.what());
        return false;
    }
}

void STLROSInterface::beliefCallback(const geometry_msgs::PoseStamped::ConstPtr& msg) {
    // Convert ROS message to belief state
    current_belief_ = convertToBelief(*msg);
}

void STLROSInterface::mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg) {
    // Convert ROS message to trajectory
    mpc_trajectory_ = convertToTrajectory(*msg);
}

void STLROSInterface::robotStateCallback(const sensor_msgs::JointState::ConstPtr& msg) {
    // Update robot state if needed
    // For now, this is a placeholder
}

void STLROSInterface::evaluationTimerCallback(const ros::TimerEvent& event) {
    // Check if we have data to evaluate
    if (mpc_trajectory_.empty()) {
        return;
    }

    try {
        // Evaluate STL formulas
        auto results = monitor_->evaluate(current_belief_, mpc_trajectory_);

        // Publish results for each formula
        for (const auto& pair : results) {
            const std::string& formula_name = pair.first;
            const MonitorResult& result = pair.second;

            // Publish robustness
            stl_ros::STLRobustness robustness_msg = createRobustnessMessage(formula_name, result);
            robustness_pub_.publish(robustness_msg);

            // Publish budget
            stl_ros::STLBudget budget_msg = createBudgetMessage();
            budget_pub_.publish(budget_msg);
        }

    } catch (const std::exception& e) {
        ROS_ERROR("Error in STL evaluation: %s", e.what());
    }
}

void STLROSInterface::reconfigureCallback(stl_ros::STLMonitorParamsConfig& config, uint32_t level) {
    // Update configuration
    config_.prediction_horizon = config.prediction_horizon;
    config_.dt = config.time_step;
    config_.tau = config.temperature_tau;
    config_.num_samples = config.num_monte_carlo_samples;
    config_.baseline_r = config.baseline_robustness_r;
    config_.lambda = config.stl_weight_lambda;

    ROS_INFO("STL Monitor parameters updated via dynamic reconfigure");
}

BeliefState STLROSInterface::convertToBelief(const geometry_msgs::PoseStamped& msg) const {
    BeliefState belief(config_.state_dim);

    // Extract position
    belief.mean(0) = msg.pose.position.x;
    belief.mean(1) = msg.pose.position.y;

    // Extract orientation (yaw)
    double yaw = tf::getYaw(msg.pose.orientation);
    belief.mean(2) = yaw;

    // Add velocity if available (would come from twist in practice)
    belief.mean(3) = 0.0;  // v
    belief.mean(4) = 0.0;  // w

    // Set covariance (simple diagonal for now)
    double position_cov = 0.1;
    double orientation_cov = 0.05;
    belief.cov = Eigen::MatrixXd::Identity(config_.state_dim, config_.state_dim);
    belief.cov(0, 0) = position_cov;
    belief.cov(1, 1) = position_cov;
    belief.cov(2, 2) = orientation_cov;

    return belief;
}

std::vector<Eigen::VectorXd> STLROSInterface::convertToTrajectory(const nav_msgs::Path& msg) const {
    std::vector<Eigen::VectorXd> trajectory;

    for (const auto& pose_stamped : msg.poses) {
        Eigen::VectorXd state(config_.state_dim);

        state(0) = pose_stamped.pose.position.x;
        state(1) = pose_stamped.pose.position.y;

        double yaw = tf::getYaw(pose_stamped.pose.orientation);
        state(2) = yaw;

        state(3) = 0.0;  // v (would need velocity info)
        state(4) = 0.0;  // w

        trajectory.push_back(state);
    }

    return trajectory;
}

stl_ros::STLRobustness STLROSInterface::createRobustnessMessage(
    const std::string& name,
    const MonitorResult& result) const {

    stl_ros::STLRobustness msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "map";

    msg.formula_name = name;
    msg.robustness = result.robustness;
    msg.expected_robustness = result.expected_robustness;
    msg.satisfied = result.satisfied;
    msg.violation_probability = result.violation_probability;
    msg.budget = result.budget;
    msg.computation_time_ms = result.computation_time_ms;

    // Copy per-timestep robustness
    msg.timestep_robustness = result.timestep_robustness;

    // Copy samples if in debug mode
    if (config_.num_samples > 0) {
        msg.sample_values = result.samples;
        msg.num_samples = result.samples.size();
    }

    return msg;
}

stl_ros::STLBudget STLROSInterface::createBudgetMessage() const {
    stl_ros::STLBudget msg;

    msg.header.stamp = ros::Time::now();
    msg.header.frame_id = "map";

    // Aggregate budget information
    double total_budget = 0.0;
    bool all_feasible = true;

    std::vector<std::string> formula_names;
    std::vector<double> budgets;
    std::vector<bool> feasible;

    for (const auto& pair : monitor_->getFormulas()) {
        const std::string& name = pair.first;
        try {
            const RobustnessBudget& budget = monitor_->getBudget(name);

            formula_names.push_back(name);
            budgets.push_back(budget.getBudget());
            feasible.push_back(budget.isFeasible());

            total_budget += budget.getBudget();
            if (!budget.isFeasible()) {
                all_feasible = false;
            }
        } catch (...) {
            // Formula may not have budget
        }
    }

    msg.current_budget = total_budget;
    msg.feasible = all_feasible;

    msg.formula_names = formula_names;
    msg.budgets = budgets;
    msg.formula_feasible = feasible;

    msg.update_frequency_hz = control_frequency_;

    return msg;
}

void STLROSInterface::shutdown() {
    evaluation_timer_.stop();
    robustness_pub_.shutdown();
    budget_pub_.shutdown();
    formula_pub_.shutdown();
    visualization_pub_.shutdown();

    belief_sub_.shutdown();
    mpc_trajectory_sub_.shutdown();
    robot_state_sub_.shutdown();

    ROS_INFO("STL ROS Interface shutdown complete");
}

} // namespace stl_ros
