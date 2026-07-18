/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-03-28 10:21:04
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-15 02:49:39
 * @FilePath: ////src/display/robot_map.cpp
 */
#include "display/display_occ_map.h"
#include <QtConcurrent>
#include <QEasingCurve>
#include <QMetaObject>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "core/framework/framework.h"
#include "msg/msg_info.h"
#include "display/manager/display_factory.h"
#include "widgets/ui_style.h"
namespace Display {
DisplayOccMap::DisplayOccMap(const std::string &display_type,
                             const int &z_value, std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name) {
  SetMoveEnable(true);
  discovery_timeline_ = new QTimeLine(discovery_animation_duration_ms_, this);
  discovery_timeline_->setFrameRange(0, 100);
  discovery_timeline_->setEasingCurve(QEasingCurve::OutCubic);
  connect(discovery_timeline_, &QTimeLine::frameChanged, this,
          [this](int) { update(); });
  connect(discovery_timeline_, &QTimeLine::finished, this, [this]() {
    discovery_overlay_ = QImage();
    update();
  });
  SUBSCRIBE_QOBJECT(this, MSG_ID_OCCUPANCY_MAP, [this](const OccupancyMap& data) {
    map_data_ = data;
    ParseOccupyMap();
  });
}

DisplayOccMap::~DisplayOccMap() {
  // Background conversions capture this only while the object is alive. Wait
  // before QObject teardown; queued deliveries targeting this object are then
  // either applied before destruction or discarded by Qt with the receiver.
  map_tasks_.waitForFinished();
}
bool DisplayOccMap::SetDisplayConfig(const std::string &config_name,
                                     const std::any &config_data) {
  if (config_name == "SubMapValue") {
    GetAnyData(double, config_data, sub_map_value_);
  } else if (config_name == "RobotPose") {
    // using type = Eigen::Vector3f;
    GetAnyData(Eigen::Vector3f, config_data, sub_map_center_pose_);
  } else {
    return false;
  }
  update();
  return true;
}
void DisplayOccMap::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  // Occupancy cells are categorical data rather than a photograph. Bilinear
  // filtering blends occupied/free/unknown cells and makes walls look blurry,
  // so keep nearest-neighbour sampling at fractional zoom levels.
  painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter->drawImage(0, 0, map_image_);
  if (!discovery_overlay_.isNull() && discovery_timeline_ &&
      discovery_timeline_->state() == QTimeLine::Running) {
    painter->save();
    const qreal progress =
        std::clamp(discovery_timeline_->currentFrame() / 100.0, 0.0, 1.0);
    painter->setOpacity(1.0 - progress);
    painter->drawImage(0, 0, discovery_overlay_);
    painter->restore();
  }
}
void DisplayOccMap::SetDiscoveryAnimation(bool enabled, int duration_ms) {
  discovery_animation_enabled_ = enabled;
  discovery_animation_duration_ms_ = std::clamp(duration_ms, 100, 500);
  if (discovery_timeline_) {
    discovery_timeline_->setDuration(discovery_animation_duration_ms_);
    if (!enabled) {
      discovery_timeline_->stop();
      discovery_overlay_ = QImage();
      update();
    }
  }
}

