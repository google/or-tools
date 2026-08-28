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

#include "ortools/sat/cp_constraint_presolve.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/sat/2d_rectangle_presolve.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_table.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/diophantine.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

LinearExpression2 GetLinearExpression2FromProto(int a, int64_t coeff_a, int b,
                                                int64_t coeff_b) {
  LinearExpression2 result;
  DCHECK(RefIsPositive(a));
  DCHECK(RefIsPositive(b));
  result.vars[0] = IntegerVariable(2 * a);
  result.vars[1] = IntegerVariable(2 * b);
  result.coeffs[0] = IntegerValue(coeff_a);
  result.coeffs[1] = IntegerValue(coeff_b);
  return result;
}

}  // namespace

bool CpConstraintPresolver::RemoveConstraint(ConstraintProto* ct) {
  ct->Clear();
  return true;
}

// Remove all empty constraints and duplicated intervals. Note that we need to
// remap the interval references.
//
// Now that they have served their purpose, we also remove dummy constraints,
// otherwise that causes issue because our model are invalid in tests.
void CpConstraintPresolver::RemoveEmptyConstraints() {
  // This should only be done at the end of presolve, so it is fine to break
  // context_ invariants.
  CpModelProto* cp_model = context_->UnsafeMutableWorkingModel();

  const int old_num_non_empty_constraints = context_->NumConstraints();
  interval_representative_.clear();
  std::vector<int> interval_mapping(old_num_non_empty_constraints, -1);
  int new_num_constraints = 0;
  for (int c = 0; c < old_num_non_empty_constraints; ++c) {
    const auto type = context_->Constraint(c).constraint_case();
    if (type == ConstraintProto::CONSTRAINT_NOT_SET) continue;
    if (type == ConstraintProto::kDummyConstraint) continue;
    cp_model->mutable_constraints(new_num_constraints)
        ->Swap(cp_model->mutable_constraints(c));
    if (type == ConstraintProto::kInterval) {
      // Warning: interval_representative_ holds a pointer to the working model
      // to compute hashes, so we need to be careful about not changing a
      // constraint after its index is added to the map.
      const auto [it, inserted] = interval_representative_.insert(
          {new_num_constraints, new_num_constraints});
      interval_mapping[c] = it->second;
      if (!inserted) {
        context_->UpdateRuleStats(
            "intervals: change duplicate index across constraints");
        continue;
      }
    }

    // After first copy, interval should always be defined before they are
    // used, so we can remap as we re-index.
    ApplyToAllIntervalIndices(
        [&interval_mapping](int* ref) {
          *ref = interval_mapping[*ref];
          CHECK_NE(-1, *ref);
        },
        cp_model->mutable_constraints(new_num_constraints));

    new_num_constraints++;
  }
  google::protobuf::util::Truncate(cp_model->mutable_constraints(),
                                   new_num_constraints);
}

bool CpConstraintPresolver::PresolveEnforcementLiteral(ConstraintProto* ct,
                                                       bool* changed) {
  *changed = false;
  if (context_->ModelIsUnsat()) return false;
  if (!HasEnforcementLiteral(*ct)) return true;

  auto remove_if_not_interval = [this, changed, ct]() {
    *changed = true;
    if (ct->constraint_case() == ConstraintProto::kInterval) {
      return MarkOptionalIntervalAsFalse(ct);
    } else {
      return RemoveConstraint(ct);
    }
  };

  int new_size = 0;
  const int old_size = ct->enforcement_literal().size();
  context_->tmp_literal_set.clear();
  for (const int literal : ct->enforcement_literal()) {
    if (context_->LiteralIsTrue(literal)) {
      // We can remove a literal at true.
      context_->UpdateRuleStats("enforcement: true literal");
      continue;
    }

    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("enforcement: false literal");
      return remove_if_not_interval();
    }

    if (context_->VariableIsUniqueAndRemovable(literal)) {
      // We can simply set it to false and ignore the constraint in this case.
      context_->UpdateRuleStats("enforcement: literal not used");
      CHECK(context_->SetLiteralToFalse(literal));
      return remove_if_not_interval();
    }

    // If the literal only appear in the objective, we might be able to fix it
    // to false. TODO(user): generalize if the literal always appear with the
    // same polarity.
    if (context_->VariableWithCostIsUniqueAndRemovable(literal)) {
      const int64_t obj_coeff =
          context_->ObjectiveMap().at(PositiveRef(literal));
      if (RefIsPositive(literal) == (obj_coeff > 0)) {
        // It is just more advantageous to set it to false!
        context_->UpdateRuleStats("enforcement: literal with unique direction");
        CHECK(context_->SetLiteralToFalse(literal));
        return remove_if_not_interval();
      }
    }

    // Deals with duplicate literals.
    //
    // TODO(user): Ideally we could do that just once during the first copy,
    // and later never create such constraint.
    if (old_size > 1) {
      const auto [_, inserted] = context_->tmp_literal_set.insert(literal);
      if (!inserted) {
        context_->UpdateRuleStats("enforcement: removed duplicate literal");
        continue;
      }
      if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
        context_->UpdateRuleStats("enforcement: can never be true");
        return remove_if_not_interval();
      }
    }

    ct->set_enforcement_literal(new_size++, literal);
  }
  *changed = new_size != old_size;
  ct->mutable_enforcement_literal()->Truncate(new_size);
  return true;
}

bool CpConstraintPresolver::PresolveBoolXor(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

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

  if (new_size == 0) {
    if (num_true_literals % 2 == 0) {
      return MarkConstraintAsFalse(ct, "bool_xor: always false");
    } else {
      context_->UpdateRuleStats("bool_xor: always true");
      return RemoveConstraint(ct);
    }
  } else if (new_size == 1 && !HasEnforcementLiteral(*ct)) {
    // We can fix the only active literal.
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
        return MarkConstraintAsFalse(ct, "bool_xor: always false");
      } else {
        context_->UpdateRuleStats("bool_xor: always true");
        return RemoveConstraint(ct);
      }
    }
    if (a == NegatedRef(b)) {
      if (num_true_literals % 2 == 1) {
        return MarkConstraintAsFalse(ct, "bool_xor: always false");
      } else {
        context_->UpdateRuleStats("bool_xor: always true");
        return RemoveConstraint(ct);
      }
    }
    if (!HasEnforcementLiteral(*ct)) {
      if (num_true_literals % 2 == 0) {  // a == not(b).
        if (!context_->StoreBooleanEqualityRelation(a, NegatedRef(b))) {
          return false;
        }
      } else {  // a == b.
        if (!context_->StoreBooleanEqualityRelation(a, b)) {
          return false;
        }
      }
      context_->UpdateRuleStats("bool_xor: two active literals");
      return RemoveConstraint(ct);
    }  // TODO(user): maybe replace the enforced XOR by an enforced equality?
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

bool CpConstraintPresolver::PresolveBoolOr(ConstraintProto* ct) {
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
  //
  // TODO(user): Because we remove literal on the first copy, maybe we can get
  // rid of the set here. However we still need to be careful when remapping
  // literals to their representatives.
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

// Note this function does not update the constraint graph. It assumes this is
// done elsewhere.
ABSL_MUST_USE_RESULT bool CpConstraintPresolver::MarkConstraintAsFalse(
    ConstraintProto* ct, std::string_view reason) {
  if (!context_->MarkConstraintAsFalse(ct, reason)) return false;
  if (ct->constraint_case() == ConstraintProto::kBoolOr) {
    PresolveBoolOr(ct);
    return !context_->ModelIsUnsat();
  }
  return true;
}

ABSL_MUST_USE_RESULT bool CpConstraintPresolver::MarkOptionalIntervalAsFalse(
    ConstraintProto* ct) {
  DCHECK_EQ(ct->constraint_case(), ConstraintProto::kInterval);
  CHECK_EQ(ct->enforcement_literal_size(), 1);
  const int enforcement_literal = ct->enforcement_literal(0);
  if (!context_->SetLiteralToFalse(enforcement_literal)) {
    return false;
  }
  // Now that we forced the interval to be unperformed we know it will be
  // ignored no matter what it contains as start/end/size, so we can make it
  // trivial. But we cannot remove the interval constraint itself though,
  // because it may be referenced in some no_overlap/no_overlap_2d constraints.
  ct->mutable_interval()->clear_start();
  ct->mutable_interval()->clear_end();
  ct->mutable_interval()->clear_size();
  return true;
}

bool CpConstraintPresolver::PresolveBoolAnd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (!HasEnforcementLiteral(*ct)) {
    context_->UpdateRuleStats("bool_and: non-reified");
    for (const int literal : ct->bool_and().literals()) {
      if (!context_->SetLiteralToTrue(literal)) return true;
    }
    return RemoveConstraint(ct);
  }

  bool changed = false;
  context_->tmp_literals.clear();
  context_->tmp_literal_set.clear();
  const absl::flat_hash_set<int> enforcement_literals_set(
      ct->enforcement_literal().begin(), ct->enforcement_literal().end());
  for (const int literal : ct->bool_and().literals()) {
    if (context_->LiteralIsFalse(literal)) {
      return MarkConstraintAsFalse(ct, "bool_and: always false");
    }
    if (context_->LiteralIsTrue(literal)) {
      changed = true;
      continue;
    }
    if (enforcement_literals_set.contains(literal)) {
      context_->UpdateRuleStats("bool_and: x => x");
      changed = true;
      continue;
    }
    if (enforcement_literals_set.contains(NegatedRef(literal))) {
      return MarkConstraintAsFalse(ct, "bool_and: x => not x");
    }
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      // This is a "dual" reduction.
      changed = true;
      context_->UpdateRuleStats(
          "bool_and: setting unused literal in rhs to true");
      if (!context_->SetLiteralToTrue(literal)) return true;
      continue;
    }

    if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
      return MarkConstraintAsFalse(ct, "bool_and: cannot be enforced");
    }

    const auto [_, inserted] = context_->tmp_literal_set.insert(literal);
    if (inserted) {
      context_->tmp_literals.push_back(literal);
    } else {
      changed = true;
      context_->UpdateRuleStats("bool_and: removed duplicate literal");
    }
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
        context_->UpdateRuleStats("bool_and: dual equality");
        // Extending `ct` = "enforcement => implied_literal" to an equality can
        // break the hint only if hint(implied_literal) = 1 and
        // hint(enforcement) = 0. But in this case the `enforcement` hint can be
        // increased to 1 to preserve the hint feasibility.
        const int implied_literal = ct->bool_and().literals(0);
        solution_crush_.SetLiteralToValueIf(enforcement, true, implied_literal);
        if (!context_->StoreBooleanEqualityRelation(enforcement,
                                                    implied_literal)) {
          return false;
        }
      }
    }
  }

  return changed;
}

bool CpConstraintPresolver::PresolveAtMostOrExactlyOne(
    ConstraintProto* ct, bool use_dual_reduction) {
  bool is_at_most_one = ct->constraint_case() == ConstraintProto::kAtMostOne;
  const std::string name = is_at_most_one ? "at_most_one: " : "exactly_one: ";
  auto* literals = is_at_most_one
                       ? ct->mutable_at_most_one()->mutable_literals()
                       : ct->mutable_exactly_one()->mutable_literals();

  // Having a canonical constraint is needed for duplicate detection.
  // This also change how we regroup bool_and.
  std::sort(literals->begin(), literals->end());

  // Deal with duplicate variable reference.
  context_->tmp_literal_set.clear();
  for (const int literal : *literals) {
    const auto [_, inserted] = context_->tmp_literal_set.insert(literal);
    if (!inserted) {
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
  }

  // We can always remove all singleton variables (with or without cost) in an
  // at_most_one or exactly one. We collect them and deal with this at the end.
  std::vector<std::pair<int, int64_t>> singleton_literal_with_cost;

  // Remove fixed variables.
  bool changed = false;
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

    // A singleton variable with or without cost can be removed. See below.
    if (use_dual_reduction) {
      if (context_->VariableIsUniqueAndRemovable(literal)) {
        // A variable that doesn't appear in the objective can be seen as
        // appearing with a coefficient of zero.
        singleton_literal_with_cost.push_back({literal, 0});
        continue;
      }
      if (context_->VariableWithCostIsUniqueAndRemovable(literal)) {
        const auto it = context_->ObjectiveMap().find(PositiveRef(literal));
        DCHECK(it != context_->ObjectiveMap().end());
        if (RefIsPositive(literal)) {
          singleton_literal_with_cost.push_back({literal, it->second});
        } else {
          // Note that we actually just store the objective change if this
          // literal is true compared to it being false.
          singleton_literal_with_cost.push_back({literal, -it->second});
        }
        continue;
      }
    }

    context_->tmp_literals.push_back(literal);
  }

  bool transform_to_at_most_one = false;
  if (!singleton_literal_with_cost.empty()) {
    changed = true;

    // By domination argument, we can fix to false everything but the minimum.
    if (singleton_literal_with_cost.size() > 1) {
      absl::c_stable_sort(
          singleton_literal_with_cost,
          [](const std::pair<int, int64_t>& a,
             const std::pair<int, int64_t>& b) { return a.second < b.second; });
      for (int i = 1; i < singleton_literal_with_cost.size(); ++i) {
        context_->UpdateRuleStats("at_most_one: dominated singleton");
        if (!context_->SetLiteralToFalse(
                singleton_literal_with_cost[i].first)) {
          return false;
        }
      }
      singleton_literal_with_cost.resize(1);
    }

    const int literal = singleton_literal_with_cost[0].first;
    const int64_t literal_cost = singleton_literal_with_cost[0].second;
    if (is_at_most_one && literal_cost >= 0) {
      // We can just always set it to false in this case.
      context_->UpdateRuleStats("at_most_one: singleton");
      if (!context_->SetLiteralToFalse(literal)) return false;
    } else if (context_->ShiftCostInExactlyOne(*literals, literal_cost)) {
      // We can make the constraint an exactly one if needed since it is always
      // beneficial to set this literal to true if everything else is zero. Now
      // that we have an exactly one, we can transfer the cost to the other
      // terms. The objective of literal should become zero, and we can then
      // decide its value at postsolve and just have an at most one on the other
      // literals.
      DCHECK(!context_->ObjectiveMap().contains(PositiveRef(literal)));

      if (!is_at_most_one) transform_to_at_most_one = true;
      is_at_most_one = true;

      context_->UpdateRuleStats("exactly_one: singleton");
      context_->MarkVariableAsRemoved(PositiveRef(literal));

      // Put a constraint in the mapping proto for postsolve.
      auto* mapping_exo = context_->NewMappingConstraint(__FILE__, __LINE__)
                              ->mutable_exactly_one();
      for (const int lit : context_->tmp_literals) {
        mapping_exo->add_literals(lit);
      }
      mapping_exo->add_literals(literal);
    } else {
      // If ShiftCostInExactlyOne() failed, keep the literal in the amo.
      context_->tmp_literals.push_back(literal);
    }
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

bool CpConstraintPresolver::PresolveAtMostOne(ConstraintProto* ct,
                                              bool use_dual_reduction) {
  if (context_->ModelIsUnsat()) return false;

  CHECK(!HasEnforcementLiteral(*ct));
  const bool changed = PresolveAtMostOrExactlyOne(ct, use_dual_reduction);
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

bool CpConstraintPresolver::PresolveExactlyOne(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  CHECK(!HasEnforcementLiteral(*ct));
  const bool changed =
      PresolveAtMostOrExactlyOne(ct, /*use_dual_reduction=*/true);
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
    if (!context_->StoreBooleanEqualityRelation(literals[0],
                                                NegatedRef(literals[1]))) {
      return false;
    }
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpConstraintPresolver::CanonicalizeLinearArgument(
    const ConstraintProto& ct, LinearArgumentProto* proto) {
  if (context_->ModelIsUnsat()) return false;

  // Canonicalize all involved expression.
  bool changed = CanonicalizeLinearExpression(ct, proto->mutable_target());
  for (LinearExpressionProto& exp : *(proto->mutable_exprs())) {
    changed |= CanonicalizeLinearExpression(ct, &exp);
  }
  return changed;
}

// Deal with X = lin_max(exprs) where all exprs are divisible by gcd.
// X must be divisible also, and we can divide everything.
bool CpConstraintPresolver::DivideLinMaxByGcd(int c, ConstraintProto* ct) {
  LinearArgumentProto* lin_max = ct->mutable_lin_max();

  // Compute gcd of exprs first.
  int64_t gcd = 0;
  for (const LinearExpressionProto& expr : lin_max->exprs()) {
    gcd = LinearExpressionGcd(expr, gcd);
    if (gcd == 1) break;
  }
  if (gcd <= 1) return true;

  // TODO(user): deal with all UNSAT cases.
  // Also if the target is affine, we can canonicalize it.
  const LinearExpressionProto& target = lin_max->target();
  const int64_t old_gcd = gcd;
  gcd = LinearExpressionGcd(target, gcd);
  if (gcd != old_gcd) {
    if (target.vars().empty()) {
      return context_->NotifyThatModelIsUnsat("infeasible lin_max");
    }

    // If the target is affine, we can solve the diophantine equation and
    // express the target in term of a new variable.
    if (target.vars().size() == 1) {
      gcd = old_gcd;
      context_->UpdateRuleStats("lin_max: canonicalize target using gcd");
      if (!context_->CanonicalizeAffineVariable(
              target.vars(0), target.coeffs(0), gcd, -target.offset())) {
        return false;
      }
      CanonicalizeLinearExpression(*ct, lin_max->mutable_target());
      context_->UpdateConstraintVariableUsage(c);
      CHECK_EQ(LinearExpressionGcd(target, gcd), gcd);
    } else {
      context_->UpdateRuleStats(
          "TODO lin_max: lhs not trivially divisible by rhs gcd");
    }
  }
  if (gcd <= 1) return true;

  context_->UpdateRuleStats("lin_max: dividing by gcd");
  DivideLinearExpression(gcd, lin_max->mutable_target());
  for (LinearExpressionProto& expr : *lin_max->mutable_exprs()) {
    DivideLinearExpression(gcd, &expr);
  }
  return true;
}

namespace {

int64_t EvaluateSingleVariableExpression(const LinearExpressionProto& expr,
                                         int var, int64_t value) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars().size(); ++i) {
    CHECK_EQ(expr.vars(i), var);
    result += expr.coeffs(i) * value;
  }
  return result;
}

template <class ExpressionList>
int GetFirstVar(const ExpressionList& exprs) {
  for (const LinearExpressionProto& expr : exprs) {
    for (const int var : expr.vars()) {
      DCHECK(RefIsPositive(var));
      return var;
    }
  }
  return -1;
}

}  // namespace

bool CpConstraintPresolver::PropagateAndReduceAffineMax(ConstraintProto* ct) {
  // Get the unique variable appearing in the expressions.
  const int unique_var = GetFirstVar(ct->lin_max().exprs());

  const auto& lin_max = ct->lin_max();
  const int num_exprs = lin_max.exprs_size();
  const auto& target = lin_max.target();
  std::vector<int> num_wins(num_exprs, 0);
  std::vector<int64_t> reachable_target_values;
  std::vector<int64_t> valid_variable_values;
  std::vector<int64_t> tmp_values(num_exprs);

  const bool target_has_same_unique_var =
      target.vars_size() == 1 && target.vars(0) == unique_var;

  CHECK_LE(context_->DomainOf(unique_var).Size(), 1000);

  for (const int64_t value : context_->DomainOf(unique_var).Values()) {
    int64_t current_max = kint64min;

    // Fill tmp_values and compute current_max;
    for (int i = 0; i < num_exprs; ++i) {
      const int64_t v =
          EvaluateSingleVariableExpression(lin_max.exprs(i), unique_var, value);
      current_max = std::max(current_max, v);
      tmp_values[i] = v;
    }

    // Check if any expr produced a value compatible with the target.
    if (!context_->DomainContains(target, current_max)) continue;

    // Special case: affine(x) == max(exprs(x)). We can check if the affine()
    // and the max(exprs) are compatible.
    if (target_has_same_unique_var &&
        EvaluateSingleVariableExpression(target, unique_var, value) !=
            current_max) {
      continue;
    }

    valid_variable_values.push_back(value);
    reachable_target_values.push_back(current_max);
    for (int i = 0; i < num_exprs; ++i) {
      DCHECK_LE(tmp_values[i], current_max);
      if (tmp_values[i] == current_max) {
        num_wins[i]++;
      }
    }
  }

  if (reachable_target_values.empty() || valid_variable_values.empty()) {
    return MarkConstraintAsFalse(ct,
                                 "lin_max: infeasible affine_max constraint");
  }

  {
    bool reduced = false;
    if (!context_->IntersectDomainWith(
            target, Domain::FromValues(reachable_target_values), &reduced)) {
      return true;
    }
    if (reduced) {
      context_->UpdateRuleStats("lin_max: affine_max target domain reduced");
    }
  }

  {
    bool reduced = false;
    if (!context_->IntersectDomainWith(
            unique_var, Domain::FromValues(valid_variable_values), &reduced)) {
      return true;
    }
    if (reduced) {
      context_->UpdateRuleStats(
          "lin_max: unique affine_max var domain reduced");
    }
  }

  // If one expression always wins, even tied, we can eliminate all the others.
  for (int i = 0; i < num_exprs; ++i) {
    if (num_wins[i] == valid_variable_values.size()) {
      const LinearExpressionProto winner_expr = lin_max.exprs(i);
      ct->mutable_lin_max()->clear_exprs();
      *ct->mutable_lin_max()->add_exprs() = winner_expr;
      break;
    }
  }

  bool changed = false;
  if (ct->lin_max().exprs_size() > 1) {
    int new_size = 0;
    for (int i = 0; i < num_exprs; ++i) {
      if (num_wins[i] == 0) continue;
      *ct->mutable_lin_max()->mutable_exprs(new_size) = ct->lin_max().exprs(i);
      new_size++;
    }
    if (new_size < ct->lin_max().exprs_size()) {
      context_->UpdateRuleStats("lin_max: removed affine_max exprs");
      google::protobuf::util::Truncate(ct->mutable_lin_max()->mutable_exprs(),
                                       new_size);
      changed = true;
    }
  }

  if (context_->IsFixed(target)) {
    context_->UpdateRuleStats("lin_max: fixed affine_max target");
    return RemoveConstraint(ct);
  }

  if (target_has_same_unique_var) {
    context_->UpdateRuleStats("lin_max: target_affine(x) = max(affine_i(x))");
    return RemoveConstraint(ct);
  }

  // Remove the affine_max constraint if the target is removable and if domains
  // have been propagated without loss. For now, we know that there is no loss
  // if the target is a single ref. Since all the expressions are affine, in
  // this case we are fine.
  if (ExpressionContainsSingleRef(target) &&
      context_->VariableIsUniqueAndRemovable(target.vars(0))) {
    context_->MarkVariableAsRemoved(target.vars(0));
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    context_->UpdateRuleStats("lin_max: unused affine_max target");
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpConstraintPresolver::PropagateAndReduceLinMax(ConstraintProto* ct) {
  const LinearExpressionProto& target = ct->lin_max().target();

  // Compute the infered min/max of the target.
  // Update target domain (if it is not a complex expression).
  {
    int64_t infered_min = context_->MinOf(target);
    int64_t infered_max = kint64min;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      infered_min = std::max(infered_min, context_->MinOf(expr));
      infered_max = std::max(infered_max, context_->MaxOf(expr));
    }

    if (target.vars().empty()) {
      if (!Domain(infered_min, infered_max).Contains(target.offset())) {
        return MarkConstraintAsFalse(ct, "lin_max: infeasible");
      }
    }
    if (target.vars().size() <= 1) {  // Affine
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
  bool changed = false;
  {
    // If one expression is >= target_min,
    // We can remove all the expression <= target min.
    //
    // Note that we must keep an expression >= target_min though, for corner
    // case like [2,3] = max([2], [0][3]);
    bool has_greater_or_equal_to_target_min = false;
    int64_t max_at_index_to_keep = kint64min;
    int index_to_keep = -1;
    for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
      const LinearExpressionProto& expr = ct->lin_max().exprs(i);
      if (context_->MinOf(expr) >= target_min) {
        const int64_t expr_max = context_->MaxOf(expr);
        if (expr_max > max_at_index_to_keep) {
          max_at_index_to_keep = expr_max;
          index_to_keep = i;
        }
        has_greater_or_equal_to_target_min = true;
      }
    }

    int new_size = 0;
    for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
      const LinearExpressionProto& expr = ct->lin_max().exprs(i);
      const int64_t expr_max = context_->MaxOf(expr);
      // TODO(user): Also remove expression whose domain is incompatible with
      // the target even if the bounds are like [2] and [0][3]?
      if (expr_max < target_min) continue;
      if (expr_max == target_min && has_greater_or_equal_to_target_min &&
          i != index_to_keep) {
        continue;
      }
      *ct->mutable_lin_max()->mutable_exprs(new_size) = expr;
      new_size++;
    }
    if (new_size < ct->lin_max().exprs_size()) {
      context_->UpdateRuleStats("lin_max: removed exprs");
      google::protobuf::util::Truncate(ct->mutable_lin_max()->mutable_exprs(),
                                       new_size);
      changed = true;
    }
  }

  return changed;
}

void CpConstraintPresolver::AddLinear2ToModel(const LinearExpression2& linear2,
                                              int64_t lb, int64_t ub) {
  auto* ct = context_->AddConstraint();
  auto* linear = ct->mutable_linear();
  linear->add_domain(lb);
  linear->add_domain(ub);
  const absl::Span<const IntegerVariable> vars = linear2.non_zero_vars();
  const absl::Span<const IntegerValue> coeffs = linear2.non_zero_coeffs();
  linear->mutable_vars()->Reserve(vars.size());
  linear->mutable_coeffs()->Reserve(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    linear->add_vars(GetPositiveOnlyIndex(vars[i]).value());
    linear->add_coeffs(VariableIsPositive(vars[i]) ? coeffs[i].value()
                                                   : -coeffs[i].value());
  }
  bool changed = false;
  (void)CanonicalizeLinear(ct, &changed);
  context_->UpdateRuleStats("linear2: added to model");
  known_model_linear2_.Add(linear2, lb, ub);
}

bool CpConstraintPresolver::PresolveLinMax(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;
  const LinearExpressionProto& target = ct->lin_max().target();

  // x = max(x, xi...) => forall i, x >= xi.
  for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
    if (LinearExpressionProtosAreEqual(expr, target)) {
      for (const LinearExpressionProto& e : ct->lin_max().exprs()) {
        if (LinearExpressionProtosAreEqual(e, target)) continue;
        LinearConstraintProto* prec =
            context_->AddConstraint()->mutable_linear();
        prec->add_domain(0);
        prec->add_domain(kint64max);
        AddLinearExpressionToLinearConstraint(target, 1, prec);
        AddLinearExpressionToLinearConstraint(e, -1, prec);
      }
      context_->UpdateRuleStats("lin_max: x = max(x, ...)");
      return RemoveConstraint(ct);
    }
  }

  const bool is_one_var_affine_max =
      ExpressionsContainsOnlyOneVar(ct->lin_max().exprs()) &&
      ct->lin_max().target().vars_size() <= 1;
  bool unique_var_is_small_enough = false;
  const bool is_int_abs = IsAffineIntAbs(*ct);

  if (is_one_var_affine_max) {
    const int unique_var = GetFirstVar(ct->lin_max().exprs());
    unique_var_is_small_enough = context_->DomainOf(unique_var).Size() <= 1000;
  }

  bool changed;
  if (is_one_var_affine_max && unique_var_is_small_enough) {
    changed = PropagateAndReduceAffineMax(ct);
  } else if (is_int_abs) {
    changed = PropagateAndReduceIntAbs(ct);
  } else {
    changed = PropagateAndReduceLinMax(ct);
  }

  if (context_->ModelIsUnsat()) return false;
  if (ct->constraint_case() != ConstraintProto::kLinMax) {
    // The constraint was removed by the propagate helpers.
    return changed;
  }

  if (ct->lin_max().exprs().empty()) {
    return MarkConstraintAsFalse(ct, "lin_max: no exprs");
  }

  // Try to reduce lin_max using known relation.
  if (ct->lin_max().exprs().size() < 10) {
    const int num_exprs = ct->lin_max().exprs().size();

    bool simplified = false;
    std::vector<bool> can_be_removed(num_exprs, false);
    for (int i = 0; i < num_exprs; ++i) {
      if (ct->lin_max().exprs(i).vars().size() != 1) continue;
      for (int j = 0; j < num_exprs; ++j) {
        if (i == j) continue;
        if (can_be_removed[j]) continue;

        // Note that we skip constant expressions as this should already be
        // handled when we compute the domain of each expression and remove
        // the ones that are smaller than the target.
        if (ct->lin_max().exprs(j).vars().size() != 1) continue;

        // Do we know if expr(i) <= expr(j) ?
        const LinearExpression2 expr2 = GetLinearExpression2FromProto(
            ct->lin_max().exprs(i).vars(0), ct->lin_max().exprs(i).coeffs(0),
            ct->lin_max().exprs(j).vars(0), -ct->lin_max().exprs(j).coeffs(0));
        const IntegerValue lb = kMinIntegerValue;
        const IntegerValue ub(ct->lin_max().exprs(j).offset() -
                              ct->lin_max().exprs(i).offset());
        const RelationStatus status = known_linear2_.GetStatus(expr2, lb, ub);
        if (status == RelationStatus::IS_TRUE) {
          if (known_model_linear2_.GetStatus(expr2, lb, ub) !=
              RelationStatus::IS_TRUE) {
            // Subtle: the linear2 might have been indirectly deduced using this
            // lin_max constraint. The linear2 could be encoded as a boolean
            // when loading the model on probing and then this boolean could
            // have been assigned using clauses that are coming from this
            // lin_max during propagation. But since we know that those bounds
            // are always true, adding it to the model is safe.
            AddLinear2ToModel(expr2, lb.value(), ub.value());
          }
          simplified = true;
          can_be_removed[i] = true;
          break;
        }
      }
    }

    if (simplified) {
      context_->UpdateRuleStats(
          "lin_max: removed expression smaller than others");
      int new_size = 0;
      for (int i = 0; i < num_exprs; ++i) {
        if (can_be_removed[i]) continue;
        *ct->mutable_lin_max()->mutable_exprs(new_size++) =
            ct->lin_max().exprs(i);
      }
      google::protobuf::util::Truncate(ct->mutable_lin_max()->mutable_exprs(),
                                       new_size);
      context_->UpdateConstraintVariableUsage(c);
    }
  }

  // If only one is left, we can convert to an equality. Note that we create a
  // new constraint otherwise it might not be processed again.
  if (ct->lin_max().exprs().size() == 1) {
    context_->UpdateRuleStats("lin_max: converted to equality");
    ConstraintProto* new_ct = context_->AddConstraint();
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
    return RemoveConstraint(ct);
  }

  if (!DivideLinMaxByGcd(c, ct)) return false;

  // Cut everything above the max if possible.
  // If one of the linear expression has many term and is above the max, we
  // abort early since none of the other rule can be applied.
  const int64_t target_min = context_->MinOf(target);
  const int64_t target_max = context_->MaxOf(target);
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
        context_->UpdateRuleStats("lin_max: reduced expression domain");
      }
      const int64_t value_max = context_->MaxOf(expr);
      if (value_max > target_max) {
        context_->UpdateRuleStats("TODO lin_max: linear expression above max");
        abort = true;
      }
    }
    if (abort) return changed;
  }

  // Checks if the affine target domain is constraining.
  bool linear_target_domain_contains_max_domain = false;
  if (ExpressionContainsSingleRef(target)) {  // target = +/- var.
    int64_t infered_min = kint64min;
    int64_t infered_max = kint64min;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      infered_min = std::max(infered_min, context_->MinOf(expr));
      infered_max = std::max(infered_max, context_->MaxOf(expr));
    }
    Domain rhs_domain;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      rhs_domain = rhs_domain.UnionWith(
          context_->DomainSuperSetOf(expr).IntersectionWith(
              {infered_min, infered_max}));
    }

    // Checks if all values from the max(exprs) belong in the domain of the
    // target.
    // Note that the target is +/-var.
    DCHECK_EQ(std::abs(target.coeffs(0)), 1);
    const Domain target_domain =
        target.coeffs(0) == 1 ? context_->DomainOf(target.vars(0))
                              : context_->DomainOf(target.vars(0)).Negation();
    linear_target_domain_contains_max_domain =
        rhs_domain.IsIncludedIn(target_domain);
  }

  // Avoid to remove the constraint for special cases:
  // affine(x) = max(expr(x, ...), ...);
  //
  // TODO(user): We could presolve this, but there are a few types of cases.
  // for example:
  // - x = max(x + 3, ...) : infeasible.
  // - x = max(x - 2, ...) : reduce arity: x = max(...)
  // - x = max(2x, ...) we have x <= 0
  // - etc...
  // Actually, I think for the expr=affine' case, it reduces to:
  // affine(x) >= affine'(x)
  // affine(x) = max(...);
  if (linear_target_domain_contains_max_domain) {
    const int target_var = target.vars(0);
    bool abort = false;
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      for (const int var : expr.vars()) {
        if (var == target_var &&
            !LinearExpressionProtosAreEqual(expr, target)) {
          abort = true;
          break;
        }
      }
      if (abort) break;
    }
    if (abort) {
      // Actually the expression can be more than affine.
      // We only know that the target is affine here.
      context_->UpdateRuleStats(
          "TODO lin_max: affine(x) = max(affine'(x), ...) !!");
      linear_target_domain_contains_max_domain = false;
    }
  }

  // If the target is not used, and safe, we can remove the constraint.
  if (linear_target_domain_contains_max_domain &&
      context_->VariableIsUniqueAndRemovable(target.vars(0))) {
    context_->UpdateRuleStats("lin_max: unused affine target");
    context_->MarkVariableAsRemoved(target.vars(0));
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    return RemoveConstraint(ct);
  }

  // If the target is only used in the objective, and safe, we can simplify the
  // constraint.
  if (linear_target_domain_contains_max_domain &&
      context_->VariableWithCostIsUniqueAndRemovable(target.vars(0)) &&
      (target.coeffs(0) > 0) ==
          (context_->ObjectiveCoeff(target.vars(0)) > 0)) {
    context_->UpdateRuleStats("lin_max: rewrite with precedences");
    for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
      LinearConstraintProto* prec = context_->AddConstraint()->mutable_linear();
      prec->add_domain(0);
      prec->add_domain(kint64max);
      AddLinearExpressionToLinearConstraint(target, 1, prec);
      AddLinearExpressionToLinearConstraint(expr, -1, prec);
    }
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    return RemoveConstraint(ct);
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
        return MarkConstraintAsFalse(ct, "lin_max: all boolean and no support");
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

  changed |= PresolveLinMaxWhenAllBoolean(ct);
  return changed;
}

