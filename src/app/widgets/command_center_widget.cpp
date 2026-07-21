#include "widgets/command_center_widget.h"

#include <QByteArray>
#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "core/framework/framework.h"
#include "msg/channel_publish_result.h"
#include "msg/msg_info.h"
#include "widgets/diagnostic_dock_widget.h"
#include "app/diagnostic_policy.h"
#include "widgets/ui_style.h"

namespace {

QLabel* AddCardTitle(QVBoxLayout* layout, const QString& text, QWidget* parent) {
  auto* title = new QLabel(text, parent);
  title->setObjectName(QStringLiteral("sectionTitle"));
  title->setStyleSheet(QStringLiteral(
                           "QLabel#sectionTitle { color:%1; font-size:%2px; font-weight:800; "
                           "padding:0 0 2px 0; margin:0; background:transparent; "
                           "border:0px; border-style:none; border-radius:0px; }")
                           .arg(UiStyle::Palette::Text, UiStyle::FontBasePx()));
  layout->addWidget(title);
  return title;
}

QString FriendlySystemState(const QString& state) {
  static const QHash<QString, QString> labels = {
      {QStringLiteral("ready"), QObject::tr("就绪")},
      {QStringLiteral("degraded"), QObject::tr("部分功能异常")},
      {QStringLiteral("switching"), QObject::tr("切换中")},
      {QStringLiteral("starting"), QObject::tr("启动中")},
      {QStringLiteral("stopping"), QObject::tr("停止中")},
      {QStringLiteral("error"), QObject::tr("错误")},
  };
  return labels.value(state.trimmed(), state.trimmed().isEmpty() ? QObject::tr("未知")
                                                                 : state.trimmed());
}

QString FriendlyAutoMappingMessage(const QString& message, const QString& state) {
  const QString normalized = message.trimmed().toLower();
  if (normalized.isEmpty() || normalized == QStringLiteral("waiting for status")) {
    return state == QStringLiteral("idle")
               ? QObject::tr("已就绪，等待启动自动探索。")
               : QObject::tr("正在等待小车反馈…");
  }
  if (normalized == QStringLiteral("automatic mapping ready")) {
    return QObject::tr("已就绪，等待启动自动探索。");
  }
  return message.trimmed();
}

bool ConfirmAutoMappingAction(QWidget* parent, const QString& title,
                              const QString& message,
                              const QString& accept_text) {
  QMessageBox dialog(QMessageBox::Question, title, message,
                     QMessageBox::NoButton, parent);
  // This dialog is parented to a panel with a broad local QWidget style.
  // Bind the modal theme directly so the parent cannot override its surface.
  dialog.setStyleSheet(UiStyle::MessageBoxStyleSheet());
  QPushButton* accept = dialog.addButton(accept_text, QMessageBox::AcceptRole);
  QPushButton* cancel = dialog.addButton(QObject::tr("取消"),
                                         QMessageBox::RejectRole);
  dialog.setDefaultButton(cancel);
  dialog.setEscapeButton(cancel);
  dialog.exec();
  return dialog.clickedButton() == accept;
}

}  // namespace

