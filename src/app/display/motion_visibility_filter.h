#pragma once

#include <cmath>
#include <optional>

#include "point_type.h"

namespace Display {

class MotionVisibilityFilter {
 public:
  void SetAbsolutePose(const basic::RobotPose& pose) {
    pose_ = pose;
    has_absolute_pose_ = true;
  }

  std::optional<basic::RobotPose> UpdateOdometry(
      const basic::RobotPose& odom_pose, bool absolute_pose_fresh = true) {
    if (!has_odom_pose_) {
      odom_pose_ = odom_pose;
      has_odom_pose_ = true;
      return std::nullopt;
    }
    const double dx = odom_pose.x - odom_pose_.x;
    const double dy = odom_pose.y - odom_pose_.y;
    const double dtheta = NormalizeAngle(odom_pose.theta - odom_pose_.theta);
    const double distance = std::hypot(dx, dy);
    const basic::RobotPose previous_odom = odom_pose_;
    odom_pose_ = odom_pose;
    if (!has_absolute_pose_ || !absolute_pose_fresh ||
        !std::isfinite(distance) ||
        !std::isfinite(dtheta) || distance > 0.75 ||
        std::abs(dtheta) > 1.05) {
      return std::nullopt;
    }
    const double map_from_odom_yaw =
        NormalizeAngle(pose_.theta - previous_odom.theta);
    const double cosine = std::cos(map_from_odom_yaw);
    const double sine = std::sin(map_from_odom_yaw);
    pose_.x += cosine * dx - sine * dy;
    pose_.y += sine * dx + cosine * dy;
    pose_.theta = NormalizeAngle(pose_.theta + dtheta);
    return pose_;
  }

  const basic::RobotPose& pose() const { return pose_; }

 private:
  static double NormalizeAngle(double angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
  }

  basic::RobotPose pose_{0.0, 0.0, 0.0};
  basic::RobotPose odom_pose_{0.0, 0.0, 0.0};
  bool has_absolute_pose_{false};
  bool has_odom_pose_{false};
};

}  // namespace Display
