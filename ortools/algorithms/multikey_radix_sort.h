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
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/numeric/bits.h"

namespace operations_research {

// Transformation Functions. These are bijections from a floating point number
// to an unsigned integer number of the same size. They preserve the order of
// the numbers. This makes it possible to sort floating point numbers using
// integer-based sorting algorithms like radix sort, while keeping the stability
// of the sort.
//
// Although it was rediscovered here, the idea is not new, but still quite
// recent considering the history of sorting algorithms. The following
// references, mostly from the graphics and game development world, describe the
// transformation in detail.

// Pierre Terdiman, 2000, "Radix Sort Revisited",
// https://codercorner.com/RadixSortRevisited.htm
// Michael Herf, 2001, "Radix Tricks", https://stereopsis.com/radix.html

// Mapping from float to sortable uint.
// 1. IEEE 754 floats are sign-magnitude. For positive values, the raw bits
// already sort correctly. For negative values, larger magnitudes have
// larger raw integer values, which is the inverse of the desired order.
// 2. If the float is negative (MSB is 1):
// - We flip all the bits (one's complement).
// - This flips the magnitude bits (reversing the order) AND flips the
// sign bit to 0.
// - Result: Large negative magnitudes (like -inf) become small integers,
//   and -0.0 becomes the largest value in the lower half (0x7FF...FF).
// 3. If the float is positive (MSB is 0):
// - We only flip the MSB to 1 (i,e. add 0x80...00 to shift the range)
// - Result: 0.0 becomes 0x80...00, and positive values fill the
//  range up to 0xFF...FF (Positive NaNs).
// 4. This places all negative numbers below all positive numbers and
// ensures -0.0 and 0.0 are adjacent (0x7F...FF and 0x80...00).
template <typename T>
  requires std::is_same_v<T, float> || std::is_same_v<T, double>
auto FloatToSortableUint(const T f) {
  static_assert(std::numeric_limits<T>::is_iec559);
  static_assert(std::numeric_limits<T>::radix == 2);
  static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32 bits.");
  static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits.");
  using U = std::conditional_t<
      std::is_same_v<T, float>, uint32_t,
      std::conditional_t<std::is_same_v<T, double>, uint64_t, void>>;
  const U u = absl::bit_cast<U>(f);
  const U msb_only = U{1} << (std::numeric_limits<U>::digits - 1);
  const bool msb_set = u & msb_only;
  // Take one's complement if float is negative, and just flip the sign bit if
  // positive.
  return msb_set ? ~u : u ^ msb_only;
}

// Inverse mapping from sortable uint to float.
// 1. In the sortable space, we check the MSB.
// 2. If MSB is 1:
// - The value was originally positive: only flip the MSB back to 0.
// 3. If MSB is 0:
// - The value was originally negative: flip all bits (one's complement).
// Note that the function is very similar to FloatToSortableUint.
template <typename U>
  requires std::is_same_v<U, uint32_t> || std::is_same_v<U, uint64_t>
auto SortableUintToFloat(const U u) {
  using T = std::conditional_t<
      std::is_same_v<U, uint32_t>, float,
      std::conditional_t<std::is_same_v<U, uint64_t>, double, void>>;
  const U msb_only = U{1} << (std::numeric_limits<U>::digits - 1);
  const bool msb_set = u & msb_only;
  // Take one's complement if negative, and just flip the sign bit if positive.
  return absl::bit_cast<T>(msb_set ? u ^ msb_only : ~u);
}

enum class RadixSortDirection {
  kIncreasing,
  kDecreasing,
};

// Sorts a container of items by multiple keys using LSD (Least Significant
// Digit) radix sort. The keys must be signed or unsigned integers or
// enumerations. The sorting is stable, because we use LSD radix sort. MSD
// radix sort on the other hand can be unstable. See
// https://en.wikipedia.org/wiki/Radix_sort.
//
// In this version, keys must be integer types or castable to integer types.
// Keys are extracted from the items by key access callables. (`get_key`). The
// container must be swappable and indexable. For optimal performance, it
// should also have an O(1) swap operation (e.g. std::vector, std::deque). The
// items must be copy constructible and copy assignable.
//
// To sort in lexicographical order (i.e. sort by a primary key, then a
// secondary key, etc.), the key access functors must be provided in reverse
// order of importance, i.e. from least significant key to most significant
// key.
//
// Example:
//   struct Item { uint32_t a; uint32_t b; };
//   std::vector<Item> items = {{2, 1}, {1, 2}, {1, 1}};
//   // Sort primarily by 'a' and secondarily by 'b'.
//   MultikeyHistogramRadixSort(items,
//                     [](const Item& item) { return item.b; },
//                     [](const Item& item) { return item.a; });
//   // items is now {{1, 1}, {1, 2}, {2, 1}}
//
// The default radix size is 12 bits, which is a good balance between the
// number of passes and the size of the counts array. If you happen to know
// the size of your keys, you may want to use a different radix size for
// better performance.
//
// The container must be swappable and indexable. For optimal performance, it
// should also have an O(1) swap operation (e.g. std::vector, std::deque).
//
// TODO(user): Add the following features, to sort by:
//   - decreasing order.
//   - subset of bits of a key.
//   - floating keys, finally.
//
namespace radix_sort_internal {
constexpr int kContainerSizeCutOff = 8 * sizeof(uint64_t);

// Checks that the container is swappable and indexable, and that the value
// type is copy constructible and copy assignable. If not, the function will
// fail at compile time.
template <typename Container>
constexpr void CheckContainer() {
  // Also check that the underlying integer types are two's complement and
  // wrap around on overflow, as the radix sort algorithm assumes this.
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
  using KeyType =
      std::remove_cvref_t<std::invoke_result_t<KeyAccessor, ValueType>>;
  static_assert(std::is_integral_v<KeyType> || std::is_enum_v<KeyType> ||
                    std::is_floating_point_v<KeyType>,
                "Key type must be integral, enum or floating point.");
  static_assert(sizeof(KeyType) <= sizeof(uint64_t),
                "Key type must be at most 64 bits.");
}

template <typename Container>
constexpr void CheckIntegralEnumOrFloat() {
  using ValueType = typename std::iterator_traits<decltype(std::begin(
      std::declval<Container&>()))>::value_type;
  static_assert(std::is_integral_v<ValueType> || std::is_enum_v<ValueType> ||
                    std::is_same_v<ValueType, float> ||
                    std::is_same_v<ValueType, double>,
                "Container value type must be integral, enum or floating point "
                "for this overload.");
}

// Returns the radix for a given key and shift after subtracting the minimum
// value of the key to make sure the it is computed on a range starting at 0.
// For floating point values, the key is converted to a sortable uint.
// NOTE(user): This is the only function that uses the RadixSortDirection.
// All the other functions are templated on the direction and pass the direction
// as a template parameter to be used by this function.
template <RadixSortDirection Direction, typename KeyType>
uint64_t GetRadixKey(const uint64_t mask, const int shift, const KeyType key,
                     const KeyType key_min, const KeyType key_max) {
  uint64_t integer_key;
  if constexpr (std::is_floating_point_v<KeyType>) {
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      integer_key = FloatToSortableUint(key) - FloatToSortableUint(key_min);
    } else {
      integer_key = FloatToSortableUint(key_max) - FloatToSortableUint(key);
    }
  } else {
    if constexpr (Direction == RadixSortDirection::kIncreasing) {
      integer_key = static_cast<uint64_t>(key) - static_cast<uint64_t>(key_min);
    } else {
      integer_key = static_cast<uint64_t>(key_max) - static_cast<uint64_t>(key);
    }
  }
  return (integer_key >> shift) & mask;
}

// Updates count with the number of items for each radix in the given key.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename KeyType>
void CountItems(const Container& in, const uint64_t mask, const int shift,
                const KeyAccessor key_getter, const KeyType key_min,
                const KeyType key_max, std::vector<size_t>& counts) {
  std::fill(counts.begin(), counts.end(), 0);
  for (const auto& item : in) {
    ++counts[GetRadixKey<Direction>(mask, shift, key_getter(item), key_min,
                                    key_max)];
  }
}

// Computes histograms for each radix pass.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename KeyType>
void ComputeHistograms(const Container& in, const int num_bits,
                       const int num_radix_bits, const KeyAccessor key_getter,
                       const KeyType key_min, const KeyType key_max,
                       std::vector<std::vector<size_t>>& histograms) {
  const int num_passes = (num_bits + num_radix_bits - 1) / num_radix_bits;
  DCHECK_EQ(histograms.size(), num_passes);
  for (int p = 0; p < num_passes; ++p) {
    absl::c_fill(histograms[p], 0);
  }
  for (const auto& item : in) {
    const auto key = key_getter(item);
    for (int p = 0; p < num_passes; ++p) {
      const int shift = p * num_radix_bits;
      const int num_mask_bits = std::min(num_radix_bits, num_bits - shift);
      const uint64_t mask = (uint64_t{1} << num_mask_bits) - 1;
      ++histograms[p]
                  [GetRadixKey<Direction>(mask, shift, key, key_min, key_max)];
    }
  }
}

// Distributes the items into the buffer based on the given key and swaps
// 'in' and 'out'.
// Positions of each radix in the output buffer. pos is expected to have the
// same size as counts.
// pos does not need to be reset as it is just the cumulative sum of counts.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename KeyType>
void Distribute(Container& in, Container& out,
                const std::vector<size_t>& counts, const uint64_t mask,
                const int shift, const KeyAccessor key_getter,
                const KeyType key_min, const KeyType key_max,
                std::vector<size_t>& pos) {
  // TODO(user): Merge counts and pos into a single vector?
  pos[0] = 0;  // We just need to reset the first element.
  for (int i = 1; i < pos.size(); ++i) {
    pos[i] = pos[i - 1] + counts[i - 1];
  }
  for (const auto& item : in) {
    std::begin(out)[pos[GetRadixKey<Direction>(mask, shift, key_getter(item),
                                               key_min, key_max)]++] = item;
  }
  using std::swap;
  swap(in, out);
}

template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename KeyType>
void DistributeHistograms(Container& in, Container& out, int num_bits,
                          int num_radix_bits, const KeyAccessor key_getter,
                          const KeyType key_min, const KeyType key_max,
                          std::vector<std::vector<size_t>>& histograms) {
  const int num_passes = (num_bits + num_radix_bits - 1) / num_radix_bits;
  const size_t radix = histograms[0].size();
  CHECK_EQ(histograms.size(), num_passes);
  CHECK_EQ(1 << num_radix_bits, radix);

  std::vector<size_t> sum(num_passes, 0);
  for (size_t i = 0; i < radix; ++i) {
    // This inner loop should be parallelizable by the CPU.
    for (int p = 0; p < num_passes; ++p) {
      const size_t saved_sum = sum[p];
      sum[p] += histograms[p][i];
      histograms[p][i] = saved_sum;
    }
  }

  for (int p = 0; p < num_passes; ++p) {
    const int shift = p * num_radix_bits;
    const int num_mask_bits = std::min(num_radix_bits, num_bits - shift);
    const uint64_t mask = (uint64_t{1} << num_mask_bits) - 1;
    std::vector<size_t>& histogram = histograms[p];
    for (const auto& item : in) {
      const uint64_t radix_key = GetRadixKey<Direction>(
          mask, shift, key_getter(item), key_min, key_max);
      std::begin(out)[histogram[radix_key]++] = item;
    }
    using std::swap;
    swap(in, out);
  }
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
  using KeyType =
      std::remove_cvref_t<std::invoke_result_t<KeyAccessor, ValueType>>;
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
inline int ComputeNumRadixBits(const int bit_width) {
  DCHECK_GT(bit_width, 0);
  constexpr int kMaxRadixBits = 12;
  constexpr int kMaxNumPasses = 5;
  const int num_passes =
      std::min(kMaxNumPasses, (bit_width + kMaxRadixBits - 1) / kMaxRadixBits);
  // Note that for num_passes == 5, we may return 13 bits, because we prefer
  // to use fewer passes.
  return num_passes == 1 ? bit_width  // Why spend more bits?
                         : (bit_width + num_passes - 1) / num_passes;
}

template <RadixSortDirection Direction, typename KeyType, typename Container,
          typename KeyAccessor>
void RadixSortImpl(const KeyType key_min, const KeyType key_max,
                   Container& values, Container& buffer,
                   const KeyAccessor get_key) {
  CheckKeyAccessor<Container, KeyAccessor>();
  if (std::size(values) <= 1) return;

  auto& in = values;
  auto& out = buffer;

  int num_bits = 0;
  if constexpr (std::is_floating_point_v<KeyType>) {
    num_bits = absl::bit_width(FloatToSortableUint(key_max) -
                               FloatToSortableUint(key_min));
  } else {
    num_bits = absl::bit_width(static_cast<uint64_t>(key_max) -
                               static_cast<uint64_t>(key_min));
  }
  if (num_bits == 0) return;  // All elements are identical.
  const int num_radix_bits = ComputeNumRadixBits(num_bits);
  const uint64_t radix = uint64_t{1} << num_radix_bits;
  std::vector<size_t> counts(radix, 0);
  std::vector<size_t> pos(radix, 0);
  for (int shift = 0; shift < num_bits; shift += num_radix_bits) {
    // Slightly reduce the number of mask bits if we are at the last pass to
    // avoid unnecessary computation.
    const int num_mask_bits = std::min(num_radix_bits, num_bits - shift);
    const uint64_t mask = (uint64_t{1} << num_mask_bits) - 1;
    CountItems<Direction>(in, mask, shift, get_key, key_min, key_max, counts);
    Distribute<Direction>(in, out, counts, mask, shift, get_key, key_min,
                          key_max, pos);
  }
}

template <RadixSortDirection Direction, typename KeyType, typename Container,
          typename KeyAccessor>
void HistogramRadixSortImpl(const KeyType key_min, const KeyType key_max,
                            Container& values, Container& buffer,
                            const KeyAccessor get_key) {
  CheckKeyAccessor<Container, KeyAccessor>();
  if (std::size(values) <= 1) return;

  int num_bits = 0;
  if constexpr (std::is_floating_point_v<KeyType>) {
    num_bits = absl::bit_width(FloatToSortableUint(key_max) -
                               FloatToSortableUint(key_min));
  } else {
    num_bits = absl::bit_width(static_cast<uint64_t>(key_max) -
                               static_cast<uint64_t>(key_min));
  }
  if (num_bits == 0) return;  // All elements are identical.

  const int num_radix_bits = ComputeNumRadixBits(num_bits);
  const int num_passes = (num_bits + num_radix_bits - 1) / num_radix_bits;
  const uint64_t radix = uint64_t{1} << num_radix_bits;
  std::vector<std::vector<size_t>> histograms(num_passes,
                                              std::vector<size_t>(radix, 0));
  ComputeHistograms<Direction>(values, num_bits, num_radix_bits, get_key,
                               key_min, key_max, histograms);
  DistributeHistograms<Direction>(values, buffer, num_bits, num_radix_bits,
                                  get_key, key_min, key_max, histograms);
}

template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void AutoRadixSortImpl(Container& values, Container& buffer,
                       const KeyAccessor get_key) {
  const auto [key_min, key_max] = ComputeMinMax(values, get_key);
  RadixSortImpl<Direction>(key_min, key_max, values, buffer, get_key);
}

template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void MultikeyRadixSortImpl(Container& values, Container& buffer,
                           const KeyAccessor get_key) {
  CheckContainer<Container>();
  AutoRadixSortImpl<Direction>(values, buffer, get_key);
}

template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename... KeyAccessorsTail>
void MultikeyRadixSortImpl(Container& values, Container& buffer,
                           const KeyAccessor get_key,
                           KeyAccessorsTail... tail) {
  CheckContainer<Container>();
  AutoRadixSortImpl<Direction>(values, buffer, get_key);
  MultikeyRadixSortImpl<Direction>(values, buffer, tail...);
}

template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void AutoHistogramRadixSortImpl(Container& values, Container& buffer,
                                const KeyAccessor get_key) {
  const auto [key_min, key_max] = ComputeMinMax(values, get_key);
  HistogramRadixSortImpl<Direction>(key_min, key_max, values, buffer, get_key);
}

// Terminating function for the recursive calls to MultikeyHistogramRadixSort.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void MultikeyHistogramRadixSortImpl(Container& values, Container& buffer,
                                    const KeyAccessor get_key) {
  CheckContainer<Container>();
  AutoHistogramRadixSortImpl<Direction>(values, buffer, get_key);
}

// Recursive function to sort a container of items by multiple keys using LSD
// (Least Significant Digit) radix sort.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor, typename... KeyAccessorsTail>
void MultikeyHistogramRadixSortImpl(Container& values, Container& buffer,
                                    const KeyAccessor get_key,
                                    KeyAccessorsTail... tail) {
  CheckContainer<Container>();
  AutoHistogramRadixSortImpl<Direction>(values, buffer, get_key);
  MultikeyHistogramRadixSortImpl<Direction>(values, buffer, tail...);
}

// Wrapper function to sort a container of items using radix sort.
// If the container is larger than a certain size, it will be copied to a
// vector, otherwise it will be sorted in place with a temporary buffer of the
// same size as the container.
// This makes it possible to sort containers that are not swappable, such as
// std::array or C-style arrays.
template <typename Container, typename SortImpl>
void SortWrapper(Container& values, SortImpl sort_impl) {
  using ValueType =
      typename std::iterator_traits<decltype(std::begin(values))>::value_type;
  if constexpr (sizeof(Container) > kContainerSizeCutOff ||
                std::is_array_v<Container>) {
    std::vector<ValueType> vector_values(std::begin(values), std::end(values));
    std::vector<ValueType> vector_buffer =
        BuildSameSizeContainer(vector_values);
    sort_impl(vector_values, vector_buffer);
    absl::c_copy(vector_values, std::begin(values));
  } else {
    Container buffer = BuildSameSizeContainer(values);
    sort_impl(values, buffer);
  }
}

}  // namespace radix_sort_internal

