#!/bin/bash
# Debug script for TubeMPC_Node crash investigation

echo "=== TubeMPC_Node Debug Script ==="
echo "Date: $(date)"
echo "System: $(uname -a)"
echo ""

echo "=== 1. Library Dependency Check ==="
echo "Checking Ipopt library dependencies:"
ldd /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node | grep -i ipopt
echo ""

echo "=== 2. Ipopt Library Versions ==="
echo "System Ipopt libraries:"
ls -la /lib/libipopt.so* /usr/local/lib/libipopt.so* 2>/dev/null
echo ""

echo "=== 3. Recent System Updates ==="
echo "Checking recent package updates:"
grep -i "ipopt\|stdc++\|gcc" /var/log/dpkg.log* 2>/dev/null | tail -10
echo ""

echo "=== 4. Memory and Resources ==="
echo "Available memory:"
free -h
echo ""

echo "=== 5. Disk Space ==="
echo "Disk space in /tmp:"
df -h /tmp
echo ""

echo "=== 6. Running with GDB (if available) ==="
if command -v gdb &> /dev/null; then
    echo "GDB found. Creating GDB batch script..."
    cat > /tmp/gdb_tubempc.txt << 'EOF'
set pagination off
run
bt
info registers
quit
EOF
    echo "To run with GDB, execute:"
    echo "gdb -x /tmp/gdb_tubempc.txt /home/dixon/Final_Project/catkin/devel/lib/tube_mpc_ros/tube_TubeMPC_Node"
else
    echo "GDB not found."
fi

echo ""
echo "=== Debug script completed ==="
