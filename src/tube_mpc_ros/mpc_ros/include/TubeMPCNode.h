#ifndef TUBE_MPC_NODE_H
#define TUBE_MPC_NODE_H

#include <iostream>
#include <map>
#include <math.h>
#include "ros/ros.h"
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>
#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>
#include <visualization_msgs/Marker.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>
#include <fstream>

#include "TubeMPC.h"
#include "MetricsCollector.h"
#include <Eigen/Core>
#include <Eigen/QR>

using namespace std;
using namespace Eigen;

class TubeMPCNode
{
    public:
        TubeMPCNode();
        int get_thread_numbers();
        void printMetricsSummary();  // New: Print metrics summary
        
    private:
        ros::NodeHandle _nh;
        ros::Subscriber _sub_odom, _sub_path, _sub_goal, _sub_amcl;
        ros::Subscriber _sub_stl_robustness, _sub_stl_budget, _sub_stl_violation;  // STL integration
        ros::Publisher _pub_globalpath, _pub_odompath, _pub_twist, _pub_mpctraj, _pub_tube;
        ros::Publisher _pub_tracking_error;  // DR Tightening: publish tracking error/residuals
        ros::Timer _timer1;
        tf::TransformListener _tf_listener;
        ros::Time tracking_stime;
        ros::Time tracking_etime;
        ros::Time tracking_time;
        int tracking_time_sec;
        int tracking_time_nsec;

        std::ofstream file;
        unsigned int idx = 0;

        bool start_timef = false;
        bool end_timef = false;

        geometry_msgs::Point _goal_pos;
        nav_msgs::Odometry _odom;
        nav_msgs::Path _odom_path, _mpc_traj, _tube_path;
        geometry_msgs::Twist _twist_msg;

        string _globalPath_topic, _goal_topic;
        string _map_frame, _odom_frame, _car_frame;

        TubeMPC _tube_mpc;
        map<string, double> _tube_mpc_params;
        Metrics::MetricsCollector _metrics_collector;  // New: Metrics collector

        double _mpc_steps, _ref_cte, _ref_etheta, _ref_vel, _w_cte, _w_etheta, _w_vel,
               _w_angvel, _w_accel, _max_angvel, _max_throttle, _bound_value;
        
        double _dt, _w, _w_filtered, _throttle, _speed, _max_speed;
        double _pathLength, _goalRadius, _waypointsDist;
        int _controller_freq, _downSampling, _thread_numbers;
        bool _goal_received, _goal_reached, _path_computed, _pub_twist_flag, _debug_info;
        bool _in_place_rotation;  // 标记是否正在原地旋转模式
        double _rotation_direction;  // 锁定的旋转方向 (1.0 或 -1.0)
        bool _rotation_direction_locked;  // 旋转方向是否已锁定

        // Safe-Regret MPC Integration
        bool _enable_safe_regret_integration;  // 是否启用safe_regret_mpc集成模式
        
        VectorXd _w_residuals;
        int _num_residuals;
        double _disturbance_bound;
        VectorXd _e_current;
        std::string _metrics_output_csv;
        double _target_delta;

        // STL integration variables
        bool _enable_stl_constraints;
        double _stl_weight;
        double _current_stl_robustness;
        double _current_stl_budget;
        bool _current_stl_violation;
        double _stl_ref_cte_adjustment;
        double _stl_ref_vel_adjustment;

        // Terminal set integration (P1-1)
        bool _enable_terminal_set;
        ros::Subscriber _sub_terminal_set;  // P1-1: Terminal set subscription
        ros::Publisher _pub_terminal_set_viz;  // P1-1: Terminal set visualization
        bool _terminal_set_received;
        Eigen::VectorXd _terminal_set_center;
        double _terminal_set_radius;
        int _terminal_set_violation_count;

        double polyeval(Eigen::VectorXd coeffs, double x);
        Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals, int order);

        void odomCB(const nav_msgs::Odometry::ConstPtr& odomMsg);
        void pathCB(const nav_msgs::Path::ConstPtr& pathMsg);
        void goalCB(const geometry_msgs::PoseStamped::ConstPtr& goalMsg);
        void amclCB(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& amclMsg);
        void controlLoopCB(const ros::TimerEvent&);
        void estimateDisturbance(const nav_msgs::Odometry& odom);
        void visualizeTube();

        // STL callback functions
        void stlRobustnessCB(const std_msgs::Float32::ConstPtr& msg);
        void stlBudgetCB(const std_msgs::Float32::ConstPtr& msg);
        void stlViolationCB(const std_msgs::Bool::ConstPtr& msg);
        void adjustReferenceForSTL(double& ref_cte, double& ref_vel);

        // Terminal set callback functions (P1-1)
        void terminalSetCB(const std_msgs::Float64MultiArray::ConstPtr& msg);
        void visualizeTerminalSet();
        bool checkTerminalFeasibilityInLoop(const Eigen::VectorXd& terminal_state);
};

#endif
