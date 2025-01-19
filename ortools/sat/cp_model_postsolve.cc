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

#include "ortools/sat/cp_model_postsolve.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

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
        << ProtobufShortDebugString(ct);
  }
}

// Here we simply assign all non-fixed variable to a feasible value. Which
// should always exists by construction.
void PostsolveLinear(const ConstraintProto& ct, std::vector<Domain>* domains) {
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
    } else {
      // The constraint is satisfied, if there is any enforcement that are
      // not fixed yet, we need to fix them.
      //
      // Tricky: We sometime push two constraints for postsolve:
      // 1/ l      =>   A
      // 2/ not(l) =>   B
      // if B is true, it is better to fix `l` so that the constraint 2/ is
      // enforced. This way we should have no problem when processing 1/
      //
      // TODO(user): This is a bit hacky, if we need to postsolve both
      // constraints at once, it might be cleaner to do that in a single
      // postsolve operation. However this allows us to reuse normal constraints
      // for the postsolve specification, which is nice.
      for (const int enf : ct.enforcement_literal()) {
        Domain& d = (*domains)[PositiveRef(enf)];
        if (!d.IsFixed()) {
          d = Domain(RefIsPositive(enf) ? 1 : 0);
        }
      }
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
    (*domains)[var] = Domain(domain.SmallestValue());
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
  std::vector<int64_t> values(free_vars.size());
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
    if (domain.IsEmpty()) {
      LOG(INFO) << "Empty domain while trying to assign " << var;
      for (int i = 0; i < size; ++i) {
        const int var = ct.linear().vars(i);
        LOG(INFO) << var << " " << (*domains)[var];
      }
      LOG(FATAL) << "Couldn't postsolve the constraint: "
                 << ProtobufShortDebugString(ct);
    }

    CHECK(!domain.IsEmpty()) << ProtobufShortDebugString(ct);
    const int64_t value = domain.SmallestValue();
    values[i] = value;
    fixed_activity += coeff * value;
  }

  // We assign that afterwards for better debugging if we run into the domains
  // empty above.
  for (int i = 0; i < free_vars.size(); ++i) {
    (*domains)[free_vars[i]] = Domain(values[i]);
  }

  DCHECK(initial_rhs.Contains(fixed_activity));
}

namespace {

// Note that if a domain is not fixed, we just take its Min() value.
int64_t EvaluateLinearExpression(const LinearExpressionProto& expr,
                                 absl::Span<const Domain> domains) {
  int64_t value = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int ref = expr.vars(i);
    const int64_t increment =
        domains[PositiveRef(expr.vars(i))].Min() * expr.coeffs(i);
    value += RefIsPositive(ref) ? increment : -increment;
  }
  return value;
}

bool LinearExpressionIsFixed(const LinearExpressionProto& expr,
                             absl::Span<const Domain> domains) {
  for (const int var : expr.vars()) {
    if (!domains[var].IsFixed()) return false;
  }
  return true;
}

}  // namespace

// Compute the max of each expression, and assign it to the target expr. We only
// support post-solving the case where whatever the value of all expression,
// there will be a valid target.
void PostsolveLinMax(const ConstraintProto& ct, std::vector<Domain>* domains) {
  int64_t max_value = std::numeric_limits<int64_t>::min();
  for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
    // In most case all expression are fixed, except in the corner case where
    // one of the expression refer to the target itself !
    max_value = std::max(max_value, EvaluateLinearExpression(expr, *domains));
  }

  const LinearExpressionProto& target = ct.lin_max().target();
  CHECK_EQ(target.vars().size(), 1);
  CHECK(RefIsPositive(target.vars(0)));

  max_value -= target.offset();
  CHECK_EQ(max_value % target.coeffs(0), 0);
  max_value /= target.coeffs(0);
  (*domains)[target.vars(0)] = Domain(max_value);
}