// If everything is Boolean and affine, do not use a lin max!
bool CpConstraintPresolver::PresolveLinMaxWhenAllBoolean(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  const LinearExpressionProto& target = ct->lin_max().target();
  if (!context_->ExpressionIsAffineBoolean(target)) return false;

  const int64_t target_min = context_->MinOf(target);
  const int64_t target_max = context_->MaxOf(target);
  const int target_ref = context_->LiteralForExpressionMax(target);

  bool min_is_reachable = false;
  std::vector<int> min_literals;
  std::vector<int> literals_above_min;
  std::vector<int> max_literals;

  for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
    if (!context_->ExpressionIsAffineBoolean(expr)) return false;
    const int64_t value_min = context_->MinOf(expr);
    const int64_t value_max = context_->MaxOf(expr);
    const int ref = context_->LiteralForExpressionMax(expr);

    // Get corner case out of the way, and wait for the constraint to be
    // processed again in this case.
    if (value_min > target_min) {
      context_->UpdateRuleStats("lin_max: fix target");
      (void)context_->SetLiteralToTrue(target_ref);
      return false;
    }
    if (value_max > target_max) {
      context_->UpdateRuleStats("lin_max: fix bool expr");
      (void)context_->SetLiteralToFalse(ref);
      return false;
    }

    // expr is fixed.
    if (value_min == value_max) {
      if (value_min == target_min) min_is_reachable = true;
      continue;
    }

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

  context_->UpdateRuleStats("lin_max: all booleans");

  // target_ref => at_least_one(max_literals);
  ConstraintProto* clause = context_->AddConstraint();
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
    ConstraintProto* clause = context_->AddConstraint();
    clause->add_enforcement_literal(NegatedRef(target_ref));
    clause->mutable_bool_or();
    for (const int lit : min_literals) {
      clause->mutable_bool_or()->add_literals(lit);
    }
  }

  return RemoveConstraint(ct);
}

// This presolve expect that the constraint only contains 1-var affine
// expressions.
bool CpConstraintPresolver::PropagateAndReduceIntAbs(ConstraintProto* ct) {
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
            .IntersectionWith({0, kint64max});
    bool target_domain_modified = false;
    if (!context_->IntersectDomainWith(target_expr, new_target_domain,
                                       &target_domain_modified)) {
      return false;
    }
    if (expr_domain.IsFixed()) {
      context_->UpdateRuleStats("lin_max: fixed expression in int_abs");
      return RemoveConstraint(ct);
    }
    if (target_domain_modified) {
      context_->UpdateRuleStats("lin_max: propagate domain from x to abs(x)");
    }
  }

  // Propagate from target domain to variable.
  {
    const Domain target_domain = context_->DomainSuperSetOf(target_expr)
                                     .IntersectionWith(Domain(0, kint64max));
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
      context_->UpdateRuleStats("lin_max: fixed abs target");
      return RemoveConstraint(ct);
    }
    if (expr_domain_modified) {
      context_->UpdateRuleStats("lin_max: propagate domain from abs(x) to x");
    }
  }

  // Convert to equality if the sign of expr is fixed.
  if (context_->MinOf(expr) >= 0) {
    context_->UpdateRuleStats("lin_max: converted abs to equality");
    ConstraintProto* new_ct = context_->AddConstraint();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, -1, arg);
    bool changed = false;
    if (!CanonicalizeLinear(new_ct, &changed)) {
      return true;
    }
    return RemoveConstraint(ct);
  }

  if (context_->MaxOf(expr) <= 0) {
    context_->UpdateRuleStats("lin_max: converted abs to equality");
    ConstraintProto* new_ct = context_->AddConstraint();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, 1, arg);
    bool changed = false;
    if (!CanonicalizeLinear(new_ct, &changed)) {
      return true;
    }
    return RemoveConstraint(ct);
  }

  // Remove the abs constraint if the target is removable and if domains have
  // been propagated without loss.
  // For now, we know that there is no loss if the target is a single ref.
  // Since all the expressions are affine, in this case we are fine.
  if (ExpressionContainsSingleRef(target_expr) &&
      context_->VariableIsUniqueAndRemovable(target_expr.vars(0))) {
    context_->MarkVariableAsRemoved(target_expr.vars(0));
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    context_->UpdateRuleStats("lin_max: unused abs target");
    return RemoveConstraint(ct);
  }

  return false;
}

Domain EvaluateImpliedIntProdDomain(const LinearArgumentProto& expr,
                                    const PresolveContext& context) {
  if (expr.exprs().size() == 2) {
    const LinearExpressionProto& expr0 = expr.exprs(0);
    const LinearExpressionProto& expr1 = expr.exprs(1);
    if (LinearExpressionProtosAreEqual(expr0, expr1)) {
      return context.DomainSuperSetOf(expr0).SquareSuperset();
    }
    if (expr0.vars().size() == 1 && expr1.vars().size() == 1 &&
        expr0.vars(0) == expr1.vars(0)) {
      return context.DomainOf(expr0.vars(0))
          .QuadraticSuperset(expr0.coeffs(0), expr0.offset(), expr1.coeffs(0),
                             expr1.offset());
    }
  }

  Domain implied(1);
  for (const LinearExpressionProto& expr : expr.exprs()) {
    implied =
        implied.ContinuousMultiplicationBy(context.DomainSuperSetOf(expr));
  }
  return implied;
}

