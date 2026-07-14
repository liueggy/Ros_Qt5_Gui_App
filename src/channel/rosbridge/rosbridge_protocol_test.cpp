#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "include/latest_value_queue.h"
#include "include/protocol_validation.h"
#include "include/rosbridge_contract.h"

namespace {

rapidjson::Document Parse(const char* json) {
  rapidjson::Document document;
  document.Parse(json);
  return document;
}

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

TEST(RosbridgeContractTest, KeepsRawAndOverlaySemanticsStable) {
  struct ImageConfig {
    std::string location;
    std::string topic;
    bool enable;
  };
  std::vector<ImageConfig> images{{"front", "/user/raw", false}};
  rosbridge2cpp::contract::EnsureStableCameraContracts(images);

  ASSERT_EQ(images.size(), 2u);
  EXPECT_EQ(images[0].topic, "/user/raw");
  EXPECT_FALSE(images[0].enable);
  EXPECT_EQ(images[1].location, "front_overlay");
  EXPECT_EQ(images[1].topic, rosbridge2cpp::contract::kOverlayCameraTopic);
  EXPECT_FALSE(images[1].enable);
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
  std::vector<ImageConfig> images{{"front", "/camera/front/image/compressed",
                                   true}};
  rosbridge2cpp::contract::MigrateLegacyCameraTopic(images);
  EXPECT_EQ(images[0].topic,
            rosbridge2cpp::contract::kRawCameraTopic);
}

}  // namespace
