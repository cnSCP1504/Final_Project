# Test Folder Organization

This directory contains all testing materials, diagnostic tools, and reports organized by category.

## 📁 Directory Structure

```
test/
├── analysis/           # Performance analysis and data
├── archive/            # Archived experimental results
├── benchmark/          # Benchmark comparisons
├── diagnostics/        # Diagnostic and troubleshooting scripts
├── docs/              # General documentation and reports
├── fix_scripts/       # Scripts to fix common issues
├── reports/           # Phase completion reports
│   ├── phase2/        # Phase 2: STL Monitor
│   ├── phase3/        # Phase 3: DR Tightening
│   └── phase4/        # Phase 4: Reference Planner
├── scripts/           # Testing and verification scripts
└── README.md          # This file
```

## 🔍 Categories

### analysis/
Performance analysis data, charts, and experimental results.

### archive/
Historical test results and outdated reports.

### benchmark/
Performance benchmarks comparing different approaches and baselines.

### diagnostics/ ⚙️
Diagnostic tools for troubleshooting system issues:
- `diagnose_rviz.sh` - RViz display diagnostics
- `check_phase2_status.sh` - Phase 2 status checker
- `check_phase3_status.sh` - Phase 3 status checker
- `deep_diagnose_robot.sh` - Deep robot diagnostics
- `fix_robot_motion.sh` - Robot motion diagnostics

### docs/
General documentation and system reports:
- `OMPL_INTEGRATION.md` - OMPL integration status
- `DUAL_RVIZ_PROBLEM_FIX.md` - Dual RViz issue resolution
- `RVIZ_DISPLAY_ISSUE_AND_SOLUTION.md` - Display troubleshooting
- `SYSTEM_RUN_VERIFICATION.md` - System verification

### fix_scripts/ 🔧
Scripts to fix common configuration and runtime issues:
- `fix_rviz_display.sh` - Fix RViz display problems
- `phase3_complete.sh` - Phase 3 completion setup
- `test_dr_tightening_full.sh` - Full DR tightening test
- `test_dr_tightening_integration.sh` - Integration testing

### reports/ 📊
Phase-specific completion reports:

#### Phase 2: STL Monitor
- `PHASE2_ACTUAL_STATUS.md` - Final status report

#### Phase 3: DR Tightening
- `PHASE3_COMPLETION_REPORT.md` - Completion summary
- `PHASE3_CURRENT_STATUS.md` - Current implementation status
- `PHASE3_FINAL_REPORT.md` - Final technical report
- `PHASE3_TF_FIX_VERIFICATION.md` - TF system verification

#### Phase 4: Reference Planner
- `PHASE4_IMPLEMENTATION_REPORT.md` - Implementation details
- `PHASE4_ROS_INTEGRATION_COMPLETE.md` - ROS integration status
- `PHASE4_ACTIVE_INFORMATION_COMPLETE.md` - Active Information module

### scripts/ ▶️
Automated testing and verification scripts:
- `quick_verify_phase123.sh` - Quick Phase 1-3 verification
- `verify_phase123_output.sh` - Detailed verification
- `test_velocity_basic.py` - Basic velocity testing

## 🚀 Quick Start

### Run Diagnostics
```bash
# Check current system status
./test/diagnostics/check_phase3_status.sh

# Diagnose RViz issues
./test/diagnostics/diagnose_rviz.sh
```

### Run Tests
```bash
# Quick verification
./test/scripts/quick_verify_phase123.sh

# Full DR tightening test
./test/fix_scripts/test_dr_tightening_full.sh
```

### Fix Common Issues
```bash
# Fix RViz display
./test/fix_scripts/fix_rviz_display.sh

# Complete Phase 3 setup
./test/fix_scripts/phase3_complete.sh
```

## 📖 File Naming Conventions

- **Diagnostics**: `check_*.sh`, `diagnose_*.sh`
- **Fix Scripts**: `fix_*.sh`, `*_complete.sh`
- **Test Scripts**: `test_*.sh`, `verify_*.sh`
- **Reports**: `PHASE*_*.md` or `*_REPORT.md`
- **Documentation**: `*.md` with descriptive names

## 🔄 Maintenance

### Adding New Files
- Diagnostics → `test/diagnostics/`
- Test scripts → `test/scripts/`
- Fix scripts → `test/fix_scripts/`
- Reports → `test/reports/phaseX/`

### Archive Old Files
Move outdated files to `test/archive/` with appropriate date prefixes.

---

**Last Updated**: 2026-03-23
**Purpose**: Centralize all testing, diagnostic, and reporting materials