bool CpConstraintPresolver::PresolveIntProd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // Start by restricting the domain of target. We will be more precise later.
  bool domain_modified = false;
  Domain implied_domain =
      EvaluateImpliedIntProdDomain(ct->int_prod(), *context_);
  // TODO(user): if implied_domain and target domain are disjoint, mark the
  // constraint as false.
  if (!HasEnforcementLiteral(*ct) &&
      !context_->IntersectDomainWith(ct->int_prod().target(), implied_domain,
                                     &domain_modified)) {
    return false;
  }

  // Remove a constraint if the target only appears in the constraint. For this
  // to be correct some conditions must be met:
  // - The target is an affine linear with coefficient -1 or 1.
  // - The target does not appear in the rhs (no x = (a*x + b) * ...).
  // - The target domain covers all the possible range of the rhs.
  // This can be done whether or not there are enforcement literals, even if
  // they are used in the target or the rhs.
  // TODO(user): support enforced int_prod in the postsolve.
  if (!HasEnforcementLiteral(*ct) &&
      ExpressionContainsSingleRef(ct->int_prod().target()) &&
      context_->VariableIsUniqueAndRemovable(ct->int_prod().target().vars(0)) &&
      std::abs(ct->int_prod().target().coeffs(0)) == 1) {
    const LinearExpressionProto& target = ct->int_prod().target();
    if (!absl::c_any_of(ct->int_prod().exprs(),
                        [&target](const LinearExpressionProto& expr) {
                          return absl::c_linear_search(expr.vars(),
                                                       target.vars(0));
                        })) {
      const Domain target_domain =
          Domain(target.offset())
              .AdditionWith(context_->DomainOf(target.vars(0)));
      if (implied_domain.IsIncludedIn(target_domain)) {
        context_->MarkVariableAsRemoved(ct->int_prod().target().vars(0));
        context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
        context_->UpdateRuleStats("int_prod: unused affine target");
        return RemoveConstraint(ct);
      }
    }
  }

  // Remove constant expressions and compute the product of the max positive
  // divisor of each term.
  int64_t constant_factor = 1;
  int new_size = 0;
  bool changed = false;
  LinearArgumentProto old_proto = ct->int_prod();
  LinearArgumentProto* proto = ct->mutable_int_prod();
  for (int i = 0; i < ct->int_prod().exprs().size(); ++i) {
    LinearExpressionProto expr = ct->int_prod().exprs(i);
    if (context_->IsFixed(expr)) {
      const int64_t expr_value = context_->FixedValue(expr);
      constant_factor = CapProd(constant_factor, expr_value);
      context_->UpdateRuleStats("int_prod: removed constant expressions");
      changed = true;
    } else {
      const int64_t expr_divisor = LinearExpressionGcd(expr);
      DivideLinearExpression(expr_divisor, &expr);
      constant_factor = CapProd(constant_factor, expr_divisor);
      *proto->mutable_exprs(new_size++) = expr;
    }
  }
  proto->mutable_exprs()->erase(proto->mutable_exprs()->begin() + new_size,
                                proto->mutable_exprs()->end());

  if (ct->int_prod().exprs().empty() || constant_factor == 0) {
    if (!context_->DomainContains(ct->int_prod().target(), constant_factor)) {
      return MarkConstraintAsFalse(ct, "int_prod: always false");
    }
    if (!HasEnforcementLiteral(*ct)) {
      if (!context_->IntersectDomainWith(ct->int_prod().target(),
                                         Domain(constant_factor))) {
        return false;
      }
      context_->UpdateRuleStats("int_prod: constant product");
    } else {
      // Replace ct with an enforced linear "target == constant_factor".
      ConstraintProto* new_ct = context_->AddConstraint();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      LinearConstraintProto* const lin = new_ct->mutable_linear();
      lin->add_domain(constant_factor);
      lin->add_domain(constant_factor);
      AddLinearExpressionToLinearConstraint(ct->int_prod().target(), 1, lin);
      context_->UpdateRuleStats("enforced int_prod: constant product");
    }
    return RemoveConstraint(ct);
  }

  // If target is fixed to zero, we can forget the constant factor.
  if (context_->IsFixed(ct->int_prod().target()) &&
      context_->FixedValue(ct->int_prod().target()) == 0 &&
      constant_factor != 1) {
    context_->UpdateRuleStats("int_prod: simplify by constant factor");
    constant_factor = 1;
  }

  // In this case, the only possible value that fits in the domains is zero.
  // We will check for UNSAT if zero is not achievable by the rhs below.
  if (!HasEnforcementLiteral(*ct) && AtMinOrMaxInt64(constant_factor)) {
    context_->UpdateRuleStats("int_prod: overflow if non zero");
    if (!context_->IntersectDomainWith(ct->int_prod().target(), Domain(0))) {
      return false;
    }
    constant_factor = 1;
  }

  // Replace with linear if it cannot overflow.
  if (ct->int_prod().exprs().size() == 1) {
    if (context_->IsFixed(ct->int_prod().target())) {
      const int64_t target_value =
          context_->FixedValue(ct->int_prod().target());
      if (target_value % constant_factor != 0) {
        return MarkConstraintAsFalse(
            ct, "int_prod: product incompatible with fixed target");
      }

      // expression == target_value / constant_factor.
      ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
      LinearConstraintProto* const lin = new_ct->mutable_linear();
      lin->add_domain(target_value / constant_factor);
      lin->add_domain(target_value / constant_factor);
      AddLinearExpressionToLinearConstraint(ct->int_prod().exprs(0), 1, lin);
      context_->UpdateRuleStats("int_prod: expression is constant");
      return RemoveConstraint(ct);
    }

    const int64_t target_divisor = LinearExpressionGcd(ct->int_prod().target());

    // Reduce coefficients.
    const int64_t gcd =
        std::gcd(static_cast<uint64_t>(std::abs(constant_factor)),
                 static_cast<uint64_t>(std::abs(target_divisor)));
    if (gcd != 1) {
      constant_factor /= gcd;
      DivideLinearExpression(gcd, ct->mutable_int_prod()->mutable_target());
    }

    // expression * constant_factor = target.
    ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
    LinearConstraintProto* const lin = new_ct->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(0);
    const bool overflow = !SafeAddLinearExpressionToLinearConstraint(
                              ct->int_prod().target(), 1, lin) ||
                          !SafeAddLinearExpressionToLinearConstraint(
                              ct->int_prod().exprs(0), -constant_factor, lin);

    // Check for overflow.
    if (overflow ||
        PossibleIntegerOverflow(context_->WorkingModel(), lin->vars(),
                                lin->coeffs(), lin->domain(0))) {
      // The constant factor will be handled by the creation of an affine
      // relation below.
      context_->RemoveLastConstraint();
    } else {  // Replace with a linear equation.
      context_->UpdateRuleStats("int_prod: linearize product by constant");
      return RemoveConstraint(ct);
    }
  }

  if (constant_factor != 1) {
    // Lets canonicalize the target by introducing a new variable if necessary.
    //
    // coeff * X + offset must be a multiple of constant_factor, so
    // we can rewrite X so that this property is clear.
    //
    // Note(user): it is important for this to have a restricted target domain
    // so we can choose a better representative.
    const LinearExpressionProto old_target = ct->int_prod().target();
    if (!context_->IsFixed(old_target)) {
      // The call to CanonicalizeAffineVariable() creates an always enforced
      // affine relation or makes the model UNSAT. Both cases are invalid if
      // there are enforcement literals.
      if (HasEnforcementLiteral(*ct) ||
          CapProd(constant_factor, std::max(context_->MaxOf(old_target),
                                            -context_->MinOf(old_target))) >=
              kint64max / 2) {
        // Restore the original constraint (we cannot add back a new term for
        // the constant factor: this may create a constraint with more than 2
        // terms).
        *ct->mutable_int_prod() = old_proto;
        context_->UpdateRuleStats(
            "int_prod: enforcement or overflow prevented creating an affine "
            "relation");
        return true;
      }
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
        return MarkConstraintAsFalse(
            ct, "int_prod: constant factor does not divide constant target");
      }
      changed = true;
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
      if (new_coeff > absl::int128(kint64max) ||
          new_coeff < absl::int128(kint64min) ||
          new_offset > absl::int128(kint64max) ||
          new_offset < absl::int128(kint64min)) {
        return MarkConstraintAsFalse(
            ct, "int_prod: overflow during simplification");
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
  implied_domain = EvaluateImpliedIntProdDomain(ct->int_prod(), *context_);
  const bool is_square = ct->int_prod().exprs_size() == 2 &&
                         LinearExpressionProtosAreEqual(
                             ct->int_prod().exprs(0), ct->int_prod().exprs(1));
  if (!HasEnforcementLiteral(*ct) &&
      !context_->IntersectDomainWith(ct->int_prod().target(), implied_domain,
                                     &domain_modified)) {
    return false;
  }
  if (domain_modified) {
    context_->UpdateRuleStats(absl::StrCat(
        is_square ? "int_square" : "int_prod", ": reduced target domain"));
  }

  // y = x * x, we can reduce the domain of x from the domain of y.
  if (is_square && !HasEnforcementLiteral(*ct)) {
    const int64_t target_max = context_->MaxOf(ct->int_prod().target());
    DCHECK_GE(target_max, 0);
    const int64_t sqrt_max = FloorSquareRoot(target_max);
    bool expr_reduced = false;
    if (!context_->IntersectDomainWith(ct->int_prod().exprs(0),
                                       {-sqrt_max, sqrt_max}, &expr_reduced)) {
      return false;
    }
    if (expr_reduced) {
      context_->UpdateRuleStats("int_square: reduced expr domain");
    }
  }

  if (ct->int_prod().exprs_size() == 2) {
    LinearExpressionProto a = ct->int_prod().exprs(0);
    LinearExpressionProto b = ct->int_prod().exprs(1);
    const LinearExpressionProto product = ct->int_prod().target();
    if (LinearExpressionProtosAreEqual(a, b) &&
        LinearExpressionProtosAreEqual(
            a, product)) {  // x = x * x, only true for {0, 1}.
      if (!HasEnforcementLiteral(*ct)) {
        if (!context_->IntersectDomainWith(product, Domain(0, 1))) {
          return false;
        }
        context_->UpdateRuleStats("int_square: fix variable to zero or one");
        return RemoveConstraint(ct);
      } else {
        context_->UpdateRuleStats(
            "TODO enforced int_square: fix variable to zero or one");
        // Replace ct with an enforced linear "product in [0, 1]".
      }
    }
  }

  if (ct->int_prod().exprs().size() == 2) {
    const auto is_boolean_affine =
        [context = context_](const LinearExpressionProto& expr) {
          return expr.vars().size() == 1 && context->MinOf(expr.vars(0)) == 0 &&
                 context->MaxOf(expr.vars(0)) == 1;
        };
    const LinearExpressionProto* boolean_linear = nullptr;
    const LinearExpressionProto* other_linear = nullptr;
    if (is_boolean_affine(ct->int_prod().exprs(0))) {
      boolean_linear = &ct->int_prod().exprs(0);
      other_linear = &ct->int_prod().exprs(1);
    } else if (is_boolean_affine(ct->int_prod().exprs(1))) {
      boolean_linear = &ct->int_prod().exprs(1);
      other_linear = &ct->int_prod().exprs(0);
    }
    if (boolean_linear) {
      // We have:
      // (u + b * v) * other_expr = B, where `b` is a boolean variable.
      //
      // We can rewrite this as:
      //   u * other_expr = B, if b = false;
      //   (u + v) * other_expr = B, if b = true
      ConstraintProto* ct_for_false = context_->AddEnforcedConstraint(ct);
      ConstraintProto* ct_for_true = context_->AddEnforcedConstraint(ct);

      const int selector = boolean_linear->vars(0);
      ct_for_true->add_enforcement_literal(selector);
      ct_for_false->add_enforcement_literal(NegatedRef(selector));

      LinearConstraintProto* linear_for_false = ct_for_false->mutable_linear();
      linear_for_false->add_domain(0);
      linear_for_false->add_domain(0);
      AddLinearExpressionToLinearConstraint(
          *other_linear, boolean_linear->offset(), linear_for_false);
      AddLinearExpressionToLinearConstraint(ct->int_prod().target(), -1,
                                            linear_for_false);

      LinearConstraintProto* linear_for_true = ct_for_true->mutable_linear();
      linear_for_true->add_domain(0);
      linear_for_true->add_domain(0);
      AddLinearExpressionToLinearConstraint(
          *other_linear, boolean_linear->offset() + boolean_linear->coeffs(0),
          linear_for_true);
      AddLinearExpressionToLinearConstraint(ct->int_prod().target(), -1,
                                            linear_for_true);

      context_->CanonicalizeLinearConstraint(ct_for_false);
      context_->CanonicalizeLinearConstraint(ct_for_true);

      if (PossibleIntegerOverflow(context_->WorkingModel(),
                                  linear_for_false->vars(),
                                  linear_for_false->coeffs()) ||
          PossibleIntegerOverflow(context_->WorkingModel(),
                                  linear_for_true->vars(),
                                  linear_for_true->coeffs())) {
        context_->RemoveLastConstraint();
        context_->RemoveLastConstraint();
        context_->UpdateRuleStats("TODO int_prod: boolean affine term");
      } else {
        context_->UpdateRuleStats("int_prod: boolean affine term");
        return RemoveConstraint(ct);
      }
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

  // This is a Boolean constraint!
  context_->UpdateRuleStats("int_prod: all boolean");
  {
    ConstraintProto* new_ct = context_->AddConstraint();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    new_ct->add_enforcement_literal(target);
    auto* arg = new_ct->mutable_bool_and();
    for (const int lit : literals) {
      arg->add_literals(lit);
    }
  }
  {
    ConstraintProto* new_ct = context_->AddConstraint();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    auto* arg = new_ct->mutable_bool_or();
    arg->add_literals(target);
    for (const int lit : literals) {
      arg->add_literals(NegatedRef(lit));
    }
  }
  return RemoveConstraint(ct);
}

bool CpConstraintPresolver::PresolveIntDiv(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  const LinearExpressionProto& target = ct->int_div().target();
  const LinearExpressionProto& expr = ct->int_div().exprs(0);
  const LinearExpressionProto& div = ct->int_div().exprs(1);

  if (LinearExpressionProtosAreEqual(expr, div)) {
    (void)context_->MarkConstraintAsEquivalentToLinear(ct, target, Domain(1),
                                                       "int_div: y = x / x");
    return true;
  } else if (LinearExpressionProtosAreEqual(expr, div, -1)) {
    (void)context_->MarkConstraintAsEquivalentToLinear(ct, target, Domain(-1),
                                                       "int_div: y = - x / x");
    return true;
  }

  // Sometimes we have only a single variable appearing in the whole constraint.
  // If the domain is small enough, we can just restrict the domain and remove
  // the constraint.
  if (context_->ConstraintToVars(c).size() == 1) {
    const int var = context_->ConstraintToVars(c)[0];
    if (context_->DomainOf(var).Size() >= 100) {
      context_->UpdateRuleStats(
          "TODO int_div: single variable with large domain");
    } else {
      std::vector<int64_t> possible_values;
      for (const int64_t v : context_->DomainOf(var).Values()) {
        const int64_t target_v =
            EvaluateSingleVariableExpression(target, var, v);
        const int64_t expr_v = EvaluateSingleVariableExpression(expr, var, v);
        const int64_t div_v = EvaluateSingleVariableExpression(div, var, v);
        if (div_v == 0) continue;
        if (target_v == expr_v / div_v) {
          possible_values.push_back(v);
        }
      }
      LinearExpressionProto var_expr;
      var_expr.add_vars(var);
      var_expr.add_coeffs(1);
      (void)context_->MarkConstraintAsEquivalentToLinear(
          ct, var_expr, Domain::FromValues(possible_values),
          "int_div: single variable");
      return true;
    }
  }

  // For now, we only presolve the case where the divisor is constant.
  if (!context_->IsFixed(div)) return false;

  const int64_t divisor = context_->FixedValue(div);

  // Trivial case one: target = expr / +/-1.
  if (divisor == 1 || divisor == -1) {
    LinearConstraintProto* const lin =
        context_->AddEnforcedConstraint(ct)->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(0);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(target, -divisor, lin);
    context_->UpdateRuleStats("int_div: rewrite to equality");
    return RemoveConstraint(ct);
  }

  // Reduce the domain of target.
  if (ct->enforcement_literal().empty()) {
    bool domain_modified = false;
    const Domain target_implied_domain =
        context_->DomainSuperSetOf(expr).DivisionBy(divisor);

    if (!context_->IntersectDomainWith(target, target_implied_domain,
                                       &domain_modified)) {
      return false;
    }
    if (domain_modified) {
      // Note: the case target is fixed has been processed before.
      if (target_implied_domain.IsFixed()) {
        context_->UpdateRuleStats(
            "int_div: target has been fixed by propagating X / cte");
      } else {
        context_->UpdateRuleStats(
            "int_div: updated domain of target in target = X / cte");
      }
    }
  }

  // Trivial case three: fixed_target = expr / fixed_divisor.
  if (context_->IsFixed(target) &&
      CapAdd(1, CapProd(std::abs(divisor), 1 + std::abs(context_->FixedValue(
                                                   target)))) != kint64max) {
    int64_t t = context_->FixedValue(target);
    int64_t d = divisor;
    if (d < 0) {
      t = -t;
      d = -d;
    }

    const Domain expr_implied_domain =
        t > 0
            ? Domain(t * d, (t + 1) * d - 1)
            : (t == 0 ? Domain(1 - d, d - 1) : Domain((t - 1) * d + 1, t * d));
    (void)context_->MarkConstraintAsEquivalentToLinear(
        ct, expr, expr_implied_domain, "int_div: target and divisor are fixed");
    return true;
  }

  // Linearize if everything is positive, and we have no overflow.
  // TODO(user): Deal with other cases where there is no change of
  // sign. We can also deal with target = cte, div variable.
  if (context_->MinOf(target) >= 0 && context_->MinOf(expr) >= 0 &&
      divisor > 1 && CapProd(divisor, context_->MaxOf(target)) != kint64max) {
    LinearConstraintProto* const lin =
        context_->AddEnforcedConstraint(ct)->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(divisor - 1);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(target, -divisor, lin);
    context_->UpdateRuleStats(
        "int_div: linearize positive division with a constant divisor");

    return RemoveConstraint(ct);
  }

  // TODO(user): reduce the domain of X by introducing an
  // InverseDivisionOfSortedDisjointIntervals().
  return false;
}

bool CpConstraintPresolver::PresolveIntMod(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // TODO(user): Presolve f(X) = g(X) % fixed_mod.
  const LinearExpressionProto target = ct->int_mod().target();
  const LinearExpressionProto expr = ct->int_mod().exprs(0);
  const LinearExpressionProto mod = ct->int_mod().exprs(1);

  if (context_->IsFixed(target) && context_->IsFixed(mod) &&
      context_->IsFixed(expr)) {
    if (context_->FixedValue(expr) % context_->FixedValue(mod) ==
        context_->FixedValue(target)) {
      context_->UpdateRuleStats("int_mod: fixed, always true");
      return RemoveConstraint(ct);
    } else {
      return MarkConstraintAsFalse(ct, "int_mod: fixed, always false");
    }
  }

  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;

  if (context_->MinOf(target) > 0) {
    bool domain_changed = false;
    if (!context_->IntersectDomainWith(expr, Domain(0, kint64max),
                                       &domain_changed)) {
      return false;
    }
    if (domain_changed) {
      context_->UpdateRuleStats(
          "int_mod: non negative target implies positive expression");
    }
  }

  if (context_->MinOf(target) >= context_->MaxOf(mod) ||
      context_->MaxOf(target) <= -context_->MaxOf(mod)) {
    return context_->NotifyThatModelIsUnsat(
        "int_mod: incompatible target and mod");
  }

  if (context_->MaxOf(target) < 0) {
    bool domain_changed = false;
    if (!context_->IntersectDomainWith(expr, Domain(kint64min, 0),
                                       &domain_changed)) {
      return false;
    }
    if (domain_changed) {
      context_->UpdateRuleStats(
          "int_mod: non positive target implies negative expression");
    }
  }

  if (context_->IsFixed(target) && context_->IsFixed(mod) &&
      context_->FixedValue(mod) > 1 && ct->enforcement_literal().empty() &&
      expr.vars().size() == 1) {
    // We can intersect the domain of expr with {k * mod + target}.
    const int64_t fixed_mod = context_->FixedValue(mod);
    const int64_t fixed_target = context_->FixedValue(target);

    if (fixed_target >= fixed_mod || fixed_target <= -fixed_mod) {
      return context_->NotifyThatModelIsUnsat(
          "int_mod: target absolute value is larger than divisor");
    }
    if (!context_->CanonicalizeAffineVariable(expr.vars(0), expr.coeffs(0),
                                              fixed_mod,
                                              fixed_target - expr.offset())) {
      return false;
    }

    context_->UpdateRuleStats("int_mod: fixed mod and target");
    return RemoveConstraint(ct);
  }

  bool domain_changed = false;
  if (!context_->IntersectDomainWith(
          target,
          context_->DomainSuperSetOf(expr).PositiveModuloBySuperset(
              context_->DomainSuperSetOf(mod)),
          &domain_changed)) {
    return false;
  }

  if (domain_changed) {
    context_->UpdateRuleStats("int_mod: reduce target domain");
  }

  // Remove the constraint if the target is removable.
  // This is triggered on the flatzinc rotating-workforce problems.
  //
  // TODO(user): We can deal with more cases, sometime even if the domain of
  // expr.vars(0) is large, the implied domain is not too complex.
  if (target.vars().size() == 1 && expr.vars().size() == 1 &&
      context_->DomainOf(expr.vars(0)).Size() < 100 && context_->IsFixed(mod) &&
      context_->VariableIsUniqueAndRemovable(target.vars(0)) &&
      target.vars(0) != expr.vars(0) &&
      // Note: the fringe case where both the target and the expression are
      // not used elsewhere confuses the postsolve.
      !context_->VariableIsUniqueAndRemovable(expr.vars(0))) {
    const int64_t fixed_mod = context_->FixedValue(mod);
    std::vector<int64_t> values;
    const Domain dom = context_->DomainOf(target.vars(0));
    for (const int64_t v : context_->DomainOf(expr.vars(0)).Values()) {
      const int64_t rhs = (v * expr.coeffs(0) + expr.offset()) % fixed_mod;
      const int64_t target_term = rhs - target.offset();
      if (target_term % target.coeffs(0) != 0) continue;
      if (dom.Contains(target_term / target.coeffs(0))) {
        values.push_back(v);
      }
    }

    context_->UpdateRuleStats("int_mod: remove singleton target");
    if (!context_->IntersectDomainWith(expr.vars(0),
                                       Domain::FromValues(values))) {
      return false;
    }
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    ct->Clear();
    context_->UpdateConstraintVariableUsage(c);
    context_->MarkVariableAsRemoved(target.vars(0));
    return true;
  }

  return false;
}

// TODO(user): Now that everything has affine relations, we should maybe
// canonicalize all linear subexpressions in a generic way.
bool CpConstraintPresolver::ExploitEquivalenceRelations(int c,
                                                        ConstraintProto* ct) {
  bool changed = false;

  // Optim: Special case for the linear constraint. We just remap the
  // enforcement literals, the normal variables will be replaced by their
  // representative in CanonicalizeLinear().
  if (ct->constraint_case() == ConstraintProto::kLinear) {
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

bool CpConstraintPresolver::DivideLinearByGcd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // Compute the GCD of all coefficients.
  int64_t gcd = 0;
  const int num_vars = ct->linear().vars().size();
  for (int i = 0; i < num_vars; ++i) {
    const int64_t magnitude = std::abs(ct->linear().coeffs(i));
    gcd = std::gcd(gcd, magnitude);
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
      return MarkConstraintAsFalse(ct, "linear: not satisfied after GCD");
    }
  }
  return false;
}

bool CpConstraintPresolver::CanonicalizeLinearExpression(
    const ConstraintProto& ct, LinearExpressionProto* exp) {
  return context_->CanonicalizeLinearExpression(ct.enforcement_literal(), exp);
}

bool CpConstraintPresolver::CanonicalizeLinear(ConstraintProto* ct,
                                               bool* changed) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return true;
  if (context_->ModelIsUnsat()) return false;

  if (ct->linear().domain().empty()) {
    *changed = true;
    return MarkConstraintAsFalse(ct, "linear: no domain");
  }

  bool is_impossible = false;
  bool is_trivial = false;
  *changed =
      context_->CanonicalizeLinearConstraint(ct, &is_impossible, &is_trivial);
  if (is_impossible) {
    *changed = true;
    return MarkConstraintAsFalse(ct, "linear: never in domain");
  }
  if (is_trivial) {
    *changed = true;
    context_->UpdateRuleStats("linear: always true");
    return RemoveConstraint(ct);
  }
  *changed |= DivideLinearByGcd(ct);

  // For duplicate detection, we always make the first coeff positive.
  //
  // TODO(user): Move that to context_->CanonicalizeLinearConstraint(), and do
  // the same for LinearExpressionProto.
  if (!ct->linear().coeffs().empty() && ct->linear().coeffs(0) < 0) {
    for (int64_t& ref_coeff : *ct->mutable_linear()->mutable_coeffs()) {
      ref_coeff = -ref_coeff;
    }
    FillDomainInProto(ReadDomainFromProto(ct->linear()).Negation(),
                      ct->mutable_linear());
  }
  if (ct->constraint_case() != ConstraintProto::kLinear) return true;
  if (ct->linear().vars().empty()) {
    *changed = true;
    return PresolveEmptyLinearConstraint(ct);
  }
  bool changed_enforcement = false;
  if (!PresolveEnforcementLiteral(ct, &changed_enforcement)) {
    *changed = true;
    return false;
  }
  *changed = *changed || changed_enforcement;

  return true;
}

bool CpConstraintPresolver::RemoveSingletonInLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  absl::btree_set<int> index_to_erase;
  const int num_vars = ct->linear().vars().size();
  Domain rhs = ReadDomainFromProto(ct->linear());

  // First pass. Process singleton columns that are not in the objective. Note
  // that for postsolve, it is important that we process them in the same order
  // in which they will be removed.
  for (int i = 0; i < num_vars; ++i) {
    const int var = ct->linear().vars(i);
    const int64_t coeff = ct->linear().coeffs(i);
    CHECK(RefIsPositive(var));
    if (context_->VariableIsUniqueAndRemovable(var)) {
      // This is not needed for the code below, but in practice, removing
      // singleton with a large coefficient create holes in the constraint rhs
      // and we will need to add more variable to deal with that.
      // This works way better on timtab1CUTS.pb.gz for instance.
      if (std::abs(coeff) != 1) continue;

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

  // If the whole linear is independent from the rest of the problem, we
  // can solve it now. If it is enforced, then each variable will have two
  // values: Its minimum one and one minimizing the objective under the
  // constraint. The switch can be controlled by a single Boolean.
  //
  // TODO(user): Cover more cases like dedicated algorithm to solve for a small
  // number of variables that are faster than the DP we use here.
  if (index_to_erase.empty()) {
    int num_singletons = 0;
    for (const int var : ct->linear().vars()) {
      if (!RefIsPositive(var)) break;
      if (!context_->VariableWithCostIsUniqueAndRemovable(var) &&
          !context_->VariableIsUniqueAndRemovable(var)) {
        break;
      }
      ++num_singletons;
    }
    if (num_singletons == num_vars) {
      // Try to solve the equation.
      std::vector<Domain> domains;
      std::vector<int64_t> coeffs;
      std::vector<int64_t> costs;
      for (int i = 0; i < num_vars; ++i) {
        const int var = ct->linear().vars(i);
        CHECK(RefIsPositive(var));
        domains.push_back(context_->DomainOf(var));
        coeffs.push_back(ct->linear().coeffs(i));
        costs.push_back(context_->ObjectiveCoeff(var));
      }
      BasicKnapsackSolver solver;
      const auto& result = solver.Solve(domains, coeffs, costs,
                                        ReadDomainFromProto(ct->linear()));
      if (!result.solved) {
        context_->UpdateRuleStats(
            "TODO independent linear: minimize single linear constraint");
      } else if (result.infeasible) {
        return MarkConstraintAsFalse(
            ct, "independent linear: no DP solution to simple constraint");
      } else {
        if (ct->enforcement_literal().empty()) {
          // Just fix everything.
          context_->UpdateRuleStats("independent linear: solved by DP");
          for (int i = 0; i < num_vars; ++i) {
            if (!context_->IntersectDomainWith(ct->linear().vars(i),
                                               Domain(result.solution[i]))) {
              return false;
            }
          }
          return RemoveConstraint(ct);
        }

        // Each variable will take two values according to a single Boolean.
        int indicator;
        if (ct->enforcement_literal().size() == 1) {
          indicator = ct->enforcement_literal(0);
        } else {
          indicator =
              context_->NewBoolVarWithConjunction(ct->enforcement_literal());
          auto* new_ct = context_->AddConstraint();
          new_ct->mutable_bool_or()->add_literals(indicator);
          for (const int literal : ct->enforcement_literal()) {
            new_ct->mutable_bool_or()->add_literals(NegatedRef(literal));
          }
        }
        for (int i = 0; i < num_vars; ++i) {
          const int64_t best_value =
              costs[i] > 0 ? domains[i].Min() : domains[i].Max();
          const int64_t other_value = result.solution[i];
          if (best_value == other_value) {
            if (!context_->IntersectDomainWith(ct->linear().vars(i),
                                               Domain(best_value))) {
              return false;
            }
            continue;
          }
          solution_crush_.SetVarToConditionalValue(
              ct->linear().vars(i), {indicator}, other_value, best_value);
          if (RefIsPositive(indicator)) {
            if (!context_->StoreAffineRelation(ct->linear().vars(i), indicator,
                                               other_value - best_value,
                                               best_value)) {
              return false;
            }
          } else {
            if (!context_->StoreAffineRelation(
                    ct->linear().vars(i), PositiveRef(indicator),
                    best_value - other_value, other_value)) {
              return false;
            }
          }
        }
        context_->UpdateRuleStats(
            "independent linear: with enforcement, but solved by DP");
        return RemoveConstraint(ct);
      }
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

      // If the variable appears only in the objective and we have an equality,
      // we can transfer the cost to the rest of the linear expression, and
      // remove that variable. Note that this does not remove any feasible
      // solution and is not a "dual" reduction.
      //
      // Note that this is similar to the substitution code in PresolveLinear()
      // but it doesn't require the variable to be implied free since we do not
      // remove the constraints afterwards, just the variable.
      if (!context_->VariableWithCostIsUnique(var)) continue;
      DCHECK(context_->ObjectiveMap().contains(var));

      // We only support substitution that does not require multiplying the
      // objective by some factor.
      //
      // TODO(user): If the objective is a single variable, we can actually
      // "absorb" any factor into the objective scaling.
      const int64_t objective_coeff = context_->ObjectiveMap().at(var);
      CHECK_NE(coeff, 0);
      if (objective_coeff % coeff != 0) continue;

      // TODO(user): We have an issue if objective coeff is not one, because
      // the RecomputeSingletonObjectiveDomain() do not properly put holes
      // in the objective domain, which might cause an issue. Note that this
      // presolve rule is actually almost never applied on the miplib.
      if (std::abs(objective_coeff) != 1) continue;

      // We do not do that if the domain of rhs becomes too complex.
      bool exact;
      const auto term_domain =
          context_->DomainOf(var).MultiplicationBy(-coeff, &exact);
      if (!exact) continue;
      const Domain new_rhs = rhs.AdditionWith(term_domain);
      if (new_rhs.NumIntervals() > 100) continue;

      // Special case: If the objective was a single variable, we can transfer
      // the domain of var to the objective, and just completely remove this
      // equality constraint.
      //
      // TODO(user): Maybe if var has a complex domain, we might not want to
      // substitute it?
      if (context_->ObjectiveMap().size() == 1) {
        // This makes sure the domain of var is restricted and the objective
        // domain updated.
        if (!context_->RecomputeSingletonObjectiveDomain()) {
          return true;
        }

        // The function above might fix var, in which case, we just abort.
        if (context_->IsFixed(var)) continue;

        if (!context_->SubstituteVariableInObjective(var, coeff, *ct)) {
          if (context_->ModelIsUnsat()) return true;
          continue;
        }

        context_->UpdateRuleStats("linear: singleton column define objective");
        context_->MarkVariableAsRemoved(var);
        context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
        return RemoveConstraint(ct);
      }

      // On supportcase20, this transformation makes the LP relaxation way
      // worse. TODO(user): understand why.
      if (true) continue;

      // Update the objective and remove the variable from its equality
      // constraint by expanding its rhs. This might fail if the new linear
      // objective expression can lead to overflow.
      if (!context_->SubstituteVariableInObjective(var, coeff, *ct)) {
        if (context_->ModelIsUnsat()) return true;
        continue;
      }

      context_->UpdateRuleStats(
          "linear: singleton column in equality and in objective");
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
      auto* new_lin =
          context_->NewMappingConstraint(__FILE__, __LINE__)->mutable_linear();
      new_lin->add_vars(var);
      new_lin->add_coeffs(1);
      FillDomainInProto(context_->DomainOf(var), new_lin);
    }
  }

  // TODO(user): we could add the constraint to mapping_model only once
  // instead of adding a reduced version of it each time a new singleton
  // variable appear in the same constraint later. That would work but would
  // also force the postsolve to take search decisions...
  if (absl::GetFlag(FLAGS_cp_model_debug_postsolve)) {
    auto* new_ct = context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    const std::string name(new_ct->name());
    new_ct->set_name(absl::StrCat(ct->name(), " copy ", name));
  } else {
    *context_->NewMappingConstraint(*ct, __FILE__, __LINE__) = *ct;
  }

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
bool CpConstraintPresolver::AddVarAffineRepresentativeFromLinearEquality(
    int target_index, ConstraintProto* ct) {
  int64_t gcd = 0;
  const int num_variables = ct->linear().vars().size();
  for (int i = 0; i < num_variables; ++i) {
    if (i == target_index) continue;
    const int64_t magnitude = std::abs(ct->linear().coeffs(i));
    gcd = std::gcd(gcd, magnitude);
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
  bool changed = false;
  (void)CanonicalizeLinear(ct, &changed);
  return changed;
}

namespace {

bool IsLinearEqualityConstraint(const ConstraintProto& ct) {
  return ct.constraint_case() == ConstraintProto::kLinear &&
         ct.linear().domain().size() == 2 &&
         ct.linear().domain(0) == ct.linear().domain(1) &&
         ct.enforcement_literal().empty();
}

}  // namespace

// Any equality must be true modulo n.
//
// If the gcd of all but one term is not one, we can rewrite the last term using
// an affine representative by considering the equality modulo that gcd.
// As a heuristic, we only test the smallest term or small primes 2, 3, and 5.
//
// We also handle the special case of having two non-zero literals modulo 2.
//
// TODO(user): Use more complex algo to detect all the cases? By splitting the
// constraint in two, and computing the gcd of each half, we can reduce the
// problem to two problems of half size. So at least we can do it in O(n log n).
bool CpConstraintPresolver::PresolveLinearEqualityWithModulo(
    ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (!IsLinearEqualityConstraint(*ct)) return false;

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
        if (!context_->StoreBooleanEqualityRelation(literals[0],
                                                    NegatedRef(literals[1]))) {
          return false;
        }
      } else {
        if (!context_->StoreBooleanEqualityRelation(literals[0], literals[1])) {
          return false;
        }
      }
    }
  }

  // TODO(user): More than one reduction might be possible, so we will need
  // to call this again if we apply any of these reductions.
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

bool CpConstraintPresolver::PresolveLinearOfSizeOne(ConstraintProto* ct) {
  CHECK_EQ(ct->linear().vars().size(), 1);
  CHECK(RefIsPositive(ct->linear().vars(0)));
  DCHECK(context_->VariableIsAffineRepresentative(ct->linear().vars(0)));

  const int var = ct->linear().vars(0);
  const Domain var_domain = context_->DomainOf(var);
  const Domain rhs = ReadDomainFromProto(ct->linear())
                         .InverseMultiplicationBy(ct->linear().coeffs(0))
                         .IntersectionWith(var_domain);
  if (rhs.IsEmpty()) {
    return MarkConstraintAsFalse(ct, "linear1: infeasible");
  }
  if (rhs == var_domain) {
    context_->UpdateRuleStats("linear1: always true");
    return RemoveConstraint(ct);
  }

  // We can always canonicalize the constraint to a coefficient of 1.
  // Note that this should never trigger as we usually divide by gcd already.
  if (ct->linear().coeffs(0) != 1) {
    context_->UpdateRuleStats("linear1: canonicalized");
    ct->mutable_linear()->set_coeffs(0, 1);
    FillDomainInProto(rhs, ct->mutable_linear());
  }

  // Size one constraint with no enforcement?
  if (!HasEnforcementLiteral(*ct)) {
    context_->UpdateRuleStats("linear1: without enforcement");
    if (!context_->IntersectDomainWith(var, rhs)) return false;
    return RemoveConstraint(ct);
  }

  // This is just an implication, let's convert it right away.
  if (context_->CanBeUsedAsLiteral(var)) {
    DCHECK(rhs.IsFixed());
    if (rhs.FixedValue() == 1) {
      ct->mutable_bool_and()->add_literals(var);
    } else {
      CHECK_EQ(rhs.FixedValue(), 0);
      ct->mutable_bool_and()->add_literals(NegatedRef(var));
    }

    // No var <-> constraint graph changes.
    // But this is no longer a linear1.
    return true;
  }

  // Detect encoding.
  bool changed = false;
  if (ct->enforcement_literal().size() == 1) {
    // If we already have an encoding literal, this constraint is really
    // an implication.
    int lit = ct->enforcement_literal(0);

    // For correctness below, it is important lit is the canonical literal,
    // otherwise we might remove the constraint even though it is the one
    // defining an encoding literal.
    const int representative = context_->GetLiteralRepresentative(lit);
    if (lit != representative) {
      lit = representative;
      ct->set_enforcement_literal(0, lit);
      context_->UpdateRuleStats("linear1: remapped enforcement literal");
      changed = true;
    }

    if (rhs.IsFixed()) {
      const int64_t value = rhs.FixedValue();
      int encoding_lit;
      if (context_->HasVarValueEncoding(var, value, &encoding_lit)) {
        if (lit == encoding_lit) return changed;
        context_->AddImplication(lit, encoding_lit);
        ct->Clear();
        context_->UpdateRuleStats("linear1: transformed to implication");
        return true;
      } else {
        if (context_->StoreLiteralImpliesVarEqValue(lit, var, value)) {
          // The domain is not actually modified, but we want to rescan the
          // constraints linked to this variable.
          context_->modified_domains.Set(var);
        }
      }
      return changed;
    }

    const Domain complement = rhs.Complement().IntersectionWith(var_domain);
    if (complement.IsFixed()) {
      const int64_t value = complement.FixedValue();
      int encoding_lit;
      if (context_->HasVarValueEncoding(var, value, &encoding_lit)) {
        if (NegatedRef(lit) == encoding_lit) return changed;
        context_->AddImplication(lit, NegatedRef(encoding_lit));
        ct->Clear();
        context_->UpdateRuleStats("linear1: transformed to implication");
        return true;
      } else {
        if (context_->StoreLiteralImpliesVarNeValue(lit, var, value)) {
          // The domain is not actually modified, but we want to rescan the
          // constraints linked to this variable.
          context_->modified_domains.Set(var);
        }
      }
      return changed;
    }
  }

  return changed;
}

bool CpConstraintPresolver::PresolveLinearOfSizeTwo(ConstraintProto* ct) {
  DCHECK_EQ(ct->linear().vars().size(), 2);

  const LinearConstraintProto& arg = ct->linear();
  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);
  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);
  bool changed = false;

  // Start by updating our hash map of known relations.
  {
    const LinearExpression2 expr2 =
        GetLinearExpression2FromProto(var1, coeff1, var2, coeff2);
    const IntegerValue lb(arg.domain(0));
    const IntegerValue ub(arg.domain(arg.domain().size() - 1));

    const RelationStatus status = known_linear2_.GetStatus(expr2, lb, ub);
    if (status == RelationStatus::IS_TRUE) {
      // Note that we don't track what constraint implied the relation, so we
      // cannot remove this constraint even if the relation is already known.
      //
      // Even if the relation is enforced, some propagator might have detected
      // that the enforcement literal can only be false when the relationship
      // is satisfied, thus detecting the linear2 during probing. What we can
      // say here with certainty is that the constraint is always satisfied,
      // even when the enforcement literal is false, so we can remove the
      // enforcement.
      //
      // Tricky: If the constraint domain is not simple, we cannot really deduce
      // anything.
      if (!ct->enforcement_literal().empty() &&
          ct->linear().domain().size() == 2) {
        context_->UpdateRuleStats("linear2: already known enforced relation");
        ct->clear_enforcement_literal();
        const auto [known_lb, known_ub] = known_linear2_.GetBounds(expr2);
        DCHECK_GE(known_lb, lb);  // Guaranteed by GetStatus.
        DCHECK_LE(known_ub, ub);  // Guaranteed by GetStatus.
        ct->mutable_linear()->set_domain(0, known_lb.value());
        ct->mutable_linear()->set_domain(1, known_ub.value());
        changed = true;
      }
    } else if (status == RelationStatus::IS_FALSE) {
      return MarkConstraintAsFalse(ct, "linear2: infeasible relation");
    } else if (ct->enforcement_literal().empty()) {
      known_linear2_.Add(expr2, lb, ub);
      known_model_linear2_.Add(expr2, lb, ub);
      if (context_->ModelIsUnsat()) return false;
    }
  }

  const Domain rhs = ReadDomainFromProto(arg);
  bool mult1_is_exact = true;
  bool mult2_is_exact = true;
  const Domain scaled_domain1 =
      context_->DomainOf(var1).MultiplicationBy(coeff1, &mult1_is_exact);
  const Domain scaled_domain2 =
      context_->DomainOf(var2).MultiplicationBy(coeff2, &mult2_is_exact);
  if (mult1_is_exact && mult2_is_exact) {
    // We avoid IntersectionWith(rhs.Complement()) to not allocate memory
    // for problem with thousands of linear2...
    std::optional<int64_t> unique_not_reachable =
        scaled_domain1.AdditionWith(scaled_domain2).UniqueValueNotIn(rhs);
    if (unique_not_reachable != std::nullopt) {
      return PresolveLinear2NeCst(ct, *unique_not_reachable) || changed;
    }
  }

  if (rhs.IsFixed()) {
    if (ct->enforcement_literal().empty()) {
      return PresolveUnenforcedLinear2EqCst(ct, rhs.FixedValue()) || changed;
    } else {
      return PresolveEnforcedLinear2EqCst(ct, rhs.FixedValue()) || changed;
    }
  }

  return PresolveLinear2WithBooleans(ct) || changed;
}

// If it is not an equality, we only presolve the constraint if one of
// the variable is Boolean. Note that if both are Boolean, then a similar
// reduction is done by PresolveLinearOnBooleans(). If we have an equality,
// then the code below will do something stronger than this.
//
// TODO(user): We should probably instead generalize the code of
// ExtractEnforcementLiteralFromLinearConstraint(), or just temporary
// propagate domain of enforced linear constraints, to detect Boolean that
// must be true or false. This way we can do the same for longer constraints.
bool CpConstraintPresolver::PresolveLinear2WithBooleans(ConstraintProto* ct) {
  DCHECK_EQ(ct->linear().vars().size(), 2);

  const LinearConstraintProto& arg = ct->linear();
  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);
  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);

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
  if (!RefIsPositive(lit)) return false;

  // The constraint is really:
  // - enforcement & lit      => var \in var_domain_on_true
  // - enforcement & not(lit) => var \in var_domain_on_false
  // We will always rewrite it as such.
  const Domain rhs = ReadDomainFromProto(ct->linear());
  const Domain rhs_if_true =
      rhs.AdditionWith(Domain(-value_on_true)).InverseMultiplicationBy(coeff);
  const Domain rhs_if_false = rhs.InverseMultiplicationBy(coeff);

  // The lit in the linear2 can imply something on var, in which case we
  // have more information on the two possible domains of var.
  //
  // Tricky: We don't want the deduction to come from this constraint, otherwise
  // the reasoning below will be wrong. But currently this should be safe since
  // we don't push this kind of implied domain from a linear2.
  const Domain var_domain = context_->DomainOf(var);
  const Domain domain_on_true =
      var_domain.IntersectionWith(context_->deductions.ImpliedDomain(lit, var));
  const Domain domain_on_false = var_domain.IntersectionWith(
      context_->deductions.ImpliedDomain(NegatedRef(lit), var));

  // This is really the same as rhs_if_true/false EXCEPT if we already have
  // lit => var \in domain !!
  const Domain var_domain_on_true =
      domain_on_true.IntersectionWith(rhs_if_true);
  const Domain var_domain_on_false =
      domain_on_false.IntersectionWith(rhs_if_false);

  if (var_domain_on_false == var_domain_on_true) {
    // This is really just a linear1 !
    context_->UpdateRuleStats("linear2: reduce to a linear1");
    ct->mutable_linear()->Clear();
    ct->mutable_linear()->add_vars(var);
    ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(var_domain_on_true, ct->mutable_linear());
    return PresolveSmallLinear(ct) || true;
  }

  const bool implied_false = var_domain_on_true.IsEmpty();
  const bool implied_true = var_domain_on_false.IsEmpty();
  if (implied_true && implied_false) {
    return MarkConstraintAsFalse(ct, "linear2: infeasible");
  } else if (implied_true) {
    context_->UpdateRuleStats("linear2: boolean with one feasible value");

    // => true.
    ConstraintProto* new_ct = context_->AddConstraint();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    new_ct->mutable_bool_and()->add_literals(lit);

    // Rewrite to => var in var_domain_on_true.
    ct->mutable_linear()->Clear();
    ct->mutable_linear()->add_vars(var);
    ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(var_domain_on_true, ct->mutable_linear());
    return PresolveSmallLinear(ct) || true;
  } else if (implied_false) {
    context_->UpdateRuleStats("linear2: boolean with one feasible value");

    // => false.
    ConstraintProto* new_ct = context_->AddConstraint();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    new_ct->mutable_bool_and()->add_literals(NegatedRef(lit));

    // Rewrite to => var in var_domain_on_false.
    ct->mutable_linear()->Clear();
    ct->mutable_linear()->add_vars(var);
    ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(var_domain_on_false, ct->mutable_linear());
    return PresolveSmallLinear(ct) || true;
  }

  // We always expand such linear 2.
  if (domain_on_true != var_domain || domain_on_false != var_domain) {
    context_->UpdateRuleStats("linear2: contains a related Boolean");
  } else {
    context_->UpdateRuleStats("linear2: contains a Boolean");
  }

  // lit => var \in var_domain_on_true
  if (var_domain_on_true != var_domain) {
    ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
    if (var_domain_on_false.IsIncludedIn(var_domain_on_true)) {
      // This is true independently of the value of lit!
      context_->UpdateRuleStats("linear2: simplified one alternative");
    } else {
      new_ct->add_enforcement_literal(lit);
    }
    new_ct->mutable_linear()->add_vars(var);
    new_ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(var_domain_on_true, new_ct->mutable_linear());
  }

  // NegatedRef(lit) => var \in var_domain_on_false
  if (var_domain_on_false != var_domain) {
    ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
    if (var_domain_on_true.IsIncludedIn(var_domain_on_false)) {
      // This is true independently of the value of lit!
      context_->UpdateRuleStats("linear2: simplified one alternative");
    } else {
      new_ct->add_enforcement_literal(NegatedRef(lit));
    }
    new_ct->mutable_linear()->add_vars(var);
    new_ct->mutable_linear()->add_coeffs(1);
    FillDomainInProto(var_domain_on_false, new_ct->mutable_linear());
  }

  return RemoveConstraint(ct);
}

bool CpConstraintPresolver::PresolveLinear2NeCst(ConstraintProto* ct,
                                                 int64_t rhs) {
  const LinearConstraintProto& arg = ct->linear();
  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);

  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);

  // coeff1 * v1 + coeff2 * v2 != cte.
  int64_t a = coeff1;
  int64_t b = coeff2;
  int64_t cte = rhs;
  int64_t x0 = 0;
  int64_t y0 = 0;
  if (!SolveDiophantineEquationOfSizeTwo(a, b, cte, x0, y0)) {
    // no solution.
    context_->UpdateRuleStats("linear2: remove always feasible ax + by != cte");
    return RemoveConstraint(ct);
  }

  const Domain domain_of_z =
      context_->DomainOf(var1)
          .AdditionWith(Domain(-x0))
          .InverseMultiplicationBy(b)
          .IntersectionWith(context_->DomainOf(var2)
                                .AdditionWith(Domain(-y0))
                                .InverseMultiplicationBy(-a));
  const int64_t max_domain_size =
      context_->params().max_domain_size_for_linear2_expansion();
  const int64_t small_domain_size = max_domain_size / 2;
  if (domain_of_z.Size() <= max_domain_size &&
      (context_->IsMostlyFullyEncoded(var1) ||
       context_->DomainSize(var1) <= small_domain_size) &&
      (context_->IsMostlyFullyEncoded(var2) ||
       context_->DomainSize(var2) <= small_domain_size)) {
    // The number of clauses to create is small enough. We can encode the
    // constraint using just clauses.
    int num_clauses = 0;
    for (const int64_t z : domain_of_z.Values()) {
      const int64_t value1 = x0 + b * z;
      const int64_t value2 = y0 - a * z;
      DCHECK_EQ(coeff1 * value1 + coeff2 * value2, rhs);
      if (!context_->VarCanTakeValue(var1, value1) ||
          !context_->VarCanTakeValue(var2, value2)) {
        continue;
      }

      // We cannot have both lit1 and lit2 true.
      const int lit1 = context_->GetOrCreateVarValueEncoding(var1, value1);
      const int lit2 = context_->GetOrCreateVarValueEncoding(var2, value2);
      auto* bool_or = context_->AddConstraint()->mutable_bool_or();
      bool_or->add_literals(NegatedRef(lit1));
      bool_or->add_literals(NegatedRef(lit2));
      for (const int lit : ct->enforcement_literal()) {
        bool_or->add_literals(NegatedRef(lit));
      }
      ++num_clauses;
    }

    VLOG(3) << "ConvertLinear2NeCst: |enforcements| = "
            << ct->enforcement_literal_size()
            << ", domain1 = " << context_->DomainOf(var1)
            << ", domain2 = " << context_->DomainOf(var2)
            << ", coeff1 = " << coeff1 << ", coeff2 = " << coeff2
            << ", domain_of_z = " << domain_of_z
            << ", num_clauses = " << num_clauses;

    context_->UpdateRuleStats("linear2: convert ax + by != cte to clauses");
    return RemoveConstraint(ct);
  } else {
    VLOG(3) << "TODO ConvertLinear2NeCst: |enforcements| = "
            << ct->enforcement_literal_size()
            << ", domain1 = " << context_->DomainOf(var1)
            << ", domain2 = " << context_->DomainOf(var2)
            << ", coeff1 = " << coeff1 << ", coeff2 = " << coeff2
            << ", rhs = " << rhs << ", domain_of_z = " << domain_of_z
            << ", |encoding1| = " << context_->GetValueEncodingSize(var1)
            << ", |encoding2| = " << context_->GetValueEncodingSize(var2);
    context_->UpdateRuleStats(
        "TODO linear2: convert ax + by != cte to clauses for large domains");
    return false;
  }
}

bool CpConstraintPresolver::PresolveUnenforcedLinear2EqCst(ConstraintProto* ct,
                                                           int64_t rhs) {
  DCHECK_EQ(ct->linear().vars().size(), 2);

  const LinearConstraintProto& arg = ct->linear();
  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);
  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);

  // We have: enforcement => (coeff1 * v1 + coeff2 * v2 == rhs).
  CHECK(ct->enforcement_literal().empty());
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
    // Note that PresolveLinearEqualityWithModulo() will have the same effect.
    //
    // We can also decide to fully expand the equality if the variables
    // are fully encoded.
    context_->UpdateRuleStats("TODO linear2: ax + by = cte");
  }
  if (added) return RemoveConstraint(ct);
  return false;
}

