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

#ifndef ORTOOLS_BASE_HELPERS_H_
#define ORTOOLS_BASE_HELPERS_H_

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "ortools/base/file.h"
#include "ortools/base/status_macros.h"

namespace file {

// ---- Content API ----

absl::StatusOr<std::string> GetContents(absl::string_view path,
                                        Options options);

absl::Status GetContents(absl::string_view file_name, std::string* output,
                         Options options);

absl::Status SetContents(absl::string_view file_name,
                         absl::string_view contents, Options options);

absl::Status WriteString(File* file, absl::string_view contents,
                         Options options);

absl::StatusOr<int64_t> GetSize(absl::string_view path,
                                const file::Options& options);

// ---- Protobuf API ----

absl::Status GetTextProto(absl::string_view file_name,
                          google::protobuf::Message* proto, Options options);

template <typename T>
absl::StatusOr<T> GetTextProto(absl::string_view file_name, Options options) {
  T proto;
  OR_RETURN_IF_ERROR(GetTextProto(file_name, &proto, options));
  return proto;
}

absl::Status SetTextProto(absl::string_view file_name,
                          const google::protobuf::Message& proto,
                          Options options);

absl::Status GetBinaryProto(absl::string_view file_name,
                            google::protobuf::Message* proto, Options options);
template <typename T>

absl::StatusOr<T> GetBinaryProto(absl::string_view file_name, Options options) {
  T proto;
  OR_RETURN_IF_ERROR(GetBinaryProto(file_name, &proto, options));
  return proto;
}

absl::Status SetBinaryProto(absl::string_view file_name,
                            const google::protobuf::Message& proto,
                            Options options);

}  // namespace file

#endif  // ORTOOLS_BASE_HELPERS_H_
