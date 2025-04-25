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
#include <cstdlib>
#include <deque>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"

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
                                  absl::btree_set<int>* deducible_vars,
                                  absl::btree_set<int>* input_vars,
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
        deducible_vars->insert(ct.linear().vars(i));
      }
      return;
    }
    case ConstraintProto::kLinMax: {
      // We can deduce only the variables that are only in the target.
      for (int i = 0; i < ct.lin_max().target().vars_size(); ++i) {
        if (ct.lin_max().target().coeffs(i) == 0) continue;
        deducible_vars->insert(ct.lin_max().target().vars(i));
      }
      for (const auto& expr : ct.lin_max().exprs()) {
        for (const int var : expr.vars()) {
          input_vars->insert(var);
        }
      }
      for (const int var : *input_vars) {
        deducible_vars->erase(var);
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
          deducible_vars->insert(var);
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
            input_vars->insert(var);
          }
        }
      }
      for (const auto& [var, count] : variable_appearance_count) {
        if (count != 1) {
          input_vars->insert(var);
        }
      }
      return;
    }
    default:
      break;
  }
}

void CreateLinMaxFromLinearsAndObjective(
    const CpModelProto& model, int var_for_target,
    absl::Span<const int> linear_constraint_indexes,
    bool var_in_objective_is_negative, ConstraintProto* new_constraint) {
  // A variable that is in the objective with a positive coefficient and only
  // appears in inequalities will be at the lowest value that is greater or
  // equal than the variable domain lower bound and that does not violate any
  // bound coming from the inequalities. A similar reasoning applies for a
  // variable with a negative coefficient in the objective.
  LinearArgumentProto& lin_max = *new_constraint->mutable_lin_max();
  lin_max.mutable_target()->add_coeffs(var_in_objective_is_negative ? -1 : 1);
  lin_max.mutable_target()->add_vars(var_for_target);
  const Domain var_domain =
      ReadDomainFromProto(model.variables(var_for_target));
  // Add the bound coming from the variable domain.
  if (var_in_objective_is_negative) {
    lin_max.add_exprs()->set_offset(-var_domain.Max());
  } else {
    lin_max.add_exprs()->set_offset(var_domain.Min());
  }

  for (const int c : linear_constraint_indexes) {
    const LinearConstraintProto& lin = model.constraints(c).linear();
    int64_t coeff = 0;
    for (int i = 0; i < lin.coeffs_size(); ++i) {
      if (lin.vars(i) == var_for_target) {
        coeff = lin.coeffs(i);
        break;
      }
    }
    LinearExpressionProto* expr = lin_max.add_exprs();
    DCHECK_EQ(lin.domain_size(), 2);
    const int64_t expr_min = lin.domain(0);
    const int64_t expr_max = lin.domain(1);
    if ((coeff < 0) == var_in_objective_is_negative) {
      expr->set_offset(expr_min);
    } else {
      expr->set_offset(-expr_max);
    }
    const int64_t multiplier =
        ((coeff < 0) == var_in_objective_is_negative) ? -1 : 1;
    for (int i = 0; i < lin.coeffs_size(); ++i) {
      if (lin.vars(i) == var_for_target) {
        continue;
      }
      expr->add_vars(lin.vars(i));
      expr->add_coeffs(multiplier * lin.coeffs(i));
    }
  }
}

