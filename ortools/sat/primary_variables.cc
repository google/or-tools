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

#include "ortools/sat/primary_variables.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

// Declares that if all variables in input_vars and all but one of the
// variables in deducible_vars are known, then we can deduce the missing one.
// No variable should appear twice in the inputs.
//
// For example, if we have:
//  u + z + x = lin_max(x + 3, y)
//
// This function will assign:
//  deducible_vars = {u, z}
//  input_vars = {x, y}
//
// This declares that, for example, if x, y and u are all known, then z is
// known. On the other hand, if everything is known except x, then we can't
// deduce anything, since for some values of u, z and y, the constraint can be
// simplified to x + 3 = x + 3.
void GetRelationshipForConstraint(const ConstraintProto& ct,
                                  std::vector<int>* deducible_vars,
                                  std::vector<int>* input_vars,
                                  int* preferred_to_deduce) {
  deducible_vars->clear();
  input_vars->clear();
  *preferred_to_deduce = -1;
  switch (ct.constraint_case()) {
    case ConstraintProto::kLinear: {
      if (ReadDomainFromProto(ct.linear()).Size() != 1) {
        return;
      }
      if (!ct.enforcement_literal().empty()) {
        return;
      }
      for (int i = 0; i < ct.linear().vars_size(); ++i) {
        if (ct.linear().coeffs(i) == 0) continue;
        deducible_vars->push_back(ct.linear().vars(i));
      }
      // Let's be defensive if this code get called with a linear that is not
      // normalized.
      gtl::STLSortAndRemoveDuplicates(deducible_vars);
      return;
    }
    case ConstraintProto::kLinMax: {
      // We can deduce only the variables that are only in the target.
      absl::btree_set<int> deducible_vars_set;
      for (int i = 0; i < ct.lin_max().target().vars_size(); ++i) {
        if (ct.lin_max().target().coeffs(i) == 0) continue;
        deducible_vars_set.insert(ct.lin_max().target().vars(i));
      }
      for (const auto& expr : ct.lin_max().exprs()) {
        for (const int var : expr.vars()) {
          input_vars->push_back(var);
        }
      }
      gtl::STLSortAndRemoveDuplicates(input_vars);
      for (const int var : *input_vars) {
        deducible_vars_set.erase(var);
      }
      for (const int var : deducible_vars_set) {
        deducible_vars->push_back(var);
      }
      return;
    }
    case ConstraintProto::kIntProd: {
      absl::btree_map<int, int> variable_appearance_count;
      for (const int var : ct.int_prod().target().vars()) {
        variable_appearance_count[var]++;
      }
      for (const auto& expr : ct.int_prod().exprs()) {
        for (const int var : expr.vars()) {
          variable_appearance_count[var]++;
        }
      }
      for (int i = 0; i < ct.int_prod().target().vars_size(); ++i) {
        if (ct.int_prod().target().coeffs(i) == 0) continue;
        const int var = ct.int_prod().target().vars(i);
        if (variable_appearance_count[var] == 1) {
          deducible_vars->push_back(var);
        }
      }
      for (const auto& expr : ct.int_prod().exprs()) {
        for (int i = 0; i < expr.vars_size(); ++i) {
          const int var = expr.vars(i);
          if (variable_appearance_count[var] == 1) {
            // We might be tempted to make the variable deducible if the
            // coefficient is 1, but we risk trying to deduce x from 0 = 0 * x.
            // TODO(user): do it when the target domain doesn't include 0,
            // but use preferred_to_deduce to prefer the target.
            input_vars->push_back(var);
          }
        }
      }
      for (const auto& [var, count] : variable_appearance_count) {
        if (count != 1) {
          input_vars->push_back(var);
        }
      }
      return;
    }
    default:
      break;
  }
}

