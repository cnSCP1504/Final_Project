
# Comprehensive Performance Comparison of Four MPC Methods

**Test Configuration**: Each method underwent 10 independent runs, with each run containing navigation tasks to 10 shelf points.

| Method | Goal Success Rate (%) | Empirical Risk $\hat{\delta}$ (%) | Feasibility (%) | Tracking Error (m) | Tube Occupancy (%) | Solve Time (ms) | Calibration Error |
|--------|------|------|------|------|------|------|------|
| Tube MPC | 93.0±7.5 | 17.96±3.48 | 100.0 | 0.462±0.064 | 82.0±3.5 | 16.2±2.8 | 0.1746±0.0348 |
| STL MPC | 96.0±6.6 | 17.13±2.05 | 100.0 | 0.451±0.036 | 82.9±2.0 | 17.4±3.6 | 0.1663±0.0205 |
| DR MPC | 89.5±8.8 | 17.06±3.45 | 100.0 | 0.460±0.055 | 82.9±3.4 | 16.9±3.3 | 0.1656±0.0345 |
| Safe-Regret MPC | 95.0±5.5 | 17.00±2.61 | 100.0 | 0.443±0.061 | 83.0±2.6 | 19.9±4.2 | 0.1650±0.0261 |
