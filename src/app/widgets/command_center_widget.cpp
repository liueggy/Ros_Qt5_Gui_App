#include "widgets/command_center_widget.h"

#include <QByteArray>
#include <QDateTime>
#include <QFrame>
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
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "core/framework/framework.h"
#include "msg/msg_info.h"
#include "widgets/diagnostic_dock_widget.h"
#include "widgets/ui_style.h"

namespace {

QLabel* AddCardTitle(QVBoxLayout* layout, const QString& text, QWidget* parent) {
  auto* title = new QLabel(text, parent);
  title->setStyleSheet(QStringLiteral(
                           "QLabel { color:#18212f; font-size:%1px; font-weight:700; "
                           "padding:0 0 4px 0; background:transparent; border:none; }")
                           .arg(UiStyle::FontBasePx()));
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
  root->setContentsMargins(18, 18, 18, 18);
  root->setSpacing(14);
  scroll->setWidget(body);
  outer->addWidget(scroll);

  setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::SecondaryButtonStyleSheet() + UiStyle::InputStyleSheet());

  auto* title = new QLabel(tr("运行控制"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  title->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  root->addWidget(title);
  auto* camera_group = new QFrame(this);
  camera_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* camera_layout = new QVBoxLayout(camera_group);
  camera_layout->setContentsMargins(16, 14, 16, 16);
  camera_layout->setSpacing(10);
  auto* camera_header = new QHBoxLayout();
  camera_header->setSpacing(8);
  auto* camera_title = new QLabel(tr("摄像头"), camera_group);
  camera_title->setStyleSheet(QStringLiteral(
                                  "QLabel { color:#18212f; font-size:%1px; font-weight:700; background:transparent; border:none; }")
                                  .arg(UiStyle::FontBasePx()));
  camera_state_label_ = new QLabel(tr("等待刷新"), camera_group);
  camera_state_label_->setAlignment(Qt::AlignCenter);
  camera_state_label_->setStyleSheet(QStringLiteral(
                                         "QLabel { color:#657386; background:#f6f9fe; border:1px solid #dce6f5; "
                                         "border-radius:9px; padding:6px 10px; font-size:%1px; font-weight:700; }")
                                         .arg(UiStyle::FontSmallPx()));
  camera_header->addWidget(camera_title);
  camera_header->addStretch();
  camera_header->addWidget(camera_state_label_);
  camera_layout->addLayout(camera_header);
  auto* camera_row = new QHBoxLayout();
  camera_row->setSpacing(10);
  auto* camera_start_btn = new QPushButton(tr("启动摄像头"), camera_group);
  auto* camera_stop_btn = new QPushButton(tr("停止摄像头"), camera_group);
  camera_start_btn->setStyleSheet(UiStyle::MainButtonStyleSheet());
  camera_stop_btn->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  camera_row->addWidget(camera_start_btn);
  camera_row->addWidget(camera_stop_btn);
  camera_row->addStretch();
  camera_layout->addLayout(camera_row);
  root->addWidget(camera_group);

  connect(camera_start_btn, &QPushButton::clicked, this, &CommandCenterWidget::StartCamera);
  connect(camera_stop_btn, &QPushButton::clicked, this, &CommandCenterWidget::StopCamera);

  auto* network_group = new QFrame(this);
  network_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* network_layout = new QVBoxLayout(network_group);
  network_layout->setContentsMargins(16, 12, 16, 14);
  network_layout->setSpacing(8);
  AddCardTitle(network_layout, tr("网络状态"), network_group);
  auto* network_row = new QHBoxLayout();
  network_row->setSpacing(10);
  wifi_status_label_ = new QToolButton(network_group);
  cellular_status_label_ = new QToolButton(network_group);
  wifi_status_label_->setIcon(QIcon(QStringLiteral(":/icons/tabler/wifi.svg")));
  cellular_status_label_->setIcon(
      QIcon(QStringLiteral(":/icons/tabler/antenna-bars-5.svg")));
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

  auto* status_group = new QFrame(this);
  status_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* status_layout = new QVBoxLayout(status_group);
  status_layout->setContentsMargins(16, 14, 16, 16);
  status_layout->setSpacing(10);
  auto* status_header = new QHBoxLayout();
  status_header->setSpacing(8);
  auto* status_title = new QLabel(tr("运行状态"), status_group);
  status_title->setStyleSheet(QStringLiteral(
                                  "QLabel { color:#18212f; font-size:%1px; font-weight:700; background:transparent; border:none; }")
                                  .arg(UiStyle::FontBasePx()));
  auto* refresh_status_btn = new QPushButton(tr("刷新状态"), status_group);
  auto* clear_btn = new QPushButton(tr("清空日志"), status_group);
  status_header->addWidget(status_title);
  status_header->addStretch();
  status_header->addWidget(refresh_status_btn);
  status_header->addWidget(clear_btn);
  status_layout->addLayout(status_header);
  status_summary_label_ = new QLabel(tr("暂无状态"), status_group);
  status_summary_label_->setWordWrap(true);
  status_summary_label_->setMinimumHeight(48);
  status_summary_label_->setStyleSheet(QStringLiteral(
                                           "QLabel { color:#536277; background:#f8fbff; border:1px solid #dce6f5; "
                                           "border-radius:12px; padding:10px 12px; font-size:%1px; }")
                                           .arg(UiStyle::FontSmallPx()));
  status_layout->addWidget(status_summary_label_);
  log_edit_ = new QPlainTextEdit(status_group);
  log_edit_->setReadOnly(true);
  log_edit_->setPlaceholderText(tr("暂无命令记录。"));
  log_edit_->setMaximumHeight(58);
  status_layout->addWidget(log_edit_);
  root->addWidget(status_group);

  connect(refresh_status_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendStatusRequest);
  connect(clear_btn, &QPushButton::clicked, this, &CommandCenterWidget::ClearLog);

  auto* diagnostic_group = new QFrame(this);
  diagnostic_group->setStyleSheet(UiStyle::CardStyleSheet());
  auto* diagnostic_layout = new QVBoxLayout(diagnostic_group);
  diagnostic_layout->setContentsMargins(16, 14, 16, 16);
  diagnostic_layout->setSpacing(10);
  AddCardTitle(diagnostic_layout, tr("系统诊断"), diagnostic_group);
  diagnostic_widget_ = new DiagnosticDockWidget(diagnostic_group);
  diagnostic_layout->addWidget(diagnostic_widget_);
  root->addWidget(diagnostic_group, 2);

  SUBSCRIBE(MSG_ID_COMMAND_RESPONSE, [this](const std::string& json) {
    QMetaObject::invokeMethod(this, [this, json]() { AppendResponse(json); }, Qt::QueuedConnection);
  });
  SUBSCRIBE(MSG_ID_COMMAND_STATUS, [this](const std::string& json) {
    QMetaObject::invokeMethod(this, [this, json]() { UpdateStatus(json); }, Qt::QueuedConnection);
  });
}

void CommandCenterWidget::SetDiagnosticSnapshot(const basic::DiagnosticSnapshot& snapshot) {
  if (diagnostic_widget_) {
    diagnostic_widget_->SetSnapshot(snapshot);
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
            .arg(wifi_connected ? QStringLiteral("#176b3a")
                                : QStringLiteral("#657386"),
                 wifi_connected ? QStringLiteral("#eef9f2")
                                : QStringLiteral("#f6f8fb"),
                 wifi_connected ? QStringLiteral("#b9e2c8")
                                : QStringLiteral("#dce4ef"))
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
            .arg(cellular_connected ? QStringLiteral("#174ea6")
                                    : QStringLiteral("#657386"),
                 cellular_connected ? QStringLiteral("#eef5ff")
                                    : QStringLiteral("#f6f8fb"),
                 cellular_connected ? QStringLiteral("#bcd3fb")
                                    : QStringLiteral("#dce4ef"))
            .arg(UiStyle::FontSmallPx()));
  }
}

QString CommandCenterWidget::MakeRequestJson(const QString& command, const QString& target,
                                             const QString& paramsJson) const {
  QJsonParseError err;
  QJsonDocument params_doc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
  QJsonObject params;
  if (err.error == QJsonParseError::NoError && params_doc.isObject()) {
    params = params_doc.object();
  }

  QJsonObject root;
  root["request_id"] = QString("qt-%1-%2")
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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
  log_edit_->appendPlainText(QString("[%1] %2\n%3\n").arg(ts, prefix, text));
}

void CommandCenterWidget::AppendResponse(const std::string& json) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &err);
  if (err.error == QJsonParseError::NoError && doc.isObject()) {
    const QJsonObject obj = doc.object();
    const bool success = obj.value("success").toBool(false);
    QString text = QString("%1\n命令: %2  目标: %3")
                       .arg(obj.value("message").toString())
                       .arg(obj.value("command").toString())
                       .arg(obj.value("target").toString());
    if (obj.contains("details")) {
      text += "\n";
      text += QString::fromUtf8(QJsonDocument(obj.value("details").toObject()).toJson(QJsonDocument::Compact));
    }
    AppendLog(success ? tr("成功") : tr("失败"), text);
    SendStatusRequest();
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

  const QString mode = obj.value("mode").toString(tr("未知"));
  QString load_text = tr("-");
  const QJsonArray load = obj.value("loadavg").toArray();
  if (load.size() >= 3) {
    load_text = QStringLiteral("%1/%2/%3")
                    .arg(load.at(0).toDouble(), 0, 'f', 1)
                    .arg(load.at(1).toDouble(), 0, 'f', 1)
                    .arg(load.at(2).toDouble(), 0, 'f', 1);
  }

  const QString summary =
      tr("模式 %1 · 负载 %2 · 摄像头 %3 · 核心节点 %4/%5")
          .arg(mode, load_text, camera_running ? tr("在线") : tr("离线"))
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
  SetStatusSummary(summary, detail);
}

void CommandCenterWidget::SetCameraStateText(const QString& text) {
  if (camera_state_label_) {
    camera_state_label_->setText(text);
  }
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

void CommandCenterWidget::StartCamera() {
  SetCameraStateText(tr("正在启动..."));
  PublishJson(MakeRequestJson("camera_start", "camera"));
}

void CommandCenterWidget::StopCamera() {
  SetCameraStateText(tr("正在停止..."));
  PublishJson(MakeRequestJson("camera_stop", "camera"));
}

void CommandCenterWidget::ClearLog() {
  if (log_edit_) {
    log_edit_->clear();
  }
}
