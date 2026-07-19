#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

#include "point_type.h"

namespace AppContract {

inline std::string JsonStringOr(const nlohmann::json& object,
                                const char* key,
                                std::string fallback = {}) {
  if (!object.is_object()) {
    return fallback;
  }
  const auto found = object.find(key);
  return found != object.end() && found->is_string()
             ? found->get<std::string>()
             : fallback;
}

inline int JsonIntOr(const nlohmann::json& object, const char* key,
                     int fallback) {
  if (!object.is_object()) {
    return fallback;
  }
  const auto found = object.find(key);
  return found != object.end() && found->is_number_integer()
             ? found->get<int>()
             : fallback;
}

inline bool JsonBoolOr(const nlohmann::json& object, const char* key,
                       bool fallback) {
  if (!object.is_object()) {
    return fallback;
  }
  const auto found = object.find(key);
  return found != object.end() && found->is_boolean()
             ? found->get<bool>()
             : fallback;
}

struct RelocationSampleEvaluation {
  double distance = {0.0};
  double angle_error = {0.0};
  bool acceptable = {false};
};

inline bool IsRelocationConfirmationSample(
    std::chrono::steady_clock::time_point sample_received_at,
    std::chrono::steady_clock::time_point relocation_started_at,
    const RelocationSampleEvaluation& evaluation) {
  return sample_received_at >= relocation_started_at &&
         evaluation.acceptable;
}

inline bool IsMissionTerminalStage(const std::string& stage) {
  return stage == "completed" || stage == "complete" ||
         stage == "cancelled" || stage == "error" ||
         stage == "emergency_stopped";
}

inline bool CanConfigureInspectionOption(bool mission_running,
                                         bool /*capability_ready*/) {
  return !mission_running;
}

inline RelocationSampleEvaluation EvaluateRelocationSample(
    const basic::RobotPose& target,
    const basic::LocalizationEstimate& estimate) {
  constexpr double kPi = 3.14159265358979323846;
  constexpr double kMaxPositionErrorMeters = 0.35;
  constexpr double kMaxHeadingErrorRadians = 15.0 * kPi / 180.0;
  constexpr double kMaxPositionVariance = 0.50;
  constexpr double kMaxHeadingVariance = 0.30;

  const double dx = estimate.pose.x - target.x;
  const double dy = estimate.pose.y - target.y;
  const double distance = std::hypot(dx, dy);
  const double angle_error = std::abs(std::atan2(
      std::sin(estimate.pose.theta - target.theta),
      std::cos(estimate.pose.theta - target.theta)));
  const bool finite = std::isfinite(distance) && std::isfinite(angle_error) &&
                      std::isfinite(estimate.xy_variance) &&
                      std::isfinite(estimate.yaw_variance);
  return {
      distance,
      angle_error,
      finite && distance <= kMaxPositionErrorMeters &&
          angle_error <= kMaxHeadingErrorRadians &&
          estimate.xy_variance <= kMaxPositionVariance &&
          estimate.yaw_variance <= kMaxHeadingVariance,
  };
}

inline nlohmann::json BuildMissionRequest(const nlohmann::json& route,
                                          const QString& request_id,
                                          bool loop, bool return_home,
                                          bool inspection_enabled) {
  return {
      {"schema_version", 1},
      {"request_id", request_id.toStdString()},
      {"command", "start"},
      {"mission_type", "navigation"},
      {"loop", loop},
      {"return_home", return_home},
      {"on_nav_failure", "stop"},
      {"inspection",
       {{"enabled", inspection_enabled},
        {"vision_search", inspection_enabled},
        {"ai_analysis", inspection_enabled}}},
      {"route", route},
  };
}

inline nlohmann::json BuildSingleGoalMission(const basic::RobotPose& pose,
                                             const QString& request_id) {
  nlohmann::json route = nlohmann::json::array();
  route.push_back({
      {"id", "single_goal"},
      {"frame_id", "map"},
      {"x", pose.x},
      {"y", pose.y},
      {"yaw", pose.theta},
  });
  return BuildMissionRequest(route, request_id, false, false, false);
}

enum class MissionPhase {
  Idle,
  AwaitingAcceptance,
  Running,
  AwaitingCancellation,
};

class MissionTracker {
 public:
  bool Begin(const QString& request_id) {
    if (request_id.isEmpty() || phase_ != MissionPhase::Idle) {
      return false;
    }
    request_id_ = request_id;
    phase_ = MissionPhase::AwaitingAcceptance;
    return true;
  }

  bool Matches(const QString& request_id) const {
    return !request_id_.isEmpty() && request_id == request_id_;
  }

  bool Accept(const QString& request_id) {
    if (!Matches(request_id) || phase_ != MissionPhase::AwaitingAcceptance) {
      return false;
    }
    phase_ = MissionPhase::Running;
    return true;
  }

  bool BeginCancellation(const QString& request_id) {
    if (!Matches(request_id) ||
        (phase_ != MissionPhase::Running &&
         phase_ != MissionPhase::AwaitingAcceptance)) {
      return false;
    }
    phase_ = MissionPhase::AwaitingCancellation;
    return true;
  }

