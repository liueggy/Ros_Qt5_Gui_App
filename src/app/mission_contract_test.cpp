#include "app/mission_contract.h"
#include "app/diagnostic_policy.h"
#include "map/occupancy_map.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <limits>

namespace {

TEST(MapConfigContract, RepairsReversedThresholdsBeforeUpload) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString yaml_path = directory.filePath(QStringLiteral("map.yaml"));
  QFile yaml(yaml_path);
  ASSERT_TRUE(yaml.open(QIODevice::WriteOnly | QIODevice::Text));
  yaml.write("image: ./map.pgm\n"
             "resolution: 0.05\n"
             "origin: [0, 0, 0]\n"
             "negate: 0\n"
             "occupied_thresh: 0.25\n"
             "free_thresh: 0.65\n");
  yaml.close();

  basic::MapConfig config;
  ASSERT_TRUE(config.Load(yaml_path.toStdString()));
  EXPECT_DOUBLE_EQ(config.occupied_thresh,
                   basic::MapConfig::kDefaultOccupiedThresh);
  EXPECT_DOUBLE_EQ(config.free_thresh,
                   basic::MapConfig::kDefaultFreeThresh);
  EXPECT_GT(config.occupied_thresh, config.free_thresh);
  const std::string upload_yaml = config.ToYaml("./map.pgm");
  EXPECT_NE(upload_yaml.find("occupied_thresh: 0.65"), std::string::npos);
  EXPECT_NE(upload_yaml.find("free_thresh: 0.196"), std::string::npos);
}

TEST(MapConfigContract, SavesFreeOccupiedAndUnknownCellsDistinctly) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString base_path = directory.filePath(QStringLiteral("classes"));
  basic::OccupancyMap map(1, 3, Eigen::Vector3d::Zero(), 0.05);
  map(0, 0) = OCC_GRID_FREE;
  map(0, 1) = OCC_GRID_OCCUPIED;
  map(0, 2) = OCC_GRID_UNKNOWN;

  map.Save(base_path.toStdString());

  QFile pgm(base_path + QStringLiteral(".pgm"));
  ASSERT_TRUE(pgm.open(QIODevice::ReadOnly));
  const QByteArray bytes = pgm.readAll();
  ASSERT_GE(bytes.size(), 3);
  EXPECT_EQ(static_cast<unsigned char>(bytes.at(bytes.size() - 3)), 254U);
  EXPECT_EQ(static_cast<unsigned char>(bytes.at(bytes.size() - 2)), 0U);
  EXPECT_EQ(static_cast<unsigned char>(bytes.at(bytes.size() - 1)), 205U);
}

TEST(MissionContractTest, SingleGoalUsesUnifiedMissionEnvelope) {
  const basic::RobotPose pose(1.25, -2.5, 0.75);

  const auto request = AppContract::BuildSingleGoalMission(
      pose, QStringLiteral("qt-goal-fixed"));

  EXPECT_EQ(request.at("schema_version"), 1);
  EXPECT_EQ(request.at("request_id"), "qt-goal-fixed");
  EXPECT_EQ(request.at("command"), "start");
  EXPECT_EQ(request.at("mission_type"), "navigation");
  EXPECT_FALSE(request.at("loop").get<bool>());
  EXPECT_FALSE(request.at("return_home").get<bool>());
  EXPECT_FALSE(request.at("inspection").at("enabled").get<bool>());
  ASSERT_TRUE(request.at("route").is_array());
  ASSERT_EQ(request.at("route").size(), 1U);
  EXPECT_EQ(request.at("route").at(0).at("frame_id"), "map");
  EXPECT_DOUBLE_EQ(request.at("route").at(0).at("x").get<double>(), pose.x);
  EXPECT_DOUBLE_EQ(request.at("route").at(0).at("y").get<double>(), pose.y);
  EXPECT_DOUBLE_EQ(request.at("route").at(0).at("yaw").get<double>(), pose.theta);
}

