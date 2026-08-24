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

#include "ortools/sat/cp_model_presolve.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/graph_base/strongly_connected_components.h"
#include "ortools/graph_base/topologicalsorter.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_constraint_presolve.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/inclusion.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_encoding.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_model.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/sat/var_domination.h"
#include "ortools/sat/variable_expand.h"
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

namespace {

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
  amo_in_linear.AddAllAtMostOnes(context_->WorkingModel());

  int num_changes = 0;
  const int num_constraints = context_->NumConstraints();
  temp_ct_.Clear();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
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

  // TODO(user): It is possible in some corner-case that the linear constraint
  // is NOT canonicalized. This is because we might detect equivalence here and
  // we will continue with ProcessOneLinearWithAmo() in the parent loop.
  tmp_terms_.clear();
  DCHECK(temp_ct_.enforcement_literal().empty());
  temp_ct_.mutable_linear()->Clear();
  Domain non_boolean_domain(0);
  const int initial_size = ct->linear().vars().size();
  int64_t min_magnitude = kint64max;
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
    for (const auto [ref, coeff] : tmp_terms_) {
      if (coeff <= 0) {
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
    (void)constraint_presolver_->MarkConstraintAsFalse(
        ct, "linear + amo: infeasible linear constraint");
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
    if (!constraint_presolver_->PropagateDomainsInLinear(/*ct_index=*/-1,
                                                         &temp_ct_)) {
      return;
    }
    if (context_->ModelIsUnsat()) return;
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
    bool changed = false;
    if (!constraint_presolver_->CanonicalizeLinear(ct, &changed)) return;
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
        (void)constraint_presolver_->MarkConstraintAsFalse(
            ct, "linear + amo: advanced infeasible linear constraint");
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
      ConstraintProto* new_ct = context_->AddConstraint();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      for (const int lit : must_be_true) {
        new_ct->mutable_bool_and()->add_literals(lit);
        temp_set_.insert(lit);
      }
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
    constraint_presolver_->PresolveEmptyLinearConstraint(ct);
    context_->UpdateConstraintVariableUsage(ct_index);
    return;
  }

  // If the constraint is of size 1 or 2, we re-presolve it right away.
  if (initial_size != ct->linear().vars().size() &&
      constraint_presolver_->PresolveSmallLinear(ct)) {
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
        context_->UpdateRuleStats("linear: extracted at most one (max)");
      } else {
        context_->UpdateRuleStats("linear: extracted at most one (min)");
      }
      ConstraintProto* new_ct = context_->AddConstraint();
      new_ct->set_name(ct->name());
      for (const int ref : at_most_one) {
        new_ct->mutable_at_most_one()->add_literals(ref);
      }
    }
  }
}

namespace {

std::string Plural(int n, std::string_view s) {
  return n <= 1 ? absl::StrCat(n, " ", s)
                : absl::StrCat(FormatCounter(n), " ", s, "s");
};

// Add the constraint (lhs => rhs) to the given proto. The hash map lhs ->
// bool_and constraint index is used to merge implications with the same lhs.
void AddImplicationWithMerging(int lhs, int rhs, PresolveContext* context,
                               absl::flat_hash_map<int, int>* ref_to_bool_and) {
  if (ref_to_bool_and->contains(lhs)) {
    const int ct_index = (*ref_to_bool_and)[lhs];
    context->MutableConstraint(ct_index)->mutable_bool_and()->add_literals(rhs);
  } else if (ref_to_bool_and->contains(NegatedRef(rhs))) {
    const int ct_index = (*ref_to_bool_and)[NegatedRef(rhs)];
    context->MutableConstraint(ct_index)->mutable_bool_and()->add_literals(
        NegatedRef(lhs));
  } else {
    (*ref_to_bool_and)[lhs] = context->NumConstraints();
    ConstraintProto* ct = context->AddConstraint();
    ct->add_enforcement_literal(lhs);
    ct->mutable_bool_and()->add_literals(rhs);
  }
}

class SimpleDuplicateImplicationDetector {
 public:
  void AddClause2(int a, int b) {
    set_.insert({std::min(a, b), std::max(a, b)});
  }

  void AddImplication(int a, int b) { AddClause2(NegatedRef(a), b); }

  void AddAtMostOneImplicationsIfNotTooBig(absl::Span<const int> literals) {
    if (literals.empty()) return;
    const int64_t extra = literals.size() * (literals.size() - 1) / 2;
    if (set_.size() + extra > 1e7) return;

    for (int i = 0; i < literals.size(); ++i) {
      for (int j = i + 1; j < literals.size(); ++j) {
        const int min =
            std::min(NegatedRef(literals[i]), NegatedRef(literals[j]));
        const int max =
            std::max(NegatedRef(literals[i]), NegatedRef(literals[j]));
        set_.insert({min, max});
      }
    }
  }

  int64_t NumAdded() const { return set_.size(); }

  bool ContainsClause2(int a, int b) const {
    return set_.contains({std::min(a, b), std::max(a, b)});
  }

 private:
  // We encode a => b as (not(a) or b).
  absl::flat_hash_set<std::pair<int, int>> set_;
};

template <typename ClauseContainer>
void ExtractClausesToContext(absl::Span<const int> amo_or_exo_still_present,
                             absl::Span<const int> index_mapping,
                             const ClauseContainer& container,
                             PresolveContext* context) {
  // Avoid adding bool_and already encoded in amo or exo.
  // The algo here is fast but don't work if there is too many amo/exo, so
  // we have a limit in place.
  SimpleDuplicateImplicationDetector already_there;
  for (const int c : amo_or_exo_still_present) {
    const ConstraintProto& ct = context->Constraint(c);
    CHECK(ct.enforcement_literal().empty());
    if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      already_there.AddAtMostOneImplicationsIfNotTooBig(
          ct.exactly_one().literals());
    } else {
      CHECK_EQ(ct.constraint_case(), ConstraintProto::kAtMostOne);
      already_there.AddAtMostOneImplicationsIfNotTooBig(
          ct.at_most_one().literals());
    }
  }

  // We regroup the "implication" into bool_and to have a more concise proto and
  // also for nicer information about the number of binary clauses.
  //
  // Important: however, we do not do that for the model used during postsolving
  // since the order of the constraints might be important there depending on
  // how we perform the postsolve.
  absl::flat_hash_map<int, int> ref_to_bool_and;
  for (int i = 0; i < container.NumClauses(); ++i) {
    const auto& clause = container.Clause(i);
    if (clause.empty()) continue;

    // bool_and.
    //
    // TODO(user): Be smarter in how we regroup clause of size 2?
    if (clause.size() == 2) {
      const int var_a = index_mapping[clause[0].Variable().value()];
      const int var_b = index_mapping[clause[1].Variable().value()];
      const int ref_a = clause[0].IsPositive() ? var_a : NegatedRef(var_a);
      const int ref_b = clause[1].IsPositive() ? var_b : NegatedRef(var_b);
      if (already_there.ContainsClause2(ref_a, ref_b)) {
        context->UpdateRuleStats("bool_and: remove since already in amo");
        continue;
      }
      AddImplicationWithMerging(NegatedRef(ref_a), ref_b, context,
                                &ref_to_bool_and);
      continue;
    }

    // bool_or.
    ConstraintProto* ct = context->AddConstraint();
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

  DCHECK(context->ConstraintVariableUsageIsConsistent());
}

void ExtractClausesToMappingModelProto(absl::Span<const int> index_mapping,
                                       const SatPostsolver& container,
                                       CpModelProto* proto) {
  const std::string debug_name =
      absl::GetFlag(FLAGS_cp_model_debug_postsolve) ? "sat_postsolver" : "";

  // We regroup the "implication" into bool_and to have a more concise proto and
  // also for nicer information about the number of binary clauses.
  //
  // Important: however, we do not do that for the model used during postsolving
  // since the order of the constraints might be important there depending on
  // how we perform the postsolve.
  for (int i = 0; i < container.NumClauses(); ++i) {
    const auto& clause = container.Clause(i);
    if (clause.empty()) continue;
    ConstraintProto* ct = proto->add_constraints();
    if (!debug_name.empty()) {
      ct->set_name(debug_name);
    }
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

// TODO(user): It is probably more efficient to keep all the bool_and in a
// global place during all the presolve, and just output them at the end
// rather than modifying more than once the proto.
void CpModelPresolver::ConvertToBoolAnd() {
  absl::flat_hash_map<int, int> ref_to_bool_and;
  const int num_constraints = context_->NumConstraints();
  std::vector<int> to_remove;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
    if (HasEnforcementLiteral(ct)) continue;

    if (ct.constraint_case() == ConstraintProto::kBoolOr &&
        ct.bool_or().literals().size() == 2) {
      AddImplicationWithMerging(NegatedRef(ct.bool_or().literals(0)),
                                ct.bool_or().literals(1), context_,
                                &ref_to_bool_and);
      to_remove.push_back(c);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::kAtMostOne &&
        ct.at_most_one().literals().size() == 2) {
      AddImplicationWithMerging(ct.at_most_one().literals(0),
                                NegatedRef(ct.at_most_one().literals(1)),
                                context_, &ref_to_bool_and);
      to_remove.push_back(c);
      continue;
    }
  }

  DCHECK(context_->ConstraintVariableUsageIsConsistent());
  for (const int c : to_remove) {
    ConstraintProto* ct = context_->MutableConstraint(c);
    CHECK(constraint_presolver_->RemoveConstraint(ct));
    context_->UpdateConstraintVariableUsage(c);
  }
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
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
      const ConstraintProto& ct = context_->Constraint(c);
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
          auto* mutable_ct = context_->MutableConstraint(c);
          mutable_ct->Clear();
          mutable_ct->add_enforcement_literal(decision_ref);
          mutable_ct->mutable_bool_and()->add_literals(NegatedRef(false_ref));
          context_->UpdateRuleStats(
              "probing: reduced enforced constraint to implication");
          to_update.push_back(c);
          continue;
        }

        if (simplification_possible) {
          int new_size = 0;
          auto* mutable_enforcements =
              context_->MutableConstraint(c)->mutable_enforcement_literal();
          for (const int ref : ct.enforcement_literal()) {
            if (PositiveRef(ref) != decision_var &&
                assignment.LiteralIsTrue(mapping->Literal(ref))) {
              continue;
            }
            mutable_enforcements->Set(new_size++, ref);
          }
          mutable_enforcements->Truncate(new_size);
          context_->UpdateRuleStats("probing: simplified enforcement list");
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
            context_->MutableConstraint(c)->mutable_bool_or();
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
            context_->MutableConstraint(c)->mutable_bool_or();
        for (const int ref : ct.bool_or().literals()) {
          if (PositiveRef(ref) != decision_var &&
              assignment.LiteralIsFalse(mapping->Literal(ref))) {
            continue;
          }
          mutable_bool_or->set_literals(new_size++, ref);
        }
        mutable_bool_or->mutable_literals()->Truncate(new_size);
        context_->UpdateRuleStats("probing: simplified clauses");
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

  for (const auto& [expr, ub] : model.GetOrCreate<RootLevelLinear2Bounds>()
                                    ->GetSortedNonTrivialUpperBounds()) {
    if (expr.vars[0] == kNoIntegerVariable ||
        expr.vars[1] == kNoIntegerVariable) {
      continue;
    }
    const IntegerVariable var0 = PositiveVariable(expr.vars[0]);
    const IntegerVariable var1 = PositiveVariable(expr.vars[1]);
    const int proto_var0 = mapping->GetProtoVariableFromIntegerVariable(var0);
    const int proto_var1 = mapping->GetProtoVariableFromIntegerVariable(var1);
    if (proto_var0 < 0 || proto_var1 < 0) continue;
    const int64_t coeff0 = VariableIsPositive(expr.vars[0])
                               ? expr.coeffs[0].value()
                               : -expr.coeffs[0].value();
    const int64_t coeff1 = VariableIsPositive(expr.vars[1])
                               ? expr.coeffs[1].value()
                               : -expr.coeffs[1].value();
    constraint_presolver_->known_linear2().Add(
        GetLinearExpression2FromProto(proto_var0, coeff0, proto_var1, coeff1),
        kMinIntegerValue, ub);
  }

  probing_timer->AddCounter("probed", prober->num_decisions());
  probing_timer->AddToWork(
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  if (sat_solver->ModelIsUnsat() || !implication_graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat("during probing");
  }

  time_limit_->ResetHistory();

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
  const int num_variables = context_->NumVariables();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  for (int var = 0; var < num_variables; ++var) {
    // Restrict IntegerVariable domain.
    // Note that Boolean are already dealt with above.
    if (!mapping->IsBoolean(var)) {
      bool changed = false;
      if (!context_->IntersectDomainWith(
              var, integer_trail->LevelZeroDomain(mapping->Integer(var)),
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
      if (!context_->StoreBooleanEqualityRelation(
              var, r.IsPositive() ? r_var : NegatedRef(r_var))) {
        return;
      }
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
    const int num_constraints = context_->NumConstraints();
    for (int c = 0; c < num_constraints; ++c) {
      ConstraintProto* ct = context_->MutableConstraint(c);
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
      ConstraintProto* ct = context_->AddConstraint();
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
      //
      // Tricky: note that it is important to not use dual reduction here as not
      // all constraints are in the proto during the loop.
      constraint_presolver_->PresolveAtMostOne(ct,
                                               /*use_dual_reduction=*/false);
    }
    if (num_new_cliques != num_old_cliques) {
      context_->UpdateRuleStats("at_most_one: transformed into max clique");
    }

    if (num_old_cliques != num_new_cliques ||
        num_literals_before != num_literals_after) {
      timer.AddMessage(
          absl::StrCat("Merged ", Plural(num_old_cliques, "constraint"),
                       " with ", Plural(num_literals_before, "literal"),
                       " into ", Plural(num_new_cliques, "constraint"),
                       " with ", Plural(num_literals_after, "literal")));
    }
  }
  DCHECK(context_->ConstraintVariableUsageIsConsistent());
}

namespace {

bool FixFromAssignment(const VariablesAssignment& assignment,
                       absl::Span<const int> var_mapping,
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
  const int total_num_vars = context_->NumVariables();
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
  local_model.GetOrCreate<ModelSharedTimeLimit>()->DisableStop();
  *local_model.GetOrCreate<SatParameters>() = context_->params();
  auto* sat_solver = local_model.GetOrCreate<SatSolver>();
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
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
  const bool load_amo = context_->params().load_at_most_ones_in_sat_presolve();
  std::vector<int> amo_or_exo_still_present;
  for (int c = 0; c < context_->NumConstraints(); ++c) {
    const ConstraintProto& ct = context_->Constraint(c);

    if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      ++num_removed_constraints;
      clause.clear();
      for (const int ref : ct.bool_or().literals()) {
        clause.push_back(convert(ref));
      }
      for (const int ref : ct.enforcement_literal()) {
        clause.push_back(convert(ref).Negated());
      }
      sat_solver->AddProblemClause(clause);

      context_->ClearConstraint(c);
      context_->UpdateConstraintVariableUsage(c);
      continue;
    }

    // TODO(user): we should probably make sure we don't have empty amo.
    if (ct.constraint_case() == ConstraintProto::kAtMostOne &&
        ct.enforcement_literal().empty() &&
        !ct.at_most_one().literals().empty()) {
      if (load_amo) {
        clause.clear();
        for (const int ref : ct.at_most_one().literals()) {
          clause.push_back(convert(ref));
        }
        if (!graph->AddAtMostOne(clause)) return false;

        ++num_removed_constraints;
        context_->ClearConstraint(c);
        context_->UpdateConstraintVariableUsage(c);
        continue;
      } else {
        amo_or_exo_still_present.push_back(c);
      }
    }

    if (ct.constraint_case() == ConstraintProto::kExactlyOne &&
        ct.enforcement_literal().empty()) {
      if (load_amo) {
        clause.clear();
        for (const int ref : ct.exactly_one().literals()) {
          clause.push_back(convert(ref));
        }

        // We load it as two constraints.
        if (!graph->AddAtMostOne(clause)) return false;
        sat_solver->AddProblemClause(clause);

        ++num_removed_constraints;
        context_->ClearConstraint(c);
        context_->UpdateConstraintVariableUsage(c);
        continue;
      } else {
        amo_or_exo_still_present.push_back(c);
      }
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
        sat_solver->AddProblemClause(clause);
      }

      context_->ClearConstraint(c);
      context_->UpdateConstraintVariableUsage(c);
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
  SatParameters sat_params = context_->params();
  if (sat_params.debug_postsolve_with_full_solver() ||
      sat_params.fill_tightened_domains_in_response()) {
    sat_params.set_presolve_blocked_clause(false);
  }

  // This option is only supported by the custom postsolve code.
  if (!sat_params.debug_postsolve_with_full_solver()) {
    sat_params.set_filter_sat_postsolve_clauses(true);
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
    options.use_equivalence_sat_sweeping =
        context_->params().inprocessing_use_sat_sweeping();

    auto* inprocessing = local_model.GetOrCreate<Inprocessing>();
    inprocessing->ProvideLogger(logger_);
    if (!inprocessing->PresolveLoop(options)) return false;
    for (const auto& c : local_model.GetOrCreate<PostsolveClauses>()->clauses) {
      sat_postsolver.Add(c[0], c);
    }

    // Probe + find equivalent literals.
    // TODO(user): Use a derived time limit in the probing phase.
    ProbeAndFindEquivalentLiteral(sat_solver, &sat_postsolver, &equiv_map,
                                  logger_);

    if (sat_solver->ModelIsUnsat()) return false;
  } else {
    // TODO(user): BVA takes time and does not seems to help on the minizinc
    // benchmarks. So we currently disable it, except if we are on a pure-SAT
    // problem, where we follow the default (true) or the user specified value.
    sat_params.set_presolve_use_bva(false);
  }

  // Disable BVA if we want to keep the symmetries.
  //
  // TODO(user): We could still do it, we just need to do in a symmetric way
  // and also update the generators to take into account the new variables. This
  // do not seems that easy.
  if (context_->params().keep_symmetry_in_presolve()) {
    sat_params.set_presolve_use_bva(false);
  }

  // Update the time limit of the initial propagation.
  if (!sat_solver->ResetToLevelZero()) return false;
  time_limit_->AdvanceDeterministicTime(
      local_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());

  // The "old" SAT presolve do not read at_most_ones.
  // So extract them back from the sat_solver, and only continue with submodel.
  graph->CleanupAllRemovedAndFixedVariables();
  graph->ResetAtMostOneIterator();
  while (true) {
    absl::Span<const Literal> amo = graph->NextAtMostOne();
    if (amo.empty()) break;

    // Re-add the amo to the proto.
    ConstraintProto* ct = context_->AddConstraint();
    ct->mutable_at_most_one()->mutable_literals()->Reserve(amo.size());
    for (Literal l : amo) {
      // TODO(user): ProbeAndFindEquivalentLiteral() do not register newly
      // found equivalence to the BinaryImplicationGraph, It should so that
      // we already get cleaned AMO here.
      if (l.Index() < equiv_map.size()) {
        l = Literal(equiv_map[l]);
      }

      const int var = new_to_old_index[l.Variable().value()];
      ct->mutable_at_most_one()->add_literals(l.IsPositive() ? var
                                                             : NegatedRef(var));

      // These cannot be removed anymore by the old SAT presolver.
      can_be_removed[l.Variable().value()] = false;
    }
  }

  // Apply the "old" SAT presolve.
  SatPresolver sat_presolver(&sat_postsolver, logger_);
  sat_presolver.SetNumVariables(num_variables);
  if (!equiv_map.empty()) {
    sat_presolver.SetEquivalentLiteralMapping(equiv_map);
  }
  sat_presolver.SetTimeLimit(time_limit_);
  sat_presolver.SetParameters(sat_params);

  // Load in the presolver.
  // Register the fixed variables with the postsolver.
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    sat_postsolver.FixVariable(sat_solver->LiteralTrail()[i]);
  }
  if (!sat_solver->ExtractClauses(&sat_presolver)) return false;

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
      new_to_old_index.push_back(context_->NumVariables());
      context_->NewBoolVar("BVA");
    }

    // TODO(user): NewBoolVar() already call this each time.
    // Provide a batch interface? or we don't care.
    context_->InitializeNewDomains();
  }

  // Fix variables if any.
  if (!FixFromAssignment(sat_postsolver.assignment(), new_to_old_index,
                         context_)) {
    return false;
  }

  // Add the presolver clauses back into the model.
  ExtractClausesToContext(amo_or_exo_still_present, new_to_old_index,
                          sat_presolver, context_);

  // We mark as removed any variables removed by the pure SAT presolve.
  // This is mainly to discover or avoid bug as we might have stale entries
  // in our encoding hash-map for instance.
  for (int i = 0; i < num_variables; ++i) {
    const int var = new_to_old_index[i];
    if (context_->VarToConstraints(var).empty()) {
      context_->MarkVariableAsRemoved(var);
    }
  }

  // Add the sat_postsolver clauses to mapping_model.
  ExtractClausesToMappingModelProto(new_to_old_index, sat_postsolver,
                                    context_->mapping_model);
  return true;
}

namespace {
class BasicClauseContainer {
 public:
  void SetNumVariables(int /*num_variables*/) {}
  void AddBinaryClause(Literal a, Literal b) { AddClause({a, b}); }
  void AddClause(absl::Span<const Literal> clause) { clauses_.Add(clause); }

  int NumClauses() const { return clauses_.size(); }
  absl::Span<const Literal> Clause(int index) const { return clauses_[index]; }

 private:
  CompactVectorVector<int, Literal> clauses_;
};
}  // namespace

bool CpModelPresolver::PresolvePureSatProblem() {
  if (context_->ModelIsUnsat()) return true;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return true;

  Model local_model;
  LratProofHandler* lrat_proof_handler = context_->lrat_proof_handler;
  if (lrat_proof_handler != nullptr) {
    local_model.Register<LratProofHandler>(lrat_proof_handler);
  }
  local_model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit_);
  auto* sat_solver = local_model.GetOrCreate<SatSolver>();
  const int num_variables = context_->NumVariables();
  sat_solver->SetNumVariables(num_variables);

  std::vector<int> new_to_old_index;
  new_to_old_index.reserve(num_variables);
  for (int i = 0; i < num_variables; ++i) {
    new_to_old_index.push_back(i);
  }

  // 1) Load the problem in the SAT solver.
  auto ref_to_literal = [](int ref) {
    return Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref));
  };
  std::vector<Literal> clause;
  for (int i = 0; i < context_->NumConstraints(); ++i) {
    const ConstraintProto& ct = context_->Constraint(i);
    CHECK_EQ(ct.constraint_case(), ConstraintProto::kBoolOr);
    clause.clear();
    for (const int ref : ct.enforcement_literal()) {
      clause.push_back(ref_to_literal(ref).Negated());
    }
    for (const int ref : ct.bool_or().literals()) {
      clause.push_back(ref_to_literal(ref));
    }
    sat_solver->AddProblemClause(clause, /*one_based_cnf_index=*/i + 1);
    context_->ClearConstraint(i);
    context_->UpdateConstraintVariableUsage(i);
  }
  if (sat_solver->ModelIsUnsat()) return false;

  // Some problems are formulated in such a way that our SAT heuristics
  // simply works without conflict. Get them out of the way first because it
  // is possible that the presolve lose this "lucky" ordering. This is in
  // particular the case on the SAT14.crafted.complete-xxx-... problems.
  if (!LookForTrivialSatSolution(/*deterministic_time_limit=*/1.0, &local_model,
                                 logger_)) {
    return false;
  }
  if (sat_solver->LiteralTrail().Index() == num_variables) {
    // Problem solved! We should be able to assign the solution.
    CHECK(FixFromAssignment(sat_solver->Assignment(), new_to_old_index,
                            context_));
    return true;
  }

  // 2) Do a few rounds of inprocessing.
  SatPresolveOptions options;
  options.log_info = true;  // log_info;
  options.extract_binary_clauses_in_probing = false;
  options.use_transitive_reduction = false;
  options.deterministic_time_limit =
      context_->params().presolve_probing_deterministic_time_limit();
  options.use_equivalence_sat_sweeping =
      context_->params().inprocessing_use_sat_sweeping();

  auto* inprocessing = local_model.GetOrCreate<Inprocessing>();
  inprocessing->ProvideLogger(logger_);
  // TODO(user): re-index the variables to remove the unused ones between
  // each iteration?
  for (int i = 0; i < context_->params().max_presolve_iterations(); ++i) {
    if (time_limit_->LimitReached()) break;
    context_->UpdateRuleStats("presolve: iteration");
    if (!inprocessing->PresolveLoop(options)) return false;
    time_limit_->AdvanceDeterministicTime(
        local_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime());
  }

  // 3) Extract the new clauses.
  SatPostsolver sat_postsolver(num_variables);
  for (const auto& c : local_model.GetOrCreate<PostsolveClauses>()->clauses) {
    sat_postsolver.Add(c[0], c);
  }
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    sat_postsolver.FixVariable(sat_solver->LiteralTrail()[i]);
  }
  if (!FixFromAssignment(sat_postsolver.assignment(), new_to_old_index,
                         context_)) {
    return false;
  }
  // TODO(user): can we improve ExtractClausesToContext() to avoid the
  // intermediate container?
  BasicClauseContainer clauses_container;
  if (!sat_solver->ExtractClauses(&clauses_container)) return false;
  ExtractClausesToContext({}, new_to_old_index, clauses_container, context_);
  ExtractClausesToMappingModelProto(new_to_old_index, sat_postsolver,
                                    context_->mapping_model);

