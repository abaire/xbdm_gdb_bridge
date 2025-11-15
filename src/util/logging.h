#ifndef XBDM_GDB_BRIDGE_LOGGING_H
#define XBDM_GDB_BRIDGE_LOGGING_H

#include <boost/log/expressions/keyword.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <cstdint>
#include <thread>

namespace logging {

extern const char kLoggingTagAttribute[];
extern const char kLoggingFileAttribute[];
extern const char kLoggingLineAttribute[];
extern const char kLoggingThreadAttribute[];

extern const char kLoggingTagGDB[];
extern const char kLoggingTagXBDM[];
extern const char kLoggingTagXBDMNotification[];
extern const char kLoggingTagDebugger[];

BOOST_LOG_ATTRIBUTE_KEYWORD(tag_attr, kLoggingTagAttribute, std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(file_attr, kLoggingFileAttribute, std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line_attr, kLoggingLineAttribute, unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(thread_attr, kLoggingThreadAttribute,
                            std::thread::id)

#define LOG(lvl)                                                          \
  BOOST_LOG_TRIVIAL(lvl) << boost::log::add_value(::logging::file_attr,   \
                                                  __FILE__)               \
                         << boost::log::add_value(::logging::line_attr,   \
                                                  __LINE__)               \
                         << boost::log::add_value(::logging::thread_attr, \
                                                  std::this_thread::get_id())

#define LOG_TAGGED(lvl, tag) \
  LOG(lvl) << boost::log::add_value(::logging::tag_attr, tag)

#define LOG_GDB(lvl) LOG_TAGGED(lvl, ::logging::kLoggingTagGDB)

#define LOG_XBDM(lvl) LOG_TAGGED(lvl, ::logging::kLoggingTagXBDM)

#define LOG_DEBUGGER(lvl) LOG_TAGGED(lvl, ::logging::kLoggingTagDebugger)

void InitializeLogging(uint32_t verbosity);

void SetVerbosity(uint32_t verbosity);
void SetGDBTraceEnabled(bool enabled);
void SetXBDMTraceEnabled(bool enabled);
void SetDebuggerTraceEnabled(bool enabled);

void SetColorizedLoggingEnabled(bool enabled);
void SetLogLocationEnabled(bool enabled);
}  // namespace logging

#endif  // XBDM_GDB_BRIDGE_LOGGING_H
