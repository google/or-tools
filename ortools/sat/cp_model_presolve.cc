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

#include "ortools/sat/cp_model_presolve.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
#include "absl/strings/str_join.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_objective.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/var_domination.h"

namespace operations_research {
namespace sat {

bool CpModelPresolver::RemoveConstraint(ConstraintProto* ct) {
  ct->Clear();
  return true;
}

void CpModelPresolver::RemoveEmptyConstraints() {
  // Remove all empty constraints. Note that we need to remap the interval
  // references.
  std::vector<int> interval_mapping(context_->working_model->constraints_size(),
                                    -1);
  int new_num_constraints = 0;
  const int old_num_non_empty_constraints =
      context_->working_model->constraints_size();
  for (int c = 0; c < old_num_non_empty_constraints; ++c) {
    const auto type = context_->working_model->constraints(c).constraint_case();
    if (type == ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) continue;
    if (type == ConstraintProto::ConstraintCase::kInterval) {
      interval_mapping[c] = new_num_constraints;
    }
    context_->working_model->mutable_constraints(new_num_constraints++)
        ->Swap(context_->working_model->mutable_constraints(c));
  }
  context_->working_model->mutable_constraints()->DeleteSubrange(
      new_num_constraints, old_num_non_empty_constraints - new_num_constraints);
  for (ConstraintProto& ct_ref :
       *context_->working_model->mutable_constraints()) {
    ApplyToAllIntervalIndices(
        [&interval_mapping](int* ref) {
          *ref = interval_mapping[*ref];
          CHECK_NE(-1, *ref);
        },
        &ct_ref);
  }
}

bool CpModelPresolver::PresolveEnforcementLiteral(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (!HasEnforcementLiteral(*ct)) return false;

  int new_size = 0;
  const int old_size = ct->enforcement_literal().size();
  for (const int literal : ct->enforcement_literal()) {
    if (context_->LiteralIsTrue(literal)) {
      // We can remove a literal at true.
      context_->UpdateRuleStats("true enforcement literal");
      continue;
    }

    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("false enforcement literal");
      return RemoveConstraint(ct);
    }

    if (context_->VariableIsUniqueAndRemovable(literal)) {
      // We can simply set it to false and ignore the constraint in this case.
      context_->UpdateRuleStats("enforcement literal not used");
      CHECK(context_->SetLiteralToFalse(literal));
      return RemoveConstraint(ct);
    }

    // If the literal only appear in the objective, we might be able to fix it
    // to false. TODO(user): generalize if the literal always appear with the
    // same polarity.
    if (context_->VariableWithCostIsUniqueAndRemovable(literal)) {
      const int64_t obj_coeff =
          gtl::FindOrDie(context_->ObjectiveMap(), PositiveRef(literal));
      if (RefIsPositive(literal) == (obj_coeff > 0)) {
        // It is just more advantageous to set it to false!
        context_->UpdateRuleStats("enforcement literal with unique direction");
        CHECK(context_->SetLiteralToFalse(literal));
        return RemoveConstraint(ct);
      }
    }

    ct->set_enforcement_literal(new_size++, literal);
  }
  ct->mutable_enforcement_literal()->Truncate(new_size);
  return new_size != old_size;
}

bool CpModelPresolver::PresolveBoolXor(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  int new_size = 0;
  bool changed = false;
  int num_true_literals = 0;
  int true_literal = std::numeric_limits<int32_t>::min();
  for (const int literal : ct->bool_xor().literals()) {
    // TODO(user): More generally, if a variable appear in only bool xor
    // constraints, we can simply eliminate it using linear algebra on Z/2Z.
    // This should solve in polynomial time the parity-learning*.fzn problems
    // for instance. This seems low priority, but it is also easy to do. Even
    // better would be to have a dedicated propagator with all bool_xor
    // constraints that do the necessary linear algebra.
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      context_->UpdateRuleStats("TODO bool_xor: remove constraint");
    }

    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("bool_xor: remove false literal");
      changed = true;
      continue;
    } else if (context_->LiteralIsTrue(literal)) {
      true_literal = literal;  // Keep if we need to put one back.
      num_true_literals++;
      continue;
    }

    ct->mutable_bool_xor()->set_literals(new_size++, literal);
  }
  if (new_size == 1) {
    context_->UpdateRuleStats("TODO bool_xor: one active literal");
  } else if (new_size == 2) {
    context_->UpdateRuleStats("TODO bool_xor: two active literals");
  }
  if (num_true_literals % 2 == 1) {
    CHECK_NE(true_literal, std::numeric_limits<int32_t>::min());
    ct->mutable_bool_xor()->set_literals(new_size++, true_literal);
  }
  if (num_true_literals > 1) {
    context_->UpdateRuleStats("bool_xor: remove even number of true literals");
    changed = true;
  }
  ct->mutable_bool_xor()->mutable_literals()->Truncate(new_size);
  return changed;
}

bool CpModelPresolver::PresolveBoolOr(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // Move the enforcement literal inside the clause if any. Note that we do not
  // mark this as a change since the literal in the constraint are the same.
  if (HasEnforcementLiteral(*ct)) {
    context_->UpdateRuleStats("bool_or: removed enforcement literal");
    for (const int literal : ct->enforcement_literal()) {
      ct->mutable_bool_or()->add_literals(NegatedRef(literal));
    }
    ct->clear_enforcement_literal();
  }

  // Inspects the literals and deal with fixed ones.
  bool changed = false;
  context_->tmp_literals.clear();
  context_->tmp_literal_set.clear();
  for (const int literal : ct->bool_or().literals()) {
    if (context_->LiteralIsFalse(literal)) {
      changed = true;
      continue;
    }
    if (context_->LiteralIsTrue(literal)) {
      context_->UpdateRuleStats("bool_or: always true");
      return RemoveConstraint(ct);
    }
    // We can just set the variable to true in this case since it is not
    // used in any other constraint (note that we artificially bump the
    // objective var usage by 1).
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      context_->UpdateRuleStats("bool_or: singleton");
      if (!context_->SetLiteralToTrue(literal)) return true;
      return RemoveConstraint(ct);
    }
    if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
      context_->UpdateRuleStats("bool_or: always true");
      return RemoveConstraint(ct);
    }

    if (context_->tmp_literal_set.contains(literal)) {
      changed = true;
    } else {
      context_->tmp_literal_set.insert(literal);
      context_->tmp_literals.push_back(literal);
    }
  }
  context_->tmp_literal_set.clear();

  if (context_->tmp_literals.empty()) {
    context_->UpdateRuleStats("bool_or: empty");
    return context_->NotifyThatModelIsUnsat();
  }
  if (context_->tmp_literals.size() == 1) {
    context_->UpdateRuleStats("bool_or: only one literal");
    if (!context_->SetLiteralToTrue(context_->tmp_literals[0])) return true;
    return RemoveConstraint(ct);
  }
  if (context_->tmp_literals.size() == 2) {
    // For consistency, we move all "implication" into half-reified bool_and.
    // TODO(user): merge by enforcement literal and detect implication cycles.
    context_->UpdateRuleStats("bool_or: implications");
    ct->add_enforcement_literal(NegatedRef(context_->tmp_literals[0]));
    ct->mutable_bool_and()->add_literals(context_->tmp_literals[1]);
    return changed;
  }

  if (changed) {
    context_->UpdateRuleStats("bool_or: fixed literals");
    ct->mutable_bool_or()->mutable_literals()->Clear();
    for (const int lit : context_->tmp_literals) {
      ct->mutable_bool_or()->add_literals(lit);
    }
  }
  return changed;
}

ABSL_MUST_USE_RESULT bool CpModelPresolver::MarkConstraintAsFalse(
    ConstraintProto* ct) {
  if (HasEnforcementLiteral(*ct)) {
    // Change the constraint to a bool_or.
    ct->mutable_bool_or()->clear_literals();
    for (const int lit : ct->enforcement_literal()) {
      ct->mutable_bool_or()->add_literals(NegatedRef(lit));
    }
    ct->clear_enforcement_literal();
    PresolveBoolOr(ct);
    return true;
  } else {
    return context_->NotifyThatModelIsUnsat();
  }
}

bool CpModelPresolver::PresolveBoolAnd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (!HasEnforcementLiteral(*ct)) {
    context_->UpdateRuleStats("bool_and: non-reified.");
    for (const int literal : ct->bool_and().literals()) {
      if (!context_->SetLiteralToTrue(literal)) return true;
    }
    return RemoveConstraint(ct);
  }

  bool changed = false;
  context_->tmp_literals.clear();
  for (const int literal : ct->bool_and().literals()) {
    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("bool_and: always false");
      return MarkConstraintAsFalse(ct);
    }
    if (context_->LiteralIsTrue(literal)) {
      changed = true;
      continue;
    }
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      changed = true;
      if (!context_->SetLiteralToTrue(literal)) return true;
      continue;
    }
    context_->tmp_literals.push_back(literal);
  }

  // Note that this is not the same behavior as a bool_or:
  // - bool_or means "at least one", so it is false if empty.
  // - bool_and means "all literals inside true", so it is true if empty.
  if (context_->tmp_literals.empty()) return RemoveConstraint(ct);

  if (changed) {
    ct->mutable_bool_and()->mutable_literals()->Clear();
    for (const int lit : context_->tmp_literals) {
      ct->mutable_bool_and()->add_literals(lit);
    }
    context_->UpdateRuleStats("bool_and: fixed literals");
  }

  // If a variable can move freely in one direction except for this constraint,
  // we can make it an equality.
  //
  // TODO(user): also consider literal on the other side of the =>.
  if (ct->enforcement_literal().size() == 1 &&
      ct->bool_and().literals().size() == 1) {
    const int enforcement = ct->enforcement_literal(0);
    if (context_->VariableWithCostIsUniqueAndRemovable(enforcement)) {
      int var = PositiveRef(enforcement);
      int64_t obj_coeff = gtl::FindOrDie(context_->ObjectiveMap(), var);
      if (!RefIsPositive(enforcement)) obj_coeff = -obj_coeff;

      // The other case where the constraint is redundant is treated elsewhere.
      if (obj_coeff < 0) {
        context_->UpdateRuleStats("bool_and: dual equality.");
        context_->StoreBooleanEqualityRelation(enforcement,
                                               ct->bool_and().literals(0));
      }
    }
  }

  return changed;
}

bool CpModelPresolver::PresolveAtMostOrExactlyOne(ConstraintProto* ct) {
  bool is_at_most_one = ct->constraint_case() == ConstraintProto::kAtMostOne;
  const std::string name = is_at_most_one ? "at_most_one: " : "exactly_one: ";
  auto* literals = is_at_most_one
                       ? ct->mutable_at_most_one()->mutable_literals()
                       : ct->mutable_exactly_one()->mutable_literals();

  // Deal with duplicate variable reference.
  context_->tmp_literal_set.clear();
  for (const int literal : *literals) {
    if (context_->tmp_literal_set.contains(literal)) {
      if (!context_->SetLiteralToFalse(literal)) return true;
      context_->UpdateRuleStats(absl::StrCat(name, "duplicate literals"));
    }
    if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
      int num_positive = 0;
      int num_negative = 0;
      for (const int other : *literals) {
        if (PositiveRef(other) != PositiveRef(literal)) {
          if (!context_->SetLiteralToFalse(other)) return true;
          context_->UpdateRuleStats(absl::StrCat(name, "x and not(x)"));
        } else {
          if (other == literal) {
            ++num_positive;
          } else {
            ++num_negative;
          }
        }
      }

      // This is tricky for the case where the at most one reduce to (lit,
      // not(lit), not(lit)) for instance.
      if (num_positive > 1 && !context_->SetLiteralToFalse(literal)) {
        return true;
      }
      if (num_negative > 1 && !context_->SetLiteralToTrue(literal)) {
        return true;
      }
      return RemoveConstraint(ct);
    }
    context_->tmp_literal_set.insert(literal);
  }

  // Remove fixed variables.
  bool changed = false;
  bool transform_to_at_most_one = false;
  context_->tmp_literals.clear();
  for (const int literal : *literals) {
    if (context_->LiteralIsTrue(literal)) {
      context_->UpdateRuleStats(absl::StrCat(name, "satisfied"));
      for (const int other : *literals) {
        if (other != literal) {
          if (!context_->SetLiteralToFalse(other)) return true;
        }
      }
      return RemoveConstraint(ct);
    }

    if (context_->LiteralIsFalse(literal)) {
      changed = true;
      continue;
    }

    // A singleton variable in an at most one can just be set to zero.
    //
    // In an exactly one, it can be left to the postsolve to decide, and the
    // rest of the constraint can be transformed to an at most one.
    bool is_removable = context_->VariableIsUniqueAndRemovable(literal);
    if (is_at_most_one && !is_removable &&
        context_->VariableWithCostIsUniqueAndRemovable(literal)) {
      const auto it = context_->ObjectiveMap().find(PositiveRef(literal));
      CHECK(it != context_->ObjectiveMap().end());
      const int64_t coeff = it->second;

      // Fixing it to zero need to go in the correct direction.
      is_removable = (coeff > 0) == RefIsPositive(literal);
    }

    if (is_removable) {
      if (is_at_most_one) {
        context_->UpdateRuleStats("at_most_one: singleton");
        if (!context_->SetLiteralToFalse(literal)) return false;
        changed = true;
        continue;
      } else {
        changed = true;
        is_at_most_one = true;
        transform_to_at_most_one = true;
        *(context_->mapping_model->add_constraints()) = *ct;
        context_->UpdateRuleStats("exactly_one: singleton");
        continue;
      }
    }

    context_->tmp_literals.push_back(literal);
  }

  if (transform_to_at_most_one) {
    CHECK(changed);
    ct->Clear();
    literals = ct->mutable_at_most_one()->mutable_literals();
  }
  if (changed) {
    literals->Clear();
    for (const int lit : context_->tmp_literals) {
      literals->Add(lit);
    }
    context_->UpdateRuleStats(absl::StrCat(name, "removed literals"));
  }
  return changed;
}

