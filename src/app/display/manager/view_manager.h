#pragma once
#include <QApplication>
#include <QGraphicsView>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QWheelEvent>
#include "display/manager/scene_manager.h"
namespace Display {
class DisplayManager;

class ViewManager : public QGraphicsView {
  Q_OBJECT
 private:
  QToolButton *focus_robot_btn_;
  QToolButton *add_robot_pos_btn_;
  DisplayManager *display_manager_ptr_;
  QLineEdit *label_pos_map_;
  QLineEdit *label_pos_scene_;
  QLineEdit *label_pos_robot_;
  QSlider *tool_size_slider_;
  QLabel *tool_size_value_label_;
  QWidget *map_empty_state_;
  qreal map_view_rotation_deg_ = {-90.0};
  bool map_auto_fit_done_ = {false};
  bool user_map_view_adjusted_ = {false};
  bool grid_visible_ = {true};
  int grid_spacing_ = {32};
  int grid_opacity_ = {100};
  QColor grid_color_ = {96, 125, 117};
  void ApplyMapViewScale(qreal factor, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

 public:
  ViewManager(QWidget *parent = nullptr);
  void SetDisplayManagerPtr(DisplayManager *display_manager);
  QToolButton* GetAddRobotPosButton() { return add_robot_pos_btn_; }
  void ShowAddRobotPosButton(bool show);
  void UpdateMapPos(const QString &text);
  void UpdateScenePos(const QString &text);
  void UpdateRobotPos(const QString &text);
  void UpdateToolSizeSlider(double range);
  void ShowToolSizeSlider(bool show);
  void FitMapToBestView();
  void ZoomMapView(qreal factor);
  void RotateMapView(qreal delta_degrees);
  void SetGridStyle(bool visible, int spacing, int opacity, const QColor& color);

 private slots:
  void OnEditMapModeChanged(MapEditMode mode);

 protected:
  void resizeEvent(QResizeEvent *event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

  void enterEvent(QEvent *event) override;

  void leaveEvent(QEvent *event) override;
};
}  // namespace Display