// Sorts a container of items by key using radix sort.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename KeyAccessor>
void AutoRadixSort(Container& values, const KeyAccessor get_key) {
  namespace rsi = radix_sort_internal;
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::AutoRadixSortImpl<Direction>(vals, buff, get_key);
  });
}

// Version on a container of integers, with a default key callable, and with
// automatic computation of num_radix_bits depending on the key range.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container>
void AutoRadixSort(Container& values) {
  radix_sort_internal::CheckIntegralEnumOrFloat<Container>();
  AutoRadixSort<Direction>(values, [](const auto& val) { return val; });
}

// External API for the radix sort with key bounds. Sorts a container of items
// by multiple keys using LSD (Least Significant Digit) radix sort, with given
// key bounds.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
void RangeRadixSort(const KeyType key_min, const KeyType key_max,
                    Container& values, KeyAccessor get_key) {
  namespace rsi = radix_sort_internal;
  rsi::CheckContainer<Container>();
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::RadixSortImpl<Direction>(key_min, key_max, vals, buff, get_key);
  });
}

// Version on a container of integers, with a default key callable.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container>
void RangeRadixSort(const KeyType key_min, const KeyType key_max,
                    Container& values) {
  radix_sort_internal::CheckIntegralEnumOrFloat<Container>();
  RangeRadixSort<Direction>(key_min, key_max, values,
                            [](const auto& val) { return val; });
}

