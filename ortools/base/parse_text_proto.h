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

#ifndef OR_TOOLS_BASE_PARSE_TEXT_PROTO_H_
#define OR_TOOLS_BASE_PARSE_TEXT_PROTO_H_

#include <string>
#include <string_view>

#include "absl/log/check.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace google::protobuf::contrib::parse_proto {

template <typename T>
bool ParseTextProto(const std::string& input, T* proto) {
  return google::protobuf::TextFormat::ParseFromString(input, proto);
}

template <typename T>
T ParseTextOrDie(const std::string& input) {
  T result;
  CHECK(ParseTextProto(input, &result));
  return result;
}

namespace text_proto_internal {

class ParseProtoHelper {
 public:
  explicit ParseProtoHelper(std::string_view asciipb) : asciipb_(asciipb) {}
  template <class T>
  operator T() {  // NOLINT(runtime/explicit)
    T result;
    const bool ok = google::protobuf::TextFormat::TextFormat::ParseFromString(
        asciipb_, &result);
    CHECK(ok) << "Failed to parse text proto: " << asciipb_;
    return result;
  }

 private:
  const std::string asciipb_;
};

}  // namespace text_proto_internal

inline text_proto_internal::ParseProtoHelper ParseTextProtoOrDie(
    std::string_view input) {
  return text_proto_internal::ParseProtoHelper(input);
}

}  // namespace google::protobuf::contrib::parse_proto

#endif  // OR_TOOLS_BASE_PARSE_TEXT_PROTO_H_
