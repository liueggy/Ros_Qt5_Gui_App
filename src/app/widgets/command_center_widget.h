#pragma once

#include <QString>
#include <QWidget>
#include <string>

#include "app/mission_contract.h"
#include "msg/diagnostic_snapshot.h"

class DiagnosticDockWidget;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
class QJsonObject;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QToolButton;

class CommandCenterWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CommandCenterWidget(QWidget* parent = nullptr);

  void SetDiagnosticSnapshot(const basic::DiagnosticSnapshot& snapshot);
  void SetNetworkStatus(const std::string& json);
  void NotifyCameraFrameReceived();
  void SetExternalProfileSwitchBusy(bool busy, const QString& message = QString(),
                                    const QString& confirmedMode = QString());

 public slots:
  void AppendResponse(const std::string& json);
  void UpdateStatus(const std::string& json);
  void UpdateMotionOwner(const std::string& json);

 private slots:
  void SendStatusRequest();
  void StartAmclNavigation();
  void SwitchToMapping();
  void StartInspection();
  void StartCamera();
  void StartAutoMapping();
  void PauseResumeAutoMapping();
  void StopAutoMapping();
  void ToggleLog();
  void ClearLog();

 signals:
  void CameraViewRequested(bool visible);
  void WorkspaceModeRequested(const QString& mode);
  void InspectionCapabilityChanged(bool ready);

 private:
  void RefreshDiagnosticSnapshot();
  QString MakeRequestJson(const QString& command, const QString& target,
                          const QString& paramsJson = "{}",
                          const QString& requestId = QString()) const;
  void PublishJson(const QString& json);
  void AppendLog(const QString& prefix, const QString& text);
  void SetCameraStateText(const QString& text);
  void SetNavigationModeText(const QString& mode);
  void BeginProfileSwitch(const QString& profile, const QString& target);
  void SetStatusSummary(const QString& text, const QString& detail = QString());
  void SetOverviewPill(QLabel* label, const QString& title, const QString& value,
                       const QString& color, const QString& bg, const QString& border);
  void SetMetricPill(QLabel* label, const QString& title, const QString& value,
                     const QString& color);
  void SetMotionOwnerStatus(const AppContract::MotionOwnerStatus& status);
  void UpdateAutoMappingCard(const QJsonObject& status);
  void SendAutoMappingCommand(const QString& command);
  void RefreshAutoMappingControls();

  QLabel* motion_owner_label_{nullptr};
  QLabel* camera_state_label_{nullptr};
  QPushButton* camera_start_btn_{nullptr};
  QToolButton* wifi_status_label_{nullptr};
  QToolButton* cellular_status_label_{nullptr};
  QLabel* nav_mode_label_{nullptr};
  QPushButton* amcl_btn_{nullptr};
  QPushButton* mapping_btn_{nullptr};
  QPushButton* inspection_btn_{nullptr};
  QLabel* auto_mapping_state_label_{nullptr};
  QLabel* auto_mapping_message_label_{nullptr};
  QLabel* auto_mapping_frontier_metric_{nullptr};
  QLabel* auto_mapping_sensor_metric_{nullptr};
  QLabel* auto_mapping_safety_metric_{nullptr};
  QProgressBar* auto_mapping_progress_{nullptr};
  QSpinBox* auto_mapping_duration_spin_{nullptr};
  QDoubleSpinBox* auto_mapping_speed_spin_{nullptr};
  QCheckBox* auto_mapping_return_home_check_{nullptr};
  QPushButton* auto_mapping_start_btn_{nullptr};
  QPushButton* auto_mapping_pause_btn_{nullptr};
  QPushButton* auto_mapping_stop_btn_{nullptr};
  QLabel* status_summary_label_{nullptr};
  QPlainTextEdit* log_edit_{nullptr};
  QPushButton* log_toggle_btn_{nullptr};
  QPushButton* clear_log_btn_{nullptr};
  DiagnosticDockWidget* diagnostic_widget_{nullptr};
  QFrame* diagnostic_group_{nullptr};
  QString active_workspace_mode_;
  basic::DiagnosticSnapshot raw_diagnostic_snapshot_;
  QString auto_mapping_state_{QStringLiteral("idle")};
  QString auto_mapping_request_id_;
  QString camera_start_request_id_;
  bool pending_auto_mapping_start_{false};
  bool camera_start_pending_{false};
  bool camera_waiting_first_frame_{false};
  bool camera_frame_received_{false};
  int camera_start_generation_{0};
  AppContract::ProfileSwitchTracker profile_switch_tracker_;
  int motion_owner_generation_{0};
  bool mapping_profile_available_{false};
  bool navigation_profile_available_{false};
  bool inspection_profile_available_{false};
  bool external_profile_switch_busy_{false};
};
