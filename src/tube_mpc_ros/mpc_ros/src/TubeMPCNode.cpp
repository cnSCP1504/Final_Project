#define HAVE_CSTDDEF
#define HAVE_CSTDDEF
#include "TubeMPCNode.h"
#include <cppad/ipopt/solve.hpp>
#include <Eigen/Core>
#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>
#undef HAVE_CSTDDEF

using namespace std;
using namespace Eigen;

TubeMPCNode::TubeMPCNode()
{
    ros::NodeHandle pn("~");

    pn.param("thread_numbers", _thread_numbers, 2);
    pn.param("pub_twist_cmd", _pub_twist_flag, true);
    pn.param("debug_info", _debug_info, true);
    pn.param("max_speed", _max_speed, 0.50);
    pn.param("waypoints_dist", _waypointsDist, -1.0);
    pn.param("path_length", _pathLength, 8.0);
    pn.param("goal_radius", _goalRadius, 0.5);
    pn.param("controller_freq", _controller_freq, 10);
    
    _dt = double(1.0/_controller_freq);

    pn.param("mpc_steps", _mpc_steps, 20.0);
    pn.param("mpc_ref_cte", _ref_cte, 0.0);
    pn.param("mpc_ref_vel", _ref_vel, 1.0);
    pn.param("mpc_ref_etheta", _ref_etheta, 0.0);
    pn.param("mpc_w_cte", _w_cte, 50.0);
    pn.param("mpc_w_etheta", _w_etheta, 50.0);
    pn.param("mpc_w_vel", _w_vel, 10.0);  // 修复：降低默认值，鼓励机器人移动
    pn.param("mpc_w_angvel", _w_angvel, 100.0);
    pn.param("mpc_w_accel", _w_accel, 50.0);
    pn.param("mpc_max_angvel", _max_angvel, 2.0);  // 修复：提高角速度限制，允许正常转向
    pn.param("mpc_max_throttle", _max_throttle, 1.0);
    pn.param("mpc_bound_value", _bound_value, 1.0e3);
    pn.param("disturbance_bound", _disturbance_bound, 0.1);

    pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
    pn.param<std::string>("goal_topic", _goal_topic, "/move_base_simple/goal");
    pn.param<std::string>("map_frame", _map_frame, "map");
    pn.param<std::string>("odom_frame", _odom_frame, "odom");
    pn.param<std::string>("car_frame", _car_frame, "base_footprint");

    // Safe-Regret MPC Integration Mode
    pn.param("enable_safe_regret_integration", _enable_safe_regret_integration, false);

    cout << "\n===== Tube MPC Parameters =====" << endl;
    cout << "pub_twist_cmd: "  << _pub_twist_flag << endl;
    cout << "debug_info: "  << _debug_info << endl;
    cout << "frequency: "   << _dt << endl;
    cout << "mpc_steps: "   << _mpc_steps << endl;
    cout << "mpc_ref_vel: " << _ref_vel << endl;
    cout << "mpc_w_cte: "   << _w_cte << endl;
    cout << "mpc_w_etheta: "  << _w_etheta << endl;
    cout << "mpc_max_angvel: "  << _max_angvel << endl;
    cout << "disturbance_bound: " << _disturbance_bound << endl;

    _sub_odom   = _nh.subscribe("/odom", 1, &TubeMPCNode::odomCB, this);
    _sub_path   = _nh.subscribe( _globalPath_topic, 1, &TubeMPCNode::pathCB, this);
    _sub_goal   = _nh.subscribe( _goal_topic, 1, &TubeMPCNode::goalCB, this);
    _sub_amcl   = _nh.subscribe("/amcl_pose", 5, &TubeMPCNode::amclCB, this);
    
    _pub_globalpath  = _nh.advertise<nav_msgs::Path>("/global_path", 1);
    _pub_odompath  = _nh.advertise<nav_msgs::Path>("/mpc_reference", 1);
    _pub_mpctraj   = _nh.advertise<nav_msgs::Path>("/mpc_trajectory", 1);
    _pub_tube     = _nh.advertise<nav_msgs::Path>("/tube_boundaries", 1);
    _pub_tracking_error = _nh.advertise<std_msgs::Float64MultiArray>("/tube_mpc/tracking_error", 1);  // DR Tightening

    // Safe-Regret MPC Integration: 发布话题根据模式决定
    if(_pub_twist_flag) {
        if (_enable_safe_regret_integration) {
            // 集成模式：发布原始命令给safe_regret_mpc
            _pub_twist = _nh.advertise<geometry_msgs::Twist>("/cmd_vel_raw", 1);
            cout << "Safe-Regret MPC Integration: ENABLED" << endl;
            cout << "  Publishing to /cmd_vel_raw (safe_regret_mpc will process)" << endl;
        } else {
            // 独立模式：直接发布cmd_vel
            _pub_twist = _nh.advertise<geometry_msgs::Twist>("/cmd_vel", 1);
            cout << "Safe-Regret MPC Integration: DISABLED (standalone mode)" << endl;
        }
    }

    _timer1 = _nh.createTimer(ros::Duration((1.0)/_controller_freq), &TubeMPCNode::controlLoopCB, this);

    _goal_received = false;
    _goal_reached  = false;
    _path_computed = false;
    _throttle = 0.0;
    _w = 0.0;
    _w_filtered = 0.0;
    _speed = 0.5;  // 提高初始速度，避免从零开始

    _twist_msg = geometry_msgs::Twist();
    _mpc_traj = nav_msgs::Path();
    _tube_path = nav_msgs::Path();

    idx = 0;
    file.open("/tmp/tube_mpc_data.csv");
    file << "idx"<< "," << "cte" << "," <<  "etheta" << "," << "cmd_vel.linear.x" << "," << "cmd_vel.angular.z" << "," << "tube_radius" << "\n";

    _w_residuals = VectorXd::Zero(6);
    _num_residuals = 0;
    _e_current = VectorXd::Zero(6);

    // Initialize metrics collector
    pn.param<std::string>("metrics_output_csv", _metrics_output_csv, "/tmp/tube_mpc_metrics.csv");
    pn.param("target_risk_delta", _target_delta, 0.1);  // Default 10% risk

    _metrics_collector = Metrics::MetricsCollector(_metrics_output_csv);
    _metrics_collector.setTargetRisk(_target_delta);
    _metrics_collector.setupROSPublishers(_nh);

    ROS_INFO("MetricsCollector initialized with CSV output: %s", _metrics_output_csv.c_str());

    // STL integration parameters
    pn.param("enable_stl_constraints", _enable_stl_constraints, false);
    pn.param("stl_weight", _stl_weight, 10.0);

    // Initialize STL state variables
    _current_stl_robustness = 1.0;  // Start with positive robustness
    _current_stl_budget = 1.0;
    _current_stl_violation = false;
    _stl_ref_cte_adjustment = 0.0;
    _stl_ref_vel_adjustment = 0.0;

    if(_enable_stl_constraints)
    {
        ROS_INFO("STL constraints enabled with weight: %.2f", _stl_weight);

        // Subscribe to STL topics
        _sub_stl_robustness = _nh.subscribe("/stl/robustness", 1, &TubeMPCNode::stlRobustnessCB, this);
        _sub_stl_budget = _nh.subscribe("/stl/budget", 1, &TubeMPCNode::stlBudgetCB, this);
        _sub_stl_violation = _nh.subscribe("/stl/violation", 1, &TubeMPCNode::stlViolationCB, this);

        ROS_INFO("Subscribed to STL topics: /stl/robustness, /stl/budget, /stl/violation");
    }
    else
    {
        ROS_INFO("STL constraints disabled");
    }

    // Terminal set integration (P1-1)
    pn.param("enable_terminal_set", _enable_terminal_set, false);

    if(_enable_terminal_set)
    {
        ROS_INFO("Terminal set constraints enabled");

        // Subscribe to terminal set topic
        _sub_terminal_set = _nh.subscribe("/dr_terminal_set", 1,
            &TubeMPCNode::terminalSetCB, this);

        // Publisher for terminal set visualization
        _pub_terminal_set_viz = _nh.advertise<visualization_msgs::Marker>(
            "/terminal_set_viz", 1);

        // Initialize terminal set state
        _terminal_set_received = false;
        _terminal_set_center = VectorXd::Zero(6);
        _terminal_set_radius = 0.0;
        _terminal_set_violation_count = 0;

        ROS_INFO("Subscribed to terminal set topic: /dr_terminal_set");
    }
    else
    {
        ROS_INFO("Terminal set constraints disabled");
    }

    _tube_mpc_params["DT"] = _dt;
    _tube_mpc_params["STEPS"]    = _mpc_steps;
    _tube_mpc_params["REF_CTE"]  = _ref_cte;
    _tube_mpc_params["REF_ETHETA"] = _ref_etheta;
    _tube_mpc_params["REF_V"]    = _ref_vel;
    _tube_mpc_params["W_CTE"]    = _w_cte;
    _tube_mpc_params["W_EPSI"]   = _w_etheta;
    _tube_mpc_params["W_V"]      = _w_vel;
    _tube_mpc_params["W_ANGVEL"]  = _w_angvel;
    _tube_mpc_params["W_A"]      = _w_accel;
    _tube_mpc_params["ANGVEL"]   = _max_angvel;
    _tube_mpc_params["MAXTHR"]   = _max_throttle;
    _tube_mpc_params["BOUND"]    = _bound_value;

    MatrixXd E_set = MatrixXd::Identity(6, 1) * _disturbance_bound;
    _tube_mpc.setDisturbanceSet(E_set);
    _tube_mpc.LoadParams(_tube_mpc_params);
}

