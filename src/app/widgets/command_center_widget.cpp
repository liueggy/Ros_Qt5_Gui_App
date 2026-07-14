#include "widgets/command_center_widget.h"

#include <QByteArray>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QTextCursor>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "core/framework/framework.h"
#include "msg/channel_publish_result.h"
#include "msg/msg_info.h"
#include "widgets/diagnostic_dock_widget.h"
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

  auto* overview_group = new QFrame(this);
  overview_group->setProperty("uiCard", true);
  overview_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* overview_layout = new QGridLayout(overview_group);
  overview_layout->setContentsMargins(14, 14, 14, 14);
  overview_layout->setHorizontalSpacing(12);
  overview_layout->setVerticalSpacing(12);
  connection_overview_label_ = new QLabel(overview_group);
  nav_overview_label_ = new QLabel(overview_group);
  task_overview_label_ = new QLabel(overview_group);
  diagnostic_overview_label_ = new QLabel(overview_group);
  motion_owner_label_ = new QLabel(overview_group);
  overview_layout->addWidget(connection_overview_label_, 0, 0);
  overview_layout->addWidget(nav_overview_label_, 0, 1);
  overview_layout->addWidget(task_overview_label_, 1, 0);
  overview_layout->addWidget(diagnostic_overview_label_, 1, 1);
  overview_layout->addWidget(motion_owner_label_, 2, 0, 1, 2);
  root->addWidget(overview_group);
  SetConnectionOverview(false, tr("未连接"));
  SetOverviewPill(nav_overview_label_, tr("导航"), tr("未连接"),
                  UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt, UiStyle::Palette::Border);
  SetOverviewPill(task_overview_label_, tr("任务"), tr("空闲"),
                  UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt, UiStyle::Palette::Border);
  SetDiagnosticOverview(0, 0, 0);
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
                                         .arg(UiStyle::Palette::TextSecondary).arg(UiStyle::Palette::SurfaceAlt).arg(UiStyle::Palette::Border).arg(UiStyle::FontSmallPx()));
  camera_header->addWidget(camera_title);
  camera_header->addStretch();
  camera_header->addWidget(camera_state_label_);
  camera_header->addWidget(camera_start_btn_);
  camera_layout->addLayout(camera_header);
  camera_inspection_label_ = new QLabel(camera_group);
  camera_inspection_label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  camera_inspection_label_->setStyleSheet(QStringLiteral(
      "QLabel { color:%1; background:%2; border:1px solid %3; "
      "border-radius:8px; padding:8px 10px; font-size:%4px; font-weight:600; }")
      .arg(UiStyle::Palette::TextSecondary).arg(UiStyle::Palette::SurfaceAlt).arg(UiStyle::Palette::Border).arg(UiStyle::FontMiniPx()));
  camera_inspection_label_->setVisible(false);
  camera_layout->addWidget(camera_inspection_label_);
  root->addWidget(camera_group);

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
  root->addWidget(network_group);

  auto* nav_group = new QFrame(this);
  nav_group->setProperty("uiCard", true);
  nav_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* nav_layout = new QVBoxLayout(nav_group);
  nav_layout->setContentsMargins(16, 14, 16, 16);
  nav_layout->setSpacing(10);
  auto* nav_header = new QHBoxLayout();
  nav_header->setSpacing(8);
  auto* nav_title = new QLabel(tr("导航模式"), nav_group);
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
                                     .arg(UiStyle::Palette::TextSecondary).arg(UiStyle::Palette::SurfaceAlt).arg(UiStyle::Palette::Border).arg(UiStyle::FontSmallPx()));
  nav_header->addWidget(nav_title);
  nav_header->addStretch();
  nav_header->addWidget(nav_mode_label_);
  nav_layout->addLayout(nav_header);

  auto* nav_row = new QHBoxLayout();
  nav_row->setSpacing(10);
  mapping_btn_ = new QPushButton(tr("建图模式"), nav_group);
  amcl_btn_ = new QPushButton(tr("AMCL导航"), nav_group);
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

  root->addWidget(nav_group);

  connect(mapping_btn_, &QPushButton::clicked, this, &CommandCenterWidget::SwitchToMapping);
  connect(amcl_btn_, &QPushButton::clicked, this, &CommandCenterWidget::StartAmclNavigation);
  connect(inspection_btn_, &QPushButton::clicked, this, &CommandCenterWidget::StartInspection);
  mapping_btn_->setEnabled(false);
  amcl_btn_->setEnabled(false);
  inspection_btn_->setEnabled(false);

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
  auto* clear_btn = new QPushButton(tr("清空"), status_group);
  refresh_status_btn->setFixedWidth(76);
  clear_btn->setFixedWidth(76);
  status_header->addWidget(status_title);
  status_header->addStretch();
  status_header->addWidget(refresh_status_btn);
  status_header->addWidget(clear_btn);
  status_layout->addLayout(status_header);
  status_summary_label_ = new QLabel(tr("暂无状态"), status_group);
  status_summary_label_->setWordWrap(true);
  status_summary_label_->setMinimumHeight(46);
  status_summary_label_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  status_summary_label_->setStyleSheet(QStringLiteral(
                                           "QLabel { color:%1; background:%2; border:1px solid %3; "
                                           "border-radius:12px; padding:10px 12px; font-size:%4px; }")
                                           .arg(UiStyle::Palette::TextSecondary).arg(UiStyle::Palette::SurfaceAlt).arg(UiStyle::Palette::Border).arg(UiStyle::FontSmallPx()));
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
  connect(clear_btn, &QPushButton::clicked, this, &CommandCenterWidget::ClearLog);

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
    QMetaObject::invokeMethod(this, [this, json]() { AppendResponse(json); }, Qt::QueuedConnection);
  });
  SUBSCRIBE_QOBJECT(this, MSG_ID_COMMAND_STATUS, [this](const std::string& json) {
    QMetaObject::invokeMethod(this, [this, json]() { UpdateStatus(json); }, Qt::QueuedConnection);
  });
  SUBSCRIBE_QOBJECT(this, MSG_ID_CMD_VEL_CONTROL, [this](const std::string& json) {
    QMetaObject::invokeMethod(this, [this, json]() { UpdateMotionOwner(json); },
                              Qt::QueuedConnection);
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
              if (!profile_switch_tracker_.Timeout(request_id)) {
                return;
              }
              SetNavigationModeText(active_workspace_mode_);
              SetStatusSummary(tr("模式切换请求发送失败"),
                               QString::fromStdString(result.message));
              AppendLog(tr("失败"), tr("命令未发送到小车端，请检查连接。"));
            },
            Qt::QueuedConnection);
      });
}

