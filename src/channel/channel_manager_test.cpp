#include "channel_manager.h"

#include <gtest/gtest.h>

TEST(ChannelManagerTest, TailscalePresetReusesRosbridgePlugin) {
  EXPECT_EQ(ChannelManager::NormalizeStoredChannelType("tailscale_rosbridge"),
            "rosbridge");
  EXPECT_EQ(ChannelManager::NormalizeStoredChannelType("rosbridge"),
            "rosbridge");
}
