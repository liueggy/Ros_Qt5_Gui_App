/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-07-26 10:07:32
 * @LastEditors: chengyang chengyangkj@outlook.com
 * @LastEditTime: 2023-07-26 16:30:38
 * @FilePath: /ros_qt5_gui_app/include/ point_type.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef POINT_TYPE_H
#define POINT_TYPE_H
#include <Eigen/Dense>
#include <chrono>
#include <iomanip>
#include "point.h"
namespace basic {
typedef OrientedPoint RobotPose;
struct LocalizationEstimate {
  RobotPose pose;
  double xy_variance = {0};
  double yaw_variance = {0};
  std::chrono::steady_clock::time_point received_at;
};
typedef std::vector<Point> RobotPath;
typedef Eigen::Vector3d Color;
struct RobotSpeed {
  double vx = {0};
  double vy = {0};
  double w = {0};
  RobotSpeed() {}
  RobotSpeed(double vx, double vy, double w) : vx(vx), vy(vy), w(w) {}
};

inline std::ostream &operator<<(std::ostream &os, const RobotSpeed &p) {
  const auto old_flags = os.flags();
  const auto old_precision = os.precision();
  os << std::fixed << std::setprecision(2)
     << "x:" << p.vx << " y:" << p.vy << " theta:" << p.w;
  os.flags(old_flags);
  os.precision(old_precision);
  return os;
}
struct RobotState : public RobotPose, RobotSpeed {
 public:
  RobotState() = default;
  RobotState(const RobotPose &p, const RobotSpeed &s) : RobotPose(p), RobotSpeed(s) {}
  RobotState(const OrientedPoint &p) : RobotPose(p) {}
};
// 激光点云数据结构
struct LaserScan {
  LaserScan() = default;
  LaserScan(int i, std::vector<Point> d) : id(i), data(d) {}
  int id = {0};             // 激光ID
  std::vector<Point> data;  // 正常显示点；points_in_map=true 时为 map 坐标
  std::vector<Point> robot_relative_data;  // 车体坐标，用于重定位预览
  bool points_in_map{false};  // true: data 已经由 TF 转换到 map 坐标系
  void push_back(Point p) { data.push_back(p); }
  void clear() {
    data.clear();
    robot_relative_data.clear();
  }
  const std::vector<Point>& RelocationPreviewData() const {
    return robot_relative_data.empty() ? data : robot_relative_data;
  }
};
// 机器人路径 由点集组成

inline Eigen::Vector3d absoluteSum(const Eigen::Vector3d &p,
                                   const Eigen::Vector3d &d) {
  Eigen::Quaterniond q1 = Eigen::AngleAxisd(p[2], Eigen::Vector3d::UnitZ()) *
                          Eigen::AngleAxisd(0.0, Eigen::Vector3d::UnitY()) *
                          Eigen::AngleAxisd(0.0, Eigen::Vector3d::UnitX());
  Eigen::Vector3d t1 = Eigen::Vector3d(p[0], p[1], 0.0);
  Eigen::Vector3d p1 = Eigen::Vector3d(d[0], d[1], 0.0);
  Eigen::Vector3d pw;
  pw = q1 * p1 + t1;
  pw[2] += d[2];
  return pw;
}

}  // namespace basic

#endif
