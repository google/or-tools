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

#include "ortools/math_opt/storage/sparse_matrix.h"

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

void SparseSymmetricMatrix::Delete(const VariableId variable) {
  auto related_vars = related_variables_.find(variable);
  if (related_vars == related_variables_.end()) {
    return;
  }
  for (const VariableId related : related_vars->second) {
    auto mat_value = values_.find(make_key(variable, related));
    if (mat_value != values_.end() && mat_value->second != 0.0) {
      nonzeros_--;
      mat_value->second = 0.0;
    }
  }
  CompactIfNeeded();
}

std::vector<VariableId> SparseSymmetricMatrix::RelatedVariables(
    const VariableId variable) const {
  std::vector<VariableId> result;
  if (!related_variables_.contains(variable)) {
    return result;
  }
  for (const VariableId second : related_variables_.at(variable)) {
    if (get(variable, second) != 0) {
      result.push_back(second);
    }
  }
  return result;
}

std::vector<VariableId> SparseSymmetricMatrix::Variables() const {
  std::vector<VariableId> result;
  // Note: we could make this more efficient in the presence of deletions by
  // storing the actual number of neighbors in the value of related_variables_.
  for (const auto& [var, related] : related_variables_) {
    for (const VariableId other : related) {
      if (get(var, other) != 0.0) {
        result.push_back(var);
        break;
      }
    }
  }
  return result;
}

std::vector<std::pair<VariableId, double>> SparseSymmetricMatrix::Terms(
    const VariableId variable) const {
  std::vector<std::pair<VariableId, double>> result;
  if (!related_variables_.contains(variable)) {
    return result;
  }
  for (const VariableId second : related_variables_.at(variable)) {
    double val = get(variable, second);
    if (val != 0) {
      result.push_back({second, val});
    }
  }
  return result;
}

std::vector<std::tuple<VariableId, VariableId, double>>
SparseSymmetricMatrix::Terms() const {
  std::vector<std::tuple<VariableId, VariableId, double>> result;
  result.reserve(nonzeros_);
  for (const auto& [var_pair, value] : values_) {
    if (value != 0.0) {
      result.push_back({var_pair.first, var_pair.second, value});
    }
  }
  return result;
}

void SparseSymmetricMatrix::CompactIfNeeded() {
  const int64_t zeros = values_.size() - nonzeros_;
  if (values_.empty() ||
      static_cast<double>(zeros) / values_.size() <= internal::kZerosCleanup) {
    return;
  }
  for (auto related_var_it = related_variables_.begin();
       related_var_it != related_variables_.end();) {
    const VariableId v = related_var_it->first;
    std::vector<VariableId>& related = related_var_it->second;
    int64_t write = 0;
    for (int64_t read = 0; read < related.size(); ++read) {
      auto val = values_.find(make_key(v, related[read]));
      if (val != values_.end()) {
        if (val->second != 0) {
          related[write] = related[read];
          ++write;
        } else {
          values_.erase(val);
        }
      }
    }
    if (write == 0) {
      related_variables_.erase(related_var_it++);
    } else {
      related.resize(write);
      ++related_var_it;
    }
  }
}

void SparseSymmetricMatrix::Clear() {
  related_variables_.clear();
  values_.clear();
  nonzeros_ = 0;
}

SparseDoubleMatrixProto SparseSymmetricMatrix::Proto() const {
  SparseDoubleMatrixProto result;

  std::vector<VariableId> vars_in_order;
  for (const auto& [v, _] : related_variables_) {
    vars_in_order.push_back(v);
  }
  absl::c_sort(vars_in_order);

  for (const VariableId v : vars_in_order) {
    // TODO(b/233630053): reuse the allocation once an iterator API is
    // supported.
    std::vector<std::pair<VariableId, double>> related = Terms(v);
    absl::c_sort(related);
    for (const auto& [other, coef] : related) {
      if (v <= other) {
        result.add_row_ids(v.value());
        result.add_column_ids(other.value());
        result.add_coefficients(coef);
      }
    }
  }
  return result;
}

SparseDoubleMatrixProto SparseSymmetricMatrix::Update(
    const absl::flat_hash_set<VariableId>& deleted_variables,
    const absl::Span<const VariableId> new_variables,
    const absl::flat_hash_set<std::pair<VariableId, VariableId>>& dirty) const {
  std::vector<std::tuple<VariableId, VariableId, double>> updates;
  for (const std::pair<VariableId, VariableId>& pair : dirty) {
    // If either variable has been deleted, don't add it.
    if (deleted_variables.contains(pair.first) ||
        deleted_variables.contains(pair.second)) {
      continue;
    }
    updates.push_back({pair.first, pair.second, get(pair.first, pair.second)});
  }

  for (const VariableId v : new_variables) {
    if (related_variables_.contains(v)) {
      // TODO(b/233630053): do not allocate here.
      for (const auto& [other, coef] : Terms(v)) {
        if (v <= other) {
          updates.push_back({v, other, coef});
        } else if (new_variables.empty() || other < new_variables[0]) {
          updates.push_back({other, v, coef});
        }
      }
    }
  }
  return internal::EntriesToMatrixProto(std::move(updates));
}

}  // namespace operations_research::math_opt