void CommandCenterWidget::SetDiagnosticSnapshot(const basic::DiagnosticSnapshot& snapshot) {
  if (diagnostic_widget_) {
    diagnostic_widget_->SetSnapshot(snapshot);
  }
  int total = 0;
  int abnormal = 0;
  int worst_level = 0;
  auto rank = [](int level) {
    switch (level) {
      case 2: return 0;
      case 3: return 1;
      case 1: return 2;
      case 0: return 3;
      default: return 0;
    }
  };
  for (const auto& hardware : snapshot.hardware) {
    for (const auto& component : hardware.second) {
      ++total;
      if (component.second.level != 0) {
        ++abnormal;
      }
      if (rank(component.second.level) < rank(worst_level)) {
        worst_level = component.second.level;
      }
    }
  }
  if (diagnostic_group_) {
    diagnostic_group_->setVisible(total > 0);
  }
  SetDiagnosticOverview(total, abnormal, worst_level);
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
  const bool connected = wifi_connected || cellular_connected;
  const QString detail = wifi_connected
                             ? (wifi_name.isEmpty() ? tr("WiFi 已连接") : wifi_name)
                             : (cellular_connected ? tr("4G 已连接") : tr("未连接"));
  SetConnectionOverview(connected, detail);
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
    text = tr("建图模式");
    color = UiStyle::Palette::Warning;
    bg = UiStyle::Palette::WarningBg;
    border = UiStyle::Palette::WarningBorder;
  } else if (normalized == QStringLiteral("static_nav")) {
    text = tr("AMCL导航");
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
    mapping_btn_->setText(active ? tr("建图中") : tr("建图模式"));
    mapping_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                             mapping_profile_available_ && !active);
    mapping_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                       : UiStyle::SecondaryButtonStyleSheet());
  }
  if (amcl_btn_) {
    const bool active = normalized == QStringLiteral("static_nav");
    amcl_btn_->setText(active ? tr("AMCL运行中") : tr("AMCL导航"));
    amcl_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                          navigation_profile_available_ && !active);
    amcl_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                     : UiStyle::SecondaryButtonStyleSheet());
  }
  if (inspection_btn_) {
    const bool active = normalized == QStringLiteral("inspection");
    inspection_btn_->setText(active ? tr("巡检运行中") : tr("巡检模式"));
    inspection_btn_->setEnabled(!profile_switch_tracker_.pending() &&
                                inspection_profile_available_ && !active);
    inspection_btn_->setStyleSheet(active ? UiStyle::MainButtonStyleSheet()
                                           : UiStyle::SecondaryButtonStyleSheet());
  }
  SetOverviewPill(nav_overview_label_, tr("导航"), text, color, bg, border);
  if ((normalized == QStringLiteral("mapping_slam") ||
       normalized == QStringLiteral("static_nav") ||
       normalized == QStringLiteral("inspection")) &&
      normalized != active_workspace_mode_) {
    active_workspace_mode_ = normalized;
    emit WorkspaceModeRequested(normalized);
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
  log_edit_->setVisible(true);
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
        SetNavigationModeText(active_workspace_mode_);
        SetStatusSummary(obj.value("message").toString(tr("模式切换失败")));
      }
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
  SetCameraStateText(camera_running ? tr("摄像头在线") : tr("摄像头离线"));

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

  const QString summary =
      tr("模式 %1 · 状态 %2 · 负载 %3 · 摄像头 %4 · 核心节点 %5/%6")
          .arg(mode, state, load_text, camera_running ? tr("在线") : tr("离线"))
          .arg(online_count)
          .arg(core_nodes.size());

  QString detail;
  detail += tr("模式: %1\n").arg(mode);
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
  SetOverviewPill(task_overview_label_, tr("任务"), online_count == core_nodes.size() ? tr("可执行") : tr("待检查"),
                  online_count == core_nodes.size() ? UiStyle::Palette::Success : UiStyle::Palette::Warning,
                  online_count == core_nodes.size() ? UiStyle::Palette::SuccessBg : UiStyle::Palette::WarningBg,
                  online_count == core_nodes.size() ? UiStyle::Palette::SuccessBorder : UiStyle::Palette::WarningBorder);
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

