#include "logging.h"

#include <atomic>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <filesystem>
#include <iostream>

namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;

namespace logging {

static std::string base_path(__FILE__);

static uint32_t verbosity_level = boost::log::trivial::severity_level::warning;
static bool enable_gdb_messages = true;
static bool enable_xbdm_messages = true;
static bool enable_debugger_messages = true;
static bool enable_colorized_output = true;
static bool enable_log_location_info = true;

static constexpr const char ANSI_BLACK[] = "\x1b[30m";
static constexpr const char ANSI_RED[] = "\x1b[31m";
static constexpr const char ANSI_GREEN[] = "\x1b[32m";
static constexpr const char ANSI_YELLOW[] = "\x1b[33m";
static constexpr const char ANSI_BLUE[] = "\x1b[34m";
static constexpr const char ANSI_MAGENTA[] = "\x1b[35m";
static constexpr const char ANSI_CYAN[] = "\x1b[36m";
static constexpr const char ANSI_WHITE[] = "\x1b[37m";

static constexpr const char ANSI_BRIGHT_BLACK[] = "\x1b[90m";
static constexpr const char ANSI_BRIGHT_RED[] = "\x1b[91m";
static constexpr const char ANSI_BRIGHT_GREEN[] = "\x1b[92m";
static constexpr const char ANSI_BRIGHT_YELLOW[] = "\x1b[93m";
static constexpr const char ANSI_BRIGHT_BLUE[] = "\x1b[94m";
static constexpr const char ANSI_BRIGHT_MAGENTA[] = "\x1b[95m";
static constexpr const char ANSI_BRIGHT_CYAN[] = "\x1b[96m";
static constexpr const char ANSI_BRIGHT_WHITE[] = "\x1b[97m";

static constexpr const char ANSI_BLACK_BACKGROUND[] = "\x1b[40m";
static constexpr const char ANSI_RED_BACKGROUND[] = "\x1b[41m";
static constexpr const char ANSI_GREEN_BACKGROUND[] = "\x1b[42m";
static constexpr const char ANSI_YELLOW_BACKGROUND[] = "\x1b[43m";
static constexpr const char ANSI_BLUE_BACKGROUND[] = "\x1b[44m";
static constexpr const char ANSI_MAGENTA_BACKGROUND[] = "\x1b[45m";
static constexpr const char ANSI_CYAN_BACKGROUND[] = "\x1b[46m";
static constexpr const char ANSI_WHITE_BACKGROUND[] = "\x1b[47m";
static constexpr const char ANSI_BRIGHT_BLACK_BACKGROUND[] = "\x1b[100m";
static constexpr const char ANSI_BRIGHT_RED_BACKGROUND[] = "\x1b[101m";
static constexpr const char ANSI_BRIGHT_GREEN_BACKGROUND[] = "\x1b[102m";
static constexpr const char ANSI_BRIGHT_YELLOW_BACKGROUND[] = "\x1b[103m";
static constexpr const char ANSI_BRIGHT_BLUE_BACKGROUND[] = "\x1b[104m";
static constexpr const char ANSI_BRIGHT_MAGENTA_BACKGROUND[] = "\x1b[105m";
static constexpr const char ANSI_BRIGHT_CYAN_BACKGROUND[] = "\x1b[106m";
static constexpr const char ANSI_BRIGHT_WHITE_BACKGROUND[] = "\x1b[107m";

static constexpr const char ANSI_DEFAULT[] = "\x1b[39m";

static constexpr const char ANSI_RESET[] = "\x1b[0m";
static constexpr const char ANSI_BOLD[] = "\x1b[1m";
static constexpr const char ANSI_UNDERLINE[] = "\x1b[4m";
static constexpr const char ANSI_REVERSED[] = "\x1b[7m";

static std::map<std::thread::id, uint32_t> thread_names;
static std::atomic<uint32_t> next_thread_name{1};

typedef sinks::basic_formatted_sink_backend<char, sinks::synchronized_feeding>
    SynchronizedSink;

class LoggerSink : public SynchronizedSink {
 public:
  static void consume(boost::log::record_view const& rec,
                      string_type const& formatted_string) {
    std::cout << formatted_string;
  }

