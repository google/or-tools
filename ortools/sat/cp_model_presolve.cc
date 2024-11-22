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

#include "ortools/sat/cp_model_presolve.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/graph/topologicalsorter.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/2d_rectangle_presolve.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/cp_model_table.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/diophantine.h"
#include "ortools/sat/inclusion.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/util.h"
#include "ortools/sat/var_domination.h"
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

// TODO(user): Just make sure this invariant is enforced in all our linear
// constraint after copy, and simplify the code!
bool LinearConstraintIsClean(const LinearConstraintProto& linear) {
  const int num_vars = linear.vars().size();
  for (int i = 0; i < num_vars; ++i) {
    if (!RefIsPositive(linear.vars(i))) return false;
    if (linear.coeffs(i) == 0) return false;
  }
  return true;
}

}  // namespace

bool CpModelPresolver::RemoveConstraint(ConstraintProto* ct) {
  ct->Clear();
  return true;
}

// Remove all empty constraints and duplicated intervals. Note that we need to
// remap the interval references.
//
// Now that they have served their purpose, we also remove dummy constraints,
// otherwise that causes issue because our model are invalid in tests.
void CpModelPresolver::RemoveEmptyConstraints() {
  interval_representative_.clear();
  std::vector<int> interval_mapping(context_->working_model->constraints_size(),
                                    -1);
  int new_num_constraints = 0;
  const int old_num_non_empty_constraints =
      context_->working_model->constraints_size();
  for (int c = 0; c < old_num_non_empty_constraints; ++c) {
    const auto type = context_->working_model->constraints(c).constraint_case();
    if (type == ConstraintProto::CONSTRAINT_NOT_SET) continue;
    if (type == ConstraintProto::kDummyConstraint) continue;
    context_->working_model->mutable_constraints(new_num_constraints)
        ->Swap(context_->working_model->mutable_constraints(c));
    if (type == ConstraintProto::kInterval) {
      // Warning: interval_representative_ holds a pointer to the working model
      // to compute hashes, so we need to be careful about not changing a
      // constraint after its index is added to the map.
      const auto [it, inserted] = interval_representative_.insert(
          {new_num_constraints, new_num_constraints});
      interval_mapping[c] = it->second;
      if (it->second != new_num_constraints) {
        context_->UpdateRuleStats(
            "intervals: change duplicate index across constraints");
        continue;
      }
    }
    new_num_constraints++;
  }
  google::protobuf::util::Truncate(
      context_->working_model->mutable_constraints(), new_num_constraints);
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
  context_->tmp_literal_set.clear();
  for (const int literal : ct->enforcement_literal()) {
    if (context_->LiteralIsTrue(literal)) {
      // We can remove a literal at true.
      context_->UpdateRuleStats("enforcement: true literal");
      continue;
    }

    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("enforcement: false literal");
      return RemoveConstraint(ct);
    }

    if (context_->VariableIsUniqueAndRemovable(literal)) {
      // We can simply set it to false and ignore the constraint in this case.
      context_->UpdateRuleStats("enforcement: literal not used");
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
        context_->UpdateRuleStats("enforcement: literal with unique direction");
        CHECK(context_->SetLiteralToFalse(literal));
        return RemoveConstraint(ct);
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
  context_->tmp_literal_set.clear();
  const absl::flat_hash_set<int> enforcement_literals_set(
      ct->enforcement_literal().begin(), ct->enforcement_literal().end());
  for (const int literal : ct->bool_and().literals()) {
    if (context_->LiteralIsFalse(literal)) {
      context_->UpdateRuleStats("bool_and: always false");
      return MarkConstraintAsFalse(ct);
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
      context_->UpdateRuleStats("bool_and: x => not x");
      return MarkConstraintAsFalse(ct);
    }
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      changed = true;
      if (!context_->SetLiteralToTrue(literal)) return true;
      continue;
    }

    if (context_->tmp_literal_set.contains(NegatedRef(literal))) {
      context_->UpdateRuleStats("bool_and: cannot be enforced");
      return MarkConstraintAsFalse(ct);
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
    if (context_->VariableIsUniqueAndRemovable(literal)) {
      singleton_literal_with_cost.push_back({literal, 0});
      continue;
    }
    if (context_->VariableWithCostIsUniqueAndRemovable(literal)) {
      const auto it = context_->ObjectiveMap().find(PositiveRef(literal));
      DCHECK(it != context_->ObjectiveMap().end());
      if (RefIsPositive(literal)) {
        singleton_literal_with_cost.push_back({literal, it->second});
      } else {
        // Note that we actually just store the objective change if this literal
        // is true compared to it being false.
        singleton_literal_with_cost.push_back({literal, -it->second});
      }
      continue;
    }

    context_->tmp_literals.push_back(literal);
  }

  bool transform_to_at_most_one = false;
  if (!singleton_literal_with_cost.empty()) {
    changed = true;

    // By domination argument, we can fix to false everything but the minimum.
    if (singleton_literal_with_cost.size() > 1) {
      std::sort(
          singleton_literal_with_cost.begin(),
          singleton_literal_with_cost.end(),
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

bool CpModelPresolver::CanonicalizeLinearArgument(const ConstraintProto& ct,
                                                  LinearArgumentProto* proto) {
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
bool CpModelPresolver::DivideLinMaxByGcd(int c, ConstraintProto* ct) {
  LinearArgumentProto* lin_max = ct->mutable_lin_max();

  // Compute gcd of exprs first.
  int64_t gcd = 0;
  for (const LinearExpressionProto& expr : lin_max->exprs()) {
    gcd = LinearExpressionGcd(expr, gcd);
    if (gcd == 1) break;
  }
  if (gcd <= 1) return true;

  // TODO(user): deal with all UNSAT case.
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

  context_->UpdateRuleStats("lin_max: divising by gcd");
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
int GetFirstVar(ExpressionList exprs) {
  for (const LinearExpressionProto& expr : exprs) {
    for (const int var : expr.vars()) {
      DCHECK(RefIsPositive(var));
      return var;
    }
  }
  return -1;
}

bool IsAffineIntAbs(const ConstraintProto& ct) {
  if (ct.constraint_case() != ConstraintProto::kLinMax ||
      ct.lin_max().exprs_size() != 2 || ct.lin_max().target().vars_size() > 1 ||
      ct.lin_max().exprs(0).vars_size() != 1 ||
      ct.lin_max().exprs(1).vars_size() != 1) {
    return false;
  }

  const LinearArgumentProto& lin_max = ct.lin_max();
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

bool CpModelPresolver::PropagateAndReduceAffineMax(ConstraintProto* ct) {
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
    int64_t current_max = std::numeric_limits<int64_t>::min();

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
    context_->UpdateRuleStats("lin_max: infeasible affine_max constraint");
    return MarkConstraintAsFalse(ct);
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
  // have been propagated without loss. For now, we known that there is no loss
  // if the target is a single ref. Since all the expression are affine, in this
  // case we are fine.
  if (ExpressionContainsSingleRef(target) &&
      context_->VariableIsUniqueAndRemovable(target.vars(0))) {
    context_->MarkVariableAsRemoved(target.vars(0));
    context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
    context_->UpdateRuleStats("lin_max: unused affine_max target");
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpModelPresolver::PropagateAndReduceLinMax(ConstraintProto* ct) {
  const LinearExpressionProto& target = ct->lin_max().target();

  // Compute the infered min/max of the target.
  // Update target domain (if it is not a complex expression).
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
    int64_t max_at_index_to_keep = std::numeric_limits<int64_t>::min();
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

bool CpModelPresolver::PresolveLinMax(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;
  const LinearExpressionProto& target = ct->lin_max().target();

  // x = max(x, xi...) => forall i, x >= xi.
  for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
    if (LinearExpressionProtosAreEqual(expr, target)) {
      for (const LinearExpressionProto& e : ct->lin_max().exprs()) {
        if (LinearExpressionProtosAreEqual(e, target)) continue;
        LinearConstraintProto* prec =
            context_->working_model->add_constraints()->mutable_linear();
        prec->add_domain(0);
        prec->add_domain(std::numeric_limits<int64_t>::max());
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

  // This is a test.12y

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
    context_->UpdateRuleStats("lin_max: no exprs");
    return MarkConstraintAsFalse(ct);
  }

  // If only one is left, we can convert to an equality. Note that we create a
  // new constraint otherwise it might not be processed again.
  if (ct->lin_max().exprs().size() == 1) {
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

  // Checks if the affine target domain is constraining.
  bool linear_target_domain_contains_max_domain = false;
  if (ExpressionContainsSingleRef(target)) {  // target = +/- var.
    int64_t infered_min = std::numeric_limits<int64_t>::min();
    int64_t infered_max = std::numeric_limits<int64_t>::min();
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
  // TODO(user): We could presolve this, but there are a few type of cases.
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
      LinearConstraintProto* prec =
          context_->working_model->add_constraints()->mutable_linear();
      prec->add_domain(0);
      prec->add_domain(std::numeric_limits<int64_t>::max());
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

  changed |= PresolveLinMaxWhenAllBoolean(ct);
  return changed;
}

// If everything is Boolean and affine, do not use a lin max!
bool CpModelPresolver::PresolveLinMaxWhenAllBoolean(ConstraintProto* ct) {
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
    // processed again in these case.
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

// This presolve expect that the constraint only contains 1-var affine
// expressions.
bool CpModelPresolver::PropagateAndReduceIntAbs(ConstraintProto* ct) {
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
      context_->UpdateRuleStats("lin_max: fixed expression in int_abs");
      return RemoveConstraint(ct);
    }
    if (target_domain_modified) {
      context_->UpdateRuleStats("lin_max: propagate domain from x to abs(x)");
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
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, -1, arg);
    CanonicalizeLinear(new_ct);
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  if (context_->MaxOf(expr) <= 0) {
    context_->UpdateRuleStats("lin_max: converted abs to equality");
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->set_name(ct->name());
    auto* arg = new_ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    AddLinearExpressionToLinearConstraint(target_expr, 1, arg);
    AddLinearExpressionToLinearConstraint(expr, 1, arg);
    CanonicalizeLinear(new_ct);
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

bool CpModelPresolver::PresolveIntProd(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  if (HasEnforcementLiteral(*ct)) return false;

  // Start by restricting the domain of target. We will be more precise later.
  bool domain_modified = false;
  Domain implied_domain =
      EvaluateImpliedIntProdDomain(ct->int_prod(), *context_);
  if (!context_->IntersectDomainWith(ct->int_prod().target(), implied_domain,
                                     &domain_modified)) {
    return false;
  }

  // Remove a constraint if the target only appears in the constraint. For this
  // to be correct some conditions must be met:
  // - The target is an affine linear with coefficient -1 or 1.
  // - The target does not appear in the rhs (no x = (a*x + b) * ...).
  // - The target domain covers all the possible range of the rhs.
  if (ExpressionContainsSingleRef(ct->int_prod().target()) &&
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
  LinearArgumentProto* proto = ct->mutable_int_prod();
  for (int i = 0; i < ct->int_prod().exprs().size(); ++i) {
    LinearExpressionProto expr = ct->int_prod().exprs(i);
    if (context_->IsFixed(expr)) {
      const int64_t expr_value = context_->FixedValue(expr);
      constant_factor = CapProd(constant_factor, expr_value);
      context_->UpdateRuleStats("int_prod: removed constant expressions.");
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
    if (!context_->IntersectDomainWith(ct->int_prod().target(),
                                       Domain(constant_factor))) {
      return false;
    }
    context_->UpdateRuleStats("int_prod: constant product");
    return RemoveConstraint(ct);
  }

  // If target is fixed to zero, we can forget the constant factor.
  if (context_->IsFixed(ct->int_prod().target()) &&
      context_->FixedValue(ct->int_prod().target()) == 0 &&
      constant_factor != 1) {
    context_->UpdateRuleStats("int_prod: simplify by constant factor");
    constant_factor = 1;
  }

  // In this case, the only possible value that fit in the domains is zero.
  // We will check for UNSAT if zero is not achievable by the rhs below.
  if (AtMinOrMaxInt64(constant_factor)) {
    context_->UpdateRuleStats("int_prod: overflow if non zero");
    if (!context_->IntersectDomainWith(ct->int_prod().target(), Domain(0))) {
      return false;
    }
    constant_factor = 1;
  }

  // Replace by linear if it cannot overflow.
  if (ct->int_prod().exprs().size() == 1) {
    LinearExpressionProto* const target =
        ct->mutable_int_prod()->mutable_target();
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();

    if (context_->IsFixed(*target)) {
      int64_t target_value = context_->FixedValue(*target);
      if (target_value % constant_factor != 0) {
        return context_->NotifyThatModelIsUnsat(
            "int_prod: product incompatible with fixed target");
      }
      // expression == target_value / constant_factor.
      lin->add_domain(target_value / constant_factor);
      lin->add_domain(target_value / constant_factor);
      AddLinearExpressionToLinearConstraint(ct->int_prod().exprs(0), 1, lin);
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("int_prod: expression is constant.");
      return RemoveConstraint(ct);
    }

    const int64_t target_divisor = LinearExpressionGcd(*target);

    // Reduce coefficients.
    const int64_t gcd =
        std::gcd(static_cast<uint64_t>(std::abs(constant_factor)),
                 static_cast<uint64_t>(std::abs(target_divisor)));
    if (gcd != 1) {
      constant_factor /= gcd;
      DivideLinearExpression(gcd, target);
    }

    // expression * constant_factor = target.
    lin->add_domain(0);
    lin->add_domain(0);
    const bool overflow = !SafeAddLinearExpressionToLinearConstraint(
                              ct->int_prod().target(), 1, lin) ||
                          !SafeAddLinearExpressionToLinearConstraint(
                              ct->int_prod().exprs(0), -constant_factor, lin);

    // Check for overflow.
    if (overflow ||
        PossibleIntegerOverflow(*context_->working_model, lin->vars(),
                                lin->coeffs(), lin->domain(0))) {
      context_->working_model->mutable_constraints()->RemoveLast();
      // Re-add a new term with the constant factor.
      ct->mutable_int_prod()->add_exprs()->set_offset(constant_factor);
    } else {  // Replace with a linear equation.
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("int_prod: linearize product by constant.");
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
  implied_domain = EvaluateImpliedIntProdDomain(ct->int_prod(), *context_);
  const bool is_square = ct->int_prod().exprs_size() == 2 &&
                         LinearExpressionProtosAreEqual(
                             ct->int_prod().exprs(0), ct->int_prod().exprs(1));
  if (!context_->IntersectDomainWith(ct->int_prod().target(), implied_domain,
                                     &domain_modified)) {
    return false;
  }
  if (domain_modified) {
    context_->UpdateRuleStats(absl::StrCat(
        is_square ? "int_square" : "int_prod", ": reduced target domain."));
  }

  // y = x * x, we can reduce the domain of x from the domain of y.
  if (is_square) {
    const int64_t target_max = context_->MaxOf(ct->int_prod().target());
    DCHECK_GE(target_max, 0);
    const int64_t sqrt_max = FloorSquareRoot(target_max);
    bool expr_reduced = false;
    if (!context_->IntersectDomainWith(ct->int_prod().exprs(0),
                                       {-sqrt_max, sqrt_max}, &expr_reduced)) {
      return false;
    }
    if (expr_reduced) {
      context_->UpdateRuleStats("int_square: reduced expr domain.");
    }
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
      context_->UpdateRuleStats("int_square: fix variable to zero or one.");
      return RemoveConstraint(ct);
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
      ConstraintProto* constraint_for_false =
          context_->working_model->add_constraints();
      ConstraintProto* constraint_for_true =
          context_->working_model->add_constraints();
      constraint_for_true->add_enforcement_literal(boolean_linear->vars(0));
      constraint_for_false->add_enforcement_literal(
          NegatedRef(boolean_linear->vars(0)));
      LinearConstraintProto* linear_for_false =
          constraint_for_false->mutable_linear();
      LinearConstraintProto* linear_for_true =
          constraint_for_true->mutable_linear();

      linear_for_false->add_domain(0);
      linear_for_false->add_domain(0);
      AddLinearExpressionToLinearConstraint(
          *other_linear, boolean_linear->offset(), linear_for_false);
      AddLinearExpressionToLinearConstraint(ct->int_prod().target(), -1,
                                            linear_for_false);

      linear_for_true->add_domain(0);
      linear_for_true->add_domain(0);
      AddLinearExpressionToLinearConstraint(
          *other_linear, boolean_linear->offset() + boolean_linear->coeffs(0),
          linear_for_true);
      AddLinearExpressionToLinearConstraint(ct->int_prod().target(), -1,
                                            linear_for_true);
      context_->CanonicalizeLinearConstraint(constraint_for_false);
      context_->CanonicalizeLinearConstraint(constraint_for_true);
      context_->UpdateRuleStats("int_prod: boolean affine term");
      context_->UpdateNewConstraintsVariableUsage();
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

  // This is a Boolean constraint!
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

bool CpModelPresolver::PresolveIntDiv(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

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

  // Sometimes we have only a single variable appearing in the whole constraint.
  // If the domain is small enough, we can just restrict the domain and remove
  // the constraint.
  if (ct->enforcement_literal().empty() &&
      context_->ConstraintToVars(c).size() == 1) {
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
      (void)context_->IntersectDomainWith(var,
                                          Domain::FromValues(possible_values));
      context_->UpdateRuleStats("int_div: single variable");
      return RemoveConstraint(ct);
    }
  }

  // For now, we only presolve the case where the divisor is constant.
  if (!context_->IsFixed(div)) return false;

  const int64_t divisor = context_->FixedValue(div);

  // Trivial case one: target = expr / +/-1.
  if (divisor == 1 || divisor == -1) {
    LinearConstraintProto* const lin =
        context_->working_model->add_constraints()->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(0);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(target, -divisor, lin);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("int_div: rewrite to equality");
    return RemoveConstraint(ct);
  }

  // Reduce the domain of target.
  {
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
      CapAdd(1, CapProd(std::abs(divisor),
                        1 + std::abs(context_->FixedValue(target)))) !=
          std::numeric_limits<int64_t>::max()) {
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
    bool domain_modified = false;
    if (!context_->IntersectDomainWith(expr, expr_implied_domain,
                                       &domain_modified)) {
      return false;
    }
    if (domain_modified) {
      context_->UpdateRuleStats("int_div: target and divisor are fixed");
    } else {
      context_->UpdateRuleStats("int_div: always true");
    }
    return RemoveConstraint(ct);
  }

  // Linearize if everything is positive, and we have no overflow.
  // TODO(user): Deal with other cases where there is no change of
  // sign. We can also deal with target = cte, div variable.
  if (context_->MinOf(target) >= 0 && context_->MinOf(expr) >= 0 &&
      divisor > 1 &&
      CapProd(divisor, context_->MaxOf(target)) !=
          std::numeric_limits<int64_t>::max()) {
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
  return false;
}

bool CpModelPresolver::PresolveIntMod(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  // TODO(user): Presolve f(X) = g(X) % fixed_mod.
  const LinearExpressionProto target = ct->int_mod().target();
  const LinearExpressionProto expr = ct->int_mod().exprs(0);
  const LinearExpressionProto mod = ct->int_mod().exprs(1);

  if (context_->MinOf(target) > 0) {
    bool domain_changed = false;
    if (!context_->IntersectDomainWith(
            expr, Domain(0, std::numeric_limits<int64_t>::max()),
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
    if (!context_->IntersectDomainWith(
            expr, Domain(std::numeric_limits<int64_t>::min(), 0),
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
      target.vars(0) != expr.vars(0)) {
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
// canonicalize all linear subexpression in a generic way.
bool CpModelPresolver::ExploitEquivalenceRelations(int c, ConstraintProto* ct) {
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

bool CpModelPresolver::DivideLinearByGcd(ConstraintProto* ct) {
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
      return MarkConstraintAsFalse(ct);
    }
  }
  return false;
}

bool CpModelPresolver::CanonicalizeLinearExpression(
    const ConstraintProto& ct, LinearExpressionProto* exp) {
  return context_->CanonicalizeLinearExpression(ct.enforcement_literal(), exp);
}

bool CpModelPresolver::CanonicalizeLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (context_->ModelIsUnsat()) return false;

  if (ct->linear().domain().empty()) {
    context_->UpdateRuleStats("linear: no domain");
    return MarkConstraintAsFalse(ct);
  }

  bool changed = context_->CanonicalizeLinearConstraint(ct);
  changed |= DivideLinearByGcd(ct);

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

  return changed;
}

bool CpModelPresolver::RemoveSingletonInLinear(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear ||
      context_->ModelIsUnsat()) {
    return false;
  }

  absl::btree_set<int> index_to_erase;
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
  // TODO(user): Cover more case like dedicated algorithm to solve for a small
  // number of variable that are faster than the DP we use here.
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
        context_->UpdateRuleStats(
            "independent linear: no DP solution to simple constraint");
        return MarkConstraintAsFalse(ct);
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
          indicator = context_->NewBoolVar("indicator");
          auto* new_ct = context_->working_model->add_constraints();
          *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
          new_ct->mutable_bool_or()->add_literals(indicator);
          context_->UpdateNewConstraintsVariableUsage();
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
        // This make sure the domain of var is restricted and the objective
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

        context_->UpdateRuleStats("linear: singleton column define objective.");
        context_->MarkVariableAsRemoved(var);
        context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
        return RemoveConstraint(ct);
      }

      // On supportcase20, this transformation make the LP relaxation way worse.
      // TODO(user): understand why.
      if (true) continue;

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
  *context_->NewMappingConstraint(__FILE__, __LINE__) = *ct;

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
  return CanonicalizeLinear(ct);
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
// As an heuristic, we only test the smallest term or small primes 2, 3, and 5.
//
// We also handle the special case of having two non-zero literals modulo 2.
//
// TODO(user): Use more complex algo to detect all the cases? By splitting the
// constraint in two, and computing the gcd of each halves, we can reduce the
// problem to two problem of half size. So at least we can do it in O(n log n).
bool CpModelPresolver::PresolveLinearEqualityWithModulo(ConstraintProto* ct) {
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
  CHECK_EQ(ct->linear().vars().size(), 1);
  CHECK(RefIsPositive(ct->linear().vars(0)));

  const int var = ct->linear().vars(0);
  const Domain var_domain = context_->DomainOf(var);
  const Domain rhs = ReadDomainFromProto(ct->linear())
                         .InverseMultiplicationBy(ct->linear().coeffs(0))
                         .IntersectionWith(var_domain);
  if (rhs.IsEmpty()) {
    context_->UpdateRuleStats("linear1: infeasible");
    return MarkConstraintAsFalse(ct);
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

  // This is just an implication, lets convert it right away.
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
        context_->UpdateNewConstraintsVariableUsage();
        ct->Clear();
        context_->UpdateRuleStats("linear1: transformed to implication");
        return true;
      } else {
        if (context_->StoreLiteralImpliesVarEqValue(lit, var, value)) {
          // The domain is not actually modified, but we want to rescan the
          // constraints linked to this variable.
          context_->modified_domains.Set(var);
        }
        context_->UpdateNewConstraintsVariableUsage();
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
        context_->UpdateNewConstraintsVariableUsage();
        ct->Clear();
        context_->UpdateRuleStats("linear1: transformed to implication");
        return true;
      } else {
        if (context_->StoreLiteralImpliesVarNEqValue(lit, var, value)) {
          // The domain is not actually modified, but we want to rescan the
          // constraints linked to this variable.
          context_->modified_domains.Set(var);
        }
        context_->UpdateNewConstraintsVariableUsage();
      }
      return changed;
    }
  }

  return changed;
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
    if (!RefIsPositive(lit)) return false;

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
    } else if (ct->enforcement_literal().empty() &&
               !context_->CanBeUsedAsLiteral(var)) {
      // We currently only do that if there are no enforcement and we don't have
      // two Booleans as this can be presolved differently. We expand it into
      // two linear1 constraint that have a chance to be merged with other
      // "encoding" constraints.
      context_->UpdateRuleStats("linear2: contains a Boolean.");

      // lit => var \in rhs_if_true
      const Domain var_domain = context_->DomainOf(var);
      if (!var_domain.IsIncludedIn(rhs_if_true)) {
        ConstraintProto* new_ct = context_->working_model->add_constraints();
        new_ct->add_enforcement_literal(lit);
        new_ct->mutable_linear()->add_vars(var);
        new_ct->mutable_linear()->add_coeffs(1);
        FillDomainInProto(rhs_if_true.IntersectionWith(var_domain),
                          new_ct->mutable_linear());
      }

      // NegatedRef(lit) => var \in rhs_if_false
      if (!var_domain.IsIncludedIn(rhs_if_false)) {
        ConstraintProto* new_ct = context_->working_model->add_constraints();
        new_ct->add_enforcement_literal(NegatedRef(lit));
        new_ct->mutable_linear()->add_vars(var);
        new_ct->mutable_linear()->add_coeffs(1);
        FillDomainInProto(rhs_if_false.IntersectionWith(var_domain),
                          new_ct->mutable_linear());
      }

      context_->UpdateNewConstraintsVariableUsage();
      return RemoveConstraint(ct);
    }

    // Code below require equality.
    context_->UpdateRuleStats("TODO linear2: contains a Boolean.");
    return false;
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
      // Note that PresolveLinearEqualityWithModulo() will have the same effect.
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

bool CpModelPresolver::PresolveDiophantine(ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (ct->linear().vars().size() <= 1) return false;

  if (context_->ModelIsUnsat()) return false;

  const LinearConstraintProto& linear_constraint = ct->linear();
  if (linear_constraint.domain_size() != 2) return false;
  if (linear_constraint.domain(0) != linear_constraint.domain(1)) return false;

  std::vector<int64_t> lbs(linear_constraint.vars_size());
  std::vector<int64_t> ubs(linear_constraint.vars_size());
  for (int i = 0; i < linear_constraint.vars_size(); ++i) {
    lbs[i] = context_->MinOf(linear_constraint.vars(i));
    ubs[i] = context_->MaxOf(linear_constraint.vars(i));
  }
  DiophantineSolution diophantine_solution = SolveDiophantine(
      linear_constraint.coeffs(), linear_constraint.domain(0), lbs, ubs);

  if (!diophantine_solution.has_solutions) {
    context_->UpdateRuleStats("diophantine: equality has no solutions");
    return MarkConstraintAsFalse(ct);
  }
  if (diophantine_solution.no_reformulation_needed) return false;
  // Only first coefficients of kernel_basis elements and special_solution could
  // overflow int64_t due to the reduction applied in SolveDiophantineEquation,
  for (const std::vector<absl::int128>& b : diophantine_solution.kernel_basis) {
    if (!IsNegatableInt64(b[0])) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to int64_t overflow");
      return false;
    }
  }
  if (!IsNegatableInt64(diophantine_solution.special_solution[0])) {
    context_->UpdateRuleStats(
        "diophantine: couldn't apply due to int64_t overflow");
    return false;
  }

  const int num_replaced_variables =
      static_cast<int>(diophantine_solution.special_solution.size());
  const int num_new_variables =
      static_cast<int>(diophantine_solution.kernel_vars_lbs.size());
  DCHECK_EQ(num_new_variables + 1, num_replaced_variables);
  for (int i = 0; i < num_new_variables; ++i) {
    if (!IsNegatableInt64(diophantine_solution.kernel_vars_lbs[i]) ||
        !IsNegatableInt64(diophantine_solution.kernel_vars_ubs[i])) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to int64_t overflow");
      return false;
    }
  }
  // TODO(user): Make sure the newly generated linear constraint
  // satisfy our no-overflow precondition on the min/max activity.
  // We should check that the model still satisfy conditions in
  // 3/ortools/sat/cp_model_checker.cc;l=165;bpv=0

  // Create new variables.
  std::vector<int> new_variables(num_new_variables);
  for (int i = 0; i < num_new_variables; ++i) {
    new_variables[i] = context_->working_model->variables_size();
    IntegerVariableProto* var = context_->working_model->add_variables();
    var->add_domain(
        static_cast<int64_t>(diophantine_solution.kernel_vars_lbs[i]));
    var->add_domain(
        static_cast<int64_t>(diophantine_solution.kernel_vars_ubs[i]));
    if (!ct->name().empty()) {
      var->set_name(absl::StrCat("u_diophantine_", ct->name(), "_", i));
    }
  }

  // For i = 0, ..., num_replaced_variables - 1, creates
  //  x[i] = special_solution[i]
  //        + sum(kernel_basis[k][i]*y[k], max(1, i) <= k < vars.size - 1)
  // where:
  //  y[k] is the newly created variable if 0 <= k < num_new_variables
  //  y[k] = x[index_permutation[k + 1]] otherwise.
  for (int i = 0; i < num_replaced_variables; ++i) {
    ConstraintProto* identity = context_->working_model->add_constraints();
    LinearConstraintProto* lin = identity->mutable_linear();
    if (!ct->name().empty()) {
      identity->set_name(absl::StrCat("c_diophantine_", ct->name(), "_", i));
    }
    *identity->mutable_enforcement_literal() = ct->enforcement_literal();
    lin->add_vars(
        linear_constraint.vars(diophantine_solution.index_permutation[i]));
    lin->add_coeffs(1);
    lin->add_domain(
        static_cast<int64_t>(diophantine_solution.special_solution[i]));
    lin->add_domain(
        static_cast<int64_t>(diophantine_solution.special_solution[i]));
    for (int j = std::max(1, i); j < num_replaced_variables; ++j) {
      lin->add_vars(new_variables[j - 1]);
      lin->add_coeffs(
          -static_cast<int64_t>(diophantine_solution.kernel_basis[j - 1][i]));
    }
    for (int j = num_replaced_variables; j < linear_constraint.vars_size();
         ++j) {
      lin->add_vars(
          linear_constraint.vars(diophantine_solution.index_permutation[j]));
      lin->add_coeffs(
          -static_cast<int64_t>(diophantine_solution.kernel_basis[j - 1][i]));
    }

    // TODO(user): The domain in the proto are not necessarily up to date so
    // this might be stricter than necessary. Fix? It shouldn't matter too much
    // though.
    if (PossibleIntegerOverflow(*(context_->working_model), lin->vars(),
                                lin->coeffs())) {
      context_->UpdateRuleStats(
          "diophantine: couldn't apply due to overflowing activity of new "
          "constraints");
      // Cancel working_model changes.
      context_->working_model->mutable_constraints()->DeleteSubrange(
          context_->working_model->constraints_size() - i - 1, i + 1);
      context_->working_model->mutable_variables()->DeleteSubrange(
          context_->working_model->variables_size() - num_new_variables,
          num_new_variables);
      return false;
    }
  }
  context_->InitializeNewDomains();

  if (VLOG_IS_ON(2)) {
    std::string log_eq = absl::StrCat(linear_constraint.domain(0), " = ");
    const int terms_to_show = std::min<int>(15, linear_constraint.vars_size());
    for (int i = 0; i < terms_to_show; ++i) {
      if (i > 0) absl::StrAppend(&log_eq, " + ");
      absl::StrAppend(
          &log_eq,
          linear_constraint.coeffs(diophantine_solution.index_permutation[i]),
          " x",
          linear_constraint.vars(diophantine_solution.index_permutation[i]));
    }
    if (terms_to_show < linear_constraint.vars_size()) {
      absl::StrAppend(&log_eq, "+ ... (", linear_constraint.vars_size(),
                      " terms)");
    }
    VLOG(2) << "[Diophantine] " << log_eq;
  }

  context_->UpdateRuleStats("diophantine: reformulated equality");
  context_->UpdateNewConstraintsVariableUsage();
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
// TODO(user): One easy algo is to first remove all enforcement term (even
// non-Boolean one) before applying the algo here and then re-linearize the
// non-Boolean terms.
void CpModelPresolver::TryToReduceCoefficientsOfLinearConstraint(
    int c, ConstraintProto* ct) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return;
  if (context_->ModelIsUnsat()) return;

  // Only consider "simple" constraints.
  const LinearConstraintProto& lin = ct->linear();
  if (lin.domain().size() != 2) return;
  const Domain rhs = ReadDomainFromProto(lin);

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
  if (lb_sum > rhs.Max() || rhs.Min() > ub_sum) {
    (void)MarkConstraintAsFalse(ct);
    context_->UpdateConstraintVariableUsage(c);
    return;
  }
  const IntegerValue rhs_ub(CapSub(rhs.Max(), lb_sum));
  const IntegerValue rhs_lb(CapSub(ub_sum, rhs.Min()));
  const bool use_ub = max_variation > rhs_ub;
  const bool use_lb = max_variation > rhs_lb;
  if (!use_ub && !use_lb) {
    (void)RemoveConstraint(ct);
    context_->UpdateConstraintVariableUsage(c);
    return;
  }

  // No point doing more work for constraint with all coeff at +/-1.
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

    // We regroup all term with the same coefficient into one.
    //
    // TODO(user): I am not sure there is no possible simplification across two
    // term with the same coeff, but it should be rare if it ever happens.
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
        (void)MarkConstraintAsFalse(ct);
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
      (void)MarkConstraintAsFalse(ct);
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
      (void)MarkConstraintAsFalse(ct);
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

int FixLiteralFromSet(const absl::flat_hash_set<int>& literals_at_true,
                      LinearConstraintProto* linear) {
  int new_size = 0;
  int num_fixed = 0;
  const int num_terms = linear->vars().size();
  int64_t shift = 0;
  for (int i = 0; i < num_terms; ++i) {
    const int var = linear->vars(i);
    const int64_t coeff = linear->coeffs(i);
    if (literals_at_true.contains(var)) {
      // Var is at one.
      shift += coeff;
      ++num_fixed;
    } else if (!literals_at_true.contains(NegatedRef(var))) {
      linear->set_vars(new_size, var);
      linear->set_coeffs(new_size, coeff);
      ++new_size;
    } else {
      ++num_fixed;
      // Else the variable is at zero.
    }
  }
  linear->mutable_vars()->Truncate(new_size);
  linear->mutable_coeffs()->Truncate(new_size);
  if (shift != 0) {
    FillDomainInProto(ReadDomainFromProto(*linear).AdditionWith(Domain(-shift)),
                      linear);
  }
  return num_fixed;
}

}  // namespace

void CpModelPresolver::ProcessAtMostOneAndLinear() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  ActivityBoundHelper amo_in_linear;
  amo_in_linear.AddAllAtMostOnes(*context_->working_model);

  int num_changes = 0;
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() != ConstraintProto::kLinear) continue;

    // We loop if the constraint changed.
    for (int i = 0; i < 5; ++i) {
      const int old_size = ct->linear().vars().size();
      const int old_enf_size = ct->enforcement_literal().size();
      ProcessOneLinearWithAmo(c, ct, &amo_in_linear);
      if (context_->ModelIsUnsat()) return;
      if (ct->constraint_case() != ConstraintProto::kLinear) break;
      if (ct->linear().vars().size() == old_size &&
          ct->enforcement_literal().size() == old_enf_size) {
        break;
      }
      ++num_changes;
    }
  }

  timer.AddCounter("num_changes", num_changes);
}

// TODO(user): Similarly amo and bool_or intersection or amo and enforcement
// literals list can be presolved.
//
// TODO(user): This is stronger than the fully included case. Avoid having
// the second code?
void CpModelPresolver::ProcessOneLinearWithAmo(int ct_index,
                                               ConstraintProto* ct,
                                               ActivityBoundHelper* helper) {
  if (ct->constraint_case() != ConstraintProto::kLinear) return;
  if (ct->linear().vars().size() <= 1) return;

  tmp_terms_.clear();
  temp_ct_.Clear();
  Domain non_boolean_domain(0);
  const int initial_size = ct->linear().vars().size();
  int64_t min_magnitude = std::numeric_limits<int64_t>::max();
  int64_t max_magnitude = 0;
  for (int i = 0; i < initial_size; ++i) {
    // TODO(user): Just do not use negative reference in linear!
    int ref = ct->linear().vars(i);
    int64_t coeff = ct->linear().coeffs(i);
    if (!RefIsPositive(ref)) {
      ref = NegatedRef(ref);
      coeff = -coeff;
    }
    if (context_->CanBeUsedAsLiteral(ref)) {
      tmp_terms_.push_back({ref, coeff});
      min_magnitude = std::min(min_magnitude, std::abs(coeff));
      max_magnitude = std::max(max_magnitude, std::abs(coeff));
    } else {
      non_boolean_domain =
          non_boolean_domain
              .AdditionWith(
                  context_->DomainOf(ref).ContinuousMultiplicationBy(coeff))
              .RelaxIfTooComplex();
      temp_ct_.mutable_linear()->add_vars(ref);
      temp_ct_.mutable_linear()->add_coeffs(coeff);
    }
  }

  // Skip if there are no Booleans.
  if (tmp_terms_.empty()) return;

  // Detect encoded AMO.
  //
  // TODO(user): Support more coefficient strengthening cases.
  // For instance on neos-954925.pb.gz we have stuff like:
  //    20 * (AMO1 + AMO2) - [coeff in 48 to 53] >= -15
  // this is really AMO1 + AMO2 - 2 * AMO3 >= 0.
  // Maybe if we reify the AMO to exactly one, this is visible since large
  // AMO can be rewriten with single variable (1 - extra var in exactly one).
  const Domain rhs = ReadDomainFromProto(ct->linear());
  if (non_boolean_domain == Domain(0) && rhs.NumIntervals() == 1 &&
      min_magnitude < max_magnitude) {
    int64_t min_activity = 0;
    int64_t max_activity = 0;
    for (const auto [ref, coeff] : tmp_terms_) {
      if (coeff > 0) {
        max_activity += coeff;
      } else {
        min_activity += coeff;
      }
    }
    const int64_t transformed_rhs = rhs.Max() - min_activity;
    if (min_activity >= rhs.Min() && max_magnitude <= transformed_rhs) {
      std::vector<int> literals;
      for (const auto [ref, coeff] : tmp_terms_) {
        if (coeff + min_magnitude > transformed_rhs) continue;
        literals.push_back(coeff > 0 ? ref : NegatedRef(ref));
      }
      if (helper->IsAmo(literals)) {
        // We actually have an at-most-one in disguise.
        context_->UpdateRuleStats("linear + amo: detect hidden AMO");
        int64_t shift = 0;
        for (int i = 0; i < initial_size; ++i) {
          CHECK(RefIsPositive(ct->linear().vars(i)));
          if (ct->linear().coeffs(i) > 0) {
            ct->mutable_linear()->set_coeffs(i, 1);
          } else {
            ct->mutable_linear()->set_coeffs(i, -1);
            shift -= 1;
          }
        }
        FillDomainInProto(Domain(shift, shift + 1), ct->mutable_linear());
        return;
      }
    }
  }

  // Get more precise activity estimate based on at most one and heuristics.
  const int64_t min_bool_activity =
      helper->ComputeMinActivity(tmp_terms_, &conditional_mins_);
  const int64_t max_bool_activity =
      helper->ComputeMaxActivity(tmp_terms_, &conditional_maxs_);

  // Detect trivially true/false constraint under these new bounds.
  // TODO(user): relax rhs if only one side is trivial.
  const Domain activity = non_boolean_domain.AdditionWith(
      Domain(min_bool_activity, max_bool_activity));
  if (activity.IntersectionWith(rhs).IsEmpty()) {
    // Note that this covers min_bool_activity > max_bool_activity.
    context_->UpdateRuleStats("linear + amo: infeasible linear constraint");
    (void)MarkConstraintAsFalse(ct);
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  } else if (activity.IsIncludedIn(rhs)) {
    context_->UpdateRuleStats("linear + amo: trivial linear constraint");
    ct->Clear();
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  }

  // We can use the new bound to propagate other terms.
  if (ct->enforcement_literal().empty() && !temp_ct_.linear().vars().empty()) {
    FillDomainInProto(
        rhs.AdditionWith(
            Domain(min_bool_activity, max_bool_activity).Negation()),
        temp_ct_.mutable_linear());
    if (!PropagateDomainsInLinear(/*ct_index=*/-1, &temp_ct_)) {
      return;
    }
  }

  // Extract enforcement or fix literal.
  //
  // TODO(user): Do not use domain fonction, can be slow.
  //
  // TODO(user): Actually we might make the linear relaxation worse by
  // extracting some of these enforcement, as those can be "lifted" booleans. We
  // currently deal with that in RemoveEnforcementThatMakesConstraintTrivial(),
  // but that might not be the most efficient.
  //
  // TODO(user): Another reason for making the LP worse is that if we replace
  // part of the constraint via FindBig*LinearOverlap() then our activity bounds
  // might not be as precise when we will linearize this constraint again.
  std::vector<int> new_enforcement;
  std::vector<int> must_be_true;
  for (int i = 0; i < tmp_terms_.size(); ++i) {
    const int ref = tmp_terms_[i].first;

    const Domain bool0(conditional_mins_[i][0], conditional_maxs_[i][0]);
    const Domain activity0 = bool0.AdditionWith(non_boolean_domain);
    if (activity0.IntersectionWith(rhs).IsEmpty()) {
      // Must be 1.
      must_be_true.push_back(ref);
    } else if (activity0.IsIncludedIn(rhs)) {
      // Trivial constraint on 0.
      new_enforcement.push_back(ref);
    }

    const Domain bool1(conditional_mins_[i][1], conditional_maxs_[i][1]);
    const Domain activity1 = bool1.AdditionWith(non_boolean_domain);
    if (activity1.IntersectionWith(rhs).IsEmpty()) {
      // Must be 0.
      must_be_true.push_back(NegatedRef(ref));
    } else if (activity1.IsIncludedIn(rhs)) {
      // Trivial constraint on 1.
      new_enforcement.push_back(NegatedRef(ref));
    }
  }

  // Note that both list can be non empty, if for instance we have small * X +
  // big * Y + ... <= rhs and amo(X, Y). We could see that Y can never be true
  // and if X is true, then the constraint could be trivial.
  //
  // So we fix things first if we can.
  if (ct->enforcement_literal().empty() && !must_be_true.empty()) {
    // Note that our logic to do more presolve iteration depends on the
    // number of rule applied, so it is important to count this correctly.
    context_->UpdateRuleStats("linear + amo: fixed literal",
                              must_be_true.size());
    for (const int lit : must_be_true) {
      if (!context_->SetLiteralToTrue(lit)) return;
    }
    CanonicalizeLinear(ct);
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  }

  if (!new_enforcement.empty()) {
    context_->UpdateRuleStats("linear + amo: extracted enforcement literal",
                              new_enforcement.size());
    for (const int ref : new_enforcement) {
      ct->add_enforcement_literal(ref);
    }
  }

  if (!ct->enforcement_literal().empty()) {
    const int old_enf_size = ct->enforcement_literal().size();
    if (!helper->PresolveEnforcement(ct->linear().vars(), ct, &temp_set_)) {
      context_->UpdateRuleStats("linear + amo: infeasible enforcement");
      ct->Clear();
      context_->UpdateConstraintVariableUsage(ct_index);
      return;
    }
    if (ct->enforcement_literal().size() < old_enf_size) {
      context_->UpdateRuleStats("linear + amo: simplified enforcement list");
      context_->UpdateConstraintVariableUsage(ct_index);
    }

    for (const int lit : must_be_true) {
      if (temp_set_.contains(NegatedRef(lit))) {
        // A literal must be true but is incompatible with what the enforcement
        // implies. The constraint must be false!
        context_->UpdateRuleStats(
            "linear + amo: advanced infeasible linear constraint");
        (void)MarkConstraintAsFalse(ct);
        context_->UpdateConstraintVariableUsage(ct_index);
        return;
      }
    }

    // TODO(user): do that in more cases?
    if (ct->enforcement_literal().size() == 1 && !must_be_true.empty()) {
      // Add implication, and remove literal from the constraint in this case.
      // To remove them, we just add them to temp_set_ and FixLiteralFromSet()
      // will take care of it.
      context_->UpdateRuleStats("linear + amo: added implications");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      for (const int lit : must_be_true) {
        new_ct->mutable_bool_and()->add_literals(lit);
        temp_set_.insert(lit);
      }
      context_->UpdateNewConstraintsVariableUsage();
    }

    const int num_fixed = FixLiteralFromSet(temp_set_, ct->mutable_linear());
    if (num_fixed > new_enforcement.size()) {
      context_->UpdateRuleStats(
          "linear + amo: fixed literal implied by enforcement");
    }
    if (num_fixed > 0) {
      context_->UpdateConstraintVariableUsage(ct_index);
    }
  }

  if (ct->linear().vars().empty()) {
    context_->UpdateRuleStats("linear + amo: empty after processing");
    PresolveSmallLinear(ct);
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  }

  // If the constraint is of size 1 or 2, we re-presolve it right away.
  if (initial_size != ct->linear().vars().size() && PresolveSmallLinear(ct)) {
    context_->UpdateConstraintVariableUsage(ct_index);
    if (ct->constraint_case() != ConstraintProto::kLinear) return;
  }

  // Detect enforcement literal that could actually be lifted, and as such can
  // just be removed from the enforcement list. Ideally, during relaxation we
  // would lift such Boolean again.
  //
  // Note that this code is independent from anything above.
  if (!ct->enforcement_literal().empty()) {
    // TODO(user): remove duplication with code above?
    tmp_terms_.clear();
    Domain non_boolean_domain(0);
    const int num_ct_terms = ct->linear().vars().size();
    for (int i = 0; i < num_ct_terms; ++i) {
      const int ref = ct->linear().vars(i);
      const int64_t coeff = ct->linear().coeffs(i);
      CHECK(RefIsPositive(ref));
      if (context_->CanBeUsedAsLiteral(ref)) {
        tmp_terms_.push_back({ref, coeff});
      } else {
        non_boolean_domain =
            non_boolean_domain
                .AdditionWith(
                    context_->DomainOf(ref).ContinuousMultiplicationBy(coeff))
                .RelaxIfTooComplex();
      }
    }
    const int num_removed = helper->RemoveEnforcementThatMakesConstraintTrivial(
        tmp_terms_, non_boolean_domain, ReadDomainFromProto(ct->linear()), ct);
    if (num_removed > 0) {
      context_->UpdateRuleStats("linear + amo: removed enforcement literal",
                                num_removed);
      context_->UpdateConstraintVariableUsage(ct_index);
    }
  }
}

bool CpModelPresolver::PropagateDomainsInLinear(int ct_index,
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
    context_->UpdateRuleStats("linear: infeasible");
    return MarkConstraintAsFalse(ct);
  }
  if (rhs != old_rhs) {
    if (ct_index != -1) context_->UpdateRuleStats("linear: simplified rhs");
  }
  FillDomainInProto(rhs, ct->mutable_linear());

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
    const bool is_in_objective = context_->VarToConstraints(var).contains(-1);
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
      if (context_->working_model->constraints(c).constraint_case() !=
          ConstraintProto::kLinear) {
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
      others.push_back(c);
    }
    if (abort) continue;

    // If the domain implied by this constraint is the same as the current
    // domain of the variable, this variable is implied free. Otherwise, we
    // check if the intersection with the domain implied by another constraint
    // make it implied free.
    if (context_->DomainOf(var) != new_domain) {
      // We only do that for doubleton because we don't want the propagation to
      // be less strong. If we were to replace this variable in other constraint
      // the implied bound from the linear expression might not be as good.
      //
      // TODO(user): We still substitute even if this happens in the objective
      // though. Is that good?
      if (others.size() != 1) continue;
      const ConstraintProto& other_ct =
          context_->working_model->constraints(others.front());
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
      continue;
    }

    // Do the actual substitution.
    ConstraintProto copy_if_we_abort;
    absl::c_sort(others);
    for (const int c : others) {
      // TODO(user): The copy is needed to have a simpler overflow-checking
      // code were we check once the substitution is done. If needed we could
      // optimize that, but with more code.
      copy_if_we_abort = context_->working_model->constraints(c);

      // In some corner cases, this might violate our overflow precondition or
      // even create an overflow. The danger is limited since the range of the
      // linear expression used in the definition do not exceed the domain of
      // the variable we substitute. But this is not the case for the doubleton
      // case above.
      if (!SubstituteVariable(
              var, var_coeff, *ct,
              context_->working_model->mutable_constraints(c))) {
        // The function above can fail because of overflow, but also if the
        // constraint was not canonicalized yet and the variable is actually not
        // there (we have var - var for instance).
        //
        // TODO(user): we canonicalize it right away, but I am not sure it is
        // really needed.
        if (CanonicalizeLinear(
                context_->working_model->mutable_constraints(c))) {
          context_->UpdateConstraintVariableUsage(c);
        }
        abort = true;
        break;
      }

      if (PossibleIntegerOverflow(
              *context_->working_model,
              context_->working_model->constraints(c).linear().vars(),
              context_->working_model->constraints(c).linear().coeffs())) {
        // Revert the change in this case.
        *context_->working_model->mutable_constraints(c) = copy_if_we_abort;
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
  if (recanonicalize) return CanonicalizeLinear(ct);
  return false;
}

// The constraint from its lower value is sum positive_coeff * X <= rhs.
// If from_lower_bound is false, then it is the constraint from its upper value.
void CpModelPresolver::LowerThanCoeffStrengthening(bool from_lower_bound,
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
        context_->UpdateRuleStats("linear: fix variable to its bound.");
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
          "linear: coefficient strengthening by increasing it.");
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
void CpModelPresolver::ExtractEnforcementLiteralFromLinearConstraint(
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
  int64_t min_coeff_magnitude = std::numeric_limits<int64_t>::max();
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
  // be completely rewriten to 2 * (enforcement_part) + sum var >= 2 which is
  // what happen eventually when bound is even, but not if it is odd currently.
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
        // We keep this term but reduces its coeff.
        // This is only for the case where only_extract_booleans == true.
        new_magnitude = threshold;
        context_->UpdateRuleStats("linear: coefficient strenghtening.");
      } else if (coeff > second_threshold && coeff < threshold) {
        // This cover the special case where one big + on small is enough
        // to satisfy the constraint, we can reduce the big.
        new_magnitude = second_threshold;
        context_->UpdateRuleStats(
            "linear: advanced coefficient strenghtening.");
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
    context_->UpdateNewConstraintsVariableUsage();
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
  if (ct->constraint_case() != ConstraintProto::kLinear) return false;
  if (context_->ModelIsUnsat()) return false;

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
    // TODO(user): Support enforced exactly one.
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

  if (ct->enforcement_literal().empty()) {
    bool domain_changed = false;
    // Size can't be negative.
    if (!context_->IntersectDomainWith(
            interval->size(), Domain(0, std::numeric_limits<int64_t>::max()),
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
  if (context_->ConstraintVariableGraphIsUpToDate() &&
      context_->IntervalUsage(c) == 0) {
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

bool CpModelPresolver::PresolveElement(int c, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;

  if (ct->element().exprs().empty()) {
    context_->UpdateRuleStats("element: empty array");
    return context_->NotifyThatModelIsUnsat();
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

  // TODO(user): think about this once we do have such constraint.
  if (HasEnforcementLiteral(*ct)) return false;

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
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    new_ct->mutable_linear()->add_domain(0);
    new_ct->mutable_linear()->add_domain(0);
    AddLinearExpressionToLinearConstraint(target, 1, new_ct->mutable_linear());
    AddLinearExpressionToLinearConstraint(ct->element().exprs(index_value), -1,
                                          new_ct->mutable_linear());
    context_->CanonicalizeLinearConstraint(new_ct);
    context_->UpdateNewConstraintsVariableUsage();
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

    // Cleanup the array: clear unreached expressions.
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

      // The target domain can be reduced if it shares its variable with the
      // index.
      Domain reduced_target_domain = target_domain;
      if (target.vars_size() == 1 && target.vars(0) == index_var) {
        reduced_target_domain =
            Domain(AffineExpressionValueAt(target, index_var_value));
      }

      // TODO(user): Implement a more precise test here.
      if (reduced_target_domain
              .IntersectionWith(context_->DomainSuperSetOf(expr))
              .IsEmpty()) {
        ct->mutable_element()->mutable_exprs(index_value)->Clear();
        changed = true;
      } else {
        possible_index_var_values.push_back(index_var_value);
      }
    }
    if (possible_index_var_values.size() < index_var_domain.Size()) {
      if (!context_->IntersectDomainWith(
              index_var, Domain::FromValues(possible_index_var_values))) {
        return true;
      }
      context_->UpdateRuleStats("element: reduced index domain ");
      // If the index is fixed, this is a equality constraint.
      if (context_->IsFixed(index)) {
        ConstraintProto* const eq = context_->working_model->add_constraints();
        eq->mutable_linear()->add_domain(0);
        eq->mutable_linear()->add_domain(0);
        AddLinearExpressionToLinearConstraint(target, 1, eq->mutable_linear());
        AddLinearExpressionToLinearConstraint(
            ct->element().exprs(context_->FixedValue(index)), -1,
            eq->mutable_linear());
        context_->CanonicalizeLinearConstraint(eq);
        context_->UpdateNewConstraintsVariableUsage();
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

  // If the accessible part of the array is made of a single constant value,
  // then we do not care about the index. And, because of the previous target
  // domain reduction, the target is also fixed.
  if (all_constants && context_->IsFixed(target)) {
    context_->UpdateRuleStats("element: one value array");
    return RemoveConstraint(ct);
  }

  // Special case when the index is boolean, and the array does not contain
  // variables.
  if (context_->MinOf(index) == 0 && context_->MaxOf(index) == 1 &&
      all_constants) {
    const int64_t v0 = context_->FixedValue(ct->element().exprs(0));
    const int64_t v1 = context_->FixedValue(ct->element().exprs(1));

    ConstraintProto* const eq = context_->working_model->add_constraints();
    eq->mutable_linear()->add_domain(v0);
    eq->mutable_linear()->add_domain(v0);
    AddLinearExpressionToLinearConstraint(target, 1, eq->mutable_linear());
    AddLinearExpressionToLinearConstraint(index, v0 - v1, eq->mutable_linear());
    context_->CanonicalizeLinearConstraint(eq);
    context_->UpdateNewConstraintsVariableUsage();
    context_->UpdateRuleStats("element: linearize constant element of size 2");
    return RemoveConstraint(ct);
  }

  // If a variable (target or index) appears only in this constraint, it does
  // not necessarily mean that we can remove the constraint, as the variable
  // can be used multiple times in the element. So let's count the local
  // uses of each variable.
  //
  // TODO(user): now that we used fixed values for these case, this is no longer
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
      local_var_occurrence_counter.at(index_var) == 1) {
    if (all_constants) {
      // This constraint is just here to reduce the domain of the target! We can
      // add it to the mapping_model to reconstruct the index value during
      // postsolve and get rid of it now.
      context_->UpdateRuleStats(
          "element: removed  as the index is not used elsewhere");
      context_->MarkVariableAsRemoved(index_var);
      context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("TODO element: index not used elsewhere");
    }
  }

  if (target.vars_size() == 1 && !context_->IsFixed(target.vars(0)) &&
      context_->VariableIsUniqueAndRemovable(target.vars(0)) &&
      local_var_occurrence_counter.at(target.vars(0)) == 1) {
    if (all_included_in_target_domain && std::abs(target.coeffs(0)) == 1) {
      context_->UpdateRuleStats(
          "element: removed as the target is not used elsewhere");
      context_->MarkVariableAsRemoved(target.vars(0));
      context_->NewMappingConstraint(*ct, __FILE__, __LINE__);
      return RemoveConstraint(ct);
    } else {
      context_->UpdateRuleStats("TODO element: target not used elsewhere");
    }
  }

  return changed;
}

bool CpModelPresolver::PresolveTable(ConstraintProto* ct) {
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
      context_->UpdateRuleStats("table: positive table without tuples");
      return MarkConstraintAsFalse(ct);
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
      context_->UpdateRuleStats("table: always false");
      return MarkConstraintAsFalse(ct);
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
  // Note however that currently the negated table do not propagate as much as
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
void ExtractClauses(bool merge_into_bool_and,
                    const std::vector<int>& index_mapping,
                    const ClauseContainer& container, CpModelProto* proto) {
  // We regroup the "implication" into bool_and to have a more concise proto and
  // also for nicer information about the number of binary clauses.
  //
  // Important: however, we do not do that for the model used during postsolving
  // since the order of the constraints might be important there depending on
  // how we perform the postsolve.
  absl::flat_hash_map<int, int> ref_to_bool_and;
  for (int i = 0; i < container.NumClauses(); ++i) {
    const std::vector<Literal>& clause = container.Clause(i);
    if (clause.empty()) continue;

    // bool_and.
    //
    // TODO(user): Be smarter in how we regroup clause of size 2?
    if (merge_into_bool_and && clause.size() == 2) {
      const int var_a = index_mapping[clause[0].Variable().value()];
      const int var_b = index_mapping[clause[1].Variable().value()];
      const int ref_a = clause[0].IsPositive() ? var_a : NegatedRef(var_a);
      const int ref_b = clause[1].IsPositive() ? var_b : NegatedRef(var_b);
      AddImplication(NegatedRef(ref_a), ref_b, proto, &ref_to_bool_and);
      continue;
    }

    // bool_or.
    ConstraintProto* ct = proto->add_constraints();
    ct->mutable_bool_or()->mutable_literals()->Reserve(clause.size());
    for (const Literal l : clause) {
      const int var = index_mapping[l.Variable().value()];
      if (l.IsPositive()) {
        ct->mutable_bool_or()->add_literals(var);
      } else {
        ct->mutable_bool_or()->add_literals(NegatedRef(var));
      }
    }
  }
}

}  // namespace

bool CpModelPresolver::PresolveNoOverlap(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
  NoOverlapConstraintProto* proto = ct->mutable_no_overlap();
  bool changed = false;

  // Filter out absent intervals. Process duplicate intervals.
  {
    // Collect duplicate intervals.
    absl::flat_hash_set<int> visited_intervals;
    absl::flat_hash_set<int> duplicate_intervals;
    for (const int interval_index : proto->intervals()) {
      if (context_->ConstraintIsInactive(interval_index)) continue;
      if (!visited_intervals.insert(interval_index).second) {
        duplicate_intervals.insert(interval_index);
      }
    }

    const int initial_num_intervals = proto->intervals_size();
    int new_size = 0;
    visited_intervals.clear();

    for (int i = 0; i < initial_num_intervals; ++i) {
      const int interval_index = proto->intervals(i);
      if (context_->ConstraintIsInactive(interval_index)) continue;

      if (duplicate_intervals.contains(interval_index)) {
        // Once processed, we can always remove further duplicates.
        if (!visited_intervals.insert(interval_index).second) continue;

        ConstraintProto* interval_ct =
            context_->working_model->mutable_constraints(interval_index);

        // Case 1: size > 0. Interval must be unperformed.
        if (context_->SizeMin(interval_index) > 0) {
          if (!MarkConstraintAsFalse(interval_ct)) {
            return false;
          }
          context_->UpdateConstraintVariableUsage(interval_index);
          context_->UpdateRuleStats(
              "no_overlap: unperform duplicate non zero-sized intervals");
          // We can remove the interval from the no_overlap.
          continue;
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
            ConstraintProto* size_eq_zero =
                context_->working_model->add_constraints();
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
      proto->add_intervals(context_->working_model->constraints_size());
      IntervalConstraintProto* new_interval =
          context_->working_model->add_constraints()->mutable_interval();
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

  // Unfortunately, because we want all intervals to appear before a constraint
  // that uses them, we need to move the constraint last when we merged constant
  // intervals.
  if (move_constraint_last) {
    changed = true;
    *context_->working_model->add_constraints() = *ct;
    context_->UpdateNewConstraintsVariableUsage();
    return RemoveConstraint(ct);
  }

  return changed;
}

bool CpModelPresolver::PresolveNoOverlap2D(int /*c*/, ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) {
    return false;
  }

  const NoOverlap2DConstraintProto& proto = ct->no_overlap_2d();
  const int initial_num_boxes = proto.x_intervals_size();

  bool x_constant = true;
  bool y_constant = true;
  bool has_zero_sized_interval = false;
  bool has_potential_zero_sized_interval = false;

  // Filter absent boxes.
  int new_size = 0;
  std::vector<Rectangle> bounding_boxes, fixed_boxes;
  std::vector<RectangleInRange> non_fixed_boxes;
  std::vector<int> active_boxes;
  absl::flat_hash_set<int> fixed_item_indexes;
  for (int i = 0; i < proto.x_intervals_size(); ++i) {
    const int x_interval_index = proto.x_intervals(i);
    const int y_interval_index = proto.y_intervals(i);

    if (context_->ConstraintIsInactive(x_interval_index) ||
        context_->ConstraintIsInactive(y_interval_index)) {
      continue;
    }

    ct->mutable_no_overlap_2d()->set_x_intervals(new_size, x_interval_index);
    ct->mutable_no_overlap_2d()->set_y_intervals(new_size, y_interval_index);
    bounding_boxes.push_back(
        {IntegerValue(context_->StartMin(x_interval_index)),
         IntegerValue(context_->EndMax(x_interval_index)),
         IntegerValue(context_->StartMin(y_interval_index)),
         IntegerValue(context_->EndMax(y_interval_index))});
    active_boxes.push_back(new_size);
    if (context_->IntervalIsConstant(x_interval_index) &&
        context_->IntervalIsConstant(y_interval_index) &&
        context_->SizeMax(x_interval_index) > 0 &&
        context_->SizeMax(y_interval_index) > 0) {
      fixed_boxes.push_back(bounding_boxes.back());
      fixed_item_indexes.insert(new_size);
    } else {
      non_fixed_boxes.push_back(
          {.box_index = new_size,
           .bounding_area = bounding_boxes.back(),
           .x_size = context_->SizeMin(x_interval_index),
           .y_size = context_->SizeMin(y_interval_index)});
    }
    new_size++;

    if (x_constant && !context_->IntervalIsConstant(x_interval_index)) {
      x_constant = false;
    }
    if (y_constant && !context_->IntervalIsConstant(y_interval_index)) {
      y_constant = false;
    }
    if (context_->SizeMax(x_interval_index) == 0 ||
        context_->SizeMax(y_interval_index) == 0) {
      has_zero_sized_interval = true;
    }
    if (context_->SizeMin(x_interval_index) == 0 ||
        context_->SizeMin(y_interval_index) == 0) {
      has_potential_zero_sized_interval = true;
    }
  }

  std::vector<absl::Span<int>> components = GetOverlappingRectangleComponents(
      bounding_boxes, absl::MakeSpan(active_boxes));
  // The result of GetOverlappingRectangleComponents() omit singleton components
  // thus to check whether a graph is fully connected we must check also the
  // size of the unique component.
  const bool is_fully_connected =
      (components.size() == 1 && components[0].size() == active_boxes.size()) ||
      (active_boxes.size() <= 1);
  if (!is_fully_connected) {
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

  // TODO(user): handle this case. See issue #4068.
  if (!has_zero_sized_interval && (x_constant || y_constant)) {
    context_->UpdateRuleStats(
        "no_overlap_2d: a dimension is constant, splitting into many "
        "no_overlaps");
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

  // We check if the fixed boxes are not overlapping so downstream code can
  // assume it to be true.
  for (int i = 0; i < fixed_boxes.size(); ++i) {
    const Rectangle& fixed_box = fixed_boxes[i];
    for (int j = i + 1; j < fixed_boxes.size(); ++j) {
      const Rectangle& other_fixed_box = fixed_boxes[j];
      if (!fixed_box.IsDisjoint(other_fixed_box)) {
        return context_->NotifyThatModelIsUnsat(
            "Two fixed boxes in no_overlap_2d overlap");
      }
    }
  }

  if (fixed_boxes.size() == active_boxes.size()) {
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
        const int item_x_interval =
            context_->working_model->constraints().size();
        IntervalConstraintProto* new_interval =
            context_->working_model->add_constraints()->mutable_interval();
        new_interval->mutable_start()->set_offset(fixed_box.x_min.value());
        new_interval->mutable_size()->set_offset(fixed_box.SizeX().value());
        new_interval->mutable_end()->set_offset(fixed_box.x_max.value());

        const int item_y_interval =
            context_->working_model->constraints().size();
        new_interval =
            context_->working_model->add_constraints()->mutable_interval();
        new_interval->mutable_start()->set_offset(fixed_box.y_min.value());
        new_interval->mutable_size()->set_offset(fixed_box.SizeY().value());
        new_interval->mutable_end()->set_offset(fixed_box.y_max.value());

        new_no_overlap_2d.add_x_intervals(item_x_interval);
        new_no_overlap_2d.add_y_intervals(item_y_interval);
      }
      context_->working_model->add_constraints()->mutable_no_overlap_2d()->Swap(
          &new_no_overlap_2d);
      context_->UpdateNewConstraintsVariableUsage();
      context_->UpdateRuleStats("no_overlap_2d: presolved fixed rectangles");
      return RemoveConstraint(ct);
    }
  }
  RunPropagatorsForConstraint(*ct);
  return new_size < initial_num_boxes;
}

namespace {
LinearExpressionProto ConstantExpressionProto(int64_t value) {
  LinearExpressionProto expr;
  expr.set_offset(value);
  return expr;
}
}  // namespace

void CpModelPresolver::DetectDuplicateIntervals(
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

  // Merge identical intervals if the demand can be merged and is still affine.
  //
  // TODO(user): We could also merge if the first entry is constant instead of
  // the second one. Or if the variable used for the demand is the same.
  {
    absl::flat_hash_map<int, int> interval_to_i;
    int new_size = 0;
    for (int i = 0; i < proto->intervals_size(); ++i) {
      const auto [it, inserted] =
          interval_to_i.insert({proto->intervals(i), new_size});
      if (!inserted) {
        if (context_->IsFixed(proto->demands(i))) {
          const int old_index = it->second;
          proto->mutable_demands(old_index)->set_offset(
              proto->demands(old_index).offset() +
              context_->FixedValue(proto->demands(i)));
          context_->UpdateRuleStats(
              "cumulative: merged demand of identical interval");
          continue;
        } else {
          context_->UpdateRuleStats(
              "TODO cumulative: merged demand of identical interval");
        }
      }
      proto->set_intervals(new_size, proto->intervals(i));
      *proto->mutable_demands(new_size) = proto->demands(i);
      ++new_size;
    }
    if (new_size < proto->intervals_size()) {
      changed = true;
      proto->mutable_intervals()->Truncate(new_size);
      proto->mutable_demands()->erase(
          proto->mutable_demands()->begin() + new_size,
          proto->mutable_demands()->end());
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
      if (context_->SizeMax(interval_index) == 0) {
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
              context_->working_model->mutable_constraints(interval_index);
          DCHECK_EQ(interval_ct->enforcement_literal_size(), 1);
          const int literal = interval_ct->enforcement_literal(0);
          if (!context_->SetLiteralToFalse(literal)) {
            return true;
          }
          num_incompatible_intervals++;
          continue;
        } else {
          return context_->NotifyThatModelIsUnsat(
              "cumulative: inconsistent intervals cannot be performed.");
        }
      }

      if (context_->MinOf(demand_expr) > capacity_max) {
        if (context_->ConstraintIsOptional(interval_index)) {
          if (context_->SizeMin(interval_index) > 0) {
            ConstraintProto* interval_ct =
                context_->working_model->mutable_constraints(interval_index);
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
              context_->working_model->constraints(interval_index);
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
      if (context_->SizeMin(interval) == 0) continue;
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
      const int interval_index = proto->intervals(i);
      const ConstraintProto& interval_ct =
          context_->working_model->constraints(interval_index);

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
    if (num_duration_one == num_intervals && !has_optional_interval) {
      context_->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context_->working_model->add_constraints();
      auto* arg = new_ct->mutable_all_diff();
      for (const LinearExpressionProto& expr : start_exprs) {
        *arg->add_exprs() = expr;
      }
      if (!context_->IsFixed(capacity_expr)) {
        const int64_t capacity_min = context_->MinOf(capacity_expr);
        for (const LinearExpressionProto& expr : proto->demands()) {
          if (capacity_min >= context_->MaxOf(expr)) continue;
          LinearConstraintProto* fit =
              context_->working_model->add_constraints()->mutable_linear();
          fit->add_domain(0);
          fit->add_domain(std::numeric_limits<int64_t>::max());
          AddLinearExpressionToLinearConstraint(capacity_expr, 1, fit);
          AddLinearExpressionToLinearConstraint(expr, -1, fit);
        }
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

  RunPropagatorsForConstraint(*ct);
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
    // were arcs, it means it contains non zero nodes. Without arc, the
    // constraint is unfeasible.
    return context_->NotifyThatModelIsUnsat(
        "routes: graph with nodes and no arcs");
  }

  // if a node misses an incomping or outgoing arc, the model is trivially
  // infeasible.
  for (int n = 0; n < has_incoming_or_outgoing_arcs.size(); ++n) {
    if (!has_incoming_or_outgoing_arcs[n]) {
      return context_->NotifyThatModelIsUnsat(absl::StrCat(
          "routes: node ", n, " misses incoming or outgoing arcs"));
    }
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
  RunPropagatorsForConstraint(*ct);
  return false;
}

bool CpModelPresolver::PresolveAutomaton(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
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
        return MarkConstraintAsFalse(ct);
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

bool CpModelPresolver::PresolveReservoir(ConstraintProto* ct) {
  if (context_->ModelIsUnsat()) return false;
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
        "reservoir: remove zero level_changes or inactive events.");
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
    auto* const sum =
        context_->working_model->add_constraints()->mutable_linear();
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

  RunPropagatorsForConstraint(*ct);
  return changed;
}

// TODO(user): It is probably more efficient to keep all the bool_and in a
// global place during all the presolve, and just output them at the end
// rather than modifying more than once the proto.
void CpModelPresolver::ConvertToBoolAnd() {
  absl::flat_hash_map<int, int> ref_to_bool_and;
  const int num_constraints = context_->working_model->constraints_size();
  std::vector<int> to_remove;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (HasEnforcementLiteral(ct)) continue;

    if (ct.constraint_case() == ConstraintProto::kBoolOr &&
        ct.bool_or().literals().size() == 2) {
      AddImplication(NegatedRef(ct.bool_or().literals(0)),
                     ct.bool_or().literals(1), context_->working_model,
                     &ref_to_bool_and);
      to_remove.push_back(c);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::kAtMostOne &&
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

void CpModelPresolver::RunPropagatorsForConstraint(const ConstraintProto& ct) {
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

  std::vector<int> variable_mapping;
  CreateValidModelWithSingleConstraint(ct, context_, &variable_mapping,
                                       &tmp_model_);
  if (!LoadModelForPresolve(tmp_model_, std::move(local_params), context_,
                            &model, "single constraint")) {
    return;
  }

  auto* mapping = model.GetOrCreate<CpModelMapping>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* trail = model.GetOrCreate<Trail>();

  int num_equiv = 0;
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
        ++num_equiv;
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
              proto_var,
              integer_trail->InitialVariableDomain(mapping->Integer(var)),
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

// TODO(user): It might make sense to run this in parallel. The same apply for
// other expansive and self-contains steps like symmetry detection, etc...
void CpModelPresolver::Probe() {
  auto probing_timer =
      std::make_unique<PresolveTimer>(__FUNCTION__, logger_, time_limit_);

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

  // Try to detect trivial clauses thanks to implications.
  // This can be slow, so we bound the amount of work done.
  //
  // Idea: If we have l1, l2 in a bool_or and not(l1) => l2, the constraint is
  // always true.
  //
  // Correctness: Note that we always replace a clause with another one that
  // subsumes it. So we are correct even if new clauses are learned and used
  // for propagation along the way.
  //
  // TODO(user): Improve the algo?
  const auto& assignment = sat_solver->Assignment();
  prober->SetPropagationCallback([&](Literal decision) {
    if (probing_timer->WorkLimitIsReached()) return;
    const int decision_var =
        mapping->GetProtoVariableFromBooleanVariable(decision.Variable());
    if (decision_var < 0) return;
    probing_timer->TrackSimpleLoop(
        context_->VarToConstraints(decision_var).size());
    std::vector<int> to_update;
    for (const int c : context_->VarToConstraints(decision_var)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.enforcement_literal().size() > 2) {
        // Any l for which decision => l can be removed.
        //
        // If decision => not(l), constraint can never be satisfied. However
        // because we don't know if this constraint was part of the
        // propagation we replace it by an implication.
        //
        // TODO(user): remove duplication with code below.
        // TODO(user): If decision appear positively, we could potentially
        // remove a bunch of terms (all the ones involving variables implied
        // by the decision) from the inner constraint, especially in the
        // linear case.
        int decision_ref;
        int false_ref;
        bool decision_is_positive = false;
        bool has_false_literal = false;
        bool simplification_possible = false;
        probing_timer->TrackSimpleLoop(ct.enforcement_literal().size());
        for (const int ref : ct.enforcement_literal()) {
          const Literal lit = mapping->Literal(ref);
          if (PositiveRef(ref) == decision_var) {
            decision_ref = ref;
            decision_is_positive = assignment.LiteralIsTrue(lit);
            if (!decision_is_positive) break;
            continue;
          }
          if (assignment.LiteralIsFalse(lit)) {
            false_ref = ref;
            has_false_literal = true;
          } else if (assignment.LiteralIsTrue(lit)) {
            // If decision => l, we can remove l from the list.
            simplification_possible = true;
          }
        }
        if (!decision_is_positive) continue;

        if (has_false_literal) {
          // Reduce to implication.
          auto* mutable_ct = context_->working_model->mutable_constraints(c);
          mutable_ct->Clear();
          mutable_ct->add_enforcement_literal(decision_ref);
          mutable_ct->mutable_bool_and()->add_literals(NegatedRef(false_ref));
          context_->UpdateRuleStats(
              "probing: reduced enforced constraint to implication.");
          to_update.push_back(c);
          continue;
        }

        if (simplification_possible) {
          int new_size = 0;
          auto* mutable_enforcements =
              context_->working_model->mutable_constraints(c)
                  ->mutable_enforcement_literal();
          for (const int ref : ct.enforcement_literal()) {
            if (PositiveRef(ref) != decision_var &&
                assignment.LiteralIsTrue(mapping->Literal(ref))) {
              continue;
            }
            mutable_enforcements->Set(new_size++, ref);
          }
          mutable_enforcements->Truncate(new_size);
          context_->UpdateRuleStats("probing: simplified enforcement list.");
          to_update.push_back(c);
        }
        continue;
      }

      if (ct.constraint_case() != ConstraintProto::kBoolOr) continue;
      if (ct.bool_or().literals().size() <= 2) continue;

      int decision_ref;
      int true_ref;
      bool decision_is_negative = false;
      bool has_true_literal = false;
      bool simplification_possible = false;
      probing_timer->TrackSimpleLoop(ct.bool_or().literals().size());
      for (const int ref : ct.bool_or().literals()) {
        const Literal lit = mapping->Literal(ref);
        if (PositiveRef(ref) == decision_var) {
          decision_ref = ref;
          decision_is_negative = assignment.LiteralIsFalse(lit);
          if (!decision_is_negative) break;
          continue;
        }
        if (assignment.LiteralIsTrue(lit)) {
          true_ref = ref;
          has_true_literal = true;
        } else if (assignment.LiteralIsFalse(lit)) {
          // If not(l1) => not(l2), we can remove l2 from the clause.
          simplification_possible = true;
        }
      }
      if (!decision_is_negative) continue;

      if (has_true_literal) {
        // This will later be merged with the current implications and removed
        // if it is a duplicate.
        auto* mutable_bool_or =
            context_->working_model->mutable_constraints(c)->mutable_bool_or();
        mutable_bool_or->mutable_literals()->Clear();
        mutable_bool_or->add_literals(decision_ref);
        mutable_bool_or->add_literals(true_ref);
        context_->UpdateRuleStats("probing: bool_or reduced to implication");
        to_update.push_back(c);
        continue;
      }

      if (simplification_possible) {
        int new_size = 0;
        auto* mutable_bool_or =
            context_->working_model->mutable_constraints(c)->mutable_bool_or();
        for (const int ref : ct.bool_or().literals()) {
          if (PositiveRef(ref) != decision_var &&
              assignment.LiteralIsFalse(mapping->Literal(ref))) {
            continue;
          }
          mutable_bool_or->set_literals(new_size++, ref);
        }
        mutable_bool_or->mutable_literals()->Truncate(new_size);
        context_->UpdateRuleStats("probing: simplified clauses.");
        to_update.push_back(c);
      }
    }

    absl::c_sort(to_update);
    for (const int c : to_update) {
      context_->UpdateConstraintVariableUsage(c);
    }
  });

  prober->ProbeBooleanVariables(
      context_->params().probing_deterministic_time_limit());

  probing_timer->AddCounter("probed", prober->num_decisions());
  probing_timer->AddToWork(
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  if (sat_solver->ModelIsUnsat() || !implication_graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat("during probing");
  }

  // Update the presolve context with fixed Boolean variables.
  int num_fixed = 0;
  CHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    const Literal l = sat_solver->LiteralTrail()[i];
    const int var = mapping->GetProtoVariableFromBooleanVariable(l.Variable());
    if (var >= 0) {
      const int ref = l.IsPositive() ? var : NegatedRef(var);
      if (context_->IsFixed(ref)) continue;
      ++num_fixed;
      if (!context_->SetLiteralToTrue(ref)) return;
    }
  }
  probing_timer->AddCounter("fixed_bools", num_fixed);

  int num_equiv = 0;
  int num_changed_bounds = 0;
  const int num_variables = context_->working_model->variables().size();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  for (int var = 0; var < num_variables; ++var) {
    // Restrict IntegerVariable domain.
    // Note that Boolean are already dealt with above.
    if (!mapping->IsBoolean(var)) {
      bool changed = false;
      if (!context_->IntersectDomainWith(
              var, integer_trail->InitialVariableDomain(mapping->Integer(var)),
              &changed)) {
        return;
      }
      if (changed) ++num_changed_bounds;
      continue;
    }

    // Add Boolean equivalence relations.
    const Literal l = mapping->Literal(var);
    const Literal r = implication_graph->RepresentativeOf(l);
    if (r != l) {
      ++num_equiv;
      const int r_var =
          mapping->GetProtoVariableFromBooleanVariable(r.Variable());
      CHECK_GE(r_var, 0);
      context_->StoreBooleanEqualityRelation(
          var, r.IsPositive() ? r_var : NegatedRef(r_var));
    }
  }
  probing_timer->AddCounter("new_bounds", num_changed_bounds);
  probing_timer->AddCounter("equiv", num_equiv);
  probing_timer->AddCounter("new_binary_clauses",
                            prober->num_new_binary_clauses());

  // Note that we prefer to run this after we exported all equivalence to the
  // context, so that our enforcement list can be presolved to the best of our
  // knowledge.
  DetectDuplicateConstraintsWithDifferentEnforcements(
      mapping, implication_graph, model.GetOrCreate<Trail>());

  // Stop probing timer now and display info.
  probing_timer.reset();

  // Run clique merging using detected implications from probing.
  if (context_->params().merge_at_most_one_work_limit() > 0.0) {
    PresolveTimer timer("MaxClique", logger_, time_limit_);
    std::vector<std::vector<Literal>> cliques;
    std::vector<int> clique_ct_index;

    // TODO(user): On large model, most of the time is spend in this copy,
    // clearing and updating the constraint variable graph...
    int64_t num_literals_before = 0;
    const int num_constraints = context_->working_model->constraints_size();
    for (int c = 0; c < num_constraints; ++c) {
      ConstraintProto* ct = context_->working_model->mutable_constraints(c);
      if (ct->constraint_case() == ConstraintProto::kAtMostOne) {
        std::vector<Literal> clique;
        for (const int ref : ct->at_most_one().literals()) {
          clique.push_back(mapping->Literal(ref));
        }
        num_literals_before += clique.size();
        cliques.push_back(clique);
        ct->Clear();
        context_->UpdateConstraintVariableUsage(c);
      } else if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
        if (ct->enforcement_literal().size() != 1) continue;
        const Literal enforcement =
            mapping->Literal(ct->enforcement_literal(0));
        for (const int ref : ct->bool_and().literals()) {
          if (ref == ct->enforcement_literal(0)) continue;
          num_literals_before += 2;
          cliques.push_back({enforcement, mapping->Literal(ref).Negated()});
        }
        ct->Clear();
        context_->UpdateConstraintVariableUsage(c);
      }
    }
    const int64_t num_old_cliques = cliques.size();

    // We adapt the limit if there is a lot of literals in amo/implications.
    // Usually we can have big reduction on large problem so it seems
    // worthwhile.
    double limit = context_->params().merge_at_most_one_work_limit();
    if (num_literals_before > 1e6) {
      limit *= num_literals_before / 1e6;
    }

    double dtime = 0.0;
    implication_graph->MergeAtMostOnes(absl::MakeSpan(cliques),
                                       SafeDoubleToInt64(limit), &dtime);
    timer.AddToWork(dtime);

    // Note that because TransformIntoMaxCliques() extend cliques, we are ok
    // to ignore any unmapped literal. In case of equivalent literal, we always
    // use the smaller indices as a representative, so we should be good.
    int num_new_cliques = 0;
    int64_t num_literals_after = 0;
    for (const std::vector<Literal>& clique : cliques) {
      if (clique.empty()) continue;
      num_new_cliques++;
      num_literals_after += clique.size();
      ConstraintProto* ct = context_->working_model->add_constraints();
      for (const Literal literal : clique) {
        const int var =
            mapping->GetProtoVariableFromBooleanVariable(literal.Variable());
        if (var < 0) continue;
        if (literal.IsPositive()) {
          ct->mutable_at_most_one()->add_literals(var);
        } else {
          ct->mutable_at_most_one()->add_literals(NegatedRef(var));
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
      timer.AddMessage(absl::StrCat(
          "Merged ", FormatCounter(num_old_cliques), "(",
          FormatCounter(num_literals_before), " literals) into ",
          FormatCounter(num_new_cliques), "(",
          FormatCounter(num_literals_after), " literals) at_most_ones. "));
    }
  }
}

namespace {

bool FixFromAssignment(const VariablesAssignment& assignment,
                       const std::vector<int>& var_mapping,
                       PresolveContext* context) {
  const int num_vars = assignment.NumberOfVariables();
  for (int i = 0; i < num_vars; ++i) {
    const Literal lit(BooleanVariable(i), true);
    const int ref = var_mapping[i];
    if (assignment.LiteralIsTrue(lit)) {
      if (!context->SetLiteralToTrue(ref)) return false;
    } else if (assignment.LiteralIsFalse(lit)) {
      if (!context->SetLiteralToFalse(ref)) return false;
    }
  }
  return true;
}

}  // namespace

// TODO(user): What to do with the at_most_one/exactly_one constraints?
// currently we do not take them into account here.
bool CpModelPresolver::PresolvePureSatPart() {
  // TODO(user): Reenable some SAT presolve with
  // keep_all_feasible_solutions set to true.
  if (context_->ModelIsUnsat()) return true;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return true;

  // Compute a dense re-indexing for the Booleans of the problem.
  int num_variables = 0;
  int num_ignored_variables = 0;
  const int total_num_vars = context_->working_model->variables().size();
  std::vector<int> new_index(total_num_vars, -1);
  std::vector<int> new_to_old_index;
  for (int i = 0; i < total_num_vars; ++i) {
    if (!context_->CanBeUsedAsLiteral(i)) {
      ++num_ignored_variables;
      continue;
    }

    // This is important to not assign variable in equivalence to random values.
    if (context_->VarToConstraints(i).empty()) continue;

    new_to_old_index.push_back(i);
    new_index[i] = num_variables++;
    DCHECK_EQ(num_variables, new_to_old_index.size());
  }

  // The conversion from proto index to remapped Literal.
  auto convert = [&new_index](int ref) {
    const int index = new_index[PositiveRef(ref)];
    DCHECK_NE(index, -1);
    return Literal(BooleanVariable(index), RefIsPositive(ref));
  };

  // Load the pure-SAT part in a fresh Model.
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
  Model local_model;
  local_model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit_);
  auto* sat_solver = local_model.GetOrCreate<SatSolver>();
  sat_solver->SetNumVariables(num_variables);

  // Fix variables if any. Because we might not have reached the presove "fixed
  // point" above, some variable in the added clauses might be fixed. We need to
  // indicate this to the SAT presolver.
  for (const int var : new_to_old_index) {
    if (context_->IsFixed(var)) {
      if (context_->LiteralIsTrue(var)) {
        if (!sat_solver->AddUnitClause({convert(var)})) return false;
      } else {
        if (!sat_solver->AddUnitClause({convert(NegatedRef(var))})) {
          return false;
        }
      }
    }
  }

  std::vector<Literal> clause;
  int num_removed_constraints = 0;
  int num_ignored_constraints = 0;
  for (int i = 0; i < context_->working_model->constraints_size(); ++i) {
    const ConstraintProto& ct = context_->working_model->constraints(i);

    if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      ++num_removed_constraints;
      clause.clear();
      for (const int ref : ct.bool_or().literals()) {
        clause.push_back(convert(ref));
      }
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(convert(ref).Negated());
      }
      sat_solver->AddProblemClause(clause, /*is_safe=*/false);

      context_->working_model->mutable_constraints(i)->Clear();
      context_->UpdateConstraintVariableUsage(i);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      // We currently do not expand "complex" bool_and that would result
      // in too many literals.
      const int left_size = ct.enforcement_literal().size();
      const int right_size = ct.bool_and().literals().size();
      if (left_size > 1 && right_size > 1 &&
          (left_size + 1) * right_size > 10'000) {
        ++num_ignored_constraints;
        continue;
      }

      ++num_removed_constraints;
      std::vector<Literal> clause;
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(convert(ref).Negated());
      }
      clause.push_back(Literal(kNoLiteralIndex));  // will be replaced below.
      for (const int ref : ct.bool_and().literals()) {
        clause.back() = convert(ref);
        sat_solver->AddProblemClause(clause, /*is_safe=*/false);
      }

      context_->working_model->mutable_constraints(i)->Clear();
      context_->UpdateConstraintVariableUsage(i);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
      continue;
    }

    ++num_ignored_constraints;
  }
  if (sat_solver->ModelIsUnsat()) return false;

  // Abort early if there was no Boolean constraints.
  if (num_removed_constraints == 0) return true;

  // Mark the variables appearing elsewhere or in the objective as non-removable
  // by the sat presolver.
  //
  // TODO(user): do not remove variable that appear in the decision heuristic?
  // TODO(user): We could go further for variable with only one polarity by
  // removing variable from the objective if they can be set to their "low"
  // objective value, and also removing enforcement literal that can be set to
  // false and don't appear elsewhere.
  int num_in_extra_constraints = 0;
  std::vector<bool> can_be_removed(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    const int var = new_to_old_index[i];
    if (context_->VarToConstraints(var).empty()) {
      can_be_removed[i] = true;
    } else {
      // That might correspond to the objective or a variable with an affine
      // relation that is still in the model.
      ++num_in_extra_constraints;
    }
  }

  // The "full solver" postsolve does not support changing the value of a
  // variable from the solution of the presolved problem, and we do need this
  // for blocked clause. It should be possible to allow for this by adding extra
  // variable to the mapping model at presolve and some linking constraints, but
  // this is messy.
  //
  // We also disable this if the user asked for tightened domain as this might
  // fix variable to a potentially infeasible value, and just correct them later
  // during postsolve of a particular solution.
  SatParameters params = context_->params();
  if (params.debug_postsolve_with_full_solver() ||
      params.fill_tightened_domains_in_response()) {
    params.set_presolve_blocked_clause(false);
  }

  SatPostsolver sat_postsolver(num_variables);

  // If the problem is a pure-SAT problem, we run the new SAT presolver.
  // This takes more time but it is usually worthwile
  //
  // Note that the probing that it does is faster than the
  // ProbeAndFindEquivalentLiteral() call below, but does not do equivalence
  // detection as completely, so we still apply the other "probing" code
  // afterwards even if it will not fix more literals, but it will do one pass
  // of proper equivalence detection.
  util_intops::StrongVector<LiteralIndex, LiteralIndex> equiv_map;
  if (!context_->params().debug_postsolve_with_full_solver() &&
      num_ignored_variables == 0 && num_ignored_constraints == 0 &&
      num_in_extra_constraints == 0) {
    // Some problems are formulated in such a way that our SAT heuristics
    // simply works without conflict. Get them out of the way first because it
    // is possible that the presolve lose this "lucky" ordering. This is in
    // particular the case on the SAT14.crafted.complete-xxx-... problems.
    if (!LookForTrivialSatSolution(/*deterministic_time_limit=*/1.0,
                                   &local_model, logger_)) {
      return false;
    }
    if (sat_solver->LiteralTrail().Index() == num_variables) {
      // Problem solved! We should be able to assign the solution.
      CHECK(FixFromAssignment(sat_solver->Assignment(), new_to_old_index,
                              context_));
      return true;
    }

    SatPresolveOptions options;
    options.log_info = true;  // log_info;
    options.extract_binary_clauses_in_probing = false;
    options.use_transitive_reduction = false;
    options.deterministic_time_limit =
        context_->params().presolve_probing_deterministic_time_limit();

    auto* inprocessing = local_model.GetOrCreate<Inprocessing>();
    inprocessing->ProvideLogger(logger_);
    if (!inprocessing->PresolveLoop(options)) return false;
    for (const auto& c : local_model.GetOrCreate<PostsolveClauses>()->clauses) {
      sat_postsolver.Add(c[0], c);
    }

    // Probe + find equivalent literals.
    // TODO(user): Use a derived time limit in the probing phase.
    ProbeAndFindEquivalentLiteral(sat_solver, &sat_postsolver,
                                  /*drat_proof_handler=*/nullptr, &equiv_map,
                                  logger_);
    if (sat_solver->ModelIsUnsat()) return false;
  } else {
    // TODO(user): BVA takes time and does not seems to help on the minizinc
    // benchmarks. So we currently disable it, except if we are on a pure-SAT
    // problem, where we follow the default (true) or the user specified value.
    params.set_presolve_use_bva(false);
  }

  // Disable BVA if we want to keep the symmetries.
  //
  // TODO(user): We could still do it, we just need to do in a symmetric way
  // and also update the generators to take into account the new variables. This
  // do not seems that easy.
  if (context_->params().keep_symmetry_in_presolve()) {
    params.set_presolve_use_bva(false);
  }

  // Update the time limit of the initial propagation.
  if (!sat_solver->ResetToLevelZero()) return false;
  time_limit_->AdvanceDeterministicTime(
      local_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());

  // Apply the "old" SAT presolve.
  SatPresolver sat_presolver(&sat_postsolver, logger_);
  sat_presolver.SetNumVariables(num_variables);
  if (!equiv_map.empty()) {
    sat_presolver.SetEquivalentLiteralMapping(equiv_map);
  }
  sat_presolver.SetTimeLimit(time_limit_);
  sat_presolver.SetParameters(params);

  // Load in the presolver.
  // Register the fixed variables with the postsolver.
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    sat_postsolver.FixVariable(sat_solver->LiteralTrail()[i]);
  }
  sat_solver->ExtractClauses(&sat_presolver);

  // Run the presolve for a small number of passes.
  // TODO(user): Add a local time limit? this can be slow on big SAT problem.
  for (int i = 0; i < 1; ++i) {
    const int old_num_clause = sat_postsolver.NumClauses();
    if (!sat_presolver.Presolve(can_be_removed)) return false;
    if (old_num_clause == sat_postsolver.NumClauses()) break;
  }

  // Add any new variables to our internal structure.
  const int new_num_variables = sat_presolver.NumVariables();
  if (new_num_variables > num_variables) {
    VLOG(1) << "New variables added by the SAT presolver.";
    for (int i = num_variables; i < new_num_variables; ++i) {
      new_to_old_index.push_back(context_->working_model->variables().size());
      IntegerVariableProto* var_proto =
          context_->working_model->add_variables();
      var_proto->add_domain(0);
      var_proto->add_domain(1);
    }
    context_->InitializeNewDomains();
  }

  // Fix variables if any.
  if (!FixFromAssignment(sat_postsolver.assignment(), new_to_old_index,
                         context_)) {
    return false;
  }

  // Add the presolver clauses back into the model.
  ExtractClauses(/*merge_into_bool_and=*/true, new_to_old_index, sat_presolver,
                 context_->working_model);

  // Update the constraints <-> variables graph.
  context_->UpdateNewConstraintsVariableUsage();

  // Add the sat_postsolver clauses to mapping_model.
  //
  // TODO(user): Mark removed variable as removed to detect any potential bugs.
  ExtractClauses(/*merge_into_bool_and=*/false, new_to_old_index,
                 sat_postsolver, context_->mapping_model);
  return true;
}

void CpModelPresolver::ShiftObjectiveWithExactlyOnes() {
  if (context_->ModelIsUnsat()) return;

  // The objective is already loaded in the context, but we re-canonicalize
  // it with the latest information.
  if (!context_->CanonicalizeObjective()) {
    return;
  }

  std::vector<int> exos;
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      exos.push_back(c);
    }
  }

  // This is not the same from what we do in ExpandObjective() because we do not
  // make the minimum cost zero but the second minimum. Note that when we do
  // that, we still do not degrade the trivial objective bound as we would if we
  // went any further.
  //
  // One reason why this might be beneficial is that it lower the maximum cost
  // magnitude, making more Booleans with the same cost and thus simplifying
  // the core optimizer job. I am not 100% sure.
  //
  // TODO(user): We need to loop a few time to reach a fixed point. Understand
  // exactly if there is a fixed-point and how to reach it in a nicer way.
  int num_shifts = 0;
  for (int i = 0; i < 3; ++i) {
    for (const int c : exos) {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      const int num_terms = ct.exactly_one().literals().size();
      if (num_terms <= 1) continue;
      int64_t min_obj = std::numeric_limits<int64_t>::max();
      int64_t second_min = std::numeric_limits<int64_t>::max();
      for (int i = 0; i < num_terms; ++i) {
        const int literal = ct.exactly_one().literals(i);
        const int64_t var_obj = context_->ObjectiveCoeff(PositiveRef(literal));
        const int64_t obj = RefIsPositive(literal) ? var_obj : -var_obj;
        if (obj < min_obj) {
          second_min = min_obj;
          min_obj = obj;
        } else if (obj < second_min) {
          second_min = obj;
        }
      }
      if (second_min == 0) continue;
      ++num_shifts;
      if (!context_->ShiftCostInExactlyOne(ct.exactly_one().literals(),
                                           second_min)) {
        if (context_->ModelIsUnsat()) return;
        continue;
      }
    }
  }
  if (num_shifts > 0) {
    context_->UpdateRuleStats("objective: shifted cost with exactly ones",
                              num_shifts);
  }
}

// Expand the objective expression in some easy cases.
//
// The ideas is to look at all the "tight" equality constraints. These should
// give a topological order on the variable in which we can perform
// substitution.
//
// Basically, we will only use constraints of the form X' = sum ci * Xi' with ci
// > 0 and the variable X' being shifted version >= 0. Note that if there is a
// cycle with these constraints, all variables involved must be equal to each
// other and likely zero. Otherwise, we can express everything in terms of the
// leaves.
//
// This assumes we are more or less at the propagation fix point, even if we
// try to address cases where we are not.
void CpModelPresolver::ExpandObjective() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // The objective is already loaded in the context, but we re-canonicalize
  // it with the latest information.
  if (!context_->CanonicalizeObjective()) {
    return;
  }

  const int num_variables = context_->working_model->variables_size();
  const int num_constraints = context_->working_model->constraints_size();

  // We consider two types of shifted variables (X - LB(X)) and (UB(X) - X).
  const auto get_index = [](int var, bool to_lb) {
    return 2 * var + (to_lb ? 0 : 1);
  };
  const auto get_lit_index = [](int lit) {
    return RefIsPositive(lit) ? 2 * lit : 2 * PositiveRef(lit) + 1;
  };
  const int num_nodes = 2 * num_variables;
  std::vector<std::vector<int>> index_graph(num_nodes);

  // TODO(user): instead compute how much each constraint can be further
  // expanded?
  std::vector<int> index_to_best_c(num_nodes, -1);
  std::vector<int> index_to_best_size(num_nodes, 0);

  // Lets see first if there are "tight" constraint and for which variables.
  // We stop processing constraint if we have too many entries.
  int num_entries = 0;
  int num_propagations = 0;
  int num_tight_variables = 0;
  int num_tight_constraints = 0;
  const int kNumEntriesThreshold = 1e8;
  for (int c = 0; c < num_constraints; ++c) {
    if (num_entries > kNumEntriesThreshold) break;

    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (!ct.enforcement_literal().empty()) continue;

    // Deal with exactly one.
    // An exactly one is always tight on the upper bound of one term.
    //
    // Note(user): This code assume there is no fixed variable in the exactly
    // one. We thus make sure the constraint is re-presolved if for some reason
    // we didn't reach the fixed point before calling this code.
    if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      if (PresolveExactlyOne(context_->working_model->mutable_constraints(c))) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
    if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      const int num_terms = ct.exactly_one().literals().size();
      ++num_tight_constraints;
      num_tight_variables += num_terms;
      for (int i = 0; i < num_terms; ++i) {
        if (num_entries > kNumEntriesThreshold) break;
        const int neg_index = get_lit_index(ct.exactly_one().literals(i)) ^ 1;

        const int old_c = index_to_best_c[neg_index];
        if (old_c == -1 || num_terms > index_to_best_size[neg_index]) {
          index_to_best_c[neg_index] = c;
          index_to_best_size[neg_index] = num_terms;
        }

        for (int j = 0; j < num_terms; ++j) {
          if (j == i) continue;
          const int other_index = get_lit_index(ct.exactly_one().literals(j));
          ++num_entries;
          index_graph[neg_index].push_back(other_index);
        }
      }
      continue;
    }

    // Skip everything that is not a linear equality constraint.
    if (!IsLinearEqualityConstraint(ct)) continue;

    // Let see for which variable is it "tight". We need a coeff of 1, and that
    // the implied bounds match exactly.
    const auto [min_activity, max_activity] =
        context_->ComputeMinMaxActivity(ct.linear());

    bool is_tight = false;
    const int64_t rhs = ct.linear().domain(0);
    const int num_terms = ct.linear().vars_size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct.linear().vars(i);
      const int64_t coeff = ct.linear().coeffs(i);
      if (std::abs(coeff) != 1) continue;
      if (num_entries > kNumEntriesThreshold) break;

      const int index = get_index(var, coeff > 0);

      const int64_t var_range = context_->MaxOf(var) - context_->MinOf(var);
      const int64_t implied_shifted_ub = rhs - min_activity;
      if (implied_shifted_ub <= var_range) {
        if (implied_shifted_ub < var_range) ++num_propagations;
        is_tight = true;
        ++num_tight_variables;

        const int neg_index = index ^ 1;
        const int old_c = index_to_best_c[neg_index];
        if (old_c == -1 || num_terms > index_to_best_size[neg_index]) {
          index_to_best_c[neg_index] = c;
          index_to_best_size[neg_index] = num_terms;
        }

        for (int j = 0; j < num_terms; ++j) {
          if (j == i) continue;
          const int other_index =
              get_index(ct.linear().vars(j), ct.linear().coeffs(j) > 0);
          ++num_entries;
          index_graph[neg_index].push_back(other_index);
        }
      }
      const int64_t implied_shifted_lb = max_activity - rhs;
      if (implied_shifted_lb <= var_range) {
        if (implied_shifted_lb < var_range) ++num_propagations;
        is_tight = true;
        ++num_tight_variables;

        const int old_c = index_to_best_c[index];
        if (old_c == -1 || num_terms > index_to_best_size[index]) {
          index_to_best_c[index] = c;
          index_to_best_size[index] = num_terms;
        }

        for (int j = 0; j < num_terms; ++j) {
          if (j == i) continue;
          const int other_index =
              get_index(ct.linear().vars(j), ct.linear().coeffs(j) < 0);
          ++num_entries;
          index_graph[index].push_back(other_index);
        }
      }
    }
    if (is_tight) ++num_tight_constraints;
  }

  // Note(user): We assume the fixed point was already reached by the linear
  // presolve, so we don't add extra code here for that. But we still abort if
  // some are left to cover corner cases were linear a still not propagated.
  if (num_propagations > 0) {
    context_->UpdateRuleStats("TODO objective: propagation possible!");
    return;
  }

  // In most cases, we should have no cycle and thus a topo order.
  //
  // In case there is a cycle, then all member of a strongly connected component
  // must be equivalent, this is because from X to Y, if we follow the chain we
  // will have X = non_negative_sum + Y and Y = non_negative_sum + X.
  //
  // Moreover, many shifted variables will need to be zero once we start to have
  // equivalence.
  //
  // TODO(user): Make the fixing to zero? or at least when this happen redo
  // a presolve pass?
  //
  // TODO(user): Densify index to only look at variable that can be substituted
  // further.
  const auto topo_order = util::graph::FastTopologicalSort(index_graph);
  if (!topo_order.ok()) {
    // Tricky: We need to cache all domains to derive the proper relations.
    // This is because StoreAffineRelation() might propagate them.
    std::vector<int64_t> var_min(num_variables);
    std::vector<int64_t> var_max(num_variables);
    for (int var = 0; var < num_variables; ++var) {
      var_min[var] = context_->MinOf(var);
      var_max[var] = context_->MaxOf(var);
    }

    std::vector<std::vector<int>> components;
    FindStronglyConnectedComponents(static_cast<int>(index_graph.size()),
                                    index_graph, &components);
    for (const std::vector<int>& compo : components) {
      if (compo.size() == 1) continue;

      const int rep_var = compo[0] / 2;
      const bool rep_to_lp = (compo[0] % 2) == 0;
      for (int i = 1; i < compo.size(); ++i) {
        const int var = compo[i] / 2;
        const bool to_lb = (compo[i] % 2) == 0;

        // (rep - rep_lb) | (rep_ub - rep) == (var - var_lb) | (var_ub - var)
        // +/- rep = +/- var + offset.
        const int64_t rep_coeff = rep_to_lp ? 1 : -1;
        const int64_t var_coeff = to_lb ? 1 : -1;
        const int64_t offset =
            (to_lb ? -var_min[var] : var_max[var]) -
            (rep_to_lp ? -var_min[rep_var] : var_max[rep_var]);
        if (!context_->StoreAffineRelation(rep_var, var, rep_coeff * var_coeff,
                                           rep_coeff * offset)) {
          return;
        }
      }
      context_->UpdateRuleStats("objective: detected equivalence",
                                compo.size() - 1);
    }
    return;
  }

  // If the removed variable is now unique, we could remove it if it is implied
  // free. But this should already be done by RemoveSingletonInLinear(), so we
  // don't redo it here.
  int num_expands = 0;
  int num_issues = 0;
  for (const int index : *topo_order) {
    if (index_graph[index].empty()) continue;

    const int var = index / 2;
    const int64_t obj_coeff = context_->ObjectiveCoeff(var);
    if (obj_coeff == 0) continue;

    const bool to_lb = (index % 2) == 0;
    if (obj_coeff > 0 == to_lb) {
      const ConstraintProto& ct =
          context_->working_model->constraints(index_to_best_c[index]);
      if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        int64_t shift = 0;
        for (const int lit : ct.exactly_one().literals()) {
          if (PositiveRef(lit) == var) {
            shift = RefIsPositive(lit) ? obj_coeff : -obj_coeff;
            break;
          }
        }
        if (shift == 0) {
          ++num_issues;
          continue;
        }
        if (!context_->ShiftCostInExactlyOne(ct.exactly_one().literals(),
                                             shift)) {
          if (context_->ModelIsUnsat()) return;
          ++num_issues;
          continue;
        }
        CHECK_EQ(context_->ObjectiveCoeff(var), 0);
        ++num_expands;
        continue;
      }

      int64_t objective_coeff_in_expanded_constraint = 0;
      const int num_terms = ct.linear().vars().size();
      for (int i = 0; i < num_terms; ++i) {
        if (ct.linear().vars(i) == var) {
          objective_coeff_in_expanded_constraint = ct.linear().coeffs(i);
          break;
        }
      }
      if (objective_coeff_in_expanded_constraint == 0) {
        ++num_issues;
        continue;
      }

      if (!context_->SubstituteVariableInObjective(
              var, objective_coeff_in_expanded_constraint, ct)) {
        if (context_->ModelIsUnsat()) return;
        ++num_issues;
        continue;
      }

      ++num_expands;
    }
  }

  if (num_expands > 0) {
    context_->UpdateRuleStats("objective: expanded via tight equality",
                              num_expands);
  }

  timer.AddCounter("propagations", num_propagations);
  timer.AddCounter("entries", num_entries);
  timer.AddCounter("tight_variables", num_tight_variables);
  timer.AddCounter("tight_constraints", num_tight_constraints);
  timer.AddCounter("expands", num_expands);
  timer.AddCounter("issues", num_issues);
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
    if (ct.constraint_case() != ConstraintProto::kNoOverlap) continue;
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
      &cliques,
      SafeDoubleToInt64(context_->params().merge_no_overlap_work_limit()));

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
  if (context_->params().merge_at_most_one_work_limit() <= 0.0) return;

  auto convert = [](int ref) {
    if (RefIsPositive(ref)) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };
  const int num_constraints = context_->working_model->constraints_size();

  // Extract the bool_and and at_most_one constraints.
  // TODO(user): use probing info?
  std::vector<std::vector<Literal>> cliques;

  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::kAtMostOne) {
      std::vector<Literal> clique;
      for (const int ref : ct->at_most_one().literals()) {
        clique.push_back(convert(ref));
      }
      cliques.push_back(clique);
      if (RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    } else if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
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
  graph->MergeAtMostOnes(
      absl::MakeSpan(cliques),
      SafeDoubleToInt64(context_->params().merge_at_most_one_work_limit()));

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
      if (!DivideLinMaxByGcd(c, ct)) return false;
      return PresolveLinMax(ct);
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
    case ConstraintProto::kNoOverlap2D:
      DetectDuplicateIntervals(
          c, ct->mutable_no_overlap_2d()->mutable_x_intervals());
      DetectDuplicateIntervals(
          c, ct->mutable_no_overlap_2d()->mutable_y_intervals());
      return PresolveNoOverlap2D(c, ct);
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

// Returns false iff the model is UNSAT.
bool CpModelPresolver::ProcessSetPPCSubset(int subset_c, int superset_c,
                                           absl::flat_hash_set<int>* tmp_set,
                                           bool* remove_subset,
                                           bool* remove_superset,
                                           bool* stop_processing_superset) {
  ConstraintProto* subset_ct =
      context_->working_model->mutable_constraints(subset_c);
  ConstraintProto* superset_ct =
      context_->working_model->mutable_constraints(superset_c);

  if ((subset_ct->constraint_case() == ConstraintProto::kBoolOr ||
       subset_ct->constraint_case() == ConstraintProto::kExactlyOne) &&
      (superset_ct->constraint_case() == ConstraintProto::kAtMostOne ||
       superset_ct->constraint_case() == ConstraintProto::kExactlyOne)) {
    context_->UpdateRuleStats("setppc: bool_or in at_most_one.");

    tmp_set->clear();
    if (subset_ct->constraint_case() == ConstraintProto::kBoolOr) {
      tmp_set->insert(subset_ct->bool_or().literals().begin(),
                      subset_ct->bool_or().literals().end());
    } else {
      tmp_set->insert(subset_ct->exactly_one().literals().begin(),
                      subset_ct->exactly_one().literals().end());
    }

    // Fix extras in superset_c to 0, note that these will be removed from the
    // constraint later.
    for (const int literal :
         superset_ct->constraint_case() == ConstraintProto::kAtMostOne
             ? superset_ct->at_most_one().literals()
             : superset_ct->exactly_one().literals()) {
      if (tmp_set->contains(literal)) continue;
      if (!context_->SetLiteralToFalse(literal)) return false;
      context_->UpdateRuleStats("setppc: fixed variables");
    }

    // Change superset_c to exactly_one if not already.
    if (superset_ct->constraint_case() != ConstraintProto::kExactlyOne) {
      ConstraintProto copy = *superset_ct;
      (*superset_ct->mutable_exactly_one()->mutable_literals()) =
          copy.at_most_one().literals();
    }

    *remove_subset = true;
    return true;
  }

  if ((subset_ct->constraint_case() == ConstraintProto::kBoolOr ||
       subset_ct->constraint_case() == ConstraintProto::kExactlyOne) &&
      superset_ct->constraint_case() == ConstraintProto::kBoolOr) {
    context_->UpdateRuleStats("setppc: removed dominated constraints");
    *remove_superset = true;
    return true;
  }

  if (subset_ct->constraint_case() == ConstraintProto::kAtMostOne &&
      (superset_ct->constraint_case() == ConstraintProto::kAtMostOne ||
       superset_ct->constraint_case() == ConstraintProto::kExactlyOne)) {
    context_->UpdateRuleStats("setppc: removed dominated constraints");
    *remove_subset = true;
    return true;
  }

  // Note(user): Only the exactly one should really be needed, the intersection
  // is taken care of by ProcessAtMostOneAndLinear() in a better way.
  if (subset_ct->constraint_case() == ConstraintProto::kExactlyOne &&
      superset_ct->constraint_case() == ConstraintProto::kLinear) {
    tmp_set->clear();
    int64_t min_sum = std::numeric_limits<int64_t>::max();
    int64_t max_sum = std::numeric_limits<int64_t>::min();
    tmp_set->insert(subset_ct->exactly_one().literals().begin(),
                    subset_ct->exactly_one().literals().end());

    // Compute the min/max on the subset of the sum that correspond the exo.
    int num_matches = 0;
    temp_ct_.Clear();
    Domain reachable(0);
    std::vector<std::pair<int64_t, int>> coeff_counts;
    for (int i = 0; i < superset_ct->linear().vars().size(); ++i) {
      const int var = superset_ct->linear().vars(i);
      const int64_t coeff = superset_ct->linear().coeffs(i);
      if (tmp_set->contains(var)) {
        ++num_matches;
        min_sum = std::min(min_sum, coeff);
        max_sum = std::max(max_sum, coeff);
        coeff_counts.push_back({superset_ct->linear().coeffs(i), 1});
      } else {
        reachable =
            reachable
                .AdditionWith(
                    context_->DomainOf(var).ContinuousMultiplicationBy(coeff))
                .RelaxIfTooComplex();
        temp_ct_.mutable_linear()->add_vars(var);
        temp_ct_.mutable_linear()->add_coeffs(coeff);
      }
    }

    // If a linear constraint contains more than one at_most_one or exactly_one,
    // after processing one, we might no longer have an inclusion.
    //
    // TODO(user): If we have multiple disjoint inclusion, we can propagate
    // more. For instance on neos-1593097.mps we basically have a
    // weighted_sum_over_at_most_one1 >= weighted_sum_over_at_most_one2.
    if (num_matches != tmp_set->size()) return true;
    if (subset_ct->constraint_case() == ConstraintProto::kExactlyOne) {
      context_->UpdateRuleStats("setppc: exactly_one included in linear");
    } else {
      context_->UpdateRuleStats("setppc: at_most_one included in linear");
    }

    reachable = reachable.AdditionWith(Domain(min_sum, max_sum));
    const Domain superset_rhs = ReadDomainFromProto(superset_ct->linear());
    if (reachable.IsIncludedIn(superset_rhs)) {
      // The constraint is trivial !
      context_->UpdateRuleStats("setppc: removed trivial linear constraint");
      *remove_superset = true;
      return true;
    }
    if (reachable.IntersectionWith(superset_rhs).IsEmpty()) {
      // TODO(user): constraint might become bool_or.
      context_->UpdateRuleStats("setppc: removed infeasible linear constraint");
      *stop_processing_superset = true;
      return MarkConstraintAsFalse(superset_ct);
    }

    // We reuse the normal linear constraint code to propagate domains of
    // the other variable using the inclusion information.
    if (superset_ct->enforcement_literal().empty()) {
      CHECK_GT(num_matches, 0);
      FillDomainInProto(ReadDomainFromProto(superset_ct->linear())
                            .AdditionWith(Domain(-max_sum, -min_sum)),
                        temp_ct_.mutable_linear());
      PropagateDomainsInLinear(/*ct_index=*/-1, &temp_ct_);
    }

    // If we have an exactly one in a linear, we can shift the coefficients of
    // all these variables by any constant value. We select a value that reduces
    // the number of terms the most.
    std::sort(coeff_counts.begin(), coeff_counts.end());
    int new_size = 0;
    for (int i = 0; i < coeff_counts.size(); ++i) {
      if (new_size > 0 &&
          coeff_counts[i].first == coeff_counts[new_size - 1].first) {
        coeff_counts[new_size - 1].second++;
        continue;
      }
      coeff_counts[new_size++] = coeff_counts[i];
    }
    coeff_counts.resize(new_size);
    int64_t best = 0;
    int64_t best_count = 0;
    for (const auto [coeff, count] : coeff_counts) {
      if (count > best_count) {
        best = coeff;
        best_count = count;
      }
    }
    if (best != 0) {
      LinearConstraintProto new_ct = superset_ct->linear();
      int new_size = 0;
      for (int i = 0; i < new_ct.vars().size(); ++i) {
        const int var = new_ct.vars(i);
        int64_t coeff = new_ct.coeffs(i);
        if (tmp_set->contains(var)) {
          if (coeff == best) continue;  // delete term.
          coeff -= best;
        }
        new_ct.set_vars(new_size, var);
        new_ct.set_coeffs(new_size, coeff);
        ++new_size;
      }

      new_ct.mutable_vars()->Truncate(new_size);
      new_ct.mutable_coeffs()->Truncate(new_size);
      FillDomainInProto(ReadDomainFromProto(new_ct).AdditionWith(Domain(-best)),
                        &new_ct);
      if (!PossibleIntegerOverflow(*context_->working_model, new_ct.vars(),
                                   new_ct.coeffs())) {
        *superset_ct->mutable_linear() = std::move(new_ct);
        context_->UpdateConstraintVariableUsage(superset_c);
        context_->UpdateRuleStats("setppc: reduced linear coefficients");
      }
    }

    return true;
  }

  // We can't deduce anything in the last remaining cases, like an at most one
  // in an at least one.
  return true;
}

// TODO(user): TransformIntoMaxCliques() convert the bool_and to
// at_most_one, but maybe also duplicating them into bool_or would allow this
// function to do more presolving.
void CpModelPresolver::ProcessSetPPC() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // TODO(user): compute on the fly instead of temporary storing variables?
  CompactVectorVector<int> storage;
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(context_->params().presolve_inclusion_work_limit());

  // We use an encoding of literal that allows to index arrays.
  std::vector<int> temp_literals;
  const int num_constraints = context_->working_model->constraints_size();
  std::vector<int> relevant_constraints;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    const auto type = ct->constraint_case();
    if (type == ConstraintProto::kBoolOr ||
        type == ConstraintProto::kAtMostOne ||
        type == ConstraintProto::kExactlyOne) {
      // Because TransformIntoMaxCliques() can detect literal equivalence
      // relation, we make sure the constraints are presolved before being
      // inspected.
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return;

      temp_literals.clear();
      for (const int ref :
           type == ConstraintProto::kAtMostOne ? ct->at_most_one().literals()
           : type == ConstraintProto::kBoolOr  ? ct->bool_or().literals()
                                               : ct->exactly_one().literals()) {
        temp_literals.push_back(
            Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref))
                .Index()
                .value());
      }
      relevant_constraints.push_back(c);
      detector.AddPotentialSet(storage.Add(temp_literals));
    } else if (type == ConstraintProto::kLinear) {
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

      // TODO(user): We only deal with positive var here. Ideally we should
      // match the VARIABLES of the at_most_one/exactly_one with the VARIABLES
      // of the linear, and complement all variable to have a literal inclusion.
      temp_literals.clear();
      for (int i = 0; i < size; ++i) {
        const int var = ct->linear().vars(i);
        if (!context_->CanBeUsedAsLiteral(var)) continue;
        if (!RefIsPositive(var)) continue;
        temp_literals.push_back(
            Literal(BooleanVariable(var), true).Index().value());
      }
      if (temp_literals.size() > 2) {
        // Note that we only care about the linear being the superset.
        relevant_constraints.push_back(c);
        detector.AddPotentialSuperset(storage.Add(temp_literals));
      }
    }
  }

  absl::flat_hash_set<int> tmp_set;
  int64_t num_inclusions = 0;
  detector.DetectInclusions([&](int subset, int superset) {
    ++num_inclusions;
    bool remove_subset = false;
    bool remove_superset = false;
    bool stop_processing_superset = false;
    const int subset_c = relevant_constraints[subset];
    const int superset_c = relevant_constraints[superset];
    detector.IncreaseWorkDone(storage[subset].size());
    detector.IncreaseWorkDone(storage[superset].size());
    if (!ProcessSetPPCSubset(subset_c, superset_c, &tmp_set, &remove_subset,
                             &remove_superset, &stop_processing_superset)) {
      detector.Stop();
      return;
    }
    if (remove_subset) {
      context_->working_model->mutable_constraints(subset_c)->Clear();
      context_->UpdateConstraintVariableUsage(subset_c);
      detector.StopProcessingCurrentSubset();
    }
    if (remove_superset) {
      context_->working_model->mutable_constraints(superset_c)->Clear();
      context_->UpdateConstraintVariableUsage(superset_c);
      detector.StopProcessingCurrentSuperset();
    }
    if (stop_processing_superset) {
      context_->UpdateConstraintVariableUsage(superset_c);
      detector.StopProcessingCurrentSuperset();
    }
  });

  timer.AddToWork(detector.work_done() * 1e-9);
  timer.AddCounter("relevant_constraints", relevant_constraints.size());
  timer.AddCounter("num_inclusions", num_inclusions);
}

void CpModelPresolver::DetectIncludedEnforcement() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // TODO(user): compute on the fly instead of temporary storing variables?
  std::vector<int> relevant_constraints;
  CompactVectorVector<int> storage;
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(context_->params().presolve_inclusion_work_limit());

  std::vector<int> temp_literals;
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->enforcement_literal().size() <= 1) continue;

    // Make sure there is no x => x.
    if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return;
    }

    // We use an encoding of literal that allows to index arrays.
    temp_literals.clear();
    for (const int ref : ct->enforcement_literal()) {
      temp_literals.push_back(
          Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref))
              .Index()
              .value());
    }
    relevant_constraints.push_back(c);

    // We only deal with bool_and included in other. Not the other way around,
    // Altough linear enforcement included in bool_and does happen.
    if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      detector.AddPotentialSet(storage.Add(temp_literals));
    } else {
      detector.AddPotentialSuperset(storage.Add(temp_literals));
    }
  }

  int64_t num_inclusions = 0;
  detector.DetectInclusions([&](int subset, int superset) {
    ++num_inclusions;
    const int subset_c = relevant_constraints[subset];
    const int superset_c = relevant_constraints[superset];
    ConstraintProto* subset_ct =
        context_->working_model->mutable_constraints(subset_c);
    ConstraintProto* superset_ct =
        context_->working_model->mutable_constraints(superset_c);
    if (subset_ct->constraint_case() != ConstraintProto::kBoolAnd) return;

    context_->tmp_literal_set.clear();
    for (const int ref : subset_ct->bool_and().literals()) {
      context_->tmp_literal_set.insert(ref);
    }

    // Filter superset enforcement.
    {
      int new_size = 0;
      for (const int ref : superset_ct->enforcement_literal()) {
        if (context_->tmp_literal_set.contains(ref)) {
          context_->UpdateRuleStats("bool_and: filtered enforcement");
        } else if (context_->tmp_literal_set.contains(NegatedRef(ref))) {
          context_->UpdateRuleStats("bool_and: never enforced");
          superset_ct->Clear();
          context_->UpdateConstraintVariableUsage(superset_c);
          detector.StopProcessingCurrentSuperset();
          return;
        } else {
          superset_ct->set_enforcement_literal(new_size++, ref);
        }
      }
      if (new_size < superset_ct->bool_and().literals().size()) {
        context_->UpdateConstraintVariableUsage(superset_c);
        superset_ct->mutable_enforcement_literal()->Truncate(new_size);
      }
    }

    if (superset_ct->constraint_case() == ConstraintProto::kBoolAnd) {
      int new_size = 0;
      for (const int ref : superset_ct->bool_and().literals()) {
        if (context_->tmp_literal_set.contains(ref)) {
          context_->UpdateRuleStats("bool_and: filtered literal");
        } else if (context_->tmp_literal_set.contains(NegatedRef(ref))) {
          context_->UpdateRuleStats("bool_and: must be false");
          if (!MarkConstraintAsFalse(superset_ct)) return;
          context_->UpdateConstraintVariableUsage(superset_c);
          detector.StopProcessingCurrentSuperset();
          return;
        } else {
          superset_ct->mutable_bool_and()->set_literals(new_size++, ref);
        }
      }
      if (new_size < superset_ct->bool_and().literals().size()) {
        context_->UpdateConstraintVariableUsage(superset_c);
        superset_ct->mutable_bool_and()->mutable_literals()->Truncate(new_size);
      }
    }

    if (superset_ct->constraint_case() == ConstraintProto::kLinear) {
      context_->UpdateRuleStats("TODO bool_and enforcement in linear enf");
    }
  });

  timer.AddToWork(1e-9 * static_cast<double>(detector.work_done()));
  timer.AddCounter("relevant_constraints", relevant_constraints.size());
  timer.AddCounter("num_inclusions", num_inclusions);
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
    CHECK_EQ(at_most_or_exactly_one.constraint_case(),
             ConstraintProto::kExactlyOne);
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
      CHECK_EQ(target_ref, std::numeric_limits<int>::min()) << "Uniqueness";
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
  absl::btree_map<int64_t, std::vector<int>> value_to_refs;
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
  for (auto& [value, literals] : value_to_refs) {
    // For determinism.
    absl::c_sort(literals);

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
        // TODO(user): Instead of bool_or + implications, we could add an
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

struct ColumnHashForDuplicateDetection {
  explicit ColumnHashForDuplicateDetection(
      CompactVectorVector<int, std::pair<int, int64_t>>* _column)
      : column(_column) {}
  std::size_t operator()(int c) const { return absl::HashOf((*column)[c]); }

  CompactVectorVector<int, std::pair<int, int64_t>>* column;
};

struct ColumnEqForDuplicateDetection {
  explicit ColumnEqForDuplicateDetection(
      CompactVectorVector<int, std::pair<int, int64_t>>* _column)
      : column(_column) {}
  bool operator()(int a, int b) const {
    if (a == b) return true;

    // We use absl::span<> comparison.
    return (*column)[a] == (*column)[b];
  }

  CompactVectorVector<int, std::pair<int, int64_t>>* column;
};

// Note that our symmetry-detector will also identify full permutation group
// for these columns, but it is better to handle that even before. We can
// also detect variable with different domains but with indentical columns.
void CpModelPresolver::DetectDuplicateColumns() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  const int num_vars = context_->working_model->variables().size();
  const int num_constraints = context_->working_model->constraints().size();

  // Our current implementation require almost a full copy.
  // First construct a transpose var to columns (constraint_index, coeff).
  std::vector<int> flat_vars;
  std::vector<std::pair<int, int64_t>> flat_terms;
  CompactVectorVector<int, std::pair<int, int64_t>> var_to_columns;

  // We will only support columns that include:
  // - objective
  // - linear (non-enforced part)
  // - at_most_one/exactly_one/clauses (but with positive variable only).
  //
  // TODO(user): deal with enforcement_literal, especially bool_and. It is a bit
  // annoying to have to deal with all kind of constraints. Maybe convert
  // bool_and to at_most_one first? We already do that in other places. Note
  // however that an at most one of size 2 means at most 2 columns can be
  // identical. If we have a bool and with many term on the left, all column
  // could be indentical, but we have to linearize the constraint first.
  std::vector<bool> appear_in_amo(num_vars, false);
  std::vector<bool> appear_in_bool_constraint(num_vars, false);
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    absl::Span<const int> literals;

    bool is_amo = false;
    if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
      is_amo = true;
      literals = ct.at_most_one().literals();
    } else if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      is_amo = true;  // That works here.
      literals = ct.exactly_one().literals();
    } else if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      literals = ct.bool_or().literals();
    }

    if (!literals.empty()) {
      for (const int lit : literals) {
        // It is okay to ignore terms (the columns will not be full).
        if (!RefIsPositive(lit)) continue;
        if (is_amo) appear_in_amo[lit] = true;
        appear_in_bool_constraint[lit] = true;
        flat_vars.push_back(lit);
        flat_terms.push_back({c, 1});
      }
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::kLinear) {
      const int num_terms = ct.linear().vars().size();
      for (int i = 0; i < num_terms; ++i) {
        const int var = ct.linear().vars(i);
        const int64_t coeff = ct.linear().coeffs(i);
        flat_vars.push_back(var);
        flat_terms.push_back({c, coeff});
      }
      continue;
    }
  }

  // Use kObjectiveConstraint (-1) for the objective.
  //
  // TODO(user): deal with equivalent column with different objective value.
  // It might not be easy to presolve, but we can at least have a single
  // variable = sum of var appearing only in objective. And we can transfer the
  // min cost.
  if (context_->working_model->has_objective()) {
    context_->WriteObjectiveToProto();
    const int num_terms = context_->working_model->objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = context_->working_model->objective().vars(i);
      const int64_t coeff = context_->working_model->objective().coeffs(i);
      flat_vars.push_back(var);
      flat_terms.push_back({kObjectiveConstraint, coeff});
    }
  }

  // Now construct the graph.
  var_to_columns.ResetFromFlatMapping(flat_vars, flat_terms);

  // Find duplicate columns using an hash map.
  // We only consider "full" columns.
  // var -> var_representative using columns hash/comparison.
  absl::flat_hash_map<int, int, ColumnHashForDuplicateDetection,
                      ColumnEqForDuplicateDetection>
      duplicates(
          /*capacity=*/num_vars,
          ColumnHashForDuplicateDetection(&var_to_columns),
          ColumnEqForDuplicateDetection(&var_to_columns));
  std::vector<int> flat_duplicates;
  std::vector<int> flat_representatives;
  for (int var = 0; var < var_to_columns.size(); ++var) {
    const int size_seen = var_to_columns[var].size();
    if (size_seen == 0) continue;
    if (size_seen != context_->VarToConstraints(var).size()) continue;

    // TODO(user): If we have duplicate columns appearing in Boolean constraint
    // we can only easily substitute if the sum of columns is a Boolean (i.e. if
    // it appear in an at most one or exactly one). Otherwise we will need to
    // transform such constraint to linear, do that?
    if (appear_in_bool_constraint[var] && !appear_in_amo[var]) {
      context_->UpdateRuleStats(
          "TODO duplicate: duplicate columns in Boolean constraints");
      continue;
    }

    const auto [it, inserted] = duplicates.insert({var, var});
    if (!inserted) {
      flat_duplicates.push_back(var);
      flat_representatives.push_back(it->second);
    }
  }

  // Process duplicates.
  int num_equivalent_classes = 0;
  CompactVectorVector<int, int> rep_to_dups;
  rep_to_dups.ResetFromFlatMapping(flat_representatives, flat_duplicates);
  std::vector<std::pair<int, int64_t>> definition;
  std::vector<int> var_to_remove;
  std::vector<int> var_to_rep(num_vars, -1);
  for (int var = 0; var < rep_to_dups.size(); ++var) {
    if (rep_to_dups[var].empty()) continue;

    // Since columns are the same, we can introduce a new variable = sum all
    // columns. Note that we shouldn't have any overflow here by the
    // precondition on our variable domains.
    //
    // In the corner case where there is a lot of holes in the domain, and the
    // sum domain is too complex, we skip. Hopefully this should be rare.
    definition.clear();
    definition.push_back({var, 1});
    Domain domain = context_->DomainOf(var);
    for (const int other_var : rep_to_dups[var]) {
      definition.push_back({other_var, 1});
      domain = domain.AdditionWith(context_->DomainOf(other_var));
      if (domain.NumIntervals() > 100) break;
    }
    if (domain.NumIntervals() > 100) {
      context_->UpdateRuleStats(
          "TODO duplicate: domain of the sum is too complex");
      continue;
    }
    if (appear_in_amo[var]) {
      domain = domain.IntersectionWith(Domain(0, 1));
    }
    const int new_var = context_->NewIntVarWithDefinition(
        domain, definition, /*append_constraint_to_mapping_model=*/true);
    CHECK_NE(new_var, -1);

    var_to_remove.push_back(var);
    CHECK_EQ(var_to_rep[var], -1);
    var_to_rep[var] = new_var;
    for (const int other_var : rep_to_dups[var]) {
      var_to_remove.push_back(other_var);
      CHECK_EQ(var_to_rep[other_var], -1);
      var_to_rep[other_var] = new_var;
    }

    // Deal with objective right away.
    const int64_t obj_coeff = context_->ObjectiveCoeff(var);
    if (obj_coeff != 0) {
      context_->RemoveVariableFromObjective(var);
      for (const int other_var : rep_to_dups[var]) {
        CHECK_EQ(context_->ObjectiveCoeff(other_var), obj_coeff);
        context_->RemoveVariableFromObjective(other_var);
      }
      context_->AddToObjective(new_var, obj_coeff);
    }

    num_equivalent_classes++;
  }

  // Lets rescan the model, and remove all variables, replacing them by
  // the sum. We do that in one O(model size) pass.
  if (!var_to_remove.empty()) {
    absl::flat_hash_set<int> seen;
    std::vector<std::pair<int, int64_t>> new_terms;
    for (int c = 0; c < num_constraints; ++c) {
      ConstraintProto* mutable_ct =
          context_->working_model->mutable_constraints(c);

      seen.clear();
      new_terms.clear();

      // Deal with bool case.
      // TODO(user): maybe converting to linear + single code is better?
      BoolArgumentProto* mutable_arg = nullptr;
      if (mutable_ct->constraint_case() == ConstraintProto::kAtMostOne) {
        mutable_arg = mutable_ct->mutable_at_most_one();
      } else if (mutable_ct->constraint_case() ==
                 ConstraintProto::kExactlyOne) {
        mutable_arg = mutable_ct->mutable_exactly_one();
      } else if (mutable_ct->constraint_case() == ConstraintProto::kBoolOr) {
        mutable_arg = mutable_ct->mutable_bool_or();
      }
      if (mutable_arg != nullptr) {
        int new_size = 0;
        const int num_terms = mutable_arg->literals().size();
        for (int i = 0; i < num_terms; ++i) {
          const int lit = mutable_arg->literals(i);
          const int rep = var_to_rep[PositiveRef(lit)];
          if (rep != -1) {
            CHECK(RefIsPositive(lit));
            const auto [_, inserted] = seen.insert(rep);
            if (inserted) new_terms.push_back({rep, 1});
            continue;
          }
          mutable_arg->set_literals(new_size, lit);
          ++new_size;
        }
        if (new_size == num_terms) continue;  // skip.

        // TODO(user): clear amo/exo of size 1.
        mutable_arg->mutable_literals()->Truncate(new_size);
        for (const auto [var, coeff] : new_terms) {
          mutable_arg->add_literals(var);
        }
        context_->UpdateConstraintVariableUsage(c);
        continue;
      }

      // Deal with linear case.
      if (mutable_ct->constraint_case() == ConstraintProto::kLinear) {
        int new_size = 0;
        LinearConstraintProto* mutable_linear = mutable_ct->mutable_linear();
        const int num_terms = mutable_linear->vars().size();
        for (int i = 0; i < num_terms; ++i) {
          const int var = mutable_linear->vars(i);
          const int64_t coeff = mutable_linear->coeffs(i);
          const int rep = var_to_rep[var];
          if (rep != -1) {
            const auto [_, inserted] = seen.insert(rep);
            if (inserted) new_terms.push_back({rep, coeff});
            continue;
          }
          mutable_linear->set_vars(new_size, var);
          mutable_linear->set_coeffs(new_size, coeff);
          ++new_size;
        }
        if (new_size == num_terms) continue;  // skip.

        mutable_linear->mutable_vars()->Truncate(new_size);
        mutable_linear->mutable_coeffs()->Truncate(new_size);
        for (const auto [var, coeff] : new_terms) {
          mutable_linear->add_vars(var);
          mutable_linear->add_coeffs(coeff);
        }
        context_->UpdateConstraintVariableUsage(c);
        continue;
      }
    }
  }

  // We removed all occurrence of "var_to_remove" so we can remove them now.
  // Note that since we introduce a new variable per equivalence class, we
  // remove one less for each equivalent class.
  const int num_var_reduction = var_to_remove.size() - num_equivalent_classes;
  for (const int var : var_to_remove) {
    CHECK(context_->VarToConstraints(var).empty());
    context_->MarkVariableAsRemoved(var);
  }
  if (num_var_reduction > 0) {
    context_->UpdateRuleStats("duplicate: removed duplicated column",
                              num_var_reduction);
  }

  timer.AddCounter("num_equiv_classes", num_equivalent_classes);
  timer.AddCounter("num_removed_vars", num_var_reduction);
}

void CpModelPresolver::DetectDuplicateConstraints() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // We need the objective written for this.
  if (context_->working_model->has_objective()) {
    if (!context_->CanonicalizeObjective()) return;
    context_->WriteObjectiveToProto();
  }

  // Remove duplicate constraints.
  // Note that at this point the objective in the proto should be up to date.
  //
  // TODO(user): We might want to do that earlier so that our count of variable
  // usage is not biased by duplicate constraints.
  const std::vector<std::pair<int, int>> duplicates =
      FindDuplicateConstraints(*context_->working_model);
  timer.AddCounter("duplicates", duplicates.size());
  for (const auto& [dup, rep] : duplicates) {
    // Note that it is important to look at the type of the representative in
    // case the constraint became empty.
    DCHECK_LT(kObjectiveConstraint, 0);
    const int type =
        rep == kObjectiveConstraint
            ? kObjectiveConstraint
            : context_->working_model->constraints(rep).constraint_case();

    // For linear constraint, we merge their rhs since it was ignored in the
    // FindDuplicateConstraints() call.
    if (type == ConstraintProto::kLinear) {
      const Domain rep_domain = ReadDomainFromProto(
          context_->working_model->constraints(rep).linear());
      const Domain d = ReadDomainFromProto(
          context_->working_model->constraints(dup).linear());
      if (rep_domain != d) {
        context_->UpdateRuleStats("duplicate: merged rhs of linear constraint");
        const Domain rhs = rep_domain.IntersectionWith(d);
        if (rhs.IsEmpty()) {
          if (!MarkConstraintAsFalse(
                  context_->working_model->mutable_constraints(rep))) {
            SOLVER_LOG(logger_, "Unsat after merging two linear constraints");
            return;
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

    if (type == kObjectiveConstraint) {
      context_->UpdateRuleStats(
          "duplicate: linear constraint parallel to objective");
      const Domain objective_domain =
          ReadDomainFromProto(context_->working_model->objective());
      const Domain d = ReadDomainFromProto(
          context_->working_model->constraints(dup).linear());
      if (objective_domain != d) {
        context_->UpdateRuleStats("duplicate: updated objective domain");
        const Domain new_domain = objective_domain.IntersectionWith(d);
        if (new_domain.IsEmpty()) {
          return (void)context_->NotifyThatModelIsUnsat(
              "Constraint parallel to the objective makes the objective domain "
              "empty.");
        }
        FillDomainInProto(new_domain,
                          context_->working_model->mutable_objective());

        // TODO(user): this write/read is a bit unclean, but needed.
        context_->ReadObjectiveFromProto();
      }
    }
    context_->working_model->mutable_constraints(dup)->Clear();
    context_->UpdateConstraintVariableUsage(dup);
    context_->UpdateRuleStats("duplicate: removed constraint");
  }
}

void CpModelPresolver::DetectDuplicateConstraintsWithDifferentEnforcements(
    const CpModelMapping* mapping, BinaryImplicationGraph* implication_graph,
    Trail* trail) {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // We need the objective written for this.
  if (context_->working_model->has_objective()) {
    if (!context_->CanonicalizeObjective()) return;
    context_->WriteObjectiveToProto();
  }

  absl::flat_hash_set<Literal> enforcement_vars;
  std::vector<std::pair<Literal, Literal>> implications_used;
  // TODO(user): We can also do similar stuff to linear constraint that just
  // differ at a singleton variable. Or that are equalities. Like if expr + X =
  // cte and expr + Y = other_cte, we can see that X is in affine relation with
  // Y.
  const std::vector<std::pair<int, int>> duplicates_without_enforcement =
      FindDuplicateConstraints(*context_->working_model, true);
  timer.AddCounter("without_enforcements",
                   duplicates_without_enforcement.size());
  for (const auto& [dup, rep] : duplicates_without_enforcement) {
    auto* dup_ct = context_->working_model->mutable_constraints(dup);
    auto* rep_ct = context_->working_model->mutable_constraints(rep);

    // Make sure our enforcement list are up to date: nothing fixed and that
    // its uses the literal representatives.
    if (PresolveEnforcementLiteral(dup_ct)) {
      context_->UpdateConstraintVariableUsage(dup);
    }
    if (PresolveEnforcementLiteral(rep_ct)) {
      context_->UpdateConstraintVariableUsage(rep);
    }

    // Skip this pair if one of the constraint was simplified
    if (rep_ct->constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET ||
        dup_ct->constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
      continue;
    }

    // If one of them has no enforcement, then the other can be ignored.
    // We always keep rep, but clear its enforcement if any.
    if (dup_ct->enforcement_literal().empty() ||
        rep_ct->enforcement_literal().empty()) {
      context_->UpdateRuleStats("duplicate: removed enforced constraint");
      rep_ct->mutable_enforcement_literal()->Clear();
      context_->UpdateConstraintVariableUsage(rep);
      dup_ct->Clear();
      context_->UpdateConstraintVariableUsage(dup);
      continue;
    }

    const int a = rep_ct->enforcement_literal(0);
    const int b = dup_ct->enforcement_literal(0);

    if (a == NegatedRef(b) && rep_ct->enforcement_literal().size() == 1 &&
        dup_ct->enforcement_literal().size() == 1) {
      context_->UpdateRuleStats(
          "duplicate: both with enforcement and its negation");
      rep_ct->mutable_enforcement_literal()->Clear();
      context_->UpdateConstraintVariableUsage(rep);
      dup_ct->Clear();
      context_->UpdateConstraintVariableUsage(dup);
      continue;
    }

    // Special case. This looks specific but users might reify with a cost
    // a duplicate constraint. In this case, no need to have two variables,
    // we can make them equal by duality argument.
    //
    // TODO(user): Deal with more general situation? Note that we already
    // do something similar in dual_bound_strengthening.Strengthen() were we
    // are more general as we just require an unique blocking constraint rather
    // than a singleton variable.
    //
    // But we could detect that "a <=> constraint" and "b <=> constraint", then
    // we can also add the equality. Alternatively, we can just introduce a new
    // variable and merge all duplicate constraint into 1 + bunch of boolean
    // constraints liking enforcements.
    if (context_->VariableWithCostIsUniqueAndRemovable(a) &&
        context_->VariableWithCostIsUniqueAndRemovable(b)) {
      // Both these case should be presolved before, but it is easy to deal with
      // if we encounter them here in some corner cases.
      bool skip = false;
      if (RefIsPositive(a) == context_->ObjectiveCoeff(PositiveRef(a)) > 0) {
        context_->UpdateRuleStats("duplicate: dual fixing enforcement.");
        if (!context_->SetLiteralToFalse(a)) return;
        skip = true;
      }
      if (RefIsPositive(b) == context_->ObjectiveCoeff(PositiveRef(b)) > 0) {
        context_->UpdateRuleStats("duplicate: dual fixing enforcement.");
        if (!context_->SetLiteralToFalse(b)) return;
        skip = true;
      }
      if (skip) continue;

      // If there are more than one enforcement literal, then the Booleans
      // are not necessarily equivalent: if a constraint is disabled by other
      // literal, we don't want to put a or b at 1 and pay an extra cost.
      //
      // TODO(user): If a is alone, then b==1 can implies a == 1.
      // We can also replace [(b, others) => constraint] with (b, others) <=> a.
      //
      // TODO(user): If the other enforcements are the same, we can also add
      // the equivalence and remove the duplicate constraint.
      if (rep_ct->enforcement_literal().size() > 1 ||
          dup_ct->enforcement_literal().size() > 1) {
        context_->UpdateRuleStats(
            "TODO duplicate: identical constraint with unique enforcement "
            "cost");
        continue;
      }

      // Sign is correct, i.e. ignoring the constraint is expensive.
      // The two enforcement can be made equivalent.
      context_->UpdateRuleStats("duplicate: dual equivalence of enforcement");
      context_->StoreBooleanEqualityRelation(a, b);

      // We can also remove duplicate constraint now. It will be done later but
      // it seems more efficient to just do it now.
      if (dup_ct->enforcement_literal().size() == 1 &&
          rep_ct->enforcement_literal().size() == 1) {
        dup_ct->Clear();
        context_->UpdateConstraintVariableUsage(dup);
        continue;
      }
    }

    // Check if the enforcement of one constraint implies the ones of the other.
    if (implication_graph != nullptr && mapping != nullptr &&
        trail != nullptr) {
      for (int i = 0; i < 2; i++) {
        // When A and B only differ on their enforcement literals and the
        // enforcements of constraint A implies the enforcements of constraint
        // B, then constraint A is redundant and we can remove it.
        const int c_a = i == 0 ? dup : rep;
        const int c_b = i == 0 ? rep : dup;
        const auto& ct_a = context_->working_model->constraints(c_a);
        const auto& ct_b = context_->working_model->constraints(c_b);

        enforcement_vars.clear();
        implications_used.clear();
        for (const int proto_lit : ct_b.enforcement_literal()) {
          const Literal lit = mapping->Literal(proto_lit);
          DCHECK(!trail->Assignment().LiteralIsAssigned(lit));
          enforcement_vars.insert(lit);
        }
        for (const int proto_lit : ct_a.enforcement_literal()) {
          const Literal lit = mapping->Literal(proto_lit);
          DCHECK(!trail->Assignment().LiteralIsAssigned(lit));
          for (const Literal implication_lit :
               implication_graph->DirectImplications(lit)) {
            auto extracted = enforcement_vars.extract(implication_lit);
            if (!extracted.empty() && lit != implication_lit) {
              implications_used.push_back({lit, implication_lit});
            }
          }
        }
        if (enforcement_vars.empty()) {
          // Tricky: Because we keep track of literal <=> var == value, we
          // cannot easily simplify linear1 here. This is because a scenario
          // like this can happen:
          //
          // We have registered the fact that a <=> X=1 because we saw two
          // constraints a => X=1 and not(a) => X!= 1
          //
          // Now, we are here and we have:
          // a => X=1, b => X=1, a => b
          // So we rewrite this as
          // a => b, b => X=1
          //
          // But later, the PresolveLinearOfSizeOne() see
          // b => X=1 and just rewrite this as b => a since (a <=> X=1).
          // This is wrong because the constraint "b => X=1" is needed for the
          // equivalence (a <=> X=1), but we lost that fact.
          //
          // Note(user): In the scenario above we can see that a <=> b, and if
          // we know that fact, then the transformation is correctly handled.
          // The bug was triggered when the Probing finished early due to time
          // limit and we never detected that equivalence.
          //
          // TODO(user): Try to find a cleaner way to handle this. We could
          // query our HasVarValueEncoding() directly here and directly detect a
          // <=> b. However we also need to figure the case of
          // half-implications.
          {
            if (ct_a.constraint_case() == ConstraintProto::kLinear &&
                ct_a.linear().vars().size() == 1 &&
                ct_a.enforcement_literal().size() == 1) {
              const int var = ct_a.linear().vars(0);
              const Domain var_domain = context_->DomainOf(var);
              const Domain rhs =
                  ReadDomainFromProto(ct_a.linear())
                      .InverseMultiplicationBy(ct_a.linear().coeffs(0))
                      .IntersectionWith(var_domain);

              // IsFixed() do not work on empty domain.
              if (rhs.IsEmpty()) {
                context_->UpdateRuleStats("duplicate: linear1 infeasible");
                if (!MarkConstraintAsFalse(rep_ct)) return;
                if (!MarkConstraintAsFalse(dup_ct)) return;
                context_->UpdateConstraintVariableUsage(rep);
                context_->UpdateConstraintVariableUsage(dup);
                continue;
              }
              if (rhs == var_domain) {
                context_->UpdateRuleStats("duplicate: linear1 always true");
                rep_ct->Clear();
                dup_ct->Clear();
                context_->UpdateConstraintVariableUsage(rep);
                context_->UpdateConstraintVariableUsage(dup);
                continue;
              }

              // We skip if it is a var == value or var != value constraint.
              if (rhs.IsFixed() ||
                  rhs.Complement().IntersectionWith(var_domain).IsFixed()) {
                context_->UpdateRuleStats(
                    "TODO duplicate: skipped identical encoding constraints");
                continue;
              }
            }
          }

          context_->UpdateRuleStats(
              "duplicate: identical constraint with implied enforcements");
          if (c_a == rep) {
            // We don't want to remove the representative element of the
            // duplicates detection, so swap the constraints.
            rep_ct->Swap(dup_ct);
            context_->UpdateConstraintVariableUsage(rep);
          }
          dup_ct->Clear();
          context_->UpdateConstraintVariableUsage(dup);
          // Subtle point: we need to add the implications we used back to the
          // graph. This is because in some case the implications are only true
          // in the presence of the "duplicated" constraints.
          for (const auto& [a, b] : implications_used) {
            const int proto_lit_a = mapping->GetProtoLiteralFromLiteral(a);
            const int proto_lit_b = mapping->GetProtoLiteralFromLiteral(b);
            context_->AddImplication(proto_lit_a, proto_lit_b);
          }
          context_->UpdateNewConstraintsVariableUsage();
          break;
        }
      }
    }
  }
}

void CpModelPresolver::DetectDifferentVariables() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // List the variable that are pairwise different, also store in offset[x, y]
  // the offsets such that x >= y + offset.second OR y >= x + offset.first.
  std::vector<std::pair<int, int>> different_vars;
  absl::flat_hash_map<std::pair<int, int>, std::pair<int64_t, int64_t>> offsets;

  // Process the fact "v1 - v2 \in Domain".
  const auto process_difference = [&different_vars, &offsets](int v1, int v2,
                                                              Domain d) {
    Domain exclusion = d.Complement().PartAroundZero();
    if (exclusion.IsEmpty()) return;
    if (v1 == v2) return;
    std::pair<int, int> key = {v1, v2};
    if (v1 > v2) {
      std::swap(key.first, key.second);
      exclusion = exclusion.Negation();
    }

    // We have x - y not in exclusion,
    // so x - y > exclusion.Max() --> x > y + exclusion.Max();
    // OR x - y < exclusion.Min() --> y > x - exclusion.Min();
    different_vars.push_back(key);
    offsets[key] = {exclusion.Min() == std::numeric_limits<int64_t>::min()
                        ? std::numeric_limits<int64_t>::max()
                        : CapAdd(-exclusion.Min(), 1),
                    CapAdd(exclusion.Max(), 1)};
  };

  // Try to find identical linear constraint with incompatible domains.
  // This works really well on neos16.mps.gz where we have
  // a <=> x <= y
  // b <=> x >= y
  // and a => not(b),
  // Because of this presolve, we detect that not(a) => b and thus that a and
  // not(b) are equivalent. We can thus simplify the problem to just
  // a => x < y
  // not(a) => x > y
  //
  // TODO(user): On that same problem, we could actually just have x != y and
  // remove the enforcement literal that is just used for that. But then we
  // will just re-create it, since we don't have a native way to handle x != y.
  //
  // TODO(user): Again on neos16.mps, we actually have cliques of x != y so we
  // end up with a bunch of groups of 7 variables in [0, 6] that are all
  // different. If we can detect that, then we close the problem quickly instead
  // of not closing it.
  bool has_all_diff = false;
  bool has_no_overlap = false;
  std::vector<std::pair<uint64_t, int>> hashes;
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() == ConstraintProto::kAllDiff) {
      has_all_diff = true;
      continue;
    }
    if (ct.constraint_case() == ConstraintProto::kNoOverlap) {
      has_no_overlap = true;
      continue;
    }
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    if (ct.linear().vars().size() == 1) continue;

    // Detect direct encoding of x != y. Note that we also see that from x > y
    // and related.
    if (ct.linear().vars().size() == 2 && ct.enforcement_literal().empty() &&
        ct.linear().coeffs(0) == -ct.linear().coeffs(1)) {
      // We assume the constraint was already divided by its gcd.
      if (ct.linear().coeffs(0) == 1) {
        process_difference(ct.linear().vars(0), ct.linear().vars(1),
                           ReadDomainFromProto(ct.linear()));
      } else if (ct.linear().coeffs(0) == -1) {
        process_difference(ct.linear().vars(1), ct.linear().vars(0),
                           ReadDomainFromProto(ct.linear()).Negation());
      }
    }

    // TODO(user): Handle this case?
    if (ct.enforcement_literal().size() > 1) continue;

    uint64_t hash = kDefaultFingerprintSeed;
    hash = FingerprintRepeatedField(ct.linear().vars(), hash);
    hash = FingerprintRepeatedField(ct.linear().coeffs(), hash);
    hashes.push_back({hash, c});
  }
  std::sort(hashes.begin(), hashes.end());
  for (int next, start = 0; start < hashes.size(); start = next) {
    next = start + 1;
    while (next < hashes.size() && hashes[next].first == hashes[start].first) {
      ++next;
    }
    absl::Span<const std::pair<uint64_t, int>> range(&hashes[start],
                                                     next - start);
    if (range.size() <= 1) continue;
    if (range.size() > 10) continue;

    for (int i = 0; i < range.size(); ++i) {
      const ConstraintProto& ct1 =
          context_->working_model->constraints(range[i].second);
      const int num_terms = ct1.linear().vars().size();
      for (int j = i + 1; j < range.size(); ++j) {
        const ConstraintProto& ct2 =
            context_->working_model->constraints(range[j].second);
        if (ct2.linear().vars().size() != num_terms) continue;
        if (!ReadDomainFromProto(ct1.linear())
                 .IntersectionWith(ReadDomainFromProto(ct2.linear()))
                 .IsEmpty()) {
          continue;
        }
        if (absl::MakeSpan(ct1.linear().vars().data(), num_terms) !=
            absl::MakeSpan(ct2.linear().vars().data(), num_terms)) {
          continue;
        }
        if (absl::MakeSpan(ct1.linear().coeffs().data(), num_terms) !=
            absl::MakeSpan(ct2.linear().coeffs().data(), num_terms)) {
          continue;
        }

        if (ct1.enforcement_literal().empty() &&
            ct2.enforcement_literal().empty()) {
          (void)context_->NotifyThatModelIsUnsat(
              "two incompatible linear constraint");
          return;
        }
        if (ct1.enforcement_literal().empty()) {
          context_->UpdateRuleStats(
              "incompatible linear: set enforcement to false");
          if (!context_->SetLiteralToFalse(ct2.enforcement_literal(0))) {
            return;
          }
          continue;
        }
        if (ct2.enforcement_literal().empty()) {
          context_->UpdateRuleStats(
              "incompatible linear: set enforcement to false");
          if (!context_->SetLiteralToFalse(ct1.enforcement_literal(0))) {
            return;
          }
          continue;
        }

        const int lit1 = ct1.enforcement_literal(0);
        const int lit2 = ct2.enforcement_literal(0);

        // Detect x != y via lit => x > y && not(lit) => x < y.
        if (ct1.linear().vars().size() == 2 &&
            ct1.linear().coeffs(0) == -ct1.linear().coeffs(1) &&
            lit1 == NegatedRef(lit2)) {
          // We have x - y in domain1 or in domain2, so it must be in the union.
          Domain union_of_domain =
              ReadDomainFromProto(ct1.linear())
                  .UnionWith(ReadDomainFromProto(ct2.linear()));

          // We assume the constraint was already divided by its gcd.
          if (ct1.linear().coeffs(0) == 1) {
            process_difference(ct1.linear().vars(0), ct1.linear().vars(1),
                               std::move(union_of_domain));
          } else if (ct1.linear().coeffs(0) == -1) {
            process_difference(ct1.linear().vars(1), ct1.linear().vars(0),
                               union_of_domain.Negation());
          }
        }

        if (lit1 != NegatedRef(lit2)) {
          context_->UpdateRuleStats("incompatible linear: add implication");
          context_->AddImplication(lit1, NegatedRef(lit2));
        }
      }
    }
  }

  // Detect all_different cliques.
  // We reuse the max-clique code from sat.
  //
  // TODO(user): To avoid doing that more than once, we only run it if there
  // is no all-diff in the model already. This is not perfect.
  //
  // Note(user): The all diff added here will not be expanded since we run this
  // after expansion. This is fragile though. Not even sure this is what we
  // want.
  //
  // TODO(user): Start with the existing all diff and expand them rather than
  // not running this if there are all_diff present.
  //
  // TODO(user): Only add them at the end of the presolve! it hurt our presolve
  // (like probing is slower) and only serve for linear relaxation.
  if (context_->params().infer_all_diffs() && !has_all_diff &&
      !has_no_overlap && different_vars.size() > 2) {
    WallTimer local_time;
    local_time.Start();

    std::vector<std::vector<Literal>> cliques;
    absl::flat_hash_set<int> used_var;

    Model local_model;
    const int num_variables = context_->working_model->variables().size();
    local_model.GetOrCreate<Trail>()->Resize(num_variables);
    auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
    graph->Resize(num_variables);
    for (const auto [var1, var2] : different_vars) {
      if (!RefIsPositive(var1)) continue;
      if (!RefIsPositive(var2)) continue;
      if (var1 == var2) {
        (void)context_->NotifyThatModelIsUnsat("x != y with x == y");
        return;
      }
      // All variables at false is always a valid solution of the local model,
      // so this should never return UNSAT.
      CHECK(graph->AddAtMostOne({Literal(BooleanVariable(var1), true),
                                 Literal(BooleanVariable(var2), true)}));
      if (!used_var.contains(var1)) {
        used_var.insert(var1);
        cliques.push_back({Literal(BooleanVariable(var1), true),
                           Literal(BooleanVariable(var2), true)});
      }
      if (!used_var.contains(var2)) {
        used_var.insert(var2);
        cliques.push_back({Literal(BooleanVariable(var1), true),
                           Literal(BooleanVariable(var2), true)});
      }
    }
    CHECK(graph->DetectEquivalences());
    graph->TransformIntoMaxCliques(&cliques, 1e8);

    int num_cliques = 0;
    int64_t cumulative_size = 0;
    for (std::vector<Literal>& clique : cliques) {
      if (clique.size() <= 2) continue;

      ++num_cliques;
      cumulative_size += clique.size();
      std::sort(clique.begin(), clique.end());

      // We have an all-diff, but inspect the offsets to see if we have a
      // disjunctive ! Note that this is quadratic, but no more complex than the
      // scan of the model we just did above, since we had one linear constraint
      // per entry.
      const int num_terms = clique.size();
      std::vector<int64_t> sizes(num_terms,
                                 std::numeric_limits<int64_t>::max());
      for (int i = 0; i < num_terms; ++i) {
        const int v1 = clique[i].Variable().value();
        for (int j = i + 1; j < num_terms; ++j) {
          const int v2 = clique[j].Variable().value();
          const auto [o1, o2] = offsets.at({v1, v2});
          sizes[i] = std::min(sizes[i], o1);
          sizes[j] = std::min(sizes[j], o2);
        }
      }

      int num_greater_than_one = 0;
      int64_t issue = 0;
      for (int i = 0; i < num_terms; ++i) {
        CHECK_GE(sizes[i], 1);
        if (sizes[i] > 1) ++num_greater_than_one;

        // When this happens, it means this interval can never be before
        // any other. We should probably handle this case better, but for now we
        // abort.
        issue = CapAdd(issue, sizes[i]);
        if (issue == std::numeric_limits<int64_t>::max()) {
          context_->UpdateRuleStats("TODO no_overlap: with task always last");
          num_greater_than_one = 0;
          break;
        }
      }

      if (num_greater_than_one > 0) {
        // We have one size greater than 1, lets add a no_overlap!
        //
        // TODO(user): try to remove all the quadratic boolean and their
        // corresponding linear2 ? Any Boolean not used elsewhere could be
        // removed.
        context_->UpdateRuleStats(
            "no_overlap: inferred from x != y constraints");

        std::vector<int> intervals;
        for (int i = 0; i < num_terms; ++i) {
          intervals.push_back(context_->working_model->constraints().size());
          auto* new_interval =
              context_->working_model->add_constraints()->mutable_interval();
          new_interval->mutable_start()->set_offset(0);
          new_interval->mutable_start()->add_coeffs(1);
          new_interval->mutable_start()->add_vars(clique[i].Variable().value());

          new_interval->mutable_size()->set_offset(sizes[i]);

          new_interval->mutable_end()->set_offset(sizes[i]);
          new_interval->mutable_end()->add_coeffs(1);
          new_interval->mutable_end()->add_vars(clique[i].Variable().value());
        }
        auto* new_ct =
            context_->working_model->add_constraints()->mutable_no_overlap();
        for (const int interval : intervals) {
          new_ct->add_intervals(interval);
        }
      } else {
        context_->UpdateRuleStats("all_diff: inferred from x != y constraints");
        auto* new_ct =
            context_->working_model->add_constraints()->mutable_all_diff();
        for (const Literal l : clique) {
          auto* expr = new_ct->add_exprs();
          expr->add_vars(l.Variable().value());
          expr->add_coeffs(1);
        }
      }
    }

    timer.AddCounter("different", different_vars.size());
    timer.AddCounter("cliques", num_cliques);
    timer.AddCounter("size", cumulative_size);
  }

  context_->UpdateNewConstraintsVariableUsage();
}

namespace {

// Add factor * subset_ct to the given superset_ct.
void Substitute(int64_t factor,
                const absl::flat_hash_map<int, int64_t>& subset_coeff_map,
                const Domain& subset_rhs, const Domain& superset_rhs,
                LinearConstraintProto* mutable_linear) {
  int new_size = 0;
  const int old_size = mutable_linear->vars().size();
  for (int i = 0; i < old_size; ++i) {
    const int var = mutable_linear->vars(i);
    int64_t coeff = mutable_linear->coeffs(i);
    const auto it = subset_coeff_map.find(var);
    if (it != subset_coeff_map.end()) {
      coeff += factor * it->second;
      if (coeff == 0) continue;
    }

    mutable_linear->set_vars(new_size, var);
    mutable_linear->set_coeffs(new_size, coeff);
    ++new_size;
  }
  mutable_linear->mutable_vars()->Truncate(new_size);
  mutable_linear->mutable_coeffs()->Truncate(new_size);
  FillDomainInProto(
      superset_rhs.AdditionWith(subset_rhs.MultiplicationBy(factor)),
      mutable_linear);
}

}  // namespace

void CpModelPresolver::DetectDominatedLinearConstraints() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // Because we only deal with linear constraint and we want to ignore the
  // enforcement part, we reuse the variable list in the inclusion detector.
  // Note that we ignore "unclean" constraint, so we only have positive
  // reference there.
  class Storage {
   public:
    explicit Storage(CpModelProto* proto) : proto_(*proto) {}
    int size() const { return static_cast<int>(proto_.constraints().size()); }
    absl::Span<const int> operator[](int c) const {
      return absl::MakeSpan(proto_.constraints(c).linear().vars());
    }

   private:
    const CpModelProto& proto_;
  };
  Storage storage(context_->working_model);
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(context_->params().presolve_inclusion_work_limit());

  // Because we use the constraint <-> variable graph, we cannot modify it
  // during DetectInclusions(). So we delay the update of the graph.
  std::vector<int> constraint_indices_to_clean;

  // Cache the linear expression domain.
  // TODO(user): maybe we should store this instead of recomputing it.
  absl::flat_hash_map<int, Domain> cached_expr_domain;

  const int num_constraints = context_->working_model->constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;

    // We only look at long enforced constraint to avoid all the linear of size
    // one or two which can be numerous.
    if (!ct.enforcement_literal().empty()) {
      if (ct.linear().vars().size() < 3) continue;
    }

    if (!LinearConstraintIsClean(ct.linear())) {
      // This shouldn't happen except in potential corner cases were the
      // constraints were not canonicalized before this point. We just skip
      // such constraint.
      continue;
    }

    detector.AddPotentialSet(c);

    const auto [min_activity, max_activity] =
        context_->ComputeMinMaxActivity(ct.linear());
    cached_expr_domain[c] = Domain(min_activity, max_activity);
  }

  int64_t num_inclusions = 0;
  absl::flat_hash_map<int, int64_t> coeff_map;
  detector.DetectInclusions([&](int subset_c, int superset_c) {
    ++num_inclusions;

    // Store the coeff of the subset linear constraint in a map.
    const ConstraintProto& subset_ct =
        context_->working_model->constraints(subset_c);
    const LinearConstraintProto& subset_lin = subset_ct.linear();
    coeff_map.clear();
    detector.IncreaseWorkDone(subset_lin.vars().size());
    for (int i = 0; i < subset_lin.vars().size(); ++i) {
      coeff_map[subset_lin.vars(i)] = subset_lin.coeffs(i);
    }

    // We have a perfect match if 'factor_a * subset == factor_b * superset' on
    // the common positions. Note that assuming subset has been gcd reduced,
    // there is not point considering factor_b != 1.
    bool perfect_match = true;

    // Find interesting factor of the subset that cancels terms of the superset.
    int64_t factor = 0;
    int64_t min_pos_factor = std::numeric_limits<int64_t>::max();
    int64_t max_neg_factor = std::numeric_limits<int64_t>::min();

    // Lets compute the implied domain of the linear expression
    // "superset - subset". Note that we actually do not need exact inclusion
    // for this algorithm to work, but it is an heuristic to not try it with
    // all pair of constraints.
    const ConstraintProto& superset_ct =
        context_->working_model->constraints(superset_c);
    const LinearConstraintProto& superset_lin = superset_ct.linear();
    int64_t diff_min_activity = 0;
    int64_t diff_max_activity = 0;
    detector.IncreaseWorkDone(superset_lin.vars().size());
    for (int i = 0; i < superset_lin.vars().size(); ++i) {
      const int var = superset_lin.vars(i);
      int64_t coeff = superset_lin.coeffs(i);
      const auto it = coeff_map.find(var);

      if (it != coeff_map.end()) {
        const int64_t subset_coeff = it->second;

        const int64_t div = coeff / subset_coeff;
        if (div > 0) {
          min_pos_factor = std::min(div, min_pos_factor);
        } else {
          max_neg_factor = std::max(div, max_neg_factor);
        }

        if (perfect_match) {
          if (coeff % subset_coeff == 0) {
            if (factor == 0) {
              // Note that factor can be negative.
              factor = div;
            } else if (factor != div) {
              perfect_match = false;
            }
          } else {
            perfect_match = false;
          }
        }

        // TODO(user): compute the factor first in case it is != 1 ?
        coeff -= subset_coeff;
      }
      if (coeff == 0) continue;
      context_->CappedUpdateMinMaxActivity(var, coeff, &diff_min_activity,
                                           &diff_max_activity);
    }

    const Domain diff_domain(diff_min_activity, diff_max_activity);
    const Domain subset_rhs = ReadDomainFromProto(subset_lin);
    const Domain superset_rhs = ReadDomainFromProto(superset_lin);

    // Case 1: superset is redundant.
    // We process this one first as it let us remove the longest constraint.
    //
    // Important: because of how we computed the inclusion, the diff_domain is
    // only valid if none of the enforcement appear in the subset.
    //
    // TODO(user): Compute the correct infered domain in this case.
    if (subset_ct.enforcement_literal().empty()) {
      const Domain implied_superset_domain =
          subset_rhs.AdditionWith(diff_domain)
              .IntersectionWith(cached_expr_domain[superset_c]);
      if (implied_superset_domain.IsIncludedIn(superset_rhs)) {
        context_->UpdateRuleStats(
            "linear inclusion: redundant containing constraint");
        context_->working_model->mutable_constraints(superset_c)->Clear();
        constraint_indices_to_clean.push_back(superset_c);
        detector.StopProcessingCurrentSuperset();
        return;
      }
    }

    // Case 2: subset is redundant.
    if (superset_ct.enforcement_literal().empty()) {
      const Domain implied_subset_domain =
          superset_rhs.AdditionWith(diff_domain.Negation())
              .IntersectionWith(cached_expr_domain[subset_c]);
      if (implied_subset_domain.IsIncludedIn(subset_rhs)) {
        context_->UpdateRuleStats(
            "linear inclusion: redundant included constraint");
        context_->working_model->mutable_constraints(subset_c)->Clear();
        constraint_indices_to_clean.push_back(subset_c);
        detector.StopProcessingCurrentSubset();
        return;
      }
    }

    // If the subset is an equality, and we can add a factor of it to the
    // superset so that the activity range is guaranteed to be tighter, we
    // always do it. This should both sparsify the problem but also lead to
    // tighter propagation.
    if (subset_rhs.IsFixed() && subset_ct.enforcement_literal().empty()) {
      const int64_t best_factor =
          max_neg_factor > -min_pos_factor ? max_neg_factor : min_pos_factor;

      // Compute the activity range before and after. Because our pos/neg factor
      // are the smallest possible, if one is undefined then we are guaranteed
      // to be tighter, and do not need to compute this.
      //
      // TODO(user): can we compute the best factor that make this as tight as
      // possible instead? that looks doable.
      bool is_tigher = true;
      if (min_pos_factor != std::numeric_limits<int64_t>::max() &&
          max_neg_factor != std::numeric_limits<int64_t>::min()) {
        int64_t min_before = 0;
        int64_t max_before = 0;
        int64_t min_after = CapProd(best_factor, subset_rhs.FixedValue());
        int64_t max_after = min_after;
        for (int i = 0; i < superset_lin.vars().size(); ++i) {
          const int var = superset_lin.vars(i);
          const auto it = coeff_map.find(var);
          if (it == coeff_map.end()) continue;

          const int64_t coeff_before = superset_lin.coeffs(i);
          const int64_t coeff_after = coeff_before - best_factor * it->second;
          context_->CappedUpdateMinMaxActivity(var, coeff_before, &min_before,
                                               &max_before);
          context_->CappedUpdateMinMaxActivity(var, coeff_after, &min_after,
                                               &max_after);
        }
        is_tigher = min_after >= min_before && max_after <= max_before;
      }
      if (is_tigher) {
        context_->UpdateRuleStats("linear inclusion: sparsify superset");
        Substitute(-best_factor, coeff_map, subset_rhs, superset_rhs,
                   context_->working_model->mutable_constraints(superset_c)
                       ->mutable_linear());
        constraint_indices_to_clean.push_back(superset_c);
        detector.StopProcessingCurrentSuperset();
        return;
      }
    }

    // We do a bit more if we have an exact match and factor * subset is exactly
    // a subpart of the superset constraint.
    if (perfect_match && subset_ct.enforcement_literal().empty() &&
        superset_ct.enforcement_literal().empty()) {
      CHECK_NE(factor, 0);

      // Propagate domain on the superset - subset variables.
      // TODO(user): We can probably still do that if the inclusion is not
      // perfect.
      temp_ct_.Clear();
      auto* mutable_linear = temp_ct_.mutable_linear();
      for (int i = 0; i < superset_lin.vars().size(); ++i) {
        const int var = superset_lin.vars(i);
        const int64_t coeff = superset_lin.coeffs(i);
        const auto it = coeff_map.find(var);
        if (it != coeff_map.end()) continue;
        mutable_linear->add_vars(var);
        mutable_linear->add_coeffs(coeff);
      }
      FillDomainInProto(
          superset_rhs.AdditionWith(subset_rhs.MultiplicationBy(-factor)),
          mutable_linear);
      PropagateDomainsInLinear(/*ct_index=*/-1, &temp_ct_);
      if (context_->ModelIsUnsat()) detector.Stop();

      if (superset_rhs.IsFixed()) {
        if (subset_lin.vars().size() + 1 == superset_lin.vars().size()) {
          // Because we propagated the equation on the singleton variable above,
          // and we have an equality, the subset is redundant!
          context_->UpdateRuleStats(
              "linear inclusion: subset + singleton is equality");
          context_->working_model->mutable_constraints(subset_c)->Clear();
          constraint_indices_to_clean.push_back(subset_c);
          detector.StopProcessingCurrentSubset();
          return;
        }

        // This one could make sense if subset is large vs superset.
        context_->UpdateRuleStats(
            "TODO linear inclusion: superset is equality");
      }
    }
  });

  for (const int c : constraint_indices_to_clean) {
    context_->UpdateConstraintVariableUsage(c);
  }

  timer.AddToWork(1e-9 * static_cast<double>(detector.work_done()));
  timer.AddCounter("relevant_constraints", detector.num_potential_supersets());
  timer.AddCounter("num_inclusions", num_inclusions);
  timer.AddCounter("num_redundant", constraint_indices_to_clean.size());
}

// TODO(user): Also substitute if this appear in the objective?
// TODO(user): In some case we only need common_part <= new_var.
bool CpModelPresolver::RemoveCommonPart(
    const absl::flat_hash_map<int, int64_t>& common_var_coeff_map,
    const std::vector<std::pair<int, int64_t>>& block,
    ActivityBoundHelper* helper) {
  int new_var;
  int64_t gcd = 0;
  int64_t offset = 0;

  // If the common part is expressable via one of the constraint in the block as
  // == gcd * X + offset, we can just use this variable instead of creating a
  // new variable.
  int definiting_equation = -1;
  for (const auto [c, multiple] : block) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (std::abs(multiple) != 1) continue;
    if (!IsLinearEqualityConstraint(ct)) continue;
    if (ct.linear().vars().size() != common_var_coeff_map.size() + 1) continue;

    context_->UpdateRuleStats(
        "linear matrix: defining equation for common rectangle");
    definiting_equation = c;

    // Find the missing term and its coefficient.
    int64_t coeff = 0;
    const int num_terms = ct.linear().vars().size();
    for (int k = 0; k < num_terms; ++k) {
      if (common_var_coeff_map.contains(ct.linear().vars(k))) continue;
      new_var = ct.linear().vars(k);
      coeff = ct.linear().coeffs(k);
      break;
    }
    CHECK_NE(coeff, 0);

    // We have multiple * common + coeff * X = constant.
    // So common = multiple^-1 * constant - multiple^-1 * coeff * X;
    gcd = -multiple * coeff;
    offset = multiple * ct.linear().domain(0);
    break;
  }

  // We need a new variable and defining equation.
  if (definiting_equation == -1) {
    offset = 0;
    int64_t min_activity = 0;
    int64_t max_activity = 0;
    tmp_terms_.clear();
    std::vector<std::pair<int, int64_t>> common_part;
    for (const auto [var, coeff] : common_var_coeff_map) {
      common_part.push_back({var, coeff});
      gcd = std::gcd(gcd, std::abs(coeff));
      if (context_->CanBeUsedAsLiteral(var) && !context_->IsFixed(var)) {
        tmp_terms_.push_back({var, coeff});
        continue;
      }
      if (coeff > 0) {
        min_activity += coeff * context_->MinOf(var);
        max_activity += coeff * context_->MaxOf(var);
      } else {
        min_activity += coeff * context_->MaxOf(var);
        max_activity += coeff * context_->MinOf(var);
      }
    }

    // We isolated the Boolean in tmp_terms_, use the helper to get
    // more precise activity bounds. Note that while tmp_terms_ was built from
    // a hash map and is in an unspecified order, the Compute*Activity() helpers
    // will still return a deterministic result.
    if (!tmp_terms_.empty()) {
      min_activity += helper->ComputeMinActivity(tmp_terms_);
      max_activity += helper->ComputeMaxActivity(tmp_terms_);
    }

    if (gcd > 1) {
      min_activity /= gcd;
      max_activity /= gcd;
      for (int i = 0; i < common_part.size(); ++i) {
        common_part[i].second /= gcd;
      }
    }

    // Create new variable.
    std::sort(common_part.begin(), common_part.end());
    new_var = context_->NewIntVarWithDefinition(
        Domain(min_activity, max_activity), common_part);
    if (new_var == -1) return false;
  }

  // Replace in each constraint the common part by gcd * multiple * new_var !
  for (const auto [c, multiple] : block) {
    if (c == definiting_equation) continue;

    auto* mutable_linear =
        context_->working_model->mutable_constraints(c)->mutable_linear();
    const int num_terms = mutable_linear->vars().size();
    int new_size = 0;
    bool new_var_already_seen = false;
    for (int k = 0; k < num_terms; ++k) {
      if (common_var_coeff_map.contains(mutable_linear->vars(k))) {
        CHECK_EQ(common_var_coeff_map.at(mutable_linear->vars(k)) * multiple,
                 mutable_linear->coeffs(k));
        continue;
      }

      // Tricky: the new variable can already be present in this expression!
      int64_t new_coeff = mutable_linear->coeffs(k);
      if (mutable_linear->vars(k) == new_var) {
        new_var_already_seen = true;
        new_coeff += gcd * multiple;
        if (new_coeff == 0) continue;
      }

      mutable_linear->set_vars(new_size, mutable_linear->vars(k));
      mutable_linear->set_coeffs(new_size, new_coeff);
      ++new_size;
    }
    mutable_linear->mutable_vars()->Truncate(new_size);
    mutable_linear->mutable_coeffs()->Truncate(new_size);
    if (!new_var_already_seen) {
      mutable_linear->add_vars(new_var);
      mutable_linear->add_coeffs(gcd * multiple);
    }
    if (offset != 0) {
      FillDomainInProto(ReadDomainFromProto(*mutable_linear)
                            .AdditionWith(Domain(-offset * multiple)),
                        mutable_linear);
    }
    context_->UpdateConstraintVariableUsage(c);
  }
  return true;
}

namespace {

int64_t FindVarCoeff(int var, const ConstraintProto& ct) {
  const int num_terms = ct.linear().vars().size();
  for (int k = 0; k < num_terms; ++k) {
    if (ct.linear().vars(k) == var) return ct.linear().coeffs(k);
  }
  return 0;
}

int64_t ComputeNonZeroReduction(size_t block_size, size_t common_part_size) {
  // We replace the block by a column of new variable.
  // But we also need to define this new variable.
  return static_cast<int64_t>(block_size * (common_part_size - 1) -
                              common_part_size - 1);
}

}  // namespace

// The idea is to find a set of literal in AMO relationship that appear in
// many linear constraints. If this is the case, we can create a new variable to
// make an exactly one constraint, and replace it in the linear.
void CpModelPresolver::FindBigAtMostOneAndLinearOverlap(
    ActivityBoundHelper* helper) {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  int64_t num_blocks = 0;
  int64_t nz_reduction = 0;
  std::vector<int> amo_cts;
  std::vector<int> amo_literals;

  std::vector<int> common_part;
  std::vector<int> best_common_part;

  std::vector<bool> common_part_sign;
  std::vector<bool> best_common_part_sign;

  // We store for each var if the literal was positive or not.
  absl::flat_hash_map<int, bool> var_in_amo;

  for (int x = 0; x < context_->working_model->variables().size(); ++x) {
    // We pick a variable x that appear in some AMO.
    if (time_limit_->LimitReached()) break;
    if (timer.WorkLimitIsReached()) break;
    if (helper->NumAmoForVariable(x) == 0) continue;

    amo_cts.clear();
    timer.TrackSimpleLoop(context_->VarToConstraints(x).size());
    for (const int c : context_->VarToConstraints(x)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
        amo_cts.push_back(c);
      } else if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        amo_cts.push_back(c);
      }
    }
    if (amo_cts.empty()) continue;

    // Pick a random AMO containing x.
    //
    // TODO(user): better algo!
    //
    // Note that we don't care about the polarity, for each linear constraint,
    // if the coeff magnitude are the same, we will just have two values
    // controlled by whether the AMO (or EXO subset) is at one or zero.
    var_in_amo.clear();
    amo_literals.clear();
    common_part.clear();
    common_part_sign.clear();
    int base_ct_index;
    {
      // For determinism.
      std::sort(amo_cts.begin(), amo_cts.end());
      const int random_c =
          absl::Uniform<int>(*context_->random(), 0, amo_cts.size());
      base_ct_index = amo_cts[random_c];
      const ConstraintProto& ct =
          context_->working_model->constraints(base_ct_index);
      const auto& literals = ct.constraint_case() == ConstraintProto::kAtMostOne
                                 ? ct.at_most_one().literals()
                                 : ct.exactly_one().literals();
      timer.TrackSimpleLoop(5 * literals.size());  // hash insert are slow.
      for (const int literal : literals) {
        amo_literals.push_back(literal);
        common_part.push_back(PositiveRef(literal));
        common_part_sign.push_back(RefIsPositive(literal));
        const auto [_, inserted] =
            var_in_amo.insert({PositiveRef(literal), RefIsPositive(literal)});
        CHECK(inserted);
      }
    }

    const int64_t x_multiplier = var_in_amo.at(x) ? 1 : -1;

    // Collect linear constraints with at least two Boolean terms in var_in_amo
    // with the same coefficient than x.
    std::vector<int> block_cts;
    std::vector<int> linear_cts;
    int max_common_part = 0;
    timer.TrackSimpleLoop(context_->VarToConstraints(x).size());
    for (const int c : context_->VarToConstraints(x)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.constraint_case() != ConstraintProto::kLinear) continue;
      const int num_terms = ct.linear().vars().size();
      if (num_terms < 2) continue;

      timer.TrackSimpleLoop(2 * num_terms);
      const int64_t x_coeff = x_multiplier * FindVarCoeff(x, ct);
      if (x_coeff == 0) continue;  // could be in enforcement.

      int num_in_amo = 0;
      for (int k = 0; k < num_terms; ++k) {
        const int var = ct.linear().vars(k);
        if (!RefIsPositive(var)) {
          num_in_amo = 0;  // Abort.
          break;
        }
        const auto it = var_in_amo.find(var);
        if (it == var_in_amo.end()) continue;
        int64_t coeff = ct.linear().coeffs(k);
        if (!it->second) coeff = -coeff;
        if (coeff != x_coeff) continue;
        ++num_in_amo;
      }
      if (num_in_amo < 2) continue;

      max_common_part += num_in_amo;
      if (num_in_amo == common_part.size()) {
        // This is a perfect match!
        block_cts.push_back(c);
      } else {
        linear_cts.push_back(c);
      }
    }
    if (linear_cts.empty() && block_cts.empty()) continue;
    if (max_common_part < 100) continue;

    // Remember the best block encountered in the greedy algo below.
    // Note that we always start with the current perfect match.
    best_common_part = common_part;
    best_common_part_sign = common_part_sign;
    int best_block_size = block_cts.size();
    int best_saved_nz =
        ComputeNonZeroReduction(block_cts.size() + 1, common_part.size());

    // For determinism.
    std::sort(block_cts.begin(), block_cts.end());
    std::sort(linear_cts.begin(), linear_cts.end());

    // We will just greedily compute a big block with a random order.
    // TODO(user): We could sort by match with the full constraint instead.
    std::shuffle(linear_cts.begin(), linear_cts.end(), *context_->random());
    for (const int c : linear_cts) {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      const int num_terms = ct.linear().vars().size();
      timer.TrackSimpleLoop(2 * num_terms);
      const int64_t x_coeff = x_multiplier * FindVarCoeff(x, ct);
      CHECK_NE(x_coeff, 0);

      common_part.clear();
      common_part_sign.clear();
      for (int k = 0; k < num_terms; ++k) {
        const int var = ct.linear().vars(k);
        const auto it = var_in_amo.find(var);
        if (it == var_in_amo.end()) continue;
        int64_t coeff = ct.linear().coeffs(k);
        if (!it->second) coeff = -coeff;
        if (coeff != x_coeff) continue;
        common_part.push_back(var);
        common_part_sign.push_back(it->second);
      }
      if (common_part.size() < 2) continue;

      // Change var_in_amo;
      block_cts.push_back(c);
      if (common_part.size() < var_in_amo.size()) {
        var_in_amo.clear();
        for (int i = 0; i < common_part.size(); ++i) {
          var_in_amo[common_part[i]] = common_part_sign[i];
        }
      }

      // We have a block that can be replaced with a single new boolean +
      // defining exo constraint. Note that we can also replace in the base
      // constraint, hence the +1 to the block size.
      const int64_t saved_nz =
          ComputeNonZeroReduction(block_cts.size() + 1, common_part.size());
      if (saved_nz > best_saved_nz) {
        best_block_size = block_cts.size();
        best_saved_nz = saved_nz;
        best_common_part = common_part;
        best_common_part_sign = common_part_sign;
      }
    }
    if (best_saved_nz < 100) continue;

    // Use the best rectangle.
    // We start with the full match.
    // TODO(user): maybe we should always just use this if it is large enough?
    block_cts.resize(best_block_size);
    var_in_amo.clear();
    for (int i = 0; i < best_common_part.size(); ++i) {
      var_in_amo[best_common_part[i]] = best_common_part_sign[i];
    }

    ++num_blocks;
    nz_reduction += best_saved_nz;
    context_->UpdateRuleStats("linear matrix: common amo rectangle");

    // First filter the amo.
    int new_size = 0;
    for (const int lit : amo_literals) {
      if (!var_in_amo.contains(PositiveRef(lit))) continue;
      amo_literals[new_size++] = lit;
    }
    if (new_size == amo_literals.size()) {
      const ConstraintProto& ct =
          context_->working_model->constraints(base_ct_index);
      if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        context_->UpdateRuleStats("TODO linear matrix: constant rectangle!");
      } else {
        context_->UpdateRuleStats(
            "TODO linear matrix: reuse defining constraint");
      }
    } else if (new_size + 1 == amo_literals.size()) {
      const ConstraintProto& ct =
          context_->working_model->constraints(base_ct_index);
      if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        context_->UpdateRuleStats("TODO linear matrix: reuse exo constraint");
      }
    }
    amo_literals.resize(new_size);

    // Create a new literal that is one iff one of the literal in AMO is one.
    const int new_var = context_->NewBoolVarWithClause(amo_literals);
    {
      auto* new_exo =
          context_->working_model->add_constraints()->mutable_exactly_one();
      new_exo->mutable_literals()->Reserve(amo_literals.size() + 1);
      for (const int lit : amo_literals) {
        new_exo->add_literals(lit);
      }
      new_exo->add_literals(NegatedRef(new_var));
      context_->UpdateNewConstraintsVariableUsage();
    }

    // Filter the base amo/exo.
    {
      ConstraintProto* ct =
          context_->working_model->mutable_constraints(base_ct_index);
      auto* mutable_literals =
          ct->constraint_case() == ConstraintProto::kAtMostOne
              ? ct->mutable_at_most_one()->mutable_literals()
              : ct->mutable_exactly_one()->mutable_literals();
      int new_size = 0;
      for (const int lit : *mutable_literals) {
        if (var_in_amo.contains(PositiveRef(lit))) continue;
        (*mutable_literals)[new_size++] = lit;
      }
      (*mutable_literals)[new_size++] = new_var;
      mutable_literals->Truncate(new_size);
      context_->UpdateConstraintVariableUsage(base_ct_index);
    }

    // Use this Boolean in all the linear constraints.
    for (const int c : block_cts) {
      auto* mutable_linear =
          context_->working_model->mutable_constraints(c)->mutable_linear();

      // The removed expression will be (offset + coeff_x * new_bool).
      int64_t offset = 0;
      int64_t coeff_x = 0;

      int new_size = 0;
      const int num_terms = mutable_linear->vars().size();
      for (int k = 0; k < num_terms; ++k) {
        const int var = mutable_linear->vars(k);
        CHECK(RefIsPositive(var));
        int64_t coeff = mutable_linear->coeffs(k);
        const auto it = var_in_amo.find(var);
        if (it != var_in_amo.end()) {
          if (it->second) {
            // default is zero, amo at one means we add coeff.
          } else {
            // term is -coeff * (1 - var) + coeff.
            // default is coeff, amo at 1 means we remove coeff.
            offset += coeff;
            coeff = -coeff;
          }
          if (coeff_x == 0) coeff_x = coeff;
          CHECK_EQ(coeff, coeff_x);
          continue;
        }
        mutable_linear->set_vars(new_size, mutable_linear->vars(k));
        mutable_linear->set_coeffs(new_size, coeff);
        ++new_size;
      }

      // Add the new term.
      mutable_linear->set_vars(new_size, new_var);
      mutable_linear->set_coeffs(new_size, coeff_x);
      ++new_size;

      mutable_linear->mutable_vars()->Truncate(new_size);
      mutable_linear->mutable_coeffs()->Truncate(new_size);
      if (offset != 0) {
        FillDomainInProto(
            ReadDomainFromProto(*mutable_linear).AdditionWith(Domain(-offset)),
            mutable_linear);
      }
      context_->UpdateConstraintVariableUsage(c);
    }
  }

  timer.AddCounter("blocks", num_blocks);
  timer.AddCounter("saved_nz", nz_reduction);
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
}

// This helps on neos-5045105-creuse.pb.gz for instance.
void CpModelPresolver::FindBigVerticalLinearOverlap(
    ActivityBoundHelper* helper) {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  int64_t num_blocks = 0;
  int64_t nz_reduction = 0;
  absl::flat_hash_map<int, int64_t> coeff_map;
  for (int x = 0; x < context_->working_model->variables().size(); ++x) {
    if (timer.WorkLimitIsReached()) break;

    bool in_enforcement = false;
    std::vector<int> linear_cts;
    timer.TrackSimpleLoop(context_->VarToConstraints(x).size());
    for (const int c : context_->VarToConstraints(x)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.constraint_case() != ConstraintProto::kLinear) continue;

      const int num_terms = ct.linear().vars().size();
      if (num_terms < 2) continue;
      bool is_canonical = true;
      timer.TrackSimpleLoop(num_terms);
      for (int k = 0; k < num_terms; ++k) {
        if (!RefIsPositive(ct.linear().vars(k))) {
          is_canonical = false;
          break;
        }
      }
      if (!is_canonical) continue;

      // We don't care about enforcement literal, but we don't want x inside.
      timer.TrackSimpleLoop(ct.enforcement_literal().size());
      for (const int lit : ct.enforcement_literal()) {
        if (PositiveRef(lit) == x) {
          in_enforcement = true;
          break;
        }
      }

      // Note(user): We will actually abort right away in this case, but we
      // want work_done to be deterministic! so we do the work anyway.
      if (in_enforcement) continue;
      linear_cts.push_back(c);
    }

    // If a Boolean is used in enforcement, we prefer not to combine it with
    // others. TODO(user): more generally ignore Boolean or only replace if
    // there is a big non-zero improvement.
    if (in_enforcement) continue;
    if (linear_cts.size() < 10) continue;

    // For determinism.
    std::sort(linear_cts.begin(), linear_cts.end());
    std::shuffle(linear_cts.begin(), linear_cts.end(), *context_->random());

    // Now it is almost the same algo as for FindBigHorizontalLinearOverlap().
    // We greedely compute a "common" rectangle using the first constraint
    // as a "base" one. Note that if a aX + bY appear in the majority of
    // constraint, we have a good chance to find this block since we start by
    // a random constraint.
    coeff_map.clear();

    std::vector<std::pair<int, int64_t>> block;
    std::vector<std::pair<int, int64_t>> common_part;
    for (const int c : linear_cts) {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      const int num_terms = ct.linear().vars().size();
      timer.TrackSimpleLoop(num_terms);

      // Compute the coeff of x.
      const int64_t x_coeff = FindVarCoeff(x, ct);
      if (x_coeff == 0) continue;

      if (block.empty()) {
        // This is our base constraint.
        coeff_map.clear();
        for (int k = 0; k < num_terms; ++k) {
          coeff_map[ct.linear().vars(k)] = ct.linear().coeffs(k);
        }
        if (coeff_map.size() < 2) continue;
        block.push_back({c, x_coeff});
        continue;
      }

      // We are looking for a common divisor of coeff_map and this constraint.
      const int64_t gcd =
          std::gcd(std::abs(coeff_map.at(x)), std::abs(x_coeff));
      const int64_t multiple_base = coeff_map.at(x) / gcd;
      const int64_t multiple_ct = x_coeff / gcd;
      common_part.clear();
      for (int k = 0; k < num_terms; ++k) {
        const int64_t coeff = ct.linear().coeffs(k);
        if (coeff % multiple_ct != 0) continue;

        const auto it = coeff_map.find(ct.linear().vars(k));
        if (it == coeff_map.end()) continue;
        if (it->second % multiple_base != 0) continue;
        if (it->second / multiple_base != coeff / multiple_ct) continue;

        common_part.push_back({ct.linear().vars(k), coeff / multiple_ct});
      }

      // Skip bad constraint.
      if (common_part.size() < 2) continue;

      // Update coeff_map.
      block.push_back({c, x_coeff});
      coeff_map.clear();
      for (const auto [var, coeff] : common_part) {
        coeff_map[var] = coeff;
      }
    }

    // We have a candidate.
    const int64_t saved_nz =
        ComputeNonZeroReduction(block.size(), coeff_map.size());
    if (saved_nz < 30) continue;

    // Fix multiples, currently this contain the coeff of x for each constraint.
    const int64_t base_x = coeff_map.at(x);
    for (auto& [c, multipier] : block) {
      CHECK_EQ(multipier % base_x, 0);
      multipier /= base_x;
    }

    // Introduce new_var = coeff_map and perform the substitution.
    if (!RemoveCommonPart(coeff_map, block, helper)) continue;
    ++num_blocks;
    nz_reduction += saved_nz;
    context_->UpdateRuleStats("linear matrix: common vertical rectangle");
  }

  timer.AddCounter("blocks", num_blocks);
  timer.AddCounter("saved_nz", nz_reduction);
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
}

// Note that internally, we already split long linear into smaller chunk, so
// it should be beneficial to identify common part between many linear
// constraint.
//
// Note(user): This was made to work on var-smallemery-m6j6.pb.gz, but applies
// to quite a few miplib problem. Try to improve the heuristics and algorithm to
// be faster and detect larger block.
void CpModelPresolver::FindBigHorizontalLinearOverlap(
    ActivityBoundHelper* helper) {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  const int num_constraints = context_->working_model->constraints_size();
  std::vector<std::pair<int, int>> to_sort;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    const int size = ct.linear().vars().size();
    if (size < 5) continue;
    to_sort.push_back({-size, c});
  }
  std::sort(to_sort.begin(), to_sort.end());

  std::vector<int> sorted_linear;
  for (int i = 0; i < to_sort.size(); ++i) {
    sorted_linear.push_back(to_sort[i].second);
  }

  // On large problem, using and hash_map can be slow, so we use the vector
  // version and for now fill the map only when doing the change.
  std::vector<int> var_to_coeff_non_zeros;
  std::vector<int64_t> var_to_coeff(context_->working_model->variables_size(),
                                    0);

  int64_t num_blocks = 0;
  int64_t nz_reduction = 0;
  for (int i = 0; i < sorted_linear.size(); ++i) {
    const int c = sorted_linear[i];
    if (c < 0) continue;
    if (timer.WorkLimitIsReached()) break;

    for (const int var : var_to_coeff_non_zeros) {
      var_to_coeff[var] = 0;
    }
    var_to_coeff_non_zeros.clear();
    {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      const int num_terms = ct.linear().vars().size();
      timer.TrackSimpleLoop(num_terms);
      for (int k = 0; k < num_terms; ++k) {
        const int var = ct.linear().vars(k);
        var_to_coeff[var] = ct.linear().coeffs(k);
        var_to_coeff_non_zeros.push_back(var);
      }
    }

    // Look for an initial overlap big enough.
    //
    // Note that because we construct it incrementally, we need the first two
    // constraint to have an overlap of at least half this.
    int saved_nz = 100;
    std::vector<int> used_sorted_linear = {i};
    std::vector<std::pair<int, int64_t>> block = {{c, 1}};
    std::vector<std::pair<int, int64_t>> common_part;
    std::vector<std::pair<int, int>> old_matches;

    for (int j = 0; j < sorted_linear.size(); ++j) {
      if (i == j) continue;
      const int other_c = sorted_linear[j];
      if (other_c < 0) continue;
      const ConstraintProto& ct = context_->working_model->constraints(other_c);

      // No need to continue if linear is not large enough.
      const int num_terms = ct.linear().vars().size();
      const int best_saved_nz =
          ComputeNonZeroReduction(block.size() + 1, num_terms);
      if (best_saved_nz <= saved_nz) break;

      // This is the hot loop here.
      timer.TrackSimpleLoop(num_terms);
      common_part.clear();
      for (int k = 0; k < num_terms; ++k) {
        const int var = ct.linear().vars(k);
        if (var_to_coeff[var] == ct.linear().coeffs(k)) {
          common_part.push_back({var, ct.linear().coeffs(k)});
        }
      }

      // We replace (new_block_size) * (common_size) by
      // 1/ and equation of size common_size + 1
      // 2/ new_block_size variable
      // So new_block_size * common_size - common_size - 1 - new_block_size
      // which is (new_block_size - 1) * (common_size - 1) - 2;
      const int64_t new_saved_nz =
          ComputeNonZeroReduction(block.size() + 1, common_part.size());
      if (new_saved_nz > saved_nz) {
        saved_nz = new_saved_nz;
        used_sorted_linear.push_back(j);
        block.push_back({other_c, 1});

        // Rebuild the map.
        // TODO(user): We could only clear the non-common part.
        for (const int var : var_to_coeff_non_zeros) {
          var_to_coeff[var] = 0;
        }
        var_to_coeff_non_zeros.clear();
        for (const auto [var, coeff] : common_part) {
          var_to_coeff[var] = coeff;
          var_to_coeff_non_zeros.push_back(var);
        }
      } else {
        if (common_part.size() > 1) {
          old_matches.push_back({j, common_part.size()});
        }
      }
    }

    // Introduce a new variable = common_part.
    // Use it in all linear constraint.
    if (block.size() > 1) {
      // Try to extend with exact matches that were skipped.
      const int match_size = var_to_coeff_non_zeros.size();
      for (const auto [index, old_match_size] : old_matches) {
        if (old_match_size < match_size) continue;

        int new_match_size = 0;
        const int other_c = sorted_linear[index];
        const ConstraintProto& ct =
            context_->working_model->constraints(other_c);
        const int num_terms = ct.linear().vars().size();
        for (int k = 0; k < num_terms; ++k) {
          if (var_to_coeff[ct.linear().vars(k)] == ct.linear().coeffs(k)) {
            ++new_match_size;
          }
        }
        if (new_match_size == match_size) {
          context_->UpdateRuleStats(
              "linear matrix: common horizontal rectangle extension");
          used_sorted_linear.push_back(index);
          block.push_back({other_c, 1});
        }
      }

      // TODO(user): avoid creating the map? this is not visible in profile
      // though since we only do it when a reduction is performed.
      absl::flat_hash_map<int, int64_t> coeff_map;
      for (const int var : var_to_coeff_non_zeros) {
        coeff_map[var] = var_to_coeff[var];
      }
      if (!RemoveCommonPart(coeff_map, block, helper)) continue;

      ++num_blocks;
      nz_reduction += ComputeNonZeroReduction(block.size(), coeff_map.size());
      context_->UpdateRuleStats("linear matrix: common horizontal rectangle");
      for (const int i : used_sorted_linear) sorted_linear[i] = -1;
    }
  }

  timer.AddCounter("blocks", num_blocks);
  timer.AddCounter("saved_nz", nz_reduction);
  timer.AddCounter("linears", sorted_linear.size());
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
}

// Find two linear constraints of the form:
// - term1 + identical_terms = rhs1
// - term2 + identical_terms = rhs2
// This allows to infer an affine relation, and remove one constraint and one
// variable.
void CpModelPresolver::FindAlmostIdenticalLinearConstraints() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;

  // Work tracking is required, since in the worst case (n identical
  // constraints), we are in O(n^3). In practice we are way faster though. And
  // identical constraints should have already be removed when we call this.
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // Only keep non-enforced linear equality of size > 2. Sort by size.
  std::vector<std::pair<int, int>> to_sort;
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (!IsLinearEqualityConstraint(ct)) continue;
    if (ct.linear().vars().size() <= 2) continue;

    // Our canonicalization should sort constraints, we skip non-canonical ones.
    if (!std::is_sorted(ct.linear().vars().begin(), ct.linear().vars().end())) {
      continue;
    }

    to_sort.push_back({ct.linear().vars().size(), c});
  }
  std::sort(to_sort.begin(), to_sort.end());

  // One watcher data structure.
  // This is similar to what is used by the inclusion detector.
  std::vector<int> var_to_clear;
  std::vector<std::vector<std::pair<int, int64_t>>> var_to_ct_coeffs_;
  const int num_variables = context_->working_model->variables_size();
  var_to_ct_coeffs_.resize(num_variables);

  int end;
  int num_tested_pairs = 0;
  int num_affine_relations = 0;
  for (int start = 0; start < to_sort.size(); start = end) {
    // Split by identical size.
    end = start + 1;
    const int length = to_sort[start].first;
    for (; end < to_sort.size(); ++end) {
      if (to_sort[end].first != length) break;
    }
    const int span_size = end - start;
    if (span_size == 1) continue;

    // Watch one term of each constraint randomly.
    for (const int var : var_to_clear) var_to_ct_coeffs_[var].clear();
    var_to_clear.clear();
    for (int i = start; i < end; ++i) {
      const int c = to_sort[i].second;
      const LinearConstraintProto& lin =
          context_->working_model->constraints(c).linear();
      const int index =
          absl::Uniform<int>(*context_->random(), 0, lin.vars().size());
      const int var = lin.vars(index);
      if (var_to_ct_coeffs_[var].empty()) var_to_clear.push_back(var);
      var_to_ct_coeffs_[var].push_back({c, lin.coeffs(index)});
    }

    // For each constraint, try other constraints that have at least one term in
    // common with the same coeff. Note that for two constraint of size 3, we
    // will miss a working pair only if we both watch the variable that is
    // different. So only with a probability (1/3)^2. Since we call this more
    // than once per presolve, we should be mostly good. For larger constraint,
    // we shouldn't miss much.
    for (int i1 = start; i1 < end; ++i1) {
      if (timer.WorkLimitIsReached()) break;
      const int c1 = to_sort[i1].second;
      const LinearConstraintProto& lin1 =
          context_->working_model->constraints(c1).linear();
      bool skip = false;
      for (int i = 0; !skip && i < lin1.vars().size(); ++i) {
        for (const auto [c2, coeff2] : var_to_ct_coeffs_[lin1.vars(i)]) {
          if (c2 == c1) continue;

          // TODO(user): we could easily deal with * -1 or other multiples.
          if (coeff2 != lin1.coeffs(i)) continue;
          if (timer.WorkLimitIsReached()) break;

          // Skip if we processed this earlier and deleted it.
          const ConstraintProto& ct2 = context_->working_model->constraints(c2);
          if (ct2.constraint_case() != ConstraintProto::kLinear) continue;
          const LinearConstraintProto& lin2 =
              context_->working_model->constraints(c2).linear();
          if (lin2.vars().size() != length) continue;

          // TODO(user): In practice LinearsDifferAtOneTerm() will abort
          // early if the constraints differ early, so we are even faster than
          // this.
          timer.TrackSimpleLoop(length);

          ++num_tested_pairs;
          if (LinearsDifferAtOneTerm(lin1, lin2)) {
            // The two equalities only differ at one term !
            // do c1 -= c2 and presolve c1 right away.
            // We should detect new affine relation and remove it.
            auto* to_modify = context_->working_model->mutable_constraints(c1);
            if (!AddLinearConstraintMultiple(
                    -1, context_->working_model->constraints(c2), to_modify)) {
              continue;
            }

            // Affine will be of size 2, but we might also have the same
            // variable with different coeff in both constraint, in which case
            // the linear will be of size 1.
            DCHECK_LE(to_modify->linear().vars().size(), 2);

            ++num_affine_relations;
            context_->UpdateRuleStats(
                "linear: advanced affine relation from 2 constraints.");

            // We should stop processing c1 since it should be empty afterward.
            DivideLinearByGcd(to_modify);
            PresolveSmallLinear(to_modify);
            context_->UpdateConstraintVariableUsage(c1);
            skip = true;
            break;
          }
        }
      }
    }
  }

  timer.AddCounter("num_tested_pairs", num_tested_pairs);
  timer.AddCounter("found", num_affine_relations);
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
}