int TubeMPCNode::get_thread_numbers()
{
    return _thread_numbers;
}

double TubeMPCNode::polyeval(Eigen::VectorXd coeffs, double x) 
{
    double result = 0.0;
    for (int i = 0; i < coeffs.size(); i++) 
    {
        result += coeffs[i] * pow(x, i);
    }
    return result;
}

Eigen::VectorXd TubeMPCNode::polyfit(Eigen::VectorXd xvals, Eigen::VectorXd yvals, int order) 
{
    assert(xvals.size() == yvals.size());
    assert(order >= 1 && order <= xvals.size() - 1);
    Eigen::MatrixXd A(xvals.size(), order + 1);

    for (int i = 0; i < xvals.size(); i++)
        A(i, 0) = 1.0;

    for (int j = 0; j < xvals.size(); j++) 
    {
        for (int i = 0; i < order; i++) 
            A(j, i + 1) = A(j, i) * xvals(j);
    }
    auto Q = A.householderQr();
    auto result = Q.solve(yvals);
    return result;
}

void TubeMPCNode::odomCB(const nav_msgs::Odometry::ConstPtr& odomMsg)
{
    _odom = *odomMsg;
    estimateDisturbance(_odom);
}

void TubeMPCNode::pathCB(const nav_msgs::Path::ConstPtr& pathMsg)
{
    if(_goal_received && !_goal_reached)
    {
        nav_msgs::Path odom_path = nav_msgs::Path();

        // TF变换时序修复：等待TF可用
        bool tf_available = false;
        int tf_retry_count = 0;
        const int max_tf_retries = 5;

        while (!tf_available && tf_retry_count < max_tf_retries)
        {
            try
            {
                // 检查TF是否可用
                if (!_tf_listener.canTransform(_odom_frame, _map_frame, ros::Time(0)))
                {
                    ROS_WARN("Waiting for transform from %s to %s... (attempt %d/%d)",
                             _map_frame.c_str(), _odom_frame.c_str(),
                             tf_retry_count + 1, max_tf_retries);

                    // 等待TF变得可用
                    _tf_listener.waitForTransform(_odom_frame, _map_frame,
                                                   ros::Time(0), ros::Duration(2.0));
                }
                tf_available = true;
            }
            catch (tf::TransformException &ex)
            {
                tf_retry_count++;
                if (tf_retry_count < max_tf_retries)
                {
                    ROS_WARN("TF lookup failed, retrying... (%d/%d): %s",
                             tf_retry_count, max_tf_retries, ex.what());
                    ros::Duration(1.0).sleep();
                }
                else
                {
                    ROS_ERROR("TF transform failed after %d attempts: %s",
                              max_tf_retries, ex.what());
                    return;  // 放弃本次路径更新
                }
            }
        }

        try
        {
            double total_length = 0.0;
            int sampling = _downSampling;

            if(_waypointsDist <=0.0)
            {
                double dx = pathMsg->poses[1].pose.position.x - pathMsg->poses[0].pose.position.x;
                double dy = pathMsg->poses[1].pose.position.y - pathMsg->poses[0].pose.position.y;
                _waypointsDist = sqrt(dx*dx + dy*dy);
                _downSampling = int(_pathLength/10.0/_waypointsDist);
            }

            for(int i =0; i< pathMsg->poses.size(); i++)
            {
                if(total_length > _pathLength)
                    break;

                if(sampling == _downSampling)
                {
                    geometry_msgs::PoseStamped tempPose;
                    _tf_listener.transformPose(_odom_frame, ros::Time(0) , pathMsg->poses[i], _map_frame, tempPose);
                    odom_path.poses.push_back(tempPose);
                    sampling = 0;
                }
                total_length = total_length + _waypointsDist;
                sampling = sampling + 1;
            }

            if(odom_path.poses.size() >= 4 )  // 需要至少4个点才能进行3阶多项式拟合
            {
                _odom_path = odom_path;
                _path_computed = true;
                odom_path.header.frame_id = _odom_frame;
                odom_path.header.stamp = ros::Time::now();
                _pub_odompath.publish(odom_path);

                ROS_INFO("Path transformation successful: %d points transformed from %s to %s",
                         (int)odom_path.poses.size(), _map_frame.c_str(), _odom_frame.c_str());
            }
            else  // 路径点数<4，检查是否已经到达目标
            {
                // 检查机器人是否已经非常接近目标
                try {
                    tf::StampedTransform transform;
                    _tf_listener.lookupTransform(_map_frame, _car_frame, ros::Time(0), transform);

                    double car2goal_x = _goal_pos.x - transform.getOrigin().x();
                    double car2goal_y = _goal_pos.y - transform.getOrigin().y();
                    double dist2goal = sqrt(car2goal_x*car2goal_x + car2goal_y*car2goal_y);

                    // 使用严格的到达阈值（goalRadius）确保精确到达目标
                    double arrival_threshold = _goalRadius;  // 0.5米（严格模式）
                    if(dist2goal < arrival_threshold)
                    {
                        _goal_reached = true;
                        _path_computed = false;
                        ROS_INFO("Goal reached! Path has only %d point(s), distance to goal: %.2f m < %.2f m",
                                 (int)odom_path.poses.size(), dist2goal, arrival_threshold);
                        cout << "=== Goal Reached ===" << endl;
                    }
                    else
                    {
                        // ⚠️ 关键修复：即使路径点数少，如果距离目标还远，仍然允许控制
                        // 只要有至少2个点，就可以继续控制
                        if(odom_path.poses.size() >= 2)
                        {
                            _path_computed = true;  // 允许继续控制
                            ROS_WARN_THROTTLE(2.0, "Path has only %d point(s), distance to goal: %.2f m. Continuing control...",
                                              (int)odom_path.poses.size(), dist2goal);
                        }
                        else
                        {
                            // 路径点数太少（<2），无法控制
                            ROS_WARN_THROTTLE(2.0, "Path has only %d point(s), distance to goal: %.2f m. Waiting for replan...",
                                              (int)odom_path.poses.size(), dist2goal);
                            _path_computed = false;
                        }
                    }
                }
                catch(tf::TransformException& ex) {
                    ROS_WARN_THROTTLE(2.0, "Cannot check goal distance: %s", ex.what());
                    _path_computed = false;
                }
                _waypointsDist = -1;
            }
        }
        catch(tf::TransformException &ex)
        {
            ROS_ERROR("Transform failed during path processing: %s", ex.what());
            _path_computed = false;
            _waypointsDist = -1;
        }
    }
}

