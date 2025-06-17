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

#include "ortools/sat/2d_distances_propagator.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

Precedences2DPropagator::Precedences2DPropagator(
    NoOverlap2DConstraintHelper* helper, Model* model)
    : helper_(*helper),
      linear2_bounds_(model->GetOrCreate<Linear2Bounds>()),
      linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()) {
  model->GetOrCreate<LinearPropagator>()->SetPushAffineUbForBinaryRelation();
}

void Precedences2DPropagator::CollectPairsOfBoxesWithNonTrivialDistance() {
  helper_.SynchronizeAndSetDirection();
  non_trivial_pairs_.clear();

  struct VarUsage {
    // boxes[0=x, 1=y][0=start, 1=end]
    std::vector<int> boxes[2][2];
  };
  absl::flat_hash_map<IntegerVariable, VarUsage> var_to_box_and_coeffs;
  SparseBitset<IntegerVariable>& var_set =
      *linear2_bounds_->GetTemporyClearedAndResizedBitset();

  for (int dim = 0; dim < 2; ++dim) {
    const SchedulingConstraintHelper& dim_helper =
        dim == 0 ? helper_.x_helper() : helper_.y_helper();
    for (int j = 0; j < 2; ++j) {
      const absl::Span<const AffineExpression> interval_points =
          j == 0 ? dim_helper.Starts() : dim_helper.Ends();
      for (int i = 0; i < helper_.NumBoxes(); ++i) {
        const IntegerVariable var = interval_points[i].var;
        if (var != kNoIntegerVariable) {
          var_set.Set(PositiveVariable(var));
          var_to_box_and_coeffs[PositiveVariable(var)].boxes[dim][j].push_back(
              i);
        }
      }
    }
  }

  const absl::Span<const LinearExpression2> exprs =
      linear2_bounds_->GetAllExpressionsWithPotentialNonTrivialBounds(
          var_set.BitsetConstView());
  VLOG(2) << "CollectPairsOfBoxesWithNonTrivialDistance called, num_exprs: "
          << exprs.size();
  for (const LinearExpression2& expr : exprs) {
    auto it1 = var_to_box_and_coeffs.find(PositiveVariable(expr.vars[0]));
    auto it2 = var_to_box_and_coeffs.find(PositiveVariable(expr.vars[1]));
    DCHECK(it1 != var_to_box_and_coeffs.end());
    DCHECK(it2 != var_to_box_and_coeffs.end());

    const VarUsage& usage1 = it1->second;
    const VarUsage& usage2 = it2->second;
    for (int dim = 0; dim < 2; ++dim) {
      const SchedulingConstraintHelper& dim_helper =
          dim == 0 ? helper_.x_helper() : helper_.y_helper();
      for (const int box1 : usage1.boxes[dim][0 /* start */]) {
        for (const int box2 : usage2.boxes[dim][1 /* end */]) {
          if (box1 == box2) continue;
          const auto [expr2, unused] = EncodeDifferenceLowerThan(
              dim_helper.Starts()[box1], dim_helper.Ends()[box2],
              /*ub=unused*/ 0);
          if (expr == expr2) {
            if (box1 < box2) {
              non_trivial_pairs_.push_back({box1, box2});
            } else {
              non_trivial_pairs_.push_back({box2, box1});
            }
          }
        }
      }
    }
  }

  gtl::STLSortAndRemoveDuplicates(&non_trivial_pairs_);
}

bool Precedences2DPropagator::Propagate() {
  if (!helper_.SynchronizeAndSetDirection()) return false;
  if (last_helper_inprocessing_count_ != helper_.InProcessingCount() ||
      helper_.x_helper().CurrentDecisionLevel() == 0 ||
      last_linear2_timestamp_ != linear2_watcher_->Timestamp()) {
    last_helper_inprocessing_count_ = helper_.InProcessingCount();
    last_linear2_timestamp_ = linear2_watcher_->Timestamp();
    CollectPairsOfBoxesWithNonTrivialDistance();
  }

  num_calls_++;

  SchedulingConstraintHelper* helpers[2] = {&helper_.x_helper(),
                                            &helper_.y_helper()};

  for (const auto& [box1, box2] : non_trivial_pairs_) {
    DCHECK(box1 < helper_.NumBoxes());
    DCHECK(box2 < helper_.NumBoxes());
    DCHECK_NE(box1, box2);
    if (!helper_.IsPresent(box1) || !helper_.IsPresent(box2)) {
      continue;
    }

    bool is_unfeasible = true;
    for (int dim = 0; dim < 2; dim++) {
      const SchedulingConstraintHelper* helper = helpers[dim];
      for (int j = 0; j < 2; j++) {
        int b1 = box1;
        int b2 = box2;
        if (j == 1) {
          std::swap(b1, b2);
        }
        const auto [expr, ub_for_no_overlap] = EncodeDifferenceLowerThan(
            helper->Starts()[b1], helper->Ends()[b2], 0);
        if (linear2_bounds_->UpperBound(expr) >= ub_for_no_overlap) {
          is_unfeasible = false;
          break;
        }
      }
      if (!is_unfeasible) break;
    }
    if (!is_unfeasible) continue;

    // We have a mandatory overlap on both x and y! Explain and propagate.

    helper_.ClearReason();
    num_conflicts_++;

    for (int dim = 0; dim < 2; dim++) {
      SchedulingConstraintHelper* helper = helpers[dim];
      for (int j = 0; j < 2; j++) {
        int b1 = box1;
        int b2 = box2;
        if (j == 1) {
          std::swap(b1, b2);
        }
        const auto [expr, ub] = EncodeDifferenceLowerThan(
            helper->Starts()[b1], helper->Ends()[b2], -1);
        linear2_bounds_->AddReasonForUpperBoundLowerThan(
            expr, ub, helper_.x_helper().MutableLiteralReason(),
            helper_.x_helper().MutableIntegerReason());
      }
    }
    helper_.AddPresenceReason(box1);
    helper_.AddPresenceReason(box2);
    return helper_.ReportConflict();
  }
  return true;
}

int Precedences2DPropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_.WatchAllBoxes(id);
  linear2_watcher_->WatchAllLinearExpressions2(id);
  return id;
}

Precedences2DPropagator::~Precedences2DPropagator() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"Precedences2DPropagator/called", num_calls_});
  stats.push_back({"Precedences2DPropagator/conflicts", num_conflicts_});
  stats.push_back({"Precedences2DPropagator/pairs", non_trivial_pairs_.size()});

  shared_stats_->AddStats(stats);
}

}  // namespace sat
}  // namespace operations_research
