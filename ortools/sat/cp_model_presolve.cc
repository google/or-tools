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

#include "ortools/sat/cp_model_presolve.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_objective.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/simplification.h"

namespace operations_research {
namespace sat {

bool CpModelPresolver::RemoveConstraint(ConstraintProto* ct) {
  ct->Clear();
  return true;
}

void CpModelPresolver::SyncDomainAndRemoveEmptyConstraints() {
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

  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    FillDomainInProto(context_->DomainOf(i),
                      context_->working_model->mutable_variables(i));
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
      const int64 obj_coeff =
          gtl::FindOrDie(context_->ObjectiveMap(), PositiveRef(literal));
      if (RefIsPositive(literal) == obj_coeff > 0) {
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
  int true_literal = kint32min;
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
    CHECK_NE(true_literal, kint32min);
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
  return changed;
}

bool CpModelPresolver::PresolveAtMostOne(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  CHECK(!HasEnforcementLiteral(*ct));

  // Fix to false any duplicate literals.
  std::sort(ct->mutable_at_most_one()->mutable_literals()->begin(),
            ct->mutable_at_most_one()->mutable_literals()->end());
  int previous = kint32max;
  for (const int literal : ct->at_most_one().literals()) {
    if (literal == previous) {
      if (!context_->SetLiteralToFalse(literal)) return true;
      context_->UpdateRuleStats("at_most_one: duplicate literals");
    }
    previous = literal;
  }

  bool changed = false;
  context_->tmp_literals.clear();
  for (const int literal : ct->at_most_one().literals()) {
    if (context_->LiteralIsTrue(literal)) {
      context_->UpdateRuleStats("at_most_one: satisfied");
      for (const int other : ct->at_most_one().literals()) {
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

    context_->tmp_literals.push_back(literal);
  }
  if (context_->tmp_literals.empty()) {
    context_->UpdateRuleStats("at_most_one: all false");
    return RemoveConstraint(ct);
  }

  if (changed) {
    ct->mutable_at_most_one()->mutable_literals()->Clear();
    for (const int lit : context_->tmp_literals) {
      ct->mutable_at_most_one()->add_literals(lit);
    }
    context_->UpdateRuleStats("at_most_one: removed literals");
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
  int64 infered_min = kint64min;
  int64 infered_max = kint64min;
  bool contains_target_ref = false;
  std::set<int> used_ref;
  int new_size = 0;
  for (const int ref : ct->int_max().vars()) {
    if (ref == target_ref) contains_target_ref = true;
    if (gtl::ContainsKey(used_ref, ref)) continue;
    if (gtl::ContainsKey(used_ref, NegatedRef(ref)) ||
        ref == NegatedRef(target_ref)) {
      infered_min = std::max(infered_min, int64{0});
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
      arg->add_domain(kint64max);
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
    if (infered_domain.IntersectionWith(Domain(kint64min, target_domain.Max()))
            .IsIncludedIn(target_domain)) {
      if (infered_domain.Max() <= target_domain.Max()) {
        // The constraint is always satisfiable.
        context_->UpdateRuleStats("int_max: always true");
      } else if (ct->enforcement_literal().empty()) {
        // The constraint just restrict the upper bound of its variable.
        for (const int ref : ct->int_max().vars()) {
          context_->UpdateRuleStats("int_max: lower than constant");
          if (!context_->IntersectDomainWith(
                  ref, Domain(kint64min, target_domain.Max()))) {
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
          ct->mutable_linear()->add_domain(kint64min);
          ct->mutable_linear()->add_domain(target_domain.Max());
        }
      }

      // In all cases we delete the original constraint.
      *(context_->mapping_model->add_constraints()) = *ct;
      return RemoveConstraint(ct);
    }
  }

  // Pass 2, update the argument domains. Filter them eventually.
  new_size = 0;
  const int size = ct->int_max().vars_size();
  const int64 target_max = context_->MaxOf(target_ref);
  for (const int ref : ct->int_max().vars()) {
    if (!HasEnforcementLiteral(*ct)) {
      if (!context_->IntersectDomainWith(ref, Domain(kint64min, target_max),
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

  // TODO(user): Remove duplicate expressions. This might be expensive.

  // Pass 1, Compute the infered min of the target.
  int64 infered_min = context_->MinOf(ct->lin_max().target());
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

  return false;
}

bool CpModelPresolver::PresolveIntAbs(ConstraintProto* ct) {
  CHECK_EQ(ct->enforcement_literal_size(), 0);
  if (context_->ModelIsUnsat()) return false;
  const int target_ref = ct->int_max().target();
  const int var = PositiveRef(ct->int_max().vars(0));

  // Propagate from the variable domain to the target variable.
  const Domain var_domain = context_->DomainOf(var);
  const Domain new_target_domain = var_domain.UnionWith(var_domain.Negation())
                                       .IntersectionWith({0, kint64max});
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

  bool changed = false;

  // Replace any affine relation without offset.
  int64 constant = 1;
  for (int i = 0; i < ct->int_prod().vars().size(); ++i) {
    const int ref = ct->int_prod().vars(i);
    const AffineRelation::Relation& r = context_->GetAffineRelation(ref);
    if (r.representative != ref && r.offset == 0) {
      changed = true;
      ct->mutable_int_prod()->set_vars(i, r.representative);
      constant *= r.coeff;
    }
  }
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

    ConstraintProto* new_ct = context_->working_model->add_constraints();
    LinearConstraintProto* lin = new_ct->mutable_linear();
    lin->add_vars(old_target);
    lin->add_coeffs(1);
    lin->add_vars(new_target);
    lin->add_coeffs(-constant);
    lin->add_domain(0);
    lin->add_domain(0);
    context_->UpdateNewConstraintsVariableUsage();
    context_->StoreAffineRelation(*new_ct, old_target, new_target, constant, 0);
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
      if (b != product) {
        ConstraintProto* const lin = context_->working_model->add_constraints();
        lin->mutable_linear()->add_vars(b);
        lin->mutable_linear()->add_coeffs(context_->MinOf(a));
        lin->mutable_linear()->add_vars(product);
        lin->mutable_linear()->add_coeffs(-1);
        lin->mutable_linear()->add_domain(0);
        lin->mutable_linear()->add_domain(0);
        context_->UpdateNewConstraintsVariableUsage();
        context_->UpdateRuleStats("int_prod: linearize product by constant.");
        return RemoveConstraint(ct);
      } else if (context_->MinOf(a) != 1) {
        bool domain_modified = false;
        if (!context_->IntersectDomainWith(product, Domain(0, 0),
                                           &domain_modified)) {
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

  const int64 divisor = context_->MinOf(ref_div);
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
  if (gtl::ContainsKey(context_->affine_constraints, ct)) return false;
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
  int64 gcd = 0;
  const int num_vars = ct->linear().vars().size();
  for (int i = 0; i < num_vars; ++i) {
    const int64 magnitude = std::abs(ct->linear().coeffs(i));
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

bool CpModelPresolver::CanonicalizeLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  // Special case. We must not touch the half implications.
  //   literal => var ==/!= constant
  // because in case this is fully reified, var will be shifted back, and the
  // constraint will become a no-op.
  if (ct->enforcement_literal_size() == 1 && ct->linear().vars_size() == 1 &&
      context_->GetAffineRelation(PositiveRef(ct->linear().vars(0)))
              .representative == PositiveRef(ct->enforcement_literal(0))) {
    VLOG(2) << "Skip half-reified linear: " << ct->DebugString();
    return false;
  }

  // First regroup the terms on the same variables and sum the fixed ones.
  //
  // TODO(user): move terms in context to reuse its memory? Add a quick pass
  // to skip most of the work below if the constraint is already in canonical
  // form (strictly increasing var, no-fixed var, gcd = 1).
  tmp_terms_.clear();
  const bool was_affine = gtl::ContainsKey(context_->affine_constraints, ct);

  int64 sum_of_fixed_terms = 0;
  bool remapped = false;
  const int num_vars = ct->linear().vars().size();
  DCHECK_EQ(num_vars, ct->linear().coeffs().size());
  for (int i = 0; i < num_vars; ++i) {
    const int ref = ct->linear().vars(i);
    const int var = PositiveRef(ref);
    const int64 coeff =
        RefIsPositive(ref) ? ct->linear().coeffs(i) : -ct->linear().coeffs(i);
    if (coeff == 0) continue;

    // TODO(user): Avoid the quadratic loop for the corner case of many
    // enforcement literal (this should be pretty rare though).
    bool removed = false;
    for (const int enf : ct->enforcement_literal()) {
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
      context_->UpdateRuleStats("linear: enforcement literal in constraint");
      continue;
    }

    if (context_->IsFixed(var)) {
      sum_of_fixed_terms += coeff * context_->MinOf(var);
      continue;
    }

    if (!was_affine) {
      const AffineRelation::Relation r = context_->GetAffineRelation(var);
      if (r.representative != var) {
        remapped = true;
        sum_of_fixed_terms += coeff * r.offset;
      }
      tmp_terms_.push_back({r.representative, coeff * r.coeff});
    } else {
      tmp_terms_.push_back({var, coeff});
    }
  }

  if (sum_of_fixed_terms != 0) {
    Domain rhs = ReadDomainFromProto(ct->linear());
    rhs = rhs.AdditionWith({-sum_of_fixed_terms, -sum_of_fixed_terms});
    FillDomainInProto(rhs, ct->mutable_linear());
  }

  ct->mutable_linear()->clear_vars();
  ct->mutable_linear()->clear_coeffs();
  std::sort(tmp_terms_.begin(), tmp_terms_.end());
  int current_var = 0;
  int64 current_coeff = 0;
  for (const auto entry : tmp_terms_) {
    CHECK(RefIsPositive(entry.first));
    if (entry.first == current_var) {
      current_coeff += entry.second;
    } else {
      if (current_coeff != 0) {
        ct->mutable_linear()->add_vars(current_var);
        ct->mutable_linear()->add_coeffs(current_coeff);
      }
      current_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    ct->mutable_linear()->add_vars(current_var);
    ct->mutable_linear()->add_coeffs(current_coeff);
  }
  DivideLinearByGcd(ct);

  bool var_constraint_graph_changed = false;
  if (remapped) {
    context_->UpdateRuleStats("linear: remapped using affine relations");
    var_constraint_graph_changed = true;
  }
  if (ct->linear().vars().size() < num_vars) {
    context_->UpdateRuleStats("linear: fixed or dup variables");
    var_constraint_graph_changed = true;
  }
  return var_constraint_graph_changed;
}

bool CpModelPresolver::RemoveSingletonInLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  std::set<int> index_to_erase;
  const int num_vars = ct->linear().vars().size();
  Domain rhs = ReadDomainFromProto(ct->linear());

  // First pass. Process singleton column that are not in the objective.
  for (int i = 0; i < num_vars; ++i) {
    const int var = ct->linear().vars(i);
    const int64 coeff = ct->linear().coeffs(i);
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
    if (options_.parameters.presolve_substitution_level() <= 0) return false;
    if (!ct->enforcement_literal().empty()) return false;

    // If it is possible to do so, note that we can transform constraint into
    // equalities in PropagateDomainsInLinear().
    if (rhs.Min() != rhs.Max()) return false;

    for (int i = 0; i < num_vars; ++i) {
      const int var = ct->linear().vars(i);
      const int64 coeff = ct->linear().coeffs(i);
      CHECK(RefIsPositive(var));

      // If the variable appear only in the objective and we have an equality,
      // we can transfer the cost to the rest of the linear expression, and
      // remove that variable.
      //
      // Note that is similar to the substitution code in PresolveLinear() but
      // it doesn't require the variable to be implied free since we do not
      // remove the constraints afterwards, just the variable.
      if (!context_->VariableWithCostIsUniqueAndRemovable(var)) continue;

      // We only support substitution that does not require to multiply the
      // objective by some factor.
      //
      // TODO(user): If the objective is a single variable, we can actually
      // "absorb" any factor into the objective scaling.
      const int64 objective_coeff =
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
      if (context_->ObjectiveMap().size() == 1 &&
          context_->ObjectiveMap().contains(var)) {
        if (!context_->IntersectDomainWith(
                var, context_->ObjectiveDomain().InverseMultiplicationBy(
                         objective_coeff))) {
          return true;
        }

        // This makes sure the domain of var is propagated back to the
        // objective. We have the DCHECK() below, because this code rely on the
        // fact that var is neither fixed nor have a different representative
        // otherwise CanonicalizeLinear() will remove it. Note the special case
        // for affine constraint where the substitution is already done by
        // CanonicalizeObjective().
        if (!context_->CanonicalizeObjective()) {
          return context_->NotifyThatModelIsUnsat();
        }
        if (!context_->affine_constraints.contains(ct)) {
          DCHECK(context_->ObjectiveMap().contains(var));
          context_->UpdateRuleStats(
              "linear: singleton column define objective.");
          context_->SubstituteVariableInObjective(var, coeff, *ct);
        }

        *(context_->mapping_model->add_constraints()) = *ct;
        return RemoveConstraint(ct);
      }

      // Update the objective and remove the variable from its equality
      // constraint by expanding its rhs.
      context_->UpdateRuleStats(
          "linear: singleton column in equality and in objective.");
      context_->SubstituteVariableInObjective(var, coeff, *ct);
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
    if (index_to_erase.count(i)) continue;
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

  // If the constraint is literal => abs(x) in domain, we can remove the abs()
  // and its associated intermediate variables if we extend the domain
  // correctly.
  const auto abs_it = context_->abs_relations.find(ct->linear().vars(0));
  if (ct->linear().vars_size() == 1 && ct->enforcement_literal_size() > 0 &&
      abs_it != context_->abs_relations.end() && ct->linear().coeffs(0) == 1) {
    // TODO(user): Deal with coeff = -1, here or during canonicalization.
    context_->UpdateRuleStats("linear: remove abs from abs(x) in domain");
    const Domain implied_abs_target_domain =
        ReadDomainFromProto(ct->linear())
            .IntersectionWith({0, kint64max})
            .IntersectionWith(context_->DomainOf(abs_it->first));

    if (implied_abs_target_domain.IsEmpty()) {
      return MarkConstraintAsFalse(ct);
    }

    const Domain new_abs_var_domain =
        implied_abs_target_domain
            .UnionWith(implied_abs_target_domain.Negation())
            .IntersectionWith(context_->DomainOf(abs_it->second));

    if (new_abs_var_domain.IsEmpty()) {
      return MarkConstraintAsFalse(ct);
    }

    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    for (const int literal : ct->enforcement_literal()) {
      new_ct->add_enforcement_literal(literal);
    }
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(abs_it->second);
    arg->add_coeffs(1);
    FillDomainInProto(new_abs_var_domain, new_ct->mutable_linear());
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  if (HasEnforcementLiteral(*ct)) {
    if (ct->enforcement_literal_size() != 1 || ct->linear().vars_size() != 1 ||
        (ct->linear().coeffs(0) != 1 && ct->linear().coeffs(0) == -1)) {
      return false;
    }

    const int literal = ct->enforcement_literal(0);
    const LinearConstraintProto& linear = ct->linear();
    const int ref = linear.vars(0);
    const int var = PositiveRef(ref);
    const int64 coeff =
        RefIsPositive(ref) ? ct->linear().coeffs(0) : -ct->linear().coeffs(0);

    if (linear.domain_size() == 2 && linear.domain(0) == linear.domain(1)) {
      const int64 value = RefIsPositive(ref) ? linear.domain(0) * coeff
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
      const int64 value = RefIsPositive(ref) ? complement.Min() * coeff
                                             : -complement.Min() * coeff;
      if (context_->StoreLiteralImpliesVarNEqValue(literal, var, value)) {
        // The domain is not actually modified, but we want to rescan the
        // constraints linked to this variable. See TODO below.
        context_->modified_domains.Set(var);
      }
    }

    // TODO(user): if we have l1 <=> x == value && l2 => x == value, we
    //     could rewrite the second constraint into l2 => l1.
    return false;
  }

  // Size one constraint?
  if (ct->linear().vars().size() == 1) {
    const int64 coeff = RefIsPositive(ct->linear().vars(0))
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
  if (!context_->affine_constraints.contains(ct)) {
    const LinearConstraintProto& arg = ct->linear();
    const Domain rhs = ReadDomainFromProto(ct->linear());
    const int64 rhs_min = rhs.Min();
    const int64 rhs_max = rhs.Max();
    if (rhs_min == rhs_max && arg.vars_size() == 2) {
      const int v1 = arg.vars(0);
      const int v2 = arg.vars(1);
      const int64 coeff1 = arg.coeffs(0);
      const int64 coeff2 = arg.coeffs(1);
      if (coeff1 == 1) {
        context_->StoreAffineRelation(*ct, v1, v2, -coeff2, rhs_max);
      } else if (coeff2 == 1) {
        context_->StoreAffineRelation(*ct, v2, v1, -coeff1, rhs_max);
      } else if (coeff1 == -1) {
        context_->StoreAffineRelation(*ct, v1, v2, coeff2, -rhs_max);
      } else if (coeff2 == -1) {
        context_->StoreAffineRelation(*ct, v2, v1, coeff1, -rhs_max);
      }
    }
  }

  return false;
}

namespace {

// Return true if the given domain only restrict the values with an upper bound.
bool IsLeConstraint(const Domain& domain, const Domain& all_values) {
  return all_values.IntersectionWith(Domain(kint64min, domain.Max()))
      .IsIncludedIn(domain);
}

// Same as IsLeConstraint() but in the other direction.
bool IsGeConstraint(const Domain& domain, const Domain& all_values) {
  return all_values.IntersectionWith(Domain(domain.Min(), kint64max))
      .IsIncludedIn(domain);
}

// In the equation terms + coeff * var_domain \included rhs, returns true if can
// we always fix rhs to its min value for any value in terms. It is okay to
// not be as generic as possible here.
bool RhsCanBeFixedToMin(int64 coeff, const Domain& var_domain,
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

bool RhsCanBeFixedToMax(int64 coeff, const Domain& var_domain,
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
    const int64 coeff = ct->linear().coeffs(i);
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
    const int64 var_coeff = ct->linear().coeffs(i);
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
        const int64 obj_coeff =
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
      const int64 obj_coeff = gtl::FindOrDie(context_->ObjectiveMap(), var);
      bool fixed = false;
      if ((var_coeff > 0 == obj_coeff > 0) &&
          RhsCanBeFixedToMin(var_coeff, context_->DomainOf(var),
                             implied_term_domain, rhs)) {
        rhs = Domain(rhs.Min());
        fixed = true;
      }
      if ((var_coeff > 0 != obj_coeff > 0) &&
          RhsCanBeFixedToMax(var_coeff, context_->DomainOf(var),
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
    // TODO(user): there is no guarante we will not miss some since we might
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
    if (options_.parameters.presolve_substitution_level() <= 0) continue;

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
    // appear. Basically they must all be linear and not already used for
    // affine relation.
    std::vector<int> others;
    for (const int c : context_->VarToConstraints(var)) {
      if (c == -1) continue;
      if (context_->working_model->mutable_constraints(c) == ct) continue;
      others.push_back(c);
    }
    bool abort = false;
    for (const int c : others) {
      if (context_->working_model->constraints(c).constraint_case() !=
          ConstraintProto::ConstraintCase::kLinear) {
        abort = true;
        break;
      }
      if (context_->affine_constraints.contains(
              &context_->working_model->constraints(c))) {
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
      if (abort) break;
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
    CHECK_EQ(context_->VarToConstraints(var).size(), 1);
    *context_->mapping_model->add_constraints() = *ct;
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
// This operation is similar to coefficient strengthening in the MIP world.
//
// TODO(user): For non-binary variable, we should also reduce large coefficient
// by using the same logic (i.e. real coefficient strengthening).
void CpModelPresolver::ExtractEnforcementLiteralFromLinearConstraint(
    ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::ConstraintCase::kLinear ||
      context_->ModelIsUnsat()) {
    return;
  }
  if (context_->affine_constraints.contains(ct)) return;

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();

  // No need to process size one constraints, they will be presolved separately.
  // We also do not want to split them in two.
  if (num_vars <= 1) return;

  int64 min_sum = 0;
  int64 max_sum = 0;
  int64 max_coeff_magnitude = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int ref = arg.vars(i);
    const int64 coeff = arg.coeffs(i);
    const int64 term_a = coeff * context_->MinOf(ref);
    const int64 term_b = coeff * context_->MaxOf(ref);
    max_coeff_magnitude = std::max(max_coeff_magnitude, std::abs(coeff));
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }

  // We can only extract enforcement literals if the maximum coefficient
  // magnitude is greater or equal to max_sum - rhs_domain.Max() or
  // rhs_domain.Min() - min_sum.
  Domain rhs_domain = ReadDomainFromProto(ct->linear());
  if (max_coeff_magnitude <
      std::max(max_sum - rhs_domain.Max(), rhs_domain.Min() - min_sum)) {
    return;
  }

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

  // To avoid a quadratic loop, we will rewrite the linear expression at the
  // same time as we extract enforcement literals.
  int new_size = 0;
  LinearConstraintProto* mutable_arg = ct->mutable_linear();
  for (int i = 0; i < arg.vars_size(); ++i) {
    // We currently only process binary variables.
    const int ref = arg.vars(i);
    if (context_->MinOf(ref) == 0 && context_->MaxOf(ref) == 1) {
      const int64 coeff = arg.coeffs(i);
      if (!lower_bounded) {
        if (max_sum - std::abs(coeff) <= rhs_domain.front().end) {
          if (coeff > 0) {
            // Fix the variable to 1 in the constraint and add it as enforcement
            // literal.
            rhs_domain = rhs_domain.AdditionWith(Domain(-coeff));
            ct->add_enforcement_literal(ref);
            // 'min_sum' remains unaffected.
            max_sum -= coeff;
          } else {
            // Fix the variable to 0 in the constraint and add its negation as
            // enforcement literal.
            ct->add_enforcement_literal(NegatedRef(ref));
            // 'max_sum' remains unaffected.
            min_sum -= coeff;
          }
          context_->UpdateRuleStats(
              "linear: extracted enforcement literal from constraint");
          continue;
        }
      } else {
        DCHECK(!upper_bounded);
        if (min_sum + std::abs(coeff) >= rhs_domain.back().start) {
          if (coeff > 0) {
            // Fix the variable to 0 in the constraint and add its negation as
            // enforcement literal.
            ct->add_enforcement_literal(NegatedRef(ref));
            // 'min_sum' remains unaffected.
            max_sum -= coeff;
          } else {
            // Fix the variable to 1 in the constraint and add it as enforcement
            // literal.
            rhs_domain = rhs_domain.AdditionWith(Domain(-coeff));
            ct->add_enforcement_literal(ref);
            // 'max_sum' remains unaffected.
            min_sum -= coeff;
          }
          context_->UpdateRuleStats(
              "linear: extracted enforcement literal from constraint");
          continue;
        }
      }
    }

    // We keep this term.
    mutable_arg->set_vars(new_size, mutable_arg->vars(i));
    mutable_arg->set_coeffs(new_size, mutable_arg->coeffs(i));
    ++new_size;
  }

  mutable_arg->mutable_vars()->Truncate(new_size);
  mutable_arg->mutable_coeffs()->Truncate(new_size);
  FillDomainInProto(rhs_domain, mutable_arg);
}

void CpModelPresolver::ExtractAtMostOneFromLinear(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return;
  if (HasEnforcementLiteral(*ct)) return;
  const Domain rhs = ReadDomainFromProto(ct->linear());

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64 min_sum = 0;
  int64 max_sum = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int ref = arg.vars(i);
    const int64 coeff = arg.coeffs(i);
    const int64 term_a = coeff * context_->MinOf(ref);
    const int64 term_b = coeff * context_->MaxOf(ref);
    min_sum += std::min(term_a, term_b);
    max_sum += std::max(term_a, term_b);
  }
  for (const int type : {0, 1}) {
    std::vector<int> at_most_one;
    for (int i = 0; i < num_vars; ++i) {
      const int ref = arg.vars(i);
      const int64 coeff = arg.coeffs(i);
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

  // TODO(user): the alternative to mark any newly created constraints might
  // be better.
  if (gtl::ContainsKey(context_->affine_constraints, ct)) return false;

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64 min_coeff = kint64max;
  int64 max_coeff = 0;
  int64 min_sum = 0;
  int64 max_sum = 0;
  for (int i = 0; i < num_vars; ++i) {
    // We assume we already ran PresolveLinear().
    const int var = arg.vars(i);
    const int64 coeff = arg.coeffs(i);
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
    return PresolveBoolAnd(ct);
  } else if (max_sum - min_coeff < rhs_domain.Min()) {
    // All Boolean are true if the reified literal is true.
    context_->UpdateRuleStats("linear: positive reified and");
    const auto copy = arg;
    ct->mutable_bool_and()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_and()->add_literals(
          copy.coeffs(i) > 0 ? copy.vars(i) : NegatedRef(copy.vars(i)));
    }
    return PresolveBoolAnd(ct);
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
    return PresolveBoolOr(ct);
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
    return PresolveBoolOr(ct);
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
    ConstraintProto* at_least_one = context_->working_model->add_constraints();
    ConstraintProto* at_most_one = context_->working_model->add_constraints();
    for (int i = 0; i < num_vars; ++i) {
      at_least_one->mutable_bool_or()->add_literals(
          arg.coeffs(i) > 0 ? arg.vars(i) : NegatedRef(arg.vars(i)));
      at_most_one->mutable_at_most_one()->add_literals(
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
    ConstraintProto* at_least_one = context_->working_model->add_constraints();
    ConstraintProto* at_most_one = context_->working_model->add_constraints();
    for (int i = 0; i < num_vars; ++i) {
      at_least_one->mutable_bool_or()->add_literals(
          arg.coeffs(i) > 0 ? NegatedRef(arg.vars(i)) : arg.vars(i));
      at_most_one->mutable_at_most_one()->add_literals(
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
    int64 value = 0;
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

  const int start = ct->interval().start();
  const int end = ct->interval().end();
  const int size = ct->interval().size();

  if (ct->enforcement_literal().empty()) {
    bool changed = false;
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
    // Convert to linear.
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    *(new_ct->mutable_enforcement_literal()) = ct->enforcement_literal();
    new_ct->mutable_linear()->add_domain(0);
    new_ct->mutable_linear()->add_domain(0);
    new_ct->mutable_linear()->add_vars(start);
    new_ct->mutable_linear()->add_coeffs(1);
    new_ct->mutable_linear()->add_vars(size);
    new_ct->mutable_linear()->add_coeffs(1);
    new_ct->mutable_linear()->add_vars(end);
    new_ct->mutable_linear()->add_coeffs(-1);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("interval: unused, converted to linear");

    return RemoveConstraint(ct);
  }

  // TODO(user): This currently has a side effect that both the interval and
  // a linear constraint are added to the presolved model. Fix.
  if (false && context_->IsFixed(size)) {
    // We add it even if the interval is optional.
    // TODO(user): we must verify that all the variable of an optional interval
    // do not appear in a constraint which is not reified by the same literal.
    context_->StoreAffineRelation(*ct, ct->interval().end(),
                                  ct->interval().start(), 1,
                                  context_->MinOf(size));
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

  int num_vars = 0;
  bool all_constants = true;
  absl::flat_hash_set<int64> constant_set;
  bool all_included_in_target_domain = true;

  {
    bool reduced_index_domain = false;
    if (!context_->IntersectDomainWith(index_ref,
                                       Domain(0, ct->element().vars_size() - 1),
                                       &reduced_index_domain)) {
      return false;
    }

    // Filter possible index values. Accumulate variable domains to build
    // a possible target domain.
    Domain infered_domain;
    const Domain& initial_index_domain = context_->DomainOf(index_ref);
    const Domain& target_domain = context_->DomainOf(target_ref);
    for (const ClosedInterval interval : initial_index_domain) {
      for (int value = interval.start; value <= interval.end; ++value) {
        CHECK_GE(value, 0);
        CHECK_LT(value, ct->element().vars_size());
        const int ref = ct->element().vars(value);
        const Domain& domain = context_->DomainOf(ref);
        if (domain.IntersectionWith(target_domain).IsEmpty()) {
          bool domain_modified = false;
          if (!context_->IntersectDomainWith(
                  index_ref, Domain(value).Complement(), &domain_modified)) {
            return false;
          }
          reduced_index_domain = true;
        } else {
          ++num_vars;
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
    }
    if (reduced_index_domain) {
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
    const int64 v0 = context_->MinOf(ct->element().vars(0));
    const int64 v1 = context_->MinOf(ct->element().vars(1));

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
    const int64 r_min = context_->MinOf(r_ref);
    const int64 r_max = context_->MaxOf(r_ref);
    const int array_size = ct->element().vars_size();
    if (r_min != 0) {
      context_->UpdateRuleStats("TODO element: representative has bad domain");
    } else if (r_index.offset >= 0 && r_index.offset < array_size &&
               r_index.offset + r_max * r_index.coeff >= 0 &&
               r_index.offset + r_max * r_index.coeff < array_size) {
      // This will happen eventually when domains are synchronized.
      ElementConstraintProto* const element =
          context_->working_model->add_constraints()->mutable_element();
      for (int64 v = 0; v <= r_max; ++v) {
        const int64 scaled_index = v * r_index.coeff + r_index.offset;
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
    *(context_->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct);
  }

  const bool unique_target =
      context_->VariableIsUniqueAndRemovable(target_ref) ||
      context_->IsFixed(target_ref);
  if (all_included_in_target_domain && unique_target) {
    context_->UpdateRuleStats("element: trivial index domain reduction");
    *(context_->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct);
  }

  if (target_ref == index_ref) {
    // Filter impossible index values.
    std::vector<int64> possible_indices;
    const Domain& index_domain = context_->DomainOf(index_ref);
    for (const ClosedInterval& interval : index_domain) {
      for (int64 value = interval.start; value <= interval.end; ++value) {
        const int ref = ct->element().vars(value);
        if (context_->DomainContains(ref, value)) {
          possible_indices.push_back(value);
        }
      }
    }
    if (possible_indices.size() < index_domain.Size()) {
      if (!context_->IntersectDomainWith(
              index_ref, Domain::FromValues(possible_indices))) {
        return true;
      }
    }
    context_->UpdateRuleStats(
        "element: reduce index domain when target equals index");

    return true;
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
  std::vector<int64> tuple(num_vars);
  std::vector<std::vector<int64>> new_tuples;
  new_tuples.reserve(num_tuples);
  std::vector<absl::flat_hash_set<int64>> new_domains(num_vars);
  std::vector<AffineRelation::Relation> affine_relations;

  bool modified_variables = false;
  for (int v = 0; v < num_vars; ++v) {
    const int var = ct->table().vars(v);
    AffineRelation::Relation r = context_->GetAffineRelation(PositiveRef(var));
    affine_relations.push_back(r);
    if (r.representative != var) {
      modified_variables = true;
    }
  }

  for (int i = 0; i < num_tuples; ++i) {
    bool delete_row = false;
    std::string tmp;
    for (int j = 0; j < num_vars; ++j) {
      const int ref = ct->table().vars(j);
      int64 v = ct->table().values(i * num_vars + j);
      const AffineRelation::Relation& r = affine_relations[j];
      if (r.representative != ref) {
        const int64 inverse_value = (v - r.offset) / r.coeff;
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
      const int ref = ct->table().vars(j);
      const int64 v = tuple[j];
      new_domains[j].insert(RefIsPositive(ref) ? v : -v);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&new_tuples);

  // Update the list of tuples if needed.
  if (new_tuples.size() < num_tuples || modified_variables) {
    ct->mutable_table()->clear_values();
    for (const std::vector<int64>& t : new_tuples) {
      for (const int64 v : t) {
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
            Domain::FromValues(std::vector<int64>(new_domains[j].begin(),
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
    std::vector<std::vector<int64>> var_to_values(num_vars);
    for (int j = 0; j < num_vars; ++j) {
      var_to_values[j].assign(new_domains[j].begin(), new_domains[j].end());
    }
    std::vector<std::vector<int64>> all_tuples(prod);
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
    std::vector<std::vector<int64>> diff(prod - new_tuples.size());
    std::set_difference(all_tuples.begin(), all_tuples.end(),
                        new_tuples.begin(), new_tuples.end(), diff.begin());

    // Negate the constraint.
    ct->mutable_table()->set_negated(!ct->table().negated());
    ct->mutable_table()->clear_values();
    for (const std::vector<int64>& t : diff) {
      for (const int64 v : t) ct->mutable_table()->add_values(v);
    }
    context_->UpdateRuleStats("table: negated");
  }
  return modified_variables;
}

bool CpModelPresolver::PresolveAllDiff(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  AllDifferentConstraintProto& all_diff = *ct->mutable_all_diff();

  const int size = all_diff.vars_size();
  if (size == 0) {
    context_->UpdateRuleStats("all_diff: empty constraint");
    return RemoveConstraint(ct);
  }
  if (size == 1) {
    context_->UpdateRuleStats("all_diff: only one variable");
    return RemoveConstraint(ct);
  }

  std::vector<int> new_variables;
  for (int i = 0; i < size; ++i) {
    if (!context_->IsFixed(all_diff.vars(i))) {
      new_variables.push_back(all_diff.vars(i));
      continue;
    }

    const int64 value = context_->MinOf(all_diff.vars(i));
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
    }
  }

  std::sort(new_variables.begin(), new_variables.end());
  for (int i = 1; i < new_variables.size(); ++i) {
    if (new_variables[i] == new_variables[i - 1]) {
      return context_->NotifyThatModelIsUnsat("Duplicate variable in all_diff");
    }
  }

  if (new_variables.size() < all_diff.vars_size()) {
    all_diff.mutable_vars()->Clear();
    for (const int var : new_variables) {
      all_diff.add_vars(var);
    }
    context_->UpdateRuleStats("all_diff: removed fixed variables");
    return true;
  }

  return false;
}

bool CpModelPresolver::IntervalsCanIntersect(
    const IntervalConstraintProto& interval1,
    const IntervalConstraintProto& interval2) {
  if (context_->MaxOf(interval1.end()) <= context_->MinOf(interval2.start()) ||
      context_->MaxOf(interval2.end()) <= context_->MinOf(interval1.start())) {
    return false;
  }
  return true;
}

namespace {

// Returns the sorted list of literals for given bool_or or at_most_one
// constraint.
std::vector<int> GetLiteralsFromSetPPCConstraint(ConstraintProto* ct) {
  std::vector<int> sorted_literals;
  if (ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne) {
    for (const int literal : ct->at_most_one().literals()) {
      sorted_literals.push_back(literal);
    }
  } else if (ct->constraint_case() ==
             ConstraintProto::ConstraintCase::kBoolOr) {
    for (const int literal : ct->bool_or().literals()) {
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
void ExtractClauses(const ClauseContainer& container, CpModelProto* proto) {
  // We regroup the "implication" into bool_and to have a more consise proto and
  // also for nicer information about the number of binary clauses.
  absl::flat_hash_map<int, int> ref_to_bool_and;
  for (int i = 0; i < container.NumClauses(); ++i) {
    const std::vector<Literal>& clause = container.Clause(i);
    if (clause.empty()) continue;

    // bool_and.
    if (clause.size() == 2) {
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

  const NoOverlapConstraintProto& proto = ct->no_overlap();

  // Filter absent intervals.
  int new_size = 0;
  for (int i = 0; i < proto.intervals_size(); ++i) {
    const int interval_index = proto.intervals(i);
    if (context_->working_model->constraints(interval_index)
            .constraint_case() ==
        ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
      continue;
    }
    ct->mutable_no_overlap()->set_intervals(new_size++, interval_index);
  }
  ct->mutable_no_overlap()->mutable_intervals()->Truncate(new_size);

  // TODO(user): This can be done without quadratic scan. Revisit if needed.
  if (proto.intervals_size() < 10000) {
    std::vector<bool> redundant_intervals(proto.intervals_size(), true);
    for (int i = 0; i + 1 < proto.intervals_size(); ++i) {
      for (int j = i + 1; j < proto.intervals_size(); ++j) {
        const int interval_1_index = proto.intervals(i);
        const int interval_2_index = proto.intervals(j);

        const IntervalConstraintProto interval1 =
            context_->working_model->constraints(interval_1_index).interval();
        const IntervalConstraintProto interval2 =
            context_->working_model->constraints(interval_2_index).interval();

        if (IntervalsCanIntersect(interval1, interval2)) {
          redundant_intervals[i] = false;
          redundant_intervals[j] = false;
        }
      }
    }

    new_size = 0;
    for (int i = 0; i < proto.intervals_size(); ++i) {
      if (redundant_intervals[i]) {
        context_->UpdateRuleStats("no_overlap: removed redundant intervals");
        continue;
      }
      const int interval_index = proto.intervals(i);
      ct->mutable_no_overlap()->set_intervals(new_size++, interval_index);
    }
    ct->mutable_no_overlap()->mutable_intervals()->Truncate(new_size);
  }

  if (proto.intervals_size() == 1) {
    context_->UpdateRuleStats("no_overlap: only one interval");
    return RemoveConstraint(ct);
  }
  if (proto.intervals().empty()) {
    context_->UpdateRuleStats("no_overlap: no intervals");
    return RemoveConstraint(ct);
  }
  return false;
}

bool CpModelPresolver::PresolveCumulative(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  const CumulativeConstraintProto& proto = ct->cumulative();

  // Filter absent intervals.
  int new_size = 0;
  bool changed = false;
  int num_zero_demand_removed = 0;
  for (int i = 0; i < proto.intervals_size(); ++i) {
    if (context_->working_model->constraints(proto.intervals(i))
            .constraint_case() ==
        ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
      continue;
    }

    const int demand_ref = proto.demands(i);
    const int64 demand_max = context_->MaxOf(demand_ref);
    if (demand_max == 0) {
      num_zero_demand_removed++;
      continue;
    }

    ct->mutable_cumulative()->set_intervals(new_size, proto.intervals(i));
    ct->mutable_cumulative()->set_demands(new_size, proto.demands(i));
    new_size++;
  }
  if (new_size < proto.intervals_size()) {
    changed = true;
    ct->mutable_cumulative()->mutable_intervals()->Truncate(new_size);
    ct->mutable_cumulative()->mutable_demands()->Truncate(new_size);
  }

  if (num_zero_demand_removed > 0) {
    context_->UpdateRuleStats("cumulative: removed intervals with no demands");
  }

  if (new_size == 0) {
    context_->UpdateRuleStats("cumulative: no intervals");
    return RemoveConstraint(ct);
  }

  if (HasEnforcementLiteral(*ct)) return changed;
  if (!context_->IsFixed(proto.capacity())) return changed;
  const int64 capacity = context_->MinOf(proto.capacity());

  const int size = proto.intervals_size();
  std::vector<int> start_indices(size, -1);

  int num_duration_one = 0;
  int num_greater_half_capacity = 0;

  bool has_optional_interval = false;
  for (int i = 0; i < size; ++i) {
    // TODO(user): adapt in the presence of optional intervals.
    const ConstraintProto& ct =
        context_->working_model->constraints(proto.intervals(i));
    if (!ct.enforcement_literal().empty()) has_optional_interval = true;
    const IntervalConstraintProto& interval = ct.interval();
    start_indices[i] = interval.start();
    const int duration_ref = interval.size();
    const int demand_ref = proto.demands(i);
    if (context_->IsFixed(duration_ref) && context_->MinOf(duration_ref) == 1) {
      num_duration_one++;
    }
    if (context_->MinOf(duration_ref) == 0) {
      // The behavior for zero-duration interval is currently not the same in
      // the no-overlap and the cumulative constraint.
      return changed;
    }
    const int64 demand_min = context_->MinOf(demand_ref);
    const int64 demand_max = context_->MaxOf(demand_ref);
    if (demand_min > capacity / 2) {
      num_greater_half_capacity++;
    }
    if (demand_min > capacity) {
      context_->UpdateRuleStats("cumulative: demand_min exceeds capacity");
      if (ct.enforcement_literal().empty()) {
        return context_->NotifyThatModelIsUnsat();
      } else {
        CHECK_EQ(ct.enforcement_literal().size(), 1);
        if (!context_->SetLiteralToFalse(ct.enforcement_literal(0))) {
          return true;
        }
      }
      return changed;
    } else if (demand_max > capacity) {
      if (ct.enforcement_literal().empty()) {
        context_->UpdateRuleStats("cumulative: demand_max exceeds capacity.");
        if (!context_->IntersectDomainWith(demand_ref,
                                           Domain(kint64min, capacity))) {
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

  if (num_greater_half_capacity == size) {
    if (num_duration_one == size && !has_optional_interval) {
      context_->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_all_diff();
      for (const int var : start_indices) {
        arg->add_vars(var);
      }
      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("cumulative: convert to no_overlap");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_no_overlap();
      for (const int interval : proto.intervals()) {
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

  // Convert the flat structure to a graph, note that we includes all the arcs
  // here (even if they are at false).
  std::vector<std::vector<int>> incoming_arcs;
  std::vector<std::vector<int>> outgoing_arcs;
  const int num_arcs = proto.literals_size();
  int num_nodes = 0;
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

  int num_fixed_at_true = 0;
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
      if (num_true > 0) {
        for (const int ref : refs) {
          if (ref != true_ref) {
            if (!context_->SetLiteralToFalse(ref)) return true;
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
      const int64 label = proto.transition_label(t);
      int64 inverse_label = (label - rep.offset) / rep.coeff;
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
    const int64 label = proto.transition_label(t);
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
  std::vector<std::set<int64>> reachable_states(n + 1);
  reachable_states[0].insert(proto.starting_state());
  reachable_states[n] = {proto.final_states().begin(),
                         proto.final_states().end()};

  // Forward.
  //
  // TODO(user): filter using the domain of vars[time] that may not contain
  // all the possible transitions.
  for (int time = 0; time + 1 < n; ++time) {
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64 tail = proto.transition_tail(t);
      const int64 label = proto.transition_label(t);
      const int64 head = proto.transition_head(t);
      if (!gtl::ContainsKey(reachable_states[time], tail)) continue;
      if (!context_->DomainContains(vars[time], label)) continue;
      reachable_states[time + 1].insert(head);
    }
  }

  std::vector<std::set<int64>> reached_values(n);

  // Backward.
  for (int time = n - 1; time >= 0; --time) {
    std::set<int64> new_set;
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64 tail = proto.transition_tail(t);
      const int64 label = proto.transition_label(t);
      const int64 head = proto.transition_head(t);

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

// TODO(user): It is probably more efficient to keep all the bool_and in a
// global place during all the presolve, and just output them at the end rather
// than modifying more than once the proto.
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
  // TODO(user): remove code duplication with cp_mode_solver. Here we also do
  // not run the heuristic to decide which variable to fully encode.
  //
  // TODO(user): Maybe do not load slow to propagate constraints? for instance
  // we do not use any linear relaxation here.
  Model model;

  // Adapt some of the parameters during this probing phase.
  auto* local_param = model.GetOrCreate<SatParameters>();
  *local_param = options_.parameters;
  local_param->set_use_implied_bounds(false);

  model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(options_.time_limit);
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  encoder->DisableImplicationBetweenLiteral();
  auto* mapping = model.GetOrCreate<CpModelMapping>();
  mapping->CreateVariables(model_proto, false, &model);
  mapping->DetectOptionalVariables(model_proto, &model);
  mapping->ExtractEncoding(model_proto, &model);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) continue;
    CHECK(LoadConstraint(ct, &model));
    if (sat_solver->IsModelUnsat()) {
      return (void)context_->NotifyThatModelIsUnsat();
    }
  }
  encoder->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->Propagate()) {
    return (void)context_->NotifyThatModelIsUnsat();
  }

  // Probe.
  //
  // TODO(user): Compute the transitive reduction instead of just the
  // equivalences, and use the newly learned binary clauses?
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  ProbeBooleanVariables(/*deterministic_time_limit=*/1.0, &model);
  if (options_.time_limit != nullptr) {
    options_.time_limit->AdvanceDeterministicTime(
        model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  }
  if (sat_solver->IsModelUnsat() || !implication_graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat();
  }

  // Update the presolve context with fixed Boolean variables.
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
      const Domain new_domain =
          integer_trail->InitialVariableDomain(mapping->Integer(var));
      if (!context_->IntersectDomainWith(var, new_domain)) {
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

void CpModelPresolver::PresolvePureSatPart() {
  // TODO(user,user): Reenable some SAT presolve with
  // keep_all_feasible_solutions set to true.
  if (context_->ModelIsUnsat() || context_->keep_all_feasible_solutions) return;

  const int num_variables = context_->working_model->variables_size();
  SatPostsolver sat_postsolver(num_variables);
  SatPresolver sat_presolver(&sat_postsolver);
  sat_presolver.SetNumVariables(num_variables);
  sat_presolver.SetTimeLimit(options_.time_limit);

  SatParameters params;

  // TODO(user): enable blocked clause. The problem is that our postsolve
  // do not support changing the value of a variable from the solution of the
  // presolved problem, and we do need this for blocked clause.
  params.set_presolve_blocked_clause(false);

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
  int num_removable = 0;
  std::vector<bool> can_be_removed(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    if (context_->VarToConstraints(i).empty()) {
      ++num_removable;
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
  VLOG(1) << "num removable Booleans: " << num_removable;
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
  ExtractClauses(sat_presolver, context_->working_model);

  // Update the constraints <-> variables graph.
  context_->UpdateNewConstraintsVariableUsage();

  // Add the sat_postsolver clauses to mapping_model.
  ExtractClauses(sat_postsolver, context_->mapping_model);
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
        context_->affine_constraints.contains(&ct) ||
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
    int64 objective_coeff_in_expanded_constraint;
    int64 size_of_expanded_constraint = 0;
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
      bool is_present = false;
      int64 objective_coeff;
      for (int i = 0; i < num_terms; ++i) {
        const int ref = ct.linear().vars(i);
        const int64 coeff = ct.linear().coeffs(i);
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
      if (context_->VarToConstraints(objective_var).size() == 1) {
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
      &cliques, options_.parameters.merge_no_overlap_work_limit());

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
        cliques.push_back({enforcement, convert(ref).Negated()});
      }
      if (RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }

  const int old_cliques = cliques.size();

  // We reuse the max-clique code from sat.
  Model local_model;
  const int num_variables = context_->working_model->variables().size();
  local_model.GetOrCreate<Trail>()->Resize(num_variables);
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(num_variables);
  for (const std::vector<Literal>& clique : cliques) {
    if (!graph->AddAtMostOne(clique)) {
      return (void)context_->NotifyThatModelIsUnsat();
    }
  }
  if (!graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat();
  }
  graph->TransformIntoMaxCliques(
      &cliques, options_.parameters.merge_at_most_one_work_limit());

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

  int new_cliques = 0;
  for (const std::vector<Literal>& clique : cliques) {
    if (clique.empty()) continue;
    new_cliques++;
    ConstraintProto* ct = context_->working_model->add_constraints();
    for (const Literal literal : clique) {
      if (literal.IsPositive()) {
        ct->mutable_at_most_one()->add_literals(literal.Variable().value());
      } else {
        ct->mutable_at_most_one()->add_literals(
            NegatedRef(literal.Variable().value()));
      }
    }
  }
  context_->UpdateNewConstraintsVariableUsage();
  VLOG(1) << "Merged " << old_cliques << " into " << new_cliques << " cliques";
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
        ExtractEnforcementLiteralFromLinearConstraint(ct);
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
    case ConstraintProto::ConstraintCase::kCumulative:
      return PresolveCumulative(ct);
    case ConstraintProto::ConstraintCase::kCircuit:
      return PresolveCircuit(ct);
    case ConstraintProto::ConstraintCase::kRoutes:
      return PresolveRoutes(ct);
    case ConstraintProto::ConstraintCase::kAutomaton:
      return PresolveAutomaton(ct);
    default:
      return false;
  }
}

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
  if (ct1->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr &&
      ct2->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne) {
    // fix extras in c2 to 0
    for (const int literal : c2_minus_c1) {
      if (!context_->SetLiteralToFalse(literal)) return true;
      context_->UpdateRuleStats("setppc: fixed variables");
    }
    return true;
  }
  if (ct1->constraint_case() == ct2->constraint_case()) {
    if (ct1->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      (*marked_for_removal)[c2] = true;
      context_->UpdateRuleStats("setppc: removed dominated constraints");
      return false;
    }
    CHECK_EQ(ct1->constraint_case(),
             ConstraintProto::ConstraintCase::kAtMostOne);
    (*marked_for_removal)[c1] = true;
    context_->UpdateRuleStats("setppc: removed dominated constraints");
    return false;
  }
  return false;
}

// TODO(user,user): TransformIntoMaxCliques() convert the bool_and to
// at_most_one, but maybe also duplicating them into bool_or would allow this
// function to do more presolving.
bool CpModelPresolver::ProcessSetPPC() {
  bool changed = false;
  const int num_constraints = context_->working_model->constraints_size();

  // Signatures of all the constraints. In the signature the bit i is 1 if it
  // contains a literal l such that l%64 = i.
  std::vector<uint64> signatures;

  // Graph of constraints to literals. constraint_literals[c] contains all the
  // literals in constraint indexed by 'c' in sorted order.
  std::vector<std::vector<int>> constraint_literals;

  // Graph of literals to constraints. literals_to_constraints[l] contains the
  // vector of constraint indices in which literal 'l' or 'neg(l)' appears.
  std::vector<std::vector<int>> literals_to_constraints;

  // vector of booleans indicating if the constraint is marked for removal. Note
  // that we don't remove constraints while processing them but remove all the
  // marked ones at the end.
  std::vector<bool> marked_for_removal;

  // The containers above use the local indices for setppc constraints. We store
  // the original constraint indices corresponding to those local indices here.
  std::vector<int> original_constraint_index;

  // Fill the initial constraint <-> literal graph, compute signatures and
  // initialize other containers defined above.
  int num_setppc_constraints = 0;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne) {
      // Because TransformIntoMaxCliques() can detect literal equivalence
      // relation, we make sure the constraints are presolved before being
      // inspected.
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return false;
    }
    if (ct->constraint_case() == ConstraintProto::ConstraintCase::kBoolOr ||
        ct->constraint_case() == ConstraintProto::ConstraintCase::kAtMostOne) {
      constraint_literals.push_back(GetLiteralsFromSetPPCConstraint(ct));

      uint64 signature = 0;
      for (const int literal : constraint_literals.back()) {
        const int positive_literal = PositiveRef(literal);
        signature |= (int64{1} << (positive_literal % 64));
        DCHECK_GE(positive_literal, 0);
        if (positive_literal >= literals_to_constraints.size()) {
          literals_to_constraints.resize(positive_literal + 1);
        }
        literals_to_constraints[positive_literal].push_back(
            num_setppc_constraints);
      }
      signatures.push_back(signature);
      marked_for_removal.push_back(false);
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
      if (options_.time_limit != nullptr &&
          options_.time_limit->LimitReached()) {
        return changed;
      }
      const int c1 = literal_to_constraints[index1];
      if (marked_for_removal[c1]) continue;
      const std::vector<int>& c1_literals = constraint_literals[c1];
      ConstraintProto* ct1 = context_->working_model->mutable_constraints(
          original_constraint_index[c1]);
      for (int index2 = index1 + 1; index2 < literal_to_constraints.size();
           ++index2) {
        const int c2 = literal_to_constraints[index2];
        if (marked_for_removal[c2]) continue;
        if (marked_for_removal[c1]) break;
        // TODO(user): This should not happen. Investigate.
        if (c1 == c2) continue;

        CHECK_LT(c1, c2);
        if (gtl::ContainsKey(compared_constraints,
                             std::pair<int, int>(c1, c2))) {
          continue;
        }
        compared_constraints.insert({c1, c2});

        // Hard limit on number of comparisions to avoid spending too much time
        // here.
        if (compared_constraints.size() >= 50000) return changed;

        const bool smaller = (signatures[c1] & ~signatures[c2]) == 0;
        const bool larger = (signatures[c2] & ~signatures[c1]) == 0;

        if (!(smaller || larger)) {
          continue;
        }

        // Check if literals in c1 is subset of literals in c2 or vice versa.
        const std::vector<int>& c2_literals = constraint_literals[c2];
        ConstraintProto* ct2 = context_->working_model->mutable_constraints(
            original_constraint_index[c2]);
        // TODO(user): Try avoiding computation of set differences if
        // possible.
        std::vector<int> c1_minus_c2;
        gtl::STLSetDifference(c1_literals, c2_literals, &c1_minus_c2);
        std::vector<int> c2_minus_c1;
        gtl::STLSetDifference(c2_literals, c1_literals, &c2_minus_c1);

        if (c1_minus_c2.empty() && c2_minus_c1.empty()) {
          if (ct1->constraint_case() == ct2->constraint_case()) {
            marked_for_removal[c2] = true;
            context_->UpdateRuleStats("setppc: removed redundant constraints");
          }
        } else if (c1_minus_c2.empty()) {
          if (ProcessSetPPCSubset(c1, c2, c2_minus_c1,
                                  original_constraint_index,
                                  &marked_for_removal)) {
            context_->UpdateConstraintVariableUsage(
                original_constraint_index[c1]);
            context_->UpdateConstraintVariableUsage(
                original_constraint_index[c2]);
          }
        } else if (c2_minus_c1.empty()) {
          if (ProcessSetPPCSubset(c2, c1, c1_minus_c2,
                                  original_constraint_index,
                                  &marked_for_removal)) {
            context_->UpdateConstraintVariableUsage(
                original_constraint_index[c1]);
            context_->UpdateConstraintVariableUsage(
                original_constraint_index[c2]);
          }
        }
      }
    }
  }
  for (int c = 0; c < num_setppc_constraints; ++c) {
    if (marked_for_removal[c]) {
      ConstraintProto* ct = context_->working_model->mutable_constraints(
          original_constraint_index[c]);
      changed = RemoveConstraint(ct);
      context_->UpdateConstraintVariableUsage(original_constraint_index[c]);
    }
  }

  return changed;
}

void CpModelPresolver::CanonicalizeAffineDomain(int var) {
  CHECK(RefIsPositive(var));
  if (context_->ModelIsUnsat()) return;
  if (context_->IsFixed(var)) return;
  if (context_->VariableIsNotUsedAnymore(var)) return;

  const AffineRelation::Relation r = context_->GetAffineRelation(var);
  if (r.representative != var) return;
  if (context_->VariableIsOnlyUsedInEncoding(var)) {
    // TODO(user): We can remove such variable and its constraints by:
    // - Adding proper constraints on the enforcement literals. Simple case is
    //   exactly one (or at most one) in case of a full (or partial) encoding.
    // - Moving all the old constraints to the mapping model.
    // - Updating the search heuristics/hint properly.
    context_->UpdateRuleStats("TODO variables: only used in encoding.");
  }

  const Domain& domain = context_->DomainOf(var);

  // Special case for non-Boolean domain of size 2. We will try to reuse the
  // encoding literals if present.
  if (domain.Size() == 2 && (domain.Min() != 0 || domain.Max() != 1)) {
    // Shifted and/or scaled Boolean variable.
    const int64 var_min = context_->MinOf(var);
    const int64 var_max = context_->MaxOf(var);

    int literal;
    if (!context_->HasVarValueEncoding(var, var_max, &literal)) {
      literal = context_->NewBoolVar();
    }

    ConstraintProto* const ct = context_->working_model->add_constraints();
    LinearConstraintProto* const lin = ct->mutable_linear();
    lin->add_vars(var);
    lin->add_coeffs(1);
    lin->add_vars(PositiveRef(literal));
    if (RefIsPositive(literal)) {
      lin->add_coeffs(var_min - var_max);
      lin->add_domain(var_min);
      lin->add_domain(var_min);
      context_->StoreAffineRelation(*ct, var, literal, var_max - var_min,
                                    var_min);
    } else {
      lin->add_coeffs(var_max - var_min);
      lin->add_domain(var_max);
      lin->add_domain(var_max);
      context_->StoreAffineRelation(*ct, var, NegatedRef(literal),
                                    var_min - var_max, var_max);
    }
    return;
  }

  if (domain.NumIntervals() != domain.Size()) return;

  const int64 var_min = domain.Min();
  int64 gcd = domain[1].start - var_min;
  for (int index = 2; index < domain.NumIntervals(); ++index) {
    const ClosedInterval& i = domain[index];
    CHECK_EQ(i.start, i.end);
    const int64 shifted_value = i.start - var_min;
    CHECK_GE(shifted_value, 0);

    gcd = MathUtil::GCD64(gcd, shifted_value);
    if (gcd == 1) break;
  }
  // TODO(user): Experiment with reduction if gcd == 1 && var_min != 0.
  if (gcd == 1) return;

  int new_var_index;
  {
    std::vector<int64> scaled_values;
    for (int index = 0; index < domain.NumIntervals(); ++index) {
      const ClosedInterval& i = domain[index];
      CHECK_EQ(i.start, i.end);
      const int64 shifted_value = i.start - var_min;
      scaled_values.push_back(shifted_value / gcd);
    }
    new_var_index = context_->NewIntVar(Domain::FromValues(scaled_values));
  }
  if (context_->ModelIsUnsat()) return;

  ConstraintProto* const ct = context_->working_model->add_constraints();
  LinearConstraintProto* const lin = ct->mutable_linear();
  lin->add_vars(var);
  lin->add_coeffs(1);
  lin->add_vars(new_var_index);
  lin->add_coeffs(-gcd);
  lin->add_domain(var_min);
  lin->add_domain(var_min);

  context_->StoreAffineRelation(*ct, var, new_var_index, gcd, var_min);
  context_->UpdateRuleStats("variables: canonicalize affine domain");
  context_->UpdateNewConstraintsVariableUsage();
}

void CpModelPresolver::PresolveToFixPoint() {
  if (context_->ModelIsUnsat()) return;

  // Do one pass to try to simplify the domain of variables.
  {
    const int num_vars = context_->working_model->variables_size();
    for (int var = 0; var < num_vars; ++var) {
      CanonicalizeAffineDomain(var);
    }
  }

  // Limit on number of operations.
  const int64 max_num_operations =
      options_.parameters.cp_model_max_num_presolve_operations() > 0
          ? options_.parameters.cp_model_max_num_presolve_operations()
          : kint64max;

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  absl::flat_hash_set<std::pair<int, int>> var_constraint_pair_already_called;

  TimeLimit* time_limit = options_.time_limit;

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
  std::sort(queue.begin(), queue.end(), [this](int a, int b) {
    const int score_a = context_->ConstraintToVars(a).size();
    const int score_b = context_->ConstraintToVars(b).size();
    return score_a < score_b || (score_a == score_b && a < b);
  });

  while (!queue.empty() && !context_->ModelIsUnsat()) {
    if (time_limit != nullptr && time_limit->LimitReached()) break;
    if (context_->num_presolve_operations > max_num_operations) break;
    while (!queue.empty() && !context_->ModelIsUnsat()) {
      if (time_limit != nullptr && time_limit->LimitReached()) break;
      if (context_->num_presolve_operations > max_num_operations) break;
      const int c = queue.front();
      in_queue[c] = false;
      queue.pop_front();

      const int old_num_constraint =
          context_->working_model->constraints_size();
      const bool changed = PresolveOneConstraint(c);
      if (context_->ModelIsUnsat() && options_.log_info) {
        LOG(INFO) << "Unsat after presolving constraint #" << c
                  << " (warning, dump might be inconsistent): "
                  << context_->working_model->constraints(c).ShortDebugString();
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
      // We loose a bit of preformance, but the code is simpler.
      if (changed) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }

    // Re-add to the queue the constraints that touch a variable that changed.
    // Note that it is important to use indices in the loop below because
    // CanonicalizeAffineDomain() might create new variables which will change
    // the set of modified domains.
    //
    // TODO(user): Avoid reprocessing the constraints that changed the variables
    // with the use of timestamp.
    if (context_->ModelIsUnsat()) return;
    for (int i = 0;
         i < context_->modified_domains.PositionsSetAtLeastOnce().size(); ++i) {
      const int v = context_->modified_domains.PositionsSetAtLeastOnce()[i];
      if (context_->IsFixed(v)) {
        context_->ExploitFixedDomain(v);
      } else {
        // The domain changed, maybe we can canonicalize it.
        //
        // Important: This code is a bit brittle, because it assumes
        // PositionsSetAtLeastOnce() will not change behind our back. That
        // should however be the case because CanonicalizeAffineDomain() will
        // only mark as modified via AddAffineRelation a variable that is
        // already present in the modified set.
        CanonicalizeAffineDomain(v);
        in_queue.resize(context_->working_model->constraints_size(), false);
      }
      for (const int c : context_->VarToConstraints(v)) {
        if (c >= 0 && !in_queue[c]) {
          in_queue[c] = true;
          queue.push_back(c);
        }
      }
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
      if (in_queue[c]) continue;
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
        // TODO(user): Implement if we ever support optional intervals in
        // this constraint. Currently we do not.
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
        for (const auto pair :
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

void CpModelPresolver::RemoveUnusedEquivalentVariables() {
  if (context_->ModelIsUnsat() || context_->keep_all_feasible_solutions) return;

  // Remove all affine constraints (they will be re-added later if
  // needed) in the presolved model.
  for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (gtl::ContainsKey(context_->affine_constraints, ct)) {
      ct->Clear();
      context_->UpdateConstraintVariableUsage(c);
      continue;
    }
  }

  // Add back the affine relations to the presolved model or to the mapping
  // model, depending where they are needed.
  //
  // TODO(user): unfortunately, for now, this duplicates the interval relations
  // with a fixed size.
  int num_affine_relations = 0;
  for (int var = 0; var < context_->working_model->variables_size(); ++var) {
    if (context_->IsFixed(var)) continue;

    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative == var) continue;

    // We can get rid of this variable, only if:
    // - it is not used elsewhere.
    // - whatever the value of the representative, we can always find a value
    //   for this variable.
    CpModelProto* proto;
    if (context_->VarToConstraints(var).empty()) {
      // Make sure that domain(representative) is tight.
      const Domain implied = context_->DomainOf(var)
                                 .AdditionWith({-r.offset, -r.offset})
                                 .InverseMultiplicationBy(r.coeff);
      bool domain_modified = false;
      if (!context_->IntersectDomainWith(r.representative, implied,
                                         &domain_modified)) {
        return;
      }
      if (domain_modified) {
        LOG(WARNING) << "Domain of " << r.representative
                     << " was not fully propagated using the affine relation "
                     << "(var = " << var
                     << " representative = " << r.representative
                     << " coeff = " << r.coeff << " offset = " << r.offset
                     << ")";
      }
      proto = context_->mapping_model;
    } else {
      proto = context_->working_model;
      ++num_affine_relations;
    }

    ConstraintProto* ct = proto->add_constraints();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
  }

  VLOG(1) << "num_affine_relations kept = " << num_affine_relations;

  // Update the variable usage.
  context_->UpdateNewConstraintsVariableUsage();
}

void LogInfoFromContext(const PresolveContext* context) {
  LOG(INFO) << "- " << context->NumAffineRelations()
            << " affine relations were detected.";
  LOG(INFO) << "- " << context->NumEquivRelations()
            << " variable equivalence relations were detected.";
  std::map<std::string, int> sorted_rules(context->stats_by_rule_name.begin(),
                                          context->stats_by_rule_name.end());
  for (const auto& entry : sorted_rules) {
    if (entry.second == 1) {
      LOG(INFO) << "- rule '" << entry.first << "' was applied 1 time.";
    } else {
      LOG(INFO) << "- rule '" << entry.first << "' was applied " << entry.second
                << " times.";
    }
  }
}

// =============================================================================
// Public API.
// =============================================================================

bool PresolveCpModel(const PresolveOptions& options, PresolveContext* context,
                     std::vector<int>* postsolve_mapping) {
  CpModelPresolver presolver(options, context, postsolve_mapping);
  return presolver.Presolve();
}

CpModelPresolver::CpModelPresolver(const PresolveOptions& options,
                                   PresolveContext* context,
                                   std::vector<int>* postsolve_mapping)
    : options_(options),
      postsolve_mapping_(postsolve_mapping),
      context_(context) {
  context_->keep_all_feasible_solutions =
      options.parameters.enumerate_all_solutions() ||
      options.parameters.fill_tightened_domains_in_response() ||
      !options.parameters.cp_model_presolve();

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
  context_->enable_stats = options_.log_info;

  // If presolve is false, just run expansion.
  if (!options_.parameters.cp_model_presolve()) {
    ExpandCpModel(options_, context_);
    if (options_.log_info) LogInfoFromContext(context_);
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
      case ConstraintProto::ConstraintCase::kLinear:
        CanonicalizeLinear(ct);
        break;
      default:
        break;
    }
    if (context_->ModelIsUnsat()) break;
  }
  context_->UpdateNewConstraintsVariableUsage();
  DCHECK(context_->ConstraintVariableUsageIsConsistent());

  // Main propagation loop.
  for (int iter = 0; iter < options_.parameters.max_presolve_iterations();
       ++iter) {
    context_->UpdateRuleStats("presolve: iteration");
    // Save some quantities to decide if we abort early the iteration loop.
    const int64 old_num_presolve_op = context_->num_presolve_operations;
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
    // presolved problem reduces it even more. This is probably due to
    // RemoveUnusedEquivalentVariables(). We should really improve the handling
    // of equivalence.
    PresolveToFixPoint();

    // Call expansion.
    ExpandCpModel(options_, context_);

    // TODO(user): do that and the pure-SAT part below more than once.
    if (options_.parameters.cp_model_probing_level() > 0) {
      if (options_.time_limit == nullptr ||
          !options_.time_limit->LimitReached()) {
        Probe();
        PresolveToFixPoint();
      }
    }

    // Runs SAT specific presolve on the pure-SAT part of the problem.
    // Note that because this can only remove/fix variable not used in the other
    // part of the problem, there is no need to redo more presolve afterwards.
    if (options_.parameters.cp_model_use_sat_presolve()) {
      if (options_.time_limit == nullptr ||
          !options_.time_limit->LimitReached()) {
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
        if (gtl::ContainsKey(context_->affine_constraints, ct)) {
          continue;
        }
        ExtractAtMostOneFromLinear(ct);
      }
      context_->UpdateNewConstraintsVariableUsage();
    }

    if (iter == 0) TransformIntoMaxCliques();

    // Process set packing, partitioning and covering constraint.
    if (options_.time_limit == nullptr ||
        !options_.time_limit->LimitReached()) {
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
  if (!context_->ModelIsUnsat()) MergeNoOverlapConstraints();

  if (context_->working_model->has_objective() && !context_->ModelIsUnsat()) {
    ExpandObjective();
  }

  if (context_->ModelIsUnsat()) {
    if (options_.log_info) LogInfoFromContext(context_);

    // Set presolved_model to the simplest UNSAT problem (empty clause).
    context_->working_model->Clear();
    context_->working_model->add_constraints()->mutable_bool_or();
    return true;
  }

  // Note: Removing unused equivalent variables should be done at the end.
  RemoveUnusedEquivalentVariables();

  // TODO(user): Past this point the context.constraint_to_vars[] graph is not
  // consistent and shouldn't be used. We do use var_to_constraints.size()
  // though.
  if (options_.time_limit == nullptr || !options_.time_limit->LimitReached()) {
    DCHECK(context_->ConstraintVariableUsageIsConsistent());
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

  SyncDomainAndRemoveEmptyConstraints();

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
    for (const int ref : copy.variables()) {
      const int var = PositiveRef(ref);

      // Remove fixed variables.
      if (context_->ModelIsUnsat()) return true;
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
          strategy.add_variables(rep);
          if (strategy.variable_selection_strategy() !=
              DecisionStrategyProto::CHOOSE_FIRST) {
            DecisionStrategyProto::AffineTransformation* t =
                strategy.add_transformations();
            t->set_var(rep);
            t->set_offset(r.offset);
            t->set_positive_coeff(std::abs(r.coeff));
          }
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

  // Set the variables of the mapping_model.
  context_->mapping_model->mutable_variables()->CopyFrom(
      context_->working_model->variables());

  // Remove all the unused variables from the presolved model.
  postsolve_mapping_->clear();
  std::vector<int> mapping(context_->working_model->variables_size(), -1);
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    if (context_->VarToConstraints(i).empty() &&
        !context_->keep_all_feasible_solutions) {
      continue;
    }
    mapping[i] = postsolve_mapping_->size();
    postsolve_mapping_->push_back(i);
  }
  ApplyVariableMapping(mapping, *context_);

  // Hack to display the number of deductions stored.
  if (context_->deductions.NumDeductions() > 0) {
    context_->UpdateRuleStats(absl::StrCat(
        "deductions: ", context_->deductions.NumDeductions(), " stored"));
  }

  // Stats and checks.
  if (options_.log_info) LogInfoFromContext(context_);

  // One possible error that is difficult to avoid here: because of our
  // objective expansion, we might detect a possible overflow...
  //
  // TODO(user): We could abort the expansion when this happen.
  if (!ValidateCpModel(*context_->working_model).empty()) return false;
  if (!ValidateCpModel(*context_->mapping_model).empty()) return false;
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

  // Remap the search decision heuristic.
  // Note that we delete any heuristic related to a removed variable.
  for (DecisionStrategyProto& strategy : *proto->mutable_search_strategy()) {
    DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    for (const int ref : copy.variables()) {
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        strategy.add_variables(RefIsPositive(ref) ? image : NegatedRef(image));
      }
    }
    strategy.clear_transformations();
    for (const auto& transform : copy.transformations()) {
      const int ref = transform.var();
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        auto* new_transform = strategy.add_transformations();
        *new_transform = transform;
        new_transform->set_var(RefIsPositive(ref) ? image : NegatedRef(image));
      }
    }
  }

  // Remap the solution hint.
  if (proto->has_solution_hint()) {
    auto* mutable_hint = proto->mutable_solution_hint();
    int new_size = 0;
    for (int i = 0; i < mutable_hint->vars_size(); ++i) {
      const int old_ref = mutable_hint->vars(i);
      const int64 old_value = mutable_hint->values(i);

      // Note that if (old_value - r.offset) is not divisible by r.coeff, then
      // the hint is clearly infeasible, but we still set it to a "close" value.
      const AffineRelation::Relation r = context.GetAffineRelation(old_ref);
      const int var = r.representative;
      const int64 value = (old_value - r.offset) / r.coeff;

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

  // We use a map hash: serialized_constraint_proto -> constraint index.
  absl::flat_hash_map<int64, int> equiv_constraints;

  std::string s;
  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    if (model_proto.constraints(c).constraint_case() ==
        ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
      continue;
    }
    s = model_proto.constraints(c).SerializeAsString();
    const int64 hash = std::hash<std::string>()(s);
    const auto insert = equiv_constraints.insert({hash, c});
    if (!insert.second) {
      // Already present!
      const int other_c_with_same_hash = insert.first->second;
      if (s ==
          model_proto.constraints(other_c_with_same_hash).SerializeAsString()) {
        result.push_back(c);
      }
    }
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
