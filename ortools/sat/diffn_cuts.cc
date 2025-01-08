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

#include "ortools/sat/diffn_cuts.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

}  // namespace

DiffnBaseEvent::DiffnBaseEvent(int t, SchedulingConstraintHelper* x_helper)
    : x_start_min(x_helper->StartMin(t)),
      x_start_max(x_helper->StartMax(t)),
      x_end_min(x_helper->EndMin(t)),
      x_end_max(x_helper->EndMax(t)),
      x_size_min(x_helper->SizeMin(t)) {}

struct DiffnEnergyEvent : DiffnBaseEvent {
  DiffnEnergyEvent(int t, SchedulingConstraintHelper* x_helper)
      : DiffnBaseEvent(t, x_helper) {}

  // We need this for linearizing the energy in some cases.
  AffineExpression y_size;

  // If set, this event is optional and its presence is controlled by this.
  LiteralIndex presence_literal_index = kNoLiteralIndex;

  // A linear expression which is a valid lower bound on the total energy of
  // this event. We also cache the activity of the expression to not recompute
  // it all the time.
  LinearExpression linearized_energy;
  double linearized_energy_lp_value = 0.0;

  // True if linearized_energy is not exact and a McCormick relaxation.
  bool energy_is_quadratic = false;

  // Used to minimize the increase on the y axis for rectangles.
  double y_spread = 0.0;

  // The actual value of the presence literal of the interval(s) is checked
  // when the event is created. A value of kNoLiteralIndex indicates that either
  // the interval was not optional, or that its presence literal is true at
  // level zero.
  bool IsPresent() const { return presence_literal_index == kNoLiteralIndex; }

  // Computes the mandatory minimal overlap of the interval with the time window
  // [start, end].
  IntegerValue GetMinOverlap(IntegerValue start, IntegerValue end) const {
    return std::max(std::min({x_end_min - start, end - x_start_max, x_size_min,
                              end - start}),
                    IntegerValue(0));
  }

  // This method expects all the other fields to have been filled before.
  // It must be called before the EnergyEvent is used.
  ABSL_MUST_USE_RESULT bool FillEnergyLp(
      AffineExpression x_size,
      const util_intops::StrongVector<IntegerVariable, double>& lp_values,
      Model* model) {
    LinearConstraintBuilder tmp_energy(model);
    if (IsPresent()) {
      if (!decomposed_energy.empty()) {
        if (!tmp_energy.AddDecomposedProduct(decomposed_energy)) return false;
      } else {
        tmp_energy.AddQuadraticLowerBound(x_size, y_size,
                                          model->GetOrCreate<IntegerTrail>(),
                                          &energy_is_quadratic);
      }
    } else {
      if (!tmp_energy.AddLiteralTerm(Literal(presence_literal_index),
                                     energy_min)) {
        return false;
      }
    }
    linearized_energy = tmp_energy.BuildExpression();
    linearized_energy_lp_value = linearized_energy.LpValue(lp_values);
    return true;
  }

  std::string DebugString() const {
    return absl::StrCat(
        "DiffnEnergyEvent(x_start_min = ", x_start_min.value(),
        ", x_start_max = ", x_start_max.value(),
        ", x_end_min = ", x_end_min.value(),
        ", x_end_max = ", x_end_max.value(), ", y_min = ", y_min.value(),
        ", y_max = ", y_max.value(), ", y_size = ", y_size.DebugString(),
        ", energy = ",
        decomposed_energy.empty()
            ? "{}"
            : absl::StrCat(decomposed_energy.size(), " terms"),
        ", presence_literal_index = ", presence_literal_index.value(), ")");
  }
};

