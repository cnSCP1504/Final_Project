#include <iostream>
#include <cmath>
#include <iomanip>
#include <vector>

using namespace std;

const double PI = 3.141592653589793;

// OLD METHOD (buggy)
double calculate_etheta_old(double theta, double traj_deg) {
    double temp_theta = theta;

    if(temp_theta <= -PI + traj_deg)
        temp_theta = temp_theta + 2 * PI;

    double etheta;
    if(temp_theta - traj_deg < 1.8 * PI)
        etheta = temp_theta - traj_deg;
    else
        etheta = 0;

    return etheta;
}

// NEW METHOD (fixed)
double calculate_etheta_new(double theta, double traj_deg) {
    double etheta;

    if(true) {  // gx && gy equivalent
        double angle_diff = theta - traj_deg;
        // Normalize to [-π, π] using atan2
        etheta = atan2(sin(angle_diff), cos(angle_diff));
    } else {
        etheta = 0;
    }

    return etheta;
}

int main() {
    cout << "=== Etheta Normalization Test ===" << endl;
    cout << fixed << setprecision(4);
    cout << "Testing angle difference calculation for large angle turns" << endl;
    cout << endl;

    // Test cases
    vector<tuple<double, double, string>> tests = {
        {0.0, 0.0, "No turn"},
        {0.0, PI/2, "90° left turn"},
        {0.0, -PI/2, "90° right turn"},
        {0.0, PI, "180° turn"},
        {0.0, -PI, "180° turn (negative)"},
        {-170.0 * PI/180.0, 80.0 * PI/180.0, "Large angle: -170° to 80°"},
        {150.0 * PI/180.0, -120.0 * PI/180.0, "Large angle: 150° to -120°"},
        {-179.0 * PI/180.0, 179.0 * PI/180.0, "Near boundary: -179° to 179°"},
        {170.0 * PI/180.0, -170.0 * PI/180.0, "Near boundary: 170° to -170°"}
    };

    cout << setw(35) << "Test Case"
         << setw(12) << "Theta(rad)"
         << setw(12) << "Traj(rad)"
         << setw(15) << "OLD Etheta"
         << setw(15) << "NEW Etheta"
         << setw(12) << "Correct?" << endl;
    cout << string(100, '-') << endl;

    for (const auto& test : tests) {
        double theta = get<0>(test);
        double traj_deg = get<1>(test);
        string description = get<2>(test);

        double etheta_old = calculate_etheta_old(theta, traj_deg);
        double etheta_new = calculate_etheta_new(theta, traj_deg);

        // Check if new method produces valid result (within [-π, π])
        bool is_valid = (etheta_new >= -PI && etheta_new <= PI);

        // For comparison, compute expected using manual normalization
        double expected = theta - traj_deg;
        while (expected > PI) expected -= 2 * PI;
        while (expected < -PI) expected += 2 * PI;

        bool matches_expected = (abs(etheta_new - expected) < 0.01);

        cout << setw(35) << description
             << setw(12) << theta
             << setw(12) << traj_deg
             << setw(15) << etheta_old
             << setw(15) << etheta_new
             << setw(12) << (matches_expected ? "✓ YES" : "✗ NO") << endl;
    }

    cout << endl;
    cout << "=== Key Observations ===" << endl;
    cout << "1. NEW method always returns values in [-π, π] range" << endl;
    cout << "2. OLD method can produce values outside valid range" << endl;
    cout << "3. NEW method correctly handles -π/π boundary crossings" << endl;
    cout << "4. For large angle turns, NEW method chooses shortest rotation direction" << endl;

    return 0;
}