bool CpConstraintPresolver::PresolveEnforcedLinear2EqCst(ConstraintProto* ct,
                                                         int64_t rhs) {
  CHECK(!ct->enforcement_literal().empty());
  DCHECK(context_->VariableIsAffineRepresentative(ct->linear().vars(0)));
  DCHECK(context_->VariableIsAffineRepresentative(ct->linear().vars(1)));
  const LinearConstraintProto& arg = ct->linear();

  const int var1 = arg.vars(0);
  const int64_t coeff1 = arg.coeffs(0);

  const int var2 = arg.vars(1);
  const int64_t coeff2 = arg.coeffs(1);

  // We look ahead to detect solutions to ax + by == cte.
  int64_t a = coeff1;
  int64_t b = coeff2;
  int64_t cte = rhs;
  int64_t x0 = 0;
  int64_t y0 = 0;
  if (!SolveDiophantineEquationOfSizeTwo(a, b, cte, x0, y0)) {
    return MarkConstraintAsFalse(
        ct, "linear2: implied ax + by = cte has no solutions");
  }
  const Domain reduced_domain =
      context_->DomainOf(var1)
          .AdditionWith(Domain(-x0))
          .InverseMultiplicationBy(b)
          .IntersectionWith(context_->DomainOf(var2)
                                .AdditionWith(Domain(-y0))
                                .InverseMultiplicationBy(-a));

  if (reduced_domain.IsEmpty()) {  // no solution
    return MarkConstraintAsFalse(
        ct, "linear2: implied ax + by = cte has no solutions");
  }

  if (reduced_domain.Size() == 1) {
    const int64_t z = reduced_domain.FixedValue();
    const int64_t value1 = x0 + b * z;
    const int64_t value2 = y0 - a * z;

    DCHECK(context_->DomainOf(var1).Contains(value1));
    DCHECK(context_->DomainOf(var2).Contains(value2));
    DCHECK_EQ(coeff1 * value1 + coeff2 * value2, rhs);

    LinearConstraintProto* linear1 =
        context_->AddEnforcedConstraint(ct)->mutable_linear();
    linear1->add_vars(var1);
    linear1->add_coeffs(1);
    linear1->add_domain(value1);
    linear1->add_domain(value1);

    LinearConstraintProto* linear2 =
        context_->AddEnforcedConstraint(ct)->mutable_linear();
    linear2->add_vars(var2);
    linear2->add_coeffs(1);
    linear2->add_domain(value2);
    linear2->add_domain(value2);

    context_->UpdateRuleStats(
        "linear2: implied ax + by = cte has only one solution");
    return RemoveConstraint(ct);
  }

  if ((std::abs(coeff1) == 1 || std::abs(coeff2) == 1) &&
      !context_->IsFullyEncoded(var1) && !context_->IsFullyEncoded(var2)) {
    // Solving the diophantine equation will not necessarily create a simpler
    // domain. We still do it if we have already a full encoding we can reuse.
    return false;
  }

  const int64_t domain_size_threshold =
      context_->params().max_domain_size_for_linear2_expansion();
  if (reduced_domain.Size() <= domain_size_threshold) {
    // Encode the set of possible solution to the equation.
    absl::btree_set<int64_t> seen[2];  // For determinism, small in any case.
    for (const int64_t z : reduced_domain.Values()) {
      const int64_t value1 = x0 + b * z;
      const int64_t value2 = y0 - a * z;

      seen[0].insert(value1);
      seen[1].insert(value2);

      const int lit1 = context_->GetOrCreateVarValueEncoding(var1, value1);
      const int lit2 = context_->GetOrCreateVarValueEncoding(var2, value2);

      ConstraintProto* imply_equiv1 = context_->AddConstraint();
      imply_equiv1->mutable_bool_or()->add_literals(NegatedRef(lit1));
      imply_equiv1->mutable_bool_or()->add_literals(lit2);
      for (const int lit : ct->enforcement_literal()) {
        imply_equiv1->mutable_bool_or()->add_literals(NegatedRef(lit));
      }

      ConstraintProto* imply_equiv2 = context_->AddConstraint();
      imply_equiv2->mutable_bool_or()->add_literals(lit1);
      imply_equiv2->mutable_bool_or()->add_literals(NegatedRef(lit2));
      for (const int lit : ct->enforcement_literal()) {
        imply_equiv2->mutable_bool_or()->add_literals(NegatedRef(lit));
      }
    }

    // If the domains of var1 (resp. var2) is small, exclude the other values
    // directly. Otherwise, add a clause on the possible values.
    for (const int i : {0, 1}) {
      const int var = arg.vars(i);
      const int64_t diff = context_->DomainSize(var) - seen[i].size();

      if (diff == 0) {
        // The whole domain is covered, no need for anything extra.
      } else if (diff <= domain_size_threshold ||
                 context_->IsMostlyFullyEncoded(var)) {
        // Tricky: The domain must be cached, it is not 100% clear why, but
        // GetOrCreateVarValueEncoding() might modify it somehow?
        const Domain domain = context_->DomainOf(var);

        // Exclude other values.
        BoolArgumentProto* bool_and =
            context_->AddEnforcedConstraint(ct)->mutable_bool_and();
        for (const int64_t value : domain.Values()) {
          if (!seen[i].contains(value)) {
            const int lit = context_->GetOrCreateVarValueEncoding(var, value);
            bool_and->add_literals(NegatedRef(lit));
          }
        }
      } else {
        // Add a clause on the set of possible values.
        BoolArgumentProto* clause =
            context_->AddConstraint()->mutable_bool_or();
        for (const int lit : ct->enforcement_literal()) {
          clause->add_literals(NegatedRef(lit));
        }
        for (const int64_t value : seen[i]) {
          const int lit = context_->GetOrCreateVarValueEncoding(var, value);
          clause->add_literals(lit);
        }
      }
    }

    VLOG(3) << "ConvertLinear2EqCst: |enforcements| = "
            << ct->enforcement_literal_size()
            << ", domain1 = " << context_->DomainOf(var1)
            << ", domain2 = " << context_->DomainOf(var2)
            << ", coeff1 = " << coeff1 << ", coeff2 = " << coeff2
            << " equal_size=" << reduced_domain.Size();

    context_->UpdateRuleStats(
        "linear2: convert implied ax + by == cte to clauses");
    return RemoveConstraint(ct);
  } else {
    VLOG(3) << "TODO ConvertLinear2EqCst: |enforcements| = "
            << ct->enforcement_literal_size()
            << ", domain1 = " << context_->DomainOf(var1)
            << ", domain2 = " << context_->DomainOf(var2)
            << ", coeff1 = " << coeff1 << ", coeff2 = " << coeff2
            << ", rhs = " << rhs
            << ", |encoding1| = " << context_->GetValueEncodingSize(var1)
            << ", |encoding2| = " << context_->GetValueEncodingSize(var2);
    context_->UpdateRuleStats(
        "TODO linear2: convert implied ax + by == cte to clauses for large "
        "domains");
  }
  return false;
}

bool CpConstraintPresolver::PresolveEmptyLinearConstraint(ConstraintProto* ct) {
  const Domain rhs = ReadDomainFromProto(ct->linear());
  if (rhs.Contains(0)) {
    context_->UpdateRuleStats("linear: empty");
    return RemoveConstraint(ct);
  } else {
    return MarkConstraintAsFalse(ct, "linear: empty");
  }
}

bool CpConstraintPresolver::PresolveSmallLinear(ConstraintProto* ct,
                                                bool canonicalize) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (context_->ModelIsUnsat()) return false;
  if (ct->linear().vars().size() > 2) return false;

  bool changed = false;
  if (canonicalize && !CanonicalizeLinear(ct, &changed)) return true;
  if (ct->constraint_case() != ConstraintProto::kLinear) return true;

  if (ct->linear().vars().empty()) {
    return PresolveEmptyLinearConstraint(ct);
  } else if (ct->linear().vars().size() == 1) {
    return PresolveLinearOfSizeOne(ct) || changed;
  } else if (ct->linear().vars().size() == 2) {
    return PresolveLinearOfSizeTwo(ct) || changed;
  }

  return changed;
}

bool CpConstraintPresolver::PresolveDiophantine(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (ct->linear().vars().size() <= 1) return false;
  if (context_->ModelIsUnsat()) return false;
  // The transformation can add extra variables, and creates duplicate solutions
  // when enumerate_all_solutions is true.
  if (context_->params().enumerate_all_solutions()) return false;

  const LinearConstraintProto& linear_constraint = ct->linear();
  if (linear_constraint.domain_size() != 2) return false;
  if (linear_constraint.domain(0) != linear_constraint.domain(1)) return false;

  std::vector<int64_t> lbs(linear_constraint.vars_size());
  std::vector<int64_t> ubs(linear_constraint.vars_size());
  for (int i = 0; i < linear_constraint.vars_size(); ++i) {
    lbs[i] = context_->MinOf(linear_constraint.vars(i));
    ubs[i] = context_->MaxOf(linear_constraint.vars(i));
  }
  const DiophantineSolution diophantine_sol = SolveDiophantine(
      linear_constraint.coeffs(), linear_constraint.domain(0), lbs, ubs);

  if (!diophantine_sol.has_solutions) {
    return MarkConstraintAsFalse(ct, "diophantine: equality has no solutions");
  }
  if (diophantine_sol.no_reformulation_needed) return false;
  // Only first coefficients of kernel_basis elements and special_solution could
  // overflow int64_t due to the reduction applied in SolveDiophantineEquation,
  for (const std::vector<absl::int128>& b : diophantine_sol.kernel_basis) {
    if (!IsNegatableInt64(b[0])) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to int64_t overflow");
      return false;
    }
  }
  if (!IsNegatableInt64(diophantine_sol.special_solution[0])) {
    context_->UpdateRuleStats(
        "diophantine: couldn't apply due to int64_t overflow");
    return false;
  }

  const int num_replaced_variables =
      static_cast<int>(diophantine_sol.special_solution.size());
  const int num_new_variables =
      static_cast<int>(diophantine_sol.kernel_vars_lbs.size());
  DCHECK_EQ(num_new_variables + 1, num_replaced_variables);
  for (int i = 0; i < num_new_variables; ++i) {
    if (!IsNegatableInt64(diophantine_sol.kernel_vars_lbs[i]) ||
        !IsNegatableInt64(diophantine_sol.kernel_vars_ubs[i])) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to int64_t overflow");
      return false;
    }
  }
  // TODO(user): Make sure the newly generated linear constraint
  // satisfy our no-overflow precondition on the min/max activity.
  // We should check that the model still satisfy conditions in
  // `PossibleIntegerOverflow` (sat/cp_model_checker.cc)

  // Initialize indices for new variables.
  // We will only create them if we don't abort due to overflow..
  std::vector<int> new_variables(num_new_variables);
  for (int i = 0; i < num_new_variables; ++i) {
    new_variables[i] = context_->NumVariables() + i;
  }

  // For i = 0, ..., num_replaced_variables - 1, creates
  //  x[i] = special_solution[i]
  //        + sum(kernel_basis[k][i]*y[k], max(1, i) <= k < vars.size - 1)
  // where:
  //  y[k] is the newly created variable if 0 <= k < num_new_variables
  //  y[k] = x[index_permutation[k + 1]] otherwise.
  std::vector<std::vector<int64_t>> lin_vars_lbs(num_replaced_variables);
  const int old_num_constraints = context_->NumConstraints();
  for (int i = 0; i < num_replaced_variables; ++i) {
    LinearOverflowChecker checker;
    bool safe = true;

    ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
    LinearConstraintProto* lin = new_ct->mutable_linear();
    if (!ct->name().empty()) {
      new_ct->set_name(absl::StrCat("c_diophantine_", ct->name(), "_", i));
    }

    const int var =
        linear_constraint.vars(diophantine_sol.index_permutation[i]);
    lin->add_vars(var);
    lin_vars_lbs[i].push_back(context_->MinOf(var));
    lin->add_coeffs(1);
    safe &= checker.AddTerm(1, context_->MinOf(var), context_->MaxOf(var));

    lin->add_domain(static_cast<int64_t>(diophantine_sol.special_solution[i]));
    lin->add_domain(static_cast<int64_t>(diophantine_sol.special_solution[i]));
    for (int j = std::max(1, i); j < num_replaced_variables; ++j) {
      const int64_t lb =
          static_cast<int64_t>(diophantine_sol.kernel_vars_lbs[j - 1]);
      const int64_t ub =
          static_cast<int64_t>(diophantine_sol.kernel_vars_ubs[j - 1]);
      const int64_t coeff =
          -static_cast<int64_t>(diophantine_sol.kernel_basis[j - 1][i]);
      lin_vars_lbs[i].push_back(lb);

      lin->add_vars(new_variables[j - 1]);
      lin->add_coeffs(coeff);
      if (!checker.AddTerm(coeff, lb, ub)) {
        safe = false;
        break;
      }
    }
    for (int j = num_replaced_variables; j < linear_constraint.vars_size();
         ++j) {
      const int var =
          linear_constraint.vars(diophantine_sol.index_permutation[j]);
      const int64_t lb = context_->MinOf(var);
      const int64_t ub = context_->MaxOf(var);
      const int64_t coeff =
          -static_cast<int64_t>(diophantine_sol.kernel_basis[j - 1][i]);
      lin_vars_lbs[i].push_back(lb);
      lin->add_vars(var);
      lin->add_coeffs(coeff);
      if (!checker.AddTerm(coeff, lb, ub)) {
        safe = false;
        break;
      }
    }
    if (!safe) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to overflowing activity of new "
          "constraints");
      // Cancel working_model changes.
      for (int j = 0; j <= i; ++j) {
        context_->RemoveLastConstraint();
      }
      CHECK_EQ(old_num_constraints, context_->NumConstraints());
      return false;
    }
  }

  // We are good to go. Do create the new variables now.
  for (int i = 0; i < num_new_variables; ++i) {
    const int64_t lb = static_cast<int64_t>(diophantine_sol.kernel_vars_lbs[i]);
    const int64_t ub = static_cast<int64_t>(diophantine_sol.kernel_vars_ubs[i]);
    const int var = context_->NewIntVar(Domain(lb, ub));
    context_->UpdateRuleStats("new_int_var: diophantine solution");
    CHECK_EQ(var, new_variables[i]);
    if (!ct->name().empty()) {
      context_->SetVarName(var,
                           absl::StrCat("u_diophantine_", ct->name(), "_", i));
    }
  }
  context_->InitializeNewDomains();

  // Scan the new constraints added above in reverse order so that the hint of
  // `new_variables[k]` can be computed from the hint of the existing variables
  // and from the hints of `new_variables[k']`, with k' > k.
  const int num_constraints = context_->NumConstraints();
  for (int i = 0; i < num_replaced_variables; ++i) {
    const LinearConstraintProto& linear =
        context_->Constraint(num_constraints - 1 - i).linear();
    DCHECK(linear.domain_size() == 2 && linear.domain(0) == linear.domain(1));
    solution_crush_.SetVarToLinearConstraintSolution(
        ct->enforcement_literal(), std::nullopt, linear.vars(), lin_vars_lbs[i],
        linear.coeffs(), linear.domain(0));
  }

  if (VLOG_IS_ON(2)) {
    std::string log_eq = absl::StrCat(linear_constraint.domain(0), " = ");
    const int terms_to_show = std::min<int>(15, linear_constraint.vars_size());
    for (int i = 0; i < terms_to_show; ++i) {
      if (i > 0) absl::StrAppend(&log_eq, " + ");
      absl::StrAppend(
          &log_eq,
          linear_constraint.coeffs(diophantine_sol.index_permutation[i]), " x",
          linear_constraint.vars(diophantine_sol.index_permutation[i]));
    }
    if (terms_to_show < linear_constraint.vars_size()) {
      absl::StrAppend(&log_eq, "+ ... (", linear_constraint.vars_size(),
                      " terms)");
    }
    VLOG(2) << "[Diophantine] " << log_eq;
  }

  context_->UpdateRuleStats("diophantine: reformulated equality");
  return RemoveConstraint(ct);
}

// This tries to decompose the constraint into coeff * part1 + part2 and show
// that the value that part2 take is not important, thus the constraint can
// only be transformed on a constraint on the first part.
//
// TODO(user): Improve !! we miss simple case like x + 47 y + 50 z >= 50
// for positive variables. We should remove x, and ideally we should rewrite
// this as y + 2z >= 2 if we can show that its relaxation is just better?
// We should at least see that it is the same as 47y + 50 z >= 48.
//
// TODO(user): One easy algo is to first remove all enforcement terms (even
// non-Boolean ones) before applying the algo here and then re-linearize the
// non-Boolean terms.
void CpConstraintPresolver::TryToReduceCoefficientsOfLinearConstraint(
    int c, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return;
  if (context_->ModelIsUnsat()) return;

  // Only consider "simple" constraints.
  const LinearConstraintProto& lin = ct->linear();
  if (lin.domain().size() != 2) return;
  if (lin.vars().size() <= 1) return;

  // Precompute a bunch of quantities and "canonicalize" the constraint.
  int64_t lb_sum = 0;
  int64_t ub_sum = 0;
  int64_t max_variation = 0;

  rd_entries_.clear();
  rd_magnitudes_.clear();
  rd_lbs_.clear();
  rd_ubs_.clear();

  int64_t max_magnitude = 0;
  const int num_terms = lin.vars().size();
  for (int i = 0; i < num_terms; ++i) {
    const int64_t coeff = lin.coeffs(i);
    const int64_t magnitude = std::abs(lin.coeffs(i));
    if (magnitude == 0) continue;
    max_magnitude = std::max(max_magnitude, magnitude);

    int64_t lb;
    int64_t ub;
    if (coeff > 0) {
      lb = context_->MinOf(lin.vars(i));
      ub = context_->MaxOf(lin.vars(i));
    } else {
      lb = -context_->MaxOf(lin.vars(i));
      ub = -context_->MinOf(lin.vars(i));
    }
    lb_sum += lb * magnitude;
    ub_sum += ub * magnitude;

    // Abort if fixed term, that might mess up code below.
    if (lb == ub) return;

    rd_lbs_.push_back(lb);
    rd_ubs_.push_back(ub);
    rd_magnitudes_.push_back(magnitude);
    rd_entries_.push_back({magnitude, magnitude * (ub - lb), i});
    max_variation += rd_entries_.back().max_variation;
  }

  // Mark trivially false constraint as such. This should have been already
  // done, but we require non-negative quantity below.
  const Domain rhs = ReadDomainFromProto(lin);
  if (lb_sum > rhs.Max() || rhs.Min() > ub_sum) {
    (void)MarkConstraintAsFalse(ct, "linear: trivially false");
    context_->UpdateConstraintVariableUsage(c);
    return;
  }
  const IntegerValue rhs_ub(CapSub(rhs.Max(), lb_sum));
  const IntegerValue rhs_lb(CapSub(ub_sum, rhs.Min()));
  const bool use_ub = max_variation > rhs_ub;
  const bool use_lb = max_variation > rhs_lb;
  if (!use_ub && !use_lb) {
    context_->UpdateRuleStats("linear: trivially true");
    (void)RemoveConstraint(ct);
    context_->UpdateConstraintVariableUsage(c);
    return;
  }

  // No point doing more work for constraints with all coeffs at +/-1.
  if (max_magnitude <= 1) return;

  // TODO(user): All the lb/ub_feasible/infeasible class are updated in
  // exactly the same way. Find a more efficient algo?
  if (use_lb) {
    lb_feasible_.Reset(rhs_lb.value());
    lb_infeasible_.Reset(rhs.Min() - lb_sum - 1);
  }
  if (use_ub) {
    ub_feasible_.Reset(rhs_ub.value());
    ub_infeasible_.Reset(ub_sum - rhs.Max() - 1);
  }

  // Process entries by decreasing magnitude. Update max_error to correspond
  // only to the sum of the not yet processed terms.
  uint64_t gcd = 0;
  int64_t max_error = max_variation;
  std::stable_sort(rd_entries_.begin(), rd_entries_.end(),
                   [](const RdEntry& a, const RdEntry& b) {
                     return a.magnitude > b.magnitude;
                   });
  int64_t range = 0;
  rd_divisors_.clear();
  for (int i = 0; i < rd_entries_.size(); ++i) {
    const RdEntry& e = rd_entries_[i];
    gcd = std::gcd(gcd, e.magnitude);
    max_error -= e.max_variation;

    // We regroup all terms with the same coefficient into one.
    //
    // TODO(user): I am not sure there is no possible simplification across two
    // terms with the same coeff, but it should be rare if it ever happens.
    range += e.max_variation / e.magnitude;
    if (i + 1 < rd_entries_.size() &&
        e.magnitude == rd_entries_[i + 1].magnitude) {
      continue;
    }
    const int64_t saved_range = range;
    range = 0;

    if (e.magnitude > 1) {
      if ((!use_ub ||
           max_error <= PositiveRemainder(rhs_ub, IntegerValue(e.magnitude))) &&
          (!use_lb ||
           max_error <= PositiveRemainder(rhs_lb, IntegerValue(e.magnitude)))) {
        rd_divisors_.push_back(e.magnitude);
      }
    }

    bool simplify_lb = false;
    if (use_lb) {
      lb_feasible_.AddMultiples(e.magnitude, saved_range);
      lb_infeasible_.AddMultiples(e.magnitude, saved_range);

      // For a <= constraint, the max_feasible + error is still feasible.
      if (CapAdd(lb_feasible_.CurrentMax(), max_error) <=
          lb_feasible_.Bound()) {
        simplify_lb = true;
      }
      // For a <= constraint describing the infeasible set, the max_infeasible +
      // error is still infeasible.
      if (CapAdd(lb_infeasible_.CurrentMax(), max_error) <=
          lb_infeasible_.Bound()) {
        simplify_lb = true;
      }
    } else {
      simplify_lb = true;
    }
    bool simplify_ub = false;
    if (use_ub) {
      ub_feasible_.AddMultiples(e.magnitude, saved_range);
      ub_infeasible_.AddMultiples(e.magnitude, saved_range);
      if (CapAdd(ub_feasible_.CurrentMax(), max_error) <=
          ub_feasible_.Bound()) {
        simplify_ub = true;
      }
      if (CapAdd(ub_infeasible_.CurrentMax(), max_error) <=
          ub_infeasible_.Bound()) {
        simplify_ub = true;
      }
    } else {
      simplify_ub = true;
    }

    if (max_error == 0) break;  // Last term.
    if (simplify_lb && simplify_ub) {
      // We have a simplification since the second part can be ignored.
      context_->UpdateRuleStats("linear: remove irrelevant part");
      int64_t shift_lb = 0;
      int64_t shift_ub = 0;
      rd_vars_.clear();
      rd_coeffs_.clear();
      for (int j = 0; j <= i; ++j) {
        const int index = rd_entries_[j].index;
        const int64_t m = rd_magnitudes_[index];
        shift_lb += rd_lbs_[index] * m;
        shift_ub += rd_ubs_[index] * m;
        rd_vars_.push_back(lin.vars(index));
        rd_coeffs_.push_back(lin.coeffs(index));
      }
      LinearConstraintProto* mut_lin = ct->mutable_linear();
      mut_lin->mutable_vars()->Assign(rd_vars_.begin(), rd_vars_.end());
      mut_lin->mutable_coeffs()->Assign(rd_coeffs_.begin(), rd_coeffs_.end());

      // The constraint become:
      //   sum ci (X - lb) <= rhs_ub
      //   sum ci (ub - X) <= rhs_lb
      //   sum ci ub - rhs_lb <= sum ci X <= rhs_ub + sum ci lb.
      const int64_t new_rhs_lb =
          use_lb ? shift_ub - lb_feasible_.CurrentMax() : shift_lb;
      const int64_t new_rhs_ub =
          use_ub ? shift_lb + ub_feasible_.CurrentMax() : shift_ub;
      if (new_rhs_lb > new_rhs_ub) {
        (void)MarkConstraintAsFalse(ct, "linear: false after simplification");
        context_->UpdateConstraintVariableUsage(c);
        return;
      }
      FillDomainInProto(Domain(new_rhs_lb, new_rhs_ub), mut_lin);
      DivideLinearByGcd(ct);
      context_->UpdateConstraintVariableUsage(c);
      return;
    }
  }

  if (gcd > 1) {
    // This might happen as a result of extra reduction after we already tried
    // this reduction.
    if (DivideLinearByGcd(ct)) {
      context_->UpdateConstraintVariableUsage(c);
    }
    return;
  }

  // We didn't remove any irrelevant part, but we might be able to tighten
  // the constraint bound.
  if ((use_lb && lb_feasible_.CurrentMax() < lb_feasible_.Bound()) ||
      (use_ub && ub_feasible_.CurrentMax() < ub_feasible_.Bound())) {
    context_->UpdateRuleStats("linear: reduce rhs with DP");
    const int64_t new_rhs_lb =
        use_lb ? ub_sum - lb_feasible_.CurrentMax() : lb_sum;
    const int64_t new_rhs_ub =
        use_ub ? lb_sum + ub_feasible_.CurrentMax() : ub_sum;
    if (new_rhs_lb > new_rhs_ub) {
      (void)MarkConstraintAsFalse(ct, "linear: reduce rhs with DP");
      context_->UpdateConstraintVariableUsage(c);
      return;
    }
    FillDomainInProto(Domain(new_rhs_lb, new_rhs_ub), ct->mutable_linear());
  }

  // Limit the number of "divisor" we try for approximate gcd.
  if (rd_divisors_.size() > 3) rd_divisors_.resize(3);
  for (const int64_t divisor : rd_divisors_) {
    // Try the <= side first.
    int64_t new_ub;
    if (!LinearInequalityCanBeReducedWithClosestMultiple(
            divisor, rd_magnitudes_, rd_lbs_, rd_ubs_, rhs.Max(), &new_ub)) {
      continue;
    }

    // The other side.
    int64_t minus_new_lb;
    for (int i = 0; i < rd_lbs_.size(); ++i) {
      std::swap(rd_lbs_[i], rd_ubs_[i]);
      rd_lbs_[i] = -rd_lbs_[i];
      rd_ubs_[i] = -rd_ubs_[i];
    }
    if (!LinearInequalityCanBeReducedWithClosestMultiple(
            divisor, rd_magnitudes_, rd_lbs_, rd_ubs_, -rhs.Min(),
            &minus_new_lb)) {
      for (int i = 0; i < rd_lbs_.size(); ++i) {
        std::swap(rd_lbs_[i], rd_ubs_[i]);
        rd_lbs_[i] = -rd_lbs_[i];
        rd_ubs_[i] = -rd_ubs_[i];
      }
      continue;
    }

    // Rewrite the constraint !
    context_->UpdateRuleStats("linear: simplify using approximate gcd");
    int new_size = 0;
    LinearConstraintProto* mutable_linear = ct->mutable_linear();
    for (int i = 0; i < lin.coeffs().size(); ++i) {
      const int64_t new_coeff =
          ClosestMultiple(lin.coeffs(i), divisor) / divisor;
      if (new_coeff == 0) continue;
      mutable_linear->set_vars(new_size, lin.vars(i));
      mutable_linear->set_coeffs(new_size, new_coeff);
      ++new_size;
    }
    mutable_linear->mutable_vars()->Truncate(new_size);
    mutable_linear->mutable_coeffs()->Truncate(new_size);
    const Domain new_rhs = Domain(-minus_new_lb, new_ub);
    if (new_rhs.IsEmpty()) {
      (void)MarkConstraintAsFalse(ct, "linear: false after approximate gcd");
    } else {
      FillDomainInProto(new_rhs, mutable_linear);
    }
    context_->UpdateConstraintVariableUsage(c);
    return;
  }
}

namespace {

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

}  // namespace

