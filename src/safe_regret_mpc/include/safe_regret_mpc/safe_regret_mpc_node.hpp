#ifndef SAFE_REGRET_MPC_NODE_HPP
#define SAFE_REGRET_MPC_NODE_HPP

#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float64MultiArray.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/Joy.h>
#include <visualization_msgs/Marker.h>
#include <tf/transform_listener.h>

#include "safe_regret_mpc/SafeRegretState.h"
#include "safe_regret_mpc/SafeRegretMetrics.h"
#include "safe_regret_mpc/STLRobustness.h"
#include "safe_regret_mpc/DRMargins.h"
#include "safe_regret_mpc/TerminalSet.h"

#include "safe_regret_mpc/SafeRegretMPC.hpp"
#include "safe_regret_mpc/SetSTLSpecification.h"
#include "safe_regret_mpc/GetSystemStatus.h"
#include "safe_regret_mpc/ResetRegretTracker.h"

namespace safe_regret_mpc {

/**
 * @brief Safe-Regret MPC ROS Node
 *
 * Unified ROS node for complete Safe-Regret MPC system.
 * Integrates all components and manages communication.
 */
class SafeRegretMPCNode {
public:
    /**
     * @brief Constructor
     * @param nh Node handle
     * @param private_nh Private node handle
     */
    SafeRegretMPCNode(ros::NodeHandle& nh, ros::NodeHandle& private_nh);

    /**
     * @brief Destructor
     */
    ~SafeRegretMPCNode();

    /**
     * @brief Initialize node
     * @return True if successful
     */
    bool initialize();

    /**
     * @brief Spin (main loop)
     */
    void spin();

private:
    // ========== Callbacks ==========

    /**
     * @brief Odometry callback
     */
    void odomCallback(const nav_msgs::Odometry::ConstPtr& msg);

    /**
     * @brief Global plan callback
     */
    void globalPlanCallback(const nav_msgs::Path::ConstPtr& msg);

    /**
     * @brief Goal callback
     */
    void goalCallback(const geometry_msgs::PoseStamped::ConstPtr& msg);

    /**
     * @brief STL robustness callback (from STL monitor)
     */
    void stlRobustnessCallback(const safe_regret_mpc::STLRobustness::ConstPtr& msg);

    /**
     * @brief DR margins callback (from DR tightening) - Float64MultiArray version
     */
    void drMarginsRawCallback(const std_msgs::Float64MultiArray::ConstPtr& msg);

    /**
     * @brief DR margins callback (from DR tightening) - DRMargins version
     */
    void drMarginsCallback(const safe_regret_mpc::DRMargins::ConstPtr& msg);

    /**
     * @brief Terminal set callback (from DR tightening)
     */
    void terminalSetCallback(const safe_regret_mpc::TerminalSet::ConstPtr& msg);

    /**
     * @brief Tube MPC command callback (raw command from tube_mpc)
     */
    void tubeMPCCmdCallback(const geometry_msgs::Twist::ConstPtr& msg);

    /**
     * @brief Tube MPC trajectory callback
     */
    void tubeMPCTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg);

    /**
     * @brief Tube MPC tracking error callback
     */
    void tubeMPCErrorCallback(const std_msgs::Float64MultiArray::ConstPtr& msg);

    /**
     * @brief Control timer callback
     */
    void controlTimerCallback(const ros::TimerEvent& event);

    /**
     * @brief Publishing timer callback
     */
    void publishTimerCallback(const ros::TimerEvent& event);

    // ========== Services ==========

    /**
     * @brief Set STL specification service
     */
    bool setSTLSpecificationCallback(
        safe_regret_mpc::SetSTLSpecification::Request& req,
        safe_regret_mpc::SetSTLSpecification::Response& res);

    /**
     * @brief Get system status service
     */
    bool getSystemStatusCallback(
        safe_regret_mpc::GetSystemStatus::Request& req,
        safe_regret_mpc::GetSystemStatus::Response& res);

    /**
     * @brief Reset regret tracker service
     */
    bool resetRegretTrackerCallback(
        safe_regret_mpc::ResetRegretTracker::Request& req,
        safe_regret_mpc::ResetRegretTracker::Response& res);

    // ========== Internal Methods ==========

    /**
     * @brief Load parameters
     */
    void loadParameters();

    /**
     * @brief Load and set MPC parameters
     */
    bool loadAndSetMPCParameters();

    /**
     * @brief Initialize MPC solver
     */
    bool initializeMPC();

    /**
     * @brief Solve MPC and compute control
     */
    void solveMPC();

    /**
     * @brief Check if system is ready
     */
    bool isSystemReady() const;

    /**
     * @brief Check system ready status
     */
    void checkSystemReady();

    /**
     * @brief Publish system state
     */
    void publishSystemState();

    /**
     * @brief Publish control command
     */
    void publishControl();

    /**
     * @brief Publish visualizations
     */
    void publishVisualizations();

    /**
     * @brief Process tube_mpc command through STL/DR modules
     */
    void processTubeMPCCommand();

