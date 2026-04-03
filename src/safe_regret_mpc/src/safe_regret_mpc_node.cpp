#include "safe_regret_mpc/safe_regret_mpc_node.hpp"
#include <signal.h>

namespace safe_regret_mpc {

// ========== Constructor & Destructor ==========

SafeRegretMPCNode::SafeRegretMPCNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh)
    : nh_(nh),
      private_nh_(private_nh),
      stl_enabled_(false),
      stl_received_(false),
      dr_enabled_(false),
      dr_received_(false),
      terminal_set_enabled_(false),
      terminal_set_received_(false),
      system_ready_(false),
      odom_received_(false),
      plan_received_(false),
      tube_mpc_cmd_received_(false),
      tube_mpc_traj_received_(false),
      tube_mpc_error_received_(false),
      enable_integration_mode_(true)
{
    // Initialize publishers and subscribers in initialize()
}

SafeRegretMPCNode::~SafeRegretMPCNode() {
}

// ========== Initialization ==========

bool SafeRegretMPCNode::initialize() {
    ROS_INFO("Initializing Safe-Regret MPC Node...");

    // Load parameters
    loadParameters();

    // Create MPC solver
    mpc_solver_ = std::make_unique<SafeRegretMPC>();
    if (!mpc_solver_->initialize()) {
        ROS_ERROR("Failed to initialize MPC solver!");
        return false;
    }

    // Load and set MPC matrices and constraints
    if (!loadAndSetMPCParameters()) {
        ROS_ERROR("Failed to load and set MPC parameters!");
        return false;
    }

    // Subscribers
    sub_odom_ = nh_.subscribe("odom", 1, &SafeRegretMPCNode::odomCallback, this);
    sub_global_plan_ = nh_.subscribe("global_plan", 1,
                                     &SafeRegretMPCNode::globalPlanCallback, this);
    sub_goal_ = nh_.subscribe("goal", 1, &SafeRegretMPCNode::goalCallback, this);

    if (stl_enabled_) {
        sub_stl_robustness_ = nh_.subscribe("stl_robustness", 1,
                                            &SafeRegretMPCNode::stlRobustnessCallback, this);
    }

    if (dr_enabled_) {
        // Subscribe to raw Float64MultiArray from dr_tightening node
        sub_dr_margins_raw_ = nh_.subscribe("dr_margins", 1,
                                            &SafeRegretMPCNode::drMarginsRawCallback, this);
        // Also subscribe to DRMargins if available (for compatibility)
        sub_dr_margins_ = nh_.subscribe("dr_margins_alt", 1,
                                       &SafeRegretMPCNode::drMarginsCallback, this);
        sub_terminal_set_ = nh_.subscribe("terminal_set", 1,
                                         &SafeRegretMPCNode::terminalSetCallback, this);
    }

    // Tube MPC data subscribers (integration mode)
    sub_tube_mpc_cmd_ = nh_.subscribe("/cmd_vel_raw", 1,
                                      &SafeRegretMPCNode::tubeMPCCmdCallback, this);
    sub_tube_mpc_traj_ = nh_.subscribe("/mpc_trajectory", 1,
                                       &SafeRegretMPCNode::tubeMPCTrajectoryCallback, this);
    sub_tube_mpc_error_ = nh_.subscribe("/tube_mpc/tracking_error", 1,
                                        &SafeRegretMPCNode::tubeMPCErrorCallback, this);

    // Publishers
    pub_cmd_vel_final_ = nh_.advertise<geometry_msgs::Twist>("cmd_vel", 1);  // Final command (processed)
    pub_system_state_ = nh_.advertise<safe_regret_mpc::SafeRegretState>("system_state", 1);
    pub_metrics_ = nh_.advertise<safe_regret_mpc::SafeRegretMetrics>("metrics", 1);
    pub_mpc_trajectory_ = nh_.advertise<nav_msgs::Path>("mpc_trajectory", 1);
    pub_tube_boundaries_ = nh_.advertise<nav_msgs::Path>("tube_boundaries", 1);

    // Services
    srv_set_stl_ = nh_.advertiseService("set_stl_specification",
                                        &SafeRegretMPCNode::setSTLSpecificationCallback, this);
    srv_get_status_ = nh_.advertiseService("get_system_status",
                                          &SafeRegretMPCNode::getSystemStatusCallback, this);
    srv_reset_regret_ = nh_.advertiseService("reset_regret_tracker",
                                             &SafeRegretMPCNode::resetRegretTrackerCallback, this);

    // Timers
    control_timer_ = nh_.createTimer(ros::Duration(1.0 / controller_frequency_),
                                     &SafeRegretMPCNode::controlTimerCallback, this);
    publish_timer_ = nh_.createTimer(ros::Duration(1.0 / publish_frequency_),
                                     &SafeRegretMPCNode::publishTimerCallback, this);

    ROS_INFO("Safe-Regret MPC Node initialized successfully!");
    ROS_INFO("  Mode: %s", controller_mode_.c_str());
    ROS_INFO("  STL enabled: %s", stl_enabled_ ? "yes" : "no");
    ROS_INFO("  DR enabled: %s", dr_enabled_ ? "yes" : "no");
    ROS_INFO("  Terminal set enabled: %s", terminal_set_enabled_ ? "yes" : "no");

    return true;
}