void DisplayOccMap::ParseOccupyMap() {
  // 在后台线程计算像素数据，避免阻塞 UI；
  // 完成后通过 QMetaObject::invokeMethod 回到主线程更新 map_image_ 和场景。
  OccupancyMap map_copy = map_data_;
  const OccupancyMap previous_map = rendered_map_;
  const bool previous_map_valid = rendered_map_valid_;
  const QImage previous_image = map_image_;
  const bool animate_discovery = discovery_animation_enabled_;
  const std::uint64_t generation = ++map_generation_;
  map_tasks_.addFuture(QtConcurrent::run(
      [this, map_copy, previous_map, previous_map_valid, previous_image,
       animate_discovery, generation]() mutable {
    const int cols = map_copy.Cols();
    const int rows = map_copy.Rows();
    const bool compatible_previous =
        previous_map_valid && previous_map.Cols() == cols &&
        previous_map.Rows() == rows &&
        previous_image.width() == cols && previous_image.height() == rows &&
        previous_map.map_config.origin == map_copy.map_config.origin &&
        std::abs(previous_map.map_config.resolution -
                 map_copy.map_config.resolution) < 1e-9;
    QImage local_image =
        compatible_previous
            ? previous_image.copy()
            : QImage(cols, rows, QImage::Format_ARGB32);
    if (local_image.isNull()) {
      return;
    }
    QImage discovery_overlay;
    if (animate_discovery && compatible_previous) {
      discovery_overlay = QImage(cols, rows, QImage::Format_ARGB32);
      discovery_overlay.fill(Qt::transparent);
    }

    // 复用上一帧，仅写变化像素；首次或地图尺寸变化时完整生成。
    QRgb* bits = reinterpret_cast<QRgb*>(local_image.bits());
    const int bpl = local_image.bytesPerLine() / static_cast<int>(sizeof(QRgb));
    QRgb* overlay_bits = discovery_overlay.isNull()
                             ? nullptr
                             : reinterpret_cast<QRgb*>(discovery_overlay.bits());
    const int overlay_bpl =
        discovery_overlay.isNull()
            ? 0
            : discovery_overlay.bytesPerLine() /
                  static_cast<int>(sizeof(QRgb));
    int dirty_x_min = cols;
    int dirty_y_min = rows;
    int dirty_x_max = -1;
    int dirty_y_max = -1;
    int discovered_cells = 0;
    const QColor obstacle(UiStyle::Palette::MapObstacle);
    const QColor free_space(UiStyle::Palette::MapFree);
    const QColor unknown(UiStyle::Palette::MapUnknown);
    // Eigen::matrix 坐标系与QImage坐标系不同,这里行列反着遍历
    for (int i = 0; i < cols; i++) {
      QRgb* row = bits + i;
      for (int j = 0; j < rows; j++) {
        double map_value = map_copy(j, i);
        const double previous_value =
            compatible_previous ? previous_map(j, i) : -2.0;
        if (compatible_previous && map_value == previous_value) {
          continue;
        }
        dirty_x_min = std::min(dirty_x_min, i);
        dirty_y_min = std::min(dirty_y_min, j);
        dirty_x_max = std::max(dirty_x_max, i);
        dirty_y_max = std::max(dirty_y_max, j);
        if (map_value > 0) {
          int alpha = static_cast<int>(std::clamp(map_value * 2.55, 0.0, 255.0));
          row[j * bpl] = qRgba(obstacle.red(), obstacle.green(), obstacle.blue(), alpha);
        } else if (map_value == 0) {
          row[j * bpl] = qRgba(free_space.red(), free_space.green(), free_space.blue(), 255);
        } else {
          row[j * bpl] = qRgba(unknown.red(), unknown.green(), unknown.blue(), 255);
        }
        if (overlay_bits && previous_value < 0.0 && map_value >= 0.0) {
          overlay_bits[j * overlay_bpl + i] =
              qRgba(unknown.red(), unknown.green(), unknown.blue(), 255);
          ++discovered_cells;
        }
      }
    }
    const QRect dirty_rect =
        dirty_x_max >= dirty_x_min && dirty_y_max >= dirty_y_min
            ? QRect(dirty_x_min, dirty_y_min,
                    dirty_x_max - dirty_x_min + 1,
                    dirty_y_max - dirty_y_min + 1)
            : QRect();

    // 回到主线程更新 QGraphicsItem 状态
    QMetaObject::invokeMethod(this, [this, local_image, discovery_overlay,
                                     discovered_cells, dirty_rect, map_copy,
                                     generation]() mutable {
      if (generation != map_generation_.load()) {
        return;
      }
      map_image_ = local_image;
      rendered_map_ = map_copy;
      rendered_map_valid_ = true;
      SetBoundingRect(QRectF(0, 0, map_image_.width(), map_image_.height()));
      if (discovered_cells > 0 && discovery_animation_enabled_) {
        discovery_overlay_ = discovery_overlay;
        discovery_timeline_->stop();
        discovery_timeline_->setDuration(discovery_animation_duration_ms_);
        discovery_timeline_->start();
      } else {
        discovery_overlay_ = QImage();
      }
      if (dirty_rect.isValid()) {
        update(dirty_rect);
      } else {
        update();
      }
      emit signalMapReady();
      double x, y;
      map_copy.xy2ScenePose(0, 0, x, y);
      if (!init_flag_) {
        CenterOnScene(mapToScene(x, y));
        init_flag_ = true;
      }
    }, Qt::QueuedConnection);
  }));
}
void DisplayOccMap::EraseMapRange(const QPointF &pose, double range) {
  float x = pose.x();
  float y = pose.y();
  // 确保传入的坐标在图像范围内
  if (x < 0 || x >= map_image_.width() || y < 0 || y >= map_image_.height()) {
    return;
  }
  // 计算擦除范围的矩形区域
  int left = (std::max)(0, static_cast<int>(x - range));
  int top = (std::max)(0, static_cast<int>(y - range));
  int right = (std::min)(map_image_.width() - 1, static_cast<int>(x + range));
  int bottom = (std::min)(map_image_.height() - 1, static_cast<int>(y + range));

  // 直接写内存缓冲区替代逐像素 setPixelColor()
  QRgb* erase_bits = reinterpret_cast<QRgb*>(map_image_.bits());
  const int erase_bpl = map_image_.bytesPerLine() / static_cast<int>(sizeof(QRgb));
  for (int i = left; i <= right; ++i) {
    QRgb* erase_row = erase_bits + i;
    for (int j = top; j <= bottom; ++j) {
      const QColor free_space(UiStyle::Palette::MapFree);
      erase_row[j * erase_bpl] = qRgba(free_space.red(), free_space.green(), free_space.blue(), 255);
    }
  }
  update();
}

