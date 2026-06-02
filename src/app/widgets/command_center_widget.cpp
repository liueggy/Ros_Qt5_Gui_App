#include "widgets/command_center_widget.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaObject>
#include <QStringList>
#include <QVariant>

#include "core/framework/framework.h"
#include "msg/msg_info.h"

namespace {
QJsonObject MakeDynItem(const QString &ns, const QString &name, double value) {
  QJsonObject item;
  item["namespace"] = ns;
  item["name"] = name;
  item["value"] = value;
  return item;
}

QJsonArray BuildNavigationProfileItems(const QString &profile) {
  double maxVelX = 0.45;
  double maxVelY = 0.30;
  double maxVelTheta = 1.0;
  double accX = 1.2;
  double accY = 0.5;
  double accTheta = 1.8;
  double minObstacle = 0.12;
  double obstacleWeight = 45.0;

  if (profile == "balanced") {
    maxVelX = 0.65; maxVelY = 0.40; maxVelTheta = 1.15;
    accX = 1.4; accY = 0.7; accTheta = 2.0;
    minObstacle = 0.14; obstacleWeight = 50.0;
  } else if (profile == "fast") {
    maxVelX = 0.90; maxVelY = 0.55; maxVelTheta = 1.25;
    accX = 1.8; accY = 0.9; accTheta = 2.3;
    minObstacle = 0.18; obstacleWeight = 60.0;
  } else if (profile == "very_fast") {
    maxVelX = 1.20; maxVelY = 0.65; maxVelTheta = 1.35;
    accX = 2.2; accY = 1.1; accTheta = 2.6;
    minObstacle = 0.22; obstacleWeight = 70.0;
  }

  QJsonArray items;
  const QString teb = "/move_base/TebLocalPlannerROS";
  items.append(MakeDynItem(teb, "max_vel_x", maxVelX));
  items.append(MakeDynItem(teb, "max_vel_y", maxVelY));
  items.append(MakeDynItem(teb, "max_vel_theta", maxVelTheta));
  items.append(MakeDynItem(teb, "acc_lim_x", accX));
  items.append(MakeDynItem(teb, "acc_lim_y", accY));
  items.append(MakeDynItem(teb, "acc_lim_theta", accTheta));
  items.append(MakeDynItem(teb, "min_obstacle_dist", minObstacle));
  items.append(MakeDynItem(teb, "weight_obstacle", obstacleWeight));
  return items;
}
}