  // We mark as removed any variables removed by the pure SAT presolve.
  // This is mainly to discover or avoid bug as we might have stale entries
  // in our encoding hash-map for instance.
  for (int var = 0; var < num_variables; ++var) {
    if (context_->VarToConstraints(var).empty()) {
      context_->MarkVariableAsRemoved(var);
    }
  }
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
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
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
      const ConstraintProto& ct = context_->Constraint(c);
      const int num_terms = ct.exactly_one().literals().size();
      if (num_terms <= 1) continue;
      int64_t min_obj = kint64max;
      int64_t second_min = kint64max;
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

bool CpModelPresolver::PropagateObjective() {
  if (!context_->WorkingModel().has_objective()) return true;
  if (context_->ModelIsUnsat()) return false;
  context_->WriteObjectiveToProto();

  int64_t min_activity = 0;
  int64_t max_variation = 0;
  const CpObjectiveProto& objective = context_->WorkingModel().objective();
  const int num_terms = objective.vars().size();
  for (int i = 0; i < num_terms; ++i) {
    const int var = objective.vars(i);
    const int64_t coeff = objective.coeffs(i);
    CHECK(RefIsPositive(var));
    CHECK_NE(coeff, 0);

    const int64_t domain_min = context_->MinOf(var);
    const int64_t domain_max = context_->MaxOf(var);
    if (coeff > 0) {
      min_activity += coeff * domain_min;
    } else {
      min_activity += coeff * domain_max;
    }
    const int64_t variation = std::abs(coeff) * (domain_max - domain_min);
    max_variation = std::max(max_variation, variation);
  }

  // Infeasible ?
  const int64_t slack =
      CapSub(ReadDomainFromProto(objective).Max(), min_activity);
  if (slack < 0) {
    return context_->NotifyThatModelIsUnsat(
        "infeasible while propagating objective");
  }

  // No propagation ?
  if (max_variation <= slack) return true;

  int num_propagations = 0;
  for (int i = 0; i < num_terms; ++i) {
    const int var = objective.vars(i);
    const int64_t coeff = objective.coeffs(i);
    const int64_t domain_min = context_->MinOf(var);
    const int64_t domain_max = context_->MaxOf(var);

    const int64_t new_diff = slack / std::abs(coeff);
    if (new_diff >= domain_max - domain_min) continue;

    ++num_propagations;
    if (coeff > 0) {
      if (!context_->IntersectDomainWith(
              var, Domain(domain_min, domain_min + new_diff))) {
        return false;
      }
    } else {
      if (!context_->IntersectDomainWith(
              var, Domain(domain_max - new_diff, domain_max))) {
        return false;
      }
    }
  }
  CHECK_GT(num_propagations, 0);

  context_->UpdateRuleStats("objective: restricted var domains by propagation",
                            num_propagations);
  return true;
}

namespace {

bool IsLinearEqualityConstraint(const ConstraintProto& ct) {
  return ct.constraint_case() == ConstraintProto::kLinear &&
         ct.linear().domain().size() == 2 &&
         ct.linear().domain(0) == ct.linear().domain(1) &&
         ct.enforcement_literal().empty();
}

}  // namespace

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

  const int num_variables = context_->NumVariables();
  const int num_constraints = context_->NumConstraints();

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

    const ConstraintProto& ct = context_->Constraint(c);
    if (!ct.enforcement_literal().empty()) continue;

    // Deal with exactly one.
    // An exactly one is always tight on the upper bound of one term.
    //
    // Note(user): This code assume there is no fixed variable in the exactly
    // one. We thus make sure the constraint is re-presolved if for some reason
    // we didn't reach the fixed point before calling this code.
    if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
      if (constraint_presolver_->PresolveExactlyOne(
              context_->MutableConstraint(c))) {
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
    if ((obj_coeff > 0) == to_lb) {
      const ConstraintProto& ct = context_->Constraint(index_to_best_c[index]);
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

bool CpModelPresolver::MergeCliqueConstraintsHelper(
    std::vector<std::vector<Literal>>& cliques, std::string_view entry_name,
    PresolveTimer& timer) {
  if (cliques.empty()) return false;  // Nothing has changed.
  const int num_constraints = context_->NumConstraints();
  int old_num_clique_constraints = cliques.size();
  int old_num_entries = 0;
  for (const std::vector<Literal>& clique : cliques) {
    old_num_entries += clique.size();
  }

  // We reuse the max-clique code from sat.
  Model local_model;
  local_model.GetOrCreate<Trail>()->Resize(num_constraints);
  local_model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(time_limit_);
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(num_constraints);
  for (const std::vector<Literal>& clique : cliques) {
    // All variables at false is always a valid solution of the local model,
    // so this should never return UNSAT.
    CHECK(graph->AddAtMostOne(clique));
  }

  // We shouldn't be UNSAT here.
  CHECK(graph->DetectEquivalences());
  CHECK(graph->TransformIntoMaxCliques(
      &cliques,
      SafeDoubleToInt64(context_->params().merge_no_overlap_work_limit())));
  time_limit_->ResetHistory();

  // Update the number of constraints and entries after the max-clique.
  int new_num_clique_constraints = 0;
  int new_num_entries = 0;
  for (const std::vector<Literal>& clique : cliques) {
    if (clique.empty()) continue;
    new_num_clique_constraints++;
    new_num_entries += clique.size();
  }

  if (old_num_clique_constraints != new_num_clique_constraints ||
      old_num_entries != new_num_entries) {
    timer.AddMessage(absl::StrCat(
        "Merged ", Plural(old_num_clique_constraints, "constraint"), " with ",
        Plural(old_num_entries, entry_name), " into ",
        Plural(new_num_clique_constraints, "constraint"), " with ",
        Plural(new_num_entries, entry_name)));
    return true;
  }

  return false;  // Nothing has changed.
}

bool CpModelPresolver::MergeNoOverlapConstraints() {
  PresolveTimer timer("MergeNoOverlap", logger_, time_limit_);
  if (context_->ModelIsUnsat()) return false;
  if (time_limit_->LimitReached()) return true;

  const int num_constraints = context_->NumConstraints();
  // Extract the no-overlap constraints with no enforcement literals.
  // TODO(user): generalize this to merge constraints with the same
  // enforcement literals?
  std::vector<int> disjunctive_index;
  std::vector<std::vector<Literal>> cliques;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
    if (ct.constraint_case() != ConstraintProto::kNoOverlap) continue;
    if (HasEnforcementLiteral(ct)) continue;
    std::vector<Literal> clique;
    for (const int i : ct.no_overlap().intervals()) {
      clique.push_back(Literal(BooleanVariable(i), true));
    }
    cliques.push_back(clique);
    disjunctive_index.push_back(c);
  }

  if (!MergeCliqueConstraintsHelper(cliques, "interval", timer)) {
    return true;  // Nothing to do, and model is SAT.
  }

  // Remove previous no_overlap constraints and add the new recomputed ones.
  for (int i = 0; i < cliques.size(); ++i) {
    const int ct_index = disjunctive_index[i];
    if (constraint_presolver_->RemoveConstraint(
            context_->MutableConstraint(ct_index))) {
      context_->UpdateConstraintVariableUsage(ct_index);
    }
  }
  for (int i = 0; i < cliques.size(); ++i) {
    if (cliques[i].empty()) continue;
    ConstraintProto* ct = context_->AddConstraint();
    for (const Literal l : cliques[i]) {
      CHECK(l.IsPositive());
      ct->mutable_no_overlap()->add_intervals(l.Variable().value());
    }
  }
  context_->UpdateRuleStats("no_overlap: merged constraints");
  return true;
}

bool CpModelPresolver::MergeNoOverlap2DConstraints() {
  PresolveTimer timer("MergeNoOverlap2D", logger_, time_limit_);
  if (context_->ModelIsUnsat()) return false;
  if (time_limit_->LimitReached()) return true;

  const int num_constraints = context_->NumConstraints();
  // Extract the no-overlap constraints with no enforcement literals.
  // TODO(user): generalize this to merge constraints with the same
  // enforcement literals?
  std::vector<int> no_overlap2d_index;
  std::vector<std::vector<Literal>> cliques;
  absl::flat_hash_map<std::pair<int, int>, int> rectangle_to_index;
  std::vector<std::pair<int, int>> index_to_rectangle;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
    if (ct.constraint_case() != ConstraintProto::kNoOverlap2D) continue;
    if (HasEnforcementLiteral(ct)) continue;
    std::vector<Literal> clique;
    for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
      const std::pair<int, int> rect = {ct.no_overlap_2d().x_intervals(i),
                                        ct.no_overlap_2d().y_intervals(i)};
      const auto [it, inserted] =
          rectangle_to_index.insert({rect, rectangle_to_index.size()});
      if (inserted) index_to_rectangle.push_back(rect);
      clique.push_back(Literal(BooleanVariable(it->second), true));
    }
    cliques.push_back(clique);
    no_overlap2d_index.push_back(c);
  }

  if (!MergeCliqueConstraintsHelper(cliques, "rectangle", timer)) {
    return true;  // Nothing to do, and model is SAT.
  }

  // Remove previous no_overlap constraints and add the new recomputed ones.
  for (int i = 0; i < cliques.size(); ++i) {
    const int ct_index = no_overlap2d_index[i];
    if (constraint_presolver_->RemoveConstraint(
            context_->MutableConstraint(ct_index))) {
      context_->UpdateConstraintVariableUsage(ct_index);
    }
  }
  for (int i = 0; i < cliques.size(); ++i) {
    if (cliques[i].empty()) continue;
    ConstraintProto* ct = context_->AddConstraint();
    for (const Literal l : cliques[i]) {
      CHECK(l.IsPositive());
      const std::pair<int, int> rect = index_to_rectangle[l.Variable().value()];
      ct->mutable_no_overlap_2d()->add_x_intervals(rect.first);
      ct->mutable_no_overlap_2d()->add_y_intervals(rect.second);
    }
  }
  context_->UpdateRuleStats("no_overlap_2d: merged constraints");
  return true;
}

void CpModelPresolver::DetectEncodedComplexDomains(PresolveContext* context) {
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);
  if (context->ModelIsUnsat()) return;
  if (time_limit_->LimitReached()) return;

  std::vector<VariableEncodingLocalModel> local_models =
      CreateVariableEncodingLocalModels(context);
  for (VariableEncodingLocalModel& local_model : local_models) {
    if (time_limit_->LimitReached()) return;
    if (context->ModelIsUnsat()) return;
    if (!DetectAllEncodedComplexDomain(context, local_model)) {
      return;
    }
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
  const int num_constraints = context_->NumConstraints();

  // Extract the bool_and and at_most_one constraints.
  // TODO(user): use probing info?
  std::vector<std::vector<Literal>> cliques;

  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
    if (ct->constraint_case() == ConstraintProto::kAtMostOne) {
      std::vector<Literal> clique;
      for (const int ref : ct->at_most_one().literals()) {
        clique.push_back(convert(ref));
      }
      cliques.push_back(clique);
      if (constraint_presolver_->RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    } else if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      if (ct->enforcement_literal().size() != 1) continue;
      const Literal enforcement = convert(ct->enforcement_literal(0));
      for (const int ref : ct->bool_and().literals()) {
        if (ref == ct->enforcement_literal(0)) continue;
        cliques.push_back({enforcement, convert(ref).Negated()});
      }
      if (constraint_presolver_->RemoveConstraint(ct)) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }

  int64_t num_literals_before = 0;
  const int num_old_cliques = cliques.size();

  // We reuse the max-clique code from sat.
  Model local_model;
  const int num_variables = context_->NumVariables();
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
      if (!context_->StoreBooleanEqualityRelation(
              var, r.IsPositive() ? r.Variable().value()
                                  : NegatedRef(r.Variable().value()))) {
        return;
      }
    }
  }

  int num_new_cliques = 0;
  int64_t num_literals_after = 0;
  for (const std::vector<Literal>& clique : cliques) {
    if (clique.empty()) continue;
    num_new_cliques++;
    num_literals_after += clique.size();
    ConstraintProto* ct = context_->AddConstraint();
    for (const Literal literal : clique) {
      if (literal.IsPositive()) {
        ct->mutable_at_most_one()->add_literals(literal.Variable().value());
      } else {
        ct->mutable_at_most_one()->add_literals(
            NegatedRef(literal.Variable().value()));
      }
    }

    // Make sure we do not have duplicate variable reference.
    //
    // Tricky: note that it is important to not use dual reduction here as not
    // all constraints are in the proto during the loop.
    constraint_presolver_->PresolveAtMostOne(ct, /*use_dual_reduction=*/false);
  }
  if (num_new_cliques != num_old_cliques) {
    context_->UpdateRuleStats("at_most_one: transformed into max clique");
  }

  if (num_old_cliques != num_new_cliques ||
      num_literals_before != num_literals_after) {
    SOLVER_LOG(logger_, "[MaxClique] Merged ", num_old_cliques, " with ",
               num_literals_before, " literals) into ", num_new_cliques, "(",
               num_literals_after, " literals) at_most_ones.");
  }
}

