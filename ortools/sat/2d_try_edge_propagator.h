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

#ifndef OR_TOOLS_SAT_2D_TRY_EDGE_PROPAGATOR_H_
#define OR_TOOLS_SAT_2D_TRY_EDGE_PROPAGATOR_H_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

// Propagator that for each boxes participating in a no_overlap_2d constraint
// try to find the leftmost valid position that is compatible with all the
// other boxes. If none is found, it will propagate a conflict. Otherwise, if
// it is different from the current x_min, it will propagate the new x_min.
void CreateAndRegisterTryEdgePropagator(SchedulingConstraintHelper* x,
                                        SchedulingConstraintHelper* y,
                                        Model* model,
                                        GenericLiteralWatcher* watcher);

// Exposed for testing.
class TryEdgeRectanglePropagator : public PropagatorInterface {
 public:
  TryEdgeRectanglePropagator(bool x_is_forward, bool y_is_forward,
                             SchedulingConstraintHelper* x,
                             SchedulingConstraintHelper* y, Model* model)
      : x_(*x),
        y_(*y),
        shared_stats_(model->GetOrCreate<SharedStatistics>()),
        x_is_forward_(x_is_forward),
        y_is_forward_(y_is_forward) {}

  ~TryEdgeRectanglePropagator() override;

  bool Propagate() final;
  int RegisterWith(GenericLiteralWatcher* watcher);

 protected:
  // placed_boxes_ is a list that is only meaningful for indices for which
  // is_in_cache_[box_index] is true. After applying this condition,
  // placed_boxes_ contains a list of boxes placed at their current x_min and
  // that does not overlap with the mandatory region of any other box in
  // placed_boxes_. In other words, there is no point on looking for any
  // propagation for this heuristic between boxes that are already in
  // placed_boxes_.
  std::vector<Rectangle> placed_boxes_;
  std::vector<bool> is_in_cache_;

  std::vector<Rectangle> mandatory_regions_;
  std::vector<RectangleInRange> active_box_ranges_;
  std::vector<bool> is_active_;
  Bitset64<int> has_mandatory_region_;

  std::vector<int> changed_item_;
  std::vector<int> changed_mandatory_;

  virtual bool ExplainAndPropagate(
      const std::vector<std::pair<int, std::optional<IntegerValue>>>&
          found_propagations);

  // This function assumes that a propagation is found and the box with index
  // `box_index` cannot be placed to the left of new_x_min. It returns a list of
  // indices of boxes that defines a subproblem where the propagation is still
  // valid, including `box_index` itself.
  std::vector<int> GetMinimumProblemWithPropagation(int box_index,
                                                    IntegerValue new_x_min);

 private:
  void PopulateActiveBoxRanges();

  SchedulingConstraintHelper& x_;
  SchedulingConstraintHelper& y_;
  SharedStatistics* shared_stats_;
  bool x_is_forward_;
  bool y_is_forward_;
  std::vector<IntegerValue> cached_y_hint_;

  std::vector<IntegerValue> potential_x_positions_;
  std::vector<IntegerValue> potential_y_positions_;

  int64_t num_conflicts_ = 0;
  int64_t num_propagations_ = 0;
  int64_t num_calls_ = 0;

  CompactVectorVector<int> conflicts_per_x_and_y_;
  bool CanPlace(int box_index,
                const std::pair<IntegerValue, IntegerValue>& position,
                CompactVectorVector<int>* with_conflicts = nullptr) const;

  TryEdgeRectanglePropagator(const TryEdgeRectanglePropagator&) = delete;
  TryEdgeRectanglePropagator& operator=(const TryEdgeRectanglePropagator&) =
      delete;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_2D_TRY_EDGE_PROPAGATOR_H_
