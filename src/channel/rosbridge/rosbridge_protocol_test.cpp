#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "include/bounded_payload_queue.h"
#include "include/display_subscription_policy.h"
#include "include/latest_value_queue.h"
#include "include/protocol_validation.h"
#include "include/ros_bridge.h"
#include "include/ros_topic.h"
#include "include/messages/rosbridge_publish_msg.h"
#include "include/rosbridge_contract.h"
#include "include/subscription_policy.h"
#include "tf2_rosbridge.h"

namespace {

rapidjson::Document Parse(const char* json) {
  rapidjson::Document document;
  document.Parse(json);
  return document;
}

class FakeTransport : public rosbridge2cpp::ITransportLayer {
 public:
  bool Init(std::string, int) override { return true; }
  bool SendMessage(std::string) override {
    ++send_count;
    return send_success;
  }
  bool IsHealthy() const override { return true; }
  void RegisterIncomingMessageCallback(
      std::function<void(rapidjson::Document&)>) override {}
  void RegisterErrorCallback(
      std::function<void(rosbridge2cpp::TransportError)>) override {}
  void ReportError(rosbridge2cpp::TransportError) override {}
  void SetTransportMode(TransportMode) override {}

  bool send_success{true};
  int send_count{0};
};

TEST(RosbridgeContractTest, UsesDirectMoveBaseTopics) {
  EXPECT_STREQ(rosbridge2cpp::contract::kGlobalPathTopic,
               "/move_base/NavfnROS/plan");
  EXPECT_STREQ(rosbridge2cpp::contract::kLocalPathTopic,
               "/move_base/TebLocalPlannerROS/local_plan");
  EXPECT_STREQ(rosbridge2cpp::contract::kGlobalCostMapTopic,
               "/move_base/global_costmap/costmap");
  EXPECT_STREQ(rosbridge2cpp::contract::kLocalCostMapTopic,
               "/move_base/local_costmap/costmap");
  EXPECT_STREQ(rosbridge2cpp::contract::kFootprintTopic,
               "/move_base/local_costmap/published_footprint");
  EXPECT_STREQ(rosbridge2cpp::contract::kManualCmdVelTopic,
               "/cmd_vel/manual");
  EXPECT_STREQ(rosbridge2cpp::contract::kEmergencyStopTopic,
               "/eggy/emergency_stop");
  EXPECT_STREQ(rosbridge2cpp::contract::kRawCameraTopic,
               "/camera/front/image_source/compressed");
  EXPECT_STREQ(rosbridge2cpp::contract::kOverlayCameraTopic,
               "/camera/front/image/compressed");
}

TEST(DisplaySubscriptionPolicyTest, HiddenHeavyLayersStayOffTheWire) {
  std::vector<Config::DisplayConfig> displays = {
      {"kGlobalCostMap", "/global_costmap", false},
      {"kLocalPath", "/local_path", false},
  };

  EXPECT_FALSE(rosbridge2cpp::display_subscription_policy::IsDisplayStreamVisible(
      displays, "kGlobalCostMap"));
  EXPECT_FALSE(rosbridge2cpp::display_subscription_policy::IsDisplayStreamVisible(
      displays, "kLocalPath"));
}

TEST(DisplaySubscriptionPolicyTest, MissingOrEnabledLayersRemainCompatible) {
  std::vector<Config::DisplayConfig> displays = {
      {"kGlobalPath", "/global_path", true},
  };

  EXPECT_TRUE(rosbridge2cpp::display_subscription_policy::IsDisplayStreamVisible(
      displays, "kGlobalPath"));
  EXPECT_TRUE(rosbridge2cpp::display_subscription_policy::IsDisplayStreamVisible(
      displays, "kLocalCostMap"));
}

TEST(RosbridgeSubscriptionPolicyTest, BoundsVisualizationQueuesAndLatency) {
  using namespace rosbridge2cpp::subscription_policy;
  for (const TopicPolicy policy : {kMap, kLocalCostMap, kGlobalCostMap,
                                   kLaserScan, kGlobalPath, kLocalPath,
                                   kOdometry, kLocalizationPose,
                                   kRobotFootprint, kImage}) {
    EXPECT_EQ(policy.queue_length, 1);
    EXPECT_GE(policy.throttle_rate_ms, 0);
  }
  EXPECT_LE(kOdometry.throttle_rate_ms, 50);
  EXPECT_LE(kLocalPath.throttle_rate_ms, 100);
  EXPECT_LE(kImage.throttle_rate_ms, 100);
  EXPECT_EQ(kBattery.queue_length, 1);
  EXPECT_EQ(kBattery.throttle_rate_ms, 500);
  EXPECT_EQ(kTf.queue_length, 10);
  EXPECT_EQ(kTf.throttle_rate_ms, 0);
  EXPECT_EQ(kTfStatic.queue_length, 5);
  EXPECT_EQ(kTfStatic.throttle_rate_ms, 0);
}

TEST(RosbridgeSubscriptionPolicyTest, DisablesUnavailableRos1TopologyTypes) {
  EXPECT_FALSE(
      rosbridge2cpp::subscription_policy::kRos1TopologyAvailable);
}

TEST(TF2RosbridgeTest, ReportsMissingTransformInsteadOfReturningFreshOrigin) {
  rosbridge2cpp::TF2Rosbridge tf;
  basic::RobotPose pose;
  EXPECT_FALSE(tf.TryLookUpForTransform("map", "laser_frame", &pose));
}

TEST(TF2RosbridgeTest, ComposesMapToLaserIncludingMountOffset) {
  rosbridge2cpp::TF2Rosbridge tf;
  std::unordered_map<std::string, rosbridge2cpp::TransformData> transforms;
  transforms["base_link"] = {1.0, 2.0, 0.0, "map"};
  transforms["laser_frame"] = {0.05, 0.0, 0.0, "base_link"};
  tf.UpdateTF(transforms);

  basic::RobotPose pose;
  ASSERT_TRUE(tf.TryLookUpForTransform("map", "laser_frame", &pose));
  EXPECT_NEAR(pose.x, 1.05, 1e-9);
  EXPECT_NEAR(pose.y, 2.0, 1e-9);
  EXPECT_NEAR(pose.theta, 0.0, 1e-9);
}

TEST(RosTopicLifecycleTest, RecoversAfterSubscribeAndUnsubscribeSendFailures) {
  FakeTransport transport;
  rosbridge2cpp::ROSBridge bridge(transport);
  rosbridge2cpp::ROSTopic topic(bridge, "/camera", "sensor_msgs/Image", 1);
  rosbridge2cpp::FunVrROSPublishMsg callback =
      [](const ROSBridgePublishMsg&) {};

  transport.send_success = false;
  const auto invalid_handle = topic.Subscribe(callback);
  EXPECT_FALSE(invalid_handle.IsValid());

  transport.send_success = true;
  const auto valid_handle = topic.Subscribe(callback);
  ASSERT_TRUE(valid_handle.IsValid());

  transport.send_success = false;
  EXPECT_FALSE(topic.Unsubscribe(valid_handle));

  transport.send_success = true;
  EXPECT_TRUE(topic.Unsubscribe(valid_handle));
  EXPECT_EQ(transport.send_count, 4);
}

TEST(RosbridgePublishMsgTest, OwnsPayloadAndSerializesRepeatedly) {
  rapidjson::Document payload;
  payload.SetObject();
  payload.AddMember(
      "data",
      rapidjson::Value(R"({"request_id":"mission-1","waypoints":[1,2]})",
                       payload.GetAllocator()),
      payload.GetAllocator());

  ROSBridgePublishMsg message(true);
  message.topic_ = "/eggy/mission/request";
  message.SetMessage(payload);

  ASSERT_TRUE(payload.HasMember("data"));
  EXPECT_TRUE(payload["data"].IsString());

  rapidjson::Document unused_allocator_owner;
  const auto first = message.ToJSON(unused_allocator_owner.GetAllocator());
  const auto second = message.ToJSON(unused_allocator_owner.GetAllocator());
  ASSERT_TRUE(first.HasMember("msg"));
  ASSERT_TRUE(second.HasMember("msg"));
  EXPECT_STREQ(first["msg"]["data"].GetString(),
               second["msg"]["data"].GetString());
  EXPECT_STREQ(payload["data"].GetString(), first["msg"]["data"].GetString());
}

TEST(RosbridgePublishMsgTest, ParsesIncomingEnvelopeWithoutPayloadCopy) {
  auto envelope = Parse(
      R"({"op":"publish","topic":"/scan","msg":{"ranges":[1.0,2.0]}})");
  ROSBridgePublishMsg message;
  ASSERT_TRUE(message.FromJSON(envelope));
  ASSERT_TRUE(message.msg_json_.HasMember("ranges"));
  EXPECT_EQ(message.msg_json_["ranges"].Size(), 2u);
}

TEST(ProtocolValidationTest, RejectsInvalidEnvelopeFieldTypesAndSizes) {
  std::string error;
  auto numeric_op = Parse(R"({"op":1,"topic":"/scan","msg":{}})");
  EXPECT_FALSE(rosbridge2cpp::validation::ValidateEnvelope(numeric_op, &error));

  auto numeric_topic = Parse(R"({"op":"publish","topic":7,"msg":{}})");
  EXPECT_FALSE(rosbridge2cpp::validation::ValidateEnvelope(numeric_topic, &error));

  std::string oversized_id(rosbridge2cpp::validation::kMaxIdLength + 1, 'x');
  const std::string oversized_json =
      std::string(R"({"op":"publish","topic":"/scan","id":")") +
      oversized_id + R"(","msg":{}})";
  auto oversized = Parse(oversized_json.c_str());
  EXPECT_FALSE(rosbridge2cpp::validation::ValidateEnvelope(oversized, &error));

  auto valid = Parse(R"({"op":"publish","topic":"/scan","id":"scan-1","msg":{}})");
  EXPECT_TRUE(rosbridge2cpp::validation::ValidateEnvelope(valid, &error));
}

