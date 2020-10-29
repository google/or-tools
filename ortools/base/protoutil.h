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

#ifndef OR_TOOLS_BASE_PROTOUTIL_H_
#define OR_TOOLS_BASE_PROTOUTIL_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"

namespace util_time {

inline ::absl::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    absl::Duration d) {
  google::protobuf::Duration proto;
  const int64 d_in_nano = ToInt64Nanoseconds(d);
  proto.set_seconds(static_cast<int64>(d_in_nano / 1000000000));
  proto.set_nanos(static_cast<int>(d_in_nano % 1000000000));
  return proto;
}

inline ::absl::Status EncodeGoogleApiProto(absl::Duration d,
                                           google::protobuf::Duration* proto) {
  *proto = EncodeGoogleApiProto(d).value();
  return absl::OkStatus();
}

inline ::absl::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto) {
  return absl::Seconds(proto.seconds() + 1e-9 * proto.nanos());
}

}  // namespace util_time

#endif  // OR_TOOLS_BASE_PROTOUTIL_H_
