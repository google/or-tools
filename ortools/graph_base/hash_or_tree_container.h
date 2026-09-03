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

// Automatically selects the container type (hash or btree) for sets of objects,
// or maps of objects to integers, depending on whether the user provides a
// hash functor or a comparator. See usage examples in connected_components.h.

#ifndef UTIL_GRAPH_HASH_OR_TREE_CONTAINER_H_
#define UTIL_GRAPH_HASH_OR_TREE_CONTAINER_H_

#include <functional>
#include <type_traits>
#include <utility>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/hash_container_defaults.h"

namespace util {
namespace graph {

namespace internal {

// Detects if T is hashable with absl::Hash.
template <typename T>
using IsHashable =
    std::is_default_constructible<absl::DefaultHashContainerHash<T>>;

template <typename T, typename = void>
struct DefaultHasher {
  using type = void;
};

template <typename T>
struct DefaultHasher<T, std::enable_if_t<IsHashable<T>::value>> {
  using type = typename absl::flat_hash_set<T>::hasher;
};

}  // namespace internal

// Selects absl::flat_hash_set<T>::hasher if possible, otherwise std::less<T>.
template <typename T>
using PreferHashOrCompare =
    std::conditional_t<internal::IsHashable<T>::value,
                       typename internal::DefaultHasher<T>::type, std::less<T>>;

template <typename T, typename CompareOrHashT = PreferHashOrCompare<T>,
          typename Eq = void>
struct HashOrTreeContainer {
  // SFINAE trait to detect hash functors and select unordered containers if so,
  // and ordered containers otherwise (= by default).
  template <typename U, typename V, typename E = void>
  struct SelectContainer {
    using Set = absl::btree_set<T, CompareOrHashT>;

    template <typename ValueT>
    using Map = absl::btree_map<T, ValueT, CompareOrHashT>;
  };

  // Specialization for when U is a hash functor and Eq is void (no custom
  // equality).
  // The expression inside decltype is basically saying that "H(x)" is
  // well-formed, where H is an instance of U and x is an instance of T, and is
  // a value of integral type. That is, we are "duck-typing" on whether U looks
  // like a hash functor.
  template <typename U, typename V>
  struct SelectContainer<
      U, V,
      std::enable_if_t<std::is_integral<decltype(std::declval<const U&>()(
                           std::declval<const T&>()))>::value &&
                       std::is_same_v<V, void>>> {
    using Set = absl::flat_hash_set<T, CompareOrHashT>;

    template <typename ValueT>
    using Map = absl::flat_hash_map<T, ValueT, CompareOrHashT>;
  };

  // Specialization for when U is a hash functor and Eq is provided (not void).
  template <typename U, typename V>
  struct SelectContainer<
      U, V,
      std::enable_if_t<std::is_integral<decltype(std::declval<const U&>()(
                           std::declval<const T&>()))>::value &&
                       !std::is_same_v<V, void>>> {
    using Set = absl::flat_hash_set<T, CompareOrHashT, Eq>;

    template <typename ValueT>
    using Map = absl::flat_hash_map<T, ValueT, CompareOrHashT, Eq>;
  };

  using Set = typename SelectContainer<CompareOrHashT, Eq>::Set;

  template <typename ValueT>
  using Map =
      typename SelectContainer<CompareOrHashT, Eq>::template Map<ValueT>;

  using MapInt =
      typename SelectContainer<CompareOrHashT, Eq>::template Map<int>;
};

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_HASH_OR_TREE_CONTAINER_H_