// Multikey radix sort sorts by multiple keys by calling AutoRadixSort
// for each key. The key accessors are called in the order they are specified
// in the function call, i.e. the first key accessor is the lowest-rank sort
// key, etc...
//
// This has the advantage of creating only one temporary container, regardless
// of the number of keys.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
void MultikeyRadixSort(Container& values, KeyAccessors... key_accessors) {
  namespace rsi = radix_sort_internal;
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::MultikeyRadixSortImpl<Direction>(vals, buff, key_accessors...);
  });
}

// ----------------------------------------------------------------------------
// Histogram radix sort.
// ----------------------------------------------------------------------------

// Sorts a container of items by key using radix sort.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename KeyAccessor>
void AutoHistogramRadixSort(Container& values, const KeyAccessor get_key) {
  namespace rsi = radix_sort_internal;
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::AutoHistogramRadixSortImpl<Direction>(vals, buff, get_key);
  });
}

// Version on a container of integers, with a default key callable, and with
// automatic computation of num_radix_bits depending on the key range.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container>
void AutoHistogramRadixSort(Container& values) {
  radix_sort_internal::CheckIntegralEnumOrFloat<Container>();
  AutoHistogramRadixSort<Direction>(values,
                                    [](const auto& val) { return val; });
}

// External API for the radix sort with key bounds. Sorts a container of items
// by multiple keys using LSD (Least Significant Digit) radix sort, with given
// key bounds.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
void RangeHistogramRadixSort(const KeyType key_min, const KeyType key_max,
                             Container& values, KeyAccessor get_key) {
  namespace rsi = radix_sort_internal;
  rsi::CheckContainer<Container>();
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::HistogramRadixSortImpl<Direction>(key_min, key_max, vals, buff,
                                           get_key);
  });
}

