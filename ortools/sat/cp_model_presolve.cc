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

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/random/random.h"
#include "absl/strings/str_join.h"
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

// Remove all empty constraints. Note that we need to remap the interval
// references.
//
// Now that they have served their purpose, we also remove dummy constraints,
// otherwise that causes issue because our model are invalid in tests.
void CpModelPresolver::RemoveEmptyConstraints() {
  std::vector<int> interval_mapping(context_->working_model->constraints_size(),
                                    -1);
  int new_num_constraints = 0;
  const int old_num_non_empty_constraints =
      context_->working_model->constraints_size();
  for (int c = 0; c < old_num_non_empty_constraints; ++c) {
    const auto type = context_->working_model->constraints(c).constraint_case();
    if (type == ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) continue;
    if (type == ConstraintProto::ConstraintCase::kDummyConstraint) continue;
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
          context_->ObjectiveMap().at(PositiveRef(literal));
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

  if (new_size == 0) {
    if (num_true_literals % 2 == 0) {
      return context_->NotifyThatModelIsUnsat("bool_xor: always false");
    } else {
      context_->UpdateRuleStats("bool_xor: always true");
      return RemoveConstraint(ct);
    }
  } else if (new_size == 1) {  // We can fix the only active literal.
    if (num_true_literals % 2 == 0) {
      if (!context_->SetLiteralToTrue(ct->bool_xor().literals(0))) {
        return context_->NotifyThatModelIsUnsat(
            "bool_xor: cannot fix last literal");
      }
    } else {
      if (!context_->SetLiteralToFalse(ct->bool_xor().literals(0))) {
        return context_->NotifyThatModelIsUnsat(
            "bool_xor: cannot fix last literal");
      }
    }
    context_->UpdateRuleStats("bool_xor: one active literal");
    return RemoveConstraint(ct);
  } else if (new_size == 2) {  // We can simplify the bool_xor.
    const int a = ct->bool_xor().literals(0);
    const int b = ct->bool_xor().literals(1);
    if (a == b) {
      if (num_true_literals % 2 == 0) {
        return context_->NotifyThatModelIsUnsat("bool_xor: always false");
      } else {
        context_->UpdateRuleStats("bool_xor: always true");
        return RemoveConstraint(ct);
      }
    }
    if (a == NegatedRef(b)) {
      if (num_true_literals % 2 == 1) {
        return context_->NotifyThatModelIsUnsat("bool_xor: always false");
      } else {
        context_->UpdateRuleStats("bool_xor: always true");
        return RemoveConstraint(ct);
      }
    }
    if (num_true_literals % 2 == 0) {  // a == not(b).
      context_->StoreBooleanEqualityRelation(a, NegatedRef(b));
    } else {  // a == b.
      context_->StoreBooleanEqualityRelation(a, b);
    }
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("bool_xor: two active literals");
    return RemoveConstraint(ct);
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

// Note this constraint does not update the constraint graph. Therefore, it
// assumes that the constraint being marked as false is the constraint being
// presolved.
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
      int64_t obj_coeff = context_->ObjectiveMap().at(var);
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
      if (!context_->SetLiteralToFalse(literal)) return false;
      context_->UpdateRuleStats(absl::StrCat(name, "duplicate literals"));
    }
    if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
      int num_positive = 0;
      int num_negative = 0;
      for (const int other : *literals) {
        if (PositiveRef(other) != PositiveRef(literal)) {
          if (!context_->SetLiteralToFalse(other)) return false;
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
        return false;
      }
      if (num_negative > 1 && !context_->SetLiteralToTrue(literal)) {
        return false;
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
          if (!context_->SetLiteralToFalse(other)) return false;
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
        context_->MarkVariableAsRemoved(PositiveRef(literal));
        continue;
      }
    }

    context_->tmp_literals.push_back(literal);
  }

  if (!is_at_most_one && !transform_to_at_most_one &&
      context_->ExploitExactlyOneInObjective(context_->tmp_literals)) {
    context_->UpdateRuleStats("exactly_one: simplified objective");
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

bool CpModelPresolver::CanonicalizeLinMax(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // Canonicalize all involved expression.
  //
  // TODO(user): If we start to have many constraints like this, we should
  // use reflexion (see cp_model_util) to do that generically.
  bool changed = CanonicalizeLinearExpression(
      *ct, ct->mutable_lin_max()->mutable_target());
  for (LinearExpressionProto& exp : *(ct->mutable_lin_max()->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }
  return changed;
}

bool CpModelPresolver::PresolveLinMax(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // Compute the infered min/max of the target.
  // Update target domain (if it is not a complex expression).
  const LinearExpressionProto& target = ct->lin_max().target();
  {
    int64_t infered_min = context_->MinOf(target);
    int64_t infered_max = std::numeric_limits<int64_t>::min();
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      infered_min = std::max(infered_min, context_->MinOf(expr));
      infered_max = std::max(infered_max, context_->MaxOf(expr));
    }

    if (target.vars().empty()) {
      if (!Domain(infered_min, infered_max).Contains(target.offset())) {
        context_->UpdateRuleStats("lin_max: infeasible");
        return MarkConstraintAsFalse(ct);
      }
    }
    if (!HasEnforcementLiteral(*ct) && target.vars().size() <= 1) {  // Affine
      Domain rhs_domain;
      for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
        rhs_domain = rhs_domain.UnionWith(
            context_->DomainSuperSetOf(expr).IntersectionWith(
                {infered_min, infered_max}));
      }
      bool reduced = false;
      if (!context_->IntersectDomainWith(target, rhs_domain, &reduced)) {
        return true;
      }
      if (reduced) {
        context_->UpdateRuleStats("lin_max: target domain reduced");
      }
    }
  }

  // Filter the expressions which are smaller than target_min.
  const int64_t target_min = context_->MinOf(target);
  const int64_t target_max = context_->MaxOf(target);
  bool changed = false;
  {
    int new_size = 0;
    for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
      const LinearExpressionProto& expr = ct->lin_max().exprs(i);
      if (context_->MaxOf(expr) < target_min) continue;
      *ct->mutable_lin_max()->mutable_exprs(new_size) = expr;
      new_size++;
    }
    if (new_size < ct->lin_max().exprs_size()) {
      context_->UpdateRuleStats("lin_max: removed exprs");
      ct->mutable_lin_max()->mutable_exprs()->DeleteSubrange(
          new_size, ct->lin_max().exprs_size() - new_size);
      changed = true;
    }
  }

  if (ct->lin_max().exprs().empty()) {
    context_->UpdateRuleStats("lin_max: no exprs");
    return MarkConstraintAsFalse(ct);
  }

  if (ct->lin_max().exprs().size() == 1) {
    // Convert to an equality. Note that we create a new constraint otherwise it
    // might not be processed again.
    context_->UpdateRuleStats("lin_max: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    *new_ct = *ct;  // copy name and potential reification.
    auto* arg = new_ct->mutable_linear();
    const LinearExpressionProto& a = ct->lin_max().target();
    const LinearExpressionProto& b = ct->lin_max().exprs(0);
    for (int i = 0; i < a.vars().size(); ++i) {
      arg->add_vars(a.vars(i));
      arg->add_coeffs(a.coeffs(i));
    }
    for (int i = 0; i < b.vars().size(); ++i) {
      arg->add_vars(b.vars(i));
      arg->add_coeffs(-b.coeffs(i));
    }
    arg->add_domain(b.offset() - a.offset());
    arg->add_domain(b.offset() - a.offset());
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Cut everything above the max if possible.
  // If one of the linear expression has many term and is above the max, we
  // abort early since none of the other rule can be applied.
  {
    bool abort = false;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      const int64_t value_min = context_->MinOf(expr);
      bool modified = false;
      if (!context_->IntersectDomainWith(expr, Domain(value_min, target_max),
                                         &modified)) {
        return true;
      }
      if (modified) {
        context_->UpdateRuleStats("lin_max: reduced expression domain.");
      }
      const int64_t value_max = context_->MaxOf(expr);
      if (value_max > target_max) {
        context_->UpdateRuleStats("TODO lin_max: linear expression above max.");
        abort = true;
      }
    }
    if (abort) return changed;
  }

  // Deal with fixed target case.
  if (target_min == target_max) {
    bool all_booleans = true;
    std::vector<int> literals;
    const int64_t fixed_target = target_min;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      const int64_t value_min = context_->MinOf(expr);
      const int64_t value_max = context_->MaxOf(expr);
      CHECK_LE(value_max, fixed_target) << "Presolved above";
      if (value_max < fixed_target) continue;

      if (value_min == value_max && value_max == fixed_target) {
        context_->UpdateRuleStats("lin_max: always satisfied");
        return RemoveConstraint(ct);
      }
      if (context_->ExpressionIsAffineBoolean(expr)) {
        CHECK_EQ(value_max, fixed_target);
        literals.push_back(context_->LiteralForExpressionMax(expr));
      } else {
        all_booleans = false;
      }
    }
    if (all_booleans) {
      if (literals.empty()) {
        return MarkConstraintAsFalse(ct);
      }

      // At least one true;
      context_->UpdateRuleStats("lin_max: fixed target and all booleans");
      for (const int lit : literals) {
        ct->mutable_bool_or()->add_literals(lit);
      }
      return true;
    }
    return changed;
  }

  // If everything is Boolean and affine, do not use a lin max!
  if (context_->ExpressionIsAffineBoolean(target)) {
    const int target_ref = context_->LiteralForExpressionMax(target);

    bool abort = false;

    bool min_is_reachable = false;
    std::vector<int> min_literals;
    std::vector<int> literals_above_min;
    std::vector<int> max_literals;

    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      const int64_t value_min = context_->MinOf(expr);
      const int64_t value_max = context_->MaxOf(expr);

      // This shouldn't happen, but it document the fact.
      if (value_min > target_min) {
        context_->UpdateRuleStats("lin_max: fix target");
        if (!context_->SetLiteralToTrue(target_ref)) return true;
        abort = true;
        break;
      }

      // expr is fixed.
      if (value_min == value_max) {
        if (value_min == target_min) min_is_reachable = true;
        continue;
      }

      if (!context_->ExpressionIsAffineBoolean(expr)) {
        abort = true;
        break;
      }

      const int ref = context_->LiteralForExpressionMax(expr);
      CHECK_LE(value_min, target_min);
      if (value_min == target_min) {
        min_literals.push_back(NegatedRef(ref));
      }

      CHECK_LE(value_max, target_max);
      if (value_max == target_max) {
        max_literals.push_back(ref);
        literals_above_min.push_back(ref);
      } else if (value_max > target_min) {
        literals_above_min.push_back(ref);
      } else if (value_max == target_min) {
        min_literals.push_back(ref);
      }
    }
    if (!abort) {
      context_->UpdateRuleStats("lin_max: all Booleans.");

      // target_ref => at_least_one(max_literals);
      ConstraintProto* clause = context_->working_model->add_constraints();
      clause->add_enforcement_literal(target_ref);
      clause->mutable_bool_or();
      for (const int lit : max_literals) {
        clause->mutable_bool_or()->add_literals(lit);
      }

      // not(target_ref) => not(lit) for lit in literals_above_min
      for (const int lit : literals_above_min) {
        context_->AddImplication(lit, target_ref);
      }

      if (!min_is_reachable) {
        // not(target_ref) => at_least_one(min_literals).
        ConstraintProto* clause = context_->working_model->add_constraints();
        clause->add_enforcement_literal(NegatedRef(target_ref));
        clause->mutable_bool_or();
        for (const int lit : min_literals) {
          clause->mutable_bool_or()->add_literals(lit);
        }
      }

      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    }
  }

  return changed;
}