void CpModelPresolver::SplitNoOverlapAndCumulativeConstraints() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);
  std::vector<int> all_no_overlap_intervals;
  std::vector<int> all_no_overlap_or_cumulative_constraints;
  for (int c = 0; c < context_->NumConstraints(); ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
    if (ct.constraint_case() == ConstraintProto::kNoOverlap ||
        ct.constraint_case() == ConstraintProto::kCumulative) {
      all_no_overlap_or_cumulative_constraints.push_back(c);
      const google::protobuf::RepeatedField<int32_t>& indices =
          (ct.constraint_case() == ConstraintProto::kNoOverlap)
              ? ct.no_overlap().intervals()
              : ct.cumulative().intervals();

      for (const int interval : indices) {
        all_no_overlap_intervals.push_back(interval);
      }
    }
  }

  if (all_no_overlap_intervals.empty()) return;

  int num_split_constraints = 0;
  gtl::STLSortAndRemoveDuplicates(&all_no_overlap_intervals);
  const std::vector<std::pair<int, int>> precedences =
      DetectIntervalPrecedences(context_->WorkingModel(),
                                constraint_presolver_->known_model_linear2(),
                                all_no_overlap_intervals);
  std::vector<IndexedInterval> intervals;
  for (const int c : all_no_overlap_or_cumulative_constraints) {
    intervals.clear();
    const ConstraintProto& ct = context_->Constraint(c);
    const bool is_no_overlap =
        (ct.constraint_case() == ConstraintProto::kNoOverlap);

    const google::protobuf::RepeatedField<int32_t>& interval_indices =
        is_no_overlap ? ct.no_overlap().intervals()
                      : ct.cumulative().intervals();
    intervals.reserve(interval_indices.size());
    bool has_complex_enforced_interval = false;
    for (const int interval : interval_indices) {
      const ConstraintProto& interval_ct = context_->Constraint(interval);
      if (!interval_ct.enforcement_literal().empty()) {
        const LinearExpressionProto& start = interval_ct.interval().start();
        const LinearExpressionProto& end = interval_ct.interval().end();
        if (start.vars().size() != 1 || end.vars().size() != 1 ||
            start.coeffs(0) != end.coeffs(0) || start.vars(0) != end.vars(0) ||
            start.offset() > end.offset()) {
          has_complex_enforced_interval = true;
        }
      }
      intervals.push_back(IndexedInterval{
          .index = interval,
          .start = context_->MinOf(interval_ct.interval().start()),
          .end = context_->MaxOf(interval_ct.interval().end()),
      });
    }

    // TODO(user): Handle the case of non-trivial intervals with enforcement.
    // The problem is that the code below implicitly assume that for an
    // interval, start <= end, which is not necessarily true if the interval is
    // not performed.
    if (has_complex_enforced_interval) continue;

    const auto components =
        IntervalsNonOverlappingComponents(intervals, precedences);

    if (components.size() == 1 && components[0].size() == intervals.size()) {
      continue;
    }

    const ConstraintProto orig_ct = ct;

    absl::flat_hash_map<int, int> interval_to_orig_index;
    if (!is_no_overlap) {
      interval_to_orig_index.reserve(interval_indices.size());
      for (int i = 0; i < interval_indices.size(); ++i) {
        auto [it, inserted] =
            interval_to_orig_index.insert({interval_indices[i], i});
        if (!inserted) {
          context_->UpdateRuleStats(
              "TODO: ignored duplicate interval in "
              "cumulative constraint");
          return;
        }
      }
    }

    context_->UpdateRuleStats(
        absl::StrCat((is_no_overlap ? "no_overlap" : "cumulative"),
                     ": split using precedences"));
    ++num_split_constraints;

    if (!constraint_presolver_->RemoveConstraint(
            context_->MutableConstraint(c))) {
      return;
    }
    context_->UpdateConstraintVariableUsage(c);
    for (const auto& component : components) {
      if (is_no_overlap && component.size() <= 1) {
        continue;
      }
      ConstraintProto* new_ct = context_->AddConstraint();
      *new_ct->mutable_enforcement_literal() = orig_ct.enforcement_literal();
      if (is_no_overlap) {
        for (const int interval : component) {
          new_ct->mutable_no_overlap()->mutable_intervals()->Add(interval);
        }
      } else {
        *new_ct->mutable_cumulative()->mutable_capacity() =
            orig_ct.cumulative().capacity();

        for (const int interval : component) {
          new_ct->mutable_cumulative()->mutable_intervals()->Add(interval);

          const auto it = interval_to_orig_index.find(interval);
          DCHECK(it != interval_to_orig_index.end());
          *new_ct->mutable_cumulative()->add_demands() =
              orig_ct.cumulative().demands(it->second);
        }
      }
    }
  }
  timer.AddCounter("num_split_constraints", num_split_constraints);
}

void CpModelPresolver::TransformClausesToExactlyOne() {
  if (context_->ModelIsUnsat()) return;
  if (!context_->params().find_clauses_that_are_exactly_one()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  auto convert = [](int ref) {
    if (RefIsPositive(ref)) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };
  const int num_constraints = context_->NumConstraints();

  // We reuse the BinaryImplicationGraph code to "propagate" 2-SAT.
  Model local_model;
  const int num_variables = context_->NumVariables();
  local_model.GetOrCreate<Trail>()->Resize(num_variables);
  auto* graph = local_model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(num_variables);

  // Extract the bool_and and at_most_one constraints.
  // TODO(user): use probing info?
  int num_amos = 0;
  std::vector<Literal> tmp_clique;
  std::vector<int> clause_indices;
  std::vector<std::vector<Literal>> clauses;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
    if (ct->constraint_case() == ConstraintProto::kAtMostOne) {
      tmp_clique.clear();
      for (const int ref : ct->at_most_one().literals()) {
        tmp_clique.push_back(convert(ref));
      }
      ++num_amos;
      if (!graph->AddAtMostOne(tmp_clique)) {
        return (void)context_->NotifyThatModelIsUnsat();
      }
    } else if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      if (ct->enforcement_literal().size() != 1) continue;
      const Literal enforcement = convert(ct->enforcement_literal(0));
      for (const int ref : ct->bool_and().literals()) {
        if (ref == ct->enforcement_literal(0)) continue;
        ++num_amos;
        if (!graph->AddAtMostOne({enforcement, convert(ref).Negated()})) {
          return (void)context_->NotifyThatModelIsUnsat();
        }
      }
    } else if (ct->constraint_case() == ConstraintProto::kBoolOr) {
      if (!ct->enforcement_literal().empty()) continue;
      clause_indices.push_back(c);
      std::vector<Literal> clause;
      clause.reserve(ct->bool_or().literals().size());
      for (const int ref : ct->bool_or().literals()) {
        clause.push_back(convert(ref));
      }
      clauses.push_back(std::move(clause));
    }
  }

  if (!graph->DetectEquivalences()) {
    return (void)context_->NotifyThatModelIsUnsat();
  }

  // Add the Boolean variable equivalence detected by DetectEquivalences().
  // Those are needed because TransformIntoMaxCliques() will replace all
  // variable by its representative.
  for (int var = 0; var < num_variables; ++var) {
    const Literal l = Literal(BooleanVariable(var), true);
    if (graph->RepresentativeOf(l) != l) {
      const Literal r = graph->RepresentativeOf(l);
      if (!context_->StoreBooleanEqualityRelation(
              var, r.IsPositive() ? r.Variable().value()
                                  : NegatedRef(r.Variable().value()))) {
        return;
      }
    }
  }

  auto signature = [](absl::Span<const Literal> literals) {
    uint64_t result = 0;
    for (const Literal l : literals) {
      result |= (l.Index().value()) & 63;
    }
    return result;
  };
  auto implied_signature = [](absl::Span<const Literal> literals) {
    uint64_t result = literals[0].Index().value() & 63;
    for (const Literal l : literals.subspan(1)) {
      result |= (l.NegatedIndex().value()) & 63;
    }
    return result;
  };

  // Probe variables (using only amo graph) and filter clauses.
  //
  // TODO(user): be faster. with one "probing" we can look at all the clauses
  // containing that literal and filter them.
  int num_transformed = 0;
  int num_checked = 0;
  util_intops::StrongVector<LiteralIndex, int> count(2 * num_variables, 0);
  util_intops::StrongVector<LiteralIndex, int> signatures(2 * num_variables, 0);
  for (int i = 0; i < clauses.size(); ++i) {
    ++num_checked;
    bool is_exo = true;
    const int clause_size = clauses[i].size();

    // First heuristic scan.
    timer.TrackSimpleLoop(clause_size);
    const uint64_t clause_signature = signature(clauses[i]);
    for (const Literal l : clauses[i]) {
      if (count[l] == 0) continue;
      if (count[l] < clause_size || (clause_signature & ~signatures[l])) {
        is_exo = false;
        break;
      }
    }
    if (!is_exo) continue;

    timer.TrackSimpleLoop(clause_size);
    for (const Literal l : clauses[i]) {
      graph->ResetWorkDone();
      absl::Span<const Literal> implied = graph->GetAllImpliedLiterals(l);
      CHECK_GT(implied.size(), 0);  // Always contain l.
      count[l] = implied.size();
      signatures[l] = implied_signature(implied);
      timer.AddToWork(graph->WorkDone() * 1e-9);
      if (implied.size() < clause_size || (clause_signature & ~signatures[l])) {
        is_exo = false;
        break;
      }
      timer.TrackSimpleLoop(clause_size);
      for (const Literal o : clauses[i]) {
        if (o == l) continue;
        if (!graph->LiteralIsImplied(o.Negated())) {
          is_exo = false;
          break;
        }
      }
      if (!is_exo) break;
    }
    if (is_exo) {
      ++num_transformed;
      context_->UpdateRuleStats("clauses: transformed into exactly one");
      google::protobuf::RepeatedField<int32_t> tmp =
          context_->Constraint(clause_indices[i]).bool_or().literals();
      *(context_->MutableConstraint(clause_indices[i])
            ->mutable_exactly_one()
            ->mutable_literals()) = tmp;
    }
    if (timer.WorkLimitIsReached()) break;
  }

  timer.AddCounter("num_amos", num_amos);
  timer.AddCounter("num_clauses", clauses.size());
  timer.AddCounter("num_transformed", num_transformed);
  timer.AddCounter("num_checked", num_checked);
}

