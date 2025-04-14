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

#include "ortools/sat/diffn.h"

#include <stddef.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/numeric/bits.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/2d_mandatory_overlap_propagator.h"
#include "ortools/sat/2d_orthogonal_packing.h"
#include "ortools/sat/2d_try_edge_propagator.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cumulative_energy.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/no_overlap_2d_helper.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/timetable.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

IntegerVariable CreateVariableWithTightDomain(
    absl::Span<const AffineExpression> exprs, Model* model) {
  IntegerValue min = kMaxIntegerValue;
  IntegerValue max = kMinIntegerValue;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (const AffineExpression& e : exprs) {
    min = std::min(min, integer_trail->LevelZeroLowerBound(e));
    max = std::max(max, integer_trail->LevelZeroUpperBound(e));
  }
  return integer_trail->AddIntegerVariable(min, max);
}

IntegerVariable CreateVariableAtOrAboveMinOf(
    absl::Span<const AffineExpression> exprs, Model* model) {
  const IntegerVariable var = CreateVariableWithTightDomain(exprs, model);
  auto* constraint = new MinPropagator({exprs.begin(), exprs.end()}, var,
                                       model->GetOrCreate<IntegerTrail>());
  constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  model->TakeOwnership(constraint);

  return var;
}

IntegerVariable CreateVariableAtOrBelowMaxOf(
    absl::Span<const AffineExpression> exprs, Model* model) {
  std::vector<AffineExpression> negated_exprs(exprs.begin(), exprs.end());
  for (AffineExpression& affine : negated_exprs) affine = affine.Negated();

  const IntegerVariable var =
      CreateVariableWithTightDomain(negated_exprs, model);
  auto* constraint = new MinPropagator(std::move(negated_exprs), var,
                                       model->GetOrCreate<IntegerTrail>());
  constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  model->TakeOwnership(constraint);

  return NegationOf(var);
}

// Add a cumulative relaxation. That is, on one dimension, it does not enforce
// the rectangle aspect, allowing vertical slices to move freely.
void AddDiffnCumulativeRelationOnX(SchedulingConstraintHelper* x,
                                   SchedulingConstraintHelper* y,
                                   Model* model) {
  // Note that we only need one side!
  // We want something <= max_end - min_start
  //
  // TODO(user): Use conditional affine min/max !!
  bool all_optional = true;
  for (int i = 0; i < y->NumTasks(); ++i) {
    if (!y->IsOptional(i)) {
      all_optional = false;
      break;
    }
  }

  AffineExpression capacity;
  if (all_optional) {
    IntegerValue min_start = kMaxIntegerValue;
    IntegerValue max_end = kMinIntegerValue;
    for (int i = 0; i < y->NumTasks(); ++i) {
      min_start = std::min(min_start, y->LevelZeroStartMin(i));
      max_end = std::max(max_end, y->LevelZeroEndMax(i));
    }
    if (max_end < min_start) {
      return;
    }
    capacity = AffineExpression(CapSubI(max_end, min_start).value());
  } else {
    // This might not work if all task are optional, since the min could be
    // greater than the max.
    const IntegerVariable min_start_var =
        CreateVariableAtOrAboveMinOf(y->Starts(), model);
    const IntegerVariable max_end_var =
        CreateVariableAtOrBelowMaxOf(y->Ends(), model);

    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    if (integer_trail->UpperBound(max_end_var) <
        integer_trail->LowerBound(min_start_var)) {
      // Trivial infeasible case, will be handled by the linear constraint
      // from the interval.
      return;
    }
    // (max_end - min_start) >= capacity.
    capacity = model->Add(NewIntegerVariable(
        0, CapSub(integer_trail->UpperBound(max_end_var).value(),
                  integer_trail->LowerBound(min_start_var).value())));
    const std::vector<int64_t> coeffs = {-capacity.coeff.value(), -1, 1};
    model->Add(
        WeightedSumGreaterOrEqual({capacity.var, min_start_var, max_end_var},
                                  coeffs, capacity.constant.value()));
  }

  SchedulingDemandHelper* demands =
      model->GetOrCreate<IntervalsRepository>()->GetOrCreateDemandHelper(
          x, y->Sizes());

  // Propagator responsible for applying Timetabling filtering rule. It
  // increases the minimum of the start variables, decrease the maximum of the
  // end variables, and increase the minimum of the capacity variable.
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (params.use_timetabling_in_no_overlap_2d()) {
    TimeTablingPerTask* time_tabling =
        new TimeTablingPerTask(capacity, x, demands, model);
    time_tabling->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(time_tabling);
  }

  // Propagator responsible for applying the Overload Checking filtering rule.
  // It increases the minimum of the capacity variable.
  if (params.use_energetic_reasoning_in_no_overlap_2d()) {
    AddCumulativeOverloadChecker(capacity, x, demands, model);
  }
}

}  // namespace

