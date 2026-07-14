#pragma once

#include <string>

namespace basic {

struct ChannelPublishResult {
  std::string message_id;
  std::string request_id;
  bool success{false};
  std::string message;
};

}  // namespace basic
