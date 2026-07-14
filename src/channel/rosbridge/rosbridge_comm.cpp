/**
 * @file rosbridge_comm.cpp
 * @brief ROSBridge通信通道实现
 * @details 通过WebSocket连接ROS Bridge服务器，实现ROS话题的订阅和发布
 */

#include "rosbridge_comm.h"
#include "include/rosbridge_contract.h"
#include "include/protocol_validation.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <boost/asio.hpp>
#include <cctype>
#include <chrono>
#include <charconv>
#include <cmath>
#include <limits>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <vector>
#include "include/ros_time.h"
#include "msg/diagnostic_snapshot.h"

namespace {
bool ProbeTcpEndpoint(const std::string& host, int port,
                      std::chrono::milliseconds timeout) {
  try {
    boost::asio::ip::tcp::iostream stream;
    stream.expires_after(timeout);
    stream.connect(host, std::to_string(port));
    if (!stream) {
      LOG_WARN("ROSBridge TCP probe failed: "
               << host << ":" << port << " " << stream.error().message());
      return false;
    }
    stream.close();
    return true;
  } catch (const std::exception& exc) {
    LOG_WARN("ROSBridge TCP probe exception: " << exc.what());
    return false;
  }
}

std::vector<uint8_t> DecodeBase64(const char* input, size_t length) {
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> output;
  int val = 0;
  int valb = -8;

  for (size_t i = 0; i < length; ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);
    if (std::isspace(c)) continue;
    if (c == '=') break;

    size_t index = chars.find(static_cast<char>(c));
    if (index == std::string::npos) break;

    val = (val << 6) + static_cast<int>(index);
    valb += 6;
    if (valb >= 0) {
      output.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }

  return output;
}

std::string NormalizeFrameId(const std::string& frame_id) {
  if (!frame_id.empty() && frame_id[0] == '/') {
    return frame_id.substr(1);
  }
  return frame_id;
}
}  // namespace

/**
 * @brief 构造函数，初始化默认配置
 */
RosbridgeComm::RosbridgeComm() {
  // 设置默认话题名称
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GOAL, "/goal_pose")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SET_RELOC_POSE, "/initialpose")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_LOCALIZATION_POSE, "/amcl_pose")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_MAP, "/map")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP, contract::kLocalCostMapTopic)
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP, contract::kGlobalCostMapTopic)
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LASER, "/scan")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_GLOBAL_PATH, contract::kGlobalPathTopic)
  SET_DEFAULT_TOPIC_NAME(DISPLAY_LOCAL_PATH, contract::kLocalPathTopic)
  SET_DEFAULT_TOPIC_NAME(DISPLAY_ROBOT, "/odom")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED, contract::kManualCmdVelTopic)
  SET_DEFAULT_TOPIC_NAME(MSG_ID_BATTERY_STATE, "/battery")
  SET_DEFAULT_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT, contract::kFootprintTopic)
  SET_DEFAULT_TOPIC_NAME(DISPLAY_TOPOLOGY_MAP, "/map/topology")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_TOPOLOGY_MAP_UPDATE, "/map/topology/update")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_DIAGNOSTIC, "/diagnostics")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_COMMAND_REQUEST, "/eggy/command/request")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_COMMAND_RESPONSE, "/eggy/command/response")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_COMMAND_STATUS, "/eggy/command/status")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_MISSION_REQUEST, "/eggy/mission/request")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_MISSION_STATUS, "/eggy/mission/status")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_MISSION_RESULT, "/eggy/mission/result")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_AUTO_EXPLORE_STATUS, "/auto_explore/status")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_DHT11_TEMP, "/stm32/dht11/temperature")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_DHT11_HUMI, "/stm32/dht11/humidity")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_VOICE_COMMAND, "/stm32/voice_command")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_NETWORK_STATUS, "/eggy/network/status")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_CMD_VEL_CONTROL, "/eggy/cmd_vel/control")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SHELL_REQUEST, "/eggy/shell/request")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SHELL_OUTPUT, "/eggy/shell/output")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SHELL_STATUS, "/eggy/shell/status")
  SET_DEFAULT_TOPIC_NAME(MSG_ID_SHELL_CANCEL, "/eggy/shell/cancel")

  // 设置默认键值配置
  SET_DEFAULT_KEY_VALUE("BaseFrameId", "base_link")

  // 设置默认通道配置
  Config::ConfigManager::Instance()->UpdateRootConfig([](auto& config) {
    if (config.channel_config.channel_type.empty()) config.channel_config.channel_type = "rosbridge";
    if (config.channel_config.rosbridge_config.ip.empty()) config.channel_config.rosbridge_config.ip = "192.168.31.50";
    if (config.channel_config.rosbridge_config.port.empty()) config.channel_config.rosbridge_config.port = "9090";
    contract::MigrateLegacyTopic(config.display_config, DISPLAY_LOCAL_COST_MAP,
                                 "/local_costmap/costmap",
                                 contract::kLocalCostMapTopic);
    contract::MigrateLegacyTopic(config.display_config, DISPLAY_GLOBAL_COST_MAP,
                                 "/global_costmap/costmap",
                                 contract::kGlobalCostMapTopic);
    contract::MigrateLegacyTopic(config.display_config, DISPLAY_GLOBAL_PATH,
                                 "/plan", contract::kGlobalPathTopic);
    contract::MigrateLegacyTopic(config.display_config, DISPLAY_LOCAL_PATH,
                                 "/local_plan", contract::kLocalPathTopic);
    contract::MigrateLegacyTopic(
        config.display_config, DISPLAY_ROBOT_FOOTPRINT,
        "/local_costmap/published_footprint", contract::kFootprintTopic);
    contract::MigrateLegacyTopic(config.display_config,
                                 MSG_ID_SET_ROBOT_SPEED, "/cmd_vel",
                                 contract::kManualCmdVelTopic);
    contract::MigrateLegacyCameraTopic(config.images);
    contract::EnsureStableCameraContracts(config.images);
  });
}

/**
 * @brief 启动ROSBridge连接并初始化订阅者和发布者
 * @return 成功返回true，失败返回false
 */
bool RosbridgeComm::Start() {
  // 从配置读取ROSBridge服务器地址和端口
  const auto config = Config::ConfigManager::Instance()->GetRootConfigSnapshot();
  rosbridge_ip_ = config.channel_config.rosbridge_config.ip.empty() ? "192.168.31.50" : config.channel_config.rosbridge_config.ip;
  const std::string port_text = config.channel_config.rosbridge_config.port.empty()
                                    ? "9090"
                                    : config.channel_config.rosbridge_config.port;
  int parsed_port = 0;
  const auto parse_result =
      std::from_chars(port_text.data(), port_text.data() + port_text.size(), parsed_port);
  if (parse_result.ec != std::errc{} ||
      parse_result.ptr != port_text.data() + port_text.size() ||
      parsed_port < 1 || parsed_port > 65535) {
    connecting_ = false;
    connected_ = false;
    connection_failed_ = true;
    std::lock_guard<std::mutex> lock(error_msg_mutex_);
    connection_error_msg_ = "Invalid ROSBridge port: " + port_text +
                            ". Expected an integer from 1 to 65535.";
    LOG_ERROR(connection_error_msg_);
    return false;
  }
  rosbridge_port_ = parsed_port;

  connection_failed_ = false;
  connected_ = false;
  connecting_ = true;
  reconnect_enabled_ = true;
  image_jobs_.Reset();
  if (!image_worker_thread_.joinable()) {
    image_worker_thread_ = std::thread(&RosbridgeComm::ImageWorkerLoop, this);
  }
  {
    std::lock_guard<std::mutex> lock(error_msg_mutex_);
    connection_error_msg_.clear();
  }

  connection_thread_ = std::thread(&RosbridgeComm::ConnectAsync, this);
  return true;
}

