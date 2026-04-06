#!/bin/bash
# 获取当前Gazebo相机位置的脚本

echo "请在Gazebo窗口中，调整到你想要的视角"
echo ""
echo "然后在终端中运行以下命令来获取当前相机位置："
echo ""
echo "gz camera -s gazebo_client_camera -o"
echo ""
echo "或者查看Gazebo的GUI配置文件："
echo "cat ~/.gazebo/gui.ini | grep -A 10 'camera.*pose'"
echo ""
echo "请把Gazebo中显示的相机位置信息告诉我，包括："
echo "  - 相机位置 (x, y, z)"
echo "  - 相机朝向 (yaw, pitch)"
echo "  - 或者直接截图给我看Gazebo的视角"
