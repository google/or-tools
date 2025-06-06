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

#include "ortools/sat/2d_try_edge_propagator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace sat {

int TryEdgeRectanglePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_.WatchAllBoxes(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

TryEdgeRectanglePropagator::~TryEdgeRectanglePropagator() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"TryEdgeRectanglePropagator/called", num_calls_});
  stats.push_back({"TryEdgeRectanglePropagator/conflicts", num_conflicts_});
  stats.push_back(
      {"TryEdgeRectanglePropagator/propagations", num_propagations_});

  shared_stats_->AddStats(stats);
}

void TryEdgeRectanglePropagator::PopulateActiveBoxRanges() {
  const int num_boxes = helper_.NumBoxes();
  placed_boxes_.resize(num_boxes);
  active_box_ranges_.resize(num_boxes);
  is_active_.resize(num_boxes);
  has_mandatory_region_.resize(num_boxes);
  mandatory_regions_.resize(num_boxes);
  is_in_cache_.resize(num_boxes);

  changed_mandatory_.clear();
  changed_item_.clear();
  for (int box = 0; box < num_boxes; ++box) {
    bool inactive = !helper_.IsPresent(box);
    RectangleInRange rec;
    if (!inactive) {
      rec = helper_.GetItemRangeForSizeMin(box);
      if (rec.x_size == 0 || rec.y_size == 0) {
        inactive = true;
      }
    }
    is_active_[box] = !inactive;
    if (inactive) {
      is_in_cache_[box] = false;
      has_mandatory_region_.Set(box, false);
      continue;
    }
    if (is_in_cache_[box] && rec == active_box_ranges_[box]) {
      DCHECK(mandatory_regions_[box] == rec.GetMandatoryRegion());
      DCHECK(has_mandatory_region_[box] ==
             (rec.GetMandatoryRegion() != Rectangle::GetEmpty()));
      continue;
    }
    active_box_ranges_[box] = rec;
    changed_item_.push_back(box);
    const Rectangle mandatory_region = rec.GetMandatoryRegion();
    const bool has_mandatory_region =
        (mandatory_region != Rectangle::GetEmpty());
    if (has_mandatory_region) {
      if (!has_mandatory_region_[box] || !is_in_cache_[box] ||
          mandatory_region != mandatory_regions_[box]) {
        changed_mandatory_.push_back(box);
      }
    }
    mandatory_regions_[box] = mandatory_region;
    has_mandatory_region_.Set(box, has_mandatory_region);
    is_in_cache_[box] = false;
  }
}

bool TryEdgeRectanglePropagator::CanPlace(
    int box_index, const std::pair<IntegerValue, IntegerValue>& position,
    CompactVectorVector<int>* with_conflict) const {
  bool can_place = true;
  if (with_conflict != nullptr) {
    with_conflict->Add({});
  }
  const Rectangle placed_box = {
      .x_min = position.first,
      .x_max = position.first + active_box_ranges_[box_index].x_size,
      .y_min = position.second,
      .y_max = position.second + active_box_ranges_[box_index].y_size};
  const absl::Span<const Rectangle> mandatory_regions = mandatory_regions_;
  for (const int i : has_mandatory_region_) {
    if (i == box_index) continue;
    const Rectangle& mandatory_region = mandatory_regions[i];
    if (!mandatory_region.IsDisjoint(placed_box)) {
      if (with_conflict != nullptr) {
        with_conflict->AppendToLastVector(i);
        can_place = false;
      } else {
        return false;
      }
    }
  }
  return can_place;
}