// We only support 2 cases:  either the index was removed, of the target, not
// both.
void PostsolveElement(const ConstraintProto& ct, std::vector<Domain>* domains) {
  const LinearExpressionProto& index = ct.element().linear_index();
  const LinearExpressionProto& target = ct.element().linear_target();

  DCHECK(LinearExpressionIsFixed(index, *domains) ||
         LinearExpressionIsFixed(target, *domains));

  // Deal with fixed index.
  if (LinearExpressionIsFixed(index, *domains)) {
    const int64_t index_value = EvaluateLinearExpression(index, *domains);
    const LinearExpressionProto& expr = ct.element().exprs(index_value);
    DCHECK(LinearExpressionIsFixed(expr, *domains));
    const int64_t expr_value = EvaluateLinearExpression(expr, *domains);
    if (target.vars().empty()) {
      DCHECK_EQ(expr_value, target.offset());
    } else {
      (*domains)[target.vars(0)] = Domain(GetInnerVarValue(target, expr_value));
    }
    return;
  }

  // Deal with fixed target (and constant vars).
  const int64_t target_value = EvaluateLinearExpression(target, *domains);
  int selected_index_value = -1;
  for (const int64_t v : (*domains)[index.vars(0)].Values()) {
    const int64_t index_value = index.offset() + v * index.coeffs(0);
    DCHECK_GE(index_value, 0);
    DCHECK_LT(index_value, ct.element().exprs_size());

    const LinearExpressionProto& expr = ct.element().exprs(index_value);
    const int64_t value = EvaluateLinearExpression(expr, *domains);
    if (value == target_value) {
      selected_index_value = index_value;
      break;
    }
  }

  CHECK_NE(selected_index_value, -1);
  (*domains)[index.vars(0)] =
      Domain(GetInnerVarValue(index, selected_index_value));
}

// We only support assigning to an affine target.
void PostsolveIntMod(const ConstraintProto& ct, std::vector<Domain>* domains) {
  const int64_t exp = EvaluateLinearExpression(ct.int_mod().exprs(0), *domains);
  const int64_t mod = EvaluateLinearExpression(ct.int_mod().exprs(1), *domains);
  CHECK_NE(mod, 0);
  const int64_t target_value = exp % mod;

  const LinearExpressionProto& target = ct.int_mod().target();
  CHECK_EQ(target.vars().size(), 1);
  const int64_t term_value = target_value - target.offset();
  CHECK_EQ(term_value % target.coeffs(0), 0);
  const int64_t value = term_value / target.coeffs(0);
  CHECK((*domains)[target.vars(0)].Contains(value));
  (*domains)[target.vars(0)] = Domain(value);
}

// We only support assigning to an affine target.
void PostsolveIntProd(const ConstraintProto& ct, std::vector<Domain>* domains) {
  int64_t target_value = 1;
  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    target_value *= EvaluateLinearExpression(expr, *domains);
  }

  const LinearExpressionProto& target = ct.int_prod().target();
  CHECK_EQ(target.vars().size(), 1);
  CHECK(RefIsPositive(target.vars(0)));

  target_value -= target.offset();
  CHECK_EQ(target_value % target.coeffs(0), 0);
  target_value /= target.coeffs(0);
  (*domains)[target.vars(0)] = Domain(target_value);
}

void PostsolveResponse(const int64_t num_variables_in_original_model,
                       const CpModelProto& mapping_proto,
                       absl::Span<const int> postsolve_mapping,
                       std::vector<int64_t>* solution) {
  CHECK_EQ(solution->size(), postsolve_mapping.size());

  // Read the initial variable domains, either from the fixed solution of the
  // presolved problems or from the mapping model.
  std::vector<Domain> domains(mapping_proto.variables_size());
  for (int i = 0; i < postsolve_mapping.size(); ++i) {
    CHECK_LE(postsolve_mapping[i], domains.size());
    domains[postsolve_mapping[i]] = Domain((*solution)[i]);
  }
  for (int i = 0; i < domains.size(); ++i) {
    if (domains[i].IsEmpty()) {
      domains[i] = ReadDomainFromProto(mapping_proto.variables(i));
    }
    CHECK(!domains[i].IsEmpty());
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
        PostsolveLinear(ct, &domains);
        break;
      case ConstraintProto::kLinMax:
        PostsolveLinMax(ct, &domains);
        break;
      case ConstraintProto::kElement:
        PostsolveElement(ct, &domains);
        break;
      case ConstraintProto::kIntMod:
        PostsolveIntMod(ct, &domains);
        break;
      case ConstraintProto::kIntProd:
        PostsolveIntProd(ct, &domains);
        break;
      default:
        // This should never happen as we control what kind of constraint we
        // add to the mapping_proto;
        LOG(FATAL) << "Unsupported constraint: "
                   << ProtobufShortDebugString(ct);
    }
  }

  // Fill the response.
  // Maybe fix some still unfixed variable.
  solution->clear();
  CHECK_LE(num_variables_in_original_model, domains.size());
  for (int i = 0; i < num_variables_in_original_model; ++i) {
    solution->push_back(domains[i].SmallestValue());
  }
}