void AddNonOverlappingRectangles(const std::vector<IntervalVariable>& x,
                                 const std::vector<IntervalVariable>& y,
                                 Model* model) {
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  NoOverlap2DConstraintHelper* helper_2d =
      repository->GetOrCreate2DHelper(x, y);

  GenericLiteralWatcher* const watcher =
      model->GetOrCreate<GenericLiteralWatcher>();

  CreateAndRegisterMandatoryOverlapPropagator(helper_2d, model, watcher, 3);

  NonOverlappingRectanglesDisjunctivePropagator* constraint =
      new NonOverlappingRectanglesDisjunctivePropagator(helper_2d, model);
  constraint->Register(/*fast_priority=*/3, /*slow_priority=*/4);
  model->TakeOwnership(constraint);

  RectanglePairwisePropagator* pairwise_propagator =
      new RectanglePairwisePropagator(helper_2d, model);
  watcher->SetPropagatorPriority(pairwise_propagator->RegisterWith(watcher), 4);
  model->TakeOwnership(pairwise_propagator);

  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  const bool add_cumulative_relaxation =
      params.use_timetabling_in_no_overlap_2d() ||
      params.use_energetic_reasoning_in_no_overlap_2d();

  if (add_cumulative_relaxation) {
    SchedulingConstraintHelper* x_helper = repository->GetOrCreateHelper(x);
    SchedulingConstraintHelper* y_helper = repository->GetOrCreateHelper(y);

    // We must first check if the cumulative relaxation is possible.
    bool some_boxes_are_only_optional_on_x = false;
    bool some_boxes_are_only_optional_on_y = false;
    for (int i = 0; i < x.size(); ++i) {
      if (x_helper->IsOptional(i) && y_helper->IsOptional(i) &&
          x_helper->PresenceLiteral(i) != y_helper->PresenceLiteral(i)) {
        // Abort as the task would be conditioned by two literals.
        return;
      }
      if (x_helper->IsOptional(i) && !y_helper->IsOptional(i)) {
        // We cannot use x_size as the demand of the cumulative based on
        // the y_intervals.
        some_boxes_are_only_optional_on_x = true;
      }
      if (y_helper->IsOptional(i) && !x_helper->IsOptional(i)) {
        // We cannot use y_size as the demand of the cumulative based on
        // the y_intervals.
        some_boxes_are_only_optional_on_y = true;
      }
    }
    if (!some_boxes_are_only_optional_on_y) {
      AddDiffnCumulativeRelationOnX(x_helper, y_helper, model);
    }
    if (!some_boxes_are_only_optional_on_x) {
      AddDiffnCumulativeRelationOnX(y_helper, x_helper, model);
    }
  }

  if (params.use_area_energetic_reasoning_in_no_overlap_2d()) {
    NonOverlappingRectanglesEnergyPropagator* energy_constraint =
        new NonOverlappingRectanglesEnergyPropagator(helper_2d, model);
    GenericLiteralWatcher* const watcher =
        model->GetOrCreate<GenericLiteralWatcher>();
    watcher->SetPropagatorPriority(energy_constraint->RegisterWith(watcher), 5);
    model->TakeOwnership(energy_constraint);
  }

  if (params.use_try_edge_reasoning_in_no_overlap_2d()) {
    CreateAndRegisterTryEdgePropagator(helper_2d, model, watcher, 5);
  }

  // Create all 2D "precedence" Booleans.
  //
  // TODO(user): For now we only deal with mandatory boxes.
  //
  // TODO(user): Like we do for 1D, one way to scale this is to only create such
  // Boolean dynamically as we need to take a decision, and the previously
  // created one are all assigned. It is a bit trickier though because of
  // the extra constraints between these Booleans. Maybe one easy step is to
  // create all 4 Booleans for a given pair of boxes at once.
  //
  // TODO(user): Tricky, it would be better to use the helper_2d instead of the
  // general repository, however, as we add constraints, this propagates which
  // might swap/change the underlying x_helper and y_helper...
  const int num_boxes = x.size();
  if (num_boxes < params.no_overlap_2d_boolean_relations_limit()) {
    auto* implications = model->GetOrCreate<BinaryImplicationGraph>();
    auto* sat_solver = model->GetOrCreate<SatSolver>();
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    DCHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);

    // Creates and returns the Boolean equivalent to a <= b.
    const auto f = [repository, integer_trail, encoder](
                       const AffineExpression& a, const AffineExpression& b) {
      if (a.var == b.var && a.coeff == b.coeff) {
        return (a.constant <= b.constant) ? encoder->GetTrueLiteral()
                                          : encoder->GetFalseLiteral();
      }
      if (integer_trail->UpperBound(a) <= integer_trail->LowerBound(b)) {
        return encoder->GetTrueLiteral();
      }
      if (integer_trail->LowerBound(a) > integer_trail->UpperBound(b)) {
        return encoder->GetFalseLiteral();
      }
      repository->CreatePrecedenceLiteral(a, b);
      const LiteralIndex index = repository->GetPrecedenceLiteral(a, b);
      CHECK(index != kNoLiteralIndex);
      return Literal(index);
    };

    for (int i = 0; i < num_boxes; ++i) {
      if (repository->IsAbsent(x[i])) continue;
      if (repository->IsAbsent(y[i])) continue;
      for (int j = i + 1; j < num_boxes; ++j) {
        if (repository->IsAbsent(x[j])) continue;
        if (repository->IsAbsent(y[j])) continue;

        // At most one of these two x options is true.
        const Literal x_ij = f(repository->End(x[i]), repository->Start(x[j]));
        const Literal x_ji = f(repository->End(x[j]), repository->Start(x[i]));
        if ((integer_trail->LowerBound(repository->Size(x[i])) > 0 ||
             integer_trail->LowerBound(repository->Size(x[j])) > 0) &&
            !implications->AddAtMostOne({x_ij, x_ji})) {
          sat_solver->NotifyThatModelIsUnsat();
          return;
        }

        // At most one of these two y options is true.
        const Literal y_ij = f(repository->End(y[i]), repository->Start(y[j]));
        const Literal y_ji = f(repository->End(y[j]), repository->Start(y[i]));
        if ((integer_trail->LowerBound(repository->Size(y[i])) > 0 ||
             integer_trail->LowerBound(repository->Size(y[j])) > 0) &&
            !implications->AddAtMostOne({y_ij, y_ji})) {
          sat_solver->NotifyThatModelIsUnsat();
          return;
        }

        // At least one of the 4 options is true.
        std::vector<Literal> clause = {x_ij, x_ji, y_ij, y_ji};
        if (repository->IsOptional(x[i])) {
          clause.push_back(repository->PresenceLiteral(x[i]).Negated());
        }
        if (repository->IsOptional(y[i])) {
          clause.push_back(repository->PresenceLiteral(y[i]).Negated());
        }
        if (repository->IsOptional(x[j])) {
          clause.push_back(repository->PresenceLiteral(x[j]).Negated());
        }
        if (repository->IsOptional(y[j])) {
          clause.push_back(repository->PresenceLiteral(y[j]).Negated());
        }
        gtl::STLSortAndRemoveDuplicates(&clause);
        if (!sat_solver->AddProblemClause(clause)) {
          return;
        }
      }
    }
  }
}

