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

#ifndef OR_TOOLS_UTIL_PARSE_PROTO_H_
#define OR_TOOLS_UTIL_PARSE_PROTO_H_

#include <string>

#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"

namespace operations_research {

// Tries to parse `text` as a text format proto. On a success, stores the result
// in `message_out` and returns true, otherwise, returns `false` with an
// explanation in `error_out`.
//
// NOTE: this API is optimized for implementing AbslParseFlag(). The error
// message will be multiline and is designed to be easily read when printed.
bool ParseTextProtoForFlag(absl::string_view text,
                           google::protobuf::Message* message_out,
                           std::string* error_out);

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_PARSE_PROTO_H_
