#include "widgets/command_center_widget.h"

#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>

#include "core/framework/framework.h"
#include "msg/msg_info.h"
#include "widgets/diagnostic_dock_widget.h"
#include "widgets/ui_style.h"

namespace {

QJsonObject MakeDynItem(const QString& ns, const QString& name, double value) {
  QJsonObject item;
  item["namespace"] = ns;
  item["name"] = name;
  item["value"] = value;
  return item;
}

QJsonArray BuildSpeedProfileItems(const QString& profile) {
  double max_vel_x = 0.45;
  double max_vel_y = 0.0;
  double max_vel_theta = 1.0;
  double acc_x = 1.2;
  double acc_y = 0.0;
  double acc_theta = 1.8;

  if (profile == "balanced") {
    max_vel_x = 0.60;
    max_vel_theta = 1.10;
    acc_x = 1.4;
    acc_theta = 2.0;
  } else if (profile == "fast") {
    max_vel_x = 0.80;
    max_vel_theta = 1.20;
    acc_x = 1.6;
    acc_theta = 2.2;
  }

  const QString teb = "/move_base/TebLocalPlannerROS";
  QJsonArray items;
  items.append(MakeDynItem(teb, "max_vel_x", max_vel_x));
  items.append(MakeDynItem(teb, "max_vel_y", max_vel_y));
  items.append(MakeDynItem(teb, "max_vel_theta", max_vel_theta));
  items.append(MakeDynItem(teb, "acc_lim_x", acc_x));
  items.append(MakeDynItem(teb, "acc_lim_y", acc_y));
  items.append(MakeDynItem(teb, "acc_lim_theta", acc_theta));
  return items;
}

QJsonValue ParseJsonValue(const QString& text) {
  QJsonParseError err;
  QJsonDocument value_doc = QJsonDocument::fromJson(text.toUtf8(), &err);
  if (err.error == QJsonParseError::NoError && !value_doc.isNull()) {
    return QJsonValue::fromVariant(value_doc.toVariant());
  }

  bool ok_int = false;
  const int int_value = text.toInt(&ok_int);
  bool ok_double = false;
  const double double_value = text.toDouble(&ok_double);
  if (text == "true" || text == "false") {
    return text == "true";
  }
  if (ok_int && !text.contains('.')) {
    return int_value;
  }
  if (ok_double) {
    return double_value;
  }
  return text;
}

}  // namespace

CommandCenterWidget::CommandCenterWidget(QWidget* parent) : QWidget(parent) {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  auto* body = new QWidget(scroll);
  auto* root = new QVBoxLayout(body);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(10);
  scroll->setWidget(body);
  outer->addWidget(scroll);

  setStyleSheet(UiStyle::PanelStyleSheet() + UiStyle::SecondaryButtonStyleSheet() + UiStyle::InputStyleSheet());

  auto* title = new QLabel(tr("运维面板"), this);
  title->setObjectName(QStringLiteral("pageTitle"));
  title->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  root->addWidget(title);

  auto* camera_group = new QGroupBox(tr("摄像头"), this);
  auto* camera_layout = new QHBoxLayout(camera_group);
  auto* camera_start_btn = new QPushButton(tr("启动摄像头"), camera_group);
  auto* camera_stop_btn = new QPushButton(tr("停止摄像头"), camera_group);
  camera_start_btn->setStyleSheet(UiStyle::MainButtonStyleSheet());
  camera_stop_btn->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  camera_state_label_ = new QLabel(tr("状态等待刷新"), camera_group);
  camera_state_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet() + QStringLiteral("font-weight:600;"));
  camera_layout->addWidget(camera_start_btn);
  camera_layout->addWidget(camera_stop_btn);
  camera_layout->addStretch();
  camera_layout->addWidget(camera_state_label_);
  root->addWidget(camera_group);

  connect(camera_start_btn, &QPushButton::clicked, this, &CommandCenterWidget::StartCamera);
  connect(camera_stop_btn, &QPushButton::clicked, this, &CommandCenterWidget::StopCamera);

  auto* speed_group = new QGroupBox(tr("速度参数"), this);
  auto* speed_layout = new QVBoxLayout(speed_group);

  auto* profile_row = new QHBoxLayout();
  profile_combo_ = new QComboBox(speed_group);
  profile_combo_->addItem(tr("低速稳定"), "stable");
  profile_combo_->addItem(tr("均衡巡航"), "balanced");
  profile_combo_->addItem(tr("快速测试"), "fast");
  auto* apply_profile_btn = new QPushButton(tr("应用档位"), speed_group);
  profile_row->addWidget(profile_combo_, 1);
  profile_row->addWidget(apply_profile_btn);
  speed_layout->addLayout(profile_row);
  connect(apply_profile_btn, &QPushButton::clicked, this, &CommandCenterWidget::ApplySpeedProfile);

  auto* param_layout = new QFormLayout();
  speed_param_combo_ = new QComboBox(speed_group);
  speed_param_combo_->addItem(tr("最大前进速度 max_vel_x"), "dyn|/move_base/TebLocalPlannerROS|max_vel_x|0.45");
  speed_param_combo_->addItem(tr("最大角速度 max_vel_theta"), "dyn|/move_base/TebLocalPlannerROS|max_vel_theta|1.0");
  speed_param_combo_->addItem(tr("前进加速度 acc_lim_x"), "dyn|/move_base/TebLocalPlannerROS|acc_lim_x|1.2");
  speed_param_combo_->addItem(tr("角加速度 acc_lim_theta"), "dyn|/move_base/TebLocalPlannerROS|acc_lim_theta|1.8");
  speed_value_edit_ = new QLineEdit(speed_group);
  speed_value_edit_->setPlaceholderText(tr("例如 0.45"));
  param_layout->addRow(tr("参数"), speed_param_combo_);
  param_layout->addRow(tr("值"), speed_value_edit_);
  speed_layout->addLayout(param_layout);

  auto* param_btn_row = new QHBoxLayout();
  auto* read_param_btn = new QPushButton(tr("读取"), speed_group);
  auto* write_param_btn = new QPushButton(tr("写入"), speed_group);
  param_btn_row->addWidget(read_param_btn);
  param_btn_row->addWidget(write_param_btn);
  param_btn_row->addStretch();
  speed_layout->addLayout(param_btn_row);
  root->addWidget(speed_group);

  connect(speed_param_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
    const QStringList parts = speed_param_combo_->itemData(index).toString().split('|');
    speed_value_edit_->setText(parts.value(3));
  });
  speed_param_combo_->setCurrentIndex(0);
  connect(read_param_btn, &QPushButton::clicked, this, &CommandCenterWidget::ReadSelectedSpeedParam);
  connect(write_param_btn, &QPushButton::clicked, this, &CommandCenterWidget::WriteSelectedSpeedParam);

  auto* status_group = new QGroupBox(tr("状态"), this);
  auto* status_layout = new QVBoxLayout(status_group);
  auto* status_btn_row = new QHBoxLayout();
  auto* refresh_status_btn = new QPushButton(tr("刷新状态"), status_group);
  auto* clear_btn = new QPushButton(tr("清空日志"), status_group);
  status_btn_row->addWidget(refresh_status_btn);
  status_btn_row->addWidget(clear_btn);
  status_btn_row->addStretch();
  status_layout->addLayout(status_btn_row);
  status_edit_ = new QPlainTextEdit(status_group);
  status_edit_->setReadOnly(true);
  status_edit_->setMaximumHeight(120);
  status_layout->addWidget(status_edit_);
  log_edit_ = new QPlainTextEdit(status_group);
  log_edit_->setReadOnly(true);
  log_edit_->setMaximumHeight(150);
  status_layout->addWidget(log_edit_);
  root->addWidget(status_group);

  connect(refresh_status_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendStatusRequest);
  connect(clear_btn, &QPushButton::clicked, this, &CommandCenterWidget::ClearLog);

  auto* diagnostic_group = new QGroupBox(tr("诊断"), this);
  auto* diagnostic_layout = new QVBoxLayout(diagnostic_group);
  diagnostic_widget_ = new DiagnosticDockWidget(diagnostic_group);
  diagnostic_layout->addWidget(diagnostic_widget_);
  root->addWidget(diagnostic_group, 1);

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

