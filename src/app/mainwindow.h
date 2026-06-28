#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCalendarWidget>
#include <QComboBox>
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
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeView>
#include <QWidgetAction>
#include <QPoint>
#include <QEvent>
#include <QTimer>
#include <opencv2/imgproc/imgproc.hpp>
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "channel_manager.h"
#include "config/config_manager.h"
#include "display/manager/display_manager.h"
#include "point_type.h"
#include "widgets/dashboard.h"
#include "widgets/nav_goal_table_view.h"
#include "widgets/set_pose_widget.h"
#include "widgets/speed_ctrl.h"
#include "widgets/status_chip.h"
#include "widgets/ratio_layouted_frame.h"
#include "core/framework/framework.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DiagnosticDockWidget;
class DisplayConfigWidget;
class CommandCenterWidget;
class QMenu;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();
 public slots:
  void signalCursorPose(QPointF pos);
  void RecvChannelMsg(const MsgId &id, const std::any &data);
  void updateOdomInfo(RobotState state);
  void RestoreState();
  void SlotSetBatteryStatus(double percent, double voltage);
  void SlotRecvImage(const std::string &location, std::shared_ptr<cv::Mat> data);

 protected:
  virtual void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

 private:
  QAction *SavePerspectiveAction = nullptr;
  QWidgetAction *PerspectiveListAction = nullptr;
  ChannelManager channel_manager_;
  Ui::MainWindow *ui;
  DashBoard *speed_dash_board_;
  ads::CDockManager *dock_manager_;
  ads::CDockAreaWidget *StatusDockArea;
  ads::CDockWidget *TimelineDockWidget;
  Display::DisplayManager *display_manager_;

  QThread message_thread_;
  SpeedCtrlWidget *speed_ctrl_widget_;
  NavGoalTableView *nav_goal_table_view_;
  QProgressBar *battery_bar_;
  QLabel *label_power_;
  ads::CDockAreaWidget *center_docker_area_;
  ads::CDockAreaWidget *diagnostic_dock_area_{nullptr};
  QWidget *custom_title_bar_{nullptr};
  bool dragging_window_{false};
  QPoint drag_position_;
  std::map<std::string, RatioLayoutedFrame *> image_frame_map_;
  std::string map_path_{"./map"};
  DisplayConfigWidget *display_config_widget_{nullptr};
  ads::CDockWidget *settings_dock_{nullptr};
  DiagnosticDockWidget *diagnostic_dock_widget_{nullptr};
  ads::CDockWidget *diagnostic_dock_{nullptr};
  CommandCenterWidget *command_center_widget_{nullptr};
  ads::CDockWidget *command_center_dock_{nullptr};
  QLabel *label_dht11_temp_{nullptr};
  QLabel *label_dht11_humi_{nullptr};
  QLabel *label_voice_cmd_{nullptr};
  QTimer *voice_clear_timer_{nullptr};
  QTimer *status_refresh_timer_{nullptr};
  QTimer *image_refresh_timer_{nullptr};
  StatusChip *rosbridge_status_chip_{nullptr};
  StatusChip *base_status_chip_{nullptr};
  StatusChip *camera_status_chip_{nullptr};
  StatusChip *lidar_status_chip_{nullptr};
  StatusChip *alert_status_chip_{nullptr};
  QLabel *system_notice_banner_{nullptr};
  std::atomic<qint64> last_odom_message_ms_{0};
  std::atomic<qint64> last_camera_message_ms_{0};
  std::atomic<qint64> last_lidar_message_ms_{0};
  std::atomic<qint64> last_diagnostic_message_ms_{0};
  std::atomic<int> diagnostic_error_count_{0};
  std::atomic<int> diagnostic_warning_count_{0};
  qint64 status_monitor_started_ms_{0};
  bool channel_opened_{false};
  QMenu *control_view_menu_{nullptr};
  QMenu *task_view_menu_{nullptr};
  QMenu *diagnostic_view_menu_{nullptr};
  QMenu *settings_view_menu_{nullptr};
  std::mutex pending_images_mutex_;
  std::map<std::string, std::shared_ptr<cv::Mat>> pending_images_;
  
 signals:
  void OnRecvChannelData(const MsgId &id, const std::any &data);
  
 private:
  void setupUi();
  bool openChannel();
  bool openChannel(const std::string &channel_name);
  void closeChannel();
  void registerChannel();
  void SaveState();
  bool LoadMap(const std::string& file_path);
  void RefreshStatusBar();
  void ConfigureApplicationAppearance();
  void ConfigureDockBehavior();
  void UpdateSystemNotice(qint64 now_ms, bool connection_failed);
  void SetupDockMenus();
  void SetupOperationalDocks();
  void SetupTaskDocks();
  void SetupImageDocks();
  void SetupImageRefreshPipeline();
  void FlushPendingImages();
};
#endif  // MAINWINDOW_H
