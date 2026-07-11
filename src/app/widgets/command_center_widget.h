#pragma once

#include <QString>
#include <QWidget>
#include <string>

#include "msg/diagnostic_snapshot.h"

class DiagnosticDockWidget;
class QComboBox;
class QFrame;
class QJsonArray;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

class CommandCenterWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CommandCenterWidget(QWidget* parent = nullptr);

  void SetDiagnosticSnapshot(const basic::DiagnosticSnapshot& snapshot);
  void SetNetworkStatus(const std::string& json);
  void SetRelocalizationStatus(const std::string& json);
  void SetCameraInspectionResult(const QString& type, const QString& reading, const QString& status);

 public slots:
  void AppendResponse(const std::string& json);
  void UpdateStatus(const std::string& json);

 private slots:
  void SendStatusRequest();
  void RefreshMaps();
  void StartAmclNavigation();
  void SwitchToMapping();
  void StartCamera();
  void StopCamera();
  void ClearLog();

 private:
  QString MakeRequestJson(const QString& command, const QString& target,
                          const QString& paramsJson = "{}") const;
  void PublishJson(const QString& json);
  void AppendLog(const QString& prefix, const QString& text);
  void SetCameraStateText(const QString& text);
  void SetNavigationModeText(const QString& mode);
  void UpdateMapChoices(const QJsonArray& maps);
  QString SelectedMapFile() const;
  void SetStatusSummary(const QString& text, const QString& detail = QString());
  void SetOverviewPill(QLabel* label, const QString& title, const QString& value,
                       const QString& color, const QString& bg, const QString& border);
  void SetConnectionOverview(bool online, const QString& detail);
  void SetDiagnosticOverview(int total, int abnormal, int worstLevel);

  QLabel* connection_overview_label_{nullptr};
  QLabel* nav_overview_label_{nullptr};
  QLabel* task_overview_label_{nullptr};
  QLabel* diagnostic_overview_label_{nullptr};
  QLabel* camera_state_label_{nullptr};
  QLabel* camera_inspection_label_{nullptr};
  QPushButton* camera_start_btn_{nullptr};
  QPushButton* camera_stop_btn_{nullptr};
  QToolButton* wifi_status_label_{nullptr};
  QToolButton* cellular_status_label_{nullptr};
  QLabel* nav_mode_label_{nullptr};
  QComboBox* map_combo_{nullptr};
  QPushButton* amcl_btn_{nullptr};
  QPushButton* mapping_btn_{nullptr};
  QLabel* status_summary_label_{nullptr};
  QPlainTextEdit* log_edit_{nullptr};
  DiagnosticDockWidget* diagnostic_widget_{nullptr};
  QFrame* diagnostic_group_{nullptr};
};