CommandCenterWidget::CommandCenterWidget(QWidget *parent) : QWidget(parent) {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  auto *body = new QWidget(scroll);
  auto *root = new QVBoxLayout(body);
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(8);
  scroll->setWidget(body);
  outer->addWidget(scroll);

  auto *title = new QLabel(tr("Eggy 命令中心 / 模块调试"), this);
  title->setStyleSheet("font-size:15px;font-weight:600;color:#202124;");
  setStyleSheet(QStringLiteral(
      "QGroupBox { border:1px solid rgba(0,0,0,0.10); border-radius:10px; margin-top:10px; padding:10px; font-weight:600; color:#202124; }"
      "QGroupBox::title { subcontrol-origin: margin; left:12px; padding:0 6px; background:white; }"
      "QPushButton { border:1px solid #dadce0; border-radius:8px; padding:7px 12px; background:#ffffff; color:#202124; font-weight:500; }"
      "QPushButton:hover { background:#e8f0fe; border-color:#8ab4f8; color:#174ea6; }"
      "QComboBox, QLineEdit { border:1px solid #dadce0; border-radius:8px; padding:6px 8px; background:#ffffff; }"
      "QPlainTextEdit { border:1px solid #dadce0; border-radius:8px; background:#fafafa; padding:8px; font-family:Consolas, Menlo, monospace; }"));
  root->addWidget(title);

  auto *preset_row = new QHBoxLayout;
  preset_combo_ = new QComboBox(this);
  preset_combo_->addItem(tr("获取系统状态"), "status|system");
  preset_combo_->addItem(tr("启动摄像头"), "camera_start|camera");
  preset_combo_->addItem(tr("停止摄像头"), "camera_stop|camera");
  preset_combo_->addItem(tr("清理 move_base costmaps"), "clear_costmaps|move_base");
  preset_combo_->addItem(tr("启动 gmapping"), "mapping_start|mapping");
  preset_combo_->addItem(tr("停止 gmapping"), "mapping_stop|mapping");
  preset_combo_->addItem(tr("读取 move_base 控制频率"), "get_param|system|{\"name\":\"/move_base/controller_frequency\"}");
  target_edit_ = new QLineEdit(this);
  target_edit_->setPlaceholderText(tr("target，可留空使用预设"));
  auto *send_preset_btn = new QPushButton(tr("执行预设"), this);
  auto *status_btn = new QPushButton(tr("刷新状态"), this);
  preset_row->addWidget(preset_combo_, 2);
  preset_row->addWidget(target_edit_, 1);
  preset_row->addWidget(send_preset_btn);
  preset_row->addWidget(status_btn);
  root->addLayout(preset_row);

  auto *module_group = new QGroupBox(tr("常用模块控制"), this);
  auto *module_layout = new QHBoxLayout(module_group);
  auto *camera_start_btn = new QPushButton(tr("启动摄像头"), module_group);
  auto *camera_stop_btn = new QPushButton(tr("停止摄像头"), module_group);
  auto *clear_costmap_btn = new QPushButton(tr("清理代价图"), module_group);
  auto *mapping_start_btn = new QPushButton(tr("启动建图"), module_group);
  auto *mapping_stop_btn = new QPushButton(tr("停止建图"), module_group);
  module_layout->addWidget(camera_start_btn);
  module_layout->addWidget(camera_stop_btn);
  module_layout->addWidget(clear_costmap_btn);
  module_layout->addWidget(mapping_start_btn);
  module_layout->addWidget(mapping_stop_btn);
  root->addWidget(module_group);

  connect(camera_start_btn, &QPushButton::clicked, [this]() { PublishJson(MakeRequestJson("camera_start", "camera")); });
  connect(camera_stop_btn, &QPushButton::clicked, [this]() { PublishJson(MakeRequestJson("camera_stop", "camera")); });
  connect(clear_costmap_btn, &QPushButton::clicked, [this]() { PublishJson(MakeRequestJson("clear_costmaps", "move_base")); });
  connect(mapping_start_btn, &QPushButton::clicked, [this]() { PublishJson(MakeRequestJson("mapping_start", "mapping")); });
  connect(mapping_stop_btn, &QPushButton::clicked, [this]() { PublishJson(MakeRequestJson("mapping_stop", "mapping")); });

  auto *profile_group = new QGroupBox(tr("导航速度档位（一键应用组合参数，物理上限 1.5m/s，建议逐级实测）"), this);
  auto *profile_layout = new QHBoxLayout(profile_group);
  profile_combo_ = new QComboBox(profile_group);
  profile_combo_->addItem(tr("低速稳定 0.45m/s（当前稳妥基线）"), "stable");
  profile_combo_->addItem(tr("均衡调试 0.65m/s"), "balanced");
  profile_combo_->addItem(tr("快速测试 0.90m/s"), "fast");
  profile_combo_->addItem(tr("高速验证 1.20m/s（开阔场地）"), "very_fast");
  auto *apply_profile_btn = new QPushButton(tr("应用档位"), profile_group);
  auto *save_profile_btn = new QPushButton(tr("保存当前组合"), profile_group);
  auto *load_profile_btn = new QPushButton(tr("加载组合"), profile_group);
  auto *restore_default_btn = new QPushButton(tr("恢复默认"), profile_group);
  auto *use_local_map_btn = new QPushButton(tr("使用本地地图导航"), profile_group);
  auto *exit_nav_btn = new QPushButton(tr("退出导航模式"), profile_group);
  profile_layout->addWidget(profile_combo_, 1);
  profile_layout->addWidget(apply_profile_btn);
  profile_layout->addWidget(save_profile_btn);
  profile_layout->addWidget(load_profile_btn);
  profile_layout->addWidget(restore_default_btn);
  profile_layout->addWidget(use_local_map_btn);
  profile_layout->addWidget(exit_nav_btn);
  root->addWidget(profile_group);
  connect(apply_profile_btn, &QPushButton::clicked, this, &CommandCenterWidget::ApplySpeedProfile);
  connect(save_profile_btn, &QPushButton::clicked, this, &CommandCenterWidget::SaveCurrentProfile);
  connect(load_profile_btn, &QPushButton::clicked, this, &CommandCenterWidget::LoadSavedProfile);
  connect(restore_default_btn, &QPushButton::clicked, this, &CommandCenterWidget::RestoreDefaultProfile);
  connect(use_local_map_btn, &QPushButton::clicked, this, &CommandCenterWidget::UseLocalMapForNav);
  connect(exit_nav_btn, &QPushButton::clicked, this, &CommandCenterWidget::ExitNavMode);

  auto *param_group = new QGroupBox(tr("实车调参（优先 dynamic_reconfigure，适合边跑边调）"), this);
  auto *param_layout = new QFormLayout(param_group);
  param_combo_ = new QComboBox(param_group);
  param_combo_->addItem(tr("move_base 控制频率 controller_frequency"), "dyn|/move_base|controller_frequency|12.0");
  param_combo_->addItem(tr("move_base 全局规划频率 planner_frequency"), "dyn|/move_base|planner_frequency|1.0");
  param_combo_->addItem(tr("TEB 最大前进速度 max_vel_x"), "dyn|/move_base/TebLocalPlannerROS|max_vel_x|0.45");
  param_combo_->addItem(tr("TEB 最大横移速度 max_vel_y"), "dyn|/move_base/TebLocalPlannerROS|max_vel_y|0.30");
  param_combo_->addItem(tr("TEB 最大角速度 max_vel_theta"), "dyn|/move_base/TebLocalPlannerROS|max_vel_theta|1.0");
  param_combo_->addItem(tr("TEB 前进加速度 acc_lim_x"), "dyn|/move_base/TebLocalPlannerROS|acc_lim_x|1.2");
  param_combo_->addItem(tr("TEB 横移加速度 acc_lim_y"), "dyn|/move_base/TebLocalPlannerROS|acc_lim_y|0.5");
  param_combo_->addItem(tr("TEB 角加速度 acc_lim_theta"), "dyn|/move_base/TebLocalPlannerROS|acc_lim_theta|1.8");
  param_combo_->addItem(tr("TEB 目标距离容差 xy_goal_tolerance"), "dyn|/move_base/TebLocalPlannerROS|xy_goal_tolerance|0.22");
  param_combo_->addItem(tr("TEB 目标角度容差 yaw_goal_tolerance"), "dyn|/move_base/TebLocalPlannerROS|yaw_goal_tolerance|0.5");
  param_combo_->addItem(tr("TEB 最小障碍距离 min_obstacle_dist"), "dyn|/move_base/TebLocalPlannerROS|min_obstacle_dist|0.12");
  param_combo_->addItem(tr("TEB 障碍权重 weight_obstacle"), "dyn|/move_base/TebLocalPlannerROS|weight_obstacle|45.0");
  param_combo_->addItem(tr("局部 costmap 更新频率 update_frequency"), "dyn|/move_base/local_costmap|update_frequency|8.0");
  param_combo_->addItem(tr("局部 costmap 发布频率 publish_frequency"), "dyn|/move_base/local_costmap|publish_frequency|4.0");
  param_combo_->addItem(tr("全局 costmap 更新频率 update_frequency"), "dyn|/move_base/global_costmap|update_frequency|2.0");
  param_combo_->addItem(tr("全局 costmap 发布频率 publish_frequency"), "dyn|/move_base/global_costmap|publish_frequency|1.0");
  param_combo_->addItem(tr("局部膨胀半径 inflation_radius"), "dyn|/move_base/local_costmap/inflation_layer|inflation_radius|0.30");
  param_combo_->addItem(tr("局部膨胀衰减 cost_scaling_factor"), "dyn|/move_base/local_costmap/inflation_layer|cost_scaling_factor|6.0");
  param_combo_->addItem(tr("全局膨胀半径 inflation_radius"), "dyn|/move_base/global_costmap/inflation_layer|inflation_radius|0.55");
  param_combo_->addItem(tr("全局膨胀衰减 cost_scaling_factor"), "dyn|/move_base/global_costmap/inflation_layer|cost_scaling_factor|4.0");
  param_combo_->addItem(tr("普通参数：move_base/controller_frequency"), "param|system|/move_base/controller_frequency|12.0");
  param_value_edit_ = new QLineEdit(param_group);
  param_value_edit_->setPlaceholderText(tr("参数值，例如 10.0 / true / 0.25"));
  auto *param_btn_row = new QWidget(param_group);
  auto *param_btn_layout = new QHBoxLayout(param_btn_row);
  param_btn_layout->setContentsMargins(0, 0, 0, 0);
  auto *read_param_btn = new QPushButton(tr("读取"), param_btn_row);
  auto *write_param_btn = new QPushButton(tr("写入/应用"), param_btn_row);
  param_btn_layout->addWidget(read_param_btn);
  param_btn_layout->addWidget(write_param_btn);
  param_btn_layout->addStretch();
  param_layout->addRow(tr("参数"), param_combo_);
  param_layout->addRow(tr("值"), param_value_edit_);
  param_layout->addRow(QString(), param_btn_row);
  root->addWidget(param_group);
  OnParamPresetChanged(0);
  connect(param_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CommandCenterWidget::OnParamPresetChanged);
  connect(read_param_btn, &QPushButton::clicked, this, &CommandCenterWidget::ReadSelectedParam);
  connect(write_param_btn, &QPushButton::clicked, this, &CommandCenterWidget::WriteSelectedParam);

  root->addWidget(new QLabel(tr("自定义 JSON 请求（高级调试备用，发布到 /eggy/command/request）："), this));
  custom_json_edit_ = new QPlainTextEdit(this);
  custom_json_edit_->setMaximumHeight(95);
  custom_json_edit_->setPlainText("{\n  \"command\": \"status\",\n  \"target\": \"system\",\n  \"params\": {}\n}");
  root->addWidget(custom_json_edit_);

  auto *custom_row = new QHBoxLayout;
  auto *send_custom_btn = new QPushButton(tr("发送自定义 JSON"), this);
  auto *clear_btn = new QPushButton(tr("清空日志"), this);
  custom_row->addWidget(send_custom_btn);
  custom_row->addWidget(clear_btn);
  custom_row->addStretch();
  root->addLayout(custom_row);

  root->addWidget(new QLabel(tr("实时状态 /eggy/command/status："), this));
  status_edit_ = new QPlainTextEdit(this);
  status_edit_->setReadOnly(true);
  status_edit_->setMaximumHeight(130);
  root->addWidget(status_edit_);

  root->addWidget(new QLabel(tr("命令反馈日志 /eggy/command/response："), this));
  log_edit_ = new QPlainTextEdit(this);
  log_edit_->setReadOnly(true);
  root->addWidget(log_edit_, 1);

  connect(send_preset_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendPresetCommand);
  connect(status_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendStatusRequest);
  connect(send_custom_btn, &QPushButton::clicked, this, &CommandCenterWidget::SendCustomJson);
  connect(clear_btn, &QPushButton::clicked, this, &CommandCenterWidget::ClearLog);

  SUBSCRIBE(MSG_ID_COMMAND_RESPONSE, [this](const std::string &json) {
    QMetaObject::invokeMethod(this, [this, json]() { AppendResponse(json); }, Qt::QueuedConnection);
  });
  SUBSCRIBE(MSG_ID_COMMAND_STATUS, [this](const std::string &json) {
    QMetaObject::invokeMethod(this, [this, json]() { UpdateStatus(json); }, Qt::QueuedConnection);
  });
}

