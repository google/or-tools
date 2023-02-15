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

// Interface for mapping the contents of a zipfile.
// You open a zip file archive by using the OpenZipArchive() method, which
// returns a shared_ptr to a ZipArchive object that maps its contents into the
// "/zip/" namespace. During the duration of the existence of the ZipArchive
// object, paths under it are available for File operations.
//
// Only a single ZipArchive will be opened for any unique path; all of the
// returned shared_ptrs will point to the same underlying ZipArchive object.
// The contents of the ZipArchive will remain mapped into the /zip/ namespace
// until the last shared_ptr is destroyed and its underlying object is
// released.
//
// To map the contents of zip file "/foo/bar.zip", use the following:
// {
//   std::shared_ptr<zipfile::ZipArchive> zip_archive(
//       zipfile::OpenZipArchive("/foo/bar.zip"));
//   if (zip_archive == nullptr) {
//     // Handle error
//   }
//
//   // Do interesting things with paths under "/zip/foo/bar.zip/..."
// }
//
// Paths in the zip file are available until the last shared_ptr to the
// ZipArchive object is released.
//
// You can use this pattern to read files from a Zip archive:
// {
//   std::shared_ptr<zipfile::ZipArchive> zip_archive(
//       zipfile::OpenZipArchive(filename));
//   if (zip_archive == nullptr) {
//      // Do not continue execution of the example code.
//   }
//   vector<string> files;
//   absl::Status status = file::Match(file::JoinPath("/zip", filename, "*"),
//                                     &files, file::Defaults());
//   if (!status.ok()) {
//     // Do not continue execution.
//   }
//   for (const string& fn : files) {
//     File* f;
//     absl::Status status = file::Open(fn, "r", &f, file::Defaults());
//     if (!status.ok()) {
//       // Handle error: abort, return, or continue.
//     }
//     DoStuffWithFile(f);
//     status = f->Close();
//     if (!status.ok()) {
//       // Handle error: abort, return, or continue.
//     }
//   }
// }

#ifndef OR_TOOLS_BASE_ZIPFILE_H_
#define OR_TOOLS_BASE_ZIPFILE_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace zipfile {

// Performance hint: Specify what order you expect to visit the data
// in the file.
enum class AccessPattern {
  // Keep whatever is default for your system (typically same as NORMAL).
  ACCESS_NONE = 0,
  // Moderate prefetching as the file gets accessed.
  ACCESS_NORMAL = 1,
  // No prefetching to be done, use this for random access.
  ACCESS_RANDOM = 2,
  // Aggressive prefetching on reads.
  ACCESS_SEQUENTIAL = 3,
};

class ZipArchive;

// An options struct to be provided when opening a zip archive:
//
// `access_pattern`, possible values:
//  NORMAL: Will use a small InputBuffer to cache small files, raw access to
//          big files.
//  RANDOM: Do not do any caching at all.
//  SEQUENTIAL: Always use a (big) InputBuffer to cache read access.
//  SEQUENTIAL mode
//
// `zip_bomb_max_ratio`, if greater than zero, indicates the maximum
// compression ratio to allow on any file in the zip archive. This can be used
// to reject files containing decompression bombs. A value of 0 disables zip
// bomb checking.
//
// `zip_bomb_max_size`, if greater than zero, indicates the maximum
// decompressed file size that is allowed on any file in the zip archive. This
// is an additional layer of protection against zip bombs in addition to the
// zip_bomb_max_ratio
struct ZipFileOptions {
  AccessPattern access_pattern = AccessPattern::ACCESS_NONE;
  std::optional<size_t> zip_bomb_max_ratio = std::nullopt;
  std::optional<size_t> zip_bomb_max_size = std::nullopt;
};

// Open and return a ZipArchive.  This maps the files in PATH into the /zip/
// namespace, and they will exist there until the ZipArchive is destroyed.
// If the archive is already opened, or opening it fails for some reason,
// nullptr will be returned.
//
// The path given to OpenZipArchive must be absolute.
// The ZipArchive can only be used for reading zipfile contents.  To modify
// zip file contents, use ZipFileWriter from file/zipfile/zipfilewriter.h.
//
// The value of the returned shared_ptr may be nullptr if there is an
// error opening the archive.  This may be caused by corruption, errors
// finding or opening the file, or any other problem which prevents the archive
// from being read.
//
// Note: Writing to an open ZipArchive will fail in strange and mysterious
// ways.  You have been warned.
// The input options argument provides controls over zip bombs as well as
// access pattern controls
std::shared_ptr<ZipArchive> OpenZipArchive(absl::string_view path,
                                           const ZipFileOptions& options);

inline std::shared_ptr<ZipArchive> OpenZipArchive(absl::string_view path) {
  ZipFileOptions options;
  return OpenZipArchive(path, options);
}

// A zip archive, which may contain files that can be read through the /zip
// filename prefix.
//
// Do not instantiate this class directly.  Use the OpenZipArchive factory
// methods instead.
class ZipArchive {
 public:
  ZipArchive(absl::string_view path, const ZipFileOptions& options);

  ZipArchive(const ZipArchive&) = delete;
  ZipArchive& operator=(const ZipArchive&) = delete;

  // Returns the filename at which this archive was first opened.  Since all
  // equivalent zip archive paths share the same archive, this name will not
  // necessarily match the name at which the archive was opened.  For example,
  // we provide no guarantees about whether the filename will begin with "/zip",
  // since the "/zip" prefix is optional for opening zip archives.
  const std::string& filename() const { return filename_; }

  ~ZipArchive();

 private:
  std::string filename_;
  ZipFileOptions options_;
};

};  // namespace zipfile

#endif  // OR_TOOLS_BASE_ZIPFILE_H_
