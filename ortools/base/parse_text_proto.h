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

// emulates g3/net/proto2/contrib/parse_proto/parse_text_proto.h
#ifndef ORTOOLS_BASE_PARSE_TEXT_PROTO_H_
#define ORTOOLS_BASE_PARSE_TEXT_PROTO_H_

#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/status_macros.h"

namespace google::protobuf::contrib::parse_proto {

template <typename T>
absl::Status ParseTextProtoInto(absl::string_view input, T* proto) {
  if (google::protobuf::TextFormat::ParseFromString(input, proto))
    return absl::OkStatus();
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      "Could not parse the text proto\n");
}

template <typename T>
absl::StatusOr<T> ParseTextProto(absl::string_view asciipb) {
  T msg;
  RETURN_IF_ERROR(ParseTextProtoInto(asciipb, &msg));
  return msg;
}

template <typename T>
T ParseTextOrDie(absl::string_view input) {
  T result;
  CHECK(google::protobuf::TextFormat::ParseFromString(input, &result));
  return result;
}

namespace text_proto_internal {

class ParseProtoHelper {
 public:
  explicit ParseProtoHelper(absl::string_view asciipb) : asciipb_(asciipb) {}
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
    absl::string_view input) {
  return text_proto_internal::ParseProtoHelper(input);
}

}  // namespace google::protobuf::contrib::parse_proto

#endif  // ORTOOLS_BASE_PARSE_TEXT_PROTO_H_
