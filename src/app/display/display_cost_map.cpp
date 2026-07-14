
/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-03-28 10:21:04
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-15 02:49:39
 * @FilePath: ////src/display/robot_map.cpp
 */
#include <algorithm>
#include <iostream>

#include "display/display_cost_map.h"
#include "core/framework/framework.h"
#include "msg/msg_info.h"
namespace Display {
DisplayCostMap::DisplayCostMap(const std::string &display_type,
                               const int &z_value, std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name) {
  if (display_type == DISPLAY_GLOBAL_COST_MAP) {
    SUBSCRIBE_QOBJECT(this, MSG_ID_GLOBAL_COST_MAP, [this](const OccupancyMap& data) {
      cost_map_data_ = data;
      ParseCostMap();
      data_received_ = true;
      last_update_timer_.restart();
      SetDataStale(false);
      SetBoundingRect(QRectF(0, 0, map_image_.width(), map_image_.height()));
      update();
    });
  } else if (display_type == DISPLAY_LOCAL_COST_MAP) {
    SUBSCRIBE_QOBJECT(this, MSG_ID_LOCAL_COST_MAP, [this](const OccupancyMap& data) {
      cost_map_data_ = data;
      ParseCostMap();
      data_received_ = true;
      last_update_timer_.restart();
      SetDataStale(false);
      SetBoundingRect(QRectF(0, 0, map_image_.width(), map_image_.height()));
      update();
    });
  }
}
qint64 DisplayCostMap::DataAgeMs() const {
  return data_received_ && last_update_timer_.isValid()
             ? last_update_timer_.elapsed()
             : -1;
}

void DisplayCostMap::SetDataStale(bool stale) {
  if (stale) {
    if (!hidden_by_stale_) {
      visible_before_stale_ = isVisible();
      hidden_by_stale_ = true;
      setVisible(false);
    }
  } else if (hidden_by_stale_) {
    hidden_by_stale_ = false;
    setVisible(visible_before_stale_);
  }
}
bool DisplayCostMap::SetDisplayConfig(const std::string &config_name,
                                      const std::any &config_data) {
  update();
  return true;
}
void DisplayCostMap::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) {
  //以图片中心做原点进行绘制(方便旋转)
  painter->drawImage(0, 0, map_image_);
}
void DisplayCostMap::ParseCostMap() {
  const bool size_changed = map_image_.width() != cost_map_data_.Cols() ||
                            map_image_.height() != cost_map_data_.Rows();
  if (size_changed) {
    map_image_ = QImage(cost_map_data_.Cols(), cost_map_data_.Rows(),
                        QImage::Format_ARGB32);
    map_image_.fill(Qt::transparent);
  }

  QRect dirty_rect;
  if (!size_changed && cost_map_data_.dirty_region.valid) {
    const auto& current = cost_map_data_.dirty_region;
    dirty_rect = QRect(current.col_min, current.row_min,
                       current.col_max - current.col_min,
                       current.row_max - current.row_min)
                     .united(last_dirty_rect_);
  } else {
    dirty_rect = QRect(0, 0, cost_map_data_.Cols(), cost_map_data_.Rows());
  }
  dirty_rect = dirty_rect.intersected(map_image_.rect());
  for (int row = dirty_rect.top(); row <= dirty_rect.bottom(); ++row) {
    for (int col = dirty_rect.left(); col <= dirty_rect.right(); ++col) {
      map_image_.setPixelColor(col, row, CostColor(cost_map_data_(row, col)));
    }
  }
  last_dirty_rect_ = cost_map_data_.dirty_region.valid
                         ? QRect(cost_map_data_.dirty_region.col_min,
                                 cost_map_data_.dirty_region.row_min,
                                 cost_map_data_.dirty_region.col_max -
                                     cost_map_data_.dirty_region.col_min,
                                 cost_map_data_.dirty_region.row_max -
                                     cost_map_data_.dirty_region.row_min)
                         : dirty_rect;
}

QColor DisplayCostMap::CostColor(int data) const {
  if (data >= 90) return QColor(0xe7, 0x6f, 0x51, 150);
  if (data >= 40) return QColor(0xe9, 0xc4, 0x6a, 125);
  if (data >= 1) return QColor(0xe9, 0xc4, 0x6a, 85);
  return QColor(0, 0, 0, 0);
}

} // namespace Display
