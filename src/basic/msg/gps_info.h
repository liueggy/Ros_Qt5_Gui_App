#pragma once

#include <cstdint>
#include <string>

namespace basic {

struct GpsFix {
  double latitude{0.0};
  double longitude{0.0};
  double altitude{0.0};
  int status{-1};
  int service{0};
  double horizontal_variance{0.0};
  std::int64_t stamp_sec{0};
  std::uint32_t stamp_nsec{0};
  std::string frame_id;

  bool IsValid() const {
    return status >= 0 && latitude >= -90.0 && latitude <= 90.0 &&
           longitude >= -180.0 && longitude <= 180.0;
  }
};

struct GpsStatus {
  bool connected{false};
  bool nmea_online{false};
  bool fix{false};
  int fix_quality{0};
  int satellites{0};
  int satellites_visible{0};
  int baud{0};
  double hdop{0.0};
  double nmea_age_sec{0.0};
  double fix_age_sec{0.0};
  std::string frame_id;
  std::string last_error;
};

}  // namespace basic
