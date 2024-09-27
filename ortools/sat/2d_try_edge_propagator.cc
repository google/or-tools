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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "ortools/base/logging.h"
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
  active_box_ranges_.clear();
  active_box_ranges_.reserve(num_boxes);
  for (int box = 0; box < num_boxes; ++box) {
    if (x_.SizeMin(box) == 0 || y_.SizeMin(box) == 0) continue;
    if (!x_.IsPresent(box) || !y_.IsPresent(box)) continue;

    active_box_ranges_.push_back(RectangleInRange{
        .box_index = box,
        .bounding_area = {.x_min = x_.StartMin(box),
                          .x_max = x_.StartMax(box) + x_.SizeMin(box),
                          .y_min = y_.StartMin(box),
                          .y_max = y_.StartMax(box) + y_.SizeMin(box)},
        .x_size = x_.SizeMin(box),
        .y_size = y_.SizeMin(box)});
  }
  max_box_index_ = num_boxes - 1;
}

bool TryEdgeRectanglePropagator::CanPlace(
    int box_index,
    const std::pair<IntegerValue, IntegerValue>& position) const {
  const Rectangle placed_box = {
      .x_min = position.first,
      .x_max = position.first + active_box_ranges_[box_index].x_size,
      .y_min = position.second,
      .y_max = position.second + active_box_ranges_[box_index].y_size};
  for (int i = 0; i < active_box_ranges_.size(); ++i) {
    if (i == box_index) continue;
    const RectangleInRange& box_reason = active_box_ranges_[i];
    const Rectangle mandatory_region = box_reason.GetMandatoryRegion();
    if (mandatory_region != Rectangle::GetEmpty() &&
        !mandatory_region.IsDisjoint(placed_box)) {
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

  if (cached_y_hint_.size() <= max_box_index_) {
    cached_y_hint_.resize(max_box_index_ + 1,
                          std::numeric_limits<IntegerValue>::max());
  }

  if (active_box_ranges_.size() < 2) {
    return true;
  }

  // Our algo is quadratic, so we don't want to run it on really large problems.
  if (active_box_ranges_.size() > 1000) {
    return true;
  }

  potential_x_positions_.clear();
  potential_y_positions_.clear();
  std::vector<std::pair<int, std::optional<IntegerValue>>> found_propagations;
  for (const RectangleInRange& box : active_box_ranges_) {
    const Rectangle mandatory_region = box.GetMandatoryRegion();
    if (mandatory_region == Rectangle::GetEmpty()) {
      continue;
    }
    potential_x_positions_.push_back(mandatory_region.x_max);
    potential_y_positions_.push_back(mandatory_region.y_max);
  }
  std::sort(potential_x_positions_.begin(), potential_x_positions_.end());
  std::sort(potential_y_positions_.begin(), potential_y_positions_.end());

  for (int i = 0; i < active_box_ranges_.size(); ++i) {
    const RectangleInRange& box = active_box_ranges_[i];

    // For each box, we need to answer whether there exist some y for which
    // (x_min, y) is not in conflict with any other box. If there is no such y,
    // we can propagate a larger lower bound on x. Now, for the most majority of
    // cases there is nothing to propagate, so we want to find the y that makes
    // (x_min, y) a valid placement as fast as possible. Now, since things don't
    // change that often we try the last y value that was a valid placement for
    // this box. This is just a hint: if it is not a valid placement, we will
    // try all "interesting" y values before concluding that no such y exist.
    const IntegerValue cached_y_hint = cached_y_hint_[box.box_index];
    if (cached_y_hint >= box.bounding_area.y_min &&
        cached_y_hint <= box.bounding_area.y_max - box.y_size) {
      if (CanPlace(i, {box.bounding_area.x_min, cached_y_hint})) {
        num_cache_hits_++;
        continue;
      }
    }
    num_cache_misses_++;
    if (CanPlace(i, {box.bounding_area.x_min, box.bounding_area.y_min})) {
      cached_y_hint_[box.box_index] = box.bounding_area.y_min;
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
        cached_y_hint_[box.box_index] = potential_y_positions_[j];
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
