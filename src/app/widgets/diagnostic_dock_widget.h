#pragma once

#include <QColor>
#include <QSet>
#include <QString>
#include <QWidget>
#include "msg/diagnostic_snapshot.h"

class QLabel;
class QPushButton;
class QTreeWidget;

class DiagnosticDockWidget : public QWidget {
  Q_OBJECT

 public:
  explicit DiagnosticDockWidget(QWidget* parent = nullptr);

  void SetSnapshot(const basic::DiagnosticSnapshot& snapshot);

 private:
  void RebuildUi();
  void UpdateOverallStatus();
  void SaveExpandedState();
  void RestoreExpandedState();
  QString LevelDisplayName(int level) const;
  static QColor LevelColor(int level);

  basic::DiagnosticSnapshot snapshot_;
  QSet<QString> expanded_items_;

  QLabel* overall_status_{nullptr};
  QLabel* empty_label_{nullptr};
  QTreeWidget* tree_{nullptr};
  QPushButton* refresh_btn_{nullptr};
};