void RosbridgeComm::ConnectAsync() {
  // TCP 预检: 快速判断网络是否可达
  if (!ProbeTcpEndpoint(rosbridge_ip_, rosbridge_port_,
                        std::chrono::milliseconds(1200))) {
    {
      std::lock_guard<std::mutex> lock(error_msg_mutex_);
      connection_error_msg_ =
          "ROSBridge server is not reachable at " + rosbridge_ip_ + ":" +
          std::to_string(rosbridge_port_);
    }
    connection_failed_ = true;
    connected_ = false;
    connecting_ = false;
    LOG_ERROR("ROSBridge TCP preflight failed; channel startup aborted.");
    return;
  }

  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  if (!reconnect_enabled_) {
    connecting_ = false;
    return;
  }

  LOG_INFO("Starting ROSBridge connection...");
  // 创建WebSocket连接
  websocket_connection_ = std::make_unique<SocketWebSocketConnection>();
  LOG_INFO("WebSocket connection created");
  websocket_connection_->RegisterErrorCallback([this](TransportError err) {
    {
      std::lock_guard<std::mutex> lock(error_msg_mutex_);
      if (err == TransportError::R2C_CONNECTION_CLOSED) {
        connection_error_msg_ = "ROSBridge connection closed";
        LOG_ERROR("ROSBridge connection closed");
      } else if (err == TransportError::R2C_SOCKET_ERROR) {
        connection_error_msg_ = "ROSBridge socket error";
        LOG_ERROR("ROSBridge socket error");
      } else if (err == TransportError::R2C_HEARTBEAT_TIMEOUT) {
        connection_error_msg_ = "ROSBridge heartbeat timeout";
        LOG_ERROR("ROSBridge heartbeat timeout (half-open connection)");
      }
    }
    connection_failed_ = true;
    connected_ = false;
    connecting_ = false;

    std::thread finished_reconnect_thread;
    {
      std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
      if (!reconnect_enabled_ || !init_flag_ || reconnecting_) {
        return;
      }
      reconnecting_ = true;
      if (reconnect_thread_.joinable()) {
        finished_reconnect_thread = std::move(reconnect_thread_);
      }
    }
    if (finished_reconnect_thread.joinable()) {
      finished_reconnect_thread.join();
    }
    {
      std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
      if (!reconnect_enabled_ || !init_flag_) {
        reconnecting_ = false;
        return;
      }
      reconnect_thread_ = std::thread(&RosbridgeComm::ReconnectLoop, this);
    }
    LOG_INFO("Starting reconnection loop...");
  });

  LOG_INFO("Creating ROSBridge instance");
  // 创建ROSBridge实例
  ros_bridge_ = std::make_unique<ROSBridge>(*websocket_connection_);

  // 连接到ROSBridge服务器
  LOG_INFO("Attempting to connect to ROSBridge server at " << rosbridge_ip_ << ":" << rosbridge_port_);
  if (!ros_bridge_->Init(rosbridge_ip_, rosbridge_port_)) {
    {
      std::lock_guard<std::mutex> lock(error_msg_mutex_);
      connection_error_msg_ =
          "Failed to connect to ROSBridge server " + rosbridge_ip_ + ":" +
          std::to_string(rosbridge_port_) +
          "\n\nPlease check:\n"
          "1. ROSBridge server is running\n"
          "2. IP and port are correct\n"
          "3. Network is reachable";
    }
    LOG_ERROR("Failed to connect to ROSBridge server!");
    connection_failed_ = true;
    connected_ = false;
    connecting_ = false;
    ros_bridge_.reset();
    if (websocket_connection_) {
      websocket_connection_->Disconnect();
    }
    websocket_connection_.reset();
    return;
  }

  LOG_INFO("Successfully connected to ROSBridge server!");
  connecting_ = false;
  connection_failed_ = false;

  {
    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
    reconnecting_ = false;
  }

  // ========== 订阅ROS话题 ==========

  // 地图话题订阅
  auto map_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_MAP), "nav_msgs/OccupancyGrid", 1);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_MAP)] = map_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { MapCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_MAP)] = std::move(map_topic);

  // 局部代价地图话题订阅
  auto local_cost_map_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP), "nav_msgs/OccupancyGrid", 1);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP)] = local_cost_map_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { LocalCostMapCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_LOCAL_COST_MAP)] = std::move(local_cost_map_topic);

  // 全局代价地图话题订阅
  auto global_cost_map_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP), "nav_msgs/OccupancyGrid", 1);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP)] = global_cost_map_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { GlobalCostMapCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_GLOBAL_COST_MAP)] = std::move(global_cost_map_topic);

  // 激光扫描话题订阅
  auto laser_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_LASER), "sensor_msgs/LaserScan", 5);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_LASER)] = laser_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { LaserCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_LASER)] = std::move(laser_topic);

  // 电池状态话题订阅
  auto battery_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(MSG_ID_BATTERY_STATE), "sensor_msgs/BatteryState", 1);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_BATTERY_STATE)] = battery_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { BatteryCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_BATTERY_STATE)] = std::move(battery_topic);

  // 全局路径话题订阅
  auto global_path_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_GLOBAL_PATH), "nav_msgs/Path", 5);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_GLOBAL_PATH)] = global_path_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { PathCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_GLOBAL_PATH)] = std::move(global_path_topic);

  // 局部路径话题订阅
  auto local_path_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_LOCAL_PATH), "nav_msgs/Path", 5);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_LOCAL_PATH)] = local_path_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { LocalPathCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_LOCAL_PATH)] = std::move(local_path_topic);

  // 里程计话题订阅
  auto odom_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_ROBOT), "nav_msgs/Odometry", 5);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_ROBOT)] = odom_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { OdomCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_ROBOT)] = std::move(odom_topic);

  auto localization_pose_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_LOCALIZATION_POSE),
      "geometry_msgs/PoseWithCovarianceStamped", 5);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_LOCALIZATION_POSE)] =
      localization_pose_topic->Subscribe(
          [this](const ROSBridgePublishMsg& msg) { LocalizationPoseCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_LOCALIZATION_POSE)] =
      std::move(localization_pose_topic);

  // 机器人足迹话题订阅
  auto robot_footprint_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT), "geometry_msgs/PolygonStamped", 20);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT)] = robot_footprint_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { RobotFootprintCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_ROBOT_FOOTPRINT)] = std::move(robot_footprint_topic);

  // 拓扑地图话题订阅
  auto topology_map_topic = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_TOPOLOGY_MAP), "topology_msgs/TopologyMap", 1);
  callback_handles_[GET_TOPIC_NAME(DISPLAY_TOPOLOGY_MAP)] = topology_map_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { TopologyMapCallback(msg); });
  subscribers_[GET_TOPIC_NAME(DISPLAY_TOPOLOGY_MAP)] = std::move(topology_map_topic);

  auto diagnostic_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_DIAGNOSTIC), "diagnostic_msgs/DiagnosticArray", 1);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_DIAGNOSTIC)] = diagnostic_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { DiagnosticCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_DIAGNOSTIC)] = std::move(diagnostic_topic);

  // Eggy 命令中心反馈订阅
  auto command_response_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_COMMAND_RESPONSE), "std_msgs/String", 10);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_COMMAND_RESPONSE)] = command_response_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { CommandResponseCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_COMMAND_RESPONSE)] = std::move(command_response_topic);

  auto command_status_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_COMMAND_STATUS), "std_msgs/String", 1);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_COMMAND_STATUS)] = command_status_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { CommandStatusCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_COMMAND_STATUS)] = std::move(command_status_topic);

  for (const MsgId id : {MsgId::kNetworkStatus, MsgId::kCmdVelControl,
                         MsgId::kShellOutput,
                         MsgId::kShellStatus}) {
    const std::string topic_name = GET_TOPIC_NAME(ToString(id));
    auto topic = std::make_unique<ROSTopic>(*ros_bridge_, topic_name, "std_msgs/String", 10);
    callback_handles_[topic_name] = topic->Subscribe(
        [this, id](const ROSBridgePublishMsg& msg) { StringMessageCallback(msg, id); });
    subscribers_[topic_name] = std::move(topic);
  }

  for (const MsgId id : {MsgId::kMissionStatus, MsgId::kMissionResult}) {
    const std::string topic_name = GET_TOPIC_NAME(ToString(id));
    auto topic = std::make_unique<ROSTopic>(*ros_bridge_, topic_name,
                                            "std_msgs/String", 10);
    callback_handles_[topic_name] = topic->Subscribe(
        [this, id](const ROSBridgePublishMsg& msg) {
          StringMessageCallback(msg, id);
        });
    subscribers_[topic_name] = std::move(topic);
  }

  auto auto_explore_status_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_AUTO_EXPLORE_STATUS), "std_msgs/String", 5);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_AUTO_EXPLORE_STATUS)] =
      auto_explore_status_topic->Subscribe(
          [this](const ROSBridgePublishMsg& msg) { AutoExploreStatusCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_AUTO_EXPLORE_STATUS)] =
      std::move(auto_explore_status_topic);

  // DHT11 温湿度话题订阅
  auto dht11_temp_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_DHT11_TEMP), "std_msgs/Float32", 1);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_DHT11_TEMP)] = dht11_temp_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { Dht11TempCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_DHT11_TEMP)] = std::move(dht11_temp_topic);

  auto dht11_humi_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_DHT11_HUMI), "std_msgs/Float32", 1);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_DHT11_HUMI)] = dht11_humi_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { Dht11HumiCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_DHT11_HUMI)] = std::move(dht11_humi_topic);

  // 语音命令话题订阅
  auto voice_topic = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_VOICE_COMMAND), "std_msgs/String", 10);
  callback_handles_[GET_TOPIC_NAME(MSG_ID_VOICE_COMMAND)] = voice_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { VoiceCommandCallback(msg); });
  subscribers_[GET_TOPIC_NAME(MSG_ID_VOICE_COMMAND)] = std::move(voice_topic);

  // 图像话题订阅（动态配置）
  for (const auto& one_image_display : Config::ConfigManager::Instance()->GetRootConfigSnapshot().images) {
    if (!one_image_display.enable) continue;
    LOG_INFO("image location:" << one_image_display.location << " topic:" << one_image_display.topic);
    std::string msg_type = (one_image_display.topic.find("compressed") != std::string::npos)
                               ? "sensor_msgs/CompressedImage"
                               : "sensor_msgs/Image";
    auto image_topic = std::make_unique<ROSTopic>(*ros_bridge_, one_image_display.topic, msg_type, 1);
    image_topic->SetThrottleRate(150);
    std::string location = one_image_display.location;
    callback_handles_[one_image_display.topic] = image_topic->Subscribe(
        [this, location](const ROSBridgePublishMsg& msg) { ImageCallback(msg, location); });
    subscribers_[one_image_display.topic] = std::move(image_topic);
  }

  // TF变换话题订阅
  auto tf_topic = std::make_unique<ROSTopic>(*ros_bridge_, "/tf", "tf2_msgs/TFMessage", 10);
  callback_handles_["/tf"] = tf_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { TfCallback(msg); });
  subscribers_["/tf"] = std::move(tf_topic);

  // TF静态变换话题订阅
  auto tf_static_topic = std::make_unique<ROSTopic>(*ros_bridge_, "/tf_static", "tf2_msgs/TFMessage", 5);
  callback_handles_["/tf_static"] = tf_static_topic->Subscribe(
      [this](const ROSBridgePublishMsg& msg) { TfCallback(msg); });
  subscribers_["/tf_static"] = std::move(tf_static_topic);

  // ========== 发布ROS话题 ==========

  // 导航目标点发布者
  auto nav_goal_publisher = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(DISPLAY_GOAL), "geometry_msgs/PoseStamped", 10);
  nav_goal_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(DISPLAY_GOAL)] = std::move(nav_goal_publisher);

  // 重定位位姿发布者
  auto reloc_pose_publisher = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(MSG_ID_SET_RELOC_POSE), "geometry_msgs/PoseWithCovarianceStamped", 10);
  reloc_pose_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(MSG_ID_SET_RELOC_POSE)] = std::move(reloc_pose_publisher);

  // 机器人速度发布者
  auto speed_publisher = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED), "geometry_msgs/Twist", 10);
  speed_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED)] = std::move(speed_publisher);

  // 拓扑地图更新发布者
  auto topology_map_update_publisher = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(MSG_ID_TOPOLOGY_MAP_UPDATE), "topology_msgs/TopologyMap", 1);
  topology_map_update_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(MSG_ID_TOPOLOGY_MAP_UPDATE)] = std::move(topology_map_update_publisher);

  // Eggy 命令中心请求发布者
  auto command_request_publisher = std::make_unique<ROSTopic>(*ros_bridge_, GET_TOPIC_NAME(MSG_ID_COMMAND_REQUEST), "std_msgs/String", 10);
  command_request_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(MSG_ID_COMMAND_REQUEST)] = std::move(command_request_publisher);

  for (const MsgId id : {MsgId::kShellRequest, MsgId::kShellCancel}) {
    const std::string topic_name = GET_TOPIC_NAME(ToString(id));
    auto topic = std::make_unique<ROSTopic>(*ros_bridge_, topic_name, "std_msgs/String", 10);
    topic->Advertise();
    publishers_[topic_name] = std::move(topic);
  }

  auto mission_request_publisher = std::make_unique<ROSTopic>(
      *ros_bridge_, GET_TOPIC_NAME(MSG_ID_MISSION_REQUEST), "std_msgs/String", 5);
  mission_request_publisher->Advertise();
  publishers_[GET_TOPIC_NAME(MSG_ID_MISSION_REQUEST)] =
      std::move(mission_request_publisher);

  // ========== 订阅内部消息总线 ==========

  message_bus_subscriptions_.clear();

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_SET_NAV_GOAL_POSE, [this](const basic::RobotPose& pose) {
    LOG_INFO("recv nav goal pose:" << pose);
    PubNavGoal(pose);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_SET_RELOC_POSE, [this](const basic::RobotPose& pose) {
    LOG_INFO("recv reloc pose:" << pose);
    PubRelocPose(pose);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_SET_ROBOT_SPEED, [this](const basic::RobotSpeed& speed) {
    LOG_INFO("recv robot speed:" << speed);
    PubRobotSpeed(speed);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_TOPOLOGY_MAP_UPDATE, [this](const TopologyMap& topology_map) {
    LOG_INFO("recv topology map update:" << topology_map.map_name);
    PubTopologyMapUpdate(topology_map);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_COMMAND_REQUEST, [this](const std::string& json_request) {
    LOG_INFO("recv eggy command request:" << json_request);
    PubCommandRequest(json_request);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_SHELL_REQUEST, [this](const std::string& json_request) {
    PubStringRequest(MsgId::kShellRequest, json_request);
  });
  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_SHELL_CANCEL, [this](const std::string& json_request) {
    PubStringRequest(MsgId::kShellCancel, json_request);
  });

  SUBSCRIBE_SCOPED_TO(message_bus_subscriptions_, MSG_ID_MISSION_REQUEST, [this](const std::string& json_request) {
    LOG_INFO("recv mission request:" << json_request);
    PubStringRequest(MsgId::kMissionRequest, json_request);
  });

  const bool ready = websocket_connection_ &&
                     websocket_connection_->IsConnected() &&
                     !connection_failed_.load();
  init_flag_ = ready;
  connected_ = ready;
  if (!ready) {
    connection_failed_ = true;
    connecting_ = false;
    LOG_ERROR("ROSBridge disconnected before channel initialization completed.");
  }
}

/**
 * @brief 停止ROSBridge连接并清理资源
 * @return 成功返回true
 */
bool RosbridgeComm::Stop() {
  init_flag_ = false;
  connected_ = false;
  connecting_ = false;
  reconnect_enabled_ = false;
  image_jobs_.Close();
  if (image_worker_thread_.joinable()) image_worker_thread_.join();

  std::thread reconnect_thread;
  {
    std::lock_guard<std::mutex> reconnect_lock(reconnect_mutex_);
    reconnecting_ = false;
    if (reconnect_thread_.joinable()) {
      reconnect_thread = std::move(reconnect_thread_);
    }
  }

  if (reconnect_thread.joinable()) {
    reconnect_thread.join();
  }

  if (connection_thread_.joinable()) {
    connection_thread_.join();
  }

  message_bus_subscriptions_.clear();

  {
    std::lock_guard<std::mutex> transport_lock(transport_mutex_);
    CleanupTransportLocked();
  }
  return true;
}

void RosbridgeComm::CleanupTransportLocked() {
  connected_ = false;
  // Stop the socket receiver before destroying topics and ROSBridge objects
  // whose callbacks/references it may still use.
  if (websocket_connection_) {
    websocket_connection_->Disconnect();
  }
  subscribers_.clear();
  publishers_.clear();
  callback_handles_.clear();
  ros_bridge_.reset();
  websocket_connection_.reset();
}

/**
 * @brief 重连循环，在连接断开时自动尝试重连
 */
void RosbridgeComm::ReconnectLoop() {
  const int reconnect_interval_seconds = 3;

  while (reconnect_enabled_ && init_flag_) {
    {
      std::lock_guard<std::mutex> lock(reconnect_mutex_);
      if (!reconnecting_) {
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval_seconds));

    if (!reconnect_enabled_ || !init_flag_) {
      break;
    }

    {
      std::lock_guard<std::mutex> lock(reconnect_mutex_);
      if (!reconnecting_) {
        break;
      }
    }

    LOG_INFO("Attempting to reconnect to ROSBridge server at " << rosbridge_ip_ << ":" << rosbridge_port_);

    if (connection_thread_.joinable()) {
      connection_thread_.join();
    }

    {
      std::lock_guard<std::mutex> transport_lock(transport_mutex_);
      CleanupTransportLocked();
    }

    connecting_ = true;
    connected_ = false;
    connection_failed_ = false;
    {
      std::lock_guard<std::mutex> lock(error_msg_mutex_);
      connection_error_msg_.clear();
    }

    connection_thread_ = std::thread(&RosbridgeComm::ConnectAsync, this);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
      std::lock_guard<std::mutex> lock(reconnect_mutex_);
      if (!connection_failed_ && !reconnecting_) {
        LOG_INFO("Reconnection successful!");
        break;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(reconnect_mutex_);
    reconnecting_ = false;
  }

  LOG_INFO("Reconnection loop stopped");
}

/**
 * @brief 处理循环，定期更新机器人位姿
 */
void RosbridgeComm::Process() {
  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  if (init_flag_ && ros_bridge_ && ros_bridge_->IsHealthy()) {
    GetRobotPose();
  }
}

/**
 * @brief 获取机器人位姿并发布
 */
void RosbridgeComm::GetRobotPose() {
  std::string base_frame = GET_CONFIG_VALUE("BaseFrameId", "base_link");
  auto pose = GetTransform("map", base_frame);
  PUBLISH(MSG_ID_ROBOT_POSE, pose);
}

/**
 * @brief TF变换回调函数，更新TF缓存
 * @param msg ROSBridge消息
 */
void RosbridgeComm::TfCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("transforms")) return;

  const auto& transforms = msg_json["transforms"];
  if (!transforms.IsArray()) return;

  std::lock_guard<std::mutex> lock(tf_cache_mutex_);

  // 遍历所有变换并更新缓存
  for (rapidjson::SizeType i = 0; i < transforms.Size(); i++) {
    const auto& transform_stamped = transforms[i];
    if (!transform_stamped.HasMember("header") || !transform_stamped.HasMember("child_frame_id") ||
        !transform_stamped.HasMember("transform")) {
      LOG_INFO("TfCallback: skipping invalid transform at index " << i);
      continue;
    }

    std::string child_frame = transform_stamped["child_frame_id"].GetString();
    const auto& transform = transform_stamped["transform"];

    if (!transform.HasMember("translation") || !transform.HasMember("rotation")) {
      LOG_INFO("TfCallback: skipping transform with missing translation/rotation for child_frame: " << child_frame);
      continue;
    }

    // 提取平移和旋转信息
    const auto& translation = transform["translation"];
    const auto& rotation = transform["rotation"];

    double x = translation["x"].GetDouble();
    double y = translation["y"].GetDouble();
    double qx = rotation.HasMember("x") ? rotation["x"].GetDouble() : 0.0;
    double qy = rotation.HasMember("y") ? rotation["y"].GetDouble() : 0.0;
    double qz = rotation.HasMember("z") ? rotation["z"].GetDouble() : 0.0;
    double qw = rotation.HasMember("w") ? rotation["w"].GetDouble() : 1.0;

    // 将四元数转换为欧拉角（yaw）
    double theta = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));

    // 获取父坐标系名称
    std::string parent_frame = "unknown";
    if (transform_stamped["header"].HasMember("frame_id")) {
      parent_frame = transform_stamped["header"]["frame_id"].GetString();
    }

    // LOG_INFO("TfCallback: transform - parent: [" << parent_frame << "] -> child: [" << child_frame
    //         << "] (x: " << x << ", y: " << y << ", theta: " << theta << ")");

    // 存储变换数据
    TransformData tf_data;
    tf_data.x = x;
    tf_data.y = y;
    tf_data.theta = theta;
    tf_data.parent_frame = parent_frame;

    tf_cache_[child_frame] = tf_data;
  }

  // 更新 TF2Rosbridge 图结构
  tf2_.UpdateTF(tf_cache_);
}

