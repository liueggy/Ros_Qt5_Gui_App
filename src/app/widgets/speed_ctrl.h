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
  double joystick_x_{0.0};
  double joystick_y_{0.0};
  bool joystick_active_{false};

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
      default:
        return '\0';
    }
  }

  bool LookupMoveBinding(char key, MoveBinding* binding) const {
    static constexpr std::array<MoveBinding, 16> kMoveBindings{{
        {'i', 1, 0, 0}, {'o', 1, 0, -1}, {'j', 0, 0, 1},
        {'l', 0, 0, -1}, {'u', 1, 0, 1}, {',', -1, 0, 0},
        {'.', -1, 0, 1}, {'m', -1, 0, -1}, {'O', 1, -1, 0},
        {'I', 1, 0, 0}, {'J', 0, 1, 0}, {'L', 0, -1, 0},
        {'U', 1, 1, 0}, {'<', -1, 0, 0}, {'>', -1, -1, 0},
        {'M', -1, 1, 0},
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
    if (!btn || btn->text().isEmpty()) {
      return;
    }
    MoveBinding binding{};
    if (!LookupMoveBinding(ResolveMoveKey(btn->text().toStdString()[0]), &binding)) {
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
    MoveBinding binding{};
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
    command_timer_->setInterval(50);
    connect(command_timer_, &QTimer::timeout, this,
            [this]() {
              if (joystick_active_) {
                UpdateJoystickSpeed();
              }
              PublishActiveSpeed();
            });
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    const QString moveButtonStyle = QStringLiteral(
        "QPushButton { background:#fbfdff; border:1px solid #dce6f5; border-radius:12px; color:transparent; }"
        "QPushButton:hover { background:#eef5ff; border-color:#bcd3fb; }"
        "QPushButton:pressed { background:#dbeafe; border-color:#2f6fed; }");
    const QSize moveButtonSize(56, 56);
    setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::CheckBoxStyleSheet() + QStringLiteral("QTabWidget::pane { border:1px solid #e5ebf3; border-radius:10px; background:#ffffff; top:-1px; }"
                                                                                              "QTabBar::tab { padding:7px 18px; color:#4b5563; border:none; background:transparent; }"
                                                                                              "QTabBar::tab:selected { color:#2f6fed; font-weight:700; border-bottom:2px solid #2f6fed; }"
                                                                                              "QSlider::groove:horizontal { height:5px; border-radius:2px; background:#e5eaf2; }"
                                                                                              "QSlider::sub-page:horizontal { background:#2f6fed; border-radius:2px; }"
                                                                                              "QSlider::handle:horizontal { background:#2f6fed; width:14px; height:14px; margin:-5px 0; border-radius:7px; }"));
    QVBoxLayout* verticalLayout_speed_ctrl = new QVBoxLayout();
    verticalLayout_speed_ctrl->setContentsMargins(10, 10, 10, 10);
    verticalLayout_speed_ctrl->setSpacing(8);
    verticalLayout_speed_ctrl->setObjectName(
        QString::fromUtf8("verticalLayout_speed_ctrl"));
    QFrame* control_card = new QFrame(this);
    control_card->setObjectName(QStringLiteral("speedControlCard"));
    control_card->setStyleSheet(QStringLiteral(
        "QFrame#speedControlCard { background:#ffffff; border:1px solid #e1e7f0; "
        "border-radius:12px; }"));
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
    QPushButton* pushButton_u = new QPushButton();
    pushButton_u->setObjectName(QString::fromUtf8("pushButton_u"));
    pushButton_u->setText("u");
    pushButton_u->setShortcut(QApplication::translate("Widget", "u", nullptr));
    pushButton_u->setMinimumSize(moveButtonSize);
    pushButton_u->setMaximumSize(moveButtonSize);
    pushButton_u->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up_left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_left_2.png);}"));
    pushButton_u->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up-left.svg")));
    pushButton_u->setIconSize(QSize(30, 30));
    pushButton_u->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(pushButton_u);

    QPushButton* pushButton_i = new QPushButton();
    pushButton_i->setObjectName(QString::fromUtf8("pushButton_i"));
    pushButton_i->setText("i");
    pushButton_i->setShortcut(QApplication::translate("Widget", "i", nullptr));
    pushButton_i->setMinimumSize(moveButtonSize);
    pushButton_i->setMaximumSize(moveButtonSize);
    pushButton_i->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_2.png);}"));
    pushButton_i->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up.svg")));
    pushButton_i->setIconSize(QSize(30, 30));
    pushButton_i->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(pushButton_i);

    QPushButton* pushButton_o = new QPushButton();
    pushButton_o->setObjectName(QString::fromUtf8("pushButton_o"));
    pushButton_o->setText("o");
    pushButton_o->setShortcut(QApplication::translate("Widget", "o", nullptr));
    pushButton_o->setMinimumSize(moveButtonSize);
    pushButton_o->setMaximumSize(moveButtonSize);
    pushButton_o->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/up_right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/up_right_2.png);}"));
    pushButton_o->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-up-right.svg")));
    pushButton_o->setIconSize(QSize(30, 30));
    pushButton_o->setStyleSheet(moveButtonStyle);

    horizontalLayout_2->addWidget(pushButton_o);

    verticalLayout_cmd_btn->addLayout(horizontalLayout_2);

    QHBoxLayout* horizontalLayout_18 = new QHBoxLayout();
    horizontalLayout_18->setObjectName(
        QString::fromUtf8("horizontalLayout_18"));
    horizontalLayout_18->setSpacing(42);
    horizontalLayout_18->setAlignment(Qt::AlignCenter);
    QPushButton* pushButton_j = new QPushButton();
    pushButton_j->setText("j");
    pushButton_j->setShortcut(QApplication::translate("Widget", "j", nullptr));
    pushButton_j->setObjectName(QString::fromUtf8("pushButton_j"));
    pushButton_j->setMinimumSize(moveButtonSize);
    pushButton_j->setMaximumSize(moveButtonSize);
    pushButton_j->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/left_2.png);}"));
    pushButton_j->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-left.svg")));
    pushButton_j->setIconSize(QSize(30, 30));
    pushButton_j->setStyleSheet(moveButtonStyle);

    horizontalLayout_18->addWidget(pushButton_j);

    checkBox_use_all_ = new QCheckBox();
    checkBox_use_all_->setObjectName(QString::fromUtf8("checkBox_use_all_"));
    checkBox_use_all_->setMinimumSize(QSize(78, 36));
    checkBox_use_all_->setMaximumSize(QSize(90, 36));
    checkBox_use_all_->setText("全向");
    checkBox_use_all_->setCursor(Qt::PointingHandCursor);
    checkBox_use_all_->setStyleSheet(QStringLiteral(
                                         "QCheckBox { color:#536277; font-size:%1px; font-weight:700; spacing:6px; "
                                         "background:#f6f9fe; border:1px solid #dbe6f5; border-radius:10px; padding:7px 10px; }"
                                         "QCheckBox:hover { background:#edf4ff; border-color:#bcd3fb; color:#1f5fbf; }"
                                         "QCheckBox:checked { background:#e8f1ff; border-color:#2f6fed; color:#1f5fbf; }"
                                         "QCheckBox::indicator { width:14px; height:14px; border:1px solid #c8d4e4; border-radius:4px; background:#ffffff; }"
                                         "QCheckBox::indicator:checked { background:#2f6fed; border-color:#2f6fed; }")
                                         .arg(UiStyle::FontSmallPx()));
    horizontalLayout_18->addWidget(checkBox_use_all_);

    QPushButton* pushButton_l = new QPushButton();
    pushButton_l->setObjectName(QString::fromUtf8("pushButton_l"));
    pushButton_l->setText("l");
    pushButton_l->setShortcut(QApplication::translate("Widget", "l", nullptr));
    pushButton_l->setMinimumSize(moveButtonSize);
    pushButton_l->setMaximumSize(moveButtonSize);
    pushButton_l->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/right_2.png);}"));
    pushButton_l->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-right.svg")));
    pushButton_l->setIconSize(QSize(30, 30));
    pushButton_l->setStyleSheet(moveButtonStyle);

    horizontalLayout_18->addWidget(pushButton_l);

    verticalLayout_cmd_btn->addLayout(horizontalLayout_18);

    QHBoxLayout* horizontalLayout_19 = new QHBoxLayout();
    horizontalLayout_19->setObjectName(
        QString::fromUtf8("horizontalLayout_19"));
    horizontalLayout_19->setSpacing(42);
    horizontalLayout_19->setAlignment(Qt::AlignCenter);
    QPushButton* pushButton_m = new QPushButton();
    pushButton_m->setObjectName(QString::fromUtf8("pushButton_m"));
    pushButton_m->setText("m");
    pushButton_m->setShortcut(QApplication::translate("Widget", "m", nullptr));
    pushButton_m->setMinimumSize(moveButtonSize);
    pushButton_m->setMaximumSize(moveButtonSize);
    pushButton_m->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down_left.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_left_2.png);}"));
    pushButton_m->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down-left.svg")));
    pushButton_m->setIconSize(QSize(30, 30));
    pushButton_m->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(pushButton_m);

    QPushButton* pushButton_back = new QPushButton();
    pushButton_back->setObjectName(QString::fromUtf8("pushButton_,"));
    pushButton_back->setText(",");
    pushButton_back->setShortcut(
        QApplication::translate("Widget", ",", nullptr));
    pushButton_back->setMinimumSize(moveButtonSize);
    pushButton_back->setMaximumSize(moveButtonSize);
    pushButton_back->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_2.png);}"));
    pushButton_back->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down.svg")));
    pushButton_back->setIconSize(QSize(30, 30));
    pushButton_back->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(pushButton_back);

    QPushButton* pushButton_backr = new QPushButton();
    pushButton_backr->setObjectName(QString::fromUtf8("pushButton_."));
    pushButton_backr->setText(".");
    pushButton_backr->setShortcut(
        QApplication::translate("Widget", ".", nullptr));
    pushButton_backr->setMinimumSize(moveButtonSize);
    pushButton_backr->setMaximumSize(moveButtonSize);
    pushButton_backr->setStyleSheet(QString::fromUtf8(
        "QPushButton{border-image: url(://images/down_right.png);}\n"
        "QPushButton{border:none;}\n"
        "QPushButton:pressed{border-image: url(://images/down_right_2.png);}"));

    const QList<QPushButton*> move_buttons{
        pushButton_i, pushButton_u, pushButton_o, pushButton_j,
        pushButton_l, pushButton_m, pushButton_back, pushButton_backr};
    for (auto* button : move_buttons) {
      connect(button, &QPushButton::pressed, this,
              &SpeedCtrlWidget::slotSpeedControl);
      connect(button, &QPushButton::released, this,
              &SpeedCtrlWidget::slotStopControl);
    }
    pushButton_backr->setIcon(QIcon(QStringLiteral(":/icons/tabler/arrow-down-right.svg")));
    pushButton_backr->setIconSize(QSize(30, 30));
    pushButton_backr->setStyleSheet(moveButtonStyle);

    horizontalLayout_19->addWidget(pushButton_backr);

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
    label_14->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#435267")));
    horizontalLayout_20->addWidget(label_14);

    horizontalSlider_raw_ = new QSlider();
    horizontalSlider_raw_->setObjectName(
        QString::fromUtf8("horizontalSlider_raw_"));
    horizontalSlider_raw_->setMaximum(90);
    horizontalSlider_raw_->setValue(30);
    horizontalSlider_raw_->setOrientation(Qt::Horizontal);

    horizontalLayout_20->addWidget(horizontalSlider_raw_);

    QLabel* label_raw = new QLabel();
    label_raw->setObjectName(QString::fromUtf8("label_raw"));
    label_raw->setText(QString::number(horizontalSlider_raw_->value(), 'f', 2) +
                       " deg/s");
    label_raw->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#18212f")));
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
    label_9->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#435267")));
    horizontalLayout_21->addWidget(label_9);

    horizontalSlider_linear_ = new QSlider();
    horizontalSlider_linear_->setObjectName(
        QString::fromUtf8("horizontalSlider_linear_"));
    horizontalSlider_linear_->setMaximum(100);
    horizontalSlider_linear_->setSingleStep(1);
    horizontalSlider_linear_->setValue(20);
    horizontalSlider_linear_->setOrientation(Qt::Horizontal);

    horizontalLayout_21->addWidget(horizontalSlider_linear_);

    QLabel* label_linear = new QLabel();
    label_linear->setObjectName(QString::fromUtf8("label_linear"));
    label_linear->setText(
        QString::number(horizontalSlider_linear_->value() * 0.01, 'f', 2) +
        " m/s");
    label_linear->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#18212f")));
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
    btn_stop->setText("停止(s)");
    btn_stop->setStyleSheet(UiStyle::DangerButtonStyleSheet());
    btn_stop->setShortcut(QApplication::translate("Widget", "s", nullptr));
    connect(btn_stop, &QPushButton::clicked, this,
            &SpeedCtrlWidget::slotStopControl);
    horizontalLayout_stop_button->addStretch();
    horizontalLayout_stop_button->addWidget(btn_stop);
    horizontalLayout_stop_button->addStretch();
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
};
