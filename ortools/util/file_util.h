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

#ifndef ORTOOLS_UTIL_FILE_UTIL_H_
#define ORTOOLS_UTIL_FILE_UTIL_H_

#include <string>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

// Reads a file, optionally gzipped, to a string.
absl::StatusOr<std::string> ReadFileToString(absl::string_view filename);

// Reads a proto from a file. Supports the following formats: binary, text,
// JSON, all of those optionally gzipped. Returns errors as expected: filesystem
// error, parsing errors, or type error: maybe it was a valid JSON, text proto,
// or binary proto, but not of the right proto message (this is not an exact
// science, but the heuristics used should work well in practice).
absl::Status ReadFileToProto(
    absl::string_view filename, google::protobuf::Message* proto,
    // If true, unset required fields don't cause errors. This
    // boolean doesn't work for JSON inputs.
    bool allow_partial = false);

// Exactly like ReadFileToProto(), but directly from the contents.
absl::Status StringToProto(absl::string_view data,
                           google::protobuf::Message* proto,
                           bool allow_partial = false);

template <typename Proto>
absl::StatusOr<Proto> ReadFileToProto(absl::string_view filename,
                                      bool allow_partial = false) {
  Proto proto;
  OR_RETURN_IF_ERROR(ReadFileToProto(filename, &proto, allow_partial))
      << "filename=" << filename;
  return proto;
}

// Specifies how the proto should be formatted when writing it to a file.
// kCanonicalJson converts field names to lower camel-case.
enum class ProtoWriteFormat { kProtoText, kProtoBinary, kJson, kCanonicalJson };

// Writes a proto to a file. Supports the following formats: binary, text, JSON,
// all of those optionally gzipped.
// If 'proto_write_format' is kProtoBinary, ".bin" is appended to file_name. If
// 'proto_write_format' is kJson or kCanonicalJson, ".json" is appended to
// file_name. If 'gzipped' is true, ".gz" is appended to file_name.
absl::Status WriteProtoToFile(absl::string_view filename,
                              const google::protobuf::Message& proto,
                              ProtoWriteFormat proto_write_format,
                              bool gzipped = false,
                              bool append_extension_to_file_name = true);

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FILE_UTIL_H_