// Version on a container of integers, with a default key callable.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container>
void RangeHistogramRadixSort(const KeyType key_min, const KeyType key_max,
                             Container& values) {
  radix_sort_internal::CheckIntegralEnumOrFloat<Container>();
  RangeHistogramRadixSort<Direction>(key_min, key_max, values,
                                     [](const auto& val) { return val; });
}

// Multikey radix sort sorts by multiple keys by calling
// AutoHistogramRadixSort for each key. The key accessors are called in the
// order they are specified in the function call, i.e. the first key accessor
// is the lowest-rank sort key, etc...
//
// This has the advantage of creating only one temporary container, regardless
// of the number of keys.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
void MultikeyHistogramRadixSort(Container& values,
                                KeyAccessors... key_accessors) {
  namespace rsi = radix_sort_internal;
  rsi::SortWrapper(values, [&](auto& vals, auto& buff) {
    rsi::MultikeyHistogramRadixSortImpl<Direction>(vals, buff,
                                                   key_accessors...);
  });
}

namespace radix_sort_internal {

template <typename Container, typename KeyAccessor>
[[nodiscard]] auto CreateTempKeys(const Container& values,
                                  const KeyAccessor get_key) {
  const std::size_t n = std::size(values);
  std::vector<std::remove_cvref_t<decltype(get_key(std::size_t{}))>> temp_keys;
  temp_keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    temp_keys.push_back(get_key(i));
  }
  return temp_keys;
}