/**
 * @brief 获取两个坐标系之间的变换
 * @param from 源坐标系
 * @param to 目标坐标系
 * @return 变换后的位姿
 */
basic::RobotPose RosbridgeComm::GetTransform(const std::string& from, const std::string& to) {
  std::lock_guard<std::mutex> lock(tf_cache_mutex_);
  return tf2_.LookUpForTransform(from, to);
}

/**
 * @brief 地图回调函数，处理占用栅格地图消息
 * @param msg ROSBridge消息
 */
void RosbridgeComm::MapCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  std::string validation_error;
  if (!validation::ValidateOccupancyGrid(msg_json, &validation_error)) {
    LOG_ERROR("Rejected map: " << validation_error);
    return;
  }
  if (!msg_json.HasMember("info") || !msg_json.HasMember("data")) return;

  const auto& info = msg_json["info"];
  if (!info.HasMember("origin") || !info.HasMember("width") ||
      !info.HasMember("height") || !info.HasMember("resolution")) return;

  // 提取地图元信息
  const auto& origin = info["origin"]["position"];
  double origin_x = origin["x"].GetDouble();
  double origin_y = origin["y"].GetDouble();
  int width = info["width"].GetInt();
  int height = info["height"].GetInt();
  double resolution = info["resolution"].GetDouble();

  // 创建占用栅格地图
  basic::OccupancyMap new_map(height, width, Eigen::Vector3d(origin_x, origin_y, 0), resolution);

  // 填充地图数据
  const auto& data = msg_json["data"];
  if (data.IsArray()) {
    rapidjson::SizeType max_size = static_cast<rapidjson::SizeType>(width * height);
    for (rapidjson::SizeType i = 0; i < data.Size() && i < max_size; i++) {
      int x = static_cast<int>(i / width);
      int y = static_cast<int>(i % width);
      int8_t value = data[i].GetInt();
      new_map(x, y) = value;
    }
  }
  new_map.SetFlip();

  occ_map_ = new_map;
  PUBLISH(MSG_ID_OCCUPANCY_MAP, new_map);
}

