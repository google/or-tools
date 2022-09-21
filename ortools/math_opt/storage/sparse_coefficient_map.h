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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_SPARSE_COEFFICIENT_MAP_H_
#define OR_TOOLS_MATH_OPT_STORAGE_SPARSE_COEFFICIENT_MAP_H_

#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/meta/type_traits.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

// Represents a sparse collection of linear terms: {double_i x VariableId_i}_i.
// All VariableIds not represented in the collection are implicitly treated as
// having zero coefficient.
//
// Internally it is a lightweight wrapper around absl::flat_hash_map that only
// explicitly stores nonzero elements.
class SparseCoefficientMap {
 public:
  SparseCoefficientMap() = default;

  inline explicit SparseCoefficientMap(
      absl::flat_hash_map<VariableId, double> terms);

  // Returns 0.0 by default if no value is set.
  inline double get(const VariableId id) const;

  // Returns true if the stored value changes.
  inline bool set(const VariableId id, const double coeff);

  const absl::flat_hash_map<VariableId, double>& terms() const {
    return terms_;
  }

  inline void clear();

  // Has no effect if id is not set.
  inline void erase(VariableId id);

  SparseDoubleVectorProto Proto() const;

 private:
  absl::flat_hash_map<VariableId, double> terms_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

SparseCoefficientMap::SparseCoefficientMap(
    absl::flat_hash_map<VariableId, double> terms)
    : terms_(std::move(terms)) {
  absl::erase_if(terms_, [](const auto& term) { return term.second == 0.0; });
}

double SparseCoefficientMap::get(const VariableId id) const {
  const auto it = terms_.find(id);
  if (it == terms_.end()) {
    return 0.0;
  }
  return it->second;
}

bool SparseCoefficientMap::set(const VariableId id, const double coeff) {
  if (coeff == 0.0) {
    return terms_.erase(id) > 0;
  }
  const auto [iterator, inserted] = terms_.try_emplace(id, coeff);
  if (inserted) {
    return true;
  }
  if (iterator->second != coeff) {
    iterator->second = coeff;
    return true;
  }
  return false;
}

void SparseCoefficientMap::clear() { terms_.clear(); }

void SparseCoefficientMap::erase(const VariableId id) { terms_.erase(id); }

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_SPARSE_COEFFICIENT_MAP_H_
