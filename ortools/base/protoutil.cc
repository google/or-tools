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

#include "ortools/base/protoutil.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"

namespace util_time {

::absl::Status Validate(const google::protobuf::Duration& d) {
  const int64_t sec = d.seconds();
  const int64_t ns = d.nanos();
  if (sec < -315576000000 || sec > 315576000000) {
    return absl::InvalidArgumentError(absl::StrCat("seconds=", sec));
  }
  if (ns < -999999999 || ns > 999999999) {
    return absl::InvalidArgumentError(absl::StrCat("nanos=", ns));
  }
  if ((sec < 0 && ns > 0) || (sec > 0 && ns < 0)) {
    return absl::InvalidArgumentError("sign mismatch");
  }
  return absl::OkStatus();
}

::absl::Status EncodeGoogleApiProto(absl::Duration d,
                                    google::protobuf::Duration* proto) {
  // s and n may both be negative, per the Duration proto spec.
  const int64_t s = absl::IDivDuration(d, absl::Seconds(1), &d);
  const int64_t n = absl::IDivDuration(d, absl::Nanoseconds(1), &d);
  proto->set_seconds(s);
  proto->set_nanos(n);
  return Validate(*proto);
}

::absl::StatusOr<google::protobuf::Duration> EncodeGoogleApiProto(
    absl::Duration d) {
  google::protobuf::Duration proto;
  absl::Status status = EncodeGoogleApiProto(d, &proto);
  if (!status.ok()) return status;
  return proto;
}

::absl::StatusOr<absl::Duration> DecodeGoogleApiProto(
    const google::protobuf::Duration& proto) {
  absl::Status status = Validate(proto);
  if (!status.ok()) return status;
  return absl::Seconds(proto.seconds()) + absl::Nanoseconds(proto.nanos());
}

}  // namespace util_time
