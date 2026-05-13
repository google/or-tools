// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_UTIL_STATUS_STREAMING_H_
#define ORTOOLS_UTIL_STATUS_STREAMING_H_

#include "absl/status/status.h"
#include "ortools/base/source_location.h"
#include "ortools/util/status.pb.h"

namespace operations_research {

// Streams the input Status in a proto.
//
// Note that payloads are not supported; a LOG(WARNING) will be printed if there
// are payloads in the input Status, using `warning_loc` as location.
StatusProto StreamStatus(
    const absl::Status& status,
    absl::SourceLocation warning_loc = absl::SourceLocation::current());

// Unstreams the input proto in the corresponding Status.
//
// The `loc` parameter is used as the status' location.
absl::Status UnstreamStatus(
    const StatusProto& status_proto,
    absl::SourceLocation loc = absl::SourceLocation::current());

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_STATUS_STREAMING_H_
