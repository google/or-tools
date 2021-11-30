// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/cp_model_postsolve.h"

#include <cstdint>
#include <limits>

#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

// This postsolve is "special". If the clause is not satisfied, we fix the
// first literal in the clause to true (even if it was fixed to false). This
// allows to handle more complex presolve operations used by the SAT presolver.
//
// Also, any "free" Boolean should be fixed to some value for the subsequent
// postsolve steps.
void PostsolveClause(const ConstraintProto& ct, std::vector<Domain>* domains) {
  const int size = ct.bool_or().literals_size();
  CHECK_NE(size, 0);
  bool satisfied = false;
  for (int i = 0; i < size; ++i) {
    const int ref = ct.bool_or().literals(i);
    const int var = PositiveRef(ref);
    if ((*domains)[var].IsFixed()) {
      if ((*domains)[var].FixedValue() == (RefIsPositive(ref) ? 1 : 0)) {
        satisfied = true;
      }
    } else {
      // We still need to assign free variable. Any value should work.
      (*domains)[PositiveRef(ref)] = Domain(0);
    }
  }
  if (satisfied) return;

  // Change the value of the first variable (which was chosen at presolve).
  const int first_ref = ct.bool_or().literals(0);
  (*domains)[PositiveRef(first_ref)] = Domain(RefIsPositive(first_ref) ? 1 : 0);
}

void PostsolveExactlyOne(const ConstraintProto& ct,
                         std::vector<Domain>* domains) {
  bool satisfied = false;
  std::vector<int> free_variables;
  for (const int ref : ct.exactly_one().literals()) {
    const int var = PositiveRef(ref);
    if ((*domains)[var].IsFixed()) {
      if ((*domains)[var].FixedValue() == (RefIsPositive(ref) ? 1 : 0)) {
        CHECK(!satisfied) << "Two variables at one in exactly one.";
        satisfied = true;
      }
    } else {
      free_variables.push_back(ref);
    }
  }
  if (!satisfied) {
    // Fix one at true.
    CHECK(!free_variables.empty()) << "All zero in exactly one";
    const int ref = free_variables.back();
    (*domains)[PositiveRef(ref)] = Domain(RefIsPositive(ref) ? 1 : 0);
    free_variables.pop_back();
  }

  // Fix any free variable left at false.
  for (const int ref : free_variables) {
    (*domains)[PositiveRef(ref)] = Domain(RefIsPositive(ref) ? 0 : 1);
  }
}

// For now we set the first unset enforcement literal to false.
// There must be one.
void SetEnforcementLiteralToFalse(const ConstraintProto& ct,
                                  std::vector<Domain>* domains) {
  CHECK(!ct.enforcement_literal().empty());
  bool has_free_enforcement_literal = false;
  for (const int enf : ct.enforcement_literal()) {
    if ((*domains)[PositiveRef(enf)].IsFixed()) continue;
    has_free_enforcement_literal = true;
    if (RefIsPositive(enf)) {
      (*domains)[enf] = Domain(0);
    } else {
      (*domains)[PositiveRef(enf)] = Domain(1);
    }
    break;
  }
  if (!has_free_enforcement_literal) {
    LOG(FATAL)
        << "Unsatisfied linear constraint with no free enforcement literal: "
        << ct.ShortDebugString();
  }
}

