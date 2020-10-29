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

#ifndef OR_TOOLS_PORT_PROTO_UTILS_H_
#define OR_TOOLS_PORT_PROTO_UTILS_H_

#include <string>

#ifndef __PORTABLE_PLATFORM__
#include "google/protobuf/descriptor.h"
#include "google/protobuf/text_format.h"
#endif

#include "absl/strings/str_cat.h"

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
  return absl::StrCat(enum_value);
}

template <typename ProtoType>
bool ProtobufTextFormatMergeFromString(const std::string& proto_text_string
                                           ABSL_ATTRIBUTE_UNUSED,
                                       ProtoType* proto ABSL_ATTRIBUTE_UNUSED) {
  return false;
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
  auto enum_value_descriptor = enum_descriptor->FindValueByNumber(enum_value);
  if (enum_value_descriptor == nullptr) {
    return absl::StrCat(
        "Invalid enum value of: ", enum_value, " for enum type: ",
        google::protobuf::GetEnumDescriptor<ProtoEnumType>()->name());
  }
  return enum_value_descriptor->name();
}

template <typename ProtoType>
bool ProtobufTextFormatMergeFromString(const std::string& proto_text_string,
                                       ProtoType* proto) {
  return google::protobuf::TextFormat::MergeFromString(proto_text_string,
                                                       proto);
}

#endif  // !__PORTABLE_PLATFORM__

}  // namespace operations_research

#endif  // OR_TOOLS_PORT_PROTO_UTILS_H_