#define RETURN_IF_FALSE(f) \
  if (!(f)) return false;

NonOverlappingRectanglesEnergyPropagator::
    ~NonOverlappingRectanglesEnergyPropagator() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"NonOverlappingRectanglesEnergyPropagator/called", num_calls_});
  stats.push_back(
      {"NonOverlappingRectanglesEnergyPropagator/conflicts", num_conflicts_});
  stats.push_back(
      {"NonOverlappingRectanglesEnergyPropagator/conflicts_two_boxes",
       num_conflicts_two_boxes_});
  stats.push_back({"NonOverlappingRectanglesEnergyPropagator/refined",
                   num_refined_conflicts_});
  stats.push_back(
      {"NonOverlappingRectanglesEnergyPropagator/conflicts_with_slack",
       num_conflicts_with_slack_});

  shared_stats_->AddStats(stats);
}

bool NonOverlappingRectanglesEnergyPropagator::Propagate() {
  // TODO(user): double-check/revisit the algo for box of variable sizes.
  const int num_boxes = helper_.NumBoxes();
  if (!helper_.SynchronizeAndSetDirection()) return false;

  Rectangle bounding_box = {.x_min = std::numeric_limits<IntegerValue>::max(),
                            .x_max = std::numeric_limits<IntegerValue>::min(),
                            .y_min = std::numeric_limits<IntegerValue>::max(),
                            .y_max = std::numeric_limits<IntegerValue>::min()};
  std::vector<RectangleInRange> active_box_ranges;
  active_box_ranges.reserve(num_boxes);
  for (int box = 0; box < num_boxes; ++box) {
    if (!helper_.IsPresent(box)) continue;
    RectangleInRange rec = helper_.GetItemRangeForSizeMin(box);
    if (rec.x_size == 0 || rec.y_size == 0) continue;
    bounding_box.GrowToInclude(rec.bounding_area);
    active_box_ranges.push_back(std::move(rec));
  }

  if (active_box_ranges.size() < 2) {
    return true;
  }

  // Our algo is quadratic, so we don't want to run it on really large problems.
  if (active_box_ranges.size() > 1000) {
    return true;
  }

  if (AtMinOrMaxInt64I(
          CapProdI(CapProdI(bounding_box.SizeX(), bounding_box.SizeY()),
                   active_box_ranges.size()))) {
    // Avoid integer overflows if the area of the boxes get comparable with
    // INT64_MAX.
    return true;
  }

  num_calls_++;

  std::optional<Conflict> best_conflict =
      FindConflict(std::move(active_box_ranges));
  if (!best_conflict.has_value()) {
    return true;
  }
  num_conflicts_++;

  // We found a conflict, so we can afford to run the propagator again to
  // search for a best explanation. This is specially the case since we only
  // want to re-run it over the items that participate in the conflict, so it is
  // a much smaller problem.
  IntegerValue best_explanation_size =
      best_conflict->opp_result.GetItemsParticipatingOnConflict().size();
  bool refined = false;
  while (true) {
    std::vector<RectangleInRange> items_participating_in_conflict;
    items_participating_in_conflict.reserve(
        best_conflict->items_for_opp.size());
    for (const auto& item :
         best_conflict->opp_result.GetItemsParticipatingOnConflict()) {
      items_participating_in_conflict.push_back(
          best_conflict->items_for_opp[item.index]);
    }
    std::optional<Conflict> conflict =
        FindConflict(items_participating_in_conflict);
    if (!conflict.has_value()) break;
    // We prefer an explanation with the least number of boxes.
    if (conflict->opp_result.GetItemsParticipatingOnConflict().size() >=
        best_explanation_size) {
      // The new explanation isn't better than the old one. Stop trying.
      break;
    }
    best_explanation_size =
        conflict->opp_result.GetItemsParticipatingOnConflict().size();
    best_conflict = std::move(conflict);
    refined = true;
  }

  num_refined_conflicts_ += refined;
  const std::vector<RectangleInRange> generalized_explanation =
      GeneralizeExplanation(*best_conflict);
  if (best_explanation_size == 2) {
    num_conflicts_two_boxes_++;
  }
  helper_.ReportConflictFromInfeasibleBoxRanges(generalized_explanation);
  return false;
}