// Returns false iff the model is UNSAT.
bool CpModelPresolver::ProcessSetPPCSubset(int subset_c, int superset_c,
                                           absl::flat_hash_set<int>* tmp_set,
                                           bool* remove_subset,
                                           bool* remove_superset,
                                           bool* stop_processing_superset) {
  ConstraintProto* subset_ct = context_->MutableConstraint(subset_c);
  ConstraintProto* superset_ct = context_->MutableConstraint(superset_c);

  if ((subset_ct->constraint_case() == ConstraintProto::kBoolOr ||
       subset_ct->constraint_case() == ConstraintProto::kExactlyOne) &&
      (superset_ct->constraint_case() == ConstraintProto::kAtMostOne ||
       superset_ct->constraint_case() == ConstraintProto::kExactlyOne)) {
    context_->UpdateRuleStats("setppc: bool_or in at_most_one");

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
    int64_t min_sum = kint64max;
    int64_t max_sum = kint64min;
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
      *stop_processing_superset = true;
      return constraint_presolver_->MarkConstraintAsFalse(
          superset_ct, "setppc: removed infeasible linear constraint");
    }

    // We reuse the normal linear constraint code to propagate domains of
    // the other variable using the inclusion information.
    if (superset_ct->enforcement_literal().empty()) {
      CHECK_GT(num_matches, 0);
      FillDomainInProto(ReadDomainFromProto(superset_ct->linear())
                            .AdditionWith(Domain(-max_sum, -min_sum)),
                        temp_ct_.mutable_linear());
      constraint_presolver_->PropagateDomainsInLinear(/*ct_index=*/-1,
                                                      &temp_ct_);
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
      if (!PossibleIntegerOverflow(context_->WorkingModel(), new_ct.vars(),
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
//
// TODO(user): If an exactly_one of size n and a clause/amo share n - 1 terms,
// then we can simplify the clause by using the last term of the exactly_one
// inside it instead.
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
  const int num_constraints = context_->NumConstraints();
  std::vector<int> relevant_constraints;
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
    const auto type = ct->constraint_case();
    if (type == ConstraintProto::kBoolOr ||
        type == ConstraintProto::kAtMostOne ||
        type == ConstraintProto::kExactlyOne) {
      // Because TransformIntoMaxCliques() can detect literal equivalence
      // relation, we make sure the constraints are presolved before being
      // inspected.
      if (constraint_presolver_->PresolveOneConstraint(c)) {
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
  temp_ct_.Clear();
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
      context_->ClearConstraint(subset_c);
      context_->UpdateConstraintVariableUsage(subset_c);
      detector.StopProcessingCurrentSubset();
    }
    if (remove_superset) {
      context_->ClearConstraint(superset_c);
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
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
    if (ct->enforcement_literal().size() <= 1) continue;

    // Make sure there is no x => x.
    if (ct->constraint_case() == ConstraintProto::kBoolAnd) {
      if (constraint_presolver_->PresolveOneConstraint(c)) {
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
    ConstraintProto* subset_ct = context_->MutableConstraint(subset_c);
    ConstraintProto* superset_ct = context_->MutableConstraint(superset_c);
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
          if (!constraint_presolver_->MarkConstraintAsFalse(
                  superset_ct, "bool_and: must be false"))
            return;
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
      context_->Constraint(linear_encoding_ct_index);
  int64_t rhs = linear_encoding.linear().domain(0);
  int target_ref = kint32min;
  std::vector<std::pair<int, int64_t>> ref_to_coeffs;
  const int num_terms = linear_encoding.linear().vars().size();
  for (int i = 0; i < num_terms; ++i) {
    const int ref = linear_encoding.linear().vars(i);
    const int64_t coeff = linear_encoding.linear().coeffs(i);
    const auto it = var_to_ref.find(PositiveRef(ref));

    if (it == var_to_ref.end()) {
      CHECK_EQ(target_ref, kint32min) << "Uniqueness";
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
  if (target_ref == kint32min || context_->CanBeUsedAsLiteral(target_ref)) {
    // We didn't find the unique integer variable. This might have happenned
    // because by processing other encoding we might end up with a fully boolean
    // constraint. Just abort, it will be presolved later.
    context_->UpdateRuleStats("encoding: candidate linear is all boolean now");
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
    context_->UpdateRuleStats("encoding: candidate linear is all boolean now");
    return true;
  }

  // Encode the encoding.
  absl::flat_hash_set<int64_t> value_set;
  const Domain target_domain =
      RefIsPositive(target_ref)
          ? context_->DomainOf(target_ref)
          : context_->DomainOf(NegatedRef(target_ref)).Negation();
  for (const int64_t v : target_domain.Values()) {
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
        auto* bool_or = context_->AddConstraint()->mutable_bool_or();
        for (const int lit : literals) bool_or->add_literals(lit);
        bool_or->add_literals(NegatedRef(associated_lit));
      }
    }
  }

  // Remove linear constraint now that it is fully encoded.
  context_->ClearConstraint(linear_encoding_ct_index);
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

  const int num_vars = context_->NumVariables();
  const int num_constraints = context_->NumConstraints();

  // Our current implementation require almost a full copy.
  // First construct a transpose var to columns (constraint_index, coeff).
  CompactVectorVectorBuilder<int, std::pair<int, int64_t>>
      var_to_columns_builder;

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
    const ConstraintProto& ct = context_->Constraint(c);
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
        var_to_columns_builder.Add(lit, {c, 1});
      }
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::kLinear) {
      const int num_terms = ct.linear().vars().size();
      for (int i = 0; i < num_terms; ++i) {
        const int var = ct.linear().vars(i);
        const int64_t coeff = ct.linear().coeffs(i);
        var_to_columns_builder.Add(var, {c, coeff});
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
  if (context_->WorkingModel().has_objective()) {
    context_->WriteObjectiveToProto();
    const int num_terms = context_->WorkingModel().objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = context_->WorkingModel().objective().vars(i);
      const int64_t coeff = context_->WorkingModel().objective().coeffs(i);
      var_to_columns_builder.Add(var, {kObjectiveConstraint, coeff});
    }
  }

  // Now construct the graph.
  CompactVectorVector<int, std::pair<int, int64_t>> var_to_columns;
  var_to_columns.ResetFromBuilder(var_to_columns_builder);

  // Find duplicate columns using an hash map.
  // We only consider "full" columns.
  // var -> var_representative using columns hash/comparison.
  absl::flat_hash_map<int, int, ColumnHashForDuplicateDetection,
                      ColumnEqForDuplicateDetection>
      duplicates(
          /*reservation_size=*/num_vars,
          ColumnHashForDuplicateDetection(&var_to_columns),
          ColumnEqForDuplicateDetection(&var_to_columns));
  CompactVectorVectorBuilder<int, int> rep_to_dups_builder;
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
      rep_to_dups_builder.Add(it->second, var);
    }
  }

  // Process duplicates.
  int num_equivalent_classes = 0;
  const CompactVectorVector<int, int> rep_to_dups(rep_to_dups_builder);
  std::vector<std::pair<int, int64_t>> definition;
  std::vector<int> var_to_remove;
  std::vector<int> var_to_rep(num_vars, -1);
  for (int var = 0; var < rep_to_dups.size(); ++var) {
    if (rep_to_dups[var].empty()) continue;

    // Since columns are the same, we can introduce a new variable = sum all
    // columns. Note that the linear expression will not overflow, but the
    // overflow check also requires that max_sum < int_max/2, which might
    // happen.
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
    if (new_var == -1) {
      context_->UpdateRuleStats("TODO duplicate: possible overflow");
      continue;
    }

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
      ConstraintProto* mutable_ct = context_->MutableConstraint(c);

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
  if (context_->WorkingModel().has_objective()) {
    if (!context_->CanonicalizeObjective()) return;
    context_->WriteObjectiveToProto();
  }

  // If we detect duplicate intervals, we will remap constraints using them.
  std::vector<int> interval_mapping;

  // Remove duplicate constraints.
  // Note that at this point the objective in the proto should be up to date.
  //
  // TODO(user): We might want to do that earlier so that our count of variable
  // usage is not biased by duplicate constraints.
  const std::vector<std::pair<int, int>> duplicates = FindDuplicateConstraints(
      context_->WorkingModel(), /*ignore_enforcement=*/false,
      /*ignore_linear_domain=*/true, /*ignore_target_of_expression=*/true);
  timer.AddCounter("duplicates", duplicates.size());
  for (const auto& [dup, rep] : duplicates) {
    // Note that it is important to look at the type of the representative in
    // case the constraint became empty.
    DCHECK_LT(kObjectiveConstraint, 0);
    const int type = rep == kObjectiveConstraint
                         ? kObjectiveConstraint
                         : context_->Constraint(rep).constraint_case();

    if (type == ConstraintProto::kInterval) {
      interval_mapping.resize(context_->NumConstraints(), -1);
      CHECK_EQ(interval_mapping[rep], -1);
      interval_mapping[dup] = rep;
    }

    // For linear constraint, we merge their rhs since it was ignored in the
    // FindDuplicateConstraints() call.
    if (type == ConstraintProto::kLinear) {
      const Domain rep_domain =
          ReadDomainFromProto(context_->Constraint(rep).linear());
      const Domain d = ReadDomainFromProto(context_->Constraint(dup).linear());
      if (rep_domain != d) {
        context_->UpdateRuleStats("duplicate: merged rhs of linear constraint");
        const Domain rhs = rep_domain.IntersectionWith(d);
        if (rhs.IsEmpty()) {
          if (!constraint_presolver_->MarkConstraintAsFalse(
                  context_->MutableConstraint(rep),
                  "duplicate: false after merging")) {
            return;
          }

          // The representative constraint is no longer a linear constraint,
          // so we will not enter this type case again and will just remove
          // all subsequent duplicate linear constraints.
          context_->UpdateConstraintVariableUsage(rep);
          continue;
        }
        FillDomainInProto(rhs,
                          context_->MutableConstraint(rep)->mutable_linear());
      }
    }

    if (type == kObjectiveConstraint) {
      context_->UpdateRuleStats(
          "duplicate: linear constraint parallel to objective");
      const Domain d = ReadDomainFromProto(context_->Constraint(dup).linear());
      if (!context_->RestrictObjectiveDomain(d)) return;
    }

    // Deal with A = F(exprs) and B = F(exprs) which implies A <=> B.
    const LinearExpressionProto* a = nullptr;
    const LinearExpressionProto* b = nullptr;
    if (type == ConstraintProto::kIntProd) {
      a = &context_->Constraint(rep).int_prod().target();
      b = &context_->Constraint(dup).int_prod().target();
    } else if (type == ConstraintProto::kLinMax) {
      a = &context_->Constraint(rep).lin_max().target();
      b = &context_->Constraint(dup).lin_max().target();
    } else if (type == ConstraintProto::kIntMod) {
      a = &context_->Constraint(rep).int_mod().target();
      b = &context_->Constraint(dup).int_mod().target();
    } else if (type == ConstraintProto::kIntDiv) {
      a = &context_->Constraint(rep).int_div().target();
      b = &context_->Constraint(dup).int_div().target();
    }
    if (a != nullptr) {
      // If they are equal, we fall back to the default case.
      if (!LinearExpressionProtosAreExactlyEqual(*a, *b)) {
        const std::string rule_name = absl::StrCat(
            "duplicate: new equivalence via X = Y = ",
            ConstraintCaseName(context_->Constraint(rep).constraint_case()),
            "(vars)");

        // We don't know what to do if there are enforcement.
        //
        // The code is still correct for the case of longer linear, but I am not
        // sure it is always a simplification. In any case, it shouldn't
        // happened often as in most situation our "targets" should be linear1.
        if (!context_->Constraint(rep).enforcement_literal().empty() ||
            (a->vars().size() > 1 && !b->vars().empty()) ||
            (b->vars().size() > 1 && !a->vars().empty())) {
          context_->UpdateRuleStats(absl::StrCat("TODO ", rule_name));
          continue;
        }

        // TODO(user): Do substitution right away? that would require some
        // refactoring as our code to handle complex u X + v Y = rhs is not so
        // easy to use.
        auto* linear = context_->AddConstraint()->mutable_linear();
        linear->add_domain(0);
        linear->add_domain(0);
        AddLinearExpressionToLinearConstraint(*a, 1, linear);
        AddLinearExpressionToLinearConstraint(*b, -1, linear);

        // Note that we clear the duplicate constraint below.
        context_->UpdateRuleStats(rule_name);

        // This make sure that if we have a long-linear and a constant for
        // instance, we keep the constant = f(vars) and not the other one. Note
        // that b always correspond to the "dup" constraint.
        if (b->vars().size() < a->vars().size()) {
          *context_->MutableConstraint(rep) = context_->Constraint(dup);
          context_->UpdateConstraintVariableUsage(rep);
        }
      }
    }

    // Remove the duplicate constraint.
    context_->ClearConstraint(dup);
    context_->UpdateConstraintVariableUsage(dup);
    context_->UpdateRuleStats("duplicate: removed constraint");
  }

  if (!interval_mapping.empty()) {
    context_->UpdateRuleStats("duplicate: remapped duplicate intervals");
    const int num_constraints = context_->NumConstraints();
    for (int c = 0; c < num_constraints; ++c) {
      bool changed = false;
      ApplyToAllIntervalIndices(
          [&interval_mapping, &changed](int* ref) {
            const int new_ref = interval_mapping[*ref];
            if (new_ref != -1) {
              changed = true;
              *ref = new_ref;
            }
          },
          context_->MutableConstraint(c));
      if (changed) context_->UpdateConstraintVariableUsage(c);
    }
  }
}

void CpModelPresolver::DetectDuplicateConstraintsWithDifferentEnforcements(
    const CpModelMapping* mapping, BinaryImplicationGraph* implication_graph,
    Trail* trail) {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  // We need the objective written for this.
  if (context_->WorkingModel().has_objective()) {
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
      FindDuplicateConstraints(context_->WorkingModel(),
                               /*ignore_enforcement=*/true,
                               /*ignore_linear_domain=*/false,
                               /*ignore_target_of_expression=*/false);
  timer.AddCounter("without_enforcements",
                   duplicates_without_enforcement.size());
  for (const auto& [dup, rep] : duplicates_without_enforcement) {
    if (timer.WorkLimitIsReached()) break;
    auto* dup_ct = context_->MutableConstraint(dup);
    auto* rep_ct = context_->MutableConstraint(rep);

    if (dup_ct->constraint_case() == ConstraintProto::kInterval) {
      context_->UpdateRuleStats(
          "TODO interval: same interval with different enforcement?");
      continue;
    }

    // Make sure our enforcement list are up to date: nothing fixed and that
    // its uses the literal representatives.
    bool changed = false;
    if (!constraint_presolver_->PresolveEnforcementLiteral(dup_ct, &changed)) {
      return;
    }
    if (changed) {
      context_->UpdateConstraintVariableUsage(dup);
    }
    if (!constraint_presolver_->PresolveEnforcementLiteral(rep_ct, &changed)) {
      return;
    }
    if (changed) {
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
      // if we encounter them here in some corner cases. And the code after
      // 'continue' uses this, in particular to update the hint.
      bool skip = false;
      if (RefIsPositive(a) == (context_->ObjectiveCoeff(PositiveRef(a)) > 0)) {
        context_->UpdateRuleStats("duplicate: dual fixing enforcement");
        if (!context_->SetLiteralToFalse(a)) return;
        skip = true;
      }
      if (RefIsPositive(b) == (context_->ObjectiveCoeff(PositiveRef(b)) > 0)) {
        context_->UpdateRuleStats("duplicate: dual fixing enforcement");
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
      // If `a` and `b` hints are different then the whole hint satisfies
      // the enforced constraint. We can thus change them to true (this cannot
      // increase the objective value thanks to the `skip` test above -- the
      // objective domain is non-constraining, but this only guarantees that
      // singleton variables can freely *decrease* the objective).
      solution_crush_.UpdateLiteralsToFalseIfDifferent(NegatedRef(a),
                                                       NegatedRef(b));
      if (!context_->StoreBooleanEqualityRelation(a, b)) return;

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
        const auto& ct_a = context_->Constraint(c_a);
        const auto& ct_b = context_->Constraint(c_b);

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
          absl::Span<const Literal> implied =
              implication_graph->DirectImplications(lit);
          timer.TrackSimpleLoop(implied.size());
          for (const Literal implication_lit : implied) {
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
                if (!constraint_presolver_->MarkConstraintAsFalse(
                        rep_ct, "duplicate: linear1 infeasible")) {
                  return;
                }
                if (!constraint_presolver_->MarkConstraintAsFalse(
                        dup_ct, "duplicate: linear1 infeasible")) {
                  return;
                }
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
                                                              const Domain& d) {
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
    offsets[key] = {
        exclusion.Min() == kint64min ? kint64max : CapAdd(-exclusion.Min(), 1),
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
  SimpleDuplicateImplicationDetector implications;
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
    if (ct.enforcement_literal().empty() &&
        ct.constraint_case() == ConstraintProto::kAtMostOne) {
      implications.AddAtMostOneImplicationsIfNotTooBig(
          ct.at_most_one().literals());
      continue;
    }
    if (ct.enforcement_literal().empty() &&
        ct.constraint_case() == ConstraintProto::kExactlyOne) {
      implications.AddAtMostOneImplicationsIfNotTooBig(
          ct.exactly_one().literals());
      continue;
    }
    if (ct.enforcement_literal().size() == 1 &&
        ct.constraint_case() == ConstraintProto::kBoolAnd) {
      for (const int ref : ct.bool_and().literals()) {
        implications.AddImplication(ct.enforcement_literal(0), ref);
      }
    }

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
        process_difference(ct.linear().vars(0), ct.linear().vars(1),
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
      const ConstraintProto& ct1 = context_->Constraint(range[i].second);
      const int num_terms = ct1.linear().vars().size();
      for (int j = i + 1; j < range.size(); ++j) {
        const ConstraintProto& ct2 = context_->Constraint(range[j].second);
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
            process_difference(ct1.linear().vars(0), ct1.linear().vars(1),
                               union_of_domain.Negation());
          }
        }

        // We really do not want to add already existing implication.
        if (lit1 != NegatedRef(lit2) &&
            !implications.ContainsClause2(NegatedRef(lit1), NegatedRef(lit2))) {
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
    const int num_variables = context_->NumVariables();
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
    CHECK(graph->TransformIntoMaxCliques(&cliques, 1e8));

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
      std::vector<int64_t> sizes(num_terms, kint64max);
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
        if (issue == kint64max) {
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
          intervals.push_back(context_->NumConstraints());
          auto* new_interval = context_->AddConstraint()->mutable_interval();
          new_interval->mutable_start()->set_offset(0);
          new_interval->mutable_start()->add_coeffs(1);
          new_interval->mutable_start()->add_vars(clique[i].Variable().value());

          new_interval->mutable_size()->set_offset(sizes[i]);

          new_interval->mutable_end()->set_offset(sizes[i]);
          new_interval->mutable_end()->add_coeffs(1);
          new_interval->mutable_end()->add_vars(clique[i].Variable().value());
        }
        auto* new_ct = context_->AddConstraint()->mutable_no_overlap();
        for (const int interval : intervals) {
          new_ct->add_intervals(interval);
        }
      } else {
        context_->UpdateRuleStats("all_diff: inferred from x != y constraints");
        auto* new_ct = context_->AddConstraint()->mutable_all_diff();
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
    explicit Storage(CpModelProto const* proto) : proto_(*proto) {}
    int size() const { return static_cast<int>(proto_.constraints().size()); }
    absl::Span<const int> operator[](int c) const {
      return absl::MakeSpan(proto_.constraints(c).linear().vars());
    }

   private:
    const CpModelProto& proto_;
  };
  Storage storage(&context_->WorkingModel());
  InclusionDetector detector(storage, time_limit_);
  detector.SetWorkLimit(context_->params().presolve_inclusion_work_limit());

  // Because we use the constraint <-> variable graph, we cannot modify it
  // during DetectInclusions(). So we delay the update of the graph.
  std::vector<int> constraint_indices_to_clean;

  // Cache the linear expression domain.
  // TODO(user): maybe we should store this instead of recomputing it.
  absl::flat_hash_map<int, Domain> cached_expr_domain;

  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
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
    const ConstraintProto& subset_ct = context_->Constraint(subset_c);
    const LinearConstraintProto& subset_lin = subset_ct.linear();
    coeff_map.clear();
    detector.IncreaseWorkDone(subset_lin.vars().size());
    for (int i = 0; i < subset_lin.vars().size(); ++i) {
      coeff_map[subset_lin.vars(i)] += subset_lin.coeffs(i);
    }

    // We have a perfect match if 'factor_a * subset == factor_b * superset' on
    // the common positions. Note that assuming subset has been gcd reduced,
    // there is not point considering factor_b != 1.
    bool perfect_match = true;

    // Find interesting factor of the subset that cancels terms of the superset.
    int64_t factor = 0;
    int64_t min_pos_factor = kint64max;
    int64_t max_neg_factor = kint64min;

    // Lets compute the implied domain of the linear expression
    // "superset - subset". Note that we actually do not need exact inclusion
    // for this algorithm to work, but it is an heuristic to not try it with
    // all pair of constraints.
    const ConstraintProto& superset_ct = context_->Constraint(superset_c);
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
    {
      const Domain implied_superset_domain =
          subset_rhs.AdditionWith(diff_domain)
              .IntersectionWith(cached_expr_domain[superset_c]);
      if (implied_superset_domain.IsIncludedIn(superset_rhs) &&
          std::includes(superset_ct.enforcement_literal().begin(),
                        superset_ct.enforcement_literal().end(),
                        subset_ct.enforcement_literal().begin(),
                        subset_ct.enforcement_literal().end())) {
        context_->UpdateRuleStats(absl::StrCat(
            "linear inclusion: redundant containing constraint",
            subset_ct.enforcement_literal().empty() ? ""
                                                    : " (with enforcement)"));
        context_->ClearConstraint(superset_c);
        constraint_indices_to_clean.push_back(superset_c);
        detector.StopProcessingCurrentSuperset();
        return;
      }
    }

    // Case 2: subset is redundant.
    {
      const Domain implied_subset_domain =
          superset_rhs.AdditionWith(diff_domain.Negation())
              .IntersectionWith(cached_expr_domain[subset_c]);
      if (implied_subset_domain.IsIncludedIn(subset_rhs) &&
          std::includes(subset_ct.enforcement_literal().begin(),
                        subset_ct.enforcement_literal().end(),
                        superset_ct.enforcement_literal().begin(),
                        superset_ct.enforcement_literal().end())) {
        context_->UpdateRuleStats(absl::StrCat(
            "linear inclusion: redundant included constraint",
            superset_ct.enforcement_literal().empty() ? ""
                                                      : " (with enforcement)"));
        context_->ClearConstraint(subset_c);
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
      if (min_pos_factor != kint64max && max_neg_factor != kint64min) {
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
                   context_->MutableConstraint(superset_c)->mutable_linear());
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
      DCHECK(temp_ct_.enforcement_literal().empty());
      auto* mutable_linear = temp_ct_.mutable_linear();
      mutable_linear->Clear();
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
      constraint_presolver_->PropagateDomainsInLinear(/*ct_index=*/-1,
                                                      &temp_ct_);
      if (context_->ModelIsUnsat()) detector.Stop();

      if (superset_rhs.IsFixed()) {
        if (subset_lin.vars().size() + 1 == superset_lin.vars().size()) {
          // Because we propagated the equation on the singleton variable above,
          // and we have an equality, the subset is redundant!
          context_->UpdateRuleStats(
              "linear inclusion: subset + singleton is equality");
          context_->ClearConstraint(subset_c);
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
    absl::Span<const std::pair<int, int64_t>> block,
    ActivityBoundHelper* helper) {
  int new_var;
  int64_t gcd = 0;
  int64_t offset = 0;

  // If the common part is expressable via one of the constraint in the block as
  // == gcd * X + offset, we can just use this variable instead of creating a
  // new variable.
  int definiting_equation = -1;
  for (const auto [c, multiple] : block) {
    const ConstraintProto& ct = context_->Constraint(c);
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

    auto* mutable_linear = context_->MutableConstraint(c)->mutable_linear();
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

  for (int x = 0; x < context_->NumVariables(); ++x) {
    // We pick a variable x that appear in some AMO.
    if (helper->NumAmoForVariable(x) == 0) continue;
    if (time_limit_->LimitReached()) break;
    if (timer.WorkLimitIsReached()) break;

    amo_cts.clear();
    timer.TrackSimpleLoop(context_->VarToConstraints(x).size());
    for (const int c : context_->VarToConstraints(x)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->Constraint(c);
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
          absl::Uniform<int>(context_->random(), 0, amo_cts.size());
      base_ct_index = amo_cts[random_c];
      const ConstraintProto& ct = context_->Constraint(base_ct_index);
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
      const ConstraintProto& ct = context_->Constraint(c);
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
    std::shuffle(linear_cts.begin(), linear_cts.end(), context_->random());
    for (const int c : linear_cts) {
      const ConstraintProto& ct = context_->Constraint(c);
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
      const ConstraintProto& ct = context_->Constraint(base_ct_index);
      if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        context_->UpdateRuleStats("TODO linear matrix: constant rectangle!");
      } else {
        context_->UpdateRuleStats(
            "TODO linear matrix: reuse defining constraint");
      }
    } else if (new_size + 1 == amo_literals.size()) {
      const ConstraintProto& ct = context_->Constraint(base_ct_index);
      if (ct.constraint_case() == ConstraintProto::kExactlyOne) {
        context_->UpdateRuleStats("TODO linear matrix: reuse exo constraint");
      }
    }
    amo_literals.resize(new_size);

    // Create a new literal that is one iff one of the literal in AMO is one.
    const int new_var = context_->NewBoolVarWithClause(amo_literals);
    {
      auto* new_exo = context_->AddConstraint()->mutable_exactly_one();
      new_exo->mutable_literals()->Reserve(amo_literals.size() + 1);
      for (const int lit : amo_literals) {
        new_exo->add_literals(lit);
      }
      new_exo->add_literals(NegatedRef(new_var));
    }

    // Filter the base amo/exo.
    {
      ConstraintProto* ct = context_->MutableConstraint(base_ct_index);
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
      auto* mutable_linear = context_->MutableConstraint(c)->mutable_linear();

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
  for (int x = 0; x < context_->NumVariables(); ++x) {
    if (timer.WorkLimitIsReached()) break;

    bool in_enforcement = false;
    std::vector<int> linear_cts;
    timer.TrackSimpleLoop(context_->VarToConstraints(x).size());
    for (const int c : context_->VarToConstraints(x)) {
      if (c < 0) continue;
      const ConstraintProto& ct = context_->Constraint(c);
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
    std::shuffle(linear_cts.begin(), linear_cts.end(), context_->random());

    // Now it is almost the same algo as for FindBigHorizontalLinearOverlap().
    // We greedely compute a "common" rectangle using the first constraint
    // as a "base" one. Note that if a aX + bY appear in the majority of
    // constraint, we have a good chance to find this block since we start by
    // a random constraint.
    coeff_map.clear();

    std::vector<std::pair<int, int64_t>> block;
    std::vector<std::pair<int, int64_t>> common_part;
    for (const int c : linear_cts) {
      const ConstraintProto& ct = context_->Constraint(c);
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

    // When we have a wide range of coefficient, introducing a new variable can
    // hurt the linear relaxation cuts, because we lose a lot of information
    // about the integrality while reasoning on the sum. So we do that more
    // defensively.
    //
    // This avoid degrading the perf a lot on the bppc miplib problems.
    int64_t min_magnitude = kint64max;
    int64_t max_magnitude = 0;
    for (const auto [unused, coeff] : coeff_map) {
      const int64_t magnitude = std::abs(coeff);
      min_magnitude = std::min(min_magnitude, magnitude);
      max_magnitude = std::max(max_magnitude, magnitude);
    }
    if (min_magnitude != max_magnitude && saved_nz < 1'000) continue;

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

  const int num_constraints = context_->NumConstraints();
  std::vector<std::pair<int, int>> to_sort;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
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
  std::vector<int64_t> var_to_coeff(context_->NumVariables(), 0);

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
      const ConstraintProto& ct = context_->Constraint(c);
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
      const ConstraintProto& ct = context_->Constraint(other_c);

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
        const ConstraintProto& ct = context_->Constraint(other_c);
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
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
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
  const int num_variables = context_->NumVariables();
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
      const LinearConstraintProto& lin = context_->Constraint(c).linear();
      const int index =
          absl::Uniform<int>(context_->random(), 0, lin.vars().size());
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
      const LinearConstraintProto& lin1 = context_->Constraint(c1).linear();
      bool skip = false;
      for (int i = 0; !skip && i < lin1.vars().size(); ++i) {
        for (const auto [c2, coeff2] : var_to_ct_coeffs_[lin1.vars(i)]) {
          if (c2 == c1) continue;

          // TODO(user): we could easily deal with * -1 or other multiples.
          if (coeff2 != lin1.coeffs(i)) continue;
          if (timer.WorkLimitIsReached()) break;

          // Skip if we processed this earlier and deleted it.
          const ConstraintProto& ct2 = context_->Constraint(c2);
          if (ct2.constraint_case() != ConstraintProto::kLinear) continue;
          const LinearConstraintProto& lin2 = context_->Constraint(c2).linear();
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
            auto* to_modify = context_->MutableConstraint(c1);
            if (!AddLinearConstraintMultiple(-1, context_->Constraint(c2),
                                             to_modify)) {
              continue;
            }

            // Affine will be of size 2, but we might also have the same
            // variable with different coeff in both constraint, in which case
            // the linear will be of size 1.
            DCHECK_LE(to_modify->linear().vars().size(), 2);

            ++num_affine_relations;
            context_->UpdateRuleStats(
                "linear: advanced affine relation from 2 constraints");

            // We should stop processing c1 since it should be empty afterward.
            constraint_presolver_->DivideLinearByGcd(to_modify);
            constraint_presolver_->PresolveSmallLinear(to_modify);
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
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = context_->Constraint(c);
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
    const ConstraintProto& superset_ct = context_->Constraint(superset_c);
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

void CpModelPresolver::MaybeRemoveLinkingVariable(int var, int c_linear1,
                                                  int c_linear) {
  const ConstraintProto& ct_linear1 = context_->Constraint(c_linear1);
  if (ct_linear1.enforcement_literal().empty()) return;

  CHECK_EQ(ct_linear1.linear().vars().size(), 1);
  CHECK_EQ(ct_linear1.linear().vars(0), var);
  const Domain linear1_restriction =
      ReadDomainFromProto(ct_linear1.linear())
          .InverseMultiplicationBy(ct_linear1.linear().coeffs(0))
          .IntersectionWith(context_->DomainOf(var));
  if (linear1_restriction.IsEmpty()) return;  // Dealt with elsewhere.

  const LinearConstraintProto& linear = context_->Constraint(c_linear).linear();
  const Domain rhs = ReadDomainFromProto(linear);

  Domain relaxed_rhs;
  Domain enforced_rhs;

  // Check that the constraint is trivial if var is not restricted and not used
  // elsewhere.
  const int num_terms = linear.vars().size();
  Domain implied = Domain(0);
  for (int i = 0; i < num_terms; ++i) {
    const int64_t coeff = linear.coeffs(i);
    if (linear.vars(i) == var) {
      if (std::abs(coeff) != 1) return;
      enforced_rhs =
          rhs.AdditionWith(linear1_restriction.MultiplicationBy(-coeff));
      relaxed_rhs =
          rhs.AdditionWith(context_->DomainOf(var).MultiplicationBy(-coeff));
    } else {
      implied =
          implied
              .AdditionWith(
                  context_->DomainOf(linear.vars(i)).MultiplicationBy(coeff))
              .RelaxIfTooComplex();
    }
  }

  if (num_terms == 1) {
    // Will be handled elsewhere.
    return;
  }

  // If the constraint is not trivial when the linear1 is not enforced, we
  // only remove var if we don't add too many non-zeros.
  const bool is_trivial_when_not_enforced = implied.IsIncludedIn(relaxed_rhs);
  if (!is_trivial_when_not_enforced) {
    // If num_terms == 2, we will be left with two linear1 (always good).
    // If num_terms == 3, we will be left with two linear2, not clear but since
    // we have special handling for them, we currently do it as well.
    if (num_terms > 3) {
      context_->UpdateRuleStats(
          "TODO linear: remove linking var in linear1 and linear");
      return;
    }
  }

  // We can remove var !
  context_->UpdateRuleStats("linear: remove linking var in linear1 and linear");

  // Copy the constraints to the mapping_model. Note that the linear1 should
  // appear after for our simple postsolve to work correctly.
  context_->NewMappingConstraint(context_->Constraint(c_linear), __FILE__,
                                 __LINE__);
  context_->NewMappingConstraint(ct_linear1, __FILE__, __LINE__);

  // Remove var from the long linear and update rhs.
  ConstraintProto* mutable_ct = context_->MutableConstraint(c_linear);
  LinearConstraintProto* mutable_linear = mutable_ct->mutable_linear();
  int new_size = 0;
  for (int i = 0; i < num_terms; ++i) {
    if (mutable_linear->vars(i) == var) continue;
    mutable_linear->set_vars(new_size, mutable_linear->vars(i));
    mutable_linear->set_coeffs(new_size, mutable_linear->coeffs(i));
    ++new_size;
  }
  mutable_linear->mutable_vars()->Truncate(new_size);
  mutable_linear->mutable_coeffs()->Truncate(new_size);
  FillDomainInProto(enforced_rhs, mutable_linear);

  if (!is_trivial_when_not_enforced) {
    // We need a new constraint in this case.
    ConstraintProto* new_ct = context_->AddConstraint();
    mutable_ct = context_->MutableConstraint(c_linear);
    *new_ct = *mutable_ct;
    FillDomainInProto(relaxed_rhs, new_ct->mutable_linear());
  }

  // Add the enforcement to the long linear constraint.
  for (const int lit : context_->Constraint(c_linear1).enforcement_literal()) {
    mutable_ct->add_enforcement_literal(lit);
  }
  context_->UpdateConstraintVariableUsage(c_linear);

  // Tricky: In some corner case, during postsolve, neither the variable in
  // the linear1 nor the enforcement are set. Adding this extra mapping
  // constraint should make sure that the enforcement is set to a reasonable
  // value.
  //
  // TODO(user): make the contract on when variable are assigned during
  // postsolve clearer.
  context_->NewMappingConstraint(context_->Constraint(c_linear), __FILE__,
                                 __LINE__);

  // Clear linear1.
  context_->MutableConstraint(c_linear1)->Clear();
  context_->UpdateConstraintVariableUsage(c_linear1);

  context_->MarkVariableAsRemoved(var);

  constraint_presolver_->PresolveSmallLinear(mutable_ct);
  context_->UpdateConstraintVariableUsage(c_linear);
}

void CpModelPresolver::PresolveVarOnlyInIntProdAndLinMax(int var,
                                                         int int_prod_ct_index,
                                                         int lin_max_ct_index) {
  if (context_->CanBeUsedAsLiteral(var)) return;
  if (!context_->Constraint(int_prod_ct_index).enforcement_literal().empty()) {
    return;
  }
  if (!context_->Constraint(lin_max_ct_index).enforcement_literal().empty()) {
    return;
  }

  DCHECK_EQ(context_->Constraint(int_prod_ct_index).constraint_case(),
            ConstraintProto::kIntProd);
  CHECK_EQ(context_->Constraint(lin_max_ct_index).constraint_case(),
           ConstraintProto::kLinMax);

  const LinearArgumentProto& int_prod =
      context_->Constraint(int_prod_ct_index).int_prod();
  const LinearArgumentProto& lin_max =
      context_->Constraint(lin_max_ct_index).lin_max();

  // We only handle the case:
  // - target = +/- var ^ 2;
  // - +/- var = abs(y_expr).
  //
  // Note that we only do that if y_expr is not a complex expression
  // otherwise we might not have properly "transferred" the domain of var
  // onto the one of y_expr, and we might not be able to just remove the
  // constraint. That said if the int_prod target is simple, that should
  // still work as we should transfer the domain of var there.
  if (int_prod.exprs().size() != 2) return;
  if (!ExpressionContainsSingleRef(int_prod.exprs(0))) return;
  if (int_prod.exprs(0).vars(0) != var) return;
  if (!LinearExpressionProtosAreExactlyEqual(int_prod.exprs(0),
                                             int_prod.exprs(1))) {
    return;
  }

  if (!ExpressionContainsSingleRef(lin_max.target())) return;
  if (lin_max.target().vars(0) != var) return;
  if (lin_max.exprs().size() != 2) return;
  if (lin_max.exprs(0).vars().size() > 1) return;
  if (!LinearExpressionProtosAreEqual(lin_max.exprs(0), lin_max.exprs(1), -1)) {
    return;
  }

  context_->UpdateRuleStats(
      "degree2: removed intermediate abs() variable in X = abs(Y) ^ 2");

  // Replace var by y_expr in c_square.
  const LinearExpressionProto y_expr = lin_max.exprs(0);
  LinearArgumentProto* mutable_sq =
      context_->MutableConstraint(int_prod_ct_index)->mutable_int_prod();
  *(mutable_sq->mutable_exprs(0)) = y_expr;
  *(mutable_sq->mutable_exprs(1)) = y_expr;
  context_->UpdateConstraintVariableUsage(int_prod_ct_index);

  // Copy abs() to the mapping proto and clear it.
  context_->NewMappingConstraint(context_->Constraint(lin_max_ct_index),
                                 __FILE__, __LINE__);
  context_->ClearConstraint(lin_max_ct_index);
  context_->UpdateConstraintVariableUsage(lin_max_ct_index);
  context_->MarkVariableAsRemoved(var);
}

void CpModelPresolver::PresolveVarOnlyInLinearAndLinear(int var,
                                                        int linear1_ct_index,
                                                        int linear2_ct_index) {
  DCHECK_EQ(context_->Constraint(linear1_ct_index).constraint_case(),
            ConstraintProto::kLinear);
  DCHECK_EQ(context_->Constraint(linear2_ct_index).constraint_case(),
            ConstraintProto::kLinear);

  const int smallest_linear_idx =
      context_->Constraint(linear1_ct_index).linear().vars().size() <
              context_->Constraint(linear2_ct_index).linear().vars().size()
          ? linear1_ct_index
          : linear2_ct_index;
  const int largest_linear_idx = smallest_linear_idx == linear1_ct_index
                                     ? linear2_ct_index
                                     : linear1_ct_index;

  // Special case for lit => var \in Domain and var in linear.
  if (!context_->CanBeUsedAsLiteral(var) &&
      context_->Constraint(smallest_linear_idx).linear().vars().size() == 1) {
    MaybeRemoveLinkingVariable(var, smallest_linear_idx, largest_linear_idx);
    // Stop if we removed the variable or one of the constraints.
    if (context_->VariableWasRemoved(var)) {
      return;
    }
    if (context_->Constraint(smallest_linear_idx).constraint_case() !=
        ConstraintProto::kLinear) {
      return;
    }
    if (context_->Constraint(largest_linear_idx).constraint_case() !=
        ConstraintProto::kLinear) {
      return;
    }
  }

  const ConstraintProto& linear1 = context_->Constraint(linear1_ct_index);
  const ConstraintProto& linear2 = context_->Constraint(linear2_ct_index);

  // Special case: if a literal l appear in exactly two constraints:
  // - l => var in domain1
  // - not(l) => var in domain2
  // then we know that domain(var) is included in domain1 U domain2,
  // and that the literal l can be removed (and determined at postsolve).
  //
  // TODO(user): This could be generalized further to linear of size > 1 if for
  // example the terms are the same.
  //
  // TODO(user): If var is in objective, we might be able to tighten domains.
  // ex: enf => x \in [0, 1]
  //     not(enf) => x \in [1, 2]
  // The x can be removed from one place. Maybe just do <=> not in [0,1] with
  // dual code?
  if (linear1.linear().vars().size() != 1) return;
  if (linear2.linear().vars().size() != 1) return;
  if (linear1.enforcement_literal().size() != 1) return;
  if (linear2.enforcement_literal().size() != 1) return;
  if (PositiveRef(linear1.enforcement_literal(0)) != var) return;
  if (PositiveRef(linear2.enforcement_literal(0)) != var) return;
  if (linear2.enforcement_literal(0) !=
      NegatedRef(linear1.enforcement_literal(0))) {
    return;
  }
  if (linear2.linear().vars(0) != linear1.linear().vars(0)) {
    return;
  }

  const int ct_var = linear1.linear().vars(0);
  if (ct_var == var) return;
  DCHECK(RefIsPositive(ct_var));
  const Domain linear1_domain =
      ReadDomainFromProto(linear1.linear())
          .InverseMultiplicationBy(linear1.linear().coeffs(0));
  const Domain linear2_domain =
      ReadDomainFromProto(linear2.linear())
          .InverseMultiplicationBy(linear2.linear().coeffs(0));

  const Domain union_domain = linear1_domain.UnionWith(linear2_domain);
  if (!context_->IntersectDomainWith(ct_var, union_domain)) return;

  const Domain ct_var_domain = context_->DomainOf(ct_var);
  if (!linear1_domain.OverlapsWith(ct_var_domain) ||
      !linear2_domain.OverlapsWith(ct_var_domain) ||
      ct_var_domain.IsIncludedIn(linear1_domain) ||
      ct_var_domain.IsIncludedIn(linear2_domain)) {
    // One of the constraints is unsat or trivial. This code is complicated
    // enough and this should be rare, so let's leave it to the general case.
    return;
  }
  context_->UpdateRuleStats("variables: removable enforcement literal");

  const auto& ct_var_constraints = context_->VarToConstraints(ct_var);
  if (ct_var_constraints.size() == 2) {
    // The integer variable is also not used elsewhere else. Fix it, otherwise
    // the postsolve will complain that nothing is fixing the value of this
    // variable.
    const Domain valid_domain = linear1_domain.IntersectionWith(ct_var_domain);
    DCHECK(!valid_domain.IsEmpty());
    if (!context_->IntersectDomainWith(ct_var, Domain(valid_domain.Min()))) {
      return;
    }
    const int lit = linear1.enforcement_literal(0);
    if (!context_->IntersectDomainWith(
            PositiveRef(lit), RefIsPositive(lit) ? Domain(1) : Domain(0))) {
      return;
    }
    return;
  }

  // Note(user): Only one constraint should be enough given how the postsolve
  // work. However that will not work for the case where we postsolve by solving
  // the mapping model (debug_postsolve_with_full_solver:true). Moreover, the
  // postsolve works better if the mapping constraints are non-ambiguous.
  ConstraintProto* mapping_ct1 =
      context_->NewMappingConstraint(__FILE__, __LINE__);
  mapping_ct1->mutable_linear()->add_vars(ct_var);
  mapping_ct1->mutable_linear()->add_coeffs(1);
  mapping_ct1->add_enforcement_literal(linear1.enforcement_literal(0));
  FillDomainInProto(linear1_domain, mapping_ct1->mutable_linear());
  ConstraintProto* mapping_ct2 =
      context_->NewMappingConstraint(__FILE__, __LINE__);
  mapping_ct2->mutable_linear()->add_vars(ct_var);
  mapping_ct2->mutable_linear()->add_coeffs(1);
  mapping_ct2->add_enforcement_literal(linear2.enforcement_literal(0));
  // Make sure we have
  //   enf => x \in d1
  //   not(enf) => x \in d2
  // with d1 disjoint of d1.
  FillDomainInProto(
      linear2_domain.IntersectionWith(linear1_domain.Complement()),
      mapping_ct2->mutable_linear());
  for (const int ct_index : {linear1_ct_index, linear2_ct_index}) {
    context_->ClearConstraint(ct_index);
    context_->UpdateConstraintVariableUsage(ct_index);
  }
  context_->MarkVariableAsRemoved(var);
}

void CpModelPresolver::PresolveVarOnlyInLinMaxAndLinear(int var,
                                                        int lin_max_ct_index,
                                                        int linear_ct_index) {
  // Special case for var only appearing in
  // - var = expr,
  // - target = lin_max(var, ...).
  //
  // we presolve as
  //   target = lin_max(expr, ...).
  //   expr \in Domain(var).
  DCHECK_EQ(context_->Constraint(linear_ct_index).constraint_case(),
            ConstraintProto::kLinear);
  DCHECK_EQ(context_->Constraint(lin_max_ct_index).constraint_case(),
            ConstraintProto::kLinMax);
  if (!context_->Constraint(linear_ct_index).enforcement_literal().empty()) {
    return;
  }

  const LinearArgumentProto& lin_max =
      context_->Constraint(lin_max_ct_index).lin_max();
  const LinearConstraintProto& linear =
      context_->Constraint(linear_ct_index).linear();

  if (linear.domain().size() != 2) return;
  if (linear.domain(0) != linear.domain(1)) return;

  if (absl::c_linear_search(lin_max.target().vars(), var)) return;
  if (absl::c_any_of(
          context_->Constraint(lin_max_ct_index).enforcement_literal(),
          [var](int lit) { return PositiveRef(lit) == var; })) {
    return;
  }
  DCHECK(
      absl::c_any_of(lin_max.exprs(), [var](const LinearExpressionProto& expr) {
        return absl::c_linear_search(expr.vars(), var);
      }));
  DCHECK(absl::c_linear_search(linear.vars(), var));

  const int equality_var_index =
      absl::c_find(linear.vars(), var) - linear.vars().begin();
  const int64_t equality_coeff = linear.coeffs(equality_var_index);
  if (std::abs(equality_coeff) != 1) return;
  if (context_->DomainOf(var).NumIntervals() != 1) {
    // Avoid creating linears with complex domains.
    return;
  }

  LinearArgumentProto* mutable_lin_max =
      context_->MutableConstraint(lin_max_ct_index)->mutable_lin_max();
  for (int i = 0; i < mutable_lin_max->exprs().size(); ++i) {
    LinearExpressionProto* expr = mutable_lin_max->mutable_exprs(i);
    int64_t var_coeff = 0;
    int new_size = 0;
    for (int j = 0; j < expr->vars_size(); ++j) {
      if (expr->vars(j) == var) {
        var_coeff = expr->coeffs(j);
      } else {
        expr->set_vars(new_size, expr->vars(j));
        expr->set_coeffs(new_size, expr->coeffs(j));
        ++new_size;
      }
    }
    if (new_size != expr->vars_size()) {
      expr->mutable_vars()->Truncate(new_size);
      expr->mutable_coeffs()->Truncate(new_size);
    }
    if (var_coeff != 0) {
      const int64_t multiplier = -var_coeff * equality_coeff;
      for (int j = 0; j < linear.vars().size(); ++j) {
        if (linear.vars(j) == var) {
          continue;
        }
        expr->add_vars(linear.vars(j));
        expr->add_coeffs(linear.coeffs(j) * multiplier);
      }
      expr->set_offset(expr->offset() - linear.domain(0) * multiplier);
      context_->CanonicalizeLinearExpression(
          context_->Constraint(lin_max_ct_index).enforcement_literal(), expr);
    }
  }
  context_->NewMappingConstraint(context_->Constraint(linear_ct_index),
                                 __FILE__, __LINE__);

  ConstraintProto* mut_lin = context_->MutableConstraint(linear_ct_index);
  int new_size = 0;
  for (int i = 0; i < mut_lin->linear().vars_size(); ++i) {
    if (mut_lin->linear().vars(i) != var) {
      mut_lin->mutable_linear()->set_vars(new_size, mut_lin->linear().vars(i));
      mut_lin->mutable_linear()->set_coeffs(new_size,
                                            mut_lin->linear().coeffs(i));
      ++new_size;
    }
  }
  mut_lin->mutable_linear()->mutable_vars()->Truncate(new_size);
  mut_lin->mutable_linear()->mutable_coeffs()->Truncate(new_size);
  const Domain linear_domain =
      context_->DomainOf(var)
          .MultiplicationBy(-equality_coeff)
          .AdditionWith(Domain(mut_lin->linear().domain(0)));
  FillDomainInProto(linear_domain, mut_lin->mutable_linear());

  // There is a good chance that the linear constraint is trivial now. Let's
  // handle it here instead of waiting for the fixpoint.
  bool changed = false;
  if (!constraint_presolver_->CanonicalizeLinear(mut_lin, &changed)) return;

  context_->UpdateConstraintVariableUsage(lin_max_ct_index);
  context_->UpdateConstraintVariableUsage(linear_ct_index);
  context_->MarkVariableAsRemoved(var);
  context_->UpdateRuleStats(
      "degree2: removed intermediate variable in lin_max and equality");
}

// We wait for the model expansion to take place in order to avoid removing
// encoding that will later be re-created during expansion.
void CpModelPresolver::LookAtVariableWithDegreeTwo(int var) {
  CHECK(RefIsPositive(var));
  if (context_->ModelIsUnsat()) return;
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
  if (context_->IsFixed(var)) return;
  if (!context_->ModelIsExpanded()) return;

  const auto& constraints = context_->VarToConstraints(var);
  CHECK_EQ(constraints.size(), 2);

  // For determinism.
  auto it = constraints.begin();
  int c1 = *it;
  int c2 = *(++it);
  if (c1 > c2) std::swap(c1, c2);
  if (c1 < 0) return;

  constexpr int s = kConstraintTypeBitSize;
  const int case_index = (context_->Constraint(c1).constraint_case() << s) +
                         context_->Constraint(c2).constraint_case();

  switch (case_index) {
    case (ConstraintProto::kIntProd << s) + ConstraintProto::kLinMax:
      return PresolveVarOnlyInIntProdAndLinMax(var, c1, c2);
    case (ConstraintProto::kLinMax << s) + ConstraintProto::kIntProd:
      return PresolveVarOnlyInIntProdAndLinMax(var, c2, c1);
    case (ConstraintProto::kLinear << s) + ConstraintProto::kLinear:
      return PresolveVarOnlyInLinearAndLinear(var, c1, c2);
    case (ConstraintProto::kLinMax << s) + ConstraintProto::kLinear:
      return PresolveVarOnlyInLinMaxAndLinear(var, c1, c2);
    case (ConstraintProto::kLinear << s) + ConstraintProto::kLinMax:
      return PresolveVarOnlyInLinMaxAndLinear(var, c2, c1);
    default:
      break;
  }
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
    const ConstraintProto& ct = context_->Constraint(c);
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
  int c1_ref = kint32min;
  const ConstraintProto& ct1 = context_->Constraint(c1);
  if (AtMostOneOrExactlyOneLiterals(ct1).size() <= 1) return;
  for (const int lit : AtMostOneOrExactlyOneLiterals(ct1)) {
    if (PositiveRef(lit) == var) {
      c1_ref = lit;
    } else {
      context_->tmp_literals.push_back(lit);
    }
  }
  int c2_ref = kint32min;
  const ConstraintProto& ct2 = context_->Constraint(c2);
  if (AtMostOneOrExactlyOneLiterals(ct2).size() <= 1) return;
  for (const int lit : AtMostOneOrExactlyOneLiterals(ct2)) {
    if (PositiveRef(lit) == var) {
      c2_ref = lit;
    } else {
      context_->tmp_literals.push_back(lit);
    }
  }
  DCHECK_NE(c1_ref, kint32min);
  DCHECK_NE(c2_ref, kint32min);
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
  const int new_ct_index = context_->NumConstraints();
  ConstraintProto* new_ct = context_->AddConstraint();
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

  context_->ClearConstraint(c1);
  context_->UpdateConstraintVariableUsage(c1);
  context_->ClearConstraint(c2);
  context_->UpdateConstraintVariableUsage(c2);

  context_->UpdateRuleStats(
      "at_most_one: resolved two constraints with opposite literal");
  context_->MarkVariableAsRemoved(var);

  // TODO(user): If the merged list contains duplicates or literal that are
  // negation of other, we need to deal with that right away. For some reason
  // something is not robust to that it seems. Investigate & fix!
  DCHECK_NE(new_ct->constraint_case(), ConstraintProto::CONSTRAINT_NOT_SET);
  if (constraint_presolver_->PresolveAtMostOrExactlyOne(
          new_ct, /*use_dual_reduction=*/true)) {
    context_->UpdateConstraintVariableUsage(new_ct_index);
  }
}

// TODO(user): The hint might get lost if the encoding was created during
// the presolve.
void CpModelPresolver::ProcessVariablesOnlyUsedInEncoding() {
  // TODO(user): We can still remove the variable even if we want to keep
  // all feasible solutions for the cases when we have a full encoding.
  // Similarly this shouldn't break symmetry, but we do need to do it for all
  // symmetric variable at once.
  if (context_->params().keep_all_feasible_solutions_in_presolve()) return;
  if (context_->params().keep_symmetry_in_presolve()) return;

  // TODO(user): In fixed search, we disable this rule because we don't update
  // the search strategy, but for some strategy we could.
  if (context_->params().search_branching() == SatParameters::FIXED_SEARCH) {
    return;
  }

  // For the corner case where a lot of variables are fixed/deleted and this is
  // called many time, it can make a big difference to skip variables that we
  // already know are not important.
  const int num_variables = context_->NumVariables();
  for (int var = encoding_tmp_num_vars_; var < num_variables; ++var) {
    encoding_tmp_vars_.push_back(var);
  }
  encoding_tmp_num_vars_ = num_variables;

  int new_size = 0;
  for (const int var : encoding_tmp_vars_) {
    if (context_->ModelIsUnsat()) return;
    if (context_->IsFixed(var)) continue;
    if (context_->VariableWasRemoved(var)) continue;
    if (context_->CanBeUsedAsLiteral(var)) continue;
    encoding_tmp_vars_[new_size++] = var;

    const bool is_only_used_in_encoding_and_maybe_objective =
        context_->VariableIsOnlyUsedInEncodingAndMaybeInObjective(var);
    if (is_only_used_in_encoding_and_maybe_objective) {
      // Process variables only used in encoding.
      const int old_num_constraints = context_->NumConstraints();
      TryToReplaceVariableByItsEncoding(var, context_, solution_crush_);

      // Presolve newly created constraints.
      for (int c = old_num_constraints; c < context_->NumConstraints(); ++c) {
        if (constraint_presolver_->PresolveOneConstraint(c)) {
          context_->UpdateConstraintVariableUsage(c);
        }
      }
      continue;
    }

    // Note that we don't run this if is_only_used_in_encoding is true.
    const bool is_only_used_in_linear1 =
        context_->VariableIsOnlyUsedInLinear1AndOneExtraConstraint(var);
    if (is_only_used_in_linear1) {
      VariableEncodingLocalModel local_model;
      local_model.var = var;
      local_model.single_constraint_using_the_var_outside_the_local_model = -1;
      local_model.var_in_more_than_one_constraint_outside_the_local_model =
          false;
      for (const int c : context_->VarToConstraints(var)) {
        if (c >= 0) {
          const ConstraintProto& ct = context_->Constraint(c);
          if (ct.constraint_case() == ConstraintProto::kLinear &&
              ct.linear().vars().size() == 1 && ct.linear().vars(0) == var) {
            local_model.linear1_constraints.push_back(c);
            continue;
          }
        }
        if (c == kObjectiveConstraint) {
          local_model.variable_coeff_in_objective =
              context_->ObjectiveMap().at(var);
        } else if (
            local_model
                    .single_constraint_using_the_var_outside_the_local_model ==
                -1 &&
            c >= 0) {
          // First "other" constraint.
          local_model.single_constraint_using_the_var_outside_the_local_model =
              c;
        } else {
          // We have a second "other" constraint.
          local_model.single_constraint_using_the_var_outside_the_local_model =
              -1;
          local_model.var_in_more_than_one_constraint_outside_the_local_model =
              true;
        }
      }

      MaybeTransferLinear1ToAnotherVariable(local_model, context_);
      continue;
    }
  }

  // Filter non-relevant variables.
  encoding_tmp_vars_.resize(new_size);
}

// Adds all affine relations to our model for the variables that are still used.
void CpModelPresolver::EncodeAllAffineRelations() {
  int64_t num_added = 0;
  for (int var = 0; var < context_->NumVariables(); ++var) {
    if (context_->IsFixed(var)) continue;

    const AffineRelation::Relation r = context_->GetAffineRelation(var);
    if (r.representative == var) continue;

    // TODO(user): It seems some affine relation are still removable at this
    // stage even though they should be removed inside PresolveToFixPoint().
    // Investigate. For now, we just remove such relations.
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (!constraint_presolver_->PresolveAffineRelationIfAny(var)) break;
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (context_->IsFixed(var)) continue;

    ++num_added;
    ConstraintProto* ct = context_->AddConstraint();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
  }

  // Now that we encoded all remaining affine relation with constraints, we
  // remove the special marker to have a proper constraint variable graph.
  context_->RemoveAllVariablesFromAffineRelationConstraint();

  if (num_added > 0) {
    SOLVER_LOG(logger_, num_added, " affine relations still in the model.");
  }
}

// Re-add to the queue the constraints that touch a variable that changed.
bool CpModelPresolver::ProcessChangedVariables(std::vector<bool>* in_queue,
                                               std::deque<int>* queue) {
  // TODO(user): Avoid reprocessing the constraints that changed the domain?
  if (context_->ModelIsUnsat()) return false;
  if (time_limit_->LimitReached()) return false;
  in_queue->resize(context_->NumConstraints(), false);
  const auto& vector_that_can_grow_during_iter =
      context_->modified_domains.PositionsSetAtLeastOnce();
  for (int i = 0; i < vector_that_can_grow_during_iter.size(); ++i) {
    const int v = vector_that_can_grow_during_iter[i];
    context_->modified_domains.Clear(v);
    if (context_->VariableIsNotUsedAnymore(v)) continue;
    if (context_->ModelIsUnsat()) return false;
    if (!constraint_presolver_->PresolveAffineRelationIfAny(v)) return false;
    if (context_->VariableIsNotUsedAnymore(v)) continue;

    constraint_presolver_->TryToSimplifyDomain(v);

    // TODO(user): Integrate these with TryToSimplifyDomain().
    if (context_->ModelIsUnsat()) return false;

    if (!context_->CanonicalizeOneObjectiveVariable(v)) return false;

    in_queue->resize(context_->NumConstraints(), false);
    const int size_before = queue->size();
    for (const int c : context_->VarToConstraints(v)) {
      if (c >= 0 && !(*in_queue)[c]) {
        (*in_queue)[c] = true;
        queue->push_back(c);
      }
    }

    // Make sure the order is deterministic! because var_to_constraints[]
    // order changes from one run to the next.
    std::sort(queue->begin() + size_before, queue->end());
  }
  context_->modified_domains.ResetAllToFalse();
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
          : kint64max;

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  absl::flat_hash_set<std::pair<int, int>> var_constraint_pair_already_called;

  // The queue of "active" constraints, initialized to the non-empty ones.
  std::vector<bool> in_queue(context_->NumConstraints(), false);
  std::deque<int> queue;
  for (int c = 0; c < in_queue.size(); ++c) {
    if (context_->Constraint(c).constraint_case() !=
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
    std::shuffle(queue.begin(), queue.end(), context_->random());
  } else {
    if (queue.size() > context_->NumVariables()) {
      // We do a radix-sort if there are more constraints than variables, it can
      // be way faster when there is a lot of constraints.
      CompactVectorVectorBuilder<int, int> builder;
      builder.ReserveNumItems(queue.size());
      for (const int c : queue) {
        builder.Add(context_->ConstraintToVars(c).size(), c);
      }
      CompactVectorVector<int, int> by_arity;
      by_arity.ResetFromBuilder(builder);
      queue.assign(by_arity.AllValuesSortedByKey().begin(),
                   by_arity.AllValuesSortedByKey().end());
    } else {
      // Normal sort.
      std::sort(queue.begin(), queue.end(), [this](int a, int b) {
        const int score_a = context_->ConstraintToVars(a).size();
        const int score_b = context_->ConstraintToVars(b).size();
        return score_a < score_b || (score_a == score_b && a < b);
      });
    }
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

      const int old_num_constraint = context_->NumConstraints();
      const bool changed = constraint_presolver_->PresolveOneConstraint(c);
      if (context_->ModelIsUnsat()) {
        SOLVER_LOG(logger_, "Unsat after presolving constraint #", c,
                   " (warning, dump might be inconsistent): ",
                   ProtobufShortDebugString(context_->Constraint(c)));
      }

      // Add to the queue any newly created constraints.
      const int new_num_constraints = context_->NumConstraints();
      if (new_num_constraints > old_num_constraint) {
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

    in_queue.resize(context_->NumConstraints(), false);
    const auto& vector_that_can_grow_during_iter =
        context_->MutableVarWithReducedSmallDegree()->PositionsSetAtLeastOnce();
    for (int i = 0; i < vector_that_can_grow_during_iter.size(); ++i) {
      const int v = vector_that_can_grow_during_iter[i];
      if (context_->VariableIsNotUsedAnymore(v)) continue;

      // Remove the variable from the set to allow it to be pushed again.
      // This is necessary since a few affine logic needs to add the same
      // variable back to a second pass of processing.
      context_->MutableVarWithReducedSmallDegree()->Clear(v);

      // Make sure all affine relations are propagated.
      // This also remove the relation if the degree is now one.
      if (context_->ModelIsUnsat()) return;
      if (!constraint_presolver_->PresolveAffineRelationIfAny(v)) return;

      const int degree = context_->VarToConstraints(v).size();
      if (degree == 0) continue;
      if (degree == 2) LookAtVariableWithDegreeTwo(v);
      if (degree == 2 || degree == 3) {
        // Tricky: this function can add new constraint.
        ProcessVariableInTwoAtMostOrExactlyOne(v);
        in_queue.resize(context_->NumConstraints(), false);
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
    context_->MutableVarWithReducedSmallDegree()->ResetAllToFalse();

    if (ProcessChangedVariables(&in_queue, &queue)) continue;

    DCHECK(!context_->HasUnusedAffineVariable());

    // Deal with integer variable only appearing in an encoding.
    if (!context_->CanonicalizeObjective()) return;
    ProcessVariablesOnlyUsedInEncoding();
    if (ProcessChangedVariables(&in_queue, &queue)) continue;

    // Perform dual reasoning.
    //
    // TODO(user): We can support assumptions but we need to not cut them out
    // of the feasible region.
    if (context_->params().keep_all_feasible_solutions_in_presolve()) break;
    if (!context_->WorkingModel().assumptions().empty()) break;

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
  if (!time_limit_->LimitReached()) {
    const int num_constraints = context_->NumConstraints();
    TimeLimitCheckEveryNCalls bool_or_check_time_limit(100, time_limit_);
    for (int c = 0; c < num_constraints; ++c) {
      ConstraintProto* ct = context_->MutableConstraint(c);
      // We don't want to check the time limit at each "small" constraint as
      // there could be many.
      bool check_time_limit = false;

      switch (ct->constraint_case()) {
        case ConstraintProto::kNoOverlap:
          // Filter out absent intervals.
          if (constraint_presolver_->PresolveNoOverlap(ct)) {
            context_->UpdateConstraintVariableUsage(c);
          }
          check_time_limit = true;
          break;
        case ConstraintProto::kNoOverlap2D:
          // Filter out absent intervals.
          if (constraint_presolver_->PresolveNoOverlap2D(c, ct)) {
            context_->UpdateConstraintVariableUsage(c);
          }
          check_time_limit = true;
          break;
        case ConstraintProto::kCumulative:
          // Filter out absent intervals.
          if (constraint_presolver_->PresolveCumulative(ct)) {
            context_->UpdateConstraintVariableUsage(c);
          }
          check_time_limit = true;
          break;
        case ConstraintProto::kBoolOr:
          if (ct->enforcement_literal().empty()) {
            // Try to infer domain reductions from clauses and the saved
            // "implies in domain" relations.
            for (const auto& pair :
                 context_->deductions.ProcessClause(ct->bool_or().literals())) {
              bool modified = false;
              if (!context_->IntersectDomainWith(pair.first, pair.second,
                                                 &modified)) {
                return;
              }
              if (modified) {
                context_->UpdateRuleStats(
                    "deductions: reduced variable domain");
              }
            }
            if (bool_or_check_time_limit.LimitReached())
              check_time_limit = true;
          }
          break;
        default:
          break;
      }
      if (check_time_limit && time_limit_->LimitReached()) break;
    }
  }

  timer.AddCounter("num_loops", num_loops);
  timer.AddCounter("num_dual_strengthening", num_dual_strengthening);
  context_->deductions.MarkProcessingAsDoneForNow();
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
  const int num_variables = context_->NumVariables();
  std::vector<int> bool_or_indices;
  std::vector<int64_t> literal_score(2 * num_variables, 0);
  const auto get_index = [](int ref) {
    return 2 * PositiveRef(ref) + (RefIsPositive(ref) ? 0 : 1);
  };

  int64_t num_collisions = 0;
  int64_t num_merges = 0;
  int64_t num_saved_literals = 0;
  ClauseWithOneMissingHasher hasher(context_->random());
  const int num_constraints = context_->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    ConstraintProto* ct = context_->MutableConstraint(c);
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
          ConstraintProto* other_ct = context_->MutableConstraint(it->second);
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
    ConstraintProto* ct = context_->MutableConstraint(c);

    bool merged = false;
    timer.TrackSimpleLoop(ct->bool_or().literals().size());
    if (timer.WorkLimitIsReached()) break;
    for (const int ref : ct->bool_or().literals()) {
      const uint64_t hash = hasher.HashWithout(c, ref);
      const auto it = bool_and_map.find(hash);
      if (it != bool_and_map.end()) {
        ++num_collisions;
        const int base_c = it->second;
        auto* and_ct = context_->MutableConstraint(base_c);
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
    ConstraintProto* ct = context_->MutableConstraint(c);
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
      solution_crush_(context->solution_crush()),
      constraint_presolver_(std::make_unique<CpConstraintPresolver>(context)),
      logger_(context->logger()),
      time_limit_(context->time_limit()) {}

CpSolverStatus CpModelPresolver::InfeasibleStatus() {
  if (logger_->LoggingIsEnabled()) context_->LogInfo();
  return CpSolverStatus::INFEASIBLE;
}

// At the end of presolve, the mapping model is initialized to contains all
// the variable from the original model + the one created during presolve
// expand. It also contains the tightened domains.
namespace {
void InitializeMappingModelVariables(absl::Span<const Domain> domains,
                                     std::vector<int>* fixed_postsolve_mapping,
                                     CpModelProto* mapping_proto) {
  // Extend the fixed mapping to take into account all newly created variable
  // since the time it was constructed.
  int old_num_variables = mapping_proto->variables().size();
  while (fixed_postsolve_mapping->size() < domains.size()) {
    mapping_proto->add_variables();
    fixed_postsolve_mapping->push_back(old_num_variables++);
    DCHECK_EQ(old_num_variables, mapping_proto->variables().size());
  }

  // Overwrite the domains.
  //
  // Note that if the fixed_postsolve_mapping was not null, the mapping model
  // should contains the original variable domains at the time the fixed mapping
  // was computed.
  for (int i = 0; i < domains.size(); ++i) {
    FillDomainInProto(domains[i], mapping_proto->mutable_variables(
                                      (*fixed_postsolve_mapping)[i]));
  }

  // Remap the mapping proto.
  // We only deal with constraint here, do not touch the rest.
  //
  // TODO(user): Maybe we should have a real "postsolve" proto so we can
  // interleave postsolve "constraint" and remapping phase. This would allow to
  // do that in the middle of the presolve. But maybe this is not as impactful.
  auto mapping_function = [fixed_postsolve_mapping](int* ref) {
    const int image = (*fixed_postsolve_mapping)[PositiveRef(*ref)];
    CHECK_GE(image, 0);
    *ref = RefIsPositive(*ref) ? image : NegatedRef(image);
  };
  for (ConstraintProto& ct_ref : *mapping_proto->mutable_constraints()) {
    ApplyToAllVariableIndices(mapping_function, &ct_ref);
    ApplyToAllLiteralIndices(mapping_function, &ct_ref);
  }
}
}  // namespace

void CpModelPresolver::ExpandCpModelAndCanonicalizeConstraints() {
  const int num_constraints_before_expansion = context_->NumConstraints();
  ExpandCpModel(context_);
  if (context_->ModelIsUnsat()) return;

  // TODO(user): Make sure we can't have duplicate in these constraint.
  // These are due to ExpandCpModel() were we create such constraint with
  // duplicate. The problem is that some code assumes these are presolved
  // before being called.
  const int num_constraints = context_->NumConstraints();
  for (int c = num_constraints_before_expansion; c < num_constraints; ++c) {
    const auto type = context_->Constraint(c).constraint_case();
    if (type == ConstraintProto::kAtMostOne ||
        type == ConstraintProto::kExactlyOne) {
      if (constraint_presolver_->PresolveOneConstraint(c)) {
        context_->UpdateConstraintVariableUsage(c);
      }
      if (context_->ModelIsUnsat()) return;
    } else if (type == ConstraintProto::kLinear) {
      bool changed = false;
      if (!constraint_presolver_->CanonicalizeLinear(
              context_->MutableConstraint(c), &changed)) {
        return;
      }
      if (changed) {
        context_->UpdateConstraintVariableUsage(c);
      }
    }
  }
}

namespace {

// Canonicalizes the routes constraints node expressions. In particular,
// replaces the variables in these expressions with their representative.
void CanonicalizeRoutesConstraintNodeExpressions(PresolveContext* context) {
  const int num_constraints = context->NumConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    if (context->Constraint(c).constraint_case() != ConstraintProto::kRoutes) {
      continue;
    }
    for (RoutesConstraintProto::NodeExpressions& node_exprs :
         *context->MutableConstraint(c)
              ->mutable_routes()
              ->mutable_dimensions()) {
      for (LinearExpressionProto& expr : *node_exprs.mutable_exprs()) {
        context->CanonicalizeLinearExpression({}, &expr);
      }
    }
  }
}

}  // namespace

bool CpModelPresolver::CanonicalizeAllLinears() {
  if (context_->ModelIsUnsat()) return false;
  std::queue<int> constraints_to_process;
  absl::flat_hash_set<int> queued;
  for (int c = 0; c < context_->NumConstraints(); ++c) {
    ConstraintProto& ct = *context_->MutableConstraint(c);
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    for (const int v : ct.linear().vars()) {
      if (context_->IsFixed(v)) {
        constraints_to_process.push(c);
        queued.insert(c);
        break;
      }
    }
  }
  std::vector<int> non_fixed_vars;
  while (!constraints_to_process.empty()) {
    const int c = constraints_to_process.front();
    constraints_to_process.pop();
    queued.erase(c);

    ConstraintProto& ct = *context_->MutableConstraint(c);
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;

    bool has_fixed_vars = false;
    non_fixed_vars.clear();
    for (const int v : UsedVariables(ct)) {
      if (context_->IsFixed(v)) {
        has_fixed_vars = true;
      } else {
        non_fixed_vars.push_back(v);
      }
    }
    if (!has_fixed_vars) continue;
    bool changed = false;
    if (!constraint_presolver_->CanonicalizeLinear(&ct, &changed)) {
      return false;
    }
    DCHECK(changed);  // It should change, since it has fixed variables.
    context_->UpdateConstraintVariableUsage(c);
    for (const int v : non_fixed_vars) {
      if (context_->IsFixed(v)) {
        // Canonicalizing the linear fixed something!
        for (const int c : context_->VarToConstraints(v)) {
          if (c > 0 && queued.insert(c).second) {
            constraints_to_process.push(c);
          }
        }
      }
    }
  }
  return true;
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
  // Initialize the initial context.working_model domains.
  //
  // Note that we did some basic presolving during the first copy of the model.
  // This is important since initializing the constraint <-> variable graph can
  // be costly, so better to remove trivially feasible constraint for instance.
  context_->InitializeNewDomains();

  if (context_->params().cp_model_pure_sat_presolve()) {
    if (!PresolvePureSatProblem()) {
      (void)context_->NotifyThatModelIsUnsat(
          "Proved Infeasible during SAT presolve");
      return InfeasibleStatus();
    }

    // Sync the domains and initialize the mapping model variables.
    context_->WriteVariableDomainsToProto();

    // Starts the postsolve mapping model.
    std::vector<int> fixed_postsolve_mapping;
    InitializeMappingModelVariables(context_->AllDomains(),
                                    &fixed_postsolve_mapping,
                                    context_->mapping_model);

    // Remove all the unused variables from the presolved model.
    postsolve_mapping_->clear();
    std::vector<int> mapping(context_->NumVariables(), -1);
    int num_unused_variables = 0;
    for (int i = 0; i < context_->NumVariables(); ++i) {
      if (mapping[i] != -1) continue;  // Already mapped.
      if (context_->VariableWasRemoved(i)) continue;

      // Deal with unused variables.
      //
      // If the variable is not fixed, we have multiple feasible solutions for
      // this variable, so we can't remove it if we want all of them.
      if (context_->VariableIsNotUsedAnymore(i) &&
          (!context_->params().keep_all_feasible_solutions_in_presolve() ||
           context_->IsFixed(i))) {
        // Tricky. Variables that were not removed by a presolve rule should be
        // fixed first during postsolve, so that more complex postsolve rules
        // can use their values. One way to do that is to fix them here.
        //
        // We prefer to fix them to zero if possible.
        ++num_unused_variables;
        FillDomainInProto(Domain(context_->DomainOf(i).SmallestValue()),
                          context_->mapping_model->mutable_variables(
                              fixed_postsolve_mapping[i]));
        continue;
      }
      mapping[i] = postsolve_mapping_->size();
      postsolve_mapping_->push_back(fixed_postsolve_mapping[i]);
    }
    context_->UpdateRuleStats(absl::StrCat("presolve: ", num_unused_variables,
                                           " unused variables removed."));

    MaybePermuteVariablesRandomly(mapping);

    DCHECK(context_->ConstraintVariableUsageIsConsistent());
    const int old_size = postsolve_mapping_->size();
    ApplyVariableMapping(absl::MakeSpan(mapping),
                         context_->UnsafeMutableWorkingModel(),
                         postsolve_mapping_, context_->solution_crush());
    CHECK_EQ(old_size, postsolve_mapping_->size());
    if (context_->lrat_proof_handler != nullptr) {
      context_->lrat_proof_handler->RemapBooleanVariables(
          absl::MakeSpan(*postsolve_mapping_));
    }

    // Compact all non-empty constraint at the beginning.
    constraint_presolver_->RemoveEmptyConstraints();

    return LogAndValidatePresolvedModel();
  }

  // If the objective is a floating point one, we scale it.
  //
  // TODO(user): We should probably try to delay this even more. For that we
  // just need to isolate more the "dual" reduction that usually need to look at
  // the objective.
  if (context_->WorkingModel().has_floating_point_objective()) {
    context_->WriteVariableDomainsToProto();
    if (!ScaleFloatingPointObjective(context_->params(), logger_,
                                     context_->UnsafeMutableWorkingModel())) {
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
        context_->WorkingModel().objective();
  }

  context_->InitializeNewDomains();
  if (!context_->solution_crush().SolutionIsLoaded()) {
    context_->LoadAndClampSolutionHint();
  }

  // If there is a large proprotion of fixed variables, lets remap the model
  // before we start the actual presolve. This is useful for LNS in particular.
  //
  // fixed_postsolve_mapping[i] will contains the original index of the variable
  // that will be at position i after MaybeRemoveFixedVariables(). If the
  // mapping is left empty, it will be set to the identity mapping later by
  // InitializeMappingModelVariables().
  //
  // TODO(user): Integrate this with first copy of the main model instead ? It
  // needs a bit more work since we don't currently canonicalize and remap at
  // the same time.
  std::vector<int> fixed_postsolve_mapping;
  if (!MaybeRemoveFixedVariables(&fixed_postsolve_mapping)) {
    return InfeasibleStatus();
  }

  context_->InitializeNewDomains();
  context_->ReadObjectiveFromProto();
  if (!context_->CanonicalizeObjective()) return InfeasibleStatus();

  context_->RegisterVariablesUsedInAssumptions();
  DCHECK(context_->ConstraintVariableUsageIsConsistent());

  // If presolve is false, just run expansion.
  if (!context_->params().cp_model_presolve()) {
    ExpandCpModelAndCanonicalizeConstraints();
    if (!CanonicalizeAllLinears()) return InfeasibleStatus();
    context_->WriteHintToProto();
    if (context_->ModelIsUnsat()) return InfeasibleStatus();

    // We still write back the canonical objective has we don't deal well
    // with uninitialized domain or duplicate variables.
    if (context_->WorkingModel().has_objective()) {
      context_->WriteObjectiveToProto();
    }

    // We need to append all the variable equivalence that are still used!
    EncodeAllAffineRelations();

    // Make sure we also have an initialized mapping model as we use this for
    // filling the tightened variables. Even without presolve, we do some
    // trivial presolving during the initial copy of the model, and expansion
    // might do more.
    context_->WriteVariableDomainsToProto();
    InitializeMappingModelVariables(context_->AllDomains(),
                                    &fixed_postsolve_mapping,
                                    context_->mapping_model);

    // We don't want to run postsolve when the presolve is disabled, but the
    // expansion might have added some constraints to the mapping model. To
    // restore correctness, we merge them with the working model.
    if (!context_->mapping_model->constraints().empty()) {
      context_->UpdateRuleStats(
          "TODO: mapping model not empty with presolve disabled");
      for (const ConstraintProto& ct : context_->mapping_model->constraints()) {
        *context_->AddConstraint() = ct;
      }
      context_->mapping_model->clear_constraints();
    }

    if (logger_->LoggingIsEnabled()) context_->LogInfo();
    return CpSolverStatus::UNKNOWN;
  }

  // Presolve all variable domain once. The PresolveToFixPoint() function will
  // only reprocess domain that changed.
  if (context_->ModelIsUnsat()) return InfeasibleStatus();
  for (int var = 0; var < context_->NumVariables(); ++var) {
    if (context_->VariableIsNotUsedAnymore(var)) continue;
    if (!constraint_presolver_->PresolveAffineRelationIfAny(var)) {
      return InfeasibleStatus();
    }

    // Try to canonicalize the domain, note that we should have detected all
    // affine relations before, so we don't recreate "canononical" variables
    // if they already exist in the model.
    constraint_presolver_->TryToSimplifyDomain(var);
    if (context_->ModelIsUnsat()) return InfeasibleStatus();
  }
  if (!context_->CanonicalizeObjective()) return InfeasibleStatus();

  // Main propagation loop.
  for (int iter = 0; iter < context_->params().max_presolve_iterations();
       ++iter) {
    if (time_limit_->LimitReached()) break;
    context_->UpdateRuleStats("presolve: iteration");
    const int64_t old_num_presolve_op = context_->num_presolve_operations;

    // Propagate the objective.
    if (!PropagateObjective()) return InfeasibleStatus();

    // TODO(user): The presolve transformations we do after this is called might
    // result in even more presolve if we were to call this again! improve the
    // code. See for instance plusexample_6_sat.fzn were represolving the
    // presolved problem reduces it even more.
    PresolveToFixPoint();
    DCHECK(context_->ConstraintVariableUsageIsConsistent());

    // Call expansion.
    if (!context_->ModelIsExpanded()) {
      ExtractEncodingFromLinear();
      ExpandCpModelAndCanonicalizeConstraints();
      if (context_->ModelIsUnsat()) return InfeasibleStatus();

      // We need to re-evaluate the degree because some presolve rule only
      // run after expansion.
      const int num_vars = context_->NumVariables();
      for (int var = 0; var < num_vars; ++var) {
        if (context_->VarToConstraints(var).size() <= 3) {
          context_->MutableVarWithReducedSmallDegree()->Set(var);
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
        //
        // Note that this will always create a "symmetry" message even if it is
        // empty. We use that to know that symmetry where computed, and there is
        // none.
        if (!context_->WorkingModel().has_symmetry()) {
          DetectAndAddSymmetryToProto(context_->params(),
                                      context_->WorkingModel(),
                                      context_->MutableWorkingModelSymmetry(),
                                      logger_, context_->time_limit());
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
              "Proved Infeasible during SAT presolve");
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
      const int old_size = context_->NumConstraints();
      for (int c = 0; c < old_size; ++c) {
        ConstraintProto* ct = context_->MutableConstraint(c);
        if (ct->constraint_case() != ConstraintProto::kLinear) continue;
        ExtractAtMostOneFromLinear(ct);
      }
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
    DetectUnenforcedEnforcedLinearPair();
    DetectDominatedLinearConstraints();
    DetectDifferentVariables();
    ProcessSetPPC();
    TransformClausesToExactlyOne();
    SplitNoOverlapAndCumulativeConstraints();

    if (!time_limit_->LimitReached() &&
        context_->params().detect_encoded_complex_domain()) {
      DetectEncodedComplexDomains(context_);
    }

    // These operations might break symmetry. Or at the very least, the newly
    // created variable must be incorporated in the generators.
    if (context_->params().find_big_linear_overlap() &&
        !context_->params().keep_symmetry_in_presolve()) {
      FindAlmostIdenticalLinearConstraints();

      ActivityBoundHelper activity_amo_helper;
      activity_amo_helper.AddAllAtMostOnes(context_->WorkingModel());
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

  if (!MergeNoOverlapConstraints()) return InfeasibleStatus();
  if (!MergeNoOverlap2DConstraints()) return InfeasibleStatus();

  // Tries to spread the objective amongst many variables.
  // We re-do a canonicalization with the final linear expression.
  if (context_->WorkingModel().has_objective()) {
    if (!context_->params().keep_symmetry_in_presolve()) {
      ExpandObjective();
      if (!context_->modified_domains.PositionsSetAtLeastOnce().empty()) {
        // If we have fixed variables or created new affine relations, there
        // might be more things to presolve.
        PresolveToFixPoint();
      }
      if (context_->ModelIsUnsat()) return InfeasibleStatus();
      ShiftObjectiveWithExactlyOnes();
      if (context_->ModelIsUnsat()) return InfeasibleStatus();
    }
  }

  // Now that everything that could possibly be fixed was fixed, make sure we
  // don't leave any linear constraint with fixed variables.
  if (!CanonicalizeAllLinears()) return InfeasibleStatus();

  // Take care of linear constraint with a complex rhs.
  FinalExpansionForLinearConstraint(context_);

  // Adds all needed affine relation to working_model.
  EncodeAllAffineRelations();
  if (context_->ModelIsUnsat()) return InfeasibleStatus();

  // If we have symmetry information, lets filter it.
  if (context_->WorkingModel().has_symmetry()) {
    if (!FilterOrbitOnUnusedOrFixedVariables(
            context_->MutableWorkingModelSymmetry(), context_)) {
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
       *context_->UnsafeMutableWorkingModel()->mutable_search_strategy()) {
    CHECK(strategy.variables().empty());
    if (strategy.exprs().empty()) continue;

    // Canonicalize each expression to use affine representative.
    ConstraintProto empy_enforcement;
    for (LinearExpressionProto& expr : *strategy.mutable_exprs()) {
      constraint_presolver_->CanonicalizeLinearExpression(empy_enforcement,
                                                          &expr);
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
  context_->WriteVariableDomainsToProto();

  // Some vars may have been fixed by the affine relations. This may can impact
  // the objective. Let's re-do the canonicalization.
  if (context_->WorkingModel().has_objective()) {
    // We re-do a canonicalization with the final linear expression.
    if (!context_->CanonicalizeObjective()) return InfeasibleStatus();
    context_->WriteObjectiveToProto();
    DCHECK(absl::c_all_of(
        context_->WorkingModel().objective().vars(),
        [ctx = context_](int var) { return !ctx->IsFixed(var); }));
  }

  // Starts the postsolve mapping model.
  InitializeMappingModelVariables(context_->AllDomains(),
                                  &fixed_postsolve_mapping,
                                  context_->mapping_model);

  // Remove all the unused variables from the presolved model.
  postsolve_mapping_->clear();
  std::vector<int> mapping(context_->NumVariables(), -1);
  absl::flat_hash_map<int64_t, int> constant_to_index;
  int num_unused_variables = 0;
  for (int i = 0; i < context_->NumVariables(); ++i) {
    if (mapping[i] != -1) continue;  // Already mapped.

    if (context_->VariableWasRemoved(i)) {
      // Heuristic: If a variable is removed and has a representative that is
      // not, we "move" the representative to the spot of that variable in the
      // original order. This is to preserve any info encoded in the variable
      // order by the modeler.
      const int r = PositiveRef(context_->GetAffineRelation(i).representative);
      if (mapping[r] == -1 && !context_->VariableIsNotUsedAnymore(r)) {
        mapping[r] = postsolve_mapping_->size();
        postsolve_mapping_->push_back(fixed_postsolve_mapping[r]);
      }
      continue;
    }

    // Deal with unused variables.
    //
    // If the variable is not fixed, we have multiple feasible solutions for
    // this variable, so we can't remove it if we want all of them.
    if (context_->VariableIsNotUsedAnymore(i) &&
        (!context_->params().keep_all_feasible_solutions_in_presolve() ||
         context_->IsFixed(i))) {
      // Tricky. Variables that were not removed by a presolve rule should be
      // fixed first during postsolve, so that more complex postsolve rules
      // can use their values. One way to do that is to fix them here.
      //
      // We prefer to fix them to zero if possible.
      ++num_unused_variables;
      FillDomainInProto(Domain(context_->DomainOf(i).SmallestValue()),
                        context_->mapping_model->mutable_variables(
                            fixed_postsolve_mapping[i]));
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
    postsolve_mapping_->push_back(fixed_postsolve_mapping[i]);
  }
  context_->UpdateRuleStats(absl::StrCat("presolve: ", num_unused_variables,
                                         " unused variables removed."));

  MaybePermuteVariablesRandomly(mapping);

  DCHECK(context_->ConstraintVariableUsageIsConsistent());
  CanonicalizeRoutesConstraintNodeExpressions(context_);
  context_->WriteHintToProto();

  // Context shouldn't really be used after this since everything was remapped.
  const int old_size = postsolve_mapping_->size();
  ApplyVariableMapping(absl::MakeSpan(mapping),
                       context_->UnsafeMutableWorkingModel(),
                       postsolve_mapping_, context_->solution_crush());
  CHECK_EQ(old_size, postsolve_mapping_->size());

  // Compact all non-empty constraint at the beginning.
  constraint_presolver_->RemoveEmptyConstraints();

  // Hack to display the number of deductions stored.
  if (context_->deductions.NumDeductions() > 0) {
    context_->UpdateRuleStats(absl::StrCat(
        "deductions: ", context_->deductions.NumDeductions(), " stored"));
  }
  return LogAndValidatePresolvedModel();
}

void CpModelPresolver::MaybePermuteVariablesRandomly(
    std::vector<int>& mapping) {
  if (!context_->params().permute_variable_randomly()) return;
  // The mapping might merge variable, so we have to be careful here.
  const int n = postsolve_mapping_->size();
  std::vector<int> perm(n);
  std::iota(perm.begin(), perm.end(), 0);
  std::shuffle(perm.begin(), perm.end(), context_->random());
  for (int i = 0; i < context_->NumVariables(); ++i) {
    if (mapping[i] != -1) mapping[i] = perm[mapping[i]];
  }
  std::vector<int> new_postsolve_mapping(n);
  for (int i = 0; i < n; ++i) {
    new_postsolve_mapping[perm[i]] = (*postsolve_mapping_)[i];
  }
  *postsolve_mapping_ = std::move(new_postsolve_mapping);
}

CpSolverStatus CpModelPresolver::LogAndValidatePresolvedModel() {
  // Stats and checks.
  if (logger_->LoggingIsEnabled()) context_->LogInfo();

  // This is not supposed to happen, and is more indicative of an error than an
  // INVALID model. But for our no-overflow preconditions, we might run into bad
  // situation that causes the final model to be invalid.
  {
    const std::string error =
        ValidateCpModel(context_->WorkingModel(), /*after_presolve=*/true);
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

void ApplyVariableMapping(absl::Span<int> mapping, CpModelProto* cp_model,
                          std::vector<int>* reverse_mapping,
                          SolutionCrush& solution_crush) {
  // Remap all the variable/literal references in the constraints and the
  // enforcement literals in the variables.
  const auto mapping_function = [&mapping, &reverse_mapping](int* ref) {
    const int var = PositiveRef(*ref);
    int image = mapping[var];
    if (image < 0) {
      // We extend the mapping if this variable is still used.
      image = mapping[var] = reverse_mapping->size();
      reverse_mapping->push_back(var);
    }
    *ref = RefIsPositive(*ref) ? image : NegatedRef(image);
  };
  for (ConstraintProto& ct_ref : *cp_model->mutable_constraints()) {
    ApplyToAllVariableIndices(mapping_function, &ct_ref);
    ApplyToAllLiteralIndices(mapping_function, &ct_ref);
    if (ct_ref.constraint_case() == ConstraintProto::kRoutes) {
      for (RoutesConstraintProto::NodeExpressions& node_exprs :
           *ct_ref.mutable_routes()->mutable_dimensions()) {
        for (LinearExpressionProto& expr : *node_exprs.mutable_exprs()) {
          if (expr.vars().empty()) continue;
          CHECK_EQ(expr.vars().size(), 1);
          CHECK(RefIsPositive(expr.vars(0)));
          const int var = expr.vars(0);
          const auto& definition = cp_model->variables(var);
          const int64_t min = definition.domain(0);
          const int64_t max = definition.domain(definition.domain().size() - 1);
          if (min == max) {
            expr.set_offset(expr.offset() + min * expr.coeffs(0));
            expr.clear_vars();
            expr.clear_coeffs();
            continue;
          }
          const int image = mapping[var];
          if (image < 0) {
            // TODO(user): is this correct? may this lead to incorrect cuts
            // in routing_cuts.cc in some cases?
            expr.clear_vars();
            expr.clear_coeffs();
            continue;
          }
          expr.set_vars(0, image);
        }
      }
    }
  }

  // Remap the objective variables.
  if (cp_model->has_objective()) {
    for (int& mutable_ref : *cp_model->mutable_objective()->mutable_vars()) {
      mapping_function(&mutable_ref);
    }
  }

  // Remap the assumptions.
  for (int& mutable_ref : *cp_model->mutable_assumptions()) {
    mapping_function(&mutable_ref);
  }

  // Remap the symmetries. Note that we should have properly dealt with fixed
  // orbit and such in FilterOrbitOnUnusedOrFixedVariables().
  if (cp_model->has_symmetry()) {
    for (SparsePermutationProto& generator :
         *cp_model->mutable_symmetry()->mutable_permutations()) {
      for (int& var : *generator.mutable_support()) {
        mapping_function(&var);
      }
    }

    // We clear the orbitope info (we don't really use it after presolve).
    cp_model->mutable_symmetry()->clear_orbitopes();
  }

  // Note: For the rest of the mapping, if mapping[i] is -1, we can just ignore
  // the variable instead of trying to map it.

  // Remap the search decision heuristic.
  // Note that we delete any heuristic related to a removed variable.
  for (DecisionStrategyProto& strategy : *cp_model->mutable_search_strategy()) {
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
    for (const DecisionStrategyProto& strategy : cp_model->search_strategy()) {
      if (strategy.exprs().empty()) continue;
      *cp_model->mutable_search_strategy(new_size++) = strategy;
    }
    google::protobuf::util::Truncate(cp_model->mutable_search_strategy(),
                                     new_size);
  }

  // Remap the solution hint.
  solution_crush.RemapVariables(mapping);
  solution_crush.StoreSolutionAsHint(*cp_model);

  // Move the variable definitions.
  google::protobuf::RepeatedPtrField<IntegerVariableProto>
      new_variables_storage;
  google::protobuf::RepeatedPtrField<IntegerVariableProto>* new_variables;
  if (cp_model->GetArena() == nullptr) {
    new_variables = &new_variables_storage;
  } else {
    new_variables = google::protobuf::Arena::Create<
        google::protobuf::RepeatedPtrField<IntegerVariableProto>>(
        cp_model->GetArena());
  }
  for (int i = 0; i < mapping.size(); ++i) {
    const int image = mapping[i];
    if (image < 0) continue;
    while (image >= new_variables->size()) {
      new_variables->Add();
    }
    (*new_variables)[image].Swap(cp_model->mutable_variables(i));
  }
  cp_model->mutable_variables()->Swap(new_variables);

  // Check that all variables have a non-empty domain.
  for (const IntegerVariableProto& v : cp_model->variables()) {
    CHECK_GT(v.domain_size(), 0);
  }
}

bool CpModelPresolver::MaybeRemoveFixedVariables(
    std::vector<int>* postsolve_mapping) {
  postsolve_mapping->clear();
  if (!context_->params().remove_fixed_variables_early()) return true;
  if (!context_->params().cp_model_presolve()) return true;

  // This is supposed to be already called, but it is a no-opt if this was the
  // case, and it comment nicely that we do require domains to be up to date
  // in the context.
  context_->InitializeNewDomains();
  if (context_->ModelIsUnsat()) return false;

  // Initialize the mapping to remove all fixed variables.
  const int num_vars = context_->NumVariables();
  std::vector<int> mapping(num_vars, -1);
  for (int i = 0; i < num_vars; ++i) {
    if (context_->IsFixed(i)) continue;
    mapping[i] = postsolve_mapping->size();
    postsolve_mapping->push_back(i);
  }

  // Lets only do this if the proportion of fixed variables is large enough.
  const int num_fixed = num_vars - postsolve_mapping->size();
  if (num_fixed < 1000 || num_fixed * 2 <= num_vars) {
    postsolve_mapping->clear();
    return true;
  }

  // TODO(user): Right now the copy does not remove fixed variables from the
  // objective, but ReadObjectiveFromProto() does it. Maybe we should just not
  // copy them in the first place.
  if (context_->WorkingModel().has_objective()) {
    context_->ReadObjectiveFromProto();
    if (!context_->CanonicalizeObjective()) return false;
    if (!PropagateObjective()) return false;
    if (context_->ModelIsUnsat()) return false;
    context_->WriteObjectiveToProto();
  }

  // Copy the current domains into the mapping model.
  // Note that we are not sure the domain where properly written.
  context_->WriteVariableDomainsToProto();
  *context_->mapping_model->mutable_variables() =
      context_->WorkingModel().variables();

  SOLVER_LOG(logger_, "Large number of fixed variables ",
             FormatCounter(num_fixed), " / ", FormatCounter(num_vars),
             ", doing a first remapping phase to go down to ",
             FormatCounter(postsolve_mapping->size()), " variables.");

  // Perform the actual mapping.
  // Note that this might re-add fixed variable that are still used.
  const int old_size = postsolve_mapping->size();
  ApplyVariableMapping(absl::MakeSpan(mapping),
                       context_->UnsafeMutableWorkingModel(), postsolve_mapping,
                       context_->solution_crush());
  if (postsolve_mapping->size() > old_size) {
    const int new_extra = postsolve_mapping->size() - old_size;
    SOLVER_LOG(logger_, "TODO: ", new_extra,
               " fixed variables still required in the model!");
  }

  // Reset some part of the context, the caller re-reads the new domains.
  context_->ResetAfterCopy();
  return true;
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
  const CpModelProto& cp_model;
  bool ignore_enforcement;
  bool ignore_linear_domain;
  bool ignore_target_of_expression;
  ConstraintProto objective_constraint;

  ConstraintHashForDuplicateDetection(const CpModelProto* working_model,
                                      bool ignore_enforcement,
                                      bool ignore_linear_domain,
                                      bool ignore_target_of_expression)
      : cp_model(*working_model),
        ignore_enforcement(ignore_enforcement),
        ignore_linear_domain(ignore_linear_domain),
        ignore_target_of_expression(ignore_target_of_expression),
        objective_constraint(
            CopyObjectiveForDuplicateDetection(cp_model.objective())) {}

  // We hash our mostly frequently used constraint directly without extra memory
  // allocation. We revert to a generic code using proto serialization for the
  // others.
  std::size_t operator()(int ct_idx) const {
    const ConstraintProto& ct = ct_idx == kObjectiveConstraint
                                    ? objective_constraint
                                    : cp_model.constraints(ct_idx);
    const std::pair<ConstraintProto::ConstraintCase, absl::Span<const int>>
        type_and_enforcement = {ct.constraint_case(),
                                ignore_enforcement
                                    ? absl::Span<const int>()
                                    : absl::MakeSpan(ct.enforcement_literal())};
    switch (ct.constraint_case()) {
      case ConstraintProto::kLinear:
        if (ignore_linear_domain) {
          // We ignore domain for linear constraint, because if the rest of the
          // constraint is the same we can just intersect them.
          return absl::HashOf(type_and_enforcement,
                              absl::MakeSpan(ct.linear().vars()),
                              absl::MakeSpan(ct.linear().coeffs()));
        } else {
          return absl::HashOf(type_and_enforcement,
                              absl::MakeSpan(ct.linear().vars()),
                              absl::MakeSpan(ct.linear().coeffs()),
                              absl::MakeSpan(ct.linear().domain()));
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
      case ConstraintProto::kInterval:
        return absl::HashOf(type_and_enforcement,
                            absl::MakeSpan(ct.interval().start().vars()),
                            absl::MakeSpan(ct.interval().start().coeffs()),
                            ct.interval().start().offset(),
                            absl::MakeSpan(ct.interval().size().vars()),
                            absl::MakeSpan(ct.interval().size().coeffs()),
                            ct.interval().size().offset(),
                            absl::MakeSpan(ct.interval().end().vars()),
                            absl::MakeSpan(ct.interval().end().coeffs()),
                            ct.interval().end().offset());
      default:
        ConstraintProto copy = ct;
        copy.clear_name();
        if (ignore_enforcement) {
          copy.mutable_enforcement_literal()->Clear();
        }
        // TODO(user): Hash directly.
        if (ignore_target_of_expression) {
          if (ct.constraint_case() == ConstraintProto::kIntProd) {
            copy.mutable_int_prod()->clear_target();
          } else if (ct.constraint_case() == ConstraintProto::kLinMax) {
            copy.mutable_lin_max()->clear_target();
          } else if (ct.constraint_case() == ConstraintProto::kIntDiv) {
            copy.mutable_int_div()->clear_target();
          } else if (ct.constraint_case() == ConstraintProto::kIntMod) {
            copy.mutable_int_mod()->clear_target();
          }
        }
        return absl::HashOf(copy.SerializeAsString());
    }
  }
};

struct ConstraintEqForDuplicateDetection {
  const CpModelProto& cp_model;
  bool ignore_enforcement;
  bool ignore_linear_domain;
  bool ignore_target_of_expression;
  ConstraintProto objective_constraint;

  ConstraintEqForDuplicateDetection(const CpModelProto* working_model,
                                    bool ignore_enforcement,
                                    bool ignore_linear_domain,
                                    bool ignore_target_of_expression)
      : cp_model(*working_model),
        ignore_enforcement(ignore_enforcement),
        ignore_linear_domain(ignore_linear_domain),
        ignore_target_of_expression(ignore_target_of_expression),
        objective_constraint(
            CopyObjectiveForDuplicateDetection(cp_model.objective())) {}

  bool operator()(int a, int b) const {
    if (a == b) {
      return true;
    }
    const ConstraintProto& ct_a = a == kObjectiveConstraint
                                      ? objective_constraint
                                      : cp_model.constraints(a);
    const ConstraintProto& ct_b = b == kObjectiveConstraint
                                      ? objective_constraint
                                      : cp_model.constraints(b);

    if (ct_a.constraint_case() != ct_b.constraint_case()) return false;
    if (!ignore_enforcement) {
      if (absl::MakeSpan(ct_a.enforcement_literal()) !=
          absl::MakeSpan(ct_b.enforcement_literal())) {
        return false;
      }
    }
    auto compare_linear_argument = [this](const LinearArgumentProto& a,
                                          const LinearArgumentProto& b) {
      if (!ignore_target_of_expression) {
        if (!LinearExpressionProtosAreExactlyEqual(a.target(), b.target())) {
          return false;
        }
      }
      if (a.exprs().size() != b.exprs().size()) return false;
      for (int i = 0; i < a.exprs().size(); ++i) {
        if (!LinearExpressionProtosAreExactlyEqual(a.exprs(i), b.exprs(i))) {
          return false;
        }
      }
      return true;
    };
    switch (ct_a.constraint_case()) {
      case ConstraintProto::kLinear:
        // As above, we ignore domain for linear constraint, because if the rest
        // of the constraint is the same we can just intersect them.
        if (!ignore_linear_domain &&
            absl::MakeSpan(ct_a.linear().domain()) !=
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
      case ConstraintProto::kInterval: {
        return LinearExpressionProtosAreExactlyEqual(ct_a.interval().start(),
                                                     ct_b.interval().start()) &&
               LinearExpressionProtosAreExactlyEqual(ct_a.interval().size(),
                                                     ct_b.interval().size()) &&
               LinearExpressionProtosAreExactlyEqual(ct_a.interval().end(),
                                                     ct_b.interval().end());
      }
      case ConstraintProto::kIntProd:
        return compare_linear_argument(ct_a.int_prod(), ct_b.int_prod());
      case ConstraintProto::kLinMax:
        return compare_linear_argument(ct_a.lin_max(), ct_b.lin_max());
      case ConstraintProto::kIntDiv:
        return compare_linear_argument(ct_a.int_div(), ct_b.int_div());
      case ConstraintProto::kIntMod:
        return compare_linear_argument(ct_a.int_mod(), ct_b.int_mod());
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
    const CpModelProto& model_proto, bool ignore_enforcement,
    bool ignore_linear_domain, bool ignore_target_of_expression) {
  std::vector<std::pair<int, int>> result;

  // We use a map hash that uses the underlying constraint to compute the hash
  // and the equality for the indices.
  absl::flat_hash_map<int, int, ConstraintHashForDuplicateDetection,
                      ConstraintEqForDuplicateDetection>
      equiv_constraints(model_proto.constraints_size(),
                        ConstraintHashForDuplicateDetection{
                            &model_proto, ignore_enforcement,
                            ignore_linear_domain, ignore_target_of_expression},
                        ConstraintEqForDuplicateDetection{
                            &model_proto, ignore_enforcement,
                            ignore_linear_domain, ignore_target_of_expression});

  // Create a special representative for the linear objective.
  if (model_proto.has_objective() && !ignore_enforcement) {
    equiv_constraints[kObjectiveConstraint] = kObjectiveConstraint;
  }

  const int num_constraints = model_proto.constraints().size();
  for (int c = 0; c < num_constraints; ++c) {
    const auto type = model_proto.constraints(c).constraint_case();
    if (type == ConstraintProto::CONSTRAINT_NOT_SET) continue;

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

void CpModelPresolver::DetectUnenforcedEnforcedLinearPair() {
  if (time_limit_->LimitReached()) return;
  if (context_->ModelIsUnsat()) return;
  PresolveTimer timer(__FUNCTION__, logger_, time_limit_);

  const CpModelProto& model_proto = context_->WorkingModel();
  const int num_constraints = model_proto.constraints().size();

  // Quick check.
  int num_enforced_linear = 0;
  int num_unenforced_linear = 0;
  timer.TrackSimpleLoop(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    const auto type = model_proto.constraints(c).constraint_case();
    if (type != ConstraintProto::kLinear) continue;
    if (model_proto.constraints(c).enforcement_literal().empty()) {
      ++num_unenforced_linear;
    } else {
      ++num_enforced_linear;
    }
  }
  if (num_enforced_linear == 0 || num_unenforced_linear == 0) return;
  timer.AddCounter("num_enforced", num_enforced_linear);
  timer.AddCounter("num_unenforced", num_unenforced_linear);

  // We use a map hash that uses the underlying constraint to compute the hash
  // and the equality for the indices.
  const bool ignore_enforcement = true;
  const bool ignore_linear_domain = true;
  const bool ignore_target_of_expression = false;
  absl::flat_hash_map<int, int, ConstraintHashForDuplicateDetection,
                      ConstraintEqForDuplicateDetection>
      equiv_constraints(model_proto.constraints_size(),
                        ConstraintHashForDuplicateDetection{
                            &model_proto, ignore_enforcement,
                            ignore_linear_domain, ignore_target_of_expression},
                        ConstraintEqForDuplicateDetection{
                            &model_proto, ignore_enforcement,
                            ignore_linear_domain, ignore_target_of_expression});

  // First pass, add all non-enforced linear.
  timer.TrackSimpleLoop(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    const auto type = model_proto.constraints(c).constraint_case();
    if (type != ConstraintProto::kLinear) continue;
    if (!model_proto.constraints(c).enforcement_literal().empty()) continue;
    equiv_constraints.insert({c, c});
  }

  // Second pass. Find identical enforced constraint.
  int num_changes = 0;
  timer.TrackSimpleLoop(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    const auto type = model_proto.constraints(c).constraint_case();
    if (type != ConstraintProto::kLinear) continue;
    if (model_proto.constraints(c).enforcement_literal().empty()) continue;
    const auto it = equiv_constraints.find(c);
    if (it == equiv_constraints.end()) continue;

    const Domain always_true =
        ReadDomainFromProto(context_->Constraint(it->second).linear());
    const Domain rhs = ReadDomainFromProto(context_->Constraint(c).linear());
    const Domain new_rhs = rhs.IntersectionWith(always_true);
    if (new_rhs.IsEmpty()) {
      ++num_changes;
      (void)constraint_presolver_->MarkConstraintAsFalse(
          context_->MutableConstraint(c),
          "duplicate: infeasible enforced constraint");
      context_->UpdateConstraintVariableUsage(c);
      continue;
    }

    if (new_rhs != rhs) {
      ++num_changes;
      context_->UpdateRuleStats(
          "duplicate: tightened enforced constraint domain");
      FillDomainInProto(new_rhs,
                        context_->MutableConstraint(c)->mutable_linear());
    }
  }

  timer.AddCounter("num_changes", num_changes);
}

}  // namespace sat
}  // namespace operations_research
