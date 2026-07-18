#pragma once

namespace rosbridge2cpp::subscription_policy {

struct TopicPolicy {
  int queue_length;
  int throttle_rate_ms;
};

// Visualization data may be sampled because rendering stale intermediate
// frames adds latency without improving the operator's view. Command, safety,
// mission and diagnostic topics intentionally do not use these policies.
inline constexpr TopicPolicy kMap{1, 500};
inline constexpr TopicPolicy kBattery{1, 500};
inline constexpr TopicPolicy kLocalCostMap{1, 200};
inline constexpr TopicPolicy kGlobalCostMap{1, 500};
inline constexpr TopicPolicy kLaserScan{1, 100};
inline constexpr TopicPolicy kGlobalPath{1, 500};
inline constexpr TopicPolicy kLocalPath{1, 100};
inline constexpr TopicPolicy kOdometry{1, 50};
inline constexpr TopicPolicy kLocalizationPose{1, 100};
inline constexpr TopicPolicy kRobotFootprint{1, 100};
inline constexpr TopicPolicy kImage{1, 100};
// TF messages can contain different frame edges. Dropping a whole message can
// break the transform graph, so keep their original queues and do not throttle.
inline constexpr TopicPolicy kTf{10, 0};
inline constexpr TopicPolicy kTfStatic{5, 0};

// The deployed ROS1 image does not install topology_msgs. Keep this explicit
// so an eventual board capability can be enabled without restoring
// unconditional subscriptions that make rosbridge log type-resolution errors.
inline constexpr bool kRos1TopologyAvailable = false;

}  // namespace rosbridge2cpp::subscription_policy
