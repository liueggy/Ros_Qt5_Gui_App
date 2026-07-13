/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-04-10 15:38:40
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-14 09:47:41
 * @FilePath: ////src/display/laser_points.cpp
 * @Description:
 */
#include "display/robot_shape.h"
#include "algorithm.h"
#include "msg/msg_info.h"
#include "core/framework/framework.h"
#include <algorithm>
#include <cmath>
namespace Display {
RobotShape::RobotShape(const std::string &display_type, const int &z_value,
                       std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name) {
  // 使用默认颜色和透明度
  color_ = QColor(0x1E90FF);  // 默认蓝色
  opacity_ = 0.5;
  
  // 初始化空的路径
  path_ = QPainterPath();
  SetBoundingRect(QRectF(0, 0, 0, 0));
  setZValue(10);
  appearance_config_ = Config::ConfigManager::Instance()->GetRootConfigSnapshot().robot_shape_config;
  SetAppearanceConfig(appearance_config_);
  
  SUBSCRIBE_QOBJECT(this, MSG_ID_OCCUPANCY_MAP, [this](const OccupancyMap& data) {
    map_data_ = data;
    hasValidCustomShape() ? updateCustomPath() : updateFootprintPath();
    update();
  });
  
  SUBSCRIBE_QOBJECT(this, MSG_ID_ROBOT_FOOTPRINT, [this](const RobotPath& data) {
    robot_footprint_ = data;
    hasValidCustomShape() ? updateCustomPath() : updateFootprintPath();
    update();
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_ROBOT_POSE, [this](const RobotPose& data) {
    robot_pose_ = data;
    if (hasValidCustomShape()) {
      updateCustomPath();
    }
    update();
  });
}

void RobotShape::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget) {
  drawFrame(painter);
}

RobotShape::~RobotShape() {}


void RobotShape::updateFootprintPath() {
  updatePathFromWorldPoints(robot_footprint_);
}

bool RobotShape::hasValidCustomShape() const {
  if (appearance_config_.shaped_points.size() < 3) {
    return false;
  }
  for (const auto& point : appearance_config_.shaped_points) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      return false;
    }
  }
  return true;
}

void RobotShape::updateCustomPath() {
  std::vector<Config::Point> local_points = appearance_config_.shaped_points;
  if (appearance_config_.is_ellipse) {
    double min_x = appearance_config_.shaped_points.front().x;
    double max_x = min_x;
    double min_y = appearance_config_.shaped_points.front().y;
    double max_y = min_y;
    for (const auto& point : appearance_config_.shaped_points) {
      min_x = (std::min)(min_x, point.x);
      max_x = (std::max)(max_x, point.x);
      min_y = (std::min)(min_y, point.y);
      max_y = (std::max)(max_y, point.y);
    }
    local_points.clear();
    constexpr int kEllipseSegments = 32;
    const double pi = std::acos(-1.0);
    const double center_x = (min_x + max_x) / 2.0;
    const double center_y = (min_y + max_y) / 2.0;
    const double radius_x = (max_x - min_x) / 2.0;
    const double radius_y = (max_y - min_y) / 2.0;
    for (int index = 0; index < kEllipseSegments; ++index) {
      const double angle = 2.0 * pi * index / kEllipseSegments;
      local_points.push_back({center_x + radius_x * std::cos(angle),
                              center_y + radius_y * std::sin(angle)});
    }
  }
  RobotPath world_points;
  world_points.reserve(local_points.size());
  const double cos_theta = std::cos(robot_pose_.theta);
  const double sin_theta = std::sin(robot_pose_.theta);
  for (const auto& point : local_points) {
    world_points.push_back({robot_pose_.x + cos_theta * point.x - sin_theta * point.y,
                            robot_pose_.y + sin_theta * point.x + cos_theta * point.y});
  }
  updatePathFromWorldPoints(world_points);
}

void RobotShape::updatePathFromWorldPoints(const RobotPath& points) {
  path_ = QPainterPath();
  if (points.empty()) {
    SetBoundingRect(QRectF(0, 0, 0, 0));
    return;
  }
  
  // 计算边界框
  double max_x = std::numeric_limits<double>::lowest();
  double max_y = std::numeric_limits<double>::lowest();
  double min_x = (std::numeric_limits<double>::max)();
  double min_y = (std::numeric_limits<double>::max)();
  
  bool first_point = true;
  for (const auto& point : points) {
    double scene_x, scene_y;
    map_data_.xy2ScenePose(point.x, point.y, scene_x, scene_y);
    
    if (first_point) {
      path_.moveTo(scene_x, scene_y);
      first_point = false;
    } else {
      path_.lineTo(scene_x, scene_y);
    }
    
    // 更新边界框
    if (scene_x > max_x) max_x = scene_x;
    if (scene_y > max_y) max_y = scene_y;
    if (scene_x < min_x) min_x = scene_x;
    if (scene_y < min_y) min_y = scene_y;
  }
  
  path_.closeSubpath();
  
  SetBoundingRect(QRectF(min_x, min_y, max_x - min_x, max_y - min_y));
}

void RobotShape::drawFrame(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing, true);  // 设置反锯齿 反走样

  painter->save();
  painter->setPen(QPen(Qt::transparent, 1));
  painter->setOpacity(opacity_);
  painter->setBrush(color_);
  painter->drawPath(path_);
  painter->restore();
}
void RobotShape::SetAppearanceConfig(const Config::RobotShapedConfig& config) {
  appearance_config_ = config;
  const QString color_text = QString::fromStdString(config.color);
  bool ok = false;
  const uint rgba = color_text.startsWith(QStringLiteral("0x"))
                        ? color_text.mid(2).toUInt(&ok, 16)
                        : color_text.toUInt(&ok, 16);
  if (ok) {
    color_ = rgba <= 0x00FFFFFF ? QColor::fromRgb(rgba) : QColor::fromRgba(rgba);
  }
  opacity_ = std::clamp(config.opacity, 0.0f, 1.0f);
  hasValidCustomShape() ? updateCustomPath() : updateFootprintPath();
  update();
}
bool RobotShape::SetDisplayConfig(const std::string &config_name,
                                 const std::any &config_data) {
  if (config_name == "Color") {
    Color color;
    GetAnyData(Color, config_data, color);
    color_ = QColor(color[0], color[1], color[2]);
  } else if (config_name == "Opacity") {
    GetAnyData(float, config_data, opacity_);
  } else {
    return false;
  }
  update();
  return true;
}
}  // namespace Display