void GenerateNoOverlap2dEnergyCut(
    absl::Span<const std::vector<LiteralValueValue>> energies,
    absl::Span<const int> rectangles, absl::string_view cut_name, Model* model,
    LinearConstraintManager* manager, SchedulingConstraintHelper* x_helper,
    SchedulingConstraintHelper* y_helper,
    SchedulingDemandHelper* y_demands_helper) {
  std::vector<DiffnEnergyEvent> events;
  const auto& lp_values = manager->LpValues();
  for (const int rect : rectangles) {
    if (y_helper->SizeMax(rect) == 0 || x_helper->SizeMax(rect) == 0) {
      continue;
    }

    DiffnEnergyEvent e(rect, x_helper);
    e.y_min = y_helper->StartMin(rect);
    e.y_max = y_helper->EndMax(rect);
    e.y_size = y_helper->Sizes()[rect];
    e.decomposed_energy = energies[rect];
    e.presence_literal_index =
        x_helper->IsPresent(rect)
            ? (y_helper->IsPresent(rect)
                   ? kNoLiteralIndex
                   : y_helper->PresenceLiteral(rect).Index())
            : x_helper->PresenceLiteral(rect).Index();
    e.y_size_min = y_helper->SizeMin(rect);
    e.energy_min = y_demands_helper->EnergyMin(rect);
    e.energy_is_quadratic = y_demands_helper->EnergyIsQuadratic(rect);

    // We can always skip events.
    if (!e.FillEnergyLp(x_helper->Sizes()[rect], lp_values, model)) continue;
    events.push_back(e);
  }

  if (events.empty()) return;

  // Compute y_spread.
  double average_d = 0.0;
  for (const auto& e : events) {
    average_d += ToDouble(e.y_min + e.y_max);
  }
  const double average = average_d / 2.0 / static_cast<double>(events.size());
  for (auto& e : events) {
    e.y_spread = std::abs(ToDouble(e.y_max) - average) +
                 std::abs(average - ToDouble(e.y_min));
  }

  TopNCuts top_n_cuts(5);

  std::sort(events.begin(), events.end(),
            [](const DiffnEnergyEvent& a, const DiffnEnergyEvent& b) {
              return std::tie(a.x_start_min, a.y_spread, a.x_end_max) <
                     std::tie(b.x_start_min, b.y_spread, b.x_end_max);
            });

  // The sum of all energies can be used to stop iterating early.
  double sum_of_all_energies = 0.0;
  for (const auto& e : events) {
    sum_of_all_energies += e.linearized_energy_lp_value;
  }

  CapacityProfile capacity_profile;
  for (int i1 = 0; i1 + 1 < events.size(); ++i1) {
    // For each start time, we will keep the most violated cut generated while
    // scanning the residual intervals.
    int max_violation_end_index = -1;
    double max_relative_violation = 1.0 + kMinCutViolation;
    IntegerValue max_violation_window_start(0);
    IntegerValue max_violation_window_end(0);
    IntegerValue max_violation_y_min(0);
    IntegerValue max_violation_y_max(0);
    IntegerValue max_violation_area(0);
    bool max_violation_use_precise_area = false;

    // Accumulate intervals, areas, energies and check for potential cuts.
    double energy_lp = 0.0;
    IntegerValue window_min = kMaxIntegerValue;
    IntegerValue window_max = kMinIntegerValue;
    IntegerValue y_min = kMaxIntegerValue;
    IntegerValue y_max = kMinIntegerValue;
    capacity_profile.Clear();

    // We sort all tasks (x_start_min(task) >= x_start_min(start_index) by
    // increasing end max.
    std::vector<DiffnEnergyEvent> residual_events(events.begin() + i1,
                                                  events.end());
    std::sort(residual_events.begin(), residual_events.end(),
              [](const DiffnEnergyEvent& a, const DiffnEnergyEvent& b) {
                return std::tie(a.x_end_max, a.y_spread) <
                       std::tie(b.x_end_max, b.y_spread);
              });
    // Let's process residual tasks and evaluate the violation of the cut at
    // each step. We follow the same structure as the cut creation code below.
    for (int i2 = 0; i2 < residual_events.size(); ++i2) {
      const DiffnEnergyEvent& e = residual_events[i2];
      energy_lp += e.linearized_energy_lp_value;
      window_min = std::min(window_min, e.x_start_min);
      window_max = std::max(window_max, e.x_end_max);
      y_min = std::min(y_min, e.y_min);
      y_max = std::max(y_max, e.y_max);
      capacity_profile.AddRectangle(e.x_start_min, e.x_end_max, e.y_min,
                                    e.y_max);

      // Dominance rule. If the next interval also fits in
      // [window_min, window_max]*[y_min, y_max], the cut will be stronger with
      // the next interval/rectangle.
      if (i2 + 1 < residual_events.size() &&
          residual_events[i2 + 1].x_start_min >= window_min &&
          residual_events[i2 + 1].x_end_max <= window_max &&
          residual_events[i2 + 1].y_min >= y_min &&
          residual_events[i2 + 1].y_max <= y_max) {
        continue;
      }

      // Checks the current area vs the sum of all energies.
      // The area is capacity_profile.GetBoundingArea().
      //     We can compare it to the bounding box area:
      //         (window_max - window_min) * (y_max - y_min).
      bool use_precise_area = false;
      IntegerValue precise_area(0);
      double area_lp = 0.0;
      const IntegerValue bbox_area =
          (window_max - window_min) * (y_max - y_min);
      precise_area = capacity_profile.GetBoundingArea();
      use_precise_area = precise_area < bbox_area;
      area_lp = ToDouble(std::min(precise_area, bbox_area));

      if (area_lp >= sum_of_all_energies) {
        break;
      }

      // Compute the violation of the potential cut.
      const double relative_violation = energy_lp / area_lp;
      if (relative_violation > max_relative_violation) {
        max_violation_end_index = i2;
        max_relative_violation = relative_violation;
        max_violation_window_start = window_min;
        max_violation_window_end = window_max;
        max_violation_y_min = y_min;
        max_violation_y_max = y_max;
        max_violation_area = std::min(precise_area, bbox_area);
        max_violation_use_precise_area = use_precise_area;
      }
    }

    if (max_violation_end_index == -1) continue;

    // A maximal violated cut has been found.
    // Build it and add it to the pool.
    bool add_opt_to_name = false;
    bool add_quadratic_to_name = false;
    bool add_energy_to_name = false;
    LinearConstraintBuilder cut(model, kMinIntegerValue, max_violation_area);
    for (int i2 = 0; i2 <= max_violation_end_index; ++i2) {
      const DiffnEnergyEvent& event = residual_events[i2];
      cut.AddLinearExpression(event.linearized_energy);
      if (!event.IsPresent()) add_opt_to_name = true;
      if (event.energy_is_quadratic) add_quadratic_to_name = true;
      if (event.energy_min > event.x_size_min * event.y_size_min) {
        add_energy_to_name = true;
      }
    }
    std::string full_name(cut_name);
    if (add_opt_to_name) full_name.append("_optional");
    if (add_quadratic_to_name) full_name.append("_quadratic");
    if (add_energy_to_name) full_name.append("_energy");
    if (max_violation_use_precise_area) full_name.append("_precise");
    top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
  }
  top_n_cuts.TransferToManager(manager);
}

