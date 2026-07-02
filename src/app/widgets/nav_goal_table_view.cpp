#include "widgets/nav_goal_table_view.h"
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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
                  << "任务状态"
                  << "删除"
                  << "运行";
  QHeaderView* headerView = new QHeaderView(Qt::Horizontal);
  headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
  headerView->setSelectionBehavior(QAbstractItemView::SelectRows);
  headerView->setCascadingSectionResizes(false);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setAlternatingRowColors(true);
  setShowGrid(false);
  verticalHeader()->setVisible(false);
  verticalHeader()->setDefaultSectionSize(40);
  setStyleSheet(UiStyle::TableStyleSheet() + UiStyle::InputStyleSheet() + UiStyle::SecondaryButtonStyleSheet());
  this->setHorizontalHeader(headerView);
  // 添加数据模型
  table_model_->setHorizontalHeaderLabels(table_h_headers);
  connect(table_model_, &QStandardItemModel::itemChanged, this,
          &NavGoalTableView::onItemChanged);
}

NavGoalTableView::~NavGoalTableView() {}

void NavGoalTableView::onItemChanged(QStandardItem* item) {
  if (item->column() == 0) {
    qDebug() << "点位名: " << item->text();
  } else if (item->column() == 2) {
    qDebug() << "任务状态: " << item->checkState();
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
  QComboBox* targetType = new QComboBox();
  targetType->addItem("自动识别", "any");
  targetType->addItem("水表", "water_meter");
  targetType->addItem("压力表", "pressure_gauge");
  const int targetIndex = targetType->findData(expected_class);
  targetType->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
  QLabel* label_status = new QLabel("无");
  QPushButton* button_remove = new QPushButton("删除");
  QPushButton* button_run = new QPushButton("运行");
  button_remove->setStyleSheet(UiStyle::LinkButtonStyleSheet(QStringLiteral("#c5221f")));
  button_run->setStyleSheet(UiStyle::MainButtonStyleSheet());
  int row = table_model_->rowCount();

  connect(button_remove, &QPushButton::clicked, [this, row]() {
    QModelIndexList selectedIndexes = selectionModel()->selectedRows();
    if (selectedIndexes.size() == 1) {
      table_model_->removeRow(selectedIndexes[0].row());
    }
  });
  connect(button_run, &QPushButton::clicked, [this, comboBox]() {
    auto point =
        topologyMap_.GetPoint(comboBox->currentText().toStdString());
    if (!point.name.empty()) {
      emit signalSendNavGoal(point.ToRobotPose());
    }
  });
  table_model_->insertRow(row);

  setIndexWidget(table_model_->index(row, 0), comboBox);
  setIndexWidget(table_model_->index(row, 1), targetType);
  setIndexWidget(table_model_->index(row, 2), label_status);
  setIndexWidget(table_model_->index(row, 3), button_remove);
  setIndexWidget(table_model_->index(row, 4), button_run);
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
