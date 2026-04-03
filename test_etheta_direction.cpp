#include <iostream>
#include <cmath>
#include <vector>

using namespace std;

const double PI = 3.141592653589793;

// 模拟当前etheta计算逻辑
double calculate_etheta_current(double traj_deg, double theta) {
    double angle_diff = traj_deg - theta;
    double etheta = atan2(sin(angle_diff), cos(angle_diff));
    return etheta;
}

// 改进的etheta计算：考虑最短转向路径
double calculate_etheta_improved(double traj_deg, double theta) {
    double angle_diff = traj_deg - theta;

    // 归一化到[-π, π]
    while (angle_diff > PI) angle_diff -= 2 * PI;
    while (angle_diff < -PI) angle_diff += 2 * PI;

    // 如果角度差接近±π，选择最短的转向方向
    if (fabs(angle_diff) > PI / 2) {
        // 如果角度差>90度，选择反转方向
        if (angle_diff > 0) {
            angle_diff -= PI;
        } else {
            angle_diff += PI;
        }
    }

    return angle_diff;
}

// 测试用例
struct TestCase {
    double traj_deg;   // 路径方向
    double theta;      // 机器人朝向
    string description;
};

int main() {
    vector<TestCase> test_cases = {
        {0.0, 0.0, "同向"},
        {PI/2, 0.0, "向左转90度"},
        {-PI/2, 0.0, "向右转90度"},
        {PI, 0.0, "反向180度（大角度转向）"},
        {0.0, PI, "反向180度（机器人朝向相反）"},
        {PI*0.9, 0.0, "接近180度"},
        {-PI*0.9, 0.0, "接近-180度"},
        {0.0, -PI/2, "机器人朝南，路径朝东"},
        {PI, PI, "都是180度，同向"}
    };

    cout << "=========================================" << endl;
    cout << "ETHeta 计算对比测试" << endl;
    cout << "=========================================" << endl;
    cout << endl;

    for (const auto& test : test_cases) {
        double etheta_current = calculate_etheta_current(test.traj_deg, test.theta);
        double etheta_improved = calculate_etheta_improved(test.traj_deg, test.theta);

        cout << "测试: " << test.description << endl;
        cout << "  路径方向: " << test.traj_deg << " (" << (test.traj_deg * 180.0 / PI) << "°)" << endl;
        cout << "  机器人朝向: " << test.theta << " (" << (test.theta * 180.0 / PI) << "°)" << endl;
        cout << "  当前方法 etheta: " << etheta_current << " (" << (etheta_current * 180.0 / PI) << "°)" << endl;
        cout << "  改进方法 etheta: " << etheta_improved << " (" << (etheta_improved * 180.0 / PI) << "°)" << endl;

        // 判断哪个更合理
        double current_deg = etheta_current * 180.0 / PI;
        double improved_deg = etheta_improved * 180.0 / PI;

        if (fabs(current_deg) > 90.0 && fabs(improved_deg) <= 90.0) {
            cout << "  ⚠️  当前方法会选择错误方向！" << endl;
        } else if (fabs(current_deg) <= 90.0 && fabs(improved_deg) <= 90.0) {
            cout << "  ✓  两种方法都合理" << endl;
        } else {
            cout << "  ℹ️  两种方法都选择大角度转向" << endl;
        }
        cout << endl;
    }

    cout << "=========================================" << endl;
    cout << "问题分析：" << endl;
    cout << "=========================================" << endl;
    cout << "当机器人需要转向>90度时：" << endl;
    cout << "1. 当前方法：会计算角度差为接近180度" << endl;
    cout << "2. 机器人可能选择继续前进（实际上是反向）" << endl;
    cout << "3. 应该优先考虑原地旋转或大幅转向" << endl;
    cout << endl;
    cout << "建议的修复策略：" << endl;
    cout << "1. 当|angle_diff| > 90度时，应该优先调整朝向" << endl;
    cout << "2. 可以考虑临时降低速度，增加角速度" << endl;
    cout << "3. 或者使用原地旋转（如果机器人支持）" << endl;

    return 0;
}
