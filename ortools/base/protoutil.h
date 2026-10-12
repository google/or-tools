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

#ifndef ORTOOLS_BASE_PROTOUTIL_H_
#define ORTOOLS_BASE_PROTOUTIL_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"

namespace util_time {

::absl::Status Validate(const google::protobuf::Duration& d);

::absl::Status EncodeGoogleApiProto(absl::Duration d,
                                    google::protobuf::Duration* proto);

::absl::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    absl::Duration d);

::absl::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto);

}  // namespace util_time

#endif  // ORTOOLS_BASE_PROTOUTIL_H_
