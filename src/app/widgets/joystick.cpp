#include "widgets/joystick.h"

#include <QDebug>
#include <cmath>
JoyStick::JoyStick(QWidget* parent) : QWidget(parent) {
  setPalette(QPalette(Qt::white));
  if (parent != 0)
    resize(parent->width(), parent->height());
  setMinimumSize(100, 100);
  mouseX = width() / 2;
  mouseY = height() / 2;
  tim = new QTimer(this);
  tim->setInterval(50);
  connect(tim, &QTimer::timeout, this, &JoyStick::emitAxes);
}

JoyStick::~JoyStick() {}
void JoyStick::paintEvent(QPaintEvent*) {
  QPainter painter(this);

  int side = qMin(width(), height());

  padR = side / 2;       // 底盘半径
  padX = padR;           // 底盘圆心
  padY = padR;           // 底盘圆心
  JoyStickR = padR / 4;  // 摇杆圆半径
  int JoyStickMaxR = padR - JoyStickR;
  QColor JoyStickColor;
  JoyStickColor.setRgb(85, 87, 83);
  // 加载底盘图像
  //     painter.save();

  //    painter.scale(side / 400.0, side / 400.0);//坐标会随窗口缩放
  //    painter.drawPixmap(0, 0, QPixmap(":/image/pad.png"));
  //    painter.restore();
  // 自绘底盘
  painter.save();
  QRadialGradient RadialGradient(padR, padR, padR * 3, padR,
                                 padR);                      // 圆心2，半径1，焦点2
  RadialGradient.setColorAt(0, QColor(255, 253, 253, 255));  // 渐变
  RadialGradient.setColorAt(1, QColor(255, 240, 245, 190));  // 渐变
  painter.setBrush(RadialGradient);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPoint(padR, padR), side / 2, side / 2);  // 大圆盘
  painter.restore();

  // painter.drawText(20,20,tr("%1,%2,%3").arg(mouseX).arg(mouseY).arg(handPadDis));

  if (!mousePressed) {  // 鼠标没按下则摇杆恢复到底盘中心
    mouseX = padX;
    mouseY = padY;
  }
  handPadDis = Pointdis(padR, padR, mouseX, mouseY);
  if (handPadDis <= JoyStickMaxR) {
    JoyStickX = mouseX;
    JoyStickY = mouseY;
  } else {
    JoyStickX = (int)(JoyStickMaxR * (mouseX - padX) / handPadDis + padX);
    JoyStickY = (int)(JoyStickMaxR * (mouseY - padY) / handPadDis + padY);
  }
  // painter.drawText(200,200,tr("%1,%2,%3").arg(JoyStickX).arg(JoyStickY).arg(handPaddis));
  painter.setPen(Qt::NoPen);
  painter.setBrush(JoyStickColor);
  painter.drawEllipse(QPoint(JoyStickX, JoyStickY), JoyStickR,
                      JoyStickR);  // 摇杆
}
void JoyStick::mouseMoveEvent(QMouseEvent* event) {
  if (!mousePressed) return;
  mouseX = event->pos().x();
  mouseY = event->pos().y();
  update();
  emitAxes();
}
void JoyStick::mouseReleaseEvent(QMouseEvent* event) {
  mouseX = width() / 2;
  mouseY = height() / 2;
  tim->stop();
  mousePressed = false;
  emit keyNumchanged(JoyStick::stop);
  emit axesChanged(0.0, 0.0);
  update();
}
void JoyStick::mousePressEvent(QMouseEvent* event) {
  mouseX = event->pos().x();
  mouseY = event->pos().y();
  mousePressed = true;
  update();
  emitAxes();
  tim->start();
}

double JoyStick::Pointdis(int a, int b, int x, int y) {
  return sqrt((double)((x - a) * (x - a) + (y - b) * (y - b)));
}
int JoyStick::getKeyNum() {
  int x, y;
  int keynum;
  x = (int)(JoyStickX * 3.0 / (padR * 2));
  y = (int)(JoyStickY * 3.0 / (padR * 2));
  keynum = 3 * y + x;
  return keynum;
}

void JoyStick::emitAxes() {
  if (!mousePressed || padR <= 0) {
    emit axesChanged(0.0, 0.0);
    return;
  }
  const double max_radius = qMax(1, padR - JoyStickR);
  double x = (mouseX - padX) / max_radius;
  double y = (padY - mouseY) / max_radius;
  const double magnitude = std::sqrt(x * x + y * y);
  constexpr double dead_zone = 0.12;
  if (magnitude <= dead_zone) {
    emit axesChanged(0.0, 0.0);
    return;
  }
  const double clamped = qMin(1.0, magnitude);
  const double scaled = (clamped - dead_zone) / (1.0 - dead_zone);
  x = x / magnitude * scaled;
  y = y / magnitude * scaled;
  emit axesChanged(x, y);
}
