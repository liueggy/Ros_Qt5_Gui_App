#pragma once
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QKeyEvent>
#include <QKeySequence>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtMath>
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
#include <algorithm>
#include <array>
#include "algorithm.h"
#include "point_type.h"
#include "widgets/joystick.h"
#include "widgets/ui_style.h"
using namespace basic;
class SpeedCtrlWidget : public QWidget {
  Q_OBJECT
 private:
  QCheckBox* checkBox_use_all_;
  JoyStick* joyStick_widget_;
  QSlider* horizontalSlider_raw_;
  QSlider* horizontalSlider_linear_;
  QTimer* command_timer_{nullptr};
  RobotSpeed active_speed_;
  double joystick_x_ = {0.0};
  double joystick_y_ = {0.0};
  bool joystick_active_ = {false};

  QPushButton* move_btn_u_{nullptr};
  QPushButton* move_btn_i_{nullptr};
  QPushButton* move_btn_o_{nullptr};
  QPushButton* move_btn_j_{nullptr};
  QPushButton* move_btn_l_{nullptr};
  QPushButton* move_btn_m_{nullptr};
  QPushButton* move_btn_back_{nullptr};
  QPushButton* move_btn_backr_{nullptr};
  char active_keyboard_key_ = {'\0'};

  struct MoveBinding {
    char key;
    double x;
    double y;
    double theta;
  };

  double LinearSpeedLimit() const {
    return horizontalSlider_linear_->value() * 0.01;
  }

  double AngularSpeedLimitRad() const {
    return qDegreesToRadians(static_cast<double>(horizontalSlider_raw_->value()));
  }

  char ResolveMoveKey(char button_key) const {
    const bool is_all = checkBox_use_all_->isChecked();
    switch (button_key) {
      case 'u':
        return is_all ? 'U' : 'u';
      case 'i':
        return is_all ? 'I' : 'i';
      case 'o':
        return is_all ? 'O' : 'o';
      case 'j':
        return is_all ? 'J' : 'j';
      case 'l':
        return is_all ? 'L' : 'l';
      case 'm':
        return is_all ? 'M' : 'm';
      case ',':
        return is_all ? '<' : ',';
      case '.':
        return is_all ? '>' : '.';
      case 'q':
        return is_all ? 'Q' : 'q';
      case 'w':
        return is_all ? 'W' : 'w';
      case 'e':
        return is_all ? 'E' : 'e';
      case 'a':
        return is_all ? 'A' : 'a';
      case 'd':
        return is_all ? 'D' : 'd';
      case 'z':
        return is_all ? 'Z' : 'z';
      case 'x':
        return is_all ? 'X' : 'x';
      case 'c':
        return is_all ? 'C' : 'c';
      default:
        return '\0';
    }
  }

  bool LookupMoveBinding(char key, MoveBinding* binding) const {
    static constexpr std::array<MoveBinding, 32> kMoveBindings{{
        {'i', 1, 0, 0}, {'o', 1, 0, -1}, {'j', 0, 0, 1},
        {'l', 0, 0, -1}, {'u', 1, 0, 1}, {',', -1, 0, 0},
        {'.', -1, 0, 1}, {'m', -1, 0, -1}, {'O', 1, -1, 0},
        {'I', 1, 0, 0}, {'J', 0, 1, 0}, {'L', 0, -1, 0},
        {'U', 1, 1, 0}, {'<', -1, 0, 0}, {'>', -1, -1, 0},
        {'M', -1, 1, 0},
        // QWEASDZXC 键盘绑定（非全向 / 全向）
        {'q', 1, 0, 1}, {'w', 1, 0, 0}, {'e', 1, 0, -1},
        {'a', 0, 0, 1}, {'d', 0, 0, -1},
        {'z', -1, 0, 1}, {'x', -1, 0, 0}, {'c', -1, 0, -1},
        {'Q', 1, 1, 0}, {'W', 1, 0, 0}, {'E', 1, -1, 0},
        {'A', 0, 1, 0}, {'D', 0, -1, 0},
        {'Z', -1, 1, 0}, {'X', -1, 0, 0}, {'C', -1, -1, 0},
    }};
    const auto it = std::find_if(
        kMoveBindings.begin(), kMoveBindings.end(),
        [key](const MoveBinding& candidate) { return candidate.key == key; });
    if (it == kMoveBindings.end()) {
      return false;
    }
    *binding = *it;
    return true;
  }

