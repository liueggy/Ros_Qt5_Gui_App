#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCalendarWidget>
#include <QComboBox>
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
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QWidgetAction>
#include <memory>
#include <opencv2/imgproc/imgproc.hpp>
#include <vector>
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "channel_manager.h"
#include "config/config_manager.h"
#include "core/framework/framework.h"
#include "display/manager/display_manager.h"
#include "point_type.h"
#include "widgets/dashboard.h"
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
  DashBoard* speed_dash_board_;
  ads::CDockManager* dock_manager_;
  ads::CDockAreaWidget* StatusDockArea;
  ads::CDockWidget* TimelineDockWidget;
  Display::DisplayManager* display_manager_;

  QThread message_thread_;
  SpeedCtrlWidget* speed_ctrl_widget_;
  NavGoalTableView* nav_goal_table_view_;
  QProgressBar* battery_bar_;
  ads::CDockAreaWidget* center_docker_area_;
  ads::CDockAreaWidget* settings_dock_area_{nullptr};
  ads::CDockAreaWidget* command_center_dock_area_{nullptr};
  QWidget* custom_title_bar_{nullptr};
  QPushButton* maximize_button_{nullptr};
  bool dragging_window_{false};
  QPoint drag_position_;
  std::map<std::string, RatioLayoutedFrame*> image_frame_map_;
  std::string map_path_{"./map"};
  DisplayConfigWidget* display_config_widget_{nullptr};
  ads::CDockWidget* settings_dock_{nullptr};
  DiagnosticDockWidget* diagnostic_dock_widget_{nullptr};
  ads::CDockWidget* diagnostic_dock_{nullptr};
  CommandCenterWidget* command_center_widget_{nullptr};
  ads::CDockWidget* command_center_dock_{nullptr};
  TerminalWidget* terminal_widget_{nullptr};
  ads::CDockWidget* terminal_dock_{nullptr};
  QLabel* inspection_status_label_{nullptr};
  QPlainTextEdit* inspection_result_view_{nullptr};
  QPushButton* inspection_start_button_{nullptr};
  QLabel* label_dht11_temp_{nullptr};
  QLabel* label_dht11_humi_{nullptr};
  QLabel* label_voice_cmd_{nullptr};
  QTimer* voice_clear_timer_{nullptr};
  int connection_attempt_id_{0};
  bool channel_subscriptions_registered_{false};
  bool relocation_pending_{false};
  RobotPose relocation_target_;
  int relocation_stable_samples_{0};
  int relocation_attempt_id_{0};
  QElapsedTimer relocation_elapsed_;

 signals:
  void OnRecvChannelData(const MsgId& id, const std::any& data);

 private:
  void setupUi();
  bool openChannel();
  bool openChannel(const std::string& channel_name);
  void closeChannel();
  void registerChannel();
  void SaveState();
  void SaveMapToLocalAndRobot();
  bool LoadMap(const std::string& file_path);
  void ApplyCenteredWindowGeometry();
  void ApplyDefaultDockSizes();
  void ConfigureFloatingOnOpen(ads::CDockWidget* dock, const QSize& preferred_size);
  void CenterFloatingDock(ads::CDockWidget* dock, const QSize& preferred_size);
  void UpdateMaximizeButton();
  void BeginRelocation(const RobotPose& pose);
  void CheckRelocationProgress(const RobotPose& pose);
  bool IsRelocationPoseValid(const RobotPose& pose, QString* reason);
};
#endif  // MAINWINDOW_H
