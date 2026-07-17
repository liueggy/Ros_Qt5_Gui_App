#include "app/mission_contract.h"
#include "app/diagnostic_policy.h"

#include <gtest/gtest.h>

namespace {

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

}  // namespace