void TubeMPCNode::goalCB(const geometry_msgs::PoseStamped::ConstPtr& goalMsg)
{
    _goal_pos = goalMsg->pose.position;
    _goal_received = true;
    _goal_reached = false;
    ROS_INFO("Goal Received :goalCB!");
}

void TubeMPCNode::amclCB(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& amclMsg)
{
    if(_goal_received)
    {
        double car2goal_x = _goal_pos.x - amclMsg->pose.pose.position.x;
        double car2goal_y = _goal_pos.y - amclMsg->pose.pose.position.y;
        double dist2goal = sqrt(car2goal_x*car2goal_x + car2goal_y*car2goal_y);
        if(dist2goal < _goalRadius)
        {
            if(start_timef)
            {
                tracking_etime = ros::Time::now();
                tracking_time_sec = tracking_etime.sec - tracking_stime.sec;
                tracking_time_nsec = tracking_etime.nsec - tracking_stime.nsec;

                file << "tracking time"<< "," << tracking_time_sec << "," <<  tracking_time_nsec << "\n";

                // Print metrics summary at goal
                printMetricsSummary();

                file.close();
                _metrics_collector.closeCSV();  // Close metrics CSV

                start_timef = false;
            }
            _goal_received = false;
            _goal_reached = true;
            _path_computed = false;
            ROS_INFO("Goal Reached !");
            cout << "tracking time: " << tracking_time_sec << "." << tracking_time_nsec << endl;

        }
    }
}

