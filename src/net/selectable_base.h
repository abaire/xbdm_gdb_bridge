#ifndef XBDM_GDB_BRIDGE_SELECTABLE_BASE_H
#define XBDM_GDB_BRIDGE_SELECTABLE_BASE_H

#include <sys/select.h>

#include <chrono>
#include <optional>
#include <string>

/**
 * Base class for objects that may be processed by a SelectThread.
 */
class SelectableBase {
 public:
  explicit SelectableBase(std::string name);
  virtual ~SelectableBase() = default;

  [[nodiscard]] const std::string& Name() const { return name_; }
  [[nodiscard]] bool IsShutdown() const { return is_shutdown_; }

  //! Sets one or more file descriptors in the given `fd_set`s and returns the
  //! maximum file descriptor that was set.
  virtual int Select(fd_set& read_fds, fd_set& write_fds,
                     fd_set& except_fds) = 0;

  //! Processes pending data as indicated in the given `fd_sets`. Returns true
  //! if this socket remains valid.
  virtual bool Process(const fd_set& read_fds, const fd_set& write_fds,
                       const fd_set& except_fds) = 0;

  /**
   * Returns the absolute time of the next event for this selectable.
   *
   * @return The absolute time of the next event using std::chrono::steady_clock
   *         or nullopt if no time-based wakeup is needed.
   */
  virtual std::optional<std::chrono::steady_clock::time_point>
  GetNextEventTime() {
    return std::nullopt;
  }

  virtual std::ostream& Print(std::ostream& os) const {
    return os << "Selectable[" << name_ << "]";
  }

 protected:
  std::string name_;
  bool is_shutdown_{false};
};

template <class OS>
OS& operator<<(OS& os, SelectableBase const& base) {
  return base.Print(os);
}

#endif  // XBDM_GDB_BRIDGE_SELECTABLE_BASE_H
