#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <type_traits>
#include <typeinfo>
#include <optional>
#include "logger/logger.h"

#include "callback_executor.h"

#ifdef QT_CORE_LIB
#include <QObject>
#include <QPointer>
#endif

namespace Framework {

class ScopedSubscription {
 public:
  ScopedSubscription() = default;
  explicit ScopedSubscription(std::function<void()> unsubscribe)
      : unsubscribe_(std::move(unsubscribe)) {}
  ~ScopedSubscription() { Reset(); }

  ScopedSubscription(const ScopedSubscription&) = delete;
  ScopedSubscription& operator=(const ScopedSubscription&) = delete;
  ScopedSubscription(ScopedSubscription&& other) noexcept
      : unsubscribe_(std::move(other.unsubscribe_)) {
    other.unsubscribe_ = {};
  }
  ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
    if (this != &other) {
      Reset();
      unsubscribe_ = std::move(other.unsubscribe_);
      other.unsubscribe_ = {};
    }
    return *this;
  }

  void Reset() {
    if (unsubscribe_) {
      auto unsubscribe = std::move(unsubscribe_);
      unsubscribe();
    }
  }

 private:
  std::function<void()> unsubscribe_;
};
class MessageBus;
}

// 类型特征：提取函数/可调用对象的参数类型
namespace Framework {
namespace detail {
  template<typename T>
  struct function_traits;

  // std::function 特化
  template<typename R, typename Arg>
  struct function_traits<std::function<R(Arg)>> {
    using arg_type = std::decay_t<Arg>;
  };

  // 函数指针特化
  template<typename R, typename Arg>
  struct function_traits<R(*)(Arg)> {
    using arg_type = std::decay_t<Arg>;
  };

  // 成员函数指针特化 (const)
  template<typename T, typename R, typename Arg>
  struct function_traits<R(T::*)(Arg) const> {
    using arg_type = std::decay_t<Arg>;
  };

  // 成员函数指针特化 (非const)
  template<typename T, typename R, typename Arg>
  struct function_traits<R(T::*)(Arg)> {
    using arg_type = std::decay_t<Arg>;
  };

  // lambda 和其他可调用对象：通过 operator() 提取
  template<typename T>
  struct function_traits : function_traits<decltype(&T::operator())> {};

  // 提取 lambda 的参数类型
  template<typename Lambda>
  struct lambda_traits {
    using arg_type = typename function_traits<Lambda>::arg_type;
  };
}
}

#ifdef _WIN32
#define FRAMEWORK_EXPORT __declspec(dllexport)
#else
#define FRAMEWORK_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
FRAMEWORK_EXPORT Framework::MessageBus* GetMessageBusInstance();
}

namespace Framework {

// 类型擦除回调包装器基类
class CallbackBase {
 public:
  virtual ~CallbackBase() = default;
  virtual void call(const void* data, const std::type_info& type) = 0;
  virtual const std::type_info& getType() const = 0;
};

// 类型安全的回调包装器
template<typename T>
class TypedCallback : public CallbackBase {
 public:
  explicit TypedCallback(std::function<void(const T&)> callback)
      : callback_(callback) {}
  
  void call(const void* data, const std::type_info& type) override {
    // 类型匹配检查：如果发布者类型与订阅者类型不匹配，则不调用
    if (typeid(T) == type) {
      callback_(*static_cast<const T*>(data));
    } else {
      LOG_WARN("[TypedCallback::call] Type mismatch: expected " 
                << typeid(T).name() << ", got " << type.name());
    }
  }
  
  const std::type_info& getType() const override {
    return typeid(T);
  }

 private:
  std::function<void(const T&)> callback_;
};


class MessageBus {
 public:
  using CallbackId = size_t;
  
  static MessageBus& Instance();

  static MessageBus* GetInstance() {
    return &Instance();
  }
  