// This presolve expect that the constraint only contains affine expressions.
bool CpModelPresolver::PresolveIntAbs(ConstraintProto* ct) {
  CHECK_EQ(ct->enforcement_literal_size(), 0);
  if (context_->ModelIsUnsat()) return false;
  const LinearExpressionProto& target_expr = ct->lin_max().target();
  const LinearExpressionProto& expr = ct->lin_max().exprs(0);
  DCHECK_EQ(expr.vars_size(), 1);

  // Propagate domain from the expression to the target.
  {
    const Domain expr_domain = context_->DomainSuperSetOf(expr);
    const Domain new_target_domain =
        expr_domain.UnionWith(expr_domain.Negation())
            .IntersectionWith({0, std::numeric_limits<int64_t>::max()});
    bool target_domain_modified = false;
    if (!context_->IntersectDomainWith(target_expr, new_target_domain,
                                       &target_domain_modified)) {
      return false;
    }
    if (expr_domain.IsFixed()) {
      context_->UpdateRuleStats("int_abs: fixed expression");
      return RemoveConstraint(ct);
    }
    if (target_domain_modified) {
      context_->UpdateRuleStats("int_abs: propagate domain from x to abs(x)");
    }
  }

  // Propagate from target domain to variable.
  {
    const Domain target_domain =
        context_->DomainSuperSetOf(target_expr)
            .IntersectionWith(Domain(0, std::numeric_limits<int64_t>::max()));
    const Domain new_expr_domain =
        target_domain.UnionWith(target_domain.Negation());
    bool expr_domain_modified = false;
    if (!context_->IntersectDomainWith(expr, new_expr_domain,
                                       &expr_domain_modified)) {
      return true;
    }
    // This is the only reason why we don't support fully generic linear
    // expression.
    if (context_->IsFixed(target_expr)) {
      context_->UpdateRuleStats("int_abs: fixed target");
      return RemoveConstraint(ct);
    }
    if (expr_domain_modified) {
      context_->UpdateRuleStats("int_abs: propagate domain from abs(x) to x");
    }
  }

  // Convert to equality if the sign of expr is fixed.
  if (context_->MinOf(expr) >= 0) {
    context_->UpdateRuleStats("int_abs: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, -1, arg);
    if (!CanonicalizeLinear(new_ct)) return false;
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  if (context_->MaxOf(expr) <= 0) {
    context_->UpdateRuleStats("int_abs: converted to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, 1, arg);
    if (!CanonicalizeLinear(new_ct)) return false;
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Remove the abs constraint if the target is removable and if domains have
  // been propagated without loss.
  // For now, we known that there is no loss if the target is a single ref.
  // Since all the expression are affine, in this case we are fine.
  if (ExpressionContainsSingleRef(target_expr) &&
      context_->VariableIsUniqueAndRemovable(target_expr.vars(0))) {
    context_->MarkVariableAsRemoved(target_expr.vars(0));
    *context_->mapping_model->add_constraints() = *ct;
    context_->UpdateRuleStats("int_abs: unused target");
    return RemoveConstraint(ct);
  }

  // Store the x == abs(y) relation if expr and target_expr can be cast into a
  // ref.
  // TODO(user): Support general affine expression in for expr in the Store
  //                method call.
  {
    if (ExpressionContainsSingleRef(target_expr) &&
        ExpressionContainsSingleRef(expr)) {
      const int target_ref = GetSingleRefFromExpression(target_expr);
      const int expr_ref = GetSingleRefFromExpression(expr);
      if (context_->StoreAbsRelation(target_ref, expr_ref)) {
        context_->UpdateRuleStats("int_abs: store abs(x) == y");
      }
    }
  }

  return false;
}

bool CpModelPresolver::PresolveIntProd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  LinearArgumentProto* proto = ct->mutable_int_prod();

  bool changed = CanonicalizeLinearExpression(*ct, proto->mutable_target());
  for (LinearExpressionProto& exp : *(proto->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  // Remove constant expressions.
  int64_t constant_factor = 1;
  int new_size = 0;
  for (int i = 0; i < ct->int_prod().exprs().size(); ++i) {
    LinearExpressionProto expr = ct->int_prod().exprs(i);
    if (context_->IsFixed(expr)) {
      context_->UpdateRuleStats("int_prod: removed constant expressions.");
      changed = true;
      constant_factor = CapProd(constant_factor, context_->FixedValue(expr));
      continue;
    } else {
      const int64_t coeff = expr.coeffs(0);
      const int64_t offset = expr.offset();
      const int64_t gcd =
          MathUtil::GCD64(static_cast<uint64_t>(std::abs(coeff)),
                          static_cast<uint64_t>(std::abs(offset)));
      if (gcd != 1) {
        constant_factor = CapProd(constant_factor, gcd);
        expr.set_coeffs(0, coeff / gcd);
        expr.set_offset(offset / gcd);
      }
    }
    *proto->mutable_exprs(new_size++) = expr;
  }
  proto->mutable_exprs()->erase(proto->mutable_exprs()->begin() + new_size,
                                proto->mutable_exprs()->end());

  if (ct->int_prod().exprs().empty()) {
    if (!context_->IntersectDomainWith(ct->int_prod().target(),
                                       Domain(constant_factor))) {
      return false;
    }
    context_->UpdateRuleStats("int_prod: constant product");
    return RemoveConstraint(ct);
  }

  if (constant_factor == 0) {
    context_->UpdateRuleStats("int_prod: multiplication by zero");
    if (!context_->IntersectDomainWith(ct->int_prod().target(), Domain(0))) {
      return false;
    }
    return RemoveConstraint(ct);
  }

  // In this case, the only possible value that fit in the domains is zero.
  // We will check for UNSAT if zero is not achievable by the rhs below.
  if (constant_factor == std::numeric_limits<int64_t>::min() ||
      constant_factor == std::numeric_limits<int64_t>::max()) {
    context_->UpdateRuleStats("int_prod: overflow if non zero");
    if (!context_->IntersectDomainWith(ct->int_prod().target(), Domain(0))) {
      return false;
    }
    constant_factor = 1;
  }

  // Replace by linear!
  if (ct->int_prod().exprs().size() == 1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(0);
    AddLinearExpressionToLinearConstraint(ct->int_prod().target(), 1, lin);
    AddLinearExpressionToLinearConstraint(ct->int_prod().exprs(0),
                                          -constant_factor, lin);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("int_prod: linearize product by constant.");
    return RemoveConstraint(ct);
  }

  if (constant_factor != 1) {
    // Lets canonicalize the target by introducing a new variable if necessary.
    //
    // coeff * X + offset must be a multiple of constant_factor, so
    // we can rewrite X so that this property is clear.
    const LinearExpressionProto old_target = ct->int_prod().target();
    if (!context_->IsFixed(old_target)) {
      const int ref = old_target.vars(0);
      const int64_t coeff = old_target.coeffs(0);
      const int64_t offset = old_target.offset();
      if (!context_->CanonicalizeAffineVariable(ref, coeff, constant_factor,
                                                -offset)) {
        return false;
      }
      if (context_->IsFixed(ref)) {
        changed = true;
      }
    }

    // This can happen during CanonicalizeAffineVariable().
    if (context_->IsFixed(old_target)) {
      const int64_t target_value = context_->FixedValue(old_target);
      if (target_value % constant_factor != 0) {
        return context_->NotifyThatModelIsUnsat(
            "int_prod: constant factor does not divide constant target");
      }
      proto->clear_target();
      proto->mutable_target()->set_offset(target_value / constant_factor);
      context_->UpdateRuleStats(
          "int_prod: divide product and fixed target by constant factor");
    } else {
      // We use absl::int128 to be resistant to overflow here.
      const AffineRelation::Relation r =
          context_->GetAffineRelation(old_target.vars(0));
      const absl::int128 temp_coeff =
          absl::int128(old_target.coeffs(0)) * absl::int128(r.coeff);
      CHECK_EQ(temp_coeff % absl::int128(constant_factor), 0);
      const absl::int128 temp_offset =
          absl::int128(old_target.coeffs(0)) * absl::int128(r.offset) +
          absl::int128(old_target.offset());
      CHECK_EQ(temp_offset % absl::int128(constant_factor), 0);
      const absl::int128 new_coeff = temp_coeff / absl::int128(constant_factor);
      const absl::int128 new_offset =
          temp_offset / absl::int128(constant_factor);

      // TODO(user): We try to keep coeff/offset small, if this happens, it
      // probably means there is no feasible solution involving int64_t and that
      // do not causes overflow while evaluating it, but it is hard to be
      // exactly sure we are correct here since it depends on the evaluation
      // order. Similarly, by introducing intermediate variable we might loose
      // solution if this intermediate variable value do not fit on an int64_t.
      if (new_coeff > absl::int128(std::numeric_limits<int64_t>::max()) ||
          new_coeff < absl::int128(std::numeric_limits<int64_t>::min()) ||
          new_offset > absl::int128(std::numeric_limits<int64_t>::max()) ||
          new_offset < absl::int128(std::numeric_limits<int64_t>::min())) {
        return context_->NotifyThatModelIsUnsat(
            "int_prod: overflow during simplification.");
      }

      // Rewrite the target.
      proto->mutable_target()->set_coeffs(0, static_cast<int64_t>(new_coeff));
      proto->mutable_target()->set_vars(0, r.representative);
      proto->mutable_target()->set_offset(static_cast<int64_t>(new_offset));
      context_->UpdateRuleStats("int_prod: divide product by constant factor");
      changed = true;
    }
  }

  // Restrict the target domain if possible.
  Domain implied(1);
  for (const LinearExpressionProto& expr : ct->int_prod().exprs()) {
    implied =
        implied.ContinuousMultiplicationBy(context_->DomainSuperSetOf(expr));
  }
  bool domain_modified = false;
  if (!context_->IntersectDomainWith(ct->int_prod().target(), implied,
                                     &domain_modified)) {
    return false;
  }
  if (domain_modified) {
    context_->UpdateRuleStats("int_prod: reduced target domain.");
  }

  if (ct->int_prod().exprs_size() == 2) {
    LinearExpressionProto a = ct->int_prod().exprs(0);
    LinearExpressionProto b = ct->int_prod().exprs(1);
    const LinearExpressionProto product = ct->int_prod().target();
    if (LinearExpressionProtosAreEqual(a, b) &&
        LinearExpressionProtosAreEqual(
            a, product)) {  // x = x * x, only true for {0, 1}.
      if (!context_->IntersectDomainWith(product, Domain(0, 1))) {
        return false;
      }
      context_->UpdateRuleStats("int_prod: fix variable to zero or one.");
      return RemoveConstraint(ct);
    }
  }

  // For now, we only presolve the case where all variables are Booleans.
  const LinearExpressionProto target_expr = ct->int_prod().target();
  int target;
  if (!context_->ExpressionIsALiteral(target_expr, &target)) {
    return changed;
  }
  std::vector<int> literals;
  for (const LinearExpressionProto& expr : ct->int_prod().exprs()) {
    int lit;
    if (!context_->ExpressionIsALiteral(expr, &lit)) {
      return changed;
    }
    literals.push_back(lit);
  }

  // This is a bool constraint!
  context_->UpdateRuleStats("int_prod: all Boolean.");
  {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->add_enforcement_literal(target);
    auto* arg = new_ct->mutable_bool_and();
    for (const int lit : literals) {
      arg->add_literals(lit);
    }
  }
  {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    auto* arg = new_ct->mutable_bool_or();
    arg->add_literals(target);
    for (const int lit : literals) {
      arg->add_literals(NegatedRef(lit));
    }
  }
  context_->UpdateNewConstraintsVariableUsage();
  return RemoveConstraint(ct);
}

bool CpModelPresolver::PresolveIntDiv(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  bool changed = CanonicalizeLinearExpression(
      *ct, ct->mutable_int_div()->mutable_target());
  for (LinearExpressionProto& exp : *(ct->mutable_int_div()->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  const LinearExpressionProto target = ct->int_div().target();
  const LinearExpressionProto expr = ct->int_div().exprs(0);
  const LinearExpressionProto div = ct->int_div().exprs(1);

  if (LinearExpressionProtosAreEqual(expr, div)) {
    if (!context_->IntersectDomainWith(target, Domain(1))) {
      return false;
    }
    context_->UpdateRuleStats("int_div: y = x / x");
    return RemoveConstraint(ct);
  } else if (LinearExpressionProtosAreEqual(expr, div, -1)) {
    if (!context_->IntersectDomainWith(target, Domain(-1))) {
      return false;
    }
    context_->UpdateRuleStats("int_div: y = - x / x");
    return RemoveConstraint(ct);
  }

  // For now, we only presolve the case where the divisor is constant.
  if (!context_->IsFixed(div)) return changed;

  const int64_t divisor = context_->FixedValue(div);
  if (divisor == 1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(0);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(target, -1, lin);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("int_div: rewrite to equality");
    return RemoveConstraint(ct);
  }
  bool domain_modified = false;
  if (!context_->IntersectDomainWith(
          target, context_->DomainSuperSetOf(expr).DivisionBy(divisor),
          &domain_modified)) {
    return false;
  }
  if (domain_modified) {
    context_->UpdateRuleStats(
        "int_div: updated domain of target in target = X / cte");
  }

  // Linearize if everything is positive.
  // TODO(user): Deal with other cases where there is no change of
  // sign. We can also deal with target = cte, div variable.
  if (context_->MinOf(target) >= 0 && context_->MinOf(expr) >= 0 &&
      divisor > 1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(divisor - 1);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(target, -divisor, lin);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats(
        "int_div: linearize positive division with a constant divisor");
    return RemoveConstraint(ct);
  }

  // TODO(user): reduce the domain of X by introducing an
  // InverseDivisionOfSortedDisjointIntervals().
  return changed;
}

bool CpModelPresolver::PresolveIntMod(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  bool changed = CanonicalizeLinearExpression(
      *ct, ct->mutable_int_mod()->mutable_target());
  for (LinearExpressionProto& exp : *(ct->mutable_int_mod()->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  const LinearExpressionProto target = ct->int_mod().target();
  const LinearExpressionProto expr = ct->int_mod().exprs(0);
  const LinearExpressionProto mod = ct->int_mod().exprs(1);

  bool domain_changed = false;
  if (!context_->IntersectDomainWith(
          target,
          context_->DomainSuperSetOf(expr).PositiveModuloBySuperset(
              context_->DomainSuperSetOf(mod)),
          &domain_changed)) {
    return changed;
  }

  if (domain_changed) {
    context_->UpdateRuleStats("int_mod: reduce target domain");
  }

  return changed;
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

bool CpModelPresolver::DivideLinearByGcd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

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
      return MarkConstraintAsFalse(ct);
    }
  }
  return false;
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

  if (ct->linear().domain().empty()) {
    context_->UpdateRuleStats("linear: no domain");
    return MarkConstraintAsFalse(ct);
  }

  int64_t offset = 0;
  bool changed =
      CanonicalizeLinearExpressionInternal(*ct, ct->mutable_linear(), &offset);
  if (offset != 0) {
    FillDomainInProto(
        ReadDomainFromProto(ct->linear()).AdditionWith(Domain(-offset)),
        ct->mutable_linear());
  }
  changed |= DivideLinearByGcd(ct);
  return changed;
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
      // remove that variable. Note that this do not remove any feasible
      // solution and is not a "dual" reduction.
      //
      // Note that is similar to the substitution code in PresolveLinear() but
      // it doesn't require the variable to be implied free since we do not
      // remove the constraints afterwards, just the variable.
      if (!context_->VariableWithCostIsUnique(var)) continue;
      DCHECK(context_->ObjectiveMap().contains(var));

      // We only support substitution that does not require to multiply the
      // objective by some factor.
      //
      // TODO(user): If the objective is a single variable, we can actually
      // "absorb" any factor into the objective scaling.
      const int64_t objective_coeff = context_->ObjectiveMap().at(var);
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
        //
        // Tricky: We cannot "simplify" the domain of the objective using the
        // implied domain from the linear expression since we will substitute
        // the variable.
        //
        // TODO(user): Maybe if var has a complex domain, we might not want to
        // substitute it?
        if (!context_->CanonicalizeObjective(/*simplify_domain=*/false)) {
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
        if (!context_->SubstituteVariableInObjective(var, coeff, *ct)) {
          if (context_->ModelIsUnsat()) return true;
          continue;
        }

        context_->UpdateRuleStats("linear: singleton column define objective.");
        context_->MarkVariableAsRemoved(var);
        *(context_->mapping_model->add_constraints()) = *ct;
        return RemoveConstraint(ct);
      }

      // Update the objective and remove the variable from its equality
      // constraint by expanding its rhs. This might fail if the new linear
      // objective expression can lead to overflow.
      if (!context_->SubstituteVariableInObjective(var, coeff, *ct)) {
        if (context_->ModelIsUnsat()) return true;
        continue;
      }

      context_->UpdateRuleStats(
          "linear: singleton column in equality and in objective.");
      rhs = new_rhs;
      index_to_erase.insert(i);
      break;
    }
  }
  if (index_to_erase.empty()) return false;

  // Tricky: If we have a singleton variable in an enforced constraint, and at
  // postsolve the enforcement is false, we might just ignore the constraint.
  // This is fine, but we still need to assign any removed variable to a
  // feasible value, otherwise later postsolve rules might not work correctly.
  // Adding these linear1 achieve that.
  //
  // TODO(user): Alternatively, we could copy the constraint without the
  // enforcement to the mapping model, since singleton variable are supposed
  // to always have a feasible value anyway.
  if (!ct->enforcement_literal().empty()) {
    for (const int i : index_to_erase) {
      const int var = ct->linear().vars(i);
      auto* l = context_->mapping_model->add_constraints()->mutable_linear();
      l->add_vars(var);
      l->add_coeffs(1);
      FillDomainInProto(context_->DomainOf(var), l);
    }
  }

  // TODO(user): we could add the constraint to mapping_model only once
  // instead of adding a reduced version of it each time a new singleton
  // variable appear in the same constraint later. That would work but would
  // also force the postsolve to take search decisions...
  *context_->mapping_model->add_constraints() = *ct;

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

// If the gcd of all but one term (with index target_index) is not one, we can
// rewrite the last term using an affine representative.
bool CpModelPresolver::AddVarAffineRepresentativeFromLinearEquality(
    int target_index, ConstraintProto* ct) {
  int64_t gcd = 0;
  const int num_variables = ct->linear().vars().size();
  for (int i = 0; i < num_variables; ++i) {
    if (i == target_index) continue;
    const int64_t magnitude = std::abs(ct->linear().coeffs(i));
    gcd = MathUtil::GCD64(gcd, magnitude);
    if (gcd == 1) return false;
  }

  // If we take the constraint % gcd, we have
  // ref * coeff % gcd = rhs % gcd
  CHECK_GT(gcd, 1);
  const int ref = ct->linear().vars(target_index);
  const int64_t coeff = ct->linear().coeffs(target_index);
  const int64_t rhs = ct->linear().domain(0);

  // This should have been processed before by just dividing the whole
  // constraint by the gcd.
  if (coeff % gcd == 0) return false;

  if (!context_->CanonicalizeAffineVariable(ref, coeff, gcd, rhs)) {
    return false;
  }

  // We use the new variable in the constraint.
  // Note that we will divide everything by the gcd too.
  return CanonicalizeLinear(ct);
}

// Any equality must be true modulo n.
//
// If the gcd of all but one term is not one, we can rewrite the last term using
// an affine representative by considering the equality modulo that gcd.
// As an heuristic, we only test the smallest term or small primes 2, 3, and 5.
//
// We also handle the special case of having two non-zero literals modulo 2.
bool CpModelPresolver::PresolveLinearEqualityWithModulo(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (ct->linear().domain().size() != 2) return false;
  if (ct->linear().domain(0) != ct->linear().domain(1)) return false;
  if (!ct->enforcement_literal().empty()) return false;

  const int num_variables = ct->linear().vars().size();
  if (num_variables < 2) return false;

  std::vector<int> mod2_indices;
  std::vector<int> mod3_indices;
  std::vector<int> mod5_indices;

  int64_t min_magnitude;
  int num_smallest = 0;
  int smallest_index;
  for (int i = 0; i < num_variables; ++i) {
    const int64_t magnitude = std::abs(ct->linear().coeffs(i));
    if (num_smallest == 0 || magnitude < min_magnitude) {
      min_magnitude = magnitude;
      num_smallest = 1;
      smallest_index = i;
    } else if (magnitude == min_magnitude) {
      ++num_smallest;
    }

    if (magnitude % 2 != 0) mod2_indices.push_back(i);
    if (magnitude % 3 != 0) mod3_indices.push_back(i);
    if (magnitude % 5 != 0) mod5_indices.push_back(i);
  }

  if (mod2_indices.size() == 2) {
    bool ok = true;
    std::vector<int> literals;
    for (const int i : mod2_indices) {
      const int ref = ct->linear().vars(i);
      if (!context_->CanBeUsedAsLiteral(ref)) {
        ok = false;
        break;
      }
      literals.push_back(ref);
    }
    if (ok) {
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

  // TODO(user): More than one reduction might be possible, so we will need
  // to call this again if we apply any of these reduction.
  if (mod2_indices.size() == 1) {
    return AddVarAffineRepresentativeFromLinearEquality(mod2_indices[0], ct);
  }
  if (mod3_indices.size() == 1) {
    return AddVarAffineRepresentativeFromLinearEquality(mod3_indices[0], ct);
  }
  if (mod5_indices.size() == 1) {
    return AddVarAffineRepresentativeFromLinearEquality(mod5_indices[0], ct);
  }
  if (num_smallest == 1) {
    return AddVarAffineRepresentativeFromLinearEquality(smallest_index, ct);
  }

  return false;
}

bool CpModelPresolver::PresolveLinearOfSizeOne(ConstraintProto* ct) {
  DCHECK_EQ(ct->linear().vars().size(), 1);

  // Size one constraint with no enforcement?
  if (!HasEnforcementLiteral(*ct)) {
    const int64_t coeff = RefIsPositive(ct->linear().vars(0))
                              ? ct->linear().coeffs(0)
                              : -ct->linear().coeffs(0);
    context_->UpdateRuleStats("linear1: without enforcement");
    const int var = PositiveRef(ct->linear().vars(0));
    const Domain rhs = ReadDomainFromProto(ct->linear());
    if (!context_->IntersectDomainWith(var,
                                       rhs.InverseMultiplicationBy(coeff))) {
      return false;
    }
    return RemoveConstraint(ct);
  }

  // This is just an implication, lets convert it right away.
  if (context_->CanBeUsedAsLiteral(ct->linear().vars(0))) {
    const Domain rhs = ReadDomainFromProto(ct->linear());
    const bool zero_ok = rhs.Contains(0);
    const bool one_ok = rhs.Contains(ct->linear().coeffs(0));
    context_->UpdateRuleStats("linear1: is boolean implication");
    if (!zero_ok && !one_ok) {
      return MarkConstraintAsFalse(ct);
    }
    if (zero_ok && one_ok) {
      return RemoveConstraint(ct);
    }
    const int ref = ct->linear().vars(0);
    if (zero_ok) {
      ct->mutable_bool_and()->add_literals(NegatedRef(ref));
    } else {
      ct->mutable_bool_and()->add_literals(ref);
    }

    // No var <-> constraint graph changes.
    // But this is no longer a linear1.
    return true;
  }

  // If the constraint is literal => x in domain and x = abs(abs_arg), we can
  // replace x by abs_arg and hopefully remove the variable x later.
  int abs_arg;
  if (ct->linear().coeffs(0) == 1 &&
      context_->GetAbsRelation(ct->linear().vars(0), &abs_arg)) {
    // TODO(user): Deal with coeff = -1, here or during canonicalization.
    context_->UpdateRuleStats("linear1: remove abs from abs(x) in domain");
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
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(abs_arg);
    arg->add_coeffs(1);
    FillDomainInProto(new_abs_var_domain, new_ct->mutable_linear());
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  // Detect encoding.
  if (ct->enforcement_literal_size() != 1 ||
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
    if (!context_->DomainOf(var).Contains(value)) {
      if (!context_->SetLiteralToFalse(literal)) return false;
    } else if (context_->StoreLiteralImpliesVarEqValue(literal, var, value)) {
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

bool CpModelPresolver::PresolveLinearOfSizeTwo(ConstraintProto* ct) {
  DCHECK_EQ(ct->linear().vars().size(), 2);

  const LinearConstraintProto& arg = ct->linear();
  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);
  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);

  // If it is not an equality, we only presolve the constraint if one of
  // the variable is Boolean. Note that if both are Boolean, then a similar
  // reduction is done by PresolveLinearOnBooleans(). If we have an equality,
  // then the code below will do something stronger than this.
  //
  // TODO(user): We should probably instead generalize the code of
  // ExtractEnforcementLiteralFromLinearConstraint(), or just temporary
  // propagate domain of enforced linear constraints, to detect Boolean that
  // must be true or false. This way we can do the same for longer constraints.
  const bool is_equality =
      arg.domain_size() == 2 && arg.domain(0) == arg.domain(1);
  if (!is_equality) {
    int lit, var;
    int64_t value_on_true, coeff;
    if (context_->CanBeUsedAsLiteral(var1)) {
      lit = var1;
      value_on_true = coeff1;
      var = var2;
      coeff = coeff2;
    } else if (context_->CanBeUsedAsLiteral(var2)) {
      lit = var2;
      value_on_true = coeff2;
      var = var1;
      coeff = coeff1;
    } else {
      return false;
    }

    const Domain rhs = ReadDomainFromProto(ct->linear());
    const Domain rhs_if_true =
        rhs.AdditionWith(Domain(-value_on_true)).InverseMultiplicationBy(coeff);
    const Domain rhs_if_false = rhs.InverseMultiplicationBy(coeff);
    const bool implied_false =
        context_->DomainOf(var).IntersectionWith(rhs_if_true).IsEmpty();
    const bool implied_true =
        context_->DomainOf(var).IntersectionWith(rhs_if_false).IsEmpty();
    if (implied_true && implied_false) {
      context_->UpdateRuleStats("linear2: infeasible.");
      return MarkConstraintAsFalse(ct);
    } else if (implied_true) {
      context_->UpdateRuleStats("linear2: Boolean with one feasible value.");

      // => true.
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      new_ct->mutable_bool_and()->add_literals(lit);
      context_->UpdateNewConstraintsVariableUsage();

      // Rewrite to => var in rhs_if_true.
      ct->mutable_linear()->Clear();
      ct->mutable_linear()->add_vars(var);
      ct->mutable_linear()->add_coeffs(1);
      FillDomainInProto(rhs_if_true, ct->mutable_linear());
      return PresolveLinearOfSizeOne(ct) || true;
    } else if (implied_false) {
      context_->UpdateRuleStats("linear2: Boolean with one feasible value.");

      // => false.
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      new_ct->mutable_bool_and()->add_literals(NegatedRef(lit));
      context_->UpdateNewConstraintsVariableUsage();

      // Rewrite to => var in rhs_if_false.
      ct->mutable_linear()->Clear();
      ct->mutable_linear()->add_vars(var);
      ct->mutable_linear()->add_coeffs(1);
      FillDomainInProto(rhs_if_false, ct->mutable_linear());
      return PresolveLinearOfSizeOne(ct) || true;
    } else {
      // TODO(user): We can expand this into two linear1 constraints, I am not
      // 100% sure it is always good, so for now we don't do it. Note that the
      // effect of doing it or not is not really visible on the bench. Some
      // problem are better with it some better without.
      context_->UpdateRuleStats("TODO linear2: contains a Boolean.");
      return false;
    }
  }

  // We have: enforcement => (coeff1 * v1 + coeff2 * v2 == rhs).
  const int64_t rhs = arg.domain(0);
  if (ct->enforcement_literal().empty()) {
    // Detect affine relation.
    //
    // TODO(user): it might be better to first add only the affine relation with
    // a coefficient of magnitude 1, and later the one with larger coeffs.
    bool added = false;
    if (coeff1 == 1) {
      added = context_->StoreAffineRelation(var1, var2, -coeff2, rhs);
    } else if (coeff2 == 1) {
      added = context_->StoreAffineRelation(var2, var1, -coeff1, rhs);
    } else if (coeff1 == -1) {
      added = context_->StoreAffineRelation(var1, var2, coeff2, -rhs);
    } else if (coeff2 == -1) {
      added = context_->StoreAffineRelation(var2, var1, coeff1, -rhs);
    } else {
      // In this case, we can solve the diophantine equation, and write
      // both x and y in term of a new affine representative z.
      //
      // Note that PresolveLinearEqualityWithModularInverse() will have the
      // same effect.
      //
      // We can also decide to fully expand the equality if the variables
      // are fully encoded.
      context_->UpdateRuleStats("TODO linear2: ax + by = cte");
    }
    if (added) return RemoveConstraint(ct);
  } else {
    // We look ahead to detect solutions to ax + by == cte.
    int64_t a = coeff1;
    int64_t b = coeff2;
    int64_t cte = rhs;
    int64_t x0 = 0;
    int64_t y0 = 0;
    if (!SolveDiophantineEquationOfSizeTwo(a, b, cte, x0, y0)) {
      context_->UpdateRuleStats(
          "linear2: implied ax + by = cte has no solutions");
      return MarkConstraintAsFalse(ct);
    }
    const Domain reduced_domain =
        context_->DomainOf(var1)
            .AdditionWith(Domain(-x0))
            .InverseMultiplicationBy(b)
            .IntersectionWith(context_->DomainOf(var2)
                                  .AdditionWith(Domain(-y0))
                                  .InverseMultiplicationBy(-a));

    if (reduced_domain.IsEmpty()) {  // no solution
      context_->UpdateRuleStats(
          "linear2: implied ax + by = cte has no solutions");
      return MarkConstraintAsFalse(ct);
    }

    if (reduced_domain.Size() == 1) {
      const int64_t z = reduced_domain.FixedValue();
      const int64_t value1 = x0 + b * z;
      const int64_t value2 = y0 - a * z;

      DCHECK(context_->DomainContains(var1, value1));
      DCHECK(context_->DomainContains(var2, value2));
      DCHECK_EQ(coeff1 * value1 + coeff2 * value2, rhs);

      ConstraintProto* imply1 = context_->working_model->add_constraints();
      *imply1->mutable_enforcement_literal() = ct->enforcement_literal();
      imply1->mutable_linear()->add_vars(var1);
      imply1->mutable_linear()->add_coeffs(1);
      imply1->mutable_linear()->add_domain(value1);
      imply1->mutable_linear()->add_domain(value1);

      ConstraintProto* imply2 = context_->working_model->add_constraints();
      *imply2->mutable_enforcement_literal() = ct->enforcement_literal();
      imply2->mutable_linear()->add_vars(var2);
      imply2->mutable_linear()->add_coeffs(1);
      imply2->mutable_linear()->add_domain(value2);
      imply2->mutable_linear()->add_domain(value2);
      context_->UpdateRuleStats(
          "linear2: implied ax + by = cte has only one solution");
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    }
  }

  return false;
}

bool CpModelPresolver::PresolveSmallLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (context_->ModelIsUnsat()) return false;

  if (ct->linear().vars().empty()) {
    context_->UpdateRuleStats("linear: empty");
    const Domain rhs = ReadDomainFromProto(ct->linear());
    if (rhs.Contains(0)) {
      return RemoveConstraint(ct);
    } else {
      return MarkConstraintAsFalse(ct);
    }
  } else if (ct->linear().vars().size() == 1) {
    return PresolveLinearOfSizeOne(ct);
  } else if (ct->linear().vars().size() == 2) {
    return PresolveLinearOfSizeTwo(ct);
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

bool CpModelPresolver::DetectAndProcessOneSidedLinearConstraint(
    int c, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
    return false;
  }
  if (context_->ModelIsUnsat()) return false;
  if (context_->keep_all_feasible_solutions) return false;

  // TODO(user): There is a bit of code and effort duplication with
  // PropagateDomainsInLinear(). Try to remove that.
  Domain implied_rhs(0);
  const int num_vars = ct->linear().vars().size();
  for (int i = 0; i < num_vars; ++i) {
    const int ref = ct->linear().vars(i);
    const int64_t coeff = ct->linear().coeffs(i);
    implied_rhs =
        implied_rhs
            .AdditionWith(context_->DomainOf(ref).MultiplicationBy(coeff))
            .RelaxIfTooComplex();
  }

  // Abort if trivial.
  const Domain old_rhs = ReadDomainFromProto(ct->linear());
  if (implied_rhs.IsIncludedIn(old_rhs)) {
    context_->UpdateRuleStats("linear: always true");
    return RemoveConstraint(ct);
  }

  // Incorporate the implied rhs information.
  const Domain rhs = old_rhs.SimplifyUsingImpliedDomain(implied_rhs);
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
  const bool is_le_constraint = IsLeConstraint(rhs, implied_rhs);
  const bool is_ge_constraint = IsGeConstraint(rhs, implied_rhs);
  if (!is_le_constraint && !is_ge_constraint) return false;
  CHECK_NE(is_le_constraint, is_ge_constraint);

  bool recanonicalize = false;
  for (int i = 0; i < num_vars; ++i) {
    const int var = ct->linear().vars(i);
    const int64_t var_coeff = ct->linear().coeffs(i);
    CHECK(RefIsPositive(var));

    if ((var_coeff > 0) == is_ge_constraint) {
      context_->var_to_lb_only_constraints[var].insert(c);
    } else {
      context_->var_to_ub_only_constraints[var].insert(c);
    }

    // Simple dual fixing: If for any feasible solution, any solution with var
    // higher (resp. lower) is also valid, then we can fix that variable to
    // its bound if it also moves the objective in the good direction.
    const bool is_in_objective = context_->VarToConstraints(var).contains(-1);
    const int size =
        context_->VarToConstraints(var).size() - (is_in_objective ? 1 : 0);
    const int64_t obj_coeff =
        is_in_objective ? context_->ObjectiveMap().at(var) : 0;

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
        if (!context_->IntersectDomainWith(var, Domain(context_->MaxOf(var)))) {
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
        if (!context_->IntersectDomainWith(var, Domain(context_->MinOf(var)))) {
          return false;
        }
        context_->UpdateRuleStats("linear: dual fixing");
        recanonicalize = true;
        continue;
      }
    }
  }

  if (recanonicalize) return CanonicalizeLinear(ct);
  return false;
}

bool CpModelPresolver::PropagateDomainsInLinear(int ct_index,
                                                ConstraintProto* ct) {
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

    // The other transformations below require a non-reified constraint.
    if (!ct->enforcement_literal().empty()) continue;

    // Given a variable that only appear in one constraint and in the
    // objective, for any feasible solution, it will be always better to move
    // this singleton variable as much as possible towards its good objective
    // direction. Sometime_exprs, we can detect that we will always be able to
    // do this until the only constraint of this singleton variable is tight.
    //
    // When this happens, we can make the constraint an equality. Note that it
    // might not always be good to restrict constraint like this, but in this
    // case, the RemoveSingletonInLinear() code should be able to remove this
    // variable altogether.
    if (rhs.Min() != rhs.Max() &&
        context_->VariableWithCostIsUniqueAndRemovable(var)) {
      const int64_t obj_coeff = context_->ObjectiveMap().at(var);
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
      if (c == ct_index) continue;
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
      const bool ok = SubstituteVariable(
          var, var_coeff, *ct, context_->working_model->mutable_constraints(c));
      if (!ok) {
        // This can happen if the constraint was not canonicalized and the
        // variable is actually not there (we have var - var for instance).
        CanonicalizeLinear(context_->working_model->mutable_constraints(c));
      }

      // TODO(user): We should re-enqueue these constraints for presolve.
      context_->UpdateConstraintVariableUsage(c);
    }

    // Substitute in objective.
    // This can only fail in corner cases.
    if (is_in_objective &&
        !context_->SubstituteVariableInObjective(var, var_coeff, *ct)) {
      continue;
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

bool CpModelPresolver::PresolveInterval(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  IntervalConstraintProto* interval = ct->mutable_interval();

  // If the size is < 0, then the interval cannot be performed.
  if (!ct->enforcement_literal().empty() && context_->SizeMax(c) < 0) {
    context_->UpdateRuleStats("interval: negative size implies unperformed");
    return MarkConstraintAsFalse(ct);
  }

  bool changed = false;
  if (ct->enforcement_literal().empty()) {
    // Size can't be negative.
    if (!context_->IntersectDomainWith(
            interval->size(), Domain(0, std::numeric_limits<int64_t>::max()),
            &changed)) {
      return false;
    }
    context_->UpdateRuleStats(
        "interval: performed intervals must have a positive size");
  }

  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_start());
  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_size());
  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_end());
  return changed;
}

// TODO(user): avoid code duplication between expand and presolve.
bool CpModelPresolver::PresolveInverse(ConstraintProto* ct) {
  const int size = ct->inverse().f_direct().size();
  bool changed = false;

  // Make sure the domains are included in [0, size - 1).
  for (const int ref : ct->inverse().f_direct()) {
    if (!context_->IntersectDomainWith(ref, Domain(0, size - 1), &changed)) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return false;
    }
  }
  for (const int ref : ct->inverse().f_inverse()) {
    if (!context_->IntersectDomainWith(ref, Domain(0, size - 1), &changed)) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return false;
    }
  }

  // Detect duplicated variable.
  // Even with negated variables, the reduced domain in [0..size - 1]
  // implies that the constraint is infeasible if ref and its negation
  // appear together.
  {
    absl::flat_hash_set<int> direct_vars;
    for (const int ref : ct->inverse().f_direct()) {
      const auto [it, inserted] = direct_vars.insert(PositiveRef(ref));
      if (!inserted) {
        return context_->NotifyThatModelIsUnsat("inverse: duplicated variable");
      }
    }

    absl::flat_hash_set<int> inverse_vars;
    for (const int ref : ct->inverse().f_inverse()) {
      const auto [it, inserted] = inverse_vars.insert(PositiveRef(ref));
      if (!inserted) {
        return context_->NotifyThatModelIsUnsat("inverse: duplicated variable");
      }
    }
  }

  // Propagate from one vector to its counterpart.
  // Note this reaches the fixpoint as there is a one to one mapping between
  // (variable-value) pairs in each vector.
  const auto filter_inverse_domain =
      [this, size, &changed](const auto& direct, const auto& inverse) {
        // Build the set of values in the inverse vector.
        std::vector<absl::flat_hash_set<int64_t>> inverse_values(size);
        for (int i = 0; i < size; ++i) {
          const Domain domain = context_->DomainOf(inverse[i]);
          for (const int64_t j : domain.Values()) {
            inverse_values[i].insert(j);
          }
        }

        // Propagate from the inverse vector to the direct vector. Reduce the
        // domains of each variable in the direct vector by checking that the
        // inverse value exists.
        std::vector<int64_t> possible_values;
        for (int i = 0; i < size; ++i) {
          possible_values.clear();
          const Domain domain = context_->DomainOf(direct[i]);
          bool removed_value = false;
          for (const int64_t j : domain.Values()) {
            if (inverse_values[j].contains(i)) {
              possible_values.push_back(j);
            } else {
              removed_value = true;
            }
          }
          if (removed_value) {
            changed = true;
            if (!context_->IntersectDomainWith(
                    direct[i], Domain::FromValues(possible_values))) {
              VLOG(1) << "Empty domain for a variable in ExpandInverse()";
              return false;
            }
          }
        }
        return true;
      };

  if (!filter_inverse_domain(ct->inverse().f_direct(),
                             ct->inverse().f_inverse())) {
    return false;
  }

  if (!filter_inverse_domain(ct->inverse().f_inverse(),
                             ct->inverse().f_direct())) {
    return false;
  }

  if (changed) {
    context_->UpdateRuleStats("inverse: reduce domains");
  }

  return false;
}

bool CpModelPresolver::PresolveElement(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (ct->element().vars().empty()) {
    context_->UpdateRuleStats("element: empty array");
    return context_->NotifyThatModelIsUnsat();
  }

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
      for (const int64_t index_value : index_domain.Values()) {
        const int ref = ct->element().vars(index_value);
        const int64_t target_value =
            target_ref == index_ref ? index_value : -index_value;
        if (context_->DomainContains(ref, target_value)) {
          possible_indices.push_back(target_value);
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
    for (const int64_t value : initial_index_domain.Values()) {
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

  // Should have been taken care of ealier.
  DCHECK(!context_->IsFixed(index_ref));

  // If a variable (target or index) appears only in this constraint, it does
  // not necessarily mean that we can remove the constraint, as the variable
  // can be used multiple time_exprs in the element. So let's count the local
  // uses of each variable.
  absl::flat_hash_map<int, int> local_var_occurrence_counter;
  local_var_occurrence_counter[PositiveRef(index_ref)]++;
  local_var_occurrence_counter[PositiveRef(target_ref)]++;

  for (const ClosedInterval interval : context_->DomainOf(index_ref)) {
    for (int64_t value = interval.start; value <= interval.end; ++value) {
      DCHECK_GE(value, 0);
      DCHECK_LT(value, ct->element().vars_size());
      const int ref = ct->element().vars(value);
      local_var_occurrence_counter[PositiveRef(ref)]++;
    }
  }

  if (context_->VariableIsUniqueAndRemovable(index_ref) &&
      local_var_occurrence_counter.at(PositiveRef(index_ref)) == 1) {
    if (all_constants) {
      // This constraint is just here to reduce the domain of the target! We can
      // add it to the mapping_model to reconstruct the index value during
      // postsolve and get rid of it now.
      context_->UpdateRuleStats("element: trivial target domain reduction");
      context_->MarkVariableAsRemoved(index_ref);
      *(context_->mapping_model->add_constraints()) = *ct;
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("TODO element: index not used elsewhere");
    }
  }

  if (!context_->IsFixed(target_ref) &&
      context_->VariableIsUniqueAndRemovable(target_ref) &&
      local_var_occurrence_counter.at(PositiveRef(target_ref)) == 1) {
    if (all_included_in_target_domain) {
      context_->UpdateRuleStats("element: trivial index domain reduction");
      context_->MarkVariableAsRemoved(target_ref);
      *(context_->mapping_model->add_constraints()) = *ct;
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("TODO element: target not used elsewhere");
    }
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

  const int initial_num_vars = ct->table().vars_size();
  bool changed = true;

  // Query existing affine relations.
  std::vector<AffineRelation::Relation> affine_relations;
  std::vector<int64_t> old_var_lb;
  std::vector<int64_t> old_var_ub;
  {
    for (int v = 0; v < initial_num_vars; ++v) {
      const int ref = ct->table().vars(v);
      AffineRelation::Relation r = context_->GetAffineRelation(ref);
      affine_relations.push_back(r);
      old_var_lb.push_back(context_->MinOf(ref));
      old_var_ub.push_back(context_->MaxOf(ref));
      if (r.representative != ref) {
        changed = true;
        ct->mutable_table()->set_vars(v, r.representative);
        context_->UpdateRuleStats(
            "table: replace variable by canonical affine one");
      }
    }
  }

  // Check for duplicate occurrences of variables.
  // If the ith index is -1, then the variable is not a duplicate of a smaller
  // index variable. It if is != from -1, then the values stored is the new
  // index of the first occurrence of the variable.
  std::vector<int> old_index_of_duplicate_to_new_index_of_first_occurrence(
      initial_num_vars, -1);
  // If == -1, then the variable is a duplicate of a smaller index variable.
  std::vector<int> old_index_to_new_index(initial_num_vars, -1);
  int num_vars = 0;
  {
    absl::flat_hash_map<int, int> first_visit;
    for (int p = 0; p < initial_num_vars; ++p) {
      const int ref = ct->table().vars(p);
      const int var = PositiveRef(ref);
      const auto& it = first_visit.find(var);
      if (it != first_visit.end()) {
        const int previous = it->second;
        old_index_of_duplicate_to_new_index_of_first_occurrence[p] = previous;
        context_->UpdateRuleStats("table: duplicate variables");
        changed = true;
      } else {
        ct->mutable_table()->set_vars(num_vars, ref);
        first_visit[var] = num_vars;
        old_index_to_new_index[p] = num_vars;
        num_vars++;
      }
    }

    if (num_vars < initial_num_vars) {
      ct->mutable_table()->mutable_vars()->Truncate(num_vars);
    }
  }

  // Check each tuple for validity w.r.t. affine relations, variable domains,
  // and consistency with duplicate variables. Reduce the size of the tuple in
  // case of duplicate variables.
  std::vector<std::vector<int64_t>> new_tuples;
  const int initial_num_tuples = ct->table().values_size() / initial_num_vars;
  std::vector<absl::flat_hash_set<int64_t>> new_domains(num_vars);

  {
    std::vector<int64_t> tuple(num_vars);
    new_tuples.reserve(initial_num_tuples);
    for (int i = 0; i < initial_num_tuples; ++i) {
      bool delete_row = false;
      std::string tmp;
      for (int j = 0; j < initial_num_vars; ++j) {
        const int64_t old_value = ct->table().values(i * initial_num_vars + j);

        // Corner case to avoid overflow, assuming the domain where already
        // propagated between a variable and its affine representative.
        if (old_value < old_var_lb[j] || old_value > old_var_ub[j]) {
          delete_row = true;
          break;
        }

        // Affine relations are defined on the initial variables.
        const AffineRelation::Relation& r = affine_relations[j];
        const int64_t value = (old_value - r.offset) / r.coeff;
        if (value * r.coeff + r.offset != old_value) {
          // Value not reachable by affine relation.
          delete_row = true;
          break;
        }
        const int mapped_position = old_index_to_new_index[j];
        if (mapped_position == -1) {  // The current variable is duplicate.
          const int new_index_of_first_occurrence =
              old_index_of_duplicate_to_new_index_of_first_occurrence[j];
          if (value != tuple[new_index_of_first_occurrence]) {
            delete_row = true;
            break;
          }
        } else {
          const int ref = ct->table().vars(mapped_position);
          if (!context_->DomainContains(ref, value)) {
            delete_row = true;
            break;
          }
          tuple[mapped_position] = value;
        }
      }
      if (delete_row) {
        changed = true;
        continue;
      }
      new_tuples.push_back(tuple);
      for (int j = 0; j < num_vars; ++j) {
        new_domains[j].insert(tuple[j]);
      }
    }
    gtl::STLSortAndRemoveDuplicates(&new_tuples);
    if (new_tuples.size() < initial_num_tuples) {
      context_->UpdateRuleStats("table: removed rows");
    }
  }

  // Update the list of tuples if needed.
  if (changed) {
    ct->mutable_table()->clear_values();
    for (const std::vector<int64_t>& t : new_tuples) {
      for (const int64_t v : t) {
        ct->mutable_table()->add_values(v);
      }
    }
  }

  // Nothing more to do for negated tables.
  if (ct->table().negated()) return changed;

  // Filter the variable domains.
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
  return changed;
}

bool CpModelPresolver::PresolveAllDiff(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  AllDifferentConstraintProto& all_diff = *ct->mutable_all_diff();

  bool constraint_has_changed = false;
  for (LinearExpressionProto& exp :
       *(ct->mutable_all_diff()->mutable_exprs())) {
    constraint_has_changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  for (;;) {
    const int size = all_diff.exprs_size();
    if (size == 0) {
      context_->UpdateRuleStats("all_diff: empty constraint");
      return RemoveConstraint(ct);
    }
    if (size == 1) {
      context_->UpdateRuleStats("all_diff: only one variable");
      return RemoveConstraint(ct);
    }

    bool something_was_propagated = false;
    std::vector<LinearExpressionProto> kept_expressions;
    for (int i = 0; i < size; ++i) {
      if (!context_->IsFixed(all_diff.exprs(i))) {
        kept_expressions.push_back(all_diff.exprs(i));
        continue;
      }

      const int64_t value = context_->MinOf(all_diff.exprs(i));
      bool propagated = false;
      for (int j = 0; j < size; ++j) {
        if (i == j) continue;
        if (context_->DomainContains(all_diff.exprs(j), value)) {
          if (!context_->IntersectDomainWith(all_diff.exprs(j),
                                             Domain(value).Complement())) {
            return true;
          }
          propagated = true;
        }
      }
      if (propagated) {
        context_->UpdateRuleStats("all_diff: propagated fixed expressions");
        something_was_propagated = true;
      }
    }

    // CanonicalizeLinearExpression() made sure that only positive variable
    // appears here, so this order will put expr and -expr one after the other.
    std::sort(
        kept_expressions.begin(), kept_expressions.end(),
        [](const LinearExpressionProto& expr_a,
           const LinearExpressionProto& expr_b) {
          DCHECK_EQ(expr_a.vars_size(), 1);
          DCHECK_EQ(expr_b.vars_size(), 1);
          const int ref_a = expr_a.vars(0);
          const int ref_b = expr_b.vars(0);
          const int64_t coeff_a = expr_a.coeffs(0);
          const int64_t coeff_b = expr_b.coeffs(0);
          const int64_t abs_coeff_a = std::abs(coeff_a);
          const int64_t abs_coeff_b = std::abs(coeff_b);
          const int64_t offset_a = expr_a.offset();
          const int64_t offset_b = expr_b.offset();
          const int64_t abs_offset_a = std::abs(offset_a);
          const int64_t abs_offset_b = std::abs(offset_b);
          return std::tie(ref_a, abs_coeff_a, coeff_a, abs_offset_a, offset_a) <
                 std::tie(ref_b, abs_coeff_b, coeff_b, abs_offset_b, offset_b);
        });

    // TODO(user): improve algorithm if of (a + offset) and (-a - offset)
    // might not be together if (a - offset) is present.

    for (int i = 1; i < kept_expressions.size(); ++i) {
      if (LinearExpressionProtosAreEqual(kept_expressions[i],
                                         kept_expressions[i - 1], 1)) {
        return context_->NotifyThatModelIsUnsat(
            "Duplicate variable in all_diff");
      }
      if (LinearExpressionProtosAreEqual(kept_expressions[i],
                                         kept_expressions[i - 1], -1)) {
        bool domain_modified = false;
        if (!context_->IntersectDomainWith(kept_expressions[i],
                                           Domain(0).Complement(),
                                           &domain_modified)) {
          return false;
        }
        if (domain_modified) {
          context_->UpdateRuleStats(
              "all_diff: remove 0 from expression appearing with its "
              "opposite.");
        }
      }
    }

    if (kept_expressions.size() < all_diff.exprs_size()) {
      all_diff.clear_exprs();
      for (const LinearExpressionProto& expr : kept_expressions) {
        *all_diff.add_exprs() = expr;
      }
      context_->UpdateRuleStats("all_diff: removed fixed variables");
      something_was_propagated = true;
      constraint_has_changed = true;
      if (kept_expressions.size() <= 1) continue;
    }

    // Propagate mandatory value if the all diff is actually a permutation.
    CHECK_GE(all_diff.exprs_size(), 2);
    Domain domain = context_->DomainSuperSetOf(all_diff.exprs(0));
    for (int i = 1; i < all_diff.exprs_size(); ++i) {
      domain = domain.UnionWith(context_->DomainSuperSetOf(all_diff.exprs(i)));
    }
    if (all_diff.exprs_size() == domain.Size()) {
      absl::flat_hash_map<int64_t, std::vector<LinearExpressionProto>>
          value_to_exprs;
      for (const LinearExpressionProto& expr : all_diff.exprs()) {
        for (const int64_t v : context_->DomainOf(expr.vars(0)).Values()) {
          value_to_exprs[expr.coeffs(0) * v + expr.offset()].push_back(expr);
        }
      }
      bool propagated = false;
      for (const auto& it : value_to_exprs) {
        if (it.second.size() == 1 && !context_->IsFixed(it.second.front())) {
          const LinearExpressionProto& expr = it.second.front();
          if (!context_->IntersectDomainWith(expr, Domain(it.first))) {
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
      new_interval->mutable_start()->set_offset(new_start);
      new_interval->mutable_size()->set_offset(new_end - new_start);
      new_interval->mutable_end()->set_offset(new_end);
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

namespace {
LinearExpressionProto ConstantExpressionProto(int64_t value) {
  LinearExpressionProto expr;
  expr.set_offset(value);
  return expr;
}
}  // namespace

bool CpModelPresolver::PresolveCumulative(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  CumulativeConstraintProto* proto = ct->mutable_cumulative();

  bool changed = CanonicalizeLinearExpression(*ct, proto->mutable_capacity());
  for (LinearExpressionProto& exp :
       *(ct->mutable_cumulative()->mutable_demands())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  const int64_t capacity_max = context_->MaxOf(proto->capacity());

  // Checks the capacity of the constraint.
  {
    bool domain_changed = false;
    if (!context_->IntersectDomainWith(
            proto->capacity(), Domain(0, capacity_max), &domain_changed)) {
      return true;
    }
    if (domain_changed) {
      context_->UpdateRuleStats("cumulative: trimmed negative capacity");
    }
  }

  {
    // Filter absent intervals, or zero demands, or demand incompatible with the
    // capacity.
    int new_size = 0;
    int num_zero_demand_removed = 0;
    int num_zero_size_removed = 0;
    int num_incompatible_demands = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      if (context_->ConstraintIsInactive(proto->intervals(i))) continue;

      const LinearExpressionProto& demand_expr = proto->demands(i);
      const int64_t demand_max = context_->MaxOf(demand_expr);
      if (demand_max == 0) {
        num_zero_demand_removed++;
        continue;
      }

      if (context_->SizeMax(proto->intervals(i)) == 0) {
        // Size 0 intervals cannot contribute to a cumulative.
        num_zero_size_removed++;
        continue;
      }

      if (context_->MinOf(demand_expr) > capacity_max) {
        if (context_->ConstraintIsOptional(proto->intervals(i))) {
          ConstraintProto* interval_ct =
              context_->working_model->mutable_constraints(proto->intervals(i));
          DCHECK_EQ(interval_ct->enforcement_literal_size(), 1);
          const int literal = interval_ct->enforcement_literal(0);
          if (!context_->SetLiteralToFalse(literal)) {
            return true;
          }
          num_incompatible_demands++;
          continue;
        } else {  // Interval is performed.
          return context_->NotifyThatModelIsUnsat(
              "cumulative: performed demand exceeds capacity.");
        }
      }

      proto->set_intervals(new_size, proto->intervals(i));
      *proto->mutable_demands(new_size) = proto->demands(i);
      new_size++;
    }

    if (new_size < proto->intervals_size()) {
      changed = true;
      proto->mutable_intervals()->Truncate(new_size);
      proto->mutable_demands()->erase(
          proto->mutable_demands()->begin() + new_size,
          proto->mutable_demands()->end());
    }

    if (num_zero_demand_removed > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals with no demands");
    }
    if (num_zero_size_removed > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals with a size of zero");
    }
    if (num_incompatible_demands > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals demands greater than the capacity");
    }
  }

  // Checks the compatibility of demands w.r.t. the capacity.
  {
    for (int i = 0; i < proto->demands_size(); ++i) {
      const int interval = proto->intervals(i);
      const LinearExpressionProto& demand_expr = proto->demands(i);
      if (context_->ConstraintIsOptional(interval)) continue;
      bool domain_changed = false;
      if (!context_->IntersectDomainWith(demand_expr, {0, capacity_max},
                                         &domain_changed)) {
        return true;
      }
      if (domain_changed) {
        context_->UpdateRuleStats(
            "cumulative: fit demand in [0..capacity_max]");
      }
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
          *new_cumulative->add_demands() = proto->demands(i);
        }
        *new_cumulative->mutable_capacity() = proto->capacity();
      }
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("cumulative: split into disjoint components");
      return RemoveConstraint(ct);
    }
  }

  // TODO(user): move the algorithmic part of what we do below in a
  // separate function to unit test it more properly.
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
    // to max_profile height. And for each time in our discrete set of
    // time_exprs (all the start_min and end_max) we count for how often the
    // height was above the capacity before this time.
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
    // "Presolving techniques and linear relaxations for cumulative
    // scheduling" PhD dissertation by Stefan Heinz, ZIB.
    int new_size = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      const int index = proto->intervals(i);
      const int64_t start_min = context_->StartMin(index);
      const int64_t end_max = context_->EndMax(index);

      // In the cumulative, if start_min == end_max, the interval is of size
      // zero and we can just ignore it. If the model is unsat or the interval
      // must be absent (start_min > end_max), this should be dealt with at
      // the interval constraint level and we can just remove it from here.
      //
      // Note that currently, the interpretation for interval of length zero
      // is different for the no-overlap constraint.
      if (start_min >= end_max) continue;

      // Note that by construction, both point are in the map. The formula
      // counts exactly for how many time_exprs in [start_min, end_max), we have
      // a point in our discrete set of time that exceeded the capacity. Because
      // we included all the relevant points, this works.
      const int num_diff = num_possible_overloads_before.at(end_max) -
                           num_possible_overloads_before.at(start_min);
      if (num_diff == 0) continue;

      proto->set_intervals(new_size, proto->intervals(i));
      *proto->mutable_demands(new_size) = proto->demands(i);
      new_size++;
    }

    if (new_size < proto->intervals_size()) {
      changed = true;
      proto->mutable_intervals()->Truncate(new_size);
      proto->mutable_demands()->erase(
          proto->mutable_demands()->begin() + new_size,
          proto->mutable_demands()->end());
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

      const LinearExpressionProto& demand_expr = proto->demands(i);
      sum_of_max_demands += context_->MaxOf(demand_expr);

      if (interval_ct.enforcement_literal().empty()) {
        max_of_performed_demand_mins = std::max(max_of_performed_demand_mins,
                                                context_->MinOf(demand_expr));
      }
    }

    const LinearExpressionProto& capacity_expr = proto->capacity();
    if (max_of_performed_demand_mins > context_->MinOf(capacity_expr)) {
      context_->UpdateRuleStats("cumulative: propagate min capacity.");
      if (!context_->IntersectDomainWith(
              capacity_expr, Domain(max_of_performed_demand_mins,
                                    std::numeric_limits<int64_t>::max()))) {
        return true;
      }
    }

    if (max_of_performed_demand_mins > context_->MaxOf(capacity_expr)) {
      context_->UpdateRuleStats("cumulative: cannot fit performed demands");
      return context_->NotifyThatModelIsUnsat();
    }

    if (sum_of_max_demands <= context_->MinOf(capacity_expr)) {
      context_->UpdateRuleStats("cumulative: capacity exceeds sum of demands");
      return RemoveConstraint(ct);
    }
  }

  if (context_->IsFixed(proto->capacity())) {
    int64_t gcd = 0;
    for (int i = 0; i < ct->cumulative().demands_size(); ++i) {
      const LinearExpressionProto& demand_expr = ct->cumulative().demands(i);
      if (!context_->IsFixed(demand_expr)) {
        // Abort if the demand is not fixed.
        gcd = 1;
        break;
      }
      gcd = MathUtil::GCD64(gcd, context_->MinOf(demand_expr));
      if (gcd == 1) break;
    }
    if (gcd > 1) {
      changed = true;
      for (int i = 0; i < ct->cumulative().demands_size(); ++i) {
        const int64_t demand = context_->MinOf(ct->cumulative().demands(i));
        *proto->mutable_demands(i) = ConstantExpressionProto(demand / gcd);
      }

      const int64_t old_capacity = context_->MinOf(proto->capacity());
      *proto->mutable_capacity() = ConstantExpressionProto(old_capacity / gcd);
      context_->UpdateRuleStats(
          "cumulative: divide demands and capacity by gcd");
    }
  }

  const int num_intervals = proto->intervals_size();
  const LinearExpressionProto& capacity_expr = proto->capacity();

  std::vector<LinearExpressionProto> start_exprs(num_intervals);

  int num_duration_one = 0;
  int num_greater_half_capacity = 0;

  bool has_optional_interval = false;
  bool all_starts_are_variables = true;
  for (int i = 0; i < num_intervals; ++i) {
    const int index = proto->intervals(i);
    // TODO(user): adapt in the presence of optional intervals.
    if (context_->ConstraintIsOptional(index)) has_optional_interval = true;
    const ConstraintProto& ct =
        context_->working_model->constraints(proto->intervals(i));
    const IntervalConstraintProto& interval = ct.interval();
    start_exprs[i] = interval.start();

    const LinearExpressionProto& demand_expr = proto->demands(i);
    if (context_->SizeMin(index) == 1 && context_->SizeMax(index) == 1) {
      num_duration_one++;
    }
    if (context_->SizeMin(index) == 0) {
      // The behavior for zero-duration interval is currently not the same in
      // the no-overlap and the cumulative constraint.
      return changed;
    }
    const int64_t demand_min = context_->MinOf(demand_expr);
    const int64_t demand_max = context_->MaxOf(demand_expr);
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
                demand_expr,
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
        all_starts_are_variables) {
      context_->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_all_diff();
      for (const LinearExpressionProto& expr : start_exprs) {
        *arg->add_exprs() = expr;
      }
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("cumulative: convert to no_overlap");
      // Before we remove the cumulative, add constraints to enforce that the
      // capacity is greater than the demand of any performed intervals.
      for (int i = 0; i < proto->demands_size(); ++i) {
        const LinearExpressionProto& demand_expr = proto->demands(i);
        const int64_t demand_max = context_->MaxOf(demand_expr);
        if (demand_max > context_->MinOf(capacity_expr)) {
          ConstraintProto* capacity_gt =
              context_->working_model->add_constraints();
          *capacity_gt->mutable_enforcement_literal() =
              context_->working_model->constraints(proto->intervals(i))
                  .enforcement_literal();
          capacity_gt->mutable_linear()->add_domain(0);
          capacity_gt->mutable_linear()->add_domain(
              std::numeric_limits<int64_t>::max());
          AddLinearExpressionToLinearConstraint(capacity_expr, 1,
                                                capacity_gt->mutable_linear());
          AddLinearExpressionToLinearConstraint(demand_expr, -1,
                                                capacity_gt->mutable_linear());
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

  const int old_size = proto.literals_size();
  int new_size = 0;
  std::vector<bool> has_incoming_or_outgoing_arcs;
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
    if (tail >= has_incoming_or_outgoing_arcs.size()) {
      has_incoming_or_outgoing_arcs.resize(tail + 1, false);
    }
    if (head >= has_incoming_or_outgoing_arcs.size()) {
      has_incoming_or_outgoing_arcs.resize(head + 1, false);
    }
    has_incoming_or_outgoing_arcs[tail] = true;
    has_incoming_or_outgoing_arcs[head] = true;
  }

  if (old_size > 0 && new_size == 0) {
    // A routes constraint cannot have a self loop on 0. Therefore, if there
    // were arcs, it means it contains non zero nodes. Without arc, the
    // constraint is unfeasible.
    return context_->NotifyThatModelIsUnsat(
        "routes: graph with nodes and no arcs");
  }

  if (new_size < num_arcs) {
    proto.mutable_literals()->Truncate(new_size);
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    return true;
  }

  // if a node misses an incomping or outgoing arc, the model is trivially
  // infeasible.
  for (int n = 0; n < has_incoming_or_outgoing_arcs.size(); ++n) {
    if (!has_incoming_or_outgoing_arcs[n]) {
      return context_->NotifyThatModelIsUnsat(absl::StrCat(
          "routes: node ", n, " misses incoming or outgoing arcs"));
    }
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

  // All the node must have some incoming and outgoing arcs.
  for (int i = 0; i < num_nodes; ++i) {
    if (incoming_arcs[i].empty() || outgoing_arcs[i].empty()) {
      return MarkConstraintAsFalse(ct);
    }
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
      std::vector<bool> has_self_arc(num_nodes, false);
      for (int i = 0; i < num_arcs; ++i) {
        if (visited[proto.tails(i)]) continue;
        if (proto.tails(i) == proto.heads(i)) {
          has_self_arc[proto.tails(i)] = true;
          if (!context_->SetLiteralToTrue(proto.literals(i))) return true;
        } else {
          if (!context_->SetLiteralToFalse(proto.literals(i))) return true;
        }
      }
      for (int n = 0; n < num_nodes; ++n) {
        if (!visited[n] && !has_self_arc[n]) {
          // We have a subircuit, but it doesn't cover all the mandatory nodes.
          return MarkConstraintAsFalse(ct);
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

  bool all_have_same_affine_relation = true;
  std::vector<AffineRelation::Relation> affine_relations;
  for (int v = 0; v < proto.vars_size(); ++v) {
    const int var = ct->automaton().vars(v);
    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    affine_relations.push_back(r);
    if (r.representative == var) {
      all_have_same_affine_relation = false;
      break;
    }
    if (v > 0 && (r.coeff != affine_relations[v - 1].coeff ||
                  r.offset != affine_relations[v - 1].offset)) {
      all_have_same_affine_relation = false;
      break;
    }
  }

  if (all_have_same_affine_relation) {  // Unscale labels.
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

  ReservoirConstraintProto& proto = *ct->mutable_reservoir();
  bool changed = false;
  for (LinearExpressionProto& exp : *(proto.mutable_time_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  if (proto.active_literals().empty()) {
    const int true_literal = context_->GetOrCreateConstantVar(1);
    for (int i = 0; i < proto.time_exprs_size(); ++i) {
      proto.add_active_literals(true_literal);
    }
    changed = true;
  }

  const auto& demand_is_null = [&](int i) {
    return proto.level_changes(i) == 0 ||
           context_->LiteralIsFalse(proto.active_literals(i));
  };

  // Remove zero level_changes, and inactive events.
  int num_zeros = 0;
  for (int i = 0; i < proto.level_changes_size(); ++i) {
    if (demand_is_null(i)) num_zeros++;
  }

  if (num_zeros > 0) {  // Remove null events
    changed = true;
    int new_size = 0;
    for (int i = 0; i < proto.level_changes_size(); ++i) {
      if (demand_is_null(i)) continue;
      proto.set_level_changes(new_size, proto.level_changes(i));
      *proto.mutable_time_exprs(new_size) = proto.time_exprs(i);
      proto.set_active_literals(new_size, proto.active_literals(i));
      new_size++;
    }

    proto.mutable_level_changes()->Truncate(new_size);
    proto.mutable_time_exprs()->erase(
        proto.mutable_time_exprs()->begin() + new_size,
        proto.mutable_time_exprs()->end());
    proto.mutable_active_literals()->Truncate(new_size);

    context_->UpdateRuleStats(
        "reservoir: remove zero level_changes or inactive events.");
  }

  const int num_events = proto.level_changes_size();
  int64_t gcd =
      proto.level_changes().empty() ? 0 : std::abs(proto.level_changes(0));
  int num_positives = 0;
  int num_negatives = 0;
  int64_t max_sum_of_positive_level_changes = 0;
  int64_t min_sum_of_negative_level_changes = 0;
  for (int i = 0; i < num_events; ++i) {
    const int64_t demand = proto.level_changes(i);
    gcd = MathUtil::GCD64(gcd, std::abs(demand));
    if (demand > 0) {
      num_positives++;
      max_sum_of_positive_level_changes += demand;
    } else {
      DCHECK_LT(demand, 0);
      num_negatives++;
      min_sum_of_negative_level_changes += demand;
    }
  }

  if (min_sum_of_negative_level_changes >= proto.min_level() &&
      max_sum_of_positive_level_changes <= proto.max_level()) {
    context_->UpdateRuleStats("reservoir: always feasible");
    return RemoveConstraint(ct);
  }

  if (min_sum_of_negative_level_changes > proto.max_level() ||
      max_sum_of_positive_level_changes < proto.min_level()) {
    context_->UpdateRuleStats("reservoir: trivially infeasible");
    return context_->NotifyThatModelIsUnsat();
  }

  if (min_sum_of_negative_level_changes > proto.min_level()) {
    proto.set_min_level(min_sum_of_negative_level_changes);
    context_->UpdateRuleStats(
        "reservoir: increase min_level to reachable value");
  }

  if (max_sum_of_positive_level_changes < proto.max_level()) {
    proto.set_max_level(max_sum_of_positive_level_changes);
    context_->UpdateRuleStats("reservoir: reduce max_level to reachable value");
  }

  if (proto.min_level() <= 0 && proto.max_level() >= 0 &&
      (num_positives == 0 || num_negatives == 0)) {
    // If all level_changes have the same sign, and if the initial state is
    // always feasible, we do not care about the order, just the sum.
    auto* const sum =
        context_->working_model->add_constraints()->mutable_linear();
    int64_t fixed_contrib = 0;
    for (int i = 0; i < proto.level_changes_size(); ++i) {
      const int64_t demand = proto.level_changes(i);
      DCHECK_NE(demand, 0);

      const int active = proto.active_literals(i);
      if (RefIsPositive(active)) {
        sum->add_vars(active);
        sum->add_coeffs(demand);
      } else {
        sum->add_vars(PositiveRef(active));
        sum->add_coeffs(-demand);
        fixed_contrib += demand;
      }
    }
    sum->add_domain(proto.min_level() - fixed_contrib);
    sum->add_domain(proto.max_level() - fixed_contrib);
    context_->UpdateRuleStats("reservoir: converted to linear");
    return RemoveConstraint(ct);
  }

  if (gcd > 1) {
    for (int i = 0; i < proto.level_changes_size(); ++i) {
      proto.set_level_changes(i, proto.level_changes(i) / gcd);
    }

    // Adjust min and max levels.
    //   max level is always rounded down.
    //   min level is always rounded up.
    const Domain reduced_domain = Domain({proto.min_level(), proto.max_level()})
                                      .InverseMultiplicationBy(gcd);
    proto.set_min_level(reduced_domain.Min());
    proto.set_max_level(reduced_domain.Max());
    context_->UpdateRuleStats(
        "reservoir: simplify level_changes and levels by gcd.");
  }

  if (num_positives == 1 && num_negatives > 0) {
    context_->UpdateRuleStats(
        "TODO reservoir: one producer, multiple consumers.");
  }

  absl::flat_hash_set<std::tuple<int, int64_t, int64_t, int>> time_active_set;
  for (int i = 0; i < proto.level_changes_size(); ++i) {
    const LinearExpressionProto& time = proto.time_exprs(i);
    const int var = context_->IsFixed(time) ? std::numeric_limits<int>::min()
                                            : time.vars(0);
    const int64_t coeff = context_->IsFixed(time) ? 0 : time.coeffs(0);
    const std::tuple<int, int64_t, int64_t, int> key = std::make_tuple(
        var, coeff,
        context_->IsFixed(time) ? context_->FixedValue(time) : time.offset(),
        proto.active_literals(i));
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

// TODO(user): It might make sense to run this in parallel. The same apply for
// other expansive and self-contains steps like symmetry detection, etc...
void CpModelPresolver::Probe() {
  Model model;
  if (!LoadModelForProbing(context_, &model)) return;

  // Probe.
  //
  // TODO(user): Compute the transitive reduction instead of just the
  // equivalences, and use the newly learned binary clauses?
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* mapping = model.GetOrCreate<CpModelMapping>();
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
  // TODO(user): Reenable some SAT presolve with
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
  if (params.debug_postsolve_with_full_solver()) {
    params.set_presolve_blocked_clause(false);
  }

  // TODO(user): BVA takes time_exprs and do not seems to help on the minizinc
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

  // The objective is already loaded in the context, but we re-canonicalize
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
  int last_expanded_objective_var;
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
      // Update the objective map. Note that the division is possible because
      // currently we only expand with coeff with a magnitude of 1.
      CHECK_EQ(std::abs(objective_coeff_in_expanded_constraint), 1);
      const ConstraintProto& ct =
          context_->working_model->constraints(expanded_linear_index);
      if (!context_->SubstituteVariableInObjective(
              objective_var, objective_coeff_in_expanded_constraint, ct,
              &new_vars_in_objective)) {
        if (context_->ModelIsUnsat()) return;
        continue;
      }

      context_->UpdateRuleStats("objective: expanded objective constraint.");

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
          context_->MarkVariableAsRemoved(objective_var);
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
      last_expanded_objective_var = objective_var;
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
    context_->MarkVariableAsRemoved(last_expanded_objective_var);
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

namespace {

bool IsAffineIntAbs(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinMax ||
      ct->lin_max().exprs_size() != 2 ||
      ct->lin_max().target().vars_size() > 1 ||
      ct->lin_max().exprs(0).vars_size() != 1 ||
      ct->lin_max().exprs(1).vars_size() != 1) {
    return false;
  }

  const LinearArgumentProto& lin_max = ct->lin_max();
  if (lin_max.exprs(0).offset() != -lin_max.exprs(1).offset()) return false;
  if (PositiveRef(lin_max.exprs(0).vars(0)) !=
      PositiveRef(lin_max.exprs(1).vars(0))) {
    return false;
  }

  const int64_t left_coeff = RefIsPositive(lin_max.exprs(0).vars(0))
                                 ? lin_max.exprs(0).coeffs(0)
                                 : -lin_max.exprs(0).coeffs(0);
  const int64_t right_coeff = RefIsPositive(lin_max.exprs(1).vars(0))
                                  ? lin_max.exprs(1).coeffs(0)
                                  : -lin_max.exprs(1).coeffs(0);
  return left_coeff == -right_coeff;
}

}  // namespace

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
    case ConstraintProto::ConstraintCase::kLinMax:
      if (CanonicalizeLinMax(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (IsAffineIntAbs(ct)) {
        return PresolveIntAbs(ct);
      } else {
        return PresolveLinMax(ct);
      }
    case ConstraintProto::ConstraintCase::kIntProd:
      return PresolveIntProd(ct);
    case ConstraintProto::ConstraintCase::kIntDiv:
      return PresolveIntDiv(ct);
    case ConstraintProto::ConstraintCase::kIntMod:
      return PresolveIntMod(ct);
    case ConstraintProto::ConstraintCase::kLinear: {
      // In the presence of affine relation, it is possible that the sign of a
      // variable change during canonicalization, and a variable that could
      // freely move in one direction can no longer do so. So we make sure we
      // always remove c from all the maps before re-inserting it in
      // PropagateDomainsInLinear().
      //
      // TODO(user): The move in only one direction code is redundant with the
      // dual bound strengthening code. So maybe we don't need both.
      for (const int ref : ct->linear().vars()) {
        const int var = PositiveRef(ref);
        context_->var_to_lb_only_constraints[var].erase(c);
        context_->var_to_ub_only_constraints[var].erase(c);
      }

      if (CanonicalizeLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PropagateDomainsInLinear(c, ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PresolveSmallLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PresolveLinearEqualityWithModulo(ct)) {
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
      if (PresolveSmallLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PresolveLinearOnBooleans(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (ct->constraint_case() == ConstraintProto::kLinear) {
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
      }

      // Note that it is important for this to be last, so that if a constraint
      // is marked as being in one direction, no other presolve is applied until
      // it is processed again and unmarked at the beginning of this case.
      if (ct->constraint_case() == ConstraintProto::kLinear &&
          DetectAndProcessOneSidedLinearConstraint(c, ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      return false;
    }
    case ConstraintProto::ConstraintCase::kInterval:
      return PresolveInterval(c, ct);
    case ConstraintProto::ConstraintCase::kInverse:
      return PresolveInverse(ct);
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

// This is called with constraint c1 whose literals are included in the literals
// of c2.
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

  if (ct1->constraint_case() == ConstraintProto::kExactlyOne &&
      ct2->constraint_case() == ConstraintProto::kLinear) {
    // If we have an exactly one in a linear, we can shift the coefficients of
    // all these variables by any constant value. For now, since we only deal
    // with positive coefficient, we shift by the min.
    //
    // TODO(user): It might be more interesting to maximize the number of terms
    // that are shifted to zero to reduce the constraint size.
    absl::flat_hash_set<int> literals(ct1->exactly_one().literals().begin(),
                                      ct1->exactly_one().literals().end());
    int64_t min_coeff = std::numeric_limits<int64_t>::max();
    int num_matches = 0;
    for (int i = 0; i < ct2->linear().vars().size(); ++i) {
      const int var = ct2->linear().vars(i);
      if (literals.contains(var)) {
        ++num_matches;
        min_coeff = std::min(min_coeff, std::abs(ct2->linear().coeffs(i)));
      }
    }

    // If a linear constraint contains more than one at most one, after
    // processing one, we might no longer have an inclusion.
    if (num_matches != literals.size()) return true;

    // TODO(user): It would be cool to propagate other variable domains with
    // the knowledge that the partial sum in is [min_coeff, max_coeff]. I am
    // a bit relunctant to duplicate the code for that here.
    int new_size = 0;
    for (int i = 0; i < ct2->linear().vars().size(); ++i) {
      const int var = ct2->linear().vars(i);
      int64_t coeff = ct2->linear().coeffs(i);
      if (literals.contains(var)) {
        CHECK_GE(coeff, 0);
        if (coeff == min_coeff) continue;  // delete term.
        coeff -= min_coeff;
      }
      ct2->mutable_linear()->set_vars(new_size, var);
      ct2->mutable_linear()->set_coeffs(new_size, coeff);
      ++new_size;
    }

    ct2->mutable_linear()->mutable_vars()->Truncate(new_size);
    ct2->mutable_linear()->mutable_coeffs()->Truncate(new_size);
    FillDomainInProto(
        ReadDomainFromProto(ct2->linear()).AdditionWith(Domain(-min_coeff)),
        ct2->mutable_linear());
    context_->UpdateConstraintVariableUsage(original_constraint_index[c2]);
    context_->UpdateRuleStats("setppc: reduced linear coefficients.");
  }

  // We can't deduce anything in the last remaining case:
  // ct1->constraint_case() == ConstraintProto::kAtMostOne &&
  // ct2->constraint_case() == ConstraintProto::kBoolOr

  return true;
}

// TODO(user): TransformIntoMaxCliques() convert the bool_and to
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
  std::vector<int> temp_literals;
  if (context_->ModelIsUnsat()) return false;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::kBoolOr ||
        ct->constraint_case() == ConstraintProto::kAtMostOne ||
        ct->constraint_case() == ConstraintProto::kExactlyOne) {
      // Because TransformIntoMaxCliques() can detect literal equivalence
      // relation, we make sure the constraints are presolved before being
      // inspected.
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return false;
      constraint_literals.push_back(GetLiteralsFromSetPPCConstraint(*ct));
    } else if (ct->constraint_case() == ConstraintProto::kLinear) {
      // We also want to test inclusion with the pseudo-Boolean part of
      // linear constraints of size at least 3. Exactly one of size two are
      // equivalent literals, and we already deal with this case.
      //
      // TODO(user): This is not ideal as we currently only process exactly one
      // included into linear, and we add overhead by detecting all the other
      // cases that we ignore later. That said, we could just propagate a bit
      // more the domain if we know at_least_one or at_most_one between literals
      // in a linear constraint.
      const int size = ct->linear().vars().size();
      if (size <= 2) continue;

      // TODO(user): We only deal with every literal having a positive coeff
      // here. We should probably do the same with all negative. We can also
      // consider NegatedRef(var).
      temp_literals.clear();
      for (int i = 0; i < size; ++i) {
        const int var = ct->linear().vars(i);
        const int64_t coeff = ct->linear().coeffs(i);
        if (!context_->CanBeUsedAsLiteral(var)) continue;
        if (!RefIsPositive(var)) continue;
        if (coeff < 0) continue;
        temp_literals.push_back(var);
      }
      if (temp_literals.size() <= 2) continue;
      constraint_literals.push_back(temp_literals);
    } else {
      continue;
    }

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
        if (compared_constraints.contains(std::pair<int, int>(c1, c2))) {
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

// Note that because we remove the linear constraint, this will not be called
// often, so it is okay to use "heavy" data structure here.
//
// TODO(user): in the at most one case, consider always creating an associated
// literal (l <=> var == rhs), and add the exactly_one = at_most_one U not(l)?
// This constraint is implicit from what we create, however internally we will
// not recover it easily, so we might not add the linear relaxation
// corresponding to the constraint we just removed.
bool CpModelPresolver::ProcessEncodingFromLinear(
    const int linear_encoding_ct_index,
    const ConstraintProto& at_most_or_exactly_one, int64_t* num_unique_terms,
    int64_t* num_multiple_terms) {
  // Preprocess exactly or at most one.
  bool in_exactly_one = false;
  absl::flat_hash_map<int, int> var_to_ref;
  if (at_most_or_exactly_one.constraint_case() == ConstraintProto::kAtMostOne) {
    for (const int ref : at_most_or_exactly_one.at_most_one().literals()) {
      CHECK(!var_to_ref.contains(PositiveRef(ref)));
      var_to_ref[PositiveRef(ref)] = ref;
    }
  } else {
    in_exactly_one = true;
    for (const int ref : at_most_or_exactly_one.exactly_one().literals()) {
      CHECK(!var_to_ref.contains(PositiveRef(ref)));
      var_to_ref[PositiveRef(ref)] = ref;
    }
  }

  // Preprocess the linear constraints.
  const ConstraintProto& linear_encoding =
      context_->working_model->constraints(linear_encoding_ct_index);
  int64_t rhs = linear_encoding.linear().domain(0);
  int target_ref = std::numeric_limits<int>::min();
  std::vector<std::pair<int, int64_t>> ref_to_coeffs;
  const int num_terms = linear_encoding.linear().vars().size();
  for (int i = 0; i < num_terms; ++i) {
    const int ref = linear_encoding.linear().vars(i);
    const int64_t coeff = linear_encoding.linear().coeffs(i);
    const auto it = var_to_ref.find(PositiveRef(ref));

    if (it == var_to_ref.end()) {
      CHECK_EQ(target_ref, std::numeric_limits<int>::min()) << "Uniqueness ";
      CHECK_EQ(std::abs(coeff), 1);
      target_ref = coeff == 1 ? ref : NegatedRef(ref);
      continue;
    }

    // We transform the constraint so that the Boolean reference match exactly
    // what is in the at most one.
    if (it->second == ref) {
      // The term in the constraint is the same as in the at_most_one.
      ref_to_coeffs.push_back({ref, coeff});
    } else {
      // We replace "coeff * ref" by "coeff - coeff * (1 - ref)"
      rhs -= coeff;
      ref_to_coeffs.push_back({NegatedRef(ref), -coeff});
    }
  }
  if (target_ref == std::numeric_limits<int>::min() ||
      context_->CanBeUsedAsLiteral(target_ref)) {
    // We didn't find the unique integer variable. This might have happenned
    // because by processing other encoding we might end up with a fully boolean
    // constraint. Just abort, it will be presolved later.
    context_->UpdateRuleStats("encoding: candidate linear is all Boolean now.");
    return true;
  }

  // Extract the encoding.
  std::vector<int64_t> all_values;
  std::map<int64_t, std::vector<int>> value_to_refs;
  for (const auto& [ref, coeff] : ref_to_coeffs) {
    const int64_t value = rhs - coeff;
    all_values.push_back(value);
    value_to_refs[value].push_back(ref);
    var_to_ref.erase(PositiveRef(ref));
  }
  // The one not used "encodes" the rhs value.
  for (const auto& [var, ref] : var_to_ref) {
    all_values.push_back(rhs);
    value_to_refs[rhs].push_back(ref);
  }
  if (!in_exactly_one) {
    // To cover the corner case when the inclusion is an equality. For an at
    // most one, the rhs should be always reachable when all Boolean are false.
    all_values.push_back(rhs);
  }

  // Make sure the target domain is up to date.
  const Domain new_domain = Domain::FromValues(all_values);
  bool domain_reduced = false;
  if (!context_->IntersectDomainWith(target_ref, new_domain, &domain_reduced)) {
    return false;
  }
  if (domain_reduced) {
    context_->UpdateRuleStats("encoding: reduced target domain");
  }

  if (context_->CanBeUsedAsLiteral(target_ref)) {
    // If target is now a literal, lets not process it here.
    context_->UpdateRuleStats("encoding: candidate linear is all Boolean now.");
    return true;
  }

  // Encode the encoding.
  absl::flat_hash_set<int64_t> value_set;
  for (const int64_t v : context_->DomainOf(target_ref).Values()) {
    value_set.insert(v);
  }
  for (const auto& [value, literals] : value_to_refs) {
    // If the value is not in the domain, just set all literal to false.
    if (!value_set.contains(value)) {
      for (const int lit : literals) {
        if (!context_->SetLiteralToFalse(lit)) return false;
      }
      continue;
    }

    if (literals.size() == 1 && (in_exactly_one || value != rhs)) {
      // Optimization if there is just one literal for this value.
      // Note that for the "at most one" case, we can't do that for the rhs.
      ++*num_unique_terms;
      if (!context_->InsertVarValueEncoding(literals[0], target_ref, value)) {
        return false;
      }
    } else {
      ++*num_multiple_terms;
      const int associated_lit =
          context_->GetOrCreateVarValueEncoding(target_ref, value);
      for (const int lit : literals) {
        context_->AddImplication(lit, associated_lit);
      }

      // All false means associated_lit is false too.
      // But not for the rhs case if we are not in exactly one.
      if (in_exactly_one || value != rhs) {
        // TODO(user): Insted of bool_or + implications, we could add an
        // exactly one! Experiment with this. In particular it might capture
        // more structure for later heuristic to add the exactly one instead.
        // This also applies to automata/table/element expansion.
        auto* bool_or =
            context_->working_model->add_constraints()->mutable_bool_or();
        for (const int lit : literals) bool_or->add_literals(lit);
        bool_or->add_literals(NegatedRef(associated_lit));
      }
    }
  }

  // Remove linear constraint now that it is fully encoded.
  context_->working_model->mutable_constraints(linear_encoding_ct_index)
      ->Clear();
  context_->UpdateNewConstraintsVariableUsage();
  context_->UpdateConstraintVariableUsage(linear_encoding_ct_index);
  return true;
}

namespace {

// We want something that is order invariant and is compatible with inclusion.
uint64_t ComputeSignature(const std::vector<int>& integers) {
  uint64_t signature = 0;
  for (const int i : integers) signature |= (int64_t{1} << (i & 63));
  return signature;
}

}  // namespace

void CpModelPresolver::ExtractEncodingFromLinear() {
  if (context_->time_limit()->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;

  WallTimer wall_timer;
  wall_timer.Start();

  struct Superset {
    int c;  // Index of the constraint.
    std::vector<int> vars;
    bool is_exactly_one;
  };
  std::vector<Superset> potential_supersets;

  struct Subset {
    int c;  // Index of the linear constraint.
    std::vector<int> vars;
  };
  std::vector<Subset> potential_subsets;

  // Loop over the constraints and fill the structures above.
  const int num_constraints = context_->working_model->constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kAtMostOne: {
        std::vector<int> vars;
        for (const int ref : ct.at_most_one().literals()) {
          vars.push_back(PositiveRef(ref));
        }
        potential_supersets.push_back({c, std::move(vars), false});
        break;
      }
      case ConstraintProto::kExactlyOne: {
        std::vector<int> vars;
        for (const int ref : ct.exactly_one().literals()) {
          vars.push_back(PositiveRef(ref));
        }
        potential_supersets.push_back({c, std::move(vars), true});
        break;
      }
      case ConstraintProto::kLinear: {
        // We only consider equality with no enforcement.
        if (!ct.enforcement_literal().empty()) continue;
        if (ct.linear().domain().size() != 2) continue;
        if (ct.linear().domain(0) != ct.linear().domain(1)) continue;

        // We also want a single non-Boolean.
        // Note that this assume the constraint is canonicalized.
        std::vector<int> vars;
        bool is_candidate = true;
        int num_integers = 0;
        const int num_terms = ct.linear().vars().size();
        for (int i = 0; i < num_terms; ++i) {
          const int ref = ct.linear().vars(i);
          if (context_->CanBeUsedAsLiteral(ref)) {
            vars.push_back(PositiveRef(ref));
          } else {
            ++num_integers;
            if (std::abs(ct.linear().coeffs(i)) != 1) {
              is_candidate = false;
              break;
            }
            if (num_integers == 2) {
              is_candidate = false;
              break;
            }
          }
        }

        // We ignore cases with just one Boolean as this should be already dealt
        // with elsewhere.
        if (is_candidate && num_integers == 1 && vars.size() > 1) {
          potential_subsets.push_back({c, std::move(vars)});
        }
        break;
      }
      default:
        break;
    }
  }

  if (potential_supersets.empty()) return;
  if (potential_subsets.empty()) return;

  // Sort by size. We also want to process exactly_ones first in case a linear
  // constraint is also included in an at_most_one of the same size.
  std::sort(potential_supersets.begin(), potential_supersets.end(),
            [](const Superset& a, const Superset& b) {
              const int size_a = a.vars.size();
              const int size_b = b.vars.size();
              return std::tie(size_a, a.is_exactly_one) <
                     std::tie(size_b, b.is_exactly_one);
            });
  std::sort(potential_subsets.begin(), potential_subsets.end(),
            [](const Subset& a, const Subset& b) {
              return a.vars.size() < b.vars.size();
            });

  // Stats.
  int64_t num_exactly_one_encodings = 0;
  int64_t num_at_most_one_encodings = 0;
  int64_t num_literals = 0;
  int64_t num_unique_terms = 0;
  int64_t num_multiple_terms = 0;

  // Structure for efficient inclusion detection.
  //
  // TODO(user): Factor this and ComputeSignature() out and merge the 3 places
  // where we do something similar.
  int subset_index = 0;
  std::vector<uint64_t> signatures;
  std::vector<std::vector<int>> one_watcher;
  std::vector<bool> is_in_superset;
  for (const Superset& superset : potential_supersets) {
    // Include all the potential subset.
    const int superset_size = superset.vars.size();
    for (; subset_index < potential_subsets.size(); ++subset_index) {
      const std::vector<int>& vars = potential_subsets[subset_index].vars;
      if (vars.size() > superset_size) break;

      // Choose to watch the one with smallest list.
      int best_choice = -1;
      for (const int var : vars) {
        if (one_watcher.size() <= var) one_watcher.resize(var + 1);
        if (best_choice == -1 ||
            one_watcher[var].size() < one_watcher[best_choice].size()) {
          best_choice = var;
        }
      }
      one_watcher[best_choice].push_back(subset_index);
      CHECK_EQ(signatures.size(), subset_index);
      signatures.push_back(ComputeSignature(vars));
    }

    // Find any subset included in current superset.
    DCHECK(absl::c_all_of(is_in_superset, [](bool b) { return !b; }));
    for (const int var : superset.vars) {
      if (var >= is_in_superset.size()) {
        is_in_superset.resize(var + 1, false);
      }
      is_in_superset[var] = true;
    }
    {
      const int max_size = std::max(one_watcher.size(), is_in_superset.size());
      one_watcher.resize(max_size);
      is_in_superset.resize(max_size, false);
    }

    const uint64_t superset_signature = ComputeSignature(superset.vars);
    for (const int superset_var : superset.vars) {
      for (int i = 0; i < one_watcher[superset_var].size(); ++i) {
        const int subset_index = one_watcher[superset_var][i];
        const Subset& subset = potential_subsets[subset_index];
        CHECK_LE(subset.vars.size(), superset_size);

        // Quick check with signature.
        if ((signatures[subset_index] & ~superset_signature) != 0) continue;

        // Long check with bitset.
        bool is_included = true;
        for (const int subset_var : subset.vars) {
          if (!is_in_superset[subset_var]) {
            is_included = false;
            break;
          }
        }
        if (!is_included) continue;

        if (!superset.is_exactly_one) {
          ++num_at_most_one_encodings;
        } else {
          ++num_exactly_one_encodings;
        }
        num_literals += subset.vars.size();
        context_->UpdateRuleStats("encoding: extracted from linear");

        if (!ProcessEncodingFromLinear(
                subset.c, context_->working_model->constraints(superset.c),
                &num_unique_terms, &num_multiple_terms)) {
          // UNSAT, we return right away, no cleanup needed.
          return;
        }

        // Remove from the watcher list.
        std::swap(one_watcher[superset_var][i],
                  one_watcher[superset_var].back());
        one_watcher[superset_var].pop_back();
        --i;
      }
    }

    // Cleanup.
    for (const int var : superset.vars) {
      is_in_superset[var] = false;
    }
  }

  SOLVER_LOG(logger_, "[ExtractEncodingFromLinear]",
             " #potential_supersets=", potential_supersets.size(),
             " #potential_subsets=", potential_subsets.size(),
             " #at_most_one_encodings=", num_at_most_one_encodings,
             " #exactly_one_encodings=", num_exactly_one_encodings,
             " #unique_terms=", num_unique_terms,
             " #multiple_terms=", num_multiple_terms,
             " #literals=", num_literals, " time=", wall_timer.Get(), "s");
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
      !context_->CanBeUsedAsLiteral(var) &&
      context_->VariableIsOnlyUsedInEncodingAndMaybeInObjective(var) &&
      context_->params().search_branching() != SatParameters::FIXED_SEARCH) {
    // We can currently only deal with the case where all encoding constraint
    // are of the form literal => var ==/!= value.
    // If they are more complex linear1 involved, we just abort.
    //
    // TODO(user): Also deal with the case all >= or <= where we can add a
    // serie of implication between all involved literals.
    absl::flat_hash_set<int64_t> values_set;
    absl::flat_hash_map<int64_t, std::vector<int>> value_to_equal_literals;
    absl::flat_hash_map<int64_t, std::vector<int>> value_to_not_equal_literals;
    bool abort = false;
    for (const int c : context_->VarToConstraints(var)) {
      if (c < 0) continue;
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
        if (!context_->DomainOf(var).Contains(rhs.FixedValue())) {
          if (!context_->SetLiteralToFalse(ct.enforcement_literal(0))) {
            return;
          }
        } else {
          values_set.insert(rhs.FixedValue());
          value_to_equal_literals[rhs.FixedValue()].push_back(
              ct.enforcement_literal(0));
        }
      } else {
        const Domain complement =
            context_->DomainOf(var).IntersectionWith(rhs.Complement());
        if (complement.IsEmpty()) {
          // TODO(user): This should be dealt with elsewhere.
          abort = true;
          break;
        }
        if (complement.IsFixed()) {
          if (context_->DomainOf(var).Contains(complement.FixedValue())) {
            values_set.insert(complement.FixedValue());
            value_to_not_equal_literals[complement.FixedValue()].push_back(
                ct.enforcement_literal(0));
          }
        } else {
          abort = true;
          break;
        }
      }
    }
    if (abort) {
      context_->UpdateRuleStats("TODO variables: only used in linear1.");
    } else if (value_to_not_equal_literals.empty() &&
               value_to_equal_literals.empty()) {
      // This is just a variable not used anywhere, it should be removed by
      // another part of the presolve.
    } else {
      // For determinism, sort all the encoded values first.
      std::vector<int64_t> encoded_values(values_set.begin(), values_set.end());
      std::sort(encoded_values.begin(), encoded_values.end());
      CHECK(!encoded_values.empty());
      const bool is_fully_encoded =
          encoded_values.size() == context_->DomainOf(var).Size();

      // Link all Boolean in out linear1 to the encoding literals. Note that we
      // should hopefully already have detected such literal before and this
      // should add trivial implications.
      for (const int64_t v : encoded_values) {
        const int encoding_lit = context_->GetOrCreateVarValueEncoding(var, v);
        const auto eq_it = value_to_equal_literals.find(v);
        if (eq_it != value_to_equal_literals.end()) {
          for (const int lit : eq_it->second) {
            context_->AddImplication(lit, encoding_lit);
          }
        }
        const auto neq_it = value_to_not_equal_literals.find(v);
        if (neq_it != value_to_not_equal_literals.end()) {
          for (const int lit : neq_it->second) {
            context_->AddImplication(lit, NegatedRef(encoding_lit));
          }
        }
      }
      context_->UpdateNewConstraintsVariableUsage();

      // This is the set of other values.
      Domain other_values;
      if (!is_fully_encoded) {
        other_values = context_->DomainOf(var).IntersectionWith(
            Domain::FromValues(encoded_values).Complement());
      }

      // Update the objective if needed. Note that this operation can fail if
      // the new expression result in potential overflow.
      if (context_->VarToConstraints(var).contains(kObjectiveConstraint)) {
        int64_t min_value;
        const int64_t obj_coeff = context_->ObjectiveMap().at(var);
        if (is_fully_encoded) {
          // We substract the min_value from all coefficients.
          // This should reduce the objective size and helps with the bounds.
          min_value =
              obj_coeff > 0 ? encoded_values.front() : encoded_values.back();
        } else {
          // Tricky: If the variable is not fully encoded, then when all
          // partial encoding literal are false, it must take the "best" value
          // in other_values. That depend on the sign of the objective coeff.
          //
          // We also restrict other value so that the postsolve code below
          // will fix the variable to the correct value when this happen.
          other_values =
              Domain(obj_coeff > 0 ? other_values.Min() : other_values.Max());
          min_value = other_values.FixedValue();
        }

        // Checks for overflow before trying to substitute the variable in the
        // objective.
        int64_t accumulated = std::abs(min_value);
        for (const int64_t value : encoded_values) {
          accumulated = CapAdd(accumulated, std::abs(CapSub(value, min_value)));
          if (accumulated == std::numeric_limits<int64_t>::max()) {
            context_->UpdateRuleStats(
                "TODO variables: only used in objective and in encoding");
            return;
          }
        }

        ConstraintProto encoding_ct;
        LinearConstraintProto* linear = encoding_ct.mutable_linear();
        const int64_t coeff_in_equality = -1;
        linear->add_vars(var);
        linear->add_coeffs(coeff_in_equality);

        linear->add_domain(-min_value);
        linear->add_domain(-min_value);
        for (const int64_t value : encoded_values) {
          if (value == min_value) continue;
          const int enf = context_->GetOrCreateVarValueEncoding(var, value);
          const int64_t coeff = value - min_value;
          if (RefIsPositive(enf)) {
            linear->add_vars(enf);
            linear->add_coeffs(coeff);
          } else {
            // (1 - var) * coeff;
            linear->set_domain(0, encoding_ct.linear().domain(0) - coeff);
            linear->set_domain(1, encoding_ct.linear().domain(1) - coeff);
            linear->add_vars(PositiveRef(enf));
            linear->add_coeffs(-coeff);
          }
        }
        if (!context_->SubstituteVariableInObjective(var, coeff_in_equality,
                                                     encoding_ct)) {
          context_->UpdateRuleStats(
              "TODO variables: only used in objective and in encoding");
          return;
        }
        context_->UpdateRuleStats(
            "variables: only used in objective and in encoding");
      } else {
        context_->UpdateRuleStats("variables: only used in encoding");
      }

      // Clear all involved constraint.
      auto copy = context_->VarToConstraints(var);
      for (const int c : copy) {
        if (c < 0) continue;
        context_->working_model->mutable_constraints(c)->Clear();
        context_->UpdateConstraintVariableUsage(c);
      }

      // Add enough constraints to the mapping model to recover a valid value
      // for var when all the booleans are fixed.
      for (const int64_t value : encoded_values) {
        const int enf = context_->GetOrCreateVarValueEncoding(var, value);
        ConstraintProto* ct = context_->mapping_model->add_constraints();
        ct->add_enforcement_literal(enf);
        ct->mutable_linear()->add_vars(var);
        ct->mutable_linear()->add_coeffs(1);
        ct->mutable_linear()->add_domain(value);
        ct->mutable_linear()->add_domain(value);
      }

      // This must be done after we removed all the constraint containing var.
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      if (is_fully_encoded) {
        // The encoding is full: add an exactly one.
        for (const int64_t value : encoded_values) {
          new_ct->mutable_exactly_one()->add_literals(
              context_->GetOrCreateVarValueEncoding(var, value));
        }
        PresolveExactlyOne(new_ct);
      } else {
        // If all literal are false, then var must take one of the other values.
        ConstraintProto* mapping_ct =
            context_->mapping_model->add_constraints();
        mapping_ct->mutable_linear()->add_vars(var);
        mapping_ct->mutable_linear()->add_coeffs(1);
        FillDomainInProto(other_values, mapping_ct->mutable_linear());

        for (const int64_t value : encoded_values) {
          const int literal = context_->GetOrCreateVarValueEncoding(var, value);
          mapping_ct->add_enforcement_literal(NegatedRef(literal));
          new_ct->mutable_at_most_one()->add_literals(literal);
        }
        PresolveAtMostOne(new_ct);
      }

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
      if (c < 0) {
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
    DCHECK_EQ(i.start, i.end);
    const int64_t shifted_value = i.start - var_min;
    DCHECK_GT(shifted_value, 0);

    gcd = MathUtil::GCD64(gcd, shifted_value);
    if (gcd == 1) break;
  }
  if (gcd == 1) return;

  // This does all the work since var * 1 % gcd = var_min % gcd.
  context_->CanonicalizeAffineVariable(var, 1, gcd, var_min);
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
      context_->params().debug_max_num_presolve_operations() > 0
          ? context_->params().debug_max_num_presolve_operations()
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

    // This is needed to remove variable with a different representative from
    // the objective. This allows to remove them completely in the loop below.
    if (context_->ModelIsUnsat()) return;
    if (!context_->CanonicalizeObjective()) return;

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
    in_queue.resize(context_->working_model->constraints_size(), false);
    for (int v = 0; v < num_vars; ++v) {
      const auto& constraints = context_->VarToConstraints(v);
      if (constraints.size() != 1) continue;
      const int c = *constraints.begin();
      if (c < 0) continue;

      // Note that to avoid bad complexity in problem like a TSP with just one
      // big constraint. we mark all the singleton variables of a constraint
      // even if this constraint is already in the queue.
      if (var_constraint_pair_already_called.contains(
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
      // variables with the use of time_exprstamp.
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

  // TODO(user): Compute domain and avoid copying constraint that are always
  // true.
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
  if (temp_literals_.size() == 1 && num_true == 1) return true;

  // TODO(user): presolve if num_true == 1 and not everything is false.
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
  if (in_model.has_floating_point_objective()) {
    *context->working_model->mutable_floating_point_objective() =
        in_model.floating_point_objective();
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

CpSolverStatus PresolveCpModel(PresolveContext* context,
                               std::vector<int>* postsolve_mapping) {
  CpModelPresolver presolver(context, postsolve_mapping);
  return presolver.Presolve();
}

CpModelPresolver::CpModelPresolver(PresolveContext* context,
                                   std::vector<int>* postsolve_mapping)
    : postsolve_mapping_(postsolve_mapping),
      context_(context),
      logger_(context->logger()) {}

CpSolverStatus CpModelPresolver::InfeasibleStatus() {
  if (logger_->LoggingIsEnabled()) context_->LogInfo();
  return CpSolverStatus::INFEASIBLE;
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
CpSolverStatus CpModelPresolver::Presolve() {
  // TODO(user): move in the context.
  context_->keep_all_feasible_solutions =
      context_->params().keep_all_feasible_solutions_in_presolve() ||
      context_->params().enumerate_all_solutions() ||
      context_->params().fill_tightened_domains_in_response() ||
      !context_->working_model->assumptions().empty() ||
      !context_->params().cp_model_presolve();

  // We copy the search strategy to the mapping_model.
  for (const auto& decision_strategy :
       context_->working_model->search_strategy()) {
    *(context_->mapping_model->add_search_strategy()) = decision_strategy;
  }

  // Initialize the initial context.working_model domains.
  context_->InitializeNewDomains();

  // Before initializing the constraint <-> variable graph (which is costly), we
  // run a bunch of simple presolve rules. Note that these function should NOT
  // use the graph, or the part that uses it should properly check for
  // context_->ConstraintVariableGraphIsUpToDate() before doing anything that
  // depends on the graph.
  if (context_->params().cp_model_presolve()) {
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
  }
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // If the objective is a floating point one, we scale it.
  //
  // TODO(user): We should probably try to delay this even more. For that we
  // just need to isolate more the "dual" reduction that usually need to look at
  // the objective.
  if (context_->working_model->has_floating_point_objective()) {
    if (!context_->ScaleFloatingPointObjective()) {
      SOLVER_LOG(logger_,
                 "The floating point objective cannot be scaled with enough "
                 "precision");
      return CpSolverStatus::MODEL_INVALID;
    }

    // At this point, we didn't create any new variables, so the integer
    // objective is in term of the orinal problem variables. We save it so that
    // we can expose to the user what exact objective we are actually
    // optimizing.
    *context_->mapping_model->mutable_objective() =
        context_->working_model->objective();
  }

  // Initialize the objective and the constraint <-> variable graph.
  context_->ReadObjectiveFromProto();
  if (!context_->CanonicalizeObjective()) return InfeasibleStatus();
  context_->UpdateNewConstraintsVariableUsage();
  context_->RegisterVariablesUsedInAssumptions();
  DCHECK(context_->ConstraintVariableUsageIsConsistent());

  // If presolve is false, just run expansion.
  if (!context_->params().cp_model_presolve()) {
    ExpandCpModel(context_);
    if (context_->ModelIsUnsat()) return InfeasibleStatus();

    // We need to append all the variable equivalence that are still used!
    EncodeAllAffineRelations();
    if (logger_->LoggingIsEnabled()) context_->LogInfo();
    return CpSolverStatus::UNKNOWN;
  }

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
    if (!context_->ModelIsExpanded()) {
      ExtractEncodingFromLinear();
      ExpandCpModel(context_);
    }
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
    // Hack: to facilitate experiments, if the requested number of iterations
    // is large, we always execute all of them.
    if (context_->params().max_presolve_iterations() >= 5) continue;
    if (context_->num_presolve_operations - old_num_presolve_op <
        0.8 * (context_->working_model->variables_size() +
               old_num_non_empty_constraints)) {
      break;
    }
  }
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // Regroup no-overlaps into max-cliques.
  MergeNoOverlapConstraints();
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // Tries to spread the objective amongst many variables.
  if (context_->working_model->has_objective()) {
    ExpandObjective();
    if (context_->ModelIsUnsat()) return InfeasibleStatus();
  }

  // Adds all needed affine relation to context_->working_model.
  EncodeAllAffineRelations();
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // Remove duplicate constraints.
  //
  // TODO(user): We might want to do that earlier so that our count of variable
  // usage is not biased by duplicate constraints.
  const std::vector<std::pair<int, int>> duplicates =
      FindDuplicateConstraints(*context_->working_model);
  for (const auto [dup, rep] : duplicates) {
    // Note that it is important to look at the type of the representative in
    // case the constraint became empty.
    const int type =
        context_->working_model->constraints(rep).constraint_case();

    // TODO(user): we could delete duplicate identical interval, but we need
    // to make sure reference to them are updated.
    if (type == ConstraintProto::ConstraintCase::kInterval) {
      continue;
    }

    // For linear constraint, we merge their rhs since it was ignored in the
    // FindDuplicateConstraints() call.
    if (type == ConstraintProto::kLinear) {
      const Domain d1 = ReadDomainFromProto(
          context_->working_model->constraints(rep).linear());
      const Domain d2 = ReadDomainFromProto(
          context_->working_model->constraints(dup).linear());
      if (d1 != d2) {
        context_->UpdateRuleStats("duplicate: merged rhs of linear constraint");
        const Domain rhs = d1.IntersectionWith(d2);
        if (rhs.IsEmpty()) {
          if (!MarkConstraintAsFalse(
                  context_->working_model->mutable_constraints(rep))) {
            SOLVER_LOG(logger_, "Unsat after merging two linear constraints");
            break;
          }

          // The representative constraint is no longer a linear constraint,
          // so we will not enter this type case again and will just remove
          // all subsequent duplicate linear constraints.
          context_->UpdateConstraintVariableUsage(rep);
          continue;
        }
        FillDomainInProto(rhs, context_->working_model->mutable_constraints(rep)
                                   ->mutable_linear());
      }
    }

    context_->working_model->mutable_constraints(dup)->Clear();
    context_->UpdateConstraintVariableUsage(dup);
    context_->UpdateRuleStats("duplicate: removed constraint");
  }

  if (context_->ModelIsUnsat()) return InfeasibleStatus();

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
      if (used_variables.contains(var)) continue;
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
  int num_free_variables = 0;
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    if (mapping[i] != -1) continue;  // Already mapped.

    if (context_->VariableWasRemoved(i)) {
      // Heuristic: If a variable is removed and has a representative that is
      // not, we "move" the representative to the spot of that variable in the
      // original order. This is to preserve any info encoded in the variable
      // order by the modeler.
      const int r = PositiveRef(context_->GetAffineRelation(i).representative);
      if (mapping[r] == -1 && !context_->VariableIsNotUsedAnymore(r)) {
        mapping[r] = postsolve_mapping_->size();
        postsolve_mapping_->push_back(r);
      }
      continue;
    }

    if (context_->VariableIsNotUsedAnymore(i) &&
        !context_->keep_all_feasible_solutions) {
      // Tricky. Variables that where not removed by a presolve rule should be
      // fixed first during postsolve, so that more complex postsolve rules
      // can use their values. One way to do that is to fix them here.
      //
      // We prefer to fix them to zero if possible.
      ++num_free_variables;
      FillDomainInProto(Domain(context_->DomainOf(i).SmallestValue()),
                        context_->mapping_model->mutable_variables(i));
      continue;
    }

    mapping[i] = postsolve_mapping_->size();
    postsolve_mapping_->push_back(i);
  }
  context_->UpdateRuleStats(absl::StrCat("presolve: ", num_free_variables,
                                         " unused variables removed."));

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

  // This is not supposed to happen, and is more indicative of an error than an
  // INVALID model. But for our no-overflow preconditions, we might run into bad
  // situation that causes the final model to be invalid.
  {
    const std::string error = ValidateCpModel(*context_->working_model);
    if (!error.empty()) {
      SOLVER_LOG(logger_, "Error while validating postsolved model: ", error);
      return CpSolverStatus::MODEL_INVALID;
    }
  }
  {
    const std::string error = ValidateCpModel(*context_->mapping_model);
    if (!error.empty()) {
      SOLVER_LOG(logger_,
                 "Error while validating mapping_model model: ", error);
      return CpSolverStatus::MODEL_INVALID;
    }
  }

  return CpSolverStatus::UNKNOWN;
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

  // Remap the solution hint. Note that after remapping, we may have duplicate
  // variable, so we only keep the first occurrence.
  if (proto->has_solution_hint()) {
    absl::flat_hash_set<int> used_vars;
    auto* mutable_hint = proto->mutable_solution_hint();
    int new_size = 0;
    for (int i = 0; i < mutable_hint->vars_size(); ++i) {
      const int old_ref = mutable_hint->vars(i);
      int64_t old_value = mutable_hint->values(i);

      // We always move a hint within bounds.
      // This also make sure a hint of INT_MIN or INT_MAX does not overflow.
      if (old_value < context.MinOf(old_ref)) {
        old_value = context.MinOf(old_ref);
      }
      if (old_value > context.MaxOf(old_ref)) {
        old_value = context.MaxOf(old_ref);
      }

      // Note that if (old_value - r.offset) is not divisible by r.coeff, then
      // the hint is clearly infeasible, but we still set it to a "close" value.
      const AffineRelation::Relation r = context.GetAffineRelation(old_ref);
      const int var = r.representative;
      const int64_t value = (old_value - r.offset) / r.coeff;

      const int image = mapping[var];
      if (image >= 0) {
        if (!used_vars.insert(image).second) continue;
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

namespace {
ConstraintProto CopyConstraintForDuplicateDetection(const ConstraintProto& ct) {
  ConstraintProto copy = ct;
  copy.clear_name();
  if (ct.constraint_case() == ConstraintProto::kLinear) {
    copy.mutable_linear()->clear_domain();
  }
  return copy;
}
}  // namespace

std::vector<std::pair<int, int>> FindDuplicateConstraints(
    const CpModelProto& model_proto) {
  std::vector<std::pair<int, int>> result;

  // We use a map hash: serialized_constraint_proto hash -> constraint index.
  ConstraintProto copy;
  absl::flat_hash_map<uint64_t, int> equiv_constraints;

  std::string s;
  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    if (model_proto.constraints(c).constraint_case() ==
        ConstraintProto::CONSTRAINT_NOT_SET) {
      continue;
    }

    // We ignore names when comparing constraints.
    //
    // TODO(user): This is not particularly efficient.
    copy = CopyConstraintForDuplicateDetection(model_proto.constraints(c));
    s = copy.SerializeAsString();

    const uint64_t hash = absl::Hash<std::string>()(s);
    const auto [it, inserted] = equiv_constraints.insert({hash, c});
    if (!inserted) {
      // Already present!
      const int other_c_with_same_hash = it->second;
      copy = CopyConstraintForDuplicateDetection(
          model_proto.constraints(other_c_with_same_hash));
      if (s == copy.SerializeAsString()) {
        result.push_back({c, other_c_with_same_hash});
      }
    }
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
