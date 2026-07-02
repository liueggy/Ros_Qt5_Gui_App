#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include "topology_map.h"
struct TaskChain {
  std::vector<TopologyMap::PointInfo> points;
  std::map<std::string, std::string> expected_classes;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TaskChain, points, expected_classes);