std::optional<NonOverlappingRectanglesEnergyPropagator::Conflict>
NonOverlappingRectanglesEnergyPropagator::FindConflict(
    std::vector<RectangleInRange> active_box_ranges) {
  const auto rectangles_with_too_much_energy =
      FindRectanglesWithEnergyConflictMC(active_box_ranges, *random_, 1.0, 0.8);

  if (rectangles_with_too_much_energy.conflicts.empty() &&
      rectangles_with_too_much_energy.candidates.empty()) {
    return std::nullopt;
  }

  Conflict best_conflict;

  // Sample 10 rectangles (at least five among the ones for which we already
  // detected an energy overflow), extract an orthogonal packing subproblem for
  // each and look for conflict. Sampling avoids making this heuristic too
  // costly.
  constexpr int kSampleSize = 10;
  absl::InlinedVector<Rectangle, kSampleSize> sampled_rectangles;
  std::sample(rectangles_with_too_much_energy.conflicts.begin(),
              rectangles_with_too_much_energy.conflicts.end(),
              std::back_inserter(sampled_rectangles), 5, *random_);
  std::sample(rectangles_with_too_much_energy.candidates.begin(),
              rectangles_with_too_much_energy.candidates.end(),
              std::back_inserter(sampled_rectangles),
              kSampleSize - sampled_rectangles.size(), *random_);
  std::sort(sampled_rectangles.begin(), sampled_rectangles.end(),
            [](const Rectangle& a, const Rectangle& b) {
              const bool larger = std::make_pair(a.SizeX(), a.SizeY()) >
                                  std::make_pair(b.SizeX(), b.SizeY());
              // Double-check the invariant from
              // FindRectanglesWithEnergyConflictMC() that given two returned
              // rectangles there is one that is fully inside the other.
              if (larger) {
                // Rectangle b is fully contained inside a
                DCHECK(a.x_min <= b.x_min && a.x_max >= b.x_max &&
                       a.y_min <= b.y_min && a.y_max >= b.y_max);
              } else {
                // Rectangle a is fully contained inside b
                DCHECK(a.x_min >= b.x_min && a.x_max <= b.x_max &&
                       a.y_min >= b.y_min && a.y_max <= b.y_max);
              }
              return larger;
            });
  std::vector<IntegerValue> sizes_x, sizes_y;
  sizes_x.reserve(active_box_ranges.size());
  sizes_y.reserve(active_box_ranges.size());
  std::vector<RectangleInRange> filtered_items;
  filtered_items.reserve(active_box_ranges.size());
  for (const Rectangle& r : sampled_rectangles) {
    sizes_x.clear();
    sizes_y.clear();
    filtered_items.clear();
    for (int i = 0; i < active_box_ranges.size(); ++i) {
      const RectangleInRange& box = active_box_ranges[i];
      const Rectangle intersection = box.GetMinimumIntersection(r);
      if (intersection.SizeX() > 0 && intersection.SizeY() > 0) {
        sizes_x.push_back(intersection.SizeX());
        sizes_y.push_back(intersection.SizeY());
        filtered_items.push_back(box);
      }
    }
    // This check the feasibility of a related orthogonal packing problem where
    // our rectangle is the bounding box, and we need to fit inside it a set of
    // items corresponding to the minimum intersection of the original items
    // with this bounding box.
    const auto opp_result = orthogonal_packing_checker_.TestFeasibility(
        sizes_x, sizes_y, {r.SizeX(), r.SizeY()},
        OrthogonalPackingOptions{
            .use_pairwise = true,
            .use_dff_f0 = true,
            .use_dff_f2 = true,
            .brute_force_threshold = 7,
            .dff2_max_number_of_parameters_to_check = 100});
    if (opp_result.GetResult() == OrthogonalPackingResult::Status::INFEASIBLE &&
        (best_conflict.opp_result.GetResult() !=
             OrthogonalPackingResult::Status::INFEASIBLE ||
         best_conflict.opp_result.GetItemsParticipatingOnConflict().size() >
             opp_result.GetItemsParticipatingOnConflict().size())) {
      best_conflict.items_for_opp = filtered_items;
      best_conflict.opp_result = opp_result;
      best_conflict.rectangle_with_too_much_energy = r;
    }
    // Use the fact that our rectangles are ordered in shrinking order to remove
    // all items that will never contribute again.
    filtered_items.swap(active_box_ranges);
  }
  if (best_conflict.opp_result.GetResult() ==
      OrthogonalPackingResult::Status::INFEASIBLE) {
    return best_conflict;
  } else {
    return std::nullopt;
  }
}