CommandCenterWidget::CommandCenterWidget(QWidget* parent) : QWidget(parent) {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  auto* body = new QWidget(scroll);
  auto* root = new QVBoxLayout(body);
  root->setContentsMargins(20, 18, 20, 22);
  root->setSpacing(16);
  scroll->setWidget(body);
  outer->addWidget(scroll);

  setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::SecondaryButtonStyleSheet() + UiStyle::InputStyleSheet());

  motion_owner_label_ = new QLabel(this);
  root->addWidget(motion_owner_label_);
  SetMotionOwnerStatus({AppContract::MotionOwnerState::Unknown, {}});
  auto* camera_group = new QFrame(this);
  camera_group->setProperty("uiCard", true);
  camera_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* camera_layout = new QVBoxLayout(camera_group);
  camera_layout->setContentsMargins(16, 14, 16, 16);
  camera_layout->setSpacing(10);
  auto* camera_header = new QHBoxLayout();
  camera_header->setSpacing(8);
  auto* camera_title = new QLabel(tr("摄像头"), camera_group);
  camera_title->setObjectName(QStringLiteral("sectionTitle"));
  camera_title->setStyleSheet(QStringLiteral(
                                  "QLabel#sectionTitle { color:%1; font-size:%2px; font-weight:700; "
                                  "margin:0; padding:0; background:transparent; border:0px; border-style:none; border-radius:0px; }")
                                  .arg(UiStyle::Palette::Text, UiStyle::FontBasePx()));
  camera_state_label_ = new QLabel(tr("未连接"), camera_group);
  camera_start_btn_ = new QPushButton(tr("打开画面"), camera_group);
  camera_start_btn_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  camera_start_btn_->setAccessibleDescription(tr("打开机器人前置摄像头实时画面"));
  camera_start_btn_->setFixedWidth(118);
  camera_state_label_->setAlignment(Qt::AlignCenter);
  camera_state_label_->setStyleSheet(QStringLiteral(
                                         "QLabel { color:%1; background:%2; border:1px solid %3; "
                                         "border-radius:9px; padding:6px 10px; font-size:%4px; font-weight:700; }")
                                         .arg(UiStyle::Palette::TextSecondary)
                                         .arg(UiStyle::Palette::SurfaceAlt)
                                         .arg(UiStyle::Palette::Border)
                                         .arg(UiStyle::FontSmallPx()));
  camera_header->addWidget(camera_title);
  camera_header->addStretch();
  camera_header->addWidget(camera_state_label_);
  camera_header->addWidget(camera_start_btn_);
  camera_layout->addLayout(camera_header);

  connect(camera_start_btn_, &QPushButton::clicked, this, &CommandCenterWidget::StartCamera);

  auto* network_group = new QFrame(this);
  network_group->setProperty("uiCard", true);
  network_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* network_layout = new QVBoxLayout(network_group);
  network_layout->setContentsMargins(16, 12, 16, 14);
  network_layout->setSpacing(8);
  AddCardTitle(network_layout, tr("连接与外设"), network_group);
  auto* network_row = new QHBoxLayout();
  network_row->setSpacing(10);
  wifi_status_label_ = new QToolButton(network_group);
  cellular_status_label_ = new QToolButton(network_group);
  wifi_status_label_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/wifi.svg"), QSize(26, 26)));
  cellular_status_label_->setIcon(
      UiStyle::TintedIcon(QStringLiteral(":/icons/tabler/antenna-bars-5.svg"),
                          QSize(26, 26)));
  wifi_status_label_->setText(tr("WiFi\n未连接"));
  cellular_status_label_->setText(tr("4G\n未连接"));
  for (auto* status : {wifi_status_label_, cellular_status_label_}) {
    status->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    status->setIconSize(QSize(26, 26));
    status->setMinimumHeight(58);
    status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    status->setFocusPolicy(Qt::NoFocus);
    status->setCursor(Qt::ArrowCursor);
    network_row->addWidget(status, 1);
  }
  network_layout->addLayout(network_row);

  auto* nav_group = new QFrame(this);
  nav_group->setProperty("uiCard", true);
  nav_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* nav_layout = new QVBoxLayout(nav_group);
  nav_layout->setContentsMargins(16, 14, 16, 16);
  nav_layout->setSpacing(10);
  auto* nav_header = new QHBoxLayout();
  nav_header->setSpacing(8);
  auto* nav_title = new QLabel(tr("工作模式"), nav_group);
  nav_title->setObjectName(QStringLiteral("sectionTitle"));
  nav_title->setStyleSheet(QStringLiteral(
                               "QLabel#sectionTitle { color:%1; font-size:%2px; font-weight:700; "
                               "margin:0; padding:0; background:transparent; border:0px; border-style:none; border-radius:0px; }")
                               .arg(UiStyle::Palette::Text, UiStyle::FontBasePx()));
  nav_mode_label_ = new QLabel(tr("未连接"), nav_group);
  nav_mode_label_->setAlignment(Qt::AlignCenter);
  nav_mode_label_->setStyleSheet(QStringLiteral(
                                     "QLabel { color:%1; background:%2; border:1px solid %3; "
                                     "border-radius:9px; padding:6px 10px; font-size:%4px; font-weight:700; }")
                                     .arg(UiStyle::Palette::TextSecondary)
                                     .arg(UiStyle::Palette::SurfaceAlt)
                                     .arg(UiStyle::Palette::Border)
                                     .arg(UiStyle::FontSmallPx()));
  nav_header->addWidget(nav_title);
  nav_header->addStretch();
  nav_header->addWidget(nav_mode_label_);
  nav_layout->addLayout(nav_header);

  auto* nav_row = new QHBoxLayout();
  nav_row->setSpacing(10);
  mapping_btn_ = new QPushButton(tr("SLAM 建图"), nav_group);
  amcl_btn_ = new QPushButton(tr("AMCL 导航"), nav_group);
  inspection_btn_ = new QPushButton(tr("巡检模式"), nav_group);
  mapping_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  amcl_btn_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  mapping_btn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  amcl_btn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  inspection_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  inspection_btn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  nav_row->addWidget(mapping_btn_, 1);
  nav_row->addWidget(amcl_btn_, 1);
  nav_row->addWidget(inspection_btn_, 1);
  nav_layout->addLayout(nav_row);

  connect(mapping_btn_, &QPushButton::clicked, this, &CommandCenterWidget::SwitchToMapping);
  connect(amcl_btn_, &QPushButton::clicked, this, &CommandCenterWidget::StartAmclNavigation);
  connect(inspection_btn_, &QPushButton::clicked, this, &CommandCenterWidget::StartInspection);
  mapping_btn_->setEnabled(false);
  amcl_btn_->setEnabled(false);
  inspection_btn_->setEnabled(false);

  auto* auto_mapping_group = new QFrame(this);
  auto_mapping_group->setProperty("uiCard", true);
  auto_mapping_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* auto_mapping_layout = new QVBoxLayout(auto_mapping_group);
  auto_mapping_layout->setContentsMargins(16, 14, 16, 16);
  auto_mapping_layout->setSpacing(10);
  auto* auto_mapping_header = new QHBoxLayout();
  auto* auto_mapping_title = new QLabel(tr("自动建图"), auto_mapping_group);
  auto_mapping_title->setObjectName(QStringLiteral("sectionTitle"));
  auto_mapping_title->setStyleSheet(QStringLiteral(
                                        "QLabel#sectionTitle { color:%1; font-size:%2px; font-weight:800; "
                                        "padding:0; margin:0; background:transparent; border:0px; }")
                                        .arg(UiStyle::Palette::Text, UiStyle::FontBasePx()));
  auto_mapping_state_label_ = new QLabel(tr("未启动"), auto_mapping_group);
  auto_mapping_state_label_->setAlignment(Qt::AlignCenter);
  auto_mapping_state_label_->setStyleSheet(QStringLiteral(
                                               "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:9px; "
                                               "padding:6px 10px; font-size:%4px; font-weight:700; }")
                                               .arg(UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt,
                                                    UiStyle::Palette::Border)
                                               .arg(UiStyle::FontSmallPx()));
  auto_mapping_header->addWidget(auto_mapping_title);
  auto_mapping_header->addStretch();
  auto_mapping_header->addWidget(auto_mapping_state_label_);
  auto_mapping_layout->addLayout(auto_mapping_header);

  auto_mapping_message_label_ = new QLabel(
      tr("进入 SLAM 建图后，可自动探索高信息量边界；安全传感器异常时立即停驶。"),
      auto_mapping_group);
  auto_mapping_message_label_->setWordWrap(true);
  auto_mapping_message_label_->setStyleSheet(QStringLiteral(
                                                 "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:10px; "
                                                 "padding:9px 11px; font-size:%4px; }")
                                                 .arg(UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt,
                                                      UiStyle::Palette::Border)
                                                 .arg(UiStyle::FontMiniPx()));
  auto_mapping_layout->addWidget(auto_mapping_message_label_);

  auto* auto_mapping_form = new QFormLayout();
  auto_mapping_form->setHorizontalSpacing(12);
  auto_mapping_form->setVerticalSpacing(8);
  auto_mapping_duration_spin_ = new QSpinBox(auto_mapping_group);
  auto_mapping_duration_spin_->setRange(1, 60);
  auto_mapping_duration_spin_->setValue(15);
  auto_mapping_duration_spin_->setSuffix(tr(" 分钟"));
  auto_mapping_speed_spin_ = new QDoubleSpinBox(auto_mapping_group);
  auto_mapping_speed_spin_->setRange(0.05, 0.30);
  auto_mapping_speed_spin_->setSingleStep(0.01);
  auto_mapping_speed_spin_->setDecimals(2);
  auto_mapping_speed_spin_->setValue(0.22);
  auto_mapping_speed_spin_->setSuffix(tr(" m/s"));
  auto_mapping_return_home_check_ = new QCheckBox(tr("完成后返回起点"), auto_mapping_group);
  auto_mapping_return_home_check_->setChecked(true);
  auto_mapping_form->addRow(tr("最长运行"), auto_mapping_duration_spin_);
  auto_mapping_form->addRow(tr("速度上限"), auto_mapping_speed_spin_);
  auto_mapping_form->addRow(QString(), auto_mapping_return_home_check_);
  auto_mapping_layout->addLayout(auto_mapping_form);

  auto_mapping_progress_ = new QProgressBar(auto_mapping_group);
  auto_mapping_progress_->setRange(0, 100);
  auto_mapping_progress_->setValue(0);
  auto_mapping_progress_->setTextVisible(true);
  auto_mapping_progress_->setFormat(tr("尚未启动"));
  auto_mapping_layout->addWidget(auto_mapping_progress_);
  auto* auto_mapping_metrics = new QGridLayout();
  auto_mapping_metrics->setHorizontalSpacing(8);
  auto_mapping_metrics->setVerticalSpacing(8);
  auto_mapping_frontier_metric_ = new QLabel(auto_mapping_group);
  auto_mapping_sensor_metric_ = new QLabel(auto_mapping_group);
  auto_mapping_safety_metric_ = new QLabel(auto_mapping_group);
  auto_mapping_metrics->addWidget(auto_mapping_frontier_metric_, 0, 0);
  auto_mapping_metrics->addWidget(auto_mapping_safety_metric_, 0, 1);
  auto_mapping_metrics->addWidget(auto_mapping_sensor_metric_, 1, 0, 1, 2);
  SetMetricPill(auto_mapping_frontier_metric_, tr("可达边界"), tr("等待数据"),
                UiStyle::Palette::TextSecondary);
  SetMetricPill(auto_mapping_sensor_metric_, tr("传感器新鲜度"), tr("等待数据"),
                UiStyle::Palette::TextSecondary);
  SetMetricPill(auto_mapping_safety_metric_, tr("安全联锁"), tr("等待数据"),
                UiStyle::Palette::TextSecondary);
  auto_mapping_layout->addLayout(auto_mapping_metrics);

  auto* auto_mapping_actions = new QHBoxLayout();
  auto_mapping_actions->setSpacing(8);
  auto_mapping_start_btn_ = new QPushButton(tr("开始自动建图"), auto_mapping_group);
  auto_mapping_pause_btn_ = new QPushButton(tr("暂停"), auto_mapping_group);
  auto_mapping_stop_btn_ = new QPushButton(tr("停止并保存草稿"), auto_mapping_group);
  auto_mapping_start_btn_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  auto_mapping_pause_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  auto_mapping_stop_btn_->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  auto_mapping_start_btn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto_mapping_actions->addWidget(auto_mapping_start_btn_, 2);
  auto_mapping_actions->addWidget(auto_mapping_pause_btn_, 1);
  auto_mapping_actions->addWidget(auto_mapping_stop_btn_, 2);
  auto_mapping_layout->addLayout(auto_mapping_actions);
  // 任务优先：总览和工作模式常驻顶部，当前任务紧随其后；
  // 摄像头、网络与诊断属于按需查看的次级信息。
  root->addWidget(nav_group);
  root->addWidget(auto_mapping_group);
  root->addWidget(camera_group);
  root->addWidget(network_group);
  connect(auto_mapping_start_btn_, &QPushButton::clicked,
          this, &CommandCenterWidget::StartAutoMapping);
  connect(auto_mapping_pause_btn_, &QPushButton::clicked,
          this, &CommandCenterWidget::PauseResumeAutoMapping);
  connect(auto_mapping_stop_btn_, &QPushButton::clicked,
          this, &CommandCenterWidget::StopAutoMapping);
  RefreshAutoMappingControls();

  auto* status_group = new QFrame(this);
  status_group->setProperty("uiCard", true);
  status_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* status_layout = new QVBoxLayout(status_group);
  status_layout->setContentsMargins(16, 14, 16, 16);
  status_layout->setSpacing(10);
  auto* status_header = new QHBoxLayout();
  status_header->setSpacing(8);
  auto* status_title = new QLabel(tr("运行状态"), status_group);
  status_title->setObjectName(QStringLiteral("sectionTitle"));
  status_title->setStyleSheet(QStringLiteral(
                                  "QLabel#sectionTitle { color:%1; font-size:%2px; font-weight:700; "
                                  "margin:0; padding:0; background:transparent; border:0px; border-style:none; border-radius:0px; }")
                                  .arg(UiStyle::Palette::Text, UiStyle::FontBasePx()));
  auto* refresh_status_btn = new QPushButton(tr("刷新"), status_group);
  log_toggle_btn_ = new QPushButton(tr("日志"), status_group);
  clear_log_btn_ = new QPushButton(tr("清空"), status_group);
  refresh_status_btn->setFixedWidth(76);
  log_toggle_btn_->setFixedWidth(76);
  clear_log_btn_->setFixedWidth(76);
  clear_log_btn_->setVisible(false);
  status_header->addWidget(status_title);
  status_header->addStretch();
  status_header->addWidget(refresh_status_btn);
  status_header->addWidget(log_toggle_btn_);
  status_header->addWidget(clear_log_btn_);
  status_layout->addLayout(status_header);
  status_summary_label_ = new QLabel(tr("暂无状态"), status_group);
  status_summary_label_->setWordWrap(true);
  status_summary_label_->setMinimumHeight(46);
  status_summary_label_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  status_summary_label_->setStyleSheet(QStringLiteral(
                                           "QLabel { color:%1; background:%2; border:1px solid %3; "
                                           "border-radius:12px; padding:10px 12px; font-size:%4px; }")
                                           .arg(UiStyle::Palette::TextSecondary)
                                           .arg(UiStyle::Palette::SurfaceAlt)
                                           .arg(UiStyle::Palette::Border)
                                           .arg(UiStyle::FontSmallPx()));
  status_layout->addWidget(status_summary_label_);
  log_edit_ = new QPlainTextEdit(status_group);
  log_edit_->setReadOnly(true);
  log_edit_->setMinimumHeight(96);
  log_edit_->setMaximumHeight(160);
  log_edit_->setStyleSheet(QStringLiteral(
                               "QPlainTextEdit { color:%1; background:%2; border:1px solid %3; border-radius:8px; "
                               "padding:8px 10px; font-family:%4; font-size:%5px; }")
                               .arg(UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt,
                                    UiStyle::Palette::Border, UiStyle::Font::Mono,
                                    QString::number(UiStyle::FontMiniPx())));
  log_edit_->setVisible(false);
  status_layout->addWidget(log_edit_);
  root->addWidget(status_group);

  connect(refresh_status_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendStatusRequest);
  connect(log_toggle_btn_, &QPushButton::clicked, this, &CommandCenterWidget::ToggleLog);
  connect(clear_log_btn_, &QPushButton::clicked, this, &CommandCenterWidget::ClearLog);

  diagnostic_group_ = new QFrame(this);
  diagnostic_group_->setProperty("uiCard", true);
  diagnostic_group_->setStyleSheet(UiStyle::CardStyleSheet());
  auto* diagnostic_layout = new QVBoxLayout(diagnostic_group_);
  diagnostic_layout->setContentsMargins(16, 14, 16, 16);
  diagnostic_layout->setSpacing(10);
  AddCardTitle(diagnostic_layout, tr("系统诊断"), diagnostic_group_);
  diagnostic_widget_ = new DiagnosticDockWidget(diagnostic_group_);
  diagnostic_layout->addWidget(diagnostic_widget_);
  diagnostic_group_->setVisible(false);
  root->addWidget(diagnostic_group_);
  root->addStretch(1);

  SUBSCRIBE_QOBJECT(this, MSG_ID_COMMAND_RESPONSE, [this](const std::string& json) {
    AppendResponse(json);
  });
  SUBSCRIBE_QOBJECT(this, MSG_ID_COMMAND_STATUS, [this](const std::string& json) {
    UpdateStatus(json);
  });
  SUBSCRIBE_QOBJECT(this, MSG_ID_CMD_VEL_CONTROL, [this](const std::string& json) {
    UpdateMotionOwner(json);
  });
  SUBSCRIBE_QOBJECT(
      this, MSG_ID_CHANNEL_PUBLISH_RESULT,
      [this](const basic::ChannelPublishResult& result) {
        if (result.success || result.message_id != MSG_ID_COMMAND_REQUEST) {
          return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, result]() {
              const QString request_id =
                  QString::fromStdString(result.request_id);
              if (camera_start_pending_ &&
                  request_id == camera_start_request_id_) {
                camera_start_pending_ = false;
                camera_waiting_first_frame_ = false;
                camera_start_request_id_.clear();
                SetCameraStateText(tr("请求发送失败"));
                SetStatusSummary(tr("摄像头启动请求发送失败"),
                                 QString::fromStdString(result.message));
                AppendLog(tr("失败"), tr("摄像头启动命令未发送到小车端，请检查连接。"));
                return;
              }
              if (!profile_switch_tracker_.Timeout(request_id)) {
                return;
              }
              pending_auto_mapping_start_ = false;
              SetNavigationModeText(active_workspace_mode_);
              SetStatusSummary(tr("模式切换请求发送失败"),
                               QString::fromStdString(result.message));
              AppendLog(tr("失败"), tr("命令未发送到小车端，请检查连接。"));
            },
            Qt::QueuedConnection);
      });
}

