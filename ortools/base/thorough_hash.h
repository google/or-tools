// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_THOROUGH_HASH_H_
#define OR_TOOLS_BASE_THOROUGH_HASH_H_

#include "ortools/base/integral_types.h"

namespace operations_research {
inline uint64 MixTwoUInt64(uint64 fp1, uint64 fp2) {
  // Two big prime numbers.
  const uint64 kMul1 = 0xc6a4a7935bd1e995ULL;
  const uint64 kMul2 = 0x228876a7198b743ULL;
  uint64 a = fp1 * kMul1 + fp2 * kMul2;
  // Note: The following line also makes sure we never return 0 or 1, because we
  // will only add something to 'a' if there are any MSBs (the remaining bits
  // after the shift) being 0, in which case wrapping around would not happen.
  return a + (~a >> 47);
}

// This should be better (collision-wise) than the default hash<std::string>, without
// being much slower. It never returns 0 or 1.
inline uint64 ThoroughHash(const char* bytes, size_t len) {
  // Some big prime numer.
  uint64 fp = 0xa5b85c5e198ed849ULL;
  const char* end = bytes + len;
  while (bytes + 8 <= end) {
    fp = MixTwoUInt64(fp, *(reinterpret_cast<const uint64*>(bytes)));
    bytes += 8;
  }
  // Note: we don't care about "consistency" (little or big endian) between
  // the bulk and the suffix of the message.
  uint64 last_bytes = 0;
  while (bytes < end) {
    last_bytes += *bytes;
    last_bytes <<= 8;
    bytes++;
  }
  return MixTwoUInt64(fp, last_bytes);
}
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_THOROUGH_HASH_H_