// ========== Load Parameters ==========

void SafeRegretMPCNode::loadParameters() {
    // System parameters
    private_nh_.param("controller_mode", controller_mode_, std::string("SAFE_REGRET_MPC"));
    private_nh_.param("controller_frequency", controller_frequency_, 20.0);
    private_nh_.param("publish_frequency", publish_frequency_, 10.0);
    private_nh_.param("debug_mode", debug_mode_, false);

    // STL parameters
    private_nh_.param("enable_stl_constraints", stl_enabled_, false);

    // DR parameters
    private_nh_.param("enable_dr_constraints", dr_enabled_, false);

    // Terminal set parameters
    private_nh_.param("enable_terminal_set", terminal_set_enabled_, false);

    // Integration mode parameters
    private_nh_.param("enable_integration_mode", enable_integration_mode_, true);

    // MPC parameters
    private_nh_.param("mpc_steps", mpc_steps_, 20);
    private_nh_.param("mpc_ref_vel", ref_vel_, 0.5);

    // Frames
    private_nh_.param<std::string>("global_frame", global_frame_, "map");
    private_nh_.param<std::string>("robot_base_frame", robot_base_frame_, "base_link");

    private_nh_.param("enable_visualization", enable_visualization_, true);
}

bool SafeRegretMPCNode::loadAndSetMPCParameters() {
    ROS_INFO("Loading MPC parameters...");

    // Load system dynamics matrices
    Eigen::MatrixXd A(6, 6), B(6, 2), G(6, 2);
    std::vector<double> A_vec, B_vec, G_vec;

    if (nh_.getParam("dynamics/A", A_vec) && A_vec.size() == 36) {
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                A(i, j) = A_vec[i * 6 + j];
            }
        }
    } else {
        ROS_WARN("Failed to load A matrix, using identity");
        A = Eigen::MatrixXd::Identity(6, 6);
    }

    if (nh_.getParam("dynamics/B", B_vec) && B_vec.size() == 12) {
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 2; ++j) {
                B(i, j) = B_vec[i * 2 + j];
            }
        }
    } else {
        ROS_WARN("Failed to load B matrix, using zeros");
        B = Eigen::MatrixXd::Zero(6, 2);
    }

    if (nh_.getParam("dynamics/G", G_vec) && G_vec.size() == 12) {
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 2; ++j) {
                G(i, j) = G_vec[i * 2 + j];
            }
        }
    } else {
        ROS_WARN("Failed to load G matrix, using zeros");
        G = Eigen::MatrixXd::Zero(6, 2);
    }

    mpc_solver_->setSystemDynamics(A, B, G);

    // Load cost matrices
    Eigen::MatrixXd Q(6, 6), R(2, 2);
    std::vector<double> Q_vec, R_vec;

    if (nh_.getParam("cost/Q", Q_vec) && Q_vec.size() == 6) {
        Q = Eigen::VectorXd::Map(Q_vec.data(), 6).asDiagonal();
    } else {
        ROS_WARN("Failed to load Q matrix, using identity");
        Q = Eigen::MatrixXd::Identity(6, 6);
    }

    if (nh_.getParam("cost/R", R_vec) && R_vec.size() == 2) {
        R = Eigen::VectorXd::Map(R_vec.data(), 2).asDiagonal();
    } else {
        ROS_WARN("Failed to load R matrix, using identity");
        R = Eigen::MatrixXd::Identity(2, 2);
    }

    mpc_solver_->setCostWeights(Q, R);

    // Load constraints
    Eigen::VectorXd x_min(6), x_max(6), u_min(2), u_max(2);
    std::vector<double> x_min_vec, x_max_vec, u_min_vec, u_max_vec;

    if (nh_.getParam("constraints/x_min", x_min_vec) && x_min_vec.size() == 6) {
        x_min = Eigen::VectorXd::Map(x_min_vec.data(), 6);
    } else {
        ROS_WARN("Failed to load x_min, using default");
        x_min << -10.0, -10.0, -3.14159, 0.0, -1.0, -0.5;
    }

    if (nh_.getParam("constraints/x_max", x_max_vec) && x_max_vec.size() == 6) {
        x_max = Eigen::VectorXd::Map(x_max_vec.data(), 6);
    } else {
        ROS_WARN("Failed to load x_max, using default");
        x_max << 10.0, 10.0, 3.14159, 1.0, 1.0, 0.5;
    }

    if (nh_.getParam("constraints/u_min", u_min_vec) && u_min_vec.size() == 2) {
        u_min = Eigen::VectorXd::Map(u_min_vec.data(), 2);
    } else {
        ROS_WARN("Failed to load u_min, using default");
        u_min << 0.0, -1.5;
    }

    if (nh_.getParam("constraints/u_max", u_max_vec) && u_max_vec.size() == 2) {
        u_max = Eigen::VectorXd::Map(u_max_vec.data(), 2);
    } else {
        ROS_WARN("Failed to load u_max, using default");
        u_max << 1.0, 1.5;
    }

    mpc_solver_->setConstraints(x_min, x_max, u_min, u_max);

    // Set horizon
    mpc_solver_->setHorizon(mpc_steps_);

    // Enable/disable features based on parameters
    mpc_solver_->enableSTLConstraints(stl_enabled_);
    mpc_solver_->enableDRConstraints(dr_enabled_);
    mpc_solver_->enableTerminalSet(terminal_set_enabled_);

    ROS_INFO("MPC parameters loaded successfully");
    return true;
}