void CommandCenterWidget::SetCameraInspectionResult(const QString& type,
                                             const QString& reading,
                                             const QString& status) {
  if (!camera_inspection_label_) {
    return;
  }
  if (reading.isEmpty() && status.isEmpty()) {
    camera_inspection_label_->setVisible(false);
    return;
  }
  const bool normal = (status == QStringLiteral("正常"));
  const bool abnormal = (status == QStringLiteral("异常"));
  const QString color = normal ? UiStyle::Palette::Success
                        : abnormal ? UiStyle::Palette::Danger
                        : UiStyle::Palette::Warning;
  const QString bg = normal ? UiStyle::Palette::SuccessBg
                     : abnormal ? UiStyle::Palette::DangerBg
                     : UiStyle::Palette::WarningBg;
  const QString border = normal ? UiStyle::Palette::SuccessBorder
                         : abnormal ? UiStyle::Palette::DangerBorder
                         : UiStyle::Palette::WarningBorder;
  camera_inspection_label_->setStyleSheet(QStringLiteral(
      "QLabel { color:%1; background:%2; border:1px solid %3; "
      "border-radius:8px; padding:8px 10px; font-size:%4px; font-weight:700; }")
      .arg(color).arg(bg).arg(border).arg(UiStyle::FontMiniPx()));
  const QString displayReading = reading.isEmpty() ? QStringLiteral("未识别") : reading;
  const QString displayStatus = status.isEmpty() ? QStringLiteral("未识别") : status;
  camera_inspection_label_->setText(
      QStringLiteral("AI识别结果\n%1：%2\n状态：%3")
          .arg(type.isEmpty() ? QStringLiteral("水表") : type, displayReading, displayStatus));
  camera_inspection_label_->setVisible(true);
}

void CommandCenterWidget::SetCameraStateText(const QString& text) {
  if (camera_state_label_) {
    camera_state_label_->setText(text);
  }
  const bool pending = text.contains(tr("正在"));
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
  label->setText(QStringLiteral("<span style='font-size:%1px;color:%2;font-weight:600;'>%3</span><br/><span style='font-size:%4px;color:%5;font-weight:800;'>%6</span>")
                     .arg(UiStyle::FontMiniPx())
                     .arg(UiStyle::Palette::TextSecondary)
                     .arg(title.toHtmlEscaped())
                     .arg(UiStyle::FontBasePx())
                     .arg(color)
                     .arg(value.toHtmlEscaped()));
  label->setMinimumHeight(58);
  label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  label->setStyleSheet(QStringLiteral(
      "QLabel { background:%1; border:1px solid %2; border-radius:12px; padding:9px 11px; }")
                           .arg(bg, border));
}

