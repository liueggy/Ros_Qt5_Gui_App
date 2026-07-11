/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-03-28 10:21:04
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-15 02:49:39
 * @FilePath: ////src/display/robot_map.cpp
 */
#include "display/display_occ_map.h"
#include <QtConcurrent>
#include <QMetaObject>
#include <algorithm>
#include <iostream>
#include "core/framework/framework.h"
#include "msg/msg_info.h"
#include "display/manager/display_factory.h"
namespace Display {
DisplayOccMap::DisplayOccMap(const std::string &display_type,
                             const int &z_value, std::string parent_name)
    : VirtualDisplay(display_type, z_value, parent_name) {
  SetMoveEnable(true);
  SUBSCRIBE(MSG_ID_OCCUPANCY_MAP, [this](const OccupancyMap& data) {
    map_data_ = data;
    ParseOccupyMap();
    LOG_INFO("map update calling:" << map_image_.width() << " "
            << map_image_.height() << std::endl);
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
  painter->drawImage(0, 0, map_image_);
}
void DisplayOccMap::ParseOccupyMap() {
  // 在后台线程计算像素数据，避免阻塞 UI；
  // 完成后通过 QMetaObject::invokeMethod 回到主线程更新 map_image_ 和场景。
  OccupancyMap map_copy = map_data_;
  const std::uint64_t generation = ++map_generation_;
  map_tasks_.addFuture(QtConcurrent::run([this, map_copy, generation]() mutable {
    const int cols = map_copy.Cols();
    const int rows = map_copy.Rows();
    QImage local_image(cols, rows, QImage::Format_ARGB32);
    if (local_image.isNull()) {
      return;
    }

    // 直接写入 QImage 内存缓冲区，避免逐像素 setPixel() 的函数调用和边界检查开销
    QRgb* bits = reinterpret_cast<QRgb*>(local_image.bits());
    const int bpl = local_image.bytesPerLine() / static_cast<int>(sizeof(QRgb));
    // Eigen::matrix 坐标系与QImage坐标系不同,这里行列反着遍历
    for (int i = 0; i < cols; i++) {
      QRgb* row = bits + i;
      for (int j = 0; j < rows; j++) {
        double map_value = map_copy(j, i);
        if (map_value > 0) {
          int alpha = static_cast<int>(std::clamp(map_value * 2.55, 0.0, 255.0));
          row[j * bpl] = qRgba(0, 0, 0, alpha);
        } else if (map_value == 0) {
          row[j * bpl] = qRgba(255, 255, 255, 255);
        } else {
          row[j * bpl] = qRgba(128, 128, 128, 255);
        }
      }
    }

    // 回到主线程更新 QGraphicsItem 状态
    QMetaObject::invokeMethod(this, [this, local_image, map_copy, generation]() mutable {
      if (generation != map_generation_.load()) {
        return;
      }
      map_image_ = local_image;
      SetBoundingRect(QRectF(0, 0, map_image_.width(), map_image_.height()));
      update();
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
      erase_row[j * erase_bpl] = qRgba(255, 255, 255, 255);
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

  for (int j = 0; j < h; j++) {
    const QRgb* row = reinterpret_cast<const QRgb*>(bits + j * bpl);
    for (int i = 0; i < w; i++) {
      QRgb pixelValue = row[i];
      int alpha = qAlpha(pixelValue);

      if (qRed(pixelValue) == 0 && qGreen(pixelValue) == 0 && qBlue(pixelValue) == 0 && alpha > 0) {
        map(j, i) = static_cast<int>(alpha / 2.55);
      } else if (alpha == 255) {
        map(j, i) = 0;
      } else {
        map(j, i) = -1;
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



