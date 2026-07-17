#include "config/config_manager.h"
#include <gtest/gtest.h>

TEST(ConfigManagerTest, RootSnapshotIsDetachedAndUpdatesAreControlled) {
  auto* manager = Config::ConfigManager::Instance();
  const auto original = manager->GetRootConfigSnapshot();
  auto detached = original;
  detached.map_config.path = "detached-only";

  EXPECT_NE(manager->GetRootConfigSnapshot().map_config.path, "detached-only");

  EXPECT_TRUE(manager->UpdateRootConfig(
      [](auto& config) { config.map_config.path = "controlled-update"; }, false));
  EXPECT_EQ(manager->GetRootConfigSnapshot().map_config.path, "controlled-update");

  EXPECT_TRUE(manager->UpdateRootConfig(
      [&original](auto& config) { config = original; }, false));
}

TEST(ConfigManagerTest, MalformedRootConfigIsRejectedWithoutPartialMutation) {
  Config::ConfigRoot parsed;
  parsed.map_config.path = "unchanged";
  std::string error;

  EXPECT_FALSE(Config::ConfigManager::ParseRootConfig(
      R"({"channel_config": )", parsed, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(parsed.map_config.path, "unchanged");
}

TEST(ConfigManagerTest, MapStyleColorsRoundTripAndOldConfigUsesDefaults) {
  Config::ConfigRoot styled;
  styled.map_style_config.grid_color = "#123456";
  styled.map_style_config.laser_color = "#ABCDEF";
  styled.map_style_config.global_path_color = "#102030";
  styled.map_style_config.local_path_color = "#0F766E";

  const nlohmann::json serialized = styled;
  const auto restored = serialized.get<Config::ConfigRoot>();
  EXPECT_EQ(restored.map_style_config.grid_color, "#123456");
  EXPECT_EQ(restored.map_style_config.laser_color, "#ABCDEF");
  EXPECT_EQ(restored.map_style_config.global_path_color, "#102030");
  EXPECT_EQ(restored.map_style_config.local_path_color, "#0F766E");
  EXPECT_TRUE(restored.map_style_config.discovery_animation);
  EXPECT_EQ(restored.map_style_config.discovery_animation_duration_ms, 280);

  const auto legacy = nlohmann::json::object().get<Config::ConfigRoot>();
  EXPECT_EQ(legacy.map_style_config.grid_color, "#607D75");
  EXPECT_EQ(legacy.map_style_config.global_path_color, "#2563EB");
  EXPECT_EQ(legacy.map_style_config.laser_point_size, 2);
  EXPECT_EQ(legacy.map_style_config.laser_opacity, 85);
  EXPECT_TRUE(legacy.map_style_config.discovery_animation);
}

TEST(TopologyMapTest, MissingPointHasSafeDefaultPose) {
  TopologyMap map;

  const auto missing = map.GetPoint("missing");

  EXPECT_TRUE(missing.name.empty());
  EXPECT_DOUBLE_EQ(missing.x, 0.0);
  EXPECT_DOUBLE_EQ(missing.y, 0.0);
  EXPECT_DOUBLE_EQ(missing.theta, 0.0);
}

TEST(TopologyMapTest, ReadAndWriteMap) {
  TopologyMap map;
  map.map_name = "test";
  map.points.push_back(TopologyMap::PointInfo(1.11, 2.22, 3.33, "test1"));
  map.points.push_back(TopologyMap::PointInfo(2.11, .22, 4.33, "test2"));
  EXPECT_TRUE(Config::ConfigManager::Instance()->WriteTopologyMap(
      "./test_map.json", map));

  TopologyMap map_read;
  EXPECT_TRUE(Config::ConfigManager::Instance()->ReadTopologyMap(
      "./test_map.json", map_read));
  EXPECT_EQ(map_read.points.size(), map.points.size());
  for (int i = 0; i < map_read.points.size(); i++) {
    EXPECT_EQ(map_read.points[i].x, map.points[i].x);
    EXPECT_EQ(map_read.points[i].y, map.points[i].y);
    EXPECT_EQ(map_read.points[i].theta, map.points[i].theta);
    EXPECT_EQ(map_read.points[i].name, map.points[i].name);
    EXPECT_EQ(map_read.points[i].type, map.points[i].type);
  }
}