  static void format(boost::log::record_view const& rec,
                     boost::log::formatting_ostream& strm) {
    auto tag = boost::log::extract<std::string>(kLoggingTagAttribute, rec);
    auto severity = rec[boost::log::trivial::severity];

    // Ideally this would be done in filter() but attributes are not set until
    // after the filtering operation.
    // https://www.boost.org/doc/libs/1_58_0/libs/log/doc/html/log/rationale/why_attribute_manips_dont_affect_filters.html
    // TODO: Consider moving to scoped logs and doing this in the filter.
    if (!tag.empty() && severity.get() < boost::log::trivial::warning) {
      if (tag == kLoggingTagGDB && !enable_gdb_messages) {
        return;
      } else if (tag == kLoggingTagXBDM && !enable_xbdm_messages) {
        return;
      } else if (tag == kLoggingTagDebugger && !enable_debugger_messages) {
        return;
      }
    }

    if (enable_log_location_info) {
      auto filename =
          boost::log::extract<std::string>(kLoggingFileAttribute, rec);
      auto line_num =
          boost::log::extract<unsigned int>(kLoggingLineAttribute, rec);
      if (!filename.empty()) {
        // Drop the leading absolute path.
        std::string path = filename.get().substr(base_path.size());

        char buf[64] = {0};
        snprintf(buf, 63, "%s:%d", path.c_str(), line_num.get());

        strm << std::left << std::setw(42) << buf;
        strm << std::right << " ";
      }
    }

    auto thread =
        boost::log::extract<std::thread::id>(kLoggingThreadAttribute, rec);
    if (!thread.empty()) {
      // This assumes that access to thread_names has been serialized by the
      // sink, which may not be true.
      auto thread_id = thread.get();
      auto it = thread_names.find(thread_id);
      uint32_t thread_short_id;
      if (it == thread_names.end()) {
        thread_short_id = next_thread_name.fetch_add(1);
        thread_names[thread_id] = thread_short_id;
      } else {
        thread_short_id = it->second;
      }
      strm << "(" << std::hex << std::setfill('0') << std::setw(2)
           << thread_short_id << ") ";
    }

    if (enable_colorized_output) {
      switch (severity.get()) {
        case boost::log::trivial::severity_level::fatal:
        case boost::log::trivial::severity_level::error:
          strm << ANSI_RED;
          break;

        case boost::log::trivial::severity_level::warning:
          strm << ANSI_YELLOW;
          break;

        case boost::log::trivial::severity_level::debug:
        case boost::log::trivial::severity_level::trace:
          strm << ANSI_BRIGHT_BLACK;
          break;

        case boost::log::trivial::severity_level::info:
          strm << ANSI_GREEN;
          break;

        default:
          break;
      }
    }
    strm << "<" << severity << "> ";
    if (enable_colorized_output) {
      strm << ANSI_RESET;
    }

    if (!tag.empty()) {
      if (enable_colorized_output) {
        if (tag == kLoggingTagXBDM) {
          strm << ANSI_CYAN << ANSI_REVERSED;
        } else if (tag == kLoggingTagGDB) {
          strm << ANSI_BLUE << ANSI_REVERSED;
        } else if (tag == kLoggingTagDebugger) {
          strm << ANSI_MAGENTA << ANSI_REVERSED;
        }
      }
      strm << "[" << tag << "]";
      if (enable_colorized_output) {
        strm << ANSI_RESET;
      }
      strm << " ";
    }

    // Finally, put the record message to the stream
    strm << rec[expr::smessage];
    strm << std::endl;
  }

  static bool filter(const boost::log::attribute_value_set& values) {
    auto severity =
        values["Severity"].extract<boost::log::trivial::severity_level>();

    int info_level = boost::log::trivial::info;

    if (verbosity_level > info_level) {
      verbosity_level = info_level;
    } else if (info_level - verbosity_level > boost::log::trivial::fatal) {
      verbosity_level = info_level - boost::log::trivial::fatal;
    }

    return severity >= (boost::log::trivial::info - verbosity_level);
  };
};

void InitializeLogging(uint32_t verbosity) {
  auto idx = base_path.rfind('/');      // Drop the filename
  idx = base_path.rfind('/', idx - 1);  // Drop util
  idx = base_path.rfind('/', idx - 1);  // Drop src
  base_path = base_path.substr(0, idx + 1);
  auto core = boost::log::core::get();

  SetVerbosity(verbosity);
  core->set_filter(&LoggerSink::filter);

  typedef boost::log::sinks::synchronous_sink<LoggerSink> logger_sink;
  auto sink = boost::make_shared<logger_sink>();
  sink->set_formatter(&LoggerSink::format);
  core->add_sink(sink);

  boost::log::add_common_attributes();
}

void SetVerbosity(uint32_t verbosity) { verbosity_level = verbosity; }

void SetGDBTraceEnabled(bool enabled) { enable_gdb_messages = enabled; }

void SetXBDMTraceEnabled(bool enabled) { enable_xbdm_messages = enabled; }

void SetDebuggerTraceEnabled(bool enabled) {
  enable_debugger_messages = enabled;
}

void SetColorizedLoggingEnabled(bool enabled) {
  enable_colorized_output = enabled;
}

void SetLogLocationEnabled(bool enabled) { enable_log_location_info = enabled; }

}  // namespace logging