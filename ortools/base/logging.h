// Copyright 2010-2014 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OR_TOOLS_BASE_LOGGING_H_
#define OR_TOOLS_BASE_LOGGING_H_

#include <cassert>
#include <cstdlib>
#include <iostream>  // NOLINT
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"

DECLARE_int32(log_level);
DECLARE_bool(log_prefix);

#if defined(_MSC_VER)
#pragma warning(disable : 4722)
#endif

// Always-on checking
#define CHECK(x)                                           \
  if (!(x))                                                \
  LogMessageFatal(__FILE__, __LINE__).stream() << "Check " \
                                                  "failed: " #x
#define CHECK_LT(x, y) CHECK((x) < (y))
#define CHECK_GT(x, y) CHECK((x) > (y))
#define CHECK_LE(x, y) CHECK((x) <= (y))
#define CHECK_GE(x, y) CHECK((x) >= (y))
#define CHECK_EQ(x, y) CHECK((x) == (y))
#define CHECK_NE(x, y) CHECK((x) != (y))

// Debug-only checking.
#ifdef NDEBUG
#define DCHECK(x) \
  while (false) CHECK(x)
#define DCHECK_LT(x, y) \
  while (false) CHECK((x) < (y))
#define DCHECK_GT(x, y) \
  while (false) CHECK((x) > (y))
#define DCHECK_LE(x, y) \
  while (false) CHECK((x) <= (y))
#define DCHECK_GE(x, y) \
  while (false) CHECK((x) >= (y))
#define DCHECK_EQ(x, y) \
  while (false) CHECK((x) == (y))
#define DCHECK_NE(x, y) \
  while (false) CHECK((x) != (y))
#else
#define DCHECK(x) CHECK(x)
#define DCHECK_LT(x, y) CHECK((x) < (y))
#define DCHECK_GT(x, y) CHECK((x) > (y))
#define DCHECK_LE(x, y) CHECK((x) <= (y))
#define DCHECK_GE(x, y) CHECK((x) >= (y))
#define DCHECK_EQ(x, y) CHECK((x) == (y))
#define DCHECK_NE(x, y) CHECK((x) != (y))
#endif  // NDEBUG

#define LOG_INFO LogMessage(__FILE__, __LINE__)
#define LOG_ERROR LOG_INFO
#define LOG_WARNING LOG_INFO
#define LOG_FATAL LogMessageFatal(__FILE__, __LINE__)
#define LOG_QFATAL LOG_FATAL

#define VLOG(x) \
  if ((x) <= FLAGS_log_level) LOG_INFO.stream()

#define VLOG_IS_ON(x) ((x) <= FLAGS_log_level)

#define LOG(severity) LOG_##severity.stream()
#define LG LOG_INFO.stream()
#define LOG_IF(severity, condition) \
  !(condition) ? (void)0 : LogMessageVoidify() & LOG(severity)

#ifdef NDEBUG
#define LOG_DFATAL LOG_ERROR
#define DFATAL ERROR
#define DLOG(severity) true ? (void)0 : LogMessageVoidify() & LOG(severity)
#define DLOG_IF(severity, condition) \
  (true || !(condition)) ? (void)0 : LogMessageVoidify() & LOG(severity)
#define DVLOG(x) \
  while (false && VLOG_IS_ON(x)) LogMessageVoidify() & LOG_INFO.stream()
#else
#define LOG_DFATAL LOG_FATAL
#define DFATAL FATAL
#define DLOG(severity) LOG(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DVLOG VLOG
#endif

// Poor man version of LOG_EVERY_N
#define LOG_EVERY_N(severity, n) LOG(severity)
#define LOG_EVERY_N_SEC(severity, n) LOG(severity)

namespace operations_research {
class DateLogger {
 public:
  DateLogger();
  char* const HumanDate();

 private:
  char buffer_[9];
};
}  // namespace operations_research

class LogMessage {
 public:
  LogMessage(const char* file, int line)
      :
#ifdef __ANDROID__
        log_stream_(std::cout)
#else
        log_stream_(std::cerr)
#endif
  {
    if (FLAGS_log_prefix) {
      log_stream_ << "[" << pretty_date_.HumanDate() << "] " << file << ":"
                  << line << ": ";
    }
  }
  ~LogMessage() { log_stream_ << "\n"; }
  std::ostream& stream() { return log_stream_; }

 protected:
  std::ostream& log_stream_;

 private:
  operations_research::DateLogger pretty_date_;
  DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

class LogMessageFatal : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line) : LogMessage(file, line) {}
  ~LogMessageFatal() {
    log_stream_ << "\n";
    abort();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LogMessageFatal);
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than "?:". See its usage.
  void operator&(std::ostream&) {}
};

template <typename T>
T&& CheckNotNull(T&& t) {
  CHECK(t != nullptr);
  return std::forward<T>(t);
}
#define CHECK_NOTNULL(x) CheckNotNull((x))

#endif  // OR_TOOLS_BASE_LOGGING_H_
