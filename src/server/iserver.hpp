#pragma once

#include <bitset>
#include <concepts>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

namespace messenger {

class IServer {
 public:
  virtual void exec(std::stop_token stok) = 0;
};

constexpr auto DEFAULT_STATES_BITSIZE = 2;

class BaseServer : public IServer {
  class States {
   public:
    enum State : uint8_t {
      OFF = 0b00,
      PREPARE = 0b01,
      RUN = 0b10,
      WARN = 0b11
    };

    States() = default;

    bool off() const noexcept {
      std::lock_guard lock(mutex_);
      return state_ == OFF;
    }

    bool prepare() const noexcept {
      std::lock_guard lock(mutex_);
      return state_ == PREPARE;
    }

    bool run() const noexcept {
      std::lock_guard lock(mutex_);
      return state_ == RUN;
    }

    bool warn() const noexcept {
      std::lock_guard lock(mutex_);
      return state_ == WARN;
    }

    bool set_prepare() noexcept {
      std::lock_guard lock(mutex_);
      if (state_ == OFF || state_ == PREPARE) {
        state_ = PREPARE;
        return true;
      }
      return false;
    }

    bool set_run() noexcept {
      std::lock_guard lock(mutex_);
      if (state_ == PREPARE) {
        state_ = RUN;
        return true;
      }
      return false;
    }

    void set_warn() noexcept {
      std::lock_guard lock(mutex_);
      state_ = WARN;
    }

    void set_off() noexcept {
      std::lock_guard lock(mutex_);
      state_ = OFF;
    }

    State get_state() const noexcept {
      std::lock_guard lock(mutex_);
      return state_;
    }

   private:
    mutable std::mutex mutex_;
    State state_ = OFF;
  };

 public:
  BaseServer(uint64_t id) : id_{id} {}

 public:
  uint64_t id() const { return id_.value(); }
  const States& state() const noexcept { return stts_; }
  bool off() const noexcept { return stts_.off(); }
  bool prepare() const noexcept { return stts_.prepare(); }
  bool run() const noexcept { return stts_.run(); }
  bool warn() const noexcept { return stts_.warn(); }

 protected:
  void unsafe_set_off() noexcept { stts_.set_off(); }
  void set_prepare() noexcept { stts_.set_prepare(); }
  void set_run() noexcept { stts_.set_run(); }
  void set_warn() noexcept { stts_.set_warn(); }

 private:
  States stts_;
  std::optional<uint64_t> id_{std::nullopt};
};

template <typename T>
concept Callable = requires(T t, std::stop_token stok) {
  { t(stok) } -> std::convertible_to<void>;
};

}  // namespace messenger
