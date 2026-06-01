/*
 * @Author: chengyang chengyangkj@outlook.com
 * @Date: 2023-07-27 14:47:24
 * @LastEditors: chengyang chengyangkj@outlook.com
 * @LastEditTime: 2023-07-28 10:21:19
 * @FilePath: /ros_qt5_gui_app/src/channel/ros1/RosNode.cpp
 * @Description: ros1通讯类
 */
#include "rosnode.h"
#include <opencv2/opencv.hpp>
#include "config/config_manager.h"
#include "msg/diagnostic_snapshot.h"
#include "msg/msg_info.h"
RosNode::RosNode(/* args */) {
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GOAL, "/move_base_simple/goal")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SET_RELOC_POSE, "/initialpose")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_MAP, "/map")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP, "/move_base/local_costmap/costmap")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP, "/move_base/global_costmap/costmap")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LASER, "/scan")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GLOBAL_PATH, "/move_base/DWAPlannerROS/global_plan")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LOCAL_PATH, "/move_base/DWAPlannerROS/local_plan")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_ROBOT, "/odom")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED, "/cmd_vel")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_BATTERY_STATE, "/battery")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_DIAGNOSTIC, "/diagnostics")
  SET_DEFAULT_TOPIC_NAME("MoveBaseStatus", "/move_base/status")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT, "/move_base/local_costmap/published_footprint")
  SET_DEFAULT_KEY_VALUE("BaseFrameId", "base_link")
  if (Config::ConfigManager::Instance()->GetRootConfig().images.empty()) {
    Config::ConfigManager::Instance()->GetRootConfig().images.push_back(
        Config::ImageDisplayConfig{.location = "front",
                                   .topic = "/camera/front/image/compressed",
                                   .enable = true});

  }
  Config::ConfigManager::Instance()->StoreConfig();
  std::cout << "ros node start" << std::endl;
}
basic::RobotPose Convert(const geometry_msgs::Pose &pose) {
  basic::RobotPose robot_pose;

  // 提取位置信息
  robot_pose.x = pose.position.x;
  robot_pose.y = pose.position.y;

  // 提取姿态信息
  tf::Quaternion quat(pose.orientation.x, pose.orientation.y,
                      pose.orientation.z, pose.orientation.w);
  double r, p;
  tf::Matrix3x3(quat).getRPY(r, p, robot_pose.theta);

  return robot_pose;
}

