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

#include "ortools/sat/2d_mandatory_overlap_propagator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/scheduling_helpers.h"

namespace operations_research {
namespace sat {

int MandatoryOverlapPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_.WatchAllBoxes(id);
  return id;
}

MandatoryOverlapPropagator::~MandatoryOverlapPropagator() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"MandatoryOverlapPropagator/called_with_zero_area",
                   num_calls_zero_area_});
  stats.push_back({"MandatoryOverlapPropagator/called_without_zero_area",
                   num_calls_nonzero_area_});
  stats.push_back({"MandatoryOverlapPropagator/conflicts", num_conflicts_});

  shared_stats_->AddStats(stats);
}

bool MandatoryOverlapPropagator::Propagate() {
  if (!helper_.SynchronizeAndSetDirection(true, true, false)) return false;

  mandatory_regions_.clear();
  mandatory_regions_index_.clear();
  bool has_zero_area_boxes = false;
  absl::Span<const TaskTime> tasks =
      helper_.x_helper().TaskByIncreasingNegatedStartMax();
  for (int i = tasks.size() - 1; i >= 0; --i) {
    const int b = tasks[i].task_index;
    if (!helper_.IsPresent(b)) continue;
    const ItemWithVariableSize item = helper_.GetItemWithVariableSize(b);
    if (item.x.start_max > item.x.end_min ||
        item.y.start_max > item.y.end_min) {
      continue;
    }
    mandatory_regions_.push_back({.x_min = item.x.start_max,
                                  .x_max = item.x.end_min,
                                  .y_min = item.y.start_max,
                                  .y_max = item.y.end_min});
    mandatory_regions_index_.push_back(b);

    if (mandatory_regions_.back().SizeX() == 0 ||
        mandatory_regions_.back().SizeY() == 0) {
      has_zero_area_boxes = true;
    }
  }
  std::optional<std::pair<int, int>> conflict;
  if (has_zero_area_boxes) {
    num_calls_zero_area_++;
    conflict = FindOneIntersectionIfPresentWithZeroArea(mandatory_regions_);
  } else {
    num_calls_nonzero_area_++;
    conflict = FindOneIntersectionIfPresent(mandatory_regions_);
  }

  if (conflict.has_value()) {
    num_conflicts_++;
    return helper_.ReportConflictFromTwoBoxes(
        mandatory_regions_index_[conflict->first],
        mandatory_regions_index_[conflict->second]);
  }
  return true;
}

void CreateAndRegisterMandatoryOverlapPropagator(
    NoOverlap2DConstraintHelper* helper, Model* model,
    GenericLiteralWatcher* watcher, int priority) {
  MandatoryOverlapPropagator* propagator =
      new MandatoryOverlapPropagator(helper, model);
  watcher->SetPropagatorPriority(propagator->RegisterWith(watcher), priority);
  model->TakeOwnership(propagator);
}

}  // namespace sat
}  // namespace operations_research