void DisplayOccMap::DrawMapRange(const QPointF &pose, double range) {
  float x = pose.x();
  float y = pose.y();
  // 确保传入的坐标在图像范围内
  if (x < 0 || x >= map_image_.width() || y < 0 || y >= map_image_.height()) {
    return;
  }
  // 计算绘制范围的矩形区域
  int left = (std::max)(0, static_cast<int>(x - range));
  int top = (std::max)(0, static_cast<int>(y - range));
  int right = (std::min)(map_image_.width() - 1, static_cast<int>(x + range));
  int bottom = (std::min)(map_image_.height() - 1, static_cast<int>(y + range));

  // 直接写内存缓冲区替代逐像素 setPixelColor()
  QRgb* draw_bits = reinterpret_cast<QRgb*>(map_image_.bits());
  const int draw_bpl = map_image_.bytesPerLine() / static_cast<int>(sizeof(QRgb));
  for (int i = left; i <= right; ++i) {
    QRgb* draw_row = draw_bits + i;
    for (int j = top; j <= bottom; ++j) {
      draw_row[j * draw_bpl] = qRgba(0, 0, 0, 255);
    }
  }
  update();
}

OccupancyMap DisplayOccMap::GetOccupancyMap() {
  OccupancyMap map = map_data_;
  const int w = map_image_.width();
  const int h = map_image_.height();
  const int bpl = map_image_.bytesPerLine();
  // 使用 constBits() 直接访问内存缓冲区，避免逐像素函数调用开销
  const uchar* bits = map_image_.constBits();

  const QColor free_space(UiStyle::Palette::MapFree);
  for (int j = 0; j < h; j++) {
    const QRgb* row = reinterpret_cast<const QRgb*>(bits + j * bpl);
    for (int i = 0; i < w; i++) {
      const QRgb pixel_value = row[i];
      const bool opaque_black =
          qAlpha(pixel_value) == 255 && qRed(pixel_value) == 0 &&
          qGreen(pixel_value) == 0 && qBlue(pixel_value) == 0;
      const bool opaque_free =
          qAlpha(pixel_value) == 255 &&
          qRed(pixel_value) == free_space.red() &&
          qGreen(pixel_value) == free_space.green() &&
          qBlue(pixel_value) == free_space.blue();

      // map_image_ is a themed rendering, not an occupancy data source.
      // Preserve the original grid and apply only the two explicit editor
      // strokes: opaque black draws an obstacle; free-space color erases it.
      if (opaque_black) {
        map(j, i) = OCC_GRID_OCCUPIED;
      } else if (opaque_free) {
        map(j, i) = 0;
      }
    }
  }

  return map;
}

void DisplayOccMap::StartDrawLine(const QPointF &pose) {
  line_start_pose_ = pose;
}
void DisplayOccMap::EndDrawLine(const QPointF &pose, bool is_draw) {
  if (!is_draw_line_) {
    line_tmp_image_ = map_image_;
    is_draw_line_ = true;
  }
  map_image_ = line_tmp_image_;
  QPainter painter(&map_image_);
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawLine(line_start_pose_, pose);
  if (is_draw) {
    is_draw_line_ = false;
  }
  update();
}
void DisplayOccMap::DrawPoint(const QPointF &point) {
  QPainter painter(&map_image_);
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.drawPoint(point);
  update();
}

QImage DisplayOccMap::GetMapImageRegion(const QRectF &region) {
  QRect rect = region.toRect();
  rect = rect.intersected(QRect(0, 0, map_image_.width(), map_image_.height()));
  if (rect.isEmpty()) {
    return QImage();
  }
  return map_image_.copy(rect);
}

void DisplayOccMap::RestoreMapImageRegion(const QRectF &region, const QImage &image) {
  QRect rect = region.toRect();
  rect = rect.intersected(QRect(0, 0, map_image_.width(), map_image_.height()));
  if (rect.isEmpty() || image.isNull()) {
    return;
  }
  QPainter painter(&map_image_);
  painter.drawImage(rect.topLeft(), image);
  update();
}

void DisplayOccMap::SetMapImage(const QImage &image) {
  map_image_ = image;
  emit signalMapReady();
  update();
}
}  // namespace Display



