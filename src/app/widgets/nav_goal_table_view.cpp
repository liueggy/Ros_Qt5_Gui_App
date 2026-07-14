#include "widgets/nav_goal_table_view.h"
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QSize>
#include <QToolButton>
#include <QUuid>
#include <QWidget>
#include <fstream>
#include <nlohmann/json.hpp>
#include "algorithm.h"
#include "config/config_manager.h"
#include "logger/logger.h"
#include "widgets/ui_style.h"

namespace {
constexpr int kOrderColumn = 0;
constexpr int kPointColumn = 1;
constexpr int kTargetColumn = 2;
constexpr int kStateColumn = 3;
constexpr int kActionColumn = 4;

std::string BuildMissionJson(const nlohmann::json& route, bool loop,
                             bool return_home, bool inspection_enabled) {
  const nlohmann::json request = {
      {"schema_version", 1},
      {"request_id",
       QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()},
      {"command", "start"},
      {"mission_type", "navigation"},
      {"loop", loop},
      {"return_home", return_home},
      {"on_nav_failure", "stop"},
      {"inspection",
       {{"enabled", inspection_enabled},
        {"vision_search", inspection_enabled},
        {"ai_analysis", inspection_enabled}}},
      {"route", route},
  };
  return request.dump();
}
}  // namespace

