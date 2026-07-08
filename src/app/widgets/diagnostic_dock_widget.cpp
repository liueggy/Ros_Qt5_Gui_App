#include "diagnostic_dock_widget.h"

#include <algorithm>
#include <vector>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include "widgets/ui_style.h"

namespace {

struct ModuleEntry {
  QString hardware;
  QString name;
  const basic::DiagnosticComponentState* state;
};

int SeverityRank(int level) {
  switch (level) {
    case 2:
      return 0;
    case 3:
      return 1;
    case 1:
      return 2;
    case 0:
      return 3;
    default:
      return 0;
  }
}

}  // namespace

DiagnosticDockWidget::DiagnosticDockWidget(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(7);

  auto* summary_row = new QHBoxLayout();
  summary_row->setSpacing(6);
  overall_status_ = new QLabel();
  overall_status_->setMinimumHeight(34);
  overall_status_->setAlignment(Qt::AlignCenter);
  summary_row->addWidget(overall_status_, 1);
  refresh_btn_ = new QPushButton(tr("刷新"));
  refresh_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  refresh_btn_->setFixedHeight(34);
  connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
    SaveExpandedState();
    RebuildUi();
  });
  toggle_modules_btn_ = new QPushButton(tr("全部"));
  toggle_modules_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  toggle_modules_btn_->setFixedHeight(34);
  connect(toggle_modules_btn_, &QPushButton::clicked, this, [this]() {
    SaveExpandedState();
    show_all_modules_ = !show_all_modules_;
    RebuildUi();
  });
  summary_row->addWidget(refresh_btn_);
  summary_row->addWidget(toggle_modules_btn_);
  root->addLayout(summary_row);

  tree_ = new QTreeWidget();
  tree_->setStyleSheet(UiStyle::TableStyleSheet());
  tree_->setColumnCount(3);
  tree_->setHeaderLabels({tr("模块"), tr("状态"), tr("消息")});
  tree_->setAlternatingRowColors(true);
  tree_->setUniformRowHeights(true);
  tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  root->addWidget(tree_, 1);

  empty_label_ = new QLabel(tr("暂无诊断数据"));
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet() + QStringLiteral("padding:24px;"));
  empty_label_->hide();
  root->addWidget(empty_label_);

  UpdateOverallStatus();
  RebuildUi();
}

QString DiagnosticDockWidget::LevelDisplayName(int level) const {
  switch (level) {
    case 0:
      return tr("正常");
    case 1:
      return tr("警告");
    case 2:
      return tr("错误");
    case 3:
      return tr("过期");
    default:
      return tr("未知");
  }
}

QColor DiagnosticDockWidget::LevelColor(int level) {
  switch (level) {
    case 0:
      return QColor(UiStyle::Palette::Success);
    case 1:
      return QColor(UiStyle::Palette::Warning);
    case 2:
      return QColor(UiStyle::Palette::Danger);
    case 3:
      return QColor(UiStyle::Palette::DisabledText);
    default:
      return QColor(UiStyle::Palette::TextMuted);
  }
}

void DiagnosticDockWidget::SetSnapshot(const basic::DiagnosticSnapshot& snapshot) {
  SaveExpandedState();
  snapshot_ = snapshot;
  UpdateOverallStatus();
  RebuildUi();
}

void DiagnosticDockWidget::SaveExpandedState() {
  if (!tree_) {
    return;
  }
  expanded_items_.clear();
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* module_item = tree_->topLevelItem(i);
    if (module_item->isExpanded()) {
      expanded_items_.insert(module_item->data(0, Qt::UserRole).toString());
    }
  }
}

void DiagnosticDockWidget::RestoreExpandedState() {
  if (!tree_) {
    return;
  }
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* module_item = tree_->topLevelItem(i);
    module_item->setExpanded(
        expanded_items_.contains(module_item->data(0, Qt::UserRole).toString()));
  }
}