    /**
     * @brief Adjust command based on STL robustness
     */
    geometry_msgs::Twist adjustCommandForSTL(const geometry_msgs::Twist& cmd_raw);

    /**
     * @brief Adjust command based on DR safety margins
     */
    geometry_msgs::Twist adjustCommandForDR(const geometry_msgs::Twist& cmd_raw);

    /**
     * @brief Publish final command to robot
     */
    void publishFinalCommand(const geometry_msgs::Twist& cmd);

    /**
     * @brief Update reference trajectory
     */
    void updateReferenceTrajectory();

    /**
     * @brief Check system ready
     */
    bool isSystemReady();

    /**
     * @brief Get current mode string
     */
    std::string getSystemMode() const;

    /**
     * @brief Normalize angle to [-π, π]
     */
    double normalizeAngle(double angle) const;

    // ========== Member Variables ==========

    // ROS handles
    ros::NodeHandle& nh_;
    ros::NodeHandle& private_nh_;

    // Subscribers
    ros::Subscriber sub_odom_;
    ros::Subscriber sub_global_plan_;
    ros::Subscriber sub_goal_;
    ros::Subscriber sub_stl_robustness_;
    ros::Subscriber sub_dr_margins_raw_;
    ros::Subscriber sub_dr_margins_;
    ros::Subscriber sub_terminal_set_;

    // Tube MPC data subscribers (integration mode)
    ros::Subscriber sub_tube_mpc_cmd_;        // Raw command from tube_mpc
    ros::Subscriber sub_tube_mpc_traj_;       // MPC trajectory from tube_mpc
    ros::Subscriber sub_tube_mpc_error_;      // Tracking error from tube_mpc

    // Publishers
    ros::Publisher pub_cmd_vel_final_;        // Final command (processed) - renamed from pub_cmd_vel_
    ros::Publisher pub_system_state_;
    ros::Publisher pub_metrics_;
    ros::Publisher pub_mpc_trajectory_;       // Forwarded from tube_mpc
    ros::Publisher pub_tube_boundaries_;       // Forwarded from tube_mpc
    ros::Publisher pub_stl_info_;
    ros::Publisher pub_dr_info_;
    ros::Publisher pub_terminal_info_;

    // Visualization publishers
    ros::Publisher pub_viz_trajectory_;
    ros::Publisher pub_viz_tube_;
    ros::Publisher pub_viz_terminal_set_;
    ros::Publisher pub_viz_robustness_;

    // Services
    ros::ServiceServer srv_set_stl_;
    ros::ServiceServer srv_get_status_;
    ros::ServiceServer srv_reset_regret_;

    // Timers
    ros::Timer control_timer_;
    ros::Timer publish_timer_;

    // TF
    std::shared_ptr<tf::TransformListener> tf_listener_;

    // MPC solver
    std::unique_ptr<SafeRegretMPC> mpc_solver_;

    // System state
    nav_msgs::Odometry current_odom_;
    nav_msgs::Path global_plan_;
    geometry_msgs::PoseStamped current_goal_;
    std::vector<Eigen::VectorXd> reference_trajectory_;
    bool goal_received_;
    bool goal_reached_;
    double goal_radius_;  // Distance threshold for goal arrival (meters)

    // STL state
    safe_regret_mpc::STLRobustness stl_info_;
    bool stl_enabled_;
    bool stl_received_;

    // DR state
    safe_regret_mpc::DRMargins dr_info_;
    bool dr_enabled_;
    bool dr_received_;

    // Terminal set state
    safe_regret_mpc::TerminalSet terminal_info_;
    bool terminal_set_enabled_;
    bool terminal_set_received_;

    // Control state
    geometry_msgs::Twist cmd_vel_;               // Deprecated: use tube_mpc_cmd_raw_ and cmd_vel_final_
    bool system_ready_;
    bool odom_received_;
    bool plan_received_;

    // Tube MPC data (integration mode)
    geometry_msgs::Twist tube_mpc_cmd_raw_;     // Raw command from tube_mpc
    nav_msgs::Path tube_mpc_trajectory_;         // Trajectory from tube_mpc
    std_msgs::Float64MultiArray tube_mpc_error_; // Tracking error from tube_mpc
    bool tube_mpc_cmd_received_;
    bool tube_mpc_traj_received_;
    bool tube_mpc_error_received_;
    bool enable_integration_mode_;               // Integration mode flag

    // Parameters
    std::string controller_mode_;     // "STL_MPC", "DR_MPC", "SAFE_REGRET_MPC"
    double controller_frequency_;     // Hz
    double publish_frequency_;        // Hz
    int mpc_steps_;                   // Horizon N
    double ref_vel_;                  // Reference velocity
    bool debug_mode_;
    bool enable_visualization_;

    // Frame IDs
    std::string global_frame_;
    std::string robot_base_frame_;
};

} // namespace safe_regret_mpc

#endif // SAFE_REGRET_MPC_NODE_HPP