bool CpModelPresolver::PresolveAtMostOne(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  CHECK(!HasEnforcementLiteral(*ct));
  const bool changed = PresolveAtMostOrExactlyOne(ct);
  if (ct->constraint_case() != ConstraintProto::kAtMostOne) return changed;

  // Size zero: ok.
  const auto& literals = ct->at_most_one().literals();
  if (literals.empty()) {
    context_->UpdateRuleStats("at_most_one: empty or all false");
    return RemoveConstraint(ct);
  }

  // Size one: always satisfied.
  if (literals.size() == 1) {
    context_->UpdateRuleStats("at_most_one: size one");
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpModelPresolver::PresolveExactlyOne(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  CHECK(!HasEnforcementLiteral(*ct));
  const bool changed = PresolveAtMostOrExactlyOne(ct);
  if (ct->constraint_case() != ConstraintProto::kExactlyOne) return changed;

  // Size zero: UNSAT.
  const auto& literals = ct->exactly_one().literals();
  if (literals.empty()) {
    return context_->NotifyThatModelIsUnsat("exactly_one: empty or all false");
  }

  // Size one: fix variable.
  if (literals.size() == 1) {
    context_->UpdateRuleStats("exactly_one: size one");
    if (!context_->SetLiteralToTrue(literals[0])) return false;
    return RemoveConstraint(ct);
  }

  // Size two: Equivalence.
  if (literals.size() == 2) {
    context_->UpdateRuleStats("exactly_one: size two");
    context_->StoreBooleanEqualityRelation(literals[0],
                                           NegatedRef(literals[1]));
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpModelPresolver::PresolveIntMax(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (ct->int_max().vars().empty()) {
    context_->UpdateRuleStats("int_max: no variables!");
    return MarkConstraintAsFalse(ct);
  }
  const int target_ref = ct->int_max().target();

  // Pass 1, compute the infered min of the target, and remove duplicates.
  int64_t infered_min = std::numeric_limits<int64_t>::min();
  int64_t infered_max = std::numeric_limits<int64_t>::min();
  bool contains_target_ref = false;
  std::set<int> used_ref;
  int new_size = 0;
  for (const int ref : ct->int_max().vars()) {
    if (ref == target_ref) contains_target_ref = true;
    if (gtl::ContainsKey(used_ref, ref)) continue;
    if (gtl::ContainsKey(used_ref, NegatedRef(ref)) ||
        ref == NegatedRef(target_ref)) {
      infered_min = std::max(infered_min, int64_t{0});
    }
    used_ref.insert(ref);
    ct->mutable_int_max()->set_vars(new_size++, ref);
    infered_min = std::max(infered_min, context_->MinOf(ref));
    infered_max = std::max(infered_max, context_->MaxOf(ref));
  }
  if (new_size < ct->int_max().vars_size()) {
    context_->UpdateRuleStats("int_max: removed dup");
  }
  ct->mutable_int_max()->mutable_vars()->Truncate(new_size);
  if (contains_target_ref) {
    context_->UpdateRuleStats("int_max: x = max(x, ...)");
    for (const int ref : ct->int_max().vars()) {
      if (ref == target_ref) continue;
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      auto* arg = new_ct->mutable_linear();
      arg->add_vars(target_ref);
      arg->add_coeffs(1);
      arg->add_vars(ref);
      arg->add_coeffs(-1);
      arg->add_domain(0);
      arg->add_domain(std::numeric_limits<int64_t>::max());
    }
    return RemoveConstraint(ct);
  }

  // Compute the infered target_domain.
  Domain infered_domain;
  for (const int ref : ct->int_max().vars()) {
    infered_domain = infered_domain.UnionWith(
        context_->DomainOf(ref).IntersectionWith({infered_min, infered_max}));
  }

  // Update the target domain.
  bool domain_reduced = false;
  if (!HasEnforcementLiteral(*ct)) {
    if (!context_->IntersectDomainWith(target_ref, infered_domain,
                                       &domain_reduced)) {
      return true;
    }
  }

  // If the target is only used here and if
  // infered_domain ∩ [kint64min, target_ub] ⊂ target_domain
  // then the constraint is really max(...) <= target_ub and we can simplify it.
  if (context_->VariableIsUniqueAndRemovable(target_ref)) {
    const Domain& target_domain = context_->DomainOf(target_ref);
    if (infered_domain
            .IntersectionWith(Domain(std::numeric_limits<int64_t>::min(),
                                     target_domain.Max()))
            .IsIncludedIn(target_domain)) {
      if (infered_domain.Max() <= target_domain.Max()) {
        // The constraint is always satisfiable.
        context_->UpdateRuleStats("int_max: always true");
      } else if (ct->enforcement_literal().empty()) {
        // The constraint just restrict the upper bound of its variable.
        for (const int ref : ct->int_max().vars()) {
          context_->UpdateRuleStats("int_max: lower than constant");
          if (!context_->IntersectDomainWith(
                  ref, Domain(std::numeric_limits<int64_t>::min(),
                              target_domain.Max()))) {
            return false;
          }
        }
      } else {
        // We simply transform this into n reified constraints
        // enforcement => [var_i <= target_domain.Max()].
        context_->UpdateRuleStats("int_max: reified lower than constant");
        for (const int ref : ct->int_max().vars()) {
          ConstraintProto* new_ct = context_->working_model->add_constraints();
          *(new_ct->mutable_enforcement_literal()) = ct->enforcement_literal();
          ct->mutable_linear()->add_vars(ref);
          ct->mutable_linear()->add_coeffs(1);
          ct->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
          ct->mutable_linear()->add_domain(target_domain.Max());
        }
      }

      // In all cases we delete the original constraint.
      context_->MarkVariableAsRemoved(target_ref);
      *(context_->mapping_model->add_constraints()) = *ct;
      return RemoveConstraint(ct);
    }
  }

  // Pass 2, update the argument domains. Filter them eventually.
  new_size = 0;
  const int size = ct->int_max().vars_size();
  const int64_t target_max = context_->MaxOf(target_ref);
  for (const int ref : ct->int_max().vars()) {
    if (!HasEnforcementLiteral(*ct)) {
      if (!context_->IntersectDomainWith(
              ref, Domain(std::numeric_limits<int64_t>::min(), target_max),
              &domain_reduced)) {
        return true;
      }
    }
    if (context_->MaxOf(ref) >= infered_min) {
      ct->mutable_int_max()->set_vars(new_size++, ref);
    }
  }
  if (domain_reduced) {
    context_->UpdateRuleStats("int_max: reduced domains");
  }

  bool modified = false;
  if (new_size < size) {
    context_->UpdateRuleStats("int_max: removed variables");
    ct->mutable_int_max()->mutable_vars()->Truncate(new_size);
    modified = true;
  }

  if (new_size == 0) {
    context_->UpdateRuleStats("int_max: no variables!");
    return MarkConstraintAsFalse(ct);
  }
  if (new_size == 1) {
    // Convert to an equality. Note that we create a new constraint otherwise it
    // might not be processed again.
    context_->UpdateRuleStats("int_max: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    *new_ct = *ct;  // copy name and potential reification.
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(target_ref);
    arg->add_coeffs(1);
    arg->add_vars(ct->int_max().vars(0));
    arg->add_coeffs(-1);
    arg->add_domain(0);
    arg->add_domain(0);
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }
  return modified;
}

bool CpModelPresolver::PresolveLinMin(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // Convert to lin_max and presolve lin_max.
  const auto copy = ct->lin_min();
  SetToNegatedLinearExpression(copy.target(),
                               ct->mutable_lin_max()->mutable_target());
  for (const LinearExpressionProto& expr : copy.exprs()) {
    LinearExpressionProto* const new_expr = ct->mutable_lin_max()->add_exprs();
    SetToNegatedLinearExpression(expr, new_expr);
  }
  return PresolveLinMax(ct);
}

bool CpModelPresolver::PresolveLinMax(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (ct->lin_max().exprs().empty()) {
    context_->UpdateRuleStats("lin_max: no exprs");
    return MarkConstraintAsFalse(ct);
  }

  // Canonicalize all involved expression.
  //
  // TODO(user): If we start to have many constraints like this, we should
  // use reflexion (see cp_model_util) to do that generically.
  bool changed = CanonicalizeLinearExpression(
      *ct, ct->mutable_lin_max()->mutable_target());
  for (LinearExpressionProto& exp : *(ct->mutable_lin_max()->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  // TODO(user): Remove duplicate expressions. This might be expensive.

  // Pass 1, Compute the infered min of the target.
  int64_t infered_min = context_->MinOf(ct->lin_max().target());
  for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
    // TODO(user): Check if the expressions contain target.

    // TODO(user): Check if the negated expression is already present and
    // reduce inferred domain if so.

    infered_min = std::max(infered_min, context_->MinOf(expr));
  }

  // Pass 2, Filter the expressions which are smaller than inferred min.
  int new_size = 0;
  for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
    const LinearExpressionProto& expr = ct->lin_max().exprs(i);
    if (context_->MaxOf(expr) >= infered_min) {
      *ct->mutable_lin_max()->mutable_exprs(new_size) = expr;
      new_size++;
    }
  }

  if (new_size < ct->lin_max().exprs_size()) {
    context_->UpdateRuleStats("lin_max: Removed exprs");
    ct->mutable_lin_max()->mutable_exprs()->DeleteSubrange(
        new_size, ct->lin_max().exprs_size() - new_size);
    return true;
  }

  return changed;
}

bool CpModelPresolver::PresolveIntAbs(ConstraintProto* ct) {
  CHECK_EQ(ct->enforcement_literal_size(), 0);
  if (context_->ModelIsUnsat()) return false;
  const int target_ref = ct->int_max().target();
  const int var = PositiveRef(ct->int_max().vars(0));

  // Propagate from the variable domain to the target variable.
  const Domain var_domain = context_->DomainOf(var);
  const Domain new_target_domain =
      var_domain.UnionWith(var_domain.Negation())
          .IntersectionWith({0, std::numeric_limits<int64_t>::max()});
  if (!context_->DomainOf(target_ref).IsIncludedIn(new_target_domain)) {
    if (!context_->IntersectDomainWith(target_ref, new_target_domain)) {
      return true;
    }
    context_->UpdateRuleStats("int_abs: propagate domain x to abs(x)");
  }

  // Propagate from target domain to variable.
  const Domain target_domain = context_->DomainOf(target_ref);
  const Domain new_var_domain =
      target_domain.UnionWith(target_domain.Negation());
  if (!context_->DomainOf(var).IsIncludedIn(new_var_domain)) {
    if (!context_->IntersectDomainWith(var, new_var_domain)) {
      return true;
    }
    context_->UpdateRuleStats("int_abs: propagate domain abs(x) to x");
  }

  if (context_->MinOf(var) >= 0 && !context_->IsFixed(var)) {
    context_->UpdateRuleStats("int_abs: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(target_ref);
    arg->add_coeffs(1);
    arg->add_vars(var);
    arg->add_coeffs(-1);
    arg->add_domain(0);
    arg->add_domain(0);
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  if (context_->MaxOf(var) <= 0 && !context_->IsFixed(var)) {
    context_->UpdateRuleStats("int_abs: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(target_ref);
    arg->add_coeffs(1);
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_domain(0);
    arg->add_domain(0);
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Remove the abs constraint if the target is removable or fixed, as domains
  // have been propagated.
  if (context_->VariableIsUniqueAndRemovable(target_ref) ||
      context_->IsFixed(target_ref)) {
    if (!context_->IsFixed(target_ref)) {
      context_->MarkVariableAsRemoved(target_ref);
      *context_->mapping_model->add_constraints() = *ct;
    }
    context_->UpdateRuleStats("int_abs: remove constraint");
    return RemoveConstraint(ct);
  }

  if (context_->StoreAbsRelation(target_ref, var)) {
    context_->UpdateRuleStats("int_abs: store abs(x) == y");
  }

  return false;
}

bool CpModelPresolver::PresolveIntMin(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  const auto copy = ct->int_min();
  ct->mutable_int_max()->set_target(NegatedRef(copy.target()));
  for (const int ref : copy.vars()) {
    ct->mutable_int_max()->add_vars(NegatedRef(ref));
  }
  return PresolveIntMax(ct);
}

bool CpModelPresolver::PresolveIntProd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  if (ct->int_prod().vars().empty()) {
    if (!context_->IntersectDomainWith(ct->int_prod().target(), Domain(1))) {
      return false;
    }
    context_->UpdateRuleStats("int_prod: empty_product");
    return RemoveConstraint(ct);
  }
  bool changed = false;

  // Replace any affine relation without offset.
  // TODO(user): Also remove constant rhs variables.
  int64_t constant = 1;
  for (int i = 0; i < ct->int_prod().vars().size(); ++i) {
    const int ref = ct->int_prod().vars(i);
    const AffineRelation::Relation& r = context_->GetAffineRelation(ref);
    if (r.representative != ref && r.offset == 0) {
      changed = true;
      ct->mutable_int_prod()->set_vars(i, r.representative);
      constant *= r.coeff;
    }
  }

  // TODO(user): Probably better to add a fixed variable to the product
  // instead in this case. But we do need to support product with more than
  // two variables properly for that.
  //
  // TODO(user): We might do that too early since the other presolve step below
  // might simplify the constraint in such a way that there is no need to create
  // a new variable!
  if (constant != 1) {
    context_->UpdateRuleStats("int_prod: extracted product by constant.");

    const int old_target = ct->int_prod().target();
    const int new_target = context_->working_model->variables_size();

    IntegerVariableProto* var_proto = context_->working_model->add_variables();
    FillDomainInProto(
        context_->DomainOf(old_target).InverseMultiplicationBy(constant),
        var_proto);
    context_->InitializeNewDomains();
    if (context_->ModelIsUnsat()) return false;

    ct->mutable_int_prod()->set_target(new_target);
    if (context_->IsFixed(new_target)) {
      // We need to fix old_target too.
      if (!context_->IntersectDomainWith(
              old_target,
              context_->DomainOf(new_target).MultiplicationBy(constant))) {
        return false;
      }
    } else {
      if (!context_->StoreAffineRelation(old_target, new_target, constant, 0)) {
        // We cannot store the affine relation because the old target seems
        // to already be in affine relation with another variable. This is rare
        // and we need to add a new constraint in that case.
        ConstraintProto* new_ct = context_->working_model->add_constraints();
        LinearConstraintProto* lin = new_ct->mutable_linear();
        lin->add_vars(old_target);
        lin->add_coeffs(1);
        lin->add_vars(new_target);
        lin->add_coeffs(-constant);
        lin->add_domain(0);
        lin->add_domain(0);
        context_->UpdateNewConstraintsVariableUsage();
      }
    }
  }

  // Restrict the target domain if possible.
  Domain implied(1);
  for (const int ref : ct->int_prod().vars()) {
    implied = implied.ContinuousMultiplicationBy(context_->DomainOf(ref));
  }
  bool modified = false;
  if (!context_->IntersectDomainWith(ct->int_prod().target(), implied,
                                     &modified)) {
    return false;
  }
  if (modified) {
    context_->UpdateRuleStats("int_prod: reduced target domain.");
  }

  if (ct->int_prod().vars_size() == 2) {
    int a = ct->int_prod().vars(0);
    int b = ct->int_prod().vars(1);
    const int product = ct->int_prod().target();

    if (context_->IsFixed(b)) std::swap(a, b);
    if (context_->IsFixed(a)) {
      const int64_t value_a = context_->MinOf(a);
      if (value_a == 0) {  // Fix target to 0.
        if (!context_->IntersectDomainWith(product, Domain(0, 0))) {
          return false;
        }
        context_->UpdateRuleStats("int_prod: fix target to zero.");
        return RemoveConstraint(ct);
      } else if (b != product) {
        ConstraintProto* const lin = context_->working_model->add_constraints();
        lin->mutable_linear()->add_vars(b);
        lin->mutable_linear()->add_coeffs(value_a);
        lin->mutable_linear()->add_vars(product);
        lin->mutable_linear()->add_coeffs(-1);
        lin->mutable_linear()->add_domain(0);
        lin->mutable_linear()->add_domain(0);
        context_->UpdateNewConstraintsVariableUsage();
        context_->UpdateRuleStats("int_prod: linearize product by constant.");
        return RemoveConstraint(ct);
      } else if (value_a != 1) {
        if (!context_->IntersectDomainWith(product, Domain(0, 0))) {
          return false;
        }
        context_->UpdateRuleStats("int_prod: fix variable to zero.");
        return RemoveConstraint(ct);
      } else {
        context_->UpdateRuleStats("int_prod: remove identity.");
        return RemoveConstraint(ct);
      }
    } else if (a == b && a == product) {  // x = x * x, only true for {0, 1}.
      if (!context_->IntersectDomainWith(product, Domain(0, 1))) {
        return false;
      }
      context_->UpdateRuleStats("int_prod: fix variable to zero or one.");
      return RemoveConstraint(ct);
    }
  }

  // For now, we only presolve the case where all variables are Booleans.
  const int target_ref = ct->int_prod().target();
  if (!RefIsPositive(target_ref)) return changed;
  for (const int var : ct->int_prod().vars()) {
    if (!RefIsPositive(var)) return changed;
    if (context_->MinOf(var) < 0) return changed;
    if (context_->MaxOf(var) > 1) return changed;
  }

  // This is a bool constraint!
  if (!context_->IntersectDomainWith(target_ref, Domain(0, 1))) {
    return false;
  }
  context_->UpdateRuleStats("int_prod: all Boolean.");
  {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->add_enforcement_literal(target_ref);
    auto* arg = new_ct->mutable_bool_and();
    for (const int var : ct->int_prod().vars()) {
      arg->add_literals(var);
    }
  }
  {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    auto* arg = new_ct->mutable_bool_or();
    arg->add_literals(target_ref);
    for (const int var : ct->int_prod().vars()) {
      arg->add_literals(NegatedRef(var));
    }
  }
  context_->UpdateNewConstraintsVariableUsage();
  return RemoveConstraint(ct);
}

bool CpModelPresolver::PresolveIntDiv(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // For now, we only presolve the case where the divisor is constant.
  const int target = ct->int_div().target();
  const int ref_x = ct->int_div().vars(0);
  const int ref_div = ct->int_div().vars(1);
  if (!RefIsPositive(target) || !RefIsPositive(ref_x) ||
      !RefIsPositive(ref_div) || context_->DomainIsEmpty(ref_div) ||
      !context_->IsFixed(ref_div)) {
    return false;
  }

  const int64_t divisor = context_->MinOf(ref_div);
  if (divisor == 1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_vars(ref_x);
    lin->add_coeffs(1);
    lin->add_vars(target);
    lin->add_coeffs(-1);
    lin->add_domain(0);
    lin->add_domain(0);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("int_div: rewrite to equality");
    return RemoveConstraint(ct);
  }
  bool domain_modified = false;
  if (context_->IntersectDomainWith(
          target, context_->DomainOf(ref_x).DivisionBy(divisor),
          &domain_modified)) {
    if (domain_modified) {
      context_->UpdateRuleStats(
          "int_div: updated domain of target in target = X / cte");
    }
  } else {
    // Model is unsat.
    return false;
  }

  // Linearize if everything is positive.
  // TODO(user,user): Deal with other cases where there is no change of
  // sign.We can also deal with target = cte, div variable.

  if (context_->MinOf(target) >= 0 && context_->MinOf(ref_x) >= 0 &&
      divisor > 1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_vars(ref_x);
    lin->add_coeffs(1);
    lin->add_vars(target);
    lin->add_coeffs(-divisor);
    lin->add_domain(0);
    lin->add_domain(divisor - 1);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats(
        "int_div: linearize positive division with a constant divisor");
    return RemoveConstraint(ct);
  }

  // TODO(user): reduce the domain of X by introducing an
  // InverseDivisionOfSortedDisjointIntervals().
  return false;
}

bool CpModelPresolver::ExploitEquivalenceRelations(int c, ConstraintProto* ct) {
  bool changed = false;

  // Optim: Special case for the linear constraint. We just remap the
  // enforcement literals, the normal variables will be replaced by their
  // representative in CanonicalizeLinear().
  if (ct->constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    for (int& ref : *ct->mutable_enforcement_literal()) {
      const int rep = this->context_->GetLiteralRepresentative(ref);
      if (rep != ref) {
        changed = true;
        ref = rep;
      }
    }
    return changed;
  }

  // Optim: This extra loop is a lot faster than reparsing the variable from the
  // proto when there is nothing to do, which is quite often.
  bool work_to_do = false;
  for (const int var : context_->ConstraintToVars(c)) {
    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative != var) {
      work_to_do = true;
      break;
    }
  }
  if (!work_to_do) return false;

  // Remap equal and negated variables to their representative.
  ApplyToAllVariableIndices(
      [&changed, this](int* ref) {
        const int rep = context_->GetVariableRepresentative(*ref);
        if (rep != *ref) {
          changed = true;
          *ref = rep;
        }
      },
      ct);

  // Remap literal and negated literal to their representative.
  ApplyToAllLiteralIndices(
      [&changed, this](int* ref) {
        const int rep = this->context_->GetLiteralRepresentative(*ref);
        if (rep != *ref) {
          changed = true;
          *ref = rep;
        }
      },
      ct);
  return changed;
}

void CpModelPresolver::DivideLinearByGcd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return;

  // Compute the GCD of all coefficients.
  int64_t gcd = 0;
  const int num_vars = ct->linear().vars().size();
  for (int i = 0; i < num_vars; ++i) {
    const int64_t magnitude = std::abs(ct->linear().coeffs(i));
    gcd = MathUtil::GCD64(gcd, magnitude);
    if (gcd == 1) break;
  }
  if (gcd > 1) {
    context_->UpdateRuleStats("linear: divide by GCD");
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_linear()->set_coeffs(i, ct->linear().coeffs(i) / gcd);
    }
    const Domain rhs = ReadDomainFromProto(ct->linear());
    FillDomainInProto(rhs.InverseMultiplicationBy(gcd), ct->mutable_linear());
    if (ct->linear().domain_size() == 0) {
      return (void)MarkConstraintAsFalse(ct);
    }
  }
}

void CpModelPresolver::PresolveLinearEqualityModuloTwo(ConstraintProto* ct) {
  if (!ct->enforcement_literal().empty()) return;
  if (ct->linear().domain().size() != 2) return;
  if (ct->linear().domain(0) != ct->linear().domain(1)) return;
  if (context_->ModelIsUnsat()) return;

  // Any equality must be true modulo n.
  // The case modulo 2 is interesting if the non-zero terms are Booleans.
  std::vector<int> literals;
  for (int i = 0; i < ct->linear().vars().size(); ++i) {
    const int64_t coeff = ct->linear().coeffs(i);
    const int ref = ct->linear().vars(i);
    if (coeff % 2 == 0) continue;
    if (!context_->CanBeUsedAsLiteral(ref)) return;
    literals.push_back(PositiveRef(ref));
    if (literals.size() > 2) return;
  }
  if (literals.size() == 1) {
    const int64_t rhs = std::abs(ct->linear().domain(0));
    context_->UpdateRuleStats("linear: only one odd Boolean in equality");
    if (!context_->IntersectDomainWith(literals[0], Domain(rhs % 2))) return;
  } else if (literals.size() == 2) {
    const int64_t rhs = std::abs(ct->linear().domain(0));
    context_->UpdateRuleStats("linear: only two odd Booleans in equality");
    if (rhs % 2) {
      context_->StoreBooleanEqualityRelation(literals[0],
                                             NegatedRef(literals[1]));
    } else {
      context_->StoreBooleanEqualityRelation(literals[0], literals[1]);
    }
  }
}

template <typename ProtoWithVarsAndCoeffs>
bool CpModelPresolver::CanonicalizeLinearExpressionInternal(
    const ConstraintProto& ct, ProtoWithVarsAndCoeffs* proto, int64_t* offset) {
  // First regroup the terms on the same variables and sum the fixed ones.
  //
  // TODO(user): Add a quick pass to skip most of the work below if the
  // constraint is already in canonical form?
  tmp_terms_.clear();
  int64_t sum_of_fixed_terms = 0;
  bool remapped = false;
  const int old_size = proto->vars().size();
  DCHECK_EQ(old_size, proto->coeffs().size());
  for (int i = 0; i < old_size; ++i) {
    const int ref = proto->vars(i);
    const int var = PositiveRef(ref);
    const int64_t coeff =
        RefIsPositive(ref) ? proto->coeffs(i) : -proto->coeffs(i);
    if (coeff == 0) continue;

    if (context_->IsFixed(var)) {
      sum_of_fixed_terms += coeff * context_->MinOf(var);
      continue;
    }

    // TODO(user): Avoid the quadratic loop for the corner case of many
    // enforcement literal (this should be pretty rare though).
    bool removed = false;
    for (const int enf : ct.enforcement_literal()) {
      if (var == PositiveRef(enf)) {
        if (RefIsPositive(enf)) {
          // If the constraint is enforced, we can assume the variable is at 1.
          sum_of_fixed_terms += coeff;
        } else {
          // We can assume the variable is at zero.
        }
        removed = true;
        break;
      }
    }
    if (removed) {
      context_->UpdateRuleStats("linear: enforcement literal in expression");
      continue;
    }

    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative != var) {
      remapped = true;
      sum_of_fixed_terms += coeff * r.offset;
    }
    tmp_terms_.push_back({r.representative, coeff * r.coeff});
  }
  proto->clear_vars();
  proto->clear_coeffs();
  std::sort(tmp_terms_.begin(), tmp_terms_.end());
  int current_var = 0;
  int64_t current_coeff = 0;
  for (const auto entry : tmp_terms_) {
    CHECK(RefIsPositive(entry.first));
    if (entry.first == current_var) {
      current_coeff += entry.second;
    } else {
      if (current_coeff != 0) {
        proto->add_vars(current_var);
        proto->add_coeffs(current_coeff);
      }
      current_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    proto->add_vars(current_var);
    proto->add_coeffs(current_coeff);
  }
  if (remapped) {
    context_->UpdateRuleStats("linear: remapped using affine relations");
  }
  if (proto->vars().size() < old_size) {
    context_->UpdateRuleStats("linear: fixed or dup variables");
  }
  *offset = sum_of_fixed_terms;
  return remapped || proto->vars().size() < old_size;
}

bool CpModelPresolver::CanonicalizeLinearExpression(
    const ConstraintProto& ct, LinearExpressionProto* exp) {
  int64_t offset = 0;
  const bool result = CanonicalizeLinearExpressionInternal(ct, exp, &offset);
  exp->set_offset(exp->offset() + offset);
  return result;
}

bool CpModelPresolver::CanonicalizeLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }
  int64_t offset = 0;
  const bool result =
      CanonicalizeLinearExpressionInternal(*ct, ct->mutable_linear(), &offset);
  if (offset != 0) {
    FillDomainInProto(
        ReadDomainFromProto(ct->linear()).AdditionWith(Domain(-offset)),
        ct->mutable_linear());
  }
  DivideLinearByGcd(ct);
  return result;
}

bool CpModelPresolver::RemoveSingletonInLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  std::set<int> index_to_erase;
  const int num_vars = ct->linear().vars().size();
  Domain rhs = ReadDomainFromProto(ct->linear());

  // First pass. Process singleton column that are not in the objective. Note
  // that for postsolve, it is important that we process them in the same order
  // in which they will be removed.
  for (int i = 0; i < num_vars; ++i) {
    const int var = ct->linear().vars(i);
    const int64_t coeff = ct->linear().coeffs(i);
    CHECK(RefIsPositive(var));
    if (context_->VariableIsUniqueAndRemovable(var)) {
      bool exact;
      const auto term_domain =
          context_->DomainOf(var).MultiplicationBy(-coeff, &exact);
      if (!exact) continue;

      // We do not do that if the domain of rhs becomes too complex.
      const Domain new_rhs = rhs.AdditionWith(term_domain);
      if (new_rhs.NumIntervals() > 100) continue;

      // Note that we can't do that if we loose information in the
      // multiplication above because the new domain might not be as strict
      // as the initial constraint otherwise. TODO(user): because of the
      // addition, it might be possible to cover more cases though.
      context_->UpdateRuleStats("linear: singleton column");
      index_to_erase.insert(i);
      rhs = new_rhs;
      continue;
    }
  }

  // If we didn't find any, look for the one appearing in the objective.
  if (index_to_erase.empty()) {
    // Note that we only do that if we have a non-reified equality.
    if (context_->params().presolve_substitution_level() <= 0) return false;
    if (!ct->enforcement_literal().empty()) return false;

    // If it is possible to do so, note that we can transform constraint into
    // equalities in PropagateDomainsInLinear().
    if (rhs.Min() != rhs.Max()) return false;

    for (int i = 0; i < num_vars; ++i) {
      const int var = ct->linear().vars(i);
      const int64_t coeff = ct->linear().coeffs(i);
      CHECK(RefIsPositive(var));

      // If the variable appear only in the objective and we have an equality,
      // we can transfer the cost to the rest of the linear expression, and
      // remove that variable.
      //
      // Note that is similar to the substitution code in PresolveLinear() but
      // it doesn't require the variable to be implied free since we do not
      // remove the constraints afterwards, just the variable.
      if (!context_->VariableWithCostIsUniqueAndRemovable(var)) continue;
      DCHECK(context_->ObjectiveMap().contains(var));

      // We only support substitution that does not require to multiply the
      // objective by some factor.
      //
      // TODO(user): If the objective is a single variable, we can actually
      // "absorb" any factor into the objective scaling.
      const int64_t objective_coeff =
          gtl::FindOrDie(context_->ObjectiveMap(), var);
      CHECK_NE(coeff, 0);
      if (objective_coeff % coeff != 0) continue;

      // We do not do that if the domain of rhs becomes too complex.
      bool exact;
      const auto term_domain =
          context_->DomainOf(var).MultiplicationBy(-coeff, &exact);
      if (!exact) continue;
      const Domain new_rhs = rhs.AdditionWith(term_domain);
      if (new_rhs.NumIntervals() > 100) continue;

      // Special case: If the objective was a single variable, we can transfer
      // the domain of var to the objective, and just completely remove this
      // equality constraint like it is done in ExpandObjective().
      if (context_->ObjectiveMap().size() == 1) {
        if (!context_->IntersectDomainWith(
                var, context_->ObjectiveDomain().InverseMultiplicationBy(
                         objective_coeff))) {
          return true;
        }

        // The intersection above might fix var, in which case, we just abort.
        if (context_->IsFixed(var)) continue;

        // This makes sure the domain of var is propagated back to the
        // objective.
        if (!context_->CanonicalizeObjective()) {
          return context_->NotifyThatModelIsUnsat();
        }

        // Normally, CanonicalizeObjective() shouldn't remove var because
        // we work on a linear constraint that has been canonicalized. We keep
        // the test here in case this ever happen so we are notified.
        if (!context_->ObjectiveMap().contains(var)) {
          LOG(WARNING) << "This was not supposed to happen and the presolve "
                          "could be improved.";
          continue;
        }
        context_->UpdateRuleStats("linear: singleton column define objective.");
        context_->SubstituteVariableInObjective(var, coeff, *ct);
        context_->MarkVariableAsRemoved(var);
        *(context_->mapping_model->add_constraints()) = *ct;
        return RemoveConstraint(ct);
      }

      // Update the objective and remove the variable from its equality
      // constraint by expanding its rhs. This might fail if the new linear
      // objective expression can lead to overflow.
      if (!context_->SubstituteVariableInObjective(var, coeff, *ct)) continue;

      context_->UpdateRuleStats(
          "linear: singleton column in equality and in objective.");
      rhs = new_rhs;
      index_to_erase.insert(i);
      break;
    }
  }
  if (index_to_erase.empty()) return false;

  // TODO(user): we could add the constraint to mapping_model only once
  // instead of adding a reduced version of it each time a new singleton
  // variable appear in the same constraint later. That would work but would
  // also force the postsolve to take search decisions...
  *(context_->mapping_model->add_constraints()) = *ct;

  int new_size = 0;
  for (int i = 0; i < num_vars; ++i) {
    if (index_to_erase.count(i)) {
      context_->MarkVariableAsRemoved(ct->linear().vars(i));
      continue;
    }
    ct->mutable_linear()->set_coeffs(new_size, ct->linear().coeffs(i));
    ct->mutable_linear()->set_vars(new_size, ct->linear().vars(i));
    ++new_size;
  }
  ct->mutable_linear()->mutable_vars()->Truncate(new_size);
  ct->mutable_linear()->mutable_coeffs()->Truncate(new_size);
  FillDomainInProto(rhs, ct->mutable_linear());
  DivideLinearByGcd(ct);
  return true;
}

bool CpModelPresolver::PresolveSmallLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  // Empty constraint?
  if (ct->linear().vars().empty()) {
    context_->UpdateRuleStats("linear: empty");
    const Domain rhs = ReadDomainFromProto(ct->linear());
    if (rhs.Contains(0)) {
      return RemoveConstraint(ct);
    } else {
      return MarkConstraintAsFalse(ct);
    }
  }

  // If the constraint is literal => x in domain and x = abs(abs_arg), we can
  // replace x by abs_arg and hopefully remove the variable x later.
  int abs_arg;
  if (ct->linear().vars_size() == 1 && ct->enforcement_literal_size() > 0 &&
      ct->linear().coeffs(0) == 1 &&
      context_->GetAbsRelation(ct->linear().vars(0), &abs_arg)) {
    // TODO(user): Deal with coeff = -1, here or during canonicalization.
    context_->UpdateRuleStats("linear: remove abs from abs(x) in domain");
    const Domain implied_abs_target_domain =
        ReadDomainFromProto(ct->linear())
            .IntersectionWith({0, std::numeric_limits<int64_t>::max()})
            .IntersectionWith(context_->DomainOf(ct->linear().vars(0)));

    if (implied_abs_target_domain.IsEmpty()) {
      return MarkConstraintAsFalse(ct);
    }

    const Domain new_abs_var_domain =
        implied_abs_target_domain
            .UnionWith(implied_abs_target_domain.Negation())
            .IntersectionWith(context_->DomainOf(abs_arg));

    if (new_abs_var_domain.IsEmpty()) {
      return MarkConstraintAsFalse(ct);
    }

    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    for (const int literal : ct->enforcement_literal()) {
      new_ct->add_enforcement_literal(literal);
    }
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(abs_arg);
    arg->add_coeffs(1);
    FillDomainInProto(new_abs_var_domain, new_ct->mutable_linear());
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Detect encoding.
  if (HasEnforcementLiteral(*ct)) {
    if (ct->enforcement_literal_size() != 1 || ct->linear().vars_size() != 1 ||
        (ct->linear().coeffs(0) != 1 && ct->linear().coeffs(0) == -1)) {
      return false;
    }

    // Currently, we only use encoding during expansion, so when it is done,
    // there is no need to updates the maps.
    if (context_->ModelIsExpanded()) return false;

    const int literal = ct->enforcement_literal(0);
    const LinearConstraintProto& linear = ct->linear();
    const int ref = linear.vars(0);
    const int var = PositiveRef(ref);
    const int64_t coeff =
        RefIsPositive(ref) ? ct->linear().coeffs(0) : -ct->linear().coeffs(0);

    if (linear.domain_size() == 2 && linear.domain(0) == linear.domain(1)) {
      const int64_t value = RefIsPositive(ref) ? linear.domain(0) * coeff
                                               : -linear.domain(0) * coeff;
      if (context_->StoreLiteralImpliesVarEqValue(literal, var, value)) {
        // The domain is not actually modified, but we want to rescan the
        // constraints linked to this variable. See TODO below.
        context_->modified_domains.Set(var);
      }
    } else {
      const Domain complement = context_->DomainOf(ref).IntersectionWith(
          ReadDomainFromProto(linear).Complement());
      if (complement.Size() != 1) return false;
      const int64_t value = RefIsPositive(ref) ? complement.Min() * coeff
                                               : -complement.Min() * coeff;
      if (context_->StoreLiteralImpliesVarNEqValue(literal, var, value)) {
        // The domain is not actually modified, but we want to rescan the
        // constraints linked to this variable. See TODO below.
        context_->modified_domains.Set(var);
      }
    }

    // TODO(user): if we have l1 <=> x == value && l2 => x == value, we
    //     could rewrite the second constraint into l2 => l1.
    context_->UpdateNewConstraintsVariableUsage();
    return false;
  }

  // Size one constraint?
  if (ct->linear().vars().size() == 1) {
    const int64_t coeff = RefIsPositive(ct->linear().vars(0))
                              ? ct->linear().coeffs(0)
                              : -ct->linear().coeffs(0);
    context_->UpdateRuleStats("linear: size one");
    const int var = PositiveRef(ct->linear().vars(0));
    const Domain rhs = ReadDomainFromProto(ct->linear());
    if (!context_->IntersectDomainWith(var,
                                       rhs.InverseMultiplicationBy(coeff))) {
      return true;
    }
    return RemoveConstraint(ct);
  }

  // Detect affine relation.
  //
  // TODO(user): it might be better to first add only the affine relation with
  // a coefficient of magnitude 1, and later the one with larger coeffs.
  const LinearConstraintProto& arg = ct->linear();
  if (arg.vars_size() == 2) {
    const Domain rhs = ReadDomainFromProto(ct->linear());
    const int64_t rhs_min = rhs.Min();
    const int64_t rhs_max = rhs.Max();
    if (rhs_min == rhs_max) {
      const int v1 = arg.vars(0);
      const int v2 = arg.vars(1);
      const int64_t coeff1 = arg.coeffs(0);
      const int64_t coeff2 = arg.coeffs(1);
      bool added = false;
      if (coeff1 == 1) {
        added = context_->StoreAffineRelation(v1, v2, -coeff2, rhs_max);
      } else if (coeff2 == 1) {
        added = context_->StoreAffineRelation(v2, v1, -coeff1, rhs_max);
      } else if (coeff1 == -1) {
        added = context_->StoreAffineRelation(v1, v2, coeff2, -rhs_max);
      } else if (coeff2 == -1) {
        added = context_->StoreAffineRelation(v2, v1, coeff1, -rhs_max);
      }
      if (added) return RemoveConstraint(ct);
    }
  }

  return false;
}

namespace {

// Return true if the given domain only restrict the values with an upper bound.
bool IsLeConstraint(const Domain& domain, const Domain& all_values) {
  return all_values
      .IntersectionWith(
          Domain(std::numeric_limits<int64_t>::min(), domain.Max()))
      .IsIncludedIn(domain);
}

// Same as IsLeConstraint() but in the other direction.
bool IsGeConstraint(const Domain& domain, const Domain& all_values) {
  return all_values
      .IntersectionWith(
          Domain(domain.Min(), std::numeric_limits<int64_t>::max()))
      .IsIncludedIn(domain);
}

// In the equation terms + coeff * var_domain \included rhs, returns true if can
// we always fix rhs to its min value for any value in terms. It is okay to
// not be as generic as possible here.
bool RhsCanBeFixedToMin(int64_t coeff, const Domain& var_domain,
                        const Domain& terms, const Domain& rhs) {
  if (var_domain.NumIntervals() != 1) return false;
  if (std::abs(coeff) != 1) return false;

  // If for all values in terms, there is one value below rhs.Min(), then
  // because we add only one integer interval, if there is a feasible value, it
  // can be at rhs.Min().
  //
  // TODO(user): generalize to larger coeff magnitude if rhs is also a multiple
  // or if terms is a multiple.
  if (coeff == 1 && terms.Max() + var_domain.Min() <= rhs.Min()) {
    return true;
  }
  if (coeff == -1 && terms.Max() - var_domain.Max() <= rhs.Min()) {
    return true;
  }
  return false;
}

bool RhsCanBeFixedToMax(int64_t coeff, const Domain& var_domain,
                        const Domain& terms, const Domain& rhs) {
  if (var_domain.NumIntervals() != 1) return false;
  if (std::abs(coeff) != 1) return false;

  if (coeff == 1 && terms.Min() + var_domain.Max() >= rhs.Max()) {
    return true;
  }
  if (coeff == -1 && terms.Min() - var_domain.Min() >= rhs.Max()) {
    return true;
  }
  return false;
}

// Remove from to_clear any entry not in current.
void TakeIntersectionWith(const absl::flat_hash_set<int>& current,
                          absl::flat_hash_set<int>* to_clear) {
  std::vector<int> new_set;
  for (const int c : *to_clear) {
    if (current.contains(c)) new_set.push_back(c);
  }
  to_clear->clear();
  for (const int c : new_set) to_clear->insert(c);
}

}  // namespace

bool CpModelPresolver::PropagateDomainsInLinear(int c, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  // Compute the implied rhs bounds from the variable ones.
  auto& term_domains = context_->tmp_term_domains;
  auto& left_domains = context_->tmp_left_domains;
  const int num_vars = ct->linear().vars_size();
  term_domains.resize(num_vars + 1);
  left_domains.resize(num_vars + 1);
  left_domains[0] = Domain(0);
  for (int i = 0; i < num_vars; ++i) {
    const int var = ct->linear().vars(i);
    const int64_t coeff = ct->linear().coeffs(i);
    CHECK(RefIsPositive(var));
    term_domains[i] = context_->DomainOf(var).MultiplicationBy(coeff);
    left_domains[i + 1] =
        left_domains[i].AdditionWith(term_domains[i]).RelaxIfTooComplex();
  }
  const Domain& implied_rhs = left_domains[num_vars];

  // Abort if trivial.
  const Domain old_rhs = ReadDomainFromProto(ct->linear());
  if (implied_rhs.IsIncludedIn(old_rhs)) {
    context_->UpdateRuleStats("linear: always true");
    return RemoveConstraint(ct);
  }

  // Incorporate the implied rhs information.
  Domain rhs = old_rhs.SimplifyUsingImpliedDomain(implied_rhs);
  if (rhs.IsEmpty()) {
    context_->UpdateRuleStats("linear: infeasible");
    return MarkConstraintAsFalse(ct);
  }
  if (rhs != old_rhs) {
    context_->UpdateRuleStats("linear: simplified rhs");
  }
  FillDomainInProto(rhs, ct->mutable_linear());

  // Detect if it is always good for a term of this constraint to move towards
  // its lower (resp. upper) bound. This is the same as saying that this
  // constraint only bound in one direction.
  bool is_le_constraint = IsLeConstraint(rhs, implied_rhs);
  bool is_ge_constraint = IsGeConstraint(rhs, implied_rhs);

  // Propagate the variable bounds.
  if (ct->enforcement_literal().size() > 1) return false;

  bool new_bounds = false;
  bool recanonicalize = false;
  Domain negated_rhs = rhs.Negation();
  Domain right_domain(0);
  Domain new_domain;
  Domain implied_term_domain;
  term_domains[num_vars] = Domain(0);
  for (int i = num_vars - 1; i >= 0; --i) {
    const int var = ct->linear().vars(i);
    const int64_t var_coeff = ct->linear().coeffs(i);
    right_domain =
        right_domain.AdditionWith(term_domains[i + 1]).RelaxIfTooComplex();
    implied_term_domain = left_domains[i].AdditionWith(right_domain);
    new_domain = implied_term_domain.AdditionWith(negated_rhs)
                     .InverseMultiplicationBy(-var_coeff);

    if (ct->enforcement_literal().empty()) {
      // Push the new domain.
      if (!context_->IntersectDomainWith(var, new_domain, &new_bounds)) {
        return true;
      }
    } else if (ct->enforcement_literal().size() == 1) {
      // We cannot push the new domain, but we can add some deduction.
      CHECK(RefIsPositive(var));
      if (!context_->DomainOfVarIsIncludedIn(var, new_domain)) {
        context_->deductions.AddDeduction(ct->enforcement_literal(0), var,
                                          new_domain);
      }
    }

    if (context_->IsFixed(var)) {
      // This will make sure we remove that fixed variable from the constraint.
      recanonicalize = true;
      continue;
    }

    if (is_le_constraint || is_ge_constraint) {
      CHECK_NE(is_le_constraint, is_ge_constraint);
      if ((var_coeff > 0) == is_ge_constraint) {
        context_->var_to_lb_only_constraints[var].insert(c);
      } else {
        context_->var_to_ub_only_constraints[var].insert(c);
      }

      // Simple dual fixing: If for any feasible solution, any solution with var
      // higher (resp. lower) is also valid, then we can fix that variable to
      // its bound if it also moves the objective in the good direction.
      //
      // A bit tricky. If a linear constraint was detected to not block a
      // variable in one direction, this shouldn't change later (expect in the
      // tightening code below, and we do take care of it). However variable can
      // appear in new constraints.
      if (!context_->keep_all_feasible_solutions) {
        const bool is_in_objective =
            context_->VarToConstraints(var).contains(-1);
        const int size =
            context_->VarToConstraints(var).size() - (is_in_objective ? 1 : 0);
        const int64_t obj_coeff =
            is_in_objective ? gtl::FindOrDie(context_->ObjectiveMap(), var) : 0;

        // We cannot fix anything if the domain of the objective is excluding
        // some objective values.
        if (obj_coeff != 0 && context_->ObjectiveDomainIsConstraining()) {
          continue;
        }

        if (obj_coeff <= 0 &&
            context_->var_to_lb_only_constraints[var].size() >= size) {
          TakeIntersectionWith(context_->VarToConstraints(var),
                               &(context_->var_to_lb_only_constraints[var]));
          if (context_->var_to_lb_only_constraints[var].size() >= size) {
            if (!context_->IntersectDomainWith(var,
                                               Domain(context_->MaxOf(var)))) {
              return false;
            }
            context_->UpdateRuleStats("linear: dual fixing");
            recanonicalize = true;
            continue;
          }
        }
        if (obj_coeff >= 0 &&
            context_->var_to_ub_only_constraints[var].size() >= size) {
          TakeIntersectionWith(context_->VarToConstraints(var),
                               &(context_->var_to_ub_only_constraints[var]));
          if (context_->var_to_ub_only_constraints[var].size() >= size) {
            if (!context_->IntersectDomainWith(var,
                                               Domain(context_->MinOf(var)))) {
              return false;
            }
            context_->UpdateRuleStats("linear: dual fixing");
            recanonicalize = true;
            continue;
          }
        }
      }
    }

    // The other transformations below require a non-reified constraint.
    if (!ct->enforcement_literal().empty()) continue;

    // Given a variable that only appear in one constraint and in the
    // objective, for any feasible solution, it will be always better to move
    // this singleton variable as much as possible towards its good objective
    // direction. Sometimes, we can detect that we will always be able to do
    // this until the only constraint of this singleton variable is tight.
    //
    // When this happens, we can make the constraint an equality. Note that it
    // might not always be good to restrict constraint like this, but in this
    // case, the RemoveSingletonInLinear() code should be able to remove this
    // variable altogether.
    if (rhs.Min() != rhs.Max() &&
        context_->VariableWithCostIsUniqueAndRemovable(var)) {
      const int64_t obj_coeff = gtl::FindOrDie(context_->ObjectiveMap(), var);
      const bool same_sign = (var_coeff > 0) == (obj_coeff > 0);
      bool fixed = false;
      if (same_sign && RhsCanBeFixedToMin(var_coeff, context_->DomainOf(var),
                                          implied_term_domain, rhs)) {
        rhs = Domain(rhs.Min());
        fixed = true;
      }
      if (!same_sign && RhsCanBeFixedToMax(var_coeff, context_->DomainOf(var),
                                           implied_term_domain, rhs)) {
        rhs = Domain(rhs.Max());
        fixed = true;
      }
      if (fixed) {
        context_->UpdateRuleStats("linear: tightened into equality");
        FillDomainInProto(rhs, ct->mutable_linear());
        negated_rhs = rhs.Negation();

        // Restart the loop.
        i = num_vars;
        right_domain = Domain(0);

        // An equality is a >= (or <=) constraint iff all its term are fixed.
        // Since we restart the loop, we will detect that.
        is_le_constraint = false;
        is_ge_constraint = false;
        for (const int var : ct->linear().vars()) {
          context_->var_to_lb_only_constraints[var].erase(c);
          context_->var_to_ub_only_constraints[var].erase(c);
        }
        continue;
      }
    }

    // Can we perform some substitution?
    //
    // TODO(user): there is no guarantee we will not miss some since we might
    // not reprocess a constraint once other have been deleted.

    // Skip affine constraint. It is more efficient to substitute them lazily
    // when we process other constraints. Note that if we relax the fact that
    // we substitute only equalities, we can deal with inequality of size 2
    // here.
    if (ct->linear().vars().size() <= 2) continue;

    // TODO(user): We actually do not need a strict equality when
    // keep_all_feasible_solutions is false, but that simplifies things as the
    // SubstituteVariable() function cannot fail this way.
    if (rhs.Min() != rhs.Max()) continue;

    // Only consider "implied free" variables. Note that the coefficient of
    // magnitude 1 is important otherwise we can't easily remove the
    // constraint since the fact that the sum of the other terms must be a
    // multiple of coeff will not be enforced anymore.
    if (context_->DomainOf(var) != new_domain) continue;
    if (std::abs(var_coeff) != 1) continue;
    if (context_->params().presolve_substitution_level() <= 0) continue;

    // NOTE: The mapping doesn't allow us to remove a variable if
    // 'keep_all_feasible_solutions' is true.
    if (context_->keep_all_feasible_solutions) continue;

    bool is_in_objective = false;
    if (context_->VarToConstraints(var).contains(-1)) {
      is_in_objective = true;
      DCHECK(context_->ObjectiveMap().contains(var));
    }

    // Only consider low degree columns.
    int col_size = context_->VarToConstraints(var).size();
    if (is_in_objective) col_size--;
    const int row_size = ct->linear().vars_size();

    // This is actually an upper bound on the number of entries added since
    // some of them might already be present.
    const int num_entries_added = (row_size - 1) * (col_size - 1);
    const int num_entries_removed = col_size + row_size - 1;

    if (num_entries_added > num_entries_removed) {
      continue;
    }

    // Check pre-conditions on all the constraints in which this variable
    // appear. Basically they must all be linear.
    std::vector<int> others;
    bool abort = false;
    for (const int c : context_->VarToConstraints(var)) {
      if (c == kObjectiveConstraint) continue;
      if (c == kAffineRelationConstraint) {
        abort = true;
        break;
      }
      if (context_->working_model->mutable_constraints(c) == ct) continue;
      if (context_->working_model->constraints(c).constraint_case() !=
          ConstraintProto::ConstraintCase::kLinear) {
        abort = true;
        break;
      }
      for (const int ref :
           context_->working_model->constraints(c).enforcement_literal()) {
        if (PositiveRef(ref) == var) {
          abort = true;
          break;
        }
      }
      others.push_back(c);
    }
    if (abort) continue;

    // Do the actual substitution.
    for (const int c : others) {
      // TODO(user): In some corner cases, this might create integer overflow
      // issues. The danger is limited since the range of the linear
      // expression used in the definition do not exceed the domain of the
      // variable we substitute.
      SubstituteVariable(var, var_coeff, *ct,
                         context_->working_model->mutable_constraints(c));

      // TODO(user): We should re-enqueue these constraints for presolve.
      context_->UpdateConstraintVariableUsage(c);
    }

    // Substitute in objective.
    if (is_in_objective) {
      context_->SubstituteVariableInObjective(var, var_coeff, *ct);
    }

    context_->UpdateRuleStats(
        absl::StrCat("linear: variable substitution ", others.size()));

    // The variable now only appear in its definition and we can remove it
    // because it was implied free.
    //
    // Tricky: If the linear constraint contains other variables that are only
    // used here, then the postsolve needs more info. We do need to indicate
    // that whatever the value of those other variables, we will have a way to
    // assign var. We do that by putting it fist.
    CHECK_EQ(context_->VarToConstraints(var).size(), 1);
    context_->MarkVariableAsRemoved(var);
    const int ct_index = context_->mapping_model->constraints().size();
    *context_->mapping_model->add_constraints() = *ct;
    LinearConstraintProto* mapping_linear_ct =
        context_->mapping_model->mutable_constraints(ct_index)
            ->mutable_linear();
    std::swap(mapping_linear_ct->mutable_vars()->at(0),
              mapping_linear_ct->mutable_vars()->at(i));
    std::swap(mapping_linear_ct->mutable_coeffs()->at(0),
              mapping_linear_ct->mutable_coeffs()->at(i));
    return RemoveConstraint(ct);
  }
  if (new_bounds) {
    context_->UpdateRuleStats("linear: reduced variable domains");
  }
  if (recanonicalize) return CanonicalizeLinear(ct);
  return false;
}

// Identify Boolean variable that makes the constraint always true when set to
// true or false. Moves such literal to the constraint enforcement literals
// list.
//
// We also generalize this to integer variable at one of their bound.
//
// This operation is similar to coefficient strengthening in the MIP world.
void CpModelPresolver::ExtractEnforcementLiteralFromLinearConstraint(
    int ct_index, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return;
  }

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();

  // No need to process size one constraints, they will be presolved separately.
  // We also do not want to split them in two.
  if (num_vars <= 1) return;

  int64_t min_sum = 0;
  int64_t max_sum = 0;
  int64_t max_coeff_magnitude = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int ref = arg.vars(i);
    const int64_t coeff = arg.coeffs(i);
    const int64_t term_a = coeff * context_->MinOf(ref);
    const int64_t term_b = coeff * context_->MaxOf(ref);
    max_coeff_magnitude = std::max(max_coeff_magnitude, std::abs(coeff));
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }

  // We can only extract enforcement literals if the maximum coefficient
  // magnitude is large enough. Note that we handle complex domain.
  //
  // TODO(user): Depending on how we split below, the threshold are not the
  // same. This is maybe not too important, we just don't split as often as we
  // could, but it is still unclear if splitting is good.
  const auto& domain = ct->linear().domain();
  const int64_t ub_threshold = domain[domain.size() - 2] - min_sum;
  const int64_t lb_threshold = max_sum - domain[1];
  const Domain rhs_domain = ReadDomainFromProto(ct->linear());
  if (max_coeff_magnitude < std::max(ub_threshold, lb_threshold)) return;

  // We need the constraint to be only bounded on one side in order to extract
  // enforcement literal.
  //
  // If it is boxed and we know that some coefficient are big enough (see test
  // above), then we split the constraint in two. That might not seems always
  // good, but for the CP propagation engine, we don't loose anything by doing
  // so, and for the LP we will regroup the constraints if they still have the
  // exact same coeff after the presolve.
  //
  // TODO(user): Creating two new constraints and removing the current one might
  // not be the most efficient, but it simplify the presolve code by not having
  // to do anything special to trigger a new presolving of these constraints.
  // Try to improve if this becomes a problem.
  //
  // TODO(user): At the end of the presolve we should probably remerge any
  // identical linear constraints. That also cover the corner cases where
  // constraints are just redundant...
  const bool lower_bounded = min_sum < rhs_domain.Min();
  const bool upper_bounded = max_sum > rhs_domain.Max();
  if (!lower_bounded && !upper_bounded) return;
  if (lower_bounded && upper_bounded) {
    context_->UpdateRuleStats("linear: split boxed constraint");
    ConstraintProto* new_ct1 = context_->working_model->add_constraints();
    *new_ct1 = *ct;
    if (!ct->name().empty()) {
      new_ct1->set_name(absl::StrCat(ct->name(), " (part 1)"));
    }
    FillDomainInProto(Domain(min_sum, rhs_domain.Max()),
                      new_ct1->mutable_linear());

    ConstraintProto* new_ct2 = context_->working_model->add_constraints();
    *new_ct2 = *ct;
    if (!ct->name().empty()) {
      new_ct2->set_name(absl::StrCat(ct->name(), " (part 2)"));
    }
    FillDomainInProto(rhs_domain.UnionWith(Domain(rhs_domain.Max(), max_sum)),
                      new_ct2->mutable_linear());

    context_->UpdateNewConstraintsVariableUsage();
    return (void)RemoveConstraint(ct);
  }

  // Any coefficient greater than this will cause the constraint to be trivially
  // satisfied when the variable move away from its bound. Note that as we
  // remove coefficient, the threshold do not change!
  const int64_t threshold = lower_bounded ? ub_threshold : lb_threshold;

  // Do we only extract Booleans?
  //
  // Note that for now the default is false, and also there are problem calling
  // GetOrCreateVarValueEncoding() after expansion because we might have removed
  // the variable used in the encoding.
  const bool only_booleans =
      !context_->params().presolve_extract_integer_enforcement() ||
      context_->ModelIsExpanded();

  // To avoid a quadratic loop, we will rewrite the linear expression at the
  // same time as we extract enforcement literals.
  int new_size = 0;
  int64_t rhs_offset = 0;
  bool some_integer_encoding_were_extracted = false;
  LinearConstraintProto* mutable_arg = ct->mutable_linear();
  for (int i = 0; i < arg.vars_size(); ++i) {
    int ref = arg.vars(i);
    int64_t coeff = arg.coeffs(i);
    if (coeff < 0) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }

    const bool is_boolean = context_->CanBeUsedAsLiteral(ref);
    if (context_->IsFixed(ref) || coeff < threshold ||
        (only_booleans && !is_boolean)) {
      // We keep this term.
      mutable_arg->set_vars(new_size, mutable_arg->vars(i));
      mutable_arg->set_coeffs(new_size, mutable_arg->coeffs(i));
      ++new_size;
      continue;
    }

    if (is_boolean) {
      context_->UpdateRuleStats("linear: extracted enforcement literal");
    } else {
      some_integer_encoding_were_extracted = true;
      context_->UpdateRuleStats(
          "linear: extracted integer enforcement literal");
    }
    if (lower_bounded) {
      ct->add_enforcement_literal(is_boolean
                                      ? NegatedRef(ref)
                                      : context_->GetOrCreateVarValueEncoding(
                                            ref, context_->MinOf(ref)));
      rhs_offset -= coeff * context_->MinOf(ref);
    } else {
      ct->add_enforcement_literal(is_boolean
                                      ? ref
                                      : context_->GetOrCreateVarValueEncoding(
                                            ref, context_->MaxOf(ref)));
      rhs_offset -= coeff * context_->MaxOf(ref);
    }
  }
  mutable_arg->mutable_vars()->Truncate(new_size);
  mutable_arg->mutable_coeffs()->Truncate(new_size);
  FillDomainInProto(rhs_domain.AdditionWith(Domain(rhs_offset)), mutable_arg);
  if (some_integer_encoding_were_extracted) {
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateConstraintVariableUsage(ct_index);
  }
}

