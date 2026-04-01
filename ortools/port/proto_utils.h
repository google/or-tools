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

#ifndef ORTOOLS_PORT_PROTO_UTILS_H_
#define ORTOOLS_PORT_PROTO_UTILS_H_

#include <string>

#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/util/parse_proto.h"

namespace operations_research {

template <class P>
std::string ProtobufDebugString(const P& message) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, P>) {
    std::string output;
    google::protobuf::TextFormat::PrintToString(message, &output);
    absl::StripTrailingAsciiWhitespace(&output);
    return output;
  } else if constexpr (std::is_base_of_v<google::protobuf::MessageLite, P>) {
    return std::string(message.GetTypeName());
  } else {
    LOG(FATAL) << "Unsupported type";
    return "";
  }
}

template <class P>
std::string ProtobufShortDebugString(const P& message) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, P>) {
    std::string output;
    google::protobuf::TextFormat::Printer printer;
    printer.SetSingleLineMode(true);
    printer.PrintToString(message, &output);
    absl::StripTrailingAsciiWhitespace(&output);
    return output;
  } else if constexpr (std::is_base_of_v<google::protobuf::MessageLite, P>) {
    return std::string(message.GetTypeName());
  } else {
    LOG(FATAL) << "Unsupported type";
    return "";
  }
}

template <typename ProtoEnumType>
std::string ProtoEnumToString(ProtoEnumType enum_value) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
  static_assert(kTargetOsSupportsProtoDescriptor);
  auto enum_descriptor = google::protobuf::GetEnumDescriptor<ProtoEnumType>();
  auto enum_value_descriptor = enum_descriptor->FindValueByNumber(enum_value);
  if (enum_value_descriptor == nullptr) {
    return absl::StrCat("Invalid enum value of: ", enum_value,
                        " for enum type: ", enum_descriptor->name());
  }
  return std::string(enum_value_descriptor->name());
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
  static_assert(!kTargetOsSupportsProtoDescriptor);
  return absl::StrCat(enum_value);
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR)
}

template <typename ProtoType>
bool ProtobufTextFormatMergeFromString(absl::string_view proto_text_string,
                                       ProtoType* proto) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoType>) {
    return google::protobuf::TextFormat::MergeFromString(proto_text_string,
                                                         proto);
  } else if constexpr (std::is_base_of_v<google::protobuf::MessageLite,
                                         ProtoType>) {
    return false;
  } else {
    LOG(FATAL) << "Unsupported type";
    return false;
  }
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
  if constexpr (std::is_base_of_v<google::protobuf::Message, ProtoType>) {
    return ParseTextProtoForFlag(text, message_out, error_out);
  } else if constexpr (std::is_base_of_v<google::protobuf::MessageLite,
                                         ProtoType>) {
    if (text.empty()) {
      *message_out = ProtoType();
      return true;
    }
    *error_out =
        "cannot parse text protos on this platform (platform uses lite protos "
        "do not support parsing text protos)";
    return false;
  } else {
    LOG(FATAL) << "Unsupported type";
    return false;
  }
}

// Prints the input proto to a string on a single line in a format compatible
// with ProtobufParseTextProtoForFlag().
std::string ProtobufTextFormatPrintToStringForFlag(
    const google::protobuf::Message& proto);

// Prints an error message when compiling with lite protos.
std::string ProtobufTextFormatPrintToStringForFlag(
    const google::protobuf::MessageLite& proto);

}  // namespace operations_research

#endif  // ORTOOLS_PORT_PROTO_UTILS_H_
