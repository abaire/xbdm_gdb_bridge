#include "tracer.h"

#include <filesystem>
#include <fstream>
#include <memory>

#include "dyndxt_loader/dyndxt_requests.h"
#include "dyndxt_loader/loader.h"
#include "notification_ntrc.h"
#include "ntrc_dyndxt.h"
#include "ntrc_dyndxt_xbox.h"
#include "rdcp/xbdm_requests.h"
#include "util/logging.h"
#include "util/timer.h"
#include "xbox/xbdm_context.h"
#include "xbox/xbox_interface.h"

constexpr const char kLoggingTagTracer[] = "TRC";
#define LOG_TRACER(lvl) LOG_TAGGED(lvl, kLoggingTagTracer)

namespace NTRCTracer {

Tracer *Tracer::singleton_ = nullptr;

static bool InstallDynDXT(XBOXInterface &interface);

bool Tracer::Initialize(XBOXInterface &interface) {
  if (!singleton_) {
    singleton_ = new Tracer();
  }
  return singleton_->Install(interface);
}

bool Tracer::Install(XBOXInterface &interface) {
  if (!notification_handler_id_) {
    notification_handler_id_ = interface.Context()->RegisterNotificationHandler(
        [this](const std::shared_ptr<XBDMNotification> &notification,
               XBDMContext &context) {
          if (notification->Type() != NT_CUSTOM ||
              notification->NotificationPrefix() != NTRC_HANDLER_NAME) {
            return;
          }

          auto ntrc_notification =
              std::dynamic_pointer_cast<NotificationNTRC>(notification);

          this->OnNotification(ntrc_notification, context);
        });
    RegisterXBDMNotificationConstructor(
        NTRC_HANDLER_NAME, MakeXBDMNotificationConstructor<NotificationNTRC>());
  }

  // Create a dedicated connection. If the tracer isn't already running, this
  // will fail to find the command handler and spawn an install.
  {
    auto request = std::make_shared<Dedicate>(NTRC_HANDLER_NAME);
    interface.SendCommandSync(request, NTRC_HANDLER_NAME);
    if (!request->IsOK()) {
      if (!InstallDynDXT(interface)) {
        return false;
      }
      interface.SendCommandSync(request, NTRC_HANDLER_NAME);
      if (!request->IsOK()) {
        return false;
      }
    }
  }

  return true;
}

static bool InstallDynDXT(XBOXInterface &interface) {
  auto ntrcDynDXT = std::vector<uint8_t>(
      kNTRCDynDXT, kNTRCDynDXT + sizeof(kNTRCDynDXT) / sizeof(kNTRCDynDXT[0]));

  if (!DynDXTLoader::Loader::Install(interface, ntrcDynDXT)) {
    LOG_TRACER(error) << "Failed to install tracer dyndxt.";
    return false;
  }

  return true;
}

bool Tracer::Attach(XBOXInterface &interface) {
  auto request = std::make_shared<DynDXTLoader::InvokeSimple>(
      NTRC_HANDLER_NAME "!attach tcap=1 dcap=1 ccap=1 rdicap=0");
  interface.SendCommandSync(request, NTRC_HANDLER_NAME);
  if (!request->IsOK()) {
    LOG_TRACER(error) << *request << std::endl;
    return false;
  }

  return true;
}

bool Tracer::Detach(XBOXInterface &interface) {
  auto request =
      std::make_shared<DynDXTLoader::InvokeSimple>(NTRC_HANDLER_NAME "!detach");
  interface.SendCommandSync(request, NTRC_HANDLER_NAME);
  if (!request->IsOK()) {
    LOG_TRACER(error) << *request << std::endl;
    return false;
  }

  return true;
}

void Tracer::OnNotification(
    const std::shared_ptr<NotificationNTRC> &notification,
    XBDMContext &context) {
  const auto &content = notification->content;

  if (content.HasKey("new_state")) {
    OnNewState(content.GetDWORD("new_state"), context);
  } else if (content.HasKey("req_processed")) {
    request_processed_ = true;
  } else if (content.HasKey("w_pgraph")) {
    pgraph_data_available_ = true;
  } else if (content.HasKey("w_aux")) {
    aux_data_available_ = true;
  } else {
    LOG_TRACER(error) << "Notification handler called with unknown type: "
                      << *notification;
  }
}

void Tracer::OnNewState(int new_state, XBDMContext &context) {
#define PRINT_STATE(lvl, val) \
  case val:                   \
    LOG_TRACER(lvl) << #val;  \
    return

  switch (new_state) {
    PRINT_STATE(error, STATE_FATAL_NOT_IN_NEW_FRAME_STATE);
    PRINT_STATE(error, STATE_FATAL_NOT_IN_STABLE_STATE);
    PRINT_STATE(error, STATE_FATAL_DISCARDING_FAILED);
    PRINT_STATE(error, STATE_FATAL_PROCESS_PUSH_BUFFER_COMMAND_FAILED);

    case STATE_SHUTDOWN_REQUESTED:
      LOG_TRACER(info) << "Shutting down...";
      return;

    case STATE_SHUTDOWN:
      OnShutdown(context);
      return;

    case STATE_UNINITIALIZED:
      LOG_TRACER(info) << "STATE_UNINITIALIZED";
      return;

    case STATE_INITIALIZING:
      LOG_TRACER(info) << "Initializing...";
      return;

    case STATE_INITIALIZED:
      LOG_TRACER(info) << "Initialized";
      return;

    case STATE_IDLE:
      LOG_TRACER(info) << "Idle";
      return;

    case STATE_IDLE_STABLE_PUSH_BUFFER:
      LOG_TRACER(info) << "Idle with stable pushbuffer";
      return;

    case STATE_IDLE_NEW_FRAME:
      LOG_TRACER(info) << "Idle at start of new frame";
      return;

    case STATE_WAITING_FOR_STABLE_PUSH_BUFFER:
      LOG_TRACER(info) << "Waiting for stable pushbuffer";
      return;

    case STATE_DISCARDING_UNTIL_FLIP:
      LOG_TRACER(info) << "Discarding until buffer flip";
      return;

    case STATE_TRACING_UNTIL_FLIP:
      LOG_TRACER(info) << "Tracing until buffer flip";
      return;

    default:
      LOG_TRACER(error) << "TODO: Handle new state " << new_state;
  }
}

void Tracer::OnShutdown(XBDMContext &context) {
  context.UnregisterNotificationHandler(notification_handler_id_);
  notification_handler_id_ = 0;
  UnregisterXBDMNotificationConstructor(NTRC_HANDLER_NAME);
}  // namespace NTRCTracer

bool Tracer::BreakOnFrameStart(XBOXInterface &interface, bool require_flip) {
  Tracer *instance = singleton_;
  if (!instance) {
    LOG_TRACER(error) << "Tracer not initialized.";
    return false;
  }

  return instance->BreakOnFrameStart_(interface, require_flip);
}

bool Tracer::BreakOnFrameStart_(XBOXInterface &interface, bool require_flip) {
  {
    request_processed_ = false;
    auto request = std::make_shared<DynDXTLoader::InvokeSimple>(
        NTRC_HANDLER_NAME "!wait_stable_pb");
    interface.SendCommandSync(request, NTRC_HANDLER_NAME);
    if (!request->IsOK()) {
      LOG_TRACER(error) << *request << std::endl;
      return false;
    }

    while (!request_processed_.exchange(false)) {
      WaitMilliseconds(10);
    }
  }
  {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), NTRC_HANDLER_NAME "!discard_until_flip%s",
             require_flip ? " require_flip" : "");

