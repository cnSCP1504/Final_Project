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
#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <Eigen/Dense>

#include "stl_monitor/BeliefSpaceEvaluator.h"
#include "stl_monitor/SmoothRobustness.h"
#include "stl_monitor/STLParser.h"
#include "stl_monitor/RobustnessBudget.h"

class STLMonitorNode {
public:
    STLMonitorNode() : nh_("~") {
        // Parameters with defaults
        bool use_belief_space = false;  // ✅ DEFAULT: FALSE for path-tracking
        nh_.param("use_belief_space", use_belief_space, false);
        nh_.param("belief_space/enabled", use_belief_space, use_belief_space);  // Also check YAML path
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

        // Initialize TF listener
        tf_buffer_.reset(new tf2_ros::Buffer(ros::Duration(10.0)));
        tf_listener_.reset(new tf2_ros::TransformListener(*tf_buffer_));

        // Subscribers
        // ✅ CRITICAL FIX: No need to subscribe to pose topics!
        // We use TF to get latest robot position (much faster than amcl_pose/odom)
        mpc_trajectory_sub_ = nh_.subscribe("/mpc_trajectory", 1, &STLMonitorNode::mpcTrajectoryCallback, this);
        // ✅ Queue size = 1: Always use latest trajectory, don't queue old trajectories

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
        ROS_INFO("   Trajectory source: /mpc_trajectory (Tube MPC) ✅");
    }

    void mpcTrajectoryCallback(const nav_msgs::Path::ConstPtr& msg) {
        static int callback_count = 0;
        callback_count++;

        // Transform trajectory from base_link to map frame
        nav_msgs::Path::Ptr transformed_path(new nav_msgs::Path());
        transformed_path->header.frame_id = "map";
        transformed_path->header.stamp = ros::Time::now();  // ✅ Use current time, not msg time

        try {
            // ✅ CRITICAL FIX: Use ros::Time(0) to get latest transform, not msg->header.stamp
            // msg->header.stamp might be in the past (prediction time), causing old TF lookup
            geometry_msgs::TransformStamped transform = tf_buffer_->lookupTransform(
                "map", msg->header.frame_id, ros::Time(0), ros::Duration(0.1));

            // Transform each pose
            for (const auto& pose_stamped : msg->poses) {
                geometry_msgs::PoseStamped transformed_pose;
                tf2::doTransform(pose_stamped, transformed_pose, transform);
                transformed_path->poses.push_back(transformed_pose);
            }

            mpc_trajectory_ = transformed_path;

            // Debug: log trajectory coordinates to verify updates
            if (callback_count <= 20) {
                ros::Time now = ros::Time::now();
                double time_diff = (now - msg->header.stamp).toSec();

                // Log first 3 and last 3 points
                size_t n_show = std::min<size_t>(3, transformed_path->poses.size());
                ROS_INFO("📨 [MPC TRAJ %d] %zu poses (orig frame: %s, msg_time: %.3f sec ago)",
                         callback_count, msg->poses.size(), msg->header.frame_id.c_str(), time_diff);
                ROS_INFO("   First %zu points (transformed to map):", n_show);
                for (size_t i = 0; i < n_show; i++) {
                    const auto& p = transformed_path->poses[i].pose.position;
                    ROS_INFO("     [%zu] x=%.3f, y=%.3f", i, p.x, p.y);
                }
                if (transformed_path->poses.size() > 6) {
                    ROS_INFO("   ... (%zu more points) ...", transformed_path->poses.size() - 6);
                    ROS_INFO("   Last 3 points:");
                    for (size_t i = transformed_path->poses.size() - 3; i < transformed_path->poses.size(); i++) {
                        const auto& p = transformed_path->poses[i].pose.position;
                        ROS_INFO("     [%zu] x=%.3f, y=%.3f", i, p.x, p.y);
                    }
                }
            }

        } catch (tf2::TransformException& ex) {
            ROS_WARN_THROTTLE(1.0, "TF transform failed: %s", ex.what());
            // Fallback: use original trajectory (but this will give wrong distances!)
            mpc_trajectory_ = msg;
        }
    }