  void PublishActiveSpeed() {
    emit signalControlSpeed(active_speed_);
  }

  void StartActiveSpeed(const RobotSpeed& speed) {
    active_speed_ = speed;
    PublishActiveSpeed();
    if (!command_timer_->isActive()) {
      command_timer_->start();
    }
  }

  void UpdateJoystickSpeed() {
    const double linear = LinearSpeedLimit();
    const double angular = AngularSpeedLimitRad();
    if (checkBox_use_all_->isChecked()) {
      active_speed_ = RobotSpeed(joystick_y_ * linear, -joystick_x_ * linear, 0.0);
    } else {
      active_speed_ = RobotSpeed(joystick_y_ * linear, 0.0, -joystick_x_ * angular);
    }
  }
 signals:
  void signalControlSpeed(const RobotSpeed& speed);
 private slots:
  void slotSpeedControl() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
      return;
    }
    const QByteArray move_key = btn->property("moveKey").toByteArray();
    if (move_key.isEmpty()) return;
    MoveBinding binding = {};
    if (!LookupMoveBinding(ResolveMoveKey(move_key.at(0)), &binding)) {
      return;
    }
    joystick_active_ = false;
    StartActiveSpeed(RobotSpeed(binding.x * LinearSpeedLimit(),
                                binding.y * LinearSpeedLimit(),
                                binding.theta * AngularSpeedLimitRad()));
  }
  void slotStopControl() {
    command_timer_->stop();
    joystick_active_ = false;
    joystick_x_ = 0.0;
    joystick_y_ = 0.0;
    active_speed_ = RobotSpeed();
    PublishActiveSpeed();
  }
  void slotJoyStickAxes(double x, double y) {
    if (qFuzzyIsNull(x) && qFuzzyIsNull(y)) {
      slotStopControl();
      return;
    }
    constexpr double kSmoothing = 0.35;
    joystick_active_ = true;
    joystick_x_ = joystick_x_ + (x - joystick_x_) * kSmoothing;
    joystick_y_ = joystick_y_ + (y - joystick_y_) * kSmoothing;
    UpdateJoystickSpeed();
    PublishActiveSpeed();
    if (!command_timer_->isActive()) {
      command_timer_->start();
    }
  }
  void slotJoyStickKeyChange(int value) {
    char button_key;
    switch (value) {
      case JoyStick::Direction::upleft:
        button_key = 'u';
        break;
      case JoyStick::Direction::up:
        button_key = 'i';
        break;
      case JoyStick::Direction::upright:
        button_key = 'o';
        break;
      case JoyStick::Direction::left:
        button_key = 'j';
        break;
      case JoyStick::Direction::right:
        button_key = 'l';
        break;
      case JoyStick::Direction::down:
        button_key = 'm';
        break;
      case JoyStick::Direction::downleft:
        button_key = ',';
        break;
      case JoyStick::Direction::downright:
        button_key = '.';
        break;
      default:
        return;
    }
    MoveBinding binding = {};
    if (!LookupMoveBinding(ResolveMoveKey(button_key), &binding)) {
      return;
    }
    StartActiveSpeed(RobotSpeed(binding.x * LinearSpeedLimit(),
                                binding.y * LinearSpeedLimit(),
                                binding.theta * AngularSpeedLimitRad()));
  }

 public:
  SpeedCtrlWidget(QWidget* parent = 0) : QWidget(parent) {
    command_timer_ = new QTimer(this);
    setFocusPolicy(Qt::StrongFocus);
    command_timer_->setInterval(50);
    connect(command_timer_, &QTimer::timeout, this,
            [this]() {
              if (joystick_active_) {
                UpdateJoystickSpeed();
              }
              PublishActiveSpeed();
            });
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    const QString moveButtonStyle = UiStyle::MoveButtonStyleSheet();
    const QSize moveButtonSize(56, 56);
    setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::CheckBoxStyleSheet() + QStringLiteral("QTabWidget::pane { border:1px solid %1; border-radius:10px; background:%2; top:-1px; }"
                                                                                              "QTabBar::tab { padding:7px 18px; color:%3; border:none; background:transparent; }"
                                                                                              "QTabBar::tab:selected { color:%4; font-weight:700; border-bottom:2px solid %4; }"
                                                                                              "QSlider::groove:horizontal { height:5px; border-radius:2px; background:%1; }"
                                                                                              "QSlider::sub-page:horizontal { background:%4; border-radius:2px; }"
                                                                                              "QSlider::handle:horizontal { background:%4; width:14px; height:14px; margin:-5px 0; border-radius:7px; }")
        .arg(UiStyle::Palette::Border, UiStyle::Palette::Surface, UiStyle::Palette::TextMuted, UiStyle::Palette::Primary));
    QVBoxLayout* verticalLayout_speed_ctrl = new QVBoxLayout();
    verticalLayout_speed_ctrl->setContentsMargins(10, 10, 10, 10);
    verticalLayout_speed_ctrl->setSpacing(8);
    verticalLayout_speed_ctrl->setObjectName(
        QString::fromUtf8("verticalLayout_speed_ctrl"));
    QFrame* control_card = new QFrame(this);
    control_card->setObjectName(QStringLiteral("speedControlCard"));
    control_card->setProperty("uiCard", true);
    control_card->setStyleSheet(UiStyle::CardStyleSheet());
    QVBoxLayout* control_layout = new QVBoxLayout(control_card);
    control_layout->setContentsMargins(12, 10, 12, 12);
    control_layout->setSpacing(8);
    QVBoxLayout* verticalLayout_cmd_btn = new QVBoxLayout();
    verticalLayout_cmd_btn->setContentsMargins(8, 6, 8, 10);
    verticalLayout_cmd_btn->setSpacing(8);
    QHBoxLayout* horizontalLayout_2 = new QHBoxLayout();
    horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
    horizontalLayout_2->setSpacing(42);
    horizontalLayout_2->setAlignment(Qt::AlignCenter);
    move_btn_u_ = new QPushButton();
    move_btn_u_->setObjectName(QString::fromUtf8("pushButton_u"));
    move_btn_u_->setProperty("moveKey", "u");
    move_btn_u_->setToolTip(QStringLiteral("左前"));
    move_btn_u_->setMinimumSize(moveButtonSize);
    move_btn_u_->setMaximumSize(moveButtonSize);
    move_btn_u_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up_left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_left_2.png);}"));
    move_btn_u_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up-left.svg")));
    move_btn_u_->setIconSize(QSize(30, 30));
    move_btn_u_->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(move_btn_u_);

    move_btn_i_ = new QPushButton();
    move_btn_i_->setObjectName(QString::fromUtf8("pushButton_i"));
    move_btn_i_->setProperty("moveKey", "i");
    move_btn_i_->setToolTip(QStringLiteral("前进"));
    move_btn_i_->setMinimumSize(moveButtonSize);
    move_btn_i_->setMaximumSize(moveButtonSize);
    move_btn_i_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_2.png);}"));
    move_btn_i_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up.svg")));
    move_btn_i_->setIconSize(QSize(30, 30));
    move_btn_i_->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(move_btn_i_);

    move_btn_o_ = new QPushButton();
    move_btn_o_->setObjectName(QString::fromUtf8("pushButton_o"));
    move_btn_o_->setProperty("moveKey", "o");
    move_btn_o_->setToolTip(QStringLiteral("右前"));
    move_btn_o_->setMinimumSize(moveButtonSize);
    move_btn_o_->setMaximumSize(moveButtonSize);
    move_btn_o_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up_right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_right_2.png);}"));
    move_btn_o_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up-right.svg")));
    move_btn_o_->setIconSize(QSize(30, 30));
    move_btn_o_->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(move_btn_o_);

    verticalLayout_cmd_btn->addLayout(horizontalLayout_2);

    QHBoxLayout* horizontalLayout_18 = new QHBoxLayout();
    horizontalLayout_18->setObjectName(
        QString::fromUtf8("horizontalLayout_18"));
    horizontalLayout_18->setSpacing(42);
    horizontalLayout_18->setAlignment(Qt::AlignCenter);
    move_btn_j_ = new QPushButton();
    move_btn_j_->setProperty("moveKey", "j");
    move_btn_j_->setToolTip(QStringLiteral("左移或左转"));
    move_btn_j_->setObjectName(QString::fromUtf8("pushButton_j"));
    move_btn_j_->setMinimumSize(moveButtonSize);
    move_btn_j_->setMaximumSize(moveButtonSize);
    move_btn_j_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/left_2.png);}"));
    move_btn_j_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-left.svg")));
    move_btn_j_->setIconSize(QSize(30, 30));
    move_btn_j_->setStyleSheet(moveButtonStyle);

    horizontalLayout_18->addWidget(move_btn_j_);

    checkBox_use_all_ = new QCheckBox();
    checkBox_use_all_->setObjectName(QString::fromUtf8("checkBox_use_all_"));
    checkBox_use_all_->setMinimumSize(QSize(78, 36));
    checkBox_use_all_->setMaximumSize(QSize(90, 36));
    checkBox_use_all_->setText("全向");
    checkBox_use_all_->setChecked(true);
    checkBox_use_all_->setCursor(Qt::PointingHandCursor);
    checkBox_use_all_->setStyleSheet(UiStyle::CompactCheckBoxStyleSheet());
    horizontalLayout_18->addWidget(checkBox_use_all_);

    move_btn_l_ = new QPushButton();
    move_btn_l_->setObjectName(QString::fromUtf8("pushButton_l"));
    move_btn_l_->setProperty("moveKey", "l");
    move_btn_l_->setToolTip(QStringLiteral("右移或右转"));
    move_btn_l_->setMinimumSize(moveButtonSize);
    move_btn_l_->setMaximumSize(moveButtonSize);
    move_btn_l_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/right_2.png);}"));
    move_btn_l_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-right.svg")));
    move_btn_l_->setIconSize(QSize(30, 30));
    move_btn_l_->setStyleSheet(moveButtonStyle);

    horizontalLayout_18->addWidget(move_btn_l_);

    verticalLayout_cmd_btn->addLayout(horizontalLayout_18);

    QHBoxLayout* horizontalLayout_19 = new QHBoxLayout();
    horizontalLayout_19->setObjectName(
        QString::fromUtf8("horizontalLayout_19"));
    horizontalLayout_19->setSpacing(42);
    horizontalLayout_19->setAlignment(Qt::AlignCenter);
    move_btn_m_ = new QPushButton();
    move_btn_m_->setObjectName(QString::fromUtf8("pushButton_m"));
    move_btn_m_->setProperty("moveKey", "m");
    move_btn_m_->setToolTip(QStringLiteral("左后"));
    move_btn_m_->setMinimumSize(moveButtonSize);
    move_btn_m_->setMaximumSize(moveButtonSize);
    move_btn_m_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down_left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_left_2.png);}"));
    move_btn_m_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down-left.svg")));
    move_btn_m_->setIconSize(QSize(30, 30));
    move_btn_m_->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(move_btn_m_);

    move_btn_back_ = new QPushButton();
    move_btn_back_->setObjectName(QString::fromUtf8("pushButton_,"));
    move_btn_back_->setProperty("moveKey", ",");
    move_btn_back_->setToolTip(QStringLiteral("后退"));
    move_btn_back_->setMinimumSize(moveButtonSize);
    move_btn_back_->setMaximumSize(moveButtonSize);
    move_btn_back_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_2.png);}"));
    move_btn_back_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down.svg")));
    move_btn_back_->setIconSize(QSize(30, 30));
    move_btn_back_->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(move_btn_back_);

    move_btn_backr_ = new QPushButton();
    move_btn_backr_->setObjectName(QString::fromUtf8("pushButton_."));
    move_btn_backr_->setProperty("moveKey", ".");
    move_btn_backr_->setToolTip(QStringLiteral("右后"));
    move_btn_backr_->setMinimumSize(moveButtonSize);
    move_btn_backr_->setMaximumSize(moveButtonSize);
    move_btn_backr_->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down_right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_right_2.png);}"));

    const QList<QPushButton*> move_buttons{
        move_btn_i_, move_btn_u_, move_btn_o_, move_btn_j_,
        move_btn_l_, move_btn_m_, move_btn_back_, move_btn_backr_};
    for (auto* button : move_buttons) {
      connect(button, &QPushButton::pressed, this,
              &SpeedCtrlWidget::slotSpeedControl);
      connect(button, &QPushButton::released, this,
              &SpeedCtrlWidget::slotStopControl);
    }
    move_btn_backr_->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down-right.svg")));
    move_btn_backr_->setIconSize(QSize(30, 30));
    move_btn_backr_->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(move_btn_backr_);

    verticalLayout_cmd_btn->addLayout(horizontalLayout_19);

    QWidget* cmdCtrlWidget = new QWidget();
    cmdCtrlWidget->setLayout(verticalLayout_cmd_btn);
    cmdCtrlWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QTabWidget* tabWidget = new QTabWidget;
    tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    tabWidget->addTab(cmdCtrlWidget, "命令控制");
    control_layout->addWidget(tabWidget);

    QWidget* widget_joyStick = new QWidget();
    QHBoxLayout* horizontalLayout_joyStick = new QHBoxLayout();
    joyStick_widget_ = new JoyStick();
    joyStick_widget_->setMinimumSize(QSize(200, 200));

    connect(joyStick_widget_, &JoyStick::axesChanged, this,
            &SpeedCtrlWidget::slotJoyStickAxes);

    horizontalLayout_joyStick->addStretch();
    horizontalLayout_joyStick->addWidget(joyStick_widget_);
    horizontalLayout_joyStick->addStretch();
    widget_joyStick->setLayout(horizontalLayout_joyStick);

    tabWidget->addTab(widget_joyStick, "摇杆控制");

    QHBoxLayout* horizontalLayout_20 = new QHBoxLayout();
    horizontalLayout_20->setObjectName(
        QString::fromUtf8("horizontalLayout_20"));
    QLabel* label_14 = new QLabel();
    label_14->setObjectName(QString::fromUtf8("label_14"));
    label_14->setText("角速度:");
    label_14->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(UiStyle::Palette::TextSecondary));
    horizontalLayout_20->addWidget(label_14);

    horizontalSlider_raw_ = new QSlider();
    horizontalSlider_raw_->setObjectName(
        QString::fromUtf8("horizontalSlider_raw_"));
    horizontalSlider_raw_->setMaximum(90);
    horizontalSlider_raw_->setValue(50);
    horizontalSlider_raw_->setOrientation(Qt::Horizontal);

    horizontalLayout_20->addWidget(horizontalSlider_raw_);

    QLabel* label_raw = new QLabel();
    label_raw->setObjectName(QString::fromUtf8("label_raw"));
    label_raw->setText(QString::number(horizontalSlider_raw_->value(), 'f', 2) +
                       " deg/s");
    label_raw->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(UiStyle::Palette::Text));
    connect(horizontalSlider_raw_, &QSlider::valueChanged,
            [label_raw](qreal value) {
              label_raw->setText(QString::number(value, 'f', 2) +
                                 " deg/s");
            });
    horizontalLayout_20->addWidget(label_raw);

    label_14->setFixedWidth(68);
    label_raw->setMinimumWidth(86);
    label_raw->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    control_layout->addLayout(horizontalLayout_20);

    // linear anglur
    QHBoxLayout* horizontalLayout_21 = new QHBoxLayout();
    horizontalLayout_21->setObjectName(
        QString::fromUtf8("horizontalLayout_21"));
    QLabel* label_9 = new QLabel();
    label_9->setObjectName(QString::fromUtf8("label_9"));
    label_9->setText("线速度:");
    label_9->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(UiStyle::Palette::TextSecondary));
    horizontalLayout_21->addWidget(label_9);

    horizontalSlider_linear_ = new QSlider();
    horizontalSlider_linear_->setObjectName(
        QString::fromUtf8("horizontalSlider_linear_"));
    horizontalSlider_linear_->setMaximum(100);
    horizontalSlider_linear_->setSingleStep(1);
    horizontalSlider_linear_->setValue(25);
    horizontalSlider_linear_->setOrientation(Qt::Horizontal);

    horizontalLayout_21->addWidget(horizontalSlider_linear_);

    QLabel* label_linear = new QLabel();
    label_linear->setObjectName(QString::fromUtf8("label_linear"));
    label_linear->setText(
        QString::number(horizontalSlider_linear_->value() * 0.01, 'f', 2) +
        " m/s");
    label_linear->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(UiStyle::Palette::Text));
    connect(horizontalSlider_linear_, &QSlider::valueChanged,
            [label_linear](qreal value) {
              label_linear->setText(
                  QString::number(value * 0.01, 'f', 2) + " m/s");
            });
    horizontalLayout_21->addWidget(label_linear);
    label_9->setFixedWidth(68);
    label_linear->setMinimumWidth(86);
    label_linear->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    control_layout->addLayout(horizontalLayout_21);
    QHBoxLayout* horizontalLayout_stop_button = new QHBoxLayout();
    QPushButton* btn_stop = new QPushButton();
    btn_stop->setObjectName(QString::fromUtf8("btn_stop"));
    btn_stop->setText("立即停止   Space");
    btn_stop->setStyleSheet(UiStyle::DangerButtonStyleSheet());
    btn_stop->setShortcut(QKeySequence(Qt::Key_Space));
    btn_stop->setAccessibleName(QStringLiteral("立即停止机器人"));
    btn_stop->setAccessibleDescription(
        QStringLiteral("停止当前运动，快捷键为空格键"));
    btn_stop->setMinimumHeight(52);
    btn_stop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(btn_stop, &QPushButton::clicked, this,
            &SpeedCtrlWidget::slotStopControl);
    horizontalLayout_stop_button->addWidget(btn_stop, 1);
    control_layout->addLayout(horizontalLayout_stop_button);
    verticalLayout_speed_ctrl->addWidget(control_card, 0, Qt::AlignTop);

    // QHBoxLayout *horizontalLayout_23 = new QHBoxLayout();
    // horizontalLayout_23->setObjectName(
    //     QString::fromUtf8("horizontalLayout_23"));
    // QSpacerItem *horizontalSpacer_5 =
    //     new QSpacerItem(40, 20, QSizePolicy::Expanding,
    //     QSizePolicy::Minimum);

    // horizontalLayout_23->addItem(horizontalSpacer_5);

    // QSpacerItem *horizontalSpacer_6 =
    //     new QSpacerItem(40, 20, QSizePolicy::Expanding,
    //     QSizePolicy::Minimum);

    // horizontalLayout_23->addItem(horizontalSpacer_6);

    // verticalLayout_speed_ctrl->addLayout(horizontalLayout_23);

    // QSpacerItem *verticalSpacer_4 =
    //     new QSpacerItem(385, 21, QSizePolicy::Minimum,
    //     QSizePolicy::Expanding);

    // verticalLayout_speed_ctrl->addItem(verticalSpacer_4);

    this->setLayout(verticalLayout_speed_ctrl);
  }

  ~SpeedCtrlWidget() {}

  // === 键盘控制 (QWEASDZXC) ===
  static char KeyboardKeyToButtonLabel(char key) {
    switch (key) {
      case 'q': case 'u': return 'u';
      case 'w': case 'i': return 'i';
      case 'e': case 'o': return 'o';
      case 'a': case 'j': return 'j';
      case 'd': case 'l': return 'l';
      case 'z': case 'm': return 'm';
      case 'x': case ',': return ',';
      case 'c': case '.': return '.';
      default: return '\0';
    }
  }

  QPushButton* FindButtonByLabel(char label) {
    if (label == 'u') return move_btn_u_;
    if (label == 'i') return move_btn_i_;
    if (label == 'o') return move_btn_o_;
    if (label == 'j') return move_btn_j_;
    if (label == 'l') return move_btn_l_;
    if (label == 'm') return move_btn_m_;
    if (label == ',') return move_btn_back_;
    if (label == '.') return move_btn_backr_;
    return nullptr;
  }

  void HighlightMoveButton(char keyboard_key) {
    char label = KeyboardKeyToButtonLabel(keyboard_key);
    QPushButton* btn = FindButtonByLabel(label);
    if (btn) btn->setDown(true);
  }

  void ClearMoveHighlight() {
    if (active_keyboard_key_ != '\0') {
      char label = KeyboardKeyToButtonLabel(active_keyboard_key_);
      QPushButton* btn = FindButtonByLabel(label);
      if (btn) btn->setDown(false);
      active_keyboard_key_ = '\0';
    }
  }

  void keyPressEvent(QKeyEvent* event) override {
    const char key = QChar(event->key()).toLower().toLatin1();
    if (event->key() == Qt::Key_Space || key == 's') {
      ClearMoveHighlight();
      slotStopControl();
      return;
    }
    // QWEASDZXC 方向键
    if (active_keyboard_key_ == key) return;  // 防重复
    ClearMoveHighlight();
    MoveBinding binding = {};
    if (LookupMoveBinding(ResolveMoveKey(key), &binding)) {
      active_keyboard_key_ = key;
      HighlightMoveButton(key);
      joystick_active_ = false;
      StartActiveSpeed(RobotSpeed(binding.x * LinearSpeedLimit(),
                                  binding.y * LinearSpeedLimit(),
                                  binding.theta * AngularSpeedLimitRad()));
    }
  }

  void keyReleaseEvent(QKeyEvent* event) override {
    const char key = QChar(event->key()).toLower().toLatin1();
    if (key == active_keyboard_key_) {
      ClearMoveHighlight();
      slotStopControl();
    }
  }
};
