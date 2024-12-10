// Copyright 2010-2024 Google LLC
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

// Files which read from and write to a gzip'ed files.
// This could be optimized, most notably by eliminating copies in many
// common cases.

#include "ortools/base/gzipfile.h"

#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"

// public entry points
File* GZipFileReader(const absl::string_view name, File* file,
                     Ownership ownership, AppendedStreams appended_streams) {
  // unimplemented
  LOG(INFO) << "not implemented";
  return nullptr;
}

absl::Status WriteToGzipFile(const absl::string_view filename,
                             const absl::string_view contents) {
  // 1 for fastest compression.
  gzFile gz_file = gzopen(filename.data(), "w1");
  if (gz_file == Z_NULL) {
    return {absl::StatusCode::kInternal, "Unable to open file"};
  }

  // have to write in chunks, otherwise fails to compress big files.
  constexpr size_t chunk = 1024 * 1024; // 1 MB chunk size.
  size_t remaining = contents.size();
  const char* chunk_start_point = contents.data();

  while (remaining > 0) {
    size_t current_chunk = std::min(chunk, remaining);
    size_t written = gzwrite(gz_file, chunk_start_point, current_chunk);
    if (written != current_chunk) {
      int err_no = 0;
      absl::string_view error_message = gzerror(gz_file, &err_no);
      gzclose(gz_file);
      return {
        absl::StatusCode::kInternal,
        absl::StrCat(
          "Error while writing chunk to compressed file:",
          error_message
          )
      };
    }

    chunk_start_point += current_chunk;
    remaining -= current_chunk;
  }

  if (const int status = gzclose(gz_file); status != Z_OK) {
    int err_no;
    absl::string_view error_message = gzerror(gz_file, &err_no);
    return {
      absl::StatusCode::kInternal,
      absl::StrCat(
          "Unable to close the file:",
          error_message
        )
    };
  }

  return absl::OkStatus();
}
