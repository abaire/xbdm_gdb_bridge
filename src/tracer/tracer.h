#ifndef XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_H_
#define XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_H_

#include <atomic>
#include <filesystem>
#include <memory>

#include "frame_capture.h"
#include "notification_ntrc.h"

class XBDMContext;
class XBOXInterface;
struct XBDMNotification;

namespace NTRCTracer {

//! Handles interaction with the ntrc_dyndxt, facilitating tracing of pushbuffer
//! messages and dumping of graphics related buffers.
class Tracer {
 public:
  //! Initializes the Tracer singleton.
  static bool Initialize(XBOXInterface &interface);

  //! Attaches to the tracer instance on the XBOX.
  //!
  //! \param tcap - enable texture capture
  //! \param dcap - enable depth surface capture
  //! \param ccap - enable color surface (framebuffer) capture
  //! \param rdicap - enable RDI capture
  //! \param rawpgraph - enable capture of PGRAPH registers
  //! \param rawpfb - enable capture of PFB region
  static bool Attach(XBOXInterface &interface, bool tcap = true,
                     bool dcap = false, bool ccap = true, bool rdicap = false,
                     bool rawpgraph = false, bool rawpfb = false);

  //! Detaches from the tracer instance on the XBOX.
  static bool Detach(XBOXInterface &interface);

  //! Instructs the tracer to break on the start of a frame, discarding data if
  //! not currently at a frame start. If `require_flip` is true, discards until
  //! the next frame, even if currently at the start of a frame.
  static bool BreakOnFrameStart(XBOXInterface &interface, bool require_flip);

  //! Trace one or more consecutive frames.
  static bool TraceFrames(XBOXInterface &interface,
                          const std::string &artifact_path,
                          uint32_t num_frames = 1, bool verbose = false);

 private:
  //! Installs the ntrc_dyndxt if necessary and registers for notifications.
  bool Install(XBOXInterface &interface);

  //! Handles tracer-related push notifications from the XBOX.
  void OnNotification(const std::shared_ptr<NotificationNTRC> &notification,
                      XBDMContext &context);

  //! Processes a new state push notification.
  void OnNewState(int new_state, XBDMContext &context);

  //! Handles graceful tracer shutdown.
  void OnShutdown(XBDMContext &context);

  //! Instructs the tracer to break on the start of a frame, discarding data if
  //! not currently at a frame start. If `require_flip` is true, discards until
  //  //! the next frame, even if currently at the start of a frame.
  bool BreakOnFrameStart_(XBOXInterface &interface, bool require_flip);

  //! Traces a single frame.
  bool TraceFrame(XBOXInterface &interface,
                  const std::filesystem::path &artifact_path,
                  bool verbose = false);

 private:
  static Tracer *singleton_;

  int notification_handler_id_{0};
  std::atomic_bool request_failed_{false};
  std::atomic_bool request_processed_{false};
  std::atomic_bool pgraph_data_available_{false};
  std::atomic_bool aux_data_available_{false};

  FrameCapture in_progress_frame_;
  std::list<FrameCapture> captured_frames_;
};

}  // namespace NTRCTracer

#endif  // XBDM_GDB_BRIDGE_SRC_TRACER_TRACER_H_