NavGoalTableView::NavGoalTableView(QWidget* _parent_widget)
    : QTableView(_parent_widget) {
  table_model_ = new QStandardItemModel(this);
  setModel(table_model_);
  QStringList table_h_headers;
  table_h_headers << "顺序"
                  << "导航点"
                  << "识别目标"
                  << "执行状态"
                  << "操作";
  QHeaderView* headerView = new QHeaderView(Qt::Horizontal);
  headerView->setSelectionBehavior(QAbstractItemView::SelectRows);
  headerView->setCascadingSectionResizes(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setAlternatingRowColors(true);
  setShowGrid(false);
  verticalHeader()->setVisible(false);
  verticalHeader()->setDefaultSectionSize(58);
  headerView->setMinimumHeight(48);
  setStyleSheet(UiStyle::TableStyleSheet() + UiStyle::InputStyleSheet() + UiStyle::SecondaryButtonStyleSheet());
  this->setHorizontalHeader(headerView);
  // 添加数据模型
  table_model_->setHorizontalHeaderLabels(table_h_headers);
  headerView->setSectionResizeMode(kOrderColumn, QHeaderView::Fixed);
  headerView->setSectionResizeMode(kPointColumn, QHeaderView::Stretch);
  headerView->setSectionResizeMode(kTargetColumn, QHeaderView::Fixed);
  headerView->setSectionResizeMode(kStateColumn, QHeaderView::Fixed);
  headerView->setSectionResizeMode(kActionColumn, QHeaderView::Fixed);
  headerView->resizeSection(kOrderColumn, 62);
  headerView->resizeSection(kTargetColumn, 126);
  headerView->resizeSection(kStateColumn, 132);
  headerView->resizeSection(kActionColumn, 150);
  setMinimumWidth(620);
  connect(table_model_, &QStandardItemModel::itemChanged, this,
          &NavGoalTableView::onItemChanged);
}

NavGoalTableView::~NavGoalTableView() {}

void NavGoalTableView::onItemChanged(QStandardItem* item) {
  if (item->column() == kPointColumn) {
    qDebug() << "点位名: " << item->text();
  }
}
void NavGoalTableView::UpdateTopologyMap(const TopologyMap& _topology_map) {
  topologyMap_ = _topology_map;
  emit signalRouteChanged(table_model_->rowCount());
}
void NavGoalTableView::UpdateSelectPoint(const TopologyMap::PointInfo& point) {
  if (!this->isEnabled())
    return;

  QWidget* widget =
      indexWidget(model()->index(table_model_->rowCount() - 1, kPointColumn));
  if (widget) {
    QComboBox* comboBox = static_cast<QComboBox*>(widget);
    if (comboBox->currentText() == "")
      comboBox->setCurrentText(point.name.c_str());
  }
}
void NavGoalTableView::AddItem() {
  InsertRow();
}

void NavGoalTableView::InsertRow(const QString& point_name,
                                 const QString& expected_class) {
  QComboBox* comboBox = new QComboBox();
  for (auto point : topologyMap_.points) {
    comboBox->addItem(point.name.c_str());
  }
  comboBox->addItem("");
  comboBox->setCurrentText(point_name);
  comboBox->setMinimumWidth(180);
  comboBox->setFixedHeight(UiStyle::ControlHeightPx());
  connect(comboBox, &QComboBox::currentTextChanged, this,
          [this](const QString&) {
            emit signalRouteChanged(table_model_->rowCount());
          });
  QComboBox* targetType = new QComboBox();
  targetType->addItem("自动识别", "any");
  targetType->addItem("水表", "water_meter");
  targetType->addItem("压力表", "pressure_gauge");
  targetType->setFixedSize(122, UiStyle::ControlHeightPx());
  const int targetIndex = targetType->findData(expected_class);
  targetType->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
  targetType->setEnabled(inspection_enabled_ && !route_running_);
  auto* state_label = new QLabel(QStringLiteral("等待"), this);
  state_label->setAlignment(Qt::AlignCenter);
  state_label->setStyleSheet(QStringLiteral(
      "QLabel { color:%1; background:%2; border:1px solid %3; "
      "border-radius:8px; padding:5px 8px; font-weight:700; }")
      .arg(UiStyle::Palette::TextSecondary, UiStyle::Palette::SurfaceAlt,
           UiStyle::Palette::Border));
  auto* action_cell = new QWidget(this);
  auto* action_layout = new QHBoxLayout(action_cell);
  action_layout->setContentsMargins(6, 6, 6, 6);
  action_layout->setSpacing(8);
  action_layout->setAlignment(Qt::AlignCenter);
  QToolButton* button_run = new QToolButton(action_cell);
  QToolButton* button_remove = new QToolButton(action_cell);
  button_run->setText("前往");
  button_remove->setText("删除");
  button_run->setToolTip("以单点导航任务前往该点位");
  button_remove->setToolTip("删除该点位");
  button_run->setCursor(Qt::PointingHandCursor);
  button_remove->setCursor(Qt::PointingHandCursor);
  button_run->setFixedSize(62, 36);
  button_remove->setFixedSize(62, 36);
  button_run->setStyleSheet(QStringLiteral(
      "QToolButton { background:%1; color:white; border:none; border-radius:8px; "
      "font-weight:700; padding:0; }"
      "QToolButton:hover { background:%2; }")
      .arg(UiStyle::Palette::Primary, UiStyle::Palette::PrimaryHover));
  button_remove->setStyleSheet(QStringLiteral(
      "QToolButton { background:%1; color:%2; border:1px solid %3; "
      "border-radius:8px; font-weight:700; padding:0; }"
      "QToolButton:hover { background:%4; }")
      .arg(UiStyle::Palette::DangerBg, UiStyle::Palette::Danger, UiStyle::Palette::DangerBorder, UiStyle::Palette::DangerBg));
  action_layout->addWidget(button_run);
  action_layout->addWidget(button_remove);
  int row = table_model_->rowCount();

  connect(button_remove, &QToolButton::clicked, [this, action_cell]() {
    const int row = RowForWidget(action_cell);
    if (row >= 0) {
      table_model_->removeRow(row);
      RefreshOrderNumbers();
      emit signalRouteChanged(table_model_->rowCount());
    }
  });
  connect(button_run, &QToolButton::clicked, [this, comboBox, targetType]() {
    auto point =
        topologyMap_.GetPoint(comboBox->currentText().toStdString());
    if (!point.name.empty()) {
      nlohmann::json route = nlohmann::json::array();
      route.push_back({
          {"id", point.name},
          {"frame_id", "map"},
          {"x", point.x},
          {"y", point.y},
          {"yaw", point.theta},
          {"expected_class", targetType->currentData().toString().toStdString()},
      });
      emit signalMissionRequest(
          BuildMissionJson(route, false, false, inspection_enabled_));
    }
  });
  table_model_->insertRow(row);
  auto* order_item = new QStandardItem(QString::number(row + 1));
  order_item->setTextAlignment(Qt::AlignCenter);
  order_item->setEditable(false);
  table_model_->setItem(row, kOrderColumn, order_item);
  setIndexWidget(table_model_->index(row, kPointColumn), comboBox);
  setIndexWidget(table_model_->index(row, kTargetColumn), targetType);
  setIndexWidget(table_model_->index(row, kStateColumn), state_label);
  setIndexWidget(table_model_->index(row, kActionColumn), action_cell);
  emit signalRouteChanged(table_model_->rowCount());
}
bool NavGoalTableView::LoadTaskChain(const std::string& name) {
  std::ifstream file(name);
  if (!file.is_open()) {
    LOG_ERROR("Unable to open task chain file: " << name);
    return false;
  }
  TaskChain loaded_task_chain;
  try {
    nlohmann::json j;
    file >> j;
    loaded_task_chain = j.get<TaskChain>();
  } catch (const std::exception& e) {
    fprintf(stderr, "Error parsing struct %s\n", e.what());
    file.close();
    return false;
  }
  file.close();
  task_chain_ = std::move(loaded_task_chain);
  table_model_->removeRows(0, table_model_->rowCount());
  for (auto point : task_chain_.points) {
    bool find_point = false;
    for (auto p : topologyMap_.points) {
      if (point.name == p.name) {
        find_point = true;
      }
    }
    if (!find_point) {
      LOG_ERROR(
          "Can't find point " << point.name << " in topology map skip this point!");
      continue;
    }
    const auto expected = task_chain_.expected_classes.find(point.name);
    InsertRow(QString::fromStdString(point.name),
              QString::fromStdString(expected == task_chain_.expected_classes.end()
                                         ? "any"
                                         : expected->second));
  }
  emit signalRouteChanged(table_model_->rowCount());
  return true;
}
bool NavGoalTableView::SaveTaskChain(const std::string& name) {
  task_chain_.points.clear();
  task_chain_.expected_classes.clear();
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    QComboBox* comboBoxName =
        static_cast<QComboBox*>(indexWidget(model()->index(row, kPointColumn)));
    TopologyMap::PointInfo point =
        topologyMap_.GetPoint(comboBoxName->currentText().toStdString());
    if (point.name == "") {
      continue;
    }
    task_chain_.points.push_back(point);
    auto* targetType =
        static_cast<QComboBox*>(indexWidget(model()->index(row, kTargetColumn)));
    task_chain_.expected_classes[point.name] =
        targetType ? targetType->currentData().toString().toStdString() : "any";
  }
  nlohmann::json j = task_chain_;
  std::string pretty_json = j.dump(2);
  return Config::ConfigManager::writeStringToFile(name, pretty_json);
}
void NavGoalTableView::UpdateRobotPose(const RobotPose& pose) {
  robot_pose_ = pose;
}