void CommandCenterWidget::SetDiagnosticSnapshot(const basic::DiagnosticSnapshot& snapshot) {
  raw_diagnostic_snapshot_ = snapshot;
  RefreshDiagnosticSnapshot();
}

void CommandCenterWidget::RefreshDiagnosticSnapshot() {
  const bool switching = profile_switch_tracker_.pending() ||
                         external_profile_switch_busy_;
  const auto adapted = AppContract::AdaptDiagnosticSnapshot(
      raw_diagnostic_snapshot_, active_workspace_mode_, switching);
  if (diagnostic_widget_) {
    diagnostic_widget_->SetSnapshot(adapted);
  }
  int total = 0;
  for (const auto& hardware : adapted.hardware) {
    for (const auto& component : hardware.second) {
      ++total;
    }
  }
  const int abnormal = AppContract::CountDiagnosticAbnormal(adapted);
  if (diagnostic_group_) {
    diagnostic_group_->setVisible(total > 0 && abnormal > 0);
  }
}

void CommandCenterWidget::SetNetworkStatus(const std::string& json) {
  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(QByteArray::fromStdString(json), &error);
  const QJsonObject root =
      error.error == QJsonParseError::NoError && document.isObject()
          ? document.object()
          : QJsonObject();
  const QJsonObject wifi = root.value(QStringLiteral("wifi")).toObject();
  const QJsonObject cellular =
      root.contains(QStringLiteral("cellular"))
          ? root.value(QStringLiteral("cellular")).toObject()
          : root.value(QStringLiteral("4g")).toObject();
  const bool wifi_connected = wifi.value(QStringLiteral("connected")).toBool(false);
  const bool cellular_connected =
      cellular.value(QStringLiteral("connected")).toBool(false);
  const QString wifi_name = wifi.value(QStringLiteral("name")).toString().trimmed();
  const QString cellular_operator =
      cellular.value(QStringLiteral("operator")).toString().trimmed();

  if (wifi_status_label_) {
    wifi_status_label_->setText(
        tr("WiFi  %1\n%2")
            .arg(wifi_connected ? tr("● 已连接") : tr("○ 未连接"),
                 wifi_name.isEmpty() ? tr("无连接名称") : wifi_name));
    wifi_status_label_->setStyleSheet(
        QStringLiteral(
            "QToolButton { color:%1; background:%2; border:1px solid %3; "
            "border-radius:11px; padding:8px 12px; text-align:left; "
            "font-size:%4px; font-weight:700; }")
            .arg(wifi_connected ? UiStyle::Palette::Success
                                : UiStyle::Palette::TextSecondary,
                 wifi_connected ? UiStyle::Palette::SuccessBg
                                : UiStyle::Palette::SurfaceAlt,
                 wifi_connected ? UiStyle::Palette::SuccessBorder
                                : UiStyle::Palette::Border)
            .arg(UiStyle::FontSmallPx()));
  }
  if (cellular_status_label_) {
    cellular_status_label_->setText(
        tr("4G  %1\n%2")
            .arg(cellular_connected ? tr("● 已连接") : tr("○ 未连接"),
                 cellular_operator.isEmpty() ? tr("未知运营商")
                                             : cellular_operator));
    cellular_status_label_->setStyleSheet(
        QStringLiteral(
            "QToolButton { color:%1; background:%2; border:1px solid %3; "
            "border-radius:11px; padding:8px 12px; text-align:left; "
            "font-size:%4px; font-weight:700; }")
            .arg(cellular_connected ? UiStyle::Palette::Info
                                    : UiStyle::Palette::TextSecondary,
                 cellular_connected ? UiStyle::Palette::InfoBg
                                    : UiStyle::Palette::SurfaceAlt,
                 cellular_connected ? UiStyle::Palette::InfoBorder
                                    : UiStyle::Palette::Border)
            .arg(UiStyle::FontSmallPx()));
  }
}

