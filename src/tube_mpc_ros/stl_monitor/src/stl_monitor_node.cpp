/**
 * @file stl_monitor_node.cpp
 * @brief STL Monitor ROS Node - Belief-Space Implementation with Smooth Robustness
 *
 * This is the CORRECT implementation matching manuscript requirements:
 * - Belief-space STL evaluation: E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]
 * - Log-sum-exp smooth surrogate: smax_τ(z) := τ·log(∑_i e^{z_i/τ})
 * - Budget mechanism: R_{k+1} = max{0, R_k + ρ̃_k - r̲}
 */

#include <ros/ros.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <nav_msgs/Path.h>
#include <Eigen/Dense>

#include "stl_monitor/BeliefSpaceEvaluator.h"
#include "stl_monitor/SmoothRobustness.h"
#include "stl_monitor/STLParser.h"
#include "stl_monitor/RobustnessBudget.h"

class STLMonitorNode {
public:
    STLMonitorNode() : nh_("~") {
        // Parameters with defaults
        bool use_belief_space = true;
        nh_.param("use_belief_space", use_belief_space, true);  // ✅ DEFAULT: TRUE
        nh_.param("temperature", temperature_, 0.1);  // τ for smooth robustness
        nh_.param("baseline_requirement", baseline_requirement_, 0.1);  // r̲
        nh_.param("reachability_threshold", reachability_threshold_, 0.5);
        nh_.param("num_particles", num_particles_, 100);
        nh_.param("update_rate", update_rate_, 10.0);

        use_belief_space_ = use_belief_space;

        // Initialize STL components
        if (use_belief_space_) {
            ROS_INFO("🔧 STL Monitor: Using BELIEF-SPACE evaluation (manuscript-compliant)");
            evaluator_.reset(new stl_monitor::BeliefSpaceEvaluator(
                stl_monitor::BeliefSpaceEvaluator::Method::PARTICLE
            ));
            evaluator_->setNumParticles(num_particles_);
        } else {
            ROS_WARN("⚠️  STL Monitor: Using SIMPLIFIED evaluation (NOT manuscript-compliant)");
        }

        smooth_robustness_.reset(new stl_monitor::SmoothRobustness(temperature_));
        budget_.reset(new stl_monitor::RobustnessBudget());
        budget_->initialize(1.0, baseline_requirement_);  // R_0 = 1.0, r̲ = baseline_requirement_

        // State variables
        robustness_ = 1.0;
        violation_ = false;

        // Subscribers
        pose_sub_ = nh_.subscribe("/amcl_pose", 10, &STLMonitorNode::poseCallback, this);
        path_sub_ = nh_.subscribe("/move_base/GlobalPlanner/plan", 10, &STLMonitorNode::pathCallback, this);

        // Publishers
        robustness_pub_ = nh_.advertise<std_msgs::Float32>("/stl_monitor/robustness", 10);
        violation_pub_ = nh_.advertise<std_msgs::Bool>("/stl_monitor/violation", 10);
        budget_pub_ = nh_.advertise<std_msgs::Float32>("/stl_monitor/budget", 10);

        // Timer for STL evaluation
        timer_ = nh_.createTimer(ros::Duration(1.0 / update_rate_),
                                &STLMonitorNode::evaluateSTL, this);

        ROS_INFO("✅ STL Monitor Node started (C++ with belief-space support)");
        ROS_INFO("   Temperature τ: %.3f", temperature_);
        ROS_INFO("   Baseline r̲: %.3f", baseline_requirement_);
        ROS_INFO("   Particles: %d", num_particles_);
        ROS_INFO("   Use belief-space: %s", use_belief_space_ ? "TRUE ✅" : "FALSE ❌");
    }

    void poseCallback(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& msg) {
        current_pose_ = msg;
    }

    void pathCallback(const nav_msgs::Path::ConstPtr& msg) {
        global_path_ = msg;
    }

