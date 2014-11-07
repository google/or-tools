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

#include <cstdio>
#include <ctime>

#include "base/logging.h"

DEFINE_int32(log_level, 0, "Log level (0 is the default).");
DEFINE_bool(log_prefix, true,
            "Prefix all log lines with the date, source file and line number.");

namespace operations_research {
DateLogger::DateLogger() {
#if defined(_MSC_VER)
  _tzset();
#endif
}

char* const DateLogger::HumanDate() {
#if defined(_MSC_VER)
  _strtime_s(buffer_, sizeof(buffer_));
#else
  time_t time_value = time(NULL);
  struct tm now;
  localtime_r(&time_value, &now);
  snprintf(buffer_, sizeof(buffer_), "%02d:%02d:%02d", now.tm_hour,
           now.tm_min, now.tm_sec);
#endif
  return buffer_;
}
}  // namespace operations_research