/**
 * @brief 局部代价地图回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::LocalCostMapCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull() || occ_map_.cols == 0 || occ_map_.rows == 0) return;

  const auto& msg_json = msg.msg_json_;
  std::string validation_error;
  if (!validation::ValidateOccupancyGrid(msg_json, &validation_error)) {
    LOG_ERROR("Rejected local costmap: " << validation_error);
    return;
  }
  if (!msg_json.HasMember("info") || !msg_json.HasMember("data")) return;

  const auto& info = msg_json["info"];
  if (!info.HasMember("origin") || !info.HasMember("width") ||
      !info.HasMember("height") || !info.HasMember("resolution")) return;

  // 提取原点信息（包含位置和方向）
  const auto& origin = info["origin"];
  double origin_x = origin["position"]["x"].GetDouble();
  double origin_y = origin["position"]["y"].GetDouble();
  double origin_theta = 0.0;
  if (origin.HasMember("orientation")) {
    const auto& orientation = origin["orientation"];
    double qx = orientation.HasMember("x") ? orientation["x"].GetDouble() : 0.0;
    double qy = orientation.HasMember("y") ? orientation["y"].GetDouble() : 0.0;
    double qz = orientation.HasMember("z") ? orientation["z"].GetDouble() : 0.0;
    double qw = orientation.HasMember("w") ? orientation["w"].GetDouble() : 1.0;
    origin_theta = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));
  }

  int width = info["width"].GetInt();
  int height = info["height"].GetInt();
  double resolution = info["resolution"].GetDouble();

  std::string frame_id = "map";
  if (msg_json.HasMember("header") && msg_json["header"].HasMember("frame_id") &&
      msg_json["header"]["frame_id"].IsString()) {
    frame_id = NormalizeFrameId(msg_json["header"]["frame_id"].GetString());
  }
  if (frame_id != "map") {
    basic::RobotPose tf_map_from_frame = GetTransform("map", frame_id);
    basic::RobotPose local_origin_pose;
    local_origin_pose.x = origin_x;
    local_origin_pose.y = origin_y;
    local_origin_pose.theta = origin_theta;
    basic::RobotPose map_origin_pose = basic::absoluteSum(tf_map_from_frame, local_origin_pose);
    origin_x = map_origin_pose.x;
    origin_y = map_origin_pose.y;
    origin_theta = map_origin_pose.theta;
  }

  // 创建代价地图
  basic::OccupancyMap cost_map(height, width, Eigen::Vector3d(origin_x, origin_y, 0), resolution);

  // 填充代价地图数据
  const auto& data = msg_json["data"];
  if (data.IsArray()) {
    rapidjson::SizeType max_size = static_cast<rapidjson::SizeType>(width * height);
    for (rapidjson::SizeType i = 0; i < data.Size() && i < max_size; i++) {
      int x = static_cast<int>(i / width);
      int y = static_cast<int>(i % width);
      int8_t value = data[i].GetInt();
      cost_map(x, y) = value;
    }
  }
  cost_map.SetFlip();

  // 将局部代价地图叠加到全局地图上
  basic::OccupancyMap sized_cost_map = occ_map_;
  basic::RobotPose origin_pose;
  origin_pose.x = origin_x;
  origin_pose.y = origin_y + cost_map.heightMap();
  origin_pose.theta = origin_theta;

  // 计算原点在地图坐标系中的位置
  double map_o_x, map_o_y;
  occ_map_.xy2OccPose(origin_pose.x, origin_pose.y, map_o_x, map_o_y);
  sized_cost_map.map_data.setZero();

  // 将局部代价地图数据复制到全局地图对应位置
  for (int x = 0; x < occ_map_.rows; x++)
    for (int y = 0; y < occ_map_.cols; y++) {
      if (x > map_o_x && y > map_o_y && y < map_o_y + cost_map.rows &&
          x < map_o_x + cost_map.cols) {
        sized_cost_map(x, y) = cost_map(x - map_o_x, y - map_o_y);
      } else {
        sized_cost_map(x, y) = 0;
      }
    }
  PUBLISH_LATEST(MSG_ID_LOCAL_COST_MAP, sized_cost_map);
}

/**
 * @brief 全局代价地图回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::GlobalCostMapCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  std::string validation_error;
  if (!validation::ValidateOccupancyGrid(msg_json, &validation_error)) {
    LOG_ERROR("Rejected global costmap: " << validation_error);
    return;
  }
  if (!msg_json.HasMember("info") || !msg_json.HasMember("data")) return;

  const auto& info = msg_json["info"];
  if (!info.HasMember("origin") || !info.HasMember("width") ||
      !info.HasMember("height") || !info.HasMember("resolution")) return;

  // 提取地图元信息
  const auto& origin = info["origin"]["position"];
  double origin_x = origin["x"].GetDouble();
  double origin_y = origin["y"].GetDouble();
  int width = info["width"].GetInt();
  int height = info["height"].GetInt();
  double resolution = info["resolution"].GetDouble();

  // 创建代价地图
  basic::OccupancyMap cost_map(height, width, Eigen::Vector3d(origin_x, origin_y, 0), resolution);

  // 填充地图数据
  const auto& data = msg_json["data"];
  if (data.IsArray()) {
    rapidjson::SizeType max_size = static_cast<rapidjson::SizeType>(width * height);
    for (rapidjson::SizeType i = 0; i < data.Size() && i < max_size; i++) {
      int x = static_cast<int>(i / width);
      int y = static_cast<int>(i % width);
      int8_t value = data[i].GetInt();
      cost_map(x, y) = value;
    }
  }
  cost_map.SetFlip();
  PUBLISH_LATEST(MSG_ID_GLOBAL_COST_MAP, cost_map);
}

/**
 * @brief 激光扫描回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::LaserCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("angle_min") || !msg_json.HasMember("angle_max") ||
      !msg_json.HasMember("angle_increment") || !msg_json.HasMember("ranges")) return;

  // 提取角度参数
  double angle_min = msg_json["angle_min"].GetDouble();
  double angle_increment = msg_json["angle_increment"].GetDouble();

  // 转换激光扫描数据为点云
  basic::LaserScan laser_points;
  const auto& ranges = msg_json["ranges"];
  if (ranges.IsArray()) {
    for (rapidjson::SizeType i = 0; i < ranges.Size(); i++) {
      double dist = ranges[i].GetDouble();
      // 跳过无效距离值
      if (std::isinf(dist)) continue;

      // 计算当前点的角度和坐标
      double angle = angle_min + i * angle_increment;
      double x = dist * std::cos(angle);
      double y = dist * std::sin(angle);

      basic::Point p;
      p.x = x;
      p.y = y;
      laser_points.push_back(p);
    }
  }
  laser_points.id = 0;
  PUBLISH_LATEST(MSG_ID_LASER_SCAN, laser_points);
}

/**
 * @brief 全局路径回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::PathCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("poses")) return;

  // 提取路径点
  basic::RobotPath path;
  const auto& poses = msg_json["poses"];
  if (poses.IsArray()) {
    for (rapidjson::SizeType i = 0; i < poses.Size(); i++) {
      const auto& pose = poses[i]["pose"]["position"];
      basic::Point point;
      point.x = pose["x"].GetDouble();
      point.y = pose["y"].GetDouble();
      path.push_back(point);
    }
  }
  PUBLISH_LATEST(MSG_ID_GLOBAL_PATH, path);
}

/**
 * @brief 局部路径回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::LocalPathCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("poses")) return;

  std::string frame_id = "map";
  if (msg_json.HasMember("header") && msg_json["header"].HasMember("frame_id") &&
      msg_json["header"]["frame_id"].IsString()) {
    frame_id = NormalizeFrameId(msg_json["header"]["frame_id"].GetString());
  }
  const bool need_tf = (frame_id != "map");
  basic::RobotPose tf_map_from_frame;
  if (need_tf) {
    tf_map_from_frame = GetTransform("map", frame_id);
  }

  // 提取路径点
  basic::RobotPath path;
  const auto& poses = msg_json["poses"];
  if (poses.IsArray()) {
    for (rapidjson::SizeType i = 0; i < poses.Size(); i++) {
      const auto& pose = poses[i]["pose"]["position"];
      basic::Point point;
      point.x = pose["x"].GetDouble();
      point.y = pose["y"].GetDouble();
      if (need_tf) {
        point = basic::absoluteSum(tf_map_from_frame, point);
      }
      path.push_back(point);
    }
  }
  PUBLISH_LATEST(MSG_ID_LOCAL_PATH, path);
}

/**
 * @brief 电池状态回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::BatteryCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  std::map<std::string, std::string> map;

  // 提取电池百分比和电压
  if (msg_json.HasMember("percentage")) {
    map["percent"] = std::to_string(msg_json["percentage"].GetDouble());
  }
  if (msg_json.HasMember("voltage")) {
    map["voltage"] = std::to_string(msg_json["voltage"].GetDouble());
  }
  PUBLISH(MSG_ID_BATTERY_STATE, map);
}

/**
 * @brief DHT11 温度回调
 */
