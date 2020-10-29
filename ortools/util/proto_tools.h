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

#ifndef OR_TOOLS_UTIL_PROTO_TOOLS_H_
#define OR_TOOLS_UTIL_PROTO_TOOLS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/message.h"

namespace operations_research {

// Casts a generic google::protobuf::Message* to a specific proto type, or
// returns an InvalidArgumentError if it doesn't seem to be of the right type.
// Comes in non-const and const versions.
// NOTE(user): You should rather use DynamicCastToGenerated() from message.h
// if you don't need the fancy error message or the absl::Status.
template <class Proto>
absl::StatusOr<Proto*> SafeProtoDownCast(google::protobuf::Message* proto);
template <class Proto>
absl::StatusOr<const Proto*> SafeProtoConstDownCast(
    const google::protobuf::Message* proto);

// Prints a proto2 message as a string, it behaves like TextFormat::Print()
// but also prints the default values of unset fields which is useful for
// printing parameters.
std::string FullProtocolMessageAsString(
    const google::protobuf::Message& message, int indent_level);

// =============================================================================
// Implementation of function templates.

template <class Proto>
absl::StatusOr<Proto*> SafeProtoDownCast(google::protobuf::Message* proto) {
  const google::protobuf::Descriptor* expected_descriptor =
      Proto::default_instance().GetDescriptor();
  const google::protobuf::Descriptor* actual_descriptor =
      proto->GetDescriptor();
  if (actual_descriptor == expected_descriptor)
    return reinterpret_cast<Proto*>(proto);
  return absl::InvalidArgumentError(absl::StrFormat(
      "Expected message type '%s', but got type '%s'",
      expected_descriptor->full_name(), actual_descriptor->full_name()));
}

template <class Proto>
absl::StatusOr<const Proto*> SafeProtoConstDownCast(
    const google::protobuf::Message* proto) {
  const google::protobuf::Descriptor* expected_descriptor =
      Proto::default_instance().GetDescriptor();
  const google::protobuf::Descriptor* actual_descriptor =
      proto->GetDescriptor();
  if (actual_descriptor == expected_descriptor) {
    return reinterpret_cast<const Proto*>(proto);
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Expected message type '%s', but got type '%s'",
      expected_descriptor->full_name(), actual_descriptor->full_name()));
}

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_PROTO_TOOLS_H_