VariableRelationships ComputeVariableRelationships(const CpModelProto& model) {
  VariableRelationships result;
  Bitset64<int> var_is_secondary(model.variables_size());
  Bitset64<int> var_is_primary(model.variables_size());
  std::vector<int> num_times_variable_appears_as_input(model.variables_size());
  std::vector<int> num_times_variable_appears_as_deducible(
      model.variables_size());
  std::vector<int> num_times_variable_appears_as_preferred_to_deduce(
      model.variables_size());

  struct ConstraintData {
    std::vector<int> deducible_vars;
    std::vector<int> input_vars;
    int preferred_to_deduce;
    // A variable is "undecided" until we decide to mark it as primary or
    // secondary.
    int num_undecided_vars;
  };

  std::vector<ConstraintData> constraint_data(model.constraints_size());
  std::vector<std::vector<int>> constraints_per_var(model.variables_size());

  for (int c = 0; c < model.constraints_size(); ++c) {
    ConstraintData& data = constraint_data[c];
    const ConstraintProto& ct = model.constraints(c);
    GetRelationshipForConstraint(ct, &data.deducible_vars, &data.input_vars,
                                 &data.preferred_to_deduce);
    data.num_undecided_vars =
        data.deducible_vars.size() + data.input_vars.size();
    if (data.deducible_vars.empty()) {
      continue;
    }
    if (data.preferred_to_deduce != -1) {
      num_times_variable_appears_as_preferred_to_deduce
          [data.preferred_to_deduce]++;
    }
    for (const int v : data.deducible_vars) {
      constraints_per_var[v].push_back(c);
      num_times_variable_appears_as_deducible[v]++;
    }
    for (const int v : data.input_vars) {
      constraints_per_var[v].push_back(c);
      num_times_variable_appears_as_input[v]++;
    }
  }

  // Now do a greedy heuristic: we will take each variable in a particular
  // order, and if the variable can be deduced from other variables that we have
  // already decided to declare as primary or secondary, we will mark it as
  // secondary. Otherwise we will mark it as primary. In any case, after we do
  // that, we will look at all the constraints that uses this variable and see
  // if it allows to deduce some variable. If yes, mark the variable that can be
  // deduced as secondary, look at the constraints that uses it, and so on until
  // we reach a fixed point. The heuristic for the order is to try to process
  // first the variables that are more "useful" to be marked as primary, so it
  // allows us to mark more variables as secondary in the following.
  std::vector<int> variables_ordered_by_preference(model.variables_size());
  for (int v = 0; v < model.variables_size(); ++v) {
    variables_ordered_by_preference[v] = v;
  }
  absl::c_stable_sort(variables_ordered_by_preference, [&](int a, int b) {
    const int a_diff_usage = num_times_variable_appears_as_deducible[a] -
                             num_times_variable_appears_as_input[a];
    const int b_diff_usage = num_times_variable_appears_as_deducible[b] -
                             num_times_variable_appears_as_input[b];
    return std::make_tuple(
               a_diff_usage,
               -num_times_variable_appears_as_preferred_to_deduce[a],
               -num_times_variable_appears_as_deducible[a]) <
           std::make_tuple(
               b_diff_usage,
               -num_times_variable_appears_as_preferred_to_deduce[b],
               -num_times_variable_appears_as_deducible[b]);
  });
  std::vector<int> constraints_to_check;
  for (int v : variables_ordered_by_preference) {
    if (var_is_secondary.IsSet(v) || var_is_primary.IsSet(v)) {
      continue;
    }
    // first, we will decide whether we should mark `v` as secondary or primary.
    for (const int c : constraints_per_var[v]) {
      ConstraintData& data = constraint_data[c];
      if (data.num_undecided_vars != 1) {
        // One of it inputs are neither primary nor secondary yet, we cannot
        // deduce the value of `v` using this constraint.
        continue;
      }

      // There is a single undecided variable for this constraint. Thus `v` is
      // either an input or a deducible variable. Let's check.
      if (!absl::c_linear_search(data.input_vars, v)) {
        // This is a strange case, like `z = lin_max(x, y)`, where `z` and `y`
        // are primary (we cannot deduce `x`). Let's just flag this constraint
        // as useless from now on.
        data.num_undecided_vars = 0;
        continue;
      }
      var_is_secondary.Set(v);
      result.secondary_variables.push_back(v);
      result.dependency_resolution_constraint_index.push_back(c);
      break;
    }

    // We couldn't deduce the value of `v` from any constraint, so we mark it as
    // primary.
    if (!var_is_secondary.IsSet(v)) {
      var_is_primary.Set(v);
    }

    auto update_constraints_after_var_is_decided = [&](int v) {
      for (const int c : constraints_per_var[v]) {
        ConstraintData& data = constraint_data[c];
        data.num_undecided_vars--;
        if (data.num_undecided_vars != 1) {
          // Two of the variables for this constraint are neither primary nor
          // secondary yet, we cannot deduce the value of anything using this
          // constraint.
          continue;
        }
        if (absl::c_all_of(data.deducible_vars, [&var_is_secondary,
                                                 &var_is_primary](int v) {
              return var_is_secondary.IsSet(v) || var_is_primary.IsSet(v);
            })) {
          // Same as the test for the case `z = lin_max(x, y)` above.
          data.num_undecided_vars = 0;
        } else {
          constraints_to_check.push_back(c);
        }
      }
    };

    // In any case, this variable is now decided, so we update the number of
    // undecided variables in all the constraints that use it.
    DCHECK(constraints_to_check.empty());
    update_constraints_after_var_is_decided(v);

    // Now, deduce everything that become trivially deducible until we reach a
    // fixed point.
    while (!constraints_to_check.empty()) {
      const int c = constraints_to_check.back();
      constraints_to_check.pop_back();
      ConstraintData& data = constraint_data[c];
      DCHECK_LE(data.num_undecided_vars, 1);
      if (data.num_undecided_vars != 1) {
        continue;
      }
      int single_deducible_var = -1;
      for (const int deducible_var : data.deducible_vars) {
        if (var_is_secondary.IsSet(deducible_var) ||
            var_is_primary.IsSet(deducible_var)) {
          continue;
        }
        DCHECK_EQ(single_deducible_var, -1);
        single_deducible_var = deducible_var;
      }
      if (single_deducible_var == -1) {
        // Same as the test for the case `z = lin_max(x, y)` above.
        data.num_undecided_vars = 0;
        continue;
      }
      var_is_secondary.Set(single_deducible_var);
      update_constraints_after_var_is_decided(single_deducible_var);
      result.secondary_variables.push_back(single_deducible_var);
      result.dependency_resolution_constraint_index.push_back(c);
    }
  }
  return result;
}

