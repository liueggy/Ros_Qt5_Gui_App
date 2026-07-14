/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-03-28 10:20:56
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-14 09:46:16
 * @FilePath: ////include/display/DisplayCostMap.h
 */
#pragma once
#include <Eigen/Dense>
#include <QElapsedTimer>
#include <QColor>
#include <QRect>
#include "occupancy_map.h"
#include "virtual_display.h"
namespace Display {
class DisplayCostMap : public VirtualDisplay {
 private:
  /* data */
 public:
  DisplayCostMap(const std::string &display_type, const int &z_value,
                 std::string parent_name = "");
  ~DisplayCostMap() = default;
  bool SetDisplayConfig(const std::string &config_name,
                        const std::any &config_data);
  qint64 DataAgeMs() const;
  void SetDataStale(bool stale);

 private:
  OccupancyMap cost_map_data_;

  QTransform transform_;
  QImage map_image_;
  QElapsedTimer last_update_timer_;
  QRect last_dirty_rect_;
  bool data_received_{false};
  bool hidden_by_stale_{false};
  bool visible_before_stale_{true};

 private:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = nullptr) override;
  void ParseCostMap();
  QColor CostColor(int value) const;
};
}  // namespace Display