// ========== Callbacks ==========

void SafeRegretMPCNode::odomCallback(const nav_msgs::Odometry::ConstPtr& msg) {
    current_odom_ = *msg;
    odom_received_ = true;
    checkSystemReady();
}

void SafeRegretMPCNode::globalPlanCallback(const nav_msgs::Path::ConstPtr& msg) {
    global_plan_ = *msg;
    plan_received_ = true;
    updateReferenceTrajectory();
    checkSystemReady();
}

void SafeRegretMPCNode::goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg) {
    current_goal_ = *msg;
    ROS_INFO("Received goal: (%.2f, %.2f)",
             msg->pose.position.x, msg->pose.position.y);
}

void SafeRegretMPCNode::stlRobustnessCallback(
    const safe_regret_mpc::STLRobustness::ConstPtr& msg) {
    stl_info_ = *msg;
    stl_received_ = true;

    if (debug_mode_) {
        ROS_DEBUG("STL robustness: %.3f, budget: %.3f",
                  msg->robustness_value, msg->budget);
    }
}

void SafeRegretMPCNode::drMarginsRawCallback(
    const std_msgs::Float64MultiArray::ConstPtr& msg) {
    // Convert Float64MultiArray to DRMargins message
    safe_regret_mpc::DRMargins dr_msg;
    dr_msg.header.stamp = ros::Time::now();

    // Fill margins
    dr_msg.margins = msg->data;

    // Fill other fields with default values
    dr_msg.means.clear();
    dr_msg.stds.clear();
    dr_msg.cantelli_factors.clear();
    for (size_t i = 0; i < msg->data.size(); ++i) {
        dr_msg.means.push_back(0.0);
        dr_msg.stds.push_back(msg->data[i]);
        dr_msg.cantelli_factors.push_back(1.0);
    }

    // Risk allocation
    dr_msg.total_risk = 0.1;
    dr_msg.risk_allocation.clear();
    dr_msg.allocation_method = "uniform";

    // Ambiguity set
    dr_msg.ambiguity_type = "wasserstein";
    dr_msg.ambiguity_radius = 0.1;
    dr_msg.sample_count = 200;

    // Residual statistics
    dr_msg.residual_mean.clear();
    dr_msg.residual_std.clear();
    dr_msg.residual_bounds.clear();
    for (int i = 0; i < 3; ++i) {
        dr_msg.residual_mean.push_back(0.0);
        dr_msg.residual_std.push_back(0.1);
        dr_msg.residual_bounds.push_back(0.5);
    }

    // Tube compensation
    dr_msg.tube_radius = 0.5;
    dr_msg.tube_offset = 0.1;
    dr_msg.tube_compensation = 0.1;

    // Confidence
    dr_msg.confidence_level = 0.9;
    dr_msg.safety_margin = 1.0;

    // Computational info
    dr_msg.computation_time = 1.0;
    dr_msg.iterations = 1;
    dr_msg.converged = true;

    dr_info_ = dr_msg;
    dr_received_ = true;

    if (debug_mode_) {
        ROS_DEBUG("DR margins (raw) received, count: %lu", msg->data.size());
    }
}

