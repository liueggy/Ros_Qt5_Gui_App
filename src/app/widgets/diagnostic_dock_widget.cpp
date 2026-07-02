#include "diagnostic_dock_widget.h"

#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include "widgets/ui_style.h"

namespace {

int MaxLevelInMap(const std::map<std::string, basic::DiagnosticComponentState>& m) {
  int max_level = 0;
  for (const auto& e : m) {
    if (e.second.level > max_level) {
      max_level = e.second.level;
    }
  }
  return max_level;
}

int64_t LatestUpdateMs(const std::map<std::string, basic::DiagnosticComponentState>& m) {
  int64_t t = 0;
  for (const auto& e : m) {
    if (e.second.last_update_ms > t) {
      t = e.second.last_update_ms;
    }
  }
  return t;
}

}  // namespace

DiagnosticDockWidget::DiagnosticDockWidget(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(7);

  auto* summary_row = new QHBoxLayout();
  summary_row->setSpacing(6);
  summary_ok_ = new QLabel();
  summary_warn_ = new QLabel();
  summary_error_ = new QLabel();
  summary_stale_ = new QLabel();
  for (auto* lb : {summary_ok_, summary_warn_, summary_error_, summary_stale_}) {
    lb->setMinimumHeight(28);
    lb->setMaximumHeight(32);
    lb->setAlignment(Qt::AlignCenter);
    lb->setStyleSheet(QStringLiteral("padding:3px 9px;border-radius:10px;font-size:%1px;font-weight:600;").arg(UiStyle::FontSmallPx()));
  }
  summary_ok_->setStyleSheet(summary_ok_->styleSheet() +
                             QStringLiteral("background-color:rgba(46,125,50,0.12);color:#2e7d32;"));
  summary_warn_->setStyleSheet(summary_warn_->styleSheet() +
                               QStringLiteral("background-color:rgba(245,124,0,0.12);color:#f57c00;"));
  summary_error_->setStyleSheet(summary_error_->styleSheet() +
                                QStringLiteral("background-color:rgba(211,47,47,0.12);color:#d32f2f;"));
  summary_stale_->setStyleSheet(summary_stale_->styleSheet() +
                                QStringLiteral("background-color:rgba(97,97,97,0.12);color:#616161;"));
  summary_row->addWidget(summary_ok_);
  summary_row->addWidget(summary_warn_);
  summary_row->addWidget(summary_error_);
  summary_row->addWidget(summary_stale_);
  summary_row->addStretch();
  refresh_btn_ = new QPushButton(tr("刷新"));
  refresh_btn_->setStyleSheet(UiStyle::SecondaryButtonStyleSheet());
  refresh_btn_->setFixedHeight(34);
  connect(refresh_btn_, &QPushButton::clicked, this, [this]() { RebuildUi(); });
  summary_row->addWidget(refresh_btn_);
  root->addLayout(summary_row);

  tree_ = new QTreeWidget();
  tree_->setStyleSheet(UiStyle::TableStyleSheet());
  tree_->setColumnCount(2);
  tree_->setHeaderLabels({tr("名称 / 键"), tr("状态 / 值")});
  tree_->setAlternatingRowColors(true);
  tree_->setUniformRowHeights(false);
  root->addWidget(tree_, 1);

  empty_label_ = new QLabel(tr("暂无诊断数据"));
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setStyleSheet(UiStyle::MutedLabelStyleSheet() + QStringLiteral("padding:24px;"));
  empty_label_->hide();
  root->addWidget(empty_label_);

  UpdateSummary();
  RebuildUi();
}

QString DiagnosticDockWidget::FormatTimeMs(int64_t ms) {
  if (ms <= 0) {
    return QStringLiteral("-");
  }
  QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
  return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
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
      return QColor(QStringLiteral("#2e7d32"));
    case 1:
      return QColor(QStringLiteral("#f57c00"));
    case 2:
      return QColor(QStringLiteral("#d32f2f"));
    case 3:
      return QColor(QStringLiteral("#616161"));
    default:
      return QColor(QStringLiteral("#333333"));
  }
}

void DiagnosticDockWidget::SetSnapshot(const basic::DiagnosticSnapshot& snapshot) {
  SaveExpandedState();
  snapshot_ = snapshot;
  UpdateSummary();
  RebuildUi();
}