// Extracts keys using the key accessor into a flat temporary vector, and sorts
// the permutation vector using raw AutoRadixSort.
// Unique aspect: Unlike other radix sort internals that reorder the values
// themselves (writing back and forth to buffers), this helper keeps the values
// container completely const. We extract and buffer keys once up-front to
// construct a contiguous array. This achieves cache locality during radix
// sweeps and guarantees the user's get_key callback is invoked exactly once
// per element.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void SortPermutationStep(std::vector<std::size_t>& permutation,
                         const Container& values, const KeyAccessor get_key) {
  auto temp_keys = CreateTempKeys(values, get_key);
  AutoRadixSort<Direction>(permutation,
                           [&](const std::size_t i) { return temp_keys[i]; });
}

// Splits the keys out and sorts the permutation using RangeRadixSort.
template <RadixSortDirection Direction, typename KeyType, typename Container,
          typename KeyAccessor>
void SortRangePermutationStep(std::vector<std::size_t>& permutation,
                              const KeyType key_min, const KeyType key_max,
                              const Container& values,
                              const KeyAccessor get_key) {
  auto temp_keys = CreateTempKeys(values, get_key);
  RangeRadixSort<Direction>(key_min, key_max, permutation,
                            [&](const std::size_t i) { return temp_keys[i]; });
}