bool CpConstraintPresolver::PropagateDomainsInLinear(int ct_index,
                                                     ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (context_->ModelIsUnsat()) return false;

  // For fast mode.
  int64_t min_activity;
  int64_t max_activity;

  // For slow mode.
  const int num_vars = ct->linear().vars_size();
  auto& term_domains = context_->tmp_term_domains;
  auto& left_domains = context_->tmp_left_domains;
  const bool slow_mode = num_vars < 10;

  // Compute the implied rhs bounds from the variable ones.
  if (slow_mode) {
    term_domains.resize(num_vars + 1);
    left_domains.resize(num_vars + 1);
    left_domains[0] = Domain(0);
    term_domains[num_vars] = Domain(0);
    for (int i = 0; i < num_vars; ++i) {
      const int var = ct->linear().vars(i);
      const int64_t coeff = ct->linear().coeffs(i);
      DCHECK(RefIsPositive(var));
      term_domains[i] = context_->DomainOf(var).MultiplicationBy(coeff);
      left_domains[i + 1] =
          left_domains[i].AdditionWith(term_domains[i]).RelaxIfTooComplex();
    }
  } else {
    std::tie(min_activity, max_activity) =
        context_->ComputeMinMaxActivity(ct->linear());
  }
  const Domain& implied_rhs =
      slow_mode ? left_domains[num_vars] : Domain(min_activity, max_activity);

  // Abort if trivial.
  const Domain old_rhs = ReadDomainFromProto(ct->linear());
  if (implied_rhs.IsIncludedIn(old_rhs)) {
    if (ct_index != -1) context_->UpdateRuleStats("linear: always true");
    return RemoveConstraint(ct);
  }

  // Incorporate the implied rhs information.
  Domain rhs = old_rhs.SimplifyUsingImpliedDomain(implied_rhs);
  if (rhs.IsEmpty()) {
    return MarkConstraintAsFalse(ct, "linear: infeasible");
  }
  if (rhs != old_rhs) {
    if (ct_index != -1) context_->UpdateRuleStats("linear: simplified rhs");
    FillDomainInProto(rhs, ct->mutable_linear());
  }

  if (ct_index >= 0 && num_vars > 1 && rhs.IsFixed()) {
    BoolArgumentProto* bool_and = nullptr;
    if (rhs.FixedValue() == implied_rhs.Min()) {
      if (ct->enforcement_literal().empty()) {
        context_->UpdateRuleStats("linear: all fixed to min");
        for (int i = 0; i < num_vars; ++i) {
          const int var = ct->linear().vars(i);
          const int64_t coeff = ct->linear().coeffs(i);
          const int64_t value =
              coeff > 0 ? context_->MinOf(var) : context_->MaxOf(var);
          if (!context_->IntersectDomainWith(var, Domain(value))) return false;
        }
      } else {
        // Replace by linear1 constraints.
        context_->UpdateRuleStats(
            "linear: all fixed to min in enforced constraint");
        for (int i = 0; i < num_vars; ++i) {
          const int var = ct->linear().vars(i);
          const int64_t coeff = ct->linear().coeffs(i);
          const int64_t value =
              coeff > 0 ? context_->MinOf(var) : context_->MaxOf(var);
          if (context_->CanBeUsedAsLiteral(var)) {
            if (bool_and == nullptr) {
              bool_and =
                  context_->AddEnforcedConstraint(ct)->mutable_bool_and();
            }
            bool_and->add_literals(value == 1 ? var : NegatedRef(var));
            continue;
          }
          ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
          new_ct->mutable_linear()->add_vars(var);
          new_ct->mutable_linear()->add_coeffs(1);
          new_ct->mutable_linear()->add_domain(value);
          new_ct->mutable_linear()->add_domain(value);
        }
      }
      return RemoveConstraint(ct);
    } else if (rhs.FixedValue() == implied_rhs.Max()) {
      if (ct->enforcement_literal().empty()) {
        context_->UpdateRuleStats("linear: all fixed to max");
        for (int i = 0; i < num_vars; ++i) {
          const int var = ct->linear().vars(i);
          const int64_t coeff = ct->linear().coeffs(i);
          const int64_t value =
              coeff > 0 ? context_->MaxOf(var) : context_->MinOf(var);
          if (!context_->IntersectDomainWith(var, Domain(value))) return false;
        }
      } else {
        // Replace by linear1 constraints.
        context_->UpdateRuleStats(
            "linear: all fixed to max in enforced constraint");
        for (int i = 0; i < num_vars; ++i) {
          const int var = ct->linear().vars(i);
          const int64_t coeff = ct->linear().coeffs(i);
          const int64_t value =
              coeff > 0 ? context_->MaxOf(var) : context_->MinOf(var);
          if (context_->CanBeUsedAsLiteral(var)) {
            if (bool_and == nullptr) {
              bool_and =
                  context_->AddEnforcedConstraint(ct)->mutable_bool_and();
            }
            bool_and->add_literals(value == 1 ? var : NegatedRef(var));
            continue;
          }
          ConstraintProto* new_ct = context_->AddEnforcedConstraint(ct);
          new_ct->mutable_linear()->add_vars(var);
          new_ct->mutable_linear()->add_coeffs(1);
          new_ct->mutable_linear()->add_domain(value);
          new_ct->mutable_linear()->add_domain(value);
        }
      }
      return RemoveConstraint(ct);
    }
  }

  // Propagate the variable bounds.
  if (ct->enforcement_literal().size() > 1) return false;

  bool new_bounds = false;
  bool recanonicalize = false;
  Domain negated_rhs = rhs.Negation();
  Domain right_domain(0);
  Domain new_domain;
  Domain activity_minus_term;
  for (int i = num_vars - 1; i >= 0; --i) {
    const int var = ct->linear().vars(i);
    const int64_t var_coeff = ct->linear().coeffs(i);

    if (slow_mode) {
      right_domain =
          right_domain.AdditionWith(term_domains[i + 1]).RelaxIfTooComplex();
      activity_minus_term = left_domains[i].AdditionWith(right_domain);
    } else {
      int64_t min_term = var_coeff * context_->MinOf(var);
      int64_t max_term = var_coeff * context_->MaxOf(var);
      if (var_coeff < 0) std::swap(min_term, max_term);
      activity_minus_term =
          Domain(min_activity - min_term, max_activity - max_term);
    }
    new_domain = activity_minus_term.AdditionWith(negated_rhs)
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
    if (ct_index == -1) continue;
    if (!ct->enforcement_literal().empty()) continue;

    // Given a variable that only appear in one constraint and in the
    // objective, for any feasible solution, it will be always better to move
    // this singleton variable as much as possible towards its good objective
    // direction. Sometime, we can detect that we will always be able to
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
                                          activity_minus_term, rhs)) {
        rhs = Domain(rhs.Min());
        fixed = true;
      }
      if (!same_sign && RhsCanBeFixedToMax(var_coeff, context_->DomainOf(var),
                                           activity_minus_term, rhs)) {
        rhs = Domain(rhs.Max());
        fixed = true;
      }
      if (fixed) {
        context_->UpdateRuleStats("linear: tightened into equality");
        // Compute a new `var` hint so that the lhs of `ct` is equal to `rhs`.
        solution_crush_.SetVarToLinearConstraintSolution(
            /*enforcement_lits=*/{}, i, ct->linear().vars(),
            /*default_values=*/{}, ct->linear().coeffs(), rhs.FixedValue());
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
    // we substitute only equalities, we can deal with inequalities of size 2
    // here.
    if (ct->linear().vars().size() <= 2) continue;

    // TODO(user): We actually do not need a strict equality when
    // keep_all_feasible_solutions is false, but that simplifies things as the
    // SubstituteVariable() function cannot fail this way.
    if (rhs.Min() != rhs.Max()) continue;

    // NOTE: The mapping doesn't allow us to remove a variable if
    // keep_all_feasible_solutions is true.
    //
    // TODO(user): This shouldn't be necessary, but caused some failure on
    // IntModExpandTest.FzTest. Fix.
    if (context_->params().keep_all_feasible_solutions_in_presolve()) continue;

    // Only consider "implied free" variables. Note that the coefficient of
    // magnitude 1 is important otherwise we can't easily remove the
    // constraint since the fact that the sum of the other terms must be a
    // multiple of coeff will not be enforced anymore.
    if (std::abs(var_coeff) != 1) continue;
    if (context_->params().presolve_substitution_level() <= 0) continue;

    // Only consider substitution that reduce the number of entries.
    const bool is_in_objective =
        context_->VarToConstraints(var).contains(kObjectiveConstraint);
    {
      int col_size = context_->VarToConstraints(var).size();
      if (is_in_objective) col_size--;
      const int row_size = ct->linear().vars_size();

      // This is actually an upper bound on the number of entries added since
      // some of them might already be present.
      const int num_entries_added = (row_size - 1) * (col_size - 1);
      const int num_entries_removed = col_size + row_size - 1;
      if (num_entries_added > num_entries_removed) continue;
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
      if (context_->Constraint(c).constraint_case() !=
          ConstraintProto::kLinear) {
        abort = true;
        break;
      }
      for (const int ref : context_->Constraint(c).enforcement_literal()) {
        if (PositiveRef(ref) == var) {
          abort = true;
          break;
        }
      }
      if (abort) break;
      others.push_back(c);
    }
    if (abort) continue;

    // If the domain implied by this constraint is the same as the current
    // domain of the variable, this variable is implied free. Otherwise, we
    // check if the intersection with the domain implied by another constraint
    // makes it implied free.
    if (context_->DomainOf(var) != new_domain) {
      // We only do that for doubleton because we don't want the propagation to
      // be less strong. If we were to replace this variable in other constraint
      // the implied bound from the linear expression might not be as good.
      //
      // TODO(user): We still substitute even if this happens in the objective
      // though. Is that good?
      if (others.size() != 1) continue;
      const ConstraintProto& other_ct = context_->Constraint(others.front());
      if (!other_ct.enforcement_literal().empty()) continue;

      // Compute the implied domain using the other constraint.
      // We only do that if it is not too long to avoid quadratic worst case.
      const LinearConstraintProto& other_lin = other_ct.linear();
      if (other_lin.vars().size() > 100) continue;
      Domain implied = ReadDomainFromProto(other_lin);
      int64_t other_coeff = 0;
      for (int i = 0; i < other_lin.vars().size(); ++i) {
        const int v = other_lin.vars(i);
        const int64_t coeff = other_lin.coeffs(i);
        if (v == var) {
          // It is possible the constraint is not canonical if it wasn't
          // processed yet !
          other_coeff += coeff;
        } else {
          implied =
              implied
                  .AdditionWith(context_->DomainOf(v).MultiplicationBy(-coeff))
                  .RelaxIfTooComplex();
        }
      }
      if (other_coeff == 0) continue;
      implied = implied.InverseMultiplicationBy(other_coeff);

      // Since we compute it, we can as well update the domain right now.
      // This is also needed for postsolve to have a tight domain.
      if (!context_->IntersectDomainWith(var, implied)) return false;
      if (context_->IsFixed(var)) continue;
      if (new_domain.IntersectionWith(implied) != context_->DomainOf(var)) {
        continue;
      }

      context_->UpdateRuleStats("linear: doubleton free");
    }

    // Substitute in objective.
    // This can fail in overflow corner cases, so we abort before doing any
    // actual changes.
    if (is_in_objective &&
        !context_->SubstituteVariableInObjective(var, var_coeff, *ct)) {
      if (context_->ModelIsUnsat()) return false;
      continue;
    }

    // Do the actual substitution.
    ConstraintProto copy_if_we_abort;
    absl::c_sort(others);
    for (const int c : others) {
      // TODO(user): The copy is needed to have a simpler overflow-checking
      // code were we check once the substitution is done. If needed we could
      // optimize that, but with more code.
      copy_if_we_abort = context_->Constraint(c);

      // In some corner cases, this might violate our overflow precondition or
      // even create an overflow. The danger is limited since the range of the
      // linear expression used in the definition do not exceed the domain of
      // the variable we substitute. But this is not the case for the doubleton
      // case above.
      if (!SubstituteVariable(var, var_coeff, *ct,
                              context_->MutableConstraint(c))) {
        // The function above can fail because of overflow, but also if the
        // constraint was not canonicalized yet and the variable is actually not
        // there (we have var - var for instance).
        //
        // TODO(user): we canonicalize it right away, but I am not sure it is
        // really needed.
        bool changed = false;
        if (!CanonicalizeLinear(context_->MutableConstraint(c), &changed)) {
          return true;
        }
        if (changed) {
          context_->UpdateConstraintVariableUsage(c);
        }
        abort = true;
        break;
      }

      if (PossibleIntegerOverflow(context_->WorkingModel(),
                                  context_->Constraint(c).linear().vars(),
                                  context_->Constraint(c).linear().coeffs())) {
        // Revert the change in this case.
        *context_->MutableConstraint(c) = copy_if_we_abort;
        abort = true;
        break;
      }

      // TODO(user): We should re-enqueue these constraints for presolve.
      context_->UpdateConstraintVariableUsage(c);
    }
    if (abort) continue;

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
    ConstraintProto* mapping_ct =
        context_->NewMappingConstraint(__FILE__, __LINE__);
    *mapping_ct = *ct;
    LinearConstraintProto* mapping_linear_ct = mapping_ct->mutable_linear();
    std::swap(mapping_linear_ct->mutable_vars()->at(0),
              mapping_linear_ct->mutable_vars()->at(i));
    std::swap(mapping_linear_ct->mutable_coeffs()->at(0),
              mapping_linear_ct->mutable_coeffs()->at(i));
    return RemoveConstraint(ct);
  }

  // special case.
  if (ct_index == -1) {
    if (new_bounds) {
      context_->UpdateRuleStats(
          "linear: reduced variable domains in derived constraint");
    }
    return false;
  }

  if (new_bounds) {
    context_->UpdateRuleStats("linear: reduced variable domains");
  }
  if (recanonicalize) {
    bool changed = false;
    (void)CanonicalizeLinear(ct, &changed);
    return changed;
  }
  return false;
}

// The constraint from its lower value is sum positive_coeff * X <= rhs.
// If from_lower_bound is false, then it is the constraint from its upper value.
void CpConstraintPresolver::LowerThanCoeffStrengthening(bool from_lower_bound,
                                                        int64_t min_magnitude,
                                                        int64_t rhs,
                                                        ConstraintProto* ct) {
  const LinearConstraintProto& arg = ct->linear();
  const int64_t second_threshold = rhs - min_magnitude;
  const int num_vars = arg.vars_size();

  // Special case:
  // - The terms above rhs must be fixed to zero.
  // - The terms in (second_threshold, rhs] can be fixed to rhs as
  //   they will force all other terms to zero if not at zero themselves.
  // - If what is left can be simplified to a single coefficient, we can
  //   put the constraint into a special form.
  //
  // TODO(user): More generally, if we ignore term that set everything else to
  // zero, we can preprocess the constraint left and then add them back. So we
  // can do all our other reduction like normal GCD or more advanced ones like
  // DP based or approximate GCD.
  if (min_magnitude <= second_threshold) {
    // Compute max_magnitude for the term <= second_threshold.
    int64_t max_magnitude_left = 0;
    int64_t max_activity_left = 0;
    int64_t activity_when_coeff_are_one = 0;
    int64_t gcd = 0;
    for (int i = 0; i < num_vars; ++i) {
      const int64_t magnitude = std::abs(arg.coeffs(i));
      if (magnitude <= second_threshold) {
        gcd = std::gcd(gcd, magnitude);
        max_magnitude_left = std::max(max_magnitude_left, magnitude);
        const int64_t bound_diff =
            context_->MaxOf(arg.vars(i)) - context_->MinOf(arg.vars(i));
        activity_when_coeff_are_one += bound_diff;
        max_activity_left += magnitude * bound_diff;
      }
    }
    CHECK_GT(min_magnitude, 0);
    CHECK_LE(min_magnitude, max_magnitude_left);

    // Not considering the variable that set everyone at zero when true:
    int64_t new_rhs = 0;
    bool set_all_to_one = false;
    if (max_activity_left <= rhs) {
      // We are left with a trivial constraint.
      context_->UpdateRuleStats("linear with partial amo: trivial");
      new_rhs = activity_when_coeff_are_one;
      set_all_to_one = true;
    } else if (rhs / min_magnitude == rhs / max_magnitude_left) {
      // We are left with a sum <= new_rhs constraint.
      context_->UpdateRuleStats("linear with partial amo: constant coeff");
      new_rhs = rhs / min_magnitude;
      set_all_to_one = true;
    } else if (gcd > 1) {
      // We are left with a constraint that can be simplified by gcd.
      context_->UpdateRuleStats("linear with partial amo: gcd");
      new_rhs = rhs / gcd;
    }

    if (new_rhs > 0) {
      int64_t rhs_offset = 0;
      for (int i = 0; i < num_vars; ++i) {
        const int ref = arg.vars(i);
        const int64_t coeff = from_lower_bound ? arg.coeffs(i) : -arg.coeffs(i);

        int64_t new_coeff;
        const int64_t magnitude = std::abs(coeff);
        if (magnitude > rhs) {
          new_coeff = new_rhs + 1;
        } else if (magnitude > second_threshold) {
          new_coeff = new_rhs;
        } else {
          new_coeff = set_all_to_one ? 1 : magnitude / gcd;
        }

        // In the transformed domain we will always have
        // magnitude * (var - lb) or magnitude * (ub - var)
        if (coeff > 0) {
          ct->mutable_linear()->set_coeffs(i, new_coeff);
          rhs_offset += new_coeff * context_->MinOf(ref);
        } else {
          ct->mutable_linear()->set_coeffs(i, -new_coeff);
          rhs_offset -= new_coeff * context_->MaxOf(ref);
        }
      }
      FillDomainInProto(Domain(rhs_offset, new_rhs + rhs_offset),
                        ct->mutable_linear());
      return;
    }
  }

  int64_t rhs_offset = 0;
  for (int i = 0; i < num_vars; ++i) {
    int ref = arg.vars(i);
    int64_t coeff = arg.coeffs(i);
    if (coeff < 0) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }

    if (coeff > rhs) {
      if (ct->enforcement_literal().empty()) {
        // Shifted variable must be zero.
        //
        // TODO(user): Note that here IntersectDomainWith() can only return
        // false if for some reason this variable has an affine representative
        // for which this fail. Ideally we should always replace/merge
        // representative right away, but this is a bit difficult to enforce
        // currently.
        context_->UpdateRuleStats("linear: fix variable to its bound");
        if (!context_->IntersectDomainWith(
                ref, Domain(from_lower_bound ? context_->MinOf(ref)
                                             : context_->MaxOf(ref)))) {
          return;
        }
      }

      // TODO(user): What to do with the coeff if there is enforcement?
      continue;
    }
    if (coeff > second_threshold && coeff < rhs) {
      context_->UpdateRuleStats(
          "linear: coefficient strengthening by increasing it");
      if (from_lower_bound) {
        // coeff * (X - LB + LB) -> rhs * (X - LB) + coeff * LB
        rhs_offset -= (coeff - rhs) * context_->MinOf(ref);
      } else {
        // coeff * (X - UB + UB) -> rhs * (X - UB) + coeff * UB
        rhs_offset -= (coeff - rhs) * context_->MaxOf(ref);
      }
      ct->mutable_linear()->set_coeffs(i, arg.coeffs(i) > 0 ? rhs : -rhs);
    }
  }
  if (rhs_offset != 0) {
    FillDomainInProto(ReadDomainFromProto(arg).AdditionWith(Domain(rhs_offset)),
                      ct->mutable_linear());
  }
}

// Identify Boolean variable that makes the constraint always true when set to
// true or false. Moves such literal to the constraint enforcement literals
// list.
//
// We also generalize this to integer variable at one of their bound.
//
// This operation is similar to coefficient strengthening in the MIP world.
void CpConstraintPresolver::ExtractEnforcementLiteralFromLinearConstraint(
    int ct_index, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return;
  if (context_->ModelIsUnsat()) return;

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();

  // No need to process size one constraints, they will be presolved separately.
  // We also do not want to split them in two.
  if (num_vars <= 1) return;

  int64_t min_sum = 0;
  int64_t max_sum = 0;
  int64_t max_coeff_magnitude = 0;
  int64_t min_coeff_magnitude = kint64max;
  for (int i = 0; i < num_vars; ++i) {
    const int ref = arg.vars(i);
    const int64_t coeff = arg.coeffs(i);
    if (coeff > 0) {
      max_coeff_magnitude = std::max(max_coeff_magnitude, coeff);
      min_coeff_magnitude = std::min(min_coeff_magnitude, coeff);
      min_sum += coeff * context_->MinOf(ref);
      max_sum += coeff * context_->MaxOf(ref);
    } else {
      max_coeff_magnitude = std::max(max_coeff_magnitude, -coeff);
      min_coeff_magnitude = std::min(min_coeff_magnitude, -coeff);
      min_sum += coeff * context_->MaxOf(ref);
      max_sum += coeff * context_->MinOf(ref);
    }
  }
  if (max_coeff_magnitude == 1) return;

  // We can only extract enforcement literals if the maximum coefficient
  // magnitude is large enough. Note that we handle complex domain.
  //
  // TODO(user): Depending on how we split below, the threshold are not the
  // same. This is maybe not too important, we just don't split as often as we
  // could, but it is still unclear if splitting is good.
  const auto& domain = ct->linear().domain();
  const int64_t ub_threshold = domain[domain.size() - 2] - min_sum;
  const int64_t lb_threshold = max_sum - domain[1];
  if (max_coeff_magnitude + min_coeff_magnitude <
      std::max(ub_threshold, lb_threshold)) {
    // We also have other kind of coefficient strengthening.
    // In something like 3x + 5y <= 6, the coefficient 5 can be changed to 6.
    // And in 5x + 12y <= 12, the coeff 5 can be changed to 6 (not sure how to
    // generalize this one).
    if (domain.size() == 2 && min_coeff_magnitude > 1 &&
        min_coeff_magnitude < max_coeff_magnitude) {
      const int64_t rhs_min = domain[0];
      const int64_t rhs_max = domain[1];
      if (min_sum >= rhs_min &&
          max_coeff_magnitude + min_coeff_magnitude > rhs_max - min_sum) {
        LowerThanCoeffStrengthening(/*from_lower_bound=*/true,
                                    min_coeff_magnitude, rhs_max - min_sum, ct);
        return;
      }
      if (max_sum <= rhs_max &&
          max_coeff_magnitude + min_coeff_magnitude > max_sum - rhs_min) {
        LowerThanCoeffStrengthening(/*from_lower_bound=*/false,
                                    min_coeff_magnitude, max_sum - rhs_min, ct);
        return;
      }
    }
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
  const Domain rhs_domain = ReadDomainFromProto(ct->linear());
  const bool lower_bounded = min_sum < rhs_domain.Min();
  const bool upper_bounded = max_sum > rhs_domain.Max();
  if (!lower_bounded && !upper_bounded) return;
  if (lower_bounded && upper_bounded) {
    // We disable this for now.
    if (true) return;

    // Lets not split except if we extract enforcement.
    if (max_coeff_magnitude < std::max(ub_threshold, lb_threshold)) return;

    context_->UpdateRuleStats("linear: split boxed constraint");
    ConstraintProto* new_ct1 = context_->AddConstraint();
    *new_ct1 = *ct;
    if (!ct->name().empty()) {
      new_ct1->set_name(absl::StrCat(ct->name(), " (part 1)"));
    }
    FillDomainInProto(Domain(min_sum, rhs_domain.Max()),
                      new_ct1->mutable_linear());

    ConstraintProto* new_ct2 = context_->AddConstraint();
    *new_ct2 = *ct;
    if (!ct->name().empty()) {
      new_ct2->set_name(absl::StrCat(ct->name(), " (part 2)"));
    }
    FillDomainInProto(rhs_domain.UnionWith(Domain(rhs_domain.Max(), max_sum)),
                      new_ct2->mutable_linear());

    ct->Clear();
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  }

  // Any coefficient greater than this will cause the constraint to be trivially
  // satisfied when the variable move away from its bound. Note that as we
  // remove coefficient, the threshold do not change!
  const int64_t threshold = lower_bounded ? ub_threshold : lb_threshold;

  // All coeffs in [second_threshold, threshold) can be reduced to
  // second_threshold.
  //
  // TODO(user): If 2 * min_coeff_magnitude >= bound, then the constraint can
  // be completely rewritten to 2 * (enforcement_part) + sum var >= 2 which is
  // what happens eventually when bound is even, but not if it is odd currently.
  int64_t second_threshold =
      std::max(MathUtil::CeilOfRatio(threshold, int64_t{2}),
               threshold - min_coeff_magnitude);

  // Tricky: The second threshold only work if the domain is simple. If the
  // domain has holes, changing the coefficient might change whether the
  // variable can be at one or not by herself.
  //
  // TODO(user): We could still reduce it to the smaller value with same
  // feasibility.
  if (rhs_domain.NumIntervals() > 1) {
    second_threshold = threshold;  // Disable.
  }

  // Do we only extract Booleans?
  //
  // Note that for now the default is false, and also there are problem calling
  // GetOrCreateVarValueEncoding() after expansion because we might have removed
  // the variable used in the encoding.
  const bool only_extract_booleans =
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

    // TODO(user): If the encoding Boolean already exist, we could extract
    // the non-Boolean enforcement term.
    const bool is_boolean = context_->CanBeUsedAsLiteral(ref);
    if (context_->IsFixed(ref) || coeff < threshold ||
        (only_extract_booleans && !is_boolean)) {
      mutable_arg->set_vars(new_size, mutable_arg->vars(i));

      int64_t new_magnitude = std::abs(arg.coeffs(i));
      if (coeff > threshold) {
        // We keep this term but reduce its coeff.
        // This is only for the case where only_extract_booleans == true.
        new_magnitude = threshold;
        context_->UpdateRuleStats("linear: coefficient strengthening");
      } else if (coeff > second_threshold && coeff < threshold) {
        // This covers the special case where one big + one small is enough
        // to satisfy the constraint, we can reduce the big.
        new_magnitude = second_threshold;
        context_->UpdateRuleStats("linear: advanced coefficient strengthening");
      }
      if (coeff != new_magnitude) {
        if (lower_bounded) {
          // coeff * (X - LB + LB) -> new_magnitude * (X - LB) + coeff * LB
          rhs_offset -= (coeff - new_magnitude) * context_->MinOf(ref);
        } else {
          // coeff * (X - UB + UB) -> new_magnitude * (X - UB) + coeff * UB
          rhs_offset -= (coeff - new_magnitude) * context_->MaxOf(ref);
        }
      }

      mutable_arg->set_coeffs(
          new_size, arg.coeffs(i) > 0 ? new_magnitude : -new_magnitude);
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
  if (some_integer_encoding_were_extracted || new_size == 1) {
    context_->UpdateConstraintVariableUsage(ct_index);
  }
}

// Convert some linear constraint involving only Booleans to their Boolean
// form.
bool CpConstraintPresolver::PresolveLinearOnBooleans(ConstraintProto* ct) {
  if (ct->linear().vars().empty()) return false;
  if (context_->ModelIsUnsat()) return false;

  // For special kind of constraint detection.
  int64_t sum_of_coeffs = 0;
  int num_positive = 0;
  int num_negative = 0;

  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64_t min_coeff = kint64max;
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

    sum_of_coeffs += coeff;
    if (coeff > 0) {
      ++num_positive;
      max_sum += coeff;
      min_coeff = std::min(min_coeff, coeff);
      max_coeff = std::max(max_coeff, coeff);
    } else {
      // We replace the Boolean ref, by a ref to its negation (1 - x).
      ++num_negative;
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
    return MarkConstraintAsFalse(ct,
                                 "linear: all booleans and trivially false");
  }
  if (Domain(min_sum, max_sum).IsIncludedIn(rhs_domain)) {
    context_->UpdateRuleStats("linear: all booleans and trivially true");
    return RemoveConstraint(ct);
  }

  // This discover cases like "A + B + C - 3*D = 0"
  // where all Booleans must be equivalent!
  // This happens a lot on woodlands09.mps for instance.
  //
  // TODO(user): generalize if enforced?
  // TODO(user): generalize to other variant! Use DP to identify constraint with
  // just one or two solutions? or a few solution with same variable values?
  if (ct->enforcement_literal().empty() && sum_of_coeffs == 0 &&
      (num_negative == 1 || num_positive == 1) && rhs_domain.IsFixed() &&
      rhs_domain.FixedValue() == 0) {
    // This forces either all variable at 1 or all at zero.
    context_->UpdateRuleStats("linear: all equivalent!");
    for (int i = 1; i < num_vars; ++i) {
      if (!context_->StoreBooleanEqualityRelation(ct->linear().vars(0),
                                                  ct->linear().vars(i))) {
        return false;
      }
    }
    return RemoveConstraint(ct);
  }

  // Detect clauses, reified ands, at_most_one.
  //
  // TODO(user): split a == 1 constraint or similar into a clause and an at
  // most one constraint?
  DCHECK(!rhs_domain.IsEmpty());
  if (min_sum + min_coeff > rhs_domain.Max()) {
    // All Booleans are false if the reified literal is true.
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
    // All Booleans are true if the reified literal is true.
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
    // TODO(user): Support enforced at most one.
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
    // TODO(user): Support enforced at most one.
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
    // TODO(user): Support enforced exactly one.
    context_->UpdateRuleStats("linear: positive equal one");
    ConstraintProto* exactly_one = context_->AddConstraint();
    exactly_one->set_name(ct->name());
    for (int i = 0; i < num_vars; ++i) {
      exactly_one->mutable_exactly_one()->add_literals(
          arg.coeffs(i) > 0 ? arg.vars(i) : NegatedRef(arg.vars(i)));
    }
    return RemoveConstraint(ct);
  } else if (!HasEnforcementLiteral(*ct) && rhs_domain.NumIntervals() == 1 &&
             max_sum > rhs_domain.Max() &&
             max_sum - min_coeff <= rhs_domain.Max() &&
             max_sum - 2 * min_coeff < rhs_domain.Min() &&
             max_sum - max_coeff >= rhs_domain.Min()) {
    // TODO(user): Support enforced exactly one.
    context_->UpdateRuleStats("linear: negative equal one");
    ConstraintProto* exactly_one = context_->AddConstraint();
    exactly_one->set_name(ct->name());
    for (int i = 0; i < num_vars; ++i) {
      exactly_one->mutable_exactly_one()->add_literals(
          arg.coeffs(i) > 0 ? NegatedRef(arg.vars(i)) : arg.vars(i));
    }
    return RemoveConstraint(ct);
  } else if (num_negative == 0 && rhs_domain.Min() <= 0 &&
             rhs_domain.Max() > 0 && min_coeff > 0 && max_coeff > min_coeff &&
             rhs_domain.Max() / min_coeff == rhs_domain.Max() / max_coeff) {
    // This covers cases like 5X + 4Y + 5Z <= 10
    //
    // TODO(user): Generalize this kind of coeff strengthening to more cases.
    CHECK_EQ(min_sum, 0);
    context_->UpdateRuleStats("linear: reduced all coefficient to one");
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_linear()->set_coeffs(i, 1);
    }
    FillDomainInProto(Domain(0, rhs_domain.Max() / max_coeff),
                      ct->mutable_linear());
  }

  return PresolveSmallLinearOnBooleans(ct);
}

bool CpConstraintPresolver::PresolveSmallLinearOnBooleans(ConstraintProto* ct) {
  const LinearConstraintProto& linear = ct->linear();
  const int num_vars = linear.vars().size();
  const Domain rhs_domain = ReadDomainFromProto(linear);

  if (num_vars <= 6 && ct->enforcement_literal().empty()) {
    const int max_mask = (1 << num_vars);
    int num_solutions = 0;
    int masks[3];
    for (int mask = 0; mask < max_mask; ++mask) {
      int64_t value = 0;
      for (int i = 0; i < num_vars; ++i) {
        if ((mask >> i) & 1) value += linear.coeffs(i);
      }
      if (rhs_domain.Contains(value)) {
        masks[num_solutions++] = mask;
        if (num_solutions >= 3) break;
      }
    }
    if (num_solutions == 0) {
      return MarkConstraintAsFalse(ct, "linear: small linear infeasible");
    } else if (num_solutions == 1) {
      context_->UpdateRuleStats("linear: small linear fixed");
      for (int i = 0; i < num_vars; ++i) {
        const int var = linear.vars(i);
        const int value = (masks[0] >> i) & 1;
        if (!context_->IntersectDomainWith(var, Domain(value))) {
          return false;
        }
      }
      return RemoveConstraint(ct);
    } else if (num_solutions == 2) {
      context_->UpdateRuleStats("linear: small linear fixed or equiv");
      int reference = -1;
      int reference_sol1 = 0;
      for (int i = 0; i < num_vars; ++i) {
        const int var = linear.vars(i);
        CHECK(RefIsPositive(var));
        const int sol1 = (masks[0] >> i) & 1;
        const int sol2 = (masks[1] >> i) & 1;
        if (sol1 == sol2) {
          if (!context_->IntersectDomainWith(var, Domain(sol1))) {
            return false;
          }
        } else if (reference == -1) {
          reference = var;
          reference_sol1 = sol1;
        } else {
          if (!context_->StoreBooleanEqualityRelation(
                  reference, sol1 == reference_sol1 ? var : NegatedRef(var))) {
            return false;
          }
        }
      }
      return RemoveConstraint(ct);
    }
  }

  // Expand small expression into clause.
  //
  // TODO(user): In many cases, we can have a better encoding.
  // This will eventually be recovered, but sometimes we have stuff like
  // 4x - 4y + z <= 1 which really only forbids (x == 1, y == 0) and that is
  // just an implication rather than two clauses. So we could be "faster".
  //
  // TODO(user): This is bad from a LP relaxation perspective.
  // Recover the proper linearization on loading !!
  // On another hand it is good for the SAT presolving.
  if (num_vars > 3) return false;
  context_->UpdateRuleStats("linear: small Boolean expression");

  // Enumerate all possible value of the Booleans and add a clause if constraint
  // is false. TODO(user): the encoding could be made better in some cases.
  const int max_mask = (1 << num_vars);
  for (int mask = 0; mask < max_mask; ++mask) {
    int64_t value = 0;
    for (int i = 0; i < num_vars; ++i) {
      if ((mask >> i) & 1) value += linear.coeffs(i);
    }
    if (rhs_domain.Contains(value)) continue;

    // Add a new clause to exclude this bad assignment.
    BoolArgumentProto* new_clause =
        context_->AddConstraint()->mutable_bool_or();
    for (const int lit : ct->enforcement_literal()) {
      new_clause->add_literals(NegatedRef(lit));
    }
    for (int i = 0; i < num_vars; ++i) {
      new_clause->add_literals(((mask >> i) & 1) ? NegatedRef(linear.vars(i))
                                                 : linear.vars(i));
    }
  }

  return RemoveConstraint(ct);
}

bool CpConstraintPresolver::PresolveInterval(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  IntervalConstraintProto* interval = ct->mutable_interval();

  // If the size is < 0, then the interval cannot be performed.
  if (!ct->enforcement_literal().empty() && context_->SizeMax(c) < 0) {
    context_->UpdateRuleStats("interval: negative size implies unperformed");
    return MarkOptionalIntervalAsFalse(ct);
  }

  if (ct->enforcement_literal().empty()) {
    bool domain_changed = false;
    // Size can't be negative.
    if (!context_->IntersectDomainWith(interval->size(), Domain(0, kint64max),
                                       &domain_changed)) {
      return false;
    }
    if (domain_changed) {
      context_->UpdateRuleStats(
          "interval: performed intervals must have a positive size");
    }
  }

  // Note that the linear relation is stored elsewhere, so it is safe to just
  // remove such special interval constraint.
  if (context_->IntervalUsage(c) == 0) {
    context_->UpdateRuleStats("intervals: removed unused interval");
    return RemoveConstraint(ct);
  }

  bool changed = false;
  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_start());
  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_size());
  changed |= CanonicalizeLinearExpression(*ct, interval->mutable_end());
  return changed;
}

