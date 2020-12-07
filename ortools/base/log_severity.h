// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_LOG_SEVERITY_H_
#define OR_TOOLS_BASE_LOG_SEVERITY_H_

#include "ortools/base/logging_export.h"

// Variables of type LogSeverity are widely taken to lie in the range
// [0, NUM_SEVERITIES-1].  Be careful to preserve this assumption if
// you ever need to change their values or add a new severity.
typedef int LogSeverity;

namespace google {
const int GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3,
          NUM_SEVERITIES = 4;

extern GOOGLE_GLOG_DLL_DECL const char* const LogSeverityNames[NUM_SEVERITIES];
}  // namespace google
#ifndef _MSC_VER
const int INFO = google::GLOG_INFO, WARNING = google::GLOG_WARNING,
          ERROR = google::GLOG_ERROR, FATAL = google::GLOG_FATAL;
#endif

// DFATAL is FATAL in debug mode, ERROR in normal mode
#ifdef NDEBUG
#define DFATAL_LEVEL ERROR
#else
#define DFATAL_LEVEL FATAL
#endif

#endif  // OR_TOOLS_BASE_LOG_SEVERITY_H_