// Here we simply assign all non-fixed variable to a feasible value. Which
// should always exists by construction.
void PostsolveLinear(const ConstraintProto& ct,
                     const std::vector<bool>& prefer_lower_value,
                     std::vector<Domain>* domains) {
  int64_t fixed_activity = 0;
  const int size = ct.linear().vars().size();
  std::vector<int> free_vars;
  std::vector<int64_t> free_coeffs;
  for (int i = 0; i < size; ++i) {
    const int var = ct.linear().vars(i);
    const int64_t coeff = ct.linear().coeffs(i);
    CHECK_LT(var, domains->size());
    if (coeff == 0) continue;
    if ((*domains)[var].IsFixed()) {
      fixed_activity += (*domains)[var].FixedValue() * coeff;
    } else {
      free_vars.push_back(var);
      free_coeffs.push_back(coeff);
    }
  }
  if (free_vars.empty()) {
    const Domain rhs = ReadDomainFromProto(ct.linear());
    if (!rhs.Contains(fixed_activity)) {
      SetEnforcementLiteralToFalse(ct, domains);
    }
    return;
  }

  // Fast track for the most common case.
  const Domain initial_rhs = ReadDomainFromProto(ct.linear());
  if (free_vars.size() == 1) {
    const int var = free_vars[0];
    const Domain domain = initial_rhs.AdditionWith(Domain(-fixed_activity))
                              .InverseMultiplicationBy(free_coeffs[0])
                              .IntersectionWith((*domains)[var]);
    if (domain.IsEmpty()) {
      SetEnforcementLiteralToFalse(ct, domains);
      return;
    }
    const int64_t value = prefer_lower_value[var] ? domain.Min() : domain.Max();
    (*domains)[var] = Domain(value);
    return;
  }

  // The postsolve code is a bit involved if there is more than one free
  // variable, we have to postsolve them one by one.
  //
  // Here we recompute the same domains as during the presolve. Everything is
  // like if we where substiting the variable one by one:
  //    terms[i] + fixed_activity \in rhs_domains[i]
  // In the reverse order.
  std::vector<Domain> rhs_domains;
  rhs_domains.push_back(initial_rhs);
  for (int i = 0; i + 1 < free_vars.size(); ++i) {
    // Note that these should be exactly the same computation as the one done
    // during presolve and should be exact. However, we have some tests that do
    // not comply, so we don't check exactness here. Also, as long as we don't
    // get empty domain below, and the complexity of the domain do not explode
    // here, we should be fine.
    Domain term = (*domains)[free_vars[i]].MultiplicationBy(-free_coeffs[i]);
    rhs_domains.push_back(term.AdditionWith(rhs_domains.back()));
  }
  for (int i = free_vars.size() - 1; i >= 0; --i) {
    // Choose a value for free_vars[i] that fall into rhs_domains[i] -
    // fixed_activity. This will crash if the intersection is empty, but it
    // shouldn't be.
    const int var = free_vars[i];
    const int64_t coeff = free_coeffs[i];
    const Domain domain = rhs_domains[i]
                              .AdditionWith(Domain(-fixed_activity))
                              .InverseMultiplicationBy(coeff)
                              .IntersectionWith((*domains)[var]);

    // TODO(user): I am not 100% that the algo here might cover all the presolve
    // case, so if this fail, it might indicate an issue here and not in the
    // presolve/solver code.
    CHECK(!domain.IsEmpty()) << ct.ShortDebugString();
    const int64_t value = prefer_lower_value[var] ? domain.Min() : domain.Max();
    (*domains)[var] = Domain(value);

    fixed_activity += coeff * value;
  }
  DCHECK(initial_rhs.Contains(fixed_activity));
}

// We assign any non fixed lhs variables to their minimum value. Then we assign
// the target to the max. This should always be feasible.
//
// Note(user): Our heuristic is not feasible if x = max(-x, ...) but we made
// sure we don't output such int_max here. Alternatively we could probably fix
// the code here.
void PostsolveIntMax(const ConstraintProto& ct, std::vector<Domain>* domains) {
  int64_t m = std::numeric_limits<int64_t>::min();
  for (const int ref : ct.int_max().vars()) {
    const int var = PositiveRef(ref);
    if (!(*domains)[var].IsFixed()) {
      // Assign to minimum value.
      const int64_t value =
          RefIsPositive(ref) ? (*domains)[var].Min() : (*domains)[var].Max();
      (*domains)[var] = Domain(value);
    }

    const int64_t value = (*domains)[var].FixedValue();
    m = std::max(m, RefIsPositive(ref) ? value : -value);
  }
  const int target_ref = ct.int_max().target();
  const int target_var = PositiveRef(target_ref);
  if (RefIsPositive(target_ref)) {
    (*domains)[target_var] = (*domains)[target_var].IntersectionWith(Domain(m));
  } else {
    (*domains)[target_var] =
        (*domains)[target_var].IntersectionWith(Domain(-m));
  }
  CHECK(!(*domains)[target_var].IsEmpty());
}

// We only support 3 cases in the presolve currently.
void PostsolveElement(const ConstraintProto& ct, std::vector<Domain>* domains) {
  const int index_ref = ct.element().index();
  const int index_var = PositiveRef(index_ref);
  const int target_ref = ct.element().target();
  const int target_var = PositiveRef(target_ref);

  // Deal with non-fixed target and non-fixed index. This only happen if
  // whatever the value of the index and selected variable, we can choose a
  // valid target, so we just fix the index to its min value in this case.
  if (!(*domains)[target_var].IsFixed() && !(*domains)[index_var].IsFixed()) {
    const int64_t index_var_value = (*domains)[index_var].Min();
    (*domains)[index_var] = Domain(index_var_value);

    // If the selected variable is not fixed, we also need to fix it.
    const int selected_ref = ct.element().vars(
        RefIsPositive(index_ref) ? index_var_value : -index_var_value);
    const int selected_var = PositiveRef(selected_ref);
    if (!(*domains)[selected_var].IsFixed()) {
      (*domains)[selected_var] = Domain((*domains)[selected_var].Min());
    }
  }

  // Deal with fixed index (and constant vars).
  if ((*domains)[index_var].IsFixed()) {
    const int64_t index_var_value = (*domains)[index_var].FixedValue();
    const int selected_ref = ct.element().vars(
        RefIsPositive(index_ref) ? index_var_value : -index_var_value);
    const int selected_var = PositiveRef(selected_ref);
    const int64_t selected_value = (*domains)[selected_var].FixedValue();
    (*domains)[target_var] = (*domains)[target_var].IntersectionWith(
        Domain(RefIsPositive(target_ref) == RefIsPositive(selected_ref)
                   ? selected_value
                   : -selected_value));
    DCHECK(!(*domains)[target_var].IsEmpty());
    return;
  }

  // Deal with fixed target (and constant vars).
  const int64_t target_value = (*domains)[target_var].FixedValue();
  int selected_index_value = -1;
  for (const int64_t v : (*domains)[index_var].Values()) {
    const int64_t i = index_var == index_ref ? v : -v;
    if (i < 0 || i >= ct.element().vars_size()) continue;

    const int ref = ct.element().vars(i);
    const int var = PositiveRef(ref);
    const int64_t value = (*domains)[var].FixedValue();
    if (RefIsPositive(target_ref) == RefIsPositive(ref)) {
      if (value == target_value) {
        selected_index_value = i;
        break;
      }
    } else {
      if (value == -target_value) {
        selected_index_value = i;
        break;
      }
    }
  }

  CHECK_NE(selected_index_value, -1);
  (*domains)[index_var] = (*domains)[index_var].IntersectionWith(Domain(
      RefIsPositive(index_ref) ? selected_index_value : -selected_index_value));
  DCHECK(!(*domains)[index_var].IsEmpty());
}

