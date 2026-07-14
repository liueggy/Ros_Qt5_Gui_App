#pragma once

#include <condition_variable>
#include <cstddef>
#include <iterator>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace rosbridge2cpp {

template <typename Key, typename Value>
class LatestValueQueue {
 public:
  explicit LatestValueQueue(std::size_t capacity) : capacity_(capacity) {}

  bool Push(Key key, Value value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) return false;
    auto found = values_.find(key);
    if (found != values_.end()) {
      found->second.value = std::move(value);
      condition_.notify_one();
      return true;
    }
    if (values_.size() >= capacity_) return false;
    order_.push_back(key);
    auto order_it = std::prev(order_.end());
    values_.emplace(std::move(key), Entry{std::move(value), order_it});
    condition_.notify_one();
    return true;
  }

  bool TryPop(Key* key, Value* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    return PopLocked(key, value);
  }

  bool WaitPop(Key* key, Value* value) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return closed_ || !order_.empty(); });
    return PopLocked(key, value);
  }

  void Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    condition_.notify_all();
  }

  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = false;
    order_.clear();
    values_.clear();
  }

  std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.size();
  }

 private:
  struct Entry {
    Value value;
    typename std::list<Key>::iterator order_it;
  };

  bool PopLocked(Key* key, Value* value) {
    if (order_.empty()) return false;
    auto found = values_.find(order_.front());
    if (found == values_.end()) return false;
    *key = found->first;
    *value = std::move(found->second.value);
    order_.erase(found->second.order_it);
    values_.erase(found);
    return true;
  }

  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::list<Key> order_;
  std::unordered_map<Key, Entry> values_;
  bool closed_{false};
};

}  // namespace rosbridge2cpp
