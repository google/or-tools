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

#include <sys/types.h>

#include <tuple>
#include <vector>

#include "ortools/algorithms/set_cover.pb.h"
#include "ortools/algorithms/set_cover_model.h"

namespace operations_research {

using SubsetCountVector = glop::StrictITIVector<SubsetIndex, int>;
using SubsetBoolVector = glop::StrictITIVector<SubsetIndex, bool>;

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
//   coverage_:   the number of times an elememt is covered;
//   num_free_elements_: the number of elements of a subset still uncovered;
//   num_coverage_le_1_elements_: the number of elements of a subset that
//   are covered 1 time or less (not overcovered) in the current solution;
//   is_removable_,     whether a subset can be removed from the solution.
//   is_removable_[subset] == (num_coverage_le_1_elements_[subet] == 0).
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

  // Returns the cost of current solution.
  Cost cost() const { return cost_; }

  // Returns the number of uncovered elements.
  ElementIndex num_uncovered_elements() const {
    return num_uncovered_elements_;
  }

  // Returns the subset assignment vector.
  const SubsetBoolVector& is_selected() const { return is_selected_; }

  // Returns vector containing the number of elements in each subset that are
  // not covered in the current solution.
  const SubsetToElementVector& num_free_elements() const {
    return num_free_elements_;
  }

  // Returns the vector of numbers of free or exactly covered elements for
  // each subset.
  const SubsetToElementVector& num_coverage_le_1_elements() const {
    return num_coverage_le_1_elements_;
  }

  // Returns vector containing number of subsets covering each element.
  const ElementToSubsetVector& coverage() const { return coverage_; }

  // Returns vector of Booleans telling whether each subset can be removed from
  // the solution.
  const SubsetBoolVector& is_removable() const { return is_removable_; }

  // Stores the solution and recomputes the data in the invariant.
  void LoadSolution(const SubsetBoolVector& c);

  // Returns true if the data stored in the invariant is consistent.
  bool CheckConsistency() const;

  // Toggles is_selected_[subset] to value, and incrementally updates the
  // invariant.
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

  std::vector<SubsetIndex> UnsafeUse(SubsetIndex subset);

  std::vector<SubsetIndex> UnsafeRemove(SubsetIndex subset);

  // Returns the current solution as a proto.
  SetCoverSolutionResponse ExportSolutionAsProto() const;

  // Imports the solution from a proto.
  void ImportSolutionFromProto(const SetCoverSolutionResponse& message);

 private:
  // Computes the cost and the coverage vector for the given choices.
  std::tuple<Cost, ElementToSubsetVector> ComputeCostAndCoverage(
      const SubsetBoolVector& choices) const;

  // Computes the implied data for the given coverage cvrg:
  // The implied data consists of:
  std::tuple<ElementIndex,           // Number of uncovered elements.
             SubsetToElementVector,  // Number of free elements / subset.
             SubsetToElementVector,  // Number of elem covered 0 or 1 times /
                                     // subset.
             SubsetBoolVector>       // Removability for each subset.
  ComputeImpliedData(const ElementToSubsetVector& cvrg) const;

  // Internal UnsafeToggle where value is a constant for the template.
  template <bool value>
  std::vector<SubsetIndex> UnsafeToggleInternal(SubsetIndex subset);

  // The weighted set covering model on which the solver is run.
  SetCoverModel* model_;

  // Current cost.
  Cost cost_;

  // The number of uncovered (or free) elements in the current solution.
  ElementIndex num_uncovered_elements_;

  // Current assignment.
  SubsetBoolVector is_selected_;

  // A vector that for each subset gives the number of free elements, i.e.
  // elements whose coverage is 0.
  // problem.
  SubsetToElementVector num_free_elements_;

  // Counts the number of free or exactly covered elements, i.e. whose coverage
  // is 0 or 1.
  SubsetToElementVector num_coverage_le_1_elements_;

  // The coverage of an element is the number of used subsets which contains
  // the said element.
  ElementToSubsetVector coverage_;

  // True if the subset is redundant, i.e. can be removed from the solution
  // without making it infeasible.
  SubsetBoolVector is_removable_;
};

}  // namespace operations_research
#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_INVARIANT_H_
