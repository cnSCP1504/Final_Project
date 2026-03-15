#define HAVE_CSTDDEF
#include "TubeMPCNode.h"
#include <cppad/ipopt/solve.hpp>
#include <Eigen/Core>
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
    pn.param("mpc_w_vel", _w_vel, 200.0);
    pn.param("mpc_w_angvel", _w_angvel, 100.0);
    pn.param("mpc_w_accel", _w_accel, 50.0);
    pn.param("mpc_max_angvel", _max_angvel, 0.5);
    pn.param("mpc_max_throttle", _max_throttle, 1.0);
    pn.param("mpc_bound_value", _bound_value, 1.0e3);
    pn.param("disturbance_bound", _disturbance_bound, 0.1);

    pn.param<std::string>("global_path_topic", _globalPath_topic, "/move_base/TrajectoryPlannerROS/global_plan");
    pn.param<std::string>("goal_topic", _goal_topic, "/move_base_simple/goal");
    pn.param<std::string>("map_frame", _map_frame, "map");
    pn.param<std::string>("odom_frame", _odom_frame, "odom");
    pn.param<std::string>("car_frame", _car_frame, "base_footprint");

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
    
    if(_pub_twist_flag)
        _pub_twist = _nh.advertise<geometry_msgs::Twist>("/cmd_vel", 1);

    _timer1 = _nh.createTimer(ros::Duration((1.0)/_controller_freq), &TubeMPCNode::controlLoopCB, this);

    _goal_received = false;
    _goal_reached  = false;
    _path_computed = false;
    _throttle = 0.0; 
    _w = 0.0;
    _w_filtered = 0.0;
    _speed = 0.3;

    _twist_msg = geometry_msgs::Twist();
    _mpc_traj = nav_msgs::Path();
    _tube_path = nav_msgs::Path();

    idx = 0;
    file.open("/tmp/tube_mpc_data.csv");
    file << "idx"<< "," << "cte" << "," <<  "etheta" << "," << "cmd_vel.linear.x" << "," << "cmd_vel.angular.z" << "," << "tube_radius" << "\n";

    _w_residuals = VectorXd::Zero(6);
    _num_residuals = 0;
    _e_current = VectorXd::Zero(6);

<<<<<<< Updated upstream
=======
    // Initialize metrics collector
    pn.param<std::string>("metrics_output_csv", _metrics_output_csv, "/tmp/tube_mpc_metrics.csv");
    pn.param("target_risk_delta", _target_delta, 0.1);  // Default 10% risk

    _metrics_collector = Metrics::MetricsCollector(_metrics_output_csv);
    _metrics_collector.setTargetRisk(_target_delta);
    _metrics_collector.setupROSPublishers(_nh);

    ROS_INFO("MetricsCollector initialized with CSV output: %s", _metrics_output_csv.c_str());

    // Initialize STL Integration
    pn.param("enable_stl_integration", _stl_enabled, false);
    pn.param("stl_weight_lambda", _stl_weight_lambda, 1.0);
    pn.param("stl_budget_penalty", _stl_budget_penalty, 10.0);

    if (_stl_enabled) {
        cout << "\n===== STL Integration Enabled =====" << endl;
        cout << "stl_weight_lambda: " << _stl_weight_lambda << endl;
        cout << "stl_budget_penalty: " << _stl_budget_penalty << endl;

        // Subscribe to STL topics (commented until full integration ready)
        // _stl_robustness_sub = _nh.subscribe("/stl_monitor/robustness", 1,
        //                                   &TubeMPCNode::stlRobustnessCB, this);
        // _stl_budget_sub = _nh.subscribe("/stl_monitor/budget", 1,
        //                             &TubeMPCNode::stlBudgetCB, this);

        // Publish belief and trajectory for STL
        _stl_belief_pub = _nh.advertise<geometry_msgs::PoseStamped>("/stl_monitor/belief", 1);
        _stl_trajectory_pub = _nh.advertise<nav_msgs::Path>("/stl_monitor/mpc_trajectory", 1);

        ROS_INFO("STL Integration enabled - subscribing to STL monitor topics");
    } else {
        ROS_INFO("STL Integration disabled");
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
           
            if(odom_path.poses.size() >= 6 )
            {
                _odom_path = odom_path;
                _path_computed = true;
                odom_path.header.frame_id = _odom_frame;
                odom_path.header.stamp = ros::Time::now();
                _pub_odompath.publish(odom_path);
            }
            else
            {
                cout << "Failed to path generation" << endl;
                _waypointsDist = -1;
            }
        }
        catch(tf::TransformException &ex)
        {
            ROS_ERROR("%s",ex.what());
            ros::Duration(1.0).sleep();
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

                file.close();

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

        auto coeffs = polyfit(x_veh, y_veh, 3); 
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
        
        double temp_theta = theta;
        double traj_deg = atan2(gy,gx);
        double PI = 3.141592;

        if(temp_theta <= -PI + traj_deg) 
            temp_theta = temp_theta + 2 * PI;
        
        if(gx && gy && temp_theta - traj_deg < 1.8 * PI)
            etheta = temp_theta - traj_deg;
        else
            etheta = 0;

        cout << "etheta: "<< etheta << ", atan2(gy,gx): " << atan2(gy,gx) << ", temp_theta:" << traj_deg << endl;

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
        
        ros::Time begin = ros::Time::now();
        vector<double> mpc_results = _tube_mpc.Solve(state, coeffs);    
        ros::Time end = ros::Time::now();
        cout << "Duration: " << end.sec << "." << end.nsec << endl << begin.sec<< "."  << begin.nsec << endl;
              
        _w = mpc_results[0]; 
        _throttle = mpc_results[1]; 

        _w_filtered = 0.5 * _w_filtered + 0.5 * _w;
        
        _speed = v + _throttle * _dt;  
        if (_speed >= _max_speed)
            _speed = _max_speed;
        if(_speed < 0.03)
            _speed = 0.03;

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

    // Publish STL data if enabled
    if (_stl_enabled) {
        publishSTLData();
    }
        
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

<<<<<<< Updated upstream
=======
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

// ==================== STL Integration ====================

void TubeMPCNode::publishSTLData() {
    if (!_stl_enabled) return;

    // Publish current belief state for STL monitoring
    // This would typically come from AMCL or other state estimator
    geometry_msgs::PoseStamped belief_msg;
    belief_msg.header.stamp = ros::Time::now();
    belief_msg.header.frame_id = _map_frame;

    // Use current odometry as belief (simplified)
    belief_msg.pose.position.x = _odom.pose.pose.position.x;
    belief_msg.pose.position.y = _odom.pose.pose.position.y;
    belief_msg.pose.position.z = _odom.pose.pose.position.z;
    belief_msg.pose.orientation = _odom.pose.pose.orientation;

    _stl_belief_pub.publish(belief_msg);

    // Publish MPC predicted trajectory for STL evaluation
    _stl_trajectory_pub.publish(_mpc_traj);

    ROS_DEBUG("Published STL data: belief and trajectory");
}

>>>>>>> Stashed changes
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
