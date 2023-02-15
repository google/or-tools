// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_VAR_DOMINATION_H_
#define OR_TOOLS_SAT_VAR_DOMINATION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/presolve_context.h"

namespace operations_research {
namespace sat {

// A variable X is say to dominate a variable Y if, from any feasible solution,
// doing X++ and Y-- is also feasible (modulo the domain of X and Y) and has the
// same or a better objective value.
//
// Note that we also look for dominance between the negation of the variables.
// So we detect all (X++, Y++), (X--, Y--), (X++, Y--) and (X--, Y++) cases.
// We reuse both ref / Negated(ref) and translate that to IntegerVariable for
// indexing vectors.
//
// Once detected, dominance relation can lead to more propagation. Note however,
// that we will loose feasible solution that are dominated by better solutions.
// In particular, in a linear constraint sum coeff * Xi <= rhs with positive
// coeff, if an X is dominated by a set of other variable in the constraint,
// then its upper bound can be propagated assuming the dominating variables are
// at their upper bound. This can in many case result in X being fixed to its
// lower bound.
//
// TODO(user): We have a lot of benchmarks and tests that shows that we don't
// report wrong relations, but we lack unit test that make sure we don't miss
// any. Try to improve the situation.
class VarDomination {
 public:
  VarDomination() {}

  // This is the translation used from "ref" to IntegerVariable. The API
  // understand the cp_model.proto ref, but internally we only store
  // IntegerVariable.
  static IntegerVariable RefToIntegerVariable(int ref) {
    return RefIsPositive(ref) ? IntegerVariable(2 * ref)
                              : IntegerVariable(2 * NegatedRef(ref) + 1);
  }
  static int IntegerVariableToRef(IntegerVariable var) {
    return VariableIsPositive(var) ? var.value() / 2
                                   : NegatedRef(var.value() / 2);
  }

  // Resets the class to a clean state.
  // At the beginning, we assume that there is no constraint.
  void Reset(int num_variables);

  // These functions are used to encode all of our constraints.
  // The algorithm work in two passes, so one should do:
  // - 1/ Convert all problem constraints to one or more calls
  // - 2/ Call EndFirstPhase()
  // - 3/ Redo 1. Only the one sided constraint need to be processed again. But
  //      calling the others will just do nothing, so it is fine too.
  // - 4/ Call EndSecondPhase()
  //
  // The names are pretty self-explanatory. A few linear constraint ex:
  // - To encode terms = cte, one should call ActivityShouldNotChange()
  // - To encode terms >= cte, one should call ActivityShouldNotDecrease()
  // - To encode terms <= cte, one should call ActivityShouldNotIncrease()
  //
  // The coeffs vector can be left empty, in which case all variable are assumed
  // to have the same coefficients. CanOnlyDominateEachOther() is basically the
  // same as ActivityShouldNotChange() without any coefficients.
  //
  // Note(user): It is better complexity wise to first refine the underlying
  // partition as much as possible, and then process all
  // ActivityShouldNotIncrease() and ActivityShouldNotDecrease() in two passes.
  // Experiment with it, it might require changing the API slightly since the
  // increase / decrease functions also refine the partition.
  void CanOnlyDominateEachOther(absl::Span<const int> refs);
  void ActivityShouldNotChange(absl::Span<const int> refs,
                               absl::Span<const int64_t> coeffs);
  void ActivityShouldNotDecrease(absl::Span<const int> enforcements,
                                 absl::Span<const int> refs,
                                 absl::Span<const int64_t> coeffs);
  void ActivityShouldNotIncrease(absl::Span<const int> enforcements,
                                 absl::Span<const int> refs,
                                 absl::Span<const int64_t> coeffs);

  // EndFirstPhase() must be called once all constraints have been processed
  // once. One then needs to redo the calls to ActivityShouldNotIncrease() and
  // ActivityShouldNotDecrease(). And finally call EndSecondPhase() before
  // querying the domination information.
  //
  // If EndFirstPhase() return false, there is no point continuing.
  bool EndFirstPhase();
  void EndSecondPhase();

  // This is true if this variable was never restricted by any call. We can thus
  // fix it to its lower bound. Note that we don't do that here as the
  // DualBoundStrengthening class will take care of that.
  bool CanFreelyDecrease(int ref) const;
  bool CanFreelyDecrease(IntegerVariable var) const;

  // Returns a set of variable dominating the given ones. Note that to keep the
  // algo efficient, this might not include all the possible dominations.
  //
  // Note: we never include as part of the dominating candidate variables that
  // can freely increase.
  absl::Span<const IntegerVariable> DominatingVariables(int ref) const;
  absl::Span<const IntegerVariable> DominatingVariables(
      IntegerVariable var) const;

  // Returns readable string with the possible valid combinations of the form
  // (var++/--, dom++/--) to facilitate debugging.
  std::string DominationDebugString(IntegerVariable var) const;

 private:
  struct IntegerVariableWithRank {
    IntegerVariable var;
    int part;
    int64_t rank;

    bool operator<(const IntegerVariableWithRank& o) const {
      return rank < o.rank;
    }
  };

  // This refine the partition can_dominate_partition_ with the given set.
  void RefinePartition(std::vector<int>* vars);

  // Convert the input from the public API into tmp_ranks_.
  void MakeRankEqualToStartOfPart(absl::Span<IntegerVariableWithRank> span);
  void FillTempRanks(bool reverse_references,
                     absl::Span<const int> enforcements,
                     absl::Span<const int> refs,
                     absl::Span<const int64_t> coeffs);