void TubeMPCNode::estimateDisturbance(const nav_msgs::Odometry& odom)
{
    static VectorXd x_prev = VectorXd::Zero(6);
    static VectorXd u_prev = VectorXd::Zero(6);
    
    VectorXd x_curr(6);
    x_curr << odom.pose.pose.position.x, odom.pose.pose.position.y,
             tf::getYaw(odom.pose.pose.orientation),
             odom.twist.twist.linear.x, 0, 0;
    
    VectorXd w_estimate = x_curr - (x_prev + u_prev);
    
    _w_residuals += w_estimate.cwiseAbs();
    _num_residuals++;
    
    if(_num_residuals >= 100)
    {
        double new_bound = _w_residuals.maxCoeff() / _num_residuals * 2.0;
        if(abs(new_bound - _disturbance_bound) > 0.01)
        {
            _disturbance_bound = new_bound;
            _tube_mpc.setDisturbanceBound(_disturbance_bound);
            cout << "Updated disturbance bound: " << _disturbance_bound << endl;
        }
        
        _w_residuals.setZero();
        _num_residuals = 0;
    }
    
    x_prev = x_curr;
    u_prev << _w, _throttle, 0, 0, 0, 0;
}

void TubeMPCNode::controlLoopCB(const ros::TimerEvent&)
{
    // 调试输出：显示控制状态
    static int debug_counter = 0;
    if(debug_counter++ % 50 == 0) {  // 每5秒输出一次状态
        cout << "=== Control Loop Status ===" << endl;
        cout << "Goal received: " << (_goal_received ? "YES" : "NO") << endl;
        cout << "Goal reached: " << (_goal_reached ? "YES" : "NO") << endl;
        cout << "Path computed: " << (_path_computed ? "YES" : "NO") << endl;
        cout << "Current speed: " << _speed << " m/s" << endl;
        cout << "Max speed: " << _max_speed << " m/s" << endl;
        cout << "========================" << endl;
    }

    if(_goal_received && !_goal_reached && _path_computed )
    {
        if(!start_timef)
        {
            tracking_stime = ros::Time::now();
            start_timef = true;
        }
        
        nav_msgs::Odometry odom = _odom; 
        nav_msgs::Path odom_path = _odom_path;   
        geometry_msgs::Point goal_pos = _goal_pos;

        const double px = odom.pose.pose.position.x;
        const double py = odom.pose.pose.position.y;
        tf::Pose pose;
        tf::poseMsgToTF(odom.pose.pose, pose);
        double theta = tf::getYaw(pose.getRotation());
        const double v = odom.twist.twist.linear.x;

        const int N = odom_path.poses.size();

        // Safety check: Need at least 2 points for 1st order polynomial (linear)
        // The polynomial order will be adjusted adaptively below
        if(N < 2) {
            // 检查是否已经非常接近目标（严格模式：使用goalRadius）
            const double x_err = goal_pos.x - px;
            const double y_err = goal_pos.y - py;
            const double goal_err = sqrt(x_err*x_err + y_err*y_err);

            if(goal_err < _goalRadius) {
                // 非常接近目标，标记为到达
                if(!_goal_reached) {
                    _goal_reached = true;
                    ROS_INFO("Goal reached! Distance: %.2f m < %.2f m", goal_err, _goalRadius);
                }
            } else {
                ROS_WARN_THROTTLE(1.0, "Path has only %d point(s), distance to goal: %.2f m. Waiting for replan...", N, goal_err);
            }
            return;
        }

        const double costheta = cos(theta);
        const double sintheta = sin(theta);

        VectorXd x_veh(N);
        VectorXd y_veh(N);
        for(int i = 0; i < N; i++)
        {
            const double dx = odom_path.poses[i].pose.position.x - px;
            const double dy = odom_path.poses[i].pose.position.y - py;
            x_veh[i] = dx * costheta + dy * sintheta;
            y_veh[i] = dy * costheta - dx * sintheta;
        }

        // Use lower polynomial order if we don't have enough points
        int poly_order = (N >= 6) ? 3 : (N >= 4 ? 2 : 1);
        auto coeffs = polyfit(x_veh, y_veh, poly_order); 
        const double cte  = polyeval(coeffs, 0.0);
        double etheta = atan(coeffs[1]);

        double gx = 0;
        double gy = 0;
        int N_sample = N * 0.3;
        for(int i = 1; i < N_sample; i++) 
        {
            gx += odom_path.poses[i].pose.position.x - odom_path.poses[i-1].pose.position.x;
            gy += odom_path.poses[i].pose.position.y - odom_path.poses[i-1].pose.position.y;
        }       
        
        // Calculate heading error with proper angle normalization
        // This fixes the issue where large angle turns (>90°) caused reverse movement
        double traj_deg = atan2(gy,gx);
        double PI = 3.141592653589793;

        // CRITICAL FIX: Use traj_deg - theta (not theta - traj_deg)
        // This ensures correct sign: negative = clockwise, positive = counter-clockwise
        if(gx && gy) {
            double angle_diff = traj_deg - theta;  // CORRECTED: swapped order
            // Normalize to [-π, π] using atan2
            etheta = atan2(sin(angle_diff), cos(angle_diff));

            // Additional normalization: ensure |etheta| stays within reasonable bounds
            // If angle is very close to ±π, keep it as-is (MPC can handle it)
            // The atan2 normalization above is sufficient for most cases
        } else {
            etheta = 0;
        }

        cout << "etheta: "<< etheta << ", traj_deg: " << traj_deg << ", theta: " << theta << endl;

        idx++;
        file << idx<< "," << cte << "," <<  etheta << "," << _twist_msg.linear.x << "," << _twist_msg.angular.z << "," << _tube_mpc.getTubeRadius() << "\n";

        const double x_err = goal_pos.x -  odom.pose.pose.position.x;
        const double y_err = goal_pos.y -  odom.pose.pose.position.y;
        const double goal_err = sqrt(x_err*x_err + y_err*y_err);

        cout << "x_err:"<< x_err << ", y_err:"<< y_err  << endl;

        VectorXd state(6);
        state << 0, 0, 0, v, cte, etheta;

        VectorXd nominal_state(6);
        nominal_state << 0, 0, 0, v, cte, etheta;

        // =====================================================================
        // STL Integration: Adjust references based on STL state (P0-3)
        // =====================================================================
        if (_enable_stl_constraints)
        {
            // Create local copies of reference values for adjustment
            double adjusted_ref_cte = _ref_cte;
            double adjusted_ref_vel = _ref_vel;

            // Apply STL-aware adjustments
            adjustReferenceForSTL(adjusted_ref_cte, adjusted_ref_vel);

            // Update MPC parameters with adjusted references
            _tube_mpc_params["REF_CTE"] = adjusted_ref_cte;
            _tube_mpc_params["REF_V"] = adjusted_ref_vel;

            // Reload parameters into MPC solver
            _tube_mpc.LoadParams(_tube_mpc_params);

            // Log STL integration status occasionally
            static int stl_log_counter = 0;
            if(stl_log_counter++ % 50 == 0) {
                ROS_INFO("STL-MPC Integration: Ref_CTE: %.3f (orig: %.3f), Ref_Vel: %.3f (orig: %.3f)",
                         adjusted_ref_cte, _ref_cte, adjusted_ref_vel, _ref_vel);
            }
        }

        ros::Time begin = ros::Time::now();
        vector<double> mpc_results = _tube_mpc.Solve(state, coeffs);
        ros::Time end = ros::Time::now();

        // Calculate solve time in milliseconds
        double solve_time_ms = (end - begin).toSec() * 1000.0;
        bool mpc_feasible = !mpc_results.empty();  // Check if MPC solve was successful

        cout << "MPC Solve Time: " << solve_time_ms << " ms, Feasible: " << mpc_feasible << endl;

        // Terminal set feasibility check (P1-1)
        if (mpc_feasible && _enable_terminal_set && _terminal_set_received)
        {
            // Extract terminal state from MPC prediction
            // The terminal state is the last nominal state in the MPC horizon
            VectorXd terminal_state = _tube_mpc.getNominalState();

            // Check terminal feasibility
            bool terminal_feasible = checkTerminalFeasibilityInLoop(terminal_state);

            if (!terminal_feasible && _debug_info)
            {
                ROS_WARN("Terminal feasibility violated - MPC solution may not guarantee recursive feasibility");
            }
        }

        _w = mpc_feasible ? mpc_results[0] : 0.0;
        _throttle = mpc_feasible ? mpc_results[1] : 0.0;

        _w_filtered = 0.5 * _w_filtered + 0.5 * _w;

        // 速度计算前的调试信息
        if(debug_counter % 20 == 0) {  // 每2秒输出一次详细信息
            cout << "=== MPC Results ===" << endl;
            cout << "MPC feasible: " << (mpc_feasible ? "YES" : "NO") << endl;
            cout << "MPC results size: " << mpc_results.size() << endl;
            if(mpc_results.size() >= 2) {
                cout << "w (ang_vel): " << mpc_results[0] << endl;
                cout << "throttle: " << mpc_results[1] << endl;
            }
            cout << "Current v (odom): " << v << endl;
            cout << "_dt: " << _dt << endl;
        }

        _speed = v + _throttle;  // 修复：throttle直接作为速度增量，不乘以dt

        if(debug_counter % 20 == 0) {
            cout << "Computed speed (before limits): " << _speed << endl;
        }

        // === 智能转向策略：避免倒退 ===
        // 暂时禁用以排查问题
        /*
        const double HEADING_ERROR_THRESHOLD = 1.05;  // 60度 (弧度)
        const double HEADING_ERROR_CRITICAL = 1.57;   // 90度 (弧度)

        if(std::abs(etheta) > HEADING_ERROR_THRESHOLD) {
            // 航向误差过大，进入转向优先模式
            if(std::abs(etheta) > HEADING_ERROR_CRITICAL) {
                // 严重偏离：完全停止前进，只旋转
                _speed = 0.0;
                if(debug_counter % 20 == 0) {
                    cout << "⚠️  CRITICAL HEADING ERROR: Pure rotation mode" << endl;
                    cout << "   etheta: " << etheta << " rad (" << (etheta * 180.0 / M_PI) << " deg)" << endl;
                }
            } else {
                // 中等偏离：大幅减速，允许慢速转向
                _speed = _speed * 0.2;  // 降低到20%
                if(debug_counter % 20 == 0) {
                    cout << "⚠️  High heading error: Reduced speed for turning" << endl;
                    cout << "   etheta: " << etheta << " rad (" << (etheta * 180.0 / M_PI) << " deg)" << endl;
                    cout << "   Speed reduced to: " << _speed << " m/s" << endl;
                }
            }
        }
        */

        // === Collect Metrics ===
        Metrics::MetricsCollector::MetricsSnapshot snapshot;
        snapshot.timestamp = ros::Time::now().toSec();
        snapshot.cte = cte;
        snapshot.etheta = etheta;
        snapshot.vel = _speed;
        snapshot.angvel = _w_filtered;

        // Tracking error norm (from current error state)
        snapshot.tracking_error_norm = _e_current.norm();

        // Tube status
        snapshot.tube_radius = _tube_mpc.getTubeRadius();
        double tube_threshold = _tube_mpc.getTubeRadius() * 1.1;  // 10% tolerance
        snapshot.tube_violation = (snapshot.tracking_error_norm > tube_threshold) ?
            snapshot.tracking_error_norm - tube_threshold : 0.0;

        // Safety constraint (example: distance from collision)
        // For now, use a simple safety margin based on cte and etheta
        // In Phase 3, this will be replaced by actual h(x) from DR constraints
        double safety_margin = 1.0 - (std::abs(cte) + std::abs(etheta) * 0.5);
        snapshot.safety_margin = safety_margin;
        snapshot.safety_violation = (safety_margin < 0.0) ? 1.0 : 0.0;

        // MPC solve metrics
        snapshot.mpc_solve_time = solve_time_ms;
        snapshot.mpc_feasible = mpc_feasible;

        // Cost computation (stage cost ℓ(x,u))
        double stage_cost = _w_cte * cte * cte +
                           _w_etheta * etheta * etheta +
                           _w_vel * (_speed - _ref_vel) * (_speed - _ref_vel) +
                           _w_angvel * _w_filtered * _w_filtered +
                           _w_accel * _throttle * _throttle;
        snapshot.cost = stage_cost;

        // Reference cost (for regret calculation)
        // For now, use a simple heuristic - in Phase 4, this will come from reference planner
        double reference_cost = _w_cte * 0.5 * 0.5 + _w_etheta * 0.1 * 0.1;  // Target: small errors
        snapshot.regret_dynamic = stage_cost - reference_cost;
        snapshot.regret_safe = snapshot.regret_dynamic;  // Same for now

        // STL robustness (placeholder for Phase 2)
        snapshot.robustness = safety_margin;  // Simple proxy for now

        // Record metrics
        _metrics_collector.recordMetrics(snapshot);

        // Update regret
        _metrics_collector.updateRegret(stage_cost, reference_cost, reference_cost);

        // Publish metrics every N steps
        if (idx % 10 == 0) {
            _metrics_collector.publishMetrics();
        }
        if (_speed >= _max_speed)
            _speed = _max_speed;
        if(_speed < 0.3)  // 最小速度阈值
            _speed = 0.3;

        if(_debug_info)
        {
            cout << "\n\nDEBUG" << endl;
            cout << "theta: " << theta << endl;
            cout << "V: " << v << endl;
            cout << "coeffs: \n" << coeffs << endl;
            cout << "_w: \n" << _w << endl;
            cout << "_throttle: \n" << _throttle << endl;
            cout << "_speed: \n" << _speed << endl;
            cout << "Tube radius: " << _tube_mpc.getTubeRadius() << endl;
        }

        _mpc_traj = nav_msgs::Path();
        _mpc_traj.header.frame_id = _car_frame;
        _mpc_traj.header.stamp = ros::Time::now();

        geometry_msgs::PoseStamped tempPose;
        tf2::Quaternion myQuaternion;

        for(int i=0; i<_tube_mpc.mpc_x.size(); i++)
        {
            tempPose.header = _mpc_traj.header;
            tempPose.pose.position.x = _tube_mpc.mpc_x[i];
            tempPose.pose.position.y = _tube_mpc.mpc_y[i];

            myQuaternion.setRPY( 0, 0, _tube_mpc.mpc_theta[i] );
            tempPose.pose.orientation.x = myQuaternion[0];
            tempPose.pose.orientation.y = myQuaternion[1];
            tempPose.pose.orientation.z = myQuaternion[2];
            tempPose.pose.orientation.w = myQuaternion[3];

            _mpc_traj.poses.push_back(tempPose);
        }
        _pub_mpctraj.publish(_mpc_traj);

        // DR Tightening: Publish tracking error for DR constraint tightening
        std_msgs::Float64MultiArray tracking_error_msg;
        tracking_error_msg.data.clear();
        tracking_error_msg.data.push_back(cte);           // Cross-track error
        tracking_error_msg.data.push_back(etheta);         // Heading error
        tracking_error_msg.data.push_back(_e_current.norm());  // Error norm
        tracking_error_msg.data.push_back(_tube_mpc.getTubeRadius());  // Current tube radius
        _pub_tracking_error.publish(tracking_error_msg);

        visualizeTube();
    }
    else
    {
        _throttle = 0.0;
        _speed = 0.0;
        _w = 0;
        _w_filtered = 0;
    }
    
    if(_pub_twist_flag)
    {
        _twist_msg.linear.x  = _speed;
        _twist_msg.angular.z = _w_filtered;
        _pub_twist.publish(_twist_msg);

        // 调试输出：显示发布的控制命令
        if(debug_counter % 50 == 0) {  // 每5秒输出一次
            cout << "=== Published Control Command ===" << endl;
            cout << "linear.x: " << _twist_msg.linear.x << " m/s" << endl;
            cout << "angular.z: " << _twist_msg.angular.z << " rad/s" << endl;
            cout << "Publisher active: " << (_pub_twist_flag ? "YES" : "NO") << endl;
            cout << "==================================" << endl;
        }
    }
}

