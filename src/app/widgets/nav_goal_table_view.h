#include <QDebug>
#include <QHeaderView>
#include <QPainter>
#include <QStandardItemModel>
#include <QTableView>
#include <mutex>
#include "config/task_chain.h"
#include "map/topology_map.h"
using namespace basic;
class NavGoalTableView : public QTableView {
  Q_OBJECT
 public:
  explicit NavGoalTableView(QWidget *_parent_widget = nullptr);
  ~NavGoalTableView() override;

 private:
  QStandardItemModel *table_model_;
  TopologyMap topologyMap_;
  RobotPose robot_pose_;
  TaskChain task_chain_;
 public slots:
  void UpdateTopologyMap(const TopologyMap &_topology_map);
  void AddItem();
  void UpdateSelectPoint(const TopologyMap::PointInfo &);
  void UpdateRobotPose(const RobotPose &pose);
  bool LoadTaskChain(const std::string &name);
  bool SaveTaskChain(const std::string &name);
  std::string BuildMissionRequest(bool is_loop, bool return_home = true);
  int RowCount() const;
  int ValidPointCount();
  void SetInspectionEnabled(bool enabled);
  void ResetExecutionState();
  void SetRouteRunning(bool running);
  void SetWaypointState(int row, const QString &text, int level = 0);
 signals:
  void signalMissionRequest(const std::string &request);
  void signalRouteChanged(int point_count);

 private:
  void InsertRow(const QString &point_name = QString(),
                 const QString &expected_class = QStringLiteral("any"));
  void onItemChanged(QStandardItem *item);
  void RefreshOrderNumbers();
  int RowForWidget(const QWidget *widget) const;
  bool inspection_enabled_{false};
  bool route_running_{false};

};
