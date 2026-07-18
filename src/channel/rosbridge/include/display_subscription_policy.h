#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "config/config_define.h"

namespace rosbridge2cpp::display_subscription_policy {

inline bool IsDisplayStreamVisible(
    const std::vector<Config::DisplayConfig>& displays,
    const std::string& display_name) {
  const auto item = std::find_if(
      displays.begin(), displays.end(),
      [&display_name](const Config::DisplayConfig& config) {
        return config.display_name == display_name;
      });
  // Preserve the historic behaviour for old configuration files.
  return item == displays.end() || item->visible;
}

}  // namespace rosbridge2cpp::display_subscription_policy