QString CommandCenterWidget::MakeRequestJson(const QString &command, const QString &target, const QString &paramsJson) const {
  QJsonParseError err;
  QJsonDocument paramsDoc = QJsonDocument::fromJson(paramsJson.toUtf8(), &err);
  QJsonObject params;
  if (err.error == QJsonParseError::NoError && paramsDoc.isObject()) {
    params = paramsDoc.object();
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

QString CommandCenterWidget::ParamPayloadToJson(const QString &command) const {
  const QString payload = param_combo_->currentData().toString();
  const QStringList parts = payload.split('|');
  const QString kind = parts.value(0);
  const QString nsOrTarget = parts.value(1);
  const QString name = parts.value(2);
  QString valueText = param_value_edit_->text().trimmed();
  if (valueText.isEmpty()) {
    valueText = parts.value(3);
  }

  QJsonObject params;
  if (kind == "dyn") {
    params["namespace"] = nsOrTarget;
    params["name"] = name;
  } else {
    params["name"] = name;
  }

  if (command == "dyn_set" || command == "set_param") {
    QJsonParseError err;
    QJsonDocument valueDoc = QJsonDocument::fromJson(valueText.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && !valueDoc.isNull()) {
      if (valueDoc.isObject()) {
        params["value"] = valueDoc.object();
      } else if (valueDoc.isArray()) {
        params["value"] = valueDoc.array();
      } else {
        params["value"] = QJsonValue::fromVariant(valueDoc.toVariant());
      }
    } else {
      bool okInt = false;
      int intVal = valueText.toInt(&okInt);
      bool okDouble = false;
      double doubleVal = valueText.toDouble(&okDouble);
      if (valueText == "true" || valueText == "false") {
        params["value"] = (valueText == "true");
      } else if (okInt && !valueText.contains('.')) {
        params["value"] = intVal;
      } else if (okDouble) {
        params["value"] = doubleVal;
      } else {
        params["value"] = valueText;
      }
    }
  }

  QString target = (kind == "dyn") ? nsOrTarget : "system";
  return MakeRequestJson(command, target, QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)));
}