void CpModelPresolver::ExtractAtMostOneFromLinear(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return;
  if (HasEnforcementLiteral(*ct)) return;
  const Domain rhs = ReadDomainFromProto(ct->linear());

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64_t min_sum = 0;
  int64_t max_sum = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int ref = arg.vars(i);
    const int64_t coeff = arg.coeffs(i);
    const int64_t term_a = coeff * context_->MinOf(ref);
    const int64_t term_b = coeff * context_->MaxOf(ref);
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }
  for (const int type : {0, 1}) {
    std::vector<int> at_most_one;
    for (int i = 0; i < num_vars; ++i) {
      const int ref = arg.vars(i);
      const int64_t coeff = arg.coeffs(i);
      if (context_->MinOf(ref) != 0) continue;
      if (context_->MaxOf(ref) != 1) continue;

      if (type == 0) {
        // TODO(user): we could add one more Boolean with a lower coeff as long
        // as we have lower_coeff + min_of_other_coeff > rhs.Max().
        if (min_sum + 2 * std::abs(coeff) > rhs.Max()) {
          at_most_one.push_back(coeff > 0 ? ref : NegatedRef(ref));
        }
      } else {
        if (max_sum - 2 * std::abs(coeff) < rhs.Min()) {
          at_most_one.push_back(coeff > 0 ? NegatedRef(ref) : ref);
        }
      }
    }
    if (at_most_one.size() > 1) {
      if (type == 0) {
        context_->UpdateRuleStats("linear: extracted at most one (max).");
      } else {
        context_->UpdateRuleStats("linear: extracted at most one (min).");
      }
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      new_ct->set_name(ct->name());
      for (const int ref : at_most_one) {
        new_ct->mutable_at_most_one()->add_literals(ref);
      }
      context_->UpdateNewConstraintsVariableUsage();
    }
  }
}

// Convert some linear constraint involving only Booleans to their Boolean
// form.
bool CpModelPresolver::PresolveLinearOnBooleans(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64_t min_coeff = std::numeric_limits<int64_t>::max();
  int64_t max_coeff = 0;
  int64_t min_sum = 0;
  int64_t max_sum = 0;
  for (int i = 0; i < num_vars; ++i) {
    // We assume we already ran PresolveLinear().
    const int var = arg.vars(i);
    const int64_t coeff = arg.coeffs(i);
    CHECK(RefIsPositive(var));
    CHECK_NE(coeff, 0);
    if (context_->MinOf(var) != 0) return false;
    if (context_->MaxOf(var) != 1) return false;

    if (coeff > 0) {
      max_sum += coeff;
      min_coeff = std::min(min_coeff, coeff);
      max_coeff = std::max(max_coeff, coeff);
    } else {
      // We replace the Boolean ref, by a ref to its negation (1 - x).
      min_sum += coeff;
      min_coeff = std::min(min_coeff, -coeff);
      max_coeff = std::max(max_coeff, -coeff);
    }
  }
  CHECK_LE(min_coeff, max_coeff);

  // Detect trivially true/false constraints. Note that this is not necessarily
  // detected by PresolveLinear(). We do that here because we assume below
  // that this cannot happen.
  //
  // TODO(user): this could be generalized to constraint not containing only
  // Booleans.
  const Domain rhs_domain = ReadDomainFromProto(arg);
  if ((!rhs_domain.Contains(min_sum) &&
       min_sum + min_coeff > rhs_domain.Max()) ||
      (!rhs_domain.Contains(max_sum) &&
       max_sum - min_coeff < rhs_domain.Min())) {
    context_->UpdateRuleStats("linear: all booleans and trivially false");
    return MarkConstraintAsFalse(ct);
  }
  if (Domain(min_sum, max_sum).IsIncludedIn(rhs_domain)) {
    context_->UpdateRuleStats("linear: all booleans and trivially true");
    return RemoveConstraint(ct);
  }

  // Detect clauses, reified ands, at_most_one.
  //
  // TODO(user): split a == 1 constraint or similar into a clause and an at
  // most one constraint?
  DCHECK(!rhs_domain.IsEmpty());
  if (min_sum + min_coeff > rhs_domain.Max()) {
    // All Boolean are false if the reified literal is true.
    context_->UpdateRuleStats("linear: negative reified and");
    const auto copy = arg;
    ct->mutable_bool_and()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_and()->add_literals(
          copy.coeffs(i) > 0 ? NegatedRef(copy.vars(i)) : copy.vars(i));
    }
    PresolveBoolAnd(ct);
    return true;
  } else if (max_sum - min_coeff < rhs_domain.Min()) {
    // All Boolean are true if the reified literal is true.
    context_->UpdateRuleStats("linear: positive reified and");
    const auto copy = arg;
    ct->mutable_bool_and()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_and()->add_literals(
          copy.coeffs(i) > 0 ? copy.vars(i) : NegatedRef(copy.vars(i)));
    }
    PresolveBoolAnd(ct);
    return true;
  } else if (min_sum + min_coeff >= rhs_domain.Min() &&
             rhs_domain.front().end >= max_sum) {
    // At least one Boolean is true.
    context_->UpdateRuleStats("linear: positive clause");
    const auto copy = arg;
    ct->mutable_bool_or()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_or()->add_literals(
          copy.coeffs(i) > 0 ? copy.vars(i) : NegatedRef(copy.vars(i)));
    }
    PresolveBoolOr(ct);
    return true;
  } else if (max_sum - min_coeff <= rhs_domain.Max() &&
             rhs_domain.back().start <= min_sum) {
    // At least one Boolean is false.
    context_->UpdateRuleStats("linear: negative clause");
    const auto copy = arg;
    ct->mutable_bool_or()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_or()->add_literals(
          copy.coeffs(i) > 0 ? NegatedRef(copy.vars(i)) : copy.vars(i));
    }
    PresolveBoolOr(ct);
    return true;
  } else if (!HasEnforcementLiteral(*ct) &&
             min_sum + max_coeff <= rhs_domain.Max() &&
             min_sum + 2 * min_coeff > rhs_domain.Max() &&
             rhs_domain.back().start <= min_sum) {
    // At most one Boolean is true.
    context_->UpdateRuleStats("linear: positive at most one");
    const auto copy = arg;
    ct->mutable_at_most_one()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_at_most_one()->add_literals(
          copy.coeffs(i) > 0 ? copy.vars(i) : NegatedRef(copy.vars(i)));
    }
    return true;
  } else if (!HasEnforcementLiteral(*ct) &&
             max_sum - max_coeff >= rhs_domain.Min() &&
             max_sum - 2 * min_coeff < rhs_domain.Min() &&
             rhs_domain.front().end >= max_sum) {
    // At most one Boolean is false.
    context_->UpdateRuleStats("linear: negative at most one");
    const auto copy = arg;
    ct->mutable_at_most_one()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_at_most_one()->add_literals(
          copy.coeffs(i) > 0 ? NegatedRef(copy.vars(i)) : copy.vars(i));
    }
    return true;
  } else if (!HasEnforcementLiteral(*ct) && rhs_domain.NumIntervals() == 1 &&
             min_sum < rhs_domain.Min() &&
             min_sum + min_coeff >= rhs_domain.Min() &&
             min_sum + 2 * min_coeff > rhs_domain.Max() &&
             min_sum + max_coeff <= rhs_domain.Max()) {
    context_->UpdateRuleStats("linear: positive equal one");
    ConstraintProto* exactly_one = context_->working_model->add_constraints();
    exactly_one->set_name(ct->name());
    for (int i = 0; i < num_vars; ++i) {
      exactly_one->mutable_exactly_one()->add_literals(
          arg.coeffs(i) > 0 ? arg.vars(i) : NegatedRef(arg.vars(i)));
    }
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  } else if (!HasEnforcementLiteral(*ct) && rhs_domain.NumIntervals() == 1 &&
             max_sum > rhs_domain.Max() &&
             max_sum - min_coeff <= rhs_domain.Max() &&
             max_sum - 2 * min_coeff < rhs_domain.Min() &&
             max_sum - max_coeff >= rhs_domain.Min()) {
    context_->UpdateRuleStats("linear: negative equal one");
    ConstraintProto* exactly_one = context_->working_model->add_constraints();
    exactly_one->set_name(ct->name());
    for (int i = 0; i < num_vars; ++i) {
      exactly_one->mutable_exactly_one()->add_literals(
          arg.coeffs(i) > 0 ? NegatedRef(arg.vars(i)) : arg.vars(i));
    }
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Expand small expression into clause.
  //
  // TODO(user): This is bad from a LP relaxation perspective. Do not do that
  // now? On another hand it is good for the SAT presolving.
  if (num_vars > 3) return false;
  context_->UpdateRuleStats("linear: small Boolean expression");

  // Enumerate all possible value of the Booleans and add a clause if constraint
  // is false. TODO(user): the encoding could be made better in some cases.
  const int max_mask = (1 << arg.vars_size());
  for (int mask = 0; mask < max_mask; ++mask) {
    int64_t value = 0;
    for (int i = 0; i < num_vars; ++i) {
      if ((mask >> i) & 1) value += arg.coeffs(i);
    }
    if (rhs_domain.Contains(value)) continue;

    // Add a new clause to exclude this bad assignment.
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    auto* new_arg = new_ct->mutable_bool_or();
    if (HasEnforcementLiteral(*ct)) {
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    }
    for (int i = 0; i < num_vars; ++i) {
      new_arg->add_literals(((mask >> i) & 1) ? NegatedRef(arg.vars(i))
                                              : arg.vars(i));
    }
  }

  context_->UpdateNewConstraintsVariableUsage();
  return RemoveConstraint(ct);
}

namespace {

void AddLinearConstraintFromInterval(const ConstraintProto& ct,
                                     PresolveContext* context) {
  const int start = ct.interval().start();
  const int end = ct.interval().end();
  const int size = ct.interval().size();
  ConstraintProto* new_ct = context->working_model->add_constraints();
  *(new_ct->mutable_enforcement_literal()) = ct.enforcement_literal();
  new_ct->mutable_linear()->add_domain(0);
  new_ct->mutable_linear()->add_domain(0);
  new_ct->mutable_linear()->add_vars(start);
  new_ct->mutable_linear()->add_coeffs(1);
  new_ct->mutable_linear()->add_vars(size);
  new_ct->mutable_linear()->add_coeffs(1);
  new_ct->mutable_linear()->add_vars(end);
  new_ct->mutable_linear()->add_coeffs(-1);
  context->UpdateNewConstraintsVariableUsage();
}

}  // namespace

