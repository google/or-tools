// Copyright 2010-2021 Google LLC
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

#include "ortools/base/basictypes.h"

// In SWIG mode, we don't want anything besides these top-level includes.
#if !defined(SWIG)

namespace operations_research {
// 32 bit version.
static inline void mix(uint32_t& a, uint32_t& b, uint32_t& c) {  // NOLINT
  a -= b;
  a -= c;
  a ^= (c >> 13);
  b -= c;
  b -= a;
  b ^= (a << 8);
  c -= a;
  c -= b;
  c ^= (b >> 13);
  a -= b;
  a -= c;
  a ^= (c >> 12);
  b -= c;
  b -= a;
  b ^= (a << 16);
  c -= a;
  c -= b;
  c ^= (b >> 5);
  a -= b;
  a -= c;
  a ^= (c >> 3);
  b -= c;
  b -= a;
  b ^= (a << 10);
  c -= a;
  c -= b;
  c ^= (b >> 15);
}

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
inline uint32_t Hash32NumWithSeed(uint32_t num, uint32_t c) {
  uint32_t b = 0x9e3779b9UL;  // The golden ratio; an arbitrary value.
  operations_research::mix(num, b, c);
  return c;
}

inline uint64_t Hash64NumWithSeed(uint64_t num, uint64_t c) {
  uint64_t b = uint64_t{0xe08c1d668b756f82};  // More of the golden ratio.
  operations_research::mix(num, b, c);
  return c;
}
}  // namespace operations_research

// Support a few hash<> operators, in the hash namespace.
namespace std {
template <class First, class Second>
struct hash<std::pair<First, Second>> {
  size_t operator()(const std::pair<First, Second>& p) const {
    size_t h1 = hash<First>()(p.first);
    size_t h2 = hash<Second>()(p.second);
    // The decision below is at compile time
    return (sizeof(h1) <= sizeof(uint32_t))
               ?  // NOLINT
               operations_research::Hash32NumWithSeed(h1, h2)
               : operations_research::Hash64NumWithSeed(h1, h2);
  }
};

template <class T, std::size_t N>
struct hash<std::array<T, N>> {
 public:
  size_t operator()(const std::array<T, N>& t) const {
    uint64_t current = 71;
    for (int index = 0; index < N; ++index) {
      const T& elem = t[index];
      const uint64_t new_hash = hash<T>()(elem);
      current = operations_research::Hash64NumWithSeed(current, new_hash);
    }
    return current;
  }
  // Less than operator for MSVC.
  bool operator()(const std::array<T, N>& a, const std::array<T, N>& b) const {
    return a < b;
  }
  static const size_t bucket_size = 4;  // These are required by MSVC.
  static const size_t min_buckets = 8;  // 4 and 8 are defaults.
};
}  // namespace std

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
