#pragma once
#include <QCalendarWidget>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
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
#include <QTextEdit>
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
  QPushButton* emergency_stop_button_{nullptr};
  RobotSpeed active_speed_;
  bool emergency_stop_engaged_{false};
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
    if (emergency_stop_engaged_) return;
    active_speed_ = speed;
    PublishActiveSpeed();
    if (!command_timer_->isActive()) {
      command_timer_->start();
    }
  }

  bool IsTextEntryFocused() const {
    QWidget* focus = QApplication::focusWidget();
    if (!focus) return false;
    if (qobject_cast<QLineEdit*>(focus) ||
        qobject_cast<QTextEdit*>(focus) ||
        qobject_cast<QPlainTextEdit*>(focus) ||
        qobject_cast<QAbstractSpinBox*>(focus)) {
      return true;
    }
    auto* combo = qobject_cast<QComboBox*>(focus);
    return combo && combo->isEditable();
  }

  void SetEmergencyStop(bool engaged) {
    if (engaged) {
      ClearMoveHighlight();
      slotStopControl();
    }
    if (emergency_stop_engaged_ == engaged) return;
    emergency_stop_engaged_ = engaged;
    emergency_stop_button_->setText(
        engaged ? QStringLiteral("急停已锁定 · 点击解除")
                : QStringLiteral("立即停止    SPACE"));
    emergency_stop_button_->setToolTip(
        engaged ? QStringLiteral("软件急停已锁定；确认现场安全后点击解除")
                : QStringLiteral("锁定软件急停并取消当前运动任务"));
    emit signalEmergencyStopChanged(engaged);
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
  void signalEmergencyStopChanged(bool engaged);
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
    const QSize moveButtonSize(46, 46);
    setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::CheckBoxStyleSheet() + QStringLiteral("QTabWidget::pane { border:1px solid %1; border-radius:10px; background:%2; top:-1px; }"
                                                                                              "QTabBar::tab { padding:7px 18px; color:%3; border:none; background:transparent; }"
                                                                                              "QTabBar::tab:selected { color:%4; font-weight:700; border-bottom:2px solid %4; }"
                                                                                              "QSlider::groove:horizontal { height:5px; border-radius:2px; background:%1; }"
                                                                                              "QSlider::sub-page:horizontal { background:%4; border-radius:2px; }"
                                                                                              "QSlider::handle:horizontal { background:%4; width:14px; height:14px; margin:-5px 0; border-radius:7px; }")
        .arg(UiStyle::Palette::Border, UiStyle::Palette::Surface, UiStyle::Palette::TextMuted, UiStyle::Palette::Primary));
    QVBoxLayout* verticalLayout_speed_ctrl = new QVBoxLayout();
    verticalLayout_speed_ctrl->setContentsMargins(10, 8, 10, 10);
    verticalLayout_speed_ctrl->setSpacing(10);
    verticalLayout_speed_ctrl->setObjectName(
        QString::fromUtf8("verticalLayout_speed_ctrl"));
    QFrame* control_card = new QFrame(this);
    control_card->setObjectName(QStringLiteral("speedControlCard"));
    control_card->setProperty("uiCard", true);
    control_card->setStyleSheet(UiStyle::CardStyleSheet());
    QVBoxLayout* control_layout = new QVBoxLayout(control_card);
    control_layout->setContentsMargins(14, 10, 14, 14);
    control_layout->setSpacing(10);
    QGridLayout* direction_layout = new QGridLayout();
    direction_layout->setObjectName(QString::fromUtf8("directionLayout"));
    direction_layout->setContentsMargins(8, 4, 8, 6);
    direction_layout->setHorizontalSpacing(16);
    direction_layout->setVerticalSpacing(6);
    direction_layout->setAlignment(Qt::AlignCenter);
    direction_layout->setColumnMinimumWidth(0, moveButtonSize.width());
    direction_layout->setColumnMinimumWidth(1, 62);
    direction_layout->setColumnMinimumWidth(2, moveButtonSize.width());
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
    move_btn_u_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-up-left.svg"), QSize(30, 30)));
    move_btn_u_->setIconSize(QSize(30, 30));
    move_btn_u_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_u_, 0, 0, Qt::AlignCenter);

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
    move_btn_i_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-up.svg"), QSize(30, 30)));
    move_btn_i_->setIconSize(QSize(30, 30));
    move_btn_i_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_i_, 0, 1, Qt::AlignCenter);

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
    move_btn_o_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-up-right.svg"), QSize(30, 30)));
    move_btn_o_->setIconSize(QSize(30, 30));
    move_btn_o_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_o_, 0, 2, Qt::AlignCenter);

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
    move_btn_j_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-left.svg"), QSize(30, 30)));
    move_btn_j_->setIconSize(QSize(30, 30));
    move_btn_j_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_j_, 1, 0, Qt::AlignCenter);

    checkBox_use_all_ = new QCheckBox();
    checkBox_use_all_->setObjectName(QString::fromUtf8("checkBox_use_all_"));
    checkBox_use_all_->setFixedSize(QSize(62, 36));
    checkBox_use_all_->setText("全向");
    checkBox_use_all_->setChecked(true);
    checkBox_use_all_->setCursor(Qt::PointingHandCursor);
    checkBox_use_all_->setStyleSheet(UiStyle::CompactCheckBoxStyleSheet());
    direction_layout->addWidget(checkBox_use_all_, 1, 1, Qt::AlignCenter);

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
    move_btn_l_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-right.svg"), QSize(30, 30)));
    move_btn_l_->setIconSize(QSize(30, 30));
    move_btn_l_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_l_, 1, 2, Qt::AlignCenter);

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
    move_btn_m_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-down-left.svg"), QSize(30, 30)));
    move_btn_m_->setIconSize(QSize(30, 30));
    move_btn_m_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_m_, 2, 0, Qt::AlignCenter);

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
    move_btn_back_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-down.svg"), QSize(30, 30)));
    move_btn_back_->setIconSize(QSize(30, 30));
    move_btn_back_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_back_, 2, 1, Qt::AlignCenter);

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
    move_btn_backr_->setIcon(UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/arrow-down-right.svg"), QSize(30, 30)));
    move_btn_backr_->setIconSize(QSize(30, 30));
    move_btn_backr_->setStyleSheet(moveButtonStyle);

    direction_layout->addWidget(move_btn_backr_, 2, 2, Qt::AlignCenter);

    QWidget* cmdCtrlWidget = new QWidget();
    cmdCtrlWidget->setLayout(direction_layout);
    cmdCtrlWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QTabWidget* tabWidget = new QTabWidget;
    tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    tabWidget->addTab(cmdCtrlWidget, "命令控制");
    control_layout->addWidget(tabWidget);

    QWidget* widget_joyStick = new QWidget();
    QHBoxLayout* horizontalLayout_joyStick = new QHBoxLayout();
    joyStick_widget_ = new JoyStick();
    joyStick_widget_->setMinimumSize(QSize(160, 160));
    joyStick_widget_->setMaximumSize(QSize(180, 180));

    connect(joyStick_widget_, &JoyStick::axesChanged, this,
            &SpeedCtrlWidget::slotJoyStickAxes);

    horizontalLayout_joyStick->addStretch();
    horizontalLayout_joyStick->addWidget(joyStick_widget_);
    horizontalLayout_joyStick->addStretch();
    widget_joyStick->setLayout(horizontalLayout_joyStick);

    tabWidget->addTab(widget_joyStick, "摇杆控制");

    QFrame* speed_card = new QFrame(control_card);
    speed_card->setProperty("uiCard", true);
    speed_card->setStyleSheet(UiStyle::CardStyleSheet());
    QVBoxLayout* speed_layout = new QVBoxLayout(speed_card);
    speed_layout->setContentsMargins(14, 12, 14, 14);
    speed_layout->setSpacing(10);
    QLabel* speed_title = new QLabel(QStringLiteral("速度限制"), speed_card);
    speed_title->setStyleSheet(UiStyle::CaptionLabelStyleSheet());
    speed_layout->addWidget(speed_title);

    QHBoxLayout* horizontalLayout_20 = new QHBoxLayout();
    horizontalLayout_20->setObjectName(
        QString::fromUtf8("horizontalLayout_20"));
    QLabel* label_14 = new QLabel();
    label_14->setObjectName(QString::fromUtf8("label_14"));
    label_14->setText("角速度上限");
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
    label_raw->setStyleSheet(QStringLiteral(
        "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:7px; "
        "padding:5px 8px; font-weight:700; }")
        .arg(UiStyle::Palette::Primary, UiStyle::Palette::PrimaryLight,
             UiStyle::Palette::BorderHover));
    connect(horizontalSlider_raw_, &QSlider::valueChanged,
            [label_raw](qreal value) {
              label_raw->setText(QString::number(value, 'f', 2) +
                                 " deg/s");
            });
    horizontalLayout_20->addWidget(label_raw);

    label_14->setMinimumWidth(
        label_14->fontMetrics().horizontalAdvance(label_14->text()) + 16);
    label_14->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    label_raw->setMinimumWidth(94);
    label_raw->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    speed_layout->addLayout(horizontalLayout_20);

    // linear anglur
    QHBoxLayout* horizontalLayout_21 = new QHBoxLayout();
    horizontalLayout_21->setObjectName(
        QString::fromUtf8("horizontalLayout_21"));
    QLabel* label_9 = new QLabel();
    label_9->setObjectName(QString::fromUtf8("label_9"));
    label_9->setText("线速度上限");
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
    label_linear->setStyleSheet(QStringLiteral(
        "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:7px; "
        "padding:5px 8px; font-weight:700; }")
        .arg(UiStyle::Palette::Primary, UiStyle::Palette::PrimaryLight,
             UiStyle::Palette::BorderHover));
    connect(horizontalSlider_linear_, &QSlider::valueChanged,
            [label_linear](qreal value) {
              label_linear->setText(
                  QString::number(value * 0.01, 'f', 2) + " m/s");
            });
    horizontalLayout_21->addWidget(label_linear);
    label_9->setMinimumWidth(
        label_9->fontMetrics().horizontalAdvance(label_9->text()) + 16);
    label_9->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    label_linear->setMinimumWidth(94);
    label_linear->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    speed_layout->addLayout(horizontalLayout_21);
    control_layout->addWidget(speed_card);
    QHBoxLayout* horizontalLayout_stop_button = new QHBoxLayout();
    emergency_stop_button_ = new QPushButton();
    emergency_stop_button_->setObjectName(QString::fromUtf8("btn_stop"));
    emergency_stop_button_->setText("立即停止    SPACE");
    emergency_stop_button_->setStyleSheet(UiStyle::DangerButtonStyleSheet());
    emergency_stop_button_->setToolTip(
        QStringLiteral("锁定软件急停并取消当前运动任务"));
    emergency_stop_button_->setAccessibleName(QStringLiteral("立即停止机器人"));
    emergency_stop_button_->setAccessibleDescription(
        QStringLiteral("空格键锁定软件急停，确认安全后点击按钮解除"));
    emergency_stop_button_->setMinimumHeight(52);
    emergency_stop_button_->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Fixed);
    connect(emergency_stop_button_, &QPushButton::clicked, this, [this]() {
      SetEmergencyStop(!emergency_stop_engaged_);
    });
    horizontalLayout_stop_button->addWidget(emergency_stop_button_, 1);
    control_layout->addLayout(horizontalLayout_stop_button);
    verticalLayout_speed_ctrl->addWidget(control_card, 0, Qt::AlignTop);

    this->setLayout(verticalLayout_speed_ctrl);
    qApp->installEventFilter(this);
  }

  ~SpeedCtrlWidget() override {
    if (qApp) qApp->removeEventFilter(this);
  }

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

  bool HandleKeyPress(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
      if (!event->isAutoRepeat()) SetEmergencyStop(true);
      event->accept();
      return true;
    }
    if (IsTextEntryFocused() ||
        event->modifiers().testFlag(Qt::ControlModifier) ||
        event->modifiers().testFlag(Qt::AltModifier) ||
        event->modifiers().testFlag(Qt::MetaModifier)) {
      return false;
    }
    const char key = QChar(event->key()).toLower().toLatin1();
    if (key == 's') {
      ClearMoveHighlight();
      slotStopControl();
      event->accept();
      return true;
    }
    // QWEASDZXC 方向键
    if (event->isAutoRepeat() || active_keyboard_key_ == key) {
      return active_keyboard_key_ == key;
    }
    ClearMoveHighlight();
    MoveBinding binding = {};
    if (LookupMoveBinding(ResolveMoveKey(key), &binding)) {
      active_keyboard_key_ = key;
      HighlightMoveButton(key);
      joystick_active_ = false;
      StartActiveSpeed(RobotSpeed(binding.x * LinearSpeedLimit(),
                                  binding.y * LinearSpeedLimit(),
                                  binding.theta * AngularSpeedLimitRad()));
      event->accept();
      return true;
    }
    return false;
  }

  bool HandleKeyRelease(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
      event->accept();
      return true;
    }
    const char key = QChar(event->key()).toLower().toLatin1();
    if (key == active_keyboard_key_) {
      if (event->isAutoRepeat()) return true;
      ClearMoveHighlight();
      slotStopControl();
      event->accept();
      return true;
    }
    return false;
  }

  void keyPressEvent(QKeyEvent* event) override {
    if (!HandleKeyPress(event)) QWidget::keyPressEvent(event);
  }

  void keyReleaseEvent(QKeyEvent* event) override {
    if (!HandleKeyRelease(event)) QWidget::keyReleaseEvent(event);
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    Q_UNUSED(watched);
    if (event->type() == QEvent::KeyPress) {
      return HandleKeyPress(static_cast<QKeyEvent*>(event));
    }
    if (event->type() == QEvent::KeyRelease) {
      return HandleKeyRelease(static_cast<QKeyEvent*>(event));
    }
    if (event->type() == QEvent::ApplicationDeactivate &&
        active_keyboard_key_ != '\0') {
      ClearMoveHighlight();
      slotStopControl();
    }
    return QWidget::eventFilter(watched, event);
  }
};