RosNode::~RosNode() {}
/// @brief loop for rate
void RosNode::Process() {
  if (ros::ok()) {
    GetRobotPose();
    ros::spinOnce();
  }
}
bool RosNode::Start() {
  int argc = 0;
  ros::init(argc, nullptr, "ros_qt5_gui_app", ros::init_options::AnonymousName);
  while (!ros::master::check()) {
    LOG_ERROR("The Ros Master is not running, please check the ros master or run 'roscore' first, waitting for 300ms...");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
  }
  ros::start();
  init();
  
  SUBSCRIBE(MSG_ID_SET_NAV_GOAL_POSE, [this](const basic::RobotPose& pose) {
    std::cout << "recv nav goal pose:" << pose << std::endl;
    PubNavGoal(pose);
  });
  SUBSCRIBE(MSG_ID_SET_RELOC_POSE, [this](const basic::RobotPose& pose) {
    std::cout << "recv reloc pose:" << pose << std::endl;
    PubRelocPose(pose);
  });
  SUBSCRIBE(MSG_ID_SET_ROBOT_SPEED, [this](const basic::RobotSpeed& speed) {
    std::cout << "recv robot speed:" << speed << std::endl;
    PubRobotSpeed(speed);
  });
  
  return true;
}
void RosNode::init() {
  // 设置默认的topic名称
  ros::NodeHandle nh;
  nav_goal_publisher_ =
      nh.advertise<geometry_msgs::PoseStamped>(GET_TOPIC_NAME(DISPLAY_GOAL), 10);
  reloc_pose_publisher_ =
      nh.advertise<geometry_msgs::PoseWithCovarianceStamped>(
          GET_TOPIC_NAME(MSG_ID_SET_RELOC_POSE), 10);
  speed_publisher_ =
      nh.advertise<geometry_msgs::Twist>(GET_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED), 10);

  map_subscriber_ =
      nh.subscribe(GET_TOPIC_NAME(DISPLAY_MAP), 1, &RosNode::MapCallback, this);
  local_cost_map_subscriber_ = nh.subscribe(
      GET_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP), 1, &RosNode::LocalCostMapCallback, this);
  global_cost_map_subscriber_ =
      nh.subscribe(GET_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP), 1,
                   &RosNode::GlobalCostMapCallback, this);
  laser_scan_subscriber_ = nh.subscribe(GET_TOPIC_NAME(DISPLAY_LASER), 1,
                                        &RosNode::LaserScanCallback, this);
  global_path_subscriber_ = nh.subscribe(GET_TOPIC_NAME(DISPLAY_GLOBAL_PATH), 1,
                                         &RosNode::GlobalPathCallback, this);
  local_path_subscriber_ = nh.subscribe(GET_TOPIC_NAME(DISPLAY_LOCAL_PATH), 1,
                                        &RosNode::LocalPathCallback, this);
  odometry_subscriber_ = nh.subscribe(GET_TOPIC_NAME(DISPLAY_ROBOT), 1,
                                      &RosNode::OdometryCallback, this);
  battery_subscriber_ = nh.subscribe(GET_TOPIC_NAME(MSG_ID_BATTERY_STATE), 1,
                                     &RosNode::BatteryCallback, this);
  diagnostic_subscriber_ = nh.subscribe(GET_TOPIC_NAME(MSG_ID_DIAGNOSTIC), 1,
                                      &RosNode::DiagnosticCallback, this);
  robot_footprint_subscriber_ = nh.subscribe(GET_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT), 1,
                                             &RosNode::RobotFootprintCallback, this);

  for (auto one_image_display : Config::ConfigManager::Instance()->GetRootConfig().images) {
    LOG_INFO("image location:" << one_image_display.location << " topic:" << one_image_display.topic);
    std::string location = one_image_display.location;
    bool is_compressed = (one_image_display.topic.find("compressed") != std::string::npos);

    if (is_compressed) {
      // ── CompressedImage 订阅 ──
      image_subscriber_list_.emplace_back(nh.subscribe(
          one_image_display.topic, 1,
          boost::function<void(const sensor_msgs::CompressedImageConstPtr &)>(
              [this, location](const sensor_msgs::CompressedImageConstPtr &msg) {
                cv::Mat img = cv::imdecode(cv::Mat(msg->data), cv::IMREAD_COLOR);
                if (img.empty()) {
                  LOG_ERROR("Failed to decode compressed image");
                  return;
                }
                cv::Mat conversion_mat_;
                cv::cvtColor(img, conversion_mat_, CV_BGR2RGB);
                PUBLISH(MSG_ID_IMAGE, (std::pair<std::string, std::shared_ptr<cv::Mat>>(location, std::make_shared<cv::Mat>(conversion_mat_))));
              })));
    } else {
      // ── Image 订阅（原有逻辑） ──
      image_subscriber_list_.emplace_back(nh.subscribe(
          one_image_display.topic, 1,
          boost::function<void(const sensor_msgs::ImageConstPtr &)>(
              [this, location](const sensor_msgs::ImageConstPtr &msg) {
                cv::Mat conversion_mat_;
                try {
                  cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(
                      msg, sensor_msgs::image_encodings::RGB8);
                  conversion_mat_ = cv_ptr->image;
                } catch (cv_bridge::Exception &e) {
                  try {
                    cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(msg);
                    if (msg->encoding == "CV_8UC3") {
                      conversion_mat_ = cv_ptr->image;
                    } else if (msg->encoding == "8UC1") {
                      cv::cvtColor(cv_ptr->image, conversion_mat_, CV_GRAY2RGB);
                    } else if (msg->encoding == "16UC1" || msg->encoding == "32FC1") {
                      double min = 0;
                      double max = 10;
                      if (msg->encoding == "16UC1") max *= 1000;
                      cv::Mat img_scaled_8u;
                      cv::Mat(cv_ptr->image - min).convertTo(img_scaled_8u, CV_8UC1, 255. / (max - min));
                      cv::cvtColor(img_scaled_8u, conversion_mat_, CV_GRAY2RGB);
                    } else {
                      LOG_ERROR("image from " << msg->encoding
                                              << " to 'rgb8' an exception was thrown (%s)"
                                              << e.what());
                      return;
                    }
                  } catch (cv_bridge::Exception &e) {
                    LOG_ERROR("image from " << msg->encoding
                                            << " to 'rgb8' an exception was thrown (%s)"
                                            << e.what());
                    return;
                  }
                }
                PUBLISH(MSG_ID_IMAGE, (std::pair<std::string, std::shared_ptr<cv::Mat>>(location, std::make_shared<cv::Mat>(conversion_mat_))));
              })));
    }
  }

  // move_base_status_subscriber_ = nh.subscribe(GET_TOPIC_NAME("MoveBaseStatus"), 1,
  //                                             &RosNode::BatteryCallback, this);
  tf_listener_ = new tf::TransformListener();
}

