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

#include "ortools/port/proto_utils.h"

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"

namespace operations_research {

std::string ProtobufTextFormatPrintToStringForFlag(
    const google::protobuf::Message& proto) {
  std::string result;
  google::protobuf::TextFormat::Printer printer;
  printer.SetSingleLineMode(true);
  printer.PrintToString(proto, &result);
  // TextFormat may add an empty space at the end of the string (e.g. "cpu: 3.5
  // "), likely because it always add a space after each field; we simply remove
  // it here.
  absl::StripTrailingAsciiWhitespace(&result);
  return result;
}

std::string ProtobufTextFormatPrintToStringForFlag(
    const google::protobuf::MessageLite& proto) {
  return absl::StrCat(
      "<text protos not supported with lite protobuf, cannot print proto "
      "message of type ",
      proto.GetTypeName(), ">");
}

}  // namespace operations_research