std::vector<RectangleInRange>
NonOverlappingRectanglesEnergyPropagator::GeneralizeExplanation(
    const Conflict& conflict) {
  const Rectangle& rectangle = conflict.rectangle_with_too_much_energy;
  OrthogonalPackingResult relaxed_result = conflict.opp_result;

  // Use the potential slack to have a stronger reason.
  int used_slack_count = 0;
  const auto& items = relaxed_result.GetItemsParticipatingOnConflict();
  for (int i = 0; i < items.size(); ++i) {
    if (!relaxed_result.HasSlack()) {
      break;
    }
    const RectangleInRange& range = conflict.items_for_opp[items[i].index];
    const RectangleInRange item_in_zero_level_range = {
        .bounding_area =
            {.x_min = helper_.x_helper().LevelZeroStartMin(range.box_index),
             .x_max = helper_.x_helper().LevelZeroStartMax(range.box_index) +
                      range.x_size,
             .y_min = helper_.y_helper().LevelZeroStartMin(range.box_index),
             .y_max = helper_.y_helper().LevelZeroStartMax(range.box_index) +
                      range.y_size},
        .x_size = range.x_size,
        .y_size = range.y_size};
    // There is no point trying to intersect less the item with the rectangle
    // than it would on zero-level. So do not waste the slack with it.
    const Rectangle max_overlap =
        item_in_zero_level_range.GetMinimumIntersection(rectangle);
    used_slack_count += relaxed_result.TryUseSlackToReduceItemSize(
        i, OrthogonalPackingResult::Coord::kCoordX, max_overlap.SizeX());
    used_slack_count += relaxed_result.TryUseSlackToReduceItemSize(
        i, OrthogonalPackingResult::Coord::kCoordY, max_overlap.SizeY());
  }
  num_conflicts_with_slack_ += (used_slack_count > 0);
  VLOG_EVERY_N_SEC(2, 3)
      << "Found a conflict on the OPP sub-problem of rectangle: " << rectangle
      << " using "
      << conflict.opp_result.GetItemsParticipatingOnConflict().size() << "/"
      << conflict.items_for_opp.size() << " items.";

  std::vector<RectangleInRange> ranges_for_explanation;
  ranges_for_explanation.reserve(conflict.items_for_opp.size());
  std::vector<OrthogonalPackingResult::Item> sorted_items =
      relaxed_result.GetItemsParticipatingOnConflict();
  // TODO(user) understand why sorting high-impact items first improves the
  // benchmarks
  std::sort(sorted_items.begin(), sorted_items.end(),
            [](const OrthogonalPackingResult::Item& a,
               const OrthogonalPackingResult::Item& b) {
              return a.size_x * a.size_y > b.size_x * b.size_y;
            });
  for (const auto& item : sorted_items) {
    const RectangleInRange& range = conflict.items_for_opp[item.index];
    ranges_for_explanation.push_back(
        RectangleInRange::BiggestWithMinIntersection(rectangle, range,
                                                     item.size_x, item.size_y));
  }
  return ranges_for_explanation;
}

int NonOverlappingRectanglesEnergyPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_.WatchAllBoxes(id);
  return id;
}

namespace {

// We want for different propagation to reuse as much as possible the same
// line. The idea behind this is to compute the 'canonical' line to use
// when explaining that boxes overlap on the 'y_dim' dimension. We compute
// the multiple of the biggest power of two that is common to all boxes.
IntegerValue FindCanonicalValue(IntegerValue lb, IntegerValue ub) {
  if (lb == ub) return lb;
  if (lb <= 0 && ub > 0) return IntegerValue(0);
  if (lb < 0 && ub <= 0) {
    return -FindCanonicalValue(-ub, -lb);
  }

  int64_t mask = 0;
  IntegerValue candidate = ub;
  for (int o = 0; o < 62; ++o) {
    mask = 2 * mask + 1;
    const IntegerValue masked_ub(ub.value() & ~mask);
    if (masked_ub >= lb) {
      candidate = masked_ub;
    } else {
      break;
    }
  }
  return candidate;
}

void SplitDisjointBoxes(const SchedulingConstraintHelper& x,
                        absl::Span<const int> boxes,
                        std::vector<absl::Span<const int>>* result) {
  result->clear();
  DCHECK(std::is_sorted(boxes.begin(), boxes.end(), [&x](int a, int b) {
    return x.ShiftedStartMin(a) < x.ShiftedStartMin(b);
  }));
  int current_start = 0;
  std::size_t current_length = 1;
  IntegerValue current_max_end = x.EndMax(boxes[0]);

  for (int b = 1; b < boxes.size(); ++b) {
    const int box = boxes[b];
    if (x.ShiftedStartMin(box) < current_max_end) {
      // Merge.
      current_length++;
      current_max_end = std::max(current_max_end, x.EndMax(box));
    } else {
      if (current_length > 1) {  // Ignore lists of size 1.
        result->emplace_back(&boxes[current_start], current_length);
      }
      current_start = b;
      current_length = 1;
      current_max_end = x.EndMax(box);
    }
  }

  // Push last span.
  if (current_length > 1) {
    result->emplace_back(&boxes[current_start], current_length);
  }
}

}  // namespace

// Note that x_ and y_ must be initialized with enough intervals when passed
// to the disjunctive propagators.
NonOverlappingRectanglesDisjunctivePropagator::
    NonOverlappingRectanglesDisjunctivePropagator(
        NoOverlap2DConstraintHelper* helper, Model* model)
    : helper_(helper),
      x_(helper->NumBoxes(), model),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      overload_checker_(&x_),
      forward_detectable_precedences_(true, &x_),
      backward_detectable_precedences_(false, &x_),
      forward_not_last_(true, &x_),
      backward_not_last_(false, &x_),
      forward_edge_finding_(true, &x_),
      backward_edge_finding_(false, &x_),
      disjunctive_with_two_items_(&x_) {}

NonOverlappingRectanglesDisjunctivePropagator::
    ~NonOverlappingRectanglesDisjunctivePropagator() = default;

