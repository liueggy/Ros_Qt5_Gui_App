#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCalendarWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QMenu>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QWidgetAction>
#include <chrono>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "channel_manager.h"
#include "config/config_manager.h"
#include "core/framework/framework.h"
#include "display/manager/display_manager.h"
#include "mission_contract.h"
#include "point_type.h"
#include "widgets/nav_goal_table_view.h"
#include "widgets/ratio_layouted_frame.h"
#include "widgets/set_pose_widget.h"
#include "widgets/speed_ctrl.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DiagnosticDockWidget;
class DisplayConfigWidget;
class CommandCenterWidget;
class TerminalWidget;
class GpsLocationWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget* parent = nullptr);
  ~MainWindow();
 public slots:
  void signalCursorPose(QPointF pos);
  void RecvChannelMsg(const MsgId& id, const std::any& data);
  void updateOdomInfo(RobotState state);
  void RestoreState();
  void SlotSetBatteryStatus(double percent, double voltage);
  void SlotRecvImage(const std::string& location, std::shared_ptr<cv::Mat> data);

 protected:
  virtual void closeEvent(QCloseEvent* event) override;
  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QAction* SavePerspectiveAction = nullptr;
  QWidgetAction* PerspectiveListAction = nullptr;
  ChannelManager channel_manager_;
  Ui::MainWindow* ui;
  ads::CDockManager* dock_manager_;
  ads::CDockAreaWidget* StatusDockArea;
  ads::CDockWidget* TimelineDockWidget;
  Display::DisplayManager* display_manager_;

  QThread message_thread_;
  SpeedCtrlWidget* speed_ctrl_widget_;
  NavGoalTableView* nav_goal_table_view_;
  QProgressBar* battery_bar_;
  QLabel* top_connection_status_{nullptr};
  QToolButton* open_map_btn_{nullptr};
  ads::CDockAreaWidget* center_docker_area_;
  ads::CDockAreaWidget* settings_dock_area_{nullptr};
  ads::CDockAreaWidget* command_center_dock_area_{nullptr};
  ads::CDockAreaWidget* inspection_dock_area_{nullptr};
  QWidget* custom_title_bar_{nullptr};
  QToolButton* theme_button_{nullptr};
  QPushButton* maximize_button_{nullptr};
  bool dragging_window_ = {false};
  QPoint drag_position_;
  std::map<std::string, RatioLayoutedFrame*> image_frame_map_;
  std::map<std::string, ads::CDockWidget*> image_dock_map_;
  std::string map_path_ = {"./map"};
  DisplayConfigWidget* display_config_widget_{nullptr};
  ads::CDockWidget* settings_dock_{nullptr};
  ads::CDockWidget* speed_ctrl_dock_{nullptr};
  DiagnosticDockWidget* diagnostic_dock_widget_{nullptr};
  ads::CDockWidget* diagnostic_dock_{nullptr};
  CommandCenterWidget* command_center_widget_{nullptr};
  ads::CDockWidget* command_center_dock_{nullptr};
  GpsLocationWidget* gps_location_widget_{nullptr};
  ads::CDockWidget* gps_location_dock_{nullptr};
  TerminalWidget* terminal_widget_{nullptr};
  ads::CDockWidget* terminal_dock_{nullptr};
  ads::CDockWidget* inspection_task_dock_{nullptr};
  QLabel* inspection_status_label_{nullptr};
  QLabel* inspection_route_summary_label_{nullptr};
  QLabel* inspection_progress_label_{nullptr};
  QLabel* inspection_readiness_label_{nullptr};
  QProgressBar* inspection_progress_bar_{nullptr};
  QPlainTextEdit* inspection_result_view_{nullptr};
  QPushButton* inspection_add_button_{nullptr};
  QPushButton* inspection_load_button_{nullptr};
  QPushButton* inspection_save_button_{nullptr};
  QPushButton* inspection_start_button_{nullptr};
  QCheckBox* inspection_ai_checkbox_{nullptr};
  QCheckBox* inspection_loop_checkbox_{nullptr};
  QCheckBox* inspection_return_home_checkbox_{nullptr};
  QFrame* inspection_status_card_{nullptr};
  QLabel* inspection_kimi_banner_{nullptr};
  QString last_inspection_log_line_;
  QLabel* label_dht11_temp_{nullptr};
  QLabel* label_dht11_humi_{nullptr};
  QLabel* label_voice_cmd_{nullptr};
  QTimer* connection_monitor_timer_{nullptr};
  QTimer* voice_clear_timer_{nullptr};
  std::map<std::string, qint64> last_frame_times_;
  int connection_attempt_id_ = {0};
  bool channel_subscriptions_registered_ = {false};
  bool channel_connected_ = {false};
  QString pending_map_request_id_;
  QString pending_map_yaml_path_;
  bool pending_map_activation_ = {false};
  bool relocation_pending_ = {false};
  bool localization_confirmed_ = {false};
  bool inspection_workspace_active_{false};
  bool inspection_capability_ready_{false};
  bool inspection_running_{false};
  bool active_mission_inspection_enabled_{false};
  int active_mission_point_count_{0};
  AppContract::MissionTracker mission_tracker_;
  bool previous_settings_visible_{true};
  bool previous_speed_visible_{true};
  bool previous_command_center_visible_{true};
  bool previous_terminal_visible_{false};
  bool previous_front_camera_visible_{false};
  RobotPose relocation_target_;
  std::chrono::steady_clock::time_point relocation_started_at_;
  int relocation_attempt_id_ = {0};
  QElapsedTimer relocation_elapsed_;

 signals:
  void OnRecvChannelData(const MsgId& id, const std::any& data);

 private:
  void setupUi();
  bool openChannel();
  bool openChannel(const std::string& channel_name);
  void closeChannel();
  void registerChannel();
  void PublishImageStreamVisibility() const;
  void SaveState();
  void SaveMapToLocalAndRobot();
  bool LoadMap(const std::string& file_path);
  QString MapLibraryDirectory() const;
  bool UploadLocalMap(const QString& yaml_path, bool activate);
  void HandleMapCommandResponse(const std::string& json);
  void ApplyCenteredWindowGeometry();
  void ApplyDefaultDockSizes();
  void ConfigureFloatingOnOpen(ads::CDockWidget* dock, const QSize& preferred_size);
  void CenterFloatingDock(ads::CDockWidget* dock, const QSize& preferred_size);
  void UpdateMaximizeButton();
  void StartManualRelocation();
  void PublishNavGoalSafely(const RobotPose& pose);
  void AppendInspectionLogLine(const QString& line);
  void ApplyWorkspaceMode(const QString& mode);
  void UpdateInspectionRouteSummary();
  void StartMissionRequest(const std::string& request);
  bool IsCurrentMissionMessage(const nlohmann::json& data) const;
  void SetInspectionRunning(bool running);
  void UpdateInspectionProgress(const nlohmann::json& data);
  void BeginRelocation(const RobotPose& pose);
  void CheckRelocationProgress(const LocalizationEstimate& estimate);
  bool IsRelocationPoseValid(const RobotPose& pose, QString* reason);
};
#endif  // MAINWINDOW_H