QString CommandCenterWidget::SpeedParamPayloadToJson(const QString& command) const {
  const QStringList parts = speed_param_combo_->currentData().toString().split('|');
  QJsonObject params;
  params["namespace"] = parts.value(1);
  params["name"] = parts.value(2);

  if (command == "dyn_set") {
    const QString value_text = speed_value_edit_->text().trimmed().isEmpty()
                                   ? parts.value(3)
                                   : speed_value_edit_->text().trimmed();
    params["value"] = ParseJsonValue(value_text);
  }

  return MakeRequestJson(command, parts.value(1),
                         QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)));
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
    status_edit_->setPlainText(QString::fromStdString(json));
    return;
  }

  const QJsonObject obj = doc.object();
  const QJsonObject nodes = obj.value("nodes").toObject();
  auto ok = [this, &nodes](const QString& name) { return nodes.value(name).toBool(false) ? tr("在线") : tr("离线"); };
  const QJsonObject camera = obj.value("camera").toObject();
  const bool camera_running = camera.value("running").toBool(false);
  SetCameraStateText(camera_running ? tr("摄像头在线") : tr("摄像头离线"));

  QString text;
  text += tr("模式: %1\n").arg(obj.value("mode").toString());
  const QJsonArray load = obj.value("loadavg").toArray();
  if (load.size() >= 3) {
    text += tr("负载: %1 / %2 / %3\n")
                .arg(load.at(0).toDouble())
                .arg(load.at(1).toDouble())
                .arg(load.at(2).toDouble());
  }
  text += tr("摄像头: %1  PID: %2\n")
              .arg(camera_running ? tr("在线") : tr("离线"))
              .arg(camera.value("pid").toString("-"));
  text += tr("move_base: %1  雷达: %2  底盘: %3\n")
              .arg(ok("/move_base"), ok("/rplidarNode"), ok("/stm32_base_driver"));
  text += tr("rosbridge: %1  Qt适配器: %2  里程计融合: %3")
              .arg(ok("/rosbridge_websocket"), ok("/ros_qt5_gui_adapter"), ok("/eggy_external_imu_odom_fuser"));
  status_edit_->setPlainText(text);
}

void CommandCenterWidget::SetCameraStateText(const QString& text) {
  if (camera_state_label_) {
    camera_state_label_->setText(text);
  }
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

void CommandCenterWidget::ReadSelectedSpeedParam() {
  PublishJson(SpeedParamPayloadToJson("dyn_get"));
}

void CommandCenterWidget::WriteSelectedSpeedParam() {
  PublishJson(SpeedParamPayloadToJson("dyn_set"));
}

void CommandCenterWidget::ApplySpeedProfile() {
  const QString profile = profile_combo_->currentData().toString();
  QJsonObject params;
  params["profile"] = profile;
  params["items"] = BuildSpeedProfileItems(profile);
  PublishJson(MakeRequestJson("dyn_set_many", "speed_profile",
                              QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
}

void CommandCenterWidget::ClearLog() {
  if (log_edit_) {
    log_edit_->clear();
  }
}
