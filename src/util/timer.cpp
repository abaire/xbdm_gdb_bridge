#include "timer.h"

long long Timer::MillisecondsElapsed() const {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
      .count();
}