TEST(ProtocolValidationTest, ChecksOccupancyGridProductAndExactPayloadLength) {
  std::string error;
  auto short_grid = Parse(
      R"({"info":{"width":2,"height":2,"resolution":0.05,"origin":{"position":{"x":0,"y":0}}},"data":[0,1,2]})");
  EXPECT_FALSE(
      rosbridge2cpp::validation::ValidateOccupancyGrid(short_grid, &error));

  auto valid_grid = Parse(
      R"({"info":{"width":2,"height":2,"resolution":0.05,"origin":{"position":{"x":0,"y":0}}},"data":[0,1,2,3]})");
  EXPECT_TRUE(
      rosbridge2cpp::validation::ValidateOccupancyGrid(valid_grid, &error));

  auto huge_grid = Parse(
      R"({"info":{"width":4294967295,"height":4294967295,"resolution":0.05,"origin":{"position":{"x":0,"y":0}}},"data":[]})");
  EXPECT_FALSE(
      rosbridge2cpp::validation::ValidateOccupancyGrid(huge_grid, &error));
}

TEST(ProtocolValidationTest, ValidatesRawImageStepEncodingAndDecodedSize) {
  std::string error;
  auto bad_step = Parse(
      R"({"width":2,"height":2,"step":5,"encoding":"rgb8","data":"AAAAAAAAAAAAAAAA"})");
  EXPECT_FALSE(rosbridge2cpp::validation::ValidateImageMetadata(bad_step, &error));

  auto valid = Parse(
      R"({"width":2,"height":2,"step":6,"encoding":"rgb8","data":"AAAAAAAAAAAAAAAA"})");
  EXPECT_TRUE(rosbridge2cpp::validation::ValidateImageMetadata(valid, &error));
  EXPECT_TRUE(rosbridge2cpp::validation::ValidateDecodedImageSize(
      valid, 12, &error));
  EXPECT_FALSE(rosbridge2cpp::validation::ValidateDecodedImageSize(
      valid, 11, &error));
}