bool CpModelPresolver::PresolveInterval(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (ct->enforcement_literal().empty() && !ct->interval().has_start_view()) {
    bool changed = false;
    const int start = ct->interval().start();
    const int end = ct->interval().end();
    const int size = ct->interval().size();
    const Domain start_domain = context_->DomainOf(start);
    const Domain end_domain = context_->DomainOf(end);
    const Domain size_domain = context_->DomainOf(size);
    // Size can't be negative.
    if (!context_->IntersectDomainWith(size, Domain(0, context_->MaxOf(size)),
                                       &changed)) {
      return false;
    }
    if (!context_->IntersectDomainWith(
            end, start_domain.AdditionWith(size_domain), &changed)) {
      return false;
    }
    if (!context_->IntersectDomainWith(
            start, end_domain.AdditionWith(size_domain.Negation()), &changed)) {
      return false;
    }
    if (!context_->IntersectDomainWith(
            size, end_domain.AdditionWith(start_domain.Negation()), &changed)) {
      return false;
    }
    if (changed) {
      context_->UpdateRuleStats("interval: reduced domains");
    }
  }

  if (context_->IntervalUsage(c) == 0) {
    if (!ct->interval().has_start_view()) {
      AddLinearConstraintFromInterval(*ct, context_);
    }
    context_->UpdateRuleStats("interval: unused, converted to linear");
    return RemoveConstraint(ct);
  }

  // TODO(user): Note that the conversion is not perfect for optional intervals.
  // because for a fixed size optional interval with a different start and end
  // variable, because of the optionality we will not be able to detect the
  // affine relation between start and end. So we will no remove a variable like
  // we do for non-optional fixed size intervals.
  if (context_->params().convert_intervals()) {
    bool changed = false;
    IntervalConstraintProto* interval = ct->mutable_interval();
    if (!ct->interval().has_start_view()) {
      changed = true;

      // Add a linear constraint. Our new format require a separate linear
      // constraint which allow us to reuse all the propagation code.
      AddLinearConstraintFromInterval(*ct, context_);

      // Fill the view fields.
      interval->mutable_start_view()->add_vars(interval->start());
      interval->mutable_start_view()->add_coeffs(1);
      interval->mutable_start_view()->set_offset(0);
      interval->mutable_size_view()->add_vars(interval->size());
      interval->mutable_size_view()->add_coeffs(1);
      interval->mutable_size_view()->set_offset(0);
      interval->mutable_end_view()->add_vars(interval->end());
      interval->mutable_end_view()->add_coeffs(1);
      interval->mutable_end_view()->set_offset(0);

      // Set the old fields to their default. Not really needed.
      interval->set_start(0);
      interval->set_size(0);
      interval->set_end(0);
    }

    changed |=
        CanonicalizeLinearExpression(*ct, interval->mutable_start_view());
    changed |= CanonicalizeLinearExpression(*ct, interval->mutable_size_view());
    changed |= CanonicalizeLinearExpression(*ct, interval->mutable_end_view());
    return changed;
  }

  // This never change the constraint-variable graph.
  return false;
}

bool CpModelPresolver::PresolveElement(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  const int index_ref = ct->element().index();
  const int target_ref = ct->element().target();

  // TODO(user): think about this once we do have such constraint.
  if (HasEnforcementLiteral(*ct)) return false;

  bool all_constants = true;
  absl::flat_hash_set<int64_t> constant_set;
  bool all_included_in_target_domain = true;

  {
    bool reduced_index_domain = false;
    if (!context_->IntersectDomainWith(index_ref,
                                       Domain(0, ct->element().vars_size() - 1),
                                       &reduced_index_domain)) {
      return false;
    }

    // Filter impossible index values if index == +/- target
    //
    // Note that this must be done before the unique_index/target rule.
    if (PositiveRef(target_ref) == PositiveRef(index_ref)) {
      std::vector<int64_t> possible_indices;
      const Domain& index_domain = context_->DomainOf(index_ref);
      for (const ClosedInterval& interval : index_domain) {
        for (int64_t index_value = interval.start; index_value <= interval.end;
             ++index_value) {
          const int ref = ct->element().vars(index_value);
          const int64_t target_value =
              target_ref == index_ref ? index_value : -index_value;
          if (context_->DomainContains(ref, target_value)) {
            possible_indices.push_back(target_value);
          }
        }
      }
      if (possible_indices.size() < index_domain.Size()) {
        if (!context_->IntersectDomainWith(
                index_ref, Domain::FromValues(possible_indices))) {
          return true;
        }
        context_->UpdateRuleStats(
            "element: reduced index domain when target equals index");
      }
    }

    // Filter possible index values. Accumulate variable domains to build
    // a possible target domain.
    Domain infered_domain;
    const Domain& initial_index_domain = context_->DomainOf(index_ref);
    const Domain& target_domain = context_->DomainOf(target_ref);
    std::vector<int64_t> possible_indices;
    for (const ClosedInterval interval : initial_index_domain) {
      for (int value = interval.start; value <= interval.end; ++value) {
        CHECK_GE(value, 0);
        CHECK_LT(value, ct->element().vars_size());
        const int ref = ct->element().vars(value);
        const Domain& domain = context_->DomainOf(ref);
        if (domain.IntersectionWith(target_domain).IsEmpty()) continue;
        possible_indices.push_back(value);
        if (domain.IsFixed()) {
          constant_set.insert(domain.Min());
        } else {
          all_constants = false;
        }
        if (!domain.IsIncludedIn(target_domain)) {
          all_included_in_target_domain = false;
        }
        infered_domain = infered_domain.UnionWith(domain);
      }
    }
    if (possible_indices.size() < initial_index_domain.Size()) {
      if (!context_->IntersectDomainWith(
              index_ref, Domain::FromValues(possible_indices))) {
        return true;
      }
      context_->UpdateRuleStats("element: reduced index domain");
    }
    bool domain_modified = false;
    if (!context_->IntersectDomainWith(target_ref, infered_domain,
                                       &domain_modified)) {
      return true;
    }
    if (domain_modified) {
      context_->UpdateRuleStats("element: reduced target domain");
    }
  }

  // If the index is fixed, this is a equality constraint.
  if (context_->IsFixed(index_ref)) {
    const int var = ct->element().vars(context_->MinOf(index_ref));
    if (var != target_ref) {
      LinearConstraintProto* const lin =
          context_->working_model->add_constraints()->mutable_linear();
      lin->add_vars(var);
      lin->add_coeffs(-1);
      lin->add_vars(target_ref);
      lin->add_coeffs(1);
      lin->add_domain(0);
      lin->add_domain(0);
      context_->UpdateNewConstraintsVariableUsage();
    }
    context_->UpdateRuleStats("element: fixed index");
    return RemoveConstraint(ct);
  }

  // If the accessible part of the array is made of a single constant value,
  // then we do not care about the index. And, because of the previous target
  // domain reduction, the target is also fixed.
  if (all_constants && constant_set.size() == 1) {
    CHECK(context_->IsFixed(target_ref));
    context_->UpdateRuleStats("element: one value array");
    return RemoveConstraint(ct);
  }

  // Special case when the index is boolean, and the array does not contain
  // variables.
  if (context_->MinOf(index_ref) == 0 && context_->MaxOf(index_ref) == 1 &&
      all_constants) {
    const int64_t v0 = context_->MinOf(ct->element().vars(0));
    const int64_t v1 = context_->MinOf(ct->element().vars(1));

    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_vars(target_ref);
    lin->add_coeffs(1);
    lin->add_vars(index_ref);
    lin->add_coeffs(v0 - v1);
    lin->add_domain(v0);
    lin->add_domain(v0);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("element: linearize constant element of size 2");
    return RemoveConstraint(ct);
  }

  // If the index has a canonical affine representative, rewrite the element.
  const AffineRelation::Relation r_index =
      context_->GetAffineRelation(index_ref);
  if (r_index.representative != index_ref) {
    // Checks the domains are synchronized.
    if (context_->DomainOf(r_index.representative).Size() >
        context_->DomainOf(index_ref).Size()) {
      // Postpone, we will come back later when domains are synchronized.
      return true;
    }
    const int r_ref = r_index.representative;
    const int64_t r_min = context_->MinOf(r_ref);
    const int64_t r_max = context_->MaxOf(r_ref);
    const int array_size = ct->element().vars_size();
    if (r_min != 0) {
      context_->UpdateRuleStats("TODO element: representative has bad domain");
    } else if (r_index.offset >= 0 && r_index.offset < array_size &&
               r_index.offset + r_max * r_index.coeff >= 0 &&
               r_index.offset + r_max * r_index.coeff < array_size) {
      // This will happen eventually when domains are synchronized.
      ElementConstraintProto* const element =
          context_->working_model->add_constraints()->mutable_element();
      for (int64_t v = 0; v <= r_max; ++v) {
        const int64_t scaled_index = v * r_index.coeff + r_index.offset;
        CHECK_GE(scaled_index, 0);
        CHECK_LT(scaled_index, array_size);
        element->add_vars(ct->element().vars(scaled_index));
      }
      element->set_index(r_ref);
      element->set_target(target_ref);

      if (r_index.coeff == 1) {
        context_->UpdateRuleStats("element: shifed index ");
      } else {
        context_->UpdateRuleStats("element: scaled index");
      }
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    }
  }

  const bool unique_index = context_->VariableIsUniqueAndRemovable(index_ref) ||
                            context_->IsFixed(index_ref);
  if (all_constants && unique_index) {
    // This constraint is just here to reduce the domain of the target! We can
    // add it to the mapping_model to reconstruct the index value during
    // postsolve and get rid of it now.
    context_->UpdateRuleStats("element: trivial target domain reduction");
    context_->MarkVariableAsRemoved(index_ref);
    *(context_->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct);
  }

  const bool unique_target =
      context_->VariableIsUniqueAndRemovable(target_ref) ||
      context_->IsFixed(target_ref);
  if (all_included_in_target_domain && unique_target) {
    context_->UpdateRuleStats("element: trivial index domain reduction");
    context_->MarkVariableAsRemoved(target_ref);
    *(context_->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct);
  }

  if (unique_target && !context_->IsFixed(target_ref)) {
    context_->UpdateRuleStats("TODO element: target not used elsewhere");
  }
  if (unique_index) {
    context_->UpdateRuleStats("TODO element: index not used elsewhere");
  }

  return false;
}

bool CpModelPresolver::PresolveTable(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;
  if (ct->table().vars().empty()) {
    context_->UpdateRuleStats("table: empty constraint");
    return RemoveConstraint(ct);
  }

  // Filter the unreachable tuples.
  //
  // TODO(user): this is not supper efficient. Optimize if needed.
  const int num_vars = ct->table().vars_size();
  const int num_tuples = ct->table().values_size() / num_vars;
  std::vector<int64_t> tuple(num_vars);
  std::vector<std::vector<int64_t>> new_tuples;
  new_tuples.reserve(num_tuples);
  std::vector<absl::flat_hash_set<int64_t>> new_domains(num_vars);
  std::vector<AffineRelation::Relation> affine_relations;

  absl::flat_hash_set<int> visited;
  for (const int ref : ct->table().vars()) {
    if (visited.contains(PositiveRef(ref))) {
      context_->UpdateRuleStats("TODO table: duplicate variables");
    } else {
      visited.insert(PositiveRef(ref));
    }
  }

  bool modified_variables = false;
  for (int v = 0; v < num_vars; ++v) {
    const int ref = ct->table().vars(v);
    AffineRelation::Relation r = context_->GetAffineRelation(ref);
    affine_relations.push_back(r);
    if (r.representative != ref) {
      modified_variables = true;
    }
  }

  for (int i = 0; i < num_tuples; ++i) {
    bool delete_row = false;
    std::string tmp;
    for (int j = 0; j < num_vars; ++j) {
      const int ref = ct->table().vars(j);
      int64_t v = ct->table().values(i * num_vars + j);
      const AffineRelation::Relation& r = affine_relations[j];
      if (r.representative != ref) {
        const int64_t inverse_value = (v - r.offset) / r.coeff;
        if (inverse_value * r.coeff + r.offset != v) {
          // Bad rounding.
          delete_row = true;
          break;
        }
        v = inverse_value;
      }
      tuple[j] = v;
      if (!context_->DomainContains(r.representative, v)) {
        delete_row = true;
        break;
      }
    }
    if (delete_row) continue;
    new_tuples.push_back(tuple);
    for (int j = 0; j < num_vars; ++j) {
      const int64_t v = tuple[j];
      new_domains[j].insert(v);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&new_tuples);

  // Update the list of tuples if needed.
  if (new_tuples.size() < num_tuples || modified_variables) {
    ct->mutable_table()->clear_values();
    for (const std::vector<int64_t>& t : new_tuples) {
      for (const int64_t v : t) {
        ct->mutable_table()->add_values(v);
      }
    }
    if (new_tuples.size() < num_tuples) {
      context_->UpdateRuleStats("table: removed rows");
    }
  }

  if (modified_variables) {
    for (int j = 0; j < num_vars; ++j) {
      const AffineRelation::Relation& r = affine_relations[j];
      if (r.representative != ct->table().vars(j)) {
        ct->mutable_table()->set_vars(j, r.representative);
      }
    }
    context_->UpdateRuleStats(
        "table: replace variable by canonical affine one");
  }

  // Nothing more to do for negated tables.
  if (ct->table().negated()) return modified_variables;

  // Filter the variable domains.
  bool changed = false;
  for (int j = 0; j < num_vars; ++j) {
    const int ref = ct->table().vars(j);
    if (!context_->IntersectDomainWith(
            PositiveRef(ref),
            Domain::FromValues(std::vector<int64_t>(new_domains[j].begin(),
                                                    new_domains[j].end())),
            &changed)) {
      return true;
    }
  }
  if (changed) {
    context_->UpdateRuleStats("table: reduced variable domains");
  }
  if (num_vars == 1) {
    // Now that we properly update the domain, we can remove the constraint.
    context_->UpdateRuleStats("table: only one column!");
    return RemoveConstraint(ct);
  }

  // Check that the table is not complete or just here to exclude a few tuples.
  double prod = 1.0;
  for (int j = 0; j < num_vars; ++j) prod *= new_domains[j].size();
  if (prod == new_tuples.size()) {
    context_->UpdateRuleStats("table: all tuples!");
    return RemoveConstraint(ct);
  }

  // Convert to the negated table if we gain a lot of entries by doing so.
  // Note however that currently the negated table do not propagate as much as
  // it could.
  if (new_tuples.size() > 0.7 * prod) {
    // Enumerate all tuples.
    std::vector<std::vector<int64_t>> var_to_values(num_vars);
    for (int j = 0; j < num_vars; ++j) {
      var_to_values[j].assign(new_domains[j].begin(), new_domains[j].end());
    }
    std::vector<std::vector<int64_t>> all_tuples(prod);
    for (int i = 0; i < prod; ++i) {
      all_tuples[i].resize(num_vars);
      int index = i;
      for (int j = 0; j < num_vars; ++j) {
        all_tuples[i][j] = var_to_values[j][index % var_to_values[j].size()];
        index /= var_to_values[j].size();
      }
    }
    gtl::STLSortAndRemoveDuplicates(&all_tuples);

    // Compute the complement of new_tuples.
    std::vector<std::vector<int64_t>> diff(prod - new_tuples.size());
    std::set_difference(all_tuples.begin(), all_tuples.end(),
                        new_tuples.begin(), new_tuples.end(), diff.begin());

    // Negate the constraint.
    ct->mutable_table()->set_negated(!ct->table().negated());
    ct->mutable_table()->clear_values();
    for (const std::vector<int64_t>& t : diff) {
      for (const int64_t v : t) ct->mutable_table()->add_values(v);
    }
    context_->UpdateRuleStats("table: negated");
  }
  return modified_variables;
}

bool CpModelPresolver::PresolveAllDiff(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  AllDifferentConstraintProto& all_diff = *ct->mutable_all_diff();

  bool constraint_has_changed = false;
  for (;;) {
    const int size = all_diff.vars_size();
    if (size == 0) {
      context_->UpdateRuleStats("all_diff: empty constraint");
      return RemoveConstraint(ct);
    }
    if (size == 1) {
      context_->UpdateRuleStats("all_diff: only one variable");
      return RemoveConstraint(ct);
    }

    bool something_was_propagated = false;
    std::vector<int> new_variables;
    for (int i = 0; i < size; ++i) {
      if (!context_->IsFixed(all_diff.vars(i))) {
        new_variables.push_back(all_diff.vars(i));
        continue;
      }

      const int64_t value = context_->MinOf(all_diff.vars(i));
      bool propagated = false;
      for (int j = 0; j < size; ++j) {
        if (i == j) continue;
        if (context_->DomainContains(all_diff.vars(j), value)) {
          if (!context_->IntersectDomainWith(all_diff.vars(j),
                                             Domain(value).Complement())) {
            return true;
          }
          propagated = true;
        }
      }
      if (propagated) {
        context_->UpdateRuleStats("all_diff: propagated fixed variables");
        something_was_propagated = true;
      }
    }

    std::sort(new_variables.begin(), new_variables.end());
    for (int i = 1; i < new_variables.size(); ++i) {
      if (new_variables[i] == new_variables[i - 1]) {
        return context_->NotifyThatModelIsUnsat(
            "Duplicate variable in all_diff");
      }
    }

    if (new_variables.size() < all_diff.vars_size()) {
      all_diff.mutable_vars()->Clear();
      for (const int var : new_variables) {
        all_diff.add_vars(var);
      }
      context_->UpdateRuleStats("all_diff: removed fixed variables");
      something_was_propagated = true;
      constraint_has_changed = true;
      if (new_variables.size() <= 1) continue;
    }

    // Propagate mandatory value if the all diff is actually a permutation.
    CHECK_GE(all_diff.vars_size(), 2);
    Domain domain = context_->DomainOf(all_diff.vars(0));
    for (int i = 1; i < all_diff.vars_size(); ++i) {
      domain = domain.UnionWith(context_->DomainOf(all_diff.vars(i)));
    }
    if (all_diff.vars_size() == domain.Size()) {
      absl::flat_hash_map<int64_t, std::vector<int>> value_to_refs;
      for (const int ref : all_diff.vars()) {
        for (const ClosedInterval& interval : context_->DomainOf(ref)) {
          for (int64_t v = interval.start; v <= interval.end; ++v) {
            value_to_refs[v].push_back(ref);
          }
        }
      }
      bool propagated = false;
      for (const auto& it : value_to_refs) {
        if (it.second.size() == 1 &&
            context_->DomainOf(it.second.front()).Size() > 1) {
          const int ref = it.second.front();
          if (!context_->IntersectDomainWith(ref, Domain(it.first))) {
            return true;
          }
          propagated = true;
        }
      }
      if (propagated) {
        context_->UpdateRuleStats(
            "all_diff: propagated mandatory values in permutation");
        something_was_propagated = true;
      }
    }
    if (!something_was_propagated) break;
  }

  return constraint_has_changed;
}

namespace {

// Returns the sorted list of literals for given bool_or or at_most_one
// constraint.
std::vector<int> GetLiteralsFromSetPPCConstraint(const ConstraintProto& ct) {
  std::vector<int> sorted_literals;
  if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
    for (const int literal : ct.at_most_one().literals()) {
      sorted_literals.push_back(literal);
    }
  } else if (ct.constraint_case() == ConstraintProto::kBoolOr) {
    for (const int literal : ct.bool_or().literals()) {
      sorted_literals.push_back(literal);
    }
  } else if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
    for (const int literal : ct.exactly_one().literals()) {
      sorted_literals.push_back(literal);
    }
  }
  std::sort(sorted_literals.begin(), sorted_literals.end());
  return sorted_literals;
}

// Add the constraint (lhs => rhs) to the given proto. The hash map lhs ->
// bool_and constraint index is used to merge implications with the same lhs.
void AddImplication(int lhs, int rhs, CpModelProto* proto,
                    absl::flat_hash_map<int, int>* ref_to_bool_and) {
  if (ref_to_bool_and->contains(lhs)) {
    const int ct_index = (*ref_to_bool_and)[lhs];
    proto->mutable_constraints(ct_index)->mutable_bool_and()->add_literals(rhs);
  } else if (ref_to_bool_and->contains(NegatedRef(rhs))) {
    const int ct_index = (*ref_to_bool_and)[NegatedRef(rhs)];
    proto->mutable_constraints(ct_index)->mutable_bool_and()->add_literals(
        NegatedRef(lhs));
  } else {
    (*ref_to_bool_and)[lhs] = proto->constraints_size();
    ConstraintProto* ct = proto->add_constraints();
    ct->add_enforcement_literal(lhs);
    ct->mutable_bool_and()->add_literals(rhs);
  }
}

template <typename ClauseContainer>
void ExtractClauses(bool use_bool_and, const ClauseContainer& container,
                    CpModelProto* proto) {
  // We regroup the "implication" into bool_and to have a more consise proto and
  // also for nicer information about the number of binary clauses.
  //
  // Important: however, we do not do that for the model used during presolving
  // since the order of the constraints might be important there depending on
  // how we perform the postsolve.
  absl::flat_hash_map<int, int> ref_to_bool_and;
  for (int i = 0; i < container.NumClauses(); ++i) {
    const std::vector<Literal>& clause = container.Clause(i);
    if (clause.empty()) continue;

    // bool_and.
    if (use_bool_and && clause.size() == 2) {
      const int a = clause[0].IsPositive()
                        ? clause[0].Variable().value()
                        : NegatedRef(clause[0].Variable().value());
      const int b = clause[1].IsPositive()
                        ? clause[1].Variable().value()
                        : NegatedRef(clause[1].Variable().value());
      AddImplication(NegatedRef(a), b, proto, &ref_to_bool_and);
      continue;
    }

    // bool_or.
    ConstraintProto* ct = proto->add_constraints();
    for (const Literal l : clause) {
      if (l.IsPositive()) {
        ct->mutable_bool_or()->add_literals(l.Variable().value());
      } else {
        ct->mutable_bool_or()->add_literals(NegatedRef(l.Variable().value()));
      }
    }
  }
}

}  // namespace

