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

#include "ortools/sat/2d_try_edge_propagator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

int TryEdgeRectanglePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  x_.WatchAllTasks(id);
  y_.WatchAllTasks(id);
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
  stats.push_back({"TryEdgeRectanglePropagator/cache_hits", num_cache_hits_});
  stats.push_back(
      {"TryEdgeRectanglePropagator/cache_misses", num_cache_misses_});

  shared_stats_->AddStats(stats);
}

void TryEdgeRectanglePropagator::PopulateActiveBoxRanges() {
  const int num_boxes = x_.NumTasks();
  placed_boxes_.resize(num_boxes);
  active_box_ranges_.resize(num_boxes);
  is_active_.resize(num_boxes);
  has_mandatory_region_.resize(num_boxes);
  mandatory_regions_.resize(num_boxes);
  is_in_cache_.resize(num_boxes);

  changed_mandatory_.clear();
  changed_item_.clear();
  for (int box = 0; box < num_boxes; ++box) {
    const bool inactive = (x_.SizeMin(box) == 0 || y_.SizeMin(box) == 0 ||
                           !x_.IsPresent(box) || !y_.IsPresent(box));
    is_active_[box] = !inactive;
    if (inactive) {
      is_in_cache_[box] = false;
      has_mandatory_region_.Set(box, false);
      continue;
    }
    const RectangleInRange rec = {
        .box_index = box,
        .bounding_area = {.x_min = x_.StartMin(box),
                          .x_max = x_.StartMax(box) + x_.SizeMin(box),
                          .y_min = y_.StartMin(box),
                          .y_max = y_.StartMax(box) + y_.SizeMin(box)},
        .x_size = x_.SizeMin(box),
        .y_size = y_.SizeMin(box)};
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
    int box_index,
    const std::pair<IntegerValue, IntegerValue>& position) const {
  const Rectangle placed_box = {
      .x_min = position.first,
      .x_max = position.first + active_box_ranges_[box_index].x_size,
      .y_min = position.second,
      .y_max = position.second + active_box_ranges_[box_index].y_size};
  for (const int i : has_mandatory_region_) {
    if (i == box_index) continue;
    const Rectangle& mandatory_region = mandatory_regions_[i];
    if (!mandatory_region.IsDisjoint(placed_box)) {
      return false;
    }
  }
  return true;
}

bool TryEdgeRectanglePropagator::Propagate() {
  if (!x_.SynchronizeAndSetTimeDirection(x_is_forward_)) return false;
  if (!y_.SynchronizeAndSetTimeDirection(y_is_forward_)) return false;

  num_calls_++;

  PopulateActiveBoxRanges();

  // If a mandatory region is changed, we need to replace any cached box that
  // now became overlapping with it.
  for (const int mandatory_idx : changed_mandatory_) {
    for (int i = 0; i < active_box_ranges_.size(); i++) {
      if (i == mandatory_idx || !is_in_cache_[i]) continue;
      if (!placed_boxes_[i].IsDisjoint(mandatory_regions_[mandatory_idx])) {
        changed_item_.push_back(i);
        is_in_cache_[i] = false;
      }
    }
  }

  if (changed_item_.empty()) {
    return true;
  }
  gtl::STLSortAndRemoveDuplicates(&changed_item_);

  // Our algo is quadratic, so we don't want to run it on really large problems.
  if (changed_item_.size() > 1000) {
    return true;
  }

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

bool TryEdgeRectanglePropagator::ExplainAndPropagate(
    const std::vector<std::pair<int, std::optional<IntegerValue>>>&
        found_propagations) {
  for (const auto& [box_index, new_x_min] : found_propagations) {
    const RectangleInRange& box = active_box_ranges_[box_index];
    x_.ClearReason();
    y_.ClearReason();
    for (int j = 0; j < active_box_ranges_.size(); ++j) {
      if (!is_active_[j]) continue;
      // Important: we also add to the reason the actual box we are changing the
      // x_min. This is important, since we don't check if there are any
      // feasible placement before its current x_min, so it needs to be part of
      // the reason.
      const RectangleInRange& box_reason = active_box_ranges_[j];
      if (j != box_index) {
        const Rectangle mandatory_region = box_reason.GetMandatoryRegion();
        if (mandatory_region == Rectangle::GetEmpty()) {
          continue;
        }
        // Don't add to the reason any box that was not participating in the
        // placement decision. Ie., anything before the old x_min or after the
        // new x_max.
        if (new_x_min.has_value() &&
            mandatory_region.x_min > *new_x_min + box_reason.x_size) {
          continue;
        }
        if (new_x_min.has_value() &&
            mandatory_region.x_max < box.bounding_area.x_min) {
          continue;
        }
        if (mandatory_region.y_min > box.bounding_area.y_max ||
            mandatory_region.y_max < box.bounding_area.y_min) {
          continue;
        }
      }

      const int b = box_reason.box_index;

      x_.AddStartMinReason(b, box_reason.bounding_area.x_min);
      y_.AddStartMinReason(b, box_reason.bounding_area.y_min);

      x_.AddStartMaxReason(b,
                           box_reason.bounding_area.x_max - box_reason.x_size);
      y_.AddStartMaxReason(b,
                           box_reason.bounding_area.y_max - box_reason.y_size);

      x_.AddSizeMinReason(b);
      y_.AddSizeMinReason(b);

      x_.AddPresenceReason(b);
      y_.AddPresenceReason(b);
    }
    x_.ImportOtherReasons(y_);
    if (new_x_min.has_value()) {
      num_propagations_++;
      if (!x_.IncreaseStartMin(box.box_index, *new_x_min)) {
        return false;
      }
    } else {
      num_conflicts_++;
      return x_.ReportConflict();
    }
  }
  return true;
}

void CreateAndRegisterTryEdgePropagator(SchedulingConstraintHelper* x,
                                        SchedulingConstraintHelper* y,
                                        Model* model,
                                        GenericLiteralWatcher* watcher) {
  TryEdgeRectanglePropagator* try_edge_propagator =
      new TryEdgeRectanglePropagator(true, true, x, y, model);
  watcher->SetPropagatorPriority(try_edge_propagator->RegisterWith(watcher), 5);
  model->TakeOwnership(try_edge_propagator);

  TryEdgeRectanglePropagator* try_edge_propagator_mirrored =
      new TryEdgeRectanglePropagator(false, true, x, y, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_mirrored->RegisterWith(watcher), 5);
  model->TakeOwnership(try_edge_propagator_mirrored);

  TryEdgeRectanglePropagator* try_edge_propagator_swap =
      new TryEdgeRectanglePropagator(true, true, y, x, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_swap->RegisterWith(watcher), 5);
  model->TakeOwnership(try_edge_propagator_swap);

  TryEdgeRectanglePropagator* try_edge_propagator_swap_mirrored =
      new TryEdgeRectanglePropagator(false, true, y, x, model);
  watcher->SetPropagatorPriority(
      try_edge_propagator_swap_mirrored->RegisterWith(watcher), 5);
  model->TakeOwnership(try_edge_propagator_swap_mirrored);
}

}  // namespace sat
}  // namespace operations_research