void CommandCenterWidget::SetConnectionOverview(bool online, const QString& detail) {
  SetOverviewPill(connection_overview_label_, tr("连接"),
                  online ? detail : tr("等待连接"),
                  online ? UiStyle::Palette::Success : UiStyle::Palette::TextSecondary,
                  online ? UiStyle::Palette::SuccessBg : UiStyle::Palette::SurfaceAlt,
                  online ? UiStyle::Palette::SuccessBorder : UiStyle::Palette::Border);
}

void CommandCenterWidget::SetDiagnosticOverview(int total, int abnormal, int worstLevel) {
  QString color = UiStyle::Palette::TextSecondary;
  QString bg = UiStyle::Palette::SurfaceAlt;
  QString border = UiStyle::Palette::Border;
  QString value = total == 0 ? tr("暂无数据") : tr("%1 异常 / %2").arg(abnormal).arg(total);
  if (total > 0 && abnormal == 0) {
    color = UiStyle::Palette::Success;
    bg = UiStyle::Palette::SuccessBg;
    border = UiStyle::Palette::SuccessBorder;
    value = tr("全部正常");
  } else if (worstLevel == 1) {
    color = UiStyle::Palette::Warning;
    bg = UiStyle::Palette::WarningBg;
    border = UiStyle::Palette::WarningBorder;
  } else if (worstLevel == 2 || worstLevel == 3) {
    color = UiStyle::Palette::Danger;
    bg = UiStyle::Palette::DangerBg;
    border = UiStyle::Palette::DangerBorder;
  }
  SetOverviewPill(diagnostic_overview_label_, tr("诊断"), value, color, bg, border);
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

void CommandCenterWidget::StartAmclNavigation() {
  BeginProfileSwitch(QStringLiteral("navigation"), QStringLiteral("navigation"));
}

void CommandCenterWidget::SetMotionOwnerStatus(
    const AppContract::MotionOwnerStatus& status) {
  if (status.state == AppContract::MotionOwnerState::Known) {
    const QHash<QString, QString> owner_labels = {
        {QStringLiteral("navigation"), tr("导航栈")},
        {QStringLiteral("mission"), tr("任务执行器")},
        {QStringLiteral("manual"), tr("手动控制")},
        {QStringLiteral("safety"), tr("安全控制")},
        {QStringLiteral("none"), tr("无（小车静止）")},
    };
    const QString owner = owner_labels.value(status.owner, status.owner);
    SetOverviewPill(motion_owner_label_, tr("当前运动控制者"), owner,
                    UiStyle::Palette::Info, UiStyle::Palette::InfoBg,
                    UiStyle::Palette::InfoBorder);
    return;
  }
  if (status.state == AppContract::MotionOwnerState::Stale) {
    const QString value = status.owner.isEmpty()
                              ? tr("状态过期")
                              : tr("%1（状态过期）").arg(status.owner);
    SetOverviewPill(motion_owner_label_, tr("当前运动控制者"), value,
                    UiStyle::Palette::Warning, UiStyle::Palette::WarningBg,
                    UiStyle::Palette::WarningBorder);
    return;
  }
  SetOverviewPill(motion_owner_label_, tr("当前运动控制者"), tr("未知（板端未报告）"),
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
  const QString request_id = QStringLiteral("qt-profile-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  if (!profile_switch_tracker_.Begin(profile, request_id)) {
    return;
  }
  QJsonObject params;
  params[QStringLiteral("profile")] = profile;
  PublishJson(MakeRequestJson("switch_profile", target,
                              QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)),
                              request_id));
  SetNavigationModeText(QStringLiteral("switching"));
  QTimer::singleShot(30000, this, [this, request_id]() {
    if (!profile_switch_tracker_.Timeout(request_id)) {
      return;
    }
    SetNavigationModeText(active_workspace_mode_);
    SetStatusSummary(tr("模式切换超时，请检查板端状态后重试"));
  });
}

void CommandCenterWidget::StartCamera() {
  emit CameraViewRequested(true);
}

void CommandCenterWidget::ClearLog() {
  if (log_edit_) {
    log_edit_->clear();
    log_edit_->setVisible(false);
  }
}
