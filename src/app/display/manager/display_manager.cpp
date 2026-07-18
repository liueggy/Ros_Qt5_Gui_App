// 1,图元坐标系 scenPose 对应所有图层的外部全局坐标系
// 2, 图层坐标系 每个图层的单独坐标系
// 3, 占栅格地图坐标系 occPose
// 4,机器人全局地图坐标系 wordPose
#ifdef constant
  #undef constant
#endif
#include "display/manager/display_manager.h"
#include <Eigen/Eigen>
#include <QOpenGLWidget>
#include <algorithm>
#include "algorithm.h"
#include "core/framework/framework.h"
#include "display/laser_points.h"
#include "display/manager/scene_manager.h"
#include "display/point_shape.h"
namespace Display {

DisplayManager::DisplayManager() {
  graphics_view_ptr_ = new ViewManager();
  scene_manager_ptr_ = new SceneManager();
  scene_manager_ptr_->Init(graphics_view_ptr_, this);
  // 设置绘制区域
  FactoryDisplay::Instance()->Init(graphics_view_ptr_, scene_manager_ptr_);
  connect(scene_manager_ptr_,
          SIGNAL(signalTopologyMapUpdate(const TopologyMap&)), this,
          SIGNAL(signalTopologyMapUpdate(const TopologyMap&)));
  connect(
      scene_manager_ptr_,
      SIGNAL(signalCurrentSelectPointChanged(const TopologyMap::PointInfo&)),
      this,
      SIGNAL(signalCurrentSelectPointChanged(const TopologyMap::PointInfo&)));
  connect(scene_manager_ptr_,
          SIGNAL(signalEditMapModeChanged(MapEditMode)), this,
          SIGNAL(signalEditMapModeChanged(MapEditMode)));
  //------------------------------------start display instace (register
  // display)-----------------------------
  (new DisplayOccMap(DISPLAY_MAP, 1));
  (new DisplayCostMap(DISPLAY_GLOBAL_COST_MAP, 2, DISPLAY_MAP));

  (new DisplayCostMap(DISPLAY_LOCAL_COST_MAP, 3, DISPLAY_MAP));
  (new PointShape(PointShape::ePointType::kRobot, DISPLAY_ROBOT, DISPLAY_ROBOT,
                  9, DISPLAY_MAP))
      ->SetRotateEnable(true);
  new LaserPoints(DISPLAY_LASER, 2, DISPLAY_MAP);
  new DisplayPath(DISPLAY_GLOBAL_PATH, 6, DISPLAY_MAP);
  new DisplayPath(DISPLAY_LOCAL_PATH, 6, DISPLAY_MAP);
  new RobotShape(DISPLAY_ROBOT_FOOTPRINT, 8, DISPLAY_MAP);
  // defalut display config

  SetDisplayConfig(DISPLAY_GLOBAL_PATH + "/Color", Color(0, 0, 255));
  SetDisplayConfig(DISPLAY_LOCAL_PATH + "/Color", Color(0, 255, 0));
  SetDisplayConfig(DISPLAY_ROBOT_FOOTPRINT + "/Color", Color(30, 144, 255));

  // connection

  connect(GetDisplay(DISPLAY_ROBOT),
          SIGNAL(signalPoseUpdate(const RobotPose&)), this,
          SLOT(slotRobotScenePoseChanged(const RobotPose&)));
  // 设置默认地图图层响应鼠标事件
  FactoryDisplay::Instance()->SetMoveEnable(DISPLAY_MAP);
  graphics_view_ptr_->SetDisplayManagerPtr(this);
  InitUi();
  SUBSCRIBE_QOBJECT(this, MSG_ID_TOPOLOGY_MAP, [this](const TopologyMap& data) {
    scene_manager_ptr_->UpdateTopologyMap(data);
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_OCCUPANCY_MAP, [this](const OccupancyMap& data) {
    map_data_ = data;
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_ROBOT_POSE, [this](const RobotPose& data) {
    // LOG_INFO("robot pose update:" << data.x << " " << data.y << " " << data.theta);
    if (!is_reloc_mode_) {
      motion_visibility_filter_.SetAbsolutePose(data);
      UpdateRobotPose(data);
      RefreshLaserFromRobotPose();
    }
    robot_pose_received_ = true;
    robot_pose_timer_.restart();
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_ODOM_POSE, [this](const RobotState& data) {
    constexpr qint64 kAbsoluteAnchorTimeoutMs = 750;
    const bool anchor_fresh = robot_pose_received_ &&
                              robot_pose_timer_.isValid() &&
                              robot_pose_timer_.elapsed() <=
                                  kAbsoluteAnchorTimeoutMs;
    const auto predicted =
        motion_visibility_filter_.UpdateOdometry(data, anchor_fresh);
    if (!is_reloc_mode_ && predicted.has_value()) {
      UpdateRobotPose(*predicted);
      RefreshLaserFromRobotPose();
    }
  });

  SUBSCRIBE_QOBJECT(this, MSG_ID_LASER_SCAN, [this](const LaserScan& data) {
    auto* laser_display = dynamic_cast<LaserPoints*>(GetDisplay(DISPLAY_LASER));
    if (laser_display) {
      if (!data.robot_relative_data.empty() || !data.points_in_map) {
        relocation_laser_cache_[data.id] = data.RelocationPreviewData();
        constexpr std::size_t kMaxRelocationLaserSources = 8;
        while (relocation_laser_cache_.size() >
               kMaxRelocationLaserSources) {
          relocation_laser_cache_.erase(relocation_laser_cache_.begin());
        }
      }
      std::vector<Point> transformed_points;
      if (!data.robot_relative_data.empty()) {
        transformed_points = transLaserPoint(data.robot_relative_data);
      } else if (is_reloc_mode_ && !data.RelocationPreviewData().empty() &&
                 !data.points_in_map) {
        transformed_points = transLaserPoint(data.RelocationPreviewData());
      } else {
        transformed_points = data.points_in_map
                                 ? mapPointsToScene(data.data)
                                 : transLaserPoint(data.data);
      }
      laser_display->UpdateLaserData(data.id, transformed_points);
      laser_data_received_ = true;
      laser_data_stale_ = false;
      laser_data_cleared_ = false;
      laser_data_timer_.restart();
    }
  });

  freshness_timer_ = new QTimer(this);
  freshness_timer_->setInterval(100);
  connect(freshness_timer_, &QTimer::timeout, this,
          &DisplayManager::UpdateFreshnessStatus);
  freshness_timer_->start();
}

void DisplayManager::slotSetRobotPose(const RobotPose& pose) {
  FactoryDisplay::Instance()->SetMoveEnable(DISPLAY_ROBOT, false);
  UpdateRobotPose(pose);
  RefreshRelocationLaserPreview();
  // enable move after 300ms
  QTimer::singleShot(300, [this]() {
    FactoryDisplay::Instance()->SetMoveEnable(DISPLAY_ROBOT, true);
  });
}

void DisplayManager::slotRobotScenePoseChanged(const RobotPose& pose) {
  if (is_reloc_mode_) {
    QPointF occ_pose =
        GetDisplay(DISPLAY_MAP)->mapFromScene(QPointF(pose.x, pose.y));
    double x, y;
    map_data_.ScenePose2xy(occ_pose.x(), occ_pose.y(), x, y);
    // 更新坐标
    robot_pose_.x = x;
    robot_pose_.y = y;
    robot_pose_.theta = pose.theta;
    set_reloc_pose_widget_->SetPose(
        RobotPose(robot_pose_.x, robot_pose_.y, robot_pose_.theta));
    RefreshRelocationLaserPreview();
  }
}
void DisplayManager::InitUi() {
  set_reloc_pose_widget_ = new SetPoseWidget(graphics_view_ptr_);
  set_reloc_pose_widget_->hide();
  connect(set_reloc_pose_widget_, &SetPoseWidget::SignalHandleOver,
          [this](const bool& is_submit, const RobotPose& pose) {
            SetRelocMode(false);
            if (is_submit) {
              emit signalPub2DPose(pose);
            }
          });
  connect(set_reloc_pose_widget_, SIGNAL(SignalPoseChanged(const RobotPose&)),
          this, SLOT(slotSetRobotPose(const RobotPose&)));
}
DisplayManager::~DisplayManager() {}

bool DisplayManager::SetDisplayConfig(const std::string& config_name,
                                      const std::any& data) {
  QString q_config_name = QString::fromStdString(config_name);
  auto config_list = q_config_name.split("/");
  if (config_list.empty() || config_list.size() != 2) {
    return false;
  }
  VirtualDisplay* display = GetDisplay(config_list[0].toStdString());
  if (!display) {
    std::cout << "error current display not fi csxnd:"
              << config_list[0].toStdString() << " config_name:" << config_name
              << std::endl;
    return false;
  }
  // 设置图层是否响应鼠标事件
  if (config_list[1] == "MouseEvent") {
    bool is_response;
    GetAnyData(bool, data, is_response);
    display->SetMoveEnable(is_response);
    std::cout << "config:" << config_name << " res:" << is_response
              << std::endl;
  }
  return display->SetDisplayConfig(config_list[1].toStdString(), data);
}

void DisplayManager::SetRobotAppearanceConfig(
    const Config::RobotShapedConfig& config) {
  auto* robot_shape = dynamic_cast<RobotShape*>(GetDisplay(DISPLAY_ROBOT_FOOTPRINT));
  if (robot_shape) {
    robot_shape->SetAppearanceConfig(config);
  }
}

void DisplayManager::SetMapStyleConfig(const Config::MapStyleConfig& config) {
  const QColor grid_color(QString::fromStdString(config.grid_color));
  graphics_view_ptr_->SetGridStyle(config.grid_visible, config.grid_spacing,
                                   config.grid_opacity, grid_color);
  graphics_view_ptr_->UpdateMapLegend(
      QColor(QString::fromStdString(config.laser_color)),
      QColor(QString::fromStdString(config.global_path_color)),
      QColor(QString::fromStdString(config.local_path_color)));
  if (auto* laser = dynamic_cast<LaserPoints*>(GetDisplay(DISPLAY_LASER))) {
    laser->SetVisualStyle(config.laser_point_size, config.laser_opacity,
                          QColor(QString::fromStdString(config.laser_color)));
  }
  if (auto* map = dynamic_cast<DisplayOccMap*>(GetDisplay(DISPLAY_MAP))) {
    map->SetDiscoveryAnimation(config.discovery_animation,
                               config.discovery_animation_duration_ms);
  }
  const auto apply_path_color = [this](const std::string& display_name,
                                       const std::string& color_text) {
    const QColor color(QString::fromStdString(color_text));
    if (color.isValid()) {
      SetDisplayConfig(display_name + "/Color",
                       Color(color.red(), color.green(), color.blue()));
    }
  };
  apply_path_color(DISPLAY_GLOBAL_PATH, config.global_path_color);
  apply_path_color(DISPLAY_LOCAL_PATH, config.local_path_color);
  SetDisplayConfig(DISPLAY_GLOBAL_PATH + "/LineWidth", config.path_line_width);
  SetDisplayConfig(DISPLAY_LOCAL_PATH + "/LineWidth", config.path_line_width);
  for (const auto& display_name : {DISPLAY_GLOBAL_COST_MAP, DISPLAY_LOCAL_COST_MAP}) {
    if (auto* display = GetDisplay(display_name)) {
      display->setOpacity(std::clamp(config.costmap_opacity, 0, 100) / 100.0);
    }
  }
}
/**
 * @description:坐标系转换为图元坐标系
 * @return {*}
 */
std::vector<Point>
DisplayManager::transLaserPoint(const std::vector<Point>& point) {
  // point为车身坐标系下的坐标 需要根据当前机器人坐标转换为map
  std::vector<Point> res;
  res.reserve(point.size());
  for (const auto& one_point : point) {
    // 根据机器人坐标转换为map坐标系下
    basic::RobotPose map_pose = basic::absoluteSum(
        basic::RobotPose(robot_pose_.x, robot_pose_.y, robot_pose_.theta),
        basic::RobotPose(one_point.x, one_point.y, 0));
    // 转换为图元坐标系
    double x, y;
    map_data_.xy2ScenePose(map_pose.x, map_pose.y, x, y);
    res.push_back(Point(x, y));
  }
  return res;
}

void DisplayManager::RefreshRelocationLaserPreview() {
  if (!is_reloc_mode_ || relocation_laser_cache_.empty()) {
    return;
  }
  auto* laser_display = dynamic_cast<LaserPoints*>(GetDisplay(DISPLAY_LASER));
  if (!laser_display) {
    return;
  }
  for (const auto& entry : relocation_laser_cache_) {
    laser_display->UpdateLaserData(entry.first, transLaserPoint(entry.second),
                                   false);
  }
}

void DisplayManager::RefreshLaserFromRobotPose() {
  if (relocation_laser_cache_.empty()) return;
  auto* laser_display = dynamic_cast<LaserPoints*>(GetDisplay(DISPLAY_LASER));
  if (!laser_display) return;
  for (const auto& entry : relocation_laser_cache_) {
    laser_display->UpdateLaserData(entry.first, transLaserPoint(entry.second),
                                   false);
  }
}

std::vector<Point>
DisplayManager::mapPointsToScene(const std::vector<Point>& point) {
  std::vector<Point> result;
  result.reserve(point.size());
  for (const auto& map_point : point) {
    double x = 0.0;
    double y = 0.0;
    map_data_.xy2ScenePose(map_point.x, map_point.y, x, y);
    result.emplace_back(x, y);
  }
  return result;
}

/**
 * @description: 更新机器人在世界坐标系下的坐标
 * @param {Vector3f&} pose x y theta
 * @return {*}
 */
void DisplayManager::UpdateRobotPose(const RobotPose& pose) {
  robot_pose_ = pose;
  auto* robot_display = dynamic_cast<PointShape*>(GetDisplay(DISPLAY_ROBOT));
  if (robot_display) {
    robot_display->UpdateData(wordPose2Map(pose));
    robot_display->update();
  }
}

void DisplayManager::updateScaled(double value) {
  FactoryDisplay::Instance()->SetDisplayScaled(DISPLAY_LASER, value);
}
void DisplayManager::SetRelocMode(bool is_start) {
  is_reloc_mode_ = is_start;
  if (is_start) {
    FocusDisplay("");
    set_reloc_pose_widget_->SetPose(
        RobotPose(robot_pose_.x, robot_pose_.y, robot_pose_.theta));
    set_reloc_pose_widget_->move(QPoint(18, 18));
    set_reloc_pose_widget_->show();
    RefreshRelocationLaserPreview();
  } else {
    set_reloc_pose_widget_->hide();
  }
  FactoryDisplay::Instance()->SetMoveEnable(DISPLAY_ROBOT, is_start);
}

void DisplayManager::SetRelocPositionFromScene(const QPointF& scene_pos) {
  if (!is_reloc_mode_) {
    return;
  }
  auto* map_display = GetDisplay(DISPLAY_MAP);
  if (!map_display) {
    return;
  }
  const QPointF map_pos = map_display->mapFromScene(scene_pos);
  double x = 0.0;
  double y = 0.0;
  map_data_.ScenePose2xy(map_pos.x(), map_pos.y(), x, y);
  robot_pose_.x = x;
  robot_pose_.y = y;
  slotSetRobotPose(robot_pose_);
  set_reloc_pose_widget_->SetPose(robot_pose_);
}
void DisplayManager::FocusDisplay(const std::string& display_name) {
  FactoryDisplay::Instance()->SetFocusDisplay(display_name);
}

/**
 * @description: 世界坐标系点转为全局scene坐标
 * @param {Vector2f&} point 传入的点坐标
 * @return {*}
 */
RobotPose DisplayManager::wordPose2Scene(const RobotPose& point) {
  // xy在栅格地图上的图元坐标
  double x, y;
  map_data_.xy2ScenePose(point.x, point.y, x, y);
  // xy在map图层上的坐标
  QPointF pose = FactoryDisplay::Instance()
                     ->GetDisplay(DISPLAY_MAP)
                     ->PoseToScene(QPointF(x, y));

  RobotPose res;
  res.x = pose.x();
  res.y = pose.y();
  res.theta = point.theta;
  return res;
}
/**
 * @description: 世界坐标系点转为以map图层为基准坐标的图元坐标
 * @param {Vector2f&} point 传入的点坐标
 * @return {*}
 */
QPointF DisplayManager::wordPose2Scene(const QPointF& point) {
  // xy在栅格地图上的图元坐标
  double x, y;
  map_data_.xy2ScenePose(point.x(), point.y(), x, y);
  return FactoryDisplay::Instance()
      ->GetDisplay(DISPLAY_MAP)
      ->PoseToScene(QPointF(x, y));
}
RobotPose DisplayManager::wordPose2Map(const RobotPose& pose) {
  RobotPose ret = pose;
  double x, y;
  map_data_.xy2ScenePose(pose.x, pose.y, x, y);
  ret.x = x;
  ret.y = y;
  return ret;
}
QPointF DisplayManager::wordPose2Map(const QPointF& pose) {
  QPointF ret;
  double x, y;
  map_data_.xy2ScenePose(pose.x(), pose.y(), x, y);
  ret.setX(x);
  ret.setY(y);
  return ret;
}
RobotPose DisplayManager::mapPose2Word(const RobotPose& pose) {
  RobotPose ret = pose;
  double x, y;
  map_data_.ScenePose2xy(pose.x, pose.y, x, y);
  ret.x = x;
  ret.y = y;
  return ret;
}
RobotPose DisplayManager::scenePoseToWord(const RobotPose& pose) {
  QPointF pose_map = FactoryDisplay::Instance()
                         ->GetDisplay(DISPLAY_MAP)
                         ->mapFromScene(QPointF(pose.x, pose.y));
  return mapPose2Word(RobotPose(pose_map.x(), pose_map.y(), pose.theta));
}
RobotPose DisplayManager::scenePoseToMap(const RobotPose& pose) {
  QPointF pose_map = FactoryDisplay::Instance()
                         ->GetDisplay(DISPLAY_MAP)
                         ->mapFromScene(QPointF(pose.x, pose.y));
  return RobotPose(pose_map.x(), pose_map.y(), pose.theta);
}
VirtualDisplay* DisplayManager::GetDisplay(const std::string& name) {
  return FactoryDisplay::Instance()->GetDisplay(name);
}
void DisplayManager::ApplyConfiguredDisplayVisibility() {
  const std::string default_visible_displays[] = {
      DISPLAY_MAP, DISPLAY_ROBOT, DISPLAY_LASER, DISPLAY_ROBOT_FOOTPRINT,
      DISPLAY_GLOBAL_PATH, DISPLAY_LOCAL_PATH, DISPLAY_GLOBAL_COST_MAP,
      DISPLAY_LOCAL_COST_MAP};
  for (const auto& display_name : default_visible_displays) {
    auto* display = GetDisplay(display_name);
    if (display) {
      display->setVisible(true);
    }
  }

  for (const auto& display_config :
       Config::ConfigManager::Instance()->GetRootConfigSnapshot().display_config) {
    auto* display = GetDisplay(display_config.display_name);
    if (display) {
      display->setVisible(display_config.visible);
    }
  }
}

void DisplayManager::UpdateFreshnessStatus() {
  auto* view = dynamic_cast<ViewManager*>(graphics_view_ptr_);
  auto* global_path = dynamic_cast<DisplayPath*>(GetDisplay(DISPLAY_GLOBAL_PATH));
  auto* local_path = dynamic_cast<DisplayPath*>(GetDisplay(DISPLAY_LOCAL_PATH));
  auto* global_cost = dynamic_cast<DisplayCostMap*>(GetDisplay(DISPLAY_GLOBAL_COST_MAP));
  auto* local_cost = dynamic_cast<DisplayCostMap*>(GetDisplay(DISPLAY_LOCAL_COST_MAP));
  auto* laser = dynamic_cast<LaserPoints*>(GetDisplay(DISPLAY_LASER));
  if (!view || !global_path || !local_path || !global_cost || !local_cost ||
      !laser) return;

  constexpr qint64 kPoseTimeoutMs = 750;
  constexpr qint64 kLocalPathTimeoutMs = 1200;
  constexpr qint64 kGlobalPathTimeoutMs = 2500;
  constexpr qint64 kLocalCostTimeoutMs = 1200;
  constexpr qint64 kGlobalCostTimeoutMs = 2500;
  constexpr qint64 kLaserTimeoutMs = 1500;
  constexpr qint64 kLaserClearMs = 3000;
  const qint64 pose_age = robot_pose_received_ ? robot_pose_timer_.elapsed() : -1;
  const qint64 global_path_age = global_path->DataAgeMs();
  const qint64 local_path_age = local_path->DataAgeMs();
  const qint64 global_cost_age = global_cost->DataAgeMs();
  const qint64 local_cost_age = local_cost->DataAgeMs();
  const bool pose_stale = pose_age < 0 || pose_age > kPoseTimeoutMs;
  const bool global_path_stale = global_path_age < 0 || global_path_age > kGlobalPathTimeoutMs;
  const bool local_path_stale = local_path_age < 0 || local_path_age > kLocalPathTimeoutMs;
  const bool global_cost_stale = global_cost_age < 0 || global_cost_age > kGlobalCostTimeoutMs;
  const bool local_cost_stale = local_cost_age < 0 || local_cost_age > kLocalCostTimeoutMs;
  const bool laser_stale =
      !laser_data_received_ || laser_data_timer_.elapsed() > kLaserTimeoutMs;

  global_path->SetDataStale(global_path_stale);
  local_path->SetDataStale(local_path_stale);
  global_cost->SetDataStale(global_cost_stale);
  local_cost->SetDataStale(local_cost_stale);
  if (laser_data_received_ && laser_data_timer_.elapsed() > kLaserClearMs &&
      !laser_data_cleared_) {
    laser->ClearData();
    relocation_laser_cache_.clear();
    laser_data_cleared_ = true;
  }
  laser_data_stale_ = laser_stale;

  auto age_text = [](qint64 age, bool stale) {
    if (age < 0) return QStringLiteral("等待");
    const QString value = QStringLiteral("%1s").arg(age / 1000.0, 0, 'f', 1);
    return stale ? value + QStringLiteral("（过期）") : value;
  };
  const QString text =
      QStringLiteral("位姿 %1").arg(age_text(pose_age, pose_stale));
  view->UpdateDataStatus(text, pose_stale);
}
void DisplayManager::StartReloc() {
  if (!set_reloc_pose_widget_->isVisible()) {
    SetRelocMode(true);
  }
}
void DisplayManager::SetEditMapMode(MapEditMode mode) { scene_manager_ptr_->SetEditMapMode(mode); }
void DisplayManager::SetToolRange(double range) {
  if (!scene_manager_ptr_) {
    return;
  }
  scene_manager_ptr_->SetToolRange(range);
}
double DisplayManager::GetEraserRange() const { return scene_manager_ptr_->GetEraserRange(); }
double DisplayManager::GetPenRange() const { return scene_manager_ptr_->GetPenRange(); }
void DisplayManager::AddOneNavPoint() { scene_manager_ptr_->AddOneNavPoint(); }
void DisplayManager::AddPointAtRobotPosition() { scene_manager_ptr_->AddPointAtRobotPosition(); }
OccupancyMap& DisplayManager::GetMap() { return map_data_; }
OccupancyMap DisplayManager::GetOccupancyMap() {
  auto display_map_ = static_cast<DisplayOccMap*>(FactoryDisplay::Instance()->GetDisplay(DISPLAY_MAP));
  if (display_map_ != nullptr) {
    return display_map_->GetOccupancyMap();
  }
  return map_data_;
}

void DisplayManager::UpdateOCCMap(const OccupancyMap& map) {
  PUBLISH(MSG_ID_OCCUPANCY_MAP, map);
}

TopologyMap DisplayManager::GetTopologyMap() {
  return scene_manager_ptr_->GetTopologyMap();
}

void DisplayManager::UpdateTopologyMap(const TopologyMap& topology_map) {
  PUBLISH(MSG_ID_TOPOLOGY_MAP, topology_map);
  if (graphics_view_ptr_) {
    QTimer::singleShot(500, graphics_view_ptr_, &ViewManager::FitMapToBestView);
  }
}
void DisplayManager::SetScaleBig() {
  FactoryDisplay::Instance()
      ->GetDisplay(DISPLAY_MAP)
      ->SetScaled(
          FactoryDisplay::Instance()->GetDisplay(DISPLAY_MAP)->GetScaleValue() *
          1.3);
}
void DisplayManager::SetScaleSmall() {
  FactoryDisplay::Instance()
      ->GetDisplay(DISPLAY_MAP)
      ->SetScaled(
          FactoryDisplay::Instance()->GetDisplay(DISPLAY_MAP)->GetScaleValue() *
          0.7);
}
}  // namespace Display
