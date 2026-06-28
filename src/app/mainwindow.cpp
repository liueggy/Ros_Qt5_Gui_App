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
#include <QDebug>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QMouseEvent>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <QFileInfo>
#include <QFile>
#include "AutoHideDockContainer.h"
#include "DockAreaTabBar.h"
#include "DockAreaTitleBar.h"
#include "DockAreaWidget.h"
#include "DockComponentsFactory.h"
#include "Eigen/Dense"
#include "FloatingDockContainer.h"
#include "algorithm.h"
#include "logger/logger.h"
#include "config/config_manager.h"
#include "ui_mainwindow.h"
#include <QButtonGroup>
#include <QMessageBox>
#include <QMenu>

#include "widgets/speed_ctrl.h"
#include "widgets/display_config_widget.h"
#include "widgets/diagnostic_dock_widget.h"
#include "widgets/command_center_widget.h"
#include "widgets/ui_theme.h"
#include "msg/diagnostic_snapshot.h"
#include "display/manager/view_manager.h"
#include <QDateTime>
#include <QTimer>
using namespace ads;
MainWindow::MainWindow(QWidget *parent)
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
  if (channel_manager_.OpenChannelAuto()) {
    channel_opened_ = true;
    registerChannel();
    
    // 延迟检查连接状态（连接超时是5秒）
    auto* channel = channel_manager_.GetChannel();
    if (channel) {
      QTimer::singleShot(6000, this, [this, channel]() {
        if (channel->IsConnectionFailed()) {
          std::string error_msg = channel->GetConnectionError();
          std::string channel_name = channel->Name();
          if (channel_name == "ROSBridge") {
            QString display_error = error_msg.empty()
                                        ? "无法连接到 ROSBridge 服务器。"
                                        : QString::fromStdString(error_msg);
            QMessageBox::warning(this,
                                 "ROSBridge 连接失败",
                                 display_error +
                                     "\n\n请在左侧「设置」面板中重新设置 ROSBridge 的 IP 和端口。",
                                 QMessageBox::Ok);
            LOG_ERROR("ROSBridge connection failed: " << error_msg);
          } else {
            LOG_ERROR("Channel " << channel_name << " connection failed: " << error_msg);
            if (!error_msg.empty()) {
              QMessageBox::critical(this,
                                    QString::fromStdString(channel_name) + " 连接失败",
                                    QString::fromStdString(error_msg),
                                    QMessageBox::Ok);
            } else {
              QMessageBox::critical(this,
                                    QString::fromStdString(channel_name) + " 连接失败",
                                    "无法连接到 " + QString::fromStdString(channel_name) + " 服务器。\n\n请检查：\n"
                                    "1. 服务器是否正在运行\n"
                                    "2. 配置是否正确\n"
                                    "3. 网络连接是否正常",
                                    QMessageBox::Ok);
            }
          }
        }
      });
    }
    
    return true;
  }
  channel_opened_ = false;
  return false;
}
bool MainWindow::openChannel(const std::string &channel_name) {
  if (channel_manager_.OpenChannel(channel_name)) {
    registerChannel();
    return true;
  }
  return false;
}
void MainWindow::registerChannel() {
  SUBSCRIBE(MSG_ID_ODOM_POSE, [this](const RobotState& data) {
    last_odom_message_ms_ = QDateTime::currentMSecsSinceEpoch();
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
      last_camera_message_ms_ = QDateTime::currentMSecsSinceEpoch();
      std::lock_guard<std::mutex> lock(pending_images_mutex_);
      pending_images_[location_to_mat.first] = location_to_mat.second;
  });

  SUBSCRIBE(MSG_ID_DIAGNOSTIC, [this](const basic::DiagnosticSnapshot &snap) {
    last_diagnostic_message_ms_ = QDateTime::currentMSecsSinceEpoch();
    int warning_count = 0;
    int error_count = 0;
    for (const auto &hardware : snap.hardware) {
      for (const auto &component : hardware.second) {
        if (component.second.level >= 2) {
          ++error_count;
        } else if (component.second.level == 1) {
          ++warning_count;
        }
      }
    }
    diagnostic_warning_count_ = warning_count;
    diagnostic_error_count_ = error_count;
    if (diagnostic_dock_widget_) {
      diagnostic_dock_widget_->SetSnapshot(snap);
    }
  });

  SUBSCRIBE(MSG_ID_LASER_SCAN, [this](const LaserScan&) {
    last_lidar_message_ms_ = QDateTime::currentMSecsSinceEpoch();
  });

  SUBSCRIBE(MSG_ID_DHT11_TEMP, [this](const double &temp) {
    if (label_dht11_temp_) {
      label_dht11_temp_->setText(QString::number(temp, 'f', 1) + " °C");
    }
  });

  SUBSCRIBE(MSG_ID_DHT11_HUMI, [this](const double &humi) {
    if (label_dht11_humi_) {
      label_dht11_humi_->setText(QString::number(humi, 'f', 1) + " %");
    }
  });

  SUBSCRIBE(MSG_ID_VOICE_COMMAND, [this](const std::string &json_str) {
    if (label_voice_cmd_) {
      // 解析 {"func":"00","cmd":"04"} 显示为友好文本
      QString display = QString::fromStdString(json_str);
      label_voice_cmd_->setText("\U0001F50A " + display);
      label_voice_cmd_->setVisible(true);
      voice_clear_timer_->start(5000);  // 5秒后自动隐藏
    }
  });
}