void CommandCenterWidget::SetNavigationModeText(const QString& mode) {
  if (!nav_mode_label_) {
    return;
  }

  QString text = tr("未知");
  QString color = UiStyle::Palette::TextSecondary;
  QString bg = UiStyle::Palette::SurfaceAlt;
  QString border = UiStyle::Palette::Border;
  const QString reported_mode = mode.trimmed();
  QString normalized = reported_mode;
  if (reported_mode == QStringLiteral("mapping")) {
    normalized = QStringLiteral("mapping_slam");
  } else if (reported_mode == QStringLiteral("navigation")) {
    normalized = QStringLiteral("static_nav");
  }
  if (reported_mode == QStringLiteral("switching")) {
    text = tr("切换中");
    color = UiStyle::Palette::Info;
    bg = UiStyle::Palette::InfoBg;
    border = UiStyle::Palette::InfoBorder;
  } else if (normalized == QStringLiteral("mapping_slam")) {
    text = tr("SLAM 建图");
    color = UiStyle::Palette::Warning;
    bg = UiStyle::Palette::WarningBg;
    border = UiStyle::Palette::WarningBorder;
  } else if (normalized == QStringLiteral("static_nav")) {
    text = tr("AMCL 导航");
    color = UiStyle::Palette::Info;
    bg = UiStyle::Palette::InfoBg;
    border = UiStyle::Palette::InfoBorder;
  } else if (normalized == QStringLiteral("inspection")) {
    text = tr("巡检模式");
    color = UiStyle::Palette::Success;
    bg = UiStyle::Palette::SuccessBg;
    border = UiStyle::Palette::SuccessBorder;
  } else if (!normalized.isEmpty()) {
    text = normalized;
  }

  nav_mode_label_->setText(text);
  nav_mode_label_->setStyleSheet(QStringLiteral(
                                     "QLabel { color:%1; background:%2; border:1px solid %3; "
                                     "border-radius:9px; padding:6px 10px; font-size:%4px; font-weight:700; }")
                                     .arg(color, bg, border)
                                     .arg(UiStyle::FontSmallPx()));

  if (mapping_btn_) {
    const bool active = normalized == QStringLiteral("mapping_slam");
    mapping_btn_->setText(active ? tr("当前：SLAM") : tr("SLAM 建图"));
    mapping_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                             !external_profile_switch_busy_ &&
                             mapping_profile_available_ && !active);
    mapping_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                       : UiStyle::SecondaryButtonStyleSheet());
  }
  if (amcl_btn_) {
    const bool active = normalized == QStringLiteral("static_nav");
    amcl_btn_->setText(active ? tr("当前：AMCL") : tr("AMCL 导航"));
    amcl_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                          !external_profile_switch_busy_ &&
                          navigation_profile_available_ && !active);
    amcl_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                    : UiStyle::SecondaryButtonStyleSheet());
  }
  if (inspection_btn_) {
    const bool active = normalized == QStringLiteral("inspection");
    inspection_btn_->setText(active ? tr("当前：巡检") : tr("巡检模式"));
    inspection_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                                !external_profile_switch_busy_ &&
                                inspection_profile_available_ && !active);
    inspection_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                          : UiStyle::SecondaryButtonStyleSheet());
  }
  if ((normalized == QStringLiteral("mapping_slam") ||
       normalized == QStringLiteral("static_nav") ||
       normalized == QStringLiteral("inspection")) &&
      normalized != active_workspace_mode_) {
    active_workspace_mode_ = normalized;
    emit WorkspaceModeRequested(normalized);
  }
  if (normalized != QStringLiteral("mapping_slam")) {
    pending_auto_mapping_start_ = false;
  }
  RefreshDiagnosticSnapshot();
  RefreshAutoMappingControls();
}

void CommandCenterWidget::SetExternalProfileSwitchBusy(
    bool busy, const QString& message, const QString& confirmedMode) {
  external_profile_switch_busy_ = busy;
  SetNavigationModeText(
      busy ? QStringLiteral("switching")
           : (confirmedMode.isEmpty() ? active_workspace_mode_ : confirmedMode));
  if (!message.isEmpty()) {
    SetStatusSummary(message);
  }
}

