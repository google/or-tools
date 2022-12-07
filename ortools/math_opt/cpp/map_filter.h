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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_MAP_FILTER_H_
#define OR_TOOLS_MATH_OPT_CPP_MAP_FILTER_H_

#include <algorithm>
#include <initializer_list>
#include <optional>

#include "ortools/math_opt/cpp/id_set.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

// A filter that only keeps some specific key-value pairs of a map.
//
// It is used to limit the quantity of data returned in a SolveResult or in a
// CallbackResult when the models are huge and the user is only interested in
// the values of a subset of the keys.
//
// The keys, the type KeyType, should match the definition of "key types" given
// in key_types.h.
//
// A filter is composed of two sub-filters that act as a veto system: a
// key-value pair is kept only when it is kept by both filters. Those filters
// are:
//   - skip_zero_values: when true, only keep pairs if the value is non zero (if
//     the value is boolean, keep only pairs with true value).
//   - filtered_keys: when set, only keep pairs which keys are in the provided
//     list. If this list is empty, no pairs are returned. When unset, keep all
//     pairs.
//
// See the MakeSkipAllFilter(), MakeSkipZerosFilter() and MakeKeepKeysFilter()
// functions below that provide shortcuts for making filters.
//
// This class is a factory of SparseVectorFilterProto.
template <typename KeyType>
struct MapFilter {
  // If true, omits the pairs with zero values (the pairs with false value for
  // bool vectors).
  //
  // Default is false, pairs with zero (or false) value are kept.
  //
  // Prefer using MakeSkipZerosFilter() when appropriate.
  bool skip_zero_values = false;

  // The set of keys of pairs to keep. When unset, all pairs are kept
  // (at least the ones with non-zero values, when skip_zero_values() is true).
  //
  // Default is unset, all pairs are kept.
  //
  // Example:
  //   MapFilter<Variable> filter = ...;
  //
  //   // Unset the filter.
  //   filter.filtered_keys.reset();
  //   // alternatively:
  //   filter.filtered_keys = std::nullopt;
  //
  //   // Set the filter with an empty list of keys (filtering out all pairs).
  //   //
  //   // Note that here `= {}` would NOT work since it would unset the
  //   // filtered_keys optional (i.e. like calling reset()).
  //   filter.filtered_keys.emplace();
  //
  //   // Set the filter to a fix set of variables.
  //   const Variable x = ...;
  //   const Variable y = ...;
  //   filter.filtered_keys = {x, y};
  //
  //   // Set the filter from a collection of variables.
  //   std::vector<Variable> decision_vars = {...};
  //   filter.emplace(decision_vars.begin(), decision_vars.end());
  //
  // Prefer using MakeSkipAllFilter() or MakeKeepKeysFilter() when appropriate.
  std::optional<IdSet<KeyType>> filtered_keys;

  // Returns the model of filtered keys. It returns a non-null value if and only
  // if the filtered_keys is set and non-empty.
  inline const ModelStorage* storage() const;

  // Returns the proto corresponding to this filter.
  SparseVectorFilterProto Proto() const;
};

// Returns a filter that skips all key-value pairs.
//
// This is typically used to disable the dual data in SolveResult when these are
// ignored by the user.
//
// Example:
//   const auto filter = MakeSkipAllFilter<Variable>();
template <typename KeyType>
MapFilter<KeyType> MakeSkipAllFilter() {
  MapFilter<KeyType> ret;
  ret.filtered_keys.emplace();
  return ret;
}

// Returns a filter that skips all key-value pairs with zero values (or false
// values for bool).
//
// Example:
//   const auto filter = MakeSkipZerosFilter<Variable>();
template <typename KeyType>
MapFilter<KeyType> MakeSkipZerosFilter() {
  MapFilter<KeyType> ret;
  ret.skip_zero_values = true;
  return ret;
}

// Returns a filter that keeps the key-value pairs with the given keys.
//
// Example:
//   std::vector<Variable> decision_vars = ...;
//   const auto filter = MakeKeepKeysFilter(decision_vars);
template <typename Collection,
          typename ValueType = typename Collection::value_type>
MapFilter<ValueType> MakeKeepKeysFilter(const Collection& keys) {
  MapFilter<ValueType> ret;
  ret.filtered_keys.emplace(keys.begin(), keys.end());
  return ret;
}

// Returns a filter that keeps the key-value pairs with the given keys.
//
// This overload is necessary since C++ does not automatically deduce the type
// when using the previous one.
//
// Example:
//   const Variable x = ...;
//   const Variable y = ...;
//   const auto filter = MakeKeepKeysFilter({x, y});
template <typename KeyType>
MapFilter<KeyType> MakeKeepKeysFilter(std::initializer_list<KeyType> keys) {
  MapFilter<KeyType> ret;
  ret.filtered_keys = keys;
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename KeyType>
const ModelStorage* MapFilter<KeyType>::storage() const {
  return filtered_keys ? filtered_keys->storage() : nullptr;
}

template <typename KeyType>
SparseVectorFilterProto MapFilter<KeyType>::Proto() const {
  SparseVectorFilterProto ret;
  ret.set_skip_zero_values(skip_zero_values);
  if (filtered_keys) {
    ret.set_filter_by_ids(true);
    const auto filtered_ids = ret.mutable_filtered_ids();
    filtered_ids->Reserve(filtered_keys->size());
    for (const auto id : filtered_keys->raw_set()) {
      filtered_ids->Add(id.value());
    }
    // Iteration on the set is random but we want the proto to be stable.
    std::sort(filtered_ids->begin(), filtered_ids->end());
  }
  return ret;
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_MAP_FILTER_H_