void SafeRegretMPCNode::drMarginsCallback(
    const safe_regret_mpc::DRMargins::ConstPtr& msg) {
    dr_info_ = *msg;
    dr_received_ = true;

    if (debug_mode_) {
        ROS_DEBUG("DR margins received, count: %lu", msg->margins.size());
    }
}

void SafeRegretMPCNode::terminalSetCallback(
    const safe_regret_mpc::TerminalSet::ConstPtr& msg) {
    terminal_info_ = *msg;
    terminal_set_received_ = true;

    // Update MPC with terminal set
    Eigen::VectorXd center(6);
    for (int i = 0; i < 6; ++i) {
        center(i) = msg->center[i];
    }
    mpc_solver_->setTerminalSet(center, msg->radius);

    if (debug_mode_) {
        ROS_DEBUG("Terminal set received: radius=%.3f", msg->radius);
    }
}

// ========== Tube MPC Data Callbacks (Integration Mode) ==========

void SafeRegretMPCNode::tubeMPCCmdCallback(const geometry_msgs::Twist::ConstPtr& msg) {
    tube_mpc_cmd_raw_ = *msg;
    tube_mpc_cmd_received_ = true;

    // Process the raw command through STL/DR modules
    if (enable_integration_mode_) {
        processTubeMPCCommand();
    } else {
        // Direct forwarding (no processing)
        publishFinalCommand(tube_mpc_cmd_raw_);
    }

    if (debug_mode_) {
        ROS_DEBUG("Tube MPC raw command: v=%.3f, w=%.3f",
                  msg->linear.x, msg->angular.z);
    }
}

void SafeRegretMPCNode::tubeMPCTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg) {
    tube_mpc_trajectory_ = *msg;
    tube_mpc_traj_received_ = true;

    // Forward trajectory for visualization
    if (enable_visualization_) {
        pub_mpc_trajectory_.publish(tube_mpc_trajectory_);
    }

    // Forward trajectory to STL module if enabled
    // TODO: Implement STL module integration

    if (debug_mode_) {
        ROS_DEBUG("Tube MPC trajectory received: %zu poses",
                  msg->poses.size());
    }
}

