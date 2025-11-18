#include "thread_debug_util.h"

#include "logging.h"

#if defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

static constexpr auto kMaxThreadNameLength = 64;

namespace debug {
void SetCurrentThreadName(const std::string& name) {
  if (name.empty()) {
    return;
  }

#if defined(__APPLE__)
  if (int ret = pthread_setname_np(name.c_str()); ret) {
    LOG(warning) << "Failed to set thread name on macOS: " << ret;
  }

#elif defined(__linux__)

  std::string short_name = name;
  if (short_name.length() > 15) {
    short_name.resize(15);
  }

  if (int ret = pthread_setname_np(pthread_self(), short_name.c_str()); ret) {
    LOG(warning) << "Failed to set thread name on Linux: " << ret;
  }

#endif
}

std::string GetCurrentThreadName() {
#if defined(__APPLE__) || defined(__linux__)
  char name_buffer[kMaxThreadNameLength];

  int ret =
      pthread_getname_np(pthread_self(), name_buffer, kMaxThreadNameLength);
  if (ret != 0) {
    LOG(warning) << "Failed to get thread name: " << ret;
    return "";
  }

  name_buffer[kMaxThreadNameLength - 1] = 0;
  return name_buffer;

#else
  return "<NOT_SUPPORTED>";
#endif
}

}  // namespace debug
