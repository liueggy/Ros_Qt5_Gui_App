#pragma once

#include <QString>
#include <QWidget>
#include <string>

#include "msg/diagnostic_snapshot.h"

class DiagnosticDockWidget;
class QFrame;
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
  void SetCameraInspectionResult(const QString& type, const QString& reading, const QString& status);

 public slots:
  void AppendResponse(const std::string& json);
  void UpdateStatus(const std::string& json);

 private slots:
  void SendStatusRequest();
  void StartAmclNavigation();
  void SwitchToMapping();
  void StartInspection();
  void StartCamera();
  void ClearLog();

 signals:
  void CameraViewRequested(bool visible);
  void WorkspaceModeRequested(const QString& mode);

 private:
  QString MakeRequestJson(const QString& command, const QString& target,
                          const QString& paramsJson = "{}") const;
  void PublishJson(const QString& json);
  void AppendLog(const QString& prefix, const QString& text);
  void SetCameraStateText(const QString& text);
  void SetNavigationModeText(const QString& mode);
  void BeginProfileSwitch(const QString& profile, const QString& target);
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
  QToolButton* wifi_status_label_{nullptr};
  QToolButton* cellular_status_label_{nullptr};
  QLabel* nav_mode_label_{nullptr};
  QPushButton* amcl_btn_{nullptr};
  QPushButton* mapping_btn_{nullptr};
  QPushButton* inspection_btn_{nullptr};
  QLabel* status_summary_label_{nullptr};
  QPlainTextEdit* log_edit_{nullptr};
  DiagnosticDockWidget* diagnostic_widget_{nullptr};
  QFrame* diagnostic_group_{nullptr};
  QString active_workspace_mode_;
  QString pending_profile_;
  bool mapping_profile_available_{false};
  bool navigation_profile_available_{false};
  bool inspection_profile_available_{false};
};