void SafeRegretMPCNode::tubeMPCErrorCallback(const std_msgs::Float64MultiArray::ConstPtr& msg) {
    tube_mpc_error_ = *msg;
    tube_mpc_error_received_ = true;

    // Forward error to DR module if enabled
    if (dr_enabled_ && dr_received_) {
        // DR module will use this for constraint tightening
        // TODO: Implement DR module forwarding
    }

    if (debug_mode_) {
        ROS_DEBUG("Tube MPC tracking error received: %zu values",
                  msg->data.size());
    }
}

// ========== Timer Callbacks ==========

void SafeRegretMPCNode::controlTimerCallback(const ros::TimerEvent& event) {
    // Check if we have all required data
    if (!odom_received_ || !plan_received_) {
        return;
    }

    // CRITICAL FIX: In integration mode with DR/STL enabled,
    // we should solve our own MPC to properly incorporate DR constraints and STL robustness
    // This aligns with the manuscript's theoretical framework

    if (enable_integration_mode_) {
        // In integration mode with DR or STL enabled, solve Safe-Regret MPC
        if ((dr_enabled_ && dr_received_) || (stl_enabled_ && stl_received_)) {
            if (debug_mode_) {
                ROS_DEBUG_THROTTLE(1.0, "Solving Safe-Regret MPC with DR/STL constraints");
            }

            // Update reference trajectory from global plan
            updateReferenceTrajectory();

            // Solve Safe-Regret MPC (includes DR constraints and STL robustness in optimization)
            solveMPC();

            // Publish the command computed by Safe-Regret MPC
            publishFinalCommand(cmd_vel_);
        } else {
            // Without DR/STL, just forward tube_mpc command
            if (tube_mpc_cmd_received_) {
                publishFinalCommand(tube_mpc_cmd_raw_);
            }
        }
    } else {
        // Standalone mode: solve MPC independently
        solveMPC();
        publishFinalCommand(cmd_vel_);
    }
}

void SafeRegretMPCNode::publishTimerCallback(const ros::TimerEvent& event) {
    publishSystemState();
    publishVisualizations();
}

// ========== Internal Methods ==========

void SafeRegretMPCNode::solveMPC() {
    // Get current state from odometry
    Eigen::VectorXd current_state(6);
    current_state << current_odom_.pose.pose.position.x,
                     current_odom_.pose.pose.position.y,
                     tf::getYaw(current_odom_.pose.pose.orientation),
                     current_odom_.twist.twist.linear.x,
                     0.0,  // cte
                     0.0;  // etheta

    // CRITICAL FIX: Update DR margins before solving
    if (dr_enabled_ && dr_received_) {
        // Convert DRMargins message to vector<double>
        std::vector<double> dr_margins = dr_info_.margins;

        // Create dummy residuals (in real implementation, these would come from tracking error)
        std::vector<Eigen::VectorXd> residuals;
        // For now, just pass empty residuals - margins are already computed by dr_tightening node
        residuals.clear();

        // Update DR margins in MPC solver
        mpc_solver_->updateDRMargins(residuals);

        if (debug_mode_) {
            ROS_DEBUG("Updated DR margins: %zu margins", dr_margins.size());
        }
    }

    // CRITICAL FIX: Update STL robustness before solving
    if (stl_enabled_ && stl_received_) {
        // Create belief distribution (simplified - in real implementation would use actual covariance)
        Eigen::VectorXd belief_mean = current_state;
        Eigen::MatrixXd belief_cov = Eigen::MatrixXd::Zero(6, 6);
        belief_cov(0, 0) = 0.1;  // x variance
        belief_cov(1, 1) = 0.1;  // y variance
        belief_cov(2, 2) = 0.01; // theta variance

        // Update STL robustness in MPC solver
        mpc_solver_->updateSTLRobustness(belief_mean, belief_cov);

        if (debug_mode_) {
            ROS_DEBUG("Updated STL robustness: %.3f, budget: %.3f",
                     mpc_solver_->getSTLRobustness(),
                     mpc_solver_->getSTLBudget());
        }
    }

    // Solve MPC
    bool success = mpc_solver_->solve(current_state, reference_trajectory_);

    if (success) {
        // Get optimal control
        Eigen::VectorXd control = mpc_solver_->getOptimalControl();
        cmd_vel_.linear.x = control(0);
        cmd_vel_.angular.z = control(1);

        if (debug_mode_) {
            ROS_DEBUG("MPC solved: v=%.3f, w=%.3f", cmd_vel_.linear.x, cmd_vel_.angular.z);
        }
    } else {
        ROS_WARN("MPC solve failed! Publishing zero velocity...");
        cmd_vel_.linear.x = 0.0;
        cmd_vel_.angular.z = 0.0;
    }
}