bool IsObjectiveConstraining(const CpModelProto& model) {
  if (!model.has_objective()) {
    return false;
  }
  if (model.objective().domain().empty()) {
    return false;
  }
  if (model.objective().domain_size() > 2) {
    return true;
  }
  int64_t lb = 0;
  int64_t ub = 0;
  for (int i = 0; i < model.objective().vars_size(); ++i) {
    const int var = model.objective().vars(i);
    lb += model.objective().coeffs(i) * model.variables(var).domain(0);
    ub += model.objective().coeffs(i) *
          model.variables(var).domain(model.variables(var).domain_size() - 1);
  }
  if (model.objective().domain(0) > lb || model.objective().domain(1) < ub) {
    return true;
  }
  return false;
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
    // These sets will hold only the "undecided" variables. When a variable
    // is marked as primary or secondary, it will be removed.
    absl::btree_set<int> deducible_vars;
    absl::btree_set<int> input_vars;

    int preferred_to_deduce;

    // If a variable participates in the model only via linear inequalities and
    // the objective, and *all* the other variables in those inequalities are
    // already tagged as primary or secondary, then this variable can be flagged
    // as a secondary variable and can be computed as a lin_max of the others.
    bool is_linear_inequality;
  };

  std::vector<ConstraintData> constraint_data(model.constraints_size());
  std::vector<std::vector<int>> constraints_per_var(model.variables_size());

  Bitset64<int> var_appears_only_in_objective_and_linear(
      model.variables_size());
  Bitset64<int> var_in_objective_is_negative(model.variables_size());
  if (!IsObjectiveConstraining(model)) {
    // TODO(user): if we have a constraining objective we can suppose a
    // non-constraining one + a linear constraint. But this will allow us to
    // find at most one new secondary variable, since all the variables in the
    // objective will be connected via this linear constraint.
    for (int i = 0; i < model.objective().vars_size(); ++i) {
      if (model.objective().coeffs(i) == 0) continue;
      var_appears_only_in_objective_and_linear.Set(model.objective().vars(i));
      if (model.objective().coeffs(i) < 0) {
        var_in_objective_is_negative.Set(model.objective().vars(i));
      }
    }
  }
  for (int c = 0; c < model.constraints_size(); ++c) {
    ConstraintData& data = constraint_data[c];
    const ConstraintProto& ct = model.constraints(c);
    data.is_linear_inequality = false;
    GetRelationshipForConstraint(ct, &data.deducible_vars, &data.input_vars,
                                 &data.preferred_to_deduce);
    // Now prepare the data for the handling the case of variables that only
    // appear in the objective and linear inequalities. There are two cases:
    // - either the constraint that is one such linear inequality and we flag it
    //   as such;
    // - if not, we flag all the variables used in this constraint as appearing
    //   in constraints that are not linear inequalities.
    if (ct.constraint_case() == ConstraintProto::kLinear &&
        data.deducible_vars.empty() &&  // Not allowing to fix a secondary var
                                        // directly (i.e., an equality)
        ct.enforcement_literal().empty() && ct.linear().domain_size() == 2) {
      // This is the kind of inequality we might use for a lin_max
      data.is_linear_inequality = true;
      for (int i = 0; i < ct.linear().vars_size(); ++i) {
        const int var = ct.linear().vars(i);
        if (!var_appears_only_in_objective_and_linear.IsSet(var)) {
          data.input_vars.insert(var);
          continue;
        }
        if (ct.linear().coeffs(i) == 0) continue;
        if (std::abs(ct.linear().coeffs(i)) == 1) {
          data.deducible_vars.insert(var);
        } else {
          data.input_vars.insert(var);
          // TODO(user): we can support non-unit coefficients to deduce a
          // lin_max from the objective. It will become more difficult, since
          // first we will need to compute the lcm of all coefficients (and
          // avoid overflow). Then, we will need to build a constraint that will
          // be div(target, lin_max(exprs) + lcm - 1, lcm).
          var_appears_only_in_objective_and_linear.Set(var, false);
        }
      }
    } else {
      // Other kind of constraints, tagged those variables as "used elsewhere".
      for (const int var : UsedVariables(ct)) {
        var_appears_only_in_objective_and_linear.Set(var, false);
      }
    }
  }

  // In the loop above, we lazily set some variables as deducible from linear
  // inequalities if they only appeared in the objective and linear inequalities
  // when we saw the constraint, but we did not checked how they were used in
  // following constraints. Now remove them if it was used in other constraints.
  std::vector<int> deducible_vars_to_remove;
  for (int c = 0; c < model.constraints_size(); ++c) {
    deducible_vars_to_remove.clear();
    ConstraintData& data = constraint_data[c];
    if (!data.is_linear_inequality) {
      continue;
    }
    for (const int var : data.deducible_vars) {
      if (!var_appears_only_in_objective_and_linear.IsSet(var)) {
        deducible_vars_to_remove.push_back(var);
        data.input_vars.insert(var);
      }
    }
    for (const int var : deducible_vars_to_remove) {
      data.deducible_vars.erase(var);
    }
    if (data.deducible_vars.empty()) {
      data.is_linear_inequality = false;
    }
  }

  for (int c = 0; c < constraint_data.size(); ++c) {
    ConstraintData& data = constraint_data[c];
    if (data.deducible_vars.empty()) {
      data.input_vars.clear();
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

  // Variables that cannot be potentially deduced using any constraints are
  // primary. Flag them as such and strip them from our data structures.
  for (int v = 0; v < model.variables_size(); ++v) {
    if (num_times_variable_appears_as_deducible[v] != 0) {
      continue;
    }
    num_times_variable_appears_as_input[v] = 0;
    var_is_primary.Set(v);
    for (const int c : constraints_per_var[v]) {
      ConstraintData& data = constraint_data[c];
      data.deducible_vars.erase(v);
      data.input_vars.erase(v);
    }
    constraints_per_var[v].clear();
  }

  // Now, for variables that only appear in the objective and linear
  // inequalities, we count how far we are from being able to deduce their
  // value. In practice, we count the number of linear inequalities in which
  // this variable appears alongside another variable that we have not decided
  // to be primary or secondary yet. When this count reach 0, it means we can
  // create a lin_max constraint to deduce its value.
  std::vector<int> count_of_unresolved_linear_inequalities_per_var(
      model.variables_size());
  for (int c = 0; c < model.constraints_size(); ++c) {
    ConstraintData& data = constraint_data[c];
    if (!data.is_linear_inequality) {
      continue;
    }
    if (data.input_vars.size() + data.deducible_vars.size() > 1) {
      for (const int v : data.input_vars) {
        count_of_unresolved_linear_inequalities_per_var[v]++;
      }
      for (const int v : data.deducible_vars) {
        count_of_unresolved_linear_inequalities_per_var[v]++;
      }
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
  std::deque<int> vars_queue;
  for (int v = 0; v < model.variables_size(); ++v) {
    if (var_is_primary.IsSet(v) || var_is_secondary.IsSet(v)) {
      continue;
    }
    vars_queue.push_back(v);
  }
  absl::c_stable_sort(vars_queue, [&](int a, int b) {
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
  while (!vars_queue.empty()) {
    const int v = vars_queue.front();
    vars_queue.pop_front();
    if (var_is_secondary.IsSet(v) || var_is_primary.IsSet(v)) {
      continue;
    }
    // First, we will decide whether we should mark `v` as secondary or primary
    // using the equality constraints.
    for (const int c : constraints_per_var[v]) {
      ConstraintData& data = constraint_data[c];
      if (data.is_linear_inequality) {
        continue;
      }
      if (data.deducible_vars.size() + data.input_vars.size() != 1) {
        // One of it inputs are neither primary nor secondary yet, we cannot
        // deduce the value of `v` using this constraint.
        continue;
      }

      // There is a single undecided variable for this constraint. Thus `v` is
      // either an input or a deducible variable. Let's check.
      if (data.deducible_vars.empty()) {
        // This is a strange case, like `z = lin_max(x, y)`, where `z` and `y`
        // are primary (we cannot deduce `x`). Let's just flag this constraint
        // as useless from now on.
        data.input_vars.clear();
        continue;
      }
      var_is_secondary.Set(v);
      result.secondary_variables.push_back(v);
      result.dependency_resolution_constraint.push_back(model.constraints(c));
      result.redundant_constraint_indices.push_back(c);
      break;
    }

    // We couldn't deduce the value of `v` from any constraint, check if it only
    // appears in linear inequalities.
    if (!var_is_secondary.IsSet(v)) {
      if (var_appears_only_in_objective_and_linear.IsSet(v) &&
          count_of_unresolved_linear_inequalities_per_var[v] == 0) {
        var_is_secondary.Set(v);
        result.secondary_variables.push_back(v);
        result.dependency_resolution_constraint.emplace_back();
        CreateLinMaxFromLinearsAndObjective(
            model, v, constraints_per_var[v],
            var_in_objective_is_negative.IsSet(v),
            &result.dependency_resolution_constraint.back());
        // TODO(user): set result.redundant_constraint_indices?
      } else {
        // We can't deduce the value of `v` from what we have so far, flag it as
        // primary.
        var_is_primary.Set(v);
      }
    }

    // At this point we have decided to make `v` primary or secondary, so we
    // can remove it from the model and maybe lazily deduce some variables.
    auto update_constraints_after_var_is_decided = [&](int v) {
      for (const int c : constraints_per_var[v]) {
        ConstraintData& data = constraint_data[c];
        data.deducible_vars.erase(v);
        data.input_vars.erase(v);
        if (data.input_vars.size() + data.deducible_vars.size() != 1) {
          // Two of the variables for this constraint are neither primary nor
          // secondary yet, we cannot deduce the value of anything using this
          // constraint.
          continue;
        }
        if (data.is_linear_inequality && data.deducible_vars.size() == 1) {
          const int deducible_var = *data.deducible_vars.begin();
          count_of_unresolved_linear_inequalities_per_var[deducible_var]--;
          if (count_of_unresolved_linear_inequalities_per_var[deducible_var] ==
              0) {
            // Now we can deduce a new variable from linears, process it ASAP!
            vars_queue.push_front(deducible_var);
          }
        } else {
          if (data.deducible_vars.empty()) {
            // Same as the test for the case `z = lin_max(x, y)` above.
            data.input_vars.clear();
          } else {
            // This constraint fix a secondary variable, enqueue it!
            constraints_to_check.push_back(c);
          }
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
      DCHECK_LE(data.deducible_vars.size(), 1);
      if (data.deducible_vars.size() != 1) {
        continue;
      }
      const int single_deducible_var = *data.deducible_vars.begin();
      var_is_secondary.Set(single_deducible_var);
      update_constraints_after_var_is_decided(single_deducible_var);
      result.secondary_variables.push_back(single_deducible_var);
      result.dependency_resolution_constraint.push_back(model.constraints(c));
      result.redundant_constraint_indices.push_back(c);
    }
  }

  for (int i = 0; i < result.secondary_variables.size(); ++i) {
    const int var = result.secondary_variables[i];
    ConstraintData data;
    const ConstraintProto& ct = result.dependency_resolution_constraint[i];
    GetRelationshipForConstraint(ct, &data.deducible_vars, &data.input_vars,
                                 &data.preferred_to_deduce);
    for (const int v : data.input_vars) {
      if (var_is_secondary.IsSet(v)) {
        result.variable_dependencies.push_back({var, v});
      }
    }
    for (const int v : data.deducible_vars) {
      if (var_is_secondary.IsSet(v) && v != var) {
        result.variable_dependencies.push_back({var, v});
      }
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
    const ConstraintProto& ct =
        relationships.dependency_resolution_constraint[i];
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
