#pragma once
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <iostream>
#include <map>
#include <vector>
#include "algorithm.h"
#include "point_type.h"
#include "widgets/joystick.h"
using namespace basic;
class SpeedCtrlWidget : public QWidget {
  Q_OBJECT
 private:
  QCheckBox *checkBox_use_all_;
  JoyStick *joyStick_widget_;
  QSlider *horizontalSlider_raw_;
  QSlider *horizontalSlider_linear_;
 signals:
  void signalControlSpeed(const RobotSpeed &speed);
 private slots:
  void slotSpeedControl() {
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn) {
      return;
    }
    const QByteArray control_key = btn->property("controlKey").toByteArray();
    if (control_key.isEmpty()) {
      return;
    }
    char button_key = control_key.at(0);
    //速度
    float liner = horizontalSlider_linear_->value() * 0.01;
    float turn = horizontalSlider_raw_->value() * 0.01;
    bool is_all = checkBox_use_all_->isChecked();
    char key;

    switch (button_key) {
      case 'u':
        key = is_all ? 'U' : 'u';
        break;
      case 'i':
        key = is_all ? 'I' : 'i';
        break;
      case 'o':
        key = is_all ? 'O' : 'o';
        break;
      case 'j':
        key = is_all ? 'J' : 'j';
        break;
      case 'l':
        key = is_all ? 'L' : 'l';
        break;
      case 'm':
        key = is_all ? 'M' : 'm';
        break;
      case ',':
        key = is_all ? '<' : ',';
        break;
      case '.':
        key = is_all ? '>' : '.';
        break;
      default:
        return;
    }

    std::map<char, std::vector<float>> moveBindings{
        {'i', {1, 0, 0, 0}}, {'o', {1, 0, 0, -1}}, {'j', {0, 0, 0, 1}}, {'l', {0, 0, 0, -1}}, {'u', {1, 0, 0, 1}}, {',', {-1, 0, 0, 0}}, {'.', {-1, 0, 0, 1}}, {'m', {-1, 0, 0, -1}}, {'O', {1, -1, 0, 0}}, {'I', {1, 0, 0, 0}}, {'J', {0, 1, 0, 0}}, {'L', {0, -1, 0, 0}}, {'U', {1, 1, 0, 0}}, {'<', {-1, 0, 0, 0}}, {'>', {-1, -1, 0, 0}}, {'M', {-1, 1, 0, 0}}, {'t', {0, 0, 1, 0}}, {'b', {0, 0, -1, 0}}, {'k', {0, 0, 0, 0}}, {'K', {0, 0, 0, 0}}};
    //计算是往哪个方向