QString CommandCenterWidget::MakeRequestJson(const QString& command, const QString& target,
                                             const QString& paramsJson,
                                             const QString& requestId) const {
  QJsonParseError err;
  QJsonDocument params_doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
  QJsonObject params;
  if (err.error == QJsonParseError::NoError && params_doc.isObject()) {
    params = params_doc.object();
  }

  QJsonObject root;
  root["request_id"] = requestId.isEmpty()
                           ? QString("qt-%1-%2")
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
                           : requestId;
  root["command"] = command;
  root["target"] = target;
  root["params"] = params;
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void CommandCenterWidget::PublishJson(const QString& json) {
  PUBLISH(MSG_ID_COMMAND_REQUEST, json.toStdString());
  AppendLog(tr("发送"), json);
}

void CommandCenterWidget::AppendLog(const QString& prefix, const QString& text) {
  if (!log_edit_) {
    return;
  }
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
  QString compact = text.simplified();
  if (compact.size() > 96) {
    compact = compact.left(93) + QStringLiteral("...");
  }
  static const int kMaxLogLines = 200;
  if (log_edit_->blockCount() > kMaxLogLines) {
    QTextCursor cursor = log_edit_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        log_edit_->blockCount() - kMaxLogLines);
    cursor.removeSelectedText();
  }
  log_edit_->appendPlainText(QString("[%1] %2  %3").arg(ts, prefix, compact));
}

void CommandCenterWidget::AppendResponse(const std::string& json) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &err);
  if (err.error == QJsonParseError::NoError && doc.isObject()) {
    const QJsonObject obj = doc.object();
    const bool success = obj.value("success").toBool(false);
    const QString command = obj.value("command").toString();
    const QString request_id = obj.value("request_id").toString();
    const QJsonObject details = obj.value("details").toObject();
    const bool profile_command = command == QStringLiteral("switch_nav_mode") ||
                                 command == QStringLiteral("switch_profile");
    AppContract::ProfileResponse profile_response =
        AppContract::ProfileResponse::Ignored;

    if (profile_command) {
      profile_response = profile_switch_tracker_.HandleResponse(request_id, success);
      if (profile_response == AppContract::ProfileResponse::Rejected) {
        pending_auto_mapping_start_ = false;
        SetNavigationModeText(active_workspace_mode_);
        SetStatusSummary(obj.value("message").toString(tr("模式切换失败")));
      }
    }

    if (!camera_start_request_id_.isEmpty() &&
        command == QStringLiteral("camera_start") &&
        request_id == camera_start_request_id_) {
      camera_start_pending_ = false;
      if (!success) {
        camera_waiting_first_frame_ = false;
        camera_start_request_id_.clear();
        SetCameraStateText(tr("启动失败"));
        SetStatusSummary(obj.value("message").toString(tr("摄像头启动失败")));
      } else {
        camera_waiting_first_frame_ = true;
        camera_start_request_id_.clear();
        SetCameraStateText(tr("等待首帧"));
        emit CameraViewRequested(true);
        const int generation = camera_start_generation_;
        QTimer::singleShot(8000, this, [this, generation]() {
          if (generation != camera_start_generation_ ||
              !camera_waiting_first_frame_) {
            return;
          }
          camera_waiting_first_frame_ = false;
          SetCameraStateText(tr("画面超时"));
          SetStatusSummary(tr("摄像头已启动，但 Qt 未收到图像"),
                           tr("请检查图像话题、网络带宽和 ROSBridge 订阅状态。"));
        });
      }
    }

    if (!success && command.startsWith(QStringLiteral("auto_mapping_")) &&
        request_id == auto_mapping_request_id_) {
      if (command == QStringLiteral("auto_mapping_start")) {
        auto_mapping_state_ = QStringLiteral("rejected");
      }
      if (auto_mapping_message_label_) {
        auto_mapping_message_label_->setText(
            obj.value("message").toString(tr("自动建图请求被拒绝")));
      }
      RefreshAutoMappingControls();
    }

    QString text = QString("%1\n命令: %2  目标: %3")
                       .arg(obj.value("message").toString())
                       .arg(command)
                       .arg(obj.value("target").toString());
    if (success && command == QStringLiteral("kimi_inspection")) {
      QStringList readings;
      if (details.contains("reading")) {
        readings << QStringLiteral("读数: %1").arg(details.value("reading").toVariant().toString());
      } else if (details.contains("value")) {
        readings << QStringLiteral("读数: %1").arg(details.value("value").toVariant().toString());
      }
      if (details.contains("status")) {
        readings << QStringLiteral("状态: %1").arg(details.value("status").toString());
      }
      if (details.contains("confidence")) {
        double conf = details.value("confidence").toDouble();
        readings << QStringLiteral("置信度: %1").arg(conf, 0, 'f', 2);
      }
      if (details.contains("unit")) {
        readings << QStringLiteral("单位: %1").arg(details.value("unit").toString());
      }
      if (details.contains("class_name")) {
        readings << QStringLiteral("类别: %1").arg(details.value("class_name").toString());
      }
      if (details.contains("summary")) {
        readings << QStringLiteral("摘要: %1").arg(details.value("summary").toString());
      }
      if (!readings.isEmpty()) {
        text += "\n" + readings.join(QStringLiteral(" . "));
      }
    }
    if (!success && obj.contains("details")) {
      text += "  ";
      text += QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact));
    }
    AppendLog(success ? tr("成功") : tr("失败"), text);
    if (profile_response == AppContract::ProfileResponse::Accepted) {
      const QString profile = profile_switch_tracker_.profile();
      if (profile == QStringLiteral("navigation") ||
          profile == QStringLiteral("inspection")) {
        // The board starts the camera as part of these profiles.  Open the
        // existing front-camera dock immediately so its rosbridge
        // subscription is active before the first frame arrives.
        ++camera_start_generation_;
        camera_frame_received_ = false;
        camera_waiting_first_frame_ = true;
        SetCameraStateText(tr("等待画面"));
        emit CameraViewRequested(true);
        const int generation = camera_start_generation_;
        QTimer::singleShot(10000, this, [this, generation]() {
          if (generation != camera_start_generation_ ||
              !camera_waiting_first_frame_) {
            return;
          }
          camera_waiting_first_frame_ = false;
          SetCameraStateText(tr("画面超时"));
          SetStatusSummary(tr("模式已切换，但 Qt 未收到摄像头首帧"));
        });
      }
      QTimer::singleShot(1500, this, &CommandCenterWidget::SendStatusRequest);
    }
    return;
  }
  AppendLog(tr("反馈"), QString::fromStdString(json));
}