  bool AcceptanceTimedOut(const QString& request_id) {
    if (!Matches(request_id) || phase_ != MissionPhase::AwaitingAcceptance) {
      return false;
    }
    Clear();
    return true;
  }

  bool CancelTimedOut(const QString& request_id) {
    if (!Matches(request_id) || phase_ != MissionPhase::AwaitingCancellation) {
      return false;
    }
    phase_ = MissionPhase::Running;
    return true;
  }

  bool Finish(const QString& request_id) {
    if (!Matches(request_id)) {
      return false;
    }
    Clear();
    return true;
  }

  void Clear() {
    request_id_.clear();
    phase_ = MissionPhase::Idle;
  }

  MissionPhase phase() const { return phase_; }
  const QString& requestId() const { return request_id_; }
  bool active() const { return phase_ != MissionPhase::Idle; }

 private:
  QString request_id_;
  MissionPhase phase_{MissionPhase::Idle};
};

enum class ProfileResponse {
  Ignored,
  Accepted,
  Rejected,
};

class ProfileSwitchTracker {
 public:
  bool Begin(const QString& profile, const QString& request_id) {
    if (pending() || profile.isEmpty() || request_id.isEmpty()) {
      return false;
    }
    profile_ = profile;
    request_id_ = request_id;
    response_accepted_ = false;
    return true;
  }

  ProfileResponse HandleResponse(const QString& request_id, bool success) {
    if (!pending() || request_id != request_id_) {
      return ProfileResponse::Ignored;
    }
    if (!success) {
      Clear();
      return ProfileResponse::Rejected;
    }
    response_accepted_ = true;
    return ProfileResponse::Accepted;
  }

  bool CompleteFromStatus(const QString& state, const QString& profile) {
    if (!pending() || !response_accepted_ ||
        state != QStringLiteral("ready") || profile != profile_) {
      return false;
    }
    Clear();
    return true;
  }

  bool Timeout(const QString& request_id) {
    if (!pending() || request_id != request_id_) {
      return false;
    }
    Clear();
    return true;
  }

  void Clear() {
    profile_.clear();
    request_id_.clear();
    response_accepted_ = false;
  }

  bool pending() const { return !request_id_.isEmpty(); }
  bool responseAccepted() const { return response_accepted_; }
  const QString& profile() const { return profile_; }
  const QString& requestId() const { return request_id_; }

 private:
  QString profile_;
  QString request_id_;
  bool response_accepted_{false};
};

struct ProfileAvailability {
  bool mapping{false};
  bool navigation{false};
  bool inspection{false};
};

inline ProfileAvailability ParseProfileAvailability(
    const QJsonObject& capabilities) {
  const QJsonObject profiles =
      capabilities.value(QStringLiteral("profiles")).toObject();
  if (profiles.isEmpty()) {
    return {};
  }
  return {
      profiles.value(QStringLiteral("mapping")).toBool(false),
      profiles.value(QStringLiteral("navigation")).toBool(false),
      profiles.value(QStringLiteral("inspection")).toBool(false),
  };
}

enum class MotionOwnerState {
  Known,
  Unknown,
  Stale,
};

struct MotionOwnerStatus {
  MotionOwnerState state{MotionOwnerState::Unknown};
  QString owner;
};

inline MotionOwnerStatus ParseMotionOwner(const QJsonObject& status,
                                          double now_seconds,
                                          double stale_after_seconds) {
  // Freshness is determined from the local receive timer in the widget. ROS
  // time may be simulated or unsynchronized with the Windows wall clock.
  (void)now_seconds;
  (void)stale_after_seconds;
  const QJsonObject motion =
      status.value(QStringLiteral("motion")).toObject();
  QString owner = status.value(QStringLiteral("motion_owner")).toString();
  if (owner.isEmpty()) {
    owner = status.value(QStringLiteral("active_source")).toString();
  }
  if (owner.isEmpty()) {
    owner = status.value(QStringLiteral("control_owner")).toString();
  }
  if (owner.isEmpty()) {
    owner = status.value(QStringLiteral("motion_controller")).toString();
  }
  if (owner.isEmpty()) {
    owner = motion.value(QStringLiteral("owner")).toString();
  }
  if (owner.isEmpty()) {
    owner = motion.value(QStringLiteral("controller")).toString();
  }

  const bool explicitly_stale =
      status.value(QStringLiteral("stale")).toBool(false) ||
      motion.value(QStringLiteral("stale")).toBool(false);
  if (explicitly_stale) {
    return {MotionOwnerState::Stale, owner};
  }
  if (owner.trimmed().isEmpty()) {
    return {MotionOwnerState::Unknown, {}};
  }
  return {MotionOwnerState::Known, owner.trimmed()};
}

inline QString ReconcilePointSelection(const QString& current,
                                       const QStringList& candidates) {
  return candidates.contains(current) ? current : QString();
}

}  // namespace AppContract
