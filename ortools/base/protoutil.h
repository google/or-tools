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

#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "ortools/base/status.h"
#include "ortools/base/statusor.h"

namespace util_time {

inline ::util::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    absl::Duration d) {
  google::protobuf::Duration proto;
  const int64 d_in_nano = ToInt64Nanoseconds(d);
  proto.set_seconds(static_cast<int64>(d_in_nano / 1000000000));
  proto.set_nanos(static_cast<int>(d_in_nano % 1000000000));
  return proto;
}

inline ::util::Status EncodeGoogleApiProto(absl::Duration d,
                                           google::protobuf::Duration* proto) {
  *proto = EncodeGoogleApiProto(d).ValueOrDie();
  return util::OkStatus();
}

inline ::util::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto) {
  return absl::Seconds(proto.seconds() + 1e-9 * proto.nanos());
}

}  // namespace util_time

#endif  // OR_TOOLS_BASE_PROTOUTIL_H_
