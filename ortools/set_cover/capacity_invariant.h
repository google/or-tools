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

#ifndef OR_TOOLS_SET_COVER_CAPACITY_INVARIANT_H_
#define OR_TOOLS_SET_COVER_CAPACITY_INVARIANT_H_

#include "absl/log/check.h"
#include "ortools/set_cover/capacity_model.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
class CapacityInvariant {
 public:
  // Constructs an empty capacity invariant state.
  // The model may not change after the invariant was built.
  explicit CapacityInvariant(CapacityModel* m, SetCoverModel* sc)
      : model_(m), set_cover_model_(sc) {
    DCHECK(model_->ComputeFeasibility());
    Clear();
  }

  // Clears the invariant.
  void Clear();

  // Returns `true` when the constraint is not violated by this flipping move
  // and incrementally updates the invariant. Otherwise, returns `false` and
  // does not change the object.
  //
  // Flips is_selected_[subset] to its negation, by calling Select or Deselect
  // depending on value.
  bool Flip(SubsetIndex subset);

  // Returns `true` when the constraint would not be violated if this flipping
  // move is performed. Otherwise, returns `false`. The object never changes.
  bool CanFlip(SubsetIndex subset) const;

  // Returns `true` when the constraint is not violated by selecting all of the
  // items in the subset and incrementally updates the invariant. Otherwise,
  // returns `false` and does not change the object. (If the subset is already
  // selected, the behavior is undefined.)
  bool Select(SubsetIndex subset);

  // Returns `true` when the constraint would not be violated by selecting all
  // of the items in the subset. Otherwise, returns `false`. The object never
  // changes. (If the subset is already selected, the behavior is undefined.)
  bool CanSelect(SubsetIndex subset) const;

  // Returns `true` when the constraint is not violated by unselecting all of
  // the items in the subset and incrementally updates the invariant. Otherwise,
  // returns `false` and does not change the object. (If the subset is already
  // not selected, the behavior is undefined.)
  bool Deselect(SubsetIndex subset);

  // Returns `true` when the constraint would not be violated by unselecting all
  // of the items in the subset. Otherwise, returns `false`. The object never
  // changes. (If the subset is already not selected, the behavior is
  // undefined.)
  bool CanDeselect(SubsetIndex subset) const;

  // TODO(user): implement the functions where you only select/deselect an
  // item of a subset (instead of all items at once). The behavior gets much
  // more interesting: if two subsets cover one item and the two item-subset
  // combinations are terms in this capacity constraint, only one of them counts
  // towards the capacity.
  //
  // The solver is not yet ready for this move: you need to
  // decide which subset covers a given item, instead of ensuring that an item
  // is covered by at least one subset. Currently, we could aggregate the terms
  // per subset to make the code much faster when (de)selecting at the cost of
  // increased initialization times.

 private:
  // The capacity-constraint model on which the invariant runs.
  CapacityModel* model_;

  // The set-cover model on which the invariant runs.
  SetCoverModel* set_cover_model_;

  // Current slack of the constraint.
  operations_research::CapacityWeight current_slack_;

  // Current solution assignment.
  // TODO(user): reuse the assignment of a SetCoverInvariant.
  SubsetBoolVector is_selected_;

  // Determines the change in slack when (de)selecting the given subset.
  // The returned value is nonnegative; add it to the slack when selecting
  // and subtract it when deselecting.
  double ComputeSlackChange(SubsetIndex subset) const;

  // Determines whether the given slack change violates the constraint
  // (`false`) or not (`true`).
  bool SlackChangeFitsConstraint(double slack_change) const;
};
}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_CAPACITY_INVARIANT_H_
