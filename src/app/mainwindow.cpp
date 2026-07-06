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
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyle>
#include <QUuid>
#include <cmath>
#include <iostream>
#include <map>
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
#include <nlohmann/json.hpp>
#include "display/manager/view_manager.h"
#include "msg/diagnostic_snapshot.h"
#include "widgets/command_center_widget.h"
#include "widgets/display_config_widget.h"
#include "widgets/speed_ctrl.h"
#include "widgets/terminal_widget.h"
#include "widgets/ui_style.h"
using namespace ads;
namespace {

constexpr int kUiLayoutVersion = 10;

void ConfigureDockWidget(ads::CDockWidget* dock, const QSize& minimum_size,
                         const QSize& preferred_size = QSize()) {
  if (!dock) {
    return;
  }
  dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
  dock->setMinimumSize(minimum_size);
  dock->setProperty("preferredDockSize", preferred_size.isValid() ? preferred_size : minimum_size);
}

QFrame* CreateTopStatusPill(const QString& icon_path, QWidget* value_widget,
                            const QString& tooltip, int width, QWidget* parent) {
  auto* pill = new QFrame(parent);
  pill->setObjectName(QStringLiteral("topStatusPill"));
  pill->setFixedSize(width, 34);
  pill->setToolTip(tooltip);
  pill->setStyleSheet(QStringLiteral(
      "QFrame#topStatusPill { background:#f8fbff; border:1px solid #dce4ef; border-radius:11px; }"
      "QFrame#topStatusPill:hover { background:#ffffff; border-color:#bcd3fb; }"
      "QFrame#topStatusPill QLabel { background:transparent; border:none; }"));

  auto* layout = new QHBoxLayout(pill);
  layout->setContentsMargins(8, 0, 8, 0);
  layout->setSpacing(5);

  auto* icon = new QLabel(pill);
  icon->setPixmap(QIcon(icon_path).pixmap(18, 18));
  icon->setFixedSize(18, 18);
  icon->setAlignment(Qt::AlignCenter);
  icon->setToolTip(tooltip);
  layout->addWidget(icon);
  layout->addWidget(value_widget, 1);
  return pill;
}


QString JsonValueToText(const nlohmann::json& value) {
  if (value.is_string()) {
    return QString::fromStdString(value.get<std::string>());
  }
  if (value.is_number_float()) {
    return QString::number(value.get<double>(), 'f', 2);
  }
  if (value.is_number_integer()) {
    return QString::number(value.get<long long>());
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? QStringLiteral("是") : QStringLiteral("否");
  }
  return QString();
}

QString NormalizeInspectionStatus(const QString& raw) {
  const QString s = raw.trimmed().toLower();
  if (s == QStringLiteral("normal") || s == QStringLiteral("ok") ||
      s == QStringLiteral("正常")) {
    return QStringLiteral("正常");
  }
  if (s.isEmpty()) {
    return QStringLiteral("未识别");
  }
  return QStringLiteral("异常");
}

QString MeterReadingText(const nlohmann::json& meter) {
  if (!meter.is_object()) {
    return QStringLiteral("未识别");
  }
  QString value = meter.contains("reading") ? JsonValueToText(meter["reading"])
                : meter.contains("best_effort_reading") ? JsonValueToText(meter["best_effort_reading"])
                : meter.contains("value") ? JsonValueToText(meter["value"])
                : meter.contains("读数") ? JsonValueToText(meter["读数"])
                : QString();
  if (value.trimmed().isEmpty() || value.trimmed().toLower() == QStringLiteral("unknown")) {
    return QStringLiteral("未识别");
  }
  const QString unit = meter.contains("unit") ? JsonValueToText(meter["unit"])
                     : meter.contains("单位") ? JsonValueToText(meter["单位"])
                     : QString();
  return unit.isEmpty() ? value : value + unit;
}

struct AiInspectionDisplay {
  bool valid{false};
  QString waypoint;
  QString targetName{QStringLiteral("水表")};
  QString reading{QStringLiteral("未识别")};
  QString status{QStringLiteral("未识别")};
  QString conclusion;
};

nlohmann::json ExtractKimiApiObject(const nlohmann::json& kimi) {
  if (!kimi.is_object()) {
    return nlohmann::json::object();
  }
  if (kimi.contains("api") && kimi["api"].is_object()) {
    return kimi["api"];
  }
  if (kimi.contains("result") && kimi["result"].is_object()) {
    return kimi["result"];
  }
  return kimi;
}

nlohmann::json ExtractKimiResultObject(const nlohmann::json& api) {
  if (!api.is_object()) {
    return nlohmann::json::object();
  }
  if (api.contains("result") && api["result"].is_object()) {
    return api["result"];
  }
  if (api.contains("api") && api["api"].is_object()) {
    return ExtractKimiResultObject(api["api"]);
  }
  return api;
}

AiInspectionDisplay ExtractAiInspectionDisplay(const nlohmann::json& point) {
  AiInspectionDisplay display;
  if (!point.is_object() || !point.contains("kimi") || !point["kimi"].is_object()) {
    return display;
  }
  const auto wp = point.value("waypoint", nlohmann::json::object());
  display.waypoint = QString::fromStdString(wp.value("id", std::string()));
  const auto result = ExtractKimiResultObject(ExtractKimiApiObject(point["kimi"]));
  if (!result.is_object()) {
    return display;
  }
  if (result.contains("readings") && result["readings"].is_object()) {
    const auto& readings = result["readings"];
    if (readings.contains("water_meter") && readings["water_meter"].is_object()) {
      const auto& wm = readings["water_meter"];
      display.valid = true;
      display.targetName = QStringLiteral("水表");
      display.reading = MeterReadingText(wm);
      display.status = NormalizeInspectionStatus(
          wm.contains("status") ? JsonValueToText(wm["status"]) : QString());
      if (display.reading == QStringLiteral("未识别")) {
        display.status = QStringLiteral("异常");
      }
    } else if (readings.contains("pressure_gauge") && readings["pressure_gauge"].is_object()) {
      const auto& pg = readings["pressure_gauge"];
      display.valid = true;
      display.targetName = QStringLiteral("压力表");
      display.reading = MeterReadingText(pg);
      display.status = NormalizeInspectionStatus(
          pg.contains("status") ? JsonValueToText(pg["status"]) : QString());
      if (display.reading == QStringLiteral("未识别")) {
        display.status = QStringLiteral("异常");
      }
    }
  }
  if (result.contains("analysis") && result["analysis"].is_object() &&
      result["analysis"].contains("message")) {
    display.conclusion = JsonValueToText(result["analysis"]["message"]);
  }
  if (display.conclusion.isEmpty() && result.contains("summary")) {
    display.conclusion = JsonValueToText(result["summary"]);
  }
  if (!display.valid && !display.conclusion.isEmpty()) {
    display.valid = true;
    display.status = QStringLiteral("异常");
  }
  return display;
}

QString FormatAiInspectionBanner(const AiInspectionDisplay& display) {
  QStringList lines;
  lines << QStringLiteral("AI分析结果");
  if (!display.waypoint.isEmpty()) {
    lines << QStringLiteral("点位：%1").arg(display.waypoint);
  }
  lines << QStringLiteral("%1：%2").arg(display.targetName, display.reading);
  lines << QStringLiteral("状态：%1").arg(display.status);
  if (!display.conclusion.isEmpty()) {
    lines << QStringLiteral("结论：%1").arg(display.conclusion);
  }
  return lines.join(QStringLiteral("\n"));
}
QString SummarizeKimiObject(const nlohmann::json& api) {
  if (!api.is_object()) {
    return QString();
  }
  // api is the server response: {ok, task, result: {target, readings: {water_meter, pressure_gauge}, analysis, summary}}
  const auto result = ExtractKimiResultObject(api);

  // Prefer the summary string from the AI
  if (result.contains("summary") && result["summary"].is_string()) {
    const QString s = QString::fromStdString(result.value("summary", std::string()));
    if (!s.isEmpty()) {
      return s;
    }
  }

  QStringList summaries;
  auto append_meter = [&summaries](const QString& name, const nlohmann::json& meter) {
    if (!meter.is_object()) {
      return;
    }
    QStringList fields;
    const QString value = meter.contains("value") ? JsonValueToText(meter["value"])
                         : meter.contains("reading") ? JsonValueToText(meter["reading"])
                         : meter.contains("读数") ? JsonValueToText(meter["读数"])
                         : QString();
    const QString unit = meter.contains("unit") ? JsonValueToText(meter["unit"])
                       : meter.contains("单位") ? JsonValueToText(meter["单位"])
                       : QString();
    if (!value.isEmpty()) {
      fields << (unit.isEmpty() ? value : value + unit);
    }
    if (meter.contains("status")) {
      fields << JsonValueToText(meter["status"]);
    } else if (meter.contains("状态")) {
      fields << JsonValueToText(meter["状态"]);
    }
    if (meter.contains("confidence") && meter["confidence"].is_number()) {
      fields << QStringLiteral("置信度%1").arg(meter["confidence"].get<double>(), 0, 'f', 2);
    }
    if (!fields.isEmpty()) {
      summaries << QStringLiteral("%1: %2").arg(name, fields.join(QStringLiteral("/")));
    }
  };

  // readings is an object with water_meter / pressure_gauge keys
  if (result.contains("readings") && result["readings"].is_object()) {
    const auto& readings = result["readings"];
    if (readings.contains("water_meter")) {
      append_meter(QStringLiteral("水表"), readings["water_meter"]);
    }
    if (readings.contains("pressure_gauge")) {
      append_meter(QStringLiteral("压力表"), readings["pressure_gauge"]);
    }
    // handle any other keys in readings
    for (auto it = readings.begin(); it != readings.end(); ++it) {
      if (it.key() != "water_meter" && it.key() != "pressure_gauge" && it.value().is_object()) {
        append_meter(QString::fromStdString(it.key()), it.value());
      }
    }
  }

  // Fallback: readings as array (legacy format)
  if (result.contains("readings") && result["readings"].is_array()) {
    for (const auto& reading : result["readings"]) {
      if (!reading.is_object()) {
        continue;
      }
      const QString type = QString::fromStdString(
          reading.value("type", reading.value("class_name", std::string("读数"))));
      const QString value = reading.contains("value") ? JsonValueToText(reading["value"])
                          : reading.contains("reading") ? JsonValueToText(reading["reading"])
                          : QString();
      const QString unit = reading.contains("unit") ? JsonValueToText(reading["unit"]) : QString();
      if (!value.isEmpty()) {
        summaries << QStringLiteral("%1: %2%3").arg(type, value, unit);
      }
    }
  }

  // Add analysis message if present
  if (result.contains("analysis") && result["analysis"].is_object()) {
    const auto& analysis = result["analysis"];
    if (analysis.contains("message") && analysis["message"].is_string()) {
      const QString msg = QString::fromStdString(analysis.value("message", std::string()));
      if (!msg.isEmpty()) {
        summaries << msg;
      }
    }
  }

  if (!summaries.isEmpty()) {
    return summaries.join(QStringLiteral("；"));
  }
  if (api.contains("message")) {
    return JsonValueToText(api["message"]);
  }
  return QString();
}

QString InspectionStageText(const std::string& stage) {
  static const std::map<std::string, QString> kStageText = {
      {"ready", QStringLiteral("待命")},
      {"home_recorded", QStringLiteral("已记录起点")},
      {"waiting_move_base", QStringLiteral("等待导航")},
      {"navigating", QStringLiteral("前往目标点")},
      {"search_settling", QStringLiteral("到达后识别")},
      {"searching_target", QStringLiteral("正在搜索目标")},
      {"search_rotating", QStringLiteral("90°步进旋转寻找")},
      {"search_paused", QStringLiteral("暂停判定目标")},
      {"target_confirmed", QStringLiteral("目标已确认")},
      {"target_skipped", QStringLiteral("未找到目标，跳过本点")},
      {"kimi_running", QStringLiteral("AI视觉分析")},
      {"kimi_complete", QStringLiteral("AI分析完成")},
      {"returning_home", QStringLiteral("正在返航")},
      {"complete", QStringLiteral("巡检完成")},
      {"cancelled", QStringLiteral("已取消")},
      {"error", QStringLiteral("巡检异常")},
      {"busy", QStringLiteral("任务运行中")},
  };
  const auto it = kStageText.find(stage);
  if (it != kStageText.end()) {
    return it->second;
  }
  return QString::fromStdString(stage.empty() ? "未知状态" : stage);
}

QString FormatInspectionStatus(const std::string& json_text) {
  try {
    const auto data = nlohmann::json::parse(json_text);
    const QString stage = InspectionStageText(
        data.value("stage", data.value("state", std::string())));
    const QString message =
        QString::fromStdString(data.value("message", std::string()));
    QStringList parts;
    parts << stage;
    if (data.contains("waypoint_id")) {
      parts << QStringLiteral("点位: %1").arg(QString::fromStdString(data.value("waypoint_id", std::string())));
    }
    if (data.contains("extra") && data["extra"].is_object() &&
        data["extra"].contains("waypoint") && data["extra"]["waypoint"].is_object()) {
      parts << QStringLiteral("点位: %1").arg(QString::fromStdString(data["extra"]["waypoint"].value("id", std::string())));
    }
    if (data.value("intercepted", false)) {
      parts << QStringLiteral("已由视觉截获目标");
    }
    if (data.contains("extra") && data["extra"].is_object()) {
      const auto extra = data["extra"];
      const auto target = extra.contains("target") ? extra["target"] : nlohmann::json();
      if (target.is_object()) {
        const QString cls = QString::fromStdString(
            target.value("class_name", target.value("target_class", std::string())));
        const double score = target.value("score", -1.0);
        if (!cls.isEmpty()) {
          parts << (score >= 0.0
                        ? QStringLiteral("目标: %1/%2").arg(cls).arg(score, 0, 'f', 2)
                        : QStringLiteral("目标: %1").arg(cls));
        }
      }
    }
    if (!message.isEmpty()) {
      parts << message;
    }
    return parts.join(QStringLiteral(" · "));
  } catch (const std::exception&) {
    return QString::fromStdString(json_text);
  }
}

QString FormatInspectionResult(const std::string& json_text) {
  try {
    const auto data = nlohmann::json::parse(json_text);
    QStringList lines;
    const bool ok = data.value("ok", false);
    lines << (ok ? QStringLiteral("巡检完成") : QStringLiteral("巡检未完成"));
    if (data.contains("error")) {
      lines << QStringLiteral("错误: %1").arg(QString::fromStdString(data.value("error", std::string())));
    }
    const auto points = data.contains("points") ? data["points"] : data.contains("results") ? data["results"]
                                                                                            : nlohmann::json::array();
    if (points.is_array()) {
      int index = 1;
      for (const auto& point : points) {
        const auto wp = point.value("waypoint", nlohmann::json::object());
        const QString waypoint = QString::fromStdString(
            wp.value("id", std::string("P" + std::to_string(index))));
        QStringList point_parts;
        point_parts << QStringLiteral("%1. %2").arg(index).arg(waypoint);
        if (point.contains("navigation")) {
          const auto nav = point["navigation"];
          point_parts << QStringLiteral("导航:%1").arg(
              QString::fromStdString(nav.value("state_text", std::string("-"))));
          if (nav.value("intercepted", false)) {
            point_parts << QStringLiteral("视觉截获");
          }
        }
        if (point.contains("search") && point["search"].is_object()) {
          const auto search = point["search"];
          point_parts << QStringLiteral("搜索:%1").arg(
              search.value("ok", false) ? QStringLiteral("找到") : QStringLiteral("未找到"));
        }
        if (point.contains("target") && point["target"].is_object()) {
          const auto target = point["target"];
          const QString cls = QString::fromStdString(
              target.value("class_name", target.value("target_class", std::string())));
          const double score = target.value("score", -1.0);
          if (!cls.isEmpty()) {
            point_parts << (score >= 0.0
                                ? QStringLiteral("目标:%1/%2").arg(cls).arg(score, 0, 'f', 2)
                                : QStringLiteral("目标:%1").arg(cls));
          }
        }
        if (point.contains("kimi") && point["kimi"].is_object()) {
          const auto kimi = point["kimi"];
          point_parts << QStringLiteral("Kimi:%1").arg(
              kimi.value("ok", false) ? QStringLiteral("完成") : QStringLiteral("未完成"));
          const auto api = kimi.contains("api") ? kimi["api"] : kimi.contains("result") ? kimi["result"]
                                                                                        : nlohmann::json();
          const QString kimi_summary = SummarizeKimiObject(api);
          if (!kimi_summary.isEmpty()) {
            point_parts << QStringLiteral("结果:%1").arg(kimi_summary);
          } else if (kimi.contains("error")) {
            point_parts << QStringLiteral("Kimi错误:%1").arg(
                QString::fromStdString(kimi.value("error", std::string())));
          }
        }
        lines << point_parts.join(QStringLiteral(" · "));
        ++index;
      }
    } else if (data.contains("message")) {
      lines << QString::fromStdString(data.value("message", std::string()));
    }
    return lines.join(QStringLiteral("\n"));
  } catch (const std::exception&) {
    return QString::fromStdString(json_text);
  }
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
      QTimer::singleShot(1800, this, [this, attempt_id]() {
        if (attempt_id != connection_attempt_id_) {
          return;
        }
        auto* channel = channel_manager_.GetChannel();
        if (!channel) {
          display_config_widget_->SetConnectionState(
              false, false, tr("未建立连接，可在小车启动后重试。"));
          return;
        }
        if (channel->IsConnecting()) {
          QTimer::singleShot(3000, this, [this, attempt_id]() {
            if (attempt_id != connection_attempt_id_) return;
            auto* retry_ch = channel_manager_.GetChannel();
            if (!retry_ch || retry_ch->IsConnectionFailed()) {
              display_config_widget_->SetConnectionState(
                  false, false, tr("暂时无法连接 ROSBridge。请确认小车已启动且网络可达，然后重试。"));
              return;
            }
            display_config_widget_->SetConnectionState(
                true, false, tr("已连接到小车，ROSBridge 通信正常。"));
          });
          return;
        }
        if (channel->IsConnectionFailed()) {
          std::string error_msg = channel->GetConnectionError();
          std::string channel_name = channel->Name();
          if (channel_name == "ROSBridge") {
            display_config_widget_->SetConnectionState(
                false, false,
                tr("暂时无法连接 ROSBridge。请确认小车已启动且网络可达，然后重试。"));
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

    // 启动周期性连接监控，检测小车失联/自动重连
    if (connection_monitor_timer_) {
      connection_monitor_timer_->stop();
    }
    connection_monitor_timer_ = new QTimer(this);
    connection_monitor_timer_->setObjectName(QStringLiteral("connectionMonitor"));
    connect(connection_monitor_timer_, &QTimer::timeout, this, [this, attempt_id]() {
      if (attempt_id != connection_attempt_id_) return;
      auto* ch = channel_manager_.GetChannel();
      if (!ch) {
        display_config_widget_->SetConnectionState(false, false, tr("连接已断开，可在小车启动后重新连接。"));
        return;
      }
      if (ch->IsConnectionFailed()) {
        display_config_widget_->SetConnectionState(false, false, tr("小车已失联，正在尝试重连…"));
      } else if (ch->IsReconnecting()) {
        display_config_widget_->SetConnectionState(false, true, tr("正在重连小车…"));
      } else if (!ch->IsConnecting()) {
        display_config_widget_->SetConnectionState(true, false, tr("已连接到小车，ROSBridge 通信正常。"));
      }
    });
    connection_monitor_timer_->start(2000);

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
    CheckRelocationProgress(robot_pose);
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

  SUBSCRIBE(MSG_ID_NETWORK_STATUS, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (command_center_widget_) {
        command_center_widget_->SetNetworkStatus(json_str);
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_RELOCALIZATION_STATUS, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (command_center_widget_) {
        command_center_widget_->SetRelocalizationStatus(json_str);
      }
      UpdateAutoRelocalizationStatus(json_str); }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_SHELL_OUTPUT, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (!terminal_widget_) {
        return;
      }
      try {
        const auto obj = nlohmann::json::parse(json_str);
        terminal_widget_->AppendOutput(
            QString::fromStdString(obj.value("data", std::string())));
      } catch (const std::exception&) {
        terminal_widget_->AppendOutput(QString::fromStdString(json_str));
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_SHELL_STATUS, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (!terminal_widget_) {
        return;
      }
      try {
        const auto obj = nlohmann::json::parse(json_str);
        const std::string state = obj.value("state", std::string("unknown"));
        const bool running = state == "running";
        if (running) {
          terminal_widget_->SetCommandRunning(true);
        } else if (state != "idle") {
          QString status = QString::fromStdString(state);
          if (obj.contains("exit_code") && !obj["exit_code"].is_null()) {
            status += tr("，退出码 %1").arg(obj["exit_code"].get<int>());
          }
          if (obj.contains("error")) {
            status += tr("：%1").arg(QString::fromStdString(obj["error"].get<std::string>()));
          }
          terminal_widget_->AppendStatus(status);
          terminal_widget_->SetCommandRunning(false);
        } else {
          terminal_widget_->SetCommandRunning(false);
        }
      } catch (const std::exception&) {
        terminal_widget_->AppendStatus(QString::fromStdString(json_str));
        terminal_widget_->SetCommandRunning(false);
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_INSPECTION_STATUS, [this](const std::string& json_str) {
    if (!inspection_status_label_) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, json_str]() {
      const QString line = FormatInspectionStatus(json_str);
      inspection_status_label_->setText(line);
      AppendInspectionLogLine(line);
      bool is_kimi_stage = false;
      try {
        const auto data = nlohmann::json::parse(json_str);
        const std::string stage = data.value("stage", data.value("state", std::string()));
        is_kimi_stage = (stage == "kimi_running" || stage == "kimi_complete");
      } catch (const std::exception&) {}
      if (is_kimi_stage) {
        inspection_status_label_->setStyleSheet(QStringLiteral(
            "font-weight:700; color:#0f766e; background:#ccfbf1; "
            "border:1px solid #5eead4; border-radius:6px; padding:6px 10px; font-size:13px;"));
      } else {
        inspection_status_label_->setStyleSheet(QStringLiteral(""));
      }
    }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_INSPECTION_RESULT, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (inspection_result_view_) {
        AppendInspectionLogLine(FormatInspectionResult(json_str));
      }
      if (inspection_kimi_banner_) {
        AiInspectionDisplay ai_display;
        try {
          const auto data = nlohmann::json::parse(json_str);
          const auto pts = data.contains("points") ? data["points"]
                         : data.contains("results") ? data["results"]
                         : nlohmann::json::array();
          if (pts.is_array()) {
            for (const auto& point : pts) {
              const AiInspectionDisplay candidate = ExtractAiInspectionDisplay(point);
              if (candidate.valid) {
                ai_display = candidate;
              }
            }
          }
        } catch (const std::exception&) {}
        if (ai_display.valid) {
          inspection_kimi_banner_->setText(FormatAiInspectionBanner(ai_display));
          inspection_kimi_banner_->setVisible(true);
          if (command_center_widget_) {
            command_center_widget_->SetCameraInspectionResult(
                ai_display.targetName, ai_display.reading, ai_display.status);
          }
          if (inspection_status_card_) {
            const QString flashStyle = QStringLiteral(
                "QFrame { background:#d1fae5; border:2px solid #10b981; border-radius:12px; }"
                "QLabel { background:transparent; border:none; color:#334155; }"
                "QPlainTextEdit { background:#0f172a; color:#e5eefb; border:1px solid #1e293b; "
                "border-radius:10px; padding:8px; font-family:'Microsoft YaHei UI'; }");
            const QString normalStyle = inspection_status_card_->styleSheet();
            inspection_status_card_->setStyleSheet(flashStyle);
            QTimer::singleShot(1200, this, [this, normalStyle]() {
              if (inspection_status_card_) {
                inspection_status_card_->setStyleSheet(normalStyle);
              }
            });
          }
        }
      }
      if (inspection_start_button_) {
        inspection_start_button_->setText(QStringLiteral("开始任务链"));
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE(MSG_ID_AUTO_EXPLORE_STATUS, [this](const std::string& json_str) {
    LOG_INFO("auto explore status: " << json_str);
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
  if (connection_monitor_timer_) {
    connection_monitor_timer_->stop();
  }
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
      background-color: #fbfdff;
      border-bottom: 1px solid #d9e3f0;
    }
  )");

  ///////////////////////////////////////////////////////////////地图工具栏
  QHBoxLayout* horizontalLayout_tools = new QHBoxLayout(tools_strip);
  horizontalLayout_tools->setSpacing(4);
  horizontalLayout_tools->setContentsMargins(18, 5, 10, 5);
  horizontalLayout_tools->setObjectName(
      QString::fromUtf8(" horizontalLayout_tools"));

  // 现代化工具栏样式
  QString modernToolButtonStyle = UiStyle::ToolButtonStyleSheet();

  // 添加 "view" 菜单按钮
  QToolButton* view_menu_btn = new QToolButton();
  QIcon view_icon;
  view_icon.addFile(QString::fromUtf8(":/icons/tabler/menu-2.svg"),
                    QSize(32, 32), QIcon::Normal, QIcon::Off);
  view_menu_btn->setIcon(view_icon);
  view_menu_btn->setIconSize(QSize(24, 24));
  view_menu_btn->setMinimumWidth(58);
  view_menu_btn->setPopupMode(QToolButton::InstantPopup);
  view_menu_btn->setMenu(ui->menuView);
  view_menu_btn->setToolTip(tr("面板与窗口"));
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
  reloc_btn->setIconSize(QSize(24, 24));
  reloc_btn->setPopupMode(QToolButton::InstantPopup);
  reloc_btn->setMenu(CreateRelocationMenu(reloc_btn));
  horizontalLayout_tools->addWidget(reloc_btn);

  QIcon icon5;
  icon5.addFile(QString::fromUtf8(":/icons/tabler/pointer.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* edit_map_btn = new QToolButton();
  edit_map_btn->setIcon(icon5);
  edit_map_btn->setText("编辑地图");
  edit_map_btn->setIconSize(QSize(24, 24));
  edit_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  edit_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(edit_map_btn);

  QIcon icon6;
  icon6.addFile(QString::fromUtf8(":/icons/tabler/folder-open.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* open_map_btn = new QToolButton();
  open_map_btn->setIcon(icon6);
  open_map_btn->setText("打开地图");
  open_map_btn->setIconSize(QSize(24, 24));
  open_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  open_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(open_map_btn);

  QIcon icon8;
  icon8.addFile(QString::fromUtf8(":/icons/tabler/device-floppy.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);

  QToolButton* save_map_btn = new QToolButton();
  save_map_btn->setIcon(icon8);
  save_map_btn->setText("保存地图");
  save_map_btn->setIconSize(QSize(24, 24));
  save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(save_map_btn);

  QIcon icon7;
  icon7.addFile(QString::fromUtf8(":/images/re_save.svg"),
                QSize(32, 32), QIcon::Normal, QIcon::Off);
  QToolButton* re_save_map_btn = new QToolButton();
  re_save_map_btn->setIcon(icon7);
  re_save_map_btn->setText("另存为");
  re_save_map_btn->setIconSize(QSize(24, 24));
  re_save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  re_save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(re_save_map_btn);
  center_layout->addWidget(tools_strip);

  horizontalLayout_tools->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  // Compact, consistent sensor status pills.
  battery_bar_ = new QProgressBar();
  battery_bar_->setObjectName(QString::fromUtf8("battery_bar_"));
  battery_bar_->setRange(0, 100);
  battery_bar_->setValue(0);
  battery_bar_->setFormat(QStringLiteral("%p%"));
  battery_bar_->setFixedSize(QSize(68, 22));
  battery_bar_->setStyleSheet(QStringLiteral(
                                  "QProgressBar#battery_bar_ { border:1px solid #b8cdf2; background:#edf3fb; "
                                  "border-radius:7px; text-align:center; "
                                  "color:#18212f; font-size:%1px; font-weight:700; }"
                                  "QProgressBar#battery_bar_::chunk { background:#79a7f8; border-radius:6px; }")
                                  .arg(UiStyle::FontSmallPx()));
  battery_bar_->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(CreateTopStatusPill(
      QStringLiteral(":/icons/tabler/battery.svg"), battery_bar_, tr("电池电量"), 110, tools_strip));

  label_dht11_temp_ = new QLabel(QStringLiteral("--.- °C"), this);
  label_dht11_temp_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());
  label_dht11_temp_->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(CreateTopStatusPill(
      QStringLiteral(":/icons/tabler/temperature.svg"), label_dht11_temp_, tr("环境温度"), 108, tools_strip));

  label_dht11_humi_ = new QLabel(QStringLiteral("--.- %"), this);
  label_dht11_humi_->setStyleSheet(UiStyle::TopStatusLabelStyleSheet());
  label_dht11_humi_->setAlignment(Qt::AlignCenter);
  horizontalLayout_tools->addWidget(CreateTopStatusPill(
      QStringLiteral(":/icons/tabler/droplet.svg"), label_dht11_humi_, tr("环境湿度"), 114, tools_strip));

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

  horizontalLayout_tools->addSpacing(8);
  QPushButton* min_btn = new QPushButton(this);
  maximize_button_ = new QPushButton(this);
  QPushButton* close_btn = new QPushButton(this);
  min_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
  close_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  min_btn->setToolTip(tr("最小化"));
  maximize_button_->setToolTip(tr("最大化"));
  close_btn->setToolTip(tr("关闭"));
  for (auto* button : {min_btn, maximize_button_, close_btn}) {
    button->setFixedSize(32, 28);
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
  settings_dock_->setWidget(display_config_widget_, ads::CDockWidget::ForceNoScrollArea);
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
  ConfigureDockWidget(SpeedCtrlDockWidget, QSize(400, 360), QSize(500, 420));
  auto speed_ctrl_area =
      dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                                   SpeedCtrlDockWidget, settings_dock_area_);
  ui->menuView->addAction(SpeedCtrlDockWidget->toggleViewAction());

  /////////////////////////////////////////////////////////导航任务列表
  QWidget* task_list_widget = new QWidget();
  nav_goal_table_view_ = new NavGoalTableView();
  QVBoxLayout* horizontalLayout_13 = new QVBoxLayout();
  horizontalLayout_13->setContentsMargins(14, 14, 14, 14);
  horizontalLayout_13->setSpacing(10);
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
  inspection_start_button_ = btn_start_task_chain;

  QCheckBox* loop_task_checkbox = new QCheckBox("循环任务");
  loop_task_checkbox->setStyleSheet(UiStyle::CheckBoxStyleSheet());

  QHBoxLayout* horizontalLayout_14 = new QHBoxLayout();
  horizontalLayout_15->addWidget(btn_add_one_goal);
  horizontalLayout_14->addWidget(btn_start_task_chain);

  QHBoxLayout* loop_task_layout = new QHBoxLayout();
  loop_task_layout->setContentsMargins(0, 0, 0, 0);
  loop_task_layout->addStretch();
  loop_task_layout->addWidget(loop_task_checkbox);
  loop_task_layout->addStretch();

  QPushButton* btn_load_task_chain = new QPushButton("加载任务链");
  QPushButton* btn_save_task_chain = new QPushButton("保存任务链");
  btn_load_task_chain->setStyleSheet(modernButtonStyle);
  btn_save_task_chain->setStyleSheet(modernButtonStyle);

  QHBoxLayout* horizontalLayout_16 = new QHBoxLayout();
  horizontalLayout_16->addWidget(btn_load_task_chain);
  horizontalLayout_16->addWidget(btn_save_task_chain);

  auto* inspection_status_card = new QFrame();
  inspection_status_card_ = inspection_status_card;
  inspection_status_card->setStyleSheet(QStringLiteral(
      "QFrame { background:#f8fbff; border:1px solid #dce6f2; border-radius:12px; }"
      "QLabel { background:transparent; border:none; color:#334155; }"
      "QPlainTextEdit { background:#0f172a; color:#e5eefb; border:1px solid #1e293b; "
      "border-radius:10px; padding:8px; font-family:'Microsoft YaHei UI'; }"));
  auto* inspection_status_layout = new QVBoxLayout(inspection_status_card);
  inspection_status_layout->setContentsMargins(12, 10, 12, 12);
  inspection_status_layout->setSpacing(8);
  auto* inspection_title = new QLabel(QStringLiteral("巡检实时反馈"));
  inspection_title->setStyleSheet(QStringLiteral("font-weight:700; color:#0f172a;"));
  inspection_status_label_ = new QLabel(QStringLiteral("待命。添加点位后点击开始任务链。"));
  inspection_status_label_->setWordWrap(true);
  inspection_result_view_ = new QPlainTextEdit();
  inspection_result_view_->setReadOnly(true);
  inspection_result_view_->setPlaceholderText(QStringLiteral("前往目标点 → 到达识别 → 旋转寻找 → AI视觉分析 → 返回状态"));
  inspection_result_view_->setFixedHeight(150);
  inspection_status_layout->addWidget(inspection_title);
  inspection_status_layout->addWidget(inspection_status_label_);
  inspection_status_layout->addWidget(inspection_result_view_);
  inspection_kimi_banner_ = new QLabel();
  inspection_kimi_banner_->setWordWrap(true);
  inspection_kimi_banner_->setStyleSheet(QStringLiteral(
      "QLabel { background:#e7f7ed; color:#1a4d2e; border:1px solid #8fcf9f; "
      "border-radius:8px; padding:10px 14px; font-size:14px; font-weight:700; }"));
  inspection_kimi_banner_->setVisible(false);
  inspection_status_layout->addWidget(inspection_kimi_banner_);

  horizontalLayout_13->addLayout(horizontalLayout_15);
  horizontalLayout_13->addLayout(horizontalLayout_14);
  horizontalLayout_13->addLayout(loop_task_layout);
  horizontalLayout_13->addLayout(horizontalLayout_16);
  horizontalLayout_13->addWidget(inspection_status_card);
  nav_goal_list_dock_widget->setWidget(task_list_widget);
  ConfigureDockWidget(nav_goal_list_dock_widget, QSize(540, 420), QSize(560, 660));
  nav_goal_list_dock_widget->setMaximumSize(720, 9999);
  dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                               nav_goal_list_dock_widget, center_docker_area_);
  ConfigureFloatingOnOpen(nav_goal_list_dock_widget, QSize(760, 760));
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
              if (nav_goal_table_view_->RowCount() == 0) {
                QMessageBox::information(this, QStringLiteral("任务链为空"),
                                         QStringLiteral("请先添加至少一个点位。"),
                                         QMessageBox::Ok);
                return;
              }
              btn_start_task_chain->setText("停止任务链");
              const auto request =
                  nav_goal_table_view_->BuildInspectionRequest(loop_task_checkbox->isChecked());
              if (inspection_status_label_) {
                inspection_status_label_->setText(QStringLiteral("已发送任务链，等待小车响应…"));
              }
              last_inspection_log_line_.clear();
              if (inspection_result_view_) {
                inspection_result_view_->clear();
                AppendInspectionLogLine(QStringLiteral("已发送任务链，等待小车响应…"));
              }
              PUBLISH(MSG_ID_INSPECTION_REQUEST, request);
            } else {
              btn_start_task_chain->setText("开始任务链");
              PUBLISH(MSG_ID_INSPECTION_REQUEST, std::string("{\"command\":\"cancel\"}"));
              if (inspection_status_label_) {
                inspection_status_label_->setText(QStringLiteral("已发送停止请求。"));
              }
            }
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

  //////////////////////////////////////////////////////模拟终端
  terminal_widget_ = new TerminalWidget();
  terminal_dock_ = new ads::CDockWidget("模拟终端");
  terminal_dock_->setWidget(terminal_widget_);
  ConfigureDockWidget(terminal_dock_, QSize(620, 420), QSize(860, 580));
  dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                               terminal_dock_, center_docker_area_);
  terminal_dock_->toggleView(false);
  ConfigureFloatingOnOpen(terminal_dock_, QSize(860, 580));
  ui->menuView->addAction(terminal_dock_->toggleViewAction());
  connect(terminal_widget_, &TerminalWidget::CommandRequested, this,
          [](const QString& request) {
            PUBLISH(MSG_ID_SHELL_REQUEST, request.toStdString());
          });
  connect(terminal_widget_, &TerminalWidget::TerminateRequested, this, []() {
    PUBLISH(MSG_ID_SHELL_CANCEL, std::string("{}"));
  });

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
            BeginRelocation(pose);
          });
  connect(display_manager_, &Display::DisplayManager::signalPub2DGoal,
          [this](const RobotPose& pose) {
            PUBLISH(MSG_ID_SET_NAV_GOAL_POSE, pose);
          });
  // ui相关
  connect(re_save_map_btn, &QToolButton::clicked,
          this, &MainWindow::SaveMapToLocalAndRobot);
  connect(save_map_btn, &QToolButton::clicked,
          this, &MainWindow::SaveMapToLocalAndRobot);

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
  resize_area(command_center_dock_area_, 600);
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
  Q_UNUSED(state);
}
void MainWindow::SlotSetBatteryStatus(double percent, double voltage) {
  Q_UNUSED(voltage);
  // ROS BatteryState.percentage is 0.0-1.0; QProgressBar needs 0-100
  battery_bar_->setValue(static_cast<int>(percent * 100));
}

bool MainWindow::IsRelocationPoseValid(const RobotPose& pose, QString* reason) {
  auto map = display_manager_->GetOccupancyMap();
  if (map.Rows() <= 0 || map.Cols() <= 0) {
    if (reason) *reason = tr("尚未收到有效地图");
    return false;
  }
  if (!map.inMap(pose.x, pose.y)) {
    if (reason) *reason = tr("所选位置超出地图范围");
    return false;
  }

  int col = 0;
  int row = 0;
  map.xy2idx(pose.x, pose.y, col, row);
  const int display_row = map.Rows() - 1 - row;
  const auto data = map.GetMapData();
  if (display_row < 0 || display_row >= map.Rows() || col < 0 ||
      col >= map.Cols()) {
    if (reason) *reason = tr("所选位置超出地图栅格范围");
    return false;
  }
  const int value = data(display_row, col);
  if (value < 0) {
    if (reason) *reason = tr("不能在未知区域重定位");
    return false;
  }
  if (value >= 50) {
    if (reason) *reason = tr("不能在障碍物上重定位");
    return false;
  }

  const double resolution = map.map_config.resolution;
  const int clearance_cells =
      std::max(1, static_cast<int>(std::ceil(0.15 / resolution)));
  for (int dr = -clearance_cells; dr <= clearance_cells; ++dr) {
    for (int dc = -clearance_cells; dc <= clearance_cells; ++dc) {
      const int check_row = display_row + dr;
      const int check_col = col + dc;
      if (check_row < 0 || check_row >= map.Rows() || check_col < 0 ||
          check_col >= map.Cols()) {
        continue;
      }
      if (data(check_row, check_col) >= 50) {
        if (reason) *reason = tr("所选位置距离障碍物过近（需至少约 0.15 m）");
        return false;
      }
    }
  }
  return true;
}


QMenu* MainWindow::CreateRelocationMenu(QToolButton* reloc_button) {
  auto* menu = new QMenu(reloc_button);
  auto* action = new QWidgetAction(menu);
  auto* panel = new QFrame(menu);
  panel->setMinimumWidth(330);
  panel->setStyleSheet(QStringLiteral(
      "QFrame { background:#ffffff; border:1px solid #dce6f2; border-radius:14px; }"
      "QLabel { background:transparent; border:none; color:#334155; }"));
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(14, 12, 14, 14);
  layout->setSpacing(10);
  auto* title = new QLabel(tr("重定位"), panel);
  title->setStyleSheet(QStringLiteral("font-weight:800; color:#0f172a; font-size:15px;"));
  auto* hint = new QLabel(tr("手动：在地图点选位置和朝向。自动：用当前激光轮廓匹配静态地图，5秒超时。"), panel);
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("color:#64748b;"));
  auto* manual_btn = new QPushButton(tr("手动重定位"), panel);
  auto_relocalization_start_button_ = new QPushButton(tr("自动定位（激光匹配）"), panel);
  auto_relocalization_cancel_button_ = new QPushButton(tr("取消自动定位"), panel);
  auto_relocalization_status_label_ = new QLabel(tr("自动定位：待命"), panel);
  auto_relocalization_status_label_->setWordWrap(true);
  auto_relocalization_status_label_->setStyleSheet(QStringLiteral(
      "QLabel { color:#2563eb; background:#eff6ff; border:1px solid #bfdbfe; "
      "border-radius:10px; padding:8px 10px; }"));
  manual_btn->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  auto_relocalization_start_button_->setStyleSheet(UiStyle::MainButtonStyleSheet());
  auto_relocalization_cancel_button_->setStyleSheet(UiStyle::DangerButtonStyleSheet());
  auto_relocalization_cancel_button_->setEnabled(false);
  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addWidget(manual_btn);
  layout->addWidget(auto_relocalization_start_button_);
  layout->addWidget(auto_relocalization_cancel_button_);
  layout->addWidget(auto_relocalization_status_label_);
  action->setDefaultWidget(panel);
  menu->addAction(action);
  connect(manual_btn, &QPushButton::clicked, this, [this, menu]() {
    auto map = display_manager_->GetOccupancyMap();
    if (map.Rows() <= 0 || map.Cols() <= 0) {
      QMessageBox::warning(this, tr("无法重定位"),
                           tr("当前尚未收到有效地图，请先加载静态地图并启动 AMCL。"));
      return;
    }
    statusBar()->showMessage(
        tr("手动重定位：请在地图上选择位置和朝向。"), 6000);
    display_manager_->StartReloc();
    menu->hide();
  });
  connect(auto_relocalization_start_button_, &QPushButton::clicked,
          this, &MainWindow::StartAutoRelocalization);
  connect(auto_relocalization_cancel_button_, &QPushButton::clicked,
          this, &MainWindow::CancelAutoRelocalization);
  return menu;
}

void MainWindow::StartAutoRelocalization() {
  auto map = display_manager_->GetOccupancyMap();
  if (map.Rows() <= 0 || map.Cols() <= 0) {
    QMessageBox::warning(this, tr("无法自动定位"),
                         tr("当前尚未收到有效地图，请先加载静态地图并启动 AMCL。"));
    return;
  }
  nlohmann::json request;
  request["command"] = "start";
  request["method"] = "scan_match";
  request["timeout"] = 5.0;
  PUBLISH(MSG_ID_RELOCALIZATION_REQUEST, request.dump());
  if (auto_relocalization_start_button_) {
    auto_relocalization_start_button_->setEnabled(false);
  }
  if (auto_relocalization_cancel_button_) {
    auto_relocalization_cancel_button_->setEnabled(true);
  }
  if (auto_relocalization_status_label_) {
    auto_relocalization_status_label_->setText(tr("自动定位：正在匹配当前雷达轮廓…"));
  }
  statusBar()->showMessage(tr("自动定位已开始：5秒内尝试让激光点与地图边缘重合。"), 5000);
}

void MainWindow::CancelAutoRelocalization() {
  nlohmann::json request;
  request["command"] = "cancel";
  PUBLISH(MSG_ID_RELOCALIZATION_CANCEL, request.dump());
  if (auto_relocalization_cancel_button_) {
    auto_relocalization_cancel_button_->setEnabled(false);
  }
  if (auto_relocalization_status_label_) {
    auto_relocalization_status_label_->setText(tr("自动定位：正在取消…"));
  }
}

void MainWindow::UpdateAutoRelocalizationStatus(const std::string& json) {
  try {
    const auto data = nlohmann::json::parse(json);
    const std::string state_std = data.value("state", std::string());
    const QString state = QString::fromStdString(state_std);
    QString message = QString::fromStdString(data.value("message", state_std));
    if (data.contains("score") && data["score"].is_number()) {
      message += tr(" · 匹配度 %1").arg(data["score"].get<double>(), 0, 'f', 2);
    }
    const bool running = state == QStringLiteral("preflight_ok") ||
                         state == QStringLiteral("matching") ||
                         state == QStringLiteral("applying") ||
                         state == QStringLiteral("rotating") ||
                         state == QStringLiteral("converging") ||
                         state == QStringLiteral("busy") ||
                         state == QStringLiteral("cancelling");
    if (auto_relocalization_status_label_) {
      auto_relocalization_status_label_->setText(tr("自动定位：%1").arg(message));
    }
    if (auto_relocalization_start_button_) {
      auto_relocalization_start_button_->setEnabled(!running);
    }
    if (auto_relocalization_cancel_button_) {
      auto_relocalization_cancel_button_->setEnabled(running && state != QStringLiteral("cancelling"));
    }
    if (state == QStringLiteral("success")) {
      statusBar()->showMessage(tr("自动定位成功：激光点已按匹配结果刷新到地图位置。"), 6000);
    } else if (state == QStringLiteral("failed") || state == QStringLiteral("timeout") ||
               state == QStringLiteral("rejected")) {
      statusBar()->showMessage(tr("自动定位失败：可改用手动重定位微调。"), 7000);
    }
  } catch (const std::exception&) {
    if (auto_relocalization_status_label_) {
      auto_relocalization_status_label_->setText(tr("自动定位：状态数据无效"));
    }
  }
}

void MainWindow::AppendInspectionLogLine(const QString& line) {
  if (!inspection_result_view_) {
    return;
  }
  const QString compact = line.trimmed();
  if (compact.isEmpty() || compact == last_inspection_log_line_) {
    return;
  }
  last_inspection_log_line_ = compact;
  static const int kMaxLogLines = 200;
  if (inspection_result_view_->blockCount() > kMaxLogLines) {
    QTextCursor cursor = inspection_result_view_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
                        inspection_result_view_->blockCount() - kMaxLogLines);
    cursor.removeSelectedText();
  }
  const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  inspection_result_view_->appendPlainText(QStringLiteral("[%1] %2").arg(ts, compact));
}

void MainWindow::BeginRelocation(const RobotPose& pose) {
  QString reason;
  if (!IsRelocationPoseValid(pose, &reason)) {
    QMessageBox::warning(this, tr("重定位位置无效"), reason);
    return;
  }

  const int attempt_id = ++relocation_attempt_id_;
  relocation_pending_ = true;
  relocation_target_ = pose;
  relocation_stable_samples_ = 0;
  relocation_elapsed_.restart();
  statusBar()->showMessage(
      tr("正在重定位到 (%1, %2, %3°)…")
          .arg(pose.x, 0, 'f', 2)
          .arg(pose.y, 0, 'f', 2)
          .arg(rad2deg(pose.theta), 0, 'f', 2));

  for (int index = 0; index < 3; ++index) {
    QTimer::singleShot(index * 150, this, [this, pose, attempt_id]() {
      if (relocation_pending_ && attempt_id == relocation_attempt_id_) {
        PUBLISH(MSG_ID_SET_RELOC_POSE, pose);
      }
    });
  }

  QTimer::singleShot(10000, this, [this, attempt_id]() {
    if (!relocation_pending_ || attempt_id != relocation_attempt_id_) return;
    relocation_pending_ = false;
    statusBar()->showMessage(
        tr("重定位确认超时：请检查当前是否为 AMCL 模式，以及雷达、TF 和地图是否正常。"),
        10000);
    QMessageBox::warning(
        this, tr("重定位未确认"),
        tr("10 秒内未检测到稳定的目标位姿。\n\n"
           "请确认：\n"
           "1. 当前使用静态地图 + AMCL\n"
           "2. /scan、/amcl_pose 和 map→odom TF 正常\n"
           "3. 设置的方向与小车实际方向大致一致"));
  });
}

void MainWindow::CheckRelocationProgress(const RobotPose& pose) {
  if (!relocation_pending_) return;
  const double dx = pose.x - relocation_target_.x;
  const double dy = pose.y - relocation_target_.y;
  const double distance = std::hypot(dx, dy);
  const double angle_error = std::abs(std::atan2(
      std::sin(pose.theta - relocation_target_.theta),
      std::cos(pose.theta - relocation_target_.theta)));
  if (distance <= 0.20 && angle_error <= deg2rad(10.0)) {
    ++relocation_stable_samples_;
  } else {
    relocation_stable_samples_ = 0;
  }
  if (relocation_stable_samples_ < 3) return;

  relocation_pending_ = false;
  statusBar()->showMessage(
      tr("重定位成功：位置误差 %1 m，角度误差 %2°，耗时 %3 s")
          .arg(distance, 0, 'f', 2)
          .arg(rad2deg(angle_error), 0, 'f', 2)
          .arg(relocation_elapsed_.elapsed() / 1000.0, 0, 'f', 2),
      8000);
}

void MainWindow::SaveMapToLocalAndRobot() {
  bool accepted = false;
  const QString default_name =
      QString("map_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
  const QString map_name =
      QInputDialog::getText(this, tr("保存地图"), tr("地图名称（字母、数字、下划线或短横线）："),
                            QLineEdit::Normal, default_name, &accepted)
          .trimmed();
  if (!accepted || map_name.isEmpty()) {
    return;
  }
  for (const QChar ch : map_name) {
    const ushort code = ch.unicode();
    const bool ascii_alnum =
        (code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z') ||
        (code >= 'a' && code <= 'z');
    if (!(ascii_alnum || ch == '_' || ch == '-')) {
      QMessageBox::warning(this, tr("名称无效"),
                           tr("地图名称只能包含字母、数字、下划线和短横线。"));
      return;
    }
  }

  const QString initial_dir =
      QFileInfo(QString::fromStdString(map_path_)).absolutePath();
  const QString directory =
      QFileDialog::getExistingDirectory(this, tr("选择 Windows 保存目录"),
                                        initial_dir,
                                        QFileDialog::ShowDirsOnly |
                                            QFileDialog::DontResolveSymlinks);
  if (directory.isEmpty()) {
    return;
  }

  const QString base_path = QDir(directory).filePath(map_name);
  auto occ_map = display_manager_->GetOccupancyMap();
  occ_map.Save(base_path.toStdString());
  const QString yaml_path = base_path + ".yaml";
  const QString pgm_path = base_path + ".pgm";
  if (!QFileInfo::exists(yaml_path) || !QFileInfo::exists(pgm_path)) {
    QMessageBox::critical(this, tr("保存失败"),
                          tr("未能生成地图的 YAML/PGM 文件，请检查目录写入权限。"));
    return;
  }

  display_manager_->UpdateOCCMap(occ_map);
  const auto topology_map = display_manager_->GetTopologyMap();
  Config::ConfigManager::Instance()->WriteTopologyMap(
      (base_path + ".topology").toStdString(), topology_map);
  PUBLISH(MSG_ID_TOPOLOGY_MAP_UPDATE, topology_map);
  map_path_ = base_path.toStdString();

  QFile yaml_file(yaml_path);
  QFile pgm_file(pgm_path);
  if (!yaml_file.open(QIODevice::ReadOnly) || !pgm_file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("本地已保存"),
                         tr("地图已保存到 Windows，但读取文件上传到小车时失败。"));
    return;
  }

  nlohmann::json request;
  request["request_id"] =
      QString("qt-map-%1-%2")
          .arg(QDateTime::currentMSecsSinceEpoch())
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
          .toStdString();
  request["command"] = "upload_map";
  request["target"] = "navigation";
  request["params"] = {
      {"map_name", map_name.toStdString()},
      {"yaml_b64", yaml_file.readAll().toBase64().toStdString()},
      {"pgm_b64", pgm_file.readAll().toBase64().toStdString()},
      {"activate", false}};
  PUBLISH(MSG_ID_COMMAND_REQUEST, request.dump());

  QMessageBox::information(
      this, tr("地图已保存"),
      tr("Windows 本地保存完成：\n%1\n\n已向小车发送同名地图；上传结果可在运维面板日志中查看。")
          .arg(yaml_path));
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