// Similar to SortPermutationStep but uses AutoHistogramRadixSort for
// histogram-based pass sorting of the index permutation.
template <RadixSortDirection Direction, typename Container,
          typename KeyAccessor>
void SortPermutationHistogramStep(std::vector<std::size_t>& permutation,
                                  const Container& values,
                                  const KeyAccessor get_key) {
  auto temp_keys = CreateTempKeys(values, get_key);
  AutoHistogramRadixSort<Direction>(
      permutation, [&](const std::size_t i) { return temp_keys[i]; });
}

// Splits the keys out and sorts the permutation using RangeHistogramRadixSort.
template <RadixSortDirection Direction, typename KeyType, typename Container,
          typename KeyAccessor>
void SortRangePermutationHistogramStep(std::vector<std::size_t>& permutation,
                                       const KeyType key_min,
                                       const KeyType key_max,
                                       const Container& values,
                                       const KeyAccessor get_key) {
  auto temp_keys = CreateTempKeys(values, get_key);
  RangeHistogramRadixSort<Direction>(
      key_min, key_max, permutation,
      [&](const std::size_t i) { return temp_keys[i]; });
}

}  // namespace radix_sort_internal

// Represents a permutation of indices that can be applied to any container.
class [[nodiscard]] RadixSortPermutation {
 public:
  explicit RadixSortPermutation(std::size_t n) : permutation_(n) {
    std::iota(permutation_.begin(), permutation_.end(), 0);
  }
  explicit RadixSortPermutation(std::vector<std::size_t> permutation)
      : permutation_(std::move(permutation)) {}