void CommandCenterWidget::UpdateStatus(const std::string& json) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    SetStatusSummary(QString::fromStdString(json));
    return;
  }

  const QJsonObject obj = doc.object();
  const QJsonObject nodes = obj.value("nodes").toObject();
  auto is_online = [&nodes](const QString& name) { return nodes.value(name).toBool(false); };
  const QJsonObject camera = obj.value("camera").toObject();
  const bool camera_running = camera.value("running").toBool(false);
  if (!camera_running) {
    camera_frame_received_ = false;
  }
  if (!camera_start_pending_ && !camera_waiting_first_frame_) {
    SetCameraStateText(camera_running
                           ? (camera_frame_received_ ? tr("画面可用")
                                                     : tr("等待画面"))
                           : tr("未启动"));
  }

  const QStringList core_nodes = {
      QStringLiteral("/move_base"),
      QStringLiteral("/rplidarNode"),
      QStringLiteral("/stm32_base_driver"),
      QStringLiteral("/rosbridge_websocket"),
      QStringLiteral("/eggy_external_imu_odom_fuser"),
  };
  int online_count = 0;
  for (const auto& node : core_nodes) {
    if (is_online(node)) {
      ++online_count;
    }
  }

  const QJsonObject capabilities = obj.value("capabilities").toObject();
  const auto available = AppContract::ParseProfileAvailability(capabilities);
  mapping_profile_available_ = available.mapping;
  navigation_profile_available_ = available.navigation;
  inspection_profile_available_ = available.inspection;
  const QString state =
      obj.value("state").toString(QStringLiteral("degraded"));
  emit InspectionCapabilityChanged(
      state == QStringLiteral("ready") &&
      capabilities.value(QStringLiteral("inspection")).toBool(false));
  const QString mode = obj.value("mode").toString(tr("未知"));
  const QString profile = obj.value("profile").toString();
  if (profile_switch_tracker_.pending()) {
    if (profile_switch_tracker_.CompleteFromStatus(state, profile)) {
      SetNavigationModeText(mode);
    } else {
      SetNavigationModeText(QStringLiteral("switching"));
    }
  } else if (state == QStringLiteral("switching")) {
    SetNavigationModeText(QStringLiteral("switching"));
  } else {
    SetNavigationModeText(mode);
  }
  QString load_text = tr("-");
  const QJsonArray load = obj.value("loadavg").toArray();
  if (load.size() >= 3) {
    load_text = QStringLiteral("%1/%2/%3")
                    .arg(load.at(0).toDouble(), 0, 'f', 1)
                    .arg(load.at(1).toDouble(), 0, 'f', 1)
                    .arg(load.at(2).toDouble(), 0, 'f', 1);
  }

  const QString displayed_mode = nav_mode_label_ ? nav_mode_label_->text() : mode;
  const QString displayed_state = FriendlySystemState(state);
  const QString summary = tr("系统%1 · 核心节点 %2/%3")
                              .arg(displayed_state)
                              .arg(online_count)
                              .arg(core_nodes.size());

  QString detail;
  detail += tr("工作模式: %1\n").arg(displayed_mode);
  detail += tr("系统状态: %1\n").arg(displayed_state);
  detail += tr("负载: %1\n").arg(load_text);
  detail += tr("摄像头: %1  PID: %2\n")
                .arg(camera_running ? tr("在线") : tr("离线"))
                .arg(camera.value("pid").toString("-"));
  detail += tr("move_base: %1  雷达: %2  底盘: %3\n")
                .arg(is_online("/move_base") ? tr("在线") : tr("离线"),
                     is_online("/rplidarNode") ? tr("在线") : tr("离线"),
                     is_online("/stm32_base_driver") ? tr("在线") : tr("离线"));
  detail += tr("rosbridge: %1  Qt适配器: %2  里程计融合: %3")
                .arg(is_online("/rosbridge_websocket") ? tr("在线") : tr("离线"),
                     is_online("/ros_qt5_gui_adapter") ? tr("在线") : tr("离线"),
                     is_online("/eggy_external_imu_odom_fuser") ? tr("在线") : tr("离线"));
  if (!capabilities.isEmpty()) {
    detail += tr("\n能力: 建图%1 · 导航%2 · 巡检%3 · 重定位%4")
                  .arg(capabilities.value("mapping").toBool(false) ? tr("可用") : tr("不可用"),
                       capabilities.value("navigation").toBool(false) ? tr("可用") : tr("不可用"),
                       capabilities.value("inspection").toBool(false) ? tr("可用") : tr("不可用"),
                       capabilities.value("initialpose").toBool(false) ? tr("可用") : tr("不可用"));
  }
  SetStatusSummary(summary, detail);
  UpdateAutoMappingCard(obj.value(QStringLiteral("auto_mapping")).toObject());
}

void CommandCenterWidget::UpdateMotionOwner(const std::string& json) {
  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &error);
  AppContract::MotionOwnerStatus status;
  if (error.error == QJsonParseError::NoError && document.isObject()) {
    status = AppContract::ParseMotionOwner(
        document.object(), QDateTime::currentMSecsSinceEpoch() / 1000.0, 5.0);
  }
  SetMotionOwnerStatus(status);

  const int generation = ++motion_owner_generation_;
  const QString last_owner = status.owner;
  QTimer::singleShot(6000, this, [this, generation, last_owner]() {
    if (generation != motion_owner_generation_) {
      return;
    }
    SetMotionOwnerStatus({AppContract::MotionOwnerState::Stale, last_owner});
  });
}

void CommandCenterWidget::SetCameraStateText(const QString& text) {
  if (camera_state_label_) {
    camera_state_label_->setText(text);
  }
  const bool pending = camera_start_pending_ || camera_waiting_first_frame_;
  if (camera_start_btn_) {
    camera_start_btn_->setEnabled(!pending);
    camera_start_btn_->setText(tr("打开画面"));
  }
}

void CommandCenterWidget::SetOverviewPill(QLabel* label, const QString& title, const QString& value,
                                          const QString& color, const QString& bg, const QString& border) {
  if (!label) {
    return;
  }
  label->setText(QStringLiteral("<span style='font-size:%1px;color:%2;font-weight:600;'>%3</span>"
                                "&nbsp;&nbsp;<span style='font-size:%4px;color:%5;font-weight:800;'>%6</span>")
                     .arg(UiStyle::FontMiniPx())
                     .arg(UiStyle::Palette::TextSecondary)
                     .arg(title.toHtmlEscaped())
                     .arg(UiStyle::FontBasePx())
                     .arg(color)
                     .arg(value.toHtmlEscaped()));
  label->setMinimumHeight(44);
  label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  label->setStyleSheet(QStringLiteral(
                           "QLabel { background:%1; border:1px solid %2; border-radius:10px; padding:7px 10px; }")
                           .arg(bg, border));
}

void CommandCenterWidget::SetMetricPill(QLabel* label, const QString& title,
                                        const QString& value,
                                        const QString& color) {
  if (!label) {
    return;
  }
  label->setText(
      QStringLiteral("<span style='font-size:%1px;color:%2;font-weight:600;'>%3</span>"
                     "<br/><span style='font-size:%4px;color:%5;font-weight:800;'>%6</span>")
          .arg(UiStyle::FontMiniPx())
          .arg(UiStyle::Palette::TextSecondary)
          .arg(title.toHtmlEscaped())
          .arg(UiStyle::FontSmallPx())
          .arg(color)
          .arg(value.toHtmlEscaped()));
  label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  label->setMinimumHeight(54);
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  label->setStyleSheet(QStringLiteral(
                           "QLabel { background:%1; border:1px solid %2; border-radius:10px; padding:7px 9px; }")
                           .arg(UiStyle::Palette::SurfaceAlt,
                                UiStyle::Palette::Border));
}

void CommandCenterWidget::SetStatusSummary(const QString& text, const QString& detail) {
  if (!status_summary_label_) {
    return;
  }
  status_summary_label_->setText(text.trimmed().isEmpty() ? tr("暂无状态") : text.trimmed());
  status_summary_label_->setToolTip(detail.trimmed().isEmpty() ? text : detail);
}

void CommandCenterWidget::SendStatusRequest() {
  PublishJson(MakeRequestJson("status", "system"));
}

