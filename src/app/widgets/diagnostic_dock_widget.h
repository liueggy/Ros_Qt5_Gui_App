#pragma once

#include <QSet>
#include <QString>
#include <QWidget>
#include "msg/diagnostic_snapshot.h"

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class DiagnosticDockWidget : public QWidget {
  Q_OBJECT

 public:
  explicit DiagnosticDockWidget(QWidget* parent = nullptr);

  void SetSnapshot(const basic::DiagnosticSnapshot& snapshot);

 private:
  void RebuildUi();
  void UpdateSummary();
  void SaveExpandedState();
  void RestoreExpandedState();
  static QString ItemKey(QTreeWidgetItem* item);
  static QString FormatTimeMs(int64_t ms);
  QString LevelDisplayName(int level) const;
  static QColor LevelColor(int level);

  basic::DiagnosticSnapshot snapshot_;
  QSet<QString> expanded_items_;

  QLabel* summary_ok_{nullptr};
  QLabel* summary_warn_{nullptr};
  QLabel* summary_error_{nullptr};
  QLabel* summary_stale_{nullptr};
  QLabel* empty_label_{nullptr};
  QTreeWidget* tree_{nullptr};
  QPushButton* refresh_btn_{nullptr};
};