bool CpModelPresolver::PresolveNoOverlap(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  NoOverlapConstraintProto* proto = ct->mutable_no_overlap();
  bool changed = false;

  // Filter absent intervals.
  {
    const int initial_num_intervals = proto->intervals_size();
    int new_size = 0;

    for (int i = 0; i < initial_num_intervals; ++i) {
      const int interval_index = proto->intervals(i);
      if (context_->ConstraintIsInactive(interval_index)) {
        continue;
      }

      proto->set_intervals(new_size++, interval_index);
    }

    if (new_size < initial_num_intervals) {
      proto->mutable_intervals()->Truncate(new_size);
      context_->UpdateRuleStats("no_overlap: removed absent intervals");
      changed = true;
    }
  }

  // Split constraints in disjoint sets.
  if (proto->intervals_size() > 1) {
    std::vector<IndexedInterval> indexed_intervals;
    for (int i = 0; i < proto->intervals().size(); ++i) {
      const int index = proto->intervals(i);
      indexed_intervals.push_back({index,
                                   IntegerValue(context_->StartMin(index)),
                                   IntegerValue(context_->EndMax(index))});
    }
    std::vector<std::vector<int>> components;
    GetOverlappingIntervalComponents(&indexed_intervals, &components);

    if (components.size() > 1) {
      for (const std::vector<int>& intervals : components) {
        if (intervals.size() <= 1) continue;

        NoOverlapConstraintProto* new_no_overlap =
            context_->working_model->add_constraints()->mutable_no_overlap();
        // Fill in the intervals. Unfortunately, the Assign() method does not
        // compile in or-tools.
        for (const int i : intervals) {
          new_no_overlap->add_intervals(i);
        }
      }
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("no_overlap: split into disjoint components");
      return RemoveConstraint(ct);
    }
  }

  std::vector<int> constant_intervals;
  int64_t size_min_of_non_constant_intervals =
      std::numeric_limits<int64_t>::max();
  for (int i = 0; i < proto->intervals_size(); ++i) {
    const int interval_index = proto->intervals(i);
    if (context_->IntervalIsConstant(interval_index)) {
      constant_intervals.push_back(interval_index);
    } else {
      size_min_of_non_constant_intervals =
          std::min(size_min_of_non_constant_intervals,
                   context_->SizeMin(interval_index));
    }
  }

  if (!constant_intervals.empty()) {
    // Sort constant_intervals by start min.
    std::sort(constant_intervals.begin(), constant_intervals.end(),
              [this](int i1, int i2) {
                return context_->StartMin(i1) < context_->StartMin(i2);
              });

    // Check for overlapping constant intervals. We need to check feasibility
    // before we simplify the constraint, as we might remove conflicting
    // overlapping constant intervals.
    for (int i = 0; i + 1 < constant_intervals.size(); ++i) {
      if (context_->EndMax(constant_intervals[i]) >
          context_->StartMin(constant_intervals[i + 1])) {
        context_->UpdateRuleStats("no_overlap: constant intervals overlap");
        return context_->NotifyThatModelIsUnsat();
      }
    }

    if (constant_intervals.size() == proto->intervals_size()) {
      context_->UpdateRuleStats("no_overlap: no variable intervals");
      return RemoveConstraint(ct);
    }

    absl::flat_hash_set<int> intervals_to_remove;

    // If two constant intervals are separated by a gap smaller that the min
    // size of all non-constant intervals, then we can merge them.
    for (int i = 0; i + 1 < constant_intervals.size(); ++i) {
      const int start = i;
      while (i + 1 < constant_intervals.size() &&
             context_->StartMin(constant_intervals[i + 1]) -
                     context_->EndMax(constant_intervals[i]) <
                 size_min_of_non_constant_intervals) {
        i++;
      }
      if (i == start) continue;
      for (int j = start; j <= i; ++j) {
        intervals_to_remove.insert(constant_intervals[j]);
      }
      const int64_t new_start = context_->StartMin(constant_intervals[start]);
      const int64_t new_end = context_->EndMax(constant_intervals[i]);
      proto->add_intervals(context_->working_model->constraints_size());
      IntervalConstraintProto* new_interval =
          context_->working_model->add_constraints()->mutable_interval();
      new_interval->mutable_start_view()->set_offset(new_start);
      new_interval->mutable_size_view()->set_offset(new_end - new_start);
      new_interval->mutable_end_view()->set_offset(new_end);
    }

    // Cleanup the original proto.
    if (!intervals_to_remove.empty()) {
      int new_size = 0;
      const int old_size = proto->intervals_size();
      for (int i = 0; i < old_size; ++i) {
        const int interval_index = proto->intervals(i);
        if (intervals_to_remove.contains(interval_index)) {
          continue;
        }
        proto->set_intervals(new_size++, interval_index);
      }
      CHECK_LT(new_size, old_size);
      proto->mutable_intervals()->Truncate(new_size);
      context_->UpdateRuleStats(
          "no_overlap: merge constant contiguous intervals");
      intervals_to_remove.clear();
      constant_intervals.clear();
      changed = true;
      context_->UpdateNewConstraintsVariableUsage();
    }
  }

  if (proto->intervals_size() == 1) {
    context_->UpdateRuleStats("no_overlap: only one interval");
    return RemoveConstraint(ct);
  }
  if (proto->intervals().empty()) {
    context_->UpdateRuleStats("no_overlap: no intervals");
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpModelPresolver::PresolveNoOverlap2D(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) {
    return false;
  }

  const NoOverlap2DConstraintProto& proto = ct->no_overlap_2d();
  const int initial_num_boxes = proto.x_intervals_size();

  bool has_zero_sizes = false;
  bool x_constant = true;
  bool y_constant = true;

  // Filter absent boxes.
  int new_size = 0;
  std::vector<Rectangle> bounding_boxes;
  std::vector<int> active_boxes;
  for (int i = 0; i < proto.x_intervals_size(); ++i) {
    const int x_interval_index = proto.x_intervals(i);
    const int y_interval_index = proto.y_intervals(i);

    if (context_->ConstraintIsInactive(x_interval_index) ||
        context_->ConstraintIsInactive(y_interval_index)) {
      continue;
    }

    if (proto.boxes_with_null_area_can_overlap() &&
        (context_->SizeMax(x_interval_index) == 0 ||
         context_->SizeMax(y_interval_index) == 0)) {
      if (proto.boxes_with_null_area_can_overlap()) continue;
      has_zero_sizes = true;
    }
    ct->mutable_no_overlap_2d()->set_x_intervals(new_size, x_interval_index);
    ct->mutable_no_overlap_2d()->set_y_intervals(new_size, y_interval_index);
    bounding_boxes.push_back(
        {IntegerValue(context_->StartMin(x_interval_index)),
         IntegerValue(context_->EndMax(x_interval_index)),
         IntegerValue(context_->StartMin(y_interval_index)),
         IntegerValue(context_->EndMax(y_interval_index))});
    active_boxes.push_back(new_size);
    new_size++;

    if (x_constant && !context_->IntervalIsConstant(x_interval_index)) {
      x_constant = false;
    }
    if (y_constant && !context_->IntervalIsConstant(y_interval_index)) {
      y_constant = false;
    }
  }

  std::vector<absl::Span<int>> components = GetOverlappingRectangleComponents(
      bounding_boxes, absl::MakeSpan(active_boxes));
  if (components.size() > 1) {
    for (const absl::Span<int> boxes : components) {
      if (boxes.size() <= 1) continue;

      NoOverlap2DConstraintProto* new_no_overlap_2d =
          context_->working_model->add_constraints()->mutable_no_overlap_2d();
      for (const int b : boxes) {
        new_no_overlap_2d->add_x_intervals(proto.x_intervals(b));
        new_no_overlap_2d->add_y_intervals(proto.y_intervals(b));
      }
    }
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("no_overlap_2d: split into disjoint components");
    return RemoveConstraint(ct);
  }

  if (!has_zero_sizes && (x_constant || y_constant)) {
    context_->UpdateRuleStats(
        "no_overlap_2d: a dimension is constant, splitting into many no "
        "overlaps");
    std::vector<IndexedInterval> indexed_intervals;
    for (int i = 0; i < new_size; ++i) {
      int x = proto.x_intervals(i);
      int y = proto.y_intervals(i);
      if (x_constant) std::swap(x, y);
      indexed_intervals.push_back({x, IntegerValue(context_->StartMin(y)),
                                   IntegerValue(context_->EndMax(y))});
    }
    std::vector<std::vector<int>> no_overlaps;
    ConstructOverlappingSets(/*already_sorted=*/false, &indexed_intervals,
                             &no_overlaps);
    for (const std::vector<int>& no_overlap : no_overlaps) {
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      // Unfortunately, the Assign() method does not work in or-tools as the
      // protobuf int32_t type is not the int type.
      for (const int i : no_overlap) {
        new_ct->mutable_no_overlap()->add_intervals(i);
      }
    }
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  if (new_size < initial_num_boxes) {
    context_->UpdateRuleStats("no_overlap_2d: removed inactive boxes");
    ct->mutable_no_overlap_2d()->mutable_x_intervals()->Truncate(new_size);
    ct->mutable_no_overlap_2d()->mutable_y_intervals()->Truncate(new_size);
  }

  if (new_size == 0) {
    context_->UpdateRuleStats("no_overlap_2d: no boxes");
    return RemoveConstraint(ct);
  }

  if (new_size == 1) {
    context_->UpdateRuleStats("no_overlap_2d: only one box");
    return RemoveConstraint(ct);
  }

  return new_size < initial_num_boxes;
}

bool CpModelPresolver::PresolveCumulative(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  CumulativeConstraintProto* proto = ct->mutable_cumulative();
  bool changed = false;
  int num_fixed_demands = 0;

  {
    // Filter absent intervals, or zero demands.
    int new_size = 0;
    int num_zero_demand_removed = 0;
    int num_zero_size_removed = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      if (context_->ConstraintIsInactive(proto->intervals(i))) continue;

      const int demand_ref = proto->demands(i);
      const int64_t demand_max = context_->MaxOf(demand_ref);
      if (demand_max == 0) {
        num_zero_demand_removed++;
        continue;
      }
      if (context_->IsFixed(demand_ref)) {
        num_fixed_demands++;
      }

      if (context_->SizeMax(proto->intervals(i)) == 0) {
        // Size 0 intervals cannot contribute to a cumulative.
        num_zero_size_removed++;
        continue;
      }

      proto->set_intervals(new_size, proto->intervals(i));
      proto->set_demands(new_size, proto->demands(i));
      new_size++;
    }

    if (new_size < proto->intervals_size()) {
      changed = true;
      proto->mutable_intervals()->Truncate(new_size);
      proto->mutable_demands()->Truncate(new_size);
    }

    if (num_zero_demand_removed > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals with no demands");
    }
    if (num_zero_size_removed > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals with a size of zero");
    }
  }

  // Split constraints in disjoint sets.
  //
  // TODO(user): This can be improved:
  // If we detect bridge nodes in the graph of overlapping components, we
  // can split the graph around the bridge and add the bridge node to both
  // side. Note that if it we take into account precedences between intervals,
  // we can detect more bridges.
  if (proto->intervals_size() > 1) {
    std::vector<IndexedInterval> indexed_intervals;
    for (int i = 0; i < proto->intervals().size(); ++i) {
      const int index = proto->intervals(i);
      indexed_intervals.push_back({i, IntegerValue(context_->StartMin(index)),
                                   IntegerValue(context_->EndMax(index))});
    }
    std::vector<std::vector<int>> components;
    GetOverlappingIntervalComponents(&indexed_intervals, &components);

    if (components.size() > 1) {
      for (const std::vector<int>& component : components) {
        CumulativeConstraintProto* new_cumulative =
            context_->working_model->add_constraints()->mutable_cumulative();
        for (const int i : component) {
          new_cumulative->add_intervals(proto->intervals(i));
          new_cumulative->add_demands(proto->demands(i));
        }
        new_cumulative->set_capacity(proto->capacity());
      }
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("cumulative: split into disjoint components");
      return RemoveConstraint(ct);
    }
  }

  // TODO(user): move the algorithmic part of what we do below in a separate
  // function to unit test it more properly.
  {
    // Build max load profiles.
    std::map<int64_t, int64_t> time_to_demand_deltas;
    const int64_t capacity_min = context_->MinOf(proto->capacity());
    for (int i = 0; i < proto->intervals_size(); ++i) {
      const int interval_index = proto->intervals(i);
      const int64_t demand_max = context_->MaxOf(proto->demands(i));
      time_to_demand_deltas[context_->StartMin(interval_index)] += demand_max;
      time_to_demand_deltas[context_->EndMax(interval_index)] -= demand_max;
    }

    // We construct the profile which correspond to a set of [time, next_time)
    // to max_profile height. And for each time in our discrete set of times
    // (all the start_min and end_max) we count for how often the height was
    // above the capacity before this time.
    //
    // This rely on the iteration in sorted order.
    int num_possible_overloads = 0;
    int64_t current_load = 0;
    absl::flat_hash_map<int64_t, int64_t> num_possible_overloads_before;
    for (const auto& it : time_to_demand_deltas) {
      num_possible_overloads_before[it.first] = num_possible_overloads;
      current_load += it.second;
      if (current_load > capacity_min) {
        ++num_possible_overloads;
      }
    }
    CHECK_EQ(current_load, 0);

    // No possible overload with the min capacity.
    if (num_possible_overloads == 0) {
      context_->UpdateRuleStats(
          "cumulative: max profile is always under the min capacity");
      return RemoveConstraint(ct);
    }

    // An interval that does not intersect with the potential_overload_domains
    // cannot contribute to a conflict. We can safely remove them.
    //
    // This is an extension of the presolve rule from
    // "Presolving techniques and linear relaxations for cumulative scheduling"
    // PhD dissertation by Stefan Heinz, ZIB.
    int new_size = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      const int index = proto->intervals(i);
      const int64_t start_min = context_->StartMin(index);
      const int64_t end_max = context_->EndMax(index);
      DCHECK_LT(start_min, end_max);

      // Note that by construction, both point are in the map. The formula
      // counts exactly for how many times in [start_min, end_max), we have a
      // point in our discrete set of time that exceeded the capacity. Because
      // we included all the relevant points, this works.
      const int num_diff = num_possible_overloads_before.at(end_max) -
                           num_possible_overloads_before.at(start_min);
      if (num_diff == 0) continue;

      proto->set_intervals(new_size, proto->intervals(i));
      proto->set_demands(new_size, proto->demands(i));
      new_size++;
    }

    if (new_size < proto->intervals_size()) {
      changed = true;
      proto->mutable_intervals()->Truncate(new_size);
      proto->mutable_demands()->Truncate(new_size);
      context_->UpdateRuleStats(
          "cumulative: remove never conflicting intervals.");
    }
  }

  if (proto->intervals().empty()) {
    context_->UpdateRuleStats("cumulative: no intervals");
    return RemoveConstraint(ct);
  }

  {
    int64_t max_of_performed_demand_mins = 0;
    int64_t sum_of_max_demands = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      const ConstraintProto& interval_ct =
          context_->working_model->constraints(proto->intervals(i));

      const int demand_ref = proto->demands(i);
      sum_of_max_demands += context_->MaxOf(demand_ref);

      if (interval_ct.enforcement_literal().empty()) {
        max_of_performed_demand_mins =
            std::max(max_of_performed_demand_mins, context_->MinOf(demand_ref));
      }
    }

    const int capacity_ref = proto->capacity();
    if (max_of_performed_demand_mins > context_->MinOf(capacity_ref)) {
      context_->UpdateRuleStats("cumulative: propagate min capacity.");
      if (!context_->IntersectDomainWith(
              capacity_ref, Domain(max_of_performed_demand_mins,
                                   std::numeric_limits<int64_t>::max()))) {
        return true;
      }
    }

    if (max_of_performed_demand_mins > context_->MaxOf(capacity_ref)) {
      context_->UpdateRuleStats("cumulative: cannot fit performed demands");
      return context_->NotifyThatModelIsUnsat();
    }

    if (sum_of_max_demands <= context_->MinOf(capacity_ref)) {
      context_->UpdateRuleStats("cumulative: capacity exceeds sum of demands");
      return RemoveConstraint(ct);
    }
  }

  if (num_fixed_demands == proto->intervals_size() &&
      context_->IsFixed(proto->capacity())) {
    int64_t gcd = 0;
    for (int i = 0; i < ct->cumulative().demands_size(); ++i) {
      const int64_t demand = context_->MinOf(ct->cumulative().demands(i));
      gcd = MathUtil::GCD64(gcd, demand);
      if (gcd == 1) break;
    }
    if (gcd > 1) {
      changed = true;
      for (int i = 0; i < ct->cumulative().demands_size(); ++i) {
        const int64_t demand = context_->MinOf(ct->cumulative().demands(i));
        proto->set_demands(i, context_->GetOrCreateConstantVar(demand / gcd));
      }
      context_->UpdateRuleStats(
          "cumulative: divide demands and capacity by gcd");

      const int64_t old_capacity = context_->MinOf(proto->capacity());
      proto->set_capacity(context_->GetOrCreateConstantVar(old_capacity / gcd));
    }
  }

  if (HasEnforcementLiteral(*ct)) return changed;

  const int num_intervals = proto->intervals_size();
  const int capacity_ref = proto->capacity();
  const int64_t capacity_max = context_->MaxOf(capacity_ref);

  bool with_start_view = false;
  std::vector<int> start_refs(num_intervals, -1);

  int num_duration_one = 0;
  int num_greater_half_capacity = 0;

  bool has_optional_interval = false;
  for (int i = 0; i < num_intervals; ++i) {
    const int index = proto->intervals(i);
    // TODO(user): adapt in the presence of optional intervals.
    if (context_->ConstraintIsOptional(index)) has_optional_interval = true;
    const ConstraintProto& ct =
        context_->working_model->constraints(proto->intervals(i));
    const IntervalConstraintProto& interval = ct.interval();
    if (interval.has_start_view()) with_start_view = true;
    start_refs[i] = interval.start();
    const int demand_ref = proto->demands(i);
    if (context_->SizeMin(index) == 1 && context_->SizeMax(index) == 1) {
      num_duration_one++;
    }
    if (context_->SizeMin(index) == 0) {
      // The behavior for zero-duration interval is currently not the same in
      // the no-overlap and the cumulative constraint.
      return changed;
    }
    const int64_t demand_min = context_->MinOf(demand_ref);
    const int64_t demand_max = context_->MaxOf(demand_ref);
    if (demand_min > capacity_max / 2) {
      num_greater_half_capacity++;
    }
    if (demand_min > capacity_max) {
      context_->UpdateRuleStats("cumulative: demand_min exceeds capacity max");
      if (!context_->ConstraintIsOptional(index)) {
        return context_->NotifyThatModelIsUnsat();
      } else {
        CHECK_EQ(ct.enforcement_literal().size(), 1);
        if (!context_->SetLiteralToFalse(ct.enforcement_literal(0))) {
          return true;
        }
      }
      return changed;
    } else if (demand_max > capacity_max) {
      if (ct.enforcement_literal().empty()) {
        context_->UpdateRuleStats(
            "cumulative: demand_max exceeds capacity max.");
        if (!context_->IntersectDomainWith(
                demand_ref,
                Domain(std::numeric_limits<int64_t>::min(), capacity_max))) {
          return true;
        }
      } else {
        // TODO(user): we abort because we cannot convert this to a no_overlap
        // for instance.
        context_->UpdateRuleStats(
            "cumulative: demand_max of optional interval exceeds capacity.");
        return changed;
      }
    }
  }
  if (num_greater_half_capacity == num_intervals) {
    if (num_duration_one == num_intervals && !has_optional_interval &&
        !with_start_view) {
      context_->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_all_diff();
      for (const int var : start_refs) arg->add_vars(var);
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("cumulative: convert to no_overlap");

      // Before we remove the cumulative, add constraints to enforce that the
      // capacity is greater than the demand of any performed intervals.
      for (int i = 0; i < proto->demands_size(); ++i) {
        const int demand_ref = proto->demands(i);
        const int64_t demand_max = context_->MaxOf(demand_ref);
        if (demand_max > context_->MinOf(capacity_ref)) {
          ConstraintProto* capacity_gt =
              context_->working_model->add_constraints();
          for (const int literal :
               context_->working_model->constraints(proto->intervals(i))
                   .enforcement_literal()) {
            capacity_gt->add_enforcement_literal(literal);
          }
          capacity_gt->mutable_linear()->add_vars(capacity_ref);
          capacity_gt->mutable_linear()->add_coeffs(1);
          capacity_gt->mutable_linear()->add_vars(demand_ref);
          capacity_gt->mutable_linear()->add_coeffs(-1);
          capacity_gt->mutable_linear()->add_domain(0);
          capacity_gt->mutable_linear()->add_domain(
              std::numeric_limits<int64_t>::max());
          context_->UpdateNewConstraintsVariableUsage();
        }
      }

      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_no_overlap();
      for (const int interval : proto->intervals()) {
        arg->add_intervals(interval);
      }
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    }
  }

  return changed;
}

bool CpModelPresolver::PresolveRoutes(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;
  RoutesConstraintProto& proto = *ct->mutable_routes();

  int new_size = 0;
  const int num_arcs = proto.literals_size();
  for (int i = 0; i < num_arcs; ++i) {
    const int ref = proto.literals(i);
    const int tail = proto.tails(i);
    const int head = proto.heads(i);
    if (context_->LiteralIsFalse(ref)) {
      context_->UpdateRuleStats("routes: removed false arcs");
      continue;
    }
    proto.set_literals(new_size, ref);
    proto.set_tails(new_size, tail);
    proto.set_heads(new_size, head);
    ++new_size;
  }
  if (new_size < num_arcs) {
    proto.mutable_literals()->Truncate(new_size);
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    return true;
  }
  return false;
}

bool CpModelPresolver::PresolveCircuit(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;
  CircuitConstraintProto& proto = *ct->mutable_circuit();

  // The indexing might not be dense, so fix that first.
  ReindexArcs(ct->mutable_circuit()->mutable_tails(),
              ct->mutable_circuit()->mutable_heads());

  // Convert the flat structure to a graph, note that we includes all the arcs
  // here (even if they are at false).
  std::vector<std::vector<int>> incoming_arcs;
  std::vector<std::vector<int>> outgoing_arcs;
  int num_nodes = 0;
  const int num_arcs = proto.literals_size();
  for (int i = 0; i < num_arcs; ++i) {
    const int ref = proto.literals(i);
    const int tail = proto.tails(i);
    const int head = proto.heads(i);
    num_nodes = std::max(num_nodes, std::max(tail, head) + 1);
    if (std::max(tail, head) >= incoming_arcs.size()) {
      incoming_arcs.resize(std::max(tail, head) + 1);
      outgoing_arcs.resize(std::max(tail, head) + 1);
    }
    incoming_arcs[head].push_back(ref);
    outgoing_arcs[tail].push_back(ref);
  }

  // Note that it is important to reach the fixed point here:
  // One arc at true, then all other arc at false. This is because we rely
  // on this in case the circuit is fully specified below.
  //
  // TODO(user): Use a better complexity if needed.
  bool loop_again = true;
  int num_fixed_at_true = 0;
  while (loop_again) {
    loop_again = false;
    for (const auto* node_to_refs : {&incoming_arcs, &outgoing_arcs}) {
      for (const std::vector<int>& refs : *node_to_refs) {
        if (refs.size() == 1) {
          if (!context_->LiteralIsTrue(refs.front())) {
            ++num_fixed_at_true;
            if (!context_->SetLiteralToTrue(refs.front())) return true;
          }
          continue;
        }

        // At most one true, so if there is one, mark all the other to false.
        int num_true = 0;
        int true_ref;
        for (const int ref : refs) {
          if (context_->LiteralIsTrue(ref)) {
            ++num_true;
            true_ref = ref;
            break;
          }
        }
        if (num_true > 1) {
          return context_->NotifyThatModelIsUnsat();
        }
        if (num_true == 1) {
          for (const int ref : refs) {
            if (ref != true_ref) {
              if (!context_->IsFixed(ref)) {
                context_->UpdateRuleStats("circuit: set literal to false.");
                loop_again = true;
              }
              if (!context_->SetLiteralToFalse(ref)) return true;
            }
          }
        }
      }
    }
  }
  if (num_fixed_at_true > 0) {
    context_->UpdateRuleStats("circuit: fixed singleton arcs.");
  }

  // Remove false arcs.
  int new_size = 0;
  int num_true = 0;
  int circuit_start = -1;
  std::vector<int> next(num_nodes, -1);
  std::vector<int> new_in_degree(num_nodes, 0);
  std::vector<int> new_out_degree(num_nodes, 0);
  for (int i = 0; i < num_arcs; ++i) {
    const int ref = proto.literals(i);
    if (context_->LiteralIsFalse(ref)) continue;
    if (context_->LiteralIsTrue(ref)) {
      if (next[proto.tails(i)] != -1) {
        return context_->NotifyThatModelIsUnsat();
      }
      next[proto.tails(i)] = proto.heads(i);
      if (proto.tails(i) != proto.heads(i)) {
        circuit_start = proto.tails(i);
      }
      ++num_true;
    }
    ++new_out_degree[proto.tails(i)];
    ++new_in_degree[proto.heads(i)];
    proto.set_tails(new_size, proto.tails(i));
    proto.set_heads(new_size, proto.heads(i));
    proto.set_literals(new_size, proto.literals(i));
    ++new_size;
  }

  // Detect infeasibility due to a node having no more incoming or outgoing arc.
  // This is a bit tricky because for now the meaning of the constraint says
  // that all nodes that appear in at least one of the arcs must be in the
  // circuit or have a self-arc. So if any such node ends up with an incoming or
  // outgoing degree of zero once we remove false arcs then the constraint is
  // infeasible!
  for (int i = 0; i < num_nodes; ++i) {
    // Skip initially ignored node.
    if (incoming_arcs[i].empty() && outgoing_arcs[i].empty()) continue;

    if (new_in_degree[i] == 0 || new_out_degree[i] == 0) {
      return context_->NotifyThatModelIsUnsat();
    }
  }

  // Test if a subcircuit is already present.
  if (circuit_start != -1) {
    std::vector<bool> visited(num_nodes, false);
    int current = circuit_start;
    while (current != -1 && !visited[current]) {
      visited[current] = true;
      current = next[current];
    }
    if (current == circuit_start) {
      // We have a sub-circuit! mark all other arc false except self-loop not in
      // circuit.
      for (int i = 0; i < num_arcs; ++i) {
        if (visited[proto.tails(i)]) continue;
        if (proto.tails(i) == proto.heads(i)) {
          if (!context_->SetLiteralToTrue(proto.literals(i))) return true;
        } else {
          if (!context_->SetLiteralToFalse(proto.literals(i))) return true;
        }
      }
      context_->UpdateRuleStats("circuit: fully specified.");
      return RemoveConstraint(ct);
    }
  } else {
    // All self loop?
    if (num_true == new_size) {
      context_->UpdateRuleStats("circuit: empty circuit.");
      return RemoveConstraint(ct);
    }
  }

  // Look for in/out-degree of two, this will imply that one of the indicator
  // Boolean is equal to the negation of the other.
  for (int i = 0; i < num_nodes; ++i) {
    for (const std::vector<int>* arc_literals :
         {&incoming_arcs[i], &outgoing_arcs[i]}) {
      std::vector<int> literals;
      for (const int ref : *arc_literals) {
        if (context_->LiteralIsFalse(ref)) continue;
        if (context_->LiteralIsTrue(ref)) {
          literals.clear();
          break;
        }
        literals.push_back(ref);
      }
      if (literals.size() == 2 && literals[0] != NegatedRef(literals[1])) {
        context_->UpdateRuleStats("circuit: degree 2");
        context_->StoreBooleanEqualityRelation(literals[0],
                                               NegatedRef(literals[1]));
      }
    }
  }

  // Truncate the circuit and return.
  if (new_size < num_arcs) {
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    proto.mutable_literals()->Truncate(new_size);
    context_->UpdateRuleStats("circuit: removed false arcs.");
    return true;
  }
  return false;
}

bool CpModelPresolver::PresolveAutomaton(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;
  AutomatonConstraintProto& proto = *ct->mutable_automaton();
  if (proto.vars_size() == 0 || proto.transition_label_size() == 0) {
    return false;
  }

  bool all_affine = true;
  std::vector<AffineRelation::Relation> affine_relations;
  for (int v = 0; v < proto.vars_size(); ++v) {
    const int var = ct->automaton().vars(v);
    AffineRelation::Relation r = context_->GetAffineRelation(PositiveRef(var));
    affine_relations.push_back(r);
    if (r.representative == var) {
      all_affine = false;
      break;
    }
    if (v > 0 && (r.coeff != affine_relations[v - 1].coeff ||
                  r.offset != affine_relations[v - 1].offset)) {
      all_affine = false;
      break;
    }
  }

  if (all_affine) {  // Unscale labels.
    for (int v = 0; v < proto.vars_size(); ++v) {
      proto.set_vars(v, affine_relations[v].representative);
    }
    const AffineRelation::Relation rep = affine_relations.front();
    int new_size = 0;
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64_t label = proto.transition_label(t);
      int64_t inverse_label = (label - rep.offset) / rep.coeff;
      if (inverse_label * rep.coeff + rep.offset == label) {
        if (new_size != t) {
          proto.set_transition_tail(new_size, proto.transition_tail(t));
          proto.set_transition_head(new_size, proto.transition_head(t));
        }
        proto.set_transition_label(new_size, inverse_label);
        new_size++;
      }
    }
    if (new_size < proto.transition_tail_size()) {
      proto.mutable_transition_tail()->Truncate(new_size);
      proto.mutable_transition_label()->Truncate(new_size);
      proto.mutable_transition_head()->Truncate(new_size);
      context_->UpdateRuleStats("automaton: remove invalid transitions");
    }
    context_->UpdateRuleStats("automaton: unscale all affine labels");
    return true;
  }

  Domain hull = context_->DomainOf(proto.vars(0));
  for (int v = 1; v < proto.vars_size(); ++v) {
    hull = hull.UnionWith(context_->DomainOf(proto.vars(v)));
  }

  int new_size = 0;
  for (int t = 0; t < proto.transition_tail_size(); ++t) {
    const int64_t label = proto.transition_label(t);
    if (hull.Contains(label)) {
      if (new_size != t) {
        proto.set_transition_tail(new_size, proto.transition_tail(t));
        proto.set_transition_label(new_size, label);
        proto.set_transition_head(new_size, proto.transition_head(t));
      }
      new_size++;
    }
  }
  if (new_size < proto.transition_tail_size()) {
    proto.mutable_transition_tail()->Truncate(new_size);
    proto.mutable_transition_label()->Truncate(new_size);
    proto.mutable_transition_head()->Truncate(new_size);
    context_->UpdateRuleStats("automaton: remove invalid transitions");
    return false;
  }

  const int n = proto.vars_size();
  const std::vector<int> vars = {proto.vars().begin(), proto.vars().end()};

  // Compute the set of reachable state at each time point.
  std::vector<std::set<int64_t>> reachable_states(n + 1);
  reachable_states[0].insert(proto.starting_state());
  reachable_states[n] = {proto.final_states().begin(),
                         proto.final_states().end()};

  // Forward.
  //
  // TODO(user): filter using the domain of vars[time] that may not contain
  // all the possible transitions.
  for (int time = 0; time + 1 < n; ++time) {
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64_t tail = proto.transition_tail(t);
      const int64_t label = proto.transition_label(t);
      const int64_t head = proto.transition_head(t);
      if (!gtl::ContainsKey(reachable_states[time], tail)) continue;
      if (!context_->DomainContains(vars[time], label)) continue;
      reachable_states[time + 1].insert(head);
    }
  }

  std::vector<std::set<int64_t>> reached_values(n);

  // Backward.
  for (int time = n - 1; time >= 0; --time) {
    std::set<int64_t> new_set;
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64_t tail = proto.transition_tail(t);
      const int64_t label = proto.transition_label(t);
      const int64_t head = proto.transition_head(t);

      if (!gtl::ContainsKey(reachable_states[time], tail)) continue;
      if (!context_->DomainContains(vars[time], label)) continue;
      if (!gtl::ContainsKey(reachable_states[time + 1], head)) continue;
      new_set.insert(tail);
      reached_values[time].insert(label);
    }
    reachable_states[time].swap(new_set);
  }

  bool removed_values = false;
  for (int time = 0; time < n; ++time) {
    if (!context_->IntersectDomainWith(
            vars[time],
            Domain::FromValues(
                {reached_values[time].begin(), reached_values[time].end()}),
            &removed_values)) {
      return false;
    }
  }
  if (removed_values) {
    context_->UpdateRuleStats("automaton: reduced variable domains");
  }
  return false;
}

