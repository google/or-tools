// Copyright 2010-2018 Google LLC
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
  z_stream zs;  // z_stream is zlib's control structure
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  zs.next_in = Z_NULL;
  zs.avail_in = 0;
  zs.next_out = Z_NULL;
  constexpr int window_bits = 15 + 32;
  if (inflateInit2(&zs, window_bits) != Z_OK) {
    return false;
  }

  zs.next_in = (Bytef*)str.data();
  zs.avail_in = str.size();

  int ret;
  char outbuffer[32768];

  // Decompress string by block.
  do {
    zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
    zs.avail_out = sizeof(outbuffer);

    ret = inflate(&zs, 0);

    if (out->size() < zs.total_out) {
      out->append(outbuffer, zs.total_out - out->size());
    }
  } while (ret == Z_OK);

  inflateEnd(&zs);

  if (ret != Z_STREAM_END) {  // an error occurred that was not EOF
    return false;
  }

  return true;
}

#endif  // OR_TOOLS_BASE_GZIPSTRING_H_
