#!/usr/bin/env python3
"""
Real-time visualization of Tube MPC navigation data
Plots trajectory, control inputs, and performance metrics
"""

import rospy
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from nav_msgs.msg import Path, Odometry
from geometry_msgs.msg import Twist
from std_msgs.msg import Float64
import matplotlib
matplotlib.use('TkAgg')

class TubeMPCVisualizer:
    def __init__(self):
        rospy.init_node('tube_mpc_visualizer', anonymous=True)

        # Data storage
        self trajectory_x = []
        self trajectory_y = []
        self.time_data = []
        self.vel_linear = []
        self.vel_angular = []
        self.cte_data = []
        self.solve_time = []

        # Setup figure
        self.setup_plots()

        # Subscribers
        rospy.Subscriber('/odom', Odometry, self.odom_callback)
        rospy.Subscriber('/cmd_vel', Twist, self.cmd_vel_callback)
        rospy.Subscriber('/global_path', Path, self.path_callback)
        rospy.Subscriber('/mpc_metrics/tracking_error', Float64, self.cte_callback)
        rospy.Subscriber('/mpc_metrics/solve_time_ms', Float64, self.solve_time_callback)

        # Start time
        self.start_time = rospy.Time.now()

        rospy.loginfo("Tube MPC Visualizer started")

    def setup_plots(self):
        """Setup matplotlib figure with subplots"""
        self.fig = plt.figure(figsize=(15, 10))
        self.fig.suptitle('Tube MPC Real-time Visualization', fontsize=16, fontweight='bold')

        # Create grid spec
        gs = self.fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)

        # 1. Trajectory plot (large, top left)
        self.ax_traj = self.fig.add_subplot(gs[0:2, 0:2])
        self.ax_traj.set_title('Robot Trajectory')
        self.ax_traj.set_xlabel('X Position (m)')
        self.ax_traj.set_ylabel('Y Position (m)')
        self.ax_traj.grid(True, alpha=0.3)
        self.ax_traj.axis('equal')

        # Trajectory line
        self.line_traj, = self.ax_traj.plot([], [], 'b-', linewidth=2, label='Actual Path')
        self.line_goal, = self.ax_traj.plot([], [], 'r*', markersize=15, label='Goal')
        self.line_start, = self.ax_traj.plot([], [], 'g^', markersize=12, label='Start')
        self.ax_traj.legend()

        # 2. Velocity plot (top right)
        self.ax_vel = self.fig.add_subplot(gs[0, 2])
        self.ax_vel.set_title('Velocity Commands')
        self.ax_vel.set_xlabel('Time (s)')
        self.ax_vel.set_ylabel('Velocity (m/s)')
        self.ax_vel.grid(True, alpha=0.3)

        self.line_vel_lin, = self.ax_vel.plot([], [], 'b-', linewidth=1.5, label='Linear')
        self.line_vel_ang, = self.ax_vel.plot([], [], 'r-', linewidth=1.5, label='Angular')
        self.ax_vel.legend()

        # 3. Cross-track error (middle right)
        self.ax_cte = self.fig.add_subplot(gs[1, 2])
        self.ax_cte.set_title('Cross-Track Error')
        self.ax_cte.set_xlabel('Time (s)')
        self.ax_cte.set_ylabel('CTE (m)')
        self.ax_cte.grid(True, alpha=0.3)

        self.line_cte, = self.ax_cte.plot([], [], 'g-', linewidth=1.5)

        # 4. Solve time (bottom left)
        self.ax_solve = self.fig.add_subplot(gs[2, 0])
        self.ax_solve.set_title('MPC Solve Time')
        self.ax_solve.set_xlabel('Time (s)')
        self.ax_solve.set_ylabel('Solve Time (ms)')
        self.ax_solve.grid(True, alpha=0.3)

        self.line_solve, = self.ax_solve.plot([], [], 'm-', linewidth=1.5)

        # 5. Position error (bottom middle)
        self.ax_pos = self.fig.add_subplot(gs[2, 1])
        self.ax_pos.set_title('Position Components')
        self.ax_pos.set_xlabel('Time (s)')
        self.ax_pos.set_ylabel('Position (m)')
        self.ax_pos.grid(True, alpha=0.3)

        self.line_pos_x, = self.ax_pos.plot([], [], 'r-', linewidth=1.5, label='X')
        self.line_pos_y, = self.ax_pos.plot([], [], 'b-', linewidth=1.5, label='Y')
        self.ax_pos.legend()

        # 6. Statistics text (bottom right)
        self.ax_stats = self.fig.add_subplot(gs[2, 2])
        self.ax_stats.axis('off')
        self.stats_text = self.ax_stats.text(0.1, 0.5, '', transform=self.ax_stats.transAxes,
                                            fontsize=10, verticalalignment='center',
                                            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

        # Global path reference
        self.global_path_x = []
        self.global_path_y = []

    def odom_callback(self, msg):
        """Handle odometry updates"""
        current_time = (rospy.Time.now() - self.start_time).to_sec()

        x = msg.pose.pose.position.x
        y = msg.pose.pose.position.y

        self.trajectory_x.append(x)
        self.trajectory_y.append(y)
        self.time_data.append(current_time)

        # Update trajectory plot
        self.line_traj.set_data(self.trajectory_x, self.trajectory_y)

        # Update position components
        if len(self.time_data) > 0:
            self.line_pos_x.set_data(self.time_data, [x] * len(self.time_data))
            self.line_pos_y.set_data(self.time_data, [y] * len(self.time_data))

        # Auto-scale trajectory plot
        if len(self.trajectory_x) > 0:
            margin = 1.0
            self.ax_traj.set_xlim(min(self.trajectory_x) - margin, max(self.trajectory_x) + margin)
            self.ax_traj.set_ylim(min(self.trajectory_y) - margin, max(self.trajectory_y) + margin)

        # Update statistics
        self.update_statistics()

        # Redraw
        self.fig.canvas.draw_idle()

    def cmd_vel_callback(self, msg):
        """Handle velocity commands"""
        current_time = (rospy.Time.now() - self.start_time).to_sec()

        self.vel_linear.append(msg.linear.x)
        self.vel_angular.append(msg.angular.z)

        # Keep last 100 points
        if len(self.vel_linear) > 100:
            self.vel_linear.pop(0)
            self.vel_angular.pop(0)

        times = self.time_data[-len(self.vel_linear):]

        self.line_vel_lin.set_data(times, self.vel_linear)
        self.line_vel_ang.set_data(times, self.vel_angular)

        if len(times) > 0:
            self.ax_vel.set_xlim(min(times), max(times) + 1)
            self.ax_vel.set_ylim(min(min(self.vel_linear), min(self.vel_angular)) - 0.1,
                                max(max(self.vel_linear), max(self.vel_angular)) + 0.1)

    def path_callback(self, msg):
        """Handle global path updates"""
        self.global_path_x = [pose.pose.position.x for pose in msg.poses]
        self.global_path_y = [pose.pose.position.y for pose in msg.poses]

        # Plot global path as reference
        if hasattr(self, 'line_global_path'):
            self.line_global_path.remove()

        self.line_global_path, = self.ax_traj.plot(self.global_path_x, self.global_path_y,
                                                   'g--', linewidth=1, alpha=0.5, label='Global Path')
        self.ax_traj.legend()

    def cte_callback(self, msg):
        """Handle cross-track error updates"""
        current_time = (rospy.Time.now() - self.start_time).to_sec()

        self.cte_data.append(msg.data)

        # Keep last 100 points
        if len(self.cte_data) > 100:
            self.cte_data.pop(0)

        times = self.time_data[-len(self.cte_data):]

        self.line_cte.set_data(times, self.cte_data)

        if len(times) > 0:
            self.ax_cte.set_xlim(min(times), max(times) + 1)
            if len(self.cte_data) > 0:
                self.ax_cte.set_ylim(min(self.cte_data) - 0.1, max(self.cte_data) + 0.1)

    def solve_time_callback(self, msg):
        """Handle MPC solve time updates"""
        current_time = (rospy.Time.now() - self.start_time).to_sec()

        self.solve_time.append(msg.data)

        # Keep last 100 points
        if len(self.solve_time) > 100:
            self.solve_time.pop(0)

        times = self.time_data[-len(self.solve_time):]

        self.line_solve.set_data(times, self.solve_time)

        if len(times) > 0:
            self.ax_solve.set_xlim(min(times), max(times) + 1)
            if len(self.solve_time) > 0:
                self.ax_solve.set_ylim(0, max(self.solve_time) * 1.1)

    def update_statistics(self):
        """Update statistics display"""
        if len(self.trajectory_x) < 2:
            return

        # Calculate statistics
        total_distance = np.sum(np.sqrt(np.diff(self.trajectory_x)**2 + np.diff(self.trajectory_y)**2))
        current_x = self.trajectory_x[-1]
        current_y = self.trajectory_y[-1]
        elapsed_time = self.time_data[-1]

        avg_vel = total_distance / elapsed_time if elapsed_time > 0 else 0

        avg_cte = np.mean(np.abs(self.cte_data)) if self.cte_data else 0
        max_cte = np.max(np.abs(self.cte_data)) if self.cte_data else 0

        avg_solve_time = np.mean(self.solve_time) if self.solve_time else 0

        stats = f"""
Statistics:
━━━━━━━━━━━━━━━━━━━━
Position: ({current_x:.2f}, {current_y:.2f}) m
Distance: {total_distance:.2f} m
Time: {elapsed_time:.1f} s
Avg Vel: {avg_vel:.2f} m/s

CTE:
  Avg: {avg_cte:.3f} m
  Max: {max_cte:.3f} m

MPC:
  Solve Time: {avg_solve_time:.1f} ms
"""

        self.stats_text.set_text(stats)

    def run(self):
        """Run the visualizer"""
        plt.show()
        rospy.spin()

if __name__ == '__main__':
    try:
        viz = TubeMPCVisualizer()
        viz.run()
    except rospy.ROSInterruptException:
        pass
