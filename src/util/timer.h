#ifndef XBDM_GDB_BRIDGE_SRC_UTIL_TIMER_H_
#define XBDM_GDB_BRIDGE_SRC_UTIL_TIMER_H_

#include <chrono>

class Timer {
 public:
  Timer() { Start(); }

  void Start() { start_ = std::chrono::steady_clock::now(); }
  [[nodiscard]] long long MillisecondsElapsed() const;

 private:
  std::chrono::steady_clock::time_point start_;
};

void WaitMilliseconds(uint32_t milliseconds);

#endif  // XBDM_GDB_BRIDGE_SRC_UTIL_TIMER_H_
