// Copyright 2010-2014 Google
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

// Hash maps and hash sets are compiler dependent.
#if defined(__GNUC__) && !defined(STLPORT)
#include <ext/hash_map>
#include <ext/hash_set>
namespace operations_research {
using namespace __gnu_cxx;  // NOLINT
}  // namespace operations_research
#else
#include <hash_map>
#include <hash_set>
#endif

#include <array>
#include <string>
#include <utility>

#include "base/basictypes.h"

// In SWIG mode, we don't want anything besides these top-level includes.
#if !defined(SWIG)

namespace operations_research {
// 32 bit version.
static inline void mix(uint32& a, uint32& b, uint32& c) {  // NOLINT
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
static inline void mix(uint64& a, uint64& b, uint64& c) {  // NOLINT
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
inline uint32 Hash32NumWithSeed(uint32 num, uint32 c) {
  uint32 b = 0x9e3779b9UL;  // The golden ratio; an arbitrary value.
  operations_research::mix(num, b, c);
  return c;
}

inline uint64 Hash64NumWithSeed(uint64 num, uint64 c) {
  uint64 b = GG_ULONGLONG(0xe08c1d668b756f82);  // More of the golden ratio.
  operations_research::mix(num, b, c);
  return c;
}
}  // namespace operations_research

// --------------------------------------------------------------------------
// GNU C++ port, with or without STLport.
// --------------------------------------------------------------------------
#ifdef __GNUC__
// hash namespace
#if defined(__GNUC__) && defined(STLPORT)
#define HASH_NAMESPACE std
#elif defined(__GNUC__) && !defined(STLPORT)
#define HASH_NAMESPACE __gnu_cxx
#endif

// Support a few hash<> operators, in the hash namespace.
namespace HASH_NAMESPACE {
template <class First, class Second>
struct hash<std::pair<First, Second>> {
  size_t operator()(const std::pair<First, Second>& p) const {
    size_t h1 = hash<First>()(p.first);
    size_t h2 = hash<Second>()(p.second);
    // The decision below is at compile time
    return (sizeof(h1) <= sizeof(uint32))
               ?  // NOLINT
               operations_research::Hash32NumWithSeed(h1, h2)
               : operations_research::Hash64NumWithSeed(h1, h2);
  }
};

template <class T>
struct hash<T*> {
  size_t operator()(T* x) const { return reinterpret_cast<size_t>(x); }
};

// hash<int64> and hash<std::string> are already defined with STLport.
#ifndef STLPORT
template <>
struct hash<int64> {
  size_t operator()(int64 x) const { return static_cast<size_t>(x); }
};

template <>
struct hash<uint64> {
  size_t operator()(uint64 x) const { return static_cast<size_t>(x); }
};

template <>
struct hash<const std::string> {
  size_t operator()(const std::string& x) const {
    size_t hash = 0;
    int c;
    const char* s = x.c_str();
    while ((c = *s++)) {  // Extra () to remove a warning on Windows.
      hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
  }
};

template <>
struct hash<std::string> {
  size_t operator()(const std::string& x) const { return hash<const std::string>()(x); }
};

template <class T, std::size_t N>
struct hash<std::array<T, N>> {
 public:
  size_t operator()(const std::array<T, N>& t) const {
    uint64 current = 71;
    for (int index = 0; index < N; ++index) {
      const T& elem = t[index];
      const uint64 new_hash = hash<T>()(elem);
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
#endif  // STLPORT
}  // namespace HASH_NAMESPACE

using HASH_NAMESPACE::hash;
using HASH_NAMESPACE::hash_map;
using HASH_NAMESPACE::hash_set;
#endif  // __GNUC__

#undef HASH_NAMESPACE

// --------------------------------------------------------------------------
// Microsoft Visual C++ port
// --------------------------------------------------------------------------
#ifdef _MSC_VER
// TODO(user): Nuke this section and merge with the gcc version.
// The following class defines a hash function for std::pair<int64, int64>.
template <class T>
class TypedIntHasher : public stdext::hash_compare<T> {
 public:
  size_t operator()(const T& a) const { return static_cast<size_t>(a.value()); }
  bool operator()(const T& a1, const T& a2) const {
    return a1.value() < a2.value();
  }
};

class PairInt64Hasher : public stdext::hash_compare<std::pair<int64, int64>> {
 public:
  size_t operator()(const std::pair<int64, int64>& a) const {
    uint64 x = a.first;
    uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
    uint64 z = a.second;
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<int64, int64>& a1,
                  const std::pair<int64, int64>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};

class PairIntHasher : public stdext::hash_compare<std::pair<int, int>> {
 public:
  size_t operator()(const std::pair<int, int>& a) const {
    uint32 x = a.first;
    uint32 y = 0x9e3779b9UL;
    uint32 z = a.second;
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<int, int>& a1,
                  const std::pair<int, int>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};

class PairPairInt64Hasher
    : public stdext::hash_compare<std::pair<std::pair<int64, int64>, int64>> {
 public:
  size_t operator()(const std::pair<std::pair<int64, int64>, int64>& a) const {
    uint64 x = a.first.first;
    uint64 y = a.first.second;
    uint64 z = a.second;
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<std::pair<int64, int64>, int64>& a1,
                  const std::pair<std::pair<int64, int64>, int64>& a2) const {
    return a1.first.first < a2.first.first ||
           (a1.first.first == a2.first.first &&
            a1.first.second < a2.first.second) ||
           (a1.first.first == a2.first.first &&
            a1.first.second == a2.first.second && a1.second < a2.second);
  }
};

// The following class defines a hash function for std::pair<int64, int64>.
class PairIntInt64Hasher : public stdext::hash_compare<std::pair<int, int64>> {
 public:
  size_t operator()(const std::pair<int, int64>& a) const {
    uint64 x = a.first;
    uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
    uint64 z = a.second;
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<int, int64>& a1,
                  const std::pair<int, int64>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};

// The following class defines a hash function for std::pair<T*, int>.
#if defined(_WIN64)
template <class T>
class PairPointerIntHasher : public stdext::hash_compare<std::pair<T*, int>> {
 public:
  size_t operator()(const std::pair<T*, int>& a) const {
    uint64 x = reinterpret_cast<uint64>(a.first);
    uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
    uint64 z = static_cast<uint64>(a.second);
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<T*, int>& a1,
                  const std::pair<T*, int>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};

template <class T, class U>
class PairPointerIntTypeHasher : public stdext::hash_compare<std::pair<T*, U>> {
 public:
  size_t operator()(const std::pair<T*, U>& a) const {
    uint64 x = reinterpret_cast<uint64>(a.first);
    uint64 y = GG_ULONGLONG(0xe08c1d668b756f82);
    uint64 z = static_cast<uint64>(a.second.Value());
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<T*, U>& a1,
                  const std::pair<T*, U>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};
#else
template <class T>
class PairPointerIntHasher : public stdext::hash_compare<std::pair<T*, int>> {
 public:
  size_t operator()(const std::pair<T*, int>& a) const {
    uint32 x = reinterpret_cast<uint32>(a.first);
    uint32 y = 0x9e3779b9UL;
    uint32 z = static_cast<uint32>(a.second);
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<T*, int>& a1,
                  const std::pair<T*, int>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};

template <class T, class U>
class PairPointerIntTypeHasher : public stdext::hash_compare<std::pair<T*, U>> {
 public:
  size_t operator()(const std::pair<T*, U>& a) const {
    uint32 x = reinterpret_cast<uint32>(a.first);
    uint32 y = 0x9e3779b9UL;
    uint32 z = static_cast<uint32>(a.second.Value());
    operations_research::mix(x, y, z);
    return z;
  }
  bool operator()(const std::pair<T*, U>& a1,
                  const std::pair<T*, U>& a2) const {
    return a1.first < a2.first ||
           (a1.first == a2.first && a1.second < a2.second);
  }
};
#endif

template <class T, std::size_t N>
struct StdArrayHasher : public stdext::hash_compare<std::array<T, N>> {
 public:
  size_t operator()(const std::array<T, N>& t) const {
    uint64 current = 71;
    for (int index = 0; index < N; ++index) {
      const T& elem = t[index];
      const uint64 new_hash = hash<T>()(elem);
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

#if defined(_MSC_VER)
namespace stdext {
template <>
inline size_t hash_value<std::pair<int64, int64>>(
    const std::pair<int64, int64>& a) {
  PairIntHasher pairIntHasher;
  return pairIntHasher(a);
}
}  //  namespace stdext
#endif  // _MSC_VER

using std::hash;
using stdext::hash_map;
using stdext::hash_set;
#endif  // _MSC_VER

#endif  // SWIG

#if !defined(STLPORT)
// Support a few hash<> operators, in the std namespace.
namespace std {
template <class First, class Second>
struct hash<pair<First, Second>> {
  size_t operator()(const pair<First, Second>& p) const {
    size_t h1 = hash<First>()(p.first);
    size_t h2 = hash<Second>()(p.second);
    // The decision below is at compile time
    return (sizeof(h1) <= sizeof(uint32))
               ?  // NOLINT
               operations_research::Hash32NumWithSeed(h1, h2)
               : operations_research::Hash64NumWithSeed(h1, h2);
  }
};

template <>
struct hash<const string> {
  size_t operator()(const string& x) const {
    size_t hash = 0;
    int c;
    const char* s = x.c_str();
    while ((c = *s++)) {  // Extra () to remove a warning on Windows.
      hash = ((hash << 5) + hash) ^ c;
    }
    return hash;
  }
};

template <class T, size_t N>
struct hash<array<T, N>> {
 public:
  size_t operator()(const array<T, N>& t) const {
    uint64 current = 71;
    for (int index = 0; index < N; ++index) {
      const T& elem = t[index];
      const uint64 new_hash = hash<T>()(elem);
      current = operations_research::Hash64NumWithSeed(current, new_hash);
    }
    return current;
  }
  // Less than operator for MSVC.
  bool operator()(const array<T, N>& a, const array<T, N>& b) const {
    return a < b;
  }
  static const size_t bucket_size = 4;  // These are required by MSVC.
  static const size_t min_buckets = 8;  // 4 and 8 are defaults.
};
}  // namespace std
#endif  // !STLPORT

#endif  // OR_TOOLS_BASE_HASH_H_