void TubeMPCNode::visualizeTube()
{
    double tube_radius = _tube_mpc.getTubeRadius();
    
    _tube_path = nav_msgs::Path();
    _tube_path.header.frame_id = _car_frame;
    _tube_path.header.stamp = ros::Time::now();
    
    for(int i = 0; i < _tube_mpc.mpc_x.size(); i++)
    {
        geometry_msgs::PoseStamped tempPose;
        tempPose.header = _tube_path.header;
        tempPose.pose.position.x = _tube_mpc.mpc_x[i] + tube_radius;
        tempPose.pose.position.y = _tube_mpc.mpc_y[i];
        tempPose.pose.position.z = 0;
        tempPose.pose.orientation.w = 1.0;
        _tube_path.poses.push_back(tempPose);
        
        tempPose.pose.position.x = _tube_mpc.mpc_x[i] - tube_radius;
        _tube_path.poses.push_back(tempPose);
        
        tempPose.pose.position.x = _tube_mpc.mpc_x[i];
        tempPose.pose.position.y = _tube_mpc.mpc_y[i] + tube_radius;
        _tube_path.poses.push_back(tempPose);
        
        tempPose.pose.position.y = _tube_mpc.mpc_y[i] - tube_radius;
        _tube_path.poses.push_back(tempPose);
    }
    
    _pub_tube.publish(_tube_path);
}