bool RosNode::Stop() {
  ros::shutdown();
  return true;
}

void RosNode::BatteryCallback(sensor_msgs::BatteryState::ConstPtr battery) {
  std::map<std::string, std::string> map;
  map["percent"] = std::to_string(battery->percentage);
  map["voltage"] = std::to_string(battery->voltage);
  PUBLISH(MSG_ID_BATTERY_STATE, map);
}

void RosNode::DiagnosticCallback(const diagnostic_msgs::DiagnosticArray::ConstPtr &msg) {
  basic::DiagnosticSnapshot snapshot;
  const int64_t stamp_ms =
      static_cast<int64_t>(msg->header.stamp.sec) * 1000LL +
      static_cast<int64_t>(msg->header.stamp.nsec) / 1000000LL;
  for (const auto &st : msg->status) {
    std::string hardware_id = st.hardware_id;
    if (hardware_id.empty()) {
      hardware_id = "unknown_hardware";
    }
    basic::DiagnosticComponentState comp;
    comp.level = static_cast<int>(st.level);
    comp.message = st.message;
    comp.last_update_ms = stamp_ms;
    for (const auto &kv : st.values) {
      comp.key_values[kv.key] = kv.value;
    }
    snapshot.hardware[hardware_id][st.name] = std::move(comp);
  }
  PUBLISH(MSG_ID_DIAGNOSTIC, snapshot);
}
// void RosNode::MbStatusCallback(actionlib_msgs::GoalStatusArray::ConstPtr msg) {

// }
void RosNode::OdometryCallback(const nav_msgs::Odometry::ConstPtr &msg) {
  basic::RobotState state =
      static_cast<basic::RobotState>(Convert(msg->pose.pose));
  state.vx = (double)msg->twist.twist.linear.x;
  state.vy = (double)msg->twist.twist.linear.y;
  state.w = (double)msg->twist.twist.angular.z;
  PUBLISH(MSG_ID_ODOM_POSE, state);
}

void RosNode::MapCallback(nav_msgs::OccupancyGrid::ConstPtr msg) {
  // 节流：MAP_THROTTLE_SEC 内只处理最新一帧
  ros::Time now = msg->header.stamp;
  if (now.isZero()) now = ros::Time::now();
  if (!last_map_time_.isZero() && (now - last_map_time_).toSec() < MAP_THROTTLE_SEC) {
    return;
  }
  last_map_time_ = now;

  double origin_x = msg->info.origin.position.x;
  double origin_y = msg->info.origin.position.y;
  int width = msg->info.width;
  int height = msg->info.height;
  double resolution = msg->info.resolution;
  occ_map_ = basic::OccupancyMap(
      height, width, Eigen::Vector3d(origin_x, origin_y, 0), resolution);

  for (int i = 0; i < msg->data.size(); i++) {
    int x = int(i / width);
    int y = i % width;
    occ_map_(x, y) = msg->data[i];
  }
  occ_map_.SetFlip();
  PUBLISH(MSG_ID_OCCUPANCY_MAP, occ_map_);
}