void MainWindow::RecvChannelMsg(const MsgId &id, const std::any &data) {
  // 保留此方法以兼容现有代码，但不再使用
  // 数据现在通过 message_bus 订阅接收
}


void MainWindow::SlotRecvImage(const std::string &location, std::shared_ptr<cv::Mat> data) {
  if (!data || data->empty()) {
    return;
  }
  if (image_frame_map_.count(location)) {
    QImage image(data->data, data->cols, data->rows, data->step[0], QImage::Format_RGB888);
    image_frame_map_[location]->setImage(image);
  }
}
void MainWindow::closeChannel() { channel_manager_.CloseChannel(); }
MainWindow::~MainWindow() { delete ui; }

void MainWindow::ConfigureApplicationAppearance() {
  const QStringList preferred_font_families = {
      "SF Pro Display", "SF Pro Text", ".AppleSystemUIFont", "Segoe UI",
      "Microsoft YaHei UI"};
  const QStringList available_font_families = QFontDatabase().families();
  QString selected_font_family = "Microsoft YaHei UI";
  for (const auto &font_family : preferred_font_families) {
    if (available_font_families.contains(font_family)) {
      selected_font_family = font_family;
      break;
    }
  }
  QFont ui_font(selected_font_family, 10);
  ui_font.setStyleStrategy(QFont::PreferAntialias);
  setFont(ui_font);
  setStyleSheet(UiTheme::ApplicationStyle());
}

void MainWindow::ConfigureDockBehavior() {
  CDockManager::setConfigFlag(CDockManager::OpaqueSplitterResize, true);
  CDockManager::setConfigFlag(CDockManager::XmlCompressionEnabled, false);
  CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
  CDockManager::setConfigFlag(CDockManager::DockAreaHasUndockButton, false);
  CDockManager::setConfigFlag(CDockManager::DockAreaHasTabsMenuButton, false);
  CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, true);
  CDockManager::setConfigFlag(CDockManager::EqualSplitOnInsertion, true);
  CDockManager::setConfigFlag(CDockManager::ShowTabTextOnlyForActiveTab, true);
  CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);
}

void MainWindow::SetupDockMenus() {
  control_view_menu_ = ui->menuView->addMenu(tr("控制"));
  task_view_menu_ = ui->menuView->addMenu(tr("任务"));
  diagnostic_view_menu_ = ui->menuView->addMenu(tr("诊断"));
  settings_view_menu_ = ui->menuView->addMenu(tr("设置"));
}