void SafeRegretMPCNode::publishControl() {
    // Deprecated: Use publishFinalCommand() instead
    // In integration mode, command is published via publishFinalCommand()
    if (!enable_integration_mode_) {
        pub_cmd_vel_final_.publish(cmd_vel_);
    }
}

void SafeRegretMPCNode::publishSystemState() {
    safe_regret_mpc::SafeRegretState msg;

    msg.header.stamp = ros::Time::now();
    msg.mode = getSystemMode();
    msg.system_ready = system_ready_;
    msg.mpc_feasible = mpc_solver_->isFeasible();

    // TODO: Fill in all fields

    pub_system_state_.publish(msg);
}

void SafeRegretMPCNode::publishVisualizations() {
    if (!enable_visualization_) return;

    // TODO: Publish visualization markers
}

void SafeRegretMPCNode::updateReferenceTrajectory() {
    reference_trajectory_.clear();

    for (const auto& pose : global_plan_.poses) {
        Eigen::VectorXd state(6);
        state << pose.pose.position.x,
                 pose.pose.position.y,
                 tf::getYaw(pose.pose.orientation),
                 ref_vel_,
                 0.0,  // cte
                 0.0;  // etheta
        reference_trajectory_.push_back(state);
    }
}

bool SafeRegretMPCNode::isSystemReady() {
    system_ready_ = odom_received_ && plan_received_;

    if (stl_enabled_ && !stl_received_) {
        system_ready_ = false;
    }

    if (dr_enabled_ && !dr_received_) {
        system_ready_ = false;
    }

    if (terminal_set_enabled_ && !terminal_set_received_) {
        system_ready_ = false;
    }

    return system_ready_;
}

std::string SafeRegretMPCNode::getSystemMode() const {
    return controller_mode_;
}

void SafeRegretMPCNode::checkSystemReady() {
    if (!system_ready_ && isSystemReady()) {
        ROS_INFO("System ready! Starting control...");
    }
}

// ========== Services ==========

bool SafeRegretMPCNode::setSTLSpecificationCallback(
    safe_regret_mpc::SetSTLSpecification::Request& req,
    safe_regret_mpc::SetSTLSpecification::Response& res) {
    ROS_INFO("Setting STL specification: %s", req.stl_formula.c_str());

    mpc_solver_->setSTLSpecification(req.stl_formula,
                                    req.robustness_baseline,
                                    req.weight);

    res.success = true;
    res.message = "STL specification set successfully";
    res.formula_id = 1;  // TODO: Generate unique ID

    return true;
}

bool SafeRegretMPCNode::getSystemStatusCallback(
    safe_regret_mpc::GetSystemStatus::Request& req,
    safe_regret_mpc::GetSystemStatus::Response& res) {
    res.current_state.mode = getSystemMode();
    res.current_state.system_ready = system_ready_;
    res.system_mode = getSystemMode();
    res.system_ready = system_ready_;
    res.status_message = system_ready_ ? "System ready" : "System not ready";

    // TODO: Fill in all fields

    return true;
}

bool SafeRegretMPCNode::resetRegretTrackerCallback(
    safe_regret_mpc::ResetRegretTracker::Request& req,
    safe_regret_mpc::ResetRegretTracker::Response& res) {
    ROS_INFO("Resetting regret tracker...");

    double before = mpc_solver_->getCumulativeRegret();
    mpc_solver_->resetRegret();
    double after = mpc_solver_->getCumulativeRegret();

    res.success = true;
    res.message = "Regret tracker reset successfully";
    res.cumulative_regret_before = before;
    res.cumulative_regret_after = after;

    return true;
}

// ========== Tube MPC Command Processing (Integration Mode) ==========