void CommandCenterWidget::PublishJson(const QString &json) {
  PUBLISH(MSG_ID_COMMAND_REQUEST, json.toStdString());
  AppendLog(tr("SEND"), json);
}

void CommandCenterWidget::AppendLog(const QString &prefix, const QString &text) {
  if (!log_edit_) return;
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
  log_edit_->appendPlainText(QString("[%1] %2\n%3\n").arg(ts, prefix, text));
}

void CommandCenterWidget::AppendResponse(const std::string &json) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &err);
  if (err.error == QJsonParseError::NoError && doc.isObject()) {
    const QJsonObject obj = doc.object();
    const bool success = obj.value("success").toBool(false);
    QString text = QString("%1 %2\nrequest_id: %3\ncommand: %4  target: %5")
                       .arg(success ? "✅" : "❌")
                       .arg(obj.value("message").toString())
                       .arg(obj.value("request_id").toString())
                       .arg(obj.value("command").toString())
                       .arg(obj.value("target").toString());
    if (obj.contains("details")) {
      text += "\ndetails: ";
      text += QString::fromUtf8(QJsonDocument(obj.value("details").toObject()).toJson(QJsonDocument::Compact));
    }
    if (obj.value("command").toString() == "dyn_get_many" &&
        obj.value("target").toString() == "save_profile" &&
        success && !pending_save_profile_path_.isEmpty()) {
      QJsonObject profileDoc;
      profileDoc["saved_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
      profileDoc["source"] = "Eggy Qt CommandCenter";
      profileDoc["note"] = pending_save_profile_note_;
      profileDoc["details"] = obj.value("details").toObject();
      QFile file(pending_save_profile_path_);
      if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(profileDoc).toJson(QJsonDocument::Indented));
        file.close();
        text += tr("\n已保存到: %1").arg(pending_save_profile_path_);
      } else {
        text += tr("\n保存失败: %1").arg(pending_save_profile_path_);
      }
      pending_save_profile_path_.clear();
      pending_save_profile_note_.clear();
    }
    AppendLog(tr("RESPONSE"), text);
    return;
  }
  AppendLog(tr("RESPONSE"), QString::fromStdString(json));
}