bool TryEdgeRectanglePropagator::Propagate() {
  if (!helper_.SynchronizeAndSetDirection(
          x_is_forward_after_swap_, y_is_forward_after_swap_, swap_x_and_y_)) {
    return false;
  }

  num_calls_++;

  PopulateActiveBoxRanges();

  // Our algo is quadratic, so we don't want to run it on really large problems.
  if (changed_item_.size() > 1000) {
    return true;
  }

  // If a mandatory region is changed, we need to replace any cached box that
  // now became overlapping with it.
  const int num_active_ranges = active_box_ranges_.size();
  for (const int mandatory_idx : changed_mandatory_) {
    const Rectangle& mandatory_region = mandatory_regions_[mandatory_idx];
    for (int i = 0; i < num_active_ranges; i++) {
      if (i == mandatory_idx || !is_in_cache_[i]) continue;
      if (!placed_boxes_[i].IsDisjoint(mandatory_region)) {
        changed_item_.push_back(i);
        is_in_cache_[i] = false;
      }
    }
  }

  if (changed_item_.empty()) {
    return true;
  }
  gtl::STLSortAndRemoveDuplicates(&changed_item_);

  potential_x_positions_.clear();
  potential_y_positions_.clear();
  for (const int i : has_mandatory_region_) {
    const Rectangle& mandatory_region = mandatory_regions_[i];
    potential_x_positions_.push_back(mandatory_region.x_max);
    potential_y_positions_.push_back(mandatory_region.y_max);
  }

  gtl::STLSortAndRemoveDuplicates(&potential_x_positions_);
  gtl::STLSortAndRemoveDuplicates(&potential_y_positions_);

  std::vector<std::pair<int, std::optional<IntegerValue>>> found_propagations;
  for (const int i : changed_item_) {
    DCHECK(!is_in_cache_[i]);
    DCHECK(is_active_[i]);
    const RectangleInRange& box = active_box_ranges_[i];

    if (CanPlace(i, {box.bounding_area.x_min, box.bounding_area.y_min})) {
      placed_boxes_[i] = {.x_min = box.bounding_area.x_min,
                          .x_max = box.bounding_area.x_min + box.x_size,
                          .y_min = box.bounding_area.y_min,
                          .y_max = box.bounding_area.y_min + box.y_size};
      is_in_cache_[i] = true;
      continue;
    }

    bool placed_at_x_min = false;
    const int y_start =
        absl::c_lower_bound(potential_y_positions_, box.bounding_area.y_min) -
        potential_y_positions_.begin();
    for (int j = y_start; j < potential_y_positions_.size(); ++j) {
      if (potential_y_positions_[j] > box.bounding_area.y_max - box.y_size) {
        // potential_y_positions is sorted, so we can stop here.
        break;
      }
      if (CanPlace(i, {box.bounding_area.x_min, potential_y_positions_[j]})) {
        placed_at_x_min = true;
        placed_boxes_[i] = {.x_min = box.bounding_area.x_min,
                            .x_max = box.bounding_area.x_min + box.x_size,
                            .y_min = potential_y_positions_[j],
                            .y_max = potential_y_positions_[j] + box.y_size};
        is_in_cache_[i] = true;
        break;
      }
    }
    if (placed_at_x_min) continue;

    // We could not find any placement of the box at its current lower bound!
    // Thus, we are sure we have something to propagate. Let's find the new
    // lower bound (or a conflict). Note that the code below is much less
    // performance critical than the code above, since it only triggers on
    // propagations.
    std::optional<IntegerValue> new_x_min = std::nullopt;
    for (int j = 0; j < potential_x_positions_.size(); ++j) {
      if (potential_x_positions_[j] < box.bounding_area.x_min) {
        continue;
      }
      if (potential_x_positions_[j] > box.bounding_area.x_max - box.x_size) {
        continue;
      }
      if (CanPlace(i, {potential_x_positions_[j], box.bounding_area.y_min})) {
        new_x_min = potential_x_positions_[j];
        break;
      }
      for (int k = y_start; k < potential_y_positions_.size(); ++k) {
        const IntegerValue potential_y_position = potential_y_positions_[k];
        if (potential_y_position > box.bounding_area.y_max - box.y_size) {
          break;
        }
        if (CanPlace(i, {potential_x_positions_[j], potential_y_position})) {
          // potential_x_positions is sorted, so the first we found is the
          // lowest one.
          new_x_min = potential_x_positions_[j];
          break;
        }
      }
      if (new_x_min.has_value()) {
        break;
      }
    }
    found_propagations.push_back({i, new_x_min});
  }

  return ExplainAndPropagate(found_propagations);
}

