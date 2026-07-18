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
#include <QAction>
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QScreen>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QUuid>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <opencv2/opencv.hpp>
#include <utility>
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
#include "mission_contract.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <nlohmann/json.hpp>
#include "display/manager/view_manager.h"
#include "msg/channel_publish_result.h"
#include "msg/diagnostic_snapshot.h"
#include "widgets/command_center_widget.h"
#include "widgets/display_config_widget.h"
#include "widgets/speed_ctrl.h"
#include "widgets/terminal_widget.h"
#include "widgets/ui_style.h"
using namespace ads;
namespace {

constexpr int kUiLayoutVersion = 12;

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
                          "QFrame#topStatusPill { background:%1; border:1px solid %2; border-radius:4px; }"
                          "QFrame#topStatusPill:hover { background:%3; border-color:%4; }"
                          "QFrame#topStatusPill QLabel { background:transparent; border:none; }")
                          .arg(UiStyle::Palette::ToolbarBg, UiStyle::Palette::Border,
                               UiStyle::Palette::Surface, UiStyle::Palette::BorderHover));

  auto* layout = new QHBoxLayout(pill);
  layout->setContentsMargins(8, 0, 8, 0);
  layout->setSpacing(5);

  auto* icon = new QLabel(pill);
  icon->setPixmap(UiStyle::TintedIcon(icon_path, QSize(18, 18)).pixmap(18, 18));
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
  QString value = meter.contains("reading")               ? JsonValueToText(meter["reading"])
                  : meter.contains("best_effort_reading") ? JsonValueToText(meter["best_effort_reading"])
                  : meter.contains("value")               ? JsonValueToText(meter["value"])
                  : meter.contains("读数")                ? JsonValueToText(meter["读数"])
                                                          : QString();
  if (value.trimmed().isEmpty() || value.trimmed().toLower() == QStringLiteral("unknown")) {
    return QStringLiteral("未识别");
  }
  const QString unit = meter.contains("unit")   ? JsonValueToText(meter["unit"])
                       : meter.contains("单位") ? JsonValueToText(meter["单位"])
                                                : QString();
  return unit.isEmpty() ? value : value + unit;
}

