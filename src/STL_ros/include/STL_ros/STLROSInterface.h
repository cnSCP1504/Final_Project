#ifndef STL_ROS_INTERFACE_H
#define STL_ROS_INTERFACE_H

#include <ros/ros.h>
#include <std_msgs/Header.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>
#include <sensor_msgs/JointState.h>
#include <visualization_msgs/Marker.h>
#include <tf/tf.h>

#include "STL_ros/STLMonitor.h"

// Forward declarations for ROS messages
namespace stl_ros {
    template<typename T>
    class STLRobustness;
    template<typename T>
    class STLBudget;
    template<typename T>
    class STLFormula;
}

namespace stl_ros {

/**
 * @brief ROS interface for STL Monitor
 *
 * Subscribes to:
 * - Robot state/belief from estimator
 * - MPC prediction trajectory
 * - Control inputs
 *
 * Publishes:
 * - STL robustness values
 * - Budget status
 * - Formula satisfaction
 *
 * Services:
 * - Load formulas
 * - Update parameters
 * - Query status
 */
class STLROSInterface {
public:
    /**
     * @brief Constructor
     */
    STLROSInterface(ros::NodeHandle& nh, const STLMonitor::Config& config);

    /**
     * @brief Initialize ROS communication
     */
    bool initialize();

    /**
     * @brief Main spin callback (called at control rate)
     */
    void spin();

    /**
     * @brief Shutdown ROS interface
     */
    void shutdown();

private:
    // ROS node handle
    ros::NodeHandle nh_;
    ros::NodeHandle private_nh_;

    // STL Monitor
    std::unique_ptr<STLMonitor> monitor_;

    // Subscribers
    ros::Subscriber belief_sub_;
    ros::Subscriber mpc_trajectory_sub_;
    ros::Subscriber robot_state_sub_;

    // Publishers
    ros::Publisher robustness_pub_;
    ros::Publisher budget_pub_;
    ros::Publisher formula_pub_;
    ros::Publisher visualization_pub_;

    // Timers
    ros::Timer evaluation_timer_;

    // Parameters
    STLMonitor::Config config_;
    double control_frequency_;
    bool enable_visualization_;

    // Callbacks
    void beliefCallback(const geometry_msgs::PoseStamped::ConstPtr& msg);
    void mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg);
    void robotStateCallback(const sensor_msgs::JointState::ConstPtr& msg);
    void evaluationTimerCallback(const ros::TimerEvent& event);

    // Dynamic reconfigure
    typedef dynamic_reconfigure::Server<stl_ros::STLParamsConfig> ReconfigureServer;
    std::unique_ptr<ReconfigureServer> reconfigure_server_;
    void reconfigureCallback(stl_ros::STLParamsConfig& config, uint32_t level);

    // Helper methods
    BeliefState convertToBelief(const geometry_msgs::PoseStamped& msg) const;
    std::vector<Eigen::VectorXd> convertToTrajectory(const nav_msgs::Path& msg) const;
    stl_ros::STLRobustness createRobustnessMessage(
        const std::string& name,
        const MonitorResult& result) const;
    stl_ros::STLBudget createBudgetMessage() const;
};

} // namespace stl_ros

#endif // STL_ROS_INTERFACE_H