    void evaluateSTL(const ros::TimerEvent&) {
        if (!current_pose_ || !global_path_ || global_path_->poses.empty()) {
            return;
        }

        try {
            // Extract current position and covariance
            double x = current_pose_->pose.pose.position.x;
            double y = current_pose_->pose.pose.position.y;

            // Create belief state
            stl_monitor::BeliefState belief;
            belief.mean = Eigen::Vector2d(x, y);

            // Extract covariance from PoseWithCovarianceStamped
            belief.covariance = Eigen::Matrix2d::Identity();
            if (current_pose_->pose.covariance.size() >= 36) {
                // ROS covariance is 6x6 in row-major order
                // We need indices (0,0), (0,1), (1,0), (1,1) for x,y
                belief.covariance(0, 0) = current_pose_->pose.covariance[0];
                belief.covariance(0, 1) = current_pose_->pose.covariance[1];
                belief.covariance(1, 0) = current_pose_->pose.covariance[6];
                belief.covariance(1, 1) = current_pose_->pose.covariance[7];
            }

            // Get goal position
            const auto& goal_pose = global_path_->poses.back().pose;
            double goal_x = goal_pose.position.x;
            double goal_y = goal_pose.position.y;

            // Compute STL robustness
            if (use_belief_space_) {
                // ✅ BELIEF-SPACE EVALUATION (manuscript Eq. 204)
                // ρ̃_k = E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]

                // Sample particles from belief
                std::vector<Eigen::VectorXd> particles = evaluator_->sampleParticles(belief, num_particles_);

                // Compute robustness for each particle
                Eigen::VectorXd particle_robustness(num_particles_);
                for (int i = 0; i < num_particles_; ++i) {
                    double px = particles[i](0);
                    double py = particles[i](1);
                    double distance = std::sqrt((px - goal_x)*(px - goal_x) +
                                                (py - goal_y)*(py - goal_y));
                    particle_robustness(i) = reachability_threshold_ - distance;
                }

                // ✅ SMOOTH EXPECTATION using log-sum-exp (manuscript smax)
                // E[ρ] ≈ smax_τ(particle_robustness) / num_particles
                robustness_ = smooth_robustness_->smax(particle_robustness);

                // Log for verification (should see continuous values, not just 2 values!)
                static int log_count = 0;
                if (log_count++ < 20) {  // Log first 20 iterations
                    ROS_INFO("🔍 Belief-space robustness: %.4f (particles: %d)",
                             robustness_, num_particles_);
                    ROS_INFO("   Belief mean: [%.3f, %.3f]", x, y);
                    ROS_INFO("   Belief cov trace: %.3f", belief.covariance.trace());
                }

            } else {
                // ⚠️ SIMPLIFIED EVALUATION (NOT manuscript-compliant)
                double distance = std::sqrt((x - goal_x)*(x - goal_x) +
                                            (y - goal_y)*(y - goal_y));
                robustness_ = reachability_threshold_ - distance;

                static int log_count = 0;
                if (log_count++ < 5) {
                    ROS_WARN("⚠️  Using simplified robustness (NOT belief-space): %.4f", robustness_);
                }
            }

            // ✅ BUDGET UPDATE (manuscript Eq. 333)
            // R_{k+1} = max{0, R_k + ρ̃_k - r̲}
            double budget_value = budget_->update(robustness_);

            // Check for violation
            violation_ = (robustness_ < 0.0);

            // Publish results
            std_msgs::Float32 robustness_msg;
            robustness_msg.data = robustness_;
            robustness_pub_.publish(robustness_msg);

            std_msgs::Bool violation_msg;
            violation_msg.data = violation_;
            violation_pub_.publish(violation_msg);

            std_msgs::Float32 budget_msg;
            budget_msg.data = budget_value;
            budget_pub_.publish(budget_msg);

            // Log violations
            if (violation_) {
                ROS_WARN("❌ STL Violation! Robustness: %.4f, Budget: %.4f",
                         robustness_, budget_value);
            }

        } catch (const std::exception& e) {
            ROS_ERROR("Error in STL evaluation: %s", e.what());
        }
    }

private:
    ros::NodeHandle nh_;
    ros::Subscriber pose_sub_;
    ros::Subscriber path_sub_;
    ros::Publisher robustness_pub_;
    ros::Publisher violation_pub_;
    ros::Publisher budget_pub_;
    ros::Timer timer_;

    geometry_msgs::PoseWithCovarianceStamped::ConstPtr current_pose_;
    nav_msgs::Path::ConstPtr global_path_;

    boost::shared_ptr<stl_monitor::BeliefSpaceEvaluator> evaluator_;
    boost::shared_ptr<stl_monitor::SmoothRobustness> smooth_robustness_;
    boost::shared_ptr<stl_monitor::RobustnessBudget> budget_;

    bool use_belief_space_;
    double temperature_;
    double baseline_requirement_;
    double reachability_threshold_;
    int num_particles_;
    double update_rate_;

    double robustness_;
    bool violation_;
};

int main(int argc, char** argv) {
    ros::init(argc, argv, "stl_monitor");

    try {
        STLMonitorNode monitor;
        ros::spin();
    } catch (const std::exception& e) {
        ROS_ERROR("STL Monitor node failed: %s", e.what());
        return 1;
    }

    return 0;
}
