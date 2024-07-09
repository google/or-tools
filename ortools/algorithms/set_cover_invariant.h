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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_INVARIANT_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_INVARIANT_H_

#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {

// A helper class used to store the decisions made during a search.
class SetCoverDecision {
 public:
  SetCoverDecision() : decision_(0) {}

  SetCoverDecision(SubsetIndex subset, bool value) {
    static_assert(sizeof(subset) == sizeof(decision_));
    DCHECK_GE(subset.value(), 0);
    decision_ = value ? subset.value() : ~subset.value();
  }

  SubsetIndex subset() const {
    return SubsetIndex(decision_ >= 0 ? decision_ : ~decision_);
  }

  bool decision() const { return decision_ >= 0; }

 private:
  BaseInt decision_;
};

// SetCoverInvariant does the bookkeeping for a solution to the
// SetCoverModel passed as argument.
// The state of a SetCoverInvariant instance is uniquely defined by a
// SubsetBoolVector representing whether a subset is selected in the solution
// or not.
//
// See https://cs.brown.edu/research/pubs/pdfs/1999/Michel-1999-LML.pdf
// for an explanation of the terminology.
//
// A SetCoverInvariant is (relatively) small:
//   is_selected_: a partial solution, vector of Booleans of size #subsets.
// From this, the following can be computed:
//   coverage_         :  number of times an element is covered;
//   num_free_elements_:  number of elements in a subset that are uncovered.
//   num_non_overcovered_elements_: the number of elements of a subset that
//   are covered 1 time or less (not overcovered) in the current solution;
//   is_redundant_,     whether a subset can be removed from the solution.
//   is_redundant_[subset] == (num_non_overcovered_elements_[subet] == 0).

class SetCoverInvariant {
 public:
  // Constructs an empty weighted set covering solver state.
  // The model may not change after the invariant was built.
  explicit SetCoverInvariant(SetCoverModel* m) : model_(m) { Initialize(); }

  // Initializes the solver once the data is set. The model cannot be changed
  // afterwards.
  void Initialize();

  void Clear() {
    is_selected_.assign(model_->num_subsets(), false);
    RecomputeInvariant();
  }

  // Recomputes all the invariants for the current solution.
  void RecomputeInvariant();

  // Returns the weighted set covering model to which the state applies.
  SetCoverModel* model() const { return model_; }

  const SetCoverModel* const_model() const { return model_; }

  // Returns the cost of current solution.
  Cost cost() const { return cost_; }

  // Returns the number of uncovered elements.
  BaseInt num_uncovered_elements() const { return num_uncovered_elements_; }

  // Returns the subset assignment vector.
  const SubsetBoolVector& is_selected() const { return is_selected_; }

  // Returns vector containing the number of elements in each subset that are
  // not covered in the current solution.
  const SubsetToIntVector& num_free_elements() const {
    return num_free_elements_;
  }

  // Returns the vector of numbers of free or exactly covered elements for
  // each subset.
  const SubsetToIntVector& num_coverage_le_1_elements() const {
    return num_non_overcovered_elements_;
  }
  // Returns vector containing number of subsets covering each element.
  const ElementToIntVector& coverage() const { return coverage_; }

  // Returns vector of Booleans telling whether each subset can be removed from
  // the solution.
  const SubsetBoolVector& is_redundant() const { return is_redundant_; }

  // Returns the vector of the decisions which has led to the current solution.
  const std::vector<SetCoverDecision>& trace() const { return trace_; }

  // Clears the trace.
  void ClearTrace() { trace_.clear(); }

  // Clear the removability information.
  void ClearRemovabilityInformation() {
    new_removable_subsets_.clear();
    new_non_removable_subsets_.clear();
  }

  // Returns the subsets that become removable after the last update.
  const std::vector<SubsetIndex>& new_removable_subsets() const {
    return new_removable_subsets_;
  }

  // Returns the subsets that become non removable after the last update.
  const std::vector<SubsetIndex>& new_non_removable_subsets() const {
    return new_non_removable_subsets_;
  }

  // Compresses the trace so that:
  // - each subset appears only once,
  // - there are only "positive" decisions.
  // This trace is equivalent to the original trace in the sense that the cost
  // and the covered elements are the same.
  // This can be used to recover the solution by indices after local search.
  void CompressTrace();

  // Loads the solution and recomputes the data in the invariant.
  void LoadSolution(const SubsetBoolVector& solution);

  // Returns true if the data stored in the invariant is consistent.
  // The body of the function will CHECK-fail the first time an inconsistency
  // is encountered.
  bool CheckConsistency() const;

  // Returns true if the subset is redundant within the current solution, i.e.
  // when all its elements are already covered twice. Note that the set need
  // not be selected for this to happen.
  // TODO(user): Implement this using AVX-512?
  bool ComputeIsRedundant(SubsetIndex subset) const;

  // Updates the invariant fully, so that is_redundant_ can be updated
  // incrementally later with SelectAndFullyUpdate and
  // DeselectSelectAndFullyUpdate.
  void MakeFullyUpdated();

  // Flips is_selected_[subset] to its negation, by calling Select or Deselect
  // depending on value. Updates the invariant incrementally.
  // FlipAndFullyUpdate performs a full incremental update of the invariant,
  // including num_non_overcovered_elements_, is_redundant_,
  // new_removable_subsets_, new_non_removable_subsets_. This is useful for some
  // meta-heuristics.
  void Flip(SubsetIndex subset) { Flip(subset, false); }
  void FlipAndFullyUpdate(SubsetIndex subset) { Flip(subset, true); }