std::vector<int> TryEdgeRectanglePropagator::GetMinimumProblemWithPropagation(
    int box_index, IntegerValue new_x_min) {
  // We know that we can't place the box at x < new_x_min (which can be
  // start_max for a conflict). The explanation for the propagation is complex:
  // we tried a lot of positions, and each one overlaps with the mandatory part
  // of at least one box. We want to find the smallest set of "conflicting
  // boxes" that would still forbid every possible placement. To do that, we
  // build a vector with, for each placement position, the list boxes that
  // conflict when placing the box at that position. Then we solve
  // (approximately) a set cover problem to find the smallest set of boxes that
  // will still makes all positions conflicting.
  const RectangleInRange& box = active_box_ranges_[box_index];

  // We need to rerun the main propagator loop logic, but this time keeping
  // track of which boxes conflicted for each position.
  const int y_start =
      absl::c_lower_bound(potential_y_positions_, box.bounding_area.y_min) -
      potential_y_positions_.begin();
  conflicts_per_x_and_y_.clear();
  CHECK(!CanPlace(box_index, {box.bounding_area.x_min, box.bounding_area.y_min},
                  &conflicts_per_x_and_y_));
  for (int j = y_start; j < potential_y_positions_.size(); ++j) {
    if (potential_y_positions_[j] > box.bounding_area.y_max - box.y_size) {
      // potential_y_positions is sorted, so we can stop here.
      break;
    }
    CHECK(!CanPlace(box_index,
                    {box.bounding_area.x_min, potential_y_positions_[j]},
                    &conflicts_per_x_and_y_));
  }
  for (int j = 0; j < potential_x_positions_.size(); ++j) {
    if (potential_x_positions_[j] < box.bounding_area.x_min) {
      continue;
    }
    if (potential_x_positions_[j] >= new_x_min) {
      continue;
    }
    CHECK(!CanPlace(box_index,
                    {potential_x_positions_[j], box.bounding_area.y_min},
                    &conflicts_per_x_and_y_));
    for (int k = y_start; k < potential_y_positions_.size(); ++k) {
      const IntegerValue potential_y_position = potential_y_positions_[k];
      if (potential_y_position > box.bounding_area.y_max - box.y_size) {
        break;
      }
      CHECK(!CanPlace(box_index,
                      {potential_x_positions_[j], potential_y_position},
                      &conflicts_per_x_and_y_));
    }
  }

  // Now gather the data per box to make easier to use the set cover solver API.
  // TODO(user): skip the boxes that are fixed at level zero. They do not
  // contribute to the size of the explanation (so we shouldn't minimize their
  // number) and make the SetCover problem harder to solve.
  std::vector<std::vector<int>> conflicting_position_per_box(
      active_box_ranges_.size(), std::vector<int>());
  for (int i = 0; i < conflicts_per_x_and_y_.size(); ++i) {
    DCHECK(!conflicts_per_x_and_y_[i].empty());
    for (const int j : conflicts_per_x_and_y_[i]) {
      conflicting_position_per_box[j].push_back(i);
    }
  }
  SetCoverModel model;
  for (const auto& conflicts : conflicting_position_per_box) {
    if (conflicts.empty()) continue;
    model.AddEmptySubset(/*cost=*/1);
    for (const int i : conflicts) {
      model.AddElementToLastSubset(i);
    }
  }
  DCHECK(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);
  LazyElementDegreeSolutionGenerator greedy_search(&inv);
  CHECK(greedy_search.NextSolution());
  LazySteepestSearch steepest_search(&inv);
  CHECK(steepest_search.NextSolution());
  GuidedLocalSearch search(&inv);
  CHECK(search.SetMaxIterations(100).NextSolution());
  DCHECK(inv.CheckConsistency(
      SetCoverInvariant::ConsistencyLevel::kFreeAndUncovered));

  int count = 0;
  const auto& solution = inv.is_selected();
  std::vector<int> boxes_participating_in_propagation;
  boxes_participating_in_propagation.reserve(model.num_subsets() + 1);
  boxes_participating_in_propagation.push_back(box_index);
  for (int i = 0; i < conflicting_position_per_box.size(); ++i) {
    const auto& conflicts = conflicting_position_per_box[i];
    if (conflicts.empty()) continue;
    if (solution[SubsetIndex(count)]) {
      boxes_participating_in_propagation.push_back(i);
    }
    count++;
  }
  VLOG_EVERY_N_SEC(3, 2) << "Found no_overlap_2d constraint propagation with "
                         << boxes_participating_in_propagation.size() << "/"
                         << (model.num_subsets() + 1) << " items";

  // TODO(user): We now know for each box the list of placements that it
  // contributes to the conflict. We could use this information to relax the
  // bounds of this box on the explanation of the propagation. For example, for
  // a box that always overlaps at least five units to the right when it does,
  // we could call AddStartMinReason(x_min - 4) instead of
  // AddStartMinReason(x_min).
  return boxes_participating_in_propagation;
}