void SafeRegretMPCNode::processTubeMPCCommand() {
    if (!tube_mpc_cmd_received_) {
        ROS_WARN_THROTTLE(2.0, "No tube_mpc command received yet");
        return;
    }

    geometry_msgs::Twist cmd_final = tube_mpc_cmd_raw_;

    // Process through STL module if enabled
    if (stl_enabled_ && stl_received_) {
        cmd_final = adjustCommandForSTL(cmd_final);
    }

    // Process through DR module if enabled
    if (dr_enabled_ && dr_received_) {
        cmd_final = adjustCommandForDR(cmd_final);
    }

    // Publish final command
    publishFinalCommand(cmd_final);
}

geometry_msgs::Twist SafeRegretMPCNode::adjustCommandForSTL(
    const geometry_msgs::Twist& cmd_raw) {

    geometry_msgs::Twist cmd_adjusted = cmd_raw;

    // STL-based adjustment logic
    // For now, implement a simple scaling based on robustness
    double stl_factor = 1.0;

    if (stl_info_.robustness_value < 0.0) {
        // Low robustness: scale down commands
        stl_factor = std::max(0.5, 1.0 + stl_info_.robustness_value / 2.0);
    }

    cmd_adjusted.linear.x *= stl_factor;
    cmd_adjusted.angular.z *= stl_factor;

    if (debug_mode_) {
        ROS_DEBUG("STL adjustment: factor=%.3f, v: %.3f→%.3f, w: %.3f→%.3f",
                  stl_factor,
                  cmd_raw.linear.x, cmd_adjusted.linear.x,
                  cmd_raw.angular.z, cmd_adjusted.angular.z);
    }

    return cmd_adjusted;
}

geometry_msgs::Twist SafeRegretMPCNode::adjustCommandForDR(
    const geometry_msgs::Twist& cmd_raw) {

    geometry_msgs::Twist cmd_adjusted = cmd_raw;

    // DR-based safety check
    // For now, implement basic velocity limiting based on DR margins
    double max_linear_vel = 1.0;  // Default limit
    double max_angular_vel = 1.5; // Default limit

    // If DR margins are available, use them to adjust limits
    if (dr_info_.margins.size() > 0) {
        // TODO: Implement proper DR margin interpretation
        // For now, use a conservative approach
        max_linear_vel *= 0.9;
        max_angular_vel *= 0.9;
    }

    // Apply limits
    cmd_adjusted.linear.x = std::max(-max_linear_vel,
                                      std::min(max_linear_vel,
                                              cmd_adjusted.linear.x));
    cmd_adjusted.angular.z = std::max(-max_angular_vel,
                                       std::min(max_angular_vel,
                                                  cmd_adjusted.angular.z));

    if (debug_mode_) {
        ROS_DEBUG("DR adjustment: v: %.3f→%.3f, w: %.3f→%.3f",
                  cmd_raw.linear.x, cmd_adjusted.linear.x,
                  cmd_raw.angular.z, cmd_adjusted.angular.z);
    }

    return cmd_adjusted;
}

void SafeRegretMPCNode::publishFinalCommand(const geometry_msgs::Twist& cmd) {
    cmd_vel_ = cmd;  // Store for publishing
    pub_cmd_vel_final_.publish(cmd);

    if (debug_mode_) {
        ROS_DEBUG("Final command published: v=%.3f, w=%.3f",
                  cmd.linear.x, cmd.angular.z);
    }
}

// ========== Main ==========

void SafeRegretMPCNode::spin() {
    ROS_INFO("Safe-Regret MPC Node spinning...");
    ros::spin();
}

} // namespace safe_regret_mpc

// ========== Main Function ==========

int main(int argc, char** argv) {
    ros::init(argc, argv, "safe_regret_mpc_node");

    ros::NodeHandle nh;
    ros::NodeHandle private_nh("~");

    safe_regret_mpc::SafeRegretMPCNode node(nh, private_nh);

    if (!node.initialize()) {
        ROS_ERROR("Failed to initialize Safe-Regret MPC Node!");
        return -1;
    }

    node.spin();

    return 0;
}
