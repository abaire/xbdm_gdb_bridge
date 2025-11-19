#include "signaling_base.h"

#include <sys/fcntl.h>
#include <unistd.h>

#include <cassert>

SignalingBase::SignalingBase(const std::string& name) : SelectableBase(name) {
  if (pipe(pipe_fds_) == -1) {
    assert(!"Failed to create pipe for TaskConnection");
  }
  // Make the read end non-blocking so we can drain it
  fcntl(pipe_fds_[0], F_SETFL, O_NONBLOCK);
}

SignalingBase::~SignalingBase() {
  for (int i = 0; i < 2; ++i) {
    if (pipe_fds_[i] >= 0) {
      close(pipe_fds_[i]);
    }
  }
}

void SignalingBase::SignalProcessingNeeded() const {
  if (pipe_fds_[1] < 0) {
    return;
  }

  char ignored = 'a';
  write(pipe_fds_[1], &ignored, 1);
}

int SignalingBase::Select(fd_set& read_fds, fd_set&, fd_set&) {
  if (pipe_fds_[0] < 0) {
    return -1;
  }

  // Add our read descriptor to the set
  FD_SET(pipe_fds_[0], &read_fds);
  return pipe_fds_[0];
}

bool SignalingBase::Process(const fd_set& read_fds, const fd_set&,
                            const fd_set&) {
  if (pipe_fds_[0] < 0) {
    return false;
  }

  // Discard any read signals.
  if (FD_ISSET(pipe_fds_[0], &read_fds)) {
    char buffer[128];
    while (read(pipe_fds_[0], buffer, sizeof(buffer)) > 0) {
    }
  }

  return true;
}

void SignalingBase::Close() {
  for (int& pipe_fd : pipe_fds_) {
    if (pipe_fd >= 0) {
      close(pipe_fd);
      pipe_fd = -1;
    }
  }
}
