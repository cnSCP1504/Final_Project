#include <ros/ros.h>
#include <tf/transform_datatypes.h>

#include "STL_ros/STLROSInterface.h"
#include "STL_ros/STLFormula.h"

using namespace stl_ros;

int main(int argc, char** argv) {
    ros::init(argc, argv, "stl_monitor");

    ros::NodeHandle nh("~");

    // Load configuration
    STLMonitor::Config config;

    nh.param("prediction_horizon", config.prediction_horizon, 20);
    nh.param("time_step", config.dt, 0.1);
    nh.param("temperature_tau", config.tau, 0.05);
    nh.param("num_monte_carlo_samples", config.num_samples, 100);
    nh.param("baseline_robustness_r", config.baseline_r, 0.1);
    nh.param("initial_budget", config.initial_budget, 0.0);
    nh.param("stl_weight_lambda", config.lambda, 1.0);
    nh.param("state_dimension", config.state_dim, 5);

    // Create STL ROS interface
    STLROSInterface stl_interface(nh, config);

    if (!stl_interface.initialize()) {
        ROS_ERROR("Failed to initialize STL ROS Interface");
        return -1;
    }

    // Load formulas if specified
    std::string formula_file;
    if (nh.getParam("formula_file_path", formula_file)) {
        ROS_INFO("Loading STL formulas from: %s", formula_file.c_str());
        // Formula loading would be done here
        // stl_interface.loadFormulas(formula_file);
    }

    ROS_INFO("STL Monitor node started");

    // Spin
    ros::spin();

    return 0;
}