std::string NavGoalTableView::BuildMissionRequest(bool is_loop,
                                                  bool return_home) {
  nlohmann::json route = nlohmann::json::array();
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    auto* pointCombo =
        static_cast<QComboBox*>(indexWidget(model()->index(row, kPointColumn)));
    auto* targetType =
        static_cast<QComboBox*>(indexWidget(model()->index(row, kTargetColumn)));
    if (!pointCombo) {
      continue;
    }
    const auto point =
        topologyMap_.GetPoint(pointCombo->currentText().toStdString());
    if (point.name.empty()) {
      continue;
    }
    route.push_back({
        {"id", QStringLiteral("%1#%2")
                   .arg(QString::fromStdString(point.name))
                   .arg(row + 1)
                   .toStdString()},
        {"point_name", point.name},
        {"frame_id", "map"},
        {"x", point.x},
        {"y", point.y},
        {"yaw", point.theta},
        {"expected_class",
         targetType ? targetType->currentData().toString().toStdString() : "any"},
    });
  }
  return BuildMissionJson(route, is_loop, return_home, inspection_enabled_);
}

int NavGoalTableView::RowCount() const {
  return table_model_->rowCount();
}

int NavGoalTableView::ValidPointCount() {
  int count = 0;
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    const auto* point_combo = qobject_cast<QComboBox*>(
        indexWidget(model()->index(row, kPointColumn)));
    if (point_combo &&
        !topologyMap_.GetPoint(point_combo->currentText().toStdString())
             .name.empty()) {
      ++count;
    }
  }
  return count;
}

