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

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

Precedences2DPropagator::Precedences2DPropagator(
    NoOverlap2DConstraintHelper* helper, Model* model)
    : helper_(*helper),
      linear2_bounds_(model->GetOrCreate<Linear2Bounds>()),
      linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      lin2_indices_(model->GetOrCreate<Linear2Indices>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  model->GetOrCreate<LinearPropagator>()->SetPushAffineUbForBinaryRelation();
}

void Precedences2DPropagator::UpdateVarLookups() {
  var_to_box_and_coeffs_.clear();
  for (int dim = 0; dim < 2; ++dim) {
    const SchedulingConstraintHelper& dim_helper =
        dim == 0 ? helper_.x_helper() : helper_.y_helper();
    for (int j = 0; j < 2; ++j) {
      const absl::Span<const AffineExpression> interval_points =
          j == 0 ? dim_helper.Starts() : dim_helper.Ends();
      for (int i = 0; i < helper_.NumBoxes(); ++i) {
        const IntegerVariable var = interval_points[i].var;
        if (var != kNoIntegerVariable) {
          var_to_box_and_coeffs_[PositiveVariable(var)].boxes[dim][j].push_back(
              i);
        }
      }
    }
  }
}

void Precedences2DPropagator::AddOrUpdateDataForPairOfBoxes(int box1,
                                                            int box2) {
  if (box1 > box2) std::swap(box1, box2);
  const auto [it, inserted] = non_trivial_pairs_index_.insert(
      {std::make_pair(box1, box2), static_cast<int>(pair_data_.size())});
  absl::InlinedVector<Literal, 4> presence_literals;
  for (int dim = 0; dim < 2; ++dim) {
    const SchedulingConstraintHelper& dim_helper =
        dim == 0 ? helper_.x_helper() : helper_.y_helper();
    for (const int box : {box1, box2}) {
      if (dim_helper.IsOptional(box)) {
        presence_literals.push_back(dim_helper.PresenceLiteral(box));
      }
    }
  }
  gtl::STLSortAndRemoveDuplicates(&presence_literals);
  if (inserted) {
    pair_data_.emplace_back(
        PairData{.pair_presence_literals = {presence_literals.begin(),
                                            presence_literals.end()},
                 .box1 = box1,
                 .box2 = box2});
  }
  PairData& pair_data = pair_data_[it->second];
  for (int dim = 0; dim < 2; ++dim) {
    const SchedulingConstraintHelper& dim_helper =
        dim == 0 ? helper_.x_helper() : helper_.y_helper();
    for (int j = 0; j < 2; ++j) {
      int b1 = j == 0 ? box1 : box2;
      int b2 = j == 0 ? box2 : box1;
      auto [start_minus_end_expr, start_minus_end_ub] =
          EncodeDifferenceLowerThan(dim_helper.Starts()[b1],
                                    dim_helper.Ends()[b2], 0);
      const LinearExpression2Index start_minus_end_index =
          lin2_indices_->GetIndex(start_minus_end_expr);
      pair_data.start_before_end[dim][j].ub = start_minus_end_ub;
      if (start_minus_end_index != kNoLinearExpression2Index) {
        pair_data.start_before_end[dim][j].linear2 = start_minus_end_index;
      } else {
        pair_data.start_before_end[dim][j].linear2 = start_minus_end_expr;
      }
    }
  }
}

void Precedences2DPropagator::CollectNewPairsOfBoxesWithNonTrivialDistance() {
  const absl::Span<const LinearExpression2> exprs =
      lin2_indices_->GetStoredLinear2Indices();
  if (exprs.size() == num_known_linear2_) {
    return;
  }
  VLOG(2) << "CollectPairsOfBoxesWithNonTrivialDistance called, num_exprs: "
          << exprs.size();
  for (; num_known_linear2_ < exprs.size(); ++num_known_linear2_) {
    const LinearExpression2& positive_expr = exprs[num_known_linear2_];
    LinearExpression2 negated_expr = positive_expr;
    negated_expr.Negate();
    for (const LinearExpression2& expr : {positive_expr, negated_expr}) {
      auto it1 = var_to_box_and_coeffs_.find(PositiveVariable(expr.vars[0]));
      auto it2 = var_to_box_and_coeffs_.find(PositiveVariable(expr.vars[1]));
      if (it1 == var_to_box_and_coeffs_.end()) {
        continue;
      }
      if (it2 == var_to_box_and_coeffs_.end()) {
        continue;
      }

      const VarUsage& usage1 = it1->second;
      const VarUsage& usage2 = it2->second;
      for (int dim = 0; dim < 2; ++dim) {
        for (const int box1 : usage1.boxes[dim][0 /* start */]) {
          for (const int box2 : usage2.boxes[dim][1 /* end */]) {
            if (box1 == box2) continue;
            AddOrUpdateDataForPairOfBoxes(box1, box2);
          }
        }
      }
    }
  }
}

IntegerValue Precedences2DPropagator::UpperBound(
    std::variant<LinearExpression2, LinearExpression2Index> linear2) const {
  if (std::holds_alternative<LinearExpression2Index>(linear2)) {
    return linear2_bounds_->UpperBound(
        std::get<LinearExpression2Index>(linear2));
  } else {
    return integer_trail_->UpperBound(std::get<LinearExpression2>(linear2));
  }
}

bool Precedences2DPropagator::Propagate() {
  if (!helper_.IsEnforced()) return true;
  if (last_helper_inprocessing_count_ != helper_.InProcessingCount()) {
    if (!helper_.SynchronizeAndSetDirection()) return false;
    last_helper_inprocessing_count_ = helper_.InProcessingCount();
    UpdateVarLookups();
    num_known_linear2_ = 0;
    non_trivial_pairs_index_.clear();
    pair_data_.clear();
  }
  CollectNewPairsOfBoxesWithNonTrivialDistance();

  num_calls_++;

  for (const PairData& pair_data : pair_data_) {
    if (!absl::c_all_of(pair_data.pair_presence_literals,
                        [this](const Literal& literal) {
                          return trail_->Assignment().LiteralIsTrue(literal);
                        })) {
      continue;
    }

    bool is_unfeasible = true;
    for (int dim = 0; dim < 2; dim++) {
      for (int j = 0; j < 2; j++) {
        const PairData::Condition& start_before_end =
            pair_data.start_before_end[dim][j];
        if (UpperBound(start_before_end.linear2) >= start_before_end.ub) {
          is_unfeasible = false;
          break;
        }
      }
      if (!is_unfeasible) break;
    }
    if (!is_unfeasible) continue;

    // We have a mandatory overlap on both x and y! Explain and propagate.
    if (!helper_.SynchronizeAndSetDirection()) return false;

    const int box1 = pair_data.box1;
    const int box2 = pair_data.box2;
    helper_.ResetReason();
    num_conflicts_++;

    helper_.x_helper().AddReasonForBeingBeforeAssumingNoOverlap(box1, box2);
    helper_.x_helper().AddReasonForBeingBeforeAssumingNoOverlap(box2, box1);
    helper_.y_helper().AddReasonForBeingBeforeAssumingNoOverlap(box1, box2);
    helper_.y_helper().AddReasonForBeingBeforeAssumingNoOverlap(box2, box1);

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
  stats.push_back({"Precedences2DPropagator/pairs", pair_data_.size()});

  shared_stats_->AddStats(stats);
}

}  // namespace sat
}  // namespace operations_research
