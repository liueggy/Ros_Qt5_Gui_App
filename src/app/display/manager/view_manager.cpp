#include "display/manager/view_manager.h"
#include <QDebug>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "display/display_occ_map.h"
#include "display/manager/display_factory.h"
#include "display/manager/display_manager.h"
#include "display/manager/scene_manager.h"
#include "widgets/ui_style.h"
namespace Display {
ViewManager::ViewManager(QWidget* parent) : QGraphicsView(parent) {
  setBackgroundBrush(QColor(UiStyle::Palette::Surface));
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setMouseTracking(true);  // 开启鼠标追踪，以便捕获鼠标移动事件
  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  QHBoxLayout* center_layout = new QHBoxLayout;
  QVBoxLayout* left_bar_layout = new QVBoxLayout;
  left_bar_layout->setContentsMargins(5, 5, 5, 5);
  left_bar_layout->setAlignment(Qt::AlignTop);

  // 左上角工具大小滑动条
  tool_size_slider_ = new QSlider(Qt::Horizontal);
  tool_size_slider_->setMinimum(1);    // 0.1米
  tool_size_slider_->setMaximum(500);  // 50.0米
  tool_size_slider_->setValue(1);      // 默认0.1米
  tool_size_slider_->setMaximumWidth(150);
  tool_size_slider_->setCursor(Qt::ArrowCursor);
  tool_size_slider_->setStyleSheet(QString(R"(
    QSlider {
      background: transparent;
    }
    QSlider::groove:horizontal {
      background: %1;
      height: 4px;
      border-radius: 2px;
    }
    QSlider::handle:horizontal {
      background: %2;
      border: 2px solid %3;
      width: 12px;
      height: 12px;
      border-radius: 6px;
      margin: -4px 0;
    }
    QSlider::handle:horizontal:hover {
      background: %4;
    }
  )").arg(UiStyle::Palette::Scrollbar, UiStyle::Palette::Primary, UiStyle::Palette::Surface, UiStyle::Palette::PrimaryHover));
  tool_size_slider_->hide();  // 默认隐藏

  left_bar_layout->addWidget(tool_size_slider_);

  tool_size_value_label_ = new QLabel("0.1");
  tool_size_value_label_->setStyleSheet(QStringLiteral("QLabel { color:%1; font-size:%2px; font-weight:600; min-width:30px; }").arg(UiStyle::Palette::Primary, UiStyle::FontMiniPx()));
  tool_size_value_label_->setAlignment(Qt::AlignCenter);
  tool_size_value_label_->hide();  // 默认隐藏
  left_bar_layout->addWidget(tool_size_value_label_);

  left_bar_layout->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

  center_layout->addLayout(left_bar_layout);

  map_empty_state_ = new QWidget(viewport());
  auto* empty_layout = new QVBoxLayout(map_empty_state_);
  empty_layout->setContentsMargins(30, 26, 30, 28);
  empty_layout->setSpacing(10);
  auto* empty_icon = new QLabel(map_empty_state_);
  empty_icon->setPixmap(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/map.svg"), QSize(34, 34)).pixmap(34, 34));
  empty_icon->setAlignment(Qt::AlignCenter);
  empty_icon->setFixedSize(68, 58);
  empty_icon->setStyleSheet(QStringLiteral(
      "QLabel { background:%1; border:1px solid %2; border-radius:20px; "
      "padding:0; margin-bottom:2px; }")
      .arg(UiStyle::Palette::PrimaryLight, UiStyle::Palette::BorderHover));
  auto* empty_title = new QLabel(tr("等待地图"), map_empty_state_);
  empty_title->setAlignment(Qt::AlignCenter);
  empty_title->setStyleSheet(QStringLiteral(
      "QLabel { color:%1; font-size:%2px; font-weight:800; "
      "background:transparent; border:none; padding-top:4px; }").arg(UiStyle::Palette::Text, UiStyle::FontTitlePx()));
  empty_layout->addWidget(empty_icon, 0, Qt::AlignHCenter);
  empty_layout->addWidget(empty_title);
  auto* empty_hint = new QLabel(
      tr("连接机器人后将自动加载实时地图\n也可以使用顶部“打开地图”载入本地文件"),
      map_empty_state_);
  empty_hint->setAlignment(Qt::AlignCenter);
  empty_hint->setWordWrap(true);
  empty_hint->setStyleSheet(UiStyle::HintLabelStyleSheet());
  empty_layout->addWidget(empty_hint);
  map_empty_state_->setMaximumWidth(340);
  map_empty_state_->setStyleSheet(QStringLiteral(
      "QWidget { background:%1; border:1px solid %2; border-radius:5px; } "
      "QLabel { background:transparent; border:none; }")
      .arg(UiStyle::Palette::Surface, UiStyle::Palette::Border));
  map_empty_state_->adjustSize();
  map_empty_state_->raise();
  center_layout->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
  main_layout->addLayout(center_layout);

  // 添加垂直 spacer，将底部工具栏推到底部
  main_layout->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

  // 创建一个水平布局，放在底部，包含左侧坐标显示和右侧工具按钮
  QHBoxLayout* bottom_layout = new QHBoxLayout;
  bottom_layout->setContentsMargins(5, 0, 5, 5);
  bottom_layout->setSpacing(5);

  // 左侧坐标显示
  label_pos_map_ = new QLineEdit();
  label_pos_map_->setReadOnly(true);
  label_pos_map_->setObjectName(QString::fromUtf8("label_pos_map_"));
  label_pos_map_->setMinimumWidth(160);
  label_pos_map_->setMaximumWidth(220);
  label_pos_map_->setFixedHeight(20);
  label_pos_map_->setPlaceholderText("地图: (x, y)");
  label_pos_map_->setStyleSheet(UiStyle::TransparentStatusInputStyleSheet());
  label_pos_map_->setText("地图: (0.00, 0.00)");
  bottom_layout->addWidget(label_pos_map_);

  label_pos_scene_ = new QLineEdit();
  label_pos_scene_->setReadOnly(true);
  label_pos_scene_->setObjectName(QString::fromUtf8("label_pos_scene_"));
  label_pos_scene_->setMinimumWidth(160);
  label_pos_scene_->setMaximumWidth(220);
  label_pos_scene_->setFixedHeight(20);
  label_pos_scene_->setPlaceholderText("场景: (x, y)");
  label_pos_scene_->setStyleSheet(UiStyle::TransparentStatusInputStyleSheet());
  label_pos_scene_->setText("场景: (0.00, 0.00)");
  bottom_layout->addWidget(label_pos_scene_);

  label_pos_robot_ = new QLineEdit();
  label_pos_robot_->setReadOnly(true);
  label_pos_robot_->setObjectName(QString::fromUtf8("label_pos_robot_"));
  label_pos_robot_->setMinimumWidth(180);
  label_pos_robot_->setMaximumWidth(240);
  label_pos_robot_->setFixedHeight(20);
  label_pos_robot_->setPlaceholderText("机器人: (x, y, θ)");
  label_pos_robot_->setStyleSheet(UiStyle::TransparentStatusInputStyleSheet());
  label_pos_robot_->setText("机器人: (0.00, 0.00, 0.00)");
  bottom_layout->addWidget(label_pos_robot_);

  // 中间spacer，将右侧按钮推到右边
  bottom_layout->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  // 创建工具按钮并添加到布局中
  // 添加机器人位置按钮（在放大缩小按钮左侧，初始隐藏）
  add_robot_pos_btn_ = new QToolButton();
  add_robot_pos_btn_->setIcon(
      UiStyle::TintedIcon(QStringLiteral(":/images/crosshair.svg"), QSize(25, 25)));
  add_robot_pos_btn_->setIconSize(QSize(25, 25));
  add_robot_pos_btn_->setToolTip("添加机器人当前位置为目标点");
  add_robot_pos_btn_->setCursor(Qt::PointingHandCursor);
  add_robot_pos_btn_->setStyleSheet(
      "QToolButton {"
      "   border: none;"
      "   background-color: transparent;"
      "}"
      "QToolButton:hover {"
      "   background-color: rgba(0, 0, 0, 0.1);"
      "   border-radius: 4px;"
      "}");
  add_robot_pos_btn_->hide();  // 初始隐藏
  bottom_layout->addWidget(add_robot_pos_btn_);

  QToolButton* set_big_btn_ = new QToolButton();
  set_big_btn_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/zoom-in.svg"), QSize(22, 22)));
  set_big_btn_->setToolTip("放大地图视图");
  set_big_btn_->setCursor(Qt::PointingHandCursor);
  bottom_layout->addWidget(set_big_btn_);
  QToolButton* set_small_btn_ = new QToolButton();
  set_small_btn_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/zoom-out.svg"), QSize(22, 22)));
  set_small_btn_->setToolTip("缩小地图视图");
  set_small_btn_->setCursor(Qt::PointingHandCursor);
  bottom_layout->addWidget(set_small_btn_);
  QToolButton* fit_map_btn = new QToolButton();
  fit_map_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/focus-centered.svg"), QSize(22, 22)));
  fit_map_btn->setToolTip("适配并居中地图");
  fit_map_btn->setCursor(Qt::PointingHandCursor);
  bottom_layout->addWidget(fit_map_btn);
  QToolButton* rotate_view_btn = new QToolButton();
  rotate_view_btn->setIcon(
      UiStyle::TintedIcon(QStringLiteral(":/images/rotate.svg"), QSize(22, 22)));
  rotate_view_btn->setToolTip("地图视图顺时针旋转90°");
  rotate_view_btn->setCursor(Qt::PointingHandCursor);
  bottom_layout->addWidget(rotate_view_btn);
  focus_robot_btn_ = new QToolButton();
  focus_robot_btn_->setIcon(
      UiStyle::TintedIcon(QStringLiteral(":/images/unfocus.svg"), QSize(22, 22)));
  focus_robot_btn_->setToolTip("聚焦机器人");
  focus_robot_btn_->setCursor(Qt::PointingHandCursor);
  focus_robot_btn_->setStyleSheet(
      "QToolButton {"
      "   border: none;"
      "   background-color: transparent;"
      "}");
  focus_robot_btn_->setIconSize(QSize(25, 25));
  bottom_layout->addWidget(focus_robot_btn_);

  for (auto* button : {add_robot_pos_btn_, set_big_btn_, set_small_btn_, fit_map_btn, rotate_view_btn, focus_robot_btn_}) {
    button->setFixedSize(36, 36);
    button->setIconSize(QSize(22, 22));
    button->setStyleSheet(UiStyle::GhostIconButtonStyleSheet());
  }

  main_layout->addLayout(bottom_layout);

  setViewportMargins(0, 5, 0, 0);

  // 左侧工具
  QHBoxLayout* display_config_layout = new QHBoxLayout;

  // 图层列表面板
  QHBoxLayout* display_btn_list_layout = new QHBoxLayout;
  QToolButton* display_laser_btn_ = new QToolButton();
  display_laser_btn_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/classes/LaserScan.png"), QSize(25, 25)));
  display_laser_btn_->setIconSize(QSize(25, 25));
  display_laser_btn_->setToolTip("放大");
  display_laser_btn_->setStyleSheet(
      "QToolButton {"
      "   border: none;"
      "   background-color: transparent;"
      "}");
  display_laser_btn_->setFixedSize(36, 36);
  display_laser_btn_->setStyleSheet(UiStyle::GhostIconButtonStyleSheet());
  display_btn_list_layout->addWidget(display_laser_btn_);

  // 将布局添加到视口的小部件上
  viewport()->setLayout(main_layout);

  // connect

  connect(focus_robot_btn_, &QToolButton::clicked, [this]() {
    if (focus_robot_btn_->toolTip() == "聚焦机器人") {
      FactoryDisplay::Instance()->SetFocusDisplay(DISPLAY_ROBOT);
      focus_robot_btn_->setToolTip("取消聚焦机器人");
      focus_robot_btn_->setIcon(
          UiStyle::TintedIcon(QStringLiteral(":/images/focus.svg"), QSize(22, 22)));
    } else {
      FactoryDisplay::Instance()->SetFocusDisplay("");
      focus_robot_btn_->setToolTip("聚焦机器人");
      focus_robot_btn_->setIcon(
          UiStyle::TintedIcon(QStringLiteral(":/images/unfocus.svg"), QSize(22, 22)));
    }
  });
  connect(set_big_btn_, &QToolButton::clicked, [this]() { ZoomMapView(1.20); });
  connect(set_small_btn_, &QToolButton::clicked, [this]() { ZoomMapView(1.0 / 1.20); });
  connect(fit_map_btn, &QToolButton::clicked, [this]() {
    user_map_view_adjusted_ = false;
    FitMapToBestView();
  });
  connect(rotate_view_btn, &QToolButton::clicked, [this]() { RotateMapView(90.0); });

  // 连接工具大小滑动条信号
  connect(tool_size_slider_, &QSlider::valueChanged, [this](int value) {
    double range = value / 10.0;  // 转换为米（1-500 对应 0.1-50.0米）
    if (display_manager_ptr_) {
      display_manager_ptr_->SetToolRange(range);
    }
    tool_size_value_label_->setText(QString::number(range, 'f', 1));
  });
}

void ViewManager::drawBackground(QPainter* painter, const QRectF& rect) {
  QGraphicsView::drawBackground(painter, rect);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->fillRect(rect, QColor(UiStyle::Palette::ToolbarBg));

  if (!grid_visible_) {
    painter->restore();
    return;
  }

  const qreal minor_step = grid_spacing_;
  const qreal major_step = minor_step * 4.0;
  const qreal left = std::floor(rect.left() / minor_step) * minor_step;
  const qreal top = std::floor(rect.top() / minor_step) * minor_step;

  QColor minor_color = UiStyle::IsDarkTheme() ? QColor(116, 146, 138, 34)
                                               : QColor(96, 125, 117, 22);
  minor_color.setAlpha(minor_color.alpha() * grid_opacity_ / 100);
  QPen minor_pen(minor_color);
  minor_pen.setWidthF(0.0);
  painter->setPen(minor_pen);
  for (qreal x = left; x < rect.right(); x += minor_step) {
    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
  }
  for (qreal y = top; y < rect.bottom(); y += minor_step) {
    painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
  }

  QColor major_color = UiStyle::IsDarkTheme() ? QColor(116, 158, 147, 58)
                                               : QColor(75, 115, 105, 38);
  major_color.setAlpha(major_color.alpha() * grid_opacity_ / 100);
  QPen major_pen(major_color);
  major_pen.setWidthF(0.0);
  painter->setPen(major_pen);
  const qreal major_left = std::floor(rect.left() / major_step) * major_step;
  const qreal major_top = std::floor(rect.top() / major_step) * major_step;
  for (qreal x = major_left; x < rect.right(); x += major_step) {
    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
  }
  for (qreal y = major_top; y < rect.bottom(); y += major_step) {
    painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
  }

  painter->restore();
}

void ViewManager::SetGridStyle(bool visible, int spacing, int opacity) {
  grid_visible_ = visible;
  grid_spacing_ = std::clamp(spacing, 16, 96);
  grid_opacity_ = std::clamp(opacity, 0, 100);
  viewport()->update();
}

void ViewManager::SetDisplayManagerPtr(DisplayManager* display_manager) {
  display_manager_ptr_ = display_manager;
  // 初始化滑动条值（默认0.1米）
  if (tool_size_slider_ && display_manager_ptr_) {
    tool_size_slider_->setValue(1);  // 0.1米
    display_manager_ptr_->SetToolRange(0.1);
  }

  // 连接编辑模式变化信号
  if (display_manager_ptr_) {
    auto* map = static_cast<DisplayOccMap*>(
        FactoryDisplay::Instance()->GetDisplay(DISPLAY_MAP));
    if (map) {
      map_empty_state_->setVisible(map->GetMapImage().isNull());
      connect(map, &DisplayOccMap::signalMapReady, this, [this]() {
        map_empty_state_->hide();
        if (!map_auto_fit_done_ && !user_map_view_adjusted_) {
          map_auto_fit_done_ = true;
          QTimer::singleShot(0, this, &ViewManager::FitMapToBestView);
        }
      });
    }
    connect(display_manager_ptr_, &DisplayManager::signalEditMapModeChanged,
            this, &ViewManager::OnEditMapModeChanged);
  }
}


void ViewManager::FitMapToBestView() {
  auto* map = FactoryDisplay::Instance()->GetDisplay(DISPLAY_MAP);
  if (!map || map->boundingRect().isEmpty()) {
    return;
  }
  resetTransform();
  const QRectF bounds = map->sceneBoundingRect().adjusted(-80, -80, 80, 80);
  if (!bounds.isEmpty()) {
    fitInView(bounds, Qt::KeepAspectRatio);
    rotate(map_view_rotation_deg_);
    centerOn(bounds.center());
  }
}

void ViewManager::ApplyMapViewScale(qreal factor, QGraphicsView::ViewportAnchor anchor) {
  if (factor <= 0.0) {
    return;
  }
  const qreal current_scale = std::hypot(transform().m11(), transform().m21());
  const qreal target_scale = current_scale * factor;
  if (target_scale < 0.03 || target_scale > 30.0) {
    return;
  }
  user_map_view_adjusted_ = true;
  setTransformationAnchor(anchor);
  scale(factor, factor);
  setTransformationAnchor(QGraphicsView::AnchorViewCenter);
}

void ViewManager::ZoomMapView(qreal factor) {
  ApplyMapViewScale(factor);
}

void ViewManager::RotateMapView(qreal delta_degrees) {
  map_view_rotation_deg_ += delta_degrees;
  while (map_view_rotation_deg_ >= 360.0) {
    map_view_rotation_deg_ -= 360.0;
  }
  while (map_view_rotation_deg_ <= -360.0) {
    map_view_rotation_deg_ += 360.0;
  }
  user_map_view_adjusted_ = true;
  const QPointF center = mapToScene(viewport()->rect().center());
  rotate(delta_degrees);
  centerOn(center);
}

void ViewManager::ShowAddRobotPosButton(bool show) {
  if (add_robot_pos_btn_) {
    add_robot_pos_btn_->setVisible(show);
  }
}

void ViewManager::UpdateMapPos(const QString& text) {
  if (label_pos_map_) {
    label_pos_map_->setText(text);
  }
}

void ViewManager::UpdateScenePos(const QString& text) {
  if (label_pos_scene_) {
    label_pos_scene_->setText(text);
  }
}

void ViewManager::UpdateRobotPos(const QString& text) {
  if (label_pos_robot_) {
    label_pos_robot_->setText(text);
  }
}

void ViewManager::UpdateToolSizeSlider(double range) {
  if (tool_size_slider_ && tool_size_value_label_) {
    tool_size_slider_->setValue(static_cast<int>(range * 10));
    tool_size_value_label_->setText(QString::number(range, 'f', 1));
  }
}

void ViewManager::ShowToolSizeSlider(bool show) {
  if (tool_size_slider_) {
    tool_size_slider_->setVisible(show);
  }
  if (tool_size_value_label_) {
    tool_size_value_label_->setVisible(show);
  }
}

void ViewManager::OnEditMapModeChanged(MapEditMode mode) {
  bool show = (mode == MapEditMode::kErase || mode == MapEditMode::kDrawWithPen);
  ShowToolSizeSlider(show);
}

void ViewManager::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  if (!map_empty_state_) {
    return;
  }
  map_empty_state_->adjustSize();
  const QPoint center = viewport()->rect().center();
  map_empty_state_->move(center.x() - map_empty_state_->width() / 2,
                         center.y() - map_empty_state_->height() / 2);
  map_empty_state_->raise();
}

void ViewManager::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && display_manager_ptr_ &&
      display_manager_ptr_->IsRelocMode()) {
    display_manager_ptr_->SetRelocPositionFromScene(mapToScene(event->pos()));
    event->accept();
    return;
  }
  QGraphicsView::mousePressEvent(event);
}

void ViewManager::wheelEvent(QWheelEvent* event) {
  if (event->angleDelta().y() == 0) {
    QGraphicsView::wheelEvent(event);
    return;
  }
  ApplyMapViewScale(event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12,
                    QGraphicsView::AnchorUnderMouse);
  event->accept();
}

void ViewManager::mouseMoveEvent(QMouseEvent* event) {
  // 根据需要设置不同的鼠标指针样式
  // if (someCondition)
  //   QApplication::setOverrideCursor(Qt::PointingHandCursor); //
  //   设置为手指指针
  // else
  //   QApplication::restoreOverrideCursor(); // 恢复默认鼠标指针
  QGraphicsView::mouseMoveEvent(event);
}

void ViewManager::enterEvent(QEvent* event) {
  //   QApplication::setOverrideCursor(Qt::ArrowCursor); // 设置为箭头指针
  QGraphicsView::enterEvent(event);
}

void ViewManager::leaveEvent(QEvent* event) {
  QApplication::restoreOverrideCursor();  // 恢复默认鼠标指针
  QGraphicsView::leaveEvent(event);
}
}  // namespace Display
