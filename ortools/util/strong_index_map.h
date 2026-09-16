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

#ifndef ORTOOLS_UTIL_STRONG_INDEX_MAP_H_
#define ORTOOLS_UTIL_STRONG_INDEX_MAP_H_

#include <functional>
#include <optional>
#include <utility>

#include "absl/base/casts.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph_base/hash_or_tree_container.h"
#include "ortools/graph_base/index.h"

namespace operations_research {

// Like util::graph::Index<T>, but with a strong-int type instead of int
template <typename Int, typename T,
          typename CompareOrHashT = util::graph::PreferHashOrCompare<T>,
          typename Eq = std::equal_to<>>
class StrongIndexMap : public util::graph::Index<T, CompareOrHashT, Eq> {
 public:
  using Base = util::graph::Index<T, CompareOrHashT, Eq>;

  explicit StrongIndexMap(Int reserve_num_objects = Int(0),
                          CompareOrHashT compare_or_hash = CompareOrHashT(),
                          Eq eq = Eq())
      : Base(reserve_num_objects.value(), std::move(compare_or_hash),
             std::move(eq)) {}

  template <typename U>
  Int LookupOrAdd(U&& object) {
    return TryEmplace(object).first;
  }

  std::optional<Int> Lookup(const T& object) const {
    std::optional<int> result = Base::Lookup(object);
    if (!result) return std::nullopt;
    return Int(*result);
  }

  const T& operator[](Int index) const {
    return Base::operator[](index.value());
  }

  Int size() const { return Int(Base::size()); }

  template <typename U>
  std::pair<Int, bool> TryEmplace(U&& object) {
    std::pair<int, bool> result = Base::TryEmplace(object);
    return std::make_pair(Int(result.first), result.second);
  }

  util_intops::StrongVector<Int, T> Extract() && {
    util_intops::StrongVector<Int, T> result;
    *result.mutable_get() =
        std::move(*absl::implicit_cast<Base*>(this)).Extract();
    return result;
  }
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_STRONG_INDEX_MAP_H_