void NonOverlappingRectanglesDisjunctivePropagator::Register(
    int fast_priority, int slow_priority) {
  fast_id_ = watcher_->Register(this);
  watcher_->SetPropagatorPriority(fast_id_, fast_priority);
  helper_->WatchAllBoxes(fast_id_);

  // This propagator is the one making sure our propagation is complete, so
  // we do need to make sure it is called again if it modified some bounds.
  watcher_->NotifyThatPropagatorMayNotReachFixedPointInOnePass(fast_id_);

  const int slow_id = watcher_->Register(this);
  watcher_->SetPropagatorPriority(slow_id, slow_priority);
  helper_->WatchAllBoxes(slow_id);
}

bool NonOverlappingRectanglesDisjunctivePropagator::
    FindBoxesThatMustOverlapAHorizontalLineAndPropagate(
        bool fast_propagation, absl::Span<const int> requested_boxes) {
  // When they are many fixed box that we know do not overlap, we compute
  // the bounding box of the others, and we can exclude all boxes outside this
  // region. This can help, especially for some LNS neighborhood.
  int num_fixed = 0;
  int num_others = 0;
  Rectangle other_bounding_box;

  // push_back() can be slow as it might not be inlined, so we manage directly
  // our "boxes" in boxes_data[0 .. num_boxes], with a memory that is always big
  // enough.
  indexed_boxes_.resize(helper_->NumBoxes());
  int num_boxes = 0;
  IndexedInterval* boxes_data = indexed_boxes_.data();

  SchedulingConstraintHelper* x = &helper_->x_helper();
  SchedulingConstraintHelper* y = &helper_->y_helper();

  // Optimization: we only initialize the set if we don't have all task here.
  absl::flat_hash_set<int> requested_boxes_set;
  const bool not_all_boxes = requested_boxes.size() != helper_->NumBoxes();
  if (not_all_boxes) {
    requested_boxes_set = {requested_boxes.begin(), requested_boxes.end()};
  }

  // Compute relevant boxes, the one with a mandatory part on y. Because we will
  // need to sort it this way, we consider them by increasing start max.
  const auto temp = y->TaskByIncreasingNegatedStartMax();
  auto fixed_boxes = already_checked_fixed_boxes_.view();
  for (int i = temp.size(); --i >= 0;) {
    const int box = temp[i].task_index;
    if (not_all_boxes && !requested_boxes_set.contains(box)) continue;

    // By definition, fixed boxes are always present.
    // Doing this check optimize a bit the case where we have many fixed boxes.
    if (!fixed_boxes[box]) {
      // Ignore absent boxes.
      if (x->IsAbsent(box) || y->IsAbsent(box)) continue;

      // Ignore boxes where the relevant presence literal is only on the y
      // dimension, or if both intervals are optionals with different literals.
      if (x->IsPresent(box) && !y->IsPresent(box)) continue;
      if (!x->IsPresent(box) && !y->IsPresent(box) &&
          x->PresenceLiteral(box) != y->PresenceLiteral(box)) {
        continue;
      }
    }

    // Only consider box with a mandatory part on y.
    const IntegerValue start_max = -temp[i].time;
    const IntegerValue end_min = y->EndMin(box);
    if (start_max < end_min) {
      boxes_data[num_boxes++] = {box, start_max, end_min};

      // Optim: If many rectangle are fixed and known not to overlap, we might
      // filter them out.
      if (fixed_boxes[box]) {
        ++num_fixed;
      } else {
        if (helper_->IsFixed(box)) {
          // We will "check it" below, so it will be checked next time.
          fixed_boxes.Set(box);
        }

        const Rectangle r = {x->StartMin(box), x->EndMax(box), start_max,
                             end_min};
        if (num_others == 0) {
          other_bounding_box = r;
        } else {
          other_bounding_box.GrowToInclude(r);
        }
        ++num_others;
      }
    }
  }

  // We remove from boxes_data all the fixed and checked box outside the
  // other_bounding_box.
  //
  // TODO(user): We could be smarter here, if we have just a few non-fixed
  // boxes, likely their mandatory y-part do not span the whole horizon, so
  // we could remove any fixed boxes outside these "stripes".
  if (num_others == 0) return true;
  if (num_fixed > 0) {
    int new_size = 0;
    for (int i = 0; i < num_boxes; ++i) {
      const IndexedInterval& interval = boxes_data[i];
      const int box = interval.index;
      const Rectangle r = {x->StartMin(box), x->EndMax(box), interval.start,
                           interval.end};
      if (other_bounding_box.IsDisjoint(r)) continue;
      boxes_data[new_size++] = interval;
    }
    num_boxes = new_size;
  }

  // Less than 2 boxes, no propagation.
  const auto boxes = absl::MakeSpan(boxes_data, num_boxes);
  if (boxes.size() < 2) return true;

  // Optim: Abort if all rectangle can be fixed to their mandatory y +
  // minimum x position without any overlap.
  //
  // This is guaranteed to be O(N log N) whereas the algo below is O(N ^ 2).
  //
  // TODO(user): We might still propagate the x end in this setting, but the
  // current code will just abort below in SplitDisjointBoxes() anyway.
  {
    rectangles_.clear();
    rectangles_.reserve(boxes.size());
    for (const auto [box, y_mandatory_start, y_mandatory_end] : boxes) {
      // Note that we invert the x/y position here in order to be already
      // sorted for FindOneIntersectionIfPresent()
      rectangles_.push_back(
          {/*x_min=*/y_mandatory_start, /*x_max=*/y_mandatory_end,
           /*y_min=*/x->StartMin(box), /*y_max=*/x->EndMin(box)});
    }
    const auto opt_pair = FindOneIntersectionIfPresent(rectangles_);
    {
      const size_t n = rectangles_.size();
      time_limit_->AdvanceDeterministicTime(
          static_cast<double>(n) * static_cast<double>(absl::bit_width(n)) *
          1e-8);
    }
    if (opt_pair == std::nullopt) {
      return true;
    } else {
      // TODO(user): Test if we have a conflict here.
    }
  }

  order_.assign(x->NumTasks(), 0);
  {
    int i = 0;
    for (const auto [t, _lit, _time] : x->TaskByIncreasingShiftedStartMin()) {
      order_[t] = i++;
    }
  }
  ConstructOverlappingSets(boxes, &events_overlapping_boxes_, order_);

  // Split lists of boxes into disjoint set of boxes (w.r.t. overlap).
  boxes_to_propagate_.clear();
  reduced_overlapping_boxes_.clear();
  int work_done = boxes.size();
  for (int i = 0; i < events_overlapping_boxes_.size(); ++i) {
    work_done += events_overlapping_boxes_[i].size();
    SplitDisjointBoxes(*x, events_overlapping_boxes_[i], &disjoint_boxes_);
    for (const absl::Span<const int> sub_boxes : disjoint_boxes_) {
      // Boxes are sorted in a stable manner in the Split method.
      // Note that we do not use reduced_overlapping_boxes_ directly so that
      // the order of iteration is deterministic.
      const auto& insertion = reduced_overlapping_boxes_.insert(sub_boxes);
      if (insertion.second) boxes_to_propagate_.push_back(sub_boxes);
    }
  }

  // TODO(user): This is a poor dtime, but we want it not to be zero here.
  time_limit_->AdvanceDeterministicTime(static_cast<double>(work_done) * 1e-8);

  // And finally propagate.
  //
  // TODO(user): Sorting of boxes seems influential on the performance.
  // Test.
  for (const absl::Span<const int> boxes : boxes_to_propagate_) {
    // The case of two boxes should be taken care of during "fast"
    // propagation, so we can skip it here.
    if (!fast_propagation && boxes.size() <= 2) continue;

    x_.ClearOtherHelper();
    if (!x_.ResetFromSubset(*x, boxes)) return false;

    // Collect the common overlapping coordinates of all boxes.
    IntegerValue lb(std::numeric_limits<int64_t>::min());
    IntegerValue ub(std::numeric_limits<int64_t>::max());
    for (const int b : boxes) {
      lb = std::max(lb, y->StartMax(b));
      ub = std::min(ub, y->EndMin(b) - 1);
    }
    CHECK_LE(lb, ub);

    // We want for different propagation to reuse as much as possible the same
    // line. The idea behind this is to compute the 'canonical' line to use
    // when explaining that boxes overlap on the 'y_dim' dimension. We compute
    // the multiple of the biggest power of two that is common to all boxes.
    //
    // TODO(user): We should scan the integer trail to find the oldest
    // non-empty common interval. Then we can pick the canonical value within
    // it.
    const IntegerValue line_to_use_for_reason = FindCanonicalValue(lb, ub);

    // Setup x_dim for propagation.
    x_.SetOtherHelper(y, boxes, line_to_use_for_reason);

    if (fast_propagation) {
      if (x_.NumTasks() == 2) {
        // In that case, we can use simpler algorithms.
        // Note that this case happens frequently (~30% of all calls to this
        // method according to our tests).
        RETURN_IF_FALSE(disjunctive_with_two_items_.Propagate());
      } else {
        RETURN_IF_FALSE(overload_checker_.Propagate());
        RETURN_IF_FALSE(forward_detectable_precedences_.Propagate());
        RETURN_IF_FALSE(backward_detectable_precedences_.Propagate());
      }
    } else {
      DCHECK_GT(x_.NumTasks(), 2);
      RETURN_IF_FALSE(forward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_not_last_.Propagate());
      RETURN_IF_FALSE(backward_edge_finding_.Propagate());
      RETURN_IF_FALSE(forward_edge_finding_.Propagate());
    }
  }

  return true;
}

