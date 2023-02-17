// Copyright 2010-2022 Google LLC
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

#define ABSL_LOG_INTERNAL_CONDITION_DFATAL ABSL_LOG_INTERNAL_CONDITION_ERROR
#define kLogDebugFatal LogSeverity::kError
#define kDebugFatal kError

#include "absl/base/log_severity.h"
#include "absl/flags/declare.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "ortools/base/macros.h"
#include "ortools/base/vlog.h"

// Compatibility layer for glog/previous logging code.
ABSL_DECLARE_FLAG(bool, logtostderr);

// Forward the new flag.
ABSL_DECLARE_FLAG(int, stderrthreshold);

namespace operations_research {

void FixFlagsAndEnvironmentForSwig();

}  // namespace operations_research

// Compatibility layer for SCIP
namespace google {
enum LogSeverity {
  GLOG_INFO = static_cast<int>(absl::LogSeverity::kInfo),
  GLOG_WARNING = static_cast<int>(absl::LogSeverity::kWarning),
  GLOG_ERROR = static_cast<int>(absl::LogSeverity::kError),
  GLOG_FATAL = static_cast<int>(absl::LogSeverity::kFatal),
};
}  // namespace google

// Implementation of the `AbslStringify` interface. This adds `DebugString()`
// to the sink. Do not rely on exact format.
namespace google {
namespace protobuf {
template <typename Sink>
void AbslStringify(Sink& sink, const Message& msg) {
  sink.Append(msg.DebugString());
}
}  // namespace protobuf
}  // namespace google

#endif  // OR_TOOLS_BASE_LOGGING_H_