void PostsolveResponse(const int64_t num_variables_in_original_model,
                       const CpModelProto& mapping_proto,
                       const std::vector<int>& postsolve_mapping,
                       CpSolverResponse* response) {
  // Map back the sufficient assumptions for infeasibility.
  for (int& ref :
       *(response->mutable_sufficient_assumptions_for_infeasibility())) {
    ref = RefIsPositive(ref) ? postsolve_mapping[ref]
                             : NegatedRef(postsolve_mapping[PositiveRef(ref)]);
  }

  // Abort if no solution or something is wrong.
  if (response->status() != CpSolverStatus::FEASIBLE &&
      response->status() != CpSolverStatus::OPTIMAL) {
    return;
  }
  if (response->solution_size() != postsolve_mapping.size()) return;

  // Read the initial variable domains, either from the fixed solution of the
  // presolved problems or from the mapping model.
  std::vector<Domain> domains(mapping_proto.variables_size());
  for (int i = 0; i < postsolve_mapping.size(); ++i) {
    CHECK_LE(postsolve_mapping[i], domains.size());
    domains[postsolve_mapping[i]] = Domain(response->solution(i));
  }
  for (int i = 0; i < domains.size(); ++i) {
    if (domains[i].IsEmpty()) {
      domains[i] = ReadDomainFromProto(mapping_proto.variables(i));
    }
    CHECK(!domains[i].IsEmpty());
  }

  // Some free variable should be fixed towards their good objective direction.
  //
  // TODO(user): currently the objective is not part of the mapping_proto, so
  // this shouldn't matter for our current presolve reduction.
  CHECK(!mapping_proto.has_objective());
  std::vector<bool> prefer_lower_value(domains.size(), true);
  if (mapping_proto.has_objective()) {
    const int size = mapping_proto.objective().vars().size();
    for (int i = 0; i < size; ++i) {
      int var = mapping_proto.objective().vars(i);
      int64_t coeff = mapping_proto.objective().coeffs(i);
      if (!RefIsPositive(var)) {
        var = PositiveRef(var);
        coeff = -coeff;
      }
      prefer_lower_value[i] = (coeff >= 0);
    }
  }

  // Process the constraints in reverse order.
  const int num_constraints = mapping_proto.constraints_size();
  for (int i = num_constraints - 1; i >= 0; i--) {
    const ConstraintProto& ct = mapping_proto.constraints(i);

    // We ignore constraint with an enforcement literal set to false. If the
    // enforcement is still unclear, we still process this constraint.
    bool constraint_can_be_ignored = false;
    for (const int enf : ct.enforcement_literal()) {
      const int var = PositiveRef(enf);
      const bool is_false =
          domains[var].IsFixed() &&
          RefIsPositive(enf) == (domains[var].FixedValue() == 0);
      if (is_false) {
        constraint_can_be_ignored = true;
        break;
      }
    }
    if (constraint_can_be_ignored) continue;

    switch (ct.constraint_case()) {
      case ConstraintProto::kBoolOr:
        PostsolveClause(ct, &domains);
        break;
      case ConstraintProto::kExactlyOne:
        PostsolveExactlyOne(ct, &domains);
        break;
      case ConstraintProto::kLinear:
        PostsolveLinear(ct, prefer_lower_value, &domains);
        break;
      case ConstraintProto::kIntMax:
        PostsolveIntMax(ct, &domains);
        break;
      case ConstraintProto::kElement:
        PostsolveElement(ct, &domains);
        break;
      default:
        // This should never happen as we control what kind of constraint we
        // add to the mapping_proto;
        LOG(FATAL) << "Unsupported constraint: " << ct.ShortDebugString();
    }
  }

  // Fill the response. Maybe fix some still unfixed variable.
  response->mutable_solution()->Clear();
  CHECK_LE(num_variables_in_original_model, domains.size());
  for (int i = 0; i < num_variables_in_original_model; ++i) {
    if (prefer_lower_value[i]) {
      response->add_solution(domains[i].Min());
    } else {
      response->add_solution(domains[i].Max());
    }
  }
}

}  // namespace sat
}  // namespace operations_research
