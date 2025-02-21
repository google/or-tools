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

#ifndef OR_TOOLS_SET_COVER_SET_COVER_INVARIANT_H_
#define OR_TOOLS_SET_COVER_SET_COVER_INVARIANT_H_

#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/set_cover/set_cover.pb.h"
#include "ortools/set_cover/set_cover_model.h"

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
  // The consistency level of the invariant.
  // The values denote the level of consistency of the invariant.
  // There is an order between the levels, and the invariant is consistent at
  // level k if it is consistent at all levels lower than k.
  // The consistency level that is the most natural is to use kFreeAndUncovered,
  // since it enables to implement most heuristics.
  // kCostAndCoverage is used by LazyElementDegree, a fast greedy heuristic.
  // kRedundancy is used for GuidedLocalSearch, because knowing whether a
  // subset is redundant incrementally is faster than recomputing the
  // information over and over again.
  // Below, the quantities that are maintained at each level are listed.
  enum class ConsistencyLevel {
    kInconsistent = 0,  // The invariant is not consistent.
    kCostAndCoverage,   // cost_ and coverage_.
    kFreeAndUncovered,  // num_free_elements_ and num_uncovered_elements_.
    kRedundancy         // is_redundant_ and num_non_overcovered_elements_.
  };

  // Constructs an empty weighted set covering solver state.
  // The model may not change after the invariant was built.
  explicit SetCoverInvariant(SetCoverModel* m) : model_(m) { Initialize(); }

  // Initializes the solver once the data is set. The model cannot be changed
  // afterwards.
  void Initialize();

  // Clears the invariant. Also called by Initialize.
  void Clear();

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

  // Returns a vector containing the number of subsets within `focus` covering
  // each element. Subsets that are without `focus` are not considered.
  ElementToIntVector ComputeCoverageInFocus(
      absl::Span<const SubsetIndex> focus) const;

  // Returns vector of Booleans telling whether each subset can be removed from
  // the solution.
  const SubsetBoolVector& is_redundant() const { return is_redundant_; }

  // Returns the vector of the decisions which have led to the current solution.
  const std::vector<SetCoverDecision>& trace() const { return trace_; }

  // Clears the trace.
  void ClearTrace() { trace_.clear(); }

  // Clear the removability information.
  void ClearRemovabilityInformation() {
    newly_removable_subsets_.clear();
    newly_non_removable_subsets_.clear();
  }

  // Returns the subsets that become removable after the last update.
  const std::vector<SubsetIndex>& newly_removable_subsets() const {
    return newly_removable_subsets_;
  }

  // Returns the subsets that become non removable after the last update.
  const std::vector<SubsetIndex>& newly_non_removable_subsets() const {
    return newly_non_removable_subsets_;
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

  // Checks the consistency of the invariant at the given consistency level.
  bool CheckConsistency(ConsistencyLevel consistency) const;

  // Recomputes the invariant to the given consistency level.
  void Recompute(ConsistencyLevel target_consistency);

  // Returns true if the subset is redundant within the current solution, i.e.
  // when all its elements are already covered twice. Note that the set need
  // not be selected for this to happen.
  // TODO(user): Implement this using AVX-512?
  bool ComputeIsRedundant(SubsetIndex subset) const;

  // Computes the number of free (uncovered) elements in the given subset.
  BaseInt ComputeNumFreeElements(SubsetIndex subset) const;

  // Flips is_selected_[subset] to its negation, by calling Select or Deselect
  // depending on value. Updates the invariant incrementally to the given
  // consistency level.
  void Flip(SubsetIndex subset, ConsistencyLevel consistency);

  // Includes subset in the solution by setting is_selected_[subset] to true
  // and incrementally updating the invariant to the given consistency level.
  void Select(SubsetIndex subset, ConsistencyLevel consistency);

  // Excludes subset from the solution by setting is_selected_[subset] to false
  // and incrementally updating the invariant to the given consistency level.
  void Deselect(SubsetIndex subset, ConsistencyLevel consistency);

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
  ComputeRedundancyInfo(const ElementToIntVector& cvrg) const;

  // Returns true if the current consistency level consistency_ is lower than
  // cheched_consistency and the desired consistency is higher than
  // cheched_consistency.
  bool NeedToRecompute(ConsistencyLevel cheched_consistency,
                       ConsistencyLevel target_consistency);

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

  // Subsets that became removable after the last update.
  // Takes at most |S| BaseInts. (More likely a few percent of that).
  std::vector<SubsetIndex> newly_removable_subsets_;

  // Subsets that became non removable after the last update.
  // Takes at most |S| BaseInts. (More likely a few percent of that).
  std::vector<SubsetIndex> newly_non_removable_subsets_;

  // Denotes the consistency level of the invariant.
  // Some algorithms may need to recompute the invariant to a higher consistency
  // level.
  // TODO(user): think of making the enforcement of the consistency level
  // automatic at the constructor level of the heuristic algorithms.
  ConsistencyLevel consistency_level_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_SET_COVER_SET_COVER_INVARIANT_H_
