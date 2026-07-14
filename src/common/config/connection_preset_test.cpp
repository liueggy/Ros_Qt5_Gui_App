#include "config_manager.h"

#include <gtest/gtest.h>

TEST(ConnectionPresetTest, RosbridgePresetsRoundTripIndependently) {
  Config::ConfigRoot root;
  root.channel_config.channel_type = "tailscale_rosbridge";
  root.channel_config.rosbridge_config = {"192.168.31.215", "9090"};
  root.channel_config.tailscale_rosbridge_config =
      {"eggy-firefly.tailnet.ts.net", "10090"};

  const auto restored = nlohmann::json(root).get<Config::ConfigRoot>();

  EXPECT_EQ(restored.channel_config.channel_type, "tailscale_rosbridge");
  EXPECT_EQ(restored.channel_config.rosbridge_config.ip, "192.168.31.215");
  EXPECT_EQ(restored.channel_config.rosbridge_config.port, "9090");
  EXPECT_EQ(restored.channel_config.tailscale_rosbridge_config.ip,
            "eggy-firefly.tailnet.ts.net");
  EXPECT_EQ(restored.channel_config.tailscale_rosbridge_config.port, "10090");

  auto selected = restored.channel_config;
  selected.channel_type = Config::kRosbridgeChannelType;
  EXPECT_EQ(Config::SelectedRosbridgeConfig(selected).ip, "192.168.31.215");
  selected.channel_type = Config::kTailscaleRosbridgeChannelType;
  EXPECT_EQ(Config::SelectedRosbridgeConfig(selected).ip,
            "eggy-firefly.tailnet.ts.net");
}

TEST(ConnectionPresetTest, LegacyRosbridgeConfigKeepsOriginalBehavior) {
  const auto legacy = nlohmann::json::parse(R"({
    "channel_config": {
      "channel_type": "rosbridge",
      "rosbridge_config": {"ip": "192.168.31.50", "port": "9090"}
    }
  })")
                          .get<Config::ConfigRoot>();

  EXPECT_EQ(legacy.channel_config.channel_type, "rosbridge");
  EXPECT_EQ(legacy.channel_config.rosbridge_config.ip, "192.168.31.50");
  EXPECT_EQ(legacy.channel_config.rosbridge_config.port, "9090");
  EXPECT_TRUE(legacy.channel_config.tailscale_rosbridge_config.ip.empty());
  EXPECT_EQ(legacy.channel_config.tailscale_rosbridge_config.port, "9090");
}