TEST(MissionContractTest, MissionTrackerCorrelatesAndRecoversFromTimeouts) {
  AppContract::MissionTracker tracker;
  ASSERT_TRUE(tracker.Begin(QStringLiteral("mission-a")));
  EXPECT_EQ(tracker.phase(), AppContract::MissionPhase::AwaitingAcceptance);
  EXPECT_FALSE(tracker.Accept(QStringLiteral("mission-b")));
  EXPECT_TRUE(tracker.Accept(QStringLiteral("mission-a")));
  EXPECT_EQ(tracker.phase(), AppContract::MissionPhase::Running);

  ASSERT_TRUE(tracker.BeginCancellation(QStringLiteral("mission-a")));
  EXPECT_FALSE(tracker.CancelTimedOut(QStringLiteral("mission-b")));
  EXPECT_TRUE(tracker.CancelTimedOut(QStringLiteral("mission-a")));
  EXPECT_EQ(tracker.phase(), AppContract::MissionPhase::Running);

  tracker.Finish(QStringLiteral("mission-a"));
  ASSERT_TRUE(tracker.Begin(QStringLiteral("mission-c")));
  EXPECT_TRUE(tracker.AcceptanceTimedOut(QStringLiteral("mission-c")));
  EXPECT_EQ(tracker.phase(), AppContract::MissionPhase::Idle);
  EXPECT_TRUE(tracker.requestId().isEmpty());
}

TEST(RelocationContractTest, AcceptsCovariancePublishedByQtInitialPose) {
  const basic::RobotPose target(1.0, 2.0, 0.5);
  basic::LocalizationEstimate estimate;
  estimate.pose = target;
  estimate.xy_variance = 0.257;
  estimate.yaw_variance = 0.0686;

  const auto evaluation =
      AppContract::EvaluateRelocationSample(target, estimate);

  EXPECT_TRUE(evaluation.acceptable);
  EXPECT_DOUBLE_EQ(evaluation.distance, 0.0);
  EXPECT_DOUBLE_EQ(evaluation.angle_error, 0.0);
}

TEST(RelocationContractTest, RejectsPositionAndHeadingMismatch) {
  const basic::RobotPose target(1.0, 2.0, 0.5);
  basic::LocalizationEstimate estimate;
  estimate.xy_variance = 0.1;
  estimate.yaw_variance = 0.05;

  estimate.pose = basic::RobotPose(1.36, 2.0, 0.5);
  EXPECT_FALSE(
      AppContract::EvaluateRelocationSample(target, estimate).acceptable);

  estimate.pose = basic::RobotPose(1.0, 2.0, 0.5 + deg2rad(16.0));
  EXPECT_FALSE(
      AppContract::EvaluateRelocationSample(target, estimate).acceptable);
}

TEST(RelocationContractTest, RejectsInvalidOrUncertainEstimate) {
  const basic::RobotPose target(1.0, 2.0, 0.5);
  basic::LocalizationEstimate estimate;
  estimate.pose = target;
  estimate.xy_variance = 0.51;
  estimate.yaw_variance = 0.05;
  EXPECT_FALSE(
      AppContract::EvaluateRelocationSample(target, estimate).acceptable);

  estimate.xy_variance = std::numeric_limits<double>::infinity();
  EXPECT_FALSE(
      AppContract::EvaluateRelocationSample(target, estimate).acceptable);
}

TEST(LaserScanContractTest, PrefersRobotRelativePointsForRelocation) {
  basic::LaserScan scan;
  scan.data.emplace_back(10.0, 20.0);
  scan.robot_relative_data.emplace_back(1.0, 2.0);

  const auto& preview = scan.RelocationPreviewData();

  ASSERT_EQ(preview.size(), 1U);
  EXPECT_DOUBLE_EQ(preview.front().x, 1.0);
  EXPECT_DOUBLE_EQ(preview.front().y, 2.0);
}

TEST(MissionContractTest, ProfileSwitchNeedsMatchingAckAndReadyStatus) {
  AppContract::ProfileSwitchTracker tracker;
  tracker.Begin(QStringLiteral("inspection"), QStringLiteral("profile-1"));

  EXPECT_EQ(tracker.HandleResponse(QStringLiteral("other"), true),
            AppContract::ProfileResponse::Ignored);
  EXPECT_FALSE(tracker.CompleteFromStatus(QStringLiteral("ready"),
                                          QStringLiteral("inspection")));
  EXPECT_EQ(tracker.HandleResponse(QStringLiteral("profile-1"), true),
            AppContract::ProfileResponse::Accepted);
  EXPECT_FALSE(tracker.CompleteFromStatus(QStringLiteral("switching"),
                                          QStringLiteral("inspection")));
  EXPECT_FALSE(tracker.CompleteFromStatus(QStringLiteral("ready"),
                                          QStringLiteral("navigation")));
  EXPECT_TRUE(tracker.CompleteFromStatus(QStringLiteral("ready"),
                                         QStringLiteral("inspection")));
  EXPECT_FALSE(tracker.pending());
}

