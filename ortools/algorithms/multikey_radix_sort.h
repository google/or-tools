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

#ifndef ORTOOLS_ALGORITHMS_MULTIKEY_RADIX_SORT_H_
#define ORTOOLS_ALGORITHMS_MULTIKEY_RADIX_SORT_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/numeric/bits.h"

namespace operations_research {

// Sorts a container of items by multiple keys using LSD (Least Significant
// Digit) radix sort. The keys must be signed or unsigned integers or
// enumerations. The sorting is stable, because we use LSD radix sort. MSD
// radix sort on the other hand can be unstable. See
// https://en.wikipedia.org/wiki/Radix_sort.
//
// In this version, keys must be integer types or castable to integer types.
// Keys are extracted from the items by key access callables. (`get_key`). The
// container must be swappable and indexable. For optimal performance, it should
// also have an O(1) swap operation (e.g. std::vector, std::deque). The items
// must be copy constructible and copy assignable.
//
// To sort in lexicographical order (i.e. sort by a primary key, then a
// secondary key, etc.), the key access functors must be provided in reverse
// order of importance, i.e. from least significant key to most significant key.
//
// Example:
//   struct Item { uint32_t a; uint32_t b; };
//   std::vector<Item> items = {{2, 1}, {1, 2}, {1, 1}};
//   // Sort primarily by 'a' and secondarily by 'b'.
//   MultikeyRadixSort(
//       items,
//       [](const Item& item) { return item.b; },   // Least significant key.
//       [](const Item& item) { return item.a; });  // Most significant key.
//   // items is now {{1, 1}, {1, 2}, {2, 1}}
//
// The default radix size is 12 bits, which is a good balance between the
// number of passes and the size of the counts array. If you happen to know
// the size of your keys, you may want to use a different radix size for better
// performance.
//
// The container must be swappable and indexable. For optimal performance, it
// should also have an O(1) swap operation (e.g. std::vector, std::deque).
//
// TODO(user): Add the following features, to sort by:
//   - decreasing order.
//   - subset of bits of a key.
//   - floating keys, finally.
//
// TODO(user): Add the logic for the automatic computation of the radix size
// depending on the key ranges.
// Currently, when radix_bits is 0, the radix size is set to kDefaultRadixBits
// (12 bits), or num_bits if it is smaller.
// Otherwise, the radix size is set to fixed_radix_bits.

namespace radix_sort_internal {
constexpr int kContainerSizeCutOff = 8 * sizeof(uint64_t);

// Checks that the container is swappable and indexable, and that the value type
// is copy constructible and copy assignable. If not, the function will fail
// at compile time.
template <typename Container>
constexpr void CheckContainer() {
  // Also check that the underlying integer types are two's complement and wrap
  // around on overflow, as the radix sort algorithm assumes this.
  static_assert(~0 == -1, "Integers must be two's complement.");
  static_assert(~uint64_t{0} + uint64_t{1} == 0,
                "Unsigned integers must wrap.");

  static_assert(std::is_swappable_v<Container>, "Container must be swappable.");
  using BeginType = decltype(std::begin(std::declval<Container&>()));
  static_assert(
      std::is_convertible_v<
          typename std::iterator_traits<BeginType>::iterator_category,
          std::random_access_iterator_tag>,
      "Container must be a random-access range (iterable and indexable).");

  // The value type of the container must be copy constructible and copy
  // assignable, meaning for example that this code can't sort a container of
  // std::unique_ptr.
  using ValueType = typename std::iterator_traits<BeginType>::value_type;
  static_assert(std::is_copy_constructible_v<ValueType>,
                "Container elements must be copy constructible for "
                "radix sort.");
  static_assert(std::is_copy_assignable_v<ValueType>,
                "Container elements must be copy assignable for "
                "radix sort.");
}

// Checks that the key accessor is valid for the given container. If not, the
// function will fail at compile time. If the key type is larger than 64 bits,
// the function will fail at compile time, as the algorithm can not support
// keys larger than 64 bits.
template <typename Container, typename KeyAccessor>
constexpr void CheckKeyAccessor() {
  using BeginType = decltype(std::begin(std::declval<Container&>()));
  using ValueType = typename std::iterator_traits<BeginType>::value_type;
  // Determine the integer type of the key at the lowest level.
  using KeyType = std::decay_t<std::invoke_result_t<KeyAccessor, ValueType&>>;
  static_assert(std::is_integral_v<KeyType> || std::is_enum_v<KeyType>,
                "Key type must be integral or enumeration.");
  static_assert(sizeof(KeyType) <= sizeof(uint64_t),
                "Key type must be at most 64 bits.");
}

template <typename Container>
constexpr void CheckIntegralOrEnum() {
  using ValueType = typename std::iterator_traits<decltype(std::begin(
      std::declval<Container&>()))>::value_type;
  static_assert(std::is_integral_v<ValueType> || std::is_enum_v<ValueType>,
                "Container value type must be integral or enum for this "
                "overload.");
}

// Returns the radix for a given key and shift after subtracting the minimum
// value of the key to make sure the it is computed on a range starting at 0.
template <typename KeyType>
uint64_t GetRadixKey(const uint64_t mask, const int shift, const KeyType key,
                     const KeyType key_min) {
  return ((static_cast<uint64_t>(key) - static_cast<uint64_t>(key_min)) >>
          shift) &
         mask;
}

// Updates count with the number of items for each radix in the given key.
template <typename Container, typename KeyAccessor, typename KeyMin>
void CountItems(const Container& in, const uint64_t mask, const int shift,
                const KeyAccessor key_getter, const KeyMin key_min,
                std::vector<size_t>& counts) {
  std::fill(counts.begin(), counts.end(), 0);
  for (const auto& item : in) {
    ++counts[GetRadixKey(mask, shift, key_getter(item), key_min)];
  }
}

// Distributes the items into the buffer based on the given key and swaps
// 'in' and 'out'.
// Positions of each radix in the output buffer. pos is expected to have the
// same size as counts.
// pos does not need to be reset as it is just the cumulative sum of counts.
template <typename Container, typename KeyAccessor, typename KeyMin>
void Distribute(Container& in, Container& out,
                const std::vector<size_t>& counts, const uint64_t mask,
                const int shift, const KeyAccessor key_getter,
                const KeyMin key_min, std::vector<size_t>& pos) {
  // TODO(user): Merge counts and pos into a single vector?
  pos[0] = 0;  // We just need to reset the first element.
  for (int i = 1; i < pos.size(); ++i) {
    pos[i] = pos[i - 1] + counts[i - 1];
  }
  for (const auto& item : in) {
    std::begin(
        out)[pos[GetRadixKey(mask, shift, key_getter(item), key_min)]++] = item;
  }
  using std::swap;
  swap(in, out);
}

// Builds a container of the same size as the input container.
// If ValueType is default constructible, the buffer is resized to avoid
// copying all elements. Otherwise, elements are copied from values.
// TODO(user): use placement new to avoid copying all elements, or
// std::allocator to avoid copying any elements.
template <typename Container>
Container BuildSameSizeContainer(const Container& values) {
  using ValueType =
      typename std::iterator_traits<decltype(std::begin(values))>::value_type;
  Container buffer;
  if constexpr (std::is_default_constructible_v<ValueType>) {
    buffer.resize(std::size(values));
  } else {
    buffer = values;
  }
  return buffer;
}

// Computes the minimum and maximum values over the given key extractor.
template <typename Container, typename KeyAccessor>
auto ComputeMinMax(const Container& values, const KeyAccessor get_key) {
  using ValueType =
      typename std::iterator_traits<decltype(std::begin(values))>::value_type;
  using KeyType = std::decay_t<std::invoke_result_t<KeyAccessor, ValueType&>>;
  KeyType key_min = std::numeric_limits<KeyType>::max();
  KeyType key_max = std::numeric_limits<KeyType>::lowest();
  for (const auto& item : values) {
    const KeyType key = get_key(item);
    key_min = std::min(key_min, key);
    key_max = std::max(key_max, key);
  }
  return std::make_pair(key_min, key_max);
}

// Computes the number of radix bits to use for a given key and bit width.
// If fixed_radix_bits is greater than 0, it will be used.
// Otherwise, the number of radix bits will be computed based on the bit
// width and the maximum number of passes, which is 5.
// We prefer to use fewer passes, so we may return 13 bits for 5 passes, even
// though the maximum radix bits is 12.
// Note that bit_width must be 1 or larger.
inline int ComputeNumRadixBits(const int fixed_radix_bits,
                               const int bit_width) {
  DCHECK_GT(bit_width, 0);
  if (fixed_radix_bits > 0) return fixed_radix_bits;
  constexpr int kMaxRadixBits = 12;
  constexpr int kMaxNumPasses = 5;
  const int num_passes =
      std::min(kMaxNumPasses, (bit_width + kMaxRadixBits - 1) / kMaxRadixBits);
  // Note that for num_passes == 5, we may return 13 bits, because we prefer to
  // use fewer passes.
  return num_passes == 1 ? bit_width  // Why spend more bits?
                         : (bit_width + num_passes - 1) / num_passes;
}

template <typename KeyType, typename Container, typename KeyAccessor>
void RadixSortMinMaxImpl(const int fixed_radix_bits, const KeyType key_min,
                         const KeyType key_max, Container& values,
                         Container& buffer, const KeyAccessor get_key) {
  CheckKeyAccessor<Container, KeyAccessor>();
  if (std::size(values) <= 1) return;

  auto& in = values;
  auto& out = buffer;

  const int num_bits = absl::bit_width(static_cast<uint64_t>(key_max) -
                                       static_cast<uint64_t>(key_min));
  if (num_bits == 0) return;  // All elements are identical.
  const int num_radix_bits = ComputeNumRadixBits(fixed_radix_bits, num_bits);
  const uint64_t radix = uint64_t{1} << num_radix_bits;
  std::vector<size_t> counts(radix, 0);
  std::vector<size_t> pos(radix, 0);
  for (int shift = 0; shift < num_bits; shift += num_radix_bits) {
    // Slightly reduce the number of mask bits if we are at the last pass to
    // avoid unnecessary computation.
    const int num_mask_bits = std::min(num_radix_bits, num_bits - shift);
    const uint64_t mask = (uint64_t{1} << num_mask_bits) - 1;
    CountItems(in, mask, shift, get_key, key_min, counts);
    Distribute(in, out, counts, mask, shift, get_key, key_min, pos);
  }
}

template <typename Container, typename KeyAccessor>
void AutoRadixSortImpl(const int fixed_radix_bits, Container& values,
                       Container& buffer, const KeyAccessor get_key) {
  const auto [key_min, key_max] = ComputeMinMax(values, get_key);
  RadixSortMinMaxImpl(fixed_radix_bits, key_min, key_max, values, buffer,
                      get_key);
}

// Terminating function for the recursive calls to MultikeyRadixSort.
template <typename Container, typename KeyAccessor>
void MultikeyRadixSortImpl(const int fixed_radix_bits, Container& values,
                           Container& buffer, const KeyAccessor get_key) {
  CheckContainer<Container>();
  AutoRadixSortImpl(fixed_radix_bits, values, buffer, get_key);
}

// Recursive function to sort a container of items by multiple keys using LSD
// (Least Significant Digit) radix sort.
template <typename Container, typename KeyAccessor,
          typename... KeyAccessorsTail>
void MultikeyRadixSortImpl(const int fixed_radix_bits, Container& values,
                           Container& buffer, const KeyAccessor get_key,
                           KeyAccessorsTail... tail) {
  CheckContainer<Container>();
  AutoRadixSortImpl(fixed_radix_bits, values, buffer, get_key);
  MultikeyRadixSortImpl(fixed_radix_bits, values, buffer, tail...);
}

}  // namespace radix_sort_internal

// Sorts a container of items by multiple keys using LSD (Least Significant
// Digit) radix sort. The fixed_radix_bits parameter specifies the number of
// bits to use for each radix. If 0, the number of bits will be computed
// automatically based on the key ranges.
template <typename Container, typename KeyAccessor,
          typename... KeyAccessorsTail>
void MultikeyRadixSort(const int fixed_radix_bits, Container& values,
                       const KeyAccessor get_key, KeyAccessorsTail... tail) {
  namespace rsi = radix_sort_internal;
  using ValueType =
      typename std::iterator_traits<decltype(std::begin(values))>::value_type;
  if constexpr (sizeof(Container) > rsi::kContainerSizeCutOff ||
                std::is_array_v<Container>) {
    std::vector<ValueType> vector_values(std::begin(values), std::end(values));
    std::vector<ValueType> vector_buffer =
        radix_sort_internal::BuildSameSizeContainer(vector_values);
    rsi::MultikeyRadixSortImpl(fixed_radix_bits, vector_values, vector_buffer,
                               get_key, tail...);
    absl::c_copy(vector_values, std::begin(values));
  } else {
    Container buffer = radix_sort_internal::BuildSameSizeContainer(values);
    rsi::MultikeyRadixSortImpl(fixed_radix_bits, values, buffer, get_key,
                               tail...);
  }
}

// Light version with automatic computation of num_radix_bits depending on the
// key ranges. Note: this is not yet implemented. kDefaultRadixBits is used
// instead.
template <typename Container, typename KeyAccessor,
          typename... KeyAccessorsTail>
void MultikeyRadixSort(Container& values, const KeyAccessor get_key,
                       KeyAccessorsTail... tail) {
  MultikeyRadixSort(0, values, get_key, tail...);
}

// Version on a container of integers, with a default key callable, and with
// automatic computation of num_radix_bits depending on the key range.
template <typename Container>
void MultikeyRadixSort(Container& values) {
  radix_sort_internal::CheckIntegralOrEnum<Container>();
  MultikeyRadixSort(0, values, [](const auto& val) { return val; });
}

// Version on a container of integers, with a default key accessor, and with
// the number of radix bits specified.
template <typename Container>
void MultikeyRadixSort(const int fixed_radix_bits, Container& values) {
  radix_sort_internal::CheckIntegralOrEnum<Container>();
  MultikeyRadixSort(fixed_radix_bits, values,
                    [](const auto& val) { return val; });
}

// External API for the radix sort with key bounds. Sorts a container of items
// by multiple keys using LSD (Least Significant Digit) radix sort, with given
// key bounds. The fixed_radix_bits parameter specifies the number of bits to
// use for each radix. If 0, the number of bits will be computed automatically
// based on the key ranges.
template <typename KeyType, typename Container, typename KeyAccessor>
void RadixSortMinMax(const int fixed_radix_bits, const KeyType key_min,
                     const KeyType key_max, Container& values,
                     KeyAccessor get_key) {
  namespace rsi = radix_sort_internal;
  rsi::CheckContainer<Container>();
  using ValueType =
      typename std::iterator_traits<decltype(std::begin(values))>::value_type;
  if constexpr (sizeof(Container) > rsi::kContainerSizeCutOff ||
                std::is_array_v<Container>) {
    std::vector<ValueType> vector_values(std::begin(values), std::end(values));
    std::vector<ValueType> vector_buffer =
        rsi::BuildSameSizeContainer(vector_values);
    rsi::RadixSortMinMaxImpl(fixed_radix_bits, key_min, key_max, vector_values,
                             vector_buffer, get_key);
    absl::c_copy(vector_values, std::begin(values));
  } else {
    Container buffer = radix_sort_internal::BuildSameSizeContainer(values);
    rsi::RadixSortMinMaxImpl(fixed_radix_bits, key_min, key_max, values, buffer,
                             get_key);
  }
}

template <typename KeyType, typename Container, typename KeyAccessor>
void RadixSortMinMax(const KeyType key_min, const KeyType key_max,
                     Container& values, KeyAccessor get_key) {
  operations_research::RadixSortMinMax(0, key_min, key_max, values, get_key);
}

}  // namespace operations_research

#endif  // ORTOOLS_ALGORITHMS_MULTIKEY_RADIX_SORT_H_