TEST(ProtocolValidationTest, UsesReceiveDeadlineForHalfOpenDetection) {
  using namespace std::chrono;
  EXPECT_TRUE(rosbridge2cpp::validation::IsReceiveFresh(
      milliseconds(1000), milliseconds(14999), seconds(15)));
  EXPECT_FALSE(rosbridge2cpp::validation::IsReceiveFresh(
      milliseconds(1000), milliseconds(16001), seconds(15)));
}

TEST(LatestValueQueueTest, ReplacesPendingValuePerKeyAndStaysBounded) {
  rosbridge2cpp::LatestValueQueue<std::string, int> queue(2);
  EXPECT_TRUE(queue.Push("front", 1));
  EXPECT_TRUE(queue.Push("front", 2));
  EXPECT_EQ(queue.Size(), 1u);
  EXPECT_TRUE(queue.Push("overlay", 3));
  EXPECT_EQ(queue.Size(), 2u);
  EXPECT_FALSE(queue.Push("third", 4));

  std::string key;
  int value = 0;
  ASSERT_TRUE(queue.TryPop(&key, &value));
  EXPECT_EQ(key, "front");
  EXPECT_EQ(value, 2);
}

TEST(BoundedPayloadQueueTest, EnforcesMessageAndByteLimitsInFifoOrder) {
  rosbridge2cpp::BoundedPayloadQueue queue(2, 5);
  EXPECT_TRUE(queue.Push("ab"));
  EXPECT_TRUE(queue.Push("cd"));
  EXPECT_FALSE(queue.Push("e"));
  EXPECT_EQ(queue.Size(), 2u);
  EXPECT_EQ(queue.Bytes(), 4u);

  std::string payload;
  EXPECT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "ab");
  EXPECT_TRUE(queue.Push("e"));
  EXPECT_FALSE(queue.Push("abcdef"));
  EXPECT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "cd");
  EXPECT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "e");
}