void NavGoalTableView::SetInspectionEnabled(bool enabled) {
  inspection_enabled_ = enabled;
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    if (auto* target = indexWidget(model()->index(row, kTargetColumn))) {
      target->setEnabled(enabled && !route_running_);
    }
  }
}

void NavGoalTableView::ResetExecutionState() {
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    SetWaypointState(row, QStringLiteral("等待"), 0);
  }
}

void NavGoalTableView::SetRouteRunning(bool running) {
  route_running_ = running;
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    if (auto* point = indexWidget(model()->index(row, kPointColumn))) {
      point->setEnabled(!running);
    }
    if (auto* target = indexWidget(model()->index(row, kTargetColumn))) {
      target->setEnabled(!running && inspection_enabled_);
    }
    if (auto* action = indexWidget(model()->index(row, kActionColumn))) {
      action->setEnabled(!running);
    }
  }
}

void NavGoalTableView::SetWaypointState(int row, const QString& text, int level) {
  if (row < 0 || row >= table_model_->rowCount()) {
    return;
  }
  auto* label = qobject_cast<QLabel*>(indexWidget(model()->index(row, kStateColumn)));
  if (!label) {
    return;
  }
  QString color = UiStyle::Palette::TextSecondary;
  QString background = UiStyle::Palette::SurfaceAlt;
  QString border = UiStyle::Palette::Border;
  if (level == 1) {
    color = UiStyle::Palette::Primary;
    background = UiStyle::Palette::PrimaryLight;
    border = UiStyle::Palette::BorderFocus;
  } else if (level == 2) {
    color = UiStyle::Palette::Success;
    background = UiStyle::Palette::SuccessBg;
    border = UiStyle::Palette::SuccessBorder;
  } else if (level == 3) {
    color = UiStyle::Palette::Warning;
    background = UiStyle::Palette::WarningBg;
    border = UiStyle::Palette::WarningBorder;
  } else if (level >= 4) {
    color = UiStyle::Palette::Danger;
    background = UiStyle::Palette::DangerBg;
    border = UiStyle::Palette::DangerBorder;
  }
  label->setText(text);
  label->setStyleSheet(QStringLiteral(
      "QLabel { color:%1; background:%2; border:1px solid %3; "
      "border-radius:8px; padding:5px 8px; font-weight:700; }")
      .arg(color, background, border));
}

void NavGoalTableView::RefreshOrderNumbers() {
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    auto* item = table_model_->item(row, kOrderColumn);
    if (!item) {
      item = new QStandardItem();
      item->setEditable(false);
      item->setTextAlignment(Qt::AlignCenter);
      table_model_->setItem(row, kOrderColumn, item);
    }
    item->setText(QString::number(row + 1));
  }
}

int NavGoalTableView::RowForWidget(const QWidget* widget) const {
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    if (indexWidget(model()->index(row, kActionColumn)) == widget) {
      return row;
    }
  }
  return -1;
}