// TODO(user): avoid code duplication between expand and presolve.
bool CpConstraintPresolver::PresolveInverse(ConstraintProto* ct) {
  DCHECK(ct->inverse().f_direct().empty());
  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;
  const int size = ct->inverse().f_expr_direct().size();
  bool changed = false;

  // Make sure the domains are included in [0, size - 1).
  for (const LinearExpressionProto& expr : ct->inverse().f_expr_direct()) {
    if (!context_->IntersectDomainWith(expr, Domain(0, size - 1), &changed)) {
      VLOG(1) << "Empty domain for a variable in PresolveInverse()";
      return false;
    }
  }
  for (const LinearExpressionProto& expr : ct->inverse().f_expr_inverse()) {
    if (!context_->IntersectDomainWith(expr, Domain(0, size - 1), &changed)) {
      VLOG(1) << "Empty domain for a variable in PresolveInverse()";
      return false;
    }
  }

  // Detect duplicated variable.
  // Even with negated variables, the reduced domain in [0..size - 1]
  // implies that the constraint is infeasible if ref and its negation
  // appear together.
  {
    absl::flat_hash_set<int> direct_vars;
    for (const LinearExpressionProto& expr : ct->inverse().f_expr_direct()) {
      DCHECK_LE(expr.vars_size(), 1);
      if (expr.vars().empty()) continue;
      if (abs(expr.coeffs(0)) != 1 || expr.offset() != 0) continue;
      const int var = expr.vars(0);
      DCHECK_GE(var, 0);
      const auto [it, inserted] = direct_vars.insert(var);
      if (!inserted) {
        return context_->NotifyThatModelIsUnsat("inverse: duplicated variable");
      }
    }

    absl::flat_hash_set<int> inverse_vars;
    for (const LinearExpressionProto& expr : ct->inverse().f_expr_inverse()) {
      DCHECK_LE(expr.vars_size(), 1);
      if (expr.vars().empty()) continue;
      if (abs(expr.coeffs(0)) != 1 || expr.offset() != 0) continue;
      const int var = expr.vars(0);
      DCHECK_GE(var, 0);
      const auto [it, inserted] = inverse_vars.insert(var);
      if (!inserted) {
        return context_->NotifyThatModelIsUnsat("inverse: duplicated variable");
      }
    }
  }

  // Propagate from one vector to its counterpart until fix point.
  const auto filter_inverse_domain = [this, size, &changed](const auto& direct,
                                                            const auto& inverse,
                                                            bool& filtered) {
    // Build the set of values in the inverse vector.
    std::vector<absl::flat_hash_set<int64_t>> inverse_values(size);
    for (int i = 0; i < size; ++i) {
      if (inverse[i].vars().empty()) {
        inverse_values[i].insert(inverse[i].offset());
      } else {
        for (const int64_t var_value :
             context_->DomainOf(inverse[i].vars(0)).Values()) {
          const int64_t j =
              var_value * inverse[i].coeffs(0) + inverse[i].offset();
          inverse_values[i].insert(j);
        }
      }
    }

    // Propagate from the inverse vector to the direct vector. Reduce the
    // domains of each variable in the direct vector by checking that the
    // inverse value exists.
    std::vector<int64_t> possible_values;
    for (int i = 0; i < size; ++i) {
      possible_values.clear();
      bool removed_value = false;
      if (direct[i].vars().empty()) {
        const int64_t j = direct[i].offset();
        if (inverse_values[j].contains(i)) {
          possible_values.push_back(j);
        } else {
          removed_value = true;
        }
      } else {
        for (const int64_t var_value :
             context_->DomainOf(direct[i].vars(0)).Values()) {
          const int64_t j =
              var_value * direct[i].coeffs(0) + direct[i].offset();
          if (inverse_values[j].contains(i)) {
            possible_values.push_back(j);
          } else {
            removed_value = true;
          }
        }
      }
      if (removed_value) {
        changed = true;
        filtered = true;
        if (!context_->IntersectDomainWith(
                direct[i], Domain::FromValues(possible_values))) {
          VLOG(1) << "Empty domain for a variable in PresolveInverse()";
          return false;
        }
      }
    }
    return true;
  };

  for (int i = 0; i < 100; ++i) {  // Just to avoid potentially bad cases.
    bool filtered = false;
    if (!filter_inverse_domain(ct->inverse().f_expr_direct(),
                               ct->inverse().f_expr_inverse(), filtered)) {
      return false;
    }

    if (!filter_inverse_domain(ct->inverse().f_expr_inverse(),
                               ct->inverse().f_expr_direct(), filtered)) {
      return false;
    }
    if (!filtered) break;
  }

  if (changed) {
    context_->UpdateRuleStats("inverse: reduce domains");
  }

  return false;
}

