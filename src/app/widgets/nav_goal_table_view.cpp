#include "widgets/nav_goal_table_view.h"
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QSize>
#include <QToolButton>
#include <QWidget>
#include <fstream>
#include <nlohmann/json.hpp>
#include "algorithm.h"
#include "config/config_manager.h"
#include "logger/logger.h"
#include "widgets/ui_style.h"
NavGoalTableView::NavGoalTableView(QWidget* _parent_widget)
    : QTableView(_parent_widget) {
  table_model_ = new QStandardItemModel();
  setModel(table_model_);
  QStringList table_h_headers;
  table_h_headers << "点位名"
                  << "目标类型"
                  << "操作";
  QHeaderView* headerView = new QHeaderView(Qt::Horizontal);
  headerView->setSelectionBehavior(QAbstractItemView::SelectRows);
  headerView->setCascadingSectionResizes(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setAlternatingRowColors(true);
  setShowGrid(false);
  verticalHeader()->setVisible(false);
  verticalHeader()->setDefaultSectionSize(44);
  setStyleSheet(UiStyle::TableStyleSheet() + UiStyle::InputStyleSheet() + UiStyle::SecondaryButtonStyleSheet());
  this->setHorizontalHeader(headerView);
  // 添加数据模型
  table_model_->setHorizontalHeaderLabels(table_h_headers);
  headerView->setSectionResizeMode(0, QHeaderView::Stretch);
  headerView->setSectionResizeMode(1, QHeaderView::Fixed);
  headerView->setSectionResizeMode(2, QHeaderView::Fixed);
  headerView->resizeSection(1, 124);
  headerView->resizeSection(2, 128);
  setColumnWidth(1, 124);
  setColumnWidth(2, 128);
  setMinimumWidth(430);
  connect(table_model_, &QStandardItemModel::itemChanged, this,
          &NavGoalTableView::onItemChanged);
}

NavGoalTableView::~NavGoalTableView() {}

void NavGoalTableView::onItemChanged(QStandardItem* item) {
  if (item->column() == 0) {
    qDebug() << "点位名: " << item->text();
  }
}
void NavGoalTableView::UpdateTopologyMap(const TopologyMap& _topology_map) {
  topologyMap_ = _topology_map;
}
void NavGoalTableView::UpdateSelectPoint(const TopologyMap::PointInfo& point) {
  if (!this->isEnabled())
    return;

  QWidget* widget =
      indexWidget(model()->index(table_model_->rowCount() - 1, 0));
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
  QComboBox* targetType = new QComboBox();
  targetType->addItem("自动识别", "any");
  targetType->addItem("水表", "water_meter");
  targetType->addItem("压力表", "pressure_gauge");
  targetType->setMinimumWidth(112);
  const int targetIndex = targetType->findData(expected_class);
  targetType->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
  auto* action_cell = new QWidget(this);
  auto* action_layout = new QHBoxLayout(action_cell);
  action_layout->setContentsMargins(4, 4, 4, 4);
  action_layout->setSpacing(6);
  QToolButton* button_run = new QToolButton(action_cell);
  QToolButton* button_remove = new QToolButton(action_cell);
  button_run->setText("前往");
  button_remove->setText("删除");
  button_run->setToolTip("运行到该点位");
  button_remove->setToolTip("删除该点位");
  button_run->setCursor(Qt::PointingHandCursor);
  button_remove->setCursor(Qt::PointingHandCursor);
  button_run->setFixedSize(54, 32);
  button_remove->setFixedSize(54, 32);
  button_run->setStyleSheet(QStringLiteral(
      "QToolButton { background:#2f6fed; color:white; border:none; border-radius:8px; "
      "font-weight:700; padding:0; }"
      "QToolButton:hover { background:#245ed8; }"));
  button_remove->setStyleSheet(QStringLiteral(
      "QToolButton { background:#fff7f7; color:#d93025; border:1px solid #ffd3d0; "
      "border-radius:8px; font-weight:700; padding:0; }"
      "QToolButton:hover { background:#ffeceb; }"));
  action_layout->addWidget(button_run);
  action_layout->addWidget(button_remove);
  int row = table_model_->rowCount();

  connect(button_remove, &QToolButton::clicked, [this, action_cell]() {
    const QModelIndex index = indexAt(action_cell->pos());
    if (index.isValid()) {
      table_model_->removeRow(index.row());
    }
  });
  connect(button_run, &QToolButton::clicked, [this, comboBox]() {
    auto point =
        topologyMap_.GetPoint(comboBox->currentText().toStdString());
    if (!point.name.empty()) {
      emit signalSendNavGoal(point.ToRobotPose());
    }
  });
  table_model_->insertRow(row);

  setIndexWidget(table_model_->index(row, 0), comboBox);
  setIndexWidget(table_model_->index(row, 1), targetType);
  setIndexWidget(table_model_->index(row, 2), action_cell);
}
bool NavGoalTableView::LoadTaskChain(const std::string& name) {
  // 清空模型
  table_model_->removeRows(0, table_model_->rowCount());
  std::ifstream file(name);
  try {
    nlohmann::json j;
    file >> j;
    task_chain_ = j.get<TaskChain>();
  } catch (const std::exception& e) {
    fprintf(stderr, "Error parsing struct %s\n", e.what());
    file.close();
    return false;
  }
  file.close();
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
  return true;
}
bool NavGoalTableView::SaveTaskChain(const std::string& name) {
  task_chain_.points.clear();
  task_chain_.expected_classes.clear();
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    QComboBox* comboBoxName =
        static_cast<QComboBox*>(indexWidget(model()->index(row, 0)));
    TopologyMap::PointInfo point =
        topologyMap_.GetPoint(comboBoxName->currentText().toStdString());
    if (point.name == "") {
      continue;
    }
    task_chain_.points.push_back(point);
    auto* targetType =
        static_cast<QComboBox*>(indexWidget(model()->index(row, 1)));
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

std::string NavGoalTableView::BuildInspectionRequest(bool is_loop) {
  nlohmann::json request = {
      {"command", "start"},
      {"loop", is_loop},
      {"return_home", true},
      {"route", nlohmann::json::array()},
  };
  for (int row = 0; row < table_model_->rowCount(); ++row) {
    auto* pointCombo =
        static_cast<QComboBox*>(indexWidget(model()->index(row, 0)));
    auto* targetType =
        static_cast<QComboBox*>(indexWidget(model()->index(row, 1)));
    if (!pointCombo) {
      continue;
    }
    const auto point =
        topologyMap_.GetPoint(pointCombo->currentText().toStdString());
    if (point.name.empty()) {
      continue;
    }
    request["route"].push_back({
        {"id", point.name},
        {"frame_id", "map"},
        {"x", point.x},
        {"y", point.y},
        {"yaw", point.theta},
        {"expected_class",
         targetType ? targetType->currentData().toString().toStdString() : "any"},
        {"allow_vision_intercept", true},
    });
  }
  return request.dump();
}

int NavGoalTableView::RowCount() const {
  return table_model_->rowCount();
}
