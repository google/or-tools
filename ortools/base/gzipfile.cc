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

// Files which read from and write to a gzip'ed files.
// This could be optimized, most notably by eliminating copies in many
// common cases.

#include "ortools/base/gzipfile.h"

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"

// public entry points
File* GZipFileReader(const absl::string_view name, File* file,
                     Ownership ownership, AppendedStreams appended_streams) {
  // unimplemented
  LOG(INFO) << "not implemented";
  return nullptr;
}