void FillTightenedDomainInResponse(const CpModelProto& original_model,
                                   const CpModelProto& mapping_proto,
                                   absl::Span<const int> postsolve_mapping,
                                   absl::Span<const Domain> search_domains,
                                   CpSolverResponse* response,
                                   SolverLogger* logger) {
  // The [0, num_vars) part will contain the tightened domains.
  const int num_original_vars = original_model.variables().size();
  const int num_expanded_vars = mapping_proto.variables().size();
  CHECK_LE(num_original_vars, num_expanded_vars);
  std::vector<Domain> domains(num_expanded_vars);

  // Start with the domain from the mapping proto. Note that by construction
  // this should be tighter than the original variable domains.
  for (int i = 0; i < num_expanded_vars; ++i) {
    domains[i] = ReadDomainFromProto(mapping_proto.variables(i));
    if (i < num_original_vars) {
      CHECK(domains[i].IsIncludedIn(
          ReadDomainFromProto(original_model.variables(i))));
    }
  }

  // The first test is for the corner case of presolve closing the problem,
  // in which case there is no more info to process.
  int num_common_vars = 0;
  int num_affine_reductions = 0;
  if (!search_domains.empty()) {
    if (postsolve_mapping.empty()) {
      // Currently no mapping should mean all variables are in common. This
      // happen when presolve is disabled, but we might still have more
      // variables due to expansion for instance.
      //
      // There is also the corner case of presolve closing the problem,
      CHECK_GE(search_domains.size(), num_original_vars);
      num_common_vars = num_original_vars;
      for (int i = 0; i < num_original_vars; ++i) {
        domains[i] = domains[i].IntersectionWith(search_domains[i]);
      }
    } else {
      // This is the normal presolve case.
      // Intersect the domain of the variables in common.
      CHECK_EQ(postsolve_mapping.size(), search_domains.size());
      for (int search_i = 0; search_i < postsolve_mapping.size(); ++search_i) {
        const int i_in_mapping_model = postsolve_mapping[search_i];
        if (i_in_mapping_model < num_original_vars) {
          ++num_common_vars;
        }
        domains[i_in_mapping_model] =
            domains[i_in_mapping_model].IntersectionWith(
                search_domains[search_i]);
      }

      // Look for affine relation, and do more intersection.
      for (const ConstraintProto& ct : mapping_proto.constraints()) {
        if (ct.constraint_case() != ConstraintProto::kLinear) continue;
        const LinearConstraintProto& lin = ct.linear();
        if (lin.vars().size() != 2) continue;
        if (lin.domain().size() != 2) continue;
        if (lin.domain(0) != lin.domain(1)) continue;
        int v1 = lin.vars(0);
        int v2 = lin.vars(1);
        int c1 = lin.coeffs(0);
        int c2 = lin.coeffs(1);
        if (v2 < num_original_vars && v1 >= num_original_vars) {
          std::swap(v1, v2);
          std::swap(c1, c2);
        }
        if (v1 < num_original_vars && v2 >= num_original_vars) {
          // We can reduce the domain of v1 by using the affine relation
          // and the domain of v2.
          // We have c1 * v2 + c2 * v2 = offset;
          const int64_t offset = lin.domain(0);
          const Domain restriction =
              Domain(offset)
                  .AdditionWith(domains[v2].ContinuousMultiplicationBy(-c2))
                  .InverseMultiplicationBy(c1);
          if (!domains[v1].IsIncludedIn(restriction)) {
            ++num_affine_reductions;
            domains[v1] = domains[v1].IntersectionWith(restriction);
          }
        }
      }
    }
  }

  // Copy the names and replace domains.
  *response->mutable_tightened_variables() = original_model.variables();
  int num_tigher_domains = 0;
  int num_empty = 0;
  int num_fixed = 0;
  for (int i = 0; i < num_original_vars; ++i) {
    FillDomainInProto(domains[i], response->mutable_tightened_variables(i));
    if (domains[i].IsEmpty()) {
      ++num_empty;
      continue;
    }

    if (domains[i].IsFixed()) num_fixed++;
    const Domain original = ReadDomainFromProto(original_model.variables(i));
    if (domains[i] != original) {
      DCHECK(domains[i].IsIncludedIn(original));
      ++num_tigher_domains;
    }
  }

  // Some stats.
  if (num_empty > 0) {
    SOLVER_LOG(logger, num_empty,
               " tightened domains are empty. This should not happen except if "
               "we proven infeasibility or optimality.");
  }
  SOLVER_LOG(logger, "Filled tightened domains in the response.");
  SOLVER_LOG(logger, "[TighteningInfo] num_tighter:", num_tigher_domains,
             " num_fixed:", num_fixed,
             " num_affine_reductions:", num_affine_reductions);
  SOLVER_LOG(logger,
             "[TighteningInfo] original_num_variables:", num_original_vars,
             " during_presolve:", num_expanded_vars,
             " after:", search_domains.size(), " in_common:", num_common_vars);
  SOLVER_LOG(logger, "");
}

}  // namespace sat
}  // namespace operations_research
