#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace rosbridge2cpp::contract {

inline constexpr char kGlobalPathTopic[] = "/move_base/NavfnROS/plan";
inline constexpr char kLocalPathTopic[] =
    "/move_base/TebLocalPlannerROS/local_plan";
inline constexpr char kGlobalCostMapTopic[] =
    "/move_base/global_costmap/costmap";
inline constexpr char kLocalCostMapTopic[] =
    "/move_base/local_costmap/costmap";
inline constexpr char kFootprintTopic[] =
    "/move_base/local_costmap/published_footprint";
inline constexpr char kManualCmdVelTopic[] = "/cmd_vel/manual";
inline constexpr char kEmergencyStopTopic[] = "/eggy/emergency_stop";

// "raw" means the stable, unannotated camera stream. The board publishes it
// as CompressedImage to avoid an unnecessary decode/re-encode cycle.
inline constexpr char kRawCameraLocation[] = "front";
inline constexpr char kRawCameraTopic[] =
    "/camera/front/image_source/compressed";
inline constexpr char kOverlayCameraLocation[] = "front_overlay";
inline constexpr char kOverlayCameraTopic[] =
    "/camera/front/image/compressed";

template <typename ImageConfigs>
void EnsureStableCameraContracts(ImageConfigs& images) {
  auto ensure = [&images](const char* location, const char* topic,
                          bool enabled_when_added) {
    auto found = std::find_if(images.begin(), images.end(),
                              [location](const auto& image) {
                                return image.location == location;
                              });
    if (found == images.end()) {
      images.push_back({location, topic, enabled_when_added});
    } else if (found->topic.empty()) {
      // Repair only incomplete entries. Explicit user topics and enable state
      // remain authoritative across profiles and upgrades.
      found->topic = topic;
    }
  };
  ensure(kRawCameraLocation, kRawCameraTopic, true);
  ensure(kOverlayCameraLocation, kOverlayCameraTopic, false);
}

template <typename DisplayConfigs>
void MigrateLegacyTopic(DisplayConfigs& configs, const std::string& display_name,
                        std::string_view legacy_topic,
                        std::string_view current_topic) {
  const auto found = std::find_if(configs.begin(), configs.end(),
                                  [&display_name](const auto& item) {
                                    return item.display_name == display_name;
                                  });
  if (found != configs.end() && found->topic == std::string(legacy_topic)) {
    found->topic = std::string(current_topic);
  }
}

template <typename ImageConfigs>
void MigrateLegacyCameraTopic(ImageConfigs& images) {
  const auto found = std::find_if(images.begin(), images.end(),
                                  [](const auto& image) {
                                    return image.location == kRawCameraLocation;
                                  });
  if (found != images.end() && found->topic == kOverlayCameraTopic) {
    found->topic = kRawCameraTopic;
  }
}

inline const char* PreferredCameraTopic(bool overlay_available) {
  return overlay_available ? kOverlayCameraTopic : kRawCameraTopic;
}

}  // namespace rosbridge2cpp::contract