// Note that we optimized this function for two main use cases:
// - smallish problem where we don't have more than 100 boxes.
// - large problem with many 1000s boxes, but with only a small subset that is
//   not fixed (mainly coming from LNS).
bool NonOverlappingRectanglesDisjunctivePropagator::Propagate() {
  if (!helper_->SynchronizeAndSetDirection(true, true, false)) return false;

  // If we are "diving" we maintain the set of fixed boxes for which we know
  // that they are not overlapping.
  const bool backtrack_since_last_call = !rev_is_in_dive_;
  watcher_->SetUntilNextBacktrack(&rev_is_in_dive_);
  if (backtrack_since_last_call ||
      last_helper_inprocessing_count_ != helper_->InProcessingCount()) {
    last_helper_inprocessing_count_ = helper_->InProcessingCount();
    const int num_tasks = helper_->NumBoxes();
    already_checked_fixed_boxes_.ClearAndResize(num_tasks);
  }

  // Note that the code assumes that this was registered twice in fast and slow
  // mode. So we will not redo some propagation in slow mode that was already
  // done by the fast mode.
  const bool fast_propagation = watcher_->GetCurrentId() == fast_id_;
  for (const auto subset : helper_->connected_components().AsVectorOfSpan()) {
    if (!FindBoxesThatMustOverlapAHorizontalLineAndPropagate(fast_propagation,
                                                             subset)) {
      return false;
    }
  }
  // We can actually swap dimensions to propagate vertically.
  if (!helper_->SynchronizeAndSetDirection(true, true, true)) return false;

  for (const auto subset : helper_->connected_components().AsVectorOfSpan()) {
    if (!FindBoxesThatMustOverlapAHorizontalLineAndPropagate(fast_propagation,
                                                             subset)) {
      return false;
    }
  }

  return true;
}