void RosbridgeComm::Dht11TempCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& j = msg.msg_json_;
  if (j.HasMember("data")) {
    double temp = j["data"].GetDouble();
    PUBLISH(MSG_ID_DHT11_TEMP, temp);
  }
}

/**
 * @brief DHT11 湿度回调
 */
void RosbridgeComm::Dht11HumiCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& j = msg.msg_json_;
  if (j.HasMember("data")) {
    double humi = j["data"].GetDouble();
    PUBLISH(MSG_ID_DHT11_HUMI, humi);
  }
}

/**
 * @brief 语音命令回调
 */
void RosbridgeComm::VoiceCommandCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& j = msg.msg_json_;
  if (j.HasMember("data")) {
    std::string data = j["data"].GetString();
    PUBLISH(MSG_ID_VOICE_COMMAND, data);
  }
}

namespace {

int64_t DiagnosticStampMs(const rapidjson::Value& msg_json) {
  int64_t stamp_ms = 0;
  if (msg_json.HasMember("header") && msg_json["header"].IsObject()) {
    const auto& header = msg_json["header"];
    if (header.HasMember("stamp") && header["stamp"].IsObject()) {
      const auto& st = header["stamp"];
      int64_t sec = 0;
      if (st.HasMember("sec")) {
        const auto& sv = st["sec"];
        if (sv.IsInt64()) {
          sec = sv.GetInt64();
        } else if (sv.IsInt()) {
          sec = sv.GetInt();
        } else if (sv.IsUint()) {
          sec = static_cast<int64_t>(sv.GetUint());
        }
      }
      int64_t nsec = 0;
      if (st.HasMember("nanosec")) {
        const auto& nv = st["nanosec"];
        if (nv.IsInt64()) {
          nsec = nv.GetInt64();
        } else if (nv.IsInt()) {
          nsec = nv.GetInt();
        } else if (nv.IsUint()) {
          nsec = static_cast<int64_t>(nv.GetUint());
        }
      } else if (st.HasMember("nsec")) {
        const auto& nv = st["nsec"];
        if (nv.IsInt64()) {
          nsec = nv.GetInt64();
        } else if (nv.IsInt()) {
          nsec = nv.GetInt();
        } else if (nv.IsUint()) {
          nsec = static_cast<int64_t>(nv.GetUint());
        }
      }
      stamp_ms = sec * 1000 + nsec / 1000000;
    }
  }
  if (stamp_ms <= 0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }
  return stamp_ms;
}

int DiagnosticLevelFromJson(const rapidjson::Value& v) {
  if (v.IsInt()) {
    return v.GetInt();
  }
  if (v.IsUint()) {
    return static_cast<int>(v.GetUint());
  }
  if (v.IsInt64()) {
    return static_cast<int>(v.GetInt64());
  }
  return 0;
}

}  // namespace

