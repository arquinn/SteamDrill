#ifndef __LOG_H_
#define __LOG_H_

// needs to happen before that include (!)
#ifdef SPDLOG_ACTIVE_LEVEL
#undef SPDLOG_ACTIVE_LEVEL
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"

// -------------- //
// Logging Macros //
// -------------- //

#define ERROR(...) SPDLOG_ERROR( __VA_ARGS__);
#define STATUS(...) SPDLOG_INFO (__VA_ARGS__);
#define WARN(...) SPDLOG_WARN(__VA_ARGS__);
#define INFO(...) SPDLOG_INFO (__VA_ARGS__);
#define NDEBUG(...) SPDLOG_DEBUG(__VA_ARGS__);
#define TRACE(...) SPDLOG_TRACE(__VA_ARGS__);

namespace Log {
void inline initLogger(spdlog::level::level_enum l) {
  spdlog::set_level(l);
  spdlog::set_pattern("[%P][%^%l%$] %@: %v");
} // end initLogger
} // end namespace Log

#endif /*__LOG_H_*/
