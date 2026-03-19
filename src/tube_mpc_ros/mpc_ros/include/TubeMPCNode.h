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
        
        VectorXd _w_residuals;
        int _num_residuals;
        double _disturbance_bound;
        VectorXd _e_current;
        std::string _metrics_output_csv;
        double _target_delta;

        double polyeval(Eigen::VectorXd coeffs, double x);
        Eigen::VectorXd polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals, int order);

        void odomCB(const nav_msgs::Odometry::ConstPtr& odomMsg);
        void pathCB(const nav_msgs::Path::ConstPtr& pathMsg);
        void goalCB(const geometry_msgs::PoseStamped::ConstPtr& goalMsg);
        void amclCB(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& amclMsg);
        void controlLoopCB(const ros::TimerEvent&);
        void estimateDisturbance(const nav_msgs::Odometry& odom);
        void visualizeTube();
};

#endif