  std::size_t size() const { return permutation_.size(); }

  const std::vector<std::size_t>& permutation() const { return permutation_; }
  std::vector<std::size_t>& permutation() { return permutation_; }

  // Returns true if the wrapped vector is a valid permutation of [0, ..., n-1].
  bool IsValid() const {
    const std::size_t n = permutation_.size();
    std::vector<bool> seen(n, false);
    for (const std::size_t index : permutation_) {
      if (index >= n || seen[index]) {
        return false;
      }
      seen[index] = true;
    }
    return true;
  }

  // Applies the permutation to the container in-place.
  template <typename Container>
  void ApplyTo(Container& values) const {
    DCHECK(IsValid());
    const std::size_t n = std::size(values);
    DCHECK_EQ(n, permutation_.size());
    std::vector<bool> visited(n, false);
    for (std::size_t i = 0; i < n; ++i) {
      if (visited[i]) continue;
      std::size_t current = i;
      std::size_t next = permutation_[current];
      if (next == current) {  // Cycle of length 1.
        visited[current] = true;
        continue;
      }
      auto final_value_at_step_i = std::move(std::begin(values)[current]);
      while (next != i) {
        std::begin(values)[current] = std::move(std::begin(values)[next]);
        DCHECK(!visited[current]);
        visited[current] = true;
        current = next;
        next = permutation_[current];
      }
      std::begin(values)[current] = std::move(final_value_at_step_i);
      visited[current] = true;
    }
    DCHECK(std::all_of(visited.begin(), visited.end(), std::identity{}));
  }

 private:
  std::vector<std::size_t> permutation_;
};

// Updates the permutation vector in-place, sorting the elements of 'values' by
// the keys returned by the given key accessors, starting from the given
// permutation, without modifying 'values'.
//
// Example usage for cascaded sorting:
//   RadixSortPermutation perm = ComputeAutoRadixSortPermutation(
//       container, [&](const std::size_t i) { return secondary_keys[i]; });
//   UpdateAutoRadixSortPermutation(
//       perm, container, [&](const std::size_t i) { return primary_keys[i]; });
//   perm.ApplyTo(container);
//
// This function is the most efficient for sorting by multiple keys, as it
// creates and handles only one permutation. Moving the different container
// elements is done only once, at the end. This can be a large saving if the
// elements are large or move-only.
// Note: This function is not templated on the key type because all keys are
// extracted up-front into a temporary container, and the permutation is sorted
// using AutoRadixSort.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
void UpdateAutoRadixSortPermutation(RadixSortPermutation& permutation,
                                    const Container& values,
                                    KeyAccessors... key_accessors) {
  const std::size_t n = std::size(values);
  DCHECK_EQ(n, permutation.size());
  if (n <= 1) return;

  (radix_sort_internal::SortPermutationStep<Direction>(
       permutation.permutation(), values, key_accessors),
   ...);
}

// Simplified interface for ComputeAutoRadixSortPermutation that creates the
// permutation vector internally.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
RadixSortPermutation ComputeAutoRadixSortPermutation(
    const Container& values, KeyAccessors... key_accessors) {
  RadixSortPermutation permutation(std::size(values));
  UpdateAutoRadixSortPermutation<Direction>(permutation, values,
                                            key_accessors...);
  return permutation;
}