void CommandCenterWidget::StartAutoMapping() {
  if (active_workspace_mode_ != QStringLiteral("mapping_slam")) {
    pending_auto_mapping_start_ = false;
    return;
  }
  if (auto_mapping_state_ != QStringLiteral("idle") &&
      auto_mapping_state_ != QStringLiteral("completed") &&
      auto_mapping_state_ != QStringLiteral("cancelled") &&
      auto_mapping_state_ != QStringLiteral("aborted") &&
      auto_mapping_state_ != QStringLiteral("rejected")) {
    return;
  }
  if (!ConfirmAutoMappingAction(
          this, tr("开始自动建图"),
          tr("小车将自主移动并保存新地图。请确认周围无人、无悬空台阶，急停可随时使用。\n\n是否开始？"),
          tr("开始建图"))) {
    return;
  }
  auto_mapping_request_id_ = QStringLiteral("qt-auto-map-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  SendAutoMappingCommand(QStringLiteral("start"));
}

void CommandCenterWidget::PauseResumeAutoMapping() {
  if (auto_mapping_state_ == QStringLiteral("paused")) {
    SendAutoMappingCommand(QStringLiteral("resume"));
  } else {
    SendAutoMappingCommand(QStringLiteral("pause"));
  }
}

void CommandCenterWidget::StopAutoMapping() {
  if (!ConfirmAutoMappingAction(
          this, tr("停止自动建图"),
          tr("小车会立即停止，当前地图将作为草稿保存。确定停止吗？"),
          tr("停止并保存"))) {
    return;
  }
  SendAutoMappingCommand(QStringLiteral("stop"));
}

void CommandCenterWidget::SendAutoMappingCommand(const QString& command) {
  if (auto_mapping_request_id_.isEmpty()) {
    auto_mapping_request_id_ = QStringLiteral("qt-auto-map-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  }
  QJsonObject params;
  if (command == QStringLiteral("start")) {
    params[QStringLiteral("max_duration_sec")] = auto_mapping_duration_spin_->value() * 60;
    params[QStringLiteral("max_linear_speed")] = auto_mapping_speed_spin_->value();
    params[QStringLiteral("return_home")] = auto_mapping_return_home_check_->isChecked();
    params[QStringLiteral("save_draft_on_abort")] = true;
  }
  PublishJson(MakeRequestJson(
      QStringLiteral("auto_mapping_%1").arg(command), QStringLiteral("auto_mapping"),
      QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)),
      auto_mapping_request_id_));
  if (command == QStringLiteral("start")) {
    auto_mapping_state_ = QStringLiteral("preflight");
    RefreshAutoMappingControls();
  }
  auto_mapping_message_label_->setText(
      command == QStringLiteral("start") ? tr("启动请求已发送，等待板端安全检查…")
                                         : tr("%1 请求已发送…").arg(command));
}

void CommandCenterWidget::RefreshAutoMappingControls() {
  if (!auto_mapping_start_btn_) return;
  const bool active = auto_mapping_state_ == QStringLiteral("preflight") ||
                      auto_mapping_state_ == QStringLiteral("planning") ||
                      auto_mapping_state_ == QStringLiteral("navigating") ||
                      auto_mapping_state_ == QStringLiteral("observing") ||
                      auto_mapping_state_ == QStringLiteral("returning") ||
                      auto_mapping_state_ == QStringLiteral("final_scan") ||
                      auto_mapping_state_ == QStringLiteral("saving");
  const bool paused = auto_mapping_state_ == QStringLiteral("paused");
  const bool mapping_mode_active =
      active_workspace_mode_ == QStringLiteral("mapping_slam");
  auto_mapping_start_btn_->setEnabled(mapping_mode_active &&
                                      mapping_profile_available_ && !active && !paused &&
                                      !profile_switch_tracker_.pending());
  auto_mapping_pause_btn_->setEnabled(active || paused);
  auto_mapping_pause_btn_->setText(paused ? tr("继续") : tr("暂停"));
  auto_mapping_stop_btn_->setEnabled(active || paused);
  auto_mapping_duration_spin_->setEnabled(mapping_mode_active && !active && !paused);
  auto_mapping_speed_spin_->setEnabled(mapping_mode_active && !active && !paused);
  auto_mapping_return_home_check_->setEnabled(mapping_mode_active && !active && !paused);
  const bool show_runtime = active || paused ||
                            auto_mapping_state_ == QStringLiteral("completed") ||
                            auto_mapping_state_ == QStringLiteral("cancelled") ||
                            auto_mapping_state_ == QStringLiteral("aborted") ||
                            auto_mapping_state_ == QStringLiteral("rejected");
  auto_mapping_message_label_->setVisible(show_runtime);
  auto_mapping_progress_->setVisible(show_runtime);
  auto_mapping_frontier_metric_->setVisible(show_runtime);
  auto_mapping_sensor_metric_->setVisible(show_runtime);
  auto_mapping_safety_metric_->setVisible(show_runtime);
}

void CommandCenterWidget::UpdateAutoMappingCard(const QJsonObject& status) {
  if (!auto_mapping_state_label_ || status.isEmpty()) return;
  const QString state = status.value(QStringLiteral("state")).toString(QStringLiteral("idle"));
  auto_mapping_state_ = state;
  const QString reported_request_id =
      status.value(QStringLiteral("request_id")).toString().trimmed();
  if (!reported_request_id.isEmpty()) {
    auto_mapping_request_id_ = reported_request_id;
  }
  const QHash<QString, QString> labels = {
      {QStringLiteral("idle"), tr("未启动")}, {QStringLiteral("preflight"), tr("安全检查")}, {QStringLiteral("planning"), tr("选择边界")}, {QStringLiteral("navigating"), tr("自主移动")}, {QStringLiteral("observing"), tr("更新地图")}, {QStringLiteral("returning"), tr("返回起点")}, {QStringLiteral("final_scan"), tr("最终扫描")}, {QStringLiteral("saving"), tr("保存地图")}, {QStringLiteral("paused"), tr("已暂停")}, {QStringLiteral("completed"), tr("已完成")}, {QStringLiteral("cancelled"), tr("已停止")}, {QStringLiteral("aborted"), tr("异常终止")}, {QStringLiteral("rejected"), tr("未启动")}};
  const QString label = labels.value(state, state);
  const bool danger = state == QStringLiteral("aborted") || state == QStringLiteral("rejected");
  const bool success = state == QStringLiteral("completed");
  const bool warning = state == QStringLiteral("paused") || state == QStringLiteral("cancelled");
  const QString color = danger    ? UiStyle::Palette::Danger
                        : success ? UiStyle::Palette::Success
                        : warning ? UiStyle::Palette::Warning
                                  : UiStyle::Palette::Info;
  const QString bg = danger    ? UiStyle::Palette::DangerBg
                     : success ? UiStyle::Palette::SuccessBg
                     : warning ? UiStyle::Palette::WarningBg
                               : UiStyle::Palette::InfoBg;
  const QString border = danger    ? UiStyle::Palette::DangerBorder
                         : success ? UiStyle::Palette::SuccessBorder
                         : warning ? UiStyle::Palette::WarningBorder
                                   : UiStyle::Palette::InfoBorder;
  auto_mapping_state_label_->setText(label);
  auto_mapping_state_label_->setStyleSheet(QStringLiteral(
                                               "QLabel { color:%1; background:%2; border:1px solid %3; border-radius:9px; "
                                               "padding:6px 10px; font-size:%4px; font-weight:700; }")
                                               .arg(color, bg, border)
                                               .arg(UiStyle::FontSmallPx()));
  auto_mapping_message_label_->setText(FriendlyAutoMappingMessage(
      status.value(QStringLiteral("message")).toString(), state));

  const double elapsed = status.value(QStringLiteral("elapsed_sec")).toDouble();
  const QJsonObject options = status.value(QStringLiteral("options")).toObject();
  const int duration = qMax(
      60, options.value(QStringLiteral("max_duration_sec"))
              .toInt(auto_mapping_duration_spin_->value() * 60));
  auto_mapping_progress_->setValue(qBound(0, qRound(elapsed * 100.0 / duration), 100));
  auto_mapping_progress_->setFormat(tr("已运行 %1 分钟 · 上限 %2 分钟")
                                        .arg(elapsed / 60.0, 0, 'f', 1)
                                        .arg(duration / 60));
  const QJsonObject ages = status.value(QStringLiteral("ages")).toObject();
  const QJsonObject safety = status.value(QStringLiteral("safety")).toObject();
  auto ageText = [&ages](const QString& key) {
    const QJsonValue value = ages.value(key);
    return value.isDouble() ? QStringLiteral("%1s").arg(value.toDouble(), 0, 'f', 1)
                            : QStringLiteral("—");
  };
  const QString map_age = ageText(QStringLiteral("map"));
  const QString scan_age = ageText(QStringLiteral("scan"));
  const QString odom_age = ageText(QStringLiteral("odom"));
  const QString safety_reason =
      safety.value(QStringLiteral("reason")).toString(tr("等待数据"));
  const bool sensors_ready = map_age != QStringLiteral("—") &&
                             scan_age != QStringLiteral("—") &&
                             odom_age != QStringLiteral("—");
  const bool safety_ok = safety_reason == QStringLiteral("ok") ||
                         safety_reason == QStringLiteral("ready") ||
                         safety_reason == QStringLiteral("正常");
  SetMetricPill(auto_mapping_frontier_metric_, tr("可达边界"),
                tr("%1 个").arg(status.value(QStringLiteral("reachable_frontiers")).toInt()),
                UiStyle::Palette::Info);
  SetMetricPill(auto_mapping_sensor_metric_, tr("地图 / 雷达 / 里程计"),
                tr("%1 / %2 / %3").arg(map_age, scan_age, odom_age),
                sensors_ready ? UiStyle::Palette::Success : UiStyle::Palette::Warning);
  SetMetricPill(auto_mapping_safety_metric_, tr("安全联锁"),
                safety_ok ? tr("正常") : safety_reason,
                safety_ok ? UiStyle::Palette::Success : UiStyle::Palette::Warning);
  const QJsonObject result = status.value(QStringLiteral("result")).toObject();
  if (!result.value(QStringLiteral("map_id")).toString().isEmpty()) {
    auto_mapping_message_label_->setText(
        tr("地图已保存：%1").arg(result.value(QStringLiteral("map_id")).toString()));
  }
  RefreshAutoMappingControls();
}

void CommandCenterWidget::StartAmclNavigation() {
  BeginProfileSwitch(QStringLiteral("navigation"), QStringLiteral("navigation"));
}

void CommandCenterWidget::SetMotionOwnerStatus(
    const AppContract::MotionOwnerStatus& status) {
  if (status.state == AppContract::MotionOwnerState::Known) {
    const QHash<QString, QString> owner_labels = {
        {QStringLiteral("navigation"), tr("导航栈")},
        {QStringLiteral("mapping"), tr("自动建图")},
        {QStringLiteral("mission"), tr("任务执行器")},
        {QStringLiteral("manual"), tr("手动控制")},
        {QStringLiteral("safety"), tr("安全控制")},
        {QStringLiteral("none"), tr("小车静止")},
    };
    const QString owner = owner_labels.value(status.owner, status.owner);
    SetOverviewPill(motion_owner_label_, tr("运动控制"), owner,
                    UiStyle::Palette::Info, UiStyle::Palette::InfoBg,
                    UiStyle::Palette::InfoBorder);
    return;
  }
  if (status.state == AppContract::MotionOwnerState::Stale) {
    const QString value = status.owner.isEmpty()
                              ? tr("状态过期")
                              : tr("%1（状态过期）").arg(status.owner);
    SetOverviewPill(motion_owner_label_, tr("运动控制"), value,
                    UiStyle::Palette::Warning, UiStyle::Palette::WarningBg,
                    UiStyle::Palette::WarningBorder);
    return;
  }
  SetOverviewPill(motion_owner_label_, tr("运动控制"), tr("未报告"),
                  UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt,
                  UiStyle::Palette::Border);
}

void CommandCenterWidget::SwitchToMapping() {
  BeginProfileSwitch(QStringLiteral("mapping"), QStringLiteral("mapping"));
}

void CommandCenterWidget::StartInspection() {
  BeginProfileSwitch(QStringLiteral("inspection"), QStringLiteral("inspection"));
}

void CommandCenterWidget::BeginProfileSwitch(const QString& profile,
                                             const QString& target) {
  if (external_profile_switch_busy_) {
    SetStatusSummary(tr("地图切换尚未完成，请稍候"));
    return;
  }
  const QString request_id = QStringLiteral("qt-profile-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  if (!profile_switch_tracker_.Begin(profile, request_id)) {
    return;
  }
  QJsonObject params;
  params[QStringLiteral("profile")] = profile;
  PublishJson(MakeRequestJson("switch_profile", target,
                              QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)),
                              request_id));
  SetNavigationModeText(QStringLiteral("switching"));
  QTimer::singleShot(25000, this, [this, request_id]() {
    if (!profile_switch_tracker_.Timeout(request_id)) {
      return;
    }
    pending_auto_mapping_start_ = false;
    SetNavigationModeText(active_workspace_mode_);
    SetStatusSummary(tr("模式切换超时，请检查板端状态后重试"));
  });
}