    request_processed_ = false;
    auto request = std::make_shared<DynDXTLoader::InvokeSimple>(cmd);
    interface.SendCommandSync(request, NTRC_HANDLER_NAME);
    if (!request->IsOK()) {
      LOG_TRACER(error) << *request << std::endl;
      return false;
    }

    while (!request_processed_.exchange(false)) {
      WaitMilliseconds(10);
    }
  }
  return true;
}

bool Tracer::TraceFrames(XBOXInterface &interface,
                         const std::string &artifact_path,
                         uint32_t num_frames) {
  Tracer *instance = singleton_;
  if (!instance) {
    LOG_TRACER(error) << "Tracer not initialized.";
    return false;
  }

  for (auto i = 0; i < num_frames; ++i) {
    char frame_name[32];
    snprintf(frame_name, sizeof(frame_name), "frame_%d", i + 1);
    auto output_path = std::filesystem::path(artifact_path) / frame_name;
    if (!instance->TraceFrame(interface, output_path)) {
      return false;
    }

    LOG_TRACER(trace) << "Frame trace " << (i + 1) << " completed.";
  }
  return true;
}

bool Tracer::TraceFrame(XBOXInterface &interface,
                        const std::filesystem::path &artifact_path) {
  if (!exists(artifact_path)) {
    create_directories(artifact_path);
  }

  in_progress_frame_.Setup(artifact_path);

  request_processed_ = false;
  pgraph_data_available_ = false;
  aux_data_available_ = false;

  auto request = std::make_shared<DynDXTLoader::InvokeSimple>(NTRC_HANDLER_NAME
                                                              "!trace_frame");
  interface.SendCommandSync(request, NTRC_HANDLER_NAME);
  if (!request->IsOK()) {
    LOG_TRACER(error) << *request << std::endl;
    return false;
  }

  while (!request_processed_) {
    if (pgraph_data_available_.exchange(false)) {
      if (in_progress_frame_.FetchPGRAPHTraceData(interface) ==
          FrameCapture::FetchResult::ERROR) {
        // TODO: Handle error.
        LOG_TRACER(error) << "FetchPGRAPHTraceData failed.";
      }
    }

    if (aux_data_available_.exchange(false)) {
      if (in_progress_frame_.FetchAuxTraceData(interface) ==
          FrameCapture::FetchResult::ERROR) {
        // TODO: Handle error.
        LOG_TRACER(error) << "FetchPGRAPHTraceData failed.";
      }
    }
  }

  // Consume any remaining PGRAPH data.
  while (true) {
    auto result = in_progress_frame_.FetchPGRAPHTraceData(interface);
    if (result == FrameCapture::FetchResult::NO_DATA_AVAILABLE) {
      break;
    }
    if (result == FrameCapture::FetchResult::ERROR) {
      // TODO: Handle error
      LOG_TRACER(error) << "FetchPGRAPHTraceData failed.";
    }
  }

  // Consume any remaining graphics data.
  while (true) {
    auto result = in_progress_frame_.FetchAuxTraceData(interface);
    if (result == FrameCapture::FetchResult::NO_DATA_AVAILABLE) {
      break;
    }
    if (result == FrameCapture::FetchResult::ERROR) {
      // TODO: Handle error
      LOG_TRACER(error) << "FetchAuxTraceData failed.";
    }
  }

  in_progress_frame_.Close();

  return true;
}

}  // namespace NTRCTracer