void TubeMPCNode::printMetricsSummary()
{
    std::cout << "\n";
    _metrics_collector.printSummary();

    // Print confidence interval for satisfaction probability
    auto ci = _metrics_collector.computeSatisfactionCI(0.95);
    std::cout << "\n95% Confidence Interval for Satisfaction Probability: ["
              << std::fixed << std::setprecision(4)
              << ci.first << ", " << ci.second << "]" << std::endl;

    // Write detailed analysis to file
    std::ofstream summary_file("/tmp/tube_mpc_summary.txt");
    if (summary_file.is_open()) {
        summary_file << "Tube MPC Performance Summary\n";
        summary_file << "==============================\n\n";

        auto metrics = _metrics_collector.getAggregatedMetrics();

        summary_file << "Safety Metrics:\n";
        summary_file << "  Empirical Risk (δ̂):       " << metrics.empirical_risk << "\n";
        summary_file << "  Target Risk (δ):          " << _target_delta << "\n";
        summary_file << "  Calibration Error:        " << metrics.calibration_error << "\n\n";

        summary_file << "Performance Metrics:\n";
        summary_file << "  Feasibility Rate:         " << metrics.feasibility_rate * 100 << "%\n";
        summary_file << "  Mean Tracking Error:      " << metrics.mean_tracking_error << "\n";
        summary_file << "  Median Solve Time:        " << metrics.median_solve_time_ms << " ms\n\n";

        summary_file << "Regret Analysis:\n";
        summary_file << "  Cumulative Dynamic:       " << metrics.cumulative_dynamic_regret << "\n";
        summary_file << "  Average Dynamic:          " << metrics.average_dynamic_regret << "\n\n";

        summary_file << "95% CI for Satisfaction: [" << ci.first << ", " << ci.second << "]\n";

        summary_file.close();
        ROS_INFO("Detailed summary written to /tmp/tube_mpc_summary.txt");
    }
}