bool CpModelPresolver::PresolveReservoir(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  bool changed = false;

  ReservoirConstraintProto& mutable_reservoir = *ct->mutable_reservoir();
  if (mutable_reservoir.actives().empty()) {
    const int true_literal = context_->GetOrCreateConstantVar(1);
    for (int i = 0; i < mutable_reservoir.times_size(); ++i) {
      mutable_reservoir.add_actives(true_literal);
    }
    changed = true;
  }

  const auto& demand_is_null = [&](int i) {
    return mutable_reservoir.demands(i) == 0 ||
           context_->LiteralIsFalse(mutable_reservoir.actives(i));
  };

  // Remove zero demands, and inactive events.
  int num_zeros = 0;
  for (int i = 0; i < mutable_reservoir.demands_size(); ++i) {
    if (demand_is_null(i)) num_zeros++;
  }

  if (num_zeros > 0) {  // Remove null events
    changed = true;
    int new_size = 0;
    for (int i = 0; i < mutable_reservoir.demands_size(); ++i) {
      if (demand_is_null(i)) continue;
      mutable_reservoir.set_demands(new_size, mutable_reservoir.demands(i));
      mutable_reservoir.set_times(new_size, mutable_reservoir.times(i));
      mutable_reservoir.set_actives(new_size, mutable_reservoir.actives(i));
      new_size++;
    }

    mutable_reservoir.mutable_demands()->Truncate(new_size);
    mutable_reservoir.mutable_times()->Truncate(new_size);
    mutable_reservoir.mutable_actives()->Truncate(new_size);

    context_->UpdateRuleStats(
        "reservoir: remove zero demands or inactive events.");
  }

  const int num_events = mutable_reservoir.demands_size();
  int64_t gcd = mutable_reservoir.demands().empty()
                    ? 0
                    : std::abs(mutable_reservoir.demands(0));
  int num_positives = 0;
  int num_negatives = 0;
  int64_t max_sum_of_positive_demands = 0;
  int64_t min_sum_of_negative_demands = 0;
  for (int i = 0; i < num_events; ++i) {
    const int64_t demand = mutable_reservoir.demands(i);
    gcd = MathUtil::GCD64(gcd, std::abs(demand));
    if (demand > 0) {
      num_positives++;
      max_sum_of_positive_demands += demand;
    } else {
      DCHECK_LT(demand, 0);
      num_negatives++;
      min_sum_of_negative_demands += demand;
    }
  }

  if (min_sum_of_negative_demands >= mutable_reservoir.min_level() &&
      max_sum_of_positive_demands <= mutable_reservoir.max_level()) {
    context_->UpdateRuleStats("reservoir: always feasible");
    return RemoveConstraint(ct);
  }

  if (min_sum_of_negative_demands > mutable_reservoir.max_level() ||
      max_sum_of_positive_demands < mutable_reservoir.min_level()) {
    context_->UpdateRuleStats("reservoir: trivially infeasible");
    return context_->NotifyThatModelIsUnsat();
  }

  if (min_sum_of_negative_demands > mutable_reservoir.min_level()) {
    mutable_reservoir.set_min_level(min_sum_of_negative_demands);
    context_->UpdateRuleStats(
        "reservoir: increase min_level to reachable value");
  }

  if (max_sum_of_positive_demands < mutable_reservoir.max_level()) {
    mutable_reservoir.set_max_level(max_sum_of_positive_demands);
    context_->UpdateRuleStats("reservoir: reduce max_level to reachable value");
  }

  if (mutable_reservoir.min_level() <= 0 &&
      mutable_reservoir.max_level() >= 0 &&
      (num_positives == 0 || num_negatives == 0)) {
    // If all demands have the same sign, and if the initial state is
    // always feasible, we do not care about the order, just the sum.
    auto* const sum =
        context_->working_model->add_constraints()->mutable_linear();
    int64_t fixed_contrib = 0;
    for (int i = 0; i < mutable_reservoir.demands_size(); ++i) {
      const int64_t demand = mutable_reservoir.demands(i);
      DCHECK_NE(demand, 0);

      const int active = mutable_reservoir.actives(i);
      if (RefIsPositive(active)) {
        sum->add_vars(active);
        sum->add_coeffs(demand);
      } else {
        sum->add_vars(PositiveRef(active));
        sum->add_coeffs(-demand);
        fixed_contrib += demand;
      }
    }
    sum->add_domain(mutable_reservoir.min_level() - fixed_contrib);
    sum->add_domain(mutable_reservoir.max_level() - fixed_contrib);
    context_->UpdateRuleStats("reservoir: converted to linear");
    return RemoveConstraint(ct);
  }

  if (gcd > 1) {
    for (int i = 0; i < mutable_reservoir.demands_size(); ++i) {
      mutable_reservoir.set_demands(i, mutable_reservoir.demands(i) / gcd);
    }

    // Adjust min and max levels.
    //   max level is always rounded down.
    //   min level is always rounded up.
    const Domain reduced_domain =
        Domain({mutable_reservoir.min_level(), mutable_reservoir.max_level()})
            .InverseMultiplicationBy(gcd);
    mutable_reservoir.set_min_level(reduced_domain.Min());
    mutable_reservoir.set_max_level(reduced_domain.Max());
    context_->UpdateRuleStats("reservoir: simplify demands and levels by gcd.");
  }

  if (num_positives == 1 && num_negatives > 0) {
    context_->UpdateRuleStats(
        "TODO reservoir: one producer, multiple consumers.");
  }

  absl::flat_hash_set<std::pair<int, int>> time_active_set;
  for (int i = 0; i < mutable_reservoir.demands_size(); ++i) {
    const std::pair<int, int> key = std::make_pair(
        mutable_reservoir.times(i), mutable_reservoir.actives(i));
    if (time_active_set.contains(key)) {
      context_->UpdateRuleStats("TODO reservoir: merge synchronized events.");
      break;
    } else {
      time_active_set.insert(key);
    }
  }

  return changed;
}

// TODO(user): It is probably more efficient to keep all the bool_and in a
// global place during all the presolve, and just output them at the end
// rather than modifying more than once the proto.
void CpModelPresolver::ExtractBoolAnd() {
  absl::flat_hash_map<int, int> ref_to_bool_and;
  const int num_constraints = context_->working_model->constraints_size();
  std::vector<int> to_remove;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (HasEnforcementLiteral(ct)) continue;

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr &&
        ct.bool_or().literals().size() == 2) {
      AddImplication(NegatedRef(ct.bool_or().literals(0)),
                     ct.bool_or().literals(1), context_->working_model,
                     &ref_to_bool_and);
      to_remove.push_back(c);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne &&
        ct.at_most_one().literals().size() == 2) {
      AddImplication(ct.at_most_one().literals(0),
                     NegatedRef(ct.at_most_one().literals(1)),
                     context_->working_model, &ref_to_bool_and);
      to_remove.push_back(c);
      continue;
    }
  }

  context_->UpdateNewConstraintsVariableUsage();
  for (const int c : to_remove) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    CHECK(RemoveConstraint(ct));
    context_->UpdateConstraintVariableUsage(c);
  }
}

void CpModelPresolver::Probe() {
  if (context_->ModelIsUnsat()) return;

  // Update the domain in the current CpModelProto.
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    FillDomainInProto(context_->DomainOf(i),
                      context_->working_model->mutable_variables(i));
  }
  const CpModelProto& model_proto = *(context_->working_model);

  // Load the constraints in a local model.
  //
  // TODO(user): The model we load does not contain affine relations! But
  // ideally we should be able to remove all of them once we allow more complex
  // constraints to contains linear expression.
  //
  // TODO(user): remove code duplication with cp_model_solver. Here we also do
  // not run the heuristic to decide which variable to fully encode.
  //
  // TODO(user): Maybe do not load slow to propagate constraints? for instance
  // we do not use any linear relaxation here.
  Model model;
  model.Register<SolverLogger>(context_->logger());

  // Adapt some of the parameters during this probing phase.
  auto* local_param = model.GetOrCreate<SatParameters>();
  *local_param = context_->params();
  local_param->set_use_implied_bounds(false);

  model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(
      context_->time_limit());
  model.Register<ModelRandomGenerator>(context_->random());
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  encoder->DisableImplicationBetweenLiteral();
  auto* mapping = model.GetOrCreate<CpModelMapping>();

  // Important: Because the model_proto do not contains affine relation or the
  // objective, we cannot call DetectOptionalVariables() ! This might wrongly
  // detect optionality and derive bad conclusion.
  LoadVariables(model_proto, /*view_all_booleans_as_integers=*/false, &model);
  ExtractEncoding(model_proto, &model);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) continue;
    CHECK(LoadConstraint(ct, &model));
    if (sat_solver->IsModelUnsat()) {
      return (void)context_->NotifyThatModelIsUnsat(absl::StrCat(
          "after loading constraint during probing ", ct.ShortDebugString()));
    }
  }
  encoder->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->Propagate()) {
    return (void)context_->NotifyThatModelIsUnsat(
        "during probing initial propagation");
  }

  // Probe.
  //
  // TODO(user): Compute the transitive reduction instead of just the
  // equivalences, and use the newly learned binary clauses?
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* prober = model.GetOrCreate<Prober>();
  prober->ProbeBooleanVariables(/*deterministic_time_limit=*/1.0);
  context_->time_limit()->AdvanceDeterministicTime(
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  if (sat_solver->IsModelUnsat() || !implication_graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat("during probing");
  }

  // Update the presolve context with fixed Boolean variables.
  CHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    const Literal l = sat_solver->LiteralTrail()[i];
    const int var = mapping->GetProtoVariableFromBooleanVariable(l.Variable());
    if (var >= 0) {
      const int ref = l.IsPositive() ? var : NegatedRef(var);
      if (!context_->SetLiteralToTrue(ref)) return;
    }
  }

  const int num_variables = context_->working_model->variables().size();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  for (int var = 0; var < num_variables; ++var) {
    // Restrict IntegerVariable domain.
    // Note that Boolean are already dealt with above.
    if (!mapping->IsBoolean(var)) {
      if (!context_->IntersectDomainWith(
              var,
              integer_trail->InitialVariableDomain(mapping->Integer(var)))) {
        return;
      }
      continue;
    }

    // Add Boolean equivalence relations.
    const Literal l = mapping->Literal(var);
    const Literal r = implication_graph->RepresentativeOf(l);
    if (r != l) {
      const int r_var =
          mapping->GetProtoVariableFromBooleanVariable(r.Variable());
      CHECK_GE(r_var, 0);
      context_->StoreBooleanEqualityRelation(
          var, r.IsPositive() ? r_var : NegatedRef(r_var));
    }
  }
}

// TODO(user): What to do with the at_most_one/exactly_one constraints?
// currently we do not take them into account here.
void CpModelPresolver::PresolvePureSatPart() {
  // TODO(user,user): Reenable some SAT presolve with
  // keep_all_feasible_solutions set to true.
  if (context_->ModelIsUnsat() || context_->keep_all_feasible_solutions) return;

  const int num_variables = context_->working_model->variables_size();
  SatPostsolver sat_postsolver(num_variables);
  SatPresolver sat_presolver(&sat_postsolver, logger_);
  sat_presolver.SetNumVariables(num_variables);
  sat_presolver.SetTimeLimit(context_->time_limit());

  SatParameters params = context_->params();

  // The "full solver" postsolve does not support changing the value of a
  // variable from the solution of the presolved problem, and we do need this
  // for blocked clause. It should be possible to allow for this by adding extra
  // variable to the mapping model at presolve and some linking constraints, but
  // this is messy.
  if (params.cp_model_postsolve_with_full_solver()) {
    params.set_presolve_blocked_clause(false);
  }

  // TODO(user): BVA takes times and do not seems to help on the minizinc
  // benchmarks. That said, it was useful on pure sat problems, so we may
  // want to enable it.
  params.set_presolve_use_bva(false);
  sat_presolver.SetParameters(params);

  // Converts a cp_model literal ref to a sat::Literal used by SatPresolver.
  absl::flat_hash_set<int> used_variables;
  auto convert = [&used_variables](int ref) {
    used_variables.insert(PositiveRef(ref));
    if (RefIsPositive(ref)) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };

  // We need all Boolean constraints to be presolved before loading them below.
  // Otherwise duplicate literals might result in a wrong outcome.
  //
  // TODO(user): Be a bit more efficient, and enforce this invariant before we
  // reach this point?
  for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr ||
        ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolAnd) {
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return;
    }
  }

  // Load all Clauses into the presolver and remove them from the current model.
  //
  // TODO(user): The removing and adding back of the same clause when nothing
  // happens in the presolve "seems" bad. That said, complexity wise, it is
  // a lot faster that what happens in the presolve though.
  //
  // TODO(user): Add the "small" at most one constraints to the SAT presolver by
  // expanding them to implications? that could remove a lot of clauses. Do that
  // when we are sure we don't load duplicates at_most_one/implications in the
  // solver. Ideally, the pure sat presolve could be improved to handle at most
  // one, and we could merge this with what the ProcessSetPPC() is doing.
  std::vector<Literal> clause;
  int num_removed_constraints = 0;
  for (int i = 0; i < context_->working_model->constraints_size(); ++i) {
    const ConstraintProto& ct = context_->working_model->constraints(i);

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      ++num_removed_constraints;
      clause.clear();
      for (const int ref : ct.bool_or().literals()) {
        clause.push_back(convert(ref));
      }
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(convert(ref).Negated());
      }
      sat_presolver.AddClause(clause);

      context_->working_model->mutable_constraints(i)->Clear();
      context_->UpdateConstraintVariableUsage(i);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolAnd) {
      ++num_removed_constraints;
      std::vector<Literal> clause;
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(convert(ref).Negated());
      }
      clause.push_back(Literal(kNoLiteralIndex));  // will be replaced below.
      for (const int ref : ct.bool_and().literals()) {
        clause.back() = convert(ref);
        sat_presolver.AddClause(clause);
      }

      context_->working_model->mutable_constraints(i)->Clear();
      context_->UpdateConstraintVariableUsage(i);
      continue;
    }
  }

  // Abort early if there was no Boolean constraints.
  if (num_removed_constraints == 0) return;

  // Mark the variables appearing elsewhere or in the objective as non-removable
  // by the sat presolver.
  //
  // TODO(user): do not remove variable that appear in the decision heuristic?
  // TODO(user): We could go further for variable with only one polarity by
  // removing variable from the objective if they can be set to their "low"
  // objective value, and also removing enforcement literal that can be set to
  // false and don't appear elsewhere.
  std::vector<bool> can_be_removed(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    if (context_->VarToConstraints(i).empty()) {
      can_be_removed[i] = true;
    }

    // Because we might not have reached the presove "fixed point" above, some
    // variable in the added clauses might be fixed. We need to indicate this to
    // the SAT presolver.
    if (used_variables.contains(i) && context_->IsFixed(i)) {
      if (context_->LiteralIsTrue(i)) {
        sat_presolver.AddClause({convert(i)});
      } else {
        sat_presolver.AddClause({convert(NegatedRef(i))});
      }
    }
  }

  // Run the presolve for a small number of passes.
  // TODO(user): Add probing like we do in the pure sat solver presolve loop?
  // TODO(user): Add a time limit, this can be slow on big SAT problem.
  const int num_passes = params.presolve_use_bva() ? 4 : 1;
  for (int i = 0; i < num_passes; ++i) {
    const int old_num_clause = sat_postsolver.NumClauses();
    if (!sat_presolver.Presolve(can_be_removed)) {
      VLOG(1) << "UNSAT during SAT presolve.";
      return (void)context_->NotifyThatModelIsUnsat();
    }
    if (old_num_clause == sat_postsolver.NumClauses()) break;
  }

  // Add any new variables to our internal structure.
  const int new_num_variables = sat_presolver.NumVariables();
  if (new_num_variables > context_->working_model->variables_size()) {
    VLOG(1) << "New variables added by the SAT presolver.";
    for (int i = context_->working_model->variables_size();
         i < new_num_variables; ++i) {
      IntegerVariableProto* var_proto =
          context_->working_model->add_variables();
      var_proto->add_domain(0);
      var_proto->add_domain(1);
    }
    context_->InitializeNewDomains();
  }

  // Add the presolver clauses back into the model.
  ExtractClauses(/*use_bool_and=*/true, sat_presolver, context_->working_model);

  // Update the constraints <-> variables graph.
  context_->UpdateNewConstraintsVariableUsage();

  // Add the sat_postsolver clauses to mapping_model.
  //
  // TODO(user): Mark removed variable as removed to detect any potential bugs.
  ExtractClauses(/*use_bool_and=*/false, sat_postsolver,
                 context_->mapping_model);
}

// TODO(user): The idea behind this was that it is better to have an objective
// as spreaded as possible. However on some problems this have the opposite
// effect. Like on a triangular matrix where each expansion reduced the size
// of the objective by one. Investigate and fix?
void CpModelPresolver::ExpandObjective() {
  if (context_->ModelIsUnsat()) return;

  // The objective is already loaded in the constext, but we re-canonicalize
  // it with the latest information.
  if (!context_->CanonicalizeObjective()) {
    (void)context_->NotifyThatModelIsUnsat();
    return;
  }

  if (context_->time_limit()->LimitReached()) {
    context_->WriteObjectiveToProto();
    return;
  }

  // If the objective is a single variable, then we can usually remove this
  // variable if it is only used in one linear equality constraint and we do
  // just one expansion. This is because the domain of the variable will be
  // transferred to our objective_domain.
  int unique_expanded_constraint = -1;
  const bool objective_was_a_single_variable =
      context_->ObjectiveMap().size() == 1;

  // To avoid a bad complexity, we need to compute the number of relevant
  // constraints for each variables.
  const int num_variables = context_->working_model->variables_size();
  const int num_constraints = context_->working_model->constraints_size();
  absl::flat_hash_set<int> relevant_constraints;
  std::vector<int> var_to_num_relevant_constraints(num_variables, 0);
  for (int ct_index = 0; ct_index < num_constraints; ++ct_index) {
    const ConstraintProto& ct = context_->working_model->constraints(ct_index);
    // Skip everything that is not a linear equality constraint.
    if (!ct.enforcement_literal().empty() ||
        ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
        ct.linear().domain().size() != 2 ||
        ct.linear().domain(0) != ct.linear().domain(1)) {
      continue;
    }

    relevant_constraints.insert(ct_index);
    const int num_terms = ct.linear().vars_size();
    for (int i = 0; i < num_terms; ++i) {
      var_to_num_relevant_constraints[PositiveRef(ct.linear().vars(i))]++;
    }
  }

  std::set<int> var_to_process;
  for (const auto entry : context_->ObjectiveMap()) {
    const int var = entry.first;
    CHECK(RefIsPositive(var));
    if (var_to_num_relevant_constraints[var] != 0) {
      var_to_process.insert(var);
    }
  }

  // We currently never expand a variable more than once.
  int num_expansions = 0;
  absl::flat_hash_set<int> processed_vars;
  std::vector<int> new_vars_in_objective;
  while (!relevant_constraints.empty()) {
    // Find a not yet expanded var.
    int objective_var = -1;
    while (!var_to_process.empty()) {
      const int var = *var_to_process.begin();
      CHECK(!processed_vars.contains(var));
      if (var_to_num_relevant_constraints[var] == 0) {
        processed_vars.insert(var);
        var_to_process.erase(var);
        continue;
      }
      if (!context_->ObjectiveMap().contains(var)) {
        // We do not mark it as processed in case it re-appear later.
        var_to_process.erase(var);
        continue;
      }
      objective_var = var;
      break;
    }

    if (objective_var == -1) break;
    CHECK(RefIsPositive(objective_var));
    processed_vars.insert(objective_var);
    var_to_process.erase(objective_var);

    int expanded_linear_index = -1;
    int64_t objective_coeff_in_expanded_constraint;
    int64_t size_of_expanded_constraint = 0;
    const auto& non_deterministic_list =
        context_->VarToConstraints(objective_var);
    std::vector<int> constraints_with_objective(non_deterministic_list.begin(),
                                                non_deterministic_list.end());
    std::sort(constraints_with_objective.begin(),
              constraints_with_objective.end());
    for (const int ct_index : constraints_with_objective) {
      if (relevant_constraints.count(ct_index) == 0) continue;
      const ConstraintProto& ct =
          context_->working_model->constraints(ct_index);

      // This constraint is relevant now, but it will never be later because
      // it will contain the objective_var which is already processed!
      relevant_constraints.erase(ct_index);
      const int num_terms = ct.linear().vars_size();
      for (int i = 0; i < num_terms; ++i) {
        var_to_num_relevant_constraints[PositiveRef(ct.linear().vars(i))]--;
      }

      // Find the coefficient of objective_var in this constraint, and perform
      // various checks.
      //
      // TODO(user): This can crash the program if for some reason the linear
      // constraint was not canonicalized and contains the objective variable
      // twice. Currently this can only happen if the time limit was reached
      // before all constraints where processed, but because we abort at the
      // beginning of the function when this is the case we should be safe.
      // However, it might be more robust to just handle this case properly.
      bool is_present = false;
      int64_t objective_coeff;
      for (int i = 0; i < num_terms; ++i) {
        const int ref = ct.linear().vars(i);
        const int64_t coeff = ct.linear().coeffs(i);
        if (PositiveRef(ref) == objective_var) {
          CHECK(!is_present) << "Duplicate variables not supported.";
          is_present = true;
          objective_coeff = (ref == objective_var) ? coeff : -coeff;
        } else {
          // This is not possible since we only consider relevant constraints.
          CHECK(!processed_vars.contains(PositiveRef(ref)));
        }
      }
      CHECK(is_present);

      // We use the longest equality we can find.
      //
      // TODO(user): Deal with objective_coeff with a magnitude greater than
      // 1? This will only be possible if we change the objective coeff type
      // to double.
      if (std::abs(objective_coeff) == 1 &&
          num_terms > size_of_expanded_constraint) {
        expanded_linear_index = ct_index;
        size_of_expanded_constraint = num_terms;
        objective_coeff_in_expanded_constraint = objective_coeff;
      }
    }

    if (expanded_linear_index != -1) {
      context_->UpdateRuleStats("objective: expanded objective constraint.");

      // Update the objective map. Note that the division is possible because
      // currently we only expand with coeff with a magnitude of 1.
      CHECK_EQ(std::abs(objective_coeff_in_expanded_constraint), 1);
      const ConstraintProto& ct =
          context_->working_model->constraints(expanded_linear_index);
      context_->SubstituteVariableInObjective(
          objective_var, objective_coeff_in_expanded_constraint, ct,
          &new_vars_in_objective);

      // Add not yet processed new variables.
      for (const int var : new_vars_in_objective) {
        if (!processed_vars.contains(var)) var_to_process.insert(var);
      }

      // If the objective variable wasn't used in other constraints and it can
      // be reconstructed whatever the value of the other variables, we can
      // remove the constraint.
      //
      // TODO(user): It should be possible to refactor the code so this is
      // automatically done by the linear constraint singleton presolve rule.
      if (context_->VarToConstraints(objective_var).size() == 1 &&
          !context_->keep_all_feasible_solutions) {
        // Compute implied domain on objective_var.
        Domain implied_domain = ReadDomainFromProto(ct.linear());
        for (int i = 0; i < size_of_expanded_constraint; ++i) {
          const int ref = ct.linear().vars(i);
          if (PositiveRef(ref) == objective_var) continue;
          implied_domain =
              implied_domain
                  .AdditionWith(context_->DomainOf(ref).MultiplicationBy(
                      -ct.linear().coeffs(i)))
                  .RelaxIfTooComplex();
        }
        implied_domain = implied_domain.InverseMultiplicationBy(
            objective_coeff_in_expanded_constraint);

        // Remove the constraint if the implied domain is included in the
        // domain of the objective_var term.
        if (implied_domain.IsIncludedIn(context_->DomainOf(objective_var))) {
          context_->UpdateRuleStats("objective: removed objective constraint.");
          *(context_->mapping_model->add_constraints()) = ct;
          context_->working_model->mutable_constraints(expanded_linear_index)
              ->Clear();
          context_->UpdateConstraintVariableUsage(expanded_linear_index);
        } else {
          unique_expanded_constraint = expanded_linear_index;
        }
      }

      ++num_expansions;
    }
  }

  // Special case: If we just did one expansion of a single variable, then we
  // can remove the expanded constraints if the objective wasn't used elsewhere.
  if (num_expansions == 1 && objective_was_a_single_variable &&
      unique_expanded_constraint != -1) {
    context_->UpdateRuleStats(
        "objective: removed unique objective constraint.");
    ConstraintProto* mutable_ct = context_->working_model->mutable_constraints(
        unique_expanded_constraint);
    *(context_->mapping_model->add_constraints()) = *mutable_ct;
    mutable_ct->Clear();
    context_->UpdateConstraintVariableUsage(unique_expanded_constraint);
  }

  // We re-do a canonicalization with the final linear expression.
  if (!context_->CanonicalizeObjective()) {
    (void)context_->NotifyThatModelIsUnsat();
    return;
  }
  context_->WriteObjectiveToProto();
}

void CpModelPresolver::MergeNoOverlapConstraints() {
  if (context_->ModelIsUnsat()) return;

  const int num_constraints = context_->working_model->constraints_size();
  int old_num_no_overlaps = 0;
  int old_num_intervals = 0;

  // Extract the no-overlap constraints.
  std::vector<int> disjunctive_index;
  std::vector<std::vector<Literal>> cliques;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kNoOverlap) {
      continue;
    }
    std::vector<Literal> clique;
    for (const int i : ct.no_overlap().intervals()) {
      clique.push_back(Literal(BooleanVariable(i), true));
    }
    cliques.push_back(clique);
    disjunctive_index.push_back(c);

    old_num_no_overlaps++;
    old_num_intervals += clique.size();
  }
  if (old_num_no_overlaps == 0) return;

  // We reuse the max-clique code from sat.
  Model local_model;
  local_model.GetOrCreate<Trail>()->Resize(num_constraints);
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(num_constraints);
  for (const std::vector<Literal>& clique : cliques) {
    // All variables at false is always a valid solution of the local model,
    // so this should never return UNSAT.
    CHECK(graph->AddAtMostOne(clique));
  }
  CHECK(graph->DetectEquivalences());
  graph->TransformIntoMaxCliques(
      &cliques, context_->params().merge_no_overlap_work_limit());

  // Replace each no-overlap with an extended version, or remove if empty.
  int new_num_no_overlaps = 0;
  int new_num_intervals = 0;
  for (int i = 0; i < cliques.size(); ++i) {
    const int ct_index = disjunctive_index[i];
    ConstraintProto* ct =
        context_->working_model->mutable_constraints(ct_index);
    ct->Clear();
    if (cliques[i].empty()) continue;
    for (const Literal l : cliques[i]) {
      CHECK(l.IsPositive());
      ct->mutable_no_overlap()->add_intervals(l.Variable().value());
    }
    new_num_no_overlaps++;
    new_num_intervals += cliques[i].size();
  }
  if (old_num_intervals != new_num_intervals ||
      old_num_no_overlaps != new_num_no_overlaps) {
    VLOG(1) << absl::StrCat("Merged ", old_num_no_overlaps, " no-overlaps (",
                            old_num_intervals, " intervals) into ",
                            new_num_no_overlaps, " no-overlaps (",
                            new_num_intervals, " intervals).");
    context_->UpdateRuleStats("no_overlap: merged constraints");
  }
}