void MainWindow::SetupOperationalDocks() {
  auto *dashboard_dock = new ads::CDockWidget("速度仪表盘");
  dashboard_dock->setWindowTitle(tr("控制 · 速度监控"));
  auto *dashboard_widget = new QWidget();
  dashboard_dock->setWidget(dashboard_widget);
  speed_dash_board_ = new DashBoard(dashboard_widget);
  auto *dashboard_area = dock_manager_->addDockWidget(
      ads::DockWidgetArea::RightDockWidgetArea, dashboard_dock,
      center_docker_area_);
  control_view_menu_->addAction(dashboard_dock->toggleViewAction());

  speed_ctrl_widget_ = new SpeedCtrlWidget();
  connect(speed_ctrl_widget_, &SpeedCtrlWidget::signalControlSpeed, this,
          [this](const RobotSpeed &speed) {
            PUBLISH(MSG_ID_SET_ROBOT_SPEED, speed);
          });
  auto *speed_control_dock = new ads::CDockWidget("速度控制");
  speed_control_dock->setWindowTitle(tr("控制 · 手动驾驶"));
  speed_control_dock->setWidget(speed_ctrl_widget_);
  speed_control_dock->setMinimumSizeHintMode(
      ads::CDockWidget::MinimumSizeHintFromDockWidget);
  speed_control_dock->setMinimumSize(380, 420);
  speed_control_dock->setMaximumWidth(480);
  dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                               speed_control_dock, dashboard_area);
  control_view_menu_->addAction(speed_control_dock->toggleViewAction());

  display_config_widget_ = new DisplayConfigWidget();
  display_config_widget_->SetDisplayManager(display_manager_);
  display_config_widget_->SetChannelList(
      channel_manager_.DiscoveryChannelTypes());
  settings_dock_ = new ads::CDockWidget(tr("设置"));
  settings_dock_->setWindowTitle(tr("设置 · 显示与连接"));
  settings_dock_->setWidget(display_config_widget_);
  settings_dock_->setMinimumSizeHintMode(
      ads::CDockWidget::MinimumSizeHintFromDockWidget);
  settings_dock_->setMinimumSize(340, 300);
  settings_dock_->setMaximumWidth(460);
  dock_manager_->addDockWidget(ads::DockWidgetArea::LeftDockWidgetArea,
                               settings_dock_, center_docker_area_);
  settings_dock_->toggleView(false);
  settings_view_menu_->addAction(settings_dock_->toggleViewAction());

  diagnostic_dock_widget_ = new DiagnosticDockWidget();
  diagnostic_dock_ = new ads::CDockWidget(tr("诊断"));
  diagnostic_dock_->setWindowTitle(tr("诊断 · 系统状态"));
  diagnostic_dock_->setWidget(diagnostic_dock_widget_);
  diagnostic_dock_->setMinimumSizeHintMode(
      ads::CDockWidget::MinimumSizeHintFromDockWidget);
  diagnostic_dock_->setMinimumSize(280, 200);
  diagnostic_dock_area_ = dock_manager_->addDockWidget(
      ads::DockWidgetArea::RightDockWidgetArea, diagnostic_dock_,
      center_docker_area_);
  diagnostic_dock_->toggleView(false);
  diagnostic_view_menu_->addAction(diagnostic_dock_->toggleViewAction());
}

void MainWindow::SetupTaskDocks() {
  auto *task_widget = new QWidget();
  nav_goal_table_view_ = new NavGoalTableView();
  auto *task_layout = new QVBoxLayout(task_widget);
  task_layout->addWidget(nav_goal_table_view_);

  const QString primary_button_style = UiTheme::PrimaryButtonStyle();
  auto *add_goal_button = new QPushButton("添加点位");
  auto *start_task_button = new QPushButton("开始任务链");
  auto *load_task_button = new QPushButton("加载任务链");
  auto *save_task_button = new QPushButton("保存任务链");
  for (auto *button : {add_goal_button, start_task_button, load_task_button,
                       save_task_button}) {
    button->setStyleSheet(primary_button_style);
  }
  auto *loop_task_checkbox = new QCheckBox("循环任务");

  auto *add_row = new QHBoxLayout();
  add_row->addWidget(add_goal_button);
  auto *run_row = new QHBoxLayout();
  run_row->addWidget(start_task_button);
  run_row->addWidget(loop_task_checkbox);
  auto *file_row = new QHBoxLayout();
  file_row->addWidget(load_task_button);
  file_row->addWidget(save_task_button);
  task_layout->addLayout(add_row);
  task_layout->addLayout(run_row);
  task_layout->addLayout(file_row);

  auto *task_dock = new ads::CDockWidget("任务");
  task_dock->setWindowTitle(tr("任务 · 导航队列"));
  task_dock->setWidget(task_widget);
  task_dock->setMinimumSizeHintMode(
      CDockWidget::MinimumSizeHintFromDockWidget);
  task_dock->setMinimumSize(200, 150);
  task_dock->setMaximumSize(480, 9999);
  auto *task_area = dock_manager_->addDockWidget(
      ads::DockWidgetArea::LeftDockWidgetArea, task_dock,
      center_docker_area_);
  task_dock->toggleView(false);
  task_view_menu_->addAction(task_dock->toggleViewAction());

  connect(nav_goal_table_view_, &NavGoalTableView::signalSendNavGoal, this,
          [this](const RobotPose &pose) {
            PUBLISH(MSG_ID_SET_NAV_GOAL_POSE, pose);
          });
  connect(add_goal_button, &QPushButton::clicked, this,
          [this]() { nav_goal_table_view_->AddItem(); });
  connect(load_task_button, &QPushButton::clicked, this, [this]() {
    const QString file_name = QFileDialog::getOpenFileName(
        this, "打开JSON文件", "", "JSON文件 (*.json)", nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!file_name.isEmpty()) {
      nav_goal_table_view_->LoadTaskChain(file_name.toStdString());
    }
  });
  connect(save_task_button, &QPushButton::clicked, this, [this]() {
    QString file_name = QFileDialog::getSaveFileName(
        this, "保存JSON文件", "", "JSON文件 (*.json)", nullptr,
        QFileDialog::DontUseNativeDialog);
    if (file_name.isEmpty()) {
      return;
    }
    if (!file_name.endsWith(".json")) {
      file_name += ".json";
    }
    nav_goal_table_view_->SaveTaskChain(file_name.toStdString());
    QMessageBox::information(
        this, "保存成功", "任务链文件已成功保存到:\n" + file_name,
        QMessageBox::Ok);
  });
  connect(start_task_button, &QPushButton::clicked, this,
          [this, start_task_button, loop_task_checkbox]() {
            if (start_task_button->text() == "开始任务链") {
              start_task_button->setText("停止任务链");
              nav_goal_table_view_->StartTaskChain(
                  loop_task_checkbox->isChecked());
            } else {
              start_task_button->setText("开始任务链");
              nav_goal_table_view_->StopTaskChain();
            }
          });
  connect(nav_goal_table_view_, &NavGoalTableView::signalTaskFinish, this,
          [start_task_button]() {
            LOG_INFO("task finish!");
            start_task_button->setText("开始任务链");
          });
  connect(display_manager_,
          SIGNAL(signalTopologyMapUpdate(const TopologyMap &)),
          nav_goal_table_view_, SLOT(UpdateTopologyMap(const TopologyMap &)));
  connect(
      display_manager_,
      SIGNAL(signalCurrentSelectPointChanged(const TopologyMap::PointInfo &)),
      nav_goal_table_view_,
      SLOT(UpdateSelectPoint(const TopologyMap::PointInfo &)));

  command_center_widget_ = new CommandCenterWidget();
  command_center_dock_ = new ads::CDockWidget("命令中心");
  command_center_dock_->setWindowTitle(tr("任务 · 命令中心"));
  command_center_dock_->setWidget(command_center_widget_);
  command_center_dock_->setMinimumSizeHintMode(
      CDockWidget::MinimumSizeHintFromDockWidget);
  command_center_dock_->setMinimumSize(360, 260);
  command_center_dock_->setMaximumWidth(480);
  dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                               command_center_dock_, task_area);
  command_center_dock_->toggleView(false);
  task_view_menu_->addAction(command_center_dock_->toggleViewAction());
}