void CommandCenterWidget::StartCamera() {
  if (camera_start_pending_ || camera_waiting_first_frame_) {
    return;
  }
  ++camera_start_generation_;
  camera_frame_received_ = false;
  camera_start_request_id_ = QStringLiteral("qt-camera-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  camera_start_pending_ = true;
  SetCameraStateText(tr("正在启动"));
  SetStatusSummary(tr("正在启动摄像头并检查图像流…"));
  PublishJson(MakeRequestJson(QStringLiteral("camera_start"),
                              QStringLiteral("camera"), QStringLiteral("{}"),
                              camera_start_request_id_));

  const QString request_id = camera_start_request_id_;
  const int generation = camera_start_generation_;
  QTimer::singleShot(35000, this, [this, request_id, generation]() {
    if (generation != camera_start_generation_ || !camera_start_pending_ ||
        camera_start_request_id_ != request_id) {
      return;
    }
    camera_start_pending_ = false;
    camera_start_request_id_.clear();
    SetCameraStateText(tr("启动超时"));
    SetStatusSummary(tr("摄像头启动超时，请检查板端摄像头服务"));
  });
}

void CommandCenterWidget::NotifyCameraFrameReceived() {
  if (!camera_waiting_first_frame_) {
    return;
  }
  camera_waiting_first_frame_ = false;
  camera_frame_received_ = true;
  SetCameraStateText(tr("画面在线"));
  SetStatusSummary(tr("摄像头画面已连接"));
}

void CommandCenterWidget::ToggleLog() {
  if (!log_edit_) {
    return;
  }
  const bool show = !log_edit_->isVisible();
  log_edit_->setVisible(show);
  if (log_toggle_btn_) {
    log_toggle_btn_->setText(show ? tr("收起") : tr("日志"));
  }
  if (clear_log_btn_) {
    clear_log_btn_->setVisible(show);
  }
}

void CommandCenterWidget::ClearLog() {
  if (log_edit_) {
    log_edit_->clear();
  }
}