bool TryEdgeRectanglePropagator::ExplainAndPropagate(
    const std::vector<std::pair<int, std::optional<IntegerValue>>>&
        found_propagations) {
  for (const auto& [box_index, new_x_min] : found_propagations) {
    const RectangleInRange& box = active_box_ranges_[box_index];
    helper_.ClearReason();

    const std::vector<int> minimum_problem_with_propagator =
        GetMinimumProblemWithPropagation(
            box_index, new_x_min.has_value()
                           ? *new_x_min
                           : box.bounding_area.x_max - box.x_size);
    for (const int j : minimum_problem_with_propagator) {
      DCHECK(is_active_[j]);
      // Important: we also add to the reason the actual box we are changing the
      // x_min. This is important, since we don't check if there are any
      // feasible placement before its current x_min, so it needs to be part of
      // the reason.
      const RectangleInRange& box_reason = active_box_ranges_[j];
      const int b = box_reason.box_index;

      helper_.AddLeftMinReason(b, box_reason.bounding_area.x_min);
      helper_.AddBottomMinReason(b, box_reason.bounding_area.y_min);

      if (j != box_index || !new_x_min.has_value()) {
        // We don't need to add to the reason the x_max for the box we are
        // pushing the x_min, except if we found a conflict.
        helper_.AddLeftMaxReason(
            b, box_reason.bounding_area.x_max - box_reason.x_size);
      }
      helper_.AddBottomMaxReason(
          b, box_reason.bounding_area.y_max - box_reason.y_size);

      helper_.AddSizeMinReason(b);
      helper_.AddPresenceReason(b);
    }
    if (new_x_min.has_value()) {
      num_propagations_++;
      if (!helper_.IncreaseLeftMin(box_index, *new_x_min)) {
        return false;
      }
    } else {
      num_conflicts_++;
      return helper_.ReportConflict();
    }
  }
  return true;
}

void CreateAndRegisterTryEdgePropagator(NoOverlap2DConstraintHelper* helper,
                                        Model* model,
                                        GenericLiteralWatcher* watcher,
                                        int priority) {
  TryEdgeRectanglePropagator* try_edge_propagator =
      new TryEdgeRectanglePropagator(true, true, false, helper, model);
  watcher->SetPropagatorPriority(try_edge_propagator->RegisterWith(watcher),
                                 priority);
  model->TakeOwnership(try_edge_propagator);

  TryEdgeRectanglePropagator* try_edge_propagator_mirrored =
      new TryEdgeRectanglePropagator(false, true, false, helper, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_mirrored->RegisterWith(watcher), priority);
  model->TakeOwnership(try_edge_propagator_mirrored);

  TryEdgeRectanglePropagator* try_edge_propagator_swap =
      new TryEdgeRectanglePropagator(true, true, true, helper, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_swap->RegisterWith(watcher), priority);
  model->TakeOwnership(try_edge_propagator_swap);

  TryEdgeRectanglePropagator* try_edge_propagator_swap_mirrored =
      new TryEdgeRectanglePropagator(false, true, true, helper, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_swap_mirrored->RegisterWith(watcher), priority);
  model->TakeOwnership(try_edge_propagator_swap_mirrored);
}

}  // namespace sat
}  // namespace operations_research