void RosbridgeComm::DiagnosticCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) {
    return;
  }
  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("status") || !msg_json["status"].IsArray()) {
    return;
  }
  const int64_t stamp_ms = DiagnosticStampMs(msg_json);
  basic::DiagnosticSnapshot snapshot;
  const auto& arr = msg_json["status"].GetArray();
  for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
    const auto& st = arr[i];
    if (!st.IsObject()) {
      continue;
    }
    int level = 0;
    if (st.HasMember("level")) {
      level = DiagnosticLevelFromJson(st["level"]);
    }
    std::string name;
    if (st.HasMember("name") && st["name"].IsString()) {
      name = st["name"].GetString();
    }
    std::string message;
    if (st.HasMember("message") && st["message"].IsString()) {
      message = st["message"].GetString();
    }
    std::string hardware_id;
    if (st.HasMember("hardware_id") && st["hardware_id"].IsString()) {
      hardware_id = st["hardware_id"].GetString();
    }
    if (hardware_id.empty()) {
      hardware_id = "unknown_hardware";
    }
    basic::DiagnosticComponentState comp;
    comp.level = level;
    comp.message = std::move(message);
    comp.last_update_ms = stamp_ms;
    if (st.HasMember("values") && st["values"].IsArray()) {
      for (const auto& kv : st["values"].GetArray()) {
        if (!kv.IsObject()) {
          continue;
        }
        std::string k;
        std::string vval;
        if (kv.HasMember("key") && kv["key"].IsString()) {
          k = kv["key"].GetString();
        }
        if (kv.HasMember("value") && kv["value"].IsString()) {
          vval = kv["value"].GetString();
        }
        comp.key_values[k] = std::move(vval);
      }
    }
    snapshot.hardware[hardware_id][name] = std::move(comp);
  }
  basic::DiagnosticSnapshot merged;
  {
    std::lock_guard<std::mutex> lock(diagnostic_cache_mutex_);
    for (const auto& hardware : snapshot.hardware) {
      for (const auto& component : hardware.second) {
        diagnostic_snapshot_cache_.hardware[hardware.first][component.first] =
            component.second;
      }
    }
    constexpr int64_t kDiagnosticExpiryMs = 6000;
    for (auto hardware = diagnostic_snapshot_cache_.hardware.begin();
         hardware != diagnostic_snapshot_cache_.hardware.end();) {
      for (auto component = hardware->second.begin();
           component != hardware->second.end();) {
        if (stamp_ms - component->second.last_update_ms >
            kDiagnosticExpiryMs) {
          component = hardware->second.erase(component);
        } else {
          ++component;
        }
      }
      if (hardware->second.empty()) {
        hardware = diagnostic_snapshot_cache_.hardware.erase(hardware);
      } else {
        ++hardware;
      }
    }
    merged = diagnostic_snapshot_cache_;
  }
  PUBLISH(MSG_ID_DIAGNOSTIC, merged);
}

void RosbridgeComm::CommandResponseCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("data") || !msg_json["data"].IsString()) return;
  PUBLISH(MSG_ID_COMMAND_RESPONSE, std::string(msg_json["data"].GetString()));
}

void RosbridgeComm::CommandStatusCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("data") || !msg_json["data"].IsString()) return;
  PUBLISH(MSG_ID_COMMAND_STATUS, std::string(msg_json["data"].GetString()));
}

void RosbridgeComm::StringMessageCallback(const ROSBridgePublishMsg& msg, const MsgId& id) {
  if (msg.msg_json_.IsNull()) return;
  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("data") || !msg_json["data"].IsString()) return;
  PUBLISH(ToString(id), std::string(msg_json["data"].GetString()));
}

void RosbridgeComm::AutoExploreStatusCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;
  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("data") || !msg_json["data"].IsString()) return;
  PUBLISH(MSG_ID_AUTO_EXPLORE_STATUS, std::string(msg_json["data"].GetString()));
}

/**
 * @brief 里程计回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::OdomCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("pose") || !msg_json.HasMember("twist")) return;

  basic::RobotState state;

  // 提取速度信息
  const auto& twist = msg_json["twist"]["twist"];
  state.vx = twist.HasMember("linear") ? twist["linear"]["x"].GetDouble() : 0.0;
  state.vy = twist.HasMember("linear") ? twist["linear"]["y"].GetDouble() : 0.0;
  state.w = twist.HasMember("angular") ? twist["angular"]["z"].GetDouble() : 0.0;

  // 提取位置信息
  const auto& pose = msg_json["pose"]["pose"];
  state.x = pose["position"]["x"].GetDouble();
  state.y = pose["position"]["y"].GetDouble();

  // 提取方向信息（四元数转欧拉角）
  if (pose.HasMember("orientation")) {
    const auto& orientation = pose["orientation"];
    double qx = orientation.HasMember("x") ? orientation["x"].GetDouble() : 0.0;
    double qy = orientation.HasMember("y") ? orientation["y"].GetDouble() : 0.0;
    double qz = orientation.HasMember("z") ? orientation["z"].GetDouble() : 0.0;
    double qw = orientation.HasMember("w") ? orientation["w"].GetDouble() : 1.0;
    state.theta = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));
  }

  PUBLISH(MSG_ID_ODOM_POSE, state);
}

void RosbridgeComm::LocalizationPoseCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull() || !msg.msg_json_.HasMember("pose")) return;
  const auto& pose_with_covariance = msg.msg_json_["pose"];
  if (!pose_with_covariance.IsObject() ||
      !pose_with_covariance.HasMember("pose")) return;
  const auto& pose = pose_with_covariance["pose"];
  if (!pose.IsObject() || !pose.HasMember("position") ||
      !pose.HasMember("orientation")) return;
  const auto& position = pose["position"];
  const auto& orientation = pose["orientation"];
  if (!position.IsObject() || !orientation.IsObject() ||
      !position.HasMember("x") || !position["x"].IsNumber() ||
      !position.HasMember("y") || !position["y"].IsNumber()) return;

  const auto number_or = [](const rapidjson::Value& object, const char* key,
                            double fallback) {
    return object.HasMember(key) && object[key].IsNumber()
               ? object[key].GetDouble()
               : fallback;
  };

  LocalizationEstimate estimate;
  estimate.pose.x = position["x"].GetDouble();
  estimate.pose.y = position["y"].GetDouble();
  const double qx = number_or(orientation, "x", 0.0);
  const double qy = number_or(orientation, "y", 0.0);
  const double qz = number_or(orientation, "z", 0.0);
  const double qw = number_or(orientation, "w", 1.0);
  estimate.pose.theta = std::atan2(2.0 * (qw * qz + qx * qy),
                                   1.0 - 2.0 * (qy * qy + qz * qz));
  if (pose_with_covariance.HasMember("covariance") &&
      pose_with_covariance["covariance"].IsArray()) {
    const auto& covariance = pose_with_covariance["covariance"];
    if (covariance.Size() >= 36 && covariance[0].IsNumber() &&
        covariance[7].IsNumber() && covariance[35].IsNumber()) {
      estimate.xy_variance = std::max(covariance[0].GetDouble(),
                                      covariance[7].GetDouble());
      estimate.yaw_variance = covariance[35].GetDouble();
    } else {
      estimate.xy_variance = std::numeric_limits<double>::infinity();
      estimate.yaw_variance = std::numeric_limits<double>::infinity();
    }
  } else {
    estimate.xy_variance = std::numeric_limits<double>::infinity();
    estimate.yaw_variance = std::numeric_limits<double>::infinity();
  }
  PUBLISH(MSG_ID_LOCALIZATION_POSE, estimate);
}

/**
 * @brief 机器人足迹回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::RobotFootprintCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  if (!msg_json.HasMember("polygon") || !msg_json["polygon"].HasMember("points")) return;

  std::string frame_id = "map";
  if (msg_json.HasMember("header") && msg_json["header"].HasMember("frame_id") &&
      msg_json["header"]["frame_id"].IsString()) {
    frame_id = NormalizeFrameId(msg_json["header"]["frame_id"].GetString());
  }
  const bool need_tf = (frame_id != "map");
  basic::RobotPose tf_map_from_frame;
  if (need_tf) {
    tf_map_from_frame = GetTransform("map", frame_id);
  }

  // 提取机器人足迹多边形点
  basic::RobotPath footprint;
  const auto& points = msg_json["polygon"]["points"];
  if (points.IsArray()) {
    for (rapidjson::SizeType i = 0; i < points.Size(); i++) {
      basic::Point p;
      p.x = points[i]["x"].GetDouble();
      p.y = points[i]["y"].GetDouble();
      if (need_tf) {
        p = basic::absoluteSum(tf_map_from_frame, p);
      }
      footprint.push_back(p);
    }
  }
  PUBLISH(MSG_ID_ROBOT_FOOTPRINT, footprint);
}

/**
 * @brief 拓扑地图回调函数
 * @param msg ROSBridge消息
 */
