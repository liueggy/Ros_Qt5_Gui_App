#include "widgets/gps_location_widget.h"

#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

#include "widgets/ui_style.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
double ToRad(double value) { return value * kPi / 180.0; }

QFrame* MakeCard(QWidget* parent = nullptr) {
  auto* card = new QFrame(parent);
  card->setObjectName(QStringLiteral("gpsCard"));
  card->setStyleSheet(QStringLiteral(
      "QFrame#gpsCard{background:%1;border:1px solid %2;border-radius:8px;}")
                          .arg(UiStyle::Palette::Surface,
                               UiStyle::Palette::Border));
  return card;
}

QString QualityText(int quality) {
  switch (quality) {
    case 1: return QStringLiteral("GPS 单点定位");
    case 2: return QStringLiteral("差分定位");
    case 4: return QStringLiteral("RTK 固定解");
    case 5: return QStringLiteral("RTK 浮点解");
    default: return quality > 0 ? QStringLiteral("定位质量 %1").arg(quality)
                                : QStringLiteral("无有效定位");
  }
}
}  // namespace

GlobeWidget::GlobeWidget(QWidget* parent) : QWidget(parent) {
  setMinimumSize(420, 420);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setCursor(Qt::OpenHandCursor);
  texture_.load(QStringLiteral(":/images/earth_blue_marble.jpg"));
  if (!texture_.isNull())
    texture_ = texture_.convertToFormat(QImage::Format_RGB32);
}

void GlobeWidget::SetPosition(double latitude, double longitude, bool valid) {
  gps_latitude_ = latitude;
  gps_longitude_ = longitude;
  const bool first_fix = valid && !gps_valid_;
  gps_valid_ = valid;
  if (first_fix) FocusPosition();
  update();
}

void GlobeWidget::FocusPosition() {
  if (!gps_valid_) return;
  view_latitude_ = std::clamp(gps_latitude_, -75.0, 75.0);
  view_longitude_ = gps_longitude_;
  dirty_ = true;
  update();
}

void GlobeWidget::resizeEvent(QResizeEvent*) { dirty_ = true; }

void GlobeWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  dragging_ = true;
  drag_origin_ = event->pos();
  drag_latitude_ = view_latitude_;
  drag_longitude_ = view_longitude_;
  setCursor(Qt::ClosedHandCursor);
}

void GlobeWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) return;
  const QPoint delta = event->pos() - drag_origin_;
  view_longitude_ = drag_longitude_ - delta.x() * 0.35 / zoom_;
  view_latitude_ = std::clamp(drag_latitude_ + delta.y() * 0.28 / zoom_,
                              -85.0, 85.0);
  dirty_ = true;
  update();
}

void GlobeWidget::mouseReleaseEvent(QMouseEvent*) {
  dragging_ = false;
  setCursor(Qt::OpenHandCursor);
}

void GlobeWidget::wheelEvent(QWheelEvent* event) {
  zoom_ = std::clamp(zoom_ + event->angleDelta().y() / 1200.0, 0.65, 1.18);
  dirty_ = true;
  update();
  event->accept();
}

void GlobeWidget::RenderGlobe() {
  const int side = std::max(64, std::min(width(), height()) - 32);
  globe_ = QImage(side, side, QImage::Format_ARGB32_Premultiplied);
  globe_.fill(Qt::transparent);
  if (texture_.isNull()) return;

  const double radius = side * 0.5 * zoom_;
  const double center = side * 0.5;
  const double lat0 = ToRad(view_latitude_);
  const double lon0 = ToRad(view_longitude_);
  const double sx = std::sin(lon0), cx = std::cos(lon0);
  const double sy = std::sin(lat0), cy = std::cos(lat0);
  const int tw = texture_.width(), th = texture_.height();

  for (int py = 0; py < side; ++py) {
    auto* dst = reinterpret_cast<QRgb*>(globe_.scanLine(py));
    const double ny = (center - py) / radius;
    for (int px = 0; px < side; ++px) {
      const double nx = (px - center) / radius;
      const double r2 = nx * nx + ny * ny;
      if (r2 > 1.0) continue;
      const double nz = std::sqrt(1.0 - r2);
      const double wx = nx * -sx + ny * -sy * cx + nz * cy * cx;
      const double wy = nx * cx + ny * -sy * sx + nz * cy * sx;
      const double wz = ny * cy + nz * sy;
      const double lon = std::atan2(wy, wx);
      const double lat = std::asin(std::clamp(wz, -1.0, 1.0));
      int tx = static_cast<int>((lon / (2.0 * kPi) + 0.5) * tw) % tw;
      if (tx < 0) tx += tw;
      const int ty = std::clamp(static_cast<int>((0.5 - lat / kPi) * th),
                                0, th - 1);
      QColor color(texture_.pixel(tx, ty));
      const double light = std::clamp(0.48 + 0.62 * nz - 0.18 * nx, 0.28, 1.08);
      color.setRed(std::clamp(static_cast<int>(color.red() * light), 0, 255));
      color.setGreen(std::clamp(static_cast<int>(color.green() * light), 0, 255));
      color.setBlue(std::clamp(static_cast<int>(color.blue() * light + 8), 0, 255));
      dst[px] = color.rgba();
    }
  }
  dirty_ = false;
}