void RosNode::LocalCostMapCallback(nav_msgs::OccupancyGrid::ConstPtr msg) {
  if (occ_map_.cols == 0 || occ_map_.rows == 0)
    return;
  int width = msg->info.width;
  int height = msg->info.height;
  double origin_x = msg->info.origin.position.x;
  double origin_y = msg->info.origin.position.y;
  basic::OccupancyMap cost_map(height, width,
                               Eigen::Vector3d(origin_x, origin_y, 0),
                               msg->info.resolution);
  for (int i = 0; i < msg->data.size(); i++) {
    int x = (int)i / width;
    int y = i % width;
    cost_map(x, y) = msg->data[i];
  }
  cost_map.SetFlip();
  basic::OccupancyMap sized_cost_map = occ_map_;
  basic::RobotPose origin_pose;
  try {
    geometry_msgs::PointStamped pose_map_frame;
    geometry_msgs::PointStamped pose_curr_frame;
    pose_curr_frame.point.x = origin_x;
    pose_curr_frame.point.y = origin_y;
    pose_curr_frame.header.frame_id = msg->header.frame_id;
    tf_listener_->transformPoint("map", pose_curr_frame, pose_map_frame);

    origin_pose.x = pose_map_frame.point.x;
    origin_pose.y = pose_map_frame.point.y + cost_map.heightMap();
    origin_pose.theta = 0;
  } catch (tf2::TransformException &ex) {
  }

  double map_o_x, map_o_y;
  occ_map_.xy2OccPose(origin_pose.x, origin_pose.y, map_o_x, map_o_y);

  // 只清零并填充局部代价地图覆盖的区域，不遍历全图
  sized_cost_map.map_data.setZero();
  int x_start = std::max(0, (int)map_o_x);
  int y_start = std::max(0, (int)map_o_y);
  int x_end = std::min((int)occ_map_.rows, (int)map_o_x + cost_map.rows);
  int y_end = std::min((int)occ_map_.cols, (int)map_o_y + cost_map.cols);
  for (int x = x_start; x < x_end; x++)
    for (int y = y_start; y < y_end; y++)
      sized_cost_map(x, y) = cost_map(x - (int)map_o_x, y - (int)map_o_y);
  PUBLISH(MSG_ID_LOCAL_COST_MAP, sized_cost_map);
}

void RosNode::GlobalCostMapCallback(nav_msgs::OccupancyGrid::ConstPtr msg) {
  int width = msg->info.width;
  int height = msg->info.height;
  double origin_x = msg->info.origin.position.x;
  double origin_y = msg->info.origin.position.y;
  basic::OccupancyMap cost_map(height, width,
                               Eigen::Vector3d(origin_x, origin_y, 0),
                               msg->info.resolution);
  for (int i = 0; i < msg->data.size(); i++) {
    int x = int(i / width);
    int y = i % width;
    cost_map(x, y) = msg->data[i];
  }
  cost_map.SetFlip();
  PUBLISH(MSG_ID_GLOBAL_COST_MAP, cost_map);
}

