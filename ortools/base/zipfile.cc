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

// The zip file is treated a single-level read-only directory.
// The subfiles that when unzipped would be directories
// appear to File as zero-length files.
// This implementation supports only uncompressed and deflate-compressed
// content (compression method == 8).

#include "ortools/base/zipfile.h"

#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "zlib.h"

namespace zipfile {

class ZipFile;
class ZipArchive;

std::shared_ptr<ZipArchive> OpenZipArchive(absl::string_view path,
                                           const ZipFileOptions& options) {
  // unimplemented
  LOG(INFO) << "not implemented";
  return nullptr;
}

ZipArchive::ZipArchive(absl::string_view path, const ZipFileOptions& options)
    : filename_(path), options_(options) {}

ZipArchive::~ZipArchive() {}

}  // namespace zipfile
