#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <QDebug>
#include <QDrag>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWidget>
#include <QtMath>
class JoyStick : public QWidget {
  Q_OBJECT

 public:
  JoyStick(QWidget* parent = 0);
  ~JoyStick();
  enum Direction : int {
    upleft = 0,
    up,
    upright,
    left,
    stop,
    right,
    downleft,
    down,
    downright
  };
 signals:
  void keyNumchanged(int num);
  void axesChanged(double x, double y);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  int mouseX = {0};
  int mouseY = {0};
  int JoyStickX{0};  // 摇杆
  int JoyStickY = {0};
  int JoyStickR = {0};
  int padX{0};  // 底盘
  int padY = {0};
  int padR = {0};
  double handPadDis{0.0};  // 两圆圆心距离
  bool mousePressed = {false};
  QTimer* tim;

 private:
  double Pointdis(int a, int b, int x, int y);  // 两点距离
  int getKeyNum();
  void emitAxes();
};

#endif  // JoyStick_H

