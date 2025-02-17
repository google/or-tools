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

#ifndef OR_TOOLS_SET_COVER_ASSIGNMENT_H_
#define OR_TOOLS_SET_COVER_ASSIGNMENT_H_

#include <vector>

#include "ortools/set_cover/capacity_invariant.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

// `SetCoverAssignment` stores a possibly partial, possibly infeasible solution
// to a `SetCoverModel`. It only stores a solution and no metadata,
// so that it can be shared efficiently among constraints.
//
// This class is equivalent to an `Assignment` object in the CP/routing solver.
// (//ortools/routing).
class SetCoverAssignment {
 public:
  // Constructs an empty set covering assignment.
  //
  // The model size or costs must not change after the invariant was built.
  // The caller must guarantee that the model outlives the assignment without
  // changing its costs.
  explicit SetCoverAssignment(const SetCoverModel& m)
      : model_(m), constraint_(nullptr), side_constraints_({}) {
    Clear();
  }

  // Clears the current assignment.
  void Clear();

  // Adds a constraint to the problem. At least one set-covering constraint is
  // required; use side constraints as required (no set-covering constraint can
  // be a side constraint).
  void AttachInvariant(SetCoverInvariant* i);
  void AttachInvariant(CapacityInvariant* i);

  // Returns the cost of current solution.
  Cost cost() const { return cost_; }

  // Returns the subset assignment vector.
  const SubsetBoolVector& assignment() const { return values_; }

  // Sets the subset's assignment to the given bool.
  void SetValue(SubsetIndex subset, bool is_selected,
                SetCoverInvariant::ConsistencyLevel set_cover_consistency);

  // Returns the current solution as a proto.
  SetCoverSolutionResponse ExportSolutionAsProto() const;

  // Loads the solution and recomputes the data in the invariant.
  //
  // The given assignment must fit the model of this assignment.
  void LoadAssignment(const SubsetBoolVector& solution);

  // Imports the solution from a proto.
  //
  // The given assignment must fit the model of this assignment.
  void ImportSolutionFromProto(const SetCoverSolutionResponse& message);

  // Checks the consistency of the solution (between the selected subsets and
  // the solution cost).
  bool CheckConsistency() const;

 private:
  // Computes the cost for the given choices.
  Cost ComputeCost(const SubsetBoolVector& choices) const;

  // The weighted set covering model on which the solver is run.
  const SetCoverModel& model_;

  // Current cost of the assignment.
  Cost cost_;

  // Current assignment. Takes |S| bits.
  SubsetBoolVector values_;

  // Constraints that this assignment must respect. The constraints are checked
  // every time the assignment changes (with the methods `Flip`, `Select`, and
  // `Deselect`).
  //
  // For now, the only side constraints are capacity constraints.
  SetCoverInvariant* constraint_;
  // TODO(user): merge the several constraints into one invariant.
  std::vector<CapacityInvariant*> side_constraints_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_ASSIGNMENT_H_