void RosbridgeComm::TopologyMapCallback(const ROSBridgePublishMsg& msg) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  TopologyMap topology_map;

  // 提取地图名称
  if (msg_json.HasMember("map_name")) {
    topology_map.map_name = msg_json["map_name"].GetString();
  }

  // 提取地图属性
  if (msg_json.HasMember("map_property")) {
    const auto& map_property = msg_json["map_property"];

    // 提取支持的控制器列表
    if (map_property.HasMember("support_controllers")) {
      const auto& controllers = map_property["support_controllers"];
      if (controllers.IsArray()) {
        for (rapidjson::SizeType i = 0; i < controllers.Size(); i++) {
          std::string controller = controllers[i].GetString();
          if (std::find(topology_map.map_property.support_controllers.begin(),
                        topology_map.map_property.support_controllers.end(),
                        controller) == topology_map.map_property.support_controllers.end()) {
            topology_map.map_property.support_controllers.push_back(controller);
          }
        }
      }
    }

    // 提取支持的目标检查器列表
    if (map_property.HasMember("support_goal_checkers")) {
      const auto& goal_checkers = map_property["support_goal_checkers"];
      if (goal_checkers.IsArray()) {
        for (rapidjson::SizeType i = 0; i < goal_checkers.Size(); i++) {
          std::string goal_checker = goal_checkers[i].GetString();
          if (std::find(topology_map.map_property.support_goal_checkers.begin(),
                        topology_map.map_property.support_goal_checkers.end(),
                        goal_checker) == topology_map.map_property.support_goal_checkers.end()) {
            topology_map.map_property.support_goal_checkers.push_back(goal_checker);
          }
        }
      }
    }
  }

  // 提取拓扑点信息
  if (msg_json.HasMember("points")) {
    const auto& points = msg_json["points"];
    if (points.IsArray()) {
      for (rapidjson::SizeType i = 0; i < points.Size(); i++) {
        const auto& point_msg = points[i];
        TopologyMap::PointInfo point_info;
        point_info.name = point_msg["name"].GetString();
        point_info.x = point_msg["x"].GetDouble();
        point_info.y = point_msg["y"].GetDouble();
        point_info.theta = point_msg["theta"].GetDouble();
        point_info.type = static_cast<PointType>(point_msg["type"].GetInt());
        topology_map.points.push_back(point_info);
      }
    }
  }

  // 提取路由信息
  if (msg_json.HasMember("routes")) {
    const auto& routes = msg_json["routes"];
    if (routes.IsArray()) {
      for (rapidjson::SizeType i = 0; i < routes.Size(); i++) {
        const auto& route_msg = routes[i];
        TopologyMap::RouteInfo route_info;
        route_info.controller = route_msg["route_info"]["controller"].GetString();
        route_info.speed_limit = route_msg["route_info"]["speed_limit"].GetDouble();
        route_info.goal_checker = route_msg["route_info"]["goal_checker"].GetString();
        topology_map.routes[route_msg["from_point"].GetString()][route_msg["to_point"].GetString()] = route_info;
      }
    }
  }

  LOG_INFO("recv topology map:" << topology_map.map_name);
  PUBLISH(MSG_ID_TOPOLOGY_MAP, topology_map);
}

/**
 * @brief 图像回调函数，处理图像消息
 * @param msg ROSBridge消息
 * @param location 图像位置标识
 */
void RosbridgeComm::ImageCallback(const ROSBridgePublishMsg& msg, const std::string& location) {
  if (msg.msg_json_.IsNull()) return;

  const auto& msg_json = msg.msg_json_;
  std::string error;
  if (!validation::ValidateImageMetadata(msg_json, &error)) {
    LOG_ERROR("Rejected image metadata: " << error);
    return;
  }

  ImageJob job;
  job.compressed = msg_json.HasMember("format") && !msg_json.HasMember("encoding");
  if (!job.compressed) {
    job.encoding.assign(msg_json["encoding"].GetString(),
                        msg_json["encoding"].GetStringLength());
    job.width = msg_json["width"].GetUint();
    job.height = msg_json["height"].GetUint();
    job.step = msg_json["step"].GetUint();
  }
  const auto& data = msg_json["data"];
  if (data.IsString()) {
    job.base64_encoded = true;
    job.encoded_data.assign(data.GetString(), data.GetStringLength());
  } else {
    job.bytes.reserve(data.Size());
    for (rapidjson::SizeType i = 0; i < data.Size(); ++i) {
      if (!data[i].IsUint() || data[i].GetUint() > 255U) {
        LOG_ERROR("Rejected image byte array value");
        return;
      }
      job.bytes.push_back(static_cast<uint8_t>(data[i].GetUint()));
    }
  }
  image_jobs_.Push(location, std::move(job));
}

void RosbridgeComm::ImageWorkerLoop() {
  std::string location;
  ImageJob job;
  while (image_jobs_.WaitPop(&location, &job)) {
    std::vector<uint8_t> data = job.base64_encoded
                                    ? DecodeBase64(job.encoded_data.data(),
                                                   job.encoded_data.size())
                                    : std::move(job.bytes);
    if (data.empty() || data.size() > validation::kMaxImageBytes) continue;

    cv::Mat converted;
    if (job.compressed) {
      cv::Mat decoded = cv::imdecode(data, cv::IMREAD_COLOR);
      if (decoded.empty()) continue;
      if (decoded.cols <= 0 || decoded.rows <= 0 ||
          decoded.cols > static_cast<int>(validation::kMaxImageDimension) ||
          decoded.rows > static_cast<int>(validation::kMaxImageDimension) ||
          decoded.total() > validation::kMaxImageBytes / 3U) {
        LOG_ERROR("Rejected decoded compressed image dimensions");
        continue;
      }
      cv::cvtColor(decoded, converted, cv::COLOR_BGR2RGB);
    } else {
      std::size_t expected = 0;
      if (!validation::CheckedMultiply(job.height, job.step, &expected) ||
          data.size() != expected) {
        LOG_ERROR("Rejected decoded image size");
        continue;
      }
      const int rows = static_cast<int>(job.height);
      const int cols = static_cast<int>(job.width);
      if (job.encoding == "rgb8" || job.encoding == "RGB8") {
        converted = cv::Mat(rows, cols, CV_8UC3, data.data(), job.step).clone();
      } else if (job.encoding == "bgr8" || job.encoding == "BGR8" ||
                 job.encoding == "CV_8UC3") {
        cv::Mat input(rows, cols, CV_8UC3, data.data(), job.step);
        cv::cvtColor(input, converted, cv::COLOR_BGR2RGB);
      } else if (job.encoding == "8UC1" || job.encoding == "mono8") {
        cv::Mat input(rows, cols, CV_8UC1, data.data(), job.step);
        cv::cvtColor(input, converted, cv::COLOR_GRAY2RGB);
      } else if (job.encoding == "16UC1") {
        cv::Mat input(rows, cols, CV_16UC1, data.data(), job.step);
        cv::Mat scaled;
        input.convertTo(scaled, CV_8UC1, 255.0 / 10000.0);
        cv::cvtColor(scaled, converted, cv::COLOR_GRAY2RGB);
      } else if (job.encoding == "32FC1") {
        cv::Mat input(rows, cols, CV_32FC1, data.data(), job.step);
        cv::Mat scaled;
        input.convertTo(scaled, CV_8UC1, 255.0 / 10.0);
        cv::cvtColor(scaled, converted, cv::COLOR_GRAY2RGB);
      }
    }
    if (!converted.empty()) {
      PUBLISH_LATEST(
          MSG_ID_IMAGE,
          (std::pair<std::string, std::shared_ptr<cv::Mat>>(
              location, std::make_shared<cv::Mat>(std::move(converted)))));
    }
  }
}

/**
 * @brief 发布重定位位姿
 * @param pose 机器人位姿
 */
void RosbridgeComm::PubRelocPose(const basic::RobotPose& pose) {
  rapidjson::Document msg;
  msg.SetObject();
  auto& allocator = msg.GetAllocator();

  // 构建消息头
  rapidjson::Value header(rapidjson::kObjectType);
  header.AddMember("frame_id", rapidjson::Value("map", allocator), allocator);

  // 设置当前时间戳
  ROSTime now = ROSTime::now();
  rapidjson::Value stamp(rapidjson::kObjectType);
  stamp.AddMember("secs", static_cast<uint64_t>(now.sec_), allocator);
  stamp.AddMember("nsecs", static_cast<uint64_t>(now.nsec_), allocator);
  header.AddMember("stamp", stamp, allocator);

  msg.AddMember("header", header, allocator);

  // 构建位姿信息
  rapidjson::Value pose_value(rapidjson::kObjectType);

  // 位置
  rapidjson::Value position(rapidjson::kObjectType);
  position.AddMember("x", pose.x, allocator);
  position.AddMember("y", pose.y, allocator);
  position.AddMember("z", 0.0, allocator);
  pose_value.AddMember("position", position, allocator);

  // 方向（四元数）
  rapidjson::Value orientation(rapidjson::kObjectType);
  double qw = std::cos(pose.theta / 2.0);
  double qz = std::sin(pose.theta / 2.0);
  orientation.AddMember("x", 0.0, allocator);
  orientation.AddMember("y", 0.0, allocator);
  orientation.AddMember("z", qz, allocator);
  orientation.AddMember("w", qw, allocator);
  pose_value.AddMember("orientation", orientation, allocator);

  // 协方差矩阵（36个元素）
  rapidjson::Value covariance(rapidjson::kArrayType);
  for (int i = 0; i < 36; i++) {
    covariance.PushBack(0.0, allocator);
  }
  covariance[0].SetDouble(0.25);
  covariance[7].SetDouble(0.25);
  covariance[35].SetDouble(0.06853891945200942);  // (15 deg)^2

  rapidjson::Value pose_with_covariance(rapidjson::kObjectType);
  pose_with_covariance.AddMember("pose", pose_value, allocator);
  pose_with_covariance.AddMember("covariance", covariance, allocator);

  msg.AddMember("pose", pose_with_covariance, allocator);

  // 发布消息
  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  auto it = publishers_.find(GET_TOPIC_NAME(MSG_ID_SET_RELOC_POSE));
  if (it != publishers_.end()) {
    it->second->Publish(msg);
  }
}