// 激光雷达点云话题回调
void RosNode::LaserScanCallback(sensor_msgs::LaserScanConstPtr msg) {
  double angle_min = msg->angle_min;
  double angle_increment = msg->angle_increment;
  try {
    // 一次 TF 查询获取 laser_frame → base_link 的变换
    std::string base_frame = Config::ConfigManager::Instance()->GetConfigValue("BaseFrameId", "base_link");
    tf::StampedTransform transform;
    tf_listener_->lookupTransform(base_frame, msg->header.frame_id, ros::Time(0), transform);
    double tx = transform.getOrigin().x();
    double ty = transform.getOrigin().y();
    double yaw = tf::getYaw(transform.getRotation());
    double cos_yaw = cos(yaw);
    double sin_yaw = sin(yaw);

    basic::LaserScan laser_points;
    laser_points.reserve(msg->ranges.size());
    for (size_t i = 0; i < msg->ranges.size(); i++) {
      double dist = msg->ranges[i];
      if (isinf(dist))
        continue;
      double angle = angle_min + i * angle_increment;
      double x = dist * cos(angle);
      double y = dist * sin(angle);
      // 一次旋转变换，不再逐点调用 TF
      basic::Point p;
      p.x = cos_yaw * x - sin_yaw * y + tx;
      p.y = sin_yaw * x + cos_yaw * y + ty;
      laser_points.push_back(p);
    }
    laser_points.id = 0;
    PUBLISH(MSG_ID_LASER_SCAN, laser_points);
  } catch (tf2::TransformException &ex) {
  }
}


void RosNode::GlobalPathCallback(nav_msgs::Path::ConstPtr msg) {
  // 节流
  ros::Time now = msg->header.stamp;
  if (now.isZero()) now = ros::Time::now();
  if (!last_global_path_time_.isZero() && (now - last_global_path_time_).toSec() < PATH_THROTTLE_SEC) {
    return;
  }
  last_global_path_time_ = now;

  try {
    // 一次 TF 查询
    tf::StampedTransform transform;
    tf_listener_->lookupTransform("map", msg->header.frame_id, ros::Time(0), transform);
    double tx = transform.getOrigin().x();
    double ty = transform.getOrigin().y();
    double yaw = tf::getYaw(transform.getRotation());
    double cos_yaw = cos(yaw);
    double sin_yaw = sin(yaw);

    basic::RobotPath path;
    path.reserve(msg->poses.size());
    for (size_t i = 0; i < msg->poses.size(); i++) {
      double x = msg->poses.at(i).pose.position.x;
      double y = msg->poses.at(i).pose.position.y;
      basic::Point point;
      point.x = cos_yaw * x - sin_yaw * y + tx;
      point.y = sin_yaw * x + cos_yaw * y + ty;
      path.push_back(point);
    }
    PUBLISH(MSG_ID_GLOBAL_PATH, path);
  } catch (tf2::TransformException &ex) {
  }
}


void RosNode::LocalPathCallback(nav_msgs::Path::ConstPtr msg) {
  // 节流
  ros::Time now = msg->header.stamp;
  if (now.isZero()) now = ros::Time::now();
  if (!last_local_path_time_.isZero() && (now - last_local_path_time_).toSec() < PATH_THROTTLE_SEC) {
    return;
  }
  last_local_path_time_ = now;

  try {
    // 一次 TF 查询
    tf::StampedTransform transform;
    tf_listener_->lookupTransform("map", msg->header.frame_id, ros::Time(0), transform);
    double tx = transform.getOrigin().x();
    double ty = transform.getOrigin().y();
    double yaw = tf::getYaw(transform.getRotation());
    double cos_yaw = cos(yaw);
    double sin_yaw = sin(yaw);

    basic::RobotPath path;
    path.reserve(msg->poses.size());
    for (size_t i = 0; i < msg->poses.size(); i++) {
      double x = msg->poses.at(i).pose.position.x;
      double y = msg->poses.at(i).pose.position.y;
      basic::Point point;
      point.x = cos_yaw * x - sin_yaw * y + tx;
      point.y = sin_yaw * x + cos_yaw * y + ty;
      path.push_back(point);
    }
    PUBLISH(MSG_ID_LOCAL_PATH, path);
  } catch (tf2::TransformException &ex) {
  }
}