TEST(BoundedPayloadQueueTest, CloseDropsStaleFramesAndResetAllowsReconnect) {
  rosbridge2cpp::BoundedPayloadQueue queue(4, 64);
  EXPECT_TRUE(queue.Push("stale-map"));
  queue.Close();

  std::string payload;
  EXPECT_FALSE(queue.WaitPop(&payload));
  EXPECT_FALSE(queue.Push("closed"));

  queue.Reset();
  EXPECT_TRUE(queue.Push("fresh-scan"));
  EXPECT_TRUE(queue.WaitPop(&payload));
  EXPECT_EQ(payload, "fresh-scan");
}

TEST(BoundedPayloadQueueTest, LatestValuesReplaceQueuedTelemetryInPlace) {
  rosbridge2cpp::BoundedPayloadQueue queue(4, 64);
  ASSERT_TRUE(queue.PushLatest("/scan", "scan-old"));
  ASSERT_TRUE(queue.Push("command"));
  ASSERT_TRUE(queue.PushLatest("/scan", "scan-new"));
  EXPECT_EQ(queue.Size(), 2U);

  std::string payload;
  ASSERT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "scan-new");
  ASSERT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "command");
}

TEST(BoundedPayloadQueueTest, PriorityMessagesJumpAheadOfTelemetry) {
  rosbridge2cpp::BoundedPayloadQueue queue(4, 64);
  ASSERT_TRUE(queue.PushLatest("/map", "map"));
  ASSERT_TRUE(queue.PushPriority("safety-1"));
  ASSERT_TRUE(queue.PushPriority("safety-2"));

  std::string payload;
  ASSERT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "safety-1");
  ASSERT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "safety-2");
  ASSERT_TRUE(queue.TryPop(&payload));
  EXPECT_EQ(payload, "map");
}

TEST(RosbridgeContractTest, KeepsCustomPrimaryCameraAndAvoidsDuplicateView) {
  struct ImageConfig {
    std::string location;
    std::string topic;
    bool enable;
  };
  std::vector<ImageConfig> images{{"front", "/user/raw", false}};
  rosbridge2cpp::contract::EnsureStableCameraContracts(images);

  ASSERT_EQ(images.size(), 1u);
  EXPECT_EQ(images[0].topic, "/user/raw");
  EXPECT_FALSE(images[0].enable);
}

TEST(RosbridgeContractTest, FallsBackToRawWhenOverlayIsUnavailable) {
  EXPECT_STREQ(rosbridge2cpp::contract::PreferredCameraTopic(false),
               rosbridge2cpp::contract::kRawCameraTopic);
  EXPECT_STREQ(rosbridge2cpp::contract::PreferredCameraTopic(true),
               rosbridge2cpp::contract::kOverlayCameraTopic);
}

TEST(RosbridgeContractTest, MigratesOnlyKnownLegacyDefaults) {
  struct DisplayConfig {
    std::string display_name;
    std::string topic;
  };
  std::vector<DisplayConfig> displays{{"speed", "/cmd_vel"},
                                      {"custom", "/user/topic"}};
  rosbridge2cpp::contract::MigrateLegacyTopic(
      displays, "speed", "/cmd_vel",
      rosbridge2cpp::contract::kManualCmdVelTopic);
  rosbridge2cpp::contract::MigrateLegacyTopic(
      displays, "custom", "/cmd_vel", "/should-not-change");
  EXPECT_EQ(displays[0].topic, "/cmd_vel/manual");
  EXPECT_EQ(displays[1].topic, "/user/topic");

  struct ImageConfig {
    std::string location;
    std::string topic;
    bool enable;
  };
  std::vector<ImageConfig> images{
      {"front", "/camera/front/image_source/compressed", true},
      {"front_overlay", "/camera/front/image/compressed", false}};
  rosbridge2cpp::contract::MigrateLegacyCameraTopic(images);
  ASSERT_EQ(images.size(), 1u);
  EXPECT_EQ(images[0].topic,
            rosbridge2cpp::contract::kOverlayCameraTopic);
  EXPECT_TRUE(images[0].enable);
}

}  // namespace