// ============================================================================
// STL Integration Callback Functions (P0-3)
// ============================================================================

void TubeMPCNode::stlRobustnessCB(const std_msgs::Float32::ConstPtr& msg)
{
    _current_stl_robustness = msg->data;

    // Log occasionally for debugging
    static int robustness_log_counter = 0;
    if(robustness_log_counter++ % 50 == 0) {
        ROS_INFO("STL Robustness: %.3f, Budget: %.3f, Violation: %s",
                 _current_stl_robustness, _current_stl_budget,
                 _current_stl_violation ? "YES" : "NO");
    }
}

void TubeMPCNode::stlBudgetCB(const std_msgs::Float32::ConstPtr& msg)
{
    _current_stl_budget = msg->data;
}

void TubeMPCNode::stlViolationCB(const std_msgs::Bool::ConstPtr& msg)
{
    _current_stl_violation = msg->data;
}

void TubeMPCNode::adjustReferenceForSTL(double& ref_cte, double& ref_vel)
{
    /**
     * STL-aware reference adjustment
     *
     * Strategy: When STL constraints are violated, adjust MPC references to:
     * 1. Reduce cross-track error (move toward path center)
     * 2. Adjust velocity based on budget and violation status
     *
     * This is a simplified approach that doesn't modify the CppAD cost function
     * directly, but adjusts the reference values that MPC tries to track.
     */

    if (!_enable_stl_constraints) {
        // STL not enabled, no adjustment
        _stl_ref_cte_adjustment = 0.0;
        _stl_ref_vel_adjustment = 0.0;
        return;
    }

    _stl_ref_cte_adjustment = 0.0;
    _stl_ref_vel_adjustment = 0.0;

    // Only adjust if we have valid robustness data
    if (_current_stl_robustness == 0.0 && _current_stl_budget == 0.0) {
        // No STL data yet
        return;
    }

    /**
     * Case 1: STL Violation detected (negative robustness)
     * Action: Adjust reference to improve STL satisfaction
     */
    if (_current_stl_violation || _current_stl_robustness < 0.0)
    {
        // Negative robustness means we're far from satisfying STL
        // Adjust CTE reference to move more aggressively toward path
        double violation_severity = std::abs(_current_stl_robustness);
        _stl_ref_cte_adjustment = -_stl_weight * violation_severity * 0.1;

        // Reduce velocity when violating to give more time for correction
        _stl_ref_vel_adjustment = -_stl_weight * violation_severity * 0.05;
    }

    /**
     * Case 2: Low budget (接近耗尽)
     * Action: Be conservative, reduce speed
     */
    else if (_current_stl_budget < 0.3)
    {
        // Low budget: reduce velocity to avoid further violations
        double budget_deficit = 0.3 - _current_stl_budget;
        _stl_ref_vel_adjustment = -_stl_weight * budget_deficit * 0.1;

        // Slightly tighten CTE tracking
        _stl_ref_cte_adjustment = -_stl_weight * budget_deficit * 0.05;
    }

    /**
     * Case 3: High budget + positive robustness (安全状态)
     * Action: Can be more aggressive with tracking
     */
    else if (_current_stl_budget > 0.7 && _current_stl_robustness > 0.5)
    {
        // High safety margin: can focus on performance
        // No adjustment needed, use standard references
        _stl_ref_cte_adjustment = 0.0;
        _stl_ref_vel_adjustment = 0.0;
    }

    // Apply adjustments to reference values
    ref_cte += _stl_ref_cte_adjustment;
    ref_vel += _stl_ref_vel_adjustment;

    // Clamp to reasonable limits
    ref_cte = std::max(-2.0, std::min(2.0, ref_cte));
    ref_vel = std::max(0.1, std::min(_ref_vel * 1.5, ref_vel));

    // Log adjustments occasionally
    static int adjustment_log_counter = 0;
    if (adjustment_log_counter++ % 100 == 0 &&
        (std::abs(_stl_ref_cte_adjustment) > 0.01 || std::abs(_stl_ref_vel_adjustment) > 0.01))
    {
        ROS_INFO("STL Reference Adjustment: CTE: %.3f, Vel: %.3f (Robustness: %.3f, Budget: %.3f)",
                 _stl_ref_cte_adjustment, _stl_ref_vel_adjustment,
                 _current_stl_robustness, _current_stl_budget);
    }
}

