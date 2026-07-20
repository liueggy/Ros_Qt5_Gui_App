#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QWidget>

#include "msg/gps_info.h"

class QLabel;
class QPushButton;
class QTimer;

class GlobeWidget final : public QWidget {
 public:
  explicit GlobeWidget(QWidget* parent = nullptr);

  void SetPosition(double latitude, double longitude, bool valid);
  void FocusPosition();
  void SetCloudsVisible(bool visible);
  void ZoomIn();
  void ZoomOut();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void RenderGlobe();
  QPointF Project(double latitude, double longitude, bool* visible) const;

  void ChangeZoom(double delta);

  QImage surface_texture_;
  QImage cloud_texture_;
  QImage globe_;
  QPoint drag_origin_;
  double drag_latitude_{0.0};
  double drag_longitude_{0.0};
  double view_latitude_{22.0};
  double view_longitude_{105.0};
  double gps_latitude_{0.0};
  double gps_longitude_{0.0};
  double zoom_{0.92};
  bool dragging_{false};
  bool gps_valid_{false};
  bool clouds_visible_{false};
  bool dirty_{true};
};

class GpsLocationWidget final : public QWidget {
 public:
  explicit GpsLocationWidget(QWidget* parent = nullptr);

  void SetFix(const basic::GpsFix& fix);
  void SetStatus(const basic::GpsStatus& status);
  void SetTransportConnected(bool connected);

 private:
  void Refresh();
  QLabel* MakeValueLabel(const QString& initial);

  GlobeWidget* globe_{nullptr};
  QLabel* state_badge_{nullptr};
  QLabel* latitude_value_{nullptr};
  QLabel* longitude_value_{nullptr};
  QLabel* altitude_value_{nullptr};
  QLabel* satellites_value_{nullptr};
  QLabel* hdop_value_{nullptr};
  QLabel* quality_value_{nullptr};
  QLabel* freshness_value_{nullptr};
  QLabel* source_value_{nullptr};
  QPushButton* focus_button_{nullptr};
  QTimer* freshness_timer_{nullptr};
  QElapsedTimer last_fix_received_;
  basic::GpsFix fix_;
  basic::GpsStatus status_;
  bool have_fix_{false};
  bool have_status_{false};
  bool transport_connected_{false};
};
