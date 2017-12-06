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

#ifndef OR_TOOLS_PORT_PROTO_UTILS_H_
#define OR_TOOLS_PORT_PROTO_UTILS_H_

#include <string>

#ifndef __PORTABLE_PLATFORM__
#include "google/protobuf/generated_enum_reflection.h"
#endif

#include "ortools/base/join.h"

namespace operations_research {
#if defined(__PORTABLE_PLATFORM__)
template <class P>
std::string ProtobufDebugString(const P& message) {
  return message.GetTypeName();
}

template <class P>
std::string ProtobufShortDebugString(const P& message) {
  return message.GetTypeName();
}

template <typename ProtoEnumType>
std::string ProtoEnumToString(ProtoEnumType enum_value) {
  return StrCat(enum_value);
}

#else  // __PORTABLE_PLATFORM__
template <class P>
std::string ProtobufDebugString(const P& message) {
  return message.DebugString();
}

template <class P>
std::string ProtobufShortDebugString(const P& message) {
  return message.ShortDebugString();
}

template <typename ProtoEnumType>
std::string ProtoEnumToString(ProtoEnumType enum_value) {
  auto enum_descriptor = google::protobuf::GetEnumDescriptor<ProtoEnumType>();
  auto enum_value_descriptor = enum_descriptor->value(enum_value);
  if (enum_value_descriptor == nullptr) {
    return StrCat(
        "Invalid enum value of: ", enum_value,
        " for enum type: ", google::protobuf::GetEnumDescriptor<ProtoEnumType>()->name());
  }
  return enum_value_descriptor->name();
}

#endif  // !__PORTABLE_PLATFORM__

}  // namespace operations_research

#endif  // OR_TOOLS_PORT_PROTO_UTILS_H_