void CommandCenterWidget::UpdateStatus(const std::string &json) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8(), &err);
  if (err.error == QJsonParseError::NoError && doc.isObject()) {
    const QJsonObject obj = doc.object();
    const QJsonObject nodes = obj.value("nodes").toObject();
    auto ok = [&nodes](const QString &name) { return nodes.value(name).toBool(false) ? "✅" : "❌"; };
    QString text;
    text += tr("模式: %1\n").arg(obj.value("mode").toString());
    const QJsonArray load = obj.value("loadavg").toArray();
    if (load.size() >= 3) {
      text += tr("负载: %1 / %2 / %3\n")
                  .arg(load.at(0).toDouble()).arg(load.at(1).toDouble()).arg(load.at(2).toDouble());
    }
    text += tr("%1 move_base    %2 gmapping    %3 AMCL\n")
                .arg(ok("/move_base"), ok("/slam_gmapping"), ok("/amcl"));
    text += tr("%1 rosbridge    %2 Qt适配器    %3 摄像头\n")
                .arg(ok("/rosbridge_websocket"), ok("/ros_qt5_gui_adapter"), ok("/eggy_camera"));
    text += tr("%1 雷达         %2 odom融合    %3 底盘驱动\n")
                .arg(ok("/rplidarNode"), ok("/eggy_external_imu_odom_fuser"), ok("/stm32_base_driver"));
    status_edit_->setPlainText(text);
    return;
  }
  status_edit_->setPlainText(QString::fromStdString(json));
}

void CommandCenterWidget::SendStatusRequest() {
  PublishJson(MakeRequestJson("status", "system"));
}

