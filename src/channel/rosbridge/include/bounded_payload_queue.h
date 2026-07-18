#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

namespace rosbridge2cpp {

class BoundedPayloadQueue {
 public:
  BoundedPayloadQueue(std::size_t max_messages, std::size_t max_bytes)
      : max_messages_(max_messages), max_bytes_(max_bytes) {}

  bool Push(std::string payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    return PushLocked({}, std::move(payload), false);
  }

  bool PushLatest(std::string key, std::string payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_ || key.empty() || payload.size() > max_bytes_) return false;
    for (auto& entry : payloads_) {
      if (entry.key != key) continue;
      if (bytes_ - entry.payload.size() > max_bytes_ - payload.size()) {
        return false;
      }
      bytes_ = bytes_ - entry.payload.size() + payload.size();
      entry.payload = std::move(payload);
      return true;
    }
    return PushLocked(std::move(key), std::move(payload), false);
  }

  bool PushPriority(std::string payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    return PushLocked({}, std::move(payload), true);
  }

  bool TryPop(std::string* payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    return PopLocked(payload);
  }

  bool WaitPop(std::string* payload) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return closed_ || !payloads_.empty(); });
    return PopLocked(payload);
  }

  void Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    payloads_.clear();
    bytes_ = 0;
    condition_.notify_all();
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = false;
    payloads_.clear();
    bytes_ = 0;
  }

  std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return payloads_.size();
  }

  std::size_t Bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytes_;
  }

 private:
  bool PopLocked(std::string* payload) {
    if (payloads_.empty()) return false;
    bytes_ -= payloads_.front().payload.size();
    *payload = std::move(payloads_.front().payload);
    payloads_.pop_front();
    return true;
  }

  bool PushLocked(std::string key, std::string payload, bool priority) {
    if (closed_ || payload.size() > max_bytes_ ||
        payloads_.size() >= max_messages_ ||
        bytes_ > max_bytes_ - payload.size()) {
      return false;
    }
    bytes_ += payload.size();
    Entry entry{std::move(key), std::move(payload), priority};
    if (priority) {
      const auto first_telemetry =
          std::find_if(payloads_.begin(), payloads_.end(),
                       [](const Entry& queued) { return !queued.priority; });
      payloads_.insert(first_telemetry, std::move(entry));
    } else {
      payloads_.push_back(std::move(entry));
    }
    condition_.notify_one();
    return true;
  }

  struct Entry {
    std::string key;
    std::string payload;
    bool priority{false};
  };

  const std::size_t max_messages_;
  const std::size_t max_bytes_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<Entry> payloads_;
  std::size_t bytes_{0};
  bool closed_{false};
};

}  // namespace rosbridge2cpp
