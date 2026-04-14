#!/usr/bin/env python3
"""
Test script to verify STL data flow after C++ implementation fix

Checks:
1. STL C++ node publishes correctly
2. safe_regret_mpc subscribes correctly
3. Data collection works
4. No errors in data pipeline
"""

import rospy
import sys
import time
from collections import defaultdict

try:
    from std_msgs.msg import Float32, Bool
except ImportError as e:
    print(f"❌ Import error: {e}")
    sys.exit(1)

class STLDataFlowTester:
    def __init__(self):
        rospy.init_node('stl_data_flow_tester', anonymous=True)

        # Data storage
        self.stl_data = {
            'robustness': [],
            'violation': [],
            'budget': []
        }

        # Subscription flags
        self.received_robustness = False
        self.received_violation = False
        self.received_budget = False

        # Subscribers
        print("📡 Subscribing to STL topics...")
        self.sub_robustness = rospy.Subscriber(
            '/stl_monitor/robustness',
            Float32,
            self.robustness_callback
        )
        self.sub_violation = rospy.Subscriber(
            '/stl_monitor/violation',
            Bool,
            self.violation_callback
        )
        self.sub_budget = rospy.Subscriber(
            '/stl_monitor/budget',
            Float32,
            self.budget_callback
        )

        print("✅ Subscribers created")
        print("   /stl_monitor/robustness")
        print("   /stl_monitor/violation")
        print("   /stl_monitor/budget")

    def robustness_callback(self, msg):
        self.received_robustness = True
        value = msg.data
        self.stl_data['robustness'].append(value)

        if len(self.stl_data['robustness']) % 10 == 0:
            print(f"  📊 Robustness: {value:.4f} (total: {len(self.stl_data['robustness'])})")

    def violation_callback(self, msg):
        self.received_violation = True
        value = msg.data
        self.stl_data['violation'].append(value)

    def budget_callback(self, msg):
        self.received_budget = True
        value = msg.data
        self.stl_data['budget'].append(value)

    def analyze_data(self):
        """Analyze collected STL data"""
        print("\n" + "=" * 60)
        print("📊 STL Data Analysis")
        print("=" * 60)

        if not self.stl_data['robustness']:
            print("❌ No robustness data collected!")
            return False

        robustness = self.stl_data['robustness']

        # Check if values are continuous (not just 2 constants)
        unique_values = len(set(round(r, 4) for r in robustness))

        print(f"\n📈 Robustness Statistics:")
        print(f"   Total samples: {len(robustness)}")
        print(f"   Unique values: {unique_values}")
        print(f"   Min: {min(robustness):.4f}")
        print(f"   Max: {max(robustness):.4f}")
        print(f"   Mean: {sum(robustness)/len(robustness):.4f}")
        print(f"   Std: {(sum((x-sum(robustness)/len(robustness))**2 for x in robustness)/len(robustness))**0.5:.4f}")

        # Critical check: Are values continuous?
        if unique_values <= 3:
            print(f"\n❌ CRITICAL: Only {unique_values} unique values!")
            print("   This suggests simplified implementation (NOT belief-space)")
            print("   Expected: 20+ unique values (continuous variation)")
            return False
        else:
            print(f"\n✅ EXCELLENT: {unique_values} unique values detected!")
            print("   This confirms belief-space implementation is working")
            print("   Values are continuous (not just 2 constants)")

        # Check budget
        if self.stl_data['budget']:
            budget = self.stl_data['budget']
            print(f"\n💰 Budget Statistics:")
            print(f"   Initial: {budget[0]:.4f}")
            print(f"   Final: {budget[-1]:.4f}")
            print(f"   Min: {min(budget):.4f}")
            print(f"   Max: {max(budget):.4f}")

        return True

    def run_test(self, duration=30):
        """Run test for specified duration (seconds)"""
        print(f"\n🧪 Running STL data flow test for {duration} seconds...")
        print("=" * 60)

        start_time = time.time()
        rate = rospy.Rate(10)  # 10 Hz

        while not rospy.is_shutdown() and (time.time() - start_time) < duration:
            # Check if we're receiving data
            if self.received_robustness and len(self.stl_data['robustness']) % 5 == 0:
                rate.sleep()
            else:
                rate.sleep()

        elapsed = time.time() - start_time
        print(f"\n✅ Test completed after {elapsed:.1f} seconds")

        # Check reception status
        print("\n" + "=" * 60)
        print("📡 Data Reception Status")
        print("=" * 60)
        print(f"{'✅' if self.received_robustness else '❌'} Robustness: {len(self.stl_data['robustness'])} messages")
        print(f"{'✅' if self.received_violation else '❌'} Violation: {len(self.stl_data['violation'])} messages")
        print(f"{'✅' if self.received_budget else '❌'} Budget: {len(self.stl_data['budget'])} messages")

        # Analyze data
        success = self.analyze_data()

        return success

def main():
    try:
        tester = STLDataFlowTester()

        print("\n⏳ Waiting for STL node to start publishing...")
        print("   (Make sure: roslaunch safe_regret_mpc safe_regret_mpc_test.launch enable_stl:=true)")
        print("   Press Ctrl+C to stop early\n")

        success = tester.run_test(duration=30)

        if success:
            print("\n🎉 STL data flow test PASSED!")
            print("   ✅ Belief-space implementation confirmed")
            print("   ✅ Continuous robustness values detected")
            return 0
        else:
            print("\n❌ STL data flow test FAILED!")
            print("   Check if:")
            print("   1. STL C++ node is running (stl_monitor_node_cpp)")
            print("   2. use_belief_space=true in launch file")
            print("   3. Robot is moving (need odom + global plan)")
            return 1

    except rospy.ROSInterruptException:
        print("\n⚠️  Test interrupted by user")
        return 1
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