  // Includes subset in the solution by setting is_selected_[subset] to true
  // and incrementally updating the invariant.
  // SelectAndFullyUpdate also updates the invariant in a more thorough way as
  // explained with FlipAndFullyUpdate.
  void Select(SubsetIndex subset) { Select(subset, false); }
  void SelectAndFullyUpdate(SubsetIndex subset) { Select(subset, true); }

  // Excludes subset from the solution by setting is_selected_[subset] to false
  // and incrementally updating the invariant.
  // DeselectAndFullyUpdate also updates the invariant in a more thorough way as
  // explained with FlipAndFullyUpdate.
  void Deselect(SubsetIndex subset) { Deselect(subset, false); }
  void DeselectAndFullyUpdate(SubsetIndex subset) { Deselect(subset, true); }

  // Returns the current solution as a proto.
  SetCoverSolutionResponse ExportSolutionAsProto() const;

  // Imports the solution from a proto.
  void ImportSolutionFromProto(const SetCoverSolutionResponse& message);

 private:
  // Computes the cost and the coverage vector for the given choices.
  // Temporarily uses |E| BaseInts.
  std::tuple<Cost, ElementToIntVector> ComputeCostAndCoverage(
      const SubsetBoolVector& choices) const;

  // Computes the global number of uncovered elements and the
  // vector containing the number of free elements for each subset from
  // a coverage vector.
  // Temporarily uses |S| BaseInts.
  std::tuple<BaseInt,            // Number of uncovered elements,
             SubsetToIntVector>  // Vector of number of free elements.
  ComputeNumUncoveredAndFreeElements(const ElementToIntVector& cvrg) const;

  // Computes the vector containing the number of non-overcovered elements per
  // subset and the Boolean vector telling whether a subset is redundant w.r.t.
  // the current solution.
  // Temporarily uses |S| BaseInts.
  std::tuple<SubsetToIntVector,  // Number of non-overcovered elements,
             SubsetBoolVector>   // Redundancy for each of the subsets.
  ComputeNumNonOvercoveredElementsAndIsRedundant(
      const ElementToIntVector& cvrg) const;

  // Flips is_selected_[subset] to its negation,  by calling Select or Deselect
  // depending on value. Updates the invariant incrementally.
  // When incremental_full_update is true, the following fields are also
  // updated: num_non_overcovered_elements_, is_redundant_,
  // new_removable_subsets_, new_non_removable_subsets_. This is useful for some
  // meta-heuristics.
  void Flip(SubsetIndex, bool incremental_full_update);

  // Sets is_selected_[subset] to true and incrementally updates the invariant.
  // Parameter incremental_full_update has the same meaning as with Flip.
  void Select(SubsetIndex subset, bool incremental_full_update);

  // Sets is_selected_[subset] to false and incrementally updates the invariant.
  // Parameter incremental_full_update has the same meaning as with Flip.
  void Deselect(SubsetIndex subset, bool incremental_full_update);

  // Helper function for Select when AVX-512 is supported by the processor.
  void SelectAvx512(SubsetIndex subset);

  // Helper function for Deselect when AVX-512 is supported by the processor.
  void DeselectAvx512(SubsetIndex subset);

  // The weighted set covering model on which the solver is run.
  SetCoverModel* model_;

  // Current cost.
  Cost cost_;

  // The number of uncovered (or free) elements in the current solution.
  BaseInt num_uncovered_elements_;

  // Current assignment.
  // Takes |S| bits.
  SubsetBoolVector is_selected_;

  // A trace of the decisions, i.e. a list of decisions (subset, Boolean) that
  // lead to the current solution.
  // Takes at most |S| BaseInts.
  std::vector<SetCoverDecision> trace_;

  // The coverage of an element is the number of used subsets which contains
  // the said element.
  // Takes |E| BaseInts
  ElementToIntVector coverage_;

  // A vector that for each subset gives the number of free elements, i.e.
  // elements whose coverage is 0.
  // problem.
  // Takes |S| BaseInts.
  SubsetToIntVector num_free_elements_;

  // Counts the number of free or exactly covered elements, i.e. whose coverage
  // is 0 or 1.
  // Takes at most |S| BaseInts. (More likely a few percent of that).
  SubsetToIntVector num_non_overcovered_elements_;

  // True if the subset is redundant, i.e. can be removed from the solution
  // without making it infeasible.
  // Takes |S| bits.
  SubsetBoolVector is_redundant_;

  // Subsets that become removable after the last update.
  // Takes at most |S| BaseInts. (More likely a few percent of that).
  std::vector<SubsetIndex> new_removable_subsets_;

  // Subsets that become non removable after the last update.
  // Takes at most |S| BaseInts. (More likely a few percent of that).
  std::vector<SubsetIndex> new_non_removable_subsets_;

  // Denotes whether is_redundant_ and num_non_overcovered_elements_ have been
  // updated. Initially true, it becomes false as soon as Flip,
  // Select and Deselect are called with incremental_full_update = false. The
  // fully updated status can be achieved again with a call to FullUpdate(),
  // which can be expensive,
  bool is_fully_updated_;

  // True if the CPU supports the AVX-512 instruction set.
  bool supports_avx512_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_INVARIANT_H_
