/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-04-11 10:13:22
 * @LastEditors: chengyang chengyangkj@outlook.com
 * @LastEditTime: 2023-04-20 16:46:39
 * @FilePath:
 * ////include/display/display_demo.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once
#include <Eigen/Dense>
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneWheelEvent>
#include <QElapsedTimer>

#include "virtual_display.h"
using namespace basic;
namespace Display {
class DisplayPath : public VirtualDisplay {
 private:
  QColor color_;
  int line_width_ = {1};
  QPolygonF path_points_;
  OccupancyMap map_data_;
  QElapsedTimer last_update_timer_;
  bool data_received_{false};
  bool hidden_by_stale_{false};
  bool visible_before_stale_{true};

 private:
  void drawPath(QPainter *painter);

  void computeBoundRect(const RobotPath &path);
  void updatePathPoints(const RobotPath& path);

 public:
  DisplayPath(const std::string &display_type, const int &z_value,
              std::string parent_name = "");
  ~DisplayPath();
  bool SetDisplayConfig(const std::string &config_name,
                        const std::any &config_data) override;
  qint64 DataAgeMs() const;
  void SetDataStale(bool stale);
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget = nullptr) override;
};
}  // namespace Display