struct AiInspectionDisplay {
  bool valid = {false};
  QString waypoint;
  QString targetName = {QStringLiteral("水表")};
  QString reading = {QStringLiteral("未识别")};
  QString status = {QStringLiteral("未识别")};
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
    const QString value = meter.contains("value")     ? JsonValueToText(meter["value"])
                          : meter.contains("reading") ? JsonValueToText(meter["reading"])
                          : meter.contains("读数")    ? JsonValueToText(meter["读数"])
                                                      : QString();
    const QString unit = meter.contains("unit")   ? JsonValueToText(meter["unit"])
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
      const QString value = reading.contains("value")     ? JsonValueToText(reading["value"])
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
      {"accepted", QStringLiteral("任务已受理")},
      {"home_recorded", QStringLiteral("已记录起点")},
      {"waiting_move_base", QStringLiteral("等待导航")},
      {"navigating", QStringLiteral("前往目标点")},
      {"arrived", QStringLiteral("已到达目标点")},
      {"point_complete", QStringLiteral("本点已完成")},
      {"search_settling", QStringLiteral("到达后识别")},
      {"searching_target", QStringLiteral("正在搜索目标")},
      {"search_rotating", QStringLiteral("90°步进旋转寻找")},
      {"search_paused", QStringLiteral("暂停判定目标")},
      {"aligned", QStringLiteral("目标已对准")},
      {"target_confirmed", QStringLiteral("目标已确认")},
      {"target_lost", QStringLiteral("目标暂时丢失")},
      {"rotation_sensor_stop", QStringLiteral("传感器异常，停止旋转")},
      {"tf_unavailable", QStringLiteral("定位反馈不可用")},
      {"align_timeout", QStringLiteral("目标对准超时")},
      {"invalid_detection", QStringLiteral("识别框数据无效")},
      {"target_skipped", QStringLiteral("未找到目标，跳过本点")},
      {"kimi_running", QStringLiteral("AI视觉分析")},
      {"kimi_complete", QStringLiteral("AI分析完成")},
      {"returning_home", QStringLiteral("正在返航")},
      {"complete", QStringLiteral("巡检完成")},
      {"completed", QStringLiteral("任务完成")},
      {"cancelling", QStringLiteral("正在取消")},
      {"cancelled", QStringLiteral("已取消")},
      {"error", QStringLiteral("巡检异常")},
      {"busy", QStringLiteral("任务运行中")},
      {"rejected", QStringLiteral("任务被拒绝")},
      {"bad_request", QStringLiteral("任务参数错误")},
      {"cancel_ignored", QStringLiteral("取消请求已忽略")},
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
            point_parts << QStringLiteral("Kimi错误:%1").arg(QString::fromStdString(kimi.value("error", std::string())));
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
  if (display_config_widget_) {
    display_config_widget_->SetConnectionState(
        false, true, tr("正在检测小车连接，请稍候…"));
  }
  RestoreState();
  QTimer::singleShot(30, this, [this]() { openChannel(); });
}
bool MainWindow::openChannel() {
  const int attempt_id = ++connection_attempt_id_;
  if (display_config_widget_) {
    display_config_widget_->SetConnectionState(
        false, true, tr("正在检测小车连接，请稍候…"));
  }
  if (channel_manager_.OpenChannelAuto()) {
    PublishImageStreamVisibility();
    if (!channel_subscriptions_registered_) {
      registerChannel();
      channel_subscriptions_registered_ = true;
    }

    // 延迟检查连接状态（连接超时是5秒）
    auto* channel = channel_manager_.GetChannel();
    if (channel) {
      QTimer::singleShot(800, this, [this, attempt_id]() {
        if (attempt_id != connection_attempt_id_) {
          return;
        }
        auto* channel = channel_manager_.GetChannel();
        if (!channel) {
          display_config_widget_->SetConnectionState(
              false, false, tr("未建立连接，可在小车启动后重试。"));
          return;
        }
        if (channel->IsConnected()) {
          display_config_widget_->SetConnectionState(
              true, false, tr("已连接到小车，ROSBridge 通信正常。"));
          return;
        }
        if (channel->IsConnecting()) {
          QTimer::singleShot(2000, this, [this, attempt_id]() {
            if (attempt_id != connection_attempt_id_) return;
            auto* retry_ch = channel_manager_.GetChannel();
            if (retry_ch && retry_ch->IsConnected()) {
              display_config_widget_->SetConnectionState(
                  true, false, tr("已连接到小车，ROSBridge 通信正常。"));
              return;
            }
            if (!retry_ch || retry_ch->IsConnectionFailed()) {
              display_config_widget_->SetConnectionState(
                  false, false, tr("暂时无法连接 ROSBridge。请确认小车已启动且网络可达，然后重试。"));
              return;
            }
            display_config_widget_->SetConnectionState(
                false, true, tr("仍在等待 ROSBridge 完成握手…"));
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
              false, true, tr("正在等待 ROSBridge 完成握手…"));
        }
      });
    }

    // 启动周期性连接监控，检测小车失联/自动重连
    if (connection_monitor_timer_) {
      connection_monitor_timer_->stop();
      connection_monitor_timer_->deleteLater();
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
      if (ch->IsConnected()) {
        display_config_widget_->SetConnectionState(
            true, false, tr("已连接到小车，ROSBridge 通信正常。"));
      } else if (ch->IsConnectionFailed()) {
        display_config_widget_->SetConnectionState(false, false, tr("小车已失联，正在尝试重连…"));
      } else if (ch->IsReconnecting() || ch->IsConnecting()) {
        display_config_widget_->SetConnectionState(false, true, tr("正在重连小车…"));
      } else {
        display_config_widget_->SetConnectionState(
            false, true, tr("正在确认 ROSBridge 连接状态…"));
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
    PublishImageStreamVisibility();
    registerChannel();
    return true;
  }
  return false;
}
void MainWindow::registerChannel() {
  SUBSCRIBE_QOBJECT(this, MSG_ID_ODOM_POSE, [this](const RobotState& data) {
    updateOdomInfo(data);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_ROBOT_POSE, [this](const RobotPose& robot_pose) {
    nav_goal_table_view_->UpdateRobotPose(robot_pose);
    Display::ViewManager* view_manager = dynamic_cast<Display::ViewManager*>(display_manager_->GetViewPtr());
    if (view_manager) {
      view_manager->UpdateRobotPos("机器人: (" + QString::number(robot_pose.x, 'f', 2) + ", " +
                                   QString::number(robot_pose.y, 'f', 2) + ", " +
                                   QString::number(robot_pose.theta, 'f', 2) + ")");
    }
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_BATTERY_STATE, [this](const std::map<std::string, std::string>& map) {
    this->SlotSetBatteryStatus(std::stod(map.at("percent")),
                               std::stod(map.at("voltage")));
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_IMAGE, [this](const std::pair<std::string, std::shared_ptr<cv::Mat>>& location_to_mat) {
    this->SlotRecvImage(location_to_mat.first, location_to_mat.second);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_DIAGNOSTIC, [this](const basic::DiagnosticSnapshot& snap) {
    if (command_center_widget_) {
      command_center_widget_->SetDiagnosticSnapshot(snap);
    }
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_NETWORK_STATUS, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (command_center_widget_) {
        command_center_widget_->SetNetworkStatus(json_str);
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_LOCALIZATION_POSE,
                    [this](const LocalizationEstimate& estimate) {
                      CheckRelocationProgress(estimate);
                    });

  SUBSCRIBE_QOBJECT(this, MSG_ID_COMMAND_RESPONSE, [this](const std::string& json) {
    HandleMapCommandResponse(json);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_SHELL_OUTPUT, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      if (!terminal_widget_) {
        return;
      }
      try {
        const auto obj = nlohmann::json::parse(json_str);
        terminal_widget_->AppendOutput(
            QString::fromStdString(obj.value("data", std::string())),
            QString::fromStdString(obj.value("stream", std::string("stdout"))));
      } catch (const std::exception&) {
        terminal_widget_->AppendOutput(QString::fromStdString(json_str));
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_SHELL_STATUS, [this](const std::string& json_str) {
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
          const std::map<std::string, QString> state_text = {
              {"completed", tr("命令执行完成")},
              {"cancelled", tr("命令已终止")},
              {"timeout", tr("命令执行超时")},
              {"rejected", tr("命令被安全策略拒绝")},
              {"failed", tr("命令执行失败")},
              {"error", tr("命令执行失败")},
          };
          const auto state_it = state_text.find(state);
          QString status = state_it == state_text.end()
                               ? QString::fromStdString(state)
                               : state_it->second;
          if (obj.contains("exit_code") && !obj["exit_code"].is_null()) {
            status += tr("，退出码 %1").arg(obj["exit_code"].get<int>());
          }
          if (obj.contains("error")) {
            status += tr("：%1").arg(QString::fromStdString(obj["error"].get<std::string>()));
          }
          terminal_widget_->SetCommandRunning(false);
          terminal_widget_->AppendStatus(status);
        } else {
          terminal_widget_->SetCommandRunning(false);
        }
      } catch (const std::exception&) {
        terminal_widget_->SetCommandRunning(false);
        terminal_widget_->AppendStatus(QString::fromStdString(json_str));
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_MISSION_STATUS, [this](const std::string& json_str) {
    if (!inspection_status_label_) {
      return;
    }
    QMetaObject::invokeMethod(this, [this, json_str]() {
      try {
        const nlohmann::json data = nlohmann::json::parse(json_str);
        if (!IsCurrentMissionMessage(data)) {
          return;
        }
        const QString line = FormatInspectionStatus(json_str);
        inspection_status_label_->setText(line);
        AppendInspectionLogLine(line);
        std::string stage = AppContract::JsonStringOr(data, "stage");
        if (stage.empty()) {
          stage = AppContract::JsonStringOr(data, "state");
        }
        const QString request_id = QString::fromStdString(
            AppContract::JsonStringOr(data, "request_id"));
        if (stage == "accepted") {
          mission_tracker_.Accept(request_id);
        }
        UpdateInspectionProgress(data);
        if (stage == "error" || stage == "rotation_sensor_stop" ||
            stage == "tf_unavailable" || stage == "invalid_detection") {
          inspection_status_label_->setStyleSheet(
              UiStyle::StatusDangerStyleSheet());
        } else if (stage == "target_skipped" || stage == "target_lost" ||
                   stage == "align_timeout") {
          inspection_status_label_->setStyleSheet(
              UiStyle::StatusWarningStyleSheet());
        } else if (stage == "kimi_complete" || stage == "complete" ||
                   stage == "completed") {
          inspection_status_label_->setStyleSheet(
              UiStyle::StatusSuccessStyleSheet());
        } else {
          inspection_status_label_->setStyleSheet(
              UiStyle::StatusInfoStyleSheet());
        }
        if (stage == "cancel_ignored") {
          mission_tracker_.CancelTimedOut(request_id);
          SetInspectionRunning(true);
        } else if (stage == "rejected" || stage == "bad_request" ||
                   stage == "busy") {
          mission_tracker_.Finish(request_id);
          SetInspectionRunning(false);
          active_mission_point_count_ = 0;
          active_mission_inspection_enabled_ = false;
        }
        if (AppContract::IsMissionTerminalStage(stage)) {
          QTimer::singleShot(1200, this, [this, request_id]() {
            if (!mission_tracker_.Matches(request_id)) {
              return;
            }
            mission_tracker_.Finish(request_id);
            SetInspectionRunning(false);
            active_mission_point_count_ = 0;
            active_mission_inspection_enabled_ = false;
          });
        }
      } catch (const std::exception& error) {
        LOG_ERROR("Ignored invalid mission status payload: " << error.what());
      } }, Qt::QueuedConnection);
  });

  SUBSCRIBE_QOBJECT(
      this, MSG_ID_CHANNEL_PUBLISH_RESULT,
      [this](const basic::ChannelPublishResult& result) {
        if (!result.success &&
            result.message_id == MSG_ID_TOPOLOGY_MAP_UPDATE) {
          QMetaObject::invokeMethod(
              this,
              [this]() {
                statusBar()->showMessage(
                    tr("栅格地图已保存并继续上传；当前 ROS1 车端不支持拓扑关系同步。"),
                    8000);
              },
              Qt::QueuedConnection);
          return;
        }
        if (result.success || result.message_id != MSG_ID_MISSION_REQUEST) {
          return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, result]() {
              const QString request_id =
                  QString::fromStdString(result.request_id);
              if (!mission_tracker_.Matches(request_id)) {
                return;
              }
              mission_tracker_.Finish(request_id);
              active_mission_point_count_ = 0;
              active_mission_inspection_enabled_ = false;
              SetInspectionRunning(false);
              if (inspection_status_label_) {
                inspection_status_label_->setText(
                    QStringLiteral("任务请求发送失败，请检查连接后重试。"));
                inspection_status_label_->setStyleSheet(
                    UiStyle::StatusDangerStyleSheet());
              }
            },
            Qt::QueuedConnection);
      });

  SUBSCRIBE_QOBJECT(this, MSG_ID_MISSION_RESULT, [this](const std::string& json_str) {
    QMetaObject::invokeMethod(this, [this, json_str]() {
      nlohmann::json mission_result;
      try {
        mission_result = nlohmann::json::parse(json_str);
      } catch (const std::exception&) {
        return;
      }
      if (!IsCurrentMissionMessage(mission_result)) {
        return;
      }
      const QString request_id = QString::fromStdString(
          AppContract::JsonStringOr(mission_result, "request_id"));
      if (inspection_result_view_) {
        AppendInspectionLogLine(FormatInspectionResult(json_str));
      }
      bool inspection_ok = false;
      int completed_points = 0;
      if (inspection_kimi_banner_) {
        AiInspectionDisplay ai_display;
        try {
          const auto data = nlohmann::json::parse(json_str);
          inspection_ok = AppContract::JsonBoolOr(data, "ok", false);
          const auto pts = data.contains("points") ? data["points"]
                         : data.contains("results") ? data["results"]
                         : nlohmann::json::array();
          if (pts.is_array()) {
            int row = 0;
            for (const auto& point : pts) {
              const AiInspectionDisplay candidate = ExtractAiInspectionDisplay(point);
              if (candidate.valid) {
                ai_display = candidate;
              }
              const auto navigation = point.contains("navigation")
                                          ? point["navigation"]
                                          : nlohmann::json::object();
              const bool nav_ok = navigation.is_object() &&
                                  navigation.value("ok", false);
              const auto kimi = point.contains("kimi")
                                    ? point["kimi"]
                                    : nlohmann::json();
              const auto search = point.contains("search")
                                      ? point["search"]
                                      : nlohmann::json();
              const bool search_ok = search.is_object() &&
                                     search.value("ok", false);
              const bool point_ok =
                  nav_ok &&
                  (!active_mission_inspection_enabled_ ||
                   (search_ok &&
                    (!kimi.is_object() || kimi.value("ok", true))));
              nav_goal_table_view_->SetWaypointState(
                  row, point_ok ? QStringLiteral("已完成")
                                : QStringLiteral("需检查"),
                  point_ok ? 2 : 3);
              if (point_ok) {
                ++completed_points;
              }
              ++row;
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
                "QFrame { background:%1; border:2px solid %2; border-radius:12px; }"
                "QLabel { background:transparent; border:none; color:%3; }"
                "QPlainTextEdit { background:%4; color:%5; border:1px solid %6; "
                "border-radius:10px; padding:8px; font-family:'Microsoft YaHei UI'; }")
                .arg(UiStyle::Palette::SuccessBg, UiStyle::Palette::Success,
                     UiStyle::Palette::TextSecondary,
                     UiStyle::Palette::TerminalBg, UiStyle::Palette::TerminalText,
                     UiStyle::Palette::TerminalBorder);
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
      SetInspectionRunning(false);
      const int total = active_mission_point_count_;
      const bool all_points_ok = inspection_ok && completed_points == total;
      if (inspection_progress_bar_) {
        inspection_progress_bar_->setRange(0, (std::max)(1, total));
        inspection_progress_bar_->setValue(inspection_ok ? total : completed_points);
        inspection_progress_bar_->setFormat(
            all_points_ok
                ? (active_mission_inspection_enabled_
                       ? QStringLiteral("巡检完成 · %1 / %1").arg(total)
                       : QStringLiteral("导航完成 · %1 / %1").arg(total))
                : QStringLiteral("路线结束 · %1 / %2 项结果正常")
                      .arg(completed_points)
                      .arg(total));
      }
      if (inspection_progress_label_) {
        inspection_progress_label_->setText(
            all_points_ok ? QStringLiteral("全部正常")
                          : QStringLiteral("存在异常"));
        inspection_progress_label_->setStyleSheet(
            all_points_ok ? UiStyle::StatusSuccessStyleSheet()
                          : UiStyle::StatusWarningStyleSheet());
      }
      if (inspection_status_label_) {
        inspection_status_label_->setText(
            all_points_ok
                ? (active_mission_inspection_enabled_
                       ? QStringLiteral("巡检任务已完成，全部点位结果正常。")
                       : QStringLiteral("导航任务已完成，全部点位均已到达。"))
                : (active_mission_inspection_enabled_
                       ? QStringLiteral("巡检路线已结束，请重点检查标记为异常的点位。")
                       : QStringLiteral("导航路线已结束，请检查未正常到达的点位。")));
        inspection_status_label_->setStyleSheet(
            all_points_ok ? UiStyle::StatusSuccessStyleSheet()
                          : UiStyle::StatusWarningStyleSheet());
      }
      mission_tracker_.Finish(request_id);
      active_mission_point_count_ = 0;
      active_mission_inspection_enabled_ = false; }, Qt::QueuedConnection);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_DHT11_TEMP, [this](const double& temp) {
    if (label_dht11_temp_) {
      label_dht11_temp_->setText(QString::number(temp, 'f', 1) + " °C");
    }
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_DHT11_HUMI, [this](const double& humi) {
    if (label_dht11_humi_) {
      label_dht11_humi_->setText(QString::number(humi, 'f', 1) + " %");
    }
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_VOICE_COMMAND, [this](const std::string& json_str) {
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
  if (data && !data->empty() && image_frame_map_.count(location)) {
    if (location == "front" && command_center_widget_) {
      command_center_widget_->NotifyCameraFrameReceived();
    }
    // 帧节流：同源摄像头最小间隔 33ms（约30FPS），避免高频无效渲染
    constexpr qint64 kMinFrameIntervalMs = 33;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = last_frame_times_.find(location);
    if (it != last_frame_times_.end() && (now - it->second) < kMinFrameIntervalMs) {
      return;
    }
    last_frame_times_[location] = now;
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
  dock_manager_->setStyleSheet(UiStyle::DockStyleSheet());
  QVBoxLayout* center_layout = new QVBoxLayout();    // 垂直
  QHBoxLayout* center_h_layout = new QHBoxLayout();  // 水平

  QWidget* tools_strip = new QWidget();
  custom_title_bar_ = tools_strip;
  tools_strip->installEventFilter(this);
  tools_strip->setStyleSheet(UiStyle::ToolStripStyleSheet());

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
  view_menu_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/menu-2.svg"), QSize(32, 32)));
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

  reloc_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/map-pin.svg"), QSize(32, 32)));
  reloc_btn->setText("重定位");
  reloc_btn->setIconSize(QSize(24, 24));
  reloc_btn->setToolTip(tr("在地图上手动设置机器人位置和朝向"));
  connect(reloc_btn, &QToolButton::clicked, this, &MainWindow::StartManualRelocation);
  horizontalLayout_tools->addWidget(reloc_btn);

  QToolButton* edit_map_btn = new QToolButton();
  edit_map_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/pointer.svg"), QSize(32, 32)));
  edit_map_btn->setText("编辑地图");
  edit_map_btn->setIconSize(QSize(24, 24));
  edit_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  edit_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(edit_map_btn);

  open_map_btn_ = new QToolButton();
  open_map_btn_->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/folder-open.svg"), QSize(32, 32)));
  open_map_btn_->setText("打开地图");
  open_map_btn_->setIconSize(QSize(24, 24));
  open_map_btn_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  open_map_btn_->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(open_map_btn_);

  QToolButton* save_map_btn = new QToolButton();
  save_map_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/icons/tabler/device-floppy.svg"), QSize(32, 32)));
  save_map_btn->setText("保存地图");
  save_map_btn->setIconSize(QSize(24, 24));
  save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(save_map_btn);

  QToolButton* re_save_map_btn = new QToolButton();
  re_save_map_btn->setIcon(
      UiStyle::TintedIcon(QStringLiteral(":/images/re_save.svg"), QSize(32, 32)));
  re_save_map_btn->setText("另存为");
  re_save_map_btn->setIconSize(QSize(24, 24));
  re_save_map_btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  re_save_map_btn->setStyleSheet(modernToolButtonStyle);
  horizontalLayout_tools->addWidget(re_save_map_btn);
  center_layout->addWidget(tools_strip);

  horizontalLayout_tools->addItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

  top_connection_status_ = new QLabel(tr("离线"), tools_strip);
  top_connection_status_->setAlignment(Qt::AlignCenter);
  top_connection_status_->setAccessibleName(tr("机器人连接状态"));
  top_connection_status_->setStyleSheet(
      UiStyle::TopStatusLabelStyleSheet(UiStyle::Palette::Danger));
  horizontalLayout_tools->addWidget(CreateTopStatusPill(
      QStringLiteral(":/icons/tabler/plug-connected.svg"),
      top_connection_status_, tr("机器人连接状态"), 118, tools_strip));

  // Compact, consistent sensor status pills.
  battery_bar_ = new QProgressBar();
  battery_bar_->setObjectName(QString::fromUtf8("battery_bar_"));
  battery_bar_->setRange(0, 100);
  battery_bar_->setValue(0);
  battery_bar_->setFormat(QStringLiteral("%p%"));
  battery_bar_->setFixedSize(QSize(68, 22));
  battery_bar_->setStyleSheet(QStringLiteral(
                                  "QProgressBar#battery_bar_ { border:1px solid %1; background:%2; "
                                  "border-radius:7px; text-align:center; color:%3; "
                                  "font-size:%4px; font-weight:700; }"
                                  "QProgressBar#battery_bar_::chunk { background:%1; border-radius:6px; }")
                                  .arg(UiStyle::Palette::BorderHover)
                                  .arg(UiStyle::Palette::PrimaryLight)
                                  .arg(UiStyle::Palette::Primary)
                                  .arg(UiStyle::Palette::Text)
                                  .arg(QString::number(UiStyle::Font::Small))
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
  theme_button_ = new QToolButton(this);
  theme_button_->setText(UiStyle::IsDarkTheme() ? QStringLiteral("☀")
                                                : QStringLiteral("☾"));
  theme_button_->setToolTip(UiStyle::IsDarkTheme() ? tr("切换到明亮模式")
                                                   : tr("切换到暗色模式"));
  theme_button_->setAccessibleName(theme_button_->toolTip());
  theme_button_->setFixedSize(32, 28);
  theme_button_->setStyleSheet(UiStyle::MiniToolButtonStyleSheet());
  connect(theme_button_, &QToolButton::clicked, this, [this]() {
    const bool dark = !UiStyle::IsDarkTheme();
    QSettings settings(QStringLiteral("state.ini"), QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/darkTheme"), dark);
    settings.sync();
    setUpdatesEnabled(false);
    UiStyle::ApplyApplicationTheme(qApp, this, dark);
    if (dock_manager_) {
      dock_manager_->setStyleSheet(UiStyle::DockStyleSheet());
    }
    if (display_manager_) {
      auto* view_manager = dynamic_cast<Display::ViewManager*>(
          display_manager_->GetViewPtr());
      if (view_manager) {
        view_manager->setBackgroundBrush(QColor(UiStyle::Palette::Surface));
      }
    }
    theme_button_->setText(dark ? QStringLiteral("☀") : QStringLiteral("☾"));
    theme_button_->setToolTip(dark ? tr("切换到明亮模式")
                                   : tr("切换到暗色模式"));
    theme_button_->setAccessibleName(theme_button_->toolTip());
    setUpdatesEnabled(true);
    update();
  });
  horizontalLayout_tools->addWidget(theme_button_);

  horizontalLayout_tools->addSpacing(4);
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
  min_btn->setStyleSheet(UiStyle::WindowControlButtonStyleSheet());
  maximize_button_->setStyleSheet(UiStyle::WindowControlButtonStyleSheet());
  close_btn->setStyleSheet(UiStyle::CloseButtonStyleSheet());
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
  tools_edit_map_widget->setStyleSheet(QStringLiteral(
                                           "QWidget { background-color:%1; border:1px solid %2; border-radius:8px; }"
                                           " QLabel { color:%3; }")
                                           .arg(UiStyle::Palette::Surface,
                                                UiStyle::Palette::Border, UiStyle::Palette::TextSecondary));
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

  normal_cursor_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/cursor_point_btn.svg"), QSize(24, 24)));
  layout_tools_edit_map->addWidget(normal_cursor_btn);

  // 添加点位按钮
  QToolButton* add_point_btn = new QToolButton();
  add_point_btn->setCheckable(true);
  add_point_btn->setStyleSheet(modernEditButtonStyle);
  add_point_btn->setToolTip("添加工位点");
  add_point_btn->setCursor(Qt::PointingHandCursor);
  add_point_btn->setIconSize(QSize(24, 24));

  add_point_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/point_btn.svg"), QSize(24, 24)));
  layout_tools_edit_map->addWidget(add_point_btn);

  QToolButton* add_topology_path_btn = new QToolButton();
  add_topology_path_btn->setCheckable(true);
  add_topology_path_btn->setStyleSheet(modernEditButtonStyle);
  add_topology_path_btn->setToolTip("连接工位点");
  add_topology_path_btn->setCursor(Qt::PointingHandCursor);
  add_topology_path_btn->setIconSize(QSize(24, 24));

  add_topology_path_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/topo_link_btn.svg"), QSize(24, 24)));
  layout_tools_edit_map->addWidget(add_topology_path_btn);
  add_topology_path_btn->setEnabled(true);

  // 添加区域按钮
  QToolButton* add_region_btn = new QToolButton();
  add_region_btn->setCheckable(true);
  add_region_btn->setStyleSheet(modernEditButtonStyle);
  add_region_btn->setToolTip("添加区域");
  add_region_btn->setCursor(Qt::PointingHandCursor);
  add_region_btn->setIconSize(QSize(24, 24));

  add_region_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/region_btn.svg"), QSize(24, 24)));
  add_region_btn->setEnabled(false);
  layout_tools_edit_map->addWidget(add_region_btn);

  // 分隔
  QFrame* separator = new QFrame();
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  separator->setStyleSheet(UiStyle::SeparatorStyleSheet());
  layout_tools_edit_map->addWidget(separator);

  // 橡皮擦按钮
  QToolButton* erase_btn = new QToolButton();
  erase_btn->setCheckable(true);
  erase_btn->setStyleSheet(modernEditButtonStyle);
  erase_btn->setToolTip("橡皮擦");
  erase_btn->setCursor(Qt::PointingHandCursor);
  erase_btn->setIconSize(QSize(24, 24));

  erase_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/erase_btn.svg"), QSize(24, 24)));
  layout_tools_edit_map->addWidget(erase_btn);

  // 画笔按钮
  QToolButton* draw_pen_btn = new QToolButton();
  draw_pen_btn->setCheckable(true);
  draw_pen_btn->setStyleSheet(modernEditButtonStyle);
  draw_pen_btn->setToolTip("障碍物绘制");
  draw_pen_btn->setCursor(Qt::PointingHandCursor);
  draw_pen_btn->setIconSize(QSize(24, 24));

  draw_pen_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/pen.svg"), QSize(24, 24)));
  layout_tools_edit_map->addWidget(draw_pen_btn);

  // 线段按钮
  QToolButton* draw_line_btn = new QToolButton();
  draw_line_btn->setCheckable(true);
  draw_line_btn->setStyleSheet(modernEditButtonStyle);
  draw_line_btn->setToolTip("线段绘制");
  draw_line_btn->setCursor(Qt::PointingHandCursor);
  draw_line_btn->setIconSize(QSize(24, 24));

  draw_line_btn->setIcon(UiStyle::TintedIcon(
      QStringLiteral(":/images/line_btn.svg"), QSize(24, 24)));
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
  center_widget->setStyleSheet(QStringLiteral(
                                   "QWidget { background-color:%1; }")
                                   .arg(UiStyle::Palette::Surface));
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
  connect(display_config_widget_, &DisplayConfigWidget::ConnectionStateChanged,
          this, [this](bool connected, bool connecting, const QString& message) {
            channel_connected_ = connected;
            if (terminal_widget_) {
              terminal_widget_->SetConnected(connected);
            }
            if (!top_connection_status_) {
              return;
            }
            const QString color = connecting ? UiStyle::Palette::Warning
                                             : (connected ? UiStyle::Palette::Success
                                                          : UiStyle::Palette::Danger);
            top_connection_status_->setText(connecting ? tr("连接中")
                                                       : (connected ? tr("已连接")
                                                                    : tr("离线")));
            top_connection_status_->setToolTip(message);
            top_connection_status_->setStyleSheet(
                UiStyle::TopStatusLabelStyleSheet(color));
          });
  settings_dock_ = new ads::CDockWidget(tr("设置"));
  settings_dock_->setWidget(display_config_widget_, ads::CDockWidget::ForceNoScrollArea);
  ConfigureDockWidget(settings_dock_, QSize(320, 420), QSize(350, 620));
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
  connect(speed_ctrl_widget_, &SpeedCtrlWidget::signalEmergencyStopChanged,
          [this](bool engaged) { PUBLISH(MSG_ID_EMERGENCY_STOP, engaged); });
  speed_ctrl_dock_ = new ads::CDockWidget("速度控制");
  speed_ctrl_dock_->setWidget(speed_ctrl_widget_);
  ConfigureDockWidget(speed_ctrl_dock_, QSize(320, 360), QSize(350, 420));
  auto speed_ctrl_area =
      dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                                   speed_ctrl_dock_, settings_dock_area_);
  ui->menuView->addAction(speed_ctrl_dock_->toggleViewAction());

  /////////////////////////////////////////////////////////导航任务列表
  QWidget* task_list_widget = new QWidget();
  nav_goal_table_view_ = new NavGoalTableView();
  QVBoxLayout* horizontalLayout_13 = new QVBoxLayout();
  horizontalLayout_13->setContentsMargins(12, 12, 12, 12);
  horizontalLayout_13->setSpacing(10);

  auto* inspection_header_card = new QFrame();
  inspection_header_card->setProperty("uiCard", true);
  inspection_header_card->setStyleSheet(UiStyle::CardStyleSheet());
  auto* inspection_header_layout = new QVBoxLayout(inspection_header_card);
  inspection_header_layout->setContentsMargins(16, 14, 16, 12);
  inspection_header_layout->setSpacing(6);
  auto* inspection_title = new QLabel(QStringLiteral("导航任务"));
  inspection_title->setObjectName(QStringLiteral("pageTitle"));
  inspection_title->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  auto* inspection_subtitle = new QLabel(
      QStringLiteral("按顺序前往地图点位，可选 AI 巡检、循环执行和任务结束返航。"));
  inspection_subtitle->setStyleSheet(UiStyle::HintLabelStyleSheet());
  inspection_subtitle->setWordWrap(true);
  inspection_header_layout->addWidget(inspection_title);
  inspection_header_layout->addWidget(inspection_subtitle);
  auto* inspection_header_row = new QHBoxLayout();
  inspection_header_row->setContentsMargins(0, 4, 0, 0);
  inspection_route_summary_label_ = new QLabel(QStringLiteral("路线 0 / 0"));
  inspection_route_summary_label_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  inspection_header_row->addWidget(inspection_route_summary_label_);
  inspection_header_row->addStretch();
  inspection_readiness_label_ = new QLabel(QStringLiteral("添加导航点"));
  inspection_readiness_label_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
  inspection_header_row->addWidget(inspection_readiness_label_);
  inspection_header_layout->addLayout(inspection_header_row);
  horizontalLayout_13->addWidget(inspection_header_card);

  auto* route_card = new QFrame();
  route_card->setProperty("uiCard", true);
  route_card->setStyleSheet(UiStyle::CardStyleSheet());
  auto* route_layout = new QVBoxLayout(route_card);
  route_layout->setContentsMargins(12, 10, 12, 10);
  route_layout->setSpacing(6);
  auto* route_header = new QHBoxLayout();
  auto* route_title = new QLabel(QStringLiteral("任务点位"));
  route_title->setStyleSheet(UiStyle::SectionLabelStyleSheet());
  auto* route_hint = new QLabel(QStringLiteral("点位顺序与执行状态会在这里显示。"));
  route_hint->setStyleSheet(UiStyle::HintLabelStyleSheet());
  route_hint->setWordWrap(true);
  route_header->addWidget(route_title);
  route_header->addSpacing(10);
  route_header->addWidget(route_hint, 1);
  route_layout->addLayout(route_header);
  route_layout->addWidget(nav_goal_table_view_, 1);
  horizontalLayout_13->addWidget(route_card, 1);
  task_list_widget->setLayout(horizontalLayout_13);
  inspection_task_dock_ = new ads::CDockWidget("导航任务");

  // 现代化按钮样式
  QString modernButtonStyle = UiStyle::MainButtonStyleSheet();

  inspection_add_button_ = new QPushButton("添加导航点");
  inspection_add_button_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  inspection_add_button_->setToolTip(QStringLiteral("新增一行，并从已有地图点位中选择巡检位置"));

  QHBoxLayout* horizontalLayout_15 = new QHBoxLayout();
  horizontalLayout_15->setContentsMargins(0, 0, 0, 0);
  QPushButton* btn_start_task_chain = new QPushButton("开始任务");
  btn_start_task_chain->setStyleSheet(modernButtonStyle);
  inspection_start_button_ = btn_start_task_chain;

  QCheckBox* loop_task_checkbox = new QCheckBox("循环任务");
  inspection_loop_checkbox_ = loop_task_checkbox;
  loop_task_checkbox->setStyleSheet(UiStyle::CheckBoxStyleSheet());

  inspection_ai_checkbox_ = new QCheckBox("到点后 AI 巡检");
  inspection_ai_checkbox_->setStyleSheet(UiStyle::CheckBoxStyleSheet());
  inspection_ai_checkbox_->setToolTip(
      QStringLiteral("开启后，每个点到达后执行视觉搜索与 AI 分析；需要先进入巡检模式"));
  inspection_ai_checkbox_->setChecked(false);
  inspection_ai_checkbox_->setEnabled(false);

  inspection_return_home_checkbox_ = new QCheckBox("任务结束返航");
  inspection_return_home_checkbox_->setStyleSheet(UiStyle::CheckBoxStyleSheet());
  inspection_return_home_checkbox_->setToolTip(
      QStringLiteral("开启后，任务链全部点位完成后返回任务启动位置"));
  inspection_return_home_checkbox_->setChecked(true);

  horizontalLayout_15->setSpacing(10);
  horizontalLayout_15->addWidget(inspection_add_button_);
  horizontalLayout_15->addWidget(btn_start_task_chain, 1);

  auto* options_card = new QFrame();
  options_card->setProperty("uiCard", true);
  options_card->setStyleSheet(UiStyle::CardStyleSheet());
  auto* options_layout = new QVBoxLayout(options_card);
  options_layout->setContentsMargins(14, 10, 14, 10);
  options_layout->setSpacing(6);
  auto* options_header = new QHBoxLayout();
  auto* options_title = new QLabel(QStringLiteral("任务配置"));
  options_title->setStyleSheet(UiStyle::SectionLabelStyleSheet());
  auto* options_hint = new QLabel(QStringLiteral("只影响本次任务，可随时保存为方案"));
  options_hint->setStyleSheet(UiStyle::HintLabelStyleSheet());
  options_header->addWidget(options_title);
  options_header->addStretch();
  options_header->addWidget(options_hint);
  options_layout->addLayout(options_header);

  QHBoxLayout* loop_task_layout = new QHBoxLayout();
  loop_task_layout->setContentsMargins(0, 0, 0, 0);
  loop_task_layout->setSpacing(18);
  loop_task_layout->addWidget(inspection_ai_checkbox_);
  loop_task_layout->addWidget(loop_task_checkbox);
  loop_task_layout->addWidget(inspection_return_home_checkbox_);
  loop_task_layout->addStretch();
  options_layout->addLayout(loop_task_layout);

  inspection_load_button_ = new QPushButton("加载方案");
  inspection_save_button_ = new QPushButton("保存方案");
  inspection_load_button_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  inspection_save_button_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());

  QHBoxLayout* horizontalLayout_16 = new QHBoxLayout();
  horizontalLayout_16->setContentsMargins(0, 0, 0, 0);
  horizontalLayout_16->setSpacing(8);
  horizontalLayout_16->addWidget(inspection_load_button_);
  horizontalLayout_16->addWidget(inspection_save_button_);
  options_layout->addLayout(horizontalLayout_16);

  auto* inspection_status_card = new QFrame();
  inspection_status_card_ = inspection_status_card;
  inspection_status_card->setProperty("uiCard", true);
  inspection_status_card->setStyleSheet(QStringLiteral(
                                            "QFrame { background:%1; border:1px solid %2; border-radius:12px; }"
                                            "QLabel { background:transparent; border:none; color:%3; }"
                                            "QPlainTextEdit { background:%4; color:%5; border:1px solid %6; "
                                            "border-radius:10px; padding:8px; font-family:'Microsoft YaHei UI'; }")
                                            .arg(UiStyle::Palette::ToolbarBg, UiStyle::Palette::Border,
                                                 UiStyle::Palette::TextSecondary,
                                                 UiStyle::Palette::TerminalBg, UiStyle::Palette::TerminalText,
                                                 UiStyle::Palette::TerminalBorder));
  auto* inspection_status_layout = new QVBoxLayout(inspection_status_card);
  inspection_status_layout->setContentsMargins(14, 12, 14, 14);
  inspection_status_layout->setSpacing(8);
  auto* inspection_status_header = new QHBoxLayout();
  auto* inspection_feedback_title = new QLabel(QStringLiteral("实时反馈"));
  inspection_feedback_title->setStyleSheet(UiStyle::TitleLabelStyleSheet());
  inspection_progress_label_ = new QLabel(QStringLiteral("待命"));
  inspection_progress_label_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  inspection_status_header->addWidget(inspection_feedback_title);
  inspection_status_header->addStretch();
  inspection_status_header->addWidget(inspection_progress_label_);
  inspection_progress_bar_ = new QProgressBar();
  inspection_progress_bar_->setRange(0, 1);
  inspection_progress_bar_->setValue(0);
  inspection_progress_bar_->setTextVisible(true);
  inspection_progress_bar_->setFormat(QStringLiteral("尚未开始"));
  inspection_progress_bar_->setMinimumHeight(24);
  inspection_status_label_ = new QLabel(QStringLiteral("等待任务"));
  inspection_status_label_->setWordWrap(true);
  inspection_status_label_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  inspection_result_view_ = new QPlainTextEdit();
  inspection_result_view_->setReadOnly(true);
  inspection_result_view_->setPlaceholderText(QStringLiteral("暂无记录"));
  inspection_result_view_->setMinimumHeight(110);
  inspection_result_view_->setMaximumHeight(160);
  inspection_status_layout->addLayout(inspection_status_header);
  inspection_status_layout->addWidget(inspection_progress_bar_);
  inspection_status_layout->addWidget(inspection_status_label_);
  inspection_status_layout->addWidget(inspection_result_view_);
  inspection_kimi_banner_ = new QLabel();
  inspection_kimi_banner_->setWordWrap(true);
  inspection_kimi_banner_->setStyleSheet(QStringLiteral(
                                             "QLabel { background:%1; color:%2; border:1px solid %3; "
                                             "border-radius:8px; padding:10px 14px; font-size:%4px; font-weight:700; }")
                                             .arg(UiStyle::Palette::SuccessBg, UiStyle::Palette::Success,
                                                  UiStyle::Palette::SuccessBorder)
                                             .arg(UiStyle::FontBasePx()));
  inspection_kimi_banner_->setVisible(false);
  inspection_status_layout->addWidget(inspection_kimi_banner_);

  horizontalLayout_13->addLayout(horizontalLayout_15);
  horizontalLayout_13->addWidget(options_card);
  horizontalLayout_13->addWidget(inspection_status_card);
  inspection_task_dock_->setWidget(task_list_widget);
  ConfigureDockWidget(inspection_task_dock_, QSize(660, 520), QSize(720, 780));
  inspection_task_dock_->setMaximumSize(840, 9999);
  inspection_dock_area_ = dock_manager_->addDockWidget(
      ads::DockWidgetArea::RightDockWidgetArea, inspection_task_dock_,
      center_docker_area_);
  ConfigureFloatingOnOpen(inspection_task_dock_, QSize(820, 820));
  inspection_task_dock_->toggleView(false);
  connect(nav_goal_table_view_, &NavGoalTableView::signalMissionRequest,
          this, &MainWindow::StartMissionRequest);
  connect(inspection_ai_checkbox_, &QCheckBox::toggled, this,
          [this](bool enabled) {
            nav_goal_table_view_->SetInspectionEnabled(enabled);
            UpdateInspectionRouteSummary();
          });
  nav_goal_table_view_->SetInspectionEnabled(false);
  connect(inspection_load_button_, &QPushButton::clicked, [this]() {
    QString fileName = QFileDialog::getOpenFileName(nullptr, "打开JSON文件",
                                                    "", "JSON文件 (*.json)",
                                                    nullptr, QFileDialog::DontUseNativeDialog);

    // 如果用户选择了文件，则输出文件名
    if (!fileName.isEmpty()) {
      qDebug() << "Selected file:" << fileName;
      if (!nav_goal_table_view_->LoadTaskChain(fileName.toStdString())) {
        QMessageBox::warning(this, tr("加载失败"),
                             tr("无法读取或解析巡检方案：\n%1").arg(fileName));
      }
    }
  });
  connect(inspection_save_button_, &QPushButton::clicked, [this]() {
    QString fileName = QFileDialog::getSaveFileName(nullptr, "保存JSON文件",
                                                    "", "JSON文件 (*.json)",
                                                    nullptr, QFileDialog::DontUseNativeDialog);

    // 如果用户选择了文件，则输出文件名
    if (!fileName.isEmpty()) {
      qDebug() << "Selected file:" << fileName;
      if (!fileName.endsWith(".json")) {
        fileName += ".json";
      }
      if (nav_goal_table_view_->SaveTaskChain(fileName.toStdString())) {
        QMessageBox::information(this, tr("保存成功"),
                                 tr("任务链文件已成功保存到:\n%1").arg(fileName),
                                 QMessageBox::Ok);
      } else {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("无法写入巡检方案：\n%1").arg(fileName));
      }
    }
  });

  ui->menuView->addAction(inspection_task_dock_->toggleViewAction());
  connect(
      inspection_add_button_, &QPushButton::clicked,
      [this]() { nav_goal_table_view_->AddItem(); });
  connect(nav_goal_table_view_, &NavGoalTableView::signalRouteChanged,
          this, [this](int) { UpdateInspectionRouteSummary(); });
  connect(btn_start_task_chain, &QPushButton::clicked,
          [this, btn_start_task_chain, loop_task_checkbox]() {
            if (btn_start_task_chain->text() == QStringLiteral("开始任务")) {
              if (nav_goal_table_view_->RowCount() == 0 ||
                  nav_goal_table_view_->ValidPointCount() !=
                      nav_goal_table_view_->RowCount()) {
                QMessageBox::information(this, QStringLiteral("导航路线未就绪"),
                                         QStringLiteral("请为每一行选择有效的地图点位。"),
                                         QMessageBox::Ok);
                return;
              }
              const auto request =
                  nav_goal_table_view_->BuildMissionRequest(
                      loop_task_checkbox->isChecked(),
                      inspection_return_home_checkbox_->isChecked());
              StartMissionRequest(request);
            } else {
              const QString request_id = mission_tracker_.requestId();
              if (!mission_tracker_.BeginCancellation(request_id)) {
                return;
              }
              btn_start_task_chain->setText(QStringLiteral("正在停止…"));
              btn_start_task_chain->setEnabled(false);
              const nlohmann::json cancel_request = {
                  {"schema_version", 1},
                  {"request_id", request_id.toStdString()},
                  {"command", "cancel"},
                  {"mission_type", "navigation"},
              };
              PUBLISH(MSG_ID_MISSION_REQUEST, cancel_request.dump());
              if (inspection_status_label_) {
                inspection_status_label_->setText(QStringLiteral("正在安全停止导航任务…"));
                inspection_status_label_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
              }
              QTimer::singleShot(10000, this, [this, request_id]() {
                if (!mission_tracker_.CancelTimedOut(request_id)) {
                  return;
                }
                SetInspectionRunning(true);
                if (inspection_status_label_) {
                  inspection_status_label_->setText(
                      QStringLiteral("停止请求超时，任务状态仍未确认；可重试停止。"));
                  inspection_status_label_->setStyleSheet(
                      UiStyle::StatusWarningStyleSheet());
                }
              });
            }
          });
  UpdateInspectionRouteSummary();
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
  ConfigureDockWidget(command_center_dock_, QSize(350, 560), QSize(390, 720));
  command_center_dock_area_ =
      dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                                   command_center_dock_, center_docker_area_);
  command_center_dock_->toggleView(true);
  ui->menuView->addAction(command_center_dock_->toggleViewAction());
  connect(command_center_widget_, &CommandCenterWidget::CameraViewRequested, this,
          [this](bool visible) {
            const auto it = image_dock_map_.find("front");
            if (it == image_dock_map_.end()) return;
            it->second->toggleView(visible);
            PUBLISH(MSG_ID_IMAGE_STREAM_VISIBILITY,
                    std::make_pair(std::string("front"), visible));
            if (visible) it->second->raise();
          });
  connect(command_center_widget_, &CommandCenterWidget::WorkspaceModeRequested,
          this, &MainWindow::ApplyWorkspaceMode);
  connect(command_center_widget_,
          &CommandCenterWidget::InspectionCapabilityChanged, this,
          [this](bool ready) {
            inspection_capability_ready_ = ready;
            if (!inspection_ai_checkbox_) {
              return;
            }
            inspection_ai_checkbox_->setEnabled(!inspection_running_ && ready);
            inspection_ai_checkbox_->setToolTip(
                ready ? tr("开启后，每个点位导航完成都会执行视觉搜索和 AI 分析。")
                      : tr("当前巡检能力尚未就绪，请切换巡检模式并等待状态就绪。"));
            if (!ready) {
              inspection_ai_checkbox_->setChecked(false);
            }
          });

  //////////////////////////////////////////////////////小车终端
  terminal_widget_ = new TerminalWidget();
  terminal_widget_->SetConnected(channel_connected_);
  terminal_dock_ = new ads::CDockWidget("小车终端");
  terminal_dock_->setWidget(terminal_widget_);
  ConfigureDockWidget(terminal_dock_, QSize(760, 300), QSize(1180, 420));
  dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                               terminal_dock_, center_docker_area_);
  terminal_dock_->toggleView(false);
  ConfigureFloatingOnOpen(terminal_dock_, QSize(960, 620));
  ui->menuView->addAction(terminal_dock_->toggleViewAction());
  connect(terminal_widget_, &TerminalWidget::CommandRequested, this,
          [](const QString& request) {
            PUBLISH(MSG_ID_SHELL_REQUEST, request.toStdString());
          });
  connect(terminal_widget_, &TerminalWidget::TerminateRequested, this, []() {
    PUBLISH(MSG_ID_SHELL_CANCEL, std::string("{}"));
  });

  //////////////////////////////////////////////////////图片
  for (const auto& one_image : Config::ConfigManager::Instance()->GetRootConfigSnapshot().images) {
    LOG_INFO("init image window location:" << one_image.location << " topic:" << one_image.topic);
    image_frame_map_[one_image.location] = new RatioLayoutedFrame();
    ads::CDockWidget* dock_widget = new ads::CDockWidget(std::string("image/" + one_image.location).c_str());
    dock_widget->setWidget(image_frame_map_[one_image.location]);
    ConfigureDockWidget(dock_widget, QSize(420, 320), QSize(520, 390));
    if (one_image.location == "front" && inspection_dock_area_) {
      dock_manager_->addDockWidget(ads::DockWidgetArea::BottomDockWidgetArea,
                                   dock_widget, inspection_dock_area_);
    } else {
      dock_manager_->addDockWidget(ads::DockWidgetArea::RightDockWidgetArea,
                                   dock_widget, center_docker_area_);
    }
    dock_widget->toggleView(false);
    image_dock_map_[one_image.location] = dock_widget;
    const std::string image_location = one_image.location;
    connect(dock_widget->toggleViewAction(), &QAction::toggled, this,
            [image_location](bool visible) {
              PUBLISH(MSG_ID_IMAGE_STREAM_VISIBILITY,
                      std::make_pair(image_location, visible));
            });
    ConfigureFloatingOnOpen(dock_widget, QSize(760, 560));
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
            PublishNavGoalSafely(pose);
          });
  // ui相关
  connect(re_save_map_btn, &QToolButton::clicked,
          this, &MainWindow::SaveMapToLocalAndRobot);
  connect(save_map_btn, &QToolButton::clicked,
          this, &MainWindow::SaveMapToLocalAndRobot);

  connect(open_map_btn_, &QToolButton::clicked, [this]() {
    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("打开地图"), MapLibraryDirectory(), tr("ROS 地图 (*.yaml)"));
    if (!fileName.isEmpty()) {
      LOG_INFO("用户选择的打开地图路径：" << fileName.toStdString());
      UploadLocalMap(fileName, true);
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
      (std::min)(available.width(), (std::max)(960, qRound(available.width() * 0.88))),
      (std::min)(available.height(), (std::max)(640, qRound(available.height() * 0.90))));
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
    const int clamped_target = (std::min)(target_width, (std::max)(320, total / 2));
    const int remaining = (std::max)(1, total - clamped_target);
    const int old_other_total = (std::max)(1, total - sizes.at(branch_index));
    int assigned = 0;
    for (int i = 0; i < sizes.size(); ++i) {
      if (i == branch_index) {
        sizes[i] = clamped_target;
        continue;
      }
      sizes[i] = (std::max)(1, remaining * sizes.at(i) / old_other_total);
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

  resize_area(settings_dock_area_, 350);
  resize_area(command_center_dock_area_, 390);
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
  const QSize target((std::min)(preferred_size.width(), qRound(host.width() * 0.75)),
                     (std::min)(preferred_size.height(), qRound(host.height() * 0.80)));
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
      (std::max)(1, static_cast<int>(std::ceil(0.15 / resolution)));
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

void MainWindow::PublishImageStreamVisibility() const {
  for (const auto& [location, dock] : image_dock_map_) {
    const bool visible = dock && dock->toggleViewAction() &&
                         dock->toggleViewAction()->isChecked();
    PUBLISH(MSG_ID_IMAGE_STREAM_VISIBILITY,
            std::make_pair(location, visible));
  }
}

void MainWindow::StartManualRelocation() {
  auto map = display_manager_->GetOccupancyMap();
  if (map.Rows() <= 0 || map.Cols() <= 0) {
    QMessageBox::warning(this, tr("无法重定位"),
                         tr("当前尚未收到有效地图，请先加载静态地图并启动 AMCL。"));
    return;
  }
  localization_confirmed_ = false;
  relocation_pending_ = false;
  ++relocation_attempt_id_;
  UpdateInspectionRouteSummary();
  statusBar()->showMessage(tr("手动重定位：请在地图上选择位置和朝向。"), 6000);
  display_manager_->StartReloc();
}

void MainWindow::PublishNavGoalSafely(const RobotPose& pose) {
  if (inspection_running_) {
    QMessageBox::information(
        this, tr("任务正在运行"),
        tr("请先停止当前导航任务，再发送兼容的 /goal_pose 单点目标。"));
    return;
  }
  const QString request_id =
      QStringLiteral("qt-goal-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  StartMissionRequest(
      AppContract::BuildSingleGoalMission(pose, request_id).dump());
}

void MainWindow::StartMissionRequest(const std::string& request) {
  if (inspection_running_ || mission_tracker_.active()) {
    return;
  }

  nlohmann::json mission;
  try {
    mission = nlohmann::json::parse(request);
  } catch (const std::exception&) {
    QMessageBox::warning(this, tr("任务请求无效"),
                         tr("无法生成有效的导航任务，请检查路线配置。"));
    return;
  }

  const auto route = mission.value("route", nlohmann::json::array());
  const std::string request_id = mission.value("request_id", std::string());
  const auto inspection =
      mission.contains("inspection") && mission["inspection"].is_object()
          ? mission["inspection"]
          : nlohmann::json::object();
  const bool inspection_enabled = inspection.value("enabled", false);
  if (!route.is_array() || route.empty() || request_id.empty() ||
      mission.value("mission_type", std::string()) != "navigation") {
    QMessageBox::warning(this, tr("任务请求无效"),
                         tr("导航任务缺少路线、任务类型或请求编号。"));
    return;
  }
  if (!localization_confirmed_) {
    QMessageBox::warning(
        this, tr("定位尚未确认"),
        tr("导航任务会驱动小车移动。请先完成手动重定位并等待 AMCL 定位确认。"));
    return;
  }
  if (inspection_enabled && !inspection_capability_ready_) {
    QMessageBox::warning(
        this, tr("AI 巡检服务未启用"),
        tr("当前运行模式未启动巡检执行器、视觉识别和 AI 服务。请先在命令中心切换到“巡检模式”，再启动启用了 AI 巡检的任务。"));
    return;
  }

  const QString correlated_request_id = QString::fromStdString(request_id);
  if (!mission_tracker_.Begin(correlated_request_id)) {
    return;
  }
  active_mission_inspection_enabled_ = inspection_enabled;
  active_mission_point_count_ = static_cast<int>(route.size());
  SetInspectionRunning(true);
  nav_goal_table_view_->ResetExecutionState();
  if (inspection_progress_bar_) {
    inspection_progress_bar_->setRange(0, active_mission_point_count_);
    inspection_progress_bar_->setValue(0);
    inspection_progress_bar_->setFormat(
        QStringLiteral("已完成 0 / %1").arg(active_mission_point_count_));
  }
  if (inspection_progress_label_) {
    inspection_progress_label_->setText(
        QStringLiteral("0 / %1").arg(active_mission_point_count_));
  }
  const QString pending_text = inspection_enabled
                                   ? QStringLiteral("任务已发送，等待小车开始导航与 AI 巡检…")
                                   : QStringLiteral("任务已发送，等待小车开始导航…");
  if (inspection_status_label_) {
    inspection_status_label_->setText(pending_text);
    inspection_status_label_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
  }
  last_inspection_log_line_.clear();
  if (inspection_result_view_) {
    inspection_result_view_->clear();
    AppendInspectionLogLine(pending_text);
  }
  PUBLISH(MSG_ID_MISSION_REQUEST, request);
  QTimer::singleShot(8000, this, [this, correlated_request_id]() {
    if (!mission_tracker_.AcceptanceTimedOut(correlated_request_id)) {
      return;
    }
    active_mission_point_count_ = 0;
    active_mission_inspection_enabled_ = false;
    SetInspectionRunning(false);
    if (inspection_status_label_) {
      inspection_status_label_->setText(
          QStringLiteral("任务启动超时：小车端未确认接受，请检查连接后重试。"));
      inspection_status_label_->setStyleSheet(
          UiStyle::StatusDangerStyleSheet());
    }
  });
}

bool MainWindow::IsCurrentMissionMessage(const nlohmann::json& data) const {
  if (!inspection_running_ || !mission_tracker_.active()) {
    return false;
  }
  const std::string request_id =
      AppContract::JsonStringOr(data, "request_id");
  return !request_id.empty() &&
         mission_tracker_.Matches(QString::fromStdString(request_id));
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

void MainWindow::ApplyWorkspaceMode(const QString& mode) {
  const bool inspection_mode = mode.trimmed() == QStringLiteral("inspection");
  auto is_visible = [](ads::CDockWidget* dock) {
    return dock && dock->toggleViewAction() &&
           dock->toggleViewAction()->isChecked();
  };

  const auto front_it = image_dock_map_.find("front");
  ads::CDockWidget* front_camera =
      front_it == image_dock_map_.end() ? nullptr : front_it->second;

  if (inspection_mode) {
    if (!inspection_workspace_active_) {
      previous_settings_visible_ = is_visible(settings_dock_);
      previous_speed_visible_ = is_visible(speed_ctrl_dock_);
      previous_command_center_visible_ = is_visible(command_center_dock_);
      previous_terminal_visible_ = is_visible(terminal_dock_);
      previous_front_camera_visible_ = is_visible(front_camera);
    }
    inspection_workspace_active_ = true;
    if (settings_dock_) settings_dock_->toggleView(false);
    if (speed_ctrl_dock_) speed_ctrl_dock_->toggleView(false);
    if (command_center_dock_) command_center_dock_->toggleView(false);
    if (terminal_dock_) terminal_dock_->toggleView(false);
    if (inspection_task_dock_) {
      inspection_task_dock_->toggleView(true);
      inspection_task_dock_->raise();
    }
    if (front_camera) {
      front_camera->toggleView(true);
      front_camera->raise();
    }
    statusBar()->showMessage(
        tr("已进入巡检工作台：地图、任务链与实时画面已就位。"), 6000);
    return;
  }

  if (!inspection_workspace_active_) {
    return;
  }
  inspection_workspace_active_ = false;
  if (inspection_task_dock_) inspection_task_dock_->toggleView(false);
  if (settings_dock_) settings_dock_->toggleView(previous_settings_visible_);
  if (speed_ctrl_dock_) speed_ctrl_dock_->toggleView(previous_speed_visible_);
  if (command_center_dock_) {
    command_center_dock_->toggleView(previous_command_center_visible_);
  }
  if (terminal_dock_) terminal_dock_->toggleView(previous_terminal_visible_);
  if (front_camera) front_camera->toggleView(previous_front_camera_visible_);
}

void MainWindow::UpdateInspectionRouteSummary() {
  if (!nav_goal_table_view_) {
    return;
  }
  const int point_count = nav_goal_table_view_->RowCount();
  const int valid_point_count = nav_goal_table_view_->ValidPointCount();
  if (inspection_route_summary_label_) {
    inspection_route_summary_label_->setText(
        point_count == 0
            ? QStringLiteral("路线 0 / 0")
            : QStringLiteral("路线 %1 / %2")
                  .arg(valid_point_count)
                  .arg(point_count));
    inspection_route_summary_label_->setStyleSheet(
        point_count > 0 && valid_point_count == point_count
            ? UiStyle::StatusSuccessStyleSheet()
            : UiStyle::StatusInfoStyleSheet());
  }
  if (inspection_readiness_label_) {
    if (inspection_running_) {
      inspection_readiness_label_->setText(QStringLiteral("执行中"));
      inspection_readiness_label_->setStyleSheet(UiStyle::StatusInfoStyleSheet());
    } else if (point_count == 0) {
      inspection_readiness_label_->setText(QStringLiteral("添加导航点"));
      inspection_readiness_label_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
    } else if (valid_point_count != point_count) {
      inspection_readiness_label_->setText(QStringLiteral("补全点位"));
      inspection_readiness_label_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
    } else if (!localization_confirmed_) {
      inspection_readiness_label_->setText(QStringLiteral("等待定位"));
      inspection_readiness_label_->setStyleSheet(UiStyle::StatusWarningStyleSheet());
    } else {
      inspection_readiness_label_->setText(QStringLiteral("已就绪"));
      inspection_readiness_label_->setStyleSheet(UiStyle::StatusSuccessStyleSheet());
    }
  }
  if (inspection_start_button_ && !inspection_running_) {
    inspection_start_button_->setEnabled(
        localization_confirmed_ && point_count > 0 &&
        valid_point_count == point_count);
  }
}

void MainWindow::SetInspectionRunning(bool running) {
  inspection_running_ = running;
  if (nav_goal_table_view_) {
    nav_goal_table_view_->SetRouteRunning(running);
  }
  if (inspection_add_button_) inspection_add_button_->setEnabled(!running);
  if (inspection_load_button_) inspection_load_button_->setEnabled(!running);
  if (inspection_save_button_) inspection_save_button_->setEnabled(!running);
  if (inspection_ai_checkbox_) {
    inspection_ai_checkbox_->setEnabled(!running &&
                                        inspection_capability_ready_);
  }
  if (inspection_loop_checkbox_) inspection_loop_checkbox_->setEnabled(!running);
  if (inspection_return_home_checkbox_) {
    inspection_return_home_checkbox_->setEnabled(!running);
  }
  if (inspection_start_button_) {
    inspection_start_button_->setText(
        running ? QStringLiteral("停止任务") : QStringLiteral("开始任务"));
    inspection_start_button_->setStyleSheet(
        running ? UiStyle::DangerButtonStyleSheet()
                : UiStyle::MainButtonStyleSheet());
    inspection_start_button_->setEnabled(
        running || (nav_goal_table_view_ &&
                    localization_confirmed_ &&
                    nav_goal_table_view_->RowCount() > 0 &&
                    nav_goal_table_view_->ValidPointCount() ==
                        nav_goal_table_view_->RowCount()));
  }
  UpdateInspectionRouteSummary();
}

void MainWindow::UpdateInspectionProgress(const nlohmann::json& data) {
  if (!nav_goal_table_view_) {
    return;
  }
  std::string stage_value = AppContract::JsonStringOr(data, "stage");
  if (stage_value.empty()) {
    stage_value = AppContract::JsonStringOr(data, "state");
  }
  const QString stage = QString::fromStdString(stage_value);
  const auto extra = data.contains("extra") && data["extra"].is_object()
                         ? data["extra"]
                         : nlohmann::json::object();
  const int row = AppContract::JsonIntOr(
      data, "point_index", AppContract::JsonIntOr(extra, "index", -1));
  const int total = active_mission_point_count_;
  const QString stage_text = InspectionStageText(stage_value);

  if (row >= 0 && row < total) {
    for (int completed = 0; completed < row; ++completed) {
      nav_goal_table_view_->SetWaypointState(
          completed, QStringLiteral("已完成"), 2);
    }
    int level = 1;
    QString row_text = stage_text;
    if (stage == QStringLiteral("kimi_complete")) {
      level = 2;
      row_text = QStringLiteral("识别完成");
    } else if (stage == QStringLiteral("target_skipped")) {
      level = 3;
      row_text = QStringLiteral("未找到目标");
    } else if (stage == QStringLiteral("error") ||
               stage == QStringLiteral("rotation_sensor_stop") ||
               stage == QStringLiteral("tf_unavailable") ||
               stage == QStringLiteral("invalid_detection")) {
      level = 4;
      row_text = QStringLiteral("执行异常");
    } else if (stage == QStringLiteral("target_lost") ||
               stage == QStringLiteral("align_timeout")) {
      level = 3;
      row_text = QStringLiteral("重新搜索");
    }
    nav_goal_table_view_->SetWaypointState(row, row_text, level);
  }

  int completed_count = row >= 0 ? row : 0;
  if (stage == QStringLiteral("kimi_complete") ||
      stage == QStringLiteral("target_skipped")) {
    completed_count = row + 1;
  } else if (stage == QStringLiteral("returning_home") ||
             stage == QStringLiteral("complete") ||
             stage == QStringLiteral("completed")) {
    completed_count = total;
  }
  completed_count = (std::max)(0, (std::min)(completed_count, total));

  if (inspection_progress_bar_) {
    inspection_progress_bar_->setRange(0, (std::max)(1, total));
    inspection_progress_bar_->setValue(completed_count);
    inspection_progress_bar_->setFormat(
        stage == QStringLiteral("returning_home")
            ? QStringLiteral("点位完成 · 正在返回起点")
            : QStringLiteral("已完成 %1 / %2 · %3")
                  .arg(completed_count)
                  .arg(total)
                  .arg(stage_text));
  }
  if (inspection_progress_label_) {
    inspection_progress_label_->setText(
        row >= 0 && row < total
            ? QStringLiteral("第 %1 / %2 点").arg(row + 1).arg(total)
            : stage_text);
    inspection_progress_label_->setStyleSheet(
        stage == QStringLiteral("error") ? UiStyle::StatusDangerStyleSheet()
                                         : UiStyle::StatusInfoStyleSheet());
  }
}

void MainWindow::BeginRelocation(const RobotPose& pose) {
  QString reason;
  if (!IsRelocationPoseValid(pose, &reason)) {
    QMessageBox::warning(this, tr("重定位位置无效"), reason);
    display_manager_->StartReloc();
    return;
  }

  localization_confirmed_ = false;
  UpdateInspectionRouteSummary();
  const int attempt_id = ++relocation_attempt_id_;
  relocation_pending_ = true;
  relocation_target_ = pose;
  relocation_started_at_ = std::chrono::steady_clock::now();
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

void MainWindow::CheckRelocationProgress(const LocalizationEstimate& estimate) {
  if (!relocation_pending_) return;
  const auto evaluation =
      AppContract::EvaluateRelocationSample(relocation_target_, estimate);
  if (!AppContract::IsRelocationConfirmationSample(
          estimate.received_at, relocation_started_at_, evaluation)) {
    return;
  }

  relocation_pending_ = false;
  localization_confirmed_ = true;
  UpdateInspectionRouteSummary();
  if (auto* robot = display_manager_->GetDisplay(DISPLAY_ROBOT)) {
    robot->setVisible(true);
  }
  statusBar()->showMessage(
      tr("重定位成功：位置误差 %1 m，角度误差 %2°，耗时 %3 s")
          .arg(evaluation.distance, 0, 'f', 2)
          .arg(rad2deg(evaluation.angle_error), 0, 'f', 2)
          .arg(relocation_elapsed_.elapsed() / 1000.0, 0, 'f', 2),
      8000);
  nlohmann::json clear_request;
  clear_request["request_id"] =
      QString("qt-reloc-%1").arg(QDateTime::currentMSecsSinceEpoch()).toStdString();
  clear_request["command"] = "clear_costmaps";
  clear_request["target"] = "navigation";
  clear_request["params"] = nlohmann::json::object();
  PUBLISH(MSG_ID_COMMAND_REQUEST, clear_request.dump());
}

void MainWindow::SaveMapToLocalAndRobot() {
  bool accepted = false;
  const QString default_name = QString("map_%1").arg(
      QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
  const QString map_name =
      QInputDialog::getText(this, tr("保存地图"), tr("地图名称（字母、数字、下划线或短横线）："),
                            QLineEdit::Normal, default_name, &accepted)
          .trimmed();
  if (!accepted || map_name.isEmpty()) {
    return;
  }
  const QString normalized_name = map_name.toLower();
  if (normalized_name.size() > 48 || !normalized_name.at(0).isLetterOrNumber()) {
    QMessageBox::warning(this, tr("名称无效"),
                         tr("地图名称必须以字母或数字开头，且不超过 48 个字符。"));
    return;
  }
  for (const QChar ch : normalized_name) {
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

  const QString directory = MapLibraryDirectory();
  if (!QDir().mkpath(directory)) {
    QMessageBox::critical(this, tr("保存失败"),
                          tr("无法创建地图目录：\n%1").arg(directory));
    return;
  }

  const QString base_path = QDir(directory).filePath(normalized_name);
  if (QFileInfo::exists(base_path + ".yaml") ||
      QFileInfo::exists(base_path + ".pgm")) {
    QMessageBox::warning(this, tr("名称已存在"),
                         tr("地图“%1”已经存在，请使用新名称。")
                             .arg(normalized_name));
    return;
  }
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

  UploadLocalMap(yaml_path, false);
}

QString MainWindow::MapLibraryDirectory() const {
  QString root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if (root.isEmpty()) {
    root = QDir::homePath();
  }
  const QString directory = QDir(root).filePath(QStringLiteral("EggyRobot/maps"));
  QDir().mkpath(directory);
  return QDir::toNativeSeparators(directory);
}

bool MainWindow::UploadLocalMap(const QString& yaml_path, bool activate) {
  if (!pending_map_request_id_.isEmpty()) {
    statusBar()->showMessage(tr("地图切换正在进行，请等待当前操作完成。"), 5000);
    return false;
  }
  OccupancyMap map;
  if (!map.Load(yaml_path.toStdString())) {
    QMessageBox::warning(this, tr("地图无效"),
                         tr("无法读取地图 YAML：\n%1").arg(yaml_path));
    return false;
  }
  const QString image_path = QString::fromStdString(map.map_config.image);
  QFile image_file(image_path);
  if (!image_file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, tr("地图不完整"),
                         tr("地图图像文件无法读取。\nYAML：%1\n图像：%2")
                             .arg(yaml_path, image_path));
    return false;
  }

  const QString request_id = QString("qt-map-%1-%2")
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
  nlohmann::json request;
  request["request_id"] = request_id.toStdString();
  request["command"] = "upload_map";
  request["target"] = "map_library";
  const std::string upload_image =
      "./" + QFileInfo(image_path).fileName().toStdString();
  const QByteArray normalized_yaml = QByteArray::fromStdString(
      map.map_config.ToYaml(upload_image));
  request["params"] = {
      {"map_name", QFileInfo(yaml_path).completeBaseName().toStdString()},
      {"yaml_b64", normalized_yaml.toBase64().toStdString()},
      {"pgm_b64", image_file.readAll().toBase64().toStdString()},
      {"activate", activate}};

  pending_map_request_id_ = request_id;
  pending_map_yaml_path_ = yaml_path;
  pending_map_activation_ = activate;
  if (activate) {
    LoadMap(yaml_path.toStdString());
    if (open_map_btn_) {
      open_map_btn_->setEnabled(false);
      open_map_btn_->setText(tr("切换中"));
    }
    if (command_center_widget_) {
      command_center_widget_->SetExternalProfileSwitchBusy(
          true, tr("正在切换地图：上传并校验（1/3）"));
    }
  }
  statusBar()->showMessage(activate ? tr("正在切换地图（1/3）：上传并校验…")
                                    : tr("本地保存完成，正在同步到小车…"));
  PUBLISH(MSG_ID_COMMAND_REQUEST, request.dump());
  if (activate) {
    QTimer::singleShot(2200, this, [this, request_id]() {
      if (pending_map_request_id_ != request_id) {
        return;
      }
      statusBar()->showMessage(tr("正在切换地图（2/3）：启动地图服务与 AMCL…"));
      if (command_center_widget_) {
        command_center_widget_->SetExternalProfileSwitchBusy(
            true, tr("正在切换地图：启动导航节点（2/3）"));
      }
    });
    QTimer::singleShot(6000, this, [this, request_id]() {
      if (pending_map_request_id_ != request_id) {
        return;
      }
      statusBar()->showMessage(tr("正在切换地图（3/3）：等待导航就绪…"));
      if (command_center_widget_) {
        command_center_widget_->SetExternalProfileSwitchBusy(
            true, tr("正在切换地图：等待导航就绪（3/3）"));
      }
    });
  }
  QTimer::singleShot(20000, this, [this, request_id]() {
    if (pending_map_request_id_ != request_id) {
      return;
    }
    const bool activate = pending_map_activation_;
    pending_map_request_id_.clear();
    pending_map_yaml_path_.clear();
    pending_map_activation_ = false;
    if (activate) {
      if (open_map_btn_) {
        open_map_btn_->setEnabled(true);
        open_map_btn_->setText(tr("打开地图"));
      }
      if (command_center_widget_) {
        command_center_widget_->SetExternalProfileSwitchBusy(
            false, tr("地图切换超时，请检查板端状态"));
      }
    }
    statusBar()->showMessage(tr("地图操作超时：小车端未在 20 秒内确认，请检查连接和命令中心。"),
                             10000);
    QMessageBox::warning(this, tr("地图操作超时"),
                         tr("小车端未确认地图操作。已保留本地地图预览，可检查连接后重试。"));
  });
  return true;
}

void MainWindow::HandleMapCommandResponse(const std::string& json_text) {
  try {
    const auto response = nlohmann::json::parse(json_text);
    if (QString::fromStdString(response.value("request_id", std::string())) !=
        pending_map_request_id_) {
      return;
    }
    const bool success = response.value("success", false);
    const QString message =
        QString::fromStdString(response.value("message", std::string()));
    const bool activate = pending_map_activation_;
    const QString yaml_path = pending_map_yaml_path_;
    pending_map_request_id_.clear();
    pending_map_yaml_path_.clear();
    pending_map_activation_ = false;
    if (activate) {
      if (open_map_btn_) {
        open_map_btn_->setEnabled(true);
        open_map_btn_->setText(tr("打开地图"));
      }
      if (command_center_widget_) {
        command_center_widget_->SetExternalProfileSwitchBusy(
            false, success ? tr("地图与 AMCL 已就绪") : tr("地图切换失败"),
            success ? QStringLiteral("navigation") : QString());
      }
    }

    if (!success) {
      statusBar()->showMessage(tr("地图操作失败：%1").arg(message), 10000);
      QMessageBox::warning(this, tr("地图操作失败"), message);
      return;
    }
    if (!activate) {
      statusBar()->showMessage(tr("地图已同时保存到本地和小车：%1")
                                   .arg(QDir::toNativeSeparators(yaml_path)),
                               8000);
      QMessageBox::information(
          this, tr("地图保存完成"),
          tr("地图已保存并同步。\n\n本地目录：%1")
              .arg(MapLibraryDirectory()));
      return;
    }

    localization_confirmed_ = false;
    UpdateInspectionRouteSummary();
    if (auto* robot = display_manager_->GetDisplay(DISPLAY_ROBOT)) {
      robot->setVisible(false);
    }
    LoadMap(yaml_path.toStdString());
    statusBar()->showMessage(
        tr("地图已在小车端加载。请标定小车的真实位置，确认后才可导航。"));
    if (auto* robot = display_manager_->GetDisplay(DISPLAY_ROBOT)) {
      robot->setVisible(true);  // relocation preview; hidden until this explicit step
    }
    StartManualRelocation();
  } catch (const std::exception& exc) {
    LOG_ERROR("parse map command response failed: " << exc.what());
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

    Config::ConfigManager::Instance()->UpdateRootConfig(
        [this](auto& config) { config.map_config.path = map_path_; });

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
