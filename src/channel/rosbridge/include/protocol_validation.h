#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "rapidjson/document.h"

namespace rosbridge2cpp::validation {

inline constexpr std::size_t kMaxEnvelopeBytes = 32U * 1024U * 1024U;
inline constexpr std::size_t kMaxOpLength = 32U;
inline constexpr std::size_t kMaxTopicLength = 256U;
inline constexpr std::size_t kMaxIdLength = 256U;
inline constexpr std::size_t kMaxGridCells = 16U * 1024U * 1024U;
inline constexpr std::size_t kMaxImageBytes = 24U * 1024U * 1024U;
inline constexpr unsigned kMaxImageDimension = 8192U;

bool CheckedMultiply(std::size_t left, std::size_t right,
                     std::size_t* result);
bool ValidateEnvelope(const rapidjson::Document& document,
                      std::string* error);
bool ValidateOccupancyGrid(const rapidjson::Value& message,
                           std::string* error);
bool ValidateImageMetadata(const rapidjson::Value& message,
                           std::string* error);
bool ValidateDecodedImageSize(const rapidjson::Value& message,
                              std::size_t decoded_size,
                              std::string* error);
bool IsReceiveFresh(std::chrono::milliseconds last_receive,
                    std::chrono::milliseconds now,
                    std::chrono::milliseconds deadline);

}  // namespace rosbridge2cpp::validation
