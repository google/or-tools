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

#ifndef OR_TOOLS_BASE_HASH_H_
#define OR_TOOLS_BASE_HASH_H_

#include <array>
#include <string>
#include <utility>

#include "ortools/base/integral_types.h"

// In SWIG mode, we don't want anything besides these top-level includes.
#if !defined(SWIG)

namespace operations_research {
uint64_t fasthash64(const void* buf, size_t len, uint64_t seed);

// 64 bit version.
static inline void mix(uint64_t& a, uint64_t& b, uint64_t& c) {  // NOLINT
  a -= b;
  a -= c;
  a ^= (c >> 43);
  b -= c;
  b -= a;
  b ^= (a << 9);
  c -= a;
  c -= b;
  c ^= (b >> 8);
  a -= b;
  a -= c;
  a ^= (c >> 38);
  b -= c;
  b -= a;
  b ^= (a << 23);
  c -= a;
  c -= b;
  c ^= (b >> 5);
  a -= b;
  a -= c;
  a ^= (c >> 35);
  b -= c;
  b -= a;
  b ^= (a << 49);
  c -= a;
  c -= b;
  c ^= (b >> 11);
  a -= b;
  a -= c;
  a ^= (c >> 12);
  b -= c;
  b -= a;
  b ^= (a << 18);
  c -= a;
  c -= b;
  c ^= (b >> 22);
}
}  // namespace operations_research

#endif  // SWIG

namespace util_hash {

inline uint64_t Hash(uint64_t num, uint64_t c) {
  uint64_t b = uint64_t{0xe08c1d668b756f82};  // More of the golden ratio.
  operations_research::mix(num, b, c);
  return c;
}

inline uint64_t Hash(uint64_t a, uint64_t b, uint64_t c) {
  operations_research::mix(a, b, c);
  return c;
}

}  // namespace util_hash

#endif  // OR_TOOLS_BASE_HASH_H_
