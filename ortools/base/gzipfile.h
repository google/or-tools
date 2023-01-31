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

#ifndef OR_TOOLS_BASE_GZIPFILE_H_
#define OR_TOOLS_BASE_GZIPFILE_H_

#include "absl/strings/string_view.h"
#include "ortools/base/basictypes.h"  // for Ownership enum
#include "zlib.h"                     // for Z_DEFAULT_COMPRESSION

class File;

// Argument type used in interfaces that can optionally accept appended
// compressed streams.  If kConcatenateStreams is passed, the output will
// include all streams.  Otherwise only the first stream is output.
enum class AppendedStreams {
  kConcatenateStreams,
  kIgnoreAppendedData,
};

// Argument type used in interfaces that can optionally take ownership
// of a passed in argument.  If TAKE_OWNERSHIP is passed, the called
// object takes ownership of the argument.  Otherwise it does not.
enum Ownership { DO_NOT_TAKE_OWNERSHIP, TAKE_OWNERSHIP };

// Return a readonly file that contains a uncompressed version of
// another File.
//
// If "ownership == TAKE_OWNERSHIP", the file takes ownership of
// "compressed_file".  "compressed_file" will be deleted when the file is
// closed.
//
// If "ownership == "DO_NOT_TAKE_OWNERSHIP", the file does not take
// ownership of "compressed_file".  The client must guarantee that
// "compressed_file" remains live while the file is being accessed and
// that operations on "compressed_file" do not interfere (e.g., move
// the read pointer) with the file.
//
// If "appended_streams" == "kConcatenateStreams" decompression will process
// and output each compressed stream present in the input.
//
// If "appended_streams" == "kIgnoreAppendedData" decompression will stop
// after the first stream.
//
// The compressed file may be either a gzip format file, a zlib compressed
// file, or a unix compress format file.  The only difference between gzip
// and zlib is whether the file has a gzip header and trailer.  If the file
// consists of multiple gzip files concatenated together, then we will
// decompress them into a single concatenated output.  This may seem weird
// but that's what the gzip commandline tool does.  If we didn't do this
// we would silently truncate some input files upon decompression.
// Otherwise we signal an error for any invalid data after the compressed
// stream.
File* GZipFileReader(absl::string_view name, File* compressed_file,
                     Ownership ownership, AppendedStreams appended_streams);
// appended_streams defaults to kConcatenateStreams if not specified.
inline File* GZipFileReader(absl::string_view name, File* compressed_file,
                            Ownership ownership) {
  return GZipFileReader(name, compressed_file, ownership,
                        AppendedStreams::kConcatenateStreams);
}

#endif  // OR_TOOLS_BASE_GZIPFILE_H_
