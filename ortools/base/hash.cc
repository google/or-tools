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

#include "ortools/base/hash.h"

namespace operations_research {

// This is a copy of the code from https://github.com/ztanml/fast-hash
// We include it here for build simplicity.
//
// We have made 2 modification:
//   - replace the #define with an inline method to fix compilation on windows.
//   - rename mix into mix_internal and hide inside a unnamed namespace.

namespace {
// Compression function for Merkle-Damgard construction.
// This function is generated using the framework provided.
inline uint64_t mix_internal(uint64_t h) {
  h ^= h >> 23;
  h *= 0x2127599bf4325c37ULL;
  h ^= h >> 47;
  return h;
}
}  // namespace

uint64_t fasthash64(const void* buf, size_t len, uint64_t seed) {
  const uint64_t m = 0x880355f21e6d1965ULL;
  const uint64_t* pos = (const uint64_t*)buf;
  const uint64_t* end = pos + (len / 8);
  const unsigned char* pos2;
  uint64_t h = seed ^ (len * m);
  uint64_t v;

  while (pos != end) {
    v = *pos++;
    h ^= mix_internal(v);
    h *= m;
  }

  pos2 = (const unsigned char*)pos;
  v = 0;

  switch (len & 7) {
    case 7:
      v ^= (uint64_t)pos2[6] << 48;
    case 6:
      v ^= (uint64_t)pos2[5] << 40;
    case 5:
      v ^= (uint64_t)pos2[4] << 32;
    case 4:
      v ^= (uint64_t)pos2[3] << 24;
    case 3:
      v ^= (uint64_t)pos2[2] << 16;
    case 2:
      v ^= (uint64_t)pos2[1] << 8;
    case 1:
      v ^= (uint64_t)pos2[0];
      h ^= mix_internal(v);
      h *= m;
  }

  return mix_internal(h);
}

}  // namespace operations_research