// TODO(user): Should we take into account the exactly_one constraints? note
// that such constraint cannot be extended. If if a literal implies two literals
// at one inside an exactly one constraint then it must be false. Similarly if
// it implies all literals at zero inside the exactly one.
void CpModelPresolver::TransformIntoMaxCliques() {
  if (context_->ModelIsUnsat()) return;

  auto convert = [](int ref) {
    if (RefIsPositive(ref)) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };
  const int num_constraints = context_->working_model->constraints_size();

  // Extract the bool_and and at_most_one constraints.
  std::vector<std::vector<Literal>> cliques;

  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne) {
      std::vector<Literal> clique;
      for (const int ref : ct->at_most_one().literals()) {
        clique.push_back(convert(ref));
      }
      cliques.push_back(clique);
      if (RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    } else if (ct->constraint_case() ==
               ConstraintProto::ConstraintCase::kBoolAnd) {
      if (ct->enforcement_literal().size() != 1) continue;
      const Literal enforcement = convert(ct->enforcement_literal(0));
      for (const int ref : ct->bool_and().literals()) {
        if (ref == ct->enforcement_literal(0)) continue;
        cliques.push_back({enforcement, convert(ref).Negated()});
      }
      if (RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }

  int64_t num_literals_before = 0;
  const int num_old_cliques = cliques.size();

  // We reuse the max-clique code from sat.
  Model local_model;
  const int num_variables = context_->working_model->variables().size();
  local_model.GetOrCreate<Trail>()->Resize(num_variables);
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(num_variables);
  for (const std::vector<Literal>& clique : cliques) {
    num_literals_before += clique.size();
    if (!graph->AddAtMostOne(clique)) {
      return (void)context_->NotifyThatModelIsUnsat();
    }
  }
  if (!graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat();
  }
  graph->TransformIntoMaxCliques(
      &cliques, context_->params().merge_at_most_one_work_limit());

  // Add the Boolean variable equivalence detected by DetectEquivalences().
  // Those are needed because TransformIntoMaxCliques() will replace all
  // variable by its representative.
  for (int var = 0; var < num_variables; ++var) {
    const Literal l = Literal(BooleanVariable(var), true);
    if (graph->RepresentativeOf(l) != l) {
      const Literal r = graph->RepresentativeOf(l);
      context_->StoreBooleanEqualityRelation(
          var, r.IsPositive() ? r.Variable().value()
                              : NegatedRef(r.Variable().value()));
    }
  }

  int num_new_cliques = 0;
  int64_t num_literals_after = 0;
  for (const std::vector<Literal>& clique : cliques) {
    if (clique.empty()) continue;
    num_new_cliques++;
    num_literals_after += clique.size();
    ConstraintProto* ct = context_->working_model->add_constraints();
    for (const Literal literal : clique) {
      if (literal.IsPositive()) {
        ct->mutable_at_most_one()->add_literals(literal.Variable().value());
      } else {
        ct->mutable_at_most_one()->add_literals(
            NegatedRef(literal.Variable().value()));
      }
    }

    // Make sure we do not have duplicate variable reference.
    PresolveAtMostOne(ct);
  }
  context_->UpdateNewConstraintsVariableUsage();
  if (num_new_cliques != num_old_cliques) {
    context_->UpdateRuleStats("at_most_one: transformed into max clique.");
  }

  if (num_old_cliques != num_new_cliques ||
      num_literals_before != num_literals_after) {
    SOLVER_LOG(logger_, "[MaxClique] Merged ", num_old_cliques, "(",
               num_literals_before, " literals) into ", num_new_cliques, "(",
               num_literals_after, " literals) at_most_ones.");
  }
}

bool CpModelPresolver::PresolveOneConstraint(int c) {
  if (context_->ModelIsUnsat()) return false;
  ConstraintProto* ct = context_->working_model->mutable_constraints(c);

  // Generic presolve to exploit variable/literal equivalence.
  if (ExploitEquivalenceRelations(c, ct)) {
    context_->UpdateConstraintVariableUsage(c);
  }

  // Generic presolve for reified constraint.
  if (PresolveEnforcementLiteral(ct)) {
    context_->UpdateConstraintVariableUsage(c);
  }

  // Call the presolve function for this constraint if any.
  switch (ct->constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      return PresolveBoolOr(ct);
    case ConstraintProto::ConstraintCase::kBoolAnd:
      return PresolveBoolAnd(ct);
    case ConstraintProto::ConstraintCase::kAtMostOne:
      return PresolveAtMostOne(ct);
    case ConstraintProto::ConstraintCase::kExactlyOne:
      return PresolveExactlyOne(ct);
    case ConstraintProto::ConstraintCase::kBoolXor:
      return PresolveBoolXor(ct);
    case ConstraintProto::ConstraintCase::kIntMax:
      if (ct->int_max().vars_size() == 2 &&
          NegatedRef(ct->int_max().vars(0)) == ct->int_max().vars(1)) {
        return PresolveIntAbs(ct);
      } else {
        return PresolveIntMax(ct);
      }
    case ConstraintProto::ConstraintCase::kIntMin:
      return PresolveIntMin(ct);
    case ConstraintProto::ConstraintCase::kLinMax:
      return PresolveLinMax(ct);
    case ConstraintProto::ConstraintCase::kLinMin:
      return PresolveLinMin(ct);
    case ConstraintProto::ConstraintCase::kIntProd:
      return PresolveIntProd(ct);
    case ConstraintProto::ConstraintCase::kIntDiv:
      return PresolveIntDiv(ct);
    case ConstraintProto::ConstraintCase::kLinear: {
      if (CanonicalizeLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PresolveSmallLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PropagateDomainsInLinear(c, ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PresolveSmallLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      // We first propagate the domains before calling this presolve rule.
      if (RemoveSingletonInLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);

        // There is no need to re-do a propagation here, but the constraint
        // size might have been reduced.
        if (PresolveSmallLinear(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
      }
      if (PresolveLinearOnBooleans(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (ct->constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
        const int old_num_enforcement_literals = ct->enforcement_literal_size();
        ExtractEnforcementLiteralFromLinearConstraint(c, ct);
        if (ct->constraint_case() ==
            ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
          context_->UpdateConstraintVariableUsage(c);
          return true;
        }
        if (ct->enforcement_literal_size() > old_num_enforcement_literals &&
            PresolveSmallLinear(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        PresolveLinearEqualityModuloTwo(ct);
      }
      return false;
    }
    case ConstraintProto::ConstraintCase::kInterval:
      return PresolveInterval(c, ct);
    case ConstraintProto::ConstraintCase::kElement:
      return PresolveElement(ct);
    case ConstraintProto::ConstraintCase::kTable:
      return PresolveTable(ct);
    case ConstraintProto::ConstraintCase::kAllDiff:
      return PresolveAllDiff(ct);
    case ConstraintProto::ConstraintCase::kNoOverlap:
      return PresolveNoOverlap(ct);
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      return PresolveNoOverlap2D(c, ct);
    case ConstraintProto::ConstraintCase::kCumulative:
      return PresolveCumulative(ct);
    case ConstraintProto::ConstraintCase::kCircuit:
      return PresolveCircuit(ct);
    case ConstraintProto::ConstraintCase::kRoutes:
      return PresolveRoutes(ct);
    case ConstraintProto::ConstraintCase::kAutomaton:
      return PresolveAutomaton(ct);
    case ConstraintProto::ConstraintCase::kReservoir:
      return PresolveReservoir(ct);
    default:
      return false;
  }
}

// We deal with all the all 3 x 3 possible types of inclusion.
//
// Returns false iff the model is UNSAT.
bool CpModelPresolver::ProcessSetPPCSubset(
    int c1, int c2, const std::vector<int>& c2_minus_c1,
    const std::vector<int>& original_constraint_index,
    std::vector<bool>* marked_for_removal) {
  if (context_->ModelIsUnsat()) return false;

  CHECK(!(*marked_for_removal)[c1]);
  CHECK(!(*marked_for_removal)[c2]);

  ConstraintProto* ct1 = context_->working_model->mutable_constraints(
      original_constraint_index[c1]);
  ConstraintProto* ct2 = context_->working_model->mutable_constraints(
      original_constraint_index[c2]);

  if ((ct1->constraint_case() == ConstraintProto::kBoolOr ||
       ct1->constraint_case() == ConstraintProto::kExactlyOne) &&
      (ct2->constraint_case() == ConstraintProto::kAtMostOne ||
       ct2->constraint_case() == ConstraintProto::kExactlyOne)) {
    context_->UpdateRuleStats("setppc: bool_or in at_most_one.");

    // Fix extras in c2 to 0, note that these will be removed from the
    // constraint later.
    for (const int literal : c2_minus_c1) {
      if (!context_->SetLiteralToFalse(literal)) return false;
      context_->UpdateRuleStats("setppc: fixed variables");
    }

    // Change c2 to exactly_one if not already.
    if (ct2->constraint_case() != ConstraintProto::kExactlyOne) {
      ConstraintProto copy = *ct2;
      (*ct2->mutable_exactly_one()->mutable_literals()) =
          copy.at_most_one().literals();
    }

    // Remove c1.
    (*marked_for_removal)[c1] = true;
    ct1->Clear();
    context_->UpdateConstraintVariableUsage(original_constraint_index[c1]);
    return true;
  }

  if ((ct1->constraint_case() == ConstraintProto::kBoolOr ||
       ct1->constraint_case() == ConstraintProto::kExactlyOne) &&
      ct2->constraint_case() == ConstraintProto::kBoolOr) {
    context_->UpdateRuleStats("setppc: removed dominated constraints");

    (*marked_for_removal)[c2] = true;
    ct2->Clear();
    context_->UpdateConstraintVariableUsage(original_constraint_index[c2]);
    return true;
  }

  if (ct1->constraint_case() == ConstraintProto::kAtMostOne &&
      (ct2->constraint_case() == ConstraintProto::kAtMostOne ||
       ct2->constraint_case() == ConstraintProto::kExactlyOne)) {
    context_->UpdateRuleStats("setppc: removed dominated constraints");
    (*marked_for_removal)[c1] = true;
    ct1->Clear();
    context_->UpdateConstraintVariableUsage(original_constraint_index[c1]);
    return true;
  }

  // We can't deduce anything in the last remaining case:
  // ct1->constraint_case() == ConstraintProto::kAtMostOne &&
  // ct2->constraint_case() == ConstraintProto::kBoolOr

  return true;
}

// TODO(user,user): TransformIntoMaxCliques() convert the bool_and to
// at_most_one, but maybe also duplicating them into bool_or would allow this
// function to do more presolving.
bool CpModelPresolver::ProcessSetPPC() {
  const int num_constraints = context_->working_model->constraints_size();

  // Signatures of all the constraints. In the signature the bit i is 1 if it
  // contains a literal l such that l%64 = i.
  std::vector<uint64_t> signatures;

  // Graph of constraints to literals. constraint_literals[c] contains all the
  // literals in constraint indexed by 'c' in sorted order.
  std::vector<std::vector<int>> constraint_literals;

  // Graph of literals to constraints. literals_to_constraints[l] contains the
  // vector of constraint indices in which literal 'l' or 'neg(l)' appears.
  std::vector<std::vector<int>> literals_to_constraints;

  // vector of booleans indicating if the constraint was already removed.
  std::vector<bool> removed;

  // The containers above use the local indices for setppc constraints. We store
  // the original constraint indices corresponding to those local indices here.
  std::vector<int> original_constraint_index;

  // Fill the initial constraint <-> literal graph, compute signatures and
  // initialize other containers defined above.
  int num_setppc_constraints = 0;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kExactlyOne) {
      // Because TransformIntoMaxCliques() can detect literal equivalence
      // relation, we make sure the constraints are presolved before being
      // inspected.
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return false;
    }
    if (ct->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kExactlyOne) {
      constraint_literals.push_back(GetLiteralsFromSetPPCConstraint(*ct));

      uint64_t signature = 0;
      for (const int literal : constraint_literals.back()) {
        const int positive_literal = PositiveRef(literal);
        signature |= (int64_t{1} << (positive_literal % 64));
        DCHECK_GE(positive_literal, 0);
        if (positive_literal >= literals_to_constraints.size()) {
          literals_to_constraints.resize(positive_literal + 1);
        }
        literals_to_constraints[positive_literal].push_back(
            num_setppc_constraints);
      }
      signatures.push_back(signature);
      removed.push_back(false);
      original_constraint_index.push_back(c);
      num_setppc_constraints++;
    }
  }
  VLOG(1) << "#setppc constraints: " << num_setppc_constraints;

  // Set of constraint pairs which are already compared.
  absl::flat_hash_set<std::pair<int, int>> compared_constraints;
  for (const std::vector<int>& literal_to_constraints :
       literals_to_constraints) {
    for (int index1 = 0; index1 < literal_to_constraints.size(); ++index1) {
      if (context_->time_limit()->LimitReached()) return true;

      const int c1 = literal_to_constraints[index1];
      if (removed[c1]) continue;
      const std::vector<int>& c1_literals = constraint_literals[c1];

      for (int index2 = index1 + 1; index2 < literal_to_constraints.size();
           ++index2) {
        const int c2 = literal_to_constraints[index2];
        if (removed[c2]) continue;
        if (removed[c1]) break;

        // TODO(user): This should not happen. Investigate.
        if (c1 == c2) continue;

        CHECK_LT(c1, c2);
        if (gtl::ContainsKey(compared_constraints,
                             std::pair<int, int>(c1, c2))) {
          continue;
        }
        compared_constraints.insert({c1, c2});

        // Hard limit on number of comparisons to avoid spending too much time
        // here.
        if (compared_constraints.size() >= 50000) return true;

        const bool smaller = (signatures[c1] & ~signatures[c2]) == 0;
        const bool larger = (signatures[c2] & ~signatures[c1]) == 0;
        if (!(smaller || larger)) continue;

        // Check if literals in c1 is subset of literals in c2 or vice versa.
        const std::vector<int>& c2_literals = constraint_literals[c2];

        // TODO(user): Try avoiding computation of set differences if
        // possible.
        std::vector<int> c1_minus_c2;
        gtl::STLSetDifference(c1_literals, c2_literals, &c1_minus_c2);
        std::vector<int> c2_minus_c1;
        gtl::STLSetDifference(c2_literals, c1_literals, &c2_minus_c1);

        if (c1_minus_c2.empty()) {  // c1 included in c2.
          if (!ProcessSetPPCSubset(c1, c2, c2_minus_c1,
                                   original_constraint_index, &removed)) {
            return false;
          }
        } else if (c2_minus_c1.empty()) {  // c2 included in c1.
          if (!ProcessSetPPCSubset(c2, c1, c1_minus_c2,
                                   original_constraint_index, &removed)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

void CpModelPresolver::TryToSimplifyDomain(int var) {
  CHECK(RefIsPositive(var));
  CHECK(context_->ConstraintVariableGraphIsUpToDate());
  if (context_->ModelIsUnsat()) return;
  if (context_->IsFixed(var)) return;
  if (context_->VariableWasRemoved(var)) return;
  if (context_->VariableIsNotUsedAnymore(var)) return;

  const AffineRelation::Relation r = context_->GetAffineRelation(var);
  if (r.representative != var) return;

  // TODO(user): We can still remove the variable even if we want to keep
  // all feasible solutions for the cases when we have a full encoding.
  //
  // TODO(user): In fixed search, we disable this rule because we don't update
  // the search strategy, but for some strategy we could.
  //
  // TODO(user): The hint might get lost if the encoding was created during
  // the presolve.
  if (context_->VariableIsRemovable(var) &&
      context_->VariableIsOnlyUsedInEncoding(var) &&
      context_->params().search_branching() != SatParameters::FIXED_SEARCH) {
    // Detect the full encoding case without extra constraint.
    // This is the simplest to deal with as we can just add an exactly one
    // constraint and remove all the linear1.
    std::vector<int> literals;
    std::vector<int> equality_constraints;
    std::vector<int> other_constraints;
    absl::flat_hash_map<int, int> value_to_equal_literal;
    absl::flat_hash_map<int, int> value_to_not_equal_literal;
    bool abort = false;
    for (const int c : context_->VarToConstraints(var)) {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      CHECK_EQ(ct.constraint_case(), ConstraintProto::kLinear);
      CHECK_EQ(ct.linear().vars().size(), 1);
      int64_t coeff = ct.linear().coeffs(0);
      if (std::abs(coeff) != 1 || ct.enforcement_literal().size() != 1) {
        abort = true;
        break;
      }
      if (!RefIsPositive(ct.linear().vars(0))) coeff *= 1;
      const Domain rhs =
          ReadDomainFromProto(ct.linear()).InverseMultiplicationBy(coeff);

      if (rhs.IsFixed()) {
        const int64_t value = rhs.FixedValue();
        if (value_to_equal_literal.contains(value)) {
          abort = true;
          break;
        }
        equality_constraints.push_back(c);
        literals.push_back(ct.enforcement_literal(0));
        value_to_equal_literal[value] = ct.enforcement_literal(0);
      } else {
        const Domain complement =
            context_->DomainOf(var).IntersectionWith(rhs.Complement());
        if (complement.IsEmpty()) {
          // TODO(user): This should be dealt with elsewhere.
          abort = true;
          break;
        }
        if (complement.IsFixed()) {
          const int64_t value = complement.FixedValue();
          if (value_to_not_equal_literal.contains(value)) {
            abort = true;
            break;
          }
          other_constraints.push_back(c);
          value_to_not_equal_literal[value] = ct.enforcement_literal(0);
        } else {
          abort = true;
        }
      }
    }

    // For a full encoding, we don't need all the not equal constraint to be
    // present.
    if (value_to_equal_literal.size() != context_->DomainOf(var).Size()) {
      abort = true;
    } else {
      for (const ClosedInterval interval : context_->DomainOf(var)) {
        for (int64_t value = interval.start; value <= interval.end; ++value) {
          if (!value_to_equal_literal.contains(value)) {
            abort = true;
            break;
          }
          if (value_to_not_equal_literal.contains(value) &&
              value_to_equal_literal[value] !=
                  NegatedRef(value_to_not_equal_literal[value])) {
            abort = true;
            break;
          }
        }
        if (abort) break;
      }
    }
    if (abort) {
      context_->UpdateRuleStats("TODO variables: only used in encoding.");
    } else {
      context_->UpdateRuleStats("variables: only used as full encoding.");

      // Move the encoding constraints to the mapping model. Note that only the
      // equality constraint are needed. In fact if we add the other ones, our
      // current limited postsolve code will not work.
      for (const int c : equality_constraints) {
        *context_->mapping_model->add_constraints() =
            context_->working_model->constraints(c);
        context_->working_model->mutable_constraints(c)->Clear();
        context_->UpdateConstraintVariableUsage(c);
      }
      for (const int c : other_constraints) {
        context_->working_model->mutable_constraints(c)->Clear();
        context_->UpdateConstraintVariableUsage(c);
      }

      // This must be done after we removed all the constraint containing var.
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      std::sort(literals.begin(), literals.end());  // For determinism.
      for (const int literal : literals) {
        new_ct->mutable_exactly_one()->add_literals(literal);
      }

      // In some cases there is duplicate literal, and we want to make sure
      // this is presolved.
      PresolveExactlyOne(new_ct);

      context_->UpdateNewConstraintsVariableUsage();
      context_->MarkVariableAsRemoved(var);
      return;
    }
  }

  // Special case: if a literal l appear in exactly two constraints:
  // - l => var in domain1
  // - not(l) => var in domain2
  // then we know that domain(var) is included in domain1 U domain2,
  // and that the literal l can be removed (and determined at postsolve).
  //
  // TODO(user): This could be generalized further to linear of size > 1 if for
  // example the terms are the same.
  //
  // We wait for the model expansion to take place in order to avoid removing
  // encoding that will later be re-created during expansion.
  if (context_->ModelIsExpanded() && context_->CanBeUsedAsLiteral(var) &&
      context_->VariableIsRemovable(var) &&
      context_->VarToConstraints(var).size() == 2) {
    bool abort = false;
    int ct_var = -1;
    Domain union_of_domain;
    int num_positive = 0;
    std::vector<int> constraint_indices_to_remove;
    for (const int c : context_->VarToConstraints(var)) {
      if (c <= 0) {
        abort = true;
        continue;
      }
      constraint_indices_to_remove.push_back(c);
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.enforcement_literal().size() != 1 ||
          PositiveRef(ct.enforcement_literal(0)) != var ||
          ct.constraint_case() != ConstraintProto::kLinear ||
          ct.linear().vars().size() != 1) {
        abort = true;
        break;
      }
      if (ct.enforcement_literal(0) == var) ++num_positive;
      if (ct_var != -1 && PositiveRef(ct.linear().vars(0)) != ct_var) {
        abort = true;
        break;
      }
      ct_var = PositiveRef(ct.linear().vars(0));
      union_of_domain = union_of_domain.UnionWith(
          ReadDomainFromProto(ct.linear())
              .InverseMultiplicationBy(RefIsPositive(ct.linear().vars(0))
                                           ? ct.linear().coeffs(0)
                                           : -ct.linear().coeffs(0)));
    }
    if (!abort && num_positive == 1) {
      if (!context_->IntersectDomainWith(ct_var, union_of_domain)) {
        return;
      }
      context_->UpdateRuleStats("variables: removable enforcement literal");
      for (const int c : constraint_indices_to_remove) {
        *context_->mapping_model->add_constraints() =
            context_->working_model->constraints(c);
        context_->mapping_model
            ->mutable_constraints(
                context_->mapping_model->constraints().size() - 1)
            ->set_name("here");
        context_->working_model->mutable_constraints(c)->Clear();
        context_->UpdateConstraintVariableUsage(c);
      }
      context_->MarkVariableAsRemoved(var);
      return;
    }
  }

  // Only process discrete domain.
  const Domain& domain = context_->DomainOf(var);

  // Special case for non-Boolean domain of size 2.
  if (domain.Size() == 2 && (domain.Min() != 0 || domain.Max() != 1)) {
    context_->CanonicalizeDomainOfSizeTwo(var);
    return;
  }

  if (domain.NumIntervals() != domain.Size()) return;

  const int64_t var_min = domain.Min();
  int64_t gcd = domain[1].start - var_min;
  for (int index = 2; index < domain.NumIntervals(); ++index) {
    const ClosedInterval& i = domain[index];
    CHECK_EQ(i.start, i.end);
    const int64_t shifted_value = i.start - var_min;
    CHECK_GE(shifted_value, 0);

    gcd = MathUtil::GCD64(gcd, shifted_value);
    if (gcd == 1) break;
  }
  if (gcd == 1) return;

  int new_var_index;
  {
    std::vector<int64_t> scaled_values;
    for (int index = 0; index < domain.NumIntervals(); ++index) {
      const ClosedInterval& i = domain[index];
      CHECK_EQ(i.start, i.end);
      const int64_t shifted_value = i.start - var_min;
      scaled_values.push_back(shifted_value / gcd);
    }
    new_var_index = context_->NewIntVar(Domain::FromValues(scaled_values));
  }
  if (context_->ModelIsUnsat()) return;

  CHECK(context_->StoreAffineRelation(var, new_var_index, gcd, var_min));
  context_->UpdateRuleStats("variables: canonicalize affine domain");
  context_->UpdateNewConstraintsVariableUsage();
}

// Adds all affine relations to our model for the variables that are still used.
void CpModelPresolver::EncodeAllAffineRelations() {
  int64_t num_added = 0;
  for (int var = 0; var < context_->working_model->variables_size(); ++var) {
    if (context_->IsFixed(var)) continue;

    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative == var) continue;

    if (!context_->keep_all_feasible_solutions) {
      // TODO(user): It seems some affine relation are still removable at this
      // stage even though they should be removed inside PresolveToFixPoint().
      // Investigate. For now, we just remove such relations.
      if (context_->VariableIsNotUsedAnymore(var)) continue;
      if (!PresolveAffineRelationIfAny(var)) break;
      if (context_->VariableIsNotUsedAnymore(var)) continue;
      if (context_->IsFixed(var)) continue;
    }

    ++num_added;
    ConstraintProto* ct = context_->working_model->add_constraints();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
    context_->UpdateNewConstraintsVariableUsage();
  }

  // Now that we encoded all remaining affine relation with constraints, we
  // remove the special marker to have a proper constraint variable graph.
  context_->RemoveAllVariablesFromAffineRelationConstraint();

  if (num_added > 0) {
    SOLVER_LOG(logger_, num_added, " affine relations still in the model.");
  }
}

// Presolve a variable in relation with its representative.
bool CpModelPresolver::PresolveAffineRelationIfAny(int var) {
  if (context_->VariableIsNotUsedAnymore(var)) return true;

  const AffineRelation::Relation r = context_->GetAffineRelation(var);
  if (r.representative == var) return true;

  // Propagate domains.
  if (!context_->PropagateAffineRelation(var)) return false;

  // Once an affine relation is detected, the variables should be added to
  // the kAffineRelationConstraint. The only way to be unmarked is if the
  // variable do not appear in any other constraint and is not a representative,
  // in which case it should never be added back.
  if (context_->IsFixed(var)) return true;
  CHECK(context_->VarToConstraints(var).contains(kAffineRelationConstraint));
  CHECK(!context_->VariableIsNotUsedAnymore(r.representative));

  // If var is no longer used, remove. Note that we can always do that since we
  // propagated the domain above and so we can find a feasible value for a for
  // any value of the representative.
  if (context_->VariableIsUniqueAndRemovable(var)) {
    // Add relation with current representative to the mapping model.
    ConstraintProto* ct = context_->mapping_model->add_constraints();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
    context_->RemoveVariableFromAffineRelation(var);
  }
  return true;
}

void CpModelPresolver::PresolveToFixPoint() {
  if (context_->ModelIsUnsat()) return;

  // Limit on number of operations.
  const int64_t max_num_operations =
      context_->params().cp_model_max_num_presolve_operations() > 0
          ? context_->params().cp_model_max_num_presolve_operations()
          : std::numeric_limits<int64_t>::max();

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  absl::flat_hash_set<std::pair<int, int>> var_constraint_pair_already_called;

  TimeLimit* time_limit = context_->time_limit();

  // The queue of "active" constraints, initialized to the non-empty ones.
  std::vector<bool> in_queue(context_->working_model->constraints_size(),
                             false);
  std::deque<int> queue;
  for (int c = 0; c < in_queue.size(); ++c) {
    if (context_->working_model->constraints(c).constraint_case() !=
        ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
      in_queue[c] = true;
      queue.push_back(c);
    }
  }

  // When thinking about how the presolve works, it seems like a good idea to
  // process the "simple" constraints first in order to be more efficient.
  // In September 2019, experiment on the flatzinc problems shows no changes in
  // the results. We should actually count the number of rules triggered.
  if (context_->params().permute_presolve_constraint_order()) {
    std::shuffle(queue.begin(), queue.end(), *context_->random());
  } else {
    std::sort(queue.begin(), queue.end(), [this](int a, int b) {
      const int score_a = context_->ConstraintToVars(a).size();
      const int score_b = context_->ConstraintToVars(b).size();
      return score_a < score_b || (score_a == score_b && a < b);
    });
  }

  while (!queue.empty() && !context_->ModelIsUnsat()) {
    if (time_limit->LimitReached()) break;
    if (context_->num_presolve_operations > max_num_operations) break;
    while (!queue.empty() && !context_->ModelIsUnsat()) {
      if (time_limit->LimitReached()) break;
      if (context_->num_presolve_operations > max_num_operations) break;
      const int c = queue.front();
      in_queue[c] = false;
      queue.pop_front();

      const int old_num_constraint =
          context_->working_model->constraints_size();
      const bool changed = PresolveOneConstraint(c);
      if (context_->ModelIsUnsat()) {
        SOLVER_LOG(logger_, "Unsat after presolving constraint #", c,
                   " (warning, dump might be inconsistent): ",
                   context_->working_model->constraints(c).ShortDebugString());
      }

      // Add to the queue any newly created constraints.
      const int new_num_constraints =
          context_->working_model->constraints_size();
      if (new_num_constraints > old_num_constraint) {
        context_->UpdateNewConstraintsVariableUsage();
        in_queue.resize(new_num_constraints, true);
        for (int c = old_num_constraint; c < new_num_constraints; ++c) {
          queue.push_back(c);
        }
      }

      // TODO(user): Is seems safer to simply remove the changed Boolean.
      // We loose a bit of performance, but the code is simpler.
      if (changed) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }

    // We also make sure all affine relations are propagated and any not
    // yet canonicalized domain is.
    //
    // TODO(user): maybe we can avoid iterating over all variables, but then
    // we already do that below.
    const int current_num_variables = context_->working_model->variables_size();
    for (int v = 0; v < current_num_variables; ++v) {
      if (context_->ModelIsUnsat()) return;
      if (!PresolveAffineRelationIfAny(v)) return;

      // Try to canonicalize the domain, note that we should have detected all
      // affine relations before, so we don't recreate "canononical" variables
      // if they already exist in the model.
      TryToSimplifyDomain(v);
      context_->UpdateNewConstraintsVariableUsage();
    }

    // Re-add to the queue constraints that have unique variables. Note that to
    // not enter an infinite loop, we call each (var, constraint) pair at most
    // once.
    const int num_vars = context_->working_model->variables_size();
    for (int v = 0; v < num_vars; ++v) {
      const auto& constraints = context_->VarToConstraints(v);
      if (constraints.size() != 1) continue;
      const int c = *constraints.begin();
      if (c < 0) continue;

      // Note that to avoid bad complexity in problem like a TSP with just one
      // big constraint. we mark all the singleton variables of a constraint
      // even if this constraint is already in the queue.
      if (gtl::ContainsKey(var_constraint_pair_already_called,
                           std::pair<int, int>(v, c))) {
        continue;
      }
      var_constraint_pair_already_called.insert({v, c});

      if (!in_queue[c]) {
        in_queue[c] = true;
        queue.push_back(c);
      }
    }

    for (int i = 0; i < 2; ++i) {
      // Re-add to the queue the constraints that touch a variable that changed.
      //
      // TODO(user): Avoid reprocessing the constraints that changed the
      // variables with the use of timestamp.
      if (context_->ModelIsUnsat()) return;
      in_queue.resize(context_->working_model->constraints_size(), false);
      for (const int v : context_->modified_domains.PositionsSetAtLeastOnce()) {
        if (context_->VariableIsNotUsedAnymore(v)) continue;
        if (context_->IsFixed(v)) context_->ExploitFixedDomain(v);
        for (const int c : context_->VarToConstraints(v)) {
          if (c >= 0 && !in_queue[c]) {
            in_queue[c] = true;
            queue.push_back(c);
          }
        }
      }

      // If we reach the end of the loop, try to detect dominance relations.
      if (!queue.empty() || i == 1) break;

      // Detect & exploit dominance between variables, or variables that can
      // move freely in one direction. Or variables that are just blocked by one
      // constraint in one direction.
      //
      // TODO(user): We can support assumptions but we need to not cut them out
      // of the feasible region.
      if (!context_->keep_all_feasible_solutions &&
          context_->working_model->assumptions().empty()) {
        VarDomination var_dom;
        DualBoundStrengthening dual_bound_strengthening;
        DetectDominanceRelations(*context_, &var_dom,
                                 &dual_bound_strengthening);
        if (!dual_bound_strengthening.Strengthen(context_)) return;

        // TODO(user): The Strengthen() function above might make some
        // inequality tight. Currently, because we only do that for implication,
        // this will not change who dominate who, but in general we should
        // process the new constraint direction before calling this.
        if (!ExploitDominanceRelations(var_dom, context_)) return;
      }
    }

    // Make sure the order is deterministic! because var_to_constraints[]
    // order changes from one run to the next.
    std::sort(queue.begin(), queue.end());
    context_->modified_domains.SparseClearAll();
  }

  if (context_->ModelIsUnsat()) return;

  // Second "pass" for transformation better done after all of the above and
  // that do not need a fix-point loop.
  //
  // TODO(user): Also add deductions achieved during probing!
  //
  // TODO(user): ideally we should "wake-up" any constraint that contains an
  // absent interval in the main propagation loop above. But we currently don't
  // maintain such list.
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kNoOverlap:
        // Filter out absent intervals.
        if (PresolveNoOverlap(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        // Filter out absent intervals.
        if (PresolveNoOverlap2D(c, ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        // Filter out absent intervals.
        if (PresolveCumulative(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::ConstraintCase::kBoolOr: {
        // Try to infer domain reductions from clauses and the saved "implies in
        // domain" relations.
        for (const auto& pair :
             context_->deductions.ProcessClause(ct->bool_or().literals())) {
          bool modified = false;
          if (!context_->IntersectDomainWith(pair.first, pair.second,
                                             &modified)) {
            return;
          }
          if (modified) {
            context_->UpdateRuleStats("deductions: reduced variable domain");
          }
        }
        break;
      }
      default:
        break;
    }
  }

  context_->deductions.MarkProcessingAsDoneForNow();
}

ModelCopy::ModelCopy(PresolveContext* context) : context_(context) {}

// TODO(user): Merge with the phase 1 of the presolve code.
bool ModelCopy::ImportAndSimplifyConstraints(
    const CpModelProto& in_model, const std::vector<int>& ignored_constraints) {
  const absl::flat_hash_set<int> ignored_constraints_set(
      ignored_constraints.begin(), ignored_constraints.end());
  context_->InitializeNewDomains();

  starting_constraint_index_ = context_->working_model->constraints_size();
  for (int c = 0; c < in_model.constraints_size(); ++c) {
    if (ignored_constraints_set.contains(c)) continue;

    const ConstraintProto& ct = in_model.constraints(c);
    if (OneEnforcementLiteralIsFalse(ct) &&
        ct.constraint_case() != ConstraintProto::kInterval) {
      continue;
    }
    switch (ct.constraint_case()) {
      case ConstraintProto::CONSTRAINT_NOT_SET: {
        break;
      }
      case ConstraintProto::kBoolOr: {
        if (!CopyBoolOr(ct)) return CreateUnsatModel();
        break;
      }
      case ConstraintProto::kBoolAnd: {
        if (!CopyBoolAnd(ct)) return CreateUnsatModel();
        break;
      }
      case ConstraintProto::kLinear: {
        if (!CopyLinear(ct)) return CreateUnsatModel();
        break;
      }
      case ConstraintProto::kAtMostOne: {
        if (!CopyAtMostOne(ct)) return CreateUnsatModel();
        break;
      }
      case ConstraintProto::kExactlyOne: {
        if (!CopyExactlyOne(ct)) return CreateUnsatModel();
        break;
      }
      case ConstraintProto::kInterval: {
        if (!CopyInterval(ct, c)) return CreateUnsatModel();
        break;
      }
      default: {
        *context_->working_model->add_constraints() = ct;
      }
    }
  }

  // Re-map interval indices for new constraints.
  // TODO(user): Support removing unperformed intervals.
  for (int c = starting_constraint_index_;
       c < context_->working_model->constraints_size(); ++c) {
    ConstraintProto& ct_ref = *context_->working_model->mutable_constraints(c);
    ApplyToAllIntervalIndices(
        [this](int* ref) {
          const auto& it = interval_mapping_.find(*ref);
          if (it != interval_mapping_.end()) {
            *ref = it->second;
          }
        },
        &ct_ref);
  }

  return true;
}

void ModelCopy::CopyEnforcementLiterals(const ConstraintProto& orig,
                                        ConstraintProto* dest) {
  temp_enforcement_literals_.clear();
  for (const int lit : orig.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) {
      skipped_non_zero_++;
      continue;
    }
    temp_enforcement_literals_.push_back(lit);
  }
  dest->mutable_enforcement_literal()->Add(temp_enforcement_literals_.begin(),
                                           temp_enforcement_literals_.end());
}

bool ModelCopy::OneEnforcementLiteralIsFalse(const ConstraintProto& ct) const {
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsFalse(lit)) {
      return true;
    }
  }
  return false;
}

bool ModelCopy::CopyBoolOr(const ConstraintProto& ct) {
  temp_literals_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) continue;
    temp_literals_.push_back(NegatedRef(lit));
  }
  for (const int lit : ct.bool_or().literals()) {
    if (context_->LiteralIsTrue(lit)) {
      return true;
    }
    if (context_->LiteralIsFalse(lit)) {
      skipped_non_zero_++;
    } else {
      temp_literals_.push_back(lit);
    }
  }

  context_->working_model->add_constraints()
      ->mutable_bool_or()
      ->mutable_literals()
      ->Add(temp_literals_.begin(), temp_literals_.end());
  return !temp_literals_.empty();
}

bool ModelCopy::CopyBoolAnd(const ConstraintProto& ct) {
  bool at_least_one_false = false;
  int num_non_fixed_literals = 0;
  for (const int lit : ct.bool_and().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      at_least_one_false = true;
      break;
    }
    if (!context_->LiteralIsTrue(lit)) {
      num_non_fixed_literals++;
    }
  }

  if (at_least_one_false) {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    BoolArgumentProto* bool_or = new_ct->mutable_bool_or();

    // One enforcement literal must be false.
    for (const int lit : ct.enforcement_literal()) {
      if (context_->LiteralIsTrue(lit)) {
        skipped_non_zero_++;
        continue;
      }
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  } else if (num_non_fixed_literals > 0) {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    CopyEnforcementLiterals(ct, new_ct);
    BoolArgumentProto* bool_and = new_ct->mutable_bool_and();
    bool_and->mutable_literals()->Reserve(num_non_fixed_literals);
    for (const int lit : ct.bool_and().literals()) {
      if (context_->LiteralIsTrue(lit)) {
        skipped_non_zero_++;
        continue;
      }
      bool_and->add_literals(lit);
    }
  }
  return true;
}

bool ModelCopy::CopyLinear(const ConstraintProto& ct) {
  non_fixed_variables_.clear();
  non_fixed_coefficients_.clear();
  int64_t offset = 0;
  for (int i = 0; i < ct.linear().vars_size(); ++i) {
    const int ref = ct.linear().vars(i);
    const int64_t coeff = ct.linear().coeffs(i);
    if (context_->IsFixed(ref)) {
      offset += coeff * context_->MinOf(ref);
      skipped_non_zero_++;
    } else {
      non_fixed_variables_.push_back(ref);
      non_fixed_coefficients_.push_back(coeff);
    }
  }

  const Domain new_domain =
      ReadDomainFromProto(ct.linear()).AdditionWith(Domain(-offset));
  if (non_fixed_variables_.empty() && !new_domain.Contains(0)) {
    if (ct.enforcement_literal().empty()) {
      return false;
    }
    temp_literals_.clear();
    for (const int literal : ct.enforcement_literal()) {
      if (context_->LiteralIsTrue(literal)) {
        skipped_non_zero_++;
      } else {
        temp_literals_.push_back(NegatedRef(literal));
      }
    }
    context_->working_model->add_constraints()
        ->mutable_bool_or()
        ->mutable_literals()
        ->Add(temp_literals_.begin(), temp_literals_.end());
    return !temp_literals_.empty();
  }

  ConstraintProto* new_ct = context_->working_model->add_constraints();
  CopyEnforcementLiterals(ct, new_ct);
  LinearConstraintProto* linear = new_ct->mutable_linear();
  linear->mutable_vars()->Add(non_fixed_variables_.begin(),
                              non_fixed_variables_.end());
  linear->mutable_coeffs()->Add(non_fixed_coefficients_.begin(),
                                non_fixed_coefficients_.end());
  FillDomainInProto(new_domain, linear);
  return true;
}

bool ModelCopy::CopyAtMostOne(const ConstraintProto& ct) {
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.at_most_one().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      skipped_non_zero_++;
      continue;
    }
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.size() <= 1) return true;
  if (num_true > 1) return false;
  // TODO(user): presolve if num_true == 1.

  ConstraintProto* new_ct = context_->working_model->add_constraints();
  CopyEnforcementLiterals(ct, new_ct);
  new_ct->mutable_at_most_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyExactlyOne(const ConstraintProto& ct) {
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.exactly_one().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      skipped_non_zero_++;
      continue;
    }
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.empty() || num_true > 1) return false;

  // TODO(user): presolve if num_true == 1.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  CopyEnforcementLiterals(ct, new_ct);
  new_ct->mutable_exactly_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyInterval(const ConstraintProto& ct, int c) {
  // TODO(user): remove non performed intervals.
  CHECK_EQ(starting_constraint_index_, 0)
      << "Adding new interval constraints to partially filled model is not "
         "supported.";
  interval_mapping_[c] = context_->working_model->constraints_size();
  *context_->working_model->add_constraints() = ct;
  return true;
}

bool ModelCopy::CreateUnsatModel() {
  context_->working_model->mutable_constraints()->Clear();
  context_->working_model->add_constraints()->mutable_bool_or();
  return false;
}

bool ImportConstraintsWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                                   PresolveContext* context) {
  ModelCopy copier(context);
  if (copier.ImportAndSimplifyConstraints(in_model, {})) {
    CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(in_model,
                                                                 context);
    return true;
  }
  return context->NotifyThatModelIsUnsat();
}

void CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
    const CpModelProto& in_model, PresolveContext* context) {
  if (!in_model.name().empty()) {
    context->working_model->set_name(in_model.name());
  }
  if (in_model.has_objective()) {
    *context->working_model->mutable_objective() = in_model.objective();
  }
  if (!in_model.search_strategy().empty()) {
    *context->working_model->mutable_search_strategy() =
        in_model.search_strategy();
  }
  if (!in_model.assumptions().empty()) {
    *context->working_model->mutable_assumptions() = in_model.assumptions();
  }
  if (in_model.has_symmetry()) {
    *context->working_model->mutable_symmetry() = in_model.symmetry();
  }
  if (in_model.has_solution_hint()) {
    *context->working_model->mutable_solution_hint() = in_model.solution_hint();
  }
}

// =============================================================================
// Public API.
// =============================================================================

bool PresolveCpModel(PresolveContext* context,
                     std::vector<int>* postsolve_mapping) {
  CpModelPresolver presolver(context, postsolve_mapping);
  return presolver.Presolve();
}

CpModelPresolver::CpModelPresolver(PresolveContext* context,
                                   std::vector<int>* postsolve_mapping)
    : postsolve_mapping_(postsolve_mapping),
      context_(context),
      logger_(context->logger()) {
  // TODO(user): move in the context.
  context_->keep_all_feasible_solutions =
      context_->params().keep_all_feasible_solutions_in_presolve() ||
      context_->params().enumerate_all_solutions() ||
      context_->params().fill_tightened_domains_in_response() ||
      !context_->params().cp_model_presolve();

  // We copy the search strategy to the mapping_model.
  for (const auto& decision_strategy :
       context_->working_model->search_strategy()) {
    *(context_->mapping_model->add_search_strategy()) = decision_strategy;
  }

  // Initialize the initial context.working_model domains.
  context_->InitializeNewDomains();

  // Initialize the objective.
  context_->ReadObjectiveFromProto();
  if (!context_->CanonicalizeObjective()) {
    (void)context_->NotifyThatModelIsUnsat();
  }

  // Note that we delay the call to UpdateNewConstraintsVariableUsage() for
  // efficiency during LNS presolve.
}

// The presolve works as follow:
//
// First stage:
// We will process all active constraints until a fix point is reached. During
// this stage:
// - Variable will never be deleted, but their domain will be reduced.
// - Constraint will never be deleted (they will be marked as empty if needed).
// - New variables and new constraints can be added after the existing ones.
// - Constraints are added only when needed to the mapping_problem if they are
//   needed during the postsolve.
//
// Second stage:
// - All the variables domain will be copied to the mapping_model.
// - Everything will be remapped so that only the variables appearing in some
//   constraints will be kept and their index will be in [0, num_new_variables).
bool CpModelPresolver::Presolve() {
  // If presolve is false, just run expansion.
  if (!context_->params().cp_model_presolve()) {
    context_->UpdateNewConstraintsVariableUsage();
    ExpandCpModel(context_);
    if (logger_->LoggingIsEnabled()) context_->LogInfo();
    return true;
  }

  // Before initializing the constraint <-> variable graph (which is costly), we
  // run a bunch of simple presolve rules. Note that these function should NOT
  // use the graph, or the part that uses it should properly check for
  // context_->ConstraintVariableGraphIsUpToDate() before doing anything that
  // depends on the graph.
  for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    PresolveEnforcementLiteral(ct);
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        PresolveBoolOr(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        PresolveBoolAnd(ct);
        break;
      case ConstraintProto::ConstraintCase::kAtMostOne:
        PresolveAtMostOne(ct);
        break;
      case ConstraintProto::ConstraintCase::kExactlyOne:
        PresolveExactlyOne(ct);
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        CanonicalizeLinear(ct);
        break;
      default:
        break;
    }
    if (context_->ModelIsUnsat()) break;
  }
  context_->UpdateNewConstraintsVariableUsage();
  context_->RegisterVariablesUsedInAssumptions();
  DCHECK(context_->ConstraintVariableUsageIsConsistent());

  // Main propagation loop.
  for (int iter = 0; iter < context_->params().max_presolve_iterations();
       ++iter) {
    context_->UpdateRuleStats("presolve: iteration");
    // Save some quantities to decide if we abort early the iteration loop.
    const int64_t old_num_presolve_op = context_->num_presolve_operations;
    int old_num_non_empty_constraints = 0;
    for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
      const auto type =
          context_->working_model->constraints(c).constraint_case();
      if (type == ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) continue;
      old_num_non_empty_constraints++;
    }

    // TODO(user): The presolve transformations we do after this is called might
    // result in even more presolve if we were to call this again! improve the
    // code. See for instance plusexample_6_sat.fzn were represolving the
    // presolved problem reduces it even more.
    PresolveToFixPoint();

    // Call expansion.
    ExpandCpModel(context_);
    DCHECK(context_->ConstraintVariableUsageIsConsistent());

    // TODO(user): do that and the pure-SAT part below more than once.
    if (context_->params().cp_model_probing_level() > 0) {
      if (!context_->time_limit()->LimitReached()) {
        Probe();
        PresolveToFixPoint();
      }
    }

    // Runs SAT specific presolve on the pure-SAT part of the problem.
    // Note that because this can only remove/fix variable not used in the other
    // part of the problem, there is no need to redo more presolve afterwards.
    if (context_->params().cp_model_use_sat_presolve()) {
      if (!context_->time_limit()->LimitReached()) {
        PresolvePureSatPart();
      }
    }

    // Extract redundant at most one constraint form the linear ones.
    //
    // TODO(user): more generally if we do some probing, the same relation will
    // be detected (and more). Also add an option to turn this off?
    //
    // TODO(user): instead of extracting at most one, extract pairwise conflicts
    // and add them to bool_and clauses? this is some sort of small scale
    // probing, but good for sat presolve and clique later?
    if (!context_->ModelIsUnsat() && iter == 0) {
      const int old_size = context_->working_model->constraints_size();
      for (int c = 0; c < old_size; ++c) {
        ConstraintProto* ct = context_->working_model->mutable_constraints(c);
        if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
          continue;
        }
        ExtractAtMostOneFromLinear(ct);
      }
      context_->UpdateNewConstraintsVariableUsage();
    }

    if (iter == 0) TransformIntoMaxCliques();

    // TODO(user): Decide where is the best place for this. Fow now we do it
    // after max clique to get all the bool_and converted to at most ones.
    if (context_->params().symmetry_level() > 0 && !context_->ModelIsUnsat() &&
        !context_->time_limit()->LimitReached() &&
        !context_->keep_all_feasible_solutions) {
      DetectAndExploitSymmetriesInPresolve(context_);
    }

    // Process set packing, partitioning and covering constraint.
    if (!context_->time_limit()->LimitReached()) {
      ProcessSetPPC();
      ExtractBoolAnd();

      // Call the main presolve to remove the fixed variables and do more
      // deductions.
      PresolveToFixPoint();
    }

    // Exit the loop if the reduction is not so large.
    if (context_->num_presolve_operations - old_num_presolve_op <
        0.8 * (context_->working_model->variables_size() +
               old_num_non_empty_constraints)) {
      break;
    }
  }

  // Regroup no-overlaps into max-cliques.
  if (!context_->ModelIsUnsat()) {
    MergeNoOverlapConstraints();
  }

  // Tries to spread the objective amongst many variables.
  if (context_->working_model->has_objective() && !context_->ModelIsUnsat()) {
    ExpandObjective();
  }

  // Adds all needed affine relation to context_->working_model.
  if (!context_->ModelIsUnsat()) {
    EncodeAllAffineRelations();
  }

  // Remove duplicate constraints.
  //
  // TODO(user): We might want to do that earlier so that our count of variable
  // usage is not biased by duplicate constraints.
  const std::vector<int> duplicates =
      FindDuplicateConstraints(*context_->working_model);
  if (!duplicates.empty()) {
    for (const int c : duplicates) {
      const int type =
          context_->working_model->constraints(c).constraint_case();
      if (type == ConstraintProto::ConstraintCase::kInterval) {
        // TODO(user): we could delete duplicate identical interval, but we need
        // to make sure reference to them are updated.
        continue;
      }

      context_->working_model->mutable_constraints(c)->Clear();
      context_->UpdateConstraintVariableUsage(c);
      context_->UpdateRuleStats("removed duplicate constraints");
    }
  }

  if (context_->ModelIsUnsat()) {
    if (logger_->LoggingIsEnabled()) context_->LogInfo();

    // Set presolved_model to the simplest UNSAT problem (empty clause).
    context_->working_model->Clear();
    context_->working_model->add_constraints()->mutable_bool_or();
    return true;
  }

  // The strategy variable indices will be remapped in ApplyVariableMapping()
  // but first we use the representative of the affine relations for the
  // variables that are not present anymore.
  //
  // Note that we properly take into account the sign of the coefficient which
  // will result in the same domain reduction strategy. Moreover, if the
  // variable order is not CHOOSE_FIRST, then we also encode the associated
  // affine transformation in order to preserve the order.
  absl::flat_hash_set<int> used_variables;
  for (DecisionStrategyProto& strategy :
       *context_->working_model->mutable_search_strategy()) {
    DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    strategy.clear_transformations();
    for (const int ref : copy.variables()) {
      const int var = PositiveRef(ref);

      // Remove fixed variables.
      if (context_->IsFixed(var)) continue;

      // There is not point having a variable appear twice, so we only keep
      // the first occurrence in the first strategy in which it occurs.
      if (gtl::ContainsKey(used_variables, var)) continue;
      used_variables.insert(var);

      if (context_->VarToConstraints(var).empty()) {
        const AffineRelation::Relation r = context_->GetAffineRelation(var);
        if (!context_->VarToConstraints(r.representative).empty()) {
          const int rep = (r.coeff > 0) == RefIsPositive(ref)
                              ? r.representative
                              : NegatedRef(r.representative);
          if (strategy.variable_selection_strategy() !=
              DecisionStrategyProto::CHOOSE_FIRST) {
            DecisionStrategyProto::AffineTransformation* t =
                strategy.add_transformations();
            t->set_index(strategy.variables_size());
            t->set_offset(r.offset);
            t->set_positive_coeff(std::abs(r.coeff));
          }
          strategy.add_variables(rep);
        } else {
          // TODO(user): this variable was removed entirely by the presolve (no
          // equivalent variable present). We simply ignore it entirely which
          // might result in a different search...
        }
      } else {
        strategy.add_variables(ref);
      }
    }
  }

  // Sync the domains.
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    FillDomainInProto(context_->DomainOf(i),
                      context_->working_model->mutable_variables(i));
    DCHECK_GT(context_->working_model->variables(i).domain_size(), 0);
  }

  // Set the variables of the mapping_model.
  context_->mapping_model->mutable_variables()->CopyFrom(
      context_->working_model->variables());

  // Remove all the unused variables from the presolved model.
  postsolve_mapping_->clear();
  std::vector<int> mapping(context_->working_model->variables_size(), -1);
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    if (context_->VariableIsNotUsedAnymore(i) &&
        !context_->keep_all_feasible_solutions) {
      continue;
    }
    mapping[i] = postsolve_mapping_->size();
    postsolve_mapping_->push_back(i);
  }

  if (context_->params().permute_variable_randomly()) {
    std::shuffle(postsolve_mapping_->begin(), postsolve_mapping_->end(),
                 *context_->random());
    for (int i = 0; i < postsolve_mapping_->size(); ++i) {
      mapping[(*postsolve_mapping_)[i]] = i;
    }
  }

  DCHECK(context_->ConstraintVariableUsageIsConsistent());
  ApplyVariableMapping(mapping, *context_);

  // Compact all non-empty constraint at the beginning.
  RemoveEmptyConstraints();

  // Hack to display the number of deductions stored.
  if (context_->deductions.NumDeductions() > 0) {
    context_->UpdateRuleStats(absl::StrCat(
        "deductions: ", context_->deductions.NumDeductions(), " stored"));
  }

  // Stats and checks.
  if (logger_->LoggingIsEnabled()) context_->LogInfo();

  // One possible error that is difficult to avoid here: because of our
  // objective expansion, we might detect a possible overflow...
  //
  // TODO(user): We could abort the expansion when this happen.
  {
    const std::string error = ValidateCpModel(*context_->working_model);
    if (!error.empty()) {
      SOLVER_LOG(logger_, "Error while validating postsolved model: ", error);
      return false;
    }
  }
  {
    const std::string error = ValidateCpModel(*context_->mapping_model);
    if (!error.empty()) {
      SOLVER_LOG(logger_,
                 "Error while validating mapping_model model: ", error);
      return false;
    }
  }
  return true;
}

