/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-04-10 15:38:40
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-14 09:56:25
 * @FilePath: ////src/display/laser_points.cpp
 * @Description:
 */
#include "display/laser_points.h"
#include <algorithm>
#include "core/framework/framework.h"
#include "msg/msg_info.h"
namespace Display {
LaserPoints::LaserPoints(const std::string &display_type, const int &z_value,
                         std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name) {
}
void LaserPoints::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *widget) {
  for (const auto& [id, data] : previous_laser_data_scene_) {
    drawLaser(painter, id, data, 0.28);
  }
  for (const auto& [id, data] : laser_data_scene_) {
    drawLaser(painter, id, data);
  }
}

LaserPoints::~LaserPoints() {}

QRectF LaserPoints::computeBoundRect(bool reset) {
  const QRectF frame_bounds = dataBounds();
  const QRectF next_bounds =
      reset || bounding_rect_.isEmpty()
          ? frame_bounds
          : (frame_bounds.isEmpty() ? bounding_rect_
                                    : bounding_rect_.united(frame_bounds));
  if (bounding_rect_ != next_bounds) {
    prepareGeometryChange();
    bounding_rect_ = next_bounds;
  }
  return frame_bounds;
}

QRectF LaserPoints::dataBounds() const {
  bool has_points = false;
  float xmax = 0.0f;
  float xmin = 0.0f;
  float ymax = 0.0f;
  float ymin = 0.0f;
  const auto include_scan = [&](const auto& laser_scan) {
    for (const auto& [id, points] : laser_scan) {
      if (points.empty())
        continue;
      if (!has_points) {
        xmax = xmin = points[0].x;
        ymax = ymin = points[0].y;
        has_points = true;
      }
      for (const auto& p : points) {
        xmax = xmax > p.x ? xmax : p.x;
        xmin = xmin < p.x ? xmin : p.x;
        ymax = ymax > p.y ? ymax : p.y;
        ymin = ymin < p.y ? ymin : p.y;
      }
    }
  };
  include_scan(previous_laser_data_scene_);
  include_scan(laser_data_scene_);
  const qreal margin = (std::max)(1.0, point_size_);
  return
      has_points
          ? QRectF(QPointF(xmin, ymin), QPointF(xmax, ymax))
                .normalized()
                .adjusted(-margin, -margin, margin, margin)
          : QRectF();
}
bool LaserPoints::SetDisplayConfig(const std::string &config_name,
                                   const std::any &config_data) {
  return true;
}

void LaserPoints::UpdateLaserData(int id, const std::vector<Point>& data,
                                  bool advance_history) {
  const QRectF old_bounds = dataBounds();
  const auto current = laser_data_scene_.find(id);
  if (advance_history && current != laser_data_scene_.end()) {
    previous_laser_data_scene_[id] = current->second;
  }
  laser_data_scene_[id] = data;
  const QRectF new_bounds = computeBoundRect();
  update(old_bounds.united(new_bounds));
}

void LaserPoints::ClearData() {
  const QRectF old_bounds = dataBounds();
  laser_data_scene_.clear();
  previous_laser_data_scene_.clear();
  computeBoundRect(true);
  update(old_bounds);
}

void LaserPoints::SetVisualStyle(qreal point_size, int opacity, const QColor& color) {
  point_size_ = std::clamp(point_size, 1.0, 8.0);
  opacity_ = std::clamp(opacity, 0, 100);
  if (color.isValid()) {
    laser_color_ = color;
    use_style_color_ = true;
  }
  computeBoundRect();
  update();
}

void LaserPoints::drawLaser(QPainter *painter, int id,
                            const std::vector<Point>& data,
                            qreal opacity_scale) {
  QColor color;
  if (!location_to_color_.count(id)) {
    int r, g, b;
    Id2Color(id, r, g, b);
    color = QColor(r, g, b);
    location_to_color_[id] = color;
  } else {
    color = location_to_color_[id];
  }
  if (use_style_color_) {
    color = laser_color_;
  }
  color.setAlphaF(std::clamp(color.alphaF() * opacity_ / 100.0 *
                                opacity_scale,
                            0.0, 1.0));
  painter->setPen(QPen(color, point_size_));
  QPolygonF poly;
  poly.reserve(static_cast<int>(data.size()));
  for (const auto& one_point : data) {
    poly << QPointF(one_point.x, one_point.y);
  }
  painter->drawPoints(poly);
}
void LaserPoints::Id2Color(int id, int &R, int &G, int &B) {
#define LocationColorJudge(JudegeId, color) \
  if (id == JudegeId) {                     \
    R = color & 0xFF0000;                   \
    R >>= 16;                               \
    G = color & 0x00FF00;                   \
    G >>= 8;                                \
    B = color & 0x0000FF;                   \
  }
  LocationColorJudge(0, 0xFF6347);
  LocationColorJudge(1, 0xff6600);
  LocationColorJudge(2, 0x228B22);
  LocationColorJudge(3, 0x800000);
  LocationColorJudge(4, 0x8A2BE2);
  LocationColorJudge(5, 0xF4A460);
  LocationColorJudge(6, 0xD2B48C);
  LocationColorJudge(7, 0xADFF2F);
  LocationColorJudge(8, 0xFF00FF);
  LocationColorJudge(9, 0x00FF00);
  LocationColorJudge(-1, 0x40E0D0);
  LocationColorJudge(-2, 0x2F4F4F);
  LocationColorJudge(-3, 0x00BFFF);
  LocationColorJudge(-4, 0x708090);
  LocationColorJudge(-5, 0x00008B);
  LocationColorJudge(-6, 0x006633);
  LocationColorJudge(-7, 0x003300);
  LocationColorJudge(-8, 0xDA70D6);
  LocationColorJudge(-9, 0x9900cc);
  LocationColorJudge(-20, 0x551A8B);
  LocationColorJudge(10, 0x00FF33);
}
}  // namespace Display