bool ComputeAllVariablesFromPrimaryVariables(
    const CpModelProto& model, const VariableRelationships& relationships,
    std::vector<int64_t>* solution) {
  Bitset64<int> dependent_variables_set(model.variables_size());
  for (const int var : relationships.secondary_variables) {
    dependent_variables_set.Set(var);
  }
  for (int i = 0; i < relationships.secondary_variables.size(); ++i) {
    const int var = relationships.secondary_variables[i];
    const int constraint_index =
        relationships.dependency_resolution_constraint_index[i];
    const ConstraintProto& ct = model.constraints(constraint_index);
    switch (ct.constraint_case()) {
      case ConstraintProto::kLinear: {
        const LinearConstraintProto& linear = ct.linear();
        DCHECK_EQ(ReadDomainFromProto(linear).Size(), 1);
        int64_t sum_of_free_variables = ReadDomainFromProto(linear).Min();
        int64_t coeff_of_var = 1;
        for (int j = 0; j < linear.vars_size(); ++j) {
          if (linear.vars(j) == var) {
            coeff_of_var = linear.coeffs(j);
            continue;
          }
          DCHECK(!dependent_variables_set.IsSet(linear.vars(j)));
          sum_of_free_variables -=
              linear.coeffs(j) * (*solution)[linear.vars(j)];
        }
        (*solution)[var] = sum_of_free_variables / coeff_of_var;
      } break;
      case ConstraintProto::kLinMax: {
        int64_t max = std::numeric_limits<int64_t>::min();
        for (const auto& expr : ct.lin_max().exprs()) {
          int64_t expr_value = expr.offset();
          for (int j = 0; j < expr.vars_size(); ++j) {
            DCHECK(!dependent_variables_set.IsSet(expr.vars(j)));
            expr_value += expr.coeffs(j) * (*solution)[expr.vars(j)];
          }
          max = std::max(max, expr_value);
        }
        const LinearExpressionProto& target = ct.lin_max().target();
        int64_t coeff_of_var = 1;
        for (int j = 0; j < target.vars_size(); ++j) {
          if (target.vars(j) == var) {
            coeff_of_var = target.coeffs(j);
            continue;
          }
          DCHECK(!dependent_variables_set.IsSet(target.vars(j)));
          max -= target.coeffs(j) * (*solution)[target.vars(j)];
        }
        max -= target.offset();
        (*solution)[var] = max / coeff_of_var;
        break;
      }
      case ConstraintProto::kIntProd: {
        int64_t product = 1;
        for (const auto& expr : ct.int_prod().exprs()) {
          int64_t expr_value = expr.offset();
          for (int j = 0; j < expr.vars_size(); ++j) {
            DCHECK(!dependent_variables_set.IsSet(expr.vars(j)));
            expr_value += (*solution)[expr.vars(j)] * expr.coeffs(j);
          }
          product *= expr_value;
        }
        int64_t coeff_of_var = 1;
        const LinearExpressionProto& target = ct.int_prod().target();
        for (int j = 0; j < target.vars_size(); ++j) {
          if (target.vars(j) == var) {
            coeff_of_var = target.coeffs(j);
            continue;
          }
          DCHECK(!dependent_variables_set.IsSet(target.vars(j)));
          product -= target.coeffs(j) * (*solution)[target.vars(j)];
        }
        product -= target.offset();
        (*solution)[var] = product / coeff_of_var;
      } break;
      default:
        break;
    }
    dependent_variables_set.Set(var, false);
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
