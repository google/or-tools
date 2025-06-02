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

namespace operations_research {
namespace sat {

Precedences2DPropagator::Precedences2DPropagator(
    NoOverlap2DConstraintHelper* helper, Model* model)
    : helper_(*helper),
      binary_relations_maps_(model->GetOrCreate<BinaryRelationsMaps>()),
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

  for (int dim = 0; dim < 2; ++dim) {
    const SchedulingConstraintHelper& dim_helper =
        dim == 0 ? helper_.x_helper() : helper_.y_helper();
    for (int j = 0; j < 2; ++j) {
      const absl::Span<const AffineExpression> interval_points =
          j == 0 ? dim_helper.Starts() : dim_helper.Ends();
      for (int i = 0; i < helper_.NumBoxes(); ++i) {
        if (interval_points[i].var != kNoIntegerVariable) {
          var_to_box_and_coeffs[PositiveVariable(interval_points[i].var)]
              .boxes[dim][j]
              .push_back(i);
        }
      }
    }
  }

  VLOG(2) << "CollectPairsOfBoxesWithNonTrivialDistance called, num_exprs: "
          << binary_relations_maps_->GetAllExpressionsWithAffineBounds().size();
  for (const LinearExpression2& expr :
       binary_relations_maps_->GetAllExpressionsWithAffineBounds()) {
    auto it1 = var_to_box_and_coeffs.find(PositiveVariable(expr.vars[0]));
    auto it2 = var_to_box_and_coeffs.find(PositiveVariable(expr.vars[1]));
    if (it1 == var_to_box_and_coeffs.end() ||
        it2 == var_to_box_and_coeffs.end()) {
      continue;
    }

    const VarUsage& usage1 = it1->second;
    const VarUsage& usage2 = it2->second;
    for (int dim = 0; dim < 2; ++dim) {
      const SchedulingConstraintHelper& dim_helper =
          dim == 0 ? helper_.x_helper() : helper_.y_helper();
      for (const int box1 : usage1.boxes[dim][0 /* start */]) {
        for (const int box2 : usage2.boxes[dim][1 /* end */]) {
          if (box1 == box2) continue;
          const AffineExpression& start = dim_helper.Starts()[box1];
          const AffineExpression& end = dim_helper.Ends()[box2];
          LinearExpression2 expr2;
          expr2.vars[0] = start.var;
          expr2.vars[1] = end.var;
          expr2.coeffs[0] = start.coeff;
          expr2.coeffs[1] = -end.coeff;
          expr2.SimpleCanonicalization();
          expr2.DivideByGcd();
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
      last_num_expressions_ !=
          binary_relations_maps_->NumExpressionsWithAffineBounds()) {
    last_helper_inprocessing_count_ = helper_.InProcessingCount();
    last_num_expressions_ =
        binary_relations_maps_->NumExpressionsWithAffineBounds();
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
        LinearExpression2 expr;
        expr.vars[0] = helper->Starts()[b1].var;
        expr.vars[1] = helper->Ends()[b2].var;
        expr.coeffs[0] = helper->Starts()[b1].coeff;
        expr.coeffs[1] = -helper->Ends()[b2].coeff;
        const IntegerValue ub_of_start_minus_end_value =
            binary_relations_maps_->UpperBound(expr) +
            helper->Starts()[b1].constant - helper->Ends()[b2].constant;
        if (ub_of_start_minus_end_value >= 0) {
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
        LinearExpression2 expr;
        expr.vars[0] = helper->Starts()[b1].var;
        expr.vars[1] = helper->Ends()[b2].var;
        expr.coeffs[0] = helper->Starts()[b1].coeff;
        expr.coeffs[1] = -helper->Ends()[b2].coeff;
        binary_relations_maps_->AddReasonForUpperBoundLowerThan(
            expr,
            -(helper->Starts()[b1].constant - helper->Ends()[b2].constant) - 1,
            helper_.x_helper().MutableLiteralReason(),
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
  binary_relations_maps_->WatchAllLinearExpressions2(id);
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
