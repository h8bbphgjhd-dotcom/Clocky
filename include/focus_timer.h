#pragma once
#include <cstdint>
#include <mutex>

// One device-owned session, shared by the touchscreen and every browser.
// Monotonic time keeps NTP corrections from changing a running session.
class FocusTimer {
 public:
  struct State {
    uint32_t durationSeconds = 1500;
    uint32_t remainingSeconds = 1500;
    uint32_t completedSessions = 0;
    bool running = false;
    bool completed = false;
  };
  State snapshot(uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    update(now);
    State result = state_;
    result.remainingSeconds = (remainingMs_ + 999) / 1000;
    return result;
  }
  void start(uint32_t seconds, uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (seconds < 60 || seconds > 5400) return;
    state_.durationSeconds = seconds;
    remainingMs_ = seconds * 1000;
    state_.completed = false;
    state_.running = true;
    lastTick_ = now;
  }
  void pause(uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    update(now);
    state_.running = false;
  }
  void resume(uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (remainingMs_ == 0) return;
    state_.running = true;
    lastTick_ = now;
  }
  void reset(uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    update(now);
    remainingMs_ = state_.durationSeconds * 1000;
    state_.running = state_.completed = false;
  }
 private:
  std::mutex mutex_;
  State state_;
  uint32_t remainingMs_ = 1500000;
  uint32_t lastTick_ = 0;
  void update(uint32_t now) {
    if (!state_.running) return;
    const uint32_t elapsed = now - lastTick_;
    lastTick_ = now;
    if (elapsed >= remainingMs_) {
      remainingMs_ = 0;
      state_.running = false;
      state_.completed = true;
      ++state_.completedSessions;
    } else {
      remainingMs_ -= elapsed;
    }
  }
};