int RectanglePairwisePropagator::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllBoxes(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

RectanglePairwisePropagator::~RectanglePairwisePropagator() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"RectanglePairwisePropagator/called", num_calls_});
  stats.push_back({"RectanglePairwisePropagator/pairwise_conflicts",
                   num_pairwise_conflicts_});
  stats.push_back({"RectanglePairwisePropagator/pairwise_propagations",
                   num_pairwise_propagations_});

  shared_stats_->AddStats(stats);
}

bool RectanglePairwisePropagator::Propagate() {
  if (!helper_->SynchronizeAndSetDirection()) return false;

  num_calls_++;
  std::vector<PairwiseRestriction> restrictions;

  for (int component_index = 0;
       component_index < helper_->connected_components().size();
       ++component_index) {
    horizontal_zero_area_boxes_.clear();
    vertical_zero_area_boxes_.clear();
    point_zero_area_boxes_.clear();
    fixed_non_zero_area_boxes_.clear();
    non_fixed_non_zero_area_boxes_.clear();
    for (int b : helper_->connected_components()[component_index]) {
      if (!helper_->IsPresent(b)) continue;
      const auto [x_size_max, y_size_max] = helper_->GetBoxSizesMax(b);
      ItemWithVariableSize box = helper_->GetItemWithVariableSize(b);
      if (x_size_max == 0) {
        if (y_size_max == 0) {
          point_zero_area_boxes_.push_back(std::move(box));
        } else {
          vertical_zero_area_boxes_.push_back(std::move(box));
        }
      } else if (y_size_max == 0) {
        horizontal_zero_area_boxes_.push_back(std::move(box));
      } else {
        // TODO(user): if the number of fixed boxes is large, we keep
        // reconstructing them each time this is called for no reason.
        if (box.IsFixed()) {
          fixed_non_zero_area_boxes_.push_back(std::move(box));
        } else {
          non_fixed_non_zero_area_boxes_.push_back(std::move(box));
        }
      }
    }

    // We ignore pairs of two fixed boxes. The only thing to propagate between
    // two fixed boxes is a conflict and it should already have been taken care
    // of by the MandatoryOverlapPropagator propagator.
    RETURN_IF_FALSE(FindRestrictionsAndPropagateConflict(
        non_fixed_non_zero_area_boxes_, fixed_non_zero_area_boxes_,
        &restrictions));

    // Check zero area boxes against non-zero area boxes.
    for (auto& non_zero_area_boxes :
         {fixed_non_zero_area_boxes_, non_fixed_non_zero_area_boxes_}) {
      RETURN_IF_FALSE(FindRestrictionsAndPropagateConflict(
          non_zero_area_boxes, horizontal_zero_area_boxes_, &restrictions));
      RETURN_IF_FALSE(FindRestrictionsAndPropagateConflict(
          non_zero_area_boxes, vertical_zero_area_boxes_, &restrictions));
      RETURN_IF_FALSE(FindRestrictionsAndPropagateConflict(
          non_zero_area_boxes, point_zero_area_boxes_, &restrictions));
    }

    // Check vertical zero area boxes against horizontal zero area boxes.
    RETURN_IF_FALSE(FindRestrictionsAndPropagateConflict(
        vertical_zero_area_boxes_, horizontal_zero_area_boxes_, &restrictions));
  }
  for (const PairwiseRestriction& restriction : restrictions) {
    RETURN_IF_FALSE(PropagateTwoBoxes(restriction));
  }
  return true;
}

bool RectanglePairwisePropagator::FindRestrictionsAndPropagateConflict(
    absl::Span<const ItemWithVariableSize> items1,
    absl::Span<const ItemWithVariableSize> items2,
    std::vector<PairwiseRestriction>* restrictions) {
  const int max_pairs =
      params_->max_pairs_pairwise_reasoning_in_no_overlap_2d();
  if (items1.size() * items2.size() > max_pairs) {
    return true;
  }
  AppendPairwiseRestrictions(items1, items2, restrictions);
  for (const PairwiseRestriction& restriction : *restrictions) {
    if (restriction.type ==
        PairwiseRestriction::PairwiseRestrictionType::CONFLICT) {
      RETURN_IF_FALSE(PropagateTwoBoxes(restriction));
    }
  }
  return true;
}

bool RectanglePairwisePropagator::PropagateTwoBoxes(
    const PairwiseRestriction& restriction) {
  if (restriction.type ==
      PairwiseRestriction::PairwiseRestrictionType::CONFLICT) {
    num_pairwise_conflicts_++;
  } else {
    num_pairwise_propagations_++;
  }
  return helper_->PropagateRelativePosition(
      restriction.first_index, restriction.second_index, restriction.type);
}

#undef RETURN_IF_FALSE
}  // namespace sat
}  // namespace operations_research
