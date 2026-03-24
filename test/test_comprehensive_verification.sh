#!/bin/bash
################################################################################
# Safe-Regret MPC - Comprehensive System Verification Script
# Tests P0 fixes: STL Monitor, DR Tightening, and System Integration
################################################################################

# Don't exit on error, collect all results first

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
PASSED=0
FAILED=0
WARNINGS=0

# Log file
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="/tmp/safe_regret_verification_${TIMESTAMP}.log"

################################################################################
# Helper Functions
################################################################################

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1" | tee -a "$LOG_FILE"
    ((PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1" | tee -a "$LOG_FILE"
    ((FAILED++))
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
    ((WARNINGS++))
}

print_header() {
    echo ""
    echo "========================================"
    echo "$1"
    echo "========================================"
    echo ""
}

################################################################################
# Pre-flight Checks
################################################################################

check_ros_environment() {
    print_header "1. ROS Environment Check"

    # Check if ROS is sourced
    if [ -z "$ROS_DISTRO" ]; then
        log_fail "ROS not sourced. Please run: source /opt/ros/noetic/setup.bash"
        return 1
    fi
    log_success "ROS $ROS_DISTRO is sourced"

    # Check if workspace is sourced
    if [ -z "$CATKIN_WS" ] && [ ! -f "devel/setup.bash" ]; then
        log_warning "Workspace may not be properly sourced"
    else
        log_success "Workspace appears configured"
    fi

    # Check for roscore
    pgrep -f roscore >/dev/null && log_success "roscore is running" || log_warning "roscore not detected"
}

check_package_build() {
    print_header "2. Package Build Status"

    local packages=("stl_monitor" "dr_tightening" "safe_regret" "tube_mpc_ros")

    for pkg in "${packages[@]}"; do
        if [ -d "src/$pkg" ] || [ -d "src/tube_mpc_ros/$pkg" ]; then
            log_success "Package $pkg exists"

            # Check if compiled (look for .so files or devel space)
            if find . -name "*.so" -o -name "*.pyc" 2>/dev/null | grep -q "$pkg"; then
                log_success "Package $pkg appears compiled"
            else
                log_warning "Package $pkg may not be compiled"
            fi
        else
            log_fail "Package $pkg not found"
        fi
    done
}

################################################################################
# STL Monitor Verification
################################################################################

verify_stl_monitor() {
    print_header "3. STL Monitor Verification"

    # Check STL monitor files
    log_info "Checking STL monitor files..."

    local stl_files=(
        "src/tube_mpc_ros/stl_monitor/package.xml"
        "src/tube_mpc_ros/stl_monitor/src/stl_monitor_node.py"
        "src/tube_mpc_ros/mpc_ros/params/stl_params.yaml"
        "src/tube_mpc_ros/mpc_ros/launch/stl_mpc_navigation.launch"
    )

    for file in "${stl_files[@]}"; do
        if [ -f "$file" ]; then
            log_success "File exists: $file"
        else
            log_fail "File missing: $file"
        fi
    done

    # Verify stl_params.yaml content
    log_info "Checking STL parameters..."
    if grep -q "temperature: 0.1" src/tube_mpc_ros/mpc_ros/params/stl_params.yaml; then
        log_success "Temperature parameter found (τ=0.1)"
    else
        log_fail "Temperature parameter missing or incorrect"
    fi

    if grep -q "baseline_requirement: 0.1" src/tube_mpc_ros/mpc_ros/params/stl_params.yaml; then
        log_success "Baseline requirement found (r̲=0.1)"
    else
        log_fail "Baseline requirement missing"
    fi

    # Check launch file integration
    log_info "Checking launch file integration..."
    if grep -q "pkg=\"stl_monitor\"" src/tube_mpc_ros/mpc_ros/launch/stl_mpc_navigation.launch; then
        log_success "Launch file references stl_monitor package correctly"
    else
        log_fail "Launch file does not reference stl_monitor package"
    fi

    if grep -q "enable_stl" src/tube_mpc_ros/mpc_ros/launch/stl_mpc_navigation.launch; then
        log_success "Launch file has enable_stl argument"
    else
        log_warning "Launch file missing enable_stl argument"
    fi
}

################################################################################
# DR Tightening Verification
################################################################################

verify_dr_tightening() {
    print_header "4. DR Tightening Verification"

    # Check DR tightening files
    log_info "Checking DR tightening files..."

    local dr_files=(
        "src/dr_tightening/package.xml"
        "src/dr_tightening/include/dr_tightening/dr_tightening_node.hpp"
        "src/dr_tightening/src/dr_tightening_node.cpp"
        "src/dr_tightening/launch/dr_tightening.launch"
    )

    for file in "${dr_files[@]}"; do
        if [ -f "$file" ]; then
            log_success "File exists: $file"
        else
            log_fail "File missing: $file"
        fi
    done

    # Check for DR formulas in source code
    log_info "Checking DR formula implementation..."

    if grep -q "computeCantelliFactor" src/dr_tightening/src/*.cpp 2>/dev/null; then
        log_success "Cantelli factor computation found"
    else
        log_warning "Cantelli factor computation not found"
    fi

    if grep -q "sqrt((1.0 - delta) / delta)" src/dr_tightening/src/*.cpp 2>/dev/null; then
        log_success "Cantelli formula κ_δ = sqrt((1-δ)/δ) found"
    else
        log_warning "Cantelli formula not explicitly found"
    fi
}

################################################################################
# Safe-Regret Integration Verification
################################################################################

verify_safe_regret_integration() {
    print_header "5. Safe-Regret Integration Verification"

    # Check integrated launch file
    log_info "Checking integrated launch file..."

    if [ -f "src/safe_regret/launch/safe_regret_simplified.launch" ]; then
        log_success "Integrated launch file exists"

        # Check what phases are enabled
        if grep -q "Phase 2" src/safe_regret/launch/safe_regret_simplified.launch; then
            if grep -q "temporarily disabled" src/safe_regret/launch/safe_regret_simplified.launch; then
                log_warning "Phase 2 (STL Monitor) is disabled in integrated launch"
            else
                log_success "Phase 2 (STL Monitor) is enabled in integrated launch"
            fi
        fi

        # Check for Phase 3 integration
        if grep -q "dr_tightening" src/safe_regret/launch/safe_regret_simplified.launch; then
            log_success "Phase 3 (DR Tightening) is integrated"
        else
            log_warning "Phase 3 (DR Tightening) may not be integrated"
        fi

        # Check for Phase 4 integration
        if grep -q "safe_regret_planner" src/safe_regret/launch/safe_regret_simplified.launch; then
            log_success "Phase 4 (Reference Planner) is integrated"
        else
            log_warning "Phase 4 (Reference Planner) may not be integrated"
        fi
    else
        log_fail "Integrated launch file not found"
    fi
}

################################################################################
# Code Verification (Paper Formulas)
################################################################################

verify_theory_formulas() {
    print_header "6. Theory Formula Verification"

    # Check for smooth robustness (Remark 3.2)
    log_info "Checking smooth robustness implementation..."

    if find src/tube_mpc_ros/stl_monitor -name "*.py" -exec grep -l "log-sum-exp\|smin\|smax" {} \; 2>/dev/null | grep -q .; then
        log_success "Smooth robustness (smin/smax) found in Python code"
    else
        log_warning "Smooth robustness not explicitly found"
    fi

    # Check for robustness budget (Eq. 15)
    log_info "Checking robustness budget (Eq. 15)..."

    if find src/tube_mpc_ros/stl_monitor -name "*.py" -exec grep -l "budget.*robustness\|R\[" {} \; 2>/dev/null | grep -q .; then
        log_success "Robustness budget mechanism found"
    else
        log_warning "Robustness budget not explicitly found"
    fi

    # Check for DR margin formula (Lemma 4.3)
    log_info "Checking DR margin formula (Lemma 4.3)..."

    if grep -q "tube_offset\|mean_along_sensitivity\|cantelli" src/dr_tightening/src/*.cpp 2>/dev/null; then
        log_success "DR margin formula components found"
    else
        log_warning "DR margin formula components not explicitly found"
    fi
}

################################################################################
# Documentation Verification
################################################################################

verify_documentation() {
    print_header "7. Documentation Verification"

    local docs=(
        "FIX_SUMMARY_REPORT.md"
        "SAFE_REGRET_FIX_PROGRESS.md"
        "SYSTEM_IMPLEMENTATION_EVALUATION.md"
    )

    for doc in "${docs[@]}"; do
        if [ -f "$doc" ]; then
            log_success "Documentation exists: $doc"
        else
            log_warning "Documentation missing: $doc"
        fi
    done
}

################################################################################
# Main Verification Flow
################################################################################

main() {
    print_header "Safe-Regret MPC System Verification"
    echo "Verification started at: $(date)" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"

    # Run all verification steps
    check_ros_environment
    check_package_build
    verify_stl_monitor
    verify_dr_tightening
    verify_safe_regret_integration
    verify_theory_formulas
    verify_documentation

    # Print summary
    print_header "Verification Summary"
    echo "Results logged to: $LOG_FILE"
    echo ""
    echo -e "${GREEN}Passed:${NC} $PASSED"
    echo -e "${YELLOW}Warnings:${NC} $WARNINGS"
    echo -e "${RED}Failed:${NC} $FAILED"
    echo ""

    # Overall assessment
    if [ $FAILED -eq 0 ]; then
        echo -e "${GREEN}✓ All critical checks passed!${NC}"
        if [ $WARNINGS -gt 0 ]; then
            echo -e "${YELLOW}⚠ Some warnings detected, review recommended${NC}"
        fi
        echo ""
        echo "System is ready for runtime testing."
        echo "Next steps:"
        echo "  1. Source workspace: source devel/setup.bash"
        echo "  2. Launch system: roslaunch tube_mpc_ros stl_mpc_navigation.launch enable_stl:=true"
        echo "  3. Monitor topics: rostopic echo /stl/robustness"
        return 0
    else
        echo -e "${RED}✗ Some critical checks failed${NC}"
        echo "Please review the log file for details: $LOG_FILE"
        return 1
    fi
}

# Run main function
main "$@"