CutGenerator CreateNoOverlap2dEnergyCutGenerator(
    SchedulingConstraintHelper* x_helper, SchedulingConstraintHelper* y_helper,
    SchedulingDemandHelper* x_demands_helper,
    SchedulingDemandHelper* y_demands_helper,
    absl::Span<const std::vector<LiteralValueValue>> energies, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(x_helper, model, &result.vars);
  AddIntegerVariableFromIntervals(y_helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  result.generate_cuts = [x_helper, y_helper, x_demands_helper,
                          y_demands_helper, model,
                          energies =
                              std::vector<std::vector<LiteralValueValue>>(
                                  energies.begin(), energies.end())](
                             LinearConstraintManager* manager) {
    if (!x_helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!y_helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!x_demands_helper->CacheAllEnergyValues()) return true;
    if (!y_demands_helper->CacheAllEnergyValues()) return true;

    const int num_rectangles = x_helper->NumTasks();
    std::vector<int> active_rectangles_indexes;
    active_rectangles_indexes.reserve(num_rectangles);
    std::vector<Rectangle> active_rectangles;
    active_rectangles.reserve(num_rectangles);
    for (int rect = 0; rect < num_rectangles; ++rect) {
      if (y_helper->IsAbsent(rect) || y_helper->IsAbsent(rect)) continue;
      // We do not consider rectangles controlled by 2 different unassigned
      // enforcement literals.
      if (!x_helper->IsPresent(rect) && !y_helper->IsPresent(rect) &&
          x_helper->PresenceLiteral(rect) != y_helper->PresenceLiteral(rect)) {
        continue;
      }

      // TODO(user): It might be possible/better to use some shifted value
      // here, but for now this code is not in the hot spot, so better be
      // defensive and only do connected components on really disjoint
      // rectangles.
      active_rectangles_indexes.push_back(rect);
      Rectangle& rectangle = active_rectangles.emplace_back();
      rectangle.x_min = x_helper->StartMin(rect);
      rectangle.x_max = x_helper->EndMax(rect);
      rectangle.y_min = y_helper->StartMin(rect);
      rectangle.y_max = y_helper->EndMax(rect);
    }

    if (active_rectangles.size() <= 1) return true;

    const CompactVectorVector<int> components =
        GetOverlappingRectangleComponents(active_rectangles);

    // Forward pass. No need to do a backward pass.
    std::vector<int> rectangles;
    for (int i = 0; i < components.size(); ++i) {
      absl::Span<const int> indexes = components[i];
      if (indexes.size() <= 1) continue;

      rectangles.clear();
      rectangles.reserve(indexes.size());
      for (const int index : indexes) {
        rectangles.push_back(active_rectangles_indexes[index]);
      }
      GenerateNoOverlap2dEnergyCut(energies, rectangles, "NoOverlap2dXEnergy",
                                   model, manager, x_helper, y_helper,
                                   y_demands_helper);
      GenerateNoOverlap2dEnergyCut(energies, rectangles, "NoOverlap2dYEnergy",
                                   model, manager, y_helper, x_helper,
                                   x_demands_helper);
    }

    return true;
  };
  return result;
}

DiffnCtEvent::DiffnCtEvent(int t, SchedulingConstraintHelper* x_helper)
    : DiffnBaseEvent(t, x_helper) {}

std::string DiffnCtEvent::DebugString() const {
  return absl::StrCat("DiffnCtEvent(x_end = ", x_end.DebugString(),
                      ", x_start_min = ", x_start_min.value(),
                      ", x_start_max = ", x_start_max.value(),
                      ", x_size_min = ", x_size_min.value(),
                      ", x_lp_end = ", x_lp_end, ", y_min = ", y_min.value(),
                      ", y_max = ", y_max.value(),
                      ", y_size_min = ", y_size_min.value(),
                      ", energy_min = ", energy_min.value(),
                      ", use_energy = ", use_energy, ", lifted = ", lifted);
}

// We generate the cut from the Smith's rule from:
// M. Queyranne, Structure of a simple scheduling polyhedron,
// Mathematical Programming 58 (1993), 263–285
//
// The original cut is:
//    sum(end_min_i * duration_min_i) >=
//        (sum(duration_min_i^2) + sum(duration_min_i)^2) / 2
// We strengthen this cuts by noticing that if all tasks starts after S,
// then replacing end_min_i by (end_min_i - S) is still valid.
//
// A second difference is that we look at a set of intervals starting
// after a given start_min, sorted by relative (end_lp - start_min).
//
// TODO(user): merge with Packing cuts.
void GenerateNoOvelap2dCompletionTimeCutsWithEnergy(
    absl::string_view cut_name, std::vector<DiffnCtEvent> events,
    bool use_lifting, bool skip_low_sizes, Model* model,
    LinearConstraintManager* manager) {
  TopNCuts top_n_cuts(5);

  // Sort by start min to bucketize by start_min.
  std::sort(events.begin(), events.end(),
            [](const DiffnCtEvent& e1, const DiffnCtEvent& e2) {
              return std::tie(e1.x_start_min, e1.y_size_min, e1.x_lp_end) <
                     std::tie(e2.x_start_min, e2.y_size_min, e2.x_lp_end);
            });
  for (int start = 0; start + 1 < events.size(); ++start) {
    // Skip to the next start_min value.
    if (start > 0 &&
        events[start].x_start_min == events[start - 1].x_start_min) {
      continue;
    }

    const IntegerValue sequence_start_min = events[start].x_start_min;
    std::vector<DiffnCtEvent> residual_tasks(events.begin() + start,
                                             events.end());

    // We look at event that start before sequence_start_min, but are forced
    // to cross this time point. In that case, we replace this event by a
    // truncated event starting at sequence_start_min. To do this, we reduce
    // the size_min, align the start_min with the sequence_start_min, and
    // scale the energy down accordingly.
    if (use_lifting) {
      for (int before = 0; before < start; ++before) {
        if (events[before].x_start_min + events[before].x_size_min >
            sequence_start_min) {
          // Build the vector of energies as the vector of sizes.
          DiffnCtEvent event = events[before];  // Copy.
          event.lifted = true;
          event.energy_min = ComputeEnergyMinInWindow(
              event.x_start_min, event.x_start_max, event.x_end_min,
              event.x_end_max, event.x_size_min, event.y_size_min,
              event.decomposed_energy, sequence_start_min, event.x_end_max);
          event.x_size_min =
              event.x_size_min + event.x_start_min - sequence_start_min;
          event.x_start_min = sequence_start_min;
          if (event.energy_min > event.x_size_min * event.y_size_min) {
            event.use_energy = true;
          }
          DCHECK_GE(event.energy_min, event.x_size_min * event.y_size_min);
          if (event.energy_min <= 0) continue;
          residual_tasks.push_back(event);
        }
      }
    }

    std::sort(residual_tasks.begin(), residual_tasks.end(),
              [](const DiffnCtEvent& e1, const DiffnCtEvent& e2) {
                return e1.x_lp_end < e2.x_lp_end;
              });

    int best_end = -1;
    double best_efficacy = 0.01;
    IntegerValue best_min_contrib(0);
    IntegerValue sum_duration(0);
    IntegerValue sum_square_duration(0);
    IntegerValue best_capacity(0);
    double unscaled_lp_contrib = 0.0;
    IntegerValue current_start_min(kMaxIntegerValue);
    IntegerValue y_min = kMaxIntegerValue;
    IntegerValue y_max = kMinIntegerValue;

    bool use_dp = true;
    MaxBoundedSubsetSum dp(0);
    for (int i = 0; i < residual_tasks.size(); ++i) {
      const DiffnCtEvent& event = residual_tasks[i];
      DCHECK_GE(event.x_start_min, sequence_start_min);
      const IntegerValue energy = event.energy_min;
      sum_duration += energy;
      if (!AddProductTo(energy, energy, &sum_square_duration)) break;

      unscaled_lp_contrib += event.x_lp_end * ToDouble(energy);
      current_start_min = std::min(current_start_min, event.x_start_min);

      // This is competing with the brute force approach. Skip cases covered
      // by the other code.
      if (skip_low_sizes && i < 7) continue;

      // For the capacity, we use the worse |y_max - y_min| and if all the tasks
      // so far have a fixed demand with a gcd > 1, we can round it down.
      //
      // TODO(user): Use dynamic programming to compute all possible values for
      // the sum of demands as long as the involved numbers are small or the
      // number of tasks are small.
      y_min = std::min(y_min, event.y_min);
      y_max = std::max(y_max, event.y_max);
      if (!event.y_size_is_fixed) use_dp = false;
      if (use_dp) {
        if (i == 0) {
          dp.Reset((y_max - y_min).value());
        } else {
          if (y_max - y_min != dp.Bound()) {
            use_dp = false;
          }
        }
      }
      if (use_dp) {
        dp.Add(event.y_size_min.value());
      }

      const IntegerValue capacity =
          use_dp ? IntegerValue(dp.CurrentMax()) : y_max - y_min;

      // We compute the cuts like if it was a disjunctive cut with all the
      // duration actually equal to energy / capacity. But to keep the
      // computation in the integer domain, we multiply by capacity
      // everywhere instead.
      IntegerValue min_contrib = 0;
      if (!AddProductTo(sum_duration, sum_duration, &min_contrib)) break;
      if (!AddTo(sum_square_duration, &min_contrib)) break;
      min_contrib = min_contrib / 2;  // The above is the double of the area.

      const IntegerValue intermediate = CapProdI(sum_duration, capacity);
      if (AtMinOrMaxInt64I(intermediate)) break;
      const IntegerValue offset = CapProdI(current_start_min, intermediate);
      if (AtMinOrMaxInt64I(offset)) break;
      if (!AddTo(offset, &min_contrib)) break;

      // We compute the efficacity in the unscaled domain where the l2 norm of
      // the cuts is exactly the sqrt of  the sum of squared duration.
      const double efficacy =
          (ToDouble(min_contrib) / ToDouble(capacity) - unscaled_lp_contrib) /
          std::sqrt(ToDouble(sum_square_duration));

      // TODO(user): Check overflow and ignore if too big.
      if (efficacy > best_efficacy) {
        best_efficacy = efficacy;
        best_end = i;
        best_min_contrib = min_contrib;
        best_capacity = capacity;
      }
    }
    if (best_end != -1) {
      LinearConstraintBuilder cut(model, best_min_contrib, kMaxIntegerValue);
      bool is_lifted = false;
      bool add_energy_to_name = false;
      for (int i = 0; i <= best_end; ++i) {
        const DiffnCtEvent& event = residual_tasks[i];
        is_lifted |= event.lifted;
        add_energy_to_name |= event.use_energy;
        cut.AddTerm(event.x_end, event.energy_min * best_capacity);
      }
      std::string full_name(cut_name);
      if (is_lifted) full_name.append("_lifted");
      if (add_energy_to_name) full_name.append("_energy");
      top_n_cuts.AddCut(cut.Build(), full_name, manager->LpValues());
    }
  }
  top_n_cuts.TransferToManager(manager);
}

// TODO(user): Use demands_helper and decomposed energy.
CutGenerator CreateNoOverlap2dCompletionTimeCutGenerator(
    SchedulingConstraintHelper* x_helper, SchedulingConstraintHelper* y_helper,
    Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(x_helper, model, &result.vars);
  AddIntegerVariableFromIntervals(y_helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  auto* product_decomposer = model->GetOrCreate<ProductDecomposer>();
  result.generate_cuts = [x_helper, y_helper, product_decomposer,
                          model](LinearConstraintManager* manager) {
    if (!x_helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!y_helper->SynchronizeAndSetTimeDirection(true)) return false;

    const int num_rectangles = x_helper->NumTasks();
    std::vector<int> active_rectangles_indexes;
    active_rectangles_indexes.reserve(num_rectangles);
    std::vector<Rectangle> active_rectangles;
    active_rectangles.reserve(num_rectangles);
    std::vector<IntegerValue> cached_areas(num_rectangles);
    for (int rect = 0; rect < num_rectangles; ++rect) {
      if (!y_helper->IsPresent(rect) || !y_helper->IsPresent(rect)) continue;

      cached_areas[rect] = x_helper->SizeMin(rect) * y_helper->SizeMin(rect);
      if (cached_areas[rect] == 0) continue;

      // TODO(user): It might be possible/better to use some shifted value
      // here, but for now this code is not in the hot spot, so better be
      // defensive and only do connected components on really disjoint
      // rectangles.
      active_rectangles_indexes.push_back(rect);
      Rectangle& rectangle = active_rectangles.emplace_back();
      rectangle.x_min = x_helper->StartMin(rect);
      rectangle.x_max = x_helper->EndMax(rect);
      rectangle.y_min = y_helper->StartMin(rect);
      rectangle.y_max = y_helper->EndMax(rect);
    }

    if (active_rectangles.size() <= 1) return true;

    const CompactVectorVector<int> components =
        GetOverlappingRectangleComponents(active_rectangles);
    std::vector<int> rectangles;
    for (int i = 0; i < components.size(); ++i) {
      absl::Span<const int> indexes = components[i];
      if (indexes.size() <= 1) continue;

      rectangles.clear();
      rectangles.reserve(indexes.size());
      for (const int index : indexes) {
        rectangles.push_back(active_rectangles_indexes[index]);
      }

      auto generate_cuts = [product_decomposer, manager, model, &rectangles](
                               absl::string_view cut_name,
                               SchedulingConstraintHelper* x_helper,
                               SchedulingConstraintHelper* y_helper) {
        std::vector<DiffnCtEvent> events;

        const auto& lp_values = manager->LpValues();
        for (const int rect : rectangles) {
          DiffnCtEvent event(rect, x_helper);
          event.x_end = x_helper->Ends()[rect];
          event.x_lp_end = event.x_end.LpValue(lp_values);
          event.y_min = y_helper->StartMin(rect);
          event.y_max = y_helper->EndMax(rect);
          event.y_size_min = y_helper->SizeMin(rect);

          // TODO(user): Use improved energy from demands helper.
          event.energy_min = event.x_size_min * event.y_size_min;
          event.decomposed_energy = product_decomposer->TryToDecompose(
              x_helper->Sizes()[rect], y_helper->Sizes()[rect]);
          events.push_back(event);
        }

        GenerateNoOvelap2dCompletionTimeCutsWithEnergy(
            cut_name, std::move(events),
            /*use_lifting=*/false,
            /*skip_low_sizes=*/false, model, manager);
      };

      if (!x_helper->SynchronizeAndSetTimeDirection(true)) return false;
      if (!y_helper->SynchronizeAndSetTimeDirection(true)) return false;
      generate_cuts("NoOverlap2dXCompletionTime", x_helper, y_helper);
      generate_cuts("NoOverlap2dYCompletionTime", y_helper, x_helper);
      if (!x_helper->SynchronizeAndSetTimeDirection(false)) return false;
      if (!y_helper->SynchronizeAndSetTimeDirection(false)) return false;
      generate_cuts("NoOverlap2dXCompletionTimeMirror", x_helper, y_helper);
      generate_cuts("NoOverlap2dYCompletionTimeMirror", y_helper, x_helper);
    }
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research