void MainWindow::SetupImageDocks() {
  for (const auto &image_config :
       Config::ConfigManager::Instance()->GetRootConfig().images) {
    LOG_INFO("init image window location:"
             << image_config.location << " topic:" << image_config.topic);
    auto *image_frame = new RatioLayoutedFrame();
    image_frame->setPlaceholderText(
        tr("等待 %1 图像数据")
            .arg(QString::fromStdString(image_config.location)));
    image_frame_map_[image_config.location] = image_frame;

    auto *image_dock = new ads::CDockWidget(
        std::string("image/" + image_config.location).c_str());
    image_dock->setWindowTitle(
        tr("诊断 · 相机 %1")
            .arg(QString::fromStdString(image_config.location)));
    image_dock->setWidget(image_frame);
    dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                                 image_dock, diagnostic_dock_area_);
    image_dock->toggleView(false);
    diagnostic_view_menu_->addAction(image_dock->toggleViewAction());
  }
}

void MainWindow::SetupImageRefreshPipeline() {
  image_refresh_timer_ = new QTimer(this);
  image_refresh_timer_->setTimerType(Qt::PreciseTimer);
  image_refresh_timer_->setInterval(100);
  connect(image_refresh_timer_, &QTimer::timeout, this,
          &MainWindow::FlushPendingImages);
  image_refresh_timer_->start();
}

void MainWindow::FlushPendingImages() {
  std::map<std::string, std::shared_ptr<cv::Mat>> latest_images;
  {
    std::lock_guard<std::mutex> lock(pending_images_mutex_);
    latest_images.swap(pending_images_);
  }
  for (const auto &entry : latest_images) {
    SlotRecvImage(entry.first, entry.second);
  }
}