    float x = moveBindings[key][0];
    float y = moveBindings[key][1];
    float z = moveBindings[key][2];
    float th = moveBindings[key][3];
    emit signalControlSpeed(RobotSpeed(x * liner, y * liner, th * turn));
  }
  void slotJoyStickKeyChange(int value) {
    //速度
    float liner = horizontalSlider_linear_->value() * 0.01;
    float turn = horizontalSlider_raw_->value() * 0.01;
    bool is_all = checkBox_use_all_->isChecked();
    char key;
    std::cout << "joy stic value:" << value << std::endl;
    switch (value) {
      case JoyStick::Direction::upleft:
        key = is_all ? 'U' : 'u';
        break;
      case JoyStick::Direction::up:
        key = is_all ? 'I' : 'i';
        break;
      case JoyStick::Direction::upright:
        key = is_all ? 'O' : 'o';
        break;
      case JoyStick::Direction::left:
        key = is_all ? 'J' : 'j';
        break;
      case JoyStick::Direction::right:
        key = is_all ? 'L' : 'l';
        break;
      case JoyStick::Direction::down:
        key = is_all ? 'M' : 'm';
        break;
      case JoyStick::Direction::downleft:
        key = is_all ? '<' : ',';
        break;
      case JoyStick::Direction::downright:
        key = is_all ? '>' : '.';
        break;
      default:
        return;
    }
    std::map<char, std::vector<float>> moveBindings{
        {'i', {1, 0, 0, 0}}, {'o', {1, 0, 0, -1}}, {'j', {0, 0, 0, 1}}, {'l', {0, 0, 0, -1}}, {'u', {1, 0, 0, 1}}, {',', {-1, 0, 0, 0}}, {'.', {-1, 0, 0, 1}}, {'m', {-1, 0, 0, -1}}, {'O', {1, -1, 0, 0}}, {'I', {1, 0, 0, 0}}, {'J', {0, 1, 0, 0}}, {'L', {0, -1, 0, 0}}, {'U', {1, 1, 0, 0}}, {'<', {-1, 0, 0, 0}}, {'>', {-1, -1, 0, 0}}, {'M', {-1, 1, 0, 0}}, {'t', {0, 0, 1, 0}}, {'b', {0, 0, -1, 0}}, {'k', {0, 0, 0, 0}}, {'K', {0, 0, 0, 0}}};
    //计算是往哪个方向
    float x = moveBindings[key][0];
    float y = moveBindings[key][1];
    float z = moveBindings[key][2];
    float th = moveBindings[key][3];
    emit signalControlSpeed(RobotSpeed(x * liner, y * liner, th * turn));
  }

 public:
  SpeedCtrlWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setObjectName("speedControlPanel");
    setMinimumWidth(360);
    setStyleSheet(R"(
      QWidget#speedControlPanel {
        background: #f4f7fb;
        color: #172033;
      }
      QTabWidget::pane {
        border: 1px solid #dce3ec;
        border-radius: 10px;
        background: #ffffff;
        top: -1px;
      }
      QTabBar::tab {
        min-width: 96px;
        padding: 9px 14px;
        color: #64748b;
        background: transparent;
        border-bottom: 2px solid transparent;
        font-weight: 600;
      }
      QTabBar::tab:selected {
        color: #2563eb;
        border-bottom-color: #2563eb;
      }
      QPushButton[controlButton="true"] {
        min-width: 68px;
        max-width: 68px;
        min-height: 58px;
        max-height: 58px;
        border: 1px solid #d7e0eb;
        border-radius: 12px;
        background: #f8fafc;
        color: #1d4ed8;
        font-size: 22px;
        font-weight: 700;
      }
      QPushButton[controlButton="true"]:hover {
        background: #eff6ff;
        border-color: #93b4f5;
      }
      QPushButton[controlButton="true"]:pressed {
        background: #dbeafe;
        border-color: #2563eb;
      }
      QCheckBox {
        spacing: 8px;
        color: #475569;
        font-weight: 600;
      }
      QSlider::groove:horizontal {
        height: 6px;
        border-radius: 3px;
        background: #dce3ec;
      }
      QSlider::sub-page:horizontal {
        border-radius: 3px;
        background: #2563eb;
      }
      QSlider::handle:horizontal {
        width: 16px;
        margin: -5px 0;
        border-radius: 8px;
        background: #ffffff;
        border: 2px solid #2563eb;
      }
      QLabel[metricValue="true"] {
        min-width: 78px;
        color: #0f172a;
        font-family: Consolas, monospace;
        font-weight: 600;
      }
      QPushButton#btn_stop {
        min-height: 42px;
        border: none;
        border-radius: 9px;
        background: #dc2626;
        color: #ffffff;
        font-size: 14px;
        font-weight: 700;
      }
      QPushButton#btn_stop:hover { background: #b91c1c; }
      QPushButton#btn_stop:pressed { background: #991b1b; }
    )");

    auto *root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(14, 12, 14, 14);
    root_layout->setSpacing(12);

    auto *tabs = new QTabWidget(this);
    auto *command_page = new QWidget(tabs);
    auto *command_layout = new QVBoxLayout(command_page);
    command_layout->setContentsMargins(18, 18, 18, 20);
    command_layout->setSpacing(14);

    auto *hint = new QLabel("点击方向键或使用 U / I / O / J / L / M / , / . 快捷键",
                            command_page);
    hint->setStyleSheet("color:#64748b; font-size:12px;");
    hint->setWordWrap(true);
    command_layout->addWidget(hint);

    auto *direction_grid = new QGridLayout();
    direction_grid->setHorizontalSpacing(12);
    direction_grid->setVerticalSpacing(12);
    direction_grid->setAlignment(Qt::AlignHCenter);

    auto make_button = [this, direction_grid](const QString &label,
                                               const char *key, int row,
                                               int column) {
      auto *button = new QPushButton(label, this);
      button->setProperty("controlButton", true);
      button->setProperty("controlKey", key);
      button->setShortcut(QKeySequence(QString::fromLatin1(key)));
      button->setToolTip(QString("快捷键 %1").arg(QString::fromLatin1(key)));
      connect(button, &QPushButton::clicked, this,
              &SpeedCtrlWidget::slotSpeedControl);
      direction_grid->addWidget(button, row, column);
      return button;
    };

    make_button("↖", "u", 0, 0);
    make_button("↑", "i", 0, 1);
    make_button("↗", "o", 0, 2);
    make_button("↶", "j", 1, 0);
    checkBox_use_all_ = new QCheckBox("全向模式", command_page);
    checkBox_use_all_->setToolTip("启用麦克纳姆轮横向移动");
    direction_grid->addWidget(checkBox_use_all_, 1, 1, Qt::AlignCenter);
    make_button("↷", "l", 1, 2);
    make_button("↙", "m", 2, 0);
    make_button("↓", ",", 2, 1);
    make_button("↘", ".", 2, 2);
    command_layout->addLayout(direction_grid);
    command_layout->addStretch();

    auto *joystick_page = new QWidget(tabs);
    auto *joystick_layout = new QVBoxLayout(joystick_page);
    joystick_layout->setContentsMargins(18, 18, 18, 18);
    joyStick_widget_ = new JoyStick(joystick_page);
    joyStick_widget_->setMinimumSize(QSize(220, 220));
    joyStick_widget_->setMaximumSize(QSize(320, 320));
    connect(joyStick_widget_, SIGNAL(keyNumchanged(int)), this,
            SLOT(slotJoyStickKeyChange(int)));
    joystick_layout->addStretch();
    joystick_layout->addWidget(joyStick_widget_, 0, Qt::AlignCenter);
    joystick_layout->addStretch();

    tabs->addTab(command_page, "方向控制");
    tabs->addTab(joystick_page, "虚拟摇杆");
    root_layout->addWidget(tabs, 1);

    auto add_speed_row = [root_layout](const QString &title, QSlider *slider,
                                       QLabel *value_label) {
      auto *row = new QHBoxLayout();
      row->setSpacing(10);
      auto *title_label = new QLabel(title);
      title_label->setMinimumWidth(58);
      title_label->setStyleSheet("color:#475569; font-weight:600;");
      value_label->setProperty("metricValue", true);
      value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      row->addWidget(title_label);
      row->addWidget(slider, 1);
      row->addWidget(value_label);
      root_layout->addLayout(row);
    };

    horizontalSlider_raw_ = new QSlider(Qt::Horizontal, this);
    horizontalSlider_raw_->setRange(0, 100);
    horizontalSlider_raw_->setValue(10);
    auto *angular_value = new QLabel(this);
    auto update_angular = [angular_value](int value) {
      angular_value->setText(
          QString::number(rad2deg(value * 0.01), 'f', 1) + " °/s");
    };
    connect(horizontalSlider_raw_, &QSlider::valueChanged, this,
            update_angular);
    update_angular(horizontalSlider_raw_->value());
    add_speed_row("角速度", horizontalSlider_raw_, angular_value);

    horizontalSlider_linear_ = new QSlider(Qt::Horizontal, this);
    horizontalSlider_linear_->setRange(0, 100);
    horizontalSlider_linear_->setSingleStep(1);
    horizontalSlider_linear_->setValue(10);
    auto *linear_value = new QLabel(this);
    auto update_linear = [linear_value](int value) {
      linear_value->setText(QString::number(value * 0.01, 'f', 2) + " m/s");
    };
    connect(horizontalSlider_linear_, &QSlider::valueChanged, this,
            update_linear);
    update_linear(horizontalSlider_linear_->value());
    add_speed_row("线速度", horizontalSlider_linear_, linear_value);

    auto *stop_button = new QPushButton("立即停止  ·  S", this);
    stop_button->setObjectName("btn_stop");
    stop_button->setShortcut(QKeySequence("s"));
    connect(stop_button, &QPushButton::clicked, this,
            [this]() { emit signalControlSpeed(RobotSpeed()); });
    root_layout->addWidget(stop_button);
  }

  ~SpeedCtrlWidget() {}
};
