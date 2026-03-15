#!/bin/bash
# Final validation script for STL_ros package

echo "========================================"
echo "STL_ros Package Final Validation"
echo "========================================"

cd /home/dixon/Final_Project/catkin
source devel/setup.bash

# Test counter
PASSED=0
FAILED=0

echo ""
echo "--- Running comprehensive tests ---"

# Package structure
echo -n "Package structure... "
if [ -d "src/STL_ros" ] && [ -f "src/STL_ros/package.xml" ]; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Build system
echo -n "Build system... "
if catkin_make > /tmp/catkin_output.log 2>&1; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED (see /tmp/catkin_output.log)"
    ((FAILED++))
fi

# Source workspace
echo -n "Workspace setup... "
if source devel/setup.bash 2>/dev/null; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Package discovery
echo -n "Package discovery... "
if rospack find stl_ros >/dev/null 2>&1; then
    echo "✓ PASSED ($(rospack find stl_ros))"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Messages
echo -n "Message generation... "
if rosmsg show stl_ros/STLRobustness >/dev/null 2>&1 && \
   rosmsg show stl_ros/STLBudget >/dev/null 2>&1 && \
   rosmsg show stl_ros/STLFormula >/dev/null 2>&1; then
    echo "✓ PASSED (3 messages)"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Python imports
echo -n "Python imports... "
if python3 -c "from stl_ros.msg import STLRobustness, STLBudget, STLFormula" 2>/dev/null; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Python scripts
echo -n "Python scripts... "
if [ -x "src/STL_ros/scripts/stl_monitor.py" ] && \
   python3 -m py_compile src/STL_ros/scripts/stl_monitor.py 2>/dev/null && \
   python3 -m py_compile src/STL_ros/scripts/stl_visualizer.py 2>/dev/null; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Launch files
echo -n "Launch files... "
if xmllint --noout src/STL_ros/launch/stl_monitor.launch 2>/dev/null && \
   xmllint --noout src/STL_ros/launch/test_stl_monitor.launch 2>/dev/null; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Configuration files
echo -n "Configuration files... "
if [ -f "src/STL_ros/params/stl_params.yaml" ] && \
   [ -f "src/STL_ros/params/formulas.yaml" ] && \
   [ -f "src/STL_ros/cfg/STLMonitorParams.cfg" ]; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# Documentation
echo -n "Documentation... "
if [ -f "src/STL_ros/README.md" ] && \
   [ -f "src/STL_ros/QUICK_START.md" ] && \
   [ -f "src/STL_ros/IMPLEMENTATION_SUMMARY.md" ]; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

# C++ headers
echo -n "C++ headers... "
if [ -f "src/STL_ros/include/STL_ros/STLFormula.h" ] && \
   [ -f "src/STL_ros/include/STL_ros/SmoothRobustness.h" ] && \
   [ -f "src/STL_ros/include/STL_ros/BeliefSpaceEvaluator.h" ] && \
   [ -f "src/STL_ros/include/STL_ros/RobustnessBudget.h" ]; then
    echo "✓ PASSED"
    ((PASSED++))
else
    echo "✗ FAILED"
    ((FAILED++))
fi

echo ""
echo "========================================"
echo "Validation Results"
echo "========================================"
echo "PASSED: $PASSED"
echo "FAILED: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "✓ ALL TESTS PASSED!"
    echo "========================================"
    echo ""
    echo "STL_ros package is ready for use!"
    echo ""
    echo "Package Statistics:"
    echo "- Location: $(rospack find stl_ros)"
    echo "- Messages: 3 (STLRobustness, STLBudget, STLFormula)"
    echo "- Python nodes: 2 (stl_monitor.py, stl_visualizer.py)"
    echo "- Launch files: 3"
    echo "- Config files: 3"
    echo ""
    echo "Quick Start:"
    echo "1. Test basic functionality:"
    echo "   rosrun stl_ros quick_test.sh"
    echo ""
    echo "2. Launch STL monitor (requires roscore):"
    echo "   roslaunch stl_ros test_stl_monitor.launch"
    echo ""
    echo "3. Monitor topics:"
    echo "   rostopic echo /stl_monitor/robustness"
    echo "   rostopic echo /stl_monitor/budget"
    echo ""
    echo "For full documentation, see:"
    echo "  $(rospack find stl_ros)/README.md"
    echo ""
    exit 0
else
    echo ""
    echo "========================================"
    echo "✗ VALIDATION FAILED"
    echo "========================================"
    echo "Some tests did not pass. Please check the errors above."
    exit 1
fi
