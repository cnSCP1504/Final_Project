# OMPL Integration Guide for Safe-Regret MPC

## Overview

This document describes how to integrate OMPL (Open Motion Planning Library) into the Safe-Regret MPC framework for topological path planning.

## OMPL Capabilities

- **Geometric Planning**: RRT, RRT*, PRM, EST, and many more
- **Topological Properties**: Supports topological path reasoning
- **State Spaces**: SE(2), SE(3), discrete spaces, etc.
- **ROS Integration**: Native ROS support via ompl_ros

## Installation

```bash
sudo apt-get install libompl-dev ompl-ros-dev
```

## Architecture

```
Safe-Regret Framework
├── TopologicalAbstraction (OMPL-based)
│   ├── OMPLGeometricPlanner
│   ├── OMPLTopologicalPlanner
│   └── OMPLPathSimplifier
├── BeliefSpaceOptimizer
│   ├── ParticleFilter
│   └── UnscentedTransform
└── ReferencePlanner
    └── Uses OMPL for path generation
```

## Key Components

### 1. OMPLGeometricPlanner
- Wraps OMPL's geometric planners
- Supports multiple algorithms (RRT, RRT*, PRM, EST)
- Returns collision-free paths

### 2. OMPLTopologicalPlanner
- Computes homology-informed paths
- Uses OMPL's topological properties
- Generates multiple path variants

### 3. OMPLPathSimplifier
- Simplifies paths while preserving topology
- Shortcutting
- Visibility graph refinement