void RosNode::PubRelocPose(const basic::RobotPose &pose) {
  geometry_msgs::PoseWithCovarianceStamped geo_pose;
  geo_pose.header.frame_id = "map";
  geo_pose.header.stamp = ros::Time(0);
  geo_pose.pose.pose.position.x = pose.x;
  geo_pose.pose.pose.position.y = pose.y;
  geometry_msgs::Quaternion q;  // 初始化四元数（geometry_msgs类型）
  q = tf::createQuaternionMsgFromRollPitchYaw(
      0, 0, pose.theta);  // 欧拉角转四元数（geometry_msgs::Quaternion）
  geo_pose.pose.pose.orientation = q;
  reloc_pose_publisher_.publish(geo_pose);
}


void RosNode::PubNavGoal(const basic::RobotPose &pose) {
  geometry_msgs::PoseStamped geo_pose;
  geo_pose.header.frame_id = "map";
  geo_pose.header.stamp = ros::Time(0);
  geo_pose.pose.position.x = pose.x;
  geo_pose.pose.position.y = pose.y;
  geometry_msgs::Quaternion q;  // 初始化四元数（geometry_msgs类型）
  q = tf::createQuaternionMsgFromRollPitchYaw(
      0, 0, pose.theta);  // 欧拉角转四元数（geometry_msgs::Quaternion）
  geo_pose.pose.orientation = q;
  nav_goal_publisher_.publish(geo_pose);
}


void RosNode::PubRobotSpeed(const  basic::RobotSpeed &speed) {
  geometry_msgs::Twist twist;
  twist.linear.x = speed.vx;
  twist.linear.y = speed.vy;
  twist.linear.z = 0;

  twist.angular.x = 0;
  twist.angular.y = 0;
  twist.angular.z = speed.w;

  // Publish it and resolve any remaining callbacks
  speed_publisher_.publish(twist);
}


void RosNode::GetRobotPose() {
  std::string base_frame = Config::ConfigManager::Instance()->GetConfigValue("BaseFrameId", "base_link");
  PUBLISH(MSG_ID_ROBOT_POSE, getTransform(base_frame, "map"));
}
/**
 * @description: 获取坐标变化
 * @param {string} from 要变换的坐标系
 * @param {string} to 基坐标系
 * @return {basic::RobotPose}from变换到to坐标系下，需要变换的坐标
 */
basic::RobotPose RosNode::getTransform(std::string from, std::string to) {
  basic::RobotPose ret;
  try {
    tf::StampedTransform transform;
    tf_listener_->lookupTransform(to, from, ros::Time(0), transform);
    tf::Quaternion quat = transform.getRotation();

    // 将四元数转换为欧拉角
    double roll, pitch, yaw;
    tf::Matrix3x3(quat).getRPY(roll, pitch, yaw);
    // x y
    double x = transform.getOrigin().x();
    double y = transform.getOrigin().y();
    ret.x = x;
    ret.y = y;
    ret.theta = yaw;

  } catch (tf2::TransformException &ex) {
    LOG_ERROR("getTransform error from:" << from << " to:" << to
                                          << " error:" << ex.what());
  }
  return ret;
}

void RosNode::RobotFootprintCallback(geometry_msgs::PolygonStamped::ConstPtr msg) {
  try {
    geometry_msgs::PointStamped point_map_frame;
    geometry_msgs::PointStamped point_footprint_frame;
    basic::RobotPath footprint;
    
    for (const auto& point : msg->polygon.points) {
      point_footprint_frame.point.x = point.x;
      point_footprint_frame.point.y = point.y;
      point_footprint_frame.header.frame_id = msg->header.frame_id;
      
      tf_listener_->transformPoint("map", point_footprint_frame, point_map_frame);
      
      basic::Point p;
      p.x = point_map_frame.point.x;
      p.y = point_map_frame.point.y;
      footprint.push_back(p);
    }
    
    PUBLISH(MSG_ID_ROBOT_FOOTPRINT, footprint);
  } catch (tf2::TransformException &ex) {
    LOG_ERROR("RobotFootprintCallback transform error: " << ex.what());
  }
}