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

  // Flips is_selected_[subset] to its negation, and incrementally updates the
  // invariant. Calls Select or Deselect depending on value.
  template <bool FullUpdate = false>
  void Flip(SubsetIndex subset);

  // Sets is_selected_[subset] to true and incrementally updates the invariant.
  template <bool FullUpdate = false>
  void Select(SubsetIndex subset);

  // Sets is_selected_[subset] to false and incrementally updates the invariant.
  template <bool FullUpdate = false>
  void Deselect(SubsetIndex subset);

  // Returns the current solution as a proto.
  SetCoverSolutionResponse ExportSolutionAsProto() const;

  // Imports the solution from a proto.
  void ImportSolutionFromProto(const SetCoverSolutionResponse& message);

 private:
  // Computes the cost and the coverage vector for the given choices.
  // Temporarily uses |U| BaseInts.
  std::tuple<Cost, ElementToIntVector> ComputeCostAndCoverage(
      const SubsetBoolVector& choices) const;

  // Computes the global number of uncovered elements and the
  // vector containing the number of free elements for each subset from
  // a coverage vector.
  // Temporarily uses |J| BaseInts.
  // TODO(user): there is no need to separate this from ComputeCostAndCoverage.
  // Merge.
  std::tuple<BaseInt,            // Number of uncovered elements,
             SubsetToIntVector,  // Number of free elements,
             SubsetToIntVector,  // Number of non-overcovered elements,
             SubsetBoolVector>   // Redundancy for each of the subsets.
  ComputeImpliedData(const ElementToIntVector& cvrg) const;

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
  // Takes |J| bits.
  SubsetBoolVector is_selected_;

  // A vector that for each subset gives the number of free elements, i.e.
  // elements whose coverage is 0.
  // problem.
  // Takes |J| BaseInts.
  SubsetToIntVector num_free_elements_;

  // The coverage of an element is the number of used subsets which contains
  // the said element.
  // Takes |U| BaseInts
  ElementToIntVector coverage_;

  // A trace of the decisions leading to the current solution.
  // Takes at most |J| BaseInts.
  std::vector<SetCoverDecision> trace_;

  // Counts the number of free or exactly covered elements, i.e. whose coverage
  // is 0 or 1.
  SubsetToIntVector num_non_overcovered_elements_;

  // True if the subset is redundant, i.e. can be removed from the solution
  // without making it infeasible.
  SubsetBoolVector is_redundant_;

  // Denotes whether is_redundant_ and num_non_overcovered_elements_ have been
  // updated. Initially true, it becomes false as soon as Flip<FullUpdate>,
  // Select<FullUpdate> and Deselect<FullUpdate> have been called with
  // FullUpdate == false. The full update can be achieved again with
  // RecomputeInvariant, at a high cost.
  // TODO(user): implement faster, lazy update.
  bool is_fully_updated_;

  // True if the CPU supports the AVX-512 instruction set.
  bool supports_avx512_;
};

template <bool FullUpdate>
void SetCoverInvariant::Flip(SubsetIndex subset) {
  if (!is_selected_[subset]) {
    Select<FullUpdate>(subset);
  } else {
    Deselect<FullUpdate>(subset);
  }
}

template <bool FullUpdate>
void SetCoverInvariant::Select(SubsetIndex subset) {
  if (!FullUpdate) {
    is_fully_updated_ = false;
  }
  DVLOG(1) << "Selecting subset " << subset;
  DCHECK(!is_selected_[subset]);
  DCHECK(CheckConsistency());
  trace_.push_back(SetCoverDecision(subset, true));
  is_selected_[subset] = true;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ += subset_costs[subset];
  if (supports_avx512_) {
    SelectAvx512(subset);
    return;
  }
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : columns[subset]) {
    if (coverage_[element] == 0) {
      // `element` will be newly covered.
      --num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        --num_free_elements_[impacted_subset];
      }
    } else if (FullUpdate && coverage_[element] == 1) {
      // `element` will be newly overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        --num_non_overcovered_elements_[impacted_subset];
        if (num_non_overcovered_elements_[impacted_subset] == 0) {
          // All the elements in impacted_subset are now overcovered, so it
          // is removable. Note that this happens only when the last element
          // of impacted_subset becomes overcovered.
          is_redundant_[impacted_subset] = true;
        }
      }
    }
    // Update coverage.
    ++coverage_[element];
  }
  DCHECK(CheckConsistency());
}

template <bool FullUpdate>
void SetCoverInvariant::Deselect(SubsetIndex subset) {
  if (!FullUpdate) {
    is_fully_updated_ = false;
  }
  DVLOG(1) << "Deselecting subset " << subset;
  // If already selected, then num_free_elements == 0.
  DCHECK(is_selected_[subset]);
  DCHECK_EQ(num_free_elements_[subset], 0);
  DCHECK(CheckConsistency());
  trace_.push_back(SetCoverDecision(subset, false));
  is_selected_[subset] = false;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ -= subset_costs[subset];
  if (supports_avx512_) {
    DeselectAvx512(subset);
    return;
  }
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : columns[subset]) {
    // Update coverage.
    --coverage_[element];
    if (coverage_[element] == 0) {
      // `element` is no longer covered.
      ++num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        ++num_free_elements_[impacted_subset];
      }
    } else if (FullUpdate && coverage_[element] == 1) {
      // `element` will be no longer overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        if (num_non_overcovered_elements_[impacted_subset] == 0) {
          // There is one element of impacted_subset which is not overcovered.
          // impacted_subset has just become non-removable.
          is_redundant_[impacted_subset] = false;
        }
        ++num_non_overcovered_elements_[impacted_subset];
      }
    }
  }
  DCHECK(CheckConsistency());
}

}  // namespace operations_research
#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_INVARIANT_H_