// Terminal set callback functions (P1-1)

void TubeMPCNode::terminalSetCB(const std_msgs::Float64MultiArray::ConstPtr& msg)
{
    /**
     * Terminal set callback
     *
     * Receives terminal set information from dr_tightening node
     * Message format: [x_center, y_center, theta_center, v_center, cte_center, etheta_center, radius]
     */

    if (!_enable_terminal_set) {
        return;
    }

    // Check message size
    if (msg->data.size() < 7) {
        ROS_WARN("Invalid terminal set message size: %zu (expected >= 7)", msg->data.size());
        return;
    }

    // Extract terminal set center [6D state]
    for (int i = 0; i < 6; i++) {
        _terminal_set_center(i) = msg->data[i];
    }

    // Extract terminal set radius
    _terminal_set_radius = msg->data[6];

    // Update TubeMPC with terminal set
    _tube_mpc.setTerminalSet(_terminal_set_center, _terminal_set_radius);

    _terminal_set_received = true;

    // Log terminal set updates
    static int terminal_log_counter = 0;
    if (terminal_log_counter++ % 20 == 0) {
        ROS_INFO("Terminal Set Updated: center=[%.2f,%.2f,%.2f], radius=%.2f",
                 _terminal_set_center(0), _terminal_set_center(1),
                 _terminal_set_center(2), _terminal_set_radius);
    }

    // Publish visualization
    visualizeTerminalSet();
}

void TubeMPCNode::visualizeTerminalSet()
{
    if (!_enable_terminal_set || !_terminal_set_received) {
        return;
    }

    visualization_msgs::Marker marker;
    marker.header.frame_id = _odom_frame;
    marker.header.stamp = ros::Time::now();
    marker.ns = "terminal_set";
    marker.id = 0;
    marker.type = visualization_msgs::Marker::CYLINDER;
    marker.action = visualization_msgs::Marker::ADD;

    // Position (use x, y from terminal set center)
    marker.pose.position.x = _terminal_set_center(0);
    marker.pose.position.y = _terminal_set_center(1);
    marker.pose.position.z = 0.0;

    // Orientation (use theta from terminal set center)
    tf::Quaternion q;
    q.setRPY(0, 0, _terminal_set_center(2));
    marker.pose.orientation.x = q.x();
    marker.pose.orientation.y = q.y();
    marker.pose.orientation.z = q.z();
    marker.pose.orientation.w = q.w();

    // Scale (use radius as diameter)
    marker.scale.x = 2.0 * _terminal_set_radius;
    marker.scale.y = 2.0 * _terminal_set_radius;
    marker.scale.z = 0.1;  // Thin cylinder

    // Color (red with transparency)
    marker.color.r = 1.0;
    marker.color.g = 0.0;
    marker.color.b = 0.0;
    marker.color.a = 0.3;

    marker.lifetime = ros::Duration(1.0);  // 1 second lifetime

    _pub_terminal_set_viz.publish(marker);
}

bool TubeMPCNode::checkTerminalFeasibilityInLoop(const VectorXd& terminal_state)
{
    /**
     * Check terminal feasibility in control loop
     *
     * Returns true if terminal state is feasible, false otherwise
     * Increments violation counter and logs warnings
     */

    if (!_enable_terminal_set || !_terminal_set_received) {
        return true;  // No terminal constraint
    }

    bool feasible = _tube_mpc.checkTerminalFeasibility(terminal_state);

    if (!feasible) {
        _terminal_set_violation_count++;

        // Log violation warnings occasionally
        static int violation_log_counter = 0;
        if (violation_log_counter++ % 10 == 0) {
            ROS_WARN("Terminal feasibility violation #%d: Total violations: %d",
                     violation_log_counter, _terminal_set_violation_count);
        }
    }

    return feasible;
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "TubeMPC_Node");
    TubeMPCNode tube_mpc_node;

    ROS_INFO("Waiting for global path msgs ~");
    ros::AsyncSpinner spinner(tube_mpc_node.get_thread_numbers());
    spinner.start();
    ros::waitForShutdown();
    return 0;
}
