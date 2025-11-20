#ifndef XBDM_GDB_BRIDGE_SIGNALLING_BASE_H
#define XBDM_GDB_BRIDGE_SIGNALLING_BASE_H

#include "net/selectable_base.h"

/**
 * A Selectable that provides a nop pipe that may be used to trigger the
 * SelectThread.
 */
class SignalingBase : public SelectableBase {
 public:
  explicit SignalingBase(const std::string& name);
  ~SignalingBase() override;

  int Select(fd_set& read_fds, fd_set&, fd_set&) override;

  bool Process(const fd_set& read_fds, const fd_set&, const fd_set&) override;

  virtual void Close();

  /**
   * Wake up the selection thread to force Process to be called.
   */
  virtual void SignalProcessingNeeded() const;

 private:
  int pipe_fds_[2]{-1, -1};
};

#endif  // XBDM_GDB_BRIDGE_SIGNALLING_BASE_H
