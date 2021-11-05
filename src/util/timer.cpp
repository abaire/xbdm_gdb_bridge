#include "timer.h"

#include <thread>

long long Timer::MillisecondsElapsed() const {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
      .count();
}

void WaitMilliseconds(uint32_t milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}