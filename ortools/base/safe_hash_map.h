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

#ifndef ORTOOLS_BASE_SAFE_HASH_MAP_H_
#define ORTOOLS_BASE_SAFE_HASH_MAP_H_

// This is a copy of emhash7::HashMap for C++11/14/17.
// It is used to replace absl::flat_hash_map in OR-Tools to provide an unsalted
// hash map that works across different shared libraries.
//
// The following modifications have been made:
//  * change namespace from emhash to operations_research
//  * change the name of the class from HashMap to safe_hash_map
//  * removed the WY_HASH option
//  * reformated the code to match OR-Tools style
//  * removed optional code (REHASH_LOG, STATIS)
//
// emhash7::HashMap for C++11/14/17
// version 2.2.5
// https://github.com/ktprime/emhash/blob/master/hash_table7.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2020-2026 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

// From
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution  1 - ln(1-L) - L/2     1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
// separator chain resolution      1 + L / 2             exp(-L) + L

// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0
// SEPARATE CHAN RES.
//    probes/successful lookup    1.05  1.25  1.3   1.25  1.4   1.45  1.50
//    probes/unsuccessful lookup  1.00  1.11  1.15  1.22  1.25  1.31  1.37
//    clacul/unsuccessful lookup  1.01  1.25  1.36, 1.56, 1.64, 1.81, 1.97

/****************
    under random hashCodes, the frequency of nodes in bins follows a Poisson
distribution(http://en.wikipedia.org/wiki/Poisson_distribution) with a parameter
of about 0.5 on average for the default resizing threshold of 0.75, although
with a large variance because of resizing granularity. Ignoring variance, the
expected occurrences of list size k are (exp(-0.5) * pow(0.5, k)/factorial(k)).
The first values are: 0: 0.60653066 1: 0.30326533 2: 0.07581633 3: 0.01263606 4:
0.00157952 5: 0.00015795 6: 0.00001316 7: 0.00000094 8: 0.00000006

  ============== buckets size ration ========
    1   1543981  0.36884964|0.36787944  36.885
    2    768655  0.36725597|0.36787944  73.611
    3    256236  0.18364065|0.18393972  91.975
    4     64126  0.06127757|0.06131324  98.102
    5     12907  0.01541710|0.01532831  99.644
    6      2050  0.00293841|0.00306566  99.938
    7       310  0.00051840|0.00051094  99.990
    8        49  0.00009365|0.00007299  99.999
    9         4  0.00000860|0.00000913  100.000
========== collision miss ration ===========
  _num_filled aver_size k.v size_kv = 4185936, 1.58, x.x 24
  collision,possion,cache_miss hit_find|hit_miss, load_factor
= 36.73%,36.74%,31.31%  1.50|2.00, 1.00
============== buckets size ration ========
*******************************************************/

#include <cstddef>
#include <initializer_list>
#include <tuple>
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#ifdef EMH_NEW
#undef EMH_KEY
#undef EMH_VAL
#undef EMH_PKV
#undef EMH_NEW
#undef EMH_SET
#undef EMH_BUCKET
#undef EMH_EMPTY
#undef EMH_MASK
#undef EMH_CLS
#endif

// likely/unlikely
#if defined(__GNUC__) && (__GNUC__ >= 3) && (__GNUC_MINOR__ >= 1) || \
    defined(__clang__)
#define EMH_LIKELY(condition) __builtin_expect(!!(condition), 1)
#define EMH_UNLIKELY(condition) __builtin_expect(!!(condition), 0)
#elif defined(_MSC_VER) && (_MSC_VER >= 1920)
#define EMH_LIKELY(condition) ((condition) ? ((void)__assume(condition), 1) : 0)
#define EMH_UNLIKELY(condition) \
  ((condition) ? 1 : ((void)__assume(!condition), 0))
#else
#define EMH_LIKELY(condition) (condition)
#define EMH_UNLIKELY(condition) (condition)
#endif

#ifndef EMH_BUCKET_INDEX
#define EMH_BUCKET_INDEX 1
#endif

#if EMH_BUCKET_INDEX == 0
#define EMH_KEY(p, n) p[n].second.first
#define EMH_VAL(p, n) p[n].second.second
#define EMH_BUCKET(p, n) p[n].first
#define EMH_PKV(p, n) p[n].second
#define EMH_NEW(key, val, bucket)                            \
  new (_pairs + bucket) PairT(bucket, value_type(key, val)); \
  _num_filled++;                                             \
  EMH_SET(bucket)
#elif EMH_BUCKET_INDEX == 2
#define EMH_KEY(p, n) p[n].first.first
#define EMH_VAL(p, n) p[n].first.second
#define EMH_BUCKET(p, n) p[n].second
#define EMH_PKV(p, n) p[n].first
#define EMH_NEW(key, val, bucket)                            \
  new (_pairs + bucket) PairT(value_type(key, val), bucket); \
  _num_filled++;                                             \
  EMH_SET(bucket)
#else
#define EMH_KEY(p, n) p[n].first
#define EMH_VAL(p, n) p[n].second
#define EMH_BUCKET(p, n) p[n].bucket
#define EMH_PKV(p, n) p[n]
#define EMH_NEW(key, val, bucket)                \
  new (_pairs + bucket) PairT(key, val, bucket); \
  _num_filled++;                                 \
  EMH_SET(bucket)
#endif

#define EMH_MASK(n) (1 << (n % MASK_BIT))
#define EMH_SET(n) _bitmask[n / MASK_BIT] &= (bit_type)(~(1 << (n % MASK_BIT)))
#define EMH_CLS(n) _bitmask[n / MASK_BIT] |= (bit_type)EMH_MASK(n)
#define EMH_EMPTY(n) _bitmask[n / MASK_BIT] & (bit_type)EMH_MASK(n)

#if _WIN32
#include <intrin.h>
#endif

namespace operations_research {

#ifdef EMH_SIZE_TYPE_16BIT
typedef uint16_t size_type;
static constexpr size_type INACTIVE = 0xFFFF;
#elif EMH_SIZE_TYPE_64BIT
typedef uint64_t size_type;
static constexpr size_type INACTIVE = 0 - 0x1ull;
#else
typedef uint32_t size_type;
static constexpr size_type INACTIVE = 0xFFFFFFFF;
#endif

#ifndef EMH_MALIGN
static constexpr uint32_t EMH_MALIGN = 16;
#endif
static_assert(EMH_MALIGN >= 16 && 0 == (EMH_MALIGN & (EMH_MALIGN - 1)));

#ifndef EMH_SIZE_TYPE_16BIT
static_assert((int)INACTIVE < 0, "INACTIVE must negative (to int)");
#endif

// count the leading zero bit
static inline size_type CTZ(size_t n) {
#if defined(__x86_64__) || defined(_WIN32) || \
    (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)

#elif __BIG_ENDIAN__ || \
    (__BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  n = __builtin_bswap64(n);
#else
  static uint32_t endianness = 0x12345678;
  const auto is_big = *(const char*)&endianness == 0x12;
  if (is_big) n = __builtin_bswap64(n);
#endif

#if _WIN32
  unsigned long index;
#if defined(_WIN64)
  _BitScanForward64(&index, n);
#else
  _BitScanForward(&index, n);
#endif
#elif defined(__LP64__) || (SIZE_MAX == UINT64_MAX) || defined(__x86_64__)
  auto index = __builtin_ctzll(n);
#elif 1
  auto index = __builtin_ctzl(n);
#else
#if defined(__LP64__) || (SIZE_MAX == UINT64_MAX) || defined(__x86_64__)
  size_type index;
  __asm__("bsfq %1, %0\n" : "=r"(index) : "rm"(n) : "cc");
#else
  size_type index;
  __asm__("bsf %1, %0\n" : "=r"(index) : "rm"(n) : "cc");
#endif
#endif

  return (size_type)index;
}

template <typename First, typename Second>
struct entry {
  using first_type = First;
  using second_type = Second;
  entry(const First& key, const Second& val, size_type ibucket)
      : second(val), first(key) {
    bucket = ibucket;
  }

  entry(First&& key, Second&& val, size_type ibucket)
      : second(std::move(val)), first(std::move(key)) {
    bucket = ibucket;
  }

  template <typename K, typename V>
  entry(K&& key, V&& val, size_type ibucket)
      : second(std::forward<V>(val)), first(std::forward<K>(key)) {
    bucket = ibucket;
  }

  entry(const std::pair<First, Second>& pair)
      : second(pair.second), first(pair.first) {
    bucket = INACTIVE;
  }

  entry(std::pair<First, Second>&& pair)
      : second(std::move(pair.second)), first(std::move(pair.first)) {
    bucket = INACTIVE;
  }

  entry(std::tuple<First, Second>&& tup)
      : second(std::move(std::get<2>(tup))),
        first(std::move(std::get<1>(tup))) {
    bucket = INACTIVE;
  }

  entry(const entry& rhs) : second(rhs.second), first(rhs.first) {
    bucket = rhs.bucket;
  }

  entry(entry&& rhs) noexcept
      : second(std::move(rhs.second)), first(std::move(rhs.first)) {
    bucket = rhs.bucket;
  }

  entry& operator=(entry&& rhs) noexcept {
    second = std::move(rhs.second);
    bucket = rhs.bucket;
    first = std::move(rhs.first);
    return *this;
  }

  entry& operator=(const entry& rhs) {
    if (this != &rhs)  // not a self-assignment
    {
      second = rhs.second;
      bucket = rhs.bucket;
      first = rhs.first;
    }
    return *this;
  }

  bool operator==(const entry<First, Second>& p) const {
    return first == p.first && second == p.second;
  }

  bool operator==(const std::pair<First, Second>& p) const {
    return first == p.first && second == p.second;
  }

  void swap(entry<First, Second>& o) {
    std::swap(second, o.second);
    std::swap(first, o.first);
  }

#if EMH_ORDER_KV || EMH_SIZE_TYPE_64BIT
  First first;
  size_type bucket;
  Second second;
#else
  Second second;
  size_type bucket;
  First first;
#endif
};

/// A cache-friendly hash table with open addressing, linear/qua probing and
/// power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>,
          typename EqT = std::equal_to<KeyT>>
class safe_hash_map {
#ifndef EMH_DEFAULT_LOAD_FACTOR
  constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
#endif
  constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f;

 public:
  typedef safe_hash_map<KeyT, ValueT, HashT, EqT> htype;
  typedef std::pair<KeyT, ValueT> value_type;

#if EMH_BUCKET_INDEX == 0
  typedef value_type value_pair;
  typedef std::pair<size_type, value_type> PairT;
#elif EMH_BUCKET_INDEX == 2
  typedef value_type value_pair;
  typedef std::pair<value_type, size_type> PairT;
#else
  typedef entry<KeyT, ValueT> value_pair;
  typedef entry<KeyT, ValueT> PairT;
#endif

  typedef KeyT key_type;
  typedef ValueT val_type;
  typedef ValueT mapped_type;
  typedef HashT hasher;
  typedef EqT key_equal;
  typedef PairT& reference;
  typedef const PairT& const_reference;

  class const_iterator;
  class iterator {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;
    typedef value_pair value_type;

    typedef value_pair* pointer;
    typedef value_pair& reference;

    iterator() = default;
    iterator(const const_iterator& it)
        : _map(it._map),
          _bucket(it._bucket),
          _from(it._from),
          _bmask(it._bmask) {}
    // iterator(const htype* hash_map, size_type bucket, bool) : _map(hash_map),
    // _bucket(bucket) { init(); }
#if EMH_ITER_SAFE
    iterator(const htype* hash_map, size_type bucket)
        : _map(hash_map), _bucket(bucket) {
      init();
    }
#else
    iterator(const htype* hash_map, size_type bucket)
        : _map(hash_map), _bucket(bucket), _bmask(0) {
      _from = size_type(-1);
    }
#endif

    void init() {
      _from = (_bucket / SIZE_BIT) * SIZE_BIT;
      if (_bucket < _map->bucket_count()) {
        _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
        _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
        _bmask = ~_bmask;
      } else {
        _bmask = 0;
      }
    }

    size_type bucket() const { return _bucket; }

    void clear(size_type bucket) {
      if (_bucket / SIZE_BIT == bucket / SIZE_BIT)
        _bmask &= ~(1ull << (bucket % SIZE_BIT));
    }

    iterator& next() {
      goto_next_element();
      return *this;
    }

    iterator& operator++() {
#ifndef EMH_ITER_SAFE
      if (_from == (size_type)-1) init();
#endif
      _bmask &= _bmask - 1;
      goto_next_element();
      return *this;
    }

    iterator operator++(int) {
#ifndef EMH_ITER_SAFE
      if (_from == (size_type)-1) init();
#endif
      iterator old = *this;
      _bmask &= _bmask - 1;
      goto_next_element();
      return old;
    }

    reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }

    pointer operator->() const { return &(_map->EMH_PKV(_pairs, _bucket)); }

    bool operator==(const iterator& rhs) const {
      return _bucket == rhs._bucket;
    }
    bool operator!=(const iterator& rhs) const {
      return _bucket != rhs._bucket;
    }
    bool operator==(const const_iterator& rhs) const {
      return _bucket == rhs._bucket;
    }
    bool operator!=(const const_iterator& rhs) const {
      return _bucket != rhs._bucket;
    }

   private:
    void goto_next_element() {
      if (_bmask != 0) {
        _bucket = _from + CTZ(_bmask);
        return;
      }

      do {
        _bmask = ~*(size_t*)((size_t*)_map->_bitmask +
                             (_from += SIZE_BIT) / SIZE_BIT);
      } while (_bmask == 0);

      _bucket = _from + CTZ(_bmask);
    }

   public:
    const htype* _map;
    size_type _bucket;
    size_type _from;
    size_t _bmask;
  };

  class const_iterator {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;
    typedef value_pair value_type;

    typedef const value_pair* pointer;
    typedef const value_pair& reference;

    const_iterator(const iterator& it)
        : _map(it._map),
          _bucket(it._bucket),
          _from(it._from),
          _bmask(it._bmask) {}
    // const_iterator(const htype* hash_map, size_type bucket, bool) :
    // _map(hash_map), _bucket(bucket) { init(); }
#if EMH_ITER_SAFE
    const_iterator(const htype* hash_map, size_type bucket)
        : _map(hash_map), _bucket(bucket) {
      init();
    }
#else
    const_iterator(const htype* hash_map, size_type bucket)
        : _map(hash_map), _bucket(bucket), _bmask(0) {
      _from = size_type(-1);
    }
#endif

    void init() {
      _from = (_bucket / SIZE_BIT) * SIZE_BIT;
      if (_bucket < _map->bucket_count()) {
        _bmask = *(size_t*)((size_t*)_map->_bitmask + _from / SIZE_BIT);
        _bmask |= (1ull << _bucket % SIZE_BIT) - 1;
        _bmask = ~_bmask;
      } else {
        _bmask = 0;
      }
    }

    size_type bucket() const { return _bucket; }

    const_iterator& operator++() {
#ifndef EMH_ITER_SAFE
      if (_from == (size_type)-1) init();
#endif
      goto_next_element();
      return *this;
    }

    const_iterator operator++(int) {
#ifndef EMH_ITER_SAFE
      if (_from == (size_type)-1) init();
#endif
      const_iterator old(*this);
      goto_next_element();
      return old;
    }

    reference operator*() const { return _map->EMH_PKV(_pairs, _bucket); }

    pointer operator->() const { return &(_map->EMH_PKV(_pairs, _bucket)); }

    bool operator==(const const_iterator& rhs) const {
      return _bucket == rhs._bucket;
    }
    bool operator!=(const const_iterator& rhs) const {
      return _bucket != rhs._bucket;
    }

   private:
    void goto_next_element() {
      _bmask &= _bmask - 1;
      if (_bmask != 0) {
        _bucket = _from + CTZ(_bmask);
        return;
      }

      do {
        _bmask = ~*(size_t*)((size_t*)_map->_bitmask +
                             (_from += SIZE_BIT) / SIZE_BIT);
      } while (_bmask == 0);

      _bucket = _from + CTZ(_bmask);
    }

   public:
    const htype* _map;
    size_type _bucket;
    size_type _from;
    size_t _bmask;
  };

  void init(size_type bucket, float mlf = EMH_DEFAULT_LOAD_FACTOR) {
    _pairs = nullptr;
    _bitmask = nullptr;
    _num_buckets = _num_filled = 0;
    _mlf = (uint32_t)((1 << 28) / EMH_DEFAULT_LOAD_FACTOR);
    max_load_factor(mlf);
    rehash(bucket);
  }

  safe_hash_map(size_type bucket = 2,
                float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept {
    init(bucket, mlf);
  }

  static size_t AllocSize(uint64_t num_buckets) {
    return (num_buckets + EPACK_SIZE) * sizeof(PairT) + (num_buckets + 7) / 8 +
           BIT_PACK;
  }

  static PairT* alloc_bucket(size_type num_buckets) {
#ifdef EMH_ALLOC
    auto* new_pairs = (PairT*)aligned_alloc(EMH_MALIGN, AllocSize(num_buckets));
#else
    auto* new_pairs = (PairT*)malloc(AllocSize(num_buckets));
#endif
    return new_pairs;
  }

  safe_hash_map(const safe_hash_map& rhs) noexcept {
    if (rhs.load_factor() > EMH_MIN_LOAD_FACTOR) {
      _pairs = (PairT*)alloc_bucket(rhs._num_buckets);
      clone(rhs);
    } else {
      init(rhs._num_filled + 2, rhs.max_load_factor());
      for (auto it = rhs.begin(); it != rhs.end(); ++it)
        insert_unique(it->first, it->second);
    }
  }

  safe_hash_map(safe_hash_map&& rhs) noexcept {
#ifndef EMH_ZERO_MOVE
    init(4);
#else
    _num_buckets = _num_filled = _mask = 0;
    _pairs = nullptr;
#endif
    swap(rhs);
  }

  safe_hash_map(std::initializer_list<value_type> ilist) {
    init((size_type)ilist.size());
    for (auto it = ilist.begin(); it != ilist.end(); ++it) do_insert(*it);
  }

  template <class InputIt>
  safe_hash_map(InputIt first, InputIt last, size_type bucket_count = 4) {
    init((size_type)std::distance(first, last) + bucket_count);
    for (; first != last; ++first) emplace(*first);
  }

  safe_hash_map& operator=(const safe_hash_map& rhs) noexcept {
    if (this == &rhs) return *this;

    if (rhs.load_factor() < EMH_MIN_LOAD_FACTOR) {
      clear();
      free(_pairs);
      _pairs = nullptr;
      rehash(rhs._num_filled + 2);
      for (auto it = rhs.begin(); it != rhs.end(); ++it)
        insert_unique(it->first, it->second);
      return *this;
    }

    if (_num_filled) clearkv();

    if (_num_buckets != rhs._num_buckets) {
      free(_pairs);
      _pairs = alloc_bucket(rhs._num_buckets);
    }

    clone(rhs);
    return *this;
  }

  safe_hash_map& operator=(safe_hash_map&& rhs) noexcept {
    if (this != &rhs) {
      swap(rhs);
      rhs.clear();
    }
    return *this;
  }

  template <typename Con>
  bool operator==(const Con& rhs) const {
    if (size() != rhs.size()) return false;

    for (auto it = begin(), last = end(); it != last; ++it) {
      auto oi = rhs.find(it->first);
      if (oi == rhs.end() || it->second != oi->second) return false;
    }
    return true;
  }

  template <typename Con>
  bool operator!=(const Con& rhs) const {
    return !(*this == rhs);
  }

  ~safe_hash_map() noexcept {
    if (!is_trivially_destructible() && _num_filled) {
      for (auto it = cbegin(); _num_filled; ++it) {
        _num_filled--;
        it->~value_pair();
      }
    }
    free(_pairs);
    _pairs = nullptr;
  }

  void clone(const safe_hash_map& rhs) noexcept {
    _hasher = rhs._hasher;

    _num_filled = rhs._num_filled;
    _mask = rhs._mask;
    _mlf = rhs._mlf;
    _num_buckets = rhs._num_buckets;

    _bitmask = decltype(_bitmask)(_pairs + EPACK_SIZE + _num_buckets);
    auto* opairs = rhs._pairs;

    if (is_trivially_copyable()) {
      memcpy((char*)_pairs, opairs, AllocSize(_num_buckets));
    } else {
      memcpy((char*)(_pairs + _num_buckets), opairs + _num_buckets,
             EPACK_SIZE * sizeof(PairT) + (_num_buckets + 7) / 8 + BIT_PACK);
      for (auto it = rhs.cbegin(); it.bucket() < _num_buckets; ++it) {
        const auto bucket = it.bucket();
        new (_pairs + bucket) PairT(opairs[bucket]);
        EMH_BUCKET(_pairs, bucket) = EMH_BUCKET(opairs, bucket);
      }
    }
  }

  void swap(safe_hash_map& rhs) {
    std::swap(_hasher, rhs._hasher);
    // std::swap(_eq, rhs._eq);
    std::swap(_pairs, rhs._pairs);
    std::swap(_num_buckets, rhs._num_buckets);
    std::swap(_num_filled, rhs._num_filled);
    std::swap(_mask, rhs._mask);
    std::swap(_mlf, rhs._mlf);
    std::swap(_bitmask, rhs._bitmask);
  }

  // -------------------------------------------------------------
  iterator begin() noexcept {
#ifdef EMH_ZERO_MOVE
    if (0 == _num_filled) return {this, _num_buckets};
#endif

    const auto bmask = ~(*(size_t*)_bitmask);
    if (bmask != 0) return {this, (size_type)CTZ(bmask)};

    iterator it(this, sizeof(bmask) * 8 - 1);
    it.init();
    return it.next();
  }

  const_iterator cbegin() const noexcept {
#ifdef EMH_ZERO_MOVE
    if (0 == _num_filled) return {this, _num_buckets};
#endif

    const auto bmask = ~(*(size_t*)_bitmask);
    if (bmask != 0) return {this, (size_type)CTZ(bmask)};

    iterator it(this, sizeof(bmask) * 8 - 1);
    it.init();
    return it.next();
  }

  iterator last() const {
    if (_num_filled == 0) return end();

    auto bucket = _num_buckets - 1;
    while (EMH_EMPTY(bucket)) bucket--;
    return {this, bucket};
  }

  inline const_iterator begin() const noexcept { return cbegin(); }

  inline iterator end() noexcept { return {this, _num_buckets}; }
  inline const_iterator cend() const { return {this, _num_buckets}; }
  inline const_iterator end() const { return cend(); }

  inline size_type size() const { return _num_filled; }
  inline bool empty() const { return _num_filled == 0; }

  inline size_type bucket_count() const { return _num_buckets; }
  inline float load_factor() const {
    return ((float)_num_filled) / ((float)_mask + 1.0f);
  }

  inline const HashT& hash_function() const { return _hasher; }
  inline const EqT& key_eq() const { return _eq; }

  inline void max_load_factor(float mlf) {
    if (mlf <= 0.999f && mlf > EMH_MIN_LOAD_FACTOR)
      _mlf = (uint32_t)((1 << 28) / mlf);
  }

  inline constexpr float max_load_factor() const {
    return (1 << 28) / (float)_mlf;
  }
  constexpr uint64_t max_size() const {
    return 1ull << (sizeof(_num_buckets) * 8 - 1);
  }
  constexpr uint64_t max_bucket_count() const { return max_size(); }

  size_type bucket_main() const {
    size_type main_size = 0;
    for (size_type bucket = 0; bucket < _num_buckets; ++bucket) {
      if (EMH_BUCKET(_pairs, bucket) == bucket) main_size++;
    }
    return main_size;
  }

  // ------------------------------------------------------------
  template <typename Key = KeyT>
  inline iterator find(const Key& key, size_t key_hash) noexcept {
    return {this, find_filled_hash(key, key_hash)};
  }

  template <typename Key = KeyT>
  inline const_iterator find(const Key& key, size_t key_hash) const noexcept {
    return {this, find_filled_hash(key, key_hash)};
  }

  template <typename Key = KeyT>
  inline iterator find(const Key& key) noexcept {
    return {this, find_filled_bucket(key)};
  }

  template <typename Key = KeyT>
  inline const_iterator find(const Key& key) const noexcept {
    return {this, find_filled_bucket(key)};
  }

  template <typename Key = KeyT>
  ValueT& at(const KeyT& key) {
    const auto bucket = find_filled_bucket(key);
    // throw
    return EMH_VAL(_pairs, bucket);
  }

  template <typename Key = KeyT>
  const ValueT& at(const KeyT& key) const {
    const auto bucket = find_filled_bucket(key);
    // throw
    return EMH_VAL(_pairs, bucket);
  }

  template <typename Key = KeyT>
  inline bool contains(const Key& key) const noexcept {
    return find_filled_bucket(key) != _num_buckets;
  }

  template <typename Key = KeyT>
  inline size_type count(const Key& key) const noexcept {
    return find_filled_bucket(key) != _num_buckets ? 1u : 0u;
  }

  template <typename Key = KeyT>
  std::pair<iterator, iterator> equal_range(const Key& key) const noexcept {
    const auto found = {this, find_filled_bucket(key), true};
    if (found.bucket() == _num_buckets)
      return {found, found};
    else
      return {found, std::next(found)};
  }

  template <typename K = KeyT>
  std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
    const auto found = {this, find_filled_bucket(key), true};
    if (found.bucket() == _num_buckets)
      return {found, found};
    else
      return {found, std::next(found)};
  }

  void merge(safe_hash_map& rhs) {
    if (empty()) {
      *this = std::move(rhs);
      return;
    }

    for (auto rit = rhs.begin(); rit != rhs.end();) {
      auto fit = find(rit->first);
      if (fit.bucket() == _num_buckets) {
        insert_unique(rit->first, std::move(rit->second));
        rit = rhs.erase(rit);
      } else {
        ++rit;
      }
    }
  }

#ifdef EMH_EXT
  bool try_get(const KeyT& key, ValueT& val) const noexcept {
    const auto bucket = find_filled_bucket(key);
    const auto found = bucket != _num_buckets;
    if (found) {
      val = EMH_VAL(_pairs, bucket);
    }
    return found;
  }

  /// Returns the matching ValueT or nullptr if k isn't found.
  ValueT* try_get(const KeyT& key) noexcept {
    const auto bucket = find_filled_bucket(key);
    return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
  }

  /// Const version of the above
  ValueT* try_get(const KeyT& key) const noexcept {
    const auto bucket = find_filled_bucket(key);
    return bucket == _num_buckets ? nullptr : &EMH_VAL(_pairs, bucket);
  }

  /// Convenience function.
  ValueT get_or_return_default(const KeyT& key) const noexcept {
    const auto bucket = find_filled_bucket(key);
    return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
  }
#endif

  // -----------------------------------------------------
  template <typename K = KeyT, typename V = ValueT>
  std::pair<iterator, bool> do_assign(K&& key, V&& val) {
    reserve(_num_filled);

    bool isempty;
    const auto bucket = find_or_allocate(key, isempty);
    if (isempty) {
      EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
    } else {
      EMH_VAL(_pairs, bucket) = std::forward(val);
    }
    return {{this, bucket}, isempty};
  }

  std::pair<iterator, bool> do_insert(const value_type& value) {
    bool isempty;
    const auto bucket = find_or_allocate(value.first, isempty);
    if (isempty) {
      EMH_NEW(value.first, value.second, bucket);
    }
    return {{this, bucket}, isempty};
  }

  std::pair<iterator, bool> do_insert(value_type&& value) {
    bool isempty;
    const auto bucket = find_or_allocate(value.first, isempty);
    if (isempty) {
      EMH_NEW(std::move(value.first), std::move(value.second), bucket);
    }
    return {{this, bucket}, isempty};
  }

  template <typename K = KeyT, typename V = ValueT>
  std::pair<iterator, bool> do_insert(K&& key, V&& val) {
    bool isempty;
    const auto bucket = find_or_allocate(key, isempty);
    if (isempty) {
      EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
    }
    return {{this, bucket}, isempty};
  }

  std::pair<iterator, bool> insert(const value_type& value) {
    check_expand_need();
    return do_insert(value);
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    check_expand_need();
    return do_insert(std::move(value));
  }

  void insert(std::initializer_list<value_type> ilist) {
    reserve(ilist.size() + _num_filled);
    for (auto it = ilist.begin(); it != ilist.end(); ++it) do_insert(*it);
  }

  template <typename Iter>
  void insert(Iter first, Iter last) {
    reserve(std::distance(first, last) + _num_filled);
    for (auto it = first; it != last; ++it) do_insert(it->first, it->second);
  }

  template <typename K, typename V>
  inline size_type insert_unique(K&& key, V&& val) {
    return do_insert_unique(std::forward<K>(key), std::forward<V>(val));
  }

  inline size_type insert_unique(value_type&& value) {
    return do_insert_unique(std::move(value.first), std::move(value.second));
  }

  inline size_type insert_unique(const value_type& value) {
    return do_insert_unique(value.first, value.second);
  }

  template <typename K, typename V>
  size_type do_insert_unique(K&& key, V&& val) {
    check_expand_need();
    auto bucket = find_unique_bucket(key);
    EMH_NEW(std::forward<K>(key), std::forward<V>(val), bucket);
    return bucket;
  }

  std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& val) {
    return do_assign(key, std::forward<ValueT>(val));
  }
  std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& val) {
    return do_assign(std::move(key), std::forward<ValueT>(val));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) noexcept {
    check_expand_need();
    return do_insert(std::forward<Args>(args)...);
  }

  template <class... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args) {
    (void)hint;
    check_expand_need();
    return do_insert(std::forward<Args>(args)...).first;
  }

  template <class... Args>
  std::pair<iterator, bool> try_emplace(const KeyT& key, Args&&... args) {
    check_expand_need();
    return do_insert(key, std::forward<Args>(args)...);
  }

  template <class... Args>
  std::pair<iterator, bool> try_emplace(KeyT&& key, Args&&... args) {
    check_expand_need();
    return do_insert(std::forward<KeyT>(key), std::forward<Args>(args)...);
  }

  template <class... Args>
  size_type emplace_unique(Args&&... args) noexcept {
    return insert_unique(std::forward<Args>(args)...);
  }

  /* Check if inserting a new value rather than overwriting an old entry */
  ValueT& operator[](const KeyT& key) noexcept {
    check_expand_need();

    bool isempty;
    const auto bucket = find_or_allocate(key, isempty);
    if (isempty) {
      EMH_NEW(key, std::move(ValueT()), bucket);
    }

    return EMH_VAL(_pairs, bucket);
  }

  ValueT& operator[](KeyT&& key) noexcept {
    check_expand_need();

    bool isempty;
    const auto bucket = find_or_allocate(key, isempty);
    if (isempty) {
      EMH_NEW(std::move(key), std::move(ValueT()), bucket);
    }

    return EMH_VAL(_pairs, bucket);
  }

  // -------------------------------------------------------
  /// Erase an element from the hash table.
  /// return 0 if element was not found
  template <typename Key = KeyT>
  size_type erase(const Key& key) {
    const auto bucket = erase_key(key);
    if (bucket == INACTIVE) return 0;

    clear_bucket(bucket);
    return 1;
  }

  // iterator erase const_iterator
  iterator erase(const_iterator cit) {
    iterator it(cit);
    return erase(it);
  }

  /// Erase an element typedef an iterator.
  /// Returns an iterator to the next element (or end()).
  iterator erase(iterator it) {
    const auto bucket = erase_bucket(it._bucket);
    clear_bucket(bucket);
    if (bucket == it._bucket) {
      return ++it;
    } else {
      // erase main bucket as next
      it.clear(bucket);
      return it;
    }
  }

  /// Erase an element typedef an iterator without return next iterator
  void _erase(const_iterator it) {
    const auto bucket = erase_bucket(it._bucket);
    clear_bucket(bucket);
  }

  template <typename Pred>
  size_type erase_if(Pred pred) {
    auto old_size = size();
    for (auto it = begin(), last = end(); it != last;) {
      if (pred(*it))
        it = erase(it);
      else
        ++it;
    }
    return old_size - size();
  }

  static constexpr bool is_trivially_destructible() {
#if __cplusplus >= 201402L || _MSC_VER > 1600
    return (std::is_trivially_destructible<KeyT>::value &&
            std::is_trivially_destructible<ValueT>::value);
#else
    return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
  }

  static constexpr bool is_trivially_copyable() {
#if __cplusplus >= 201402L || _MSC_VER > 1600
    return (std::is_trivially_copyable<KeyT>::value &&
            std::is_trivially_copyable<ValueT>::value);
#else
    return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
  }

  static void prefetch_heap_block(char* ctrl) {
    // Prefetch the heap-allocated memory region to resolve potential TLB
    // misses.  This is intended to overlap with execution of calculating the
    // hash for a key.
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_prefetch((const char*)ctrl, _MM_HINT_T0);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(static_cast<const void*>(ctrl));
#endif
  }

  void clearkv() {
    if (!is_trivially_destructible()) {
      auto it = cbegin();
      it.init();
      for (; _num_filled; ++it) clear_bucket(it.bucket());
    }
  }

  /// Remove all elements, keeping full capacity.
  void clear() {
    if (is_trivially_destructible() && _num_filled) {
      memset(_bitmask, (int)0xFFFFFFFF, (_num_buckets + 7) / 8);
      if (_num_buckets < 8 * sizeof(_bitmask[0]))
        _bitmask[0] = (bit_type)((1 << _num_buckets) - 1);
    } else if (_num_filled) {
      clearkv();
    }

    // EMH_BUCKET(_pairs, _num_buckets) = 0; //_last
    _num_filled = 0;
  }

  void shrink_to_fit() { rehash(_num_filled + 1); }

  /// Make room for this many elements
  bool reserve(uint64_t num_elems) {
    const auto required_buckets = (num_elems * _mlf >> 28);
    if (EMH_LIKELY(required_buckets < _num_buckets)) return false;

#if EMH_HIGH_LOAD
    if (required_buckets < 64 && _num_filled < _num_buckets) return false;
#endif

#if EMH_STATIS
    if (_num_filled > EMH_STATIS) dump_statics(true);
#endif
    rehash(required_buckets + 2);
    return true;
  }

  void rehash(uint64_t required_buckets) {
    if (required_buckets < _num_filled) return;

    uint64_t buckets = _num_filled > (1u << 16) ? (1u << 16) : 2u;
    while (buckets < required_buckets) {
      buckets *= 2;
    }

    // no need alloc large bucket for small key sizeof(KeyT) < sizeof(int).
    // set small a max_load_factor, insert/reserve() will fail and introduce
    // rehash issiue TODO: dothing ?
    // if (sizeof(KeyT) < sizeof(size_type) && buckets > (1ul <<
    // (sizeof(uint16_t) * 8)))
    //    buckets = 2ul << (sizeof(KeyT) * 8);

    if (buckets > max_size() || buckets < _num_filled)
      std::abort();  // TODO: throwOverflowError

    auto num_buckets = (size_type)buckets;
    auto old_num_filled = _num_filled;
    auto old_mask = _num_buckets - 1;
    auto old_pairs = _pairs;
    auto* obmask = _bitmask;

    _num_filled = 0;
    _num_buckets = num_buckets;
    _mask = num_buckets - 1;

    _pairs = alloc_bucket(_num_buckets);
    memset((char*)(_pairs + _num_buckets), 0, sizeof(PairT) * EPACK_SIZE);

    _bitmask = decltype(_bitmask)(_pairs + EPACK_SIZE + num_buckets);

    const auto mask_byte = (num_buckets + 7) / 8;
    memset(_bitmask, (int)0xFFFFFFFF, mask_byte);
    memset(((char*)_bitmask) + mask_byte, 0, BIT_PACK);
    if (num_buckets < 8 * sizeof(_bitmask[0]))
      _bitmask[0] = (bit_type)((1 << num_buckets) - 1);

    // for (size_type src_bucket = 0; _num_filled < old_num_filled;
    // src_bucket++) {
    for (size_type src_bucket = old_mask; _num_filled < old_num_filled;
         src_bucket--) {
      if (obmask[src_bucket / MASK_BIT] & (EMH_MASK(src_bucket))) continue;

      auto& key = EMH_KEY(old_pairs, src_bucket);
      const auto bucket = find_unique_bucket(key);
      EMH_NEW(std::move(key), std::move(EMH_VAL(old_pairs, src_bucket)),
              bucket);
      if (!is_trivially_destructible()) old_pairs[src_bucket].~PairT();
    }

    free(old_pairs);
    assert(old_num_filled == _num_filled);
  }

 private:
  // Can we fit another element?
  inline bool check_expand_need() { return reserve(_num_filled); }

  void clear_bucket(size_type bucket) {
    EMH_CLS(bucket);
    _num_filled--;
    if (!is_trivially_destructible()) _pairs[bucket].~PairT();
  }

#if 1
  // template<typename UType, typename
  // std::enable_if<std::is_integral<UType>::value, size_type>::type = 0>
  template <typename UType>
  size_type erase_key(const UType& key) {
    const auto bucket = hash_key(key) & _mask;
    if (EMH_EMPTY(bucket)) return INACTIVE;

    auto next_bucket = EMH_BUCKET(_pairs, bucket);
    const auto eqkey = _eq(key, EMH_KEY(_pairs, bucket));
    if (eqkey) {
      if (next_bucket == bucket) return bucket;

      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (is_trivially_copyable())
        EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
      else
        EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));

      EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
      return next_bucket;
    } else if (next_bucket == bucket) {
      return INACTIVE;
    }

    auto prev_bucket = bucket;
    while (true) {
      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
        EMH_BUCKET(_pairs, prev_bucket) =
            (nbucket == next_bucket) ? prev_bucket : nbucket;
        return next_bucket;
      }

      if (nbucket == next_bucket) break;
      prev_bucket = next_bucket;
      next_bucket = nbucket;
    }

    return INACTIVE;
  }
#else
  template <typename UType,
            typename std::enable_if<!std::is_integral<UType>::value,
                                    size_type>::type = 0>
  size_type erase_key(const UType& key) {
    const auto bucket = hash_key(key) & _mask;
    if (EMH_EMPTY(bucket)) return INACTIVE;

    auto next_bucket = EMH_BUCKET(_pairs, bucket);
    if (next_bucket == bucket)
      return _eq(key, EMH_KEY(_pairs, bucket)) ? bucket : INACTIVE;
    //        else if (bucket != hash_key(EMH_KEY(_pairs, bucket)))
    //            return INACTIVE;

    // find erase key and swap to last bucket
    size_type prev_bucket = bucket, find_bucket = INACTIVE;
    next_bucket = bucket;
    while (true) {
      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
        find_bucket = next_bucket;
        if (nbucket == next_bucket) {
          EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
          break;
        }
      }
      if (nbucket == next_bucket) {
        if (find_bucket != INACTIVE) {
          EMH_PKV(_pairs, find_bucket).swap(EMH_PKV(_pairs, nbucket));
          //                    EMH_PKV(_pairs, find_bucket) = EMH_PKV(_pairs,
          //                    nbucket);
          EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
          find_bucket = nbucket;
        }
        break;
      }
      prev_bucket = next_bucket;
      next_bucket = nbucket;
    }

    return find_bucket;
  }
#endif

  size_type erase_bucket(const size_type bucket) {
    const auto next_bucket = EMH_BUCKET(_pairs, bucket);
    const auto main_bucket = hash_key(EMH_KEY(_pairs, bucket)) & _mask;
    if (bucket == main_bucket) {
      if (bucket != next_bucket) {
        const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
        if (is_trivially_copyable())
          EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
        else
          EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
        EMH_BUCKET(_pairs, bucket) =
            (nbucket == next_bucket) ? bucket : nbucket;
      }
      return next_bucket;
    }

    const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
    EMH_BUCKET(_pairs, prev_bucket) =
        (bucket == next_bucket) ? prev_bucket : next_bucket;
    return bucket;
  }

  // Find the bucket with this key, or return bucket size
  template <typename K = KeyT>
  size_type find_filled_hash(const K& key, const size_t key_hash) const {
    const auto bucket = size_type(key_hash & _mask);
    if (EMH_EMPTY(bucket)) return _num_buckets;

    // prefetch_heap_block((char*)&_pairs[bucket]);

    auto next_bucket = bucket;
    while (true) {
      if (_eq(key, EMH_KEY(_pairs, next_bucket))) return next_bucket;

      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (nbucket == next_bucket) break;
      next_bucket = nbucket;
    }

    return _num_buckets;
  }

  // Find the bucket with this key, or return bucket size
  template <typename K = KeyT>
  size_type find_filled_bucket(const K& key) const {
    const auto bucket = size_type(hash_key(key) & _mask);
    if (EMH_EMPTY(bucket)) return _num_buckets;

    // prefetch_heap_block((char*)&_pairs[bucket]);
    auto next_bucket = bucket;
    //        else if (bucket != (hash_key(bucket_key) & _mask))
    //            return _num_buckets;

    while (true) {
      if (_eq(key, EMH_KEY(_pairs, next_bucket))) return next_bucket;

      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (nbucket == next_bucket) return _num_buckets;
      next_bucket = nbucket;
    }

    return 0;
  }

  // kick out bucket and find empty to occpuy
  // it will break the origin link and relnik again.
  // before: main_bucket-->prev_bucket --> bucket   --> next_bucket
  // atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket-->
  // next_bucket
  size_type kickout_bucket(const size_type kmain, const size_type kbucket) {
    const auto next_bucket = EMH_BUCKET(_pairs, kbucket);
    const auto new_bucket = find_empty_bucket(next_bucket, kbucket);
    const auto prev_bucket = find_prev_bucket(kmain, kbucket);
    new (_pairs + new_bucket) PairT(std::move(_pairs[kbucket]));
    if (!is_trivially_destructible()) _pairs[kbucket].~PairT();

    if (next_bucket == kbucket) EMH_BUCKET(_pairs, new_bucket) = new_bucket;
    EMH_BUCKET(_pairs, prev_bucket) = new_bucket;

    EMH_SET(new_bucket);
    return kbucket;
  }

  /*
  ** inserts a new key into a hash table; first check whether key's main
  ** bucket/position is free. If not, check whether colliding node/bucket is in
  * its main
  ** position or not: if it is not, move colliding bucket to an empty place and
  ** put new key in its main position; otherwise (colliding bucket is in its
  * main
  ** position), new key goes to an empty position. ***/

  template <typename K = KeyT>
  size_type find_or_allocate(const K& key, bool& isempty) {
    const auto bucket = hash_key(key) & _mask;
    const auto& bucket_key = EMH_KEY(_pairs, bucket);
    if (EMH_EMPTY(bucket)) {
      isempty = true;
      return bucket;
    } else if (_eq(key, bucket_key)) {
      isempty = false;
      return bucket;
    }

    isempty = true;
    auto next_bucket = EMH_BUCKET(_pairs, bucket);
    // check current bucket_key is in main bucket or not
    const auto kmain_bucket = hash_key(bucket_key) & _mask;
    if (kmain_bucket != bucket)
      return kickout_bucket(kmain_bucket, bucket);
    else if (next_bucket == bucket)
      return EMH_BUCKET(_pairs, next_bucket) =
                 find_empty_bucket(next_bucket, bucket);

#if EMH_LRU_SET
    auto prev_bucket = bucket;
#endif
    // find next linked bucket and check key, if lru is set then swap current
    // key with prev_bucket
    while (true) {
      if (EMH_UNLIKELY(_eq(key, EMH_KEY(_pairs, next_bucket)))) {
        isempty = false;
#if EMH_LRU_SET
        EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, prev_bucket));
        return prev_bucket;
#else
        return next_bucket;
#endif
      }

#if EMH_LRU_SET
      prev_bucket = next_bucket;
#endif

      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (nbucket == next_bucket) break;
      next_bucket = nbucket;
    }

    // find a new empty and link it to tail, TODO link after main bucket?
    const auto new_bucket = find_empty_bucket(
        next_bucket, bucket);  // : find_empty_bucket(next_bucket);
    return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
  }

  // key is not in this map. Find a place to put it.
  size_type find_empty_bucket(const size_type bucket_from,
                              const size_type main_bucket) {
#if EMH_ITER_SAFE
    const auto boset = bucket_from % 8;
    auto* const align = (uint8_t*)_bitmask + bucket_from / 8;
    (void)main_bucket;
    size_t bmask;
    memcpy(&bmask, align + 0, sizeof(bmask));
    bmask >>= boset;  // bmask |= ((size_t)align[8] << (SIZE_BIT - boset));
    if (EMH_LIKELY(bmask != 0)) return bucket_from + CTZ(bmask);
#else
    const auto boset = bucket_from % 8;
    auto* const align = (uint8_t*)_bitmask + bucket_from / 8;
    (void)main_bucket;
    const size_t bmask =
        (*(size_t*)(align) >> boset);  // & 0xF0F0F0F0FF0FF0FFull;//
    if (EMH_LIKELY(bmask != 0)) return bucket_from + CTZ(bmask);
#endif

    const auto qmask = _mask / SIZE_BIT;
    auto& last = EMH_BUCKET(_pairs, _num_buckets);
    for (;;) {
      last &= qmask;
      const auto bmask2 = *((size_t*)_bitmask + last);
      if (bmask2 != 0) return last * SIZE_BIT + CTZ(bmask2);
#if 1
      const auto next1 = (qmask / 2 + last) & qmask;
      const auto bmask1 = *((size_t*)_bitmask + next1);
      if (bmask1 != 0) {
        last = next1;
        return next1 * SIZE_BIT + CTZ(bmask1);
      }
      last += 1;
#else
      next_bucket += offset < 10 ? 1 + SIZE_BIT * offset : 1 + qmask / 32;
      if (next_bucket >= qmask) {
        next_bucket += 1;
        next_bucket &= qmask;
      }

      const auto bmask1 = *((size_t*)_bitmask + next_bucket);
      if (bmask1 != 0) {
        last = next_bucket;
        return next_bucket * SIZE_BIT + CTZ(bmask1);
      }
      offset += 1;
#endif
    }
  }

  // key is not in this map. Find a place to put it.
  size_type find_unique_empty(const size_type bucket_from) {
    const auto boset = bucket_from % 8;
    auto* const align = (uint8_t*)_bitmask + bucket_from / 8;

#if EMH_ITER_SAFE
    size_t bmask;
    memcpy(&bmask, align + 0, sizeof(bmask));
    bmask >>= boset;
#else
    const auto bmask =
        (*(size_t*)(align) >> boset);  // maybe not aligned and warning
#endif
    if (EMH_LIKELY(bmask != 0)) return bucket_from + CTZ(bmask);

    const auto qmask = _mask / SIZE_BIT;
    for (auto last = (bucket_from + _mask) & qmask;;) {
      const auto bmask2 =
          *((size_t*)_bitmask + last);  // & 0xF0F0F0F0FF0FF0FFull;
      if (EMH_LIKELY(bmask2 != 0)) return last * SIZE_BIT + CTZ(bmask2);
      last = (last + 1) & qmask;
    }

    return 0;
  }

  size_type find_last_bucket(size_type main_bucket) const {
    auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
    if (next_bucket == main_bucket) return main_bucket;

    while (true) {
      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (nbucket == next_bucket) return next_bucket;
      next_bucket = nbucket;
    }
  }

  size_type find_prev_bucket(size_type main_bucket,
                             const size_type bucket) const {
    auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
    if (next_bucket == bucket) return main_bucket;

    while (true) {
      const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
      if (nbucket == bucket) return next_bucket;
      next_bucket = nbucket;
    }
  }

  size_type find_unique_bucket(const KeyT& key) {
    const size_type bucket = hash_key(key) & _mask;
    if (EMH_EMPTY(bucket)) return bucket;

    // check current bucket_key is in main bucket or not
    const auto kmain_bucket = hash_key(EMH_KEY(_pairs, bucket)) & _mask;
    if (EMH_UNLIKELY(kmain_bucket != bucket))
      return kickout_bucket(kmain_bucket, bucket);

    auto next_bucket = EMH_BUCKET(_pairs, bucket);
    if (next_bucket != bucket) next_bucket = find_last_bucket(next_bucket);

    // find a new empty and link it to tail
    return EMH_BUCKET(_pairs, next_bucket) =
               find_empty_bucket(next_bucket, bucket);
  }

