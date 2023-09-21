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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_LEDGER_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_LEDGER_H_

#include <sys/types.h>

#include <vector>

#include "ortools/algorithms/set_cover_model.h"

namespace operations_research {

using SubsetCountVector = glop::StrictITIVector<SubsetIndex, int>;
using SubsetBoolVector = glop::StrictITIVector<SubsetIndex, bool>;

// SetCoverLedger does the bookkeeping for a solution to the
// SetCoverModel passed as argument.
// The state of a SetCoverLedger instance is uniquely de fined by a
// SubsetBoolVector representing whether a subset is selected in the solution
// or not.
// A SetCoverLedger is (relatively) small:
//   is_selected_,      a partial solution, vector of Booleans of size #subsets.
// From this, the following can be computed:
//   coverage_,         the number of times an elememt is covered;
//   marginal_impacts_, the number of elements of a subset still uncovered;
//   is_removable_,     whether a subset can be removed from the solution.
// Note that is_removable_[subset] implies is_selected_[subset], and thus
// (is_removable_[subset] <= is_selected_[subset]) == true.
class SetCoverLedger {
 public:
  // Constructs an empty weighted set covering solver state.
  // The model may not change after the ledger was built.
  explicit SetCoverLedger(SetCoverModel* m) : model_(m) { Initialize(); }

  // Initializes the solver once the data is set. The model cannot be changed
  // afterwards.
  void Initialize();

  // Recomputes all the invariants for the current solution.
  void MakeDataConsistent() {
    cost_ = ComputeCost(is_selected_);
    coverage_ = ComputeCoverage(is_selected_);
    is_removable_ = ComputeIsRemovable(coverage_);
    marginal_impacts_ = ComputeMarginalImpacts(coverage_);
    num_elements_covered_ = ComputeNumElementsCovered(coverage_);
  }

  // Returns the weighted set covering model to which the state applies.
  SetCoverModel* model() const { return model_; }

  // Returns the cost of current solution.
  Cost cost() const { return cost_; }

  // Returns whether subset is selected in the solution.
  bool is_selected(SubsetIndex subset) const { return is_selected_[subset]; }

  // Returns the number of elements in each subset that are not covered in the
  // current solution.
  ElementIndex marginal_impacts(SubsetIndex subset) const {
    return marginal_impacts_[subset];
  }

  // Returns the number of subsets covering each element.
  SubsetIndex coverage(ElementIndex subset) const { return coverage_[subset]; }

  // Returns whether subset can be removed from the solution.
  bool is_removable(SubsetIndex subset) const { return is_removable_[subset]; }

  // Returns the number of elements covered.
  ElementIndex num_elements_covered() const { return num_elements_covered_; }

  // Stores the solution and recomputes the data in the ledger.
  void LoadSolution(const SubsetBoolVector& c);

  // Returns the current solution.
  SubsetBoolVector GetSolution() const { return is_selected_; }

  // Returns true if the data stored in the ledger is consistent.
  bool CheckConsistency() const;

  // Computes is_removable_ from scratch for every subset.
  // TODO(user): reconsider exposing this.
  void RecomputeIsRemovable() { is_removable_ = ComputeIsRemovable(coverage_); }

  // Returns the subsets that share at least one element with subset.
  // TODO(user): is it worth to precompute this?
  std::vector<SubsetIndex> ComputeImpactedSubsets(SubsetIndex subset) const;

  // Updates is_removable_ for each subset in impacted_subsets.
  void UpdateIsRemovable(const std::vector<SubsetIndex>& impacted_subsets);

  // Updates marginal_impacts_ for each subset in impacted_subsets.
  void UpdateMarginalImpacts(const std::vector<SubsetIndex>& impacted_subsets);

  // Toggles is_selected_[subset] to value, and incrementally updates the
  // ledger.
  // Returns a vector of subsets impacted by the change, in case they need
  // to be reconsidered in a solution geneator or a local search algorithm.
  // Calls UnsafeToggle, with the added checks:
  // If value is true, DCHECKs that subset is removable.
  // If value is true, DCHECKs that marginal impact of subset is removable.
  std::vector<SubsetIndex> Toggle(SubsetIndex subset, bool value);

  // Same as Toggle, with less DCHECKS.
  // Useful for some meta-heuristics that allow to go through infeasible
  // solutions.
  // Only checks that value is different from is_selected_[subset].
  std::vector<SubsetIndex> UnsafeToggle(SubsetIndex subset, bool value);

  // Update coverage_ for subset when setting is_selected_[subset] to value.
  void UpdateCoverage(SubsetIndex subset, bool value);

  // Returns true if the elements selected in the current solution cover all
  // the elements of the set.
  bool CheckSolution() const;

  // Checks that coverage_  and marginal_impacts_ are consistent with  choices.
  bool CheckCoverageAndMarginalImpacts(const SubsetBoolVector& choices) const;

  // Returns the subsets that are unused that could be used to cover the still
  // uncovered subsets.
  std::vector<SubsetIndex> ComputeSettableSubsets() const;

  std::vector<SubsetIndex> ComputeResettableSubsets() const;

 private:
  // Recomputes the cost from scratch from c.
  Cost ComputeCost(const SubsetBoolVector& c) const;

  // Computes is_removable based on a coverage cvrg.
  SubsetBoolVector ComputeIsRemovable(const ElementToSubsetVector& cvrg) const;

  // Computes marginal impacts based on a coverage cvrg.
  SubsetToElementVector ComputeMarginalImpacts(
      const ElementToSubsetVector& cvrg) const;

  // Computes the number of elements covered based on coverage vector 'cvrg'.
  ElementIndex ComputeNumElementsCovered(
      const ElementToSubsetVector& cvrg) const;

  // Returns true if subset can be removed from the solution, i.e. it is
  // redundant to cover all the elements.
  // This function is used to check that is_removable[subset] is consistent.
  bool ComputeIsRemovable(SubsetIndex subset) const;

  // Returns the number of elements currently covered by subset.
  ElementToSubsetVector ComputeSingleSubsetCoverage(SubsetIndex subset) const;

  // Returns a vector containing the number of subsets covering each element.
  ElementToSubsetVector ComputeCoverage(const SubsetBoolVector& choices) const;

  // Checks that the value of coverage_ is correct by recomputing and comparing.
  bool CheckSingleSubsetCoverage(SubsetIndex subset) const;

  // Checks that coverage_ is consistent with choices.
  bool CheckCoverageAgainstSolution(const SubsetBoolVector& choices) const;

  // Returns true if is_removable_ is consistent.
  bool CheckIsRemovable() const;

  // The weighted set covering model on which the solver is run.
  SetCoverModel* model_;

  // Current cost.
  Cost cost_;

  // The number of elements covered in the current solution.
  ElementIndex num_elements_covered_;

  // Current assignment.
  SubsetBoolVector is_selected_;

  // The marginal impact of a subset is the number of elements in that subset
  // that are not covered in the current solution.
  SubsetToElementVector marginal_impacts_;

  // The coverage of an element is the number of used subsets which contains
  // the said element.
  ElementToSubsetVector coverage_;

  // True if the subset can be removed from the solution without making it
  // infeasible.
  SubsetBoolVector is_removable_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_LEDGER_H_
