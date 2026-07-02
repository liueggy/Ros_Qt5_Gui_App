/*
 * @Author: chengyangkj chengyangkj@qq.com
 * @Date: 2023-10-06 07:12:50
 * @LastEditors: chengyangkj chengyangkj@qq.com
 * @LastEditTime: 2023-10-06 14:02:27
 * @FilePath: /ROS2_Qt5_Gui_App/src/ MainWindow.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置
 * 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include "mainwindow.h"
#include <QApplication>
#include <QButtonGroup>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScreen>
#include <QSplitter>
#include <QStyle>
#include <iostream>
#include <numeric>
#include <opencv2/opencv.hpp>
#include "AutoHideDockContainer.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "Eigen/Dense"
#include "FloatingDockContainer.h"
#include "algorithm.h"
#include "config/config_manager.h"
#include "logger/logger.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include "display/manager/view_manager.h"
#include "msg/diagnostic_snapshot.h"
#include "widgets/command_center_widget.h"
#include "widgets/display_config_widget.h"
#include "widgets/speed_ctrl.h"
#include "widgets/ui_style.h"
using namespace ads;
namespace {

constexpr int kUiLayoutVersion = 8;

void ConfigureDockWidget(ads::CDockWidget* dock, const QSize& minimum_size,
                         const QSize& preferred_size = QSize()) {
  if (!dock) {
    return;
  }
  dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
  dock->setMinimumSize(minimum_size);
  dock->setProperty("preferredDockSize", preferred_size.isValid() ? preferred_size : minimum_size);
}

}  // namespace
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  Q_INIT_RESOURCE(images);
  Q_INIT_RESOURCE(media);
  Config::ConfigManager::Instance();
  LOG_INFO(" MainWindow init thread id" << QThread::currentThreadId());
  qRegisterMetaType<std::string>("std::string");
  qRegisterMetaType<RobotPose>("RobotPose");
  qRegisterMetaType<RobotSpeed>("RobotSpeed");
  qRegisterMetaType<RobotState>("RobotState");
  qRegisterMetaType<OccupancyMap>("OccupancyMap");
  qRegisterMetaType<OccupancyMap>("OccupancyMap");
  qRegisterMetaType<LaserScan>("LaserScan");
  qRegisterMetaType<RobotPath>("RobotPath");
  qRegisterMetaType<MsgId>("MsgId");
  qRegisterMetaType<std::any>("std::any");
  qRegisterMetaType<TopologyMap>("TopologyMap");
  qRegisterMetaType<TopologyMap::PointInfo>("TopologyMap::PointInfo");
  setupUi();
  ApplyCenteredWindowGeometry();
  QTimer::singleShot(30, this, [this]() { openChannel(); });
  QTimer::singleShot(50, [=]() {
    RestoreState();
    std::string map_path = Config::ConfigManager::Instance()->GetRootConfig().map_config.path;
    if (!map_path.empty()) {
      std::string yaml_path = map_path;
      if (yaml_path.find(".yaml") == std::string::npos && yaml_path.find(".yml") == std::string::npos) {
        yaml_path += ".yaml";
      }
      if (QFile::exists(QString::fromStdString(yaml_path))) {
        LoadMap(yaml_path);
      }
    }
  });
}
bool MainWindow::openChannel() {
  const int attempt_id = ++connection_attempt_id_;
  if (display_config_widget_) {
    display_config_widget_->SetConnectionState(
        false, true, tr("正在检测小车连接，请稍候…"));
  }
  if (channel_manager_.OpenChannelAuto()) {
    if (!channel_subscriptions_registered_) {
      registerChannel();
      channel_subscriptions_registered_ = true;
    }

    // 延迟检查连接状态（连接超时是5秒）
    auto* channel = channel_manager_.GetChannel();
    if (channel) {
      QTimer::singleShot(6000, this, [this, attempt_id]() {
        if (attempt_id != connection_attempt_id_) {
          return;
        }
        auto* channel = channel_manager_.GetChannel();
        if (!channel) {
          display_config_widget_->SetConnectionState(
              false, false, tr("未建立连接，可在小车启动后重试。"));
          return;
        }
        if (channel->IsConnectionFailed()) {
          std::string error_msg = channel->GetConnectionError();
          std::string channel_name = channel->Name();
          if (channel_name == "ROSBridge") {
            QString display_error = error_msg.empty()
                                        ? "无法连接到 ROSBridge 服务器。"
                                        : QString::fromStdString(error_msg);
            display_config_widget_->SetConnectionState(
                false, false,
                tr("小车当前离线：%1。启动小车后再点击“连接”。").arg(display_error));
            LOG_ERROR("ROSBridge connection failed: " << error_msg);
          } else {
            display_config_widget_->SetConnectionState(
                false, false,
                error_msg.empty()
                    ? tr("小车当前离线，请启动相关服务后重试。")
                    : QString::fromStdString(error_msg));
            LOG_ERROR("Channel " << channel_name << " connection failed: " << error_msg);
            if (!error_msg.empty()) {
              QMessageBox::critical(this,
                                    QString::fromStdString(channel_name) + " 连接失败",
                                    QString::fromStdString(error_msg),
                                    QMessageBox::Ok);
            } else {
              QMessageBox::critical(this,
                                    QString::fromStdString(channel_name) + " 连接失败",
                                    "无法连接到 " + QString::fromStdString(channel_name) +
                                        " 服务器。\n\n请检查：\n"
                                        "1. 服务器是否正在运行\n"
                                        "2. 配置是否正确\n"
                                        "3. 网络连接是否正常",
                                    QMessageBox::Ok);
            }
          }
        } else {
          display_config_widget_->SetConnectionState(
              true, false, tr("已连接到小车，ROSBridge 通信正常。"));
        }
      });
    }

    return true;
  }
  if (display_config_widget_) {
    display_config_widget_->SetConnectionState(
        false, false, tr("连接组件启动失败，请检查通道配置。"));
  }
  return false;
}
bool MainWindow::openChannel(const std::string& channel_name) {
  if (channel_manager_.OpenChannel(channel_name)) {
    registerChannel();
    return true;
  }
  return false;
}
void MainWindow::registerChannel() {
  SUBSCRIBE(MSG_ID_ODOM_POSE, [this](const RobotState& data) {
    updateOdomInfo(data);
  });

  SUBSCRIBE(MSG_ID_ROBOT_POSE, [this](const RobotPose& robot_pose) {
    nav_goal_table_view_->UpdateRobotPose(robot_pose);
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->UpdateRobotPos("机器人: (" + QString::number(robot_pose.x, 'f', 2) + ", " +
                                   QString::number(robot_pose.y, 'f', 2) + ", " +
                                   QString::number(robot_pose.theta, 'f', 2) + ")");
    }
  });

  SUBSCRIBE(MSG_ID_BATTERY_STATE, [this](const std::map<std::string, std::string>& map) {
    this->SlotSetBatteryStatus(std::stod(map.at("percent")),
                               std::stod(map.at("voltage")));
  });

  SUBSCRIBE(MSG_ID_IMAGE, [this](const std::pair<std::string, std::shared_ptr<cv::Mat>>& location_to_mat) {
    this->SlotRecvImage(location_to_mat.first, location_to_mat.second);
  });

  SUBSCRIBE(MSG_ID_DIAGNOSTIC, [this](const basic::DiagnosticSnapshot& snap) {
    if (command_center_widget_) {
      command_center_widget_->SetDiagnosticSnapshot(snap);
    }
  });

  SUBSCRIBE(MSG_ID_DHT11_TEMP, [this](const double& temp) {
    if (label_dht11_temp_) {
      label_dht11_temp_->setText(QString::number(temp, 'f', 1) + " °C");
    }
  });

  SUBSCRIBE(MSG_ID_DHT11_HUMI, [this](const double& humi) {
    if (label_dht11_humi_) {
      label_dht11_humi_->setText(QString::number(humi, 'f', 1) + " %");
    }
  });

  SUBSCRIBE(MSG_ID_VOICE_COMMAND, [this](const std::string& json_str) {
    if (label_voice_cmd_) {
      // 解析 {"func":"00","cmd":"04"} 显示为友好文本
      QString display = QString::fromStdString(json_str);
      label_voice_cmd_->setText("\U0001F50A " + display);
      label_voice_cmd_->setVisible(true);
      voice_clear_timer_->start(5000);  // 5秒后自动隐藏
    }
  });
}

void MainWindow::RecvChannelMsg(const MsgId& id, const std::any& data) {
  // 保留此方法以兼容现有代码，但不再使用
  // 数据现在通过 message_bus 订阅接收
}

void MainWindow::SlotRecvImage(const std::string& location, std::shared_ptr<cv::Mat> data) {
  if (image_frame_map_.count(location)) {
    QImage image(data->data, data->cols, data->rows, data->step[0], QImage::Format_RGB888);
    image_frame_map_[location]->setImage(image);
  }
}
void MainWindow::closeChannel() {
  ++connection_attempt_id_;
  channel_manager_.CloseChannel();
  if (display_config_widget_) {
    display_config_widget_->SetConnectionState(
        false, false, tr("连接已断开，可在小车启动后重新连接。"));
  }
}
MainWindow::~MainWindow() { delete ui; }
void MainWindow::setupUi() {
  ui->setupUi(this);
  setWindowFlags((windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowTitleHint);
  setAttribute(Qt::WA_TranslucentBackground, false);
  setMinimumSize(960, 640);

  this->setFont(QApplication::font());

  CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
  CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
  CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
  CDockManager::setConfigFlag(CDockManager::DockAreaHasUndockButton, false);
  CDockManager::setConfigFlag(CDockManager::DockAreaHasTabsMenuButton, false);
  CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, true);
  CDockManager::setConfigFlag(CDockManager::EqualSplitOnInsertion, true);
  CDockManager::setConfigFlag(CDockManager::ShowTabTextOnlyForActiveTab, true);
  CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);
  dock_manager_ = new CDockManager(this);
  QVBoxLayout* center_layout = new QVBoxLayout();    // 垂直
  QHBoxLayout* center_h_layout = new QHBoxLayout();  // 水平

  const QString window_ctrl_btn_style = UiStyle::WindowControlButtonStyleSheet();

  QWidget* tools_strip = new QWidget();
  custom_title_bar_ = tools_strip;
  tools_strip->installEventFilter(this);
  tools_strip->setStyleSheet(R"(
    QWidget {
      background-color: #ffffff;
      border-bottom: 1px solid #dce4ef;
    }
  )");

  ///////////////////////////////////////////////////////////////地图工具栏
  QHBoxLayout* horizontalLayout_tools = new QHBoxLayout(tools_strip);
  horizontalLayout_tools->setSpacing(8);
  horizontalLayout_tools->setContentsMargins(12, 6, 10, 6);
  horizontalLayout_tools->setObjectName(
      QString::fromUtf8(" horizontalLayout_tools"));

  auto* brand_icon = new QLabel(tools_strip);
  brand_icon->setPixmap(QIcon(QStringLiteral(":/icons/tabler/plug-connected.svg")).pixmap(24, 24));
  brand_icon->setFixedSize(28, 28);
  brand_icon->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(brand_icon);

  auto* brand_title = new QLabel(tr("ROS Bridge 控制台"), tools_strip);
  brand_title->setStyleSheet(QStringLiteral("QLabel { color:#18212f; font-size:15px; font-weight:700; padding-right:16px; }"));
  horizontalLayout_tools->addWidget(brand_title);
  // 现代化工具栏样式
  QString modernToolButtonStyle = UiStyle::ToolButtonStyleSheet();

  // 添加 "view" 菜单按钮
  QToolButton* view_menu_btn = new QToolButton();
  QIcon view_icon;
  view_icon.addFile(QString::fromUtf8(":/icons/tabler/menu-2.svg"),
                    QSize(32, 32), QIcon::Normal, QIcon::Off);
  view_menu_btn->setIcon(view_icon);
  view_menu_btn->setIconSize(QSize(22, 22));
  view_menu_btn->setPopupMode(QToolButton::InstantPopup);
  view_menu_btn->setMenu(ui->menuView);
  view_menu_btn->setStyleSheet(modernToolButtonStyle);
  view_menu_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  horizontalLayout_tools->addWidget(view_menu_btn);

  // 隐藏默认菜单栏
  menuBar()->setVisible(false);

  QToolButton* reloc_btn = new QToolButton();
  reloc_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  reloc_btn->setStyleSheet(modernToolButtonStyle);

  QIcon icon4;
  icon4.addFile(QString::fromUtf8(":/icons/tabler/map-pin.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  reloc_btn->setIcon(icon4);
  reloc_btn->setText("重定位");
  reloc_btn->setIconSize(QSize(22, 22));
  horizontalLayout_tools->addWidget(reloc_btn);

  QIcon icon5;
  icon5.addFile(QString::fromUtf8(":/icons/tabler/pointer.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* edit_map_btn = new QToolButton();
  edit_map_btn->setIcon(icon5);
  edit_map_btn->setText("编辑地图");
  edit_map_btn->setIconSize(QSize(22, 22));
  edit_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  edit_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(edit_map_btn);

  QIcon icon6;
  icon6.addFile(QString::fromUtf8(":/icons/tabler/folder-open.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* open_map_btn = new QToolButton();
  open_map_btn->setIcon(icon6);
  open_map_btn->setText("打开地图");
  open_map_btn->setIconSize(QSize(22, 22));
  open_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  open_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(open_map_btn);

  QIcon icon8;
  icon8.addFile(QString::fromUtf8(":/icons/tabler/device-floppy.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);

  QToolButton* save_map_btn = new QToolButton();
  save_map_btn->setIcon(icon8);
  save_map_btn->setText("保存地图");
  save_map_btn->setIconSize(QSize(22, 22));
  save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(save_map_btn);

  QIcon icon7;
  icon7.addFile(QString::fromUtf8(":/icons/tabler/device-floppy.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* re_save_map_btn = new QToolButton();
  re_save_map_btn->setIcon(icon7);
  re_save_map_btn->setText("另存为");
  re_save_map_btn->setIconSize(QSize(22, 22));
  re_save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  re_save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(re_save_map_btn);
  center_layout->addWidget(tools_strip);

  horizontalLayout_tools->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  ///////////////////////////////////////////////////////////////////电池电量 - 现代化设计
  battery_bar_ = new QProgressBar();
  battery_bar_->setObjectName(QString::fromUtf8("battery_bar_"));
  battery_bar_->setMaximumSize(QSize(130, 28));
  battery_bar_->setAutoFillBackground(true);
  battery_bar_->setStyleSheet(QStringLiteral(
                                  "QProgressBar#battery_bar_ { border:2px solid #dadce0; border-radius:12px; background:#f1f4f8; "
                                  "text-align:center; color:#202124; font-size:%1px; font-weight:600; }"
                                  "QProgressBar::chunk { background:qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                  "stop:0 #34a853, stop:0.5 #7cb342, stop:1 #34a853); border-radius:10px; margin:2px; }")
                                  .arg(UiStyle::FontBasePx()));

  battery_bar_->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(battery_bar_);

  QLabel* label_11 = new QLabel();
  label_11->setObjectName(QString::fromUtf8("label_11"));
  label_11->setMinimumSize(QSize(24, 24));
  label_11->setMaximumSize(QSize(24, 24));
  label_11->setPixmap(QPixmap(QString::fromUtf8(":/images/power-v.png")));
  horizontalLayout_tools->addWidget(label_11);

  label_power_ = new QLabel();
  label_power_->setObjectName(QString::fromUtf8("label_power_"));
  label_power_->setMinimumSize(QSize(60, 28));
  label_power_->setMaximumSize(QSize(60, 28));
  label_power_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());
  horizontalLayout_tools->addWidget(label_power_);

  // 温湿度显示
  horizontalLayout_tools->addSpacing(16);
  QLabel* dht_icon = new QLabel(this);
  dht_icon->setPixmap(QIcon(QStringLiteral(":/icons/tabler/temperature.svg")).pixmap(20, 20));
  dht_icon->setFixedSize(24, 24);
  dht_icon->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(dht_icon);

  label_dht11_temp_ = new QLabel(QStringLiteral("--.- °C"), this);
  label_dht11_temp_->setMinimumSize(QSize(70, 28));
  label_dht11_temp_->setMaximumSize(QSize(70, 28));
  label_dht11_temp_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#d93025")));
  horizontalLayout_tools->addWidget(label_dht11_temp_);

  label_dht11_humi_ = new QLabel(QStringLiteral("--.- %"), this);
  label_dht11_humi_->setMinimumSize(QSize(65, 28));
  label_dht11_humi_->setMaximumSize(QSize(65, 28));
  label_dht11_humi_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet(QStringLiteral("#1a73e8")));
  horizontalLayout_tools->addWidget(label_dht11_humi_);

  // 语音命令提示
  horizontalLayout_tools->addSpacing(8);
  label_voice_cmd_ = new QLabel(this);
  label_voice_cmd_->setMinimumSize(QSize(0, 24));
  label_voice_cmd_->setMaximumSize(QSize(220, 24));
  label_voice_cmd_->setStyleSheet(UiStyle::VoiceStatusLabelStyleSheet());
  label_voice_cmd_->setVisible(false);
  horizontalLayout_tools->addWidget(label_voice_cmd_);

  voice_clear_timer_ = new QTimer(this);
  voice_clear_timer_->setSingleShot(true);
  connect(voice_clear_timer_, &QTimer::timeout, [this]() {
    label_voice_cmd_->setVisible(false);
  });

  horizontalLayout_tools->addSpacing(12);
  QPushButton* min_btn = new QPushButton(this);
  maximize_button_ = new QPushButton(this);
  QPushButton* close_btn = new QPushButton(this);
  min_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
  close_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  min_btn->setToolTip(tr("最小化"));
  maximize_button_->setToolTip(tr("最大化"));
  close_btn->setToolTip(tr("关闭"));
  for (auto* button : {min_btn, maximize_button_, close_btn}) {
    button->setFixedSize(36, 32);
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
  }
  min_btn->setStyleSheet(window_ctrl_btn_style);
  maximize_button_->setStyleSheet(window_ctrl_btn_style);
  close_btn->setStyleSheet(window_ctrl_btn_style +
                           "\nQPushButton:hover { background-color: #ef5350; color: white; "
                           "border-color: #ef5350; }");
  connect(min_btn, &QPushButton::clicked, this, &QWidget::showMinimized);
  connect(maximize_button_, &QPushButton::clicked, [this]() {
    if (isMaximized()) {
      showNormal();
    } else {
      showMaximized();
    }
    UpdateMaximizeButton();
  });
  connect(close_btn, &QPushButton::clicked, this, &QWidget::close);
  horizontalLayout_tools->addWidget(min_btn);
  horizontalLayout_tools->addWidget(maximize_button_);
  horizontalLayout_tools->addWidget(close_btn);
  UpdateMaximizeButton();

  SlotSetBatteryStatus(0, 0);

  //////////////////////////////////////////////////////////////编辑地图工具栏 - 现代化设计
  QWidget* tools_edit_map_widget = new QWidget();
  tools_edit_map_widget->setStyleSheet(R"(
    QWidget {
      background-color: #ffffff;
      border: 1px solid #dce4ef;
      border-radius: 8px;
    }
  )");

  QVBoxLayout* layout_tools_edit_map = new QVBoxLayout();
  tools_edit_map_widget->setLayout(layout_tools_edit_map);
  layout_tools_edit_map->setSpacing(4);
  layout_tools_edit_map->setContentsMargins(8, 8, 8, 8);
  layout_tools_edit_map->setObjectName(
      QString::fromUtf8(" layout_tools_edit_map"));

  // 现代化编辑工具按钮样式
  QString modernEditButtonStyle = UiStyle::MiniToolButtonStyleSheet();

  // 地图编辑 设置鼠标按钮
  QToolButton* normal_cursor_btn = new QToolButton();
  normal_cursor_btn->setCheckable(true);
  normal_cursor_btn->setStyleSheet(modernEditButtonStyle);
  normal_cursor_btn->setToolTip("鼠标");
  normal_cursor_btn->setCursor(Qt::PointingHandCursor);
  normal_cursor_btn->setIconSize(QSize(24, 24));

  QIcon pose_tool_btn_icon;
  pose_tool_btn_icon.addFile(QString::fromUtf8(":/images/cursor_point_btn.svg"),
                             QSize(), QIcon::Normal, QIcon::Off);
  normal_cursor_btn->setIcon(pose_tool_btn_icon);
  layout_tools_edit_map->addWidget(normal_cursor_btn);

  // 添加点位按钮
  QToolButton* add_point_btn = new QToolButton();
  add_point_btn->setCheckable(true);
  add_point_btn->setStyleSheet(modernEditButtonStyle);
  add_point_btn->setToolTip("添加工位点");
  add_point_btn->setCursor(Qt::PointingHandCursor);
  add_point_btn->setIconSize(QSize(24, 24));

  QIcon add_point_btn_icon;
  add_point_btn_icon.addFile(QString::fromUtf8(":/images/point_btn.svg"),
                             QSize(), QIcon::Normal, QIcon::Off);
  add_point_btn->setIcon(add_point_btn_icon);
  layout_tools_edit_map->addWidget(add_point_btn);

  QToolButton* add_topology_path_btn = new QToolButton();
  add_topology_path_btn->setCheckable(true);
  add_topology_path_btn->setStyleSheet(modernEditButtonStyle);
  add_topology_path_btn->setToolTip("连接工位点");
  add_topology_path_btn->setCursor(Qt::PointingHandCursor);
  add_topology_path_btn->setIconSize(QSize(24, 24));

  QIcon add_topology_path_btn_icon;
  add_topology_path_btn_icon.addFile(QString::fromUtf8(":/images/topo_link_btn.svg"),
                                     QSize(), QIcon::Normal, QIcon::Off);
  add_topology_path_btn->setIcon(add_topology_path_btn_icon);
  layout_tools_edit_map->addWidget(add_topology_path_btn);
  add_topology_path_btn->setEnabled(true);

  // 添加区域按钮
  QToolButton* add_region_btn = new QToolButton();
  add_region_btn->setCheckable(true);
  add_region_btn->setStyleSheet(modernEditButtonStyle);
  add_region_btn->setToolTip("添加区域");
  add_region_btn->setCursor(Qt::PointingHandCursor);
  add_region_btn->setIconSize(QSize(24, 24));

  QIcon add_region_btn_icon;
  add_region_btn_icon.addFile(QString::fromUtf8(":/images/region_btn.svg"),
                              QSize(), QIcon::Normal, QIcon::Off);
  add_region_btn->setIcon(add_region_btn_icon);
  add_region_btn->setEnabled(false);
  layout_tools_edit_map->addWidget(add_region_btn);

  // 分隔
  QFrame* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  separator->setStyleSheet("QFrame { background-color: #dce4ef; }");
  layout_tools_edit_map->addWidget(separator);

  // 橡皮擦按钮
  QToolButton* erase_btn = new QToolButton();
  erase_btn->setCheckable(true);
  erase_btn->setStyleSheet(modernEditButtonStyle);
  erase_btn->setToolTip("橡皮擦");
  erase_btn->setCursor(Qt::PointingHandCursor);
  erase_btn->setIconSize(QSize(24, 24));

  QIcon erase_btn_icon;
  erase_btn_icon.addFile(QString::fromUtf8(":/images/erase_btn.svg"),
                         QSize(), QIcon::Normal, QIcon::Off);
  erase_btn->setIcon(erase_btn_icon);
  layout_tools_edit_map->addWidget(erase_btn);

  // 画笔按钮
  QToolButton* draw_pen_btn = new QToolButton();
  draw_pen_btn->setCheckable(true);
  draw_pen_btn->setStyleSheet(modernEditButtonStyle);
  draw_pen_btn->setToolTip("障碍物绘制");
  draw_pen_btn->setCursor(Qt::PointingHandCursor);
  draw_pen_btn->setIconSize(QSize(24, 24));

  QIcon draw_pen_btn_icon;
  draw_pen_btn_icon.addFile(QString::fromUtf8(":/images/pen.svg"),
                            QSize(), QIcon::Normal, QIcon::Off);
  draw_pen_btn->setIcon(draw_pen_btn_icon);
  layout_tools_edit_map->addWidget(draw_pen_btn);

  // 线段按钮
  QToolButton* draw_line_btn = new QToolButton();
  draw_line_btn->setCheckable(true);
  draw_line_btn->setStyleSheet(modernEditButtonStyle);
  draw_line_btn->setToolTip("线段绘制");
  draw_line_btn->setCursor(Qt::PointingHandCursor);
  draw_line_btn->setIconSize(QSize(24, 24));

  QIcon draw_line_btn_icon;
  draw_line_btn_icon.addFile(QString::fromUtf8(":/images/line_btn.svg"),
                             QSize(), QIcon::Normal, QIcon::Off);
  draw_line_btn->setIcon(draw_line_btn_icon);
  layout_tools_edit_map->addWidget(draw_line_btn);

  layout_tools_edit_map->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

  // 创建按钮组实现互斥选择
  QButtonGroup* edit_map_button_group = new QButtonGroup(this);
  edit_map_button_group->addButton(normal_cursor_btn);
  edit_map_button_group->addButton(add_point_btn);
  edit_map_button_group->addButton(add_topology_path_btn);
  edit_map_button_group->addButton(add_region_btn);
  edit_map_button_group->addButton(erase_btn);
  edit_map_button_group->addButton(draw_pen_btn);
  edit_map_button_group->addButton(draw_line_btn);

  // 默认选中normal_cursor_btn
  normal_cursor_btn->setChecked(true);

  tools_edit_map_widget->hide();
  center_h_layout->addWidget(tools_edit_map_widget);
  center_layout->addLayout(center_h_layout);

  /////////////////////////////////////////////////////////////////////////地图显示
  display_manager_ = new Display::DisplayManager();
  center_h_layout->addWidget(display_manager_->GetViewPtr());

  // 减小下方边距
  center_layout->setContentsMargins(0, 0, 0, 5);
  center_layout->setSpacing(5);

  /////////////////////////////////////////////////中心主窗体
  QWidget* center_widget = new QWidget();
  center_widget->setStyleSheet(R"(
    QWidget {
      background-color: #ffffff;
    }
  )");
  center_widget->setLayout(center_layout);
  CDockWidget* CentralDockWidget = new CDockWidget("CentralWidget");
  CentralDockWidget->setWidget(center_widget);
  center_docker_area_ = dock_manager_->setCentralWidget(CentralDockWidget);
  center_docker_area_->setAllowedAreas(DockWidgetArea::OuterDockAreas);

  ////////////////////////////////////////////////////////图层配置管理
  display_config_widget_ = new DisplayConfigWidget();
  display_config_widget_->SetDisplayManager(display_manager_);
  display_config_widget_->SetChannelList(channel_manager_.DiscoveryChannelTypes());
  connect(display_config_widget_, &DisplayConfigWidget::ConnectRequested, this,
          [this]() {
            closeChannel();
            openChannel();
          });
  connect(display_config_widget_, &DisplayConfigWidget::DisconnectRequested,
          this, &MainWindow::closeChannel);
  settings_dock_ = new ads::CDockWidget(tr("设置"));
  settings_dock_->setWidget(display_config_widget_);
  ConfigureDockWidget(settings_dock_, QSize(400, 420), QSize(500, 620));
  settings_dock_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  settings_dock_area_ =
      dock_manager_->addDockWidget(ads::DockWidgetArea::LeftDockWidgetArea,
                                   settings_dock_, center_docker_area_);
  settings_dock_->toggleView(true);
  ui->menuView->addAction(settings_dock_->toggleViewAction());

  ////////////////////////////////////////////////////////速度控制
  speed_ctrl_widget_ = new SpeedCtrlWidget();
  connect(speed_ctrl_widget_, &SpeedCtrlWidget::signalControlSpeed,
          [this](const RobotSpeed& speed) {
            PUBLISH(MSG_ID_SET_ROBOT_SPEED, speed);
          });
  ads::CDockWidget* SpeedCtrlDockWidget = new ads::CDockWidget("速度控制");
  SpeedCtrlDockWidget->setWidget(speed_ctrl_widget_);
  ConfigureDockWidget(SpeedCtrlDockWidget, QSize(400, 420), QSize(500, 500));
  auto speed_ctrl_area =
      dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                                   SpeedCtrlDockWidget, settings_dock_area_);
  ui->menuView->addAction(SpeedCtrlDockWidget->toggleViewAction());

  //////////////////////////////////////////////////////////速度仪表盘
  ads::CDockWidget* DashBoardDockWidget = new ads::CDockWidget("速度仪表盘");
  QWidget* speed_dashboard_widget = new QWidget();
  DashBoardDockWidget->setWidget(speed_dashboard_widget);
  ConfigureDockWidget(DashBoardDockWidget, QSize(300, 220), QSize(360, 260));
  speed_dash_board_ = new DashBoard(speed_dashboard_widget);
  dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                               DashBoardDockWidget, center_docker_area_);
  DashBoardDockWidget->toggleView(false);
  ConfigureFloatingOnOpen(DashBoardDockWidget, QSize(720, 520));
  ui->menuView->addAction(DashBoardDockWidget->toggleViewAction());

  /////////////////////////////////////////////////////////导航任务列表
  QWidget* task_list_widget = new QWidget();
  nav_goal_table_view_ = new NavGoalTableView();
  QVBoxLayout* horizontalLayout_13 = new QVBoxLayout();
  horizontalLayout_13->addWidget(nav_goal_table_view_);
  task_list_widget->setLayout(horizontalLayout_13);
  ads::CDockWidget* nav_goal_list_dock_widget = new ads::CDockWidget("任务");

  // 现代化按钮样式
  QString modernButtonStyle = UiStyle::MainButtonStyleSheet();

  QPushButton* btn_add_one_goal = new QPushButton("添加点位");
  btn_add_one_goal->setStyleSheet(modernButtonStyle);

  QHBoxLayout* horizontalLayout_15 = new QHBoxLayout();
  QPushButton* btn_start_task_chain = new QPushButton("开始任务链");
  btn_start_task_chain->setStyleSheet(modernButtonStyle);

  QCheckBox* loop_task_checkbox = new QCheckBox("循环任务");
  loop_task_checkbox->setStyleSheet(UiStyle::CheckBoxStyleSheet());

  QHBoxLayout* horizontalLayout_14 = new QHBoxLayout();
  horizontalLayout_15->addWidget(btn_add_one_goal);
  horizontalLayout_14->addWidget(btn_start_task_chain);
  horizontalLayout_14->addWidget(loop_task_checkbox);

  QPushButton* btn_load_task_chain = new QPushButton("加载任务链");
  QPushButton* btn_save_task_chain = new QPushButton("保存任务链");
  btn_load_task_chain->setStyleSheet(modernButtonStyle);
  btn_save_task_chain->setStyleSheet(modernButtonStyle);

  QHBoxLayout* horizontalLayout_16 = new QHBoxLayout();
  horizontalLayout_16->addWidget(btn_load_task_chain);
  horizontalLayout_16->addWidget(btn_save_task_chain);

  horizontalLayout_13->addLayout(horizontalLayout_15);
  horizontalLayout_13->addLayout(horizontalLayout_14);
  horizontalLayout_13->addLayout(horizontalLayout_16);
  nav_goal_list_dock_widget->setWidget(task_list_widget);
  ConfigureDockWidget(nav_goal_list_dock_widget, QSize(340, 360), QSize(380, 620));
  nav_goal_list_dock_widget->setMaximumSize(520, 9999);
  dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                               nav_goal_list_dock_widget, center_docker_area_);
  ConfigureFloatingOnOpen(nav_goal_list_dock_widget, QSize(680, 760));
  nav_goal_list_dock_widget->toggleView(false);
  connect(nav_goal_table_view_, &NavGoalTableView::signalSendNavGoal,
          [this](const RobotPose& pose) {
            PUBLISH(MSG_ID_SET_NAV_GOAL_POSE, pose);
          });
  connect(btn_load_task_chain, &QPushButton::clicked, [this]() {
    QString fileName = QFileDialog::getOpenFileName(nullptr, "打开JSON文件",
                                                    "", "JSON文件 (*.json)",
                                                    nullptr, QFileDialog::DontUseNativeDialog);

    // 如果用户选择了文件，则输出文件名
    if (!fileName.isEmpty()) {
      qDebug() << "Selected file:" << fileName;
      nav_goal_table_view_->LoadTaskChain(fileName.toStdString());
    }
  });
  connect(btn_save_task_chain, &QPushButton::clicked, [this]() {
    QString fileName = QFileDialog::getSaveFileName(nullptr, "保存JSON文件",
                                                    "", "JSON文件 (*.json)",
                                                    nullptr, QFileDialog::DontUseNativeDialog);

    // 如果用户选择了文件，则输出文件名
    if (!fileName.isEmpty()) {
      qDebug() << "Selected file:" << fileName;
      if (!fileName.endsWith(".json")) {
        fileName += ".json";
      }
      nav_goal_table_view_->SaveTaskChain(fileName.toStdString());

      // 显示保存成功对话框
      QMessageBox::information(this, "保存成功",
                               "任务链文件已成功保存到:\n" + fileName,
                               QMessageBox::Ok);
    }
  });

  // nav_goal_list_dock_widget->toggleView(false);
  ui->menuView->addAction(nav_goal_list_dock_widget->toggleViewAction());
  connect(
      btn_add_one_goal, &QPushButton::clicked,
      [this, nav_goal_list_dock_widget]() { nav_goal_table_view_->AddItem(); });
  connect(btn_start_task_chain, &QPushButton::clicked,
          [this, btn_start_task_chain, loop_task_checkbox]() {
            if (btn_start_task_chain->text() == "开始任务链") {
              btn_start_task_chain->setText("停止任务链");
              nav_goal_table_view_->StartTaskChain(loop_task_checkbox->isChecked());
            } else {
              btn_start_task_chain->setText("开始任务链");
              nav_goal_table_view_->StopTaskChain();
            }
          });
  connect(nav_goal_table_view_, &NavGoalTableView::signalTaskFinish,
          [this, btn_start_task_chain]() {
            LOG_INFO("task finish!");
            btn_start_task_chain->setText("开始任务链");
          });
  connect(display_manager_,
          SIGNAL(signalTopologyMapUpdate(const TopologyMap&)),
          nav_goal_table_view_, SLOT(UpdateTopologyMap(const TopologyMap&)));
  connect(
      display_manager_,
      SIGNAL(signalCurrentSelectPointChanged(const TopologyMap::PointInfo&)),
      nav_goal_table_view_,
      SLOT(UpdateSelectPoint(const TopologyMap::PointInfo&)));

  //////////////////////////////////////////////////////Eggy 运维面板
  command_center_widget_ = new CommandCenterWidget();
  command_center_dock_ = new ads::CDockWidget("运维面板");
  command_center_dock_->setWidget(command_center_widget_);
  ConfigureDockWidget(command_center_dock_, QSize(420, 560), QSize(500, 720));
  command_center_dock_area_ =
      dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                                   command_center_dock_, center_docker_area_);
  command_center_dock_->toggleView(true);
  ui->menuView->addAction(command_center_dock_->toggleViewAction());

  //////////////////////////////////////////////////////图片
  for (auto one_image : Config::ConfigManager::Instance()->GetRootConfig().images) {
    LOG_INFO("init image window location:" << one_image.location << " topic:" << one_image.topic);
    image_frame_map_[one_image.location] = new RatioLayoutedFrame();
    ads::CDockWidget* dock_widget = new ads::CDockWidget(std::string("image/" + one_image.location).c_str());
    dock_widget->setWidget(image_frame_map_[one_image.location]);
    ConfigureDockWidget(dock_widget, QSize(420, 320), QSize(520, 390));
    dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea, dock_widget, center_docker_area_);
    dock_widget->toggleView(false);
    ConfigureFloatingOnOpen(dock_widget, QSize(760, 560));
    ui->menuView->addAction(dock_widget->toggleViewAction());
  }

  //////////////////////////////////////////////////////槽链接
  connect(this, SIGNAL(OnRecvChannelData(const MsgId&, const std::any&)),
          this, SLOT(RecvChannelMsg(const MsgId&, const std::any&)), Qt::BlockingQueuedConnection);
  connect(display_manager_, &Display::DisplayManager::signalPub2DPose,
          [this](const RobotPose& pose) {
            PUBLISH(MSG_ID_SET_RELOC_POSE, pose);
          });
  connect(display_manager_, &Display::DisplayManager::signalPub2DGoal,
          [this](const RobotPose& pose) {
            PUBLISH(MSG_ID_SET_NAV_GOAL_POSE, pose);
          });
  // ui相关
  connect(reloc_btn, &QToolButton::clicked,
          [this]() { display_manager_->StartReloc(); });

  connect(re_save_map_btn, &QToolButton::clicked, [this]() {
    QString fileName = QFileDialog::getSaveFileName(nullptr, "保存地图文件",
                                                    "", "地图文件 (*.yaml,*.pgm,*.pgm.json)",
                                                    nullptr, QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
      // 用户选择了文件夹，可以在这里进行相应的操作
      LOG_INFO("用户选择的保存地图路径：" << fileName.toStdString());

      // 保存占用栅格地图
      auto occ_map = display_manager_->GetOccupancyMap();
      occ_map.Save(fileName.toStdString());
      // 让 Qt 立即使用编辑后的地图
      display_manager_->UpdateOCCMap(occ_map);

      // 保存拓扑地图
      auto topology_map = display_manager_->GetTopologyMap();

      std::string topology_path = fileName.toStdString();
      // 替换扩展名为.topology
      size_t last_dot = topology_path.find_last_of(".");
      if (last_dot != std::string::npos) {
        topology_path = topology_path.substr(0, last_dot) + ".topology";
      } else {
        topology_path += ".topology";
      }
      Config::ConfigManager::Instance()->WriteTopologyMap(topology_path, topology_map);

      // 显示保存成功对话框
      QMessageBox::information(this, "保存成功",
                               "地图文件已成功保存到:\n" + fileName,
                               QMessageBox::Ok);
    } else {
      // 用户取消了选择
      LOG_INFO("取消保存地图");
    }
  });

  connect(save_map_btn, &QToolButton::clicked, [this]() {
    // 保存占用栅格地图
    auto occ_map = display_manager_->GetOccupancyMap();
    occ_map.Save(map_path_);
    // 让 Qt 立即使用编辑后的地图
    display_manager_->UpdateOCCMap(occ_map);

    // 保存拓扑地图
    auto topology_map = display_manager_->GetTopologyMap();

    std::string topology_path = map_path_ + ".topology";
    Config::ConfigManager::Instance()->WriteTopologyMap(topology_path, topology_map);

    // 发送到ROS
    PUBLISH(MSG_ID_TOPOLOGY_MAP_UPDATE, topology_map);

    // 显示保存成功对话框
    QMessageBox::information(this, "保存成功",
                             "地图文件已成功保存到:\n" + QString::fromStdString(map_path_),
                             QMessageBox::Ok);
  });

  connect(open_map_btn, &QToolButton::clicked, [this]() {
    QStringList filters;
    filters
        << "地图(*.yaml)"
        << "拓扑地图(*.topology)";

    QString fileName = QFileDialog::getOpenFileName(nullptr, "打开地图文件",
                                                    "", filters.join(";;"),
                                                    nullptr, QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) {
      LOG_INFO("用户选择的打开地图路径：" << fileName.toStdString());
      LoadMap(fileName.toStdString());
    } else {
      LOG_INFO("取消打开地图");
    }
  });

  connect(edit_map_btn, &QToolButton::clicked, [this, tools_edit_map_widget, edit_map_btn, normal_cursor_btn]() {
    if (edit_map_btn->text() == "编辑地图") {
      display_manager_->SetEditMapMode(Display::MapEditMode::kMoveCursor);
      edit_map_btn->setText("结束编辑");
      normal_cursor_btn->setChecked(true);
      tools_edit_map_widget->show();
    } else {
      display_manager_->SetEditMapMode(Display::MapEditMode::kStopEdit);
      edit_map_btn->setText("编辑地图");
      tools_edit_map_widget->hide();
      // 重置工具栏按钮到鼠标模式
      normal_cursor_btn->setChecked(true);
      // 隐藏添加机器人位置按钮
      Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
      if (view_manager) {
        view_manager->ShowAddRobotPosButton(false);
      }
    }
  });
  connect(add_point_btn, &QToolButton::clicked, [this]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kAddPoint);
    // 显示添加机器人位置按钮
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->ShowAddRobotPosButton(true);
      // 连接按钮点击事件（只在进入模式时连接一次）
      QToolButton* add_robot_pos_btn = view_manager->GetAddRobotPosButton();
      if (add_robot_pos_btn) {
        // 先断开之前的连接（如果有）
        add_robot_pos_btn->disconnect();
        connect(add_robot_pos_btn, &QToolButton::clicked, [this]() {
          display_manager_->AddPointAtRobotPosition();
        });
      }
    }
  });
  // 当退出 kAddPoint 模式时，隐藏添加机器人位置按钮
  auto hideAddRobotPosButton = [this]() {
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->ShowAddRobotPosButton(false);
    }
  };

  connect(normal_cursor_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kMoveCursor);
    hideAddRobotPosButton();
  });
  connect(erase_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kErase);
    hideAddRobotPosButton();
    // 更新滑动条显示为红色
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->UpdateToolSizeSlider(display_manager_->GetEraserRange());
    }
  });
  connect(draw_line_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kDrawLine);
    hideAddRobotPosButton();
  });
  connect(add_region_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kRegion);
    hideAddRobotPosButton();
  });
  connect(draw_pen_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kDrawWithPen);
    hideAddRobotPosButton();
    // 更新滑动条显示为蓝色
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->UpdateToolSizeSlider(display_manager_->GetPenRange());
    }
  });
  connect(add_topology_path_btn, &QToolButton::clicked, [this, hideAddRobotPosButton]() {
    display_manager_->SetEditMapMode(Display::MapEditMode::kLinkTopology);
    hideAddRobotPosButton();
  });

  connect(display_manager_->GetDisplay(DISPLAY_MAP),
          SIGNAL(signalCursorPose(QPointF)), this,
          SLOT(signalCursorPose(QPointF)));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == custom_title_bar_) {
    if (event->type() == QEvent::MouseButtonPress) {
      QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
      if (mouse_event->button() == Qt::LeftButton) {
        dragging_window_ = true;
        drag_position_ = mouse_event->globalPos() - frameGeometry().topLeft();
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      if (dragging_window_ && !isMaximized()) {
        QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        move(mouse_event->globalPos() - drag_position_);
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
      if (mouse_event->button() == Qt::LeftButton) {
        dragging_window_ = false;
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
      if (isMaximized()) {
        showNormal();
      } else {
        showMaximized();
      }
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::WindowStateChange) {
    UpdateMaximizeButton();
  }
}

void MainWindow::UpdateMaximizeButton() {
  if (!maximize_button_) {
    return;
  }
  const bool maximized = isMaximized();
  maximize_button_->setIcon(style()->standardIcon(
      maximized ? QStyle::SP_TitleBarNormalButton : QStyle::SP_TitleBarMaxButton));
  maximize_button_->setToolTip(maximized ? tr("还原") : tr("最大化"));
}

void MainWindow::ApplyCenteredWindowGeometry() {
  QScreen* target_screen = screen();
  if (!target_screen) {
    target_screen = QApplication::primaryScreen();
  }
  if (!target_screen) {
    return;
  }

  const QRect available = target_screen->availableGeometry();
  const QSize target_size(
      qMin(available.width(), qMax(960, qRound(available.width() * 0.88))),
      qMin(available.height(), qMax(640, qRound(available.height() * 0.90))));
  resize(target_size);
  move(available.center() - QPoint(target_size.width() / 2, target_size.height() / 2));
}

void MainWindow::ApplyDefaultDockSizes() {
  auto resize_area = [](ads::CDockAreaWidget* area, int target_width) {
    if (!area) {
      return;
    }

    QWidget* branch = area;
    QSplitter* splitter = nullptr;
    while (branch && branch->parentWidget()) {
      if (auto* candidate = qobject_cast<QSplitter*>(branch->parentWidget())) {
        if (candidate->orientation() == Qt::Horizontal) {
          splitter = candidate;
          break;
        }
      }
      branch = branch->parentWidget();
    }
    if (!splitter) {
      return;
    }

    const int branch_index = splitter->indexOf(branch);
    QList<int> sizes = splitter->sizes();
    if (branch_index < 0 || branch_index >= sizes.size() || sizes.size() < 2) {
      return;
    }

    const int total = std::accumulate(sizes.cbegin(), sizes.cend(), 0);
    const int clamped_target = qMin(target_width, qMax(320, total / 2));
    const int remaining = qMax(1, total - clamped_target);
    const int old_other_total = qMax(1, total - sizes.at(branch_index));
    int assigned = 0;
    for (int i = 0; i < sizes.size(); ++i) {
      if (i == branch_index) {
        sizes[i] = clamped_target;
        continue;
      }
      sizes[i] = qMax(1, remaining * sizes.at(i) / old_other_total);
      assigned += sizes.at(i);
    }
    for (int i = 0; i < sizes.size() && assigned < remaining; ++i) {
      if (i != branch_index) {
        sizes[i] += remaining - assigned;
        break;
      }
    }
    splitter->setSizes(sizes);
  };

  resize_area(settings_dock_area_, 800);
  resize_area(command_center_dock_area_, 500);
}

void MainWindow::ConfigureFloatingOnOpen(ads::CDockWidget* dock,
                                         const QSize& preferred_size) {
  if (!dock) {
    return;
  }
  connect(dock->toggleViewAction(), &QAction::triggered, this,
          [this, dock, preferred_size](bool open) {
            if (!open) {
              return;
            }
            QTimer::singleShot(0, this, [this, dock, preferred_size]() {
              if (!dock->isInFloatingContainer()) {
                dock->setFloating();
              }
              CenterFloatingDock(dock, preferred_size);
            });
          });
}

void MainWindow::CenterFloatingDock(ads::CDockWidget* dock,
                                    const QSize& preferred_size) {
  if (!dock) {
    return;
  }
  auto* container = dock->floatingDockContainer();
  if (!container) {
    return;
  }

  const QRect host = frameGeometry();
  const QSize target(qMin(preferred_size.width(), qRound(host.width() * 0.75)),
                     qMin(preferred_size.height(), qRound(host.height() * 0.80)));
  container->resize(target);
  container->move(host.center() - QPoint(target.width() / 2, target.height() / 2));
  container->raise();
  container->activateWindow();
}

void MainWindow::signalCursorPose(QPointF pos) {
  basic::Point mapPos =
      display_manager_->mapPose2Word(basic::Point(pos.x(), pos.y()));
  Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
  if (view_manager) {
    view_manager->UpdateMapPos("地图: (" + QString::number(mapPos.x, 'f', 2) +
                               ", " + QString::number(mapPos.y, 'f', 2) + ")");
    view_manager->UpdateScenePos("场景: (" + QString::number(pos.x(), 'f', 2) +
                                 ", " + QString::number(pos.y(), 'f', 2) + ")");
  }
}

//============================================================================
void MainWindow::closeEvent(QCloseEvent* event) {
  // Delete dock manager here to delete all floating widgets. This ensures
  // that all top level windows of the dock manager are properly closed
  // write state

  disconnect(this, SIGNAL(OnRecvChannelData(const MsgId&, const std::any&)),
             this, SLOT(RecvChannelMsg(const MsgId&, const std::any&)));
  SaveState();
  dock_manager_->deleteLater();
  QMainWindow::closeEvent(event);
  LOG_INFO("ros qt5 gui app close!");
}
void MainWindow::SaveState() {
  QSettings settings("state.ini", QSettings::IniFormat);
  settings.setValue("uiLayout/version", kUiLayoutVersion);
  settings.setValue("mainWindow/Geometry", this->saveGeometry());
  settings.setValue("mainWindow/State", this->saveState());
  dock_manager_->addPerspective("history");
  dock_manager_->savePerspectives(settings);
}

//============================================================================
void MainWindow::RestoreState() {
  QSettings settings("state.ini", QSettings::IniFormat);
  if (settings.value("uiLayout/version", 0).toInt() != kUiLayoutVersion) {
    LOG_INFO("skip stale UI layout state, expected version " << kUiLayoutVersion);
    ApplyCenteredWindowGeometry();
    QTimer::singleShot(0, this, &MainWindow::ApplyDefaultDockSizes);
    return;
  }
  const bool geometry_restored =
      this->restoreGeometry(settings.value("mainWindow/Geometry").toByteArray());
  this->restoreState(settings.value("mainWindow/State").toByteArray());
  dock_manager_->loadPerspectives(settings);
  dock_manager_->openPerspective("history");

  QScreen* target_screen = screen();
  if (!target_screen) {
    target_screen = QApplication::primaryScreen();
  }
  if (!geometry_restored || !target_screen ||
      !target_screen->availableGeometry().intersects(frameGeometry())) {
    ApplyCenteredWindowGeometry();
  }
  UpdateMaximizeButton();
}
void MainWindow::updateOdomInfo(RobotState state) {
  // 转向灯
  //   if (state.w > 0.1) {
  //     ui->label_turnLeft->setPixmap(
  //         QPixmap::fromImage(QImage("://images/turnLeft_hl.png")));
  //   } else if (state.w < -0.1) {
  //     ui->label_turnRight->setPixmap(
  //         QPixmap::fromImage(QImage("://images/turnRight_hl.png")));
  //   } else {
  //     ui->label_turnLeft->setPixmap(
  //         QPixmap::fromImage(QImage("://images/turnLeft_l.png")));
  //     ui->label_turnRight->setPixmap(
  //         QPixmap::fromImage(QImage("://images/turnRight_l.png")));
  //   }
  //   // 仪表盘
  speed_dash_board_->set_speed(abs(state.vx * 100));
  if (state.vx > 0.001) {
    speed_dash_board_->set_gear(DashBoard::kGear_D);
  } else if (state.vx < -0.001) {
    speed_dash_board_->set_gear(DashBoard::kGear_R);
  } else {
    speed_dash_board_->set_gear(DashBoard::kGear_N);
  }
  //   QString number = QString::number(abs(state.vx * 100)).mid(0, 2);
  //   if (number[1] == ".") {
  //     number = number.mid(0, 1);
  //   }
  //  ui->label_speed->setText(number);
  //  ui->mapViz->grab().save("/home/chengyangkj/test.jpg");
  //  QImage image(mysize,QImage::Format_RGB32);
  //           QPainter painter(&image);
  //           myscene->render(&painter);   //关键函数
}
void MainWindow::SlotSetBatteryStatus(double percent, double voltage) {
  // ROS BatteryState.percentage is 0.0-1.0; QProgressBar needs 0-100
  battery_bar_->setValue(static_cast<int>(percent * 100));
  label_power_->setText(QString::number(voltage, 'f', 2) + "V");
}

bool MainWindow::LoadMap(const std::string& file_path) {
  if (file_path.empty()) {
    return false;
  }

  std::string extension = QFileInfo(QString::fromStdString(file_path)).suffix().toStdString();

  if (extension == "yaml") {
    map_path_ = file_path;
    size_t last_dot = map_path_.find_last_of(".");
    if (last_dot != std::string::npos) {
      map_path_ = map_path_.substr(0, last_dot);
    }

    Config::ConfigManager::Instance()->GetRootConfig().map_config.path = map_path_;
    Config::ConfigManager::Instance()->StoreConfig();

    OccupancyMap map;
    if (map.Load(file_path)) {
      display_manager_->UpdateOCCMap(map);

      std::string topology_path = file_path;
      size_t last_dot = topology_path.find_last_of(".");
      if (last_dot != std::string::npos) {
        topology_path = topology_path.substr(0, last_dot) + ".topology";
      } else {
        topology_path += ".topology";
      }

      if (QFile::exists(QString::fromStdString(topology_path))) {
        TopologyMap topology_map;
        if (Config::ConfigManager::Instance()->ReadTopologyMap(topology_path, topology_map)) {
          display_manager_->UpdateTopologyMap(topology_map);
        }
      }
      return true;
    } else {
      QMessageBox::warning(this, "打开失败", "无法打开地图文件: " + QString::fromStdString(file_path));
      return false;
    }
  } else if (extension == "topology") {
    TopologyMap topology_map;
    if (Config::ConfigManager::Instance()->ReadTopologyMap(file_path, topology_map)) {
      display_manager_->UpdateTopologyMap(topology_map);
      return true;
    } else {
      QMessageBox::warning(this, "打开失败", "无法打开拓扑地图文件: " + QString::fromStdString(file_path));
      return false;
    }
  }

  return false;
}
