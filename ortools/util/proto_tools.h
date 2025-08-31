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

#ifndef OR_TOOLS_UTIL_PROTO_TOOLS_H_
#define OR_TOOLS_UTIL_PROTO_TOOLS_H_

#include <string>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/descriptor.h"
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

// This recursive function returns all the proto fields that are set(*) in a
// proto instance, along with how many times they appeared. A repeated field is
// only counted once as itself, regardless of its (non-zero) size, but then the
// nested child fields of a repeated message are counted once per instance.
// EXAMPLE: with these .proto message definitions:
//   message YY { repeated int z; }
//   message XX { int a; int b; repeated int c; repeated YY d; }
// and this instance of `XX`:
//   XX x = Parse("a: 10 c: [ 11, 12] d: {z: [13, 14]} d: {z: [15]}"");
// We'd expect ExploreAndCountAllProtoPathsInInstance() to yield the map:
//   {"a": 1, "c": 1, "d": 1, "d.z": 3}
//
// (*) The term 'set' has the usual semantic, which varies depending on proto
//     version. Extensions and unknown fields are ignored by this function.
void ExploreAndCountAllProtoPathsInInstance(
    const google::protobuf::Message& message,
    // Output. Must be non-nullptr. Entries are added to the map, i.e., the map
    // is not cleared. That allows cumulative use of this function across
    // several proto instances to build a global count of proto paths.
    absl::flat_hash_map<std::string, int>* proto_path_counts);

// This recursive function lists all the fields of a given proto *type*
// (not a proto instance), up to the given depth of nested sub-messages, and
// inserts their proto paths into `proto_paths`.
//
// SIMPLE EXAMPLE: with this .proto message definition:
//   message YY { repeated int z; }
//   message XX { int a; int b; repeated int c; repeated YY d; },
// ExploreAndInsertAllProtoPathsInType(
//     XX().GetDescriptor(), {}, {}, /*max_depth=*/3)
// Returns: {"a", "b", "c", "d", "d.z"}.
// See the unit test for more complex examples.
absl::flat_hash_set<std::string> ExploreAndInsertAllProtoPathsInType(
    const google::protobuf::Descriptor* descriptor,
    // ADVANCED USAGE: Pruning the proto tree exploration with two possible
    // mechanisms: 1) based on proto path, or 2) based on proto type.
    // 1) List of proto paths to skip: those fields will not be output nor
    //    explored, meaning that their descendants is also skipped.
    const absl::flat_hash_set<std::string>& skip_these_proto_paths,
    // 2) Maps a field's full *type* name (as in Descriptor::full_name(), e.g.,
    //    "operations_research.MyProto") to the subset of its child field names
    //    that may be explored.
    const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        proto_type_names_to_field_name_allowlist,
    // The maximum depth, when diving into sub-messages. Note that some protos
    // may have infinite potential depth, e.g. message X { X x; }, so we must
    // limit the recursion depth.
    int max_depth);

// =============================================================================
// Implementation of function templates.

template <class Proto>
absl::StatusOr<Proto*> SafeProtoDownCast(google::protobuf::Message* proto) {
  const google::protobuf::Descriptor* expected_descriptor =
      Proto::default_instance().GetDescriptor();
  const google::protobuf::Descriptor* actual_descriptor =
      proto->GetDescriptor();
  if (actual_descriptor == expected_descriptor) return down_cast<Proto*>(proto);
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
    return down_cast<const Proto*>(proto);
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Expected message type '%s', but got type '%s'",
      expected_descriptor->full_name(), actual_descriptor->full_name()));
}

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_PROTO_TOOLS_H_