QString DiagnosticDockWidget::ItemKey(QTreeWidgetItem* item) {
  if (!item) {
    return QString();
  }
  QStringList parts;
  for (auto* current = item; current; current = current->parent()) {
    parts.prepend(current->text(0));
  }
  return parts.join(QStringLiteral("/"));
}

void DiagnosticDockWidget::SaveExpandedState() {
  if (!tree_) {
    return;
  }
  expanded_items_.clear();
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* hw_item = tree_->topLevelItem(i);
    if (hw_item->isExpanded()) {
      expanded_items_.insert(ItemKey(hw_item));
    }
    for (int j = 0; j < hw_item->childCount(); ++j) {
      auto* comp_item = hw_item->child(j);
      if (comp_item->isExpanded()) {
        expanded_items_.insert(ItemKey(comp_item));
      }
    }
  }
}

void DiagnosticDockWidget::RestoreExpandedState() {
  if (!tree_) {
    return;
  }
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* hw_item = tree_->topLevelItem(i);
    hw_item->setExpanded(expanded_items_.contains(ItemKey(hw_item)));
    for (int j = 0; j < hw_item->childCount(); ++j) {
      auto* comp_item = hw_item->child(j);
      comp_item->setExpanded(expanded_items_.contains(ItemKey(comp_item)));
    }
  }
}

void DiagnosticDockWidget::UpdateSummary() {
  int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
  for (const auto& hw : snapshot_.hardware) {
    for (const auto& comp : hw.second) {
      int lv = comp.second.level;
      if (lv < 0 || lv > 3) {
        continue;
      }
      if (lv == 0) {
        ++c0;
      } else if (lv == 1) {
        ++c1;
      } else if (lv == 2) {
        ++c2;
      } else {
        ++c3;
      }
    }
  }
  summary_ok_->setText(tr("%1 正常").arg(c0));
  summary_warn_->setText(tr("%1 警告").arg(c1));
  summary_error_->setText(tr("%1 错误").arg(c2));
  summary_stale_->setText(tr("%1 过期").arg(c3));
}

void DiagnosticDockWidget::RebuildUi() {
  tree_->clear();
  if (snapshot_.hardware.empty()) {
    tree_->hide();
    empty_label_->show();
    empty_label_->setText(tr("暂无诊断数据"));
    return;
  }

  tree_->show();
  empty_label_->hide();

  for (const auto& entry : snapshot_.hardware) {
    const std::string& hid = entry.first;
    const auto& states = entry.second;
    int max_lv = MaxLevelInMap(states);
    QString display_hid = hid == "unknown_hardware" ? tr("未知硬件") : QString::fromStdString(hid);
    auto* hw_item = new QTreeWidgetItem(tree_);
    QFont f = hw_item->font(0);
    f.setBold(true);
    hw_item->setFont(0, f);
    hw_item->setText(0, display_hid);
    hw_item->setForeground(1, LevelColor(max_lv));
    hw_item->setText(1, tr("状态: %1 | 组件: %2 | 最新: %3")
                            .arg(LevelDisplayName(max_lv))
                            .arg(static_cast<int>(states.size()))
                            .arg(FormatTimeMs(LatestUpdateMs(states))));

    for (const auto& ce : states) {
      const std::string& comp_name = ce.first;
      const basic::DiagnosticComponentState& st = ce.second;
      auto* comp_item = new QTreeWidgetItem(hw_item);
      comp_item->setText(0, QString::fromStdString(comp_name));
      comp_item->setForeground(1, LevelColor(st.level));
      QString msg = QString::fromStdString(st.message);
      if (msg == QStringLiteral("data_stale")) {
        msg = tr("数据过期");
      }
      comp_item->setText(1, tr("状态: %1 | %2 | 更新: %3")
                                .arg(LevelDisplayName(st.level))
                                .arg(msg)
                                .arg(FormatTimeMs(st.last_update_ms)));

      if (!st.key_values.empty()) {
        for (const auto& kv : st.key_values) {
          auto* kv_item = new QTreeWidgetItem(comp_item);
          kv_item->setText(0, QString::fromStdString(kv.first));
          kv_item->setText(1, QString::fromStdString(kv.second));
        }
      } else {
        auto* empty_item = new QTreeWidgetItem(comp_item);
        empty_item->setText(0, tr("（无键值详情）"));
        empty_item->setText(1, FormatTimeMs(st.last_update_ms));
      }
    }
  }
  if (expanded_items_.isEmpty()) {
    tree_->expandToDepth(0);
  } else {
    RestoreExpandedState();
  }
}
