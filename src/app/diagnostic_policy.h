#pragma once

#include <initializer_list>
#include <utility>

#include <QString>

#include "msg/diagnostic_snapshot.h"

namespace AppContract {

inline QString NormalizeWorkspaceMode(const QString& mode) {
  const QString normalized = mode.trimmed().toLower();
  if (normalized == QStringLiteral("mapping")) {
    return QStringLiteral("mapping_slam");
  }
  if (normalized == QStringLiteral("navigation")) {
    return QStringLiteral("static_nav");
  }
  return normalized;
}

inline bool ContainsAny(const QString& text,
                        std::initializer_list<const char*> needles) {
  for (const char* needle : needles) {
    if (text.contains(QString::fromLatin1(needle), Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

inline bool IsRosRuntimeDiagnostic(const QString& hardware,
                                   const QString& component) {
  const QString combined = hardware + QLatin1Char(' ') + component;
  return ContainsAny(
      combined,
      {"ros", "node", "amcl", "move_base", "planner", "costmap", "slam",
       "gmapping", "cartographer", "map_server", "mission", "inspection",
       "camera", "rknn"});
}

inline bool IsInactiveForMode(const QString& component,
                              const QString& workspace_mode) {
  const QString mode = NormalizeWorkspaceMode(workspace_mode);
  if (mode == QStringLiteral("mapping_slam")) {
    return ContainsAny(component,
                       {"amcl", "move_base", "global_planner",
                        "local_planner", "navigation"});
  }
  if (mode == QStringLiteral("static_nav") ||
      mode == QStringLiteral("inspection")) {
    return ContainsAny(component,
                       {"slam", "gmapping", "cartographer", "hector",
                        "auto_explore", "frontier"});
  }
  return false;
}

inline bool IsAmclConvergenceAdvisory(
    const QString& component, const basic::DiagnosticComponentState& state,
    const QString& workspace_mode) {
  const QString mode = NormalizeWorkspaceMode(workspace_mode);
  if (mode != QStringLiteral("static_nav") &&
      mode != QStringLiteral("inspection")) {
    return false;
  }
  const QString details =
      component + QLatin1Char(' ') + QString::fromStdString(state.message);
  return state.level == 1 && details.contains(QStringLiteral("amcl"),
                                             Qt::CaseInsensitive) &&
         ContainsAny(details, {"standard deviation", "too large"});
}

inline basic::DiagnosticSnapshot AdaptDiagnosticSnapshot(
    const basic::DiagnosticSnapshot& source, const QString& workspace_mode,
    bool profile_switching) {
  basic::DiagnosticSnapshot adapted;
  for (const auto& hardware : source.hardware) {
    for (const auto& component : hardware.second) {
      const QString hardware_name = QString::fromStdString(hardware.first);
      const QString component_name = QString::fromStdString(component.first);
      const auto& state = component.second;

      if (profile_switching &&
          IsRosRuntimeDiagnostic(hardware_name, component_name)) {
        continue;
      }
      if (IsInactiveForMode(component_name, workspace_mode)) {
        continue;
      }

      auto effective_state = state;
      if (IsAmclConvergenceAdvisory(component_name, state, workspace_mode)) {
        effective_state.level = 0;
        effective_state.message =
            "定位尚未收敛，请在地图中重定位或小范围移动后观察";
      }
      adapted.hardware[hardware.first][component.first] =
          std::move(effective_state);
    }
  }
  return adapted;
}

inline int CountDiagnosticAbnormal(
    const basic::DiagnosticSnapshot& snapshot) {
  int abnormal = 0;
  for (const auto& hardware : snapshot.hardware) {
    for (const auto& component : hardware.second) {
      if (component.second.level != 0) {
        ++abnormal;
      }
    }
  }
  return abnormal;
}

}  // namespace AppContract
