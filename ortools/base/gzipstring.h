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

#ifndef OR_TOOLS_BASE_GZIPSTRING_H_
#define OR_TOOLS_BASE_GZIPSTRING_H_

#include <string>

#include "ortools/base/logging.h"
#include "zlib.h"

bool GunzipString(const std::string& str, std::string* out) {
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  zs.next_in = Z_NULL;
  zs.avail_in = 0;
  zs.next_out = Z_NULL;
  if (inflateInit2(&zs, /*window_bits=*/15 + 32) != Z_OK) {
    return false;
  }

  zs.next_in = (Bytef*)str.data();
  zs.avail_in = str.size();

  int status;
  char buffer[32768];

  // Decompress string by block.
  do {
    zs.next_out = reinterpret_cast<Bytef*>(buffer);
    zs.avail_out = sizeof(buffer);

    status = inflate(&zs, 0);

    if (out->size() < zs.total_out) {
      out->append(buffer, zs.total_out - out->size());
    }
  } while (status == Z_OK);

  inflateEnd(&zs);

  if (status != Z_STREAM_END) {  // an error occurred that was not EOF
    VLOG(1) << "Exception during zlib decompression: (" << status << ") "
            << zs.msg;
    return false;
  }

  return true;
}

void GzipString(absl::string_view uncompressed, std::string* compressed) {
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  zs.next_in = Z_NULL;
  zs.avail_in = 0;
  zs.next_out = Z_NULL;

  if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
    VLOG(1) << "Cannot initialize zlib compression.";
    return;
  }

  zs.next_in = (Bytef*)uncompressed.data();
  zs.avail_in = uncompressed.size();  // set the z_stream's input

  int status;
  char buffer[32768];

  // compress block by block.
  do {
    zs.next_out = reinterpret_cast<Bytef*>(buffer);
    zs.avail_out = sizeof(buffer);

    status = deflate(&zs, Z_FINISH);

    if (compressed->size() < zs.total_out) {
      compressed->append(buffer, zs.total_out - compressed->size());
    }
  } while (status == Z_OK);

  deflateEnd(&zs);

  if (status != Z_STREAM_END) {  // an error occurred that was not EOF
    VLOG(1) << "Exception during zlib compression: (" << status << ") "
            << zs.msg;
  }
}

#endif  // OR_TOOLS_BASE_GZIPSTRING_H_