// Updates the permutation vector in-place, sorting the elements of 'values'
// by the keys returned by the given key accessor on the range [key_min,
// key_max], starting from the given permutation, without modifying 'values'.
// Note: Range-based radix sorting is limited to a single key accessor because
// specifying explicit bounds for multiple keys would require a complex list of
// bounds pairs. Lexicographical multikey sorting with ranges is better handled
// by the Auto variant (which scans for bounds dynamically) or by serializing
// single-key Range sort passes.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
void UpdateRangeRadixSortPermutation(RadixSortPermutation& permutation,
                                     const KeyType key_min,
                                     const KeyType key_max,
                                     const Container& values,
                                     const KeyAccessor get_key) {
  const std::size_t n = std::size(values);
  DCHECK_EQ(n, permutation.size());
  if (n <= 1) return;

  radix_sort_internal::SortRangePermutationStep<Direction>(
      permutation.permutation(), key_min, key_max, values, get_key);
}

// Simplified interface for ComputeRangeRadixSortPermutation that creates the
// permutation vector internally.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
RadixSortPermutation ComputeRangeRadixSortPermutation(
    const KeyType key_min, const KeyType key_max, const Container& values,
    const KeyAccessor get_key) {
  RadixSortPermutation permutation(std::size(values));
  UpdateRangeRadixSortPermutation<Direction>(permutation, key_min, key_max,
                                             values, get_key);
  return permutation;
}

// Updates the permutation vector in-place, sorting the elements of 'values'
// by the keys returned by the given key accessors, starting from the given
// permutation, without modifying 'values', using histogram radix sort.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
void UpdateAutoHistogramRadixSortPermutation(RadixSortPermutation& permutation,
                                             const Container& values,
                                             KeyAccessors... key_accessors) {
  const std::size_t n = std::size(values);
  DCHECK_EQ(n, permutation.size());
  if (n <= 1) return;

  (radix_sort_internal::SortPermutationHistogramStep<Direction>(
       permutation.permutation(), values, key_accessors),
   ...);
}

// Simplified interface for ComputeAutoHistogramRadixSortPermutation that
// creates the permutation vector internally.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename Container, typename... KeyAccessors>
RadixSortPermutation ComputeAutoHistogramRadixSortPermutation(
    const Container& values, KeyAccessors... key_accessors) {
  RadixSortPermutation permutation(std::size(values));
  UpdateAutoHistogramRadixSortPermutation<Direction>(permutation, values,
                                                     key_accessors...);
  return permutation;
}

// Updates the permutation vector in-place, sorting the elements of 'values'
// by the keys returned by the given key accessor on the range [key_min,
// key_max], starting from the given permutation, without modifying 'values',
// using histogram radix sort.
// Note: Range-based radix sorting is limited to a single key accessor because
// specifying explicit bounds for multiple keys would require a complex list of
// bounds pairs. Lexicographical multikey sorting with ranges is better handled
// by the Auto variant (which scans for bounds dynamically) or by serializing
// single-key Range sort passes.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
void UpdateRangeHistogramRadixSortPermutation(RadixSortPermutation& permutation,
                                              const KeyType key_min,
                                              const KeyType key_max,
                                              const Container& values,
                                              const KeyAccessor get_key) {
  const std::size_t n = std::size(values);
  DCHECK_EQ(n, permutation.size());
  if (n <= 1) return;
  radix_sort_internal::SortRangePermutationHistogramStep<Direction>(
      permutation.permutation(), key_min, key_max, values, get_key);
}

// Simplified interface for ComputeRangeHistogramRadixSortPermutation that
// creates the permutation vector internally.
template <RadixSortDirection Direction = RadixSortDirection::kIncreasing,
          typename KeyType, typename Container, typename KeyAccessor>
RadixSortPermutation ComputeRangeHistogramRadixSortPermutation(
    const KeyType key_min, const KeyType key_max, const Container& values,
    const KeyAccessor get_key) {
  RadixSortPermutation permutation(std::size(values));
  UpdateRangeHistogramRadixSortPermutation<Direction>(permutation, key_min,
                                                      key_max, values, get_key);
  return permutation;
}

}  // namespace operations_research

#endif  // ORTOOLS_ALGORITHMS_MULTIKEY_RADIX_SORT_H_