TEST(MissionContractTest, MissingProfileCapabilitiesFailClosed) {
  const auto missing = AppContract::ParseProfileAvailability(QJsonObject());
  EXPECT_FALSE(missing.mapping);
  EXPECT_FALSE(missing.navigation);
  EXPECT_FALSE(missing.inspection);

  QJsonObject profiles;
  profiles.insert(QStringLiteral("mapping"), true);
  profiles.insert(QStringLiteral("navigation"), false);
  profiles.insert(QStringLiteral("inspection"), true);
  QJsonObject capabilities;
  capabilities.insert(QStringLiteral("profiles"), profiles);
  const auto parsed = AppContract::ParseProfileAvailability(capabilities);
  EXPECT_TRUE(parsed.mapping);
  EXPECT_FALSE(parsed.navigation);
  EXPECT_TRUE(parsed.inspection);
}

TEST(MissionContractTest, MotionOwnerDistinguishesFreshUnknownAndStale) {
  QJsonObject fresh;
  fresh.insert(QStringLiteral("stamp"), 100.0);
  fresh.insert(QStringLiteral("motion_owner"), QStringLiteral("mission_runner"));
  const auto owned = AppContract::ParseMotionOwner(fresh, 102.0, 5.0);
  EXPECT_EQ(owned.state, AppContract::MotionOwnerState::Known);
  EXPECT_EQ(owned.owner, QStringLiteral("mission_runner"));

  QJsonObject unknown;
  unknown.insert(QStringLiteral("stamp"), 100.0);
  EXPECT_EQ(AppContract::ParseMotionOwner(unknown, 102.0, 5.0).state,
            AppContract::MotionOwnerState::Unknown);
  EXPECT_EQ(AppContract::ParseMotionOwner(fresh, 110.0, 5.0).state,
            AppContract::MotionOwnerState::Known);

  fresh.insert(QStringLiteral("stale"), true);
  EXPECT_EQ(AppContract::ParseMotionOwner(fresh, 102.0, 5.0).state,
            AppContract::MotionOwnerState::Stale);

  QJsonObject control_status;
  control_status.insert(QStringLiteral("active_source"),
                        QStringLiteral("teleop"));
  const auto control_owner =
      AppContract::ParseMotionOwner(control_status, 102.0, 5.0);
  EXPECT_EQ(control_owner.state, AppContract::MotionOwnerState::Known);
  EXPECT_EQ(control_owner.owner, QStringLiteral("teleop"));
}

TEST(MissionContractTest, TopologySelectionPreservesOnlyAvailablePoint) {
  const QStringList candidates = {QStringLiteral("A"), QStringLiteral("B")};
  EXPECT_EQ(AppContract::ReconcilePointSelection(QStringLiteral("B"), candidates),
            QStringLiteral("B"));
  EXPECT_TRUE(AppContract::ReconcilePointSelection(QStringLiteral("removed"),
                                                   candidates)
                  .isEmpty());
}

TEST(DiagnosticPolicyTest, ProfileSwitchSuppressesOnlyRosRuntimeDiagnostics) {
  basic::DiagnosticSnapshot snapshot;
  snapshot.hardware["ROS nodes"]["amcl"].level = 2;
  snapshot.hardware["battery"]["voltage"].level = 2;

  const auto adapted = AppContract::AdaptDiagnosticSnapshot(
      snapshot, QStringLiteral("static_nav"), true);

  EXPECT_EQ(adapted.hardware.at("battery").at("voltage").level, 2);
  EXPECT_EQ(adapted.hardware.count("ROS nodes"), 0U);
  EXPECT_EQ(AppContract::CountDiagnosticAbnormal(adapted), 1);
}

TEST(DiagnosticPolicyTest, MappingOmitsInactiveAmclDiagnostic) {
  basic::DiagnosticSnapshot snapshot;
  snapshot.hardware["ROS nodes"]["amcl: Standard deviation"].level = 1;
  snapshot.hardware["ROS nodes"]["amcl: Standard deviation"].message =
      "Too large";

  const auto adapted = AppContract::AdaptDiagnosticSnapshot(
      snapshot, QStringLiteral("mapping_slam"), false);

  EXPECT_TRUE(adapted.hardware.empty());
  EXPECT_EQ(AppContract::CountDiagnosticAbnormal(adapted), 0);
}

