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

// The primary Qt camera view uses the annotated RKNN stream. The raw stream
// remains a board-side inference input and is not added as a second Qt view.
inline constexpr char kPrimaryCameraLocation[] = "front";
inline constexpr char kRawCameraTopic[] =
    "/camera/front/image_source/compressed";
inline constexpr char kLegacyOverlayCameraLocation[] = "front_overlay";
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
  ensure(kPrimaryCameraLocation, kOverlayCameraTopic, true);
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
  const auto primary = std::find_if(images.begin(), images.end(),
                                    [](const auto& image) {
                                      return image.location ==
                                             kPrimaryCameraLocation;
                                    });
  if (primary != images.end() && primary->topic == kRawCameraTopic) {
    primary->topic = kOverlayCameraTopic;
    primary->enable = true;
  }

  images.erase(
      std::remove_if(images.begin(), images.end(), [](const auto& image) {
        return image.location == kLegacyOverlayCameraLocation &&
               image.topic == kOverlayCameraTopic;
      }),
      images.end());
}

inline const char* PreferredCameraTopic(bool overlay_available) {
  return overlay_available ? kOverlayCameraTopic : kRawCameraTopic;
}

}  // namespace rosbridge2cpp::contract