/**
 * @brief 发布导航目标点
 * @param pose 目标位姿
 */
void RosbridgeComm::PubNavGoal(const basic::RobotPose& pose) {
  rapidjson::Document msg;
  msg.SetObject();
  auto& allocator = msg.GetAllocator();

  // 构建消息头
  rapidjson::Value header(rapidjson::kObjectType);
  header.AddMember("frame_id", rapidjson::Value("map", allocator), allocator);

  // 设置当前时间戳
  ROSTime now = ROSTime::now();
  rapidjson::Value stamp(rapidjson::kObjectType);
  stamp.AddMember("secs", static_cast<uint64_t>(now.sec_), allocator);
  stamp.AddMember("nsecs", static_cast<uint64_t>(now.nsec_), allocator);
  header.AddMember("stamp", stamp, allocator);

  msg.AddMember("header", header, allocator);

  // 构建位姿信息
  rapidjson::Value pose_value(rapidjson::kObjectType);

  // 位置
  rapidjson::Value position(rapidjson::kObjectType);
  position.AddMember("x", pose.x, allocator);
  position.AddMember("y", pose.y, allocator);
  position.AddMember("z", 0.0, allocator);
  pose_value.AddMember("position", position, allocator);

  // 方向（四元数）
  rapidjson::Value orientation(rapidjson::kObjectType);
  double qw = std::cos(pose.theta / 2.0);
  double qz = std::sin(pose.theta / 2.0);
  orientation.AddMember("x", 0.0, allocator);
  orientation.AddMember("y", 0.0, allocator);
  orientation.AddMember("z", qz, allocator);
  orientation.AddMember("w", qw, allocator);
  pose_value.AddMember("orientation", orientation, allocator);

  msg.AddMember("pose", pose_value, allocator);

  // 发布消息
  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  auto it = publishers_.find(GET_TOPIC_NAME(DISPLAY_GOAL));
  if (it != publishers_.end()) {
    it->second->Publish(msg);
  }
}

/**
 * @brief 发布机器人速度
 * @param speed 速度信息
 */
void RosbridgeComm::PubRobotSpeed(const basic::RobotSpeed& speed) {
  rapidjson::Document msg;
  msg.SetObject();
  auto& allocator = msg.GetAllocator();

  // 线速度
  rapidjson::Value linear(rapidjson::kObjectType);
  linear.AddMember("x", speed.vx, allocator);
  linear.AddMember("y", speed.vy, allocator);
  linear.AddMember("z", 0.0, allocator);

  // 角速度
  rapidjson::Value angular(rapidjson::kObjectType);
  angular.AddMember("x", 0.0, allocator);
  angular.AddMember("y", 0.0, allocator);
  angular.AddMember("z", speed.w, allocator);

  msg.AddMember("linear", linear, allocator);
  msg.AddMember("angular", angular, allocator);

  // 发布消息
  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  auto it = publishers_.find(GET_TOPIC_NAME(MSG_ID_SET_ROBOT_SPEED));
  if (it != publishers_.end()) {
    it->second->Publish(msg);
  }
}

/**
 * @brief 发布 Eggy 命令中心 JSON 请求
 */
void RosbridgeComm::PubCommandRequest(const std::string& json_request) {
  PubStringRequest(MsgId::kCommandRequest, json_request);
}

bool RosbridgeComm::PubStringRequest(const MsgId& id, const std::string& json_request) {
  rapidjson::Document msg;
  msg.SetObject();
  auto& allocator = msg.GetAllocator();
  msg.AddMember("data", rapidjson::Value(json_request.c_str(), allocator), allocator);

  bool success = false;
  {
    std::lock_guard<std::mutex> transport_lock(transport_mutex_);
    auto it = publishers_.find(GET_TOPIC_NAME(ToString(id)));
    if (it != publishers_.end()) {
      success = it->second->Publish(msg);
    }
  }
  basic::ChannelPublishResult result;
  result.message_id = ToString(id);
  rapidjson::Document request;
  request.Parse(json_request.c_str(), json_request.size());
  if (!request.HasParseError() && request.IsObject() &&
      request.HasMember("request_id") && request["request_id"].IsString()) {
    result.request_id.assign(request["request_id"].GetString(),
                             request["request_id"].GetStringLength());
  }
  result.success = success;
  result.message = success ? "published" : "ROS topic is unavailable or disconnected";
  PUBLISH(MSG_ID_CHANNEL_PUBLISH_RESULT, result);
  return success;
}

/**
 * @brief 发布拓扑地图更新
 * @param topology_map 拓扑地图数据
 */
void RosbridgeComm::PubTopologyMapUpdate(const TopologyMap& topology_map) {
  rapidjson::Document msg;
  msg.SetObject();
  auto& allocator = msg.GetAllocator();

  // 地图名称
  msg.AddMember("map_name", rapidjson::Value(topology_map.map_name.c_str(), allocator), allocator);

  // 地图属性
  rapidjson::Value map_property(rapidjson::kObjectType);

  // 支持的控制器列表
  rapidjson::Value support_controllers(rapidjson::kArrayType);
  for (const auto& controller : topology_map.map_property.support_controllers) {
    support_controllers.PushBack(rapidjson::Value(controller.c_str(), allocator), allocator);
  }
  map_property.AddMember("support_controllers", support_controllers, allocator);

  // 支持的目标检查器列表
  rapidjson::Value support_goal_checkers(rapidjson::kArrayType);
  for (const auto& goal_checker : topology_map.map_property.support_goal_checkers) {
    support_goal_checkers.PushBack(rapidjson::Value(goal_checker.c_str(), allocator), allocator);
  }
  map_property.AddMember("support_goal_checkers", support_goal_checkers, allocator);

  msg.AddMember("map_property", map_property, allocator);

  // 拓扑点列表
  rapidjson::Value points(rapidjson::kArrayType);
  for (const auto& point : topology_map.points) {
    rapidjson::Value point_value(rapidjson::kObjectType);
    point_value.AddMember("name", rapidjson::Value(point.name.c_str(), allocator), allocator);
    point_value.AddMember("x", point.x, allocator);
    point_value.AddMember("y", point.y, allocator);
    point_value.AddMember("theta", point.theta, allocator);
    point_value.AddMember("type", static_cast<int>(point.type), allocator);
    points.PushBack(point_value, allocator);
  }
  msg.AddMember("points", points, allocator);

  // 路由信息
  rapidjson::Value routes(rapidjson::kArrayType);
  for (const auto& from_routes : topology_map.routes) {
    for (const auto& route : from_routes.second) {
      rapidjson::Value route_value(rapidjson::kObjectType);
      route_value.AddMember("from_point", rapidjson::Value(from_routes.first.c_str(), allocator), allocator);
      route_value.AddMember("to_point", rapidjson::Value(route.first.c_str(), allocator), allocator);

      rapidjson::Value route_info(rapidjson::kObjectType);
      route_info.AddMember("controller", rapidjson::Value(route.second.controller.c_str(), allocator), allocator);
      route_info.AddMember("speed_limit", route.second.speed_limit, allocator);
      route_info.AddMember("goal_checker", rapidjson::Value(route.second.goal_checker.c_str(), allocator), allocator);
      route_value.AddMember("route_info", route_info, allocator);

      routes.PushBack(route_value, allocator);
    }
  }
  msg.AddMember("routes", routes, allocator);

  // 发布消息
  std::lock_guard<std::mutex> transport_lock(transport_mutex_);
  auto it = publishers_.find(GET_TOPIC_NAME(MSG_ID_TOPOLOGY_MAP_UPDATE));
  if (it != publishers_.end()) {
    it->second->Publish(msg);
  }
}