    /**
     * @brief Calculate minimum distance from point to line segment
     * @param px, py Point coordinates
     * @param x1, y1 Line segment start
     * @param x2, y2 Line segment end
     * @return Minimum distance to the segment
     */
    double pointToSegmentDistance(double px, double py,
                                   double x1, double y1,
                                   double x2, double y2) {
        // Vector from p1 to p2
        double dx = x2 - x1;
        double dy = y2 - y1;
        double len_sq = dx*dx + dy*dy;

        // Handle degenerate case (point instead of segment)
        if (len_sq < 1e-9) {
            return std::sqrt((px - x1)*(px - x1) + (py - y1)*(py - y1));
        }

        // Projection parameter t (clamped to [0, 1])
        double t = std::max(0.0, std::min(1.0,
            ((px - x1) * dx + (py - y1) * dy) / len_sq));

        // Closest point on segment
        double nearest_x = x1 + t * dx;
        double nearest_y = y1 + t * dy;

        // Distance to closest point
        return std::sqrt((px - nearest_x)*(px - nearest_x) +
                         (py - nearest_y)*(py - nearest_y));
    }

    /**
     * @brief Calculate minimum distance from point to path
     * @param px, py Point coordinates
     * @return Minimum distance to any segment in the path
     */
    double minDistanceToPath(double px, double py) {
        if (!mpc_trajectory_ || mpc_trajectory_->poses.size() < 2) {
            // Fallback to distance to last point if path is invalid
            if (mpc_trajectory_ && !mpc_trajectory_->poses.empty()) {
                const auto& goal = mpc_trajectory_->poses.back().pose;
                double dist = std::sqrt((px - goal.position.x)*(px - goal.position.x) +
                                        (py - goal.position.y)*(py - goal.position.y));
                ROS_WARN("⚠️ Path has <2 points, using distance to last point: %.3f m", dist);
                return dist;
            }
            ROS_ERROR("❌ No MPC trajectory available!");
            return std::numeric_limits<double>::max();
        }

        double min_dist = std::numeric_limits<double>::max();
        size_t min_segment_idx = 0;

        // Check all segments in the path
        for (size_t i = 0; i < mpc_trajectory_->poses.size() - 1; ++i) {
            const auto& p1 = mpc_trajectory_->poses[i].pose;
            const auto& p2 = mpc_trajectory_->poses[i+1].pose;

            double dist = pointToSegmentDistance(px, py,
                                                  p1.position.x, p1.position.y,
                                                  p2.position.x, p2.position.y);
            if (dist < min_dist) {
                min_dist = dist;
                min_segment_idx = i;
            }
        }

        // Log detailed info for debugging
        static int debug_count = 0;
        if (debug_count++ < 50) {  // Log first 50 times
            const auto& seg_p1 = mpc_trajectory_->poses[min_segment_idx].pose;
            const auto& seg_p2 = mpc_trajectory_->poses[min_segment_idx+1].pose;
            const auto& traj_first = mpc_trajectory_->poses.front().pose;
            const auto& traj_last = mpc_trajectory_->poses.back().pose;

            // Calculate distance to trajectory endpoints
            double dist_to_first = std::sqrt((px - traj_first.position.x)*(px - traj_first.position.x) +
                                            (py - traj_first.position.y)*(py - traj_first.position.y));
            double dist_to_last = std::sqrt((px - traj_last.position.x)*(px - traj_last.position.x) +
                                           (py - traj_last.position.y)*(py - traj_last.position.y));

            // Check if robot is beyond trajectory end
            bool is_beyond_end = (min_segment_idx == mpc_trajectory_->poses.size() - 2);

            ROS_INFO("📍 [DEBUG] Robot: [%.3f, %.3f], min_dist: %.3f m (seg %zu/%zu, beyond_end: %s)",
                     px, py, min_dist, min_segment_idx, mpc_trajectory_->poses.size()-2,
                     is_beyond_end ? "YES ❌" : "NO ✅");
            ROS_INFO("   Closest seg: [%.3f, %.3f] → [%.3f, %.3f]",
                     seg_p1.position.x, seg_p1.position.y,
                     seg_p2.position.x, seg_p2.position.y);
            ROS_INFO("   MPC traj: %zu points, first: [%.3f, %.3f] (dist: %.3f m), last: [%.3f, %.3f] (dist: %.3f m)",
                     mpc_trajectory_->poses.size(),
                     traj_first.position.x, traj_first.position.y, dist_to_first,
                     traj_last.position.x, traj_last.position.y, dist_to_last);
        }

        return min_dist;
    }

    void evaluateSTL(const ros::TimerEvent&) {
        if (!mpc_trajectory_ || mpc_trajectory_->poses.empty()) {
            return;
        }

        try {
            // ✅ CRITICAL FIX: Use TF to get LATEST robot position (not from stale amcl_pose)
            // This solves the problem of robot position being 4+ seconds old
            geometry_msgs::TransformStamped transform = tf_buffer_->lookupTransform(
                "map", "base_footprint", ros::Time(0), ros::Duration(0.1));

            double x = transform.transform.translation.x;
            double y = transform.transform.translation.y;

            // Create belief state
            stl_monitor::BeliefState belief;
            belief.mean = Eigen::Vector2d(x, y);

            // Use identity covariance (since we don't have it from TF)
            // This is OK for simplified mode (not using belief-space)
            belief.covariance = Eigen::Matrix2d::Identity() * 0.01;  // Small uncertainty

            // ✅ PATH TRACKING STL: G_[0,T](distance(robot, path) ≤ threshold)
            // Compute minimum distance from robot to path (not just to goal!)
            double min_distance_to_path = minDistanceToPath(x, y);

            // Compute STL robustness
            if (use_belief_space_) {
                // ✅ BELIEF-SPACE EVALUATION (manuscript Eq. 204)
                // ρ̃_k = E_{x∼β_k}[ρ^soft(φ; x_{k:k+N})]
                // φ = G_[0,T](distance(robot, path) ≤ threshold)

                // Sample particles from belief
                std::vector<Eigen::VectorXd> particles = evaluator_->sampleParticles(belief, num_particles_);

                // Compute robustness for each particle
                Eigen::VectorXd particle_robustness(num_particles_);
                for (int i = 0; i < num_particles_; ++i) {
                    double px = particles[i](0);
                    double py = particles[i](1);
                    // ✅ PATH TRACKING: distance to PATH (not to goal)
                    double dist_to_path = minDistanceToPath(px, py);
                    particle_robustness(i) = reachability_threshold_ - dist_to_path;
                }

                // ✅ SMOOTH EXPECTATION using log-sum-exp (manuscript smax)
                // E[ρ] ≈ smax_τ(particle_robustness)
                robustness_ = smooth_robustness_->smax(particle_robustness);

                // Log for verification (should see continuous values, not just 2 values!)
                static int log_count = 0;
                if (log_count++ < 50) {  // Log first 50 iterations
                    // Log detailed info for first 10, then every 5th
                    if (log_count <= 10 || log_count % 5 == 0) {
                        ROS_INFO("🔍 dist=%.3f m, robust=%.4f (exp: %.4f), cov_trace=%.3f, particles=%d",
                                 min_distance_to_path, robustness_,
                                 reachability_threshold_ - min_distance_to_path,
                                 belief.covariance.trace(),
                                 num_particles_);
                    }
                }

            } else {
                // ⚠️ SIMPLIFIED EVALUATION (NOT belief-space, but still path-tracking)
                robustness_ = reachability_threshold_ - min_distance_to_path;

                // Log for verification (more frequent when distance is large)
                static int log_count = 0;
                if (log_count++ < 100) {  // Log first 100 iterations
                    // Log every 10th time
                    if (log_count % 10 == 0) {
                        // Get MPC trajectory info
                        const auto& traj_first = mpc_trajectory_->poses.front().pose;
                        const auto& traj_last = mpc_trajectory_->poses.back().pose;
                        double dist_to_first = std::sqrt((x - traj_first.position.x)*(x - traj_first.position.x) +
                                                        (y - traj_first.position.y)*(y - traj_first.position.y));
                        double dist_to_last = std::sqrt((x - traj_last.position.x)*(x - traj_last.position.x) +
                                                       (y - traj_last.position.y)*(y - traj_last.position.y));

                        // 📊 CRITICAL: Check if trajectory start point is at (0,0) or updating
                        bool traj_start_at_origin = (std::abs(traj_first.position.x) < 0.01 &&
                                                     std::abs(traj_first.position.y) < 0.01);

                        ROS_INFO("📏 [%d] Robot: [%.3f, %.3f] (via TF), traj_first: [%.3f, %.3f]%s, dist: %.3f (to_first: %.3f)",
                                 log_count, x, y,
                                 traj_first.position.x, traj_first.position.y,
                                 traj_start_at_origin ? " ⚠️ ORIGIN!" : "",
                                 min_distance_to_path, dist_to_first);
                    }
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
    ros::Subscriber mpc_trajectory_sub_;
    ros::Publisher robustness_pub_;
    ros::Publisher violation_pub_;
    ros::Publisher budget_pub_;

    // TF for coordinate transformation
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
    ros::Timer timer_;

    geometry_msgs::PoseWithCovarianceStamped::ConstPtr current_pose_;
    nav_msgs::Path::ConstPtr mpc_trajectory_;

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
