#!/bin/bash

# Script to automatically add MetricsCollector to CMakeLists.txt
# Usage: ./add_metrics_collector.sh

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CMAKELISTS="$SCRIPT_DIR/../CMakeLists.txt"
BACKUP="$CMAKELISTS.backup"

echo "=== Metrics Collector Integration Script ==="
echo "CMakeLists.txt location: $CMAKELISTS"

# Check if CMakeLists.txt exists
if [ ! -f "$CMAKELISTS" ]; then
    echo "ERROR: CMakeLists.txt not found at $CMAKELISTS"
    exit 1
fi

# Create backup
echo "Creating backup: $BACKUP"
cp "$CMAKELISTS" "$BACKUP"

# Check if MetricsCollector is already added
if grep -q "MetricsCollector.cpp" "$CMAKELISTS"; then
    echo "MetricsCollector.cpp already found in CMakeLists.txt"
    echo "No changes needed."
    exit 0
fi

# Add MetricsCollector.cpp to the executable sources
echo "Adding MetricsCollector.cpp to CMakeLists.txt..."

# Find the line with add_executable(tube_mpc_node and add MetricsCollector.cpp
# This awk script adds MetricsCollector.cpp after TubeMPC.cpp
awk '
/add_executable\(tube_mpc_node/ {
    in_executable = 1
    print
    next
}
in_executable && /src\/TubeMPC\.cpp/ {
    print
    print "  src/MetricsCollector.cpp"
    next
}
in_executable && /\)/ {
    in_executable = 0
}
{
    print
}
' "$BACKUP" > "$CMAKELISTS"

echo "✓ Updated CMakeLists.txt"
echo ""
echo "Next steps:"
echo "1. Review the changes: diff $BACKUP $CMAKELISTS"
echo "2. Rebuild the workspace:"
echo "   cd /home/dixon/Final_Project/catkin"
echo "   catkin_make"
echo "3. Source the workspace:"
echo "   source devel/setup.bash"
echo ""
echo "If everything looks good, you can remove the backup:"
echo "   rm $BACKUP"
