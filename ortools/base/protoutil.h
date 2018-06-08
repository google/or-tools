// Copyright 2010-2017 Google
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

#include "google/protobuf/duration.pb.h"
#include "ortools/base/status.h"
#include "ortools/base/statusor.h"
#include "ortools/base/time_support.h"

namespace util_time {

inline ::util::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    base::Duration d) {
  google::protobuf::Duration proto;
  proto.set_seconds(static_cast<int64>(d));
  proto.set_nanos(static_cast<int>(d * 1e9) % 1000000000);
  return proto;
}

inline ::util::Status EncodeGoogleApiProto(base::Duration d,
                                           google::protobuf::Duration* proto) {
  *proto = EncodeGoogleApiProto(d).ValueOrDie();
  return util::OkStatus();
}

inline ::util::StatusOr<base::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto) {
  return base::Seconds(proto.seconds() + 1e-9 * proto.nanos());
}

}  // namespace util_time

#endif  // OR_TOOLS_BASE_PROTOUTIL_H_