TEST(DiagnosticPolicyTest, AmclConvergenceIsGuidanceNotSystemFailure) {
  basic::DiagnosticSnapshot snapshot;
  snapshot.hardware["ROS nodes"]["amcl: Standard deviation"].level = 1;
  snapshot.hardware["ROS nodes"]["amcl: Standard deviation"].message =
      "Too large";

  const auto adapted = AppContract::AdaptDiagnosticSnapshot(
      snapshot, QStringLiteral("static_nav"), false);
  const auto& state =
      adapted.hardware.at("ROS nodes").at("amcl: Standard deviation");

  EXPECT_EQ(state.level, 0);
  EXPECT_NE(state.message.find("定位尚未收敛"), std::string::npos);
  EXPECT_EQ(AppContract::CountDiagnosticAbnormal(adapted), 0);
}

TEST(DiagnosticPolicyTest, ActiveModeKeepsUnrelatedSensorFailure) {
  basic::DiagnosticSnapshot snapshot;
  snapshot.hardware["sensors"]["lidar"].level = 2;
  snapshot.hardware["sensors"]["lidar"].message = "data_stale";

  const auto adapted = AppContract::AdaptDiagnosticSnapshot(
      snapshot, QStringLiteral("static_nav"), false);

  EXPECT_EQ(adapted.hardware.at("sensors").at("lidar").level, 2);
  EXPECT_EQ(AppContract::CountDiagnosticAbnormal(adapted), 1);
}

TEST(TelemetryContract, AutoExploreDoesNotUseADuplicateRosbridgeStream) {
  const QFileInfo test_source(QString::fromUtf8(__FILE__));
  QFile mainwindow(test_source.dir().filePath(QStringLiteral("mainwindow.cpp")));
  ASSERT_TRUE(mainwindow.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray mainwindow_source = mainwindow.readAll();

  QFile rosbridge(test_source.dir().filePath(
      QStringLiteral("../channel/rosbridge/rosbridge_comm.cpp")));
  ASSERT_TRUE(rosbridge.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray rosbridge_source = rosbridge.readAll();

  EXPECT_FALSE(mainwindow_source.contains("MSG_ID_AUTO_EXPLORE_STATUS"));
  EXPECT_FALSE(rosbridge_source.contains("auto_explore_status_topic"));
}

TEST(TelemetryLoggingContract, BoundsLogGrowthAndSkipsMapFrameNoise) {
  const QFileInfo test_source(QString::fromUtf8(__FILE__));
  QFile logger(test_source.dir().filePath(
      QStringLiteral("../common/logger/logger.cc")));
  ASSERT_TRUE(logger.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray logger_source = logger.readAll();

  QFile map_display(test_source.dir().filePath(
      QStringLiteral("display/display_occ_map.cpp")));
  ASSERT_TRUE(map_display.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray map_source = map_display.readAll();

  EXPECT_TRUE(logger_source.contains("MaxLogFileSize, \"5242880\""));
  EXPECT_TRUE(logger_source.contains("StrictLogFileSizeCheck"));
  EXPECT_FALSE(map_source.contains("map update calling:"));
}

TEST(RosbridgeLatencyContract, KeepsInboundWorkOffTheSocketThread) {
  const QFileInfo test_source(QString::fromUtf8(__FILE__));
  QFile socket_source_file(test_source.dir().filePath(QStringLiteral(
      "../channel/rosbridge/src/client/socket_websocket_connection.cpp")));
  ASSERT_TRUE(
      socket_source_file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray socket_source = socket_source_file.readAll();

  QFile bridge_source_file(test_source.dir().filePath(
      QStringLiteral("../channel/rosbridge/src/ros_bridge.cpp")));
  ASSERT_TRUE(
      bridge_source_file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray bridge_source = bridge_source_file.readAll();

  QFile comm_source_file(test_source.dir().filePath(
      QStringLiteral("../channel/rosbridge/rosbridge_comm.cpp")));
  ASSERT_TRUE(comm_source_file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QByteArray comm_source = comm_source_file.readAll();

  EXPECT_TRUE(socket_source.contains("DispatchThreadFunction"));
  EXPECT_TRUE(socket_source.contains("inbound_payloads_.Push(payload)"));
  EXPECT_TRUE(bridge_source.contains("callbacks = found->second"));
  EXPECT_FALSE(comm_source.contains("LOG_INFO(\"recv robot speed:"));
}

}  // namespace
