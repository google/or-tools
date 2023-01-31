// Copyright 2010-2022 Google LLC
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

#if !defined(__PORTABLE_PLATFORM__)
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/text_format.h"
#include "ortools/util/parse_proto.h"
#endif  // !defined(__PORTABLE_PLATFORM__)

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace operations_research {

template <class P>
std::string ProtobufDebugString(const P& message) {
#if defined(__PORTABLE_PLATFORM__)
  return message.GetTypeName();
#else   // defined(__PORTABLE_PLATFORM__)
  return message.DebugString();
#endif  // !defined(__PORTABLE_PLATFORM__)
}

template <class P>
std::string ProtobufShortDebugString(const P& message) {
#if defined(__PORTABLE_PLATFORM__)
  return message.GetTypeName();
#else   // defined(__PORTABLE_PLATFORM__)
  return message.ShortDebugString();
#endif  // !defined(__PORTABLE_PLATFORM__)
}

template <typename ProtoEnumType>
std::string ProtoEnumToString(ProtoEnumType enum_value) {
#if defined(__PORTABLE_PLATFORM__)
  return absl::StrCat(enum_value);
#else   // defined(__PORTABLE_PLATFORM__)
  auto enum_descriptor = google::protobuf::GetEnumDescriptor<ProtoEnumType>();
  auto enum_value_descriptor = enum_descriptor->FindValueByNumber(enum_value);
  if (enum_value_descriptor == nullptr) {
    return absl::StrCat(
        "Invalid enum value of: ", enum_value, " for enum type: ",
        google::protobuf::GetEnumDescriptor<ProtoEnumType>()->name());
  }
  return enum_value_descriptor->name();
#endif  // !defined(__PORTABLE_PLATFORM__)
}

template <typename ProtoType>
bool ProtobufTextFormatMergeFromString(absl::string_view proto_text_string,
                                       ProtoType* proto) {
#if defined(__PORTABLE_PLATFORM__)
  return false;
#else   // !defined(__PORTABLE_PLATFORM__)
  return google::protobuf::TextFormat::MergeFromString(
      std::string(proto_text_string), proto);
#endif  // !defined(__PORTABLE_PLATFORM__)
}

// Tries to parse `text` as a text format proto. On a success, stores the result
// in `message_out` and returns true, otherwise, returns `false` with an
// explanation in `error_out`.
//
// When compiled with lite protos, any nonempty `text` will result in an error,
// as lite protos do not support parsing from text format.
//
// NOTE: this API is optimized for implementing AbslParseFlag(). The error
// message will be multiline and is designed to be easily read when printed.
template <typename ProtoType>
bool ProtobufParseTextProtoForFlag(absl::string_view text,
                                   ProtoType* message_out,
                                   std::string* error_out) {
#if defined(__PORTABLE_PLATFORM__)
  if (text.empty()) {
    *message_out = ProtoType();
    return true;
  }
  *error_out =
      "cannot parse text protos on this platform (platform uses lite protos do "
      "not support parsing text protos)";
  return false;
#else   // defined(__PORTABLE_PLATFORM__)
  return ParseTextProtoForFlag(text, message_out, error_out);
#endif  // !defined(__PORTABLE_PLATFORM__)
}

template <typename ProtoType>
std::string ProtobufTextFormatPrintToString(const ProtoType proto) {
#if defined(__PORTABLE_PLATFORM__)
  return absl::StrCat(
      "<text protos not supported with lite protobuf, cannot print proto "
      "message of type ",
      proto.GetTypeName(), ">");
#else   // defined(__PORTABLE_PLATFORM__)
  std::string result;
  google::protobuf::TextFormat::PrintToString(proto, &result);
  return result;
#endif  // !defined(__PORTABLE_PLATFORM__)
}

}  // namespace operations_research

#endif  // OR_TOOLS_PORT_PROTO_UTILS_H_