  template<typename T>
  void Publish(const std::string& topic, const T& data) {
    // 在锁外提取订阅者列表，避免递归死锁，且用 shared_ptr 保持回调在异步执行期间存活
    std::vector<std::pair<std::shared_ptr<CallbackBase>, std::shared_ptr<const T>>> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = subscribers_.find(topic);
      if (it != subscribers_.end()) {
        const auto data_copy = std::make_shared<const T>(data);
        for (const auto& pair : it->second) {
          if (pair.second) {
            callbacks.emplace_back(pair.second, data_copy);
          }
        }
      } else {
        LOG_INFO("[MessageBus::Publish] topic: " << topic 
                  << ", type: " << typeid(T).name() 
                  << ", no subscribers");
      }
    }
    // 在锁外执行回调，避免递归死锁；shared_ptr 确保回调在异步执行期间保持存活
    const std::type_info* type_ptr = &typeid(T);
    for (const auto& [cb, data_copy] : callbacks) {
      detail::ThreadSafeCallbackExecutor::Execute([cb, data_copy, type_ptr]() {
        cb->call(static_cast<const void*>(data_copy.get()), *type_ptr);
      });
    }
  }
  
  template<typename T>
  CallbackId Subscribe(const std::string& topic, std::function<void(const T&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    CallbackId id = next_callback_id_.fetch_add(1);
    subscribers_[topic][id] = std::make_shared<TypedCallback<T>>(callback);
    LOG_INFO("[MessageBus::Subscribe] topic: " << topic 
              << ", type: " << typeid(T).name() 
              << ", callback_id: " << id);
    return id;
  }

  // High-rate telemetry keeps at most one pending GUI delivery per topic and
  // type. A newer sample replaces an older sample that has not run yet.
  template<typename T>
  void PublishLatest(const std::string& topic, T data) {
    struct State {
      std::mutex mutex;
      std::optional<T> latest;
      bool scheduled{false};
    };
    static std::mutex registry_mutex;
    static std::map<std::string, std::weak_ptr<State>> registry;

    std::shared_ptr<State> state;
    {
      std::lock_guard<std::mutex> lock(registry_mutex);
      state = registry[topic].lock();
      if (!state) {
        state = std::make_shared<State>();
        registry[topic] = state;
      }
    }
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->latest = std::move(data);
      if (state->scheduled) return;
      state->scheduled = true;
    }
    detail::ThreadSafeCallbackExecutor::Execute([this, topic, state]() {
      std::optional<T> latest;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        latest = std::move(state->latest);
        state->latest.reset();
        state->scheduled = false;
      }
      if (latest) Publish(topic, *latest);
    });
  }

  template<typename T>
  ScopedSubscription SubscribeScoped(const std::string& topic,
                                     std::function<void(const T&)> callback) {
    const auto active = std::make_shared<std::atomic_bool>(true);
    const CallbackId id = Subscribe<T>(
        topic, [active, callback = std::move(callback)](const T& data) {
          if (active->load(std::memory_order_acquire)) {
            callback(data);
          }
        });
    return ScopedSubscription([this, topic, id, active]() {
      active->store(false, std::memory_order_release);
      Unsubscribe(topic, id);
    });
  }

#ifdef QT_CORE_LIB
  template<typename T>
  CallbackId Subscribe(QObject* context, const std::string& topic,
                       std::function<void(const T&)> callback) {
    if (!context) {
      return 0;
    }

    const QPointer<QObject> guard(context);
    const CallbackId id = Subscribe<T>(topic, [guard, callback = std::move(callback)](const T& data) {
      if (guard) {
        callback(data);
      }
    });
    QObject::connect(context, &QObject::destroyed, [this, topic, id]() {
      Unsubscribe(topic, id);
    });
    return id;
  }
#endif
  
  void Unsubscribe(const std::string& topic, CallbackId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(topic);
    if (it != subscribers_.end()) {
      it->second.erase(id);
      if (it->second.empty()) {
        subscribers_.erase(it);
      }
    }
  }
  
  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.clear();
  }

 private:
  MessageBus() : next_callback_id_(1) {}
  ~MessageBus() = default;
  MessageBus(const MessageBus&) = delete;
  MessageBus& operator=(const MessageBus&) = delete;
  
  mutable std::mutex mutex_;
  std::map<std::string, std::map<CallbackId, std::shared_ptr<CallbackBase>>> subscribers_;
  std::atomic<CallbackId> next_callback_id_{1};
};

}  // namespace Framework