void ApplyVariableMapping(const std::vector<int>& mapping,
                          const PresolveContext& context) {
  CpModelProto* proto = context.working_model;

  // Remap all the variable/literal references in the constraints and the
  // enforcement literals in the variables.
  auto mapping_function = [&mapping](int* ref) {
    const int image = mapping[PositiveRef(*ref)];
    CHECK_GE(image, 0);
    *ref = RefIsPositive(*ref) ? image : NegatedRef(image);
  };
  for (ConstraintProto& ct_ref : *proto->mutable_constraints()) {
    ApplyToAllVariableIndices(mapping_function, &ct_ref);
    ApplyToAllLiteralIndices(mapping_function, &ct_ref);
  }

  // Remap the objective variables.
  if (proto->has_objective()) {
    for (int& mutable_ref : *proto->mutable_objective()->mutable_vars()) {
      mapping_function(&mutable_ref);
    }
  }

  // Remap the assumptions.
  for (int& mutable_ref : *proto->mutable_assumptions()) {
    mapping_function(&mutable_ref);
  }

  // Remap the search decision heuristic.
  // Note that we delete any heuristic related to a removed variable.
  for (DecisionStrategyProto& strategy : *proto->mutable_search_strategy()) {
    const DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    std::vector<int> new_indices(copy.variables().size(), -1);
    for (int i = 0; i < copy.variables().size(); ++i) {
      const int ref = copy.variables(i);
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        new_indices[i] = strategy.variables_size();
        strategy.add_variables(RefIsPositive(ref) ? image : NegatedRef(image));
      }
    }
    strategy.clear_transformations();
    for (const auto& transform : copy.transformations()) {
      CHECK_LT(transform.index(), new_indices.size());
      const int new_index = new_indices[transform.index()];
      if (new_index == -1) continue;
      auto* new_transform = strategy.add_transformations();
      *new_transform = transform;
      CHECK_LT(new_index, strategy.variables().size());
      new_transform->set_index(new_index);
    }
  }

  // Remap the solution hint.
  if (proto->has_solution_hint()) {
    auto* mutable_hint = proto->mutable_solution_hint();
    int new_size = 0;
    for (int i = 0; i < mutable_hint->vars_size(); ++i) {
      const int old_ref = mutable_hint->vars(i);
      const int64_t old_value = mutable_hint->values(i);

      // Note that if (old_value - r.offset) is not divisible by r.coeff, then
      // the hint is clearly infeasible, but we still set it to a "close" value.
      const AffineRelation::Relation r = context.GetAffineRelation(old_ref);
      const int var = r.representative;
      const int64_t value = (old_value - r.offset) / r.coeff;

      const int image = mapping[var];
      if (image >= 0) {
        mutable_hint->set_vars(new_size, image);
        mutable_hint->set_values(new_size, value);
        ++new_size;
      }
    }
    if (new_size > 0) {
      mutable_hint->mutable_vars()->Truncate(new_size);
      mutable_hint->mutable_values()->Truncate(new_size);
    } else {
      proto->clear_solution_hint();
    }
  }

  // Move the variable definitions.
  std::vector<IntegerVariableProto> new_variables;
  for (int i = 0; i < mapping.size(); ++i) {
    const int image = mapping[i];
    if (image < 0) continue;
    if (image >= new_variables.size()) {
      new_variables.resize(image + 1, IntegerVariableProto());
    }
    new_variables[image].Swap(proto->mutable_variables(i));
  }
  proto->clear_variables();
  for (IntegerVariableProto& proto_ref : new_variables) {
    proto->add_variables()->Swap(&proto_ref);
  }

  // Check that all variables are used.
  for (const IntegerVariableProto& v : proto->variables()) {
    CHECK_GT(v.domain_size(), 0);
  }
}

std::vector<int> FindDuplicateConstraints(const CpModelProto& model_proto) {
  std::vector<int> result;

  // We use a map hash: serialized_constraint_proto hash -> constraint index.
  ConstraintProto copy;
  absl::flat_hash_map<int64_t, int> equiv_constraints;

  std::string s;
  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    if (model_proto.constraints(c).constraint_case() ==
        ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
      continue;
    }

    // We ignore names when comparing constraints.
    //
    // TODO(user): This is not particularly efficient.
    copy = model_proto.constraints(c);
    copy.clear_name();
    s = copy.SerializeAsString();

    const int64_t hash = std::hash<std::string>()(s);
    const auto insert = equiv_constraints.insert({hash, c});
    if (!insert.second) {
      // Already present!
      const int other_c_with_same_hash = insert.first->second;
      copy = model_proto.constraints(other_c_with_same_hash);
      copy.clear_name();
      if (s == copy.SerializeAsString()) {
        result.push_back(c);
      }
    }
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