QPointF GlobeWidget::Project(double latitude, double longitude,
                             bool* visible) const {
  const double lat = ToRad(latitude), lon = ToRad(longitude);
  const double lat0 = ToRad(view_latitude_), lon0 = ToRad(view_longitude_);
  const double dlon = lon - lon0;
  const double front = std::sin(lat) * std::sin(lat0) +
                       std::cos(lat) * std::cos(lat0) * std::cos(dlon);
  if (visible) *visible = front > 0.0;
  const double x = std::cos(lat) * std::sin(dlon);
  const double y = std::sin(lat) * std::cos(lat0) -
                   std::cos(lat) * std::sin(lat0) * std::cos(dlon);
  const int side = globe_.width();
  const double radius = side * 0.5 * zoom_;
  return QPointF((width() - side) / 2.0 + side / 2.0 + x * radius,
                 (height() - side) / 2.0 + side / 2.0 - y * radius);
}

void GlobeWidget::paintEvent(QPaintEvent*) {
  if (dirty_ || globe_.isNull()) RenderGlobe();
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), UiStyle::IsDarkTheme() ? QColor("#111715")
                                                   : QColor("#eef3f2"));
  const QPoint origin((width() - globe_.width()) / 2,
                      (height() - globe_.height()) / 2);
  painter.setPen(QPen(QColor(35, 155, 177, 100), 5));
  painter.drawEllipse(QRectF(origin, globe_.size()).adjusted(8, 8, -8, -8));
  painter.drawImage(origin, globe_);
  if (gps_valid_) {
    bool visible = false;
    const QPointF marker = Project(gps_latitude_, gps_longitude_, &visible);
    if (visible) {
      painter.setPen(QPen(Qt::white, 3));
      painter.setBrush(QColor(UiStyle::Palette::Danger));
      painter.drawEllipse(marker, 8, 8);
      painter.setPen(QPen(QColor(UiStyle::Palette::Danger), 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(marker, 15, 15);
    }
  }
}

GpsLocationWidget::GpsLocationWidget(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(18, 18, 18, 18);
  root->setSpacing(12);

  auto* header = new QHBoxLayout();
  auto* title = new QLabel(QStringLiteral("卫星定位"));
  title->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  auto* subtitle = new QLabel(QStringLiteral("车载 GPS · WGS 84"));
  subtitle->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  state_badge_ = new QLabel(QStringLiteral("等待数据"));
  state_badge_->setAlignment(Qt::AlignCenter);
  state_badge_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  header->addWidget(title);
  header->addWidget(subtitle);
  header->addStretch();
  header->addWidget(state_badge_);
  root->addLayout(header);

  auto* splitter = new QSplitter(Qt::Horizontal);
  globe_ = new GlobeWidget();
  splitter->addWidget(globe_);

  auto* details = new QWidget();
  details->setMinimumWidth(330);
  auto* detail_layout = new QVBoxLayout(details);
  detail_layout->setContentsMargins(8, 0, 0, 0);
  detail_layout->setSpacing(10);

  auto* coordinate_card = MakeCard();
  auto* coordinates = new QGridLayout(coordinate_card);
  coordinates->setContentsMargins(16, 14, 16, 14);
  coordinates->addWidget(new QLabel(QStringLiteral("当前位置")), 0, 0, 1, 2);
  coordinates->addWidget(new QLabel(QStringLiteral("纬度")), 1, 0);
  latitude_value_ = MakeValueLabel(QStringLiteral("--"));
  coordinates->addWidget(latitude_value_, 1, 1);
  coordinates->addWidget(new QLabel(QStringLiteral("经度")), 2, 0);
  longitude_value_ = MakeValueLabel(QStringLiteral("--"));
  coordinates->addWidget(longitude_value_, 2, 1);
  detail_layout->addWidget(coordinate_card);

  auto* quality_card = MakeCard();
  auto* quality = new QGridLayout(quality_card);
  quality->setContentsMargins(16, 14, 16, 14);
  quality->setHorizontalSpacing(18);
  quality->setVerticalSpacing(12);
  const QStringList names = {QStringLiteral("海拔"), QStringLiteral("卫星"),
                             QStringLiteral("HDOP"), QStringLiteral("定位模式")};
  QLabel** values[] = {&altitude_value_, &satellites_value_, &hdop_value_,
                       &quality_value_};
  for (int i = 0; i < names.size(); ++i) {
    auto* name = new QLabel(names[i]);
    name->setStyleSheet(UiStyle::MutedLabelStyleSheet());
    *values[i] = MakeValueLabel(QStringLiteral("--"));
    quality->addWidget(name, i, 0);
    quality->addWidget(*values[i], i, 1);
  }
  detail_layout->addWidget(quality_card);

  auto* health_card = MakeCard();
  auto* health = new QGridLayout(health_card);
  health->setContentsMargins(16, 14, 16, 14);
  health->addWidget(new QLabel(QStringLiteral("数据状态")), 0, 0, 1, 2);
  health->addWidget(new QLabel(QStringLiteral("更新时间")), 1, 0);
  freshness_value_ = MakeValueLabel(QStringLiteral("--"));
  health->addWidget(freshness_value_, 1, 1);
  health->addWidget(new QLabel(QStringLiteral("数据源")), 2, 0);
  source_value_ = MakeValueLabel(QStringLiteral("/gps/fix"));
  health->addWidget(source_value_, 2, 1);
  detail_layout->addWidget(health_card);

  focus_button_ = new QPushButton(QStringLiteral("定位到小车"));
  focus_button_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  focus_button_->setEnabled(false);
  connect(focus_button_, &QPushButton::clicked, globe_, &GlobeWidget::FocusPosition);
  detail_layout->addWidget(focus_button_);
  detail_layout->addStretch();
  auto* credit = new QLabel(QStringLiteral("地球纹理：NASA/GSFC Scientific Visualization Studio"));
  credit->setWordWrap(true);
  credit->setStyleSheet(UiStyle::MutedLabelStyleSheet());
  detail_layout->addWidget(credit);

  splitter->addWidget(details);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setSizes({560, 360});
  root->addWidget(splitter, 1);

  freshness_timer_ = new QTimer(this);
  freshness_timer_->setInterval(500);
  connect(freshness_timer_, &QTimer::timeout, this, &GpsLocationWidget::Refresh);
  freshness_timer_->start();
}

QLabel* GpsLocationWidget::MakeValueLabel(const QString& initial) {
  auto* label = new QLabel(initial);
  label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setStyleSheet(QStringLiteral("font-weight:600;color:%1;")
                           .arg(UiStyle::Palette::Text));
  return label;
}

void GpsLocationWidget::SetFix(const basic::GpsFix& fix) {
  fix_ = fix;
  have_fix_ = true;
  last_fix_received_.restart();
  globe_->SetPosition(fix.latitude, fix.longitude, fix.IsValid());
  Refresh();
}

void GpsLocationWidget::SetStatus(const basic::GpsStatus& status) {
  status_ = status;
  have_status_ = true;
  Refresh();
}

void GpsLocationWidget::SetTransportConnected(bool connected) {
  transport_connected_ = connected;
  Refresh();
}

void GpsLocationWidget::Refresh() {
  const qint64 age_ms = have_fix_ && last_fix_received_.isValid()
                            ? last_fix_received_.elapsed() : -1;
  const bool fresh = age_ms >= 0 && age_ms <= 3500;
  const bool valid = fresh && fix_.IsValid() &&
                     (!have_status_ || status_.fix);
  const bool delayed = age_ms > 3500 && age_ms <= 10000;

  if (!transport_connected_) {
    state_badge_->setText(QStringLiteral("小车未连接"));
    state_badge_->setStyleSheet(UiStyle::StatusDangerStyleSheet());
  } else if (valid) {
    state_badge_->setText(QStringLiteral("定位有效"));
    state_badge_->setStyleSheet(UiStyle::StatusSuccessStyleSheet());
  } else if (delayed) {
    state_badge_->setText(QStringLiteral("数据延迟"));
    state_badge_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
  } else if (have_fix_) {
    state_badge_->setText(QStringLiteral("定位过期"));
    state_badge_->setStyleSheet(UiStyle::StatusDangerStyleSheet());
  } else {
    state_badge_->setText(QStringLiteral("等待 GPS"));
    state_badge_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  }

  if (have_fix_) {
    latitude_value_->setText(QString::number(fix_.latitude, 'f', 7) + QStringLiteral("°"));
    longitude_value_->setText(QString::number(fix_.longitude, 'f', 7) + QStringLiteral("°"));
    altitude_value_->setText(QString::number(fix_.altitude, 'f', 1) + QStringLiteral(" m"));
  }
  if (have_status_) {
    satellites_value_->setText(QStringLiteral("%1 使用 / %2 可见")
                                   .arg(status_.satellites)
                                   .arg(status_.satellites_visible));
    hdop_value_->setText(status_.hdop > 0.0 ? QString::number(status_.hdop, 'f', 1)
                                            : QStringLiteral("--"));
    quality_value_->setText(QualityText(status_.fix_quality));
    source_value_->setText(status_.frame_id.empty()
                               ? QStringLiteral("/gps/fix · WGS 84")
                               : QStringLiteral("/gps/fix · %1")
                                     .arg(QString::fromStdString(status_.frame_id)));
  } else if (have_fix_) {
    quality_value_->setText(fix_.IsValid() ? QStringLiteral("有效")
                                           : QStringLiteral("无定位"));
  }
  freshness_value_->setText(age_ms < 0 ? QStringLiteral("尚未收到")
                                       : QStringLiteral("%1 秒前").arg(age_ms / 1000.0, 0, 'f', 1));
  focus_button_->setEnabled(have_fix_ && fix_.IsValid());
}