bool CpConstraintPresolver::PresolveElement(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (ct->element().exprs().empty()) {
    return MarkConstraintAsFalse(ct, "element: empty array");
  }

  bool changed = false;
  changed |= CanonicalizeLinearExpression(
      *ct, ct->mutable_element()->mutable_linear_index());
  changed |= CanonicalizeLinearExpression(
      *ct, ct->mutable_element()->mutable_linear_target());
  for (int i = 0; i < ct->element().exprs_size(); ++i) {
    changed |= CanonicalizeLinearExpression(
        *ct, ct->mutable_element()->mutable_exprs(i));
  }

  const LinearExpressionProto& index = ct->element().linear_index();
  const LinearExpressionProto& target = ct->element().linear_target();

  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return changed;

  // Reduce index domain from the array size.
  {
    bool index_modified = false;
    if (!context_->IntersectDomainWith(
            index, Domain(0, ct->element().exprs_size() - 1),
            &index_modified)) {
      return false;
    }
    if (index_modified) {
      context_->UpdateRuleStats(
          "element: reduced index domain from array size");
    }
  }

  // Special case if the index is fixed.
  if (context_->IsFixed(index)) {
    const int64_t index_value = context_->FixedValue(index);
    ConstraintProto* new_ct = context_->AddConstraint();
    new_ct->mutable_linear()->add_domain(0);
    new_ct->mutable_linear()->add_domain(0);
    AddLinearExpressionToLinearConstraint(target, 1, new_ct->mutable_linear());
    AddLinearExpressionToLinearConstraint(ct->element().exprs(index_value), -1,
                                          new_ct->mutable_linear());
    bool is_impossible = false;
    context_->CanonicalizeLinearConstraint(new_ct, &is_impossible);
    if (is_impossible) {
      return context_->NotifyThatModelIsUnsat(
          "element: impossible fixed index");
    }
    context_->UpdateRuleStats("element: fixed index");
    return RemoveConstraint(ct);
  }

  // We know index is not fixed.
  const int index_var = index.vars(0);

  {
    // Cleanup the array: if exprs[i] contains index_var, fix its value.
    const Domain& index_var_domain = context_->DomainOf(index_var);
    std::vector<int64_t> reached_indices(ct->element().exprs_size(), false);
    for (const int64_t index_var_value : index_var_domain.Values()) {
      const int64_t index_value =
          AffineExpressionValueAt(index, index_var_value);
      reached_indices[index_value] = true;
      const LinearExpressionProto& expr = ct->element().exprs(index_value);
      if (expr.vars_size() == 1 && expr.vars(0) == index_var) {
        const int64_t expr_value =
            AffineExpressionValueAt(expr, index_var_value);
        ct->mutable_element()->mutable_exprs(index_value)->clear_vars();
        ct->mutable_element()->mutable_exprs(index_value)->clear_coeffs();
        ct->mutable_element()
            ->mutable_exprs(index_value)
            ->set_offset(expr_value);
        changed = true;
        context_->UpdateRuleStats(
            "element: fix expression depending on the index");
      }
    }

    // Clean up the array: clear unreached expressions.
    for (int i = 0; i < ct->element().exprs_size(); ++i) {
      if (!reached_indices[i]) {
        ct->mutable_element()->mutable_exprs(i)->Clear();
        changed = true;
      }
    }
  }

  // Canonicalization and cleanups of the expressions could have messed up the
  // var-constraint graph.
  if (changed) context_->UpdateConstraintVariableUsage(c);

  // Reduces the domain of the index.
  {
    const Domain& index_var_domain = context_->DomainOf(index_var);
    const Domain& target_domain = context_->DomainSuperSetOf(target);
    std::vector<int64_t> possible_index_var_values;
    for (const int64_t index_var_value : index_var_domain.Values()) {
      const int64_t index_value =
          AffineExpressionValueAt(index, index_var_value);
      const LinearExpressionProto& expr = ct->element().exprs(index_value);

      bool is_possible_index;
      if (target.vars_size() == 1 && target.vars(0) == index_var) {
        // The target domain can be reduced if it shares its variable with the
        // index.
        is_possible_index = context_->DomainContains(
            expr, AffineExpressionValueAt(target, index_var_value));
      } else {
        is_possible_index =
            context_->IntersectionOfAffineExprsIsNotEmpty(target, expr);
      }

      if (is_possible_index) {
        possible_index_var_values.push_back(index_var_value);
      } else {
        ct->mutable_element()->mutable_exprs(index_value)->Clear();
        changed = true;
      }
    }
    if (possible_index_var_values.size() < index_var_domain.Size()) {
      if (!context_->IntersectDomainWith(
              index_var, Domain::FromValues(possible_index_var_values))) {
        return true;
      }
      context_->UpdateRuleStats("element: reduced index domain");
      // If the index is fixed, this is an equality constraint.
      if (context_->IsFixed(index)) {
        ConstraintProto* const eq = context_->AddConstraint();
        eq->mutable_linear()->add_domain(0);
        eq->mutable_linear()->add_domain(0);
        AddLinearExpressionToLinearConstraint(target, 1, eq->mutable_linear());
        AddLinearExpressionToLinearConstraint(
            ct->element().exprs(context_->FixedValue(index)), -1,
            eq->mutable_linear());
        context_->CanonicalizeLinearConstraint(eq);
        context_->UpdateRuleStats("element: fixed index");
        return RemoveConstraint(ct);
      }
    }
  }

  bool all_included_in_target_domain = true;
  {
    // Accumulate expressions domains to build a superset of the target domain.
    Domain infered_domain;
    const Domain& index_var_domain = context_->DomainOf(index_var);
    const Domain& target_domain = context_->DomainSuperSetOf(target);
    for (const int64_t index_var_value : index_var_domain.Values()) {
      const int64_t index_value =
          AffineExpressionValueAt(index, index_var_value);
      CHECK_GE(index_value, 0);
      CHECK_LT(index_value, ct->element().exprs_size());
      const LinearExpressionProto& expr = ct->element().exprs(index_value);
      const Domain expr_domain = context_->DomainSuperSetOf(expr);
      if (!expr_domain.IsIncludedIn(target_domain)) {
        all_included_in_target_domain = false;
      }
      infered_domain = infered_domain.UnionWith(expr_domain);
    }

    bool domain_modified = false;
    if (!context_->IntersectDomainWith(target, infered_domain,
                                       &domain_modified)) {
      return true;
    }
    if (domain_modified) {
      context_->UpdateRuleStats("element: reduce target domain");
    }
  }

  bool all_constants = true;
  {
    const Domain& index_var_domain = context_->DomainOf(index_var);
    std::vector<int64_t> expr_constants;

    for (const int64_t index_var_value : index_var_domain.Values()) {
      const int64_t index_value =
          AffineExpressionValueAt(index, index_var_value);
      const LinearExpressionProto& expr = ct->element().exprs(index_value);
      if (context_->IsFixed(expr)) {
        expr_constants.push_back(context_->FixedValue(expr));
      } else {
        all_constants = false;
        break;
      }
    }
  }

  // Detect is the element can be rewritten as a * target + b * index == c.
  if (all_constants) {
    if (context_->IsFixed(target)) {
      // If the target is fixed, the previous steps of domain reduction would
      // have removed all expressions that could not be equal to the target if
      // it was run to a fixpoint. But since we don't run to a fixpoint, we
      // check each expression here.
      const int64_t target_val = context_->FixedValue(target);
      std::vector<int64_t> valid_index_var_values;

      for (const int64_t index_var_value :
           context_->DomainOf(index_var).Values()) {
        const int64_t index_value =
            AffineExpressionValueAt(index, index_var_value);
        if (context_->FixedValue(ct->element().exprs(index_value)) ==
            target_val) {
          valid_index_var_values.push_back(index_var_value);
        }
      }

      if (valid_index_var_values.size() <
          context_->DomainOf(index_var).Size()) {
        if (!context_->IntersectDomainWith(
                index_var, Domain::FromValues(valid_index_var_values))) {
          return true;
        }
      }

      context_->UpdateRuleStats("element: fixed target and constant array");
      return RemoveConstraint(ct);
    }
    int64_t first_index_var_value;
    int64_t first_target_var_value;
    int64_t d_index = 0;
    int64_t d_target = 0;
    int num_terms = 0;
    bool is_affine = true;
    const Domain& index_var_domain = context_->DomainOf(index_var);
    for (const int64_t index_var_value : index_var_domain.Values()) {
      ++num_terms;
      const int64_t index_value =
          AffineExpressionValueAt(index, index_var_value);
      const int64_t expr_value =
          context_->FixedValue(ct->element().exprs(index_value));
      const int64_t target_var_value = GetInnerVarValue(target, expr_value);
      if (num_terms == 1) {
        first_index_var_value = index_var_value;
        first_target_var_value = target_var_value;
      } else if (num_terms == 2) {
        d_index = index_var_value - first_index_var_value;
        d_target = target_var_value - first_target_var_value;
        const int64_t gcd = std::gcd(d_index, d_target);
        d_index /= gcd;
        d_target /= gcd;
      } else {
        const int64_t offset = CapSub(
            CapProd(d_index, CapSub(target_var_value, first_target_var_value)),
            CapProd(d_target, CapSub(index_var_value, first_index_var_value)));
        if (offset != 0) {
          is_affine = false;
          break;
        }
      }
    }
    if (is_affine) {
      const int64_t offset = CapSub(CapProd(first_target_var_value, d_index),
                                    CapProd(first_index_var_value, d_target));
      if (!AtMinOrMaxInt64(offset)) {
        ConstraintProto* const lin = context_->AddConstraint();
        lin->mutable_linear()->add_vars(target.vars(0));
        lin->mutable_linear()->add_coeffs(d_index);
        lin->mutable_linear()->add_vars(index_var);
        lin->mutable_linear()->add_coeffs(-d_target);
        lin->mutable_linear()->add_domain(offset);
        lin->mutable_linear()->add_domain(offset);
        context_->CanonicalizeLinearConstraint(lin);
        context_->UpdateRuleStats("element: rewrite as affine constraint");
        return RemoveConstraint(ct);
      }
    }
  }

  // If a variable (target or index) appears only in this constraint, it does
  // not necessarily mean that we can remove the constraint, as the variable
  // can be used multiple times in the element. So let's count the local
  // uses of each variable.
  //
  // TODO(user): now that we use fixed values for these cases, this is no longer
  // needed I think.
  absl::flat_hash_map<int, int> local_var_occurrence_counter;
  {
    auto count = [&local_var_occurrence_counter](
                     const LinearExpressionProto& expr) mutable {
      for (const int var : expr.vars()) {
        local_var_occurrence_counter[var]++;
      }
    };
    count(index);
    count(target);
    for (const int64_t index_var_value :
         context_->DomainOf(index_var).Values()) {
      count(
          ct->element().exprs(AffineExpressionValueAt(index, index_var_value)));
    }
  }

  if (context_->VariableIsUniqueAndRemovable(index_var) &&
      local_var_occurrence_counter.at(index_var) == 1 && all_constants) {
    // This constraint is just here to reduce the domain of the target! We can
    // add it to the mapping_model to reconstruct the index value during
    // postsolve and get rid of it now.
    //
    // The non constant case is handled during expansion.
    context_->UpdateRuleStats(
        "element: removed as the index is not used elsewhere");
    context_->MarkVariableAsRemoved(index_var);
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    return RemoveConstraint(ct);
  }

  // The case where all domains are not included in the target domain is handled
  // during expansion.
  if (!context_->IsFixed(target) &&
      context_->VariableIsUniqueAndRemovable(target.vars(0)) &&
      local_var_occurrence_counter.at(target.vars(0)) == 1 &&
      all_included_in_target_domain && std::abs(target.coeffs(0)) == 1) {
    context_->UpdateRuleStats(
        "element: removed as the target is not used elsewhere");
    context_->MarkVariableAsRemoved(target.vars(0));
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpConstraintPresolver::PresolveTable(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  bool changed = false;
  for (int i = 0; i < ct->table().exprs_size(); ++i) {
    changed |= CanonicalizeLinearExpression(
        *ct, ct->mutable_table()->mutable_exprs(i));
  }

  const int initial_num_exprs = ct->table().exprs_size();
  if (initial_num_exprs > 0) CanonicalizeTable(context_, ct);
  changed |= (ct->table().exprs_size() != initial_num_exprs);

  if (ct->table().exprs().empty()) {
    context_->UpdateRuleStats("table: no expressions");
    return RemoveConstraint(ct);
  }

  if (ct->table().values().empty()) {
    if (ct->table().negated()) {
      context_->UpdateRuleStats("table: negative table without tuples");
      return RemoveConstraint(ct);
    } else {
      return MarkConstraintAsFalse(ct, "table: positive table without tuples");
    }
  }

  int num_fixed_exprs = 0;
  for (const LinearExpressionProto& expr : ct->table().exprs()) {
    if (context_->IsFixed(expr)) ++num_fixed_exprs;
  }
  if (num_fixed_exprs == ct->table().exprs_size()) {
    context_->UpdateRuleStats("table: all expressions are fixed");
    DCHECK_LE(ct->table().values_size(), num_fixed_exprs);
    if (ct->table().negated() == ct->table().values().empty()) {
      context_->UpdateRuleStats("table: always true");
      return RemoveConstraint(ct);
    } else {
      return MarkConstraintAsFalse(ct, "table: always false");
    }
    return RemoveConstraint(ct);
  }

  if (num_fixed_exprs > 0) {
    CanonicalizeTable(context_, ct);
  }

  // Nothing more to do for negated tables.
  if (ct->table().negated()) return changed;

  // And for constraints with enforcement literals.
  if (HasEnforcementLiteral(*ct)) return changed;

  // Filter the variables domains.
  const int num_exprs = ct->table().exprs_size();
  const int num_tuples = ct->table().values_size() / num_exprs;
  std::vector<std::vector<int64_t>> new_domains(num_exprs);
  for (int e = 0; e < num_exprs; ++e) {
    const LinearExpressionProto& expr = ct->table().exprs(e);
    if (context_->IsFixed(expr)) {
      new_domains[e].push_back(context_->FixedValue(expr));
      continue;
    }

    for (int t = 0; t < num_tuples; ++t) {
      new_domains[e].push_back(ct->table().values(t * num_exprs + e));
    }
    gtl::STLSortAndRemoveDuplicates(&new_domains[e]);
    DCHECK_EQ(1, expr.vars_size());
    DCHECK_EQ(1, expr.coeffs(0));
    DCHECK_EQ(0, expr.offset());
    const int var = expr.vars(0);
    bool domain_modified = false;
    if (!context_->IntersectDomainWith(var, Domain::FromValues(new_domains[e]),
                                       &domain_modified)) {
      return true;
    }
    if (domain_modified) {
      context_->UpdateRuleStats("table: reduce variable domain");
    }
  }

  if (num_exprs == 1) {
    // Now that we have properly updated the domain, we can remove the
    // constraint.
    context_->UpdateRuleStats("table: only one column!");
    return RemoveConstraint(ct);
  }

  // Check that the table is not complete or just here to exclude a few tuples.
  double prod = 1.0;
  for (int e = 0; e < num_exprs; ++e) prod *= new_domains[e].size();
  if (prod == static_cast<double>(num_tuples)) {
    context_->UpdateRuleStats("table: all tuples!");
    return RemoveConstraint(ct);
  }

  // Convert to the negated table if we gain a lot of entries by doing so.
  // Note however that currently the negated table does not propagate as much as
  // it could.
  if (static_cast<double>(num_tuples) > 0.7 * prod) {
    std::vector<std::vector<int64_t>> current_tuples(num_tuples);
    for (int t = 0; t < num_tuples; ++t) {
      current_tuples[t].resize(num_exprs);
      for (int e = 0; e < num_exprs; ++e) {
        current_tuples[t][e] = ct->table().values(t * num_exprs + e);
      }
    }

    // Enumerate all possible tuples.
    std::vector<std::vector<int64_t>> var_to_values(num_exprs);
    for (int e = 0; e < num_exprs; ++e) {
      var_to_values[e].assign(new_domains[e].begin(), new_domains[e].end());
    }
    std::vector<std::vector<int64_t>> all_tuples(prod);
    for (int i = 0; i < prod; ++i) {
      all_tuples[i].resize(num_exprs);
      int index = i;
      for (int j = 0; j < num_exprs; ++j) {
        all_tuples[i][j] = var_to_values[j][index % var_to_values[j].size()];
        index /= var_to_values[j].size();
      }
    }
    gtl::STLSortAndRemoveDuplicates(&all_tuples);

    // Compute the complement of new_tuples.
    std::vector<std::vector<int64_t>> diff(prod - num_tuples);
    std::set_difference(all_tuples.begin(), all_tuples.end(),
                        current_tuples.begin(), current_tuples.end(),
                        diff.begin());

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

namespace {

// A container that is valid if only one value was added.
class UniqueNonNegativeValue {
 public:
  void Add(int value) {
    DCHECK_GE(value, 0);
    if (value_ == -1) {
      value_ = value;
    } else {
      value_ = -2;
    }
  }

  bool HasUniqueValue() const { return value_ >= 0; }

  int64_t value() const {
    DCHECK(HasUniqueValue());
    return value_;
  }

 private:
  int value_ = -1;
};

}  // namespace

bool CpConstraintPresolver::PresolveAllDiff(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;

  AllDifferentConstraintProto& all_diff = *ct->mutable_all_diff();

  bool variables_have_changed = false;
  for (LinearExpressionProto& exp :
       *(ct->mutable_all_diff()->mutable_exprs())) {
    variables_have_changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  const int size = all_diff.exprs_size();
  if (size == 0) {
    context_->UpdateRuleStats("all_diff: empty constraint");
    return RemoveConstraint(ct);
  }
  if (size == 1) {
    context_->UpdateRuleStats("all_diff: one expression");
    return RemoveConstraint(ct);
  }

  absl::flat_hash_set<int64_t> fixed_values;
  int new_size = 0;
  for (int i = 0; i < size; ++i) {
    if (!context_->IsFixed(all_diff.exprs(i))) {
      if (i != new_size) {
        *all_diff.mutable_exprs(new_size) = all_diff.exprs(i);
      }
      ++new_size;
    } else {
      const int64_t value = context_->FixedValue(all_diff.exprs(i));
      if (!fixed_values.insert(value).second) {
        return context_->NotifyThatModelIsUnsat(
            "all_diff: duplicate fixed values");
      }
    }
  }

  if (new_size < size) {
    all_diff.mutable_exprs()->DeleteSubrange(new_size, size - new_size);
    context_->UpdateRuleStats("all_diff: remove fixed expressions");
  }

  if (!fixed_values.empty()) {
    const Domain to_keep =
        Domain::FromValues({fixed_values.begin(), fixed_values.end()})
            .Complement();
    bool propagated = false;
    for (int i = 0; i < all_diff.exprs_size(); ++i) {
      if (!context_->IntersectDomainWith(all_diff.exprs(i), to_keep,
                                         &propagated)) {
        return true;
      }
    }
    if (propagated) {
      context_->UpdateRuleStats("all_diff: propagate fixed expressions");
    }
  }

  // Detect duplicate expressions, and remove impossible values from expressions
  // with the same variable.
  // We use btree_map to have a deterministic order.
  absl::btree_map<int, std::vector<std::pair<int64_t, int64_t>>> terms;
  std::vector<int64_t> forbidden_values;
  for (const LinearExpressionProto& expr : all_diff.exprs()) {
    if (expr.vars_size() != 1) continue;
    terms[expr.vars(0)].push_back(
        std::make_pair(expr.coeffs(0), expr.offset()));
  }
  for (auto& [var, terms] : terms) {
    if (terms.size() == 1) continue;
    std::sort(terms.begin(), terms.end());

    // Check for duplicate expressions.
    for (int i = 1; i < terms.size(); ++i) {
      if (terms[i] == terms[i - 1]) {
        return context_->NotifyThatModelIsUnsat(
            "all_diff: duplicate expressions");
      }
    }

    // Remove impossible values from expressions with the same variable.
    //   a * var + b == c * var + d
    //   -> (a - c) * var = d - b
    // Therefore var cannot take the value (d - b) / (a - c) if integral.
    forbidden_values.clear();
    for (int i = 0; i + 1 < terms.size(); ++i) {
      for (int j = i + 1; j < terms.size(); ++j) {
        const int64_t coeff = terms[i].first - terms[j].first;
        if (coeff == 0) continue;
        const int64_t offset = terms[j].second - terms[i].second;
        const int64_t value = offset / coeff;
        if (value * coeff == offset) {
          forbidden_values.push_back(value);
        }
      }
    }
    if (!forbidden_values.empty()) {
      const Domain to_keep = Domain::FromValues(forbidden_values).Complement();
      bool propagated = false;
      if (!context_->IntersectDomainWith(var, to_keep, &propagated)) {
        return true;
      }
      if (propagated) {
        context_->UpdateRuleStats(
            "all_diff: propagate expressions with the same variable");
      }
    }
  }

  // Propagate mandatory values if the all diff is actually a permutation.
  if (all_diff.exprs_size() >= 2 && all_diff.exprs_size() <= 512) {
    Domain union_of_domains = context_->DomainSuperSetOf(all_diff.exprs(0));
    for (int i = 1; i < all_diff.exprs_size(); ++i) {
      union_of_domains = union_of_domains.UnionWith(
          context_->DomainSuperSetOf(all_diff.exprs(i)));
    }

    if (union_of_domains.Size() < all_diff.exprs_size()) {
      return context_->NotifyThatModelIsUnsat(
          "all_diff: more expressions than values");
    }

    if (all_diff.exprs_size() == union_of_domains.Size()) {
      absl::btree_map<int64_t, UniqueNonNegativeValue> value_to_index;
      for (int i = 0; i < all_diff.exprs_size(); ++i) {
        const LinearExpressionProto& expr = all_diff.exprs(i);
        DCHECK_EQ(expr.vars_size(), 1);
        for (const int64_t v : context_->DomainOf(expr.vars(0)).Values()) {
          value_to_index[AffineExpressionValueAt(expr, v)].Add(i);
        }
      }

      bool propagated = false;
      for (const auto& [value, unique_index] : value_to_index) {
        if (!unique_index.HasUniqueValue()) continue;

        const LinearExpressionProto& expr =
            all_diff.exprs(unique_index.value());
        if (!context_->IntersectDomainWith(expr, Domain(value), &propagated)) {
          return true;
        }
      }

      if (propagated) {
        context_->UpdateRuleStats(
            "all_diff: propagated mandatory values in permutation");
      }
    }
  }

  return variables_have_changed;
}

bool CpConstraintPresolver::PresolveNoOverlap(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  NoOverlapConstraintProto* proto = ct->mutable_no_overlap();
  bool changed = false;

  // Filter out absent intervals.
  {
    int new_size = 0;
    const int initial_num_intervals = proto->intervals_size();
    for (int i = 0; i < initial_num_intervals; ++i) {
      const int interval_index = proto->intervals(i);
      if (context_->ConstraintIsInactive(interval_index)) continue;
      proto->set_intervals(new_size++, interval_index);
    }

    if (new_size < initial_num_intervals) {
      proto->mutable_intervals()->Truncate(new_size);
      context_->UpdateRuleStats("no_overlap: removed absent intervals");
      changed = true;
    }

    if (proto->intervals_size() == 1) {
      context_->UpdateRuleStats("no_overlap: only one interval");
      return RemoveConstraint(ct);
    }
    if (proto->intervals().empty()) {
      context_->UpdateRuleStats("no_overlap: no intervals");
      return RemoveConstraint(ct);
    }
  }

  // TODO(user): add support for enforcement literals.
  if (HasEnforcementLiteral(*ct)) return changed;

  // Process duplicate intervals.
  {
    // Collect duplicate intervals.
    absl::flat_hash_set<int> visited_intervals;
    absl::flat_hash_set<int> duplicate_intervals;
    for (const int interval_index : proto->intervals()) {
      if (!visited_intervals.insert(interval_index).second) {
        duplicate_intervals.insert(interval_index);
      }
    }

    const int initial_num_intervals = proto->intervals_size();
    int new_size = 0;
    visited_intervals.clear();

    for (int i = 0; i < initial_num_intervals; ++i) {
      const int interval_index = proto->intervals(i);

      if (duplicate_intervals.contains(interval_index)) {
        // Once processed, we can always remove further duplicates.
        if (!visited_intervals.insert(interval_index).second) continue;

        ConstraintProto* interval_ct =
            context_->MutableConstraint(interval_index);

        // Case 1: size > 0. Interval must be unperformed.
        if (context_->SizeMin(interval_index) > 0) {
          if (HasEnforcementLiteral(*interval_ct)) {
            context_->UpdateRuleStats(
                "no_overlap: unperform duplicate non zero-sized intervals");
            if (!MarkOptionalIntervalAsFalse(interval_ct)) {
              return false;
            }
            // We can remove the interval from the no_overlap.
            continue;
          } else {
            return context_->NotifyThatModelIsUnsat(
                "no_overlap: duplicate interval with positive size");
          }
        }

        // No need to do anything if the size is 0.
        if (context_->SizeMax(interval_index) > 0) {
          // Case 2: interval is performed. Size must be set to 0.
          if (!context_->ConstraintIsOptional(interval_index)) {
            if (!context_->IntersectDomainWith(interval_ct->interval().size(),
                                               Domain(0))) {
              return false;
            }
            context_->UpdateRuleStats(
                "no_overlap: zero the size of performed duplicate intervals");
            // We still need to add the interval to the no_overlap as zero sized
            // intervals still cannot overlap with other intervals.
          } else {  // Case 3: interval is optional and size can be > 0.
            const int performed_literal = interval_ct->enforcement_literal(0);
            ConstraintProto* size_eq_zero = context_->AddConstraint();
            size_eq_zero->add_enforcement_literal(performed_literal);
            size_eq_zero->mutable_linear()->add_domain(0);
            size_eq_zero->mutable_linear()->add_domain(0);
            AddLinearExpressionToLinearConstraint(
                interval_ct->interval().size(), 1,
                size_eq_zero->mutable_linear());
            context_->UpdateRuleStats(
                "no_overlap: make duplicate intervals as unperformed or zero "
                "sized");
          }
        }
      }

      proto->set_intervals(new_size++, interval_index);
    }

    if (new_size < initial_num_intervals) {
      proto->mutable_intervals()->Truncate(new_size);
      changed = true;
    }
  }

  // Split constraints in disjoint sets.
  if (proto->intervals_size() > 1) {
    std::vector<IndexedInterval> indexed_intervals;
    indexed_intervals.reserve(proto->intervals_size());
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
            context_->AddConstraint()->mutable_no_overlap();
        // Fill in the intervals. Unfortunately, the Assign() method does not
        // compile in or-tools.
        for (const int i : intervals) {
          new_no_overlap->add_intervals(i);
        }
      }
      context_->UpdateRuleStats("no_overlap: split into disjoint components");
      return RemoveConstraint(ct);
    }
  }

  std::vector<int> constant_intervals;
  int64_t size_min_of_non_constant_intervals = kint64max;
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

  bool move_constraint_last = false;
  if (!constant_intervals.empty()) {
    // Sort constant_intervals by start min.
    std::sort(constant_intervals.begin(), constant_intervals.end(),
              [this](int i1, int i2) {
                const int64_t s1 = context_->StartMin(i1);
                const int64_t e1 = context_->EndMax(i1);
                const int64_t s2 = context_->StartMin(i2);
                const int64_t e2 = context_->EndMax(i2);
                return std::tie(s1, e1) < std::tie(s2, e2);
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
      proto->add_intervals(context_->NumConstraints());
      IntervalConstraintProto* new_interval =
          context_->AddConstraint()->mutable_interval();
      new_interval->mutable_start()->set_offset(new_start);
      new_interval->mutable_size()->set_offset(new_end - new_start);
      new_interval->mutable_end()->set_offset(new_end);
      move_constraint_last = true;
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
    }
  }

  {
    // Special case for "all-diff" encoded as no-overlap.
    int num_size_zero_or_one = 0;
    bool has_optional_size_one = false;
    for (const int index : proto->intervals()) {
      const ConstraintProto& interval_ct = context_->Constraint(index);
      const LinearExpressionProto& size = interval_ct.interval().size();
      if (size.vars().empty() && size.offset() >= 0 && size.offset() <= 1) {
        ++num_size_zero_or_one;
      }
      if (size.vars().empty() && size.offset() == 1 &&
          !interval_ct.enforcement_literal().empty()) {
        has_optional_size_one = true;
      }
    }
    const int initial_num_intervals = proto->intervals().size();
    if (num_size_zero_or_one == initial_num_intervals) {
      if (has_optional_size_one) {
        // If there is only size zero or one, we can remove the size zero
        // intervals as there is no constraint on them.
        int new_size = 0;
        for (const int index : proto->intervals()) {
          const IntervalConstraintProto& interval =
              context_->Constraint(index).interval();
          if (interval.size().offset() == 0) continue;
          proto->set_intervals(new_size++, index);
        }
        if (new_size < initial_num_intervals) {
          proto->mutable_intervals()->Truncate(new_size);
          changed = true;
          context_->UpdateRuleStats("no_overlap: removed size 0 from all diff");
        }
      } else {
        // All size one intervals are present, we can convert to an
        // all_different constraint, and remove size 0 intervals.
        AllDifferentConstraintProto* all_diff =
            context_->AddEnforcedConstraint(ct)->mutable_all_diff();
        for (const int index : proto->intervals()) {
          const IntervalConstraintProto& interval =
              context_->Constraint(index).interval();
          if (interval.size().offset() == 0) continue;
          *all_diff->add_exprs() = interval.start();
        }
        if (all_diff->exprs_size() < initial_num_intervals) {
          context_->UpdateRuleStats("no_overlap: removed size 0 from all diff");
        }
        context_->UpdateRuleStats("no_overlap: converted to all diff");
        return RemoveConstraint(ct);
      }
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

  // Unfortunately, because we want all intervals to appear before a constraint
  // that uses them, we need to move the constraint last when we merged constant
  // intervals.
  if (move_constraint_last) {
    changed = true;
    *context_->AddConstraint() = *ct;
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpConstraintPresolver::PresolveNoOverlap2DFramed(
    absl::Span<const Rectangle> fixed_boxes,
    absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct) {
  const NoOverlap2DConstraintProto& proto = ct->no_overlap_2d();

  DCHECK(!non_fixed_boxes.empty());
  Rectangle bounding_box = non_fixed_boxes[0].bounding_area;
  for (const RectangleInRange& box : non_fixed_boxes) {
    bounding_box.GrowToInclude(box.bounding_area);
  }
  std::vector<Rectangle> espace_for_single_box =
      FindEmptySpaces(bounding_box, {fixed_boxes.begin(), fixed_boxes.end()});
  // TODO(user): Find a faster way to see if fixed boxes are delimiting a
  // rectangle.
  std::vector<Rectangle> empty;
  ReduceNumberofBoxesGreedy(&espace_for_single_box, &empty);
  ReduceNumberOfBoxesExactMandatory(&espace_for_single_box, &empty);
  if (espace_for_single_box.size() != 1) {
    // Not a rectangular frame, since the inside is not a rectangle.
    return false;
  }
  Rectangle fixed_boxes_bb = fixed_boxes.front();
  for (const Rectangle& box : fixed_boxes) {
    fixed_boxes_bb.GrowToInclude(box);
  }
  const Rectangle framed_region = espace_for_single_box.front();
  for (const RectangleInRange& box : non_fixed_boxes) {
    if (!box.bounding_area.IsInsideOf(fixed_boxes_bb)) {
      // Something can be outside of the frame.
      return false;
    }
    if (non_fixed_boxes.size() > 1 &&
        (2 * box.x_size <= framed_region.SizeX() ||
         2 * box.y_size <= framed_region.SizeY())) {
      // We can fit two boxes in the delimited space between the fixed boxes, so
      // we cannot replace it by an at-most-one.
      return false;
    }
    const int x_interval_index = proto.x_intervals(box.box_index);
    const int y_interval_index = proto.y_intervals(box.box_index);
    if (!context_->Constraint(x_interval_index).enforcement_literal().empty() &&
        !context_->Constraint(y_interval_index).enforcement_literal().empty()) {
      if (context_->Constraint(x_interval_index).enforcement_literal(0) !=
          context_->Constraint(y_interval_index).enforcement_literal(0)) {
        // Two different enforcement literals.
        return false;
      }
    }
  }
  // All this no_overlap_2d constraint is doing is forcing at most one of
  // the non-fixed boxes to be in the `framed_region` rectangle. A
  // better representation of this is to simply enforce that the items fit
  // that rectangle with linear constraints and add an at-most-one constraint.
  std::vector<int> enforcement_literals_for_amo;
  bool has_mandatory = false;
  for (const RectangleInRange& box : non_fixed_boxes) {
    const int box_index = box.box_index;
    const int x_interval_index = proto.x_intervals(box_index);
    const int y_interval_index = proto.y_intervals(box_index);
    const ConstraintProto& x_interval_ct =
        context_->Constraint(x_interval_index);
    const ConstraintProto& y_interval_ct =
        context_->Constraint(y_interval_index);
    if (x_interval_ct.enforcement_literal().empty() &&
        y_interval_ct.enforcement_literal().empty()) {
      // Mandatory box, update the domains.
      if (has_mandatory) {
        return context_->NotifyThatModelIsUnsat(
            "Two mandatory boxes in the same space");
      }
      has_mandatory = true;
      if (!context_->IntersectDomainWith(x_interval_ct.interval().start(),
                                         Domain(framed_region.x_min.value(),
                                                framed_region.x_max.value()))) {
        return true;
      }
      if (!context_->IntersectDomainWith(x_interval_ct.interval().end(),
                                         Domain(framed_region.x_min.value(),
                                                framed_region.x_max.value()))) {
        return true;
      }
      if (!context_->IntersectDomainWith(y_interval_ct.interval().start(),
                                         Domain(framed_region.y_min.value(),
                                                framed_region.y_max.value()))) {
        return true;
      }
      if (!context_->IntersectDomainWith(y_interval_ct.interval().end(),
                                         Domain(framed_region.y_min.value(),
                                                framed_region.y_max.value()))) {
        return true;
      }
    } else {
      auto add_linear_constraint = [&](const ConstraintProto& interval_ct,
                                       int enforcement_literal,
                                       IntegerValue min, IntegerValue max) {
        // TODO(user): If size is constant add only one linear constraint
        // instead of two.
        context_->AddImplyInDomain(enforcement_literal,
                                   interval_ct.interval().start(),
                                   Domain(min.value(), max.value()));
        context_->AddImplyInDomain(enforcement_literal,
                                   interval_ct.interval().end(),
                                   Domain(min.value(), max.value()));
      };
      const int enforcement_literal =
          x_interval_ct.enforcement_literal().empty()
              ? y_interval_ct.enforcement_literal(0)
              : x_interval_ct.enforcement_literal(0);
      enforcement_literals_for_amo.push_back(enforcement_literal);
      add_linear_constraint(x_interval_ct, enforcement_literal,
                            framed_region.x_min, framed_region.x_max);
      add_linear_constraint(y_interval_ct, enforcement_literal,
                            framed_region.y_min, framed_region.y_max);
    }
  }
  if (has_mandatory) {
    for (const int lit : enforcement_literals_for_amo) {
      if (!context_->SetLiteralToFalse(lit)) {
        return true;
      }
    }
  } else if (enforcement_literals_for_amo.size() > 1) {
    context_->AddConstraint()->mutable_at_most_one()->mutable_literals()->Add(
        enforcement_literals_for_amo.begin(),
        enforcement_literals_for_amo.end());
  }
  context_->UpdateRuleStats("no_overlap_2d: at most one rectangle in region");
  return RemoveConstraint(ct);
}

bool CpConstraintPresolver::ExpandEncoded2DBinPacking(
    absl::Span<const Rectangle> fixed_boxes,
    absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct) {
  const Disjoint2dPackingResult disjoint_packing_presolve_result =
      DetectDisjointRegionIn2dPacking(
          non_fixed_boxes, fixed_boxes,
          context_->params()
              .maximum_regions_to_split_in_disconnected_no_overlap_2d());
  if (disjoint_packing_presolve_result.bins.empty()) return false;

  const NoOverlap2DConstraintProto& proto = ct->no_overlap_2d();
  std::vector<SolutionCrush::BoxInAreaLiteral> box_in_area_lits;
  absl::flat_hash_map<int, std::vector<int>> box_to_presence_literal;
  // For the boxes that are optional, add a presence literal for each box in a
  // fake "absent" bin.
  for (int idx = 0; idx < non_fixed_boxes.size(); ++idx) {
    const int b = non_fixed_boxes[idx].box_index;
    const ConstraintProto& x_interval_ct =
        context_->Constraint(proto.x_intervals(b));
    const ConstraintProto& y_interval_ct =
        context_->Constraint(proto.y_intervals(b));
    if (x_interval_ct.enforcement_literal().empty() &&
        y_interval_ct.enforcement_literal().empty()) {
      // Mandatory box, cannot be in the "absent" bin -1.
      continue;
    }
    int enforcement_literal = x_interval_ct.enforcement_literal().empty()
                                  ? y_interval_ct.enforcement_literal(0)
                                  : x_interval_ct.enforcement_literal(0);
    int potentially_other_enforcement_literal =
        y_interval_ct.enforcement_literal().empty()
            ? x_interval_ct.enforcement_literal(0)
            : y_interval_ct.enforcement_literal(0);

    if (enforcement_literal == potentially_other_enforcement_literal) {
      // The box is in the "absent" bin -1.
      box_to_presence_literal[idx].push_back(NegatedRef(enforcement_literal));
    } else {
      const int interval_is_absent_literal =
          context_->NewBoolVarWithConjunction(
              {enforcement_literal, potentially_other_enforcement_literal});

      BoolArgumentProto* bool_or = context_->AddConstraint()->mutable_bool_or();
      bool_or->add_literals(NegatedRef(interval_is_absent_literal));
      for (const int lit :
           {enforcement_literal, potentially_other_enforcement_literal}) {
        context_->AddImplication(NegatedRef(interval_is_absent_literal), lit);
        bool_or->add_literals(NegatedRef(lit));
      }
      box_to_presence_literal[idx].push_back(interval_is_absent_literal);
    }
  }
  // Now create the literals "item i in bin j".
  for (int bin_index = 0;
       bin_index < disjoint_packing_presolve_result.bins.size(); ++bin_index) {
    const Disjoint2dPackingResult::Bin& bin =
        disjoint_packing_presolve_result.bins[bin_index];
    NoOverlap2DConstraintProto new_no_overlap_2d;
    for (const Rectangle& ret : bin.fixed_boxes) {
      new_no_overlap_2d.add_x_intervals(context_->NumConstraints());
      new_no_overlap_2d.add_y_intervals(context_->NumConstraints() + 1);
      IntervalConstraintProto* new_interval =
          context_->AddConstraint()->mutable_interval();
      new_interval->mutable_start()->set_offset(ret.x_min.value());
      new_interval->mutable_size()->set_offset(ret.SizeX().value());
      new_interval->mutable_end()->set_offset(ret.x_max.value());

      new_interval = context_->AddConstraint()->mutable_interval();
      new_interval->mutable_start()->set_offset(ret.y_min.value());
      new_interval->mutable_size()->set_offset(ret.SizeY().value());
      new_interval->mutable_end()->set_offset(ret.y_max.value());
    }
    for (const int idx : bin.non_fixed_box_indexes) {
      int presence_in_box_lit = context_->NewBoolVar("binpacking");
      box_to_presence_literal[idx].push_back(presence_in_box_lit);
      const int b = non_fixed_boxes[idx].box_index;
      box_in_area_lits.push_back({.box_index = b,
                                  .area_index = bin_index,
                                  .literal = presence_in_box_lit});
      const ConstraintProto& x_interval_ct =
          context_->Constraint(proto.x_intervals(b));
      const ConstraintProto& y_interval_ct =
          context_->Constraint(proto.y_intervals(b));
      ConstraintProto* new_interval_x = context_->AddConstraint();
      *new_interval_x = x_interval_ct;
      new_interval_x->clear_enforcement_literal();
      new_interval_x->add_enforcement_literal(presence_in_box_lit);
      ConstraintProto* new_interval_y = context_->AddConstraint();
      *new_interval_y = y_interval_ct;
      new_interval_y->clear_enforcement_literal();
      new_interval_y->add_enforcement_literal(presence_in_box_lit);
      new_no_overlap_2d.add_x_intervals(context_->NumConstraints() - 2);
      new_no_overlap_2d.add_y_intervals(context_->NumConstraints() - 1);
    }
    context_->AddConstraint()->mutable_no_overlap_2d()->Swap(
        &new_no_overlap_2d);
  }

  // Each box is in exactly one bin (including the fake "absent" bin).
  for (int box_index = 0; box_index < non_fixed_boxes.size(); ++box_index) {
    const std::vector<int>& presence_literals =
        box_to_presence_literal[box_index];
    if (presence_literals.empty()) {
      return context_->NotifyThatModelIsUnsat(
          "A mandatory box cannot be placed in any position");
    }
    auto* exactly_one = context_->AddConstraint()->mutable_exactly_one();
    for (const int presence_literal : presence_literals) {
      exactly_one->add_literals(presence_literal);
    }
  }
  CompactVectorVector<int, Rectangle> areas;
  for (int bin_index = 0;
       bin_index < disjoint_packing_presolve_result.bins.size(); ++bin_index) {
    areas.Add(disjoint_packing_presolve_result.bins[bin_index].bin_area);
  }
  solution_crush_.AssignVariableToPackingArea(
      areas, context_->WorkingModel(), proto.x_intervals(), proto.y_intervals(),
      box_in_area_lits);
  context_->UpdateRuleStats(
      "no_overlap_2d: fixed boxes partition available space, converted "
      "to optional regions");
  return RemoveConstraint(ct);
}

bool CpConstraintPresolver::PresolveNoOverlap2D(int /*c*/,
                                                ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) {
    return false;
  }
  // TODO(user): add support for enforcement literals.
  const NoOverlap2DConstraintProto& proto = ct->no_overlap_2d();
  bool truncated = false;

  // Filter absent boxes.
  {
    const int initial_num_boxes = proto.x_intervals_size();
    int new_size = 0;
    for (int i = 0; i < proto.x_intervals_size(); ++i) {
      const int x_interval_index = proto.x_intervals(i);
      const int y_interval_index = proto.y_intervals(i);

      ct->mutable_no_overlap_2d()->set_x_intervals(new_size, x_interval_index);
      ct->mutable_no_overlap_2d()->set_y_intervals(new_size, y_interval_index);

      // We don't want to fully presolve the intervals (intervals have their own
      // presolve), but we don't want to bother with negative sizes downstream
      // in this function, so we want to remove them ASAP.
      for (const int interval_index : {x_interval_index, y_interval_index}) {
        if (context_->StartMin(interval_index) >
            context_->EndMax(interval_index)) {
          const ConstraintProto& interval_ct =
              context_->Constraint(interval_index);
          if (interval_ct.enforcement_literal_size() == 1) {
            const int literal = interval_ct.enforcement_literal(0);
            if (!context_->SetLiteralToFalse(literal)) {
              return true;
            }
          } else {
            return context_->NotifyThatModelIsUnsat(
                "no_overlap_2d: impossible interval");
          }
        }

        if (context_->SizeMin(interval_index) < 0) {
          const ConstraintProto& interval_ct =
              context_->Constraint(interval_index);
          if (interval_ct.enforcement_literal().empty()) {
            bool domain_changed = false;
            // Size can't be negative.
            if (!context_->IntersectDomainWith(interval_ct.interval().size(),
                                               Domain(0, kint64max),
                                               &domain_changed)) {
              return false;
            }
          }
        }
      }

      if (context_->ConstraintIsInactive(x_interval_index) ||
          context_->ConstraintIsInactive(y_interval_index)) {
        continue;
      }

      new_size++;
    }

    if (new_size < initial_num_boxes) {
      truncated = true;
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
  }

  if (HasEnforcementLiteral(*ct)) return false;

  bool x_constant = true;
  bool y_constant = true;
  bool has_zero_sized_interval = false;
  bool has_potential_zero_sized_interval = false;

  std::vector<Rectangle> bounding_boxes, fixed_boxes, non_fixed_bounding_boxes;
  std::vector<RectangleInRange> non_fixed_boxes;
  absl::flat_hash_set<int> fixed_item_indexes;
  for (int i = 0; i < proto.x_intervals_size(); ++i) {
    const int x_interval_index = proto.x_intervals(i);
    const int y_interval_index = proto.y_intervals(i);

    bounding_boxes.push_back(
        {IntegerValue(context_->StartMin(x_interval_index)),
         IntegerValue(context_->EndMax(x_interval_index)),
         IntegerValue(context_->StartMin(y_interval_index)),
         IntegerValue(context_->EndMax(y_interval_index))});
    const IntegerValue size_max_x =
        IntegerValue(context_->SizeMax(x_interval_index));
    const IntegerValue size_max_y =
        IntegerValue(context_->SizeMax(y_interval_index));
    const IntegerValue size_min_x =
        std::max(IntegerValue(0),
                 std::min(bounding_boxes.back().SizeX(),
                          IntegerValue(context_->SizeMin(x_interval_index))));
    const IntegerValue size_min_y =
        std::max(IntegerValue(0),
                 std::min(bounding_boxes.back().SizeY(),
                          IntegerValue(context_->SizeMin(y_interval_index))));
    if (context_->IntervalIsConstant(x_interval_index) &&
        context_->IntervalIsConstant(y_interval_index) && size_max_x > 0 &&
        size_max_y > 0) {
      if (bounding_boxes.back().SizeX() != size_max_x ||
          bounding_boxes.back().SizeY() != size_max_y) {
        // This is probably unsat, but this presolve is not the right place
        // to deal with it.
        return true;
      }
      fixed_boxes.push_back(bounding_boxes.back());
      fixed_item_indexes.insert(i);
    } else {
      non_fixed_bounding_boxes.push_back(bounding_boxes.back());
      non_fixed_boxes.push_back({.box_index = i,
                                 .bounding_area = bounding_boxes.back(),
                                 .x_size = size_min_x,
                                 .y_size = size_min_y});
    }

    if (x_constant && !context_->IntervalIsConstant(x_interval_index)) {
      x_constant = false;
    }
    if (y_constant && !context_->IntervalIsConstant(y_interval_index)) {
      y_constant = false;
    }
    if (size_max_x <= 0 || size_max_y <= 0) {
      has_zero_sized_interval = true;
    }
    if (size_min_x <= 0 || size_min_y <= 0) {
      has_potential_zero_sized_interval = true;
    }
  }

  const CompactVectorVector<int> components =
      GetOverlappingRectangleComponents(bounding_boxes);
  if (components.size() > 1) {
    for (const absl::Span<const int> boxes : components) {
      if (boxes.size() <= 1) continue;

      NoOverlap2DConstraintProto* new_no_overlap_2d =
          context_->AddConstraint()->mutable_no_overlap_2d();
      for (const int b : boxes) {
        new_no_overlap_2d->add_x_intervals(proto.x_intervals(b));
        new_no_overlap_2d->add_y_intervals(proto.y_intervals(b));
      }
    }
    context_->UpdateRuleStats("no_overlap_2d: split into disjoint components");
    return RemoveConstraint(ct);
  }

  // TODO(user): handle this case. See issue #4068.
  if (!has_zero_sized_interval && (x_constant || y_constant)) {
    context_->UpdateRuleStats(
        "no_overlap_2d: a dimension is constant, splitting into many "
        "no_overlaps");
    std::vector<IndexedInterval> indexed_intervals;
    for (int i = 0; i < proto.x_intervals_size(); ++i) {
      int x = proto.x_intervals(i);
      int y = proto.y_intervals(i);
      if (x_constant) std::swap(x, y);
      indexed_intervals.push_back({x, IntegerValue(context_->StartMin(y)),
                                   IntegerValue(context_->EndMax(y))});
    }
    CompactVectorVector<int> no_overlaps;
    absl::c_stable_sort(indexed_intervals,
                        IndexedInterval::ComparatorByStart());
    ConstructOverlappingSets(absl::MakeSpan(indexed_intervals), &no_overlaps);
    for (const absl::Span<const int> component : no_overlaps) {
      ConstraintProto* new_ct = context_->AddConstraint();
      // Unfortunately, the Assign() method does not work in or-tools as the
      // protobuf int32_t type is not the int type.
      for (const int i : component) {
        new_ct->mutable_no_overlap()->add_intervals(i);
      }
    }
    return RemoveConstraint(ct);
  }

  // We check if the fixed boxes are not overlapping so downstream code can
  // assume it to be true.
  if (!FindPartialRectangleIntersections(fixed_boxes).empty()) {
    return context_->NotifyThatModelIsUnsat(
        "Two fixed boxes in no_overlap_2d overlap");
  }

  if (non_fixed_bounding_boxes.empty()) {
    context_->UpdateRuleStats("no_overlap_2d: all boxes are fixed");
    return RemoveConstraint(ct);
  }

  // TODO(user): presolve the zero-size fixed items so they are disjoint from
  // the other fixed items. Then the following presolve is still valid. On the
  // other hand, we cannot do much with non-fixed zero-size items.
  if (!has_potential_zero_sized_interval && !fixed_boxes.empty()) {
    const bool presolved =
        PresolveFixed2dRectangles(non_fixed_boxes, &fixed_boxes);
    if (presolved) {
      NoOverlap2DConstraintProto new_no_overlap_2d;

      // Replace the old fixed intervals by the new ones.
      const int old_size = proto.x_intervals_size();
      for (int i = 0; i < old_size; ++i) {
        if (fixed_item_indexes.contains(i)) {
          continue;
        }
        new_no_overlap_2d.add_x_intervals(proto.x_intervals(i));
        new_no_overlap_2d.add_y_intervals(proto.y_intervals(i));
      }
      for (const Rectangle& fixed_box : fixed_boxes) {
        const int item_x_interval = context_->NumConstraints();
        IntervalConstraintProto* new_interval =
            context_->AddConstraint()->mutable_interval();
        new_interval->mutable_start()->set_offset(fixed_box.x_min.value());
        new_interval->mutable_size()->set_offset(fixed_box.SizeX().value());
        new_interval->mutable_end()->set_offset(fixed_box.x_max.value());

        const int item_y_interval = context_->NumConstraints();
        new_interval = context_->AddConstraint()->mutable_interval();
        new_interval->mutable_start()->set_offset(fixed_box.y_min.value());
        new_interval->mutable_size()->set_offset(fixed_box.SizeY().value());
        new_interval->mutable_end()->set_offset(fixed_box.y_max.value());

        new_no_overlap_2d.add_x_intervals(item_x_interval);
        new_no_overlap_2d.add_y_intervals(item_y_interval);
      }
      context_->AddConstraint()->mutable_no_overlap_2d()->Swap(
          &new_no_overlap_2d);
      context_->UpdateRuleStats("no_overlap_2d: presolved fixed rectangles");
      return RemoveConstraint(ct);
    }
  }

  if (!fixed_boxes.empty() && fixed_boxes.size() <= 4 &&
      !non_fixed_boxes.empty() && !has_potential_zero_sized_interval) {
    if (PresolveNoOverlap2DFramed(fixed_boxes, non_fixed_boxes, ct)) {
      return true;
    }
  }

  // If the non-fixed boxes are disjoint but connected by fixed boxes, we can
  // split the constraint and duplicate the fixed boxes. To avoid duplicating
  // too many fixed boxes, we do this after we we applied the presolve reducing
  // their number to as few as possible.
  const CompactVectorVector<int> non_fixed_components =
      GetOverlappingRectangleComponents(non_fixed_bounding_boxes);
  if (non_fixed_components.size() > 1) {
    for (const absl::Span<const int> indexes : non_fixed_components) {
      // Note: we care about components of size 1 because they might be
      // overlapping with the fixed boxes.
      NoOverlap2DConstraintProto* new_no_overlap_2d =
          context_->AddConstraint()->mutable_no_overlap_2d();
      for (const int idx : indexes) {
        const int b = non_fixed_boxes[idx].box_index;
        new_no_overlap_2d->add_x_intervals(proto.x_intervals(b));
        new_no_overlap_2d->add_y_intervals(proto.y_intervals(b));
      }
      // Sort the fixed items for determinism.
      std::vector<int> fixed_item_indexes_vec(fixed_item_indexes.begin(),
                                              fixed_item_indexes.end());
      absl::c_sort(fixed_item_indexes_vec);
      for (const int b : fixed_item_indexes_vec) {
        new_no_overlap_2d->add_x_intervals(proto.x_intervals(b));
        new_no_overlap_2d->add_y_intervals(proto.y_intervals(b));
      }
    }
    context_->UpdateRuleStats(
        "no_overlap_2d: split into disjoint components duplicating fixed "
        "boxes");
    return RemoveConstraint(ct);
  }

  if (!has_potential_zero_sized_interval) {
    if (ExpandEncoded2DBinPacking(fixed_boxes, non_fixed_boxes, ct)) {
      return true;
    }
  }
  RunPropagatorsForConstraint(*ct);
  return truncated;
}

namespace {

LinearExpressionProto ConstantExpressionProto(int64_t value) {
  LinearExpressionProto expr;
  expr.set_offset(value);
  return expr;
}

}  // namespace

void CpConstraintPresolver::DetectDuplicateIntervals(
    int c, google::protobuf::RepeatedField<int32_t>* intervals) {
  interval_representative_.clear();
  bool changed = false;
  const int size = intervals->size();
  for (int i = 0; i < size; ++i) {
    const int index = (*intervals)[i];
    const auto [it, inserted] = interval_representative_.insert({index, index});
    if (it->second != index) {
      changed = true;
      intervals->Set(i, it->second);
      context_->UpdateRuleStats(
          "intervals: change duplicate index inside constraint");
    }
  }
  if (changed) context_->UpdateConstraintVariableUsage(c);
}

bool CpConstraintPresolver::PresolveCumulative(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for enforcement literals.
  if (HasEnforcementLiteral(*ct)) return false;

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

  // Merge identical intervals.
  {
    std::vector<int> intervals = {proto->intervals().begin(),
                                  proto->intervals().end()};
    gtl::STLSortAndRemoveDuplicates(&intervals);
    if (intervals.size() < proto->intervals_size()) {
      absl::btree_map<int, std::vector<LinearExpressionProto>>
          interval_to_sizes;
      for (int i = 0; i < proto->intervals_size(); ++i) {
        interval_to_sizes[proto->intervals(i)].push_back(proto->demands(i));
      }

      absl::btree_map<int, int64_t> terms;
      proto->clear_intervals();
      proto->clear_demands();
      for (const auto& [interval, demands] : interval_to_sizes) {
        terms.clear();
        int64_t offset = 0;
        for (const LinearExpressionProto& demand : demands) {
          for (int i = 0; i < demand.vars_size(); ++i) {
            terms[demand.vars(i)] += demand.coeffs(i);
          }
          offset += demand.offset();
        }
        if (terms.size() <= 1) {
          proto->add_intervals(interval);
          LinearExpressionProto* demand = proto->add_demands();
          for (const auto& [var, coeff] : terms) {
            demand->add_vars(var);
            demand->add_coeffs(coeff);
          }
          demand->set_offset(offset);
          context_->UpdateRuleStats(
              "cumulative: merged demands of identical interval");
        } else {
          LinearConstraintProto* sum_of_terms =
              context_->AddConstraint()->mutable_linear();
          std::vector<int> vars;
          vars.reserve(terms.size());
          std::vector<int64_t> coeffs;
          coeffs.reserve(terms.size());
          Domain new_domain(0);
          for (const auto& [var, coeff] : terms) {
            vars.push_back(var);
            coeffs.push_back(coeff);
            new_domain = new_domain.AdditionWith(
                context_->DomainOf(var).ContinuousMultiplicationBy(coeff));
            sum_of_terms->add_vars(var);
            sum_of_terms->add_coeffs(coeff);
          }
          const int variable_demand = context_->NewIntVar(new_domain);
          context_->solution_crush().SetVarToLinearExpression(variable_demand,
                                                              vars, coeffs);
          sum_of_terms->add_vars(variable_demand);
          sum_of_terms->add_coeffs(-1);
          FillDomainInProto(0, sum_of_terms);

          proto->add_intervals(interval);
          LinearExpressionProto* demand = proto->add_demands();
          demand->add_vars(variable_demand);
          demand->add_coeffs(1);
          demand->set_offset(offset);
          changed = true;

          context_->UpdateRuleStats(
              "cumulative: merged variable demands of identical interval");
        }
      }
    }
  }

  // Filter absent intervals, or zero demands, or demand incompatible with the
  // capacity.
  {
    int new_size = 0;
    int num_zero_demand_removed = 0;
    int num_zero_size_removed = 0;
    int num_incompatible_intervals = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      if (context_->ConstraintIsInactive(proto->intervals(i))) continue;

      const LinearExpressionProto& demand_expr = proto->demands(i);
      const int64_t demand_max = context_->MaxOf(demand_expr);
      if (demand_max == 0) {
        num_zero_demand_removed++;
        continue;
      }

      const int interval_index = proto->intervals(i);
      if (context_->SizeMax(interval_index) <= 0) {
        // Size 0 intervals cannot contribute to a cumulative.
        num_zero_size_removed++;
        continue;
      }

      // Inconsistent intervals cannot be performed.
      const int64_t start_min = context_->StartMin(interval_index);
      const int64_t end_max = context_->EndMax(interval_index);
      if (start_min > end_max) {
        if (context_->ConstraintIsOptional(interval_index)) {
          ConstraintProto* interval_ct =
              context_->MutableConstraint(interval_index);
          DCHECK_EQ(interval_ct->enforcement_literal_size(), 1);
          const int literal = interval_ct->enforcement_literal(0);
          if (!context_->SetLiteralToFalse(literal)) {
            return true;
          }
          num_incompatible_intervals++;
          continue;
        } else {
          return context_->NotifyThatModelIsUnsat(
              "cumulative: inconsistent intervals cannot be performed");
        }
      }

      if (context_->MinOf(demand_expr) > capacity_max) {
        if (context_->ConstraintIsOptional(interval_index)) {
          if (context_->SizeMin(interval_index) > 0) {
            ConstraintProto* interval_ct =
                context_->MutableConstraint(interval_index);
            DCHECK_EQ(interval_ct->enforcement_literal_size(), 1);
            const int literal = interval_ct->enforcement_literal(0);
            if (!context_->SetLiteralToFalse(literal)) {
              return true;
            }
            num_incompatible_intervals++;
            continue;
          }
        } else {  // Interval performed.
          // Try to set the size to 0.
          const ConstraintProto& interval_ct =
              context_->Constraint(interval_index);
          if (!context_->IntersectDomainWith(interval_ct.interval().size(),
                                             {0, 0})) {
            return true;
          }
          context_->UpdateRuleStats(
              "cumulative: zero size of performed demand that exceeds "
              "capacity");
          ++num_zero_demand_removed;
          continue;
        }
      }

      proto->set_intervals(new_size, interval_index);
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
    if (num_incompatible_intervals > 0) {
      context_->UpdateRuleStats(
          "cumulative: removed intervals that can't be performed");
    }
  }

  // Checks the compatibility of demands w.r.t. the capacity.
  {
    for (int i = 0; i < proto->demands_size(); ++i) {
      const int interval = proto->intervals(i);
      const LinearExpressionProto& demand_expr = proto->demands(i);
      if (context_->ConstraintIsOptional(interval)) continue;
      if (context_->SizeMin(interval) <= 0) continue;
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
            context_->AddConstraint()->mutable_cumulative();
        for (const int i : component) {
          new_cumulative->add_intervals(proto->intervals(i));
          *new_cumulative->add_demands() = proto->demands(i);
        }
        *new_cumulative->mutable_capacity() = proto->capacity();
      }
      context_->UpdateRuleStats("cumulative: split into disjoint components");
      return RemoveConstraint(ct);
    }
  }

  // TODO(user): move the algorithmic part of what we do below in a
  // separate function to unit test it more properly.
  {
    // Build max load profiles.
    absl::btree_map<int64_t, int64_t> time_to_demand_deltas;
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
          "cumulative: remove never conflicting intervals");
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
      const int interval_index = proto->intervals(i);
      const ConstraintProto& interval_ct = context_->Constraint(interval_index);

      const LinearExpressionProto& demand_expr = proto->demands(i);
      sum_of_max_demands += context_->MaxOf(demand_expr);

      if (interval_ct.enforcement_literal().empty() &&
          context_->SizeMin(interval_index) > 0) {
        max_of_performed_demand_mins = std::max(max_of_performed_demand_mins,
                                                context_->MinOf(demand_expr));
      }
    }

    const LinearExpressionProto& capacity_expr = proto->capacity();
    if (max_of_performed_demand_mins > context_->MinOf(capacity_expr)) {
      context_->UpdateRuleStats("cumulative: propagate min capacity");
      if (!context_->IntersectDomainWith(
              capacity_expr, Domain(max_of_performed_demand_mins, kint64max))) {
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
      gcd = std::gcd(gcd, context_->MinOf(demand_expr));
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
  for (int i = 0; i < num_intervals; ++i) {
    const int index = proto->intervals(i);
    // TODO(user): adapt in the presence of optional intervals.
    if (context_->ConstraintIsOptional(index)) has_optional_interval = true;
    const ConstraintProto& ct = context_->Constraint(proto->intervals(i));
    const IntervalConstraintProto& interval = ct.interval();
    start_exprs[i] = interval.start();

    const LinearExpressionProto& demand_expr = proto->demands(i);
    if (context_->SizeMin(index) == 1 && context_->SizeMax(index) == 1) {
      num_duration_one++;
    }
    if (context_->SizeMin(index) <= 0) {
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
            "cumulative: demand_max exceeds capacity max");
        if (!context_->IntersectDomainWith(demand_expr,
                                           Domain(kint64min, capacity_max))) {
          return true;
        }
      } else {
        // TODO(user): we abort because we cannot convert this to a no_overlap
        // for instance.
        context_->UpdateRuleStats(
            "cumulative: demand_max of optional interval exceeds capacity");
        return changed;
      }
    }
  }
  if (num_greater_half_capacity == num_intervals) {
    if (num_duration_one == num_intervals && !has_optional_interval) {
      context_->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context_->AddConstraint();
      auto* arg = new_ct->mutable_all_diff();
      for (const LinearExpressionProto& expr : start_exprs) {
        *arg->add_exprs() = expr;
      }
      if (!context_->IsFixed(capacity_expr)) {
        const int64_t capacity_min = context_->MinOf(capacity_expr);
        for (const LinearExpressionProto& expr : proto->demands()) {
          if (capacity_min >= context_->MaxOf(expr)) continue;
          LinearConstraintProto* fit =
              context_->AddConstraint()->mutable_linear();
          fit->add_domain(0);
          fit->add_domain(kint64max);
          AddLinearExpressionToLinearConstraint(capacity_expr, 1, fit);
          AddLinearExpressionToLinearConstraint(expr, -1, fit);
        }
      }
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("cumulative: convert to no_overlap");
      // Before we remove the cumulative, add constraints to enforce that the
      // capacity is greater than the demand of any performed intervals.
      for (int i = 0; i < proto->demands_size(); ++i) {
        const LinearExpressionProto& demand_expr = proto->demands(i);
        const int64_t demand_max = context_->MaxOf(demand_expr);
        if (demand_max > context_->MinOf(capacity_expr)) {
          ConstraintProto* capacity_gt = context_->AddConstraint();
          *capacity_gt->mutable_enforcement_literal() =
              context_->Constraint(proto->intervals(i)).enforcement_literal();
          capacity_gt->mutable_linear()->add_domain(0);
          capacity_gt->mutable_linear()->add_domain(kint64max);
          AddLinearExpressionToLinearConstraint(capacity_expr, 1,
                                                capacity_gt->mutable_linear());
          AddLinearExpressionToLinearConstraint(demand_expr, -1,
                                                capacity_gt->mutable_linear());
        }
      }

      ConstraintProto* new_ct = context_->AddConstraint();
      auto* arg = new_ct->mutable_no_overlap();
      for (const int interval : proto->intervals()) {
        arg->add_intervals(interval);
      }
      return RemoveConstraint(ct);
    }
  }

  RunPropagatorsForConstraint(*ct);
  return changed;
}

bool CpConstraintPresolver::PresolveRoutes(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
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

    if (tail >= has_incoming_or_outgoing_arcs.size()) {
      has_incoming_or_outgoing_arcs.resize(tail + 1, false);
    }
    if (head >= has_incoming_or_outgoing_arcs.size()) {
      has_incoming_or_outgoing_arcs.resize(head + 1, false);
    }

    if (context_->LiteralIsFalse(ref)) {
      context_->UpdateRuleStats("routes: removed false arcs");
      continue;
    }
    proto.set_literals(new_size, ref);
    proto.set_tails(new_size, tail);
    proto.set_heads(new_size, head);
    ++new_size;
    has_incoming_or_outgoing_arcs[tail] = true;
    has_incoming_or_outgoing_arcs[head] = true;
  }

  if (old_size > 0 && new_size == 0) {
    // A routes constraint cannot have a self loop on 0. Therefore, if there
    // were arcs, it means it contains non-zero nodes. Without arcs, the
    // constraint is unfeasible.
    return context_->NotifyThatModelIsUnsat(
        "routes: graph with nodes and no arcs");
  }

  // Node 0 is the depot and is allowed to have 0 arcs (no vehicles dispatched).
  // Non-depot nodes (n >= 1) must have at least one potential arc to be
  // feasible.
  for (int n = 1; n < has_incoming_or_outgoing_arcs.size(); ++n) {
    if (!has_incoming_or_outgoing_arcs[n]) {
      return context_->NotifyThatModelIsUnsat(absl::StrCat(
          "routes: node ", n, " misses incoming or outgoing arcs"));
    }
  }
  // Node 0 is the depot. If it has no arcs, no routes can be formed. Therefore,
  // all other nodes must be skipped via self-loops.
  if (!has_incoming_or_outgoing_arcs.empty() &&
      !has_incoming_or_outgoing_arcs[0]) {
    const int num_nodes = has_incoming_or_outgoing_arcs.size();
    int num_self_loops = 0;
    for (int i = 0; i < new_size; ++i) {
      const int ref = proto.literals(i);
      const int tail = proto.tails(i);
      const int head = proto.heads(i);

      if (tail == head) {
        // Must be a self loop
        ++num_self_loops;
        if (!context_->SetLiteralToTrue(ref)) return false;
      } else {
        // Normal paths are impossible
        if (!context_->SetLiteralToFalse(ref)) return false;
      }
    }
    // Node 0 does not have a self-loop, so we expect exactly num_nodes - 1.
    if (num_self_loops != num_nodes - 1) {
      return context_->NotifyThatModelIsUnsat(
          "routes: empty depot and self-loop missing for node");
    }
    context_->UpdateRuleStats("routes: empty depot, only self-loops allowed");
    return RemoveConstraint(ct);
  }

  if (new_size < num_arcs) {
    proto.mutable_literals()->Truncate(new_size);
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    return true;
  }

  RunPropagatorsForConstraint(*ct);
  return false;
}

bool CpConstraintPresolver::PresolveCircuit(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
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

  // All the nodes must have some incoming and outgoing arcs.
  for (int i = 0; i < num_nodes; ++i) {
    if (incoming_arcs[i].empty() || outgoing_arcs[i].empty()) {
      return MarkConstraintAsFalse(ct, "circuit: node with no arcs");
    }
  }

  // Note that it is important to reach the fixed point here:
  // One arc at true, then all other arcs at false. This is because we rely
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
                context_->UpdateRuleStats("circuit: set literal to false");
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
    context_->UpdateRuleStats("circuit: fixed singleton arcs");
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
    proto.set_literals(new_size, ref);
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
          // We have a subcircuit, but it doesn't cover all the mandatory nodes.
          return MarkConstraintAsFalse(
              ct, "circuit: non-covering fixed subcircuit");
        }
      }
      context_->UpdateRuleStats("circuit: fully specified");
      return RemoveConstraint(ct);
    }
  } else {
    // All self-loops?
    if (num_true == new_size) {
      context_->UpdateRuleStats("circuit: empty circuit");
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
        if (!context_->StoreBooleanEqualityRelation(literals[0],
                                                    NegatedRef(literals[1]))) {
          return true;
        }
      }
    }
  }

  // Truncate the circuit and return.
  if (new_size < num_arcs) {
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    proto.mutable_literals()->Truncate(new_size);
    context_->UpdateRuleStats("circuit: removed false arcs");
    return true;
  }

  // TODO(user): This can be really slow, and likely do not bring much.
  // We only do that if we have less than 1k literals. Just remove ?
  if (proto.literals().size() < 1'000) {
    RunPropagatorsForConstraint(*ct);
  }
  return false;
}