void DiagnosticDockWidget::UpdateOverallStatus() {
  int total = 0;
  int abnormal = 0;
  int overall_level = 0;
  for (const auto& hw : snapshot_.hardware) {
    for (const auto& comp : hw.second) {
      ++total;
      if (comp.second.level != 0) {
        ++abnormal;
      }
      if (SeverityRank(comp.second.level) < SeverityRank(overall_level)) {
        overall_level = comp.second.level;
      }
    }
  }
  const QString text = total == 0
                           ? tr("总体状态：暂无数据")
                           : tr("总体状态：%1 · %2 个模块 · %3 个异常")
                                 .arg(LevelDisplayName(overall_level))
                                 .arg(total)
                                 .arg(abnormal);
  const QColor color = total == 0 ? QColor(UiStyle::Palette::DisabledText)
                                  : LevelColor(overall_level);
  overall_status_->setText(text);
  overall_status_->setStyleSheet(
      QStringLiteral("QLabel { color:%1; background:rgba(%2,%3,%4,0.10); "
                     "border-radius:10px; padding:7px 10px; font-size:%5px; "
                     "font-weight:700; }")
          .arg(color.name())
          .arg(color.red())
          .arg(color.green())
          .arg(color.blue())
          .arg(UiStyle::FontSmallPx()));
}

void DiagnosticDockWidget::RebuildUi() {
  tree_->clear();
  if (toggle_modules_btn_) {
    toggle_modules_btn_->setText(show_all_modules_ ? tr("只看异常") : tr("全部"));
  }
  if (snapshot_.hardware.empty()) {
    tree_->hide();
    empty_label_->show();
    empty_label_->setText(tr("暂无诊断数据"));
    return;
  }

  tree_->show();
  empty_label_->hide();

  std::vector<ModuleEntry> modules;
  for (const auto& hardware : snapshot_.hardware) {
    for (const auto& component : hardware.second) {
      modules.push_back({QString::fromStdString(hardware.first),
                         QString::fromStdString(component.first), &component.second});
    }
  }
  std::stable_sort(modules.begin(), modules.end(), [](const ModuleEntry& left, const ModuleEntry& right) {
    return SeverityRank(left.state->level) < SeverityRank(right.state->level);
  });

  const int abnormal = CountAbnormal();
  if (!show_all_modules_ && abnormal == 0) {
    tree_->hide();
    empty_label_->show();
    empty_label_->setText(tr("当前全部模块正常，点击“全部”查看明细。"));
    return;
  }
  tree_->show();
  empty_label_->hide();

  for (const auto& module : modules) {
    const auto& state = *module.state;
    if (!show_all_modules_ && state.level == 0) {
      continue;
    }
    auto* module_item = new QTreeWidgetItem(tree_);
    module_item->setData(0, Qt::UserRole, module.hardware + "/" + module.name);
    module_item->setText(0, module.name);
    module_item->setToolTip(0, tr("硬件：%1").arg(module.hardware));
    module_item->setText(1, LevelDisplayName(state.level));
    module_item->setForeground(1, LevelColor(state.level));
    QString message = QString::fromStdString(state.message);
    if (message == QStringLiteral("data_stale")) {
      message = tr("数据过期");
    }
    module_item->setText(2, message.trimmed().isEmpty() ? tr("-") : message);
    module_item->setToolTip(2, module_item->text(2));

    for (const auto& kv : state.key_values) {
      auto* detail_item = new QTreeWidgetItem(module_item);
      detail_item->setText(0, QString::fromStdString(kv.first));
      detail_item->setText(2, QString::fromStdString(kv.second));
    }
  }
  RestoreExpandedState();
}

int DiagnosticDockWidget::CountAbnormal() const {
  int abnormal = 0;
  for (const auto& hardware : snapshot_.hardware) {
    for (const auto& component : hardware.second) {
      if (component.second.level != 0) {
        ++abnormal;
      }
    }
  }
  return abnormal;
}