  // First phase functions. We will keep for each variable a list of possible
  // candidates which is as short as possible.
  absl::Span<const IntegerVariable> InitialDominatingCandidates(
      IntegerVariable var) const;
  void ProcessTempRanks();
  void Initialize(absl::Span<IntegerVariableWithRank> span);

  // Second phase function to filter the current candidate lists.
  void FilterUsingTempRanks();

  // Debug function.
  void CheckUsingTempRanks();

  // Starts at zero on Reset(), move to one on EndFirstPhase() and to 2 on
  // EndSecondPhase(). This is used for debug checks and to control what happen
  // on the constraint processing functions.
  int phase_ = 0;

  // The variables will be sorted by non-decreasking rank. The rank is also the
  // start of the first variable in tmp_ranks_ with this rank.
  //
  // Note that the rank should be int, but to reuse the same vector when we
  // construct it, we need int64_t. See FillTempRanks().
  std::vector<IntegerVariableWithRank> tmp_ranks_;

  // This do not change after EndFirstPhase().
  //
  // We will add to the Dynamic partition, a set of subset S, each meaning that
  // any variable in S can only dominate or be dominated by another variable in
  // S.
  std::vector<int> tmp_vars_;
  std::unique_ptr<SimpleDynamicPartition> partition_;
  absl::StrongVector<IntegerVariable, bool> can_freely_decrease_;

  // For all one sided constraints, we keep the bitmap of constraint indices
  // modulo 64 that block on the lower side each variable.
  int64_t ct_index_for_signature_ = 0;
  absl::StrongVector<IntegerVariable, uint64_t> block_down_signatures_;

  // Used by FilterUsingTempRanks().
  int num_vars_with_negation_;
  absl::StrongVector<IntegerVariable, int> tmp_var_to_rank_;

  // We don't use absl::Span() because the underlying buffer can be resized.
  // This however serve the same purpose.
  struct IntegerVariableSpan {
    int start = 0;
    int size = 0;
  };

  // This hold the first phase best candidate.
  // Warning, the initial candidates span can overlap in the shared_buffer_.
  std::vector<IntegerVariable> shared_buffer_;
  absl::StrongVector<IntegerVariable, IntegerVariableSpan> initial_candidates_;

  // This will hold the final result.
  // Buffer with independent content for each vars.
  std::vector<IntegerVariable> buffer_;
  absl::StrongVector<IntegerVariable, IntegerVariableSpan> dominating_vars_;
};

// This detects variables that can move freely in one direction, or that can
// move freely as long as their value do not cross a bound.
//
// TODO(user): This is actually an important step to do before scaling as it can
// usually reduce really large bounds!
class DualBoundStrengthening {
 public:
  // Reset the class to a clean state.
  // This must be called before processing the constraints.
  void Reset(int num_variables) {
    can_freely_decrease_until_.assign(2 * num_variables, kMinIntegerValue);
    num_locks_.assign(2 * num_variables, 0);
    locking_ct_index_.assign(2 * num_variables, -1);
  }

  // All constraints should be mapped to one of more call to these functions.
  void CannotDecrease(absl::Span<const int> refs, int ct_index = -1);
  void CannotIncrease(absl::Span<const int> refs, int ct_index = -1);
  void CannotMove(absl::Span<const int> refs, int ct_index = -1);

  // Most of the logic here deals with linear constraints.
  template <typename LinearProto>
  void ProcessLinearConstraint(bool is_objective,
                               const PresolveContext& context,
                               const LinearProto& linear, int64_t min_activity,
                               int64_t max_activity, int ct_index = -1);

  // Once ALL constraints have been processed, call this to fix variables or
  // reduce their domain if possible.
  //
  // Note that this also tighten some constraint that are the only one blocking
  // in one direction. Currently we only do that for implication, so that if we
  // have two Booleans such that a + b <= 1 we transform that to = 1 and we
  // remove one variable since we have now an equivalence relation.
  bool Strengthen(PresolveContext* context);

  // The given ref can always freely decrease until the returned value.
  // Note that this does not take into account the domain of the variable.
  int64_t CanFreelyDecreaseUntil(int ref) const {
    return can_freely_decrease_until_[RefToIntegerVariable(ref)].value();
  }

  // Reset on each Strengthen() call.
  int NumDeletedConstraints() const { return num_deleted_constraints_; }

 private:
  // We encode proto ref as IntegerVariable for indexing vectors.
  static IntegerVariable RefToIntegerVariable(int ref) {
    return RefIsPositive(ref) ? IntegerVariable(2 * ref)
                              : IntegerVariable(2 * NegatedRef(ref) + 1);
  }

  // Starts with kMaxIntegerValue, and decrease as constraints are processed.
  absl::StrongVector<IntegerVariable, IntegerValue> can_freely_decrease_until_;

  // How many times can_freely_decrease_until_[var] was set by a constraints.
  // If only one constraint is blocking, we can do more presolve.
  absl::StrongVector<IntegerVariable, int64_t> num_locks_;

  // If num_locks_[var] == 1, this will be the unique constraint that block var
  // in this direction. Note that it can be set to -1 if this wasn't recorded.
  absl::StrongVector<IntegerVariable, int64_t> locking_ct_index_;

  int num_deleted_constraints_ = 0;
};

// Detect the variable dominance relations within the given model. Note that
// to avoid doing too much work, we might miss some relations. This does two
// full scan of the model.
void DetectDominanceRelations(const PresolveContext& context,
                              VarDomination* var_domination,
                              DualBoundStrengthening* dual_bound_strengthening);

// Once detected, exploit the dominance relations that appear in the same
// constraint. This does a full scan of the model.
//
// Return false if the problem is infeasible.
bool ExploitDominanceRelations(const VarDomination& var_domination,
                               PresolveContext* context);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_VAR_DOMINATION_H_
