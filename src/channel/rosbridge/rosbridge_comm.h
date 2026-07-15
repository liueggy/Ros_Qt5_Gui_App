#ifndef ROSBRIDGE_COMM_H
#define ROSBRIDGE_COMM_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <thread>
#include "algorithm.h"
#include "config/config_manager.h"
#include "core/framework/framework.h"
#include "include/client/socket_websocket_connection.h"
#include "include/messages/rosbridge_publish_msg.h"
#include "include/ros_bridge.h"
#include "include/ros_topic.h"
#include "include/types.h"
#include "include/latest_value_queue.h"
#include "logger/logger.h"
#include "msg/diagnostic_snapshot.h"
#include "msg/channel_publish_result.h"
#include "msg/msg_info.h"
#include "point_type.h"
#include "tf2_rosbridge.h"
#include "virtual_channel_node.h"

using namespace rosbridge2cpp;

class RosbridgeComm : public VirtualChannelNode {
 public:
  RosbridgeComm();
  ~RosbridgeComm() override = default;

 public:
  bool Start() override;
  bool Stop() override;
  void Process() override;
  std::string Name() override { return "ROSBridge"; };
  void PubRelocPose(const basic::RobotPose& pose);
  void PubNavGoal(const basic::RobotPose& pose);
  void PubRobotSpeed(const basic::RobotSpeed& speed);
  void PubEmergencyStop(bool engaged);
  void PubTopologyMapUpdate(const TopologyMap& topology_map);
  void PubCommandRequest(const std::string& json_request);
  bool PubStringRequest(const MsgId& id, const std::string& json_request);

  bool IsConnecting() const override { return connecting_; }
  bool IsConnected() const override { return connected_.load(); }
  bool IsConnectionFailed() const override { return connection_failed_; }
  std::string GetConnectionError() const override {
    std::lock_guard<std::mutex> lock(error_msg_mutex_);
    return connection_error_msg_;
  }
  bool IsReconnecting() const override { return reconnecting_; }

 private:
  void MapCallback(const ROSBridgePublishMsg& msg);
  void LocalCostMapCallback(const ROSBridgePublishMsg& msg);
  void GlobalCostMapCallback(const ROSBridgePublishMsg& msg);
  void LaserCallback(const ROSBridgePublishMsg& msg);
  void PathCallback(const ROSBridgePublishMsg& msg);
  void LocalPathCallback(const ROSBridgePublishMsg& msg);
  void BatteryCallback(const ROSBridgePublishMsg& msg);
  void OdomCallback(const ROSBridgePublishMsg& msg);
  void LocalizationPoseCallback(const ROSBridgePublishMsg& msg);
  void RobotFootprintCallback(const ROSBridgePublishMsg& msg);
  void TopologyMapCallback(const ROSBridgePublishMsg& msg);
  void DiagnosticCallback(const ROSBridgePublishMsg& msg);
  void CommandResponseCallback(const ROSBridgePublishMsg& msg);
  void CommandStatusCallback(const ROSBridgePublishMsg& msg);
  void StringMessageCallback(const ROSBridgePublishMsg& msg, const MsgId& id);
  void AutoExploreStatusCallback(const ROSBridgePublishMsg& msg);
  void Dht11TempCallback(const ROSBridgePublishMsg& msg);
  void Dht11HumiCallback(const ROSBridgePublishMsg& msg);
  void VoiceCommandCallback(const ROSBridgePublishMsg& msg);
  void ImageCallback(const ROSBridgePublishMsg& msg, const std::string& location);
  void ImageWorkerLoop();
  void SetImageStreamVisibility(const std::string& location, bool visible);
  void ApplyImageStreamVisibilityLocked();
  bool IsImageStreamVisible(const std::string& location) const;
  void TfCallback(const ROSBridgePublishMsg& msg);

  basic::RobotPose GetTransform(const std::string& from, const std::string& to);
  void GetRobotPose();

 private:
  std::vector<Framework::ScopedSubscription> message_bus_subscriptions_;
  std::vector<Framework::ScopedSubscription> lifecycle_subscriptions_;
  std::unique_ptr<SocketWebSocketConnection> websocket_connection_;
  std::unique_ptr<ROSBridge> ros_bridge_;

  std::unordered_map<std::string, std::unique_ptr<ROSTopic>> publishers_;
  std::unordered_map<std::string, std::unique_ptr<ROSTopic>> subscribers_;
  std::unordered_map<std::string, ROSCallbackHandle<FunVrROSPublishMsg>> callback_handles_;
  std::unordered_map<std::string, bool> image_stream_visibility_;
  mutable std::mutex image_stream_visibility_mutex_;
  std::atomic_bool image_stream_visibility_dirty_ = {false};
  std::chrono::steady_clock::time_point next_image_subscription_retry_{};
  // Owns the complete transport graph. A ROSBridge references its WebSocket,
  // and every ROSTopic references the ROSBridge, so reads and teardown must be
  // serialized as one unit across connect, reconnect, process and UI threads.
  mutable std::mutex transport_mutex_;

  std::unordered_map<std::string, TransformData> tf_cache_;
  std::mutex tf_cache_mutex_;
  TF2Rosbridge tf2_;
  basic::DiagnosticSnapshot diagnostic_snapshot_cache_;
  std::mutex diagnostic_cache_mutex_;

  basic::OccupancyMap occ_map_;
  basic::RobotPose m_currPose;
  std::atomic_bool init_flag_ = {false};
  std::atomic_bool connecting_ = {false};
  std::atomic_bool connected_ = {false};
  std::atomic_bool connection_failed_ = {false};
  std::string connection_error_msg_;
  mutable std::mutex error_msg_mutex_;
  std::thread connection_thread_;

  std::string rosbridge_ip_;
  int rosbridge_port_;

  void ConnectAsync();
  void ReconnectLoop();
  void CleanupTransportLocked();

  std::atomic_bool reconnect_enabled_ = {true};
  std::atomic_bool reconnecting_ = {false};
  std::thread reconnect_thread_;
  std::mutex reconnect_mutex_;

  struct ImageJob {
    bool compressed{false};
    bool base64_encoded{false};
    std::string encoding;
    unsigned width{0};
    unsigned height{0};
    unsigned step{0};
    std::string encoded_data;
    std::vector<uint8_t> bytes;
  };
  rosbridge2cpp::LatestValueQueue<std::string, ImageJob> image_jobs_{4};
  std::thread image_worker_thread_;
};

#endif  // ROSBRIDGE_COMM_H