void CpModelPresolver::ExtractEncodingFromLinear() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  if (context_->params().presolve_inclusion_work_limit() == 0) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // TODO(user): compute on the fly instead of temporary storing variables?
  std::vector<int> relevant_constraints;
  CompactVectorVector<int> storage;
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(context_->params().presolve_inclusion_work_limit());

  // Loop over the constraints and fill the structures above.
  //
  // TODO(user): Ideally we want to process exactly_one first in case a
  // linear constraint is both included in an at_most_one and an exactly_one.
  std::vector<int> vars;
  const int num_constraints = context_->working_model->constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->working_model->constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kAtMostOne: {
        vars.clear();
        for (const int ref : ct.at_most_one().literals()) {
          vars.push_back(PositiveRef(ref));
        }
        relevant_constraints.push_back(c);
        detector.AddPotentialSuperset(storage.Add(vars));
        break;
      }
      case ConstraintProto::kExactlyOne: {
        vars.clear();
        for (const int ref : ct.exactly_one().literals()) {
          vars.push_back(PositiveRef(ref));
        }
        relevant_constraints.push_back(c);
        detector.AddPotentialSuperset(storage.Add(vars));
        break;
      }
      case ConstraintProto::kLinear: {
        // We only consider equality with no enforcement.
        if (!IsLinearEqualityConstraint(ct)) continue;

        // We also want a single non-Boolean.
        // Note that this assume the constraint is canonicalized.
        bool is_candidate = true;
        int num_integers = 0;
        vars.clear();
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
          relevant_constraints.push_back(c);
          detector.AddPotentialSubset(storage.Add(vars));
        }
        break;
      }
      default:
        break;
    }
  }

  // Stats.
  int64_t num_exactly_one_encodings = 0;
  int64_t num_at_most_one_encodings = 0;
  int64_t num_literals = 0;
  int64_t num_unique_terms = 0;
  int64_t num_multiple_terms = 0;

  detector.DetectInclusions([&](int subset, int superset) {
    const int subset_c = relevant_constraints[subset];
    const int superset_c = relevant_constraints[superset];
    const ConstraintProto& superset_ct =
        context_->working_model->constraints(superset_c);
    if (superset_ct.constraint_case() == ConstraintProto::kAtMostOne) {
      ++num_at_most_one_encodings;
    } else {
      ++num_exactly_one_encodings;
    }
    num_literals += storage[subset].size();
    context_->UpdateRuleStats("encoding: extracted from linear");

    if (!ProcessEncodingFromLinear(subset_c, superset_ct, &num_unique_terms,
                                   &num_multiple_terms)) {
      detector.Stop();  // UNSAT.
    }

    detector.StopProcessingCurrentSubset();
  });

  timer.AddCounter("potential_supersets", detector.num_potential_supersets());
  timer.AddCounter("potential_subsets", detector.num_potential_subsets());
  timer.AddCounter("amo_encodings", num_at_most_one_encodings);
  timer.AddCounter("exo_encodings", num_exactly_one_encodings);
  timer.AddCounter("unique_terms", num_unique_terms);
  timer.AddCounter("multiple_terms", num_multiple_terms);
  timer.AddCounter("literals", num_literals);
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
void CpModelPresolver::LookAtVariableWithDegreeTwo(int var) {
  CHECK(RefIsPositive(var));
  CHECK(context_->ConstraintVariableGraphIsUpToDate());
  if (context_->ModelIsUnsat()) return;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
  if (context_->IsFixed(var)) return;
  if (!context_->ModelIsExpanded()) return;
  if (!context_->CanBeUsedAsLiteral(var)) return;

  // TODO(user): If var is in objective, we might be able to tighten domains.
  // ex: enf => x \in [0, 1]
  //     not(enf) => x \in [1, 2]
  // The x can be removed from one place. Maybe just do <=> not in [0,1] with
  // dual code?
  if (context_->VarToConstraints(var).size() != 2) return;

  bool abort = false;
  int ct_var = -1;
  Domain union_of_domain;
  int num_positive = 0;
  std::vector<int> constraint_indices_to_remove;
  for (const int c : context_->VarToConstraints(var)) {
    if (c < 0) {
      abort = true;
      break;
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
  if (abort) return;
  if (num_positive != 1) return;
  if (!context_->IntersectDomainWith(ct_var, union_of_domain)) return;

  context_->UpdateRuleStats("variables: removable enforcement literal");
  absl::c_sort(constraint_indices_to_remove);  // For determinism
  for (const int c : constraint_indices_to_remove) {
    context_->NewMappingConstraint(context_->working_model->constraints(c),
                                   __FILE__, __LINE__);
    context_->working_model->mutable_constraints(c)->Clear();
    context_->UpdateConstraintVariableUsage(c);
  }
  context_->MarkVariableAsRemoved(var);
}

namespace {

absl::Span<const int> AtMostOneOrExactlyOneLiterals(const ConstraintProto& ct) {
  if (ct.constraint_case() == ConstraintProto::kAtMostOne) {
    return {ct.at_most_one().literals()};
  } else {
    return {ct.exactly_one().literals()};
  }
}

}  // namespace

void CpModelPresolver::ProcessVariableInTwoAtMostOrExactlyOne(int var) {
  DCHECK(RefIsPositive(var));
  DCHECK(context_->ConstraintVariableGraphIsUpToDate());
  if (context_->ModelIsUnsat()) return;
  if (context_->IsFixed(var)) return;
  if (context_->VariableWasRemoved(var)) return;
  if (!context_->ModelIsExpanded()) return;
  if (!context_->CanBeUsedAsLiteral(var)) return;

  int64_t cost = 0;
  if (context_->VarToConstraints(var).contains(kObjectiveConstraint)) {
    if (context_->VarToConstraints(var).size() != 3) return;
    cost = context_->ObjectiveMap().at(var);
  } else {
    if (context_->VarToConstraints(var).size() != 2) return;
  }

  // We have a variable with a cost (or without) that appear in two constraints.
  // We want two at_most_one or exactly_one.
  // TODO(user): Also deal with bool_and.
  int c1 = -1;
  int c2 = -1;
  for (const int c : context_->VarToConstraints(var)) {
    if (c < 0) continue;
    const ConstraintProto& ct = context_->working_model->constraints(c);
    if (ct.constraint_case() != ConstraintProto::kAtMostOne &&
        ct.constraint_case() != ConstraintProto::kExactlyOne) {
      return;
    }
    if (c1 == -1) {
      c1 = c;
    } else {
      c2 = c;
    }
  }

  // This can happen for variable in a kAffineRelationConstraint.
  if (c1 == -1 || c2 == -1) return;

  // Tricky: We iterate on a map above, so the order is non-deterministic, we
  // do not want that, so we re-order the constraints.
  if (c1 > c2) std::swap(c1, c2);

  // We can always sum the two constraints.
  // If var appear in one and not(var) in the other, the two term cancel out to
  // one, so we still have an <= 1 (or eventually a ==1 (see below).
  //
  // Note that if the constraint are of size one, they can just be preprocessed
  // individually and just be removed. So we abort here as the code below
  // is incorrect if new_ct is an empty constraint.
  context_->tmp_literals.clear();
  int c1_ref = std::numeric_limits<int>::min();
  const ConstraintProto& ct1 = context_->working_model->constraints(c1);
  if (AtMostOneOrExactlyOneLiterals(ct1).size() <= 1) return;
  for (const int lit : AtMostOneOrExactlyOneLiterals(ct1)) {
    if (PositiveRef(lit) == var) {
      c1_ref = lit;
    } else {
      context_->tmp_literals.push_back(lit);
    }
  }
  int c2_ref = std::numeric_limits<int>::min();
  const ConstraintProto& ct2 = context_->working_model->constraints(c2);
  if (AtMostOneOrExactlyOneLiterals(ct2).size() <= 1) return;
  for (const int lit : AtMostOneOrExactlyOneLiterals(ct2)) {
    if (PositiveRef(lit) == var) {
      c2_ref = lit;
    } else {
      context_->tmp_literals.push_back(lit);
    }
  }
  DCHECK_NE(c1_ref, std::numeric_limits<int>::min());
  DCHECK_NE(c2_ref, std::numeric_limits<int>::min());
  if (c1_ref != NegatedRef(c2_ref)) return;

  // If the cost is non-zero, we can use an exactly one to make it zero.
  // Use that exactly one in the postsolve to recover the value of var.
  int64_t cost_shift = 0;
  absl::Span<const int> literals;
  if (ct1.constraint_case() == ConstraintProto::kExactlyOne) {
    cost_shift = RefIsPositive(c1_ref) ? cost : -cost;
    literals = ct1.exactly_one().literals();
  } else if (ct2.constraint_case() == ConstraintProto::kExactlyOne) {
    cost_shift = RefIsPositive(c2_ref) ? cost : -cost;
    literals = ct2.exactly_one().literals();
  } else {
    // Dual argument. The one with a negative cost can be transformed to
    // an exactly one.
    // Tricky: if there is a cost, we don't want the objective to be
    // constraining to be able to do that.
    if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
    if (context_->params().keep_symmetry_in_presolve()) return;
    if (cost != 0 && context_->ObjectiveDomainIsConstraining()) return;

    if (RefIsPositive(c1_ref) == (cost < 0)) {
      cost_shift = RefIsPositive(c1_ref) ? cost : -cost;
      literals = ct1.at_most_one().literals();
    } else {
      cost_shift = RefIsPositive(c2_ref) ? cost : -cost;
      literals = ct2.at_most_one().literals();
    }
  }

  if (!context_->ShiftCostInExactlyOne(literals, cost_shift)) return;
  DCHECK(!context_->ObjectiveMap().contains(var));
  context_->NewMappingConstraint(__FILE__, __LINE__)
      ->mutable_exactly_one()
      ->mutable_literals()
      ->Assign(literals.begin(), literals.end());

  // We can now replace the two constraint by a single one, and delete var!
  const int new_ct_index = context_->working_model->constraints().size();
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (ct1.constraint_case() == ConstraintProto::kExactlyOne &&
      ct2.constraint_case() == ConstraintProto::kExactlyOne) {
    for (const int lit : context_->tmp_literals) {
      new_ct->mutable_exactly_one()->add_literals(lit);
    }
  } else {
    // At most one here is enough: if all zero, we can satisfy one of the
    // two exactly one at postsolve.
    for (const int lit : context_->tmp_literals) {
      new_ct->mutable_at_most_one()->add_literals(lit);
    }
  }

  context_->UpdateNewConstraintsVariableUsage();
  context_->working_model->mutable_constraints(c1)->Clear();
  context_->UpdateConstraintVariableUsage(c1);
  context_->working_model->mutable_constraints(c2)->Clear();
  context_->UpdateConstraintVariableUsage(c2);

  context_->UpdateRuleStats(
      "at_most_one: resolved two constraints with opposite literal");
  context_->MarkVariableAsRemoved(var);

  // TODO(user): If the merged list contains duplicates or literal that are
  // negation of other, we need to deal with that right away. For some reason
  // something is not robust to that it seems. Investigate & fix!
  DCHECK_NE(new_ct->constraint_case(), ConstraintProto::CONSTRAINT_NOT_SET);
  if (PresolveAtMostOrExactlyOne(new_ct)) {
    context_->UpdateConstraintVariableUsage(new_ct_index);
  }
}

// If we have a bunch of constraint of the form literal => Y \in domain and
// another constraint Y = f(X), we can remove Y, that constraint, and transform
// all linear1 from constraining Y to constraining X.
//
// We can for instance do it for Y = abs(X) or Y = X^2 easily. More complex
// function might be trickier.
//
// Note that we can't always do it in the reverse direction though!
// If we have l => X = -1, we can't transfer that to abs(X) for instance, since
// X=1 will also map to abs(-1). We can only do it if for all implied domain D
// we have f^-1(f(D)) = D, which is not easy to check.
void CpModelPresolver::MaybeTransferLinear1ToAnotherVariable(int var) {
  // Find the extra constraint and do basic CHECKs.
  int other_c;
  int num_others = 0;
  std::vector<int> to_rewrite;
  for (const int c : context_->VarToConstraints(var)) {
    if (c >= 0) {
      const ConstraintProto& ct = context_->working_model->constraints(c);
      if (ct.constraint_case() == ConstraintProto::kLinear &&
          ct.linear().vars().size() == 1) {
        to_rewrite.push_back(c);
        continue;
      }
    }
    ++num_others;
    other_c = c;
  }
  if (num_others != 1) return;
  if (other_c < 0) return;

  // In general constraint with more than two variable can't be removed.
  // Similarly for linear2 with non-fixed rhs as we would need to check the form
  // of all implied domain.
  const auto& other_ct = context_->working_model->constraints(other_c);
  if (context_->ConstraintToVars(other_c).size() != 2 ||
      !other_ct.enforcement_literal().empty() ||
      other_ct.constraint_case() == ConstraintProto::kLinear) {
    return;
  }

  // This will be the rewriting function. It takes the implied domain of var
  // from linear1, and return a pair {new_var, new_var_implied_domain}.
  std::function<std::pair<int, Domain>(const Domain& implied)> transfer_f =
      nullptr;

  // We only support a few cases.
  //
  // TODO(user): implement more! Note that the linear2 case was tempting, but if
  // we don't have an equality, we can't transfer, and if we do, we actually
  // have affine equivalence already.
  if (other_ct.constraint_case() == ConstraintProto::kLinMax &&
      other_ct.lin_max().target().vars().size() == 1 &&
      other_ct.lin_max().target().vars(0) == var &&
      std::abs(other_ct.lin_max().target().coeffs(0)) == 1 &&
      IsAffineIntAbs(other_ct)) {
    context_->UpdateRuleStats("linear1: transferred from abs(X) to X");
    const LinearExpressionProto& target = other_ct.lin_max().target();
    const LinearExpressionProto& expr = other_ct.lin_max().exprs(0);
    transfer_f = [target = target, expr = expr](const Domain& implied) {
      Domain target_domain =
          implied.ContinuousMultiplicationBy(target.coeffs(0))
              .AdditionWith(Domain(target.offset()));
      target_domain =
          target_domain.IntersectionWith(Domain(0, target_domain.Max()));

      // We have target = abs(expr).
      const Domain expr_domain =
          target_domain.UnionWith(target_domain.Negation());
      const Domain new_domain = expr_domain.AdditionWith(Domain(-expr.offset()))
                                    .InverseMultiplicationBy(expr.coeffs(0));
      return std::make_pair(expr.vars(0), new_domain);
    };
  }

  if (transfer_f == nullptr) {
    context_->UpdateRuleStats(
        "TODO linear1: appear in only one extra 2-var constraint");
    return;
  }

  // Applies transfer_f to all linear1.
  std::sort(to_rewrite.begin(), to_rewrite.end());
  const Domain var_domain = context_->DomainOf(var);
  for (const int c : to_rewrite) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->linear().vars(0) != var || ct->linear().coeffs(0) != 1) {
      // This shouldn't happen.
      LOG(INFO) << "Aborted in MaybeTransferLinear1ToAnotherVariable()";
      return;
    }

    const Domain implied =
        var_domain.IntersectionWith(ReadDomainFromProto(ct->linear()));
    auto [new_var, new_domain] = transfer_f(implied);
    const Domain current = context_->DomainOf(new_var);
    new_domain = new_domain.IntersectionWith(current);
    if (new_domain.IsEmpty()) {
      if (!MarkConstraintAsFalse(ct)) return;
    } else if (new_domain == current) {
      ct->Clear();
    } else {
      ct->mutable_linear()->set_vars(0, new_var);
      FillDomainInProto(new_domain, ct->mutable_linear());
    }
    context_->UpdateConstraintVariableUsage(c);
  }

  // Copy other_ct to the mapping model and delete var!
  context_->NewMappingConstraint(other_ct, __FILE__, __LINE__);
  context_->working_model->mutable_constraints(other_c)->Clear();
  context_->UpdateConstraintVariableUsage(other_c);
  context_->MarkVariableAsRemoved(var);
}

