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

#include "ortools/util/status_streaming.h"

#include <cstdint>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/source_location.h"
#include "ortools/util/status.pb.h"

namespace operations_research {

StatusProto StreamStatus(const absl::Status& status,
                         const absl::SourceLocation warning_loc) {
  StatusProto ret;
  ret.set_code(static_cast<int32_t>(status.code()));
  ret.set_message(status.message());
  status.ForEachPayload(
      [&](const absl::string_view type_url, const absl::Cord&) {
        LOG(WARNING).AtLocation(warning_loc.file_name(), warning_loc.line())
            << "Ignored payload: " << type_url;
      });
  return ret;
}

absl::Status UnstreamStatus(const StatusProto& status_proto,
                            absl::SourceLocation loc) {
  return absl::Status(static_cast<absl::StatusCode>(status_proto.code()),
                      status_proto.message());
}

}  // namespace operations_research
