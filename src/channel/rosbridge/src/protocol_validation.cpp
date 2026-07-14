#include "protocol_validation.h"

#include <array>
#include <limits>
#include <string_view>

namespace rosbridge2cpp::validation {
namespace {

bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}

bool IsBoundedString(const rapidjson::Value& value, std::size_t maximum) {
  return value.IsString() && value.GetStringLength() > 0 &&
         value.GetStringLength() <= maximum;
}

bool IsKnownOperation(std::string_view operation) {
  constexpr std::array<std::string_view, 14> kOperations{
      "fragment", "png", "set_level", "status", "auth", "advertise",
      "unadvertise", "publish", "subscribe", "unsubscribe",
      "advertise_service", "unadvertise_service", "call_service",
      "service_response"};
  for (const auto candidate : kOperations) {
    if (operation == candidate) return true;
  }
  return false;
}

unsigned BytesPerPixel(std::string_view encoding) {
  if (encoding == "rgb8" || encoding == "RGB8" || encoding == "bgr8" ||
      encoding == "BGR8" || encoding == "CV_8UC3") {
    return 3;
  }
  if (encoding == "8UC1" || encoding == "mono8") return 1;
  if (encoding == "16UC1") return 2;
  if (encoding == "32FC1") return 4;
  return 0;
}

bool ValidateDataField(const rapidjson::Value& data, std::string* error) {
  if (data.IsArray()) {
    if (data.Size() > kMaxImageBytes) return Fail(error, "image array too large");
    return true;
  }
  if (data.IsString()) {
    const std::size_t max_base64 = ((kMaxImageBytes + 2U) / 3U) * 4U;
    if (data.GetStringLength() > max_base64) {
      return Fail(error, "encoded image too large");
    }
    return true;
  }
  return Fail(error, "image data must be an array or string");
}

}  // namespace

bool CheckedMultiply(std::size_t left, std::size_t right,
                     std::size_t* result) {
  if (!result) return false;
  if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
    return false;
  }
  *result = left * right;
  return true;
}

bool ValidateEnvelope(const rapidjson::Document& document,
                      std::string* error) {
  if (!document.IsObject()) return Fail(error, "envelope must be an object");
  if (!document.HasMember("op") ||
      !IsBoundedString(document["op"], kMaxOpLength)) {
    return Fail(error, "op must be a bounded string");
  }
  const std::string_view operation(document["op"].GetString(),
                                   document["op"].GetStringLength());
  if (!IsKnownOperation(operation)) return Fail(error, "unknown op");
  if (document.HasMember("id") &&
      !IsBoundedString(document["id"], kMaxIdLength)) {
    return Fail(error, "id must be a bounded string");
  }
  if (operation == "publish") {
    if (!document.HasMember("topic") ||
        !IsBoundedString(document["topic"], kMaxTopicLength)) {
      return Fail(error, "publish topic must be a bounded string");
    }
    if (!document.HasMember("msg") || !document["msg"].IsObject()) {
      return Fail(error, "publish msg must be an object");
    }
  }
  return true;
}

bool ValidateOccupancyGrid(const rapidjson::Value& message,
                           std::string* error) {
  if (!message.IsObject() || !message.HasMember("info") ||
      !message["info"].IsObject() || !message.HasMember("data") ||
      !message["data"].IsArray()) {
    return Fail(error, "invalid OccupancyGrid structure");
  }
  const auto& info = message["info"];
  if (!info.HasMember("width") || !info["width"].IsUint() ||
      !info.HasMember("height") || !info["height"].IsUint() ||
      !info.HasMember("resolution") || !info["resolution"].IsNumber() ||
      info["resolution"].GetDouble() <= 0.0 || !info.HasMember("origin") ||
      !info["origin"].IsObject()) {
    return Fail(error, "invalid OccupancyGrid metadata");
  }
  const auto& origin = info["origin"];
  if (!origin.HasMember("position") || !origin["position"].IsObject() ||
      !origin["position"].HasMember("x") ||
      !origin["position"]["x"].IsNumber() ||
      !origin["position"].HasMember("y") ||
      !origin["position"]["y"].IsNumber()) {
    return Fail(error, "invalid OccupancyGrid origin");
  }
  std::size_t cells = 0;
  if (!CheckedMultiply(info["width"].GetUint(), info["height"].GetUint(),
                       &cells) ||
      cells > kMaxGridCells) {
    return Fail(error, "OccupancyGrid dimensions exceed limit");
  }
  if (message["data"].Size() != cells) {
    return Fail(error, "OccupancyGrid data length mismatch");
  }
  for (const auto& cell : message["data"].GetArray()) {
    if (!cell.IsInt() || cell.GetInt() < -1 || cell.GetInt() > 100) {
      return Fail(error, "invalid OccupancyGrid cell");
    }
  }
  return true;
}

bool ValidateImageMetadata(const rapidjson::Value& message,
                           std::string* error) {
  if (!message.IsObject() || !message.HasMember("data") ||
      !ValidateDataField(message["data"], error)) {
    return false;
  }
  if (message.HasMember("format") && !message.HasMember("encoding")) {
    if (!IsBoundedString(message["format"], 64U)) {
      return Fail(error, "compressed image format is invalid");
    }
    return true;
  }
  if (!message.HasMember("encoding") || !message["encoding"].IsString() ||
      !message.HasMember("width") || !message["width"].IsUint() ||
      !message.HasMember("height") || !message["height"].IsUint() ||
      !message.HasMember("step") || !message["step"].IsUint()) {
    return Fail(error, "raw image metadata is incomplete");
  }
  const unsigned width = message["width"].GetUint();
  const unsigned height = message["height"].GetUint();
  if (width == 0 || height == 0 || width > kMaxImageDimension ||
      height > kMaxImageDimension) {
    return Fail(error, "raw image dimensions exceed limit");
  }
  const std::string_view encoding(message["encoding"].GetString(),
                                  message["encoding"].GetStringLength());
  const unsigned bytes_per_pixel = BytesPerPixel(encoding);
  if (bytes_per_pixel == 0) return Fail(error, "unsupported image encoding");
  std::size_t minimum_step = 0;
  std::size_t bytes = 0;
  if (!CheckedMultiply(width, bytes_per_pixel, &minimum_step) ||
      message["step"].GetUint() < minimum_step ||
      !CheckedMultiply(height, message["step"].GetUint(), &bytes) ||
      bytes > kMaxImageBytes) {
    return Fail(error, "raw image step or size is invalid");
  }
  return true;
}

bool ValidateDecodedImageSize(const rapidjson::Value& message,
                              std::size_t decoded_size,
                              std::string* error) {
  if (!ValidateImageMetadata(message, error)) return false;
  if (message.HasMember("format") && !message.HasMember("encoding")) {
    return decoded_size > 0 && decoded_size <= kMaxImageBytes;
  }
  std::size_t expected = 0;
  if (!CheckedMultiply(message["height"].GetUint(),
                       message["step"].GetUint(), &expected) ||
      decoded_size != expected) {
    return Fail(error, "decoded image length does not match height * step");
  }
  return true;
}

bool IsReceiveFresh(std::chrono::milliseconds last_receive,
                    std::chrono::milliseconds now,
                    std::chrono::milliseconds deadline) {
  return now < last_receive || now - last_receive <= deadline;
}

}  // namespace rosbridge2cpp::validation