// TODO(user): We can still remove the variable even if we want to keep
// all feasible solutions for the cases when we have a full encoding.
// Similarly this shouldn't break symmetry, but we do need to do it for all
// symmetric variable at once.
//
// TODO(user): In fixed search, we disable this rule because we don't update
// the search strategy, but for some strategy we could.
//
// TODO(user): The hint might get lost if the encoding was created during
// the presolve.
void CpModelPresolver::ProcessVariableOnlyUsedInEncoding(int var) {
  if (context_->ModelIsUnsat()) return;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
  if (context_->params().keep_symmetry_in_presolve()) return;
  if (context_->IsFixed(var)) return;
  if (context_->VariableWasRemoved(var)) return;
  if (context_->CanBeUsedAsLiteral(var)) return;
  if (context_->params().search_branching() == SatParameters::FIXED_SEARCH) {
    return;
  }

  if (!context_->VariableIsOnlyUsedInEncodingAndMaybeInObjective(var)) {
    if (context_->VariableIsOnlyUsedInLinear1AndOneExtraConstraint(var)) {
      MaybeTransferLinear1ToAnotherVariable(var);
      return;
    }
    return;
  }

  // If a variable var only appear in enf => var \in domain and in the
  // objective, we can remove its costs and the variable/constraint by
  // transferring part of the cost to the enforcement.
  //
  // More generally, we can reduce the domain to just two values. Later this
  // will be replaced by a Boolean, and the equivalence to the enforcement
  // literal will be added if it is unique.
  //
  // TODO(user): maybe we should do more here rather than delaying some
  // reduction. But then it is more code.
  if (context_->VariableWithCostIsUniqueAndRemovable(var)) {
    int unique_c = -1;
    for (const int c : context_->VarToConstraints(var)) {
      if (c < 0) continue;
      CHECK_EQ(unique_c, -1);
      unique_c = c;
    }
    CHECK_NE(unique_c, -1);
    const ConstraintProto& ct = context_->working_model->constraints(unique_c);
    const int64_t cost = context_->ObjectiveCoeff(var);
    if (ct.linear().vars(0) == var) {
      const Domain implied = ReadDomainFromProto(ct.linear())
                                 .InverseMultiplicationBy(ct.linear().coeffs(0))
                                 .IntersectionWith(context_->DomainOf(var));
      if (implied.IsEmpty()) {
        if (!MarkConstraintAsFalse(
                context_->working_model->mutable_constraints(unique_c))) {
          return;
        }
        context_->UpdateConstraintVariableUsage(unique_c);
        return;
      }

      int64_t value1, value2;
      if (cost == 0) {
        context_->UpdateRuleStats("variables: fix singleton var in linear1");
        return (void)context_->IntersectDomainWith(var, Domain(implied.Min()));
      } else if (cost > 0) {
        value1 = context_->MinOf(var);
        value2 = implied.Min();
      } else {
        value1 = context_->MaxOf(var);
        value2 = implied.Max();
      }

      // Nothing else to do in this case, the constraint will be reduced to
      // a pure Boolean constraint later.
      context_->UpdateRuleStats("variables: reduced domain to two values");
      return (void)context_->IntersectDomainWith(
          var, Domain::FromValues({value1, value2}));
    }
  }

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
    const int var = PositiveRef(ct.linear().vars(0));
    const Domain var_domain = context_->DomainOf(var);
    const Domain rhs = ReadDomainFromProto(ct.linear())
                           .InverseMultiplicationBy(coeff)
                           .IntersectionWith(var_domain);
    if (rhs.IsEmpty()) {
      if (!context_->SetLiteralToFalse(ct.enforcement_literal(0))) {
        return;
      }
      return;
    } else if (rhs.IsFixed()) {
      if (!var_domain.Contains(rhs.FixedValue())) {
        if (!context_->SetLiteralToFalse(ct.enforcement_literal(0))) {
          return;
        }
      } else {
        values_set.insert(rhs.FixedValue());
        value_to_equal_literals[rhs.FixedValue()].push_back(
            ct.enforcement_literal(0));
      }
    } else {
      const Domain complement = var_domain.IntersectionWith(rhs.Complement());
      if (complement.IsEmpty()) {
        // TODO(user): This should be dealt with elsewhere.
        abort = true;
        break;
      }
      if (complement.IsFixed()) {
        if (var_domain.Contains(complement.FixedValue())) {
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
    return;
  } else if (value_to_not_equal_literals.empty() &&
             value_to_equal_literals.empty()) {
    // This is just a variable not used anywhere, it should be removed by
    // another part of the presolve.
    return;
  }

  // For determinism, sort all the encoded values first.
  std::vector<int64_t> encoded_values(values_set.begin(), values_set.end());
  std::sort(encoded_values.begin(), encoded_values.end());
  CHECK(!encoded_values.empty());
  const bool is_fully_encoded =
      encoded_values.size() == context_->DomainOf(var).Size();

  // Link all Boolean in our linear1 to the encoding literals. Note that we
  // should hopefully already have detected such literal before and this
  // should add trivial implications.
  for (const int64_t v : encoded_values) {
    const int encoding_lit = context_->GetOrCreateVarValueEncoding(var, v);
    const auto eq_it = value_to_equal_literals.find(v);
    if (eq_it != value_to_equal_literals.end()) {
      absl::c_sort(eq_it->second);
      for (const int lit : eq_it->second) {
        context_->AddImplication(lit, encoding_lit);
      }
    }
    const auto neq_it = value_to_not_equal_literals.find(v);
    if (neq_it != value_to_not_equal_literals.end()) {
      absl::c_sort(neq_it->second);
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
      // Tricky: We cannot just choose an arbitrary value if the objective has
      // a restrictive domain!
      if (context_->ObjectiveDomainIsConstraining() &&
          !other_values.IsFixed()) {
        context_->UpdateRuleStats(
            "TODO variables: only used in objective and in encoding");
        return;
      }

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
  {
    std::vector<int> to_clear;
    for (const int c : context_->VarToConstraints(var)) {
      if (c >= 0) to_clear.push_back(c);
    }
    absl::c_sort(to_clear);
    for (const int c : to_clear) {
      if (c < 0) continue;
      context_->working_model->mutable_constraints(c)->Clear();
      context_->UpdateConstraintVariableUsage(c);
    }
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
    // Note that this one must be first in the mapping model, so that if any
    // of the literal was true, var was assigned to the correct value.
    ConstraintProto* mapping_ct =
        context_->NewMappingConstraint(__FILE__, __LINE__);
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

  // Add enough constraints to the mapping model to recover a valid value
  // for var when all the booleans are fixed.
  for (const int64_t value : encoded_values) {
    const int enf = context_->GetOrCreateVarValueEncoding(var, value);
    ConstraintProto* ct = context_->NewMappingConstraint(__FILE__, __LINE__);
    ct->add_enforcement_literal(enf);
    ct->mutable_linear()->add_vars(var);
    ct->mutable_linear()->add_coeffs(1);
    ct->mutable_linear()->add_domain(value);
    ct->mutable_linear()->add_domain(value);
  }

  context_->UpdateNewConstraintsVariableUsage();
  context_->MarkVariableAsRemoved(var);
}

void CpModelPresolver::TryToSimplifyDomain(int var) {
  DCHECK(RefIsPositive(var));
  DCHECK(context_->ConstraintVariableGraphIsUpToDate());
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

// Adds all affine relations to our model for the variables that are still used.
void CpModelPresolver::EncodeAllAffineRelations() {
  int64_t num_added = 0;
  for (int var = 0; var < context_->working_model->variables_size(); ++var) {
    if (context_->IsFixed(var)) continue;

    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative == var) continue;

    // TODO(user): It seems some affine relation are still removable at this
    // stage even though they should be removed inside PresolveToFixPoint().
    // Investigate. For now, we just remove such relations.
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (!PresolveAffineRelationIfAny(var)) break;
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (context_->IsFixed(var)) continue;

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

// Re-add to the queue the constraints that touch a variable that changed.
bool CpModelPresolver::ProcessChangedVariables(std::vector<bool>* in_queue,
                                               std::deque<int>* queue) {
  // TODO(user): Avoid reprocessing the constraints that changed the domain?
  if (context_->ModelIsUnsat()) return false;
  if (time_limit_->LimitReached()) return false;
  in_queue->resize(context_->working_model->constraints_size(), false);
  const auto& vector_that_can_grow_during_iter =
      context_->modified_domains.PositionsSetAtLeastOnce();
  for (int i = 0; i < vector_that_can_grow_during_iter.size(); ++i) {
    const int v = vector_that_can_grow_during_iter[i];
    context_->modified_domains.Clear(v);
    if (context_->VariableIsNotUsedAnymore(v)) continue;
    if (context_->ModelIsUnsat()) return false;
    if (!PresolveAffineRelationIfAny(v)) return false;
    if (context_->VariableIsNotUsedAnymore(v)) continue;

    TryToSimplifyDomain(v);

    // TODO(user): Integrate these with TryToSimplifyDomain().
    if (context_->ModelIsUnsat()) return false;
    context_->UpdateNewConstraintsVariableUsage();

    if (!context_->CanonicalizeOneObjectiveVariable(v)) return false;

    in_queue->resize(context_->working_model->constraints_size(), false);
    for (const int c : context_->VarToConstraints(v)) {
      if (c >= 0 && !(*in_queue)[c]) {
        (*in_queue)[c] = true;
        queue->push_back(c);
      }
    }
  }
  context_->modified_domains.SparseClearAll();

  // Make sure the order is deterministic! because var_to_constraints[]
  // order changes from one run to the next.
  std::sort(queue->begin(), queue->end());
  return !queue->empty();
}

void CpModelPresolver::PresolveToFixPoint() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // We do at most 2 tests per PresolveToFixPoint() call since this can be slow.
  int num_dominance_tests = 0;
  int num_dual_strengthening = 0;

  // Limit on number of operations.
  const int64_t max_num_operations =
      context_->params().debug_max_num_presolve_operations() > 0
          ? context_->params().debug_max_num_presolve_operations()
          : std::numeric_limits<int64_t>::max();

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  absl::flat_hash_set<std::pair<int, int>> var_constraint_pair_already_called;

  // The queue of "active" constraints, initialized to the non-empty ones.
  std::vector<bool> in_queue(context_->working_model->constraints_size(),
                             false);
  std::deque<int> queue;
  for (int c = 0; c < in_queue.size(); ++c) {
    if (context_->working_model->constraints(c).constraint_case() !=
        ConstraintProto::CONSTRAINT_NOT_SET) {
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

  // We put a hard limit on the number of loop to prevent some corner case with
  // propagation loops. Note that the limit is quite high so it shouldn't really
  // be reached in most situation.
  int num_loops = 0;
  constexpr int kMaxNumLoops = 1000;
  for (; num_loops < kMaxNumLoops && !queue.empty(); ++num_loops) {
    if (time_limit_->LimitReached()) break;
    if (context_->ModelIsUnsat()) break;
    if (context_->num_presolve_operations > max_num_operations) break;

    // Empty the queue of single constraint presolve.
    while (!queue.empty() && !context_->ModelIsUnsat()) {
      if (time_limit_->LimitReached()) break;
      if (context_->num_presolve_operations > max_num_operations) break;
      const int c = queue.front();
      in_queue[c] = false;
      queue.pop_front();

      const int old_num_constraint =
          context_->working_model->constraints_size();
      const bool changed = PresolveOneConstraint(c);
      if (context_->ModelIsUnsat()) {
        SOLVER_LOG(
            logger_, "Unsat after presolving constraint #", c,
            " (warning, dump might be inconsistent): ",
            ProtobufShortDebugString(context_->working_model->constraints(c)));
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

      // TODO(user): Is seems safer to remove the changed Boolean and maybe
      // just compare the number of applied "rules" before/after.
      if (changed) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }

    if (context_->ModelIsUnsat()) return;

    in_queue.resize(context_->working_model->constraints_size(), false);
    const auto& vector_that_can_grow_during_iter =
        context_->var_with_reduced_small_degree.PositionsSetAtLeastOnce();
    for (int i = 0; i < vector_that_can_grow_during_iter.size(); ++i) {
      const int v = vector_that_can_grow_during_iter[i];
      if (context_->VariableIsNotUsedAnymore(v)) continue;

      // Remove the variable from the set to allow it to be pushed again.
      // This is necessary since a few affine logic needs to add the same
      // variable back to a second pass of processing.
      context_->var_with_reduced_small_degree.Clear(v);

      // Make sure all affine relations are propagated.
      // This also remove the relation if the degree is now one.
      if (context_->ModelIsUnsat()) return;
      if (!PresolveAffineRelationIfAny(v)) return;

      const int degree = context_->VarToConstraints(v).size();
      if (degree == 0) continue;
      if (degree == 2) LookAtVariableWithDegreeTwo(v);
      if (degree == 2 || degree == 3) {
        // Tricky: this function can add new constraint.
        ProcessVariableInTwoAtMostOrExactlyOne(v);
        in_queue.resize(context_->working_model->constraints_size(), false);
        continue;
      }

      // Re-add to the queue constraints that have unique variables. Note that
      // to not enter an infinite loop, we call each (var, constraint) pair at
      // most once.
      if (degree != 1) continue;
      const int c = *context_->VarToConstraints(v).begin();
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
    context_->var_with_reduced_small_degree.SparseClearAll();

    if (ProcessChangedVariables(&in_queue, &queue)) continue;

    DCHECK(!context_->HasUnusedAffineVariable());

    // Deal with integer variable only appearing in an encoding.
    for (int v = 0; v < context_->working_model->variables().size(); ++v) {
      ProcessVariableOnlyUsedInEncoding(v);
    }
    if (ProcessChangedVariables(&in_queue, &queue)) continue;

    // Perform dual reasoning.
    //
    // TODO(user): We can support assumptions but we need to not cut them out
    // of the feasible region.
    if (context_->params().keep_all_feasible_solutions_in_presolve()) break;
    if (!context_->working_model->assumptions().empty()) break;

    // Starts by the "faster" algo that exploit variables that can move freely
    // in one direction. Or variables that are just blocked by one constraint in
    // one direction.
    for (int i = 0; i < 10; ++i) {
      if (context_->ModelIsUnsat()) return;
      ++num_dual_strengthening;
      DualBoundStrengthening dual_bound_strengthening;
      ScanModelForDualBoundStrengthening(*context_, &dual_bound_strengthening);

      // TODO(user): Make sure that if we fix one variable, we fix its full
      // symmetric orbit. There should be no reason that we don't do that
      // though.
      if (!dual_bound_strengthening.Strengthen(context_)) return;
      if (ProcessChangedVariables(&in_queue, &queue)) break;

      // It is possible we deleted some constraint, but the queue is empty.
      // In this case we redo a pass of dual bound strenghtening as we might
      // perform more reduction.
      //
      // TODO(user): maybe we could reach fix point directly?
      if (dual_bound_strengthening.NumDeletedConstraints() == 0) break;
    }
    if (!queue.empty()) continue;

    // Dominance reasoning will likely break symmetry.
    // TODO(user): We can apply the one that do not break any though, or the
    // operations that are safe.
    if (context_->params().keep_symmetry_in_presolve()) break;

    // Detect & exploit dominance between variables.
    // TODO(user): This can be slow, remove from fix-pint loop?
    if (num_dominance_tests++ < 2) {
      if (context_->ModelIsUnsat()) return;
      PresolveTimer timer("DetectDominanceRelations", logger_, time_limit_);
      VarDomination var_dom;
      ScanModelForDominanceDetection(*context_, &var_dom);
      if (!ExploitDominanceRelations(var_dom, context_)) return;
      if (ProcessChangedVariables(&in_queue, &queue)) continue;
    }
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
      case ConstraintProto::kNoOverlap:
        // Filter out absent intervals.
        if (PresolveNoOverlap(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::kNoOverlap2D:
        // Filter out absent intervals.
        if (PresolveNoOverlap2D(c, ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::kCumulative:
        // Filter out absent intervals.
        if (PresolveCumulative(ct)) {
          context_->UpdateConstraintVariableUsage(c);
        }
        break;
      case ConstraintProto::kBoolOr: {
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

  timer.AddCounter("num_loops", num_loops);
  timer.AddCounter("num_dual_strengthening", num_dual_strengthening);
  context_->deductions.MarkProcessingAsDoneForNow();
}

ModelCopy::ModelCopy(PresolveContext* context) : context_(context) {}

void ModelCopy::ImportVariablesAndMaybeIgnoreNames(
    const CpModelProto& in_model) {
  if (context_->params().ignore_names()) {
    context_->working_model->clear_variables();
    context_->working_model->mutable_variables()->Reserve(
        in_model.variables_size());
    for (const IntegerVariableProto& var_proto : in_model.variables()) {
      *context_->working_model->add_variables()->mutable_domain() =
          var_proto.domain();
    }
  } else {
    *context_->working_model->mutable_variables() = in_model.variables();
  }
}

void ModelCopy::CreateVariablesFromDomains(const std::vector<Domain>& domains) {
  for (const Domain& domain : domains) {
    FillDomainInProto(domain, context_->working_model->add_variables());
  }
}

// TODO(user): Merge with the phase 1 of the presolve code.
//
// TODO(user): It seems easy to forget to update this if any new constraint
// contains an interval or if we add a field to an existing constraint. Find a
// way to remind contributor to not forget this.
bool ModelCopy::ImportAndSimplifyConstraints(
    const CpModelProto& in_model, bool first_copy,
    std::function<bool(int)> active_constraints) {
  context_->InitializeNewDomains();
  const bool ignore_names = context_->params().ignore_names();

  // If first_copy is true, we reorder the scheduling constraint to be sure they
  // refer to interval before them.
  std::vector<int> constraints_using_intervals;

  starting_constraint_index_ = context_->working_model->constraints_size();
  for (int c = 0; c < in_model.constraints_size(); ++c) {
    if (active_constraints != nullptr && !active_constraints(c)) continue;
    const ConstraintProto& ct = in_model.constraints(c);
    if (first_copy) {
      if (!PrepareEnforcementCopyWithDup(ct)) continue;
    } else {
      if (!PrepareEnforcementCopy(ct)) continue;
    }

    // TODO(user): if ignore_names is false, we should make sure the
    // name are properly copied by all these functions. Or we should never copy
    // name and have a separate if (!ignore_name) copy the name...
    switch (ct.constraint_case()) {
      case ConstraintProto::CONSTRAINT_NOT_SET:
        break;
      case ConstraintProto::kBoolOr:
        if (first_copy) {
          if (!CopyBoolOrWithDupSupport(ct)) return CreateUnsatModel(c, ct);
        } else {
          if (!CopyBoolOr(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kBoolAnd:
        if (temp_enforcement_literals_.empty()) {
          for (const int lit : ct.bool_and().literals()) {
            context_->UpdateRuleStats("bool_and: non-reified.");
            if (!context_->SetLiteralToTrue(lit)) {
              return CreateUnsatModel(c, ct);
            }
          }
        } else if (first_copy) {
          if (!CopyBoolAndWithDupSupport(ct)) return CreateUnsatModel(c, ct);
        } else {
          if (!CopyBoolAnd(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kLinear:
        if (!CopyLinear(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntProd:
        if (!CopyIntProd(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kIntDiv:
        if (!CopyIntDiv(ct, ignore_names)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kElement:
        if (!CopyElement(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kTable:
        if (!CopyTable(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAutomaton:
        if (!CopyAutomaton(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAllDiff:
        if (!CopyAllDiff(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kAtMostOne:
        if (!CopyAtMostOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kExactlyOne:
        if (!CopyExactlyOne(ct)) return CreateUnsatModel(c, ct);
        break;
      case ConstraintProto::kInterval:
        if (!CopyInterval(ct, c, ignore_names)) return CreateUnsatModel(c, ct);
        if (first_copy) {
          if (!AddLinearConstraintForInterval(ct))
            return CreateUnsatModel(c, ct);
        }
        break;
      case ConstraintProto::kNoOverlap:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap(ct);
        }
        break;
      case ConstraintProto::kNoOverlap2D:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          CopyAndMapNoOverlap2D(ct);
        }
        break;
      case ConstraintProto::kCumulative:
        if (first_copy) {
          constraints_using_intervals.push_back(c);
        } else {
          if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        }
        break;
      default: {
        ConstraintProto* new_ct = context_->working_model->add_constraints();
        *new_ct = ct;
        new_ct->mutable_enforcement_literal()->Clear();
        FinishEnforcementCopy(new_ct);
        if (ignore_names) {
          // TODO(user): find a better way than copy then clear_name()?
          new_ct->clear_name();
        }
      }
    }
  }

  // This should be empty if first_copy is false.
  DCHECK(first_copy || constraints_using_intervals.empty());
  for (const int c : constraints_using_intervals) {
    const ConstraintProto& ct = in_model.constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kNoOverlap:
        CopyAndMapNoOverlap(ct);
        break;
      case ConstraintProto::kNoOverlap2D:
        CopyAndMapNoOverlap2D(ct);
        break;
      case ConstraintProto::kCumulative:
        if (!CopyAndMapCumulative(ct)) return CreateUnsatModel(c, ct);
        break;
      default:
        LOG(DFATAL) << "Shouldn't be here.";
    }
  }

  return true;
}

bool ModelCopy::PrepareEnforcementCopy(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) continue;
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("enforcement: always false");
      return false;
    }
    temp_enforcement_literals_.push_back(lit);
  }
  return true;  // Continue processing.
}

bool ModelCopy::PrepareEnforcementCopyWithDup(const ConstraintProto& ct) {
  temp_enforcement_literals_.clear();
  temp_enforcement_literals_set_.clear();
  for (const int lit : ct.enforcement_literal()) {
    if (context_->LiteralIsTrue(lit)) continue;
    if (temp_enforcement_literals_set_.contains(lit)) {
      context_->UpdateRuleStats("enforcement: removed duplicate literal");
      continue;
    }

    // Cannot be satisfied.
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("enforcement: always false");
      return false;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("enforcement: contains x and not(x)");
      return false;
    }

    temp_enforcement_literals_.push_back(lit);
    temp_enforcement_literals_set_.insert(lit);
  }
  return true;  // Continue processing.
}

void ModelCopy::FinishEnforcementCopy(ConstraintProto* ct) {
  ct->mutable_enforcement_literal()->Add(temp_enforcement_literals_.begin(),
                                         temp_enforcement_literals_.end());
}

bool ModelCopy::FinishBoolOrCopy() {
  if (temp_literals_.empty()) return false;

  if (temp_literals_.size() == 1) {
    context_->UpdateRuleStats("bool_or: only one literal");
    return context_->SetLiteralToTrue(temp_literals_[0]);
  }

  context_->working_model->add_constraints()
      ->mutable_bool_or()
      ->mutable_literals()
      ->Add(temp_literals_.begin(), temp_literals_.end());
  return true;
}

bool ModelCopy::CopyBoolOr(const ConstraintProto& ct) {
  temp_literals_.clear();
  for (const int lit : temp_enforcement_literals_) {
    temp_literals_.push_back(NegatedRef(lit));
  }
  for (const int lit : ct.bool_or().literals()) {
    if (context_->LiteralIsTrue(lit)) {
      return true;
    }
    if (!context_->LiteralIsFalse(lit)) {
      temp_literals_.push_back(lit);
    }
  }
  return FinishBoolOrCopy();
}

bool ModelCopy::CopyBoolOrWithDupSupport(const ConstraintProto& ct) {
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int enforcement_lit : temp_enforcement_literals_) {
    // Having an enforcement literal is the same as having its negation on
    // the clause.
    const int lit = NegatedRef(enforcement_lit);

    // Note that we already dealt with duplicate since we should have called
    // PrepareEnforcementCopyWithDup() in this case.
    temp_literals_set_.insert(lit);
    temp_literals_.push_back(lit);
  }
  for (const int lit : ct.bool_or().literals()) {
    if (context_->LiteralIsTrue(lit)) {
      context_->UpdateRuleStats("bool_or: always true");
      return true;
    }
    if (context_->LiteralIsFalse(lit)) continue;
    if (temp_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool_or: always true");
      return true;
    }
    const auto [it, inserted] = temp_literals_set_.insert(lit);
    if (inserted) temp_literals_.push_back(lit);
  }
  return FinishBoolOrCopy();
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
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        context_->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  } else if (num_non_fixed_literals > 0) {
    ConstraintProto* new_ct = context_->working_model->add_constraints();
    FinishEnforcementCopy(new_ct);
    BoolArgumentProto* bool_and = new_ct->mutable_bool_and();
    bool_and->mutable_literals()->Reserve(num_non_fixed_literals);
    for (const int lit : ct.bool_and().literals()) {
      if (context_->LiteralIsTrue(lit)) continue;
      bool_and->add_literals(lit);
    }
  }
  return true;
}

bool ModelCopy::CopyBoolAndWithDupSupport(const ConstraintProto& ct) {
  DCHECK(!temp_enforcement_literals_.empty());

  bool at_least_one_false = false;
  temp_literals_.clear();
  temp_literals_set_.clear();
  for (const int lit : ct.bool_and().literals()) {
    if (context_->LiteralIsFalse(lit)) {
      context_->UpdateRuleStats("bool and: always false");
      at_least_one_false = true;
      break;
    }
    if (temp_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool and: => x and not(x) ");
      at_least_one_false = true;
      break;
    }
    if (temp_enforcement_literals_set_.contains(NegatedRef(lit))) {
      context_->UpdateRuleStats("bool and: not(x) => x");
      at_least_one_false = true;
      break;
    }

    if (context_->LiteralIsTrue(lit)) continue;
    if (temp_enforcement_literals_set_.contains(lit)) {
      context_->UpdateRuleStats("bool and: x => x");
      continue;
    }
    const auto [it, inserted] = temp_literals_set_.insert(lit);
    if (inserted) temp_literals_.push_back(lit);
  }

  if (at_least_one_false) {
    // One enforcement literal must be false.
    BoolArgumentProto* bool_or =
        context_->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : temp_enforcement_literals_) {
      bool_or->add_literals(NegatedRef(lit));
    }
    return !bool_or->literals().empty();
  }

  if (temp_literals_.empty()) {
    context_->UpdateRuleStats("bool and: empty");
    return true;
  }

  // Copy.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_bool_and()->mutable_literals()->Add(temp_literals_.begin(),
                                                      temp_literals_.end());
  return true;
}

bool ModelCopy::CopyLinearExpression(const LinearExpressionProto& expr,
                                     LinearExpressionProto* dst) {
  non_fixed_variables_.clear();
  non_fixed_coefficients_.clear();
  int64_t offset = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int ref = expr.vars(i);
    const int64_t coeff = expr.coeffs(i);
    if (coeff == 0) continue;
    if (context_->IsFixed(ref)) {
      offset += coeff * context_->MinOf(ref);
      continue;
    }

    // Make sure we never have negative ref in a linear constraint.
    if (RefIsPositive(ref)) {
      non_fixed_variables_.push_back(ref);
      non_fixed_coefficients_.push_back(coeff);
    } else {
      non_fixed_variables_.push_back(NegatedRef(ref));
      non_fixed_coefficients_.push_back(-coeff);
    }
  }

  dst->set_offset(offset);
  dst->mutable_vars()->Add(non_fixed_variables_.begin(),
                           non_fixed_variables_.end());
  dst->mutable_coeffs()->Add(non_fixed_coefficients_.begin(),
                             non_fixed_coefficients_.end());
  return true;
}

bool ModelCopy::CopyLinear(const ConstraintProto& ct) {
  non_fixed_variables_.clear();
  non_fixed_coefficients_.clear();
  int64_t offset = 0;
  int64_t min_activity = 0;
  int64_t max_activity = 0;
  for (int i = 0; i < ct.linear().vars_size(); ++i) {
    const int ref = ct.linear().vars(i);
    const int64_t coeff = ct.linear().coeffs(i);
    if (coeff == 0) continue;
    if (context_->IsFixed(ref)) {
      offset += coeff * context_->MinOf(ref);
      continue;
    }

    if (coeff > 0) {
      min_activity += coeff * context_->MinOf(ref);
      max_activity += coeff * context_->MaxOf(ref);
    } else {
      min_activity += coeff * context_->MaxOf(ref);
      max_activity += coeff * context_->MinOf(ref);
    }

    // Make sure we never have negative ref in a linear constraint.
    if (RefIsPositive(ref)) {
      non_fixed_variables_.push_back(ref);
      non_fixed_coefficients_.push_back(coeff);
    } else {
      non_fixed_variables_.push_back(NegatedRef(ref));
      non_fixed_coefficients_.push_back(-coeff);
    }
  }

  const Domain implied(min_activity, max_activity);
  const Domain new_rhs =
      ReadDomainFromProto(ct.linear()).AdditionWith(Domain(-offset));

  // Trivial constraint?
  if (implied.IsIncludedIn(new_rhs)) {
    context_->UpdateRuleStats("linear: always true");
    return true;
  }

  // Constraint is false?
  if (implied.IntersectionWith(new_rhs).IsEmpty()) {
    if (ct.enforcement_literal().empty()) return false;
    temp_literals_.clear();
    for (const int literal : ct.enforcement_literal()) {
      if (!context_->LiteralIsTrue(literal)) {
        temp_literals_.push_back(NegatedRef(literal));
      }
    }
    context_->working_model->add_constraints()
        ->mutable_bool_or()
        ->mutable_literals()
        ->Add(temp_literals_.begin(), temp_literals_.end());
    return !temp_literals_.empty();
  }

  DCHECK(!non_fixed_variables_.empty());

  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  LinearConstraintProto* linear = new_ct->mutable_linear();
  linear->mutable_vars()->Add(non_fixed_variables_.begin(),
                              non_fixed_variables_.end());
  linear->mutable_coeffs()->Add(non_fixed_coefficients_.begin(),
                                non_fixed_coefficients_.end());
  FillDomainInProto(new_rhs, linear);
  return true;
}

bool ModelCopy::CopyElement(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (ct.element().vars().empty() && !ct.element().exprs().empty()) {
    // New format, just copy.
    *new_ct = ct;
    return true;
  }

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  fill_expr(ct.element().index(),
            new_ct->mutable_element()->mutable_linear_index());
  fill_expr(ct.element().target(),
            new_ct->mutable_element()->mutable_linear_target());
  for (const int var : ct.element().vars()) {
    fill_expr(var, new_ct->mutable_element()->add_exprs());
  }
  return true;
}

bool ModelCopy::CopyAutomaton(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  *new_ct = ct;
  if (new_ct->automaton().vars().empty()) return true;

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  for (const int var : ct.automaton().vars()) {
    fill_expr(var, new_ct->mutable_automaton()->add_exprs());
  }
  new_ct->mutable_automaton()->clear_vars();

  return true;
}

bool ModelCopy::CopyTable(const ConstraintProto& ct) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (ct.table().vars().empty() && !ct.table().exprs().empty()) {
    // New format, just copy.
    *new_ct = ct;
    return true;
  }

  auto fill_expr = [this](int var, LinearExpressionProto* expr) mutable {
    if (context_->IsFixed(var)) {
      expr->set_offset(context_->FixedValue(var));
    } else {
      DCHECK(RefIsPositive(var));
      expr->mutable_vars()->Reserve(1);
      expr->mutable_coeffs()->Reserve(1);
      expr->add_vars(var);
      expr->add_coeffs(1);
    }
  };

  for (const int var : ct.table().vars()) {
    fill_expr(var, new_ct->mutable_table()->add_exprs());
  }
  *new_ct->mutable_table()->mutable_values() = ct.table().values();
  new_ct->mutable_table()->set_negated(ct.table().negated());

  return true;
}

bool ModelCopy::CopyAllDiff(const ConstraintProto& ct) {
  if (ct.all_diff().exprs().size() <= 1) return true;
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  *new_ct = ct;
  return true;
}

bool ModelCopy::CopyAtMostOne(const ConstraintProto& ct) {
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.at_most_one().literals()) {
    if (context_->LiteralIsFalse(lit)) continue;
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.size() <= 1) return true;
  if (num_true > 1) return false;

  // TODO(user): presolve if num_true == 1.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_at_most_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyExactlyOne(const ConstraintProto& ct) {
  int num_true = 0;
  temp_literals_.clear();
  for (const int lit : ct.exactly_one().literals()) {
    if (context_->LiteralIsFalse(lit)) continue;
    temp_literals_.push_back(lit);
    if (context_->LiteralIsTrue(lit)) num_true++;
  }

  if (temp_literals_.empty() || num_true > 1) return false;
  if (temp_literals_.size() == 1 && num_true == 1) return true;

  // TODO(user): presolve if num_true == 1 and not everything is false.
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  FinishEnforcementCopy(new_ct);
  new_ct->mutable_exactly_one()->mutable_literals()->Add(temp_literals_.begin(),
                                                         temp_literals_.end());
  return true;
}

bool ModelCopy::CopyInterval(const ConstraintProto& ct, int c,
                             bool ignore_names) {
  CHECK_EQ(starting_constraint_index_, 0)
      << "Adding new interval constraints to partially filled model is not "
         "supported.";
  interval_mapping_[c] = context_->working_model->constraints_size();
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  *new_ct->mutable_enforcement_literal() = ct.enforcement_literal();
  CopyLinearExpression(ct.interval().start(),
                       new_ct->mutable_interval()->mutable_start());
  CopyLinearExpression(ct.interval().size(),
                       new_ct->mutable_interval()->mutable_size());
  CopyLinearExpression(ct.interval().end(),
                       new_ct->mutable_interval()->mutable_end());
  return true;
}

bool ModelCopy::CopyIntProd(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_prod()->add_exprs());
  }
  CopyLinearExpression(ct.int_prod().target(),
                       new_ct->mutable_int_prod()->mutable_target());
  return true;
}

bool ModelCopy::CopyIntDiv(const ConstraintProto& ct, bool ignore_names) {
  ConstraintProto* new_ct = context_->working_model->add_constraints();
  if (!ignore_names) {
    new_ct->set_name(ct.name());
  }
  for (const LinearExpressionProto& expr : ct.int_div().exprs()) {
    CopyLinearExpression(expr, new_ct->mutable_int_div()->add_exprs());
  }
  CopyLinearExpression(ct.int_div().target(),
                       new_ct->mutable_int_div()->mutable_target());
  return true;
}

bool ModelCopy::AddLinearConstraintForInterval(const ConstraintProto& ct) {
  // Add the linear constraint enforcement => (start + size == end).
  //
  // We rely on the presolve for simplification, but deal with the trivial
  // case of (start, offset, start + offset) here.
  const IntervalConstraintProto& itv = ct.interval();
  if (itv.size().vars().empty() &&
      itv.start().offset() + itv.size().offset() == itv.end().offset() &&
      absl::Span<const int>(itv.start().vars()) ==
          absl::Span<const int>(itv.end().vars()) &&
      absl::Span<const int64_t>(itv.start().coeffs()) ==
          absl::Span<const int64_t>(itv.end().coeffs())) {
    // Trivial constraint, nothing to do.
  } else {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    LinearConstraintProto* mutable_linear = tmp_constraint_.mutable_linear();

    mutable_linear->add_domain(0);
    mutable_linear->add_domain(0);
    AddLinearExpressionToLinearConstraint(itv.start(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.size(), 1, mutable_linear);
    AddLinearExpressionToLinearConstraint(itv.end(), -1, mutable_linear);
    if (!CopyLinear(tmp_constraint_)) return false;
  }

  // An enforced interval must have is size non-negative.
  const LinearExpressionProto& size_expr = itv.size();
  if (context_->MinOf(size_expr) < 0) {
    tmp_constraint_.Clear();
    *tmp_constraint_.mutable_enforcement_literal() = ct.enforcement_literal();
    *tmp_constraint_.mutable_linear()->mutable_vars() = size_expr.vars();
    *tmp_constraint_.mutable_linear()->mutable_coeffs() = size_expr.coeffs();
    tmp_constraint_.mutable_linear()->add_domain(-size_expr.offset());
    tmp_constraint_.mutable_linear()->add_domain(
        std::numeric_limits<int64_t>::max());
    if (!CopyLinear(tmp_constraint_)) return false;
  }

  return true;
}

void ModelCopy::CopyAndMapNoOverlap(const ConstraintProto& ct) {
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_no_overlap();
  new_ct->mutable_intervals()->Reserve(ct.no_overlap().intervals().size());
  for (const int index : ct.no_overlap().intervals()) {
    const auto it = interval_mapping_.find(index);
    if (it == interval_mapping_.end()) continue;
    new_ct->add_intervals(it->second);
  }
}

void ModelCopy::CopyAndMapNoOverlap2D(const ConstraintProto& ct) {
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_no_overlap_2d();

  const int num_intervals = ct.no_overlap_2d().x_intervals().size();
  new_ct->mutable_x_intervals()->Reserve(num_intervals);
  new_ct->mutable_y_intervals()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const auto x_it = interval_mapping_.find(ct.no_overlap_2d().x_intervals(i));
    if (x_it == interval_mapping_.end()) continue;
    const auto y_it = interval_mapping_.find(ct.no_overlap_2d().y_intervals(i));
    if (y_it == interval_mapping_.end()) continue;
    new_ct->add_x_intervals(x_it->second);
    new_ct->add_y_intervals(y_it->second);
  }
}

bool ModelCopy::CopyAndMapCumulative(const ConstraintProto& ct) {
  if (ct.cumulative().intervals().empty() &&
      ct.cumulative().capacity().vars().empty()) {
    // Trivial constraint, either obviously SAT or UNSAT.
    return ct.cumulative().capacity().offset() >= 0;
  }
  // Note that we don't copy names or enforcement_literal (not supported) here.
  auto* new_ct =
      context_->working_model->add_constraints()->mutable_cumulative();
  CopyLinearExpression(ct.cumulative().capacity(), new_ct->mutable_capacity());

  const int num_intervals = ct.cumulative().intervals().size();
  new_ct->mutable_intervals()->Reserve(num_intervals);
  new_ct->mutable_demands()->Reserve(num_intervals);
  for (int i = 0; i < num_intervals; ++i) {
    const auto it = interval_mapping_.find(ct.cumulative().intervals(i));
    if (it == interval_mapping_.end()) continue;
    new_ct->add_intervals(it->second);
    *new_ct->add_demands() = ct.cumulative().demands(i);
  }

  return true;
}

bool ModelCopy::CreateUnsatModel(int c, const ConstraintProto& ct) {
  context_->working_model->mutable_constraints()->Clear();
  context_->working_model->add_constraints()->mutable_bool_or();

  // If the model was already marked as unsat, we keep the old message and just
  // return. TODO(user): Append messages instead?
  if (context_->ModelIsUnsat()) return false;

  std::string proto_string;
#if !defined(__PORTABLE_PLATFORM__)
  google::protobuf::TextFormat::Printer printer;
  SetupTextFormatPrinter(&printer);
  printer.PrintToString(ct, &proto_string);
#endif  // !defined(__PORTABLE_PLATFORM__)
  std::string message = absl::StrCat(
      "proven during initial copy of constraint #", c, ":\n", proto_string);
  std::vector<int> vars = UsedVariables(ct);
  if (vars.size() < 10) {
    absl::StrAppend(&message, "With current variable domains:\n");
    for (const int var : vars) {
      absl::StrAppend(&message, "var:", var,
                      " domain:", context_->DomainOf(var).ToString(), "\n");
    }
  }
  return context_->NotifyThatModelIsUnsat(message);
}

bool ImportModelWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                             PresolveContext* context) {
  ModelCopy copier(context);
  copier.ImportVariablesAndMaybeIgnoreNames(in_model);
  if (copier.ImportAndSimplifyConstraints(in_model, /*first_copy=*/true)) {
    CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(in_model,
                                                                 context);
    return true;
  }
  return !context->ModelIsUnsat();
}

bool ImportModelAndDomainsWithBasicPresolveIntoContext(
    const CpModelProto& in_model, const std::vector<Domain>& domains,
    std::function<bool(int)> active_constraints, PresolveContext* context) {
  CHECK_EQ(domains.size(), in_model.variables_size());
  ModelCopy copier(context);
  copier.CreateVariablesFromDomains(domains);
  if (copier.ImportAndSimplifyConstraints(in_model, /*first_copy=*/false,
                                          active_constraints)) {
    CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(in_model,
                                                                 context);
    return true;
  }
  return !context->ModelIsUnsat();
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
    // We make sure we do not use the old variables field.
    *context->working_model->mutable_search_strategy() =
        in_model.search_strategy();
    for (DecisionStrategyProto& strategy :
         *context->working_model->mutable_search_strategy()) {
      google::protobuf::util::RemoveIf(strategy.mutable_exprs(),
                                       [](const LinearExpressionProto* expr) {
                                         return expr->vars().empty();
                                       });
      if (!strategy.variables().empty()) {
        CHECK(strategy.exprs().empty());
        for (const int ref : strategy.variables()) {
          LinearExpressionProto* expr = strategy.add_exprs();
          expr->add_vars(PositiveRef(ref));
          expr->add_coeffs(RefIsPositive(ref) ? 1 : -1);
        }
        strategy.clear_variables();
      }
    }
  }
  if (!in_model.assumptions().empty()) {
    *context->working_model->mutable_assumptions() = in_model.assumptions();
  }
  if (in_model.has_symmetry()) {
    *context->working_model->mutable_symmetry() = in_model.symmetry();
  }
  if (in_model.has_solution_hint()) {
    *context->working_model->mutable_solution_hint() = in_model.solution_hint();

    // We make sure the hint is within the variables domain.
    //
    // This allows to avoid overflow because we know evaluating constraints on
    // the variables domains should be safe thanks to the initial validation.
    const int num_terms = in_model.solution_hint().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = in_model.solution_hint().vars(i);
      const int64_t value = in_model.solution_hint().values(i);
      const auto& domain = in_model.variables(var).domain();
      if (domain.empty()) continue;  // UNSAT.
      const int64_t min = domain[0];
      const int64_t max = domain[domain.size() - 1];
      if (value < min) {
        context->UpdateRuleStats("hint: moved var hint within its domain.");
        context->working_model->mutable_solution_hint()->set_values(i, min);
      } else if (value > max) {
        context->working_model->mutable_solution_hint()->set_values(i, max);
        context->UpdateRuleStats("hint: moved var hint within its domain.");
      }
    }
  }
}

// TODO(user): Use better heuristic?
//
// TODO(user): This is similar to what Bounded variable addition (BVA) does.
// By adding a new variable, enforcement => literals becomes
// enforcement => x => literals, and we have one clause + #literals implication
// instead of #literals clauses. What BVA does in addition is to use the same
// x for other enforcement list if the rhs literals are shared.
void CpModelPresolver::MergeClauses() {
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // Constraint index that changed.
  std::vector<int> to_clean;

  // Keep a map from negation of enforcement_literal => bool_and ct index.
  absl::flat_hash_map<uint64_t, int> bool_and_map;

  // First loop over the constraint:
  // - Register already existing bool_and.
  // - score at_most_ones literals.
  // - Record bool_or.
  const int num_variables = context_->working_model->variables_size();
  std::vector<int> bool_or_indices;
  std::vector<int64_t> literal_score(2 * num_variables, 0);
  const auto get_index = [](int ref) {
    return 2 * PositiveRef(ref) + (RefIsPositive(ref) ? 0 : 1);
  };

  int64_t num_collisions = 0;
  int64_t num_merges = 0;
  int64_t num_saved_literals = 0;
  ClauseWithOneMissingHasher hasher(*context_->random());
  const int num_constraints = context_->working_model->constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      if (ct->enforcement_literal().size() > 1) {
        // We need to sort the negated literals.
        std::sort(ct->mutable_enforcement_literal()->begin(),
                  ct->mutable_enforcement_literal()->end(),
                  std::greater<int>());
        const auto [it, inserted] = bool_and_map.insert(
            {hasher.HashOfNegatedLiterals(ct->enforcement_literal()), c});
        if (inserted) {
          to_clean.push_back(c);
        } else {
          // See if this is a true duplicate. If yes, merge rhs.
          ConstraintProto* other_ct =
              context_->working_model->mutable_constraints(it->second);
          const absl::Span<const int> s1(ct->enforcement_literal());
          const absl::Span<const int> s2(other_ct->enforcement_literal());
          if (s1 == s2) {
            context_->UpdateRuleStats(
                "bool_and: merged constraints with same enforcement");
            other_ct->mutable_bool_and()->mutable_literals()->Add(
                ct->bool_and().literals().begin(),
                ct->bool_and().literals().end());
            ct->Clear();
            context_->UpdateConstraintVariableUsage(c);
          }
        }
      }
      continue;
    }
    if (ct->constraint_case() == ConstraintProto::kAtMostOne) {
      const int size = ct->at_most_one().literals().size();
      for (const int ref : ct->at_most_one().literals()) {
        literal_score[get_index(ref)] += size;
      }
      continue;
    }
    if (ct->constraint_case() == ConstraintProto::kExactlyOne) {
      const int size = ct->exactly_one().literals().size();
      for (const int ref : ct->exactly_one().literals()) {
        literal_score[get_index(ref)] += size;
      }
      continue;
    }

    if (ct->constraint_case() != ConstraintProto::kBoolOr) continue;

    // Both of these test shouldn't happen, but we have them to be safe.
    if (!ct->enforcement_literal().empty()) continue;
    if (ct->bool_or().literals().size() <= 2) continue;

    std::sort(ct->mutable_bool_or()->mutable_literals()->begin(),
              ct->mutable_bool_or()->mutable_literals()->end());
    hasher.RegisterClause(c, ct->bool_or().literals());
    bool_or_indices.push_back(c);
  }

  for (const int c : bool_or_indices) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);

    bool merged = false;
    timer.TrackSimpleLoop(ct->bool_or().literals().size());
    if (timer.WorkLimitIsReached()) break;
    for (const int ref : ct->bool_or().literals()) {
      const uint64_t hash = hasher.HashWithout(c, ref);
      const auto it = bool_and_map.find(hash);
      if (it != bool_and_map.end()) {
        ++num_collisions;
        const int base_c = it->second;
        auto* and_ct = context_->working_model->mutable_constraints(base_c);
        if (ClauseIsEnforcementImpliesLiteral(
                ct->bool_or().literals(), and_ct->enforcement_literal(), ref)) {
          ++num_merges;
          num_saved_literals += ct->bool_or().literals().size() - 1;
          merged = true;
          and_ct->mutable_bool_and()->add_literals(ref);
          ct->Clear();
          context_->UpdateConstraintVariableUsage(c);
          break;
        }
      }
    }

    if (!merged) {
      // heuristic: take first literal whose negation has highest score.
      int best_ref = ct->bool_or().literals(0);
      int64_t best_score = literal_score[get_index(NegatedRef(best_ref))];
      for (const int ref : ct->bool_or().literals()) {
        const int64_t score = literal_score[get_index(NegatedRef(ref))];
        if (score > best_score) {
          best_ref = ref;
          best_score = score;
        }
      }

      const uint64_t hash = hasher.HashWithout(c, best_ref);
      const auto [_, inserted] = bool_and_map.insert({hash, c});
      if (inserted) {
        to_clean.push_back(c);
        context_->tmp_literals.clear();
        for (const int lit : ct->bool_or().literals()) {
          if (lit == best_ref) continue;
          context_->tmp_literals.push_back(NegatedRef(lit));
        }
        ct->Clear();
        ct->mutable_enforcement_literal()->Assign(
            context_->tmp_literals.begin(), context_->tmp_literals.end());
        ct->mutable_bool_and()->add_literals(best_ref);
      }
    }
  }

  // Retransform to bool_or bool_and with a single rhs.
  for (const int c : to_clean) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    if (ct->bool_and().literals().size() > 1) {
      context_->UpdateConstraintVariableUsage(c);
      continue;
    }

    // We have a single bool_and, lets transform it back to single bool_or.
    context_->tmp_literals.clear();
    context_->tmp_literals.push_back(ct->bool_and().literals(0));
    for (const int ref : ct->enforcement_literal()) {
      context_->tmp_literals.push_back(NegatedRef(ref));
    }
    ct->Clear();
    ct->mutable_bool_or()->mutable_literals()->Assign(
        context_->tmp_literals.begin(), context_->tmp_literals.end());
  }

  timer.AddCounter("num_collisions", num_collisions);
  timer.AddCounter("num_merges", num_merges);
  timer.AddCounter("num_saved_literals", num_saved_literals);
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
      logger_(context->logger()),
      time_limit_(context->time_limit()),
      interval_representative_(context->working_model->constraints_size(),
                               IntervalConstraintHash{context->working_model},
                               IntervalConstraintEq{context->working_model}) {}

CpSolverStatus CpModelPresolver::InfeasibleStatus() {
  if (logger_->LoggingIsEnabled()) context_->LogInfo();
  return CpSolverStatus::INFEASIBLE;
}

void CpModelPresolver::InitializeMappingModelVariables() {
  // Sync the domains.
  for (int i = 0; i < context_->working_model->variables_size(); ++i) {
    FillDomainInProto(context_->DomainOf(i),
                      context_->working_model->mutable_variables(i));
    DCHECK_GT(context_->working_model->variables(i).domain_size(), 0);
  }

  // Set the variables of the mapping_model.
  context_->mapping_model->mutable_variables()->CopyFrom(
      context_->working_model->variables());
}

void CpModelPresolver::ExpandCpModelAndCanonicalizeConstraints() {
  const int num_constraints_before_expansion =
      context_->working_model->constraints_size();
  ExpandCpModel(context_);
  if (context_->ModelIsUnsat()) return;

  // TODO(user): Make sure we can't have duplicate in these constraint.
  // These are due to ExpandCpModel() were we create such constraint with
  // duplicate. The problem is that some code assumes these are presolved
  // before being called.
  const int num_constraints = context_->working_model->constraints().size();
  for (int c = num_constraints_before_expansion; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->working_model->mutable_constraints(c);
    const auto type = ct->constraint_case();
    if (type == ConstraintProto::kAtMostOne ||
        type == ConstraintProto::kExactlyOne) {
      if (PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return;
    } else if (type == ConstraintProto::kLinear) {
      if (CanonicalizeLinear(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }
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
  // We copy the search strategy to the mapping_model.
  for (const auto& decision_strategy :
       context_->working_model->search_strategy()) {
    *(context_->mapping_model->add_search_strategy()) = decision_strategy;
  }

  // Initialize the initial context.working_model domains.
  context_->InitializeNewDomains();
  context_->LoadSolutionHint();

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
  //
  // Note that we did some basic presolving during the first copy of the model.
  // This is important has initializing the constraint <-> variable graph can
  // be costly, so better to remove trivially feasible constraint for instance.
  context_->ReadObjectiveFromProto();
  if (!context_->CanonicalizeObjective()) return InfeasibleStatus();
  context_->UpdateNewConstraintsVariableUsage();
  context_->RegisterVariablesUsedInAssumptions();
  DCHECK(context_->ConstraintVariableUsageIsConsistent());

  // If presolve is false, just run expansion.
  if (!context_->params().cp_model_presolve()) {
    for (ConstraintProto& ct :
         *context_->working_model->mutable_constraints()) {
      if (ct.constraint_case() == ConstraintProto::kLinear) {
        context_->CanonicalizeLinearConstraint(&ct);
      }
    }

    ExpandCpModelAndCanonicalizeConstraints();
    if (context_->ModelIsUnsat()) return InfeasibleStatus();

    // We still write back the canonical objective has we don't deal well
    // with uninitialized domain or duplicate variables.
    if (context_->working_model->has_objective()) {
      context_->WriteObjectiveToProto();
    }

    // We need to append all the variable equivalence that are still used!
    EncodeAllAffineRelations();

    // Make sure we also have an initialized mapping model as we use this for
    // filling the tightened variables. Even without presolve, we do some
    // trivial presolving during the initial copy of the model, and expansion
    // might do more.
    InitializeMappingModelVariables();

    // We don't want to run postsolve when the presolve is disabled, but the
    // expansion might have added some constraints to the mapping model. To
    // restore correctness, we merge them with the working model.
    if (!context_->mapping_model->constraints().empty()) {
      context_->UpdateRuleStats(
          "TODO: mapping model not empty with presolve disabled");
      context_->working_model->mutable_constraints()->MergeFrom(
          context_->mapping_model->constraints());
      context_->mapping_model->clear_constraints();
    }

    if (logger_->LoggingIsEnabled()) context_->LogInfo();
    return CpSolverStatus::UNKNOWN;
  }

  // Presolve all variable domain once. The PresolveToFixPoint() function will
  // only reprocess domain that changed.
  if (context_->ModelIsUnsat()) return InfeasibleStatus();
  for (int var = 0; var < context_->working_model->variables().size(); ++var) {
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (!PresolveAffineRelationIfAny(var)) return InfeasibleStatus();

    // Try to canonicalize the domain, note that we should have detected all
    // affine relations before, so we don't recreate "canononical" variables
    // if they already exist in the model.
    TryToSimplifyDomain(var);
    if (context_->ModelIsUnsat()) return InfeasibleStatus();
    context_->UpdateNewConstraintsVariableUsage();
  }
  if (!context_->CanonicalizeObjective()) return InfeasibleStatus();

  // Main propagation loop.
  for (int iter = 0; iter < context_->params().max_presolve_iterations();
       ++iter) {
    if (time_limit_->LimitReached()) break;
    context_->UpdateRuleStats("presolve: iteration");
    const int64_t old_num_presolve_op = context_->num_presolve_operations;

    // TODO(user): The presolve transformations we do after this is called might
    // result in even more presolve if we were to call this again! improve the
    // code. See for instance plusexample_6_sat.fzn were represolving the
    // presolved problem reduces it even more.
    PresolveToFixPoint();

    // Call expansion.
    if (!context_->ModelIsExpanded()) {
      ExtractEncodingFromLinear();
      ExpandCpModelAndCanonicalizeConstraints();
      if (context_->ModelIsUnsat()) return InfeasibleStatus();
      // We need to re-evaluate the degree because some presolve rule only
      // run after expansion.
      const int num_vars = context_->working_model->variables().size();
      for (int var = 0; var < num_vars; ++var) {
        if (context_->VarToConstraints(var).size() <= 3) {
          context_->var_with_reduced_small_degree.Set(var);
        }
      }
    }
    DCHECK(context_->ConstraintVariableUsageIsConsistent());

    // We run the symmetry before more complex presolve rules as many of them
    // are heuristic based and might break the symmetry present in the original
    // problems. This happens for example on the flatzinc wordpress problem.
    //
    // TODO(user): Decide where is the best place for this.
    //
    // TODO(user): try not to break symmetry in our clique extension or other
    // more advanced presolve rule? Ideally we could even exploit them. But in
    // this case, it is still good to compute them early.
    if (context_->params().symmetry_level() > 0 && !context_->ModelIsUnsat() &&
        !time_limit_->LimitReached()) {
      // Both kind of duplications might introduce a lot of symmetries and we
      // want to do that before we even compute them.
      DetectDuplicateColumns();
      DetectDuplicateConstraints();
      if (context_->params().keep_symmetry_in_presolve()) {
        // If the presolve always keep symmetry, we compute it once and for all.
        if (!context_->working_model->has_symmetry()) {
          DetectAndAddSymmetryToProto(context_->params(),
                                      context_->working_model, logger_);
        }

        // We distinguish an empty symmetry message meaning that symmetry were
        // computed and there is none, and the absence of symmetry message
        // meaning we don't know.
        //
        // TODO(user): Maybe this is a bit brittle. Also move this logic to
        // DetectAndAddSymmetryToProto() ?
        if (!context_->working_model->has_symmetry()) {
          context_->working_model->mutable_symmetry()->Clear();
        }
      } else if (!context_->params()
                      .keep_all_feasible_solutions_in_presolve()) {
        DetectAndExploitSymmetriesInPresolve(context_);
      }
    }

    // Runs SAT specific presolve on the pure-SAT part of the problem.
    // Note that because this can only remove/fix variable not used in the other
    // part of the problem, there is no need to redo more presolve afterwards.
    if (context_->params().cp_model_use_sat_presolve()) {
      if (!time_limit_->LimitReached()) {
        if (!PresolvePureSatPart()) {
          (void)context_->NotifyThatModelIsUnsat(
              "Proven Infeasible during SAT presolve");
          return InfeasibleStatus();
        }
      }
    }

    // Extract redundant at most one constraint from the linear ones.
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
        if (ct->constraint_case() != ConstraintProto::kLinear) continue;
        ExtractAtMostOneFromLinear(ct);
      }
      context_->UpdateNewConstraintsVariableUsage();
    }

    if (context_->params().cp_model_probing_level() > 0) {
      if (!time_limit_->LimitReached()) {
        Probe();
        PresolveToFixPoint();
      }
    } else {
      TransformIntoMaxCliques();
    }

    // Deal with pair of constraints.
    //
    // TODO(user): revisit when different transformation appear.
    // TODO(user): merge these code instead of doing many passes?
    ProcessAtMostOneAndLinear();
    DetectDuplicateConstraints();
    DetectDuplicateConstraintsWithDifferentEnforcements();
    DetectDominatedLinearConstraints();
    DetectDifferentVariables();
    ProcessSetPPC();

    // These operations might break symmetry. Or at the very least, the newly
    // created variable must be incorporated in the generators.
    if (context_->params().find_big_linear_overlap() &&
        !context_->params().keep_symmetry_in_presolve()) {
      FindAlmostIdenticalLinearConstraints();

      ActivityBoundHelper activity_amo_helper;
      activity_amo_helper.AddAllAtMostOnes(*context_->working_model);
      FindBigAtMostOneAndLinearOverlap(&activity_amo_helper);

      // Heuristic: vertical introduce smaller defining constraint and appear in
      // many constraints, so might be more constrained. We might also still
      // make horizontal rectangle with the variable introduced.
      FindBigVerticalLinearOverlap(&activity_amo_helper);
      FindBigHorizontalLinearOverlap(&activity_amo_helper);
    }
    if (context_->ModelIsUnsat()) return InfeasibleStatus();

    // We do that after the duplicate, SAT and SetPPC constraints.
    if (!time_limit_->LimitReached()) {
      // Merge clauses that differ in just one literal.
      // Heuristic use at_most_one to try to tighten the initial LP Relaxation.
      MergeClauses();
      if (/*DISABLES CODE*/ (false)) DetectIncludedEnforcement();
    }

    // The TransformIntoMaxCliques() call above transform all bool and into
    // at most one of size 2. This does the reverse and merge them.
    ConvertToBoolAnd();

    // Call the main presolve to remove the fixed variables and do more
    // deductions.
    PresolveToFixPoint();

    // Exit the loop if no operations were performed.
    //
    // TODO(user): try to be smarter and avoid looping again if little changed.
    const int64_t num_ops =
        context_->num_presolve_operations - old_num_presolve_op;
    if (num_ops == 0) break;
  }
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // Regroup no-overlaps into max-cliques.
  MergeNoOverlapConstraints();
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // Tries to spread the objective amongst many variables.
  // We re-do a canonicalization with the final linear expression.
  if (context_->working_model->has_objective()) {
    if (!context_->params().keep_symmetry_in_presolve()) {
      ExpandObjective();
      if (context_->ModelIsUnsat()) return InfeasibleStatus();
      ShiftObjectiveWithExactlyOnes();
      if (context_->ModelIsUnsat()) return InfeasibleStatus();
    }

    // We re-do a canonicalization with the final linear expression.
    if (!context_->CanonicalizeObjective()) return InfeasibleStatus();
    context_->WriteObjectiveToProto();
  }

  // Now that everything that could possibly be fixed was fixed, make sure we
  // don't leave any linear constraint with fixed variables.
  for (int c = 0; c < context_->working_model->constraints_size(); ++c) {
    ConstraintProto& ct = *context_->working_model->mutable_constraints(c);
    bool need_canonicalize = false;
    if (ct.constraint_case() == ConstraintProto::kLinear) {
      for (const int v : ct.linear().vars()) {
        if (context_->IsFixed(v)) {
          need_canonicalize = true;
          break;
        }
      }
    }
    if (need_canonicalize) {
      if (CanonicalizeLinear(&ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }

  // Take care of linear constraint with a complex rhs.
  FinalExpansionForLinearConstraint(context_);

  // Adds all needed affine relation to context_->working_model.
  EncodeAllAffineRelations();
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // If we have symmetry information, lets filter it.
  if (context_->working_model->has_symmetry()) {
    if (!FilterOrbitOnUnusedOrFixedVariables(
            context_->working_model->mutable_symmetry(), context_)) {
      return InfeasibleStatus();
    }
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
    CHECK(strategy.variables().empty());
    if (strategy.exprs().empty()) continue;

    // Canonicalize each expression to use affine representative.
    ConstraintProto empy_enforcement;
    for (LinearExpressionProto& expr : *strategy.mutable_exprs()) {
      CanonicalizeLinearExpression(empy_enforcement, &expr);
    }

    // Remove fixed expression and affine corresponding to same variables.
    int new_size = 0;
    for (const LinearExpressionProto& expr : strategy.exprs()) {
      if (context_->IsFixed(expr)) continue;

      const auto [_, inserted] = used_variables.insert(expr.vars(0));
      if (!inserted) continue;

      *strategy.mutable_exprs(new_size++) = expr;
    }
    google::protobuf::util::Truncate(strategy.mutable_exprs(), new_size);
  }

  // Sync the domains and initialize the mapping model variables.
  InitializeMappingModelVariables();

  // Remove all the unused variables from the presolved model.
  postsolve_mapping_->clear();
  std::vector<int> mapping(context_->working_model->variables_size(), -1);
  absl::flat_hash_map<int64_t, int> constant_to_index;
  int num_unused_variables = 0;
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

    // Deal with unused variables.
    //
    // If the variable is not fixed, we have multiple feasible solution for
    // this variable, so we can't remove it if we want all of them.
    if (context_->VariableIsNotUsedAnymore(i) &&
        (!context_->params().keep_all_feasible_solutions_in_presolve() ||
         context_->IsFixed(i))) {
      // Tricky. Variables that where not removed by a presolve rule should be
      // fixed first during postsolve, so that more complex postsolve rules
      // can use their values. One way to do that is to fix them here.
      //
      // We prefer to fix them to zero if possible.
      ++num_unused_variables;
      FillDomainInProto(Domain(context_->DomainOf(i).SmallestValue()),
                        context_->mapping_model->mutable_variables(i));
      continue;
    }

    // Merge identical constant. Note that the only place were constant are
    // still left are in the circuit and route constraint for fixed arcs.
    if (context_->IsFixed(i)) {
      auto [it, inserted] = constant_to_index.insert(
          {context_->FixedValue(i), postsolve_mapping_->size()});
      if (!inserted) {
        mapping[i] = it->second;
        continue;
      }
    }

    mapping[i] = postsolve_mapping_->size();
    postsolve_mapping_->push_back(i);
  }
  context_->UpdateRuleStats(absl::StrCat("presolve: ", num_unused_variables,
                                         " unused variables removed."));

  if (context_->params().permute_variable_randomly()) {
    // The mapping might merge variable, so we have to be careful here.
    const int n = postsolve_mapping_->size();
    std::vector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), *context_->random());
    for (int i = 0; i < context_->working_model->variables_size(); ++i) {
      if (mapping[i] != -1) mapping[i] = perm[mapping[i]];
    }
    std::vector<int> new_postsolve_mapping(n);
    for (int i = 0; i < n; ++i) {
      new_postsolve_mapping[perm[i]] = (*postsolve_mapping_)[i];
    }
    *postsolve_mapping_ = std::move(new_postsolve_mapping);
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
    const std::string error =
        ValidateCpModel(*context_->working_model, /*after_presolve=*/true);
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
    int new_size = 0;
    for (LinearExpressionProto expr : strategy.exprs()) {
      DCHECK_EQ(expr.vars().size(), 1);
      const int image = mapping[expr.vars(0)];
      if (image >= 0) {
        expr.set_vars(0, image);
        *strategy.mutable_exprs(new_size++) = expr;
      }
    }
    google::protobuf::util::Truncate(strategy.mutable_exprs(), new_size);
  }

  // Remove strategy with empty affine expression.
  {
    int new_size = 0;
    for (const DecisionStrategyProto& strategy : proto->search_strategy()) {
      if (strategy.exprs().empty()) continue;
      *proto->mutable_search_strategy(new_size++) = strategy;
    }
    google::protobuf::util::Truncate(proto->mutable_search_strategy(),
                                     new_size);
  }

  // Remap the solution hint. Note that after remapping, we may have duplicate
  // variable, so we only keep the first occurrence.
  if (proto->has_solution_hint()) {
    absl::flat_hash_set<int> used_vars;
    auto* mutable_hint = proto->mutable_solution_hint();
    mutable_hint->clear_vars();
    mutable_hint->clear_values();
    const int num_vars = context.working_model->variables().size();
    for (int hinted_var = 0; hinted_var < num_vars; ++hinted_var) {
      if (!context.VarHasSolutionHint(hinted_var)) continue;
      int64_t hinted_value = context.SolutionHint(hinted_var);

      // We always move a hint within bounds.
      // This also make sure a hint of INT_MIN or INT_MAX does not overflow.
      if (hinted_value < context.MinOf(hinted_var)) {
        hinted_value = context.MinOf(hinted_var);
      }
      if (hinted_value > context.MaxOf(hinted_var)) {
        hinted_value = context.MaxOf(hinted_var);
      }

      // Note that if (hinted_value - r.offset) is not divisible by r.coeff,
      // then the hint is clearly infeasible, but we still set it to a "close"
      // value.
      const AffineRelation::Relation r = context.GetAffineRelation(hinted_var);
      const int var = r.representative;
      const int64_t value = (hinted_value - r.offset) / r.coeff;

      const int image = mapping[var];
      if (image >= 0) {
        if (!used_vars.insert(image).second) continue;
        mutable_hint->add_vars(image);
        mutable_hint->add_values(value);
      }
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

  // Remap the symmetries. Note that we should have properly dealt with fixed
  // orbit and such in FilterOrbitOnUnusedOrFixedVariables().
  if (proto->has_symmetry()) {
    for (SparsePermutationProto& generator :
         *proto->mutable_symmetry()->mutable_permutations()) {
      for (int& var : *generator.mutable_support()) {
        CHECK(RefIsPositive(var));
        var = mapping[var];
        CHECK_NE(var, -1);
      }
    }

    // We clear the orbitope info (we don't really use it after presolve).
    proto->mutable_symmetry()->clear_orbitopes();
  }
}

namespace {

// We ignore all the fields but the linear expression.
ConstraintProto CopyObjectiveForDuplicateDetection(
    const CpObjectiveProto& objective) {
  ConstraintProto copy;
  *copy.mutable_linear()->mutable_vars() = objective.vars();
  *copy.mutable_linear()->mutable_coeffs() = objective.coeffs();
  return copy;
}

struct ConstraintHashForDuplicateDetection {
  const CpModelProto* working_model;
  bool ignore_enforcement;
  ConstraintProto objective_constraint;

  ConstraintHashForDuplicateDetection(const CpModelProto* working_model,
                                      bool ignore_enforcement)
      : working_model(working_model),
        ignore_enforcement(ignore_enforcement),
        objective_constraint(
            CopyObjectiveForDuplicateDetection(working_model->objective())) {}

  // We hash our mostly frequently used constraint directly without extra memory
  // allocation. We revert to a generic code using proto serialization for the
  // others.
  std::size_t operator()(int ct_idx) const {
    const ConstraintProto& ct = ct_idx == kObjectiveConstraint
                                    ? objective_constraint
                                    : working_model->constraints(ct_idx);
    const std::pair<ConstraintProto::ConstraintCase, absl::Span<const int>>
        type_and_enforcement = {ct.constraint_case(),
                                ignore_enforcement
                                    ? absl::Span<const int>()
                                    : absl::MakeSpan(ct.enforcement_literal())};
    switch (ct.constraint_case()) {
      case ConstraintProto::kLinear:
        if (ignore_enforcement) {
          return absl::HashOf(type_and_enforcement,
                              absl::MakeSpan(ct.linear().vars()),
                              absl::MakeSpan(ct.linear().coeffs()),
                              absl::MakeSpan(ct.linear().domain()));
        } else {
          // We ignore domain for linear constraint, because if the rest of the
          // constraint is the same we can just intersect them.
          return absl::HashOf(type_and_enforcement,
                              absl::MakeSpan(ct.linear().vars()),
                              absl::MakeSpan(ct.linear().coeffs()));
        }
      case ConstraintProto::kBoolAnd:
        return absl::HashOf(type_and_enforcement,
                            absl::MakeSpan(ct.bool_and().literals()));
      case ConstraintProto::kBoolOr:
        return absl::HashOf(type_and_enforcement,
                            absl::MakeSpan(ct.bool_or().literals()));
      case ConstraintProto::kAtMostOne:
        return absl::HashOf(type_and_enforcement,
                            absl::MakeSpan(ct.at_most_one().literals()));
      case ConstraintProto::kExactlyOne:
        return absl::HashOf(type_and_enforcement,
                            absl::MakeSpan(ct.exactly_one().literals()));
      default:
        ConstraintProto copy = ct;
        copy.clear_name();
        if (ignore_enforcement) {
          copy.mutable_enforcement_literal()->Clear();
        }
        return absl::HashOf(copy.SerializeAsString());
    }
  }
};

struct ConstraintEqForDuplicateDetection {
  const CpModelProto* working_model;
  bool ignore_enforcement;
  ConstraintProto objective_constraint;

  ConstraintEqForDuplicateDetection(const CpModelProto* working_model,
                                    bool ignore_enforcement)
      : working_model(working_model),
        ignore_enforcement(ignore_enforcement),
        objective_constraint(
            CopyObjectiveForDuplicateDetection(working_model->objective())) {}

  bool operator()(int a, int b) const {
    if (a == b) {
      return true;
    }
    const ConstraintProto& ct_a = a == kObjectiveConstraint
                                      ? objective_constraint
                                      : working_model->constraints(a);
    const ConstraintProto& ct_b = b == kObjectiveConstraint
                                      ? objective_constraint
                                      : working_model->constraints(b);

    if (ct_a.constraint_case() != ct_b.constraint_case()) return false;
    if (!ignore_enforcement) {
      if (absl::MakeSpan(ct_a.enforcement_literal()) !=
          absl::MakeSpan(ct_b.enforcement_literal())) {
        return false;
      }
    }
    switch (ct_a.constraint_case()) {
      case ConstraintProto::kLinear:
        // As above, we ignore domain for linear constraint, because if the rest
        // of the constraint is the same we can just intersect them.
        if (ignore_enforcement && absl::MakeSpan(ct_a.linear().domain()) !=
                                      absl::MakeSpan(ct_b.linear().domain())) {
          return false;
        }
        return absl::MakeSpan(ct_a.linear().vars()) ==
                   absl::MakeSpan(ct_b.linear().vars()) &&
               absl::MakeSpan(ct_a.linear().coeffs()) ==
                   absl::MakeSpan(ct_b.linear().coeffs());
      case ConstraintProto::kBoolAnd:
        return absl::MakeSpan(ct_a.bool_and().literals()) ==
               absl::MakeSpan(ct_b.bool_and().literals());
      case ConstraintProto::kBoolOr:
        return absl::MakeSpan(ct_a.bool_or().literals()) ==
               absl::MakeSpan(ct_b.bool_or().literals());
      case ConstraintProto::kAtMostOne:
        return absl::MakeSpan(ct_a.at_most_one().literals()) ==
               absl::MakeSpan(ct_b.at_most_one().literals());
      case ConstraintProto::kExactlyOne:
        return absl::MakeSpan(ct_a.exactly_one().literals()) ==
               absl::MakeSpan(ct_b.exactly_one().literals());
      default:
        // Slow (hopefully comparably rare) path.
        ConstraintProto copy_a = ct_a;
        ConstraintProto copy_b = ct_b;
        copy_a.clear_name();
        copy_b.clear_name();
        if (ignore_enforcement) {
          copy_a.mutable_enforcement_literal()->Clear();
          copy_b.mutable_enforcement_literal()->Clear();
        }
        return copy_a.SerializeAsString() == copy_b.SerializeAsString();
    }
  }
};

}  // namespace

std::vector<std::pair<int, int>> FindDuplicateConstraints(
    const CpModelProto& model_proto, bool ignore_enforcement) {
  std::vector<std::pair<int, int>> result;

  // We use a map hash that uses the underlying constraint to compute the hash
  // and the equality for the indices.
  absl::flat_hash_map<int, int, ConstraintHashForDuplicateDetection,
                      ConstraintEqForDuplicateDetection>
      equiv_constraints(
          model_proto.constraints_size(),
          ConstraintHashForDuplicateDetection{&model_proto, ignore_enforcement},
          ConstraintEqForDuplicateDetection{&model_proto, ignore_enforcement});

  // Create a special representative for the linear objective.
  if (model_proto.has_objective() && !ignore_enforcement) {
    equiv_constraints[kObjectiveConstraint] = kObjectiveConstraint;
  }

  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const auto type = model_proto.constraints(c).constraint_case();
    if (type == ConstraintProto::CONSTRAINT_NOT_SET) continue;

    // TODO(user): we could delete duplicate identical interval, but we need
    // to make sure reference to them are updated.
    if (type == ConstraintProto::kInterval) continue;

    // Nothing we will presolve in this case.
    if (ignore_enforcement && type == ConstraintProto::kBoolAnd) continue;

    const auto [it, inserted] = equiv_constraints.insert({c, c});
    if (it->second != c) {
      // Already present!
      result.push_back({c, it->second});
    }
  }

  return result;
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

bool CpModelPresolver::IntervalConstraintEq::operator()(int a, int b) const {
  const ConstraintProto& ct_a = working_model->constraints(a);
  const ConstraintProto& ct_b = working_model->constraints(b);
  return absl::MakeSpan(ct_a.enforcement_literal()) ==
             absl::MakeSpan(ct_b.enforcement_literal()) &&
         SimpleLinearExprEq(ct_a.interval().start(), ct_b.interval().start()) &&
         SimpleLinearExprEq(ct_a.interval().size(), ct_b.interval().size()) &&
         SimpleLinearExprEq(ct_a.interval().end(), ct_b.interval().end());
}

std::size_t CpModelPresolver::IntervalConstraintHash::operator()(
    int ct_idx) const {
  const ConstraintProto& ct = working_model->constraints(ct_idx);
  return absl::HashOf(absl::MakeSpan(ct.enforcement_literal()),
                      LinearExpressionHash(ct.interval().start()),
                      LinearExpressionHash(ct.interval().size()),
                      LinearExpressionHash(ct.interval().end()));
}

}  // namespace sat
}  // namespace operations_research