bool CpConstraintPresolver::PresolveAutomaton(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;

  AutomatonConstraintProto* proto = ct->mutable_automaton();
  if (proto->exprs_size() == 0 || proto->transition_label_size() == 0) {
    return false;
  }

  bool changed = false;
  for (int i = 0; i < proto->exprs_size(); ++i) {
    changed |= CanonicalizeLinearExpression(*ct, proto->mutable_exprs(i));
  }

  std::vector<absl::flat_hash_set<int64_t>> reachable_states;
  std::vector<absl::flat_hash_set<int64_t>> reachable_labels;
  PropagateAutomaton(*proto, *context_, &reachable_states, &reachable_labels);

  // Filter domains and compute the union of all relevant labels.
  for (int time = 0; time < reachable_labels.size(); ++time) {
    const LinearExpressionProto& expr = proto->exprs(time);
    if (context_->IsFixed(expr)) {
      if (!reachable_labels[time].contains(context_->FixedValue(expr))) {
        return MarkConstraintAsFalse(ct, "automaton: unsat");
      }
    } else {
      std::vector<int64_t> unscaled_reachable_labels;
      for (const int64_t label : reachable_labels[time]) {
        unscaled_reachable_labels.push_back(GetInnerVarValue(expr, label));
      }
      bool removed_values = false;
      if (!context_->IntersectDomainWith(
              expr.vars(0), Domain::FromValues(unscaled_reachable_labels),
              &removed_values)) {
        return true;
      }
      if (removed_values) {
        context_->UpdateRuleStats("automaton: reduce variable domain");
      }
    }
  }

  return changed;
}

bool CpConstraintPresolver::PresolveReservoir(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  // TODO(user): add support for this case.
  if (HasEnforcementLiteral(*ct)) return false;

  ReservoirConstraintProto& proto = *ct->mutable_reservoir();
  bool changed = false;
  for (LinearExpressionProto& exp : *(proto.mutable_time_exprs())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }
  for (LinearExpressionProto& exp : *(proto.mutable_level_changes())) {
    changed |= CanonicalizeLinearExpression(*ct, &exp);
  }

  if (proto.active_literals().empty()) {
    const int true_literal = context_->GetTrueLiteral();
    for (int i = 0; i < proto.time_exprs_size(); ++i) {
      proto.add_active_literals(true_literal);
    }
    changed = true;
  }

  const auto& demand_is_null = [&](int i) {
    return (context_->IsFixed(proto.level_changes(i)) &&
            context_->FixedValue(proto.level_changes(i)) == 0) ||
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
      *proto.mutable_level_changes(new_size) = proto.level_changes(i);
      *proto.mutable_time_exprs(new_size) = proto.time_exprs(i);
      proto.set_active_literals(new_size, proto.active_literals(i));
      new_size++;
    }

    proto.mutable_level_changes()->erase(
        proto.mutable_level_changes()->begin() + new_size,
        proto.mutable_level_changes()->end());
    proto.mutable_time_exprs()->erase(
        proto.mutable_time_exprs()->begin() + new_size,
        proto.mutable_time_exprs()->end());
    proto.mutable_active_literals()->Truncate(new_size);

    context_->UpdateRuleStats(
        "reservoir: remove zero level_changes or inactive events");
  }

  // The rest of the presolve only applies if all demands are fixed.
  for (const LinearExpressionProto& level_change : proto.level_changes()) {
    if (!context_->IsFixed(level_change)) return changed;
  }

  const int num_events = proto.level_changes_size();
  int64_t gcd = proto.level_changes().empty()
                    ? 0
                    : std::abs(context_->FixedValue(proto.level_changes(0)));
  int num_positives = 0;
  int num_negatives = 0;
  int64_t max_sum_of_positive_level_changes = 0;
  int64_t min_sum_of_negative_level_changes = 0;
  for (int i = 0; i < num_events; ++i) {
    const int64_t demand = context_->FixedValue(proto.level_changes(i));
    gcd = std::gcd(gcd, std::abs(demand));
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
    auto* const sum_ct = context_->AddConstraint();
    auto* const sum = sum_ct->mutable_linear();
    int64_t fixed_contrib = 0;
    for (int i = 0; i < proto.level_changes_size(); ++i) {
      const int64_t demand = context_->FixedValue(proto.level_changes(i));
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
    bool changed = false;
    if (!CanonicalizeLinear(sum_ct, &changed)) {
      return true;
    }
    return RemoveConstraint(ct);
  }

  if (gcd > 1) {
    for (int i = 0; i < proto.level_changes_size(); ++i) {
      proto.mutable_level_changes(i)->set_offset(
          context_->FixedValue(proto.level_changes(i)) / gcd);
      proto.mutable_level_changes(i)->clear_vars();
      proto.mutable_level_changes(i)->clear_coeffs();
    }

    // Adjust min and max levels.
    //   max level is always rounded down.
    //   min level is always rounded up.
    const Domain reduced_domain = Domain({proto.min_level(), proto.max_level()})
                                      .InverseMultiplicationBy(gcd);
    proto.set_min_level(reduced_domain.Min());
    proto.set_max_level(reduced_domain.Max());
    context_->UpdateRuleStats(
        "reservoir: simplify level_changes and levels by gcd");
  }

  if (num_positives == 1 && num_negatives > 0) {
    context_->UpdateRuleStats(
        "TODO reservoir: one producer, multiple consumers");
  }

  absl::flat_hash_set<std::tuple<int, int64_t, int64_t, int>> time_active_set;
  for (int i = 0; i < proto.level_changes_size(); ++i) {
    const LinearExpressionProto& time = proto.time_exprs(i);
    const int var = context_->IsFixed(time) ? kint32min : time.vars(0);
    const int64_t coeff = context_->IsFixed(time) ? 0 : time.coeffs(0);
    const std::tuple<int, int64_t, int64_t, int> key = std::make_tuple(
        var, coeff,
        context_->IsFixed(time) ? context_->FixedValue(time) : time.offset(),
        proto.active_literals(i));
    if (time_active_set.contains(key)) {
      context_->UpdateRuleStats("TODO reservoir: merge synchronized events");
      break;
    } else {
      time_active_set.insert(key);
    }
  }

  RunPropagatorsForConstraint(*ct);
  return changed;
}

void CpConstraintPresolver::RunPropagatorsForConstraint(
    const ConstraintProto& ct) {
  if (context_->ModelIsUnsat()) return;

  Model model;

  // Enable as many propagators as possible. We do not care if some propagator
  // is a bit slow or if the explanation is too big: anything that improves our
  // bounds is an improvement.
  SatParameters local_params;
  local_params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  local_params.set_exploit_all_precedences(true);
  local_params.set_use_hard_precedences_in_cumulative(true);
  local_params.set_max_num_intervals_for_timetable_edge_finding(1000);
  local_params.set_use_overload_checker_in_cumulative(true);
  local_params.set_use_strong_propagation_in_disjunctive(true);
  local_params.set_use_timetable_edge_finding_in_cumulative(true);
  local_params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  local_params.set_use_timetabling_in_no_overlap_2d(true);
  local_params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  local_params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  local_params.set_use_conservative_scale_overload_checker(true);
  local_params.set_use_dual_scheduling_heuristics(true);

  model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit_);
  model.GetOrCreate<ModelSharedTimeLimit>()->DisableStop();
  std::vector<int> variable_mapping;
  CreateValidModelWithSingleConstraint(ct, context_, &variable_mapping,
                                       &tmp_model_);
  DCHECK_EQ(ValidateCpModel(tmp_model_, false), "");
  if (!LoadModelForPresolve(tmp_model_, std::move(local_params), context_,
                            &model, "single constraint")) {
    return;
  }

  time_limit_->AdvanceDeterministicTime(
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  auto* mapping = model.GetOrCreate<CpModelMapping>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* trail = model.GetOrCreate<Trail>();

  int num_changed_bounds = 0;
  int num_fixed_bools = 0;
  for (int var = 0; var < variable_mapping.size(); ++var) {
    const int proto_var = variable_mapping[var];
    if (mapping->IsBoolean(var)) {
      const Literal l = mapping->Literal(var);
      if (trail->Assignment().LiteralIsFalse(l)) {
        if (!context_->SetLiteralToFalse(proto_var)) return;
        ++num_fixed_bools;
        continue;
      } else if (trail->Assignment().LiteralIsTrue(l)) {
        if (!context_->SetLiteralToTrue(proto_var)) return;
        ++num_fixed_bools;
        continue;
      }
      // Add Boolean equivalence relations.
      const Literal r = implication_graph->RepresentativeOf(l);
      if (r != l) {
        const int r_var =
            mapping->GetProtoVariableFromBooleanVariable(r.Variable());
        if (r_var < 0) continue;
        if (!context_->StoreBooleanEqualityRelation(
                proto_var, r.IsPositive() ? r_var : NegatedRef(r_var))) {
          return;
        }
      }
    } else {
      // Restrict variable domain.
      bool changed = false;
      if (!context_->IntersectDomainWith(
              proto_var, integer_trail->LevelZeroDomain(mapping->Integer(var)),
              &changed)) {
        return;
      }
      if (changed) ++num_changed_bounds;
    }
  }
  if (num_changed_bounds > 0) {
    context_->UpdateRuleStats("propagators: changed bounds",
                              num_changed_bounds);
  }
  if (num_fixed_bools > 0) {
    context_->UpdateRuleStats("propagators: fixed booleans", num_fixed_bools);
  }
}

bool CpConstraintPresolver::PresolveOneConstraint(int c) {
  if (context_->ModelIsUnsat()) return false;
  ConstraintProto* ct = context_->MutableConstraint(c);

  // Generic presolve to exploit variable/literal equivalence.
  if (ExploitEquivalenceRelations(c, ct)) {
    context_->UpdateConstraintVariableUsage(c);
  }

  // Generic presolve for reified constraint.
  bool changed = false;
  if (!PresolveEnforcementLiteral(ct, &changed)) {
    return false;
  }
  if (changed) {
    context_->UpdateConstraintVariableUsage(c);
  }

  // Call the presolve function for this constraint if any.
  switch (ct->constraint_case()) {
    case ConstraintProto::kBoolOr:
      return PresolveBoolOr(ct);
    case ConstraintProto::kBoolAnd:
      return PresolveBoolAnd(ct);
    case ConstraintProto::kAtMostOne:
      return PresolveAtMostOne(ct);
    case ConstraintProto::kExactlyOne:
      return PresolveExactlyOne(ct);
    case ConstraintProto::kBoolXor:
      return PresolveBoolXor(ct);
    case ConstraintProto::kLinMax:
      if (CanonicalizeLinearArgument(*ct, ct->mutable_lin_max())) {
        context_->UpdateConstraintVariableUsage(c);
      }
      return PresolveLinMax(c, ct);
    case ConstraintProto::kIntProd:
      if (CanonicalizeLinearArgument(*ct, ct->mutable_int_prod())) {
        context_->UpdateConstraintVariableUsage(c);
      }
      return PresolveIntProd(ct);
    case ConstraintProto::kIntDiv:
      if (CanonicalizeLinearArgument(*ct, ct->mutable_int_div())) {
        context_->UpdateConstraintVariableUsage(c);
      }
      return PresolveIntDiv(c, ct);
    case ConstraintProto::kIntMod:
      if (CanonicalizeLinearArgument(*ct, ct->mutable_int_mod())) {
        context_->UpdateConstraintVariableUsage(c);
      }
      return PresolveIntMod(c, ct);
    case ConstraintProto::kLinear: {
      bool changed = false;
      if (!CanonicalizeLinear(ct, &changed)) {
        return true;
      }
      if (changed) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (PropagateDomainsInLinear(c, ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }

      // The constraint should already be canonicalized at this stage.
      if (PresolveSmallLinear(ct, /*canonicalize=*/false)) {
        context_->UpdateConstraintVariableUsage(c);
      }

      bool redo_small_linear_presolve = false;
      if (IsLinearEqualityConstraint(*ct)) {
        redo_small_linear_presolve = true;
        if (PresolveLinearEqualityWithModulo(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
      }

      // We first propagate the domains before calling this presolve rule.
      if (RemoveSingletonInLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
        redo_small_linear_presolve = true;
      }

      if (redo_small_linear_presolve) {
        // There is no need to re-do a propagation here, but the constraint
        // size might have been reduced.
        if (PresolveSmallLinear(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
      }

      if (PresolveLinearOnBooleans(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }

      // If we extracted some enforcement, we redo some presolve.
      const int old_num_enforcement_literals = ct->enforcement_literal_size();
      ExtractEnforcementLiteralFromLinearConstraint(c, ct);
      if (context_->ModelIsUnsat()) return false;
      if (ct->enforcement_literal_size() > old_num_enforcement_literals) {
        if (DivideLinearByGcd(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        if (PresolveSmallLinear(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
      }

      if (PresolveDiophantine(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }

      TryToReduceCoefficientsOfLinearConstraint(c, ct);
      return false;
    }
    case ConstraintProto::kInterval:
      return PresolveInterval(c, ct);
    case ConstraintProto::kInverse:
      DCHECK(ct->inverse().f_direct().empty() &&
             ct->inverse().f_inverse().empty());
      return PresolveInverse(ct);
    case ConstraintProto::kElement:
      return PresolveElement(c, ct);
    case ConstraintProto::kTable:
      return PresolveTable(ct);
    case ConstraintProto::kAllDiff:
      return PresolveAllDiff(ct);
    case ConstraintProto::kNoOverlap:
      DetectDuplicateIntervals(c,
                               ct->mutable_no_overlap()->mutable_intervals());
      return PresolveNoOverlap(ct);
    case ConstraintProto::kNoOverlap2D: {
      const bool changed = PresolveNoOverlap2D(c, ct);
      if (ct->constraint_case() == ConstraintProto::kNoOverlap2D) {
        // For 2D, we don't exploit index duplication between x/y so it is not
        // important to do it beforehand. Moreover in some situation
        // PresolveNoOverlap2D() remove a lot of interval, so better to do it
        // afterwards.
        DetectDuplicateIntervals(
            c, ct->mutable_no_overlap_2d()->mutable_x_intervals());
        DetectDuplicateIntervals(
            c, ct->mutable_no_overlap_2d()->mutable_y_intervals());
      }
      return changed;
    }
    case ConstraintProto::kCumulative:
      DetectDuplicateIntervals(c,
                               ct->mutable_cumulative()->mutable_intervals());
      return PresolveCumulative(ct);
    case ConstraintProto::kCircuit:
      return PresolveCircuit(ct);
    case ConstraintProto::kRoutes:
      return PresolveRoutes(ct);
    case ConstraintProto::kAutomaton:
      return PresolveAutomaton(ct);
    case ConstraintProto::kReservoir:
      return PresolveReservoir(ct);
    default:
      return false;
  }
}

void CpConstraintPresolver::TryToSimplifyDomain(int var) {
  DCHECK(RefIsPositive(var));
  if (context_->ModelIsUnsat()) return;
  if (context_->IsFixed(var)) return;
  if (context_->VariableWasRemoved(var)) return;
  if (context_->VariableIsNotUsedAnymore(var)) return;

  const AffineRelation::Relation r = context_->GetAffineRelation(var);
  if (r.representative != var) return;

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

    gcd = std::gcd(gcd, shifted_value);
    if (gcd == 1) break;
  }
  if (gcd == 1) return;

  // This does all the work since var * 1 % gcd = var_min % gcd.
  context_->CanonicalizeAffineVariable(var, 1, gcd, var_min);
}

// Presolve a variable in relation with its representative.
bool CpConstraintPresolver::PresolveAffineRelationIfAny(int var) {
  const AffineRelation::Relation r = context_->GetAffineRelation(var);
  if (r.representative == var) return true;

  // Propagate domains.
  if (!context_->PropagateAffineRelation(var)) return false;

  // Once an affine relation is detected, the variables should be added to
  // the kAffineRelationConstraint. The only way to be unmarked is if the
  // variable do not appear in any other constraint and is not a representative,
  // in which case it should never be added back.
  if (context_->IsFixed(var)) return true;
  DCHECK(context_->VarToConstraints(var).contains(kAffineRelationConstraint));
  DCHECK(!context_->VariableIsNotUsedAnymore(r.representative));

  // If var is no longer used, remove. Note that we can always do that since we
  // propagated the domain above and so we can find a feasible value for a for
  // any value of the representative.
  context_->RemoveNonRepresentativeAffineVariableIfUnused(var);
  return true;
}

CpConstraintPresolver::CpConstraintPresolver(PresolveContext* context)
    : context_(context),
      solution_crush_(context->solution_crush()),
      logger_(context->logger()),
      time_limit_(context->time_limit()),
      interval_representative_(context->NumConstraints(),
                               IntervalConstraintHash{&context->WorkingModel()},
                               IntervalConstraintEq{&context->WorkingModel()}) {
}

namespace {
bool SimpleLinearExprEq(const LinearExpressionProto& a,
                        const LinearExpressionProto& b) {
  return absl::MakeSpan(a.vars()) == absl::MakeSpan(b.vars()) &&
         absl::MakeSpan(a.coeffs()) == absl::MakeSpan(b.coeffs()) &&
         a.offset() == b.offset();
}

std::size_t LinearExpressionHash(const LinearExpressionProto& expr) {
  return absl::HashOf(absl::MakeSpan(expr.vars()),
                      absl::MakeSpan(expr.coeffs()), expr.offset());
}

}  // namespace

bool CpConstraintPresolver::IntervalConstraintEq::operator()(int a,
                                                             int b) const {
  const ConstraintProto& ct_a = working_model->constraints(a);
  const ConstraintProto& ct_b = working_model->constraints(b);
  return absl::MakeSpan(ct_a.enforcement_literal()) ==
             absl::MakeSpan(ct_b.enforcement_literal()) &&
         SimpleLinearExprEq(ct_a.interval().start(), ct_b.interval().start()) &&
         SimpleLinearExprEq(ct_a.interval().size(), ct_b.interval().size()) &&
         SimpleLinearExprEq(ct_a.interval().end(), ct_b.interval().end());
}

std::size_t CpConstraintPresolver::IntervalConstraintHash::operator()(
    int ct_idx) const {
  const ConstraintProto& ct = working_model->constraints(ct_idx);
  return absl::HashOf(absl::MakeSpan(ct.enforcement_literal()),
                      LinearExpressionHash(ct.interval().start()),
                      LinearExpressionHash(ct.interval().size()),
                      LinearExpressionHash(ct.interval().end()));
}

}  // namespace sat
}  // namespace operations_research
