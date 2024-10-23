// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/cp_model_table.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"

namespace operations_research {
namespace sat {

void CanonicalizeTable(PresolveContext* context, ConstraintProto* ct) {
  if (context->ModelIsUnsat()) return;

  const int num_exprs = ct->table().exprs_size();
  const int num_tuples = ct->table().values_size() / num_exprs;

  // Detect expressions sharing the same variable as a previous expression.
  absl::flat_hash_map<int, int> var_to_position;

  // The mapping between the position in the original list of expressions, and
  // the position in the reduced list of expressions.
  std::vector<std::optional<int>> position_mapping(num_exprs, std::nullopt);
  int num_shared_vars = 0;
  for (int i = 0; i < num_exprs; ++i) {
    const LinearExpressionProto& expr = ct->table().exprs(i);
    if (context->IsFixed(expr)) continue;

    const int var = expr.vars(0);
    const auto [it, inserted] =
        var_to_position.insert({var, var_to_position.size()});
    if (!inserted) {
      ++num_shared_vars;
      position_mapping[i] = it->second;
    }
  }

  const int num_kept_exprs = num_exprs - num_shared_vars;

  std::vector<std::vector<int64_t>> new_tuples;
  new_tuples.reserve(num_tuples);

  std::vector<int64_t> new_scaled_values;
  new_scaled_values.reserve(num_kept_exprs);

  for (int t = 0; t < num_tuples; ++t) {
    bool tuple_is_valid = true;
    new_scaled_values.clear();

    for (int e = 0; e < num_exprs; ++e) {
      const int64_t value = ct->table().values(t * num_exprs + e);
      const LinearExpressionProto& expr = ct->table().exprs(e);
      if (context->IsFixed(expr)) {
        if (value != context->FixedValue(expr)) {
          tuple_is_valid = false;
          break;
        }
        new_scaled_values.push_back(value);
      } else if (position_mapping[e].has_value()) {
        const int var_first_position = position_mapping[e].value();
        const int64_t var_value = new_scaled_values[var_first_position];
        const int64_t forced_value = AffineExpressionValueAt(expr, var_value);
        if (value != forced_value) {
          tuple_is_valid = false;
          break;
        }
      } else {
        if (!context->DomainContains(expr, value)) {
          tuple_is_valid = false;
          break;
        }
        new_scaled_values.push_back(GetInnerVarValue(expr, value));
      }
    }

    if (tuple_is_valid) {
      DCHECK_EQ(new_scaled_values.size(), num_kept_exprs);
      new_tuples.push_back(new_scaled_values);
    }
  }

  // Remove all scaling on expressions as we have stored the inner values.
  for (int e = 0; e < num_exprs; ++e) {
    if (position_mapping[e].has_value()) continue;
    if (context->IsFixed(ct->table().exprs(e))) continue;
    DCHECK_EQ(ct->table().exprs(e).coeffs_size(), 1);
    ct->mutable_table()->mutable_exprs(e)->set_offset(0);
    ct->mutable_table()->mutable_exprs(e)->set_coeffs(0, 1);
  }

  if (num_kept_exprs < num_exprs) {
    int index = 0;
    for (int e = 0; e < num_exprs; ++e) {
      if (position_mapping[e].has_value()) continue;
      ct->mutable_table()->mutable_exprs()->SwapElements(index++, e);
    }
    CHECK_EQ(index, num_kept_exprs);
    ct->mutable_table()->mutable_exprs()->DeleteSubrange(index,
                                                         num_exprs - index);
    context->UpdateRuleStats("table: remove expressions");
  }

  gtl::STLSortAndRemoveDuplicates(&new_tuples);
  if (new_tuples.size() < num_tuples) {
    context->UpdateRuleStats("table: remove tuples");
  }

  // Write sorted tuples.
  ct->mutable_table()->clear_values();
  for (const std::vector<int64_t>& tuple : new_tuples) {
    ct->mutable_table()->mutable_values()->Add(tuple.begin(), tuple.end());
  }
}

void RemoveFixedColumnsFromTable(PresolveContext* context,
                                 ConstraintProto* ct) {
  if (context->ModelIsUnsat()) return;
  const int num_exprs = ct->table().exprs_size();
  const int num_tuples = ct->table().values_size() / num_exprs;
  std::vector<bool> is_fixed(num_exprs, false);
  int num_fixed_exprs = 0;
  for (int e = 0; e < num_exprs; ++e) {
    is_fixed[e] = context->IsFixed(ct->table().exprs(e));
    num_fixed_exprs += is_fixed[e];
  }
  if (num_fixed_exprs == 0) return;

  int num_kept_exprs = num_exprs - num_fixed_exprs;

  int index = 0;
  for (int e = 0; e < num_exprs; ++e) {
    if (is_fixed[e]) continue;
    ct->mutable_table()->mutable_exprs()->SwapElements(index++, e);
  }
  CHECK_EQ(index, num_kept_exprs);
  ct->mutable_table()->mutable_exprs()->DeleteSubrange(index,
                                                       num_exprs - index);
  index = 0;
  for (int t = 0; t < num_tuples; ++t) {
    for (int e = 0; e < num_exprs; ++e) {
      if (is_fixed[e]) continue;
      ct->mutable_table()->set_values(index++,
                                      ct->table().values(t * num_exprs + e));
    }
  }
  CHECK_EQ(index, num_tuples * num_kept_exprs);
  ct->mutable_table()->mutable_values()->Truncate(index);

  context->UpdateRuleStats("table: remove fixed columns");
}

void CompressTuples(absl::Span<const int64_t> domain_sizes,
                    std::vector<std::vector<int64_t>>* tuples) {
  if (tuples->empty()) return;

  // Remove duplicates if any.
  gtl::STLSortAndRemoveDuplicates(tuples);

  const int num_vars = (*tuples)[0].size();

  std::vector<int> to_remove;
  std::vector<int64_t> tuple_minus_var_i(num_vars - 1);
  for (int i = 0; i < num_vars; ++i) {
    const int domain_size = domain_sizes[i];
    if (domain_size == 1) continue;
    absl::flat_hash_map<std::vector<int64_t>, std::vector<int>>
        masked_tuples_to_indices;
    for (int t = 0; t < tuples->size(); ++t) {
      int out = 0;
      for (int j = 0; j < num_vars; ++j) {
        if (i == j) continue;
        tuple_minus_var_i[out++] = (*tuples)[t][j];
      }
      masked_tuples_to_indices[tuple_minus_var_i].push_back(t);
    }
    to_remove.clear();
    for (const auto& it : masked_tuples_to_indices) {
      if (it.second.size() != domain_size) continue;
      (*tuples)[it.second.front()][i] = kTableAnyValue;
      to_remove.insert(to_remove.end(), it.second.begin() + 1, it.second.end());
    }
    std::sort(to_remove.begin(), to_remove.end(), std::greater<int>());
    for (const int t : to_remove) {
      (*tuples)[t] = tuples->back();
      tuples->pop_back();
    }
  }
}

namespace {

// We will call FullyCompressTuplesRecursive() for a set of prefixes of the
// original tuples, each having the same suffix (in reversed_suffix).
//
// For such set, we will compress it on the last variable of the prefixes. We
// will then for each unique compressed set of value of that variable, call
// a new FullyCompressTuplesRecursive() on the corresponding subset.
void FullyCompressTuplesRecursive(
    absl::Span<const int64_t> domain_sizes,
    absl::Span<std::vector<int64_t>> tuples,
    std::vector<absl::InlinedVector<int64_t, 2>>* reversed_suffix,
    std::vector<std::vector<absl::InlinedVector<int64_t, 2>>>* output) {
  struct TempData {
    absl::InlinedVector<int64_t, 2> values;
    int index;

    bool operator<(const TempData& other) const {
      return values < other.values;
    }
  };
  std::vector<TempData> temp_data;

  CHECK(!tuples.empty());
  CHECK(!tuples[0].empty());
  const int64_t domain_size = domain_sizes[tuples[0].size() - 1];

  // Sort tuples and regroup common prefix in temp_data.
  std::sort(tuples.begin(), tuples.end());
  for (int i = 0; i < tuples.size();) {
    const int start = i;
    temp_data.push_back({{tuples[start].back()}, start});
    tuples[start].pop_back();
    for (++i; i < tuples.size(); ++i) {
      const int64_t v = tuples[i].back();
      tuples[i].pop_back();
      if (tuples[i] == tuples[start]) {
        temp_data.back().values.push_back(v);
      } else {
        tuples[i].push_back(v);
        break;
      }
    }

    // If one of the value is the special value kTableAnyValue, we convert
    // it to the "empty means any" format.
    for (const int64_t v : temp_data.back().values) {
      if (v == kTableAnyValue) {
        temp_data.back().values.clear();
        break;
      }
    }
    gtl::STLSortAndRemoveDuplicates(&temp_data.back().values);

    // If values cover the whole domain, we clear the vector. This allows to
    // use less space and avoid creating unneeded clauses.
    if (temp_data.back().values.size() == domain_size) {
      temp_data.back().values.clear();
    }
  }

  if (temp_data.size() == 1) {
    output->push_back({});
    for (const int64_t v : tuples[temp_data[0].index]) {
      if (v == kTableAnyValue) {
        output->back().push_back({});
      } else {
        output->back().push_back({v});
      }
    }
    output->back().push_back(temp_data[0].values);
    for (int i = reversed_suffix->size(); --i >= 0;) {
      output->back().push_back((*reversed_suffix)[i]);
    }
    return;
  }

  // Sort temp_data and make recursive call for all tuples that share the
  // same suffix.
  std::sort(temp_data.begin(), temp_data.end());
  std::vector<std::vector<int64_t>> temp_tuples;
  for (int i = 0; i < temp_data.size();) {
    reversed_suffix->push_back(temp_data[i].values);
    const int start = i;
    temp_tuples.clear();
    for (; i < temp_data.size(); i++) {
      if (temp_data[start].values != temp_data[i].values) break;
      temp_tuples.push_back(tuples[temp_data[i].index]);
    }
    FullyCompressTuplesRecursive(domain_sizes, absl::MakeSpan(temp_tuples),
                                 reversed_suffix, output);
    reversed_suffix->pop_back();
  }
}

}  // namespace

// TODO(user): We can probably reuse the tuples memory always and never create
// new one. We should also be able to code an iterative version of this. Note
// however that the recursion level is bounded by the number of columns which
// should be small.
std::vector<std::vector<absl::InlinedVector<int64_t, 2>>> FullyCompressTuples(
    absl::Span<const int64_t> domain_sizes,
    std::vector<std::vector<int64_t>>* tuples) {
  std::vector<absl::InlinedVector<int64_t, 2>> reversed_suffix;
  std::vector<std::vector<absl::InlinedVector<int64_t, 2>>> output;
  FullyCompressTuplesRecursive(domain_sizes, absl::MakeSpan(*tuples),
                               &reversed_suffix, &output);
  return output;
}

}  // namespace sat
}  // namespace operations_research