void CommandCenterWidget::SendPresetCommand() {
  const QString payload = preset_combo_->currentData().toString();
  const QStringList parts = payload.split('|');
  QString command = parts.value(0);
  QString target = target_edit_->text().trimmed();
  if (target.isEmpty()) {
    target = parts.value(1);
  }
  QString params = parts.value(2, "{}");
  PublishJson(MakeRequestJson(command, target, params));
}

void CommandCenterWidget::SendCustomJson() {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(custom_json_edit_->toPlainText().toUtf8(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    AppendLog(tr("ERROR"), tr("自定义 JSON 解析失败：%1").arg(err.errorString()));
    return;
  }
  QJsonObject obj = doc.object();
  if (!obj.contains("request_id")) {
    obj["request_id"] = QString("qt-%1-%2")
                            .arg(QDateTime::currentMSecsSinceEpoch())
                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  }
  PublishJson(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void CommandCenterWidget::ReadSelectedParam() {
  const QString kind = param_combo_->currentData().toString().split('|').value(0);
  PublishJson(ParamPayloadToJson(kind == "dyn" ? "dyn_get" : "get_param"));
}

void CommandCenterWidget::WriteSelectedParam() {
  const QString kind = param_combo_->currentData().toString().split('|').value(0);
  PublishJson(ParamPayloadToJson(kind == "dyn" ? "dyn_set" : "set_param"));
}

void CommandCenterWidget::OnParamPresetChanged(int index) {
  const QString payload = param_combo_->itemData(index).toString();
  const QStringList parts = payload.split('|');
  param_value_edit_->setText(parts.value(3));
  const QString name = parts.value(2);
  QString hint;
  if (name == "max_vel_x") hint = "建议 0.3~1.2，物理上限约 1.5m/s";
  else if (name == "max_vel_y") hint = "建议 0.2~0.8，横移高速更要注意打滑";
  else if (name == "max_vel_theta") hint = "建议 0.6~1.5，过高会导致转向抖动";
  else if (name.startsWith("acc_lim")) hint = "加速度越大越灵敏，但更容易打滑/急停";
  else if (name == "min_obstacle_dist") hint = "建议 0.12~0.30，越大越保守";
  else if (name == "weight_obstacle") hint = "建议 40~90，越大越避障保守";
  else if (name == "inflation_radius") hint = "建议 0.25~0.70，越大越远离障碍";
  else if (name == "controller_frequency") hint = "建议 8~20Hz，过高会增加 CPU 压力";
  if (!hint.isEmpty()) {
    param_value_edit_->setToolTip(hint);
    AppendLog(tr("HINT"), QString("%1: %2").arg(name, hint));
  }
}

void CommandCenterWidget::ApplySpeedProfile() {
  const QString profile = profile_combo_->currentData().toString();
  if (profile == "fast" || profile == "very_fast") {
    AppendLog(tr("WARN"), tr("当前选择高速档位，请确认场地开阔、定位稳定，并有人在旁看护。"));
  }
  QJsonObject params;
  params["profile"] = profile;
  params["items"] = BuildNavigationProfileItems(profile);
  PublishJson(MakeRequestJson("dyn_set_many", "navigation_profile", QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
}

void CommandCenterWidget::SaveCurrentProfile() {
  bool ok = false;
  pending_save_profile_note_ = QInputDialog::getText(this, tr("参数组合备注"),
                                                     tr("备注（例如：实验室开阔区域 0.9m/s 稳定）："),
                                                     QLineEdit::Normal, QString(), &ok);
  if (!ok) return;
  const QString fileName = QFileDialog::getSaveFileName(this, tr("保存当前参数组合"), QString(), tr("JSON 文件 (*.json)"));
  if (fileName.isEmpty()) return;
  pending_save_profile_path_ = fileName.endsWith(".json") ? fileName : fileName + ".json";

  QJsonObject params;
  params["profile"] = "current_runtime";
  params["items"] = BuildNavigationProfileItems("stable");  // 只使用 namespace/name，value 会由 ROS 端读取实际值
  PublishJson(MakeRequestJson("dyn_get_many", "save_profile", QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
}

void CommandCenterWidget::LoadSavedProfile() {
  const QString fileName = QFileDialog::getOpenFileName(this, tr("加载参数组合"), QString(), tr("JSON 文件 (*.json)"));
  if (fileName.isEmpty()) return;
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    AppendLog(tr("ERROR"), tr("无法打开参数组合文件：%1").arg(fileName));
    return;
  }
  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(data, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    AppendLog(tr("ERROR"), tr("参数组合 JSON 解析失败：%1").arg(err.errorString()));
    return;
  }

  const QJsonObject root = doc.object();
  QJsonArray sourceItems;
  if (root.contains("details")) {
    sourceItems = root.value("details").toObject().value("results").toArray();
  } else if (root.contains("items")) {
    sourceItems = root.value("items").toArray();
  }
  if (sourceItems.isEmpty()) {
    AppendLog(tr("ERROR"), tr("参数组合文件中没有 details.results 或 items。"));
    return;
  }

  QJsonArray items;
  for (const auto &v : sourceItems) {
    const QJsonObject src = v.toObject();
    if (!src.value("success").toBool(true)) continue;
    if (!src.contains("namespace") || !src.contains("name") || !src.contains("value")) continue;
    QJsonObject item;
    item["namespace"] = src.value("namespace").toString();
    item["name"] = src.value("name").toString();
    item["value"] = src.value("value");
    items.append(item);
  }
  if (items.isEmpty()) {
    AppendLog(tr("ERROR"), tr("参数组合文件没有可应用的参数项。"));
    return;
  }

  QJsonObject params;
  params["profile"] = root.value("note").toString("loaded_profile");
  params["items"] = items;
  AppendLog(tr("INFO"), tr("加载组合：%1\n备注：%2").arg(fileName, root.value("note").toString()));
  PublishJson(MakeRequestJson("dyn_set_many", "loaded_profile", QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
}

void CommandCenterWidget::RestoreDefaultProfile() {
  profile_combo_->setCurrentIndex(0);
  AppendLog(tr("INFO"), tr("恢复默认低速稳定档参数。"));
  ApplySpeedProfile();
}

void CommandCenterWidget::UseLocalMapForNav() {
  const QString yamlPath = QFileDialog::getOpenFileName(this, tr("选择本地地图 YAML"), QString(), tr("地图文件 (*.yaml)"));
  if (yamlPath.isEmpty()) return;

  QFileInfo yamlInfo(yamlPath);
  QFile yamlFile(yamlPath);
  if (!yamlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    AppendLog(tr("ERROR"), tr("无法打开 YAML 文件：%1").arg(yamlPath));
    return;
  }
  const QString yamlText = QString::fromUtf8(yamlFile.readAll());
  yamlFile.close();

  QString imageName;
  for (const QString &line : yamlText.split('\n')) {
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith("image:")) {
      imageName = trimmed.mid(6).trimmed();
      break;
    }
  }
  if (imageName.isEmpty()) {
    AppendLog(tr("ERROR"), tr("YAML 中未找到 image 字段。"));
    return;
  }

  const QString pgmPath = QDir(yamlInfo.absoluteFilePath()).absoluteFilePath(imageName);
  QFileInfo pgmInfo(pgmPath);
  if (!pgmInfo.exists()) {
    AppendLog(tr("ERROR"), tr("PGM 文件不存在：%1").arg(pgmPath));
    return;
  }

  QFile pgmFile(pgmPath);
  if (!pgmFile.open(QIODevice::ReadOnly)) {
    AppendLog(tr("ERROR"), tr("无法打开 PGM 文件：%1").arg(pgmPath));
    return;
  }
  const QByteArray pgmData = pgmFile.readAll();
  pgmFile.close();

  QJsonObject params;
  params["map_name"] = yamlInfo.completeBaseName();
  params["yaml_b64"] = QString::fromUtf8(yamlText.toUtf8().toBase64());
  params["pgm_b64"] = QString::fromUtf8(pgmData.toBase64());
  params["activate"] = true;
  PublishJson(MakeRequestJson("upload_map", "navigation", QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
  AppendLog(tr("INFO"), tr("已发送本地地图到小车并请求切到 AMCL 导航模式。\nYAML: %1\nPGM: %2").arg(yamlPath, pgmPath));
}

void CommandCenterWidget::ExitNavMode() {
  QJsonObject params;
  params["mode"] = "mapping";
  PublishJson(MakeRequestJson("switch_nav_mode", "mapping", QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact))));
  AppendLog(tr("INFO"), tr("已请求退出导航模式，切回 gmapping 建图模式。"));
}

void CommandCenterWidget::ClearLog() {
  log_edit_->clear();
}