#if EMH_INT_HASH
  static constexpr uint64_t KC = UINT64_C(11400714819323198485);
  inline static uint64_t hash64(uint64_t key) {
#if __SIZEOF_INT128__ && EMH_INT_HASH == 1
    __uint128_t r = key;
    r *= KC;
    return (uint64_t)(r >> 64) ^ (uint64_t)r;
#elif EMH_INT_HASH == 2
    // MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
#elif _WIN64 && EMH_INT_HASH == 1
    uint64_t high;
    return _umul128(key, KC, &high) ^ high;
#elif EMH_INT_HASH == 3
    auto ror = (key >> 32) | (key << 32);
    auto low = key * 0xA24BAED4963EE407ull;
    auto high = ror * 0x9FB21C651E98DF25ull;
    auto mix = low + high;
    return mix;
#elif EMH_INT_HASH == 1
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#elif EMH_WYHASH64
    return wyhash64(key, KC);
#else
    uint64_t x = key;
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
#endif
  }
#endif

  template <typename UType,
            typename std::enable_if<std::is_integral<UType>::value,
                                    size_type>::type = 0>
  inline size_type hash_key(const UType key) const {
#if EMH_INT_HASH
    return hash64(key);
#elif EMH_IDENTITY_HASH
    return key + (key >> 24);
#else
    return (size_type)_hasher(key);
#endif
  }

  template <typename UType,
            typename std::enable_if<std::is_same<UType, std::string>::value,
                                    size_type>::type = 0>
  inline size_type hash_key(const UType& key) const {
    return (size_type)_hasher(key);
  }

  template <typename UType, typename std::enable_if<
                                !std::is_integral<UType>::value &&
                                    !std::is_same<UType, std::string>::value,
                                size_type>::type = 0>
  inline size_type hash_key(const UType& key) const {
    return (size_type)_hasher(key);
  }

 private:
  using bit_type = uint8_t;  // uint8_t uint16_t, uint32_t.
  bit_type* _bitmask;
  PairT* _pairs;
  HashT _hasher;
  EqT _eq;
  size_type _mask;
  size_type _num_buckets;

  size_type _num_filled;
  uint32_t _mlf;

 private:
  static constexpr uint32_t BIT_PACK = sizeof(uint64_t);
  static constexpr uint32_t MASK_BIT = sizeof(_bitmask[0]) * 8;
  static constexpr uint32_t SIZE_BIT = sizeof(size_t) * 8;
  static constexpr uint32_t EPACK_SIZE =
      sizeof(PairT) >= sizeof(size_t) == 0 ? 1 : 2;  // > 1
};
}  // namespace operations_research

#endif  // ORTOOLS_BASE_SAFE_HASH_MAP_H_
