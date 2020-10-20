// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

// This postsolve is "special". If the clause is not satisfied, we fix the
// first literal in the clause to true (even if it was fixed to false). This
// allows to handle more complex presolve operations used by the SAT presolver.
//
// Also, any "free" Boolean should be fixed to some value for the subsequent
// postsolve steps.
void PostsolveClause(const ConstraintProto &ct, std::vector<Domain> *domains) {
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
  if (satisfied)
    return;

  // Change the value of the first variable (which was chosen at presolve).
  const int first_ref = ct.bool_or().literals(0);
  (*domains)[PositiveRef(first_ref)] = Domain(RefIsPositive(first_ref) ? 1 : 0);
}

// Here we simply assign all non-fixed variable to a feasible value. Which
// should always exists by construction.
void PostsolveLinear(const ConstraintProto &ct,
                     const std::vector<bool> &prefer_lower_value,
                     std::vector<Domain> *domains) {
  int64 fixed_activity = 0;
  const int size = ct.linear().vars().size();
  std::vector<int> free_vars;
  std::vector<int64> free_coeffs;
  for (int i = 0; i < size; ++i) {
    const int var = ct.linear().vars(i);
    const int64 coeff = ct.linear().coeffs(i);
    CHECK_LT(var, domains->size());
    if (coeff == 0)
      continue;
    if ((*domains)[var].IsFixed()) {
      fixed_activity += (*domains)[var].FixedValue() * coeff;
    } else {
      free_vars.push_back(var);
      free_coeffs.push_back(coeff);
    }
  }
  if (free_vars.empty())
    return;

  Domain rhs =
      ReadDomainFromProto(ct.linear()).AdditionWith(Domain(-fixed_activity));

  // Fast track for the most common case.
  if (free_vars.size() == 1) {
    const int var = free_vars[0];
    const Domain domain = rhs.InverseMultiplicationBy(free_coeffs[0])
        .IntersectionWith((*domains)[var]);
    const int64 value = prefer_lower_value[var] ? domain.Min() : domain.Max();
    (*domains)[var] = Domain(value);
    return;
  }

  // The postsolve code is a bit involved if there is more than one free
  // variable, we have to postsolve them one by one.
  std::vector<Domain> to_add;
  to_add.push_back(Domain(0));
  for (int i = 0; i + 1 < free_vars.size(); ++i) {
    bool exact = false;
    Domain term =
        (*domains)[free_vars[i]].MultiplicationBy(-free_coeffs[i], &exact);
    CHECK(exact);
    to_add.push_back(term.AdditionWith(to_add.back()));
  }
  for (int i = free_vars.size() - 1; i >= 0; --i) {
    // Choose a value for free_vars[i] that fall into rhs + to_add[i].
    // This will crash if the intersection is empty, but it shouldn't be.
    const int var = free_vars[i];
    const int64 coeff = free_coeffs[i];
    const Domain domain = rhs.AdditionWith(to_add[i])
        .InverseMultiplicationBy(coeff).IntersectionWith((*domains)[var]);
    CHECK(!domain.IsEmpty()) << ct.ShortDebugString();
    const int64 value = prefer_lower_value[var] ? domain.Min() : domain.Max();
    (*domains)[var] = Domain(value);
    rhs = rhs.AdditionWith(Domain(-coeff * value));

    // Only needed in debug.
    fixed_activity += coeff * value;
  }
  DCHECK(ReadDomainFromProto(ct.linear()).Contains(fixed_activity));
}

// We assign any non fixed lhs variables to their minimum value. Then we assign
// the target to the max. This should always be feasible.
void PostsolveIntMax(const ConstraintProto &ct, std::vector<Domain> *domains) {
  int64 m = kint64min;
  for (const int ref : ct.int_max().vars()) {
    const int var = PositiveRef(ref);
    if (!(*domains)[var].IsFixed()) {
      // Assign to minimum value.
      const int64 value =
          RefIsPositive(ref) ? (*domains)[var].Min() : (*domains)[var].Max();
      (*domains)[var] = Domain(value);
    }

    const int64 value = (*domains)[var].FixedValue();
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
void PostsolveElement(const ConstraintProto &ct, std::vector<Domain> *domains) {
  const int index_ref = ct.element().index();
  const int index_var = PositiveRef(index_ref);
  const int target_ref = ct.element().target();
  const int target_var = PositiveRef(target_ref);

  // Deal with non-fixed target and non-fixed index. This only happen if
  // whatever the value of the index and selected variable, we can choose a
  // valid target, so we just fix the index to its min value in this case.
  if (!(*domains)[target_var].IsFixed() && !(*domains)[index_var].IsFixed()) {
    const int64 index_value = (*domains)[index_var].Min();
    (*domains)[index_var] = Domain(index_value);

    // If the selected variable is not fixed, we also need to fix it.
    const int selected_ref = ct.element()
        .vars(RefIsPositive(index_ref) ? index_value : -index_value);
    const int selected_var = PositiveRef(selected_ref);
    if (!(*domains)[selected_var].IsFixed()) {
      (*domains)[selected_var] = Domain((*domains)[selected_var].Min());
    }
  }

  // Deal with fixed index (and constant vars).
  if ((*domains)[index_var].IsFixed()) {
    const int64 index_value = (*domains)[index_var].FixedValue();
    const int selected_ref = ct.element()
        .vars(RefIsPositive(index_ref) ? index_value : -index_value);
    const int selected_var = PositiveRef(selected_ref);
    const int64 selected_value = (*domains)[selected_var].FixedValue();
    (*domains)[target_var] = (*domains)[target_var].IntersectionWith(
        Domain(RefIsPositive(target_ref) == RefIsPositive(selected_ref)
                   ? selected_value
                   : -selected_value));
    DCHECK(!(*domains)[target_var].IsEmpty());
    return;
  }

  // Deal with fixed target (and constant vars).
  const int64 target_value = (*domains)[target_var].FixedValue();
  int selected_index_value = -1;
  for (int i = 0; i < ct.element().vars().size(); ++i) {
    const int ref = ct.element().vars(i);
    const int var = PositiveRef(ref);
    const int64 value = (*domains)[var].FixedValue();
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
      RefIsPositive(index_var) ? selected_index_value : -selected_index_value));
  DCHECK(!(*domains)[index_var].IsEmpty());
}

void PostsolveResponse(const int64 num_variables_in_original_model,
                       const CpModelProto &mapping_proto,
                       const std::vector<int> &postsolve_mapping,
                       CpSolverResponse *response) {
  // Abort if no solution or something is wrong.
  if (response->status() != CpSolverStatus::FEASIBLE &&
      response->status() != CpSolverStatus::OPTIMAL) {
    return;
  }
  if (response->solution_size() != postsolve_mapping.size())
    return;

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
      int64 coeff = mapping_proto.objective().coeffs(i);
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
    const ConstraintProto &ct = mapping_proto.constraints(i);

    // We should only encounter assigned enforcement literal.
    bool enforced = true;
    for (const int ref : ct.enforcement_literal()) {
      if (domains[PositiveRef(ref)].FixedValue() ==
          (RefIsPositive(ref) ? 0 : 1)) {
        enforced = false;
        break;
      }
    }
    if (!enforced)
      continue;

    switch (ct.constraint_case()) {
    case ConstraintProto::kBoolOr:
      PostsolveClause(ct, &domains);
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

} // namespace sat
} // namespace operations_research