void MainWindow::setupUi() {
  ui->setupUi(this);
  setWindowFlags((windowFlags() | Qt::FramelessWindowHint) & ~Qt::WindowTitleHint);
  setAttribute(Qt::WA_TranslucentBackground, false);
  ConfigureApplicationAppearance();
  ConfigureDockBehavior();
  dock_manager_ = new CDockManager(this);
  SetupDockMenus();
  QVBoxLayout *center_layout = new QVBoxLayout();    //垂直
  QHBoxLayout *center_h_layout = new QHBoxLayout();  //水平

  const QString window_ctrl_btn_style = R"(
    QPushButton {
      min-width: 28px;
      max-width: 28px;
      min-height: 24px;
      max-height: 24px;
      border: 1px solid #e0e0e0;
      border-radius: 6px;
      background-color: #ffffff;
      color: #333333;
      font-size: 12px;
      font-weight: 600;
      padding: 0;
    }
    QPushButton:hover {
      background-color: #f5f5f5;
      border-color: #1976d2;
    }
    QPushButton:pressed {
      background-color: #e3f2fd;
    }
  )";

  QWidget *tools_strip = new QWidget();
  tools_strip->setObjectName("toolsStrip");
  custom_title_bar_ = tools_strip;
  tools_strip->installEventFilter(this);
  tools_strip->setStyleSheet(R"(
    QWidget#toolsStrip {
      background-color: #ffffff;
      border-bottom: 1px solid #e0e0e0;
    }
  )");

  ///////////////////////////////////////////////////////////////地图工具栏
  auto *tools_strip_layout = new QVBoxLayout(tools_strip);
  tools_strip_layout->setContentsMargins(8, 6, 8, 6);
  tools_strip_layout->setSpacing(5);
  QHBoxLayout *horizontalLayout_tools = new QHBoxLayout();
  QHBoxLayout *status_layout = new QHBoxLayout();
  tools_strip_layout->addLayout(horizontalLayout_tools);
  tools_strip_layout->addLayout(status_layout);
  horizontalLayout_tools->setSpacing(6);
  horizontalLayout_tools->setObjectName(
      QString::fromUtf8(" horizontalLayout_tools"));

  // 现代化工具栏样式
  const QString modernToolButtonStyle = UiTheme::ToolButtonStyle();

  auto add_toolbar_separator = [horizontalLayout_tools]() {
    auto *line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFixedSize(1, 28);
    line->setStyleSheet("background:#dce3ec; border:none;");
    horizontalLayout_tools->addSpacing(3);
    horizontalLayout_tools->addWidget(line);
    horizontalLayout_tools->addSpacing(3);
  };

  // 添加 "view" 菜单按钮
  QToolButton *view_menu_btn = new QToolButton();
  QIcon view_icon;
  view_icon.addFile(QString::fromUtf8(":/images/list_view.svg"),
                    QSize(32, 32), QIcon::Normal, QIcon::Off);
  view_menu_btn->setIcon(view_icon);
  view_menu_btn->setText("视图");
  view_menu_btn->setIconSize(QSize(18, 18));
  view_menu_btn->setPopupMode(QToolButton::InstantPopup);
  view_menu_btn->setMenu(ui->menuView);
  view_menu_btn->setStyleSheet(modernToolButtonStyle);
  view_menu_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  horizontalLayout_tools->addWidget(view_menu_btn);
  add_toolbar_separator();

  // 隐藏默认菜单栏
  menuBar()->setVisible(false);

  QToolButton *reloc_btn = new QToolButton();
  reloc_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  reloc_btn->setStyleSheet(modernToolButtonStyle);

  QIcon icon4;
  icon4.addFile(QString::fromUtf8(":/images/reloc2.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  reloc_btn->setIcon(icon4);
  reloc_btn->setText("重定位");
  reloc_btn->setIconSize(QSize(18, 18));
  horizontalLayout_tools->addWidget(reloc_btn);
  
  QIcon icon5;
  icon5.addFile(QString::fromUtf8(":/images/edit.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton *edit_map_btn = new QToolButton();
  edit_map_btn->setCheckable(true);
  edit_map_btn->setIcon(icon5);
  edit_map_btn->setText("编辑地图");
  edit_map_btn->setIconSize(QSize(18, 18));
  edit_map_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  edit_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(edit_map_btn);
  add_toolbar_separator();

  QIcon icon6;
  icon6.addFile(QString::fromUtf8(":/images/open.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton *open_map_btn = new QToolButton();
  open_map_btn->setIcon(icon6);
  open_map_btn->setText("打开地图");
  open_map_btn->setIconSize(QSize(18, 18));
  open_map_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  open_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(open_map_btn);

  QIcon icon8;
  icon8.addFile(QString::fromUtf8(":/images/save.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);

  QToolButton *save_map_btn = new QToolButton();
  save_map_btn->setIcon(icon8);
  save_map_btn->setText("保存地图");
  save_map_btn->setIconSize(QSize(18, 18));
  save_map_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(save_map_btn);

  QIcon icon7;
  icon7.addFile(QString::fromUtf8(":/images/re_save.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton *re_save_map_btn = new QToolButton();
  re_save_map_btn->setIcon(icon7);
  re_save_map_btn->setText("另存为");
  re_save_map_btn->setIconSize(QSize(18, 18));
  re_save_map_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  re_save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(re_save_map_btn);
  add_toolbar_separator();
  center_layout->addWidget(tools_strip);

  horizontalLayout_tools->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  rosbridge_status_chip_ = new StatusChip("ROS", tools_strip);
  base_status_chip_ = new StatusChip("底盘", tools_strip);
  camera_status_chip_ = new StatusChip("相机", tools_strip);
  lidar_status_chip_ = new StatusChip("雷达", tools_strip);
  alert_status_chip_ = new StatusChip("告警", tools_strip);
  status_layout->addWidget(rosbridge_status_chip_);
  status_layout->addWidget(base_status_chip_);
  status_layout->addWidget(camera_status_chip_);
  status_layout->addWidget(lidar_status_chip_);
  status_layout->addWidget(alert_status_chip_);
  status_layout->addStretch();

  status_refresh_timer_ = new QTimer(this);
  status_refresh_timer_->setInterval(1000);
  connect(status_refresh_timer_, &QTimer::timeout, this,
          &MainWindow::RefreshStatusBar);
  status_refresh_timer_->start();
  RefreshStatusBar();

  ///////////////////////////////////////////////////////////////////电池电量 - 现代化设计
  battery_bar_ = new QProgressBar();
  battery_bar_->setObjectName(QString::fromUtf8("battery_bar_"));
  battery_bar_->setMaximumSize(QSize(130, 28));
  battery_bar_->setAutoFillBackground(true);
  battery_bar_->setStyleSheet(R"(
    QProgressBar#battery_bar_ {
      border: 2px solid #e0e0e0;
      border-radius: 12px;
      background-color: #f5f5f5;
      text-align: center;
      color: #333333;
      font-weight: 600;
      font-size: 13px;
    }
    
    QProgressBar::chunk {
      background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #4caf50, stop:0.5 #8bc34a, stop:1 #4caf50);
      border-radius: 10px;
      margin: 2px;
    }
  )");

  battery_bar_->setAlignment(Qt::AlignCenter);
  status_layout->addWidget(battery_bar_);

  QLabel *label_11 = new QLabel();
  label_11->setObjectName(QString::fromUtf8("label_11"));
  label_11->setMinimumSize(QSize(24, 24));
  label_11->setMaximumSize(QSize(24, 24));
  label_11->setPixmap(QPixmap(QString::fromUtf8(":/images/power-v.png")));
  status_layout->addWidget(label_11);

  label_power_ = new QLabel();
  label_power_->setObjectName(QString::fromUtf8("label_power_"));
  label_power_->setMinimumSize(QSize(60, 28));
  label_power_->setMaximumSize(QSize(60, 28));
  label_power_->setStyleSheet(R"(
    QLabel {
      color: #333333;
      font-weight: 600;
      font-size: 13px;
      padding: 4px;
    }
  )");
  status_layout->addWidget(label_power_);

  // 温湿度显示
  status_layout->addSpacing(16);
  QLabel *dht_icon = new QLabel(QStringLiteral("\U0001F321"), this);
  dht_icon->setStyleSheet("font-size: 14px; padding: 2px;");
  status_layout->addWidget(dht_icon);

  label_dht11_temp_ = new QLabel(QStringLiteral("--.- °C"), this);
  label_dht11_temp_->setMinimumSize(QSize(70, 28));
  label_dht11_temp_->setMaximumSize(QSize(70, 28));
  label_dht11_temp_->setStyleSheet(R"(
    QLabel { color: #334155; font-weight: 600; font-size: 12px; padding: 2px; }
  )");
  status_layout->addWidget(label_dht11_temp_);

  label_dht11_humi_ = new QLabel(QStringLiteral("--.- %"), this);
  label_dht11_humi_->setMinimumSize(QSize(65, 28));
  label_dht11_humi_->setMaximumSize(QSize(65, 28));
  label_dht11_humi_->setStyleSheet(R"(
    QLabel { color: #334155; font-weight: 600; font-size: 12px; padding: 2px; }
  )");
  status_layout->addWidget(label_dht11_humi_);

  // 语音命令提示
  status_layout->addSpacing(8);
  label_voice_cmd_ = new QLabel(this);
  label_voice_cmd_->setMinimumSize(QSize(0, 24));
  label_voice_cmd_->setMaximumSize(QSize(220, 24));
  label_voice_cmd_->setStyleSheet(R"(
    QLabel { color: #ff8f00; font-weight: 600; font-size: 11px; padding: 2px 6px;
             background: #fff8e1; border: 1px solid #ffe082; border-radius: 4px; }
  )");
  label_voice_cmd_->setVisible(false);
  status_layout->addWidget(label_voice_cmd_);

  voice_clear_timer_ = new QTimer(this);
  voice_clear_timer_->setSingleShot(true);
  connect(voice_clear_timer_, &QTimer::timeout, [this]() {
    label_voice_cmd_->setVisible(false);
  });

  horizontalLayout_tools->addSpacing(12);
  QPushButton *min_btn = new QPushButton(QStringLiteral("-"), this);
  QPushButton *max_btn = new QPushButton(QStringLiteral("\u25A1"), this);
  QPushButton *close_btn = new QPushButton(QStringLiteral("\u00d7"), this);
  min_btn->setStyleSheet(window_ctrl_btn_style);
  max_btn->setStyleSheet(window_ctrl_btn_style);
  close_btn->setStyleSheet(window_ctrl_btn_style +
                          "\nQPushButton:hover { background-color: #ef5350; color: white; "
                          "border-color: #ef5350; }");
  connect(min_btn, &QPushButton::clicked, this, &QWidget::showMinimized);
  connect(max_btn, &QPushButton::clicked, [this, max_btn]() {
    if (isMaximized()) {
      showNormal();
      max_btn->setText(QStringLiteral("\u25A1"));
    } else {
      showMaximized();
      max_btn->setText(QStringLiteral("\u2750"));
    }
  });
  connect(close_btn, &QPushButton::clicked, this, &QWidget::close);
  horizontalLayout_tools->addWidget(min_btn);
  horizontalLayout_tools->addWidget(max_btn);
  horizontalLayout_tools->addWidget(close_btn);

  system_notice_banner_ = new QLabel(this);
  system_notice_banner_->setObjectName("systemNoticeBanner");
  system_notice_banner_->setWordWrap(true);
  system_notice_banner_->hide();
  center_layout->addWidget(system_notice_banner_);
  status_monitor_started_ms_ = QDateTime::currentMSecsSinceEpoch();

  SlotSetBatteryStatus(0, 0);
  
  //////////////////////////////////////////////////////////////编辑地图工具栏 - 现代化设计
  QWidget *tools_edit_map_widget = new QWidget();
  tools_edit_map_widget->setStyleSheet(R"(
    QWidget {
      background-color: #ffffff;
      border: 1px solid #e0e0e0;
      border-radius: 8px;
    }
  )");

  QVBoxLayout *layout_tools_edit_map = new QVBoxLayout();
  tools_edit_map_widget->setLayout(layout_tools_edit_map);
  layout_tools_edit_map->setSpacing(4);
  layout_tools_edit_map->setContentsMargins(8, 8, 8, 8);
  layout_tools_edit_map->setObjectName(
      QString::fromUtf8(" layout_tools_edit_map"));
  
  // 现代化编辑工具按钮样式
  const QString modernEditButtonStyle = UiTheme::EditToolButtonStyle();
  
  //地图编辑 设置鼠标按钮
  QToolButton *normal_cursor_btn = new QToolButton();
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

  //添加点位按钮
  QToolButton *add_point_btn = new QToolButton();
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

  QToolButton *add_topology_path_btn = new QToolButton();
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
  
  //添加区域按钮
  QToolButton *add_region_btn = new QToolButton();
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

  //分隔
  QFrame *separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  separator->setStyleSheet("QFrame { background-color: #e0e0e0; }");
  layout_tools_edit_map->addWidget(separator);

  //橡皮擦按钮
  QToolButton *erase_btn = new QToolButton();
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
  
  //画笔按钮
  QToolButton *draw_pen_btn = new QToolButton();
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
  
  //线段按钮
  QToolButton *draw_line_btn = new QToolButton();
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
  QButtonGroup *edit_map_button_group = new QButtonGroup(this);
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
  QWidget *center_widget = new QWidget();
  center_widget->setStyleSheet(R"(
    QWidget {
      background-color: #ffffff;
    }
  )");
  center_widget->setLayout(center_layout);
  CDockWidget *CentralDockWidget = new CDockWidget("CentralWidget");
  CentralDockWidget->setWidget(center_widget);
  center_docker_area_ = dock_manager_->setCentralWidget(CentralDockWidget);
  center_docker_area_->setAllowedAreas(DockWidgetArea::OuterDockAreas);

  SetupOperationalDocks();

  SetupTaskDocks();
  SetupImageDocks();
  SetupImageRefreshPipeline();

  //////////////////////////////////////////////////////槽链接
  connect(this, SIGNAL(OnRecvChannelData(const MsgId &, const std::any &)),
          this, SLOT(RecvChannelMsg(const MsgId &, const std::any &)), Qt::BlockingQueuedConnection);
  connect(display_manager_, &Display::DisplayManager::signalPub2DPose,
          [this](const RobotPose &pose) {
            PUBLISH(MSG_ID_SET_RELOC_POSE, pose);
          });
  connect(display_manager_, &Display::DisplayManager::signalPub2DGoal,
          [this](const RobotPose &pose) {
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
    
    //发送到ROS
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
    if (edit_map_btn->isChecked()) {
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

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == custom_title_bar_) {
    if (event->type() == QEvent::MouseButtonPress) {
      QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
      if (mouse_event->button() == Qt::LeftButton) {
        dragging_window_ = true;
        drag_position_ = mouse_event->globalPos() - frameGeometry().topLeft();
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      if (dragging_window_ && !isMaximized()) {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        move(mouse_event->globalPos() - drag_position_);
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
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
void MainWindow::closeEvent(QCloseEvent *event) {
  // Delete dock manager here to delete all floating widgets. This ensures
  // that all top level windows of the dock manager are properly closed
  // write state

  disconnect(this, SIGNAL(OnRecvChannelData(const MsgId &, const std::any &)),
             this, SLOT(RecvChannelMsg(const MsgId &, const std::any &)));
  SaveState();
  dock_manager_->deleteLater();
  QMainWindow::closeEvent(event);
  LOG_INFO("ros qt5 gui app close!");
}
void MainWindow::SaveState() {
  QSettings settings("state.ini", QSettings::IniFormat);
  settings.setValue("mainWindowV2/Geometry", this->saveGeometry());
  settings.setValue("mainWindowV2/State", this->saveState());
  dock_manager_->addPerspective("workspace_v2");
  dock_manager_->savePerspectives(settings);
}

//============================================================================
void MainWindow::RestoreState() {
  QSettings settings("state.ini", QSettings::IniFormat);
  this->restoreGeometry(settings.value("mainWindowV2/Geometry").toByteArray());
  this->restoreState(settings.value("mainWindowV2/State").toByteArray());
  dock_manager_->loadPerspectives(settings);
  dock_manager_->openPerspective("workspace_v2");
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

void MainWindow::RefreshStatusBar() {
  bool connection_failed = false;
  if (channel_opened_) {
    auto *channel = channel_manager_.GetChannel();
    if (channel->IsConnecting()) {
      rosbridge_status_chip_->SetState(StatusChip::State::Warning, "连接中");
    } else if (channel->IsConnectionFailed()) {
      connection_failed = true;
      rosbridge_status_chip_->SetState(StatusChip::State::Offline, "断开");
    } else {
      rosbridge_status_chip_->SetState(StatusChip::State::Online, "在线");
    }
  } else {
    rosbridge_status_chip_->SetState(StatusChip::State::Unknown, "未连接");
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  auto update_freshness = [now](StatusChip *chip, qint64 last_message_ms) {
    if (last_message_ms <= 0) {
      chip->SetState(StatusChip::State::Unknown, "等待数据");
    } else if (now - last_message_ms <= 3000) {
      chip->SetState(StatusChip::State::Online, "在线");
    } else {
      chip->SetState(StatusChip::State::Offline, "无数据");
    }
  };
  update_freshness(base_status_chip_, last_odom_message_ms_.load());
  update_freshness(camera_status_chip_, last_camera_message_ms_.load());
  update_freshness(lidar_status_chip_, last_lidar_message_ms_.load());

  if (last_diagnostic_message_ms_.load() <= 0) {
    alert_status_chip_->SetState(StatusChip::State::Unknown, "等待诊断");
  } else if (diagnostic_error_count_.load() > 0) {
    alert_status_chip_->SetState(
        StatusChip::State::Offline,
        QString::number(diagnostic_error_count_.load()) + " 错误");
  } else if (diagnostic_warning_count_.load() > 0) {
    alert_status_chip_->SetState(
        StatusChip::State::Warning,
        QString::number(diagnostic_warning_count_.load()) + " 警告");
  } else {
    alert_status_chip_->SetState(StatusChip::State::Online, "正常");
  }

  UpdateSystemNotice(now, connection_failed);
}

void MainWindow::UpdateSystemNotice(qint64 now_ms, bool connection_failed) {
  QStringList issues;
  const bool grace_period_over =
      now_ms - status_monitor_started_ms_ > 5000;
  if (connection_failed) {
    issues << "ROSBridge 连接已断开，请检查板端服务或连接设置";
  }
  auto append_stale_issue = [&](qint64 last_message_ms, const QString &message) {
    if ((last_message_ms <= 0 && grace_period_over) ||
        (last_message_ms > 0 && now_ms - last_message_ms > 3000)) {
      issues << message;
    }
  };
  append_stale_issue(last_odom_message_ms_.load(),
                     "底盘里程计无数据，运动状态可能不可用");
  append_stale_issue(last_camera_message_ms_.load(),
                     "相机无图像，请检查设备和图像话题");
  append_stale_issue(last_lidar_message_ms_.load(),
                     "雷达无扫描数据，导航与建图不可用");
  if (diagnostic_error_count_.load() > 0) {
    issues << QString("系统诊断报告 %1 项错误")
                  .arg(diagnostic_error_count_.load());
  }

  const qint64 last_camera_ms = last_camera_message_ms_.load();
  if ((last_camera_ms <= 0 && grace_period_over) ||
      (last_camera_ms > 0 && now_ms - last_camera_ms > 3000)) {
    for (const auto &entry : image_frame_map_) {
      if (entry.second) {
        entry.second->clearImage();
      }
    }
  }

  if (issues.isEmpty()) {
    system_notice_banner_->hide();
  } else {
    system_notice_banner_->setText("运行提示  ·  " + issues.join("  ·  "));
    system_notice_banner_->show();
  }
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
