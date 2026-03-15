#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
STL Visualizer
Visualizes STL robustness values and budget status in real-time
"""

import rospy
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from std_msgs.msg import Float64
from stl_ros.msg import STLRobustness, STLBudget
from collections import deque


class STLVisualizer:
    """Visualizes STL monitoring results"""

    def __init__(self):
        rospy.init_node('stl_visualizer', anonymous=True)

        # Data storage
        self.max_points = 100
        self.robustness_history = deque(maxlen=self.max_points)
        self.budget_history = deque(maxlen=self.max_points)
        self.time_history = deque(maxlen=self.max_points)

        # Subscribers
        rospy.Subscriber('~robustness', STLRobustness, self.robustness_callback)
        rospy.Subscriber('~budget', STLBudget, self.budget_callback)

        # Setup plot
        self.setup_plot()

    def setup_plot(self):
        """Setup matplotlib figure"""
        plt.ion()  # Interactive mode
        self.fig, (self.ax1, self.ax2) = plt.subplots(2, 1, figsize=(10, 8))

        # Robustness plot
        self.ax1.set_title('STL Robustness')
        self.ax1.set_xlabel('Time')
        self.ax1.set_ylabel('Robustness')
        self.ax1.grid(True)
        self.robustness_line, = self.ax1.plot([], [], 'b-', linewidth=2)
        self.ax1.axhline(y=0, color='r', linestyle='--', label='Satisfaction Threshold')
        self.ax1.legend()

        # Budget plot
        self.ax2.set_title('Robustness Budget')
        self.ax2.set_xlabel('Time')
        self.ax2.set_ylabel('Budget')
        self.ax2.grid(True)
        self.budget_line, = self.ax2.plot([], [], 'g-', linewidth=2)
        self.ax2.axhline(y=0, color='r', linestyle='--', label='Feasibility Threshold')
        self.ax2.legend()

        plt.tight_layout()

    def robustness_callback(self, msg):
        """Handle robustness updates"""
        current_time = rospy.Time.now().to_sec()
        self.robustness_history.append(msg.robustness)
        self.time_history.append(current_time)
        self.update_plot()

    def budget_callback(self, msg):
        """Handle budget updates"""
        self.budget_history.append(msg.current_budget)
        self.update_plot()

    def update_plot(self):
        """Update the plot with new data"""
        if len(self.time_history) > 0:
            # Convert to relative time
            times = [t - self.time_history[0] for t in self.time_history]

            # Update robustness
            self.robustness_line.set_data(times, list(self.robustness_history))
            self.ax1.relim()
            self.ax1.autoscale_view()

            # Update budget
            self.budget_line.set_data(times, list(self.budget_history))
            self.ax2.relim()
            self.ax2.autoscale_view()

            plt.draw()
            plt.pause(0.001)

    def run(self):
        """Main loop"""
        rospy.loginfo("STL Visualizer started")
        plt.show()
        rospy.spin()


def main():
    """Main entry point"""
    try:
        visualizer = STLVisualizer()
        visualizer.run()
    except rospy.ROSInterruptException:
        pass
    except Exception as e:
        rospy.logerr(f"Error in visualizer: {e}")


if __name__ == '__main__':
    main()
