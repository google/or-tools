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

#include "ortools/sat/scheduling_cuts.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {
// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;
}  // namespace

BaseEvent::BaseEvent(int t, SchedulingConstraintHelper* x_helper)
    : x_start_min(x_helper->StartMin(t)),
      x_start_max(x_helper->StartMax(t)),
      x_end_min(x_helper->EndMin(t)),
      x_end_max(x_helper->EndMax(t)),
      x_size_min(x_helper->SizeMin(t)) {}

struct EnergyEvent : BaseEvent {
  EnergyEvent(int t, SchedulingConstraintHelper* x_helper)
      : BaseEvent(t, x_helper) {}

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
        "EnergyEvent(x_start_min = ", x_start_min.value(),
        ", x_start_max = ", x_start_max.value(),
        ", x_end_min = ", x_end_min.value(),
        ", x_end_max = ", x_end_max.value(),
        ", y_size = ", y_size.DebugString(), ", energy = ",
        decomposed_energy.empty()
            ? "{}"
            : absl::StrCat(decomposed_energy.size(), " terms"),
        ", presence_literal_index = ", presence_literal_index.value(), ")");
  }
};

namespace {

// Compute the energetic contribution of a task in a given time window, and
// add it to the cut. It returns false if it tried to generate the cut, and
// failed.
ABSL_MUST_USE_RESULT bool AddOneEvent(
    const EnergyEvent& event, IntegerValue window_start,
    IntegerValue window_end, LinearConstraintBuilder* cut,
    bool* add_energy_to_name = nullptr, bool* add_quadratic_to_name = nullptr,
    bool* add_opt_to_name = nullptr, bool* add_lifted_to_name = nullptr) {
  DCHECK(cut != nullptr);

  if (event.x_end_min <= window_start || event.x_start_max >= window_end) {
    return true;  // Event can move outside the time window.
  }

  if (event.x_start_min >= window_start && event.x_end_max <= window_end) {
    // Event is always contained by the time window.
    cut->AddLinearExpression(event.linearized_energy);

    if (event.energy_is_quadratic && add_quadratic_to_name != nullptr) {
      *add_quadratic_to_name = true;
    }
    if (add_energy_to_name != nullptr &&
        event.energy_min > event.x_size_min * event.y_size_min) {
      *add_energy_to_name = true;
    }
    if (!event.IsPresent() && add_opt_to_name != nullptr) {
      *add_opt_to_name = true;
    }
  } else {  // The event has a mandatory overlap with the time window.
    const IntegerValue min_overlap =
        event.GetMinOverlap(window_start, window_end);
    if (min_overlap <= 0) return true;
    if (add_lifted_to_name != nullptr) *add_lifted_to_name = true;

    if (event.IsPresent()) {
      const std::vector<LiteralValueValue>& energy = event.decomposed_energy;
      if (energy.empty()) {
        cut->AddTerm(event.y_size, min_overlap);
      } else {
        const IntegerValue window_size = window_end - window_start;
        for (const auto [lit, fixed_size, fixed_demand] : energy) {
          const IntegerValue alt_end_min =
              std::max(event.x_end_min, event.x_start_min + fixed_size);
          const IntegerValue alt_start_max =
              std::min(event.x_start_max, event.x_end_max - fixed_size);
          const IntegerValue energy_min =
              fixed_demand *
              std::min({alt_end_min - window_start, window_end - alt_start_max,
                        fixed_size, window_size});
          if (energy_min == 0) continue;
          if (!cut->AddLiteralTerm(lit, energy_min)) return false;
        }
        if (add_energy_to_name != nullptr) *add_energy_to_name = true;
      }
    } else {
      if (add_opt_to_name != nullptr) *add_opt_to_name = true;
      const IntegerValue min_energy = ComputeEnergyMinInWindow(
          event.x_start_min, event.x_start_max, event.x_end_min,
          event.x_end_max, event.x_size_min, event.y_size_min,
          event.decomposed_energy, window_start, window_end);
      if (min_energy > event.x_size_min * event.y_size_min &&
          add_energy_to_name != nullptr) {
        *add_energy_to_name = true;
      }
      if (!cut->AddLiteralTerm(Literal(event.presence_literal_index),
                               min_energy)) {
        return false;
      }
    }
  }
  return true;
}

// Returns the list of all possible demand values for the given event.
// It returns an empty vector is the number of values is too large.
std::vector<int64_t> FindPossibleDemands(const EnergyEvent& event,
                                         const VariablesAssignment& assignment,
                                         IntegerTrail* integer_trail) {
  std::vector<int64_t> possible_demands;
  if (event.decomposed_energy.empty()) {
    if (integer_trail->IsFixed(event.y_size)) {
      possible_demands.push_back(
          integer_trail->FixedValue(event.y_size).value());
    } else {
      if (integer_trail->InitialVariableDomain(event.y_size.var).Size() >
          1000000) {
        return {};
      }
      for (const int64_t var_value :
           integer_trail->InitialVariableDomain(event.y_size.var).Values()) {
        possible_demands.push_back(event.y_size.ValueAt(var_value).value());
      }
    }
  } else {
    for (const auto [lit, fixed_size, fixed_demand] : event.decomposed_energy) {
      if (assignment.LiteralIsFalse(lit)) continue;
      possible_demands.push_back(fixed_demand.value());
    }
  }
  return possible_demands;
}

// This generates the actual cut and compute its activity vs the
// available_energy_lp.
bool CutIsEfficient(
    absl::Span<const EnergyEvent> events, IntegerValue window_start,
    IntegerValue window_end, double available_energy_lp,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraintBuilder* temp_builder) {
  temp_builder->Clear();
  for (const EnergyEvent& event : events) {
    if (!AddOneEvent(event, window_start, window_end, temp_builder)) {
      return false;
    }
  }
  return temp_builder->BuildExpression().LpValue(lp_values) >=
         available_energy_lp * (1.0 + kMinCutViolation);
}

}  // namespace

// This cumulative energetic cut generator will split the cumulative span in 2
// regions.
//
// In the region before the min of the makespan, we will compute a more
// precise reachable profile and have a better estimation of the energy
// available between two time point. the improvement can come from two sources:
//   - subset sum indicates that the max capacity cannot be reached.
//   - sum of demands < max capacity.
//
// In the region after the min of the makespan, we will use
//    fixed_capacity * (makespan - makespan_min)
// as the available energy.
void GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity(
    absl::string_view cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    std::vector<EnergyEvent> events, IntegerValue capacity,
    AffineExpression makespan, TimeLimit* time_limit, Model* model,
    LinearConstraintManager* manager) {
  // Checks the precondition of the code.
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  DCHECK(integer_trail->IsFixed(capacity));

  const VariablesAssignment& assignment =
      model->GetOrCreate<Trail>()->Assignment();

  // Currently, we look at all the possible time windows, and will push all cuts
  // in the TopNCuts object. From our observations, this generator creates only
  // a few cuts for a given run.
  //
  // The complexity of this loop is n^3. if we follow the latest research, we
  // could implement this in n log^2(n). Still, this is not visible in the
  // profile as we only this method at the root node,
  struct OverloadedTimeWindowWithMakespan {
    IntegerValue start;
    IntegerValue end;
    IntegerValue fixed_energy_rhs;  // Can be complemented by the makespan.
    bool use_makespan = false;
    bool use_subset_sum = false;
  };

  std::vector<OverloadedTimeWindowWithMakespan> overloaded_time_windows;
  // Compute relevant time points.
  // TODO(user): We could reduce this set.
  // TODO(user): we can compute the max usage between makespan_min and
  //    makespan_max.
  std::vector<IntegerValue> time_points;
  std::vector<std::vector<int64_t>> possible_demands(events.size());
  const IntegerValue makespan_min = integer_trail->LowerBound(makespan);
  IntegerValue max_end_min = kMinIntegerValue;  // Used to abort early.
  IntegerValue max_end_max = kMinIntegerValue;  // Used as a sentinel.
  for (int i = 0; i < events.size(); ++i) {
    const EnergyEvent& event = events[i];
    if (event.x_start_min < makespan_min) {
      time_points.push_back(event.x_start_min);
    }
    if (event.x_start_max < makespan_min) {
      time_points.push_back(event.x_start_max);
    }
    if (event.x_end_min < makespan_min) {
      time_points.push_back(event.x_end_min);
    }
    if (event.x_end_max < makespan_min) {
      time_points.push_back(event.x_end_max);
    }
    max_end_min = std::max(max_end_min, event.x_end_min);
    max_end_max = std::max(max_end_max, event.x_end_max);
    possible_demands[i] = FindPossibleDemands(event, assignment, integer_trail);
  }
  time_points.push_back(makespan_min);
  time_points.push_back(max_end_max);
  gtl::STLSortAndRemoveDuplicates(&time_points);

  const int num_time_points = time_points.size();
  absl::flat_hash_map<IntegerValue, IntegerValue> reachable_capacity_ending_at;

  MaxBoundedSubsetSum reachable_capacity_subset_sum(capacity.value());
  for (int i = 1; i < num_time_points; ++i) {
    const IntegerValue window_start = time_points[i - 1];
    const IntegerValue window_end = time_points[i];
    reachable_capacity_subset_sum.Reset(capacity.value());
    for (int i = 0; i < events.size(); ++i) {
      const EnergyEvent& event = events[i];
      if (event.x_start_min >= window_end || event.x_end_max <= window_start) {
        continue;
      }
      if (possible_demands[i].empty()) {  // Number of values was too large.
        // In practice, it stops the DP as the upper bound is reached.
        reachable_capacity_subset_sum.Add(capacity.value());
      } else {
        reachable_capacity_subset_sum.AddChoices(possible_demands[i]);
      }
      if (reachable_capacity_subset_sum.CurrentMax() == capacity.value()) break;
    }
    reachable_capacity_ending_at[window_end] =
        reachable_capacity_subset_sum.CurrentMax();
  }

  const double capacity_lp = ToDouble(capacity);
  const double makespan_lp = makespan.LpValue(lp_values);
  const double makespan_min_lp = ToDouble(makespan_min);
  LinearConstraintBuilder temp_builder(model);
  for (int i = 0; i + 1 < num_time_points; ++i) {
    // Checks the time limit if the problem is too big.
    if (events.size() > 50 && time_limit->LimitReached()) return;

    const IntegerValue window_start = time_points[i];
    // After max_end_min, all tasks can fit before window_start.
    if (window_start >= max_end_min) break;

    IntegerValue cumulated_max_energy = 0;
    IntegerValue cumulated_max_energy_before_makespan_min = 0;
    bool use_subset_sum = false;
    bool use_subset_sum_before_makespan_min = false;

    for (int j = i + 1; j < num_time_points; ++j) {
      const IntegerValue strip_start = time_points[j - 1];
      const IntegerValue window_end = time_points[j];
      const IntegerValue max_reachable_capacity_in_current_strip =
          reachable_capacity_ending_at[window_end];
      DCHECK_LE(max_reachable_capacity_in_current_strip, capacity);

      // Update states for the name of the generated cut.
      if (max_reachable_capacity_in_current_strip < capacity) {
        use_subset_sum = true;
        if (window_end <= makespan_min) {
          use_subset_sum_before_makespan_min = true;
        }
      }

      const IntegerValue energy_in_strip =
          (window_end - strip_start) * max_reachable_capacity_in_current_strip;
      cumulated_max_energy += energy_in_strip;
      if (window_end <= makespan_min) {
        cumulated_max_energy_before_makespan_min += energy_in_strip;
      }

      if (window_start >= makespan_min) {
        DCHECK_EQ(cumulated_max_energy_before_makespan_min, 0);
      }
      DCHECK_LE(cumulated_max_energy, capacity * (window_end - window_start));
      const double max_energy_up_to_makespan_lp =
          strip_start >= makespan_min
              ? ToDouble(cumulated_max_energy_before_makespan_min) +
                    (makespan_lp - makespan_min_lp) * capacity_lp
              : std::numeric_limits<double>::infinity();

      // We prefer using the makespan as the cut will tighten itself when the
      // objective value is improved.
      //
      // We reuse the min cut violation to allow some slack in the comparison
      // between the two computed energy values.
      const bool use_makespan =
          max_energy_up_to_makespan_lp <=
          ToDouble(cumulated_max_energy) + kMinCutViolation;
      const double available_energy_lp = use_makespan
                                             ? max_energy_up_to_makespan_lp
                                             : ToDouble(cumulated_max_energy);
      if (CutIsEfficient(events, window_start, window_end, available_energy_lp,
                         lp_values, &temp_builder)) {
        OverloadedTimeWindowWithMakespan w;
        w.start = window_start;
        w.end = window_end;
        w.fixed_energy_rhs = use_makespan
                                 ? cumulated_max_energy_before_makespan_min
                                 : cumulated_max_energy;
        w.use_makespan = use_makespan;
        w.use_subset_sum =
            use_makespan ? use_subset_sum_before_makespan_min : use_subset_sum;
        overloaded_time_windows.push_back(std::move(w));
      }
    }
  }

  if (overloaded_time_windows.empty()) return;

  VLOG(3) << "GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity: "
          << events.size() << " events, " << time_points.size()
          << " time points, " << overloaded_time_windows.size()
          << " overloads detected";

  TopNCuts top_n_cuts(5);
  for (const auto& w : overloaded_time_windows) {
    bool cut_generated = true;
    bool add_opt_to_name = false;
    bool add_lifted_to_name = false;
    bool add_quadratic_to_name = false;
    bool add_energy_to_name = false;
    LinearConstraintBuilder cut(model, kMinIntegerValue, w.fixed_energy_rhs);

    if (w.use_makespan) {  // Add the energy from makespan_min to makespan.
      cut.AddConstant(makespan_min * capacity);
      cut.AddTerm(makespan, -capacity);
    }

    // Add contributions from all events.
    for (const EnergyEvent& event : events) {
      if (!AddOneEvent(event, w.start, w.end, &cut, &add_energy_to_name,
                       &add_quadratic_to_name, &add_opt_to_name,
                       &add_lifted_to_name)) {
        cut_generated = false;
        break;  // Exit the event loop.
      }
    }

    if (cut_generated) {
      std::string full_name(cut_name);
      if (add_opt_to_name) full_name.append("_optional");
      if (add_quadratic_to_name) full_name.append("_quadratic");
      if (add_lifted_to_name) full_name.append("_lifted");
      if (add_energy_to_name) full_name.append("_energy");
      if (w.use_makespan) full_name.append("_makespan");
      if (w.use_subset_sum) full_name.append("_subsetsum");
      top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
    }
  }

  top_n_cuts.TransferToManager(manager);
}

void GenerateCumulativeEnergeticCuts(
    const std::string& cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    std::vector<EnergyEvent> events, const AffineExpression& capacity,
    TimeLimit* time_limit, Model* model, LinearConstraintManager* manager) {
  double max_possible_energy_lp = 0.0;
  for (const EnergyEvent& event : events) {
    max_possible_energy_lp += event.linearized_energy_lp_value;
  }

  // Currently, we look at all the possible time windows, and will push all cuts
  // in the TopNCuts object. From our observations, this generator creates only
  // a few cuts for a given run.
  //
  // The complexity of this loop is n^3. if we follow the latest research, we
  // could implement this in n log^2(n). Still, this is not visible in the
  // profile as we only this method at the root node,
  struct OverloadedTimeWindow {
    IntegerValue start;
    IntegerValue end;
  };
  std::vector<OverloadedTimeWindow> overloaded_time_windows;
  const double capacity_lp = capacity.LpValue(lp_values);

  // Compute relevant time points.
  // TODO(user): We could reduce this set.
  absl::btree_set<IntegerValue> time_points_set;
  IntegerValue max_end_min = kMinIntegerValue;
  for (const EnergyEvent& event : events) {
    time_points_set.insert(event.x_start_min);
    time_points_set.insert(event.x_start_max);
    time_points_set.insert(event.x_end_min);
    time_points_set.insert(event.x_end_max);
    max_end_min = std::max(max_end_min, event.x_end_min);
  }
  const std::vector<IntegerValue> time_points(time_points_set.begin(),
                                              time_points_set.end());
  const int num_time_points = time_points.size();

  LinearConstraintBuilder temp_builder(model);
  for (int i = 0; i + 1 < num_time_points; ++i) {
    // Checks the time limit if the problem is too big.
    if (events.size() > 50 && time_limit->LimitReached()) return;

    const IntegerValue window_start = time_points[i];
    // After max_end_min, all tasks can fit before window_start.
    if (window_start >= max_end_min) break;

    for (int j = i + 1; j < num_time_points; ++j) {
      const IntegerValue window_end = time_points[j];
      const double available_energy_lp =
          ToDouble(window_end - window_start) * capacity_lp;
      if (available_energy_lp >= max_possible_energy_lp) break;
      if (CutIsEfficient(events, window_start, window_end, available_energy_lp,
                         lp_values, &temp_builder)) {
        overloaded_time_windows.push_back({window_start, window_end});
      }
    }
  }

  if (overloaded_time_windows.empty()) return;

  VLOG(3) << "GenerateCumulativeEnergeticCuts: " << events.size() << " events, "
          << time_points.size() << " time points, "
          << overloaded_time_windows.size() << " overloads detected";

  TopNCuts top_n_cuts(5);
  for (const auto& [window_start, window_end] : overloaded_time_windows) {
    bool cut_generated = true;
    bool add_opt_to_name = false;
    bool add_lifted_to_name = false;
    bool add_quadratic_to_name = false;
    bool add_energy_to_name = false;
    LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));

    // Compute the max energy available for the tasks.
    cut.AddTerm(capacity, window_start - window_end);

    // Add all contributions.
    for (const EnergyEvent& event : events) {
      if (!AddOneEvent(event, window_start, window_end, &cut,
                       &add_energy_to_name, &add_quadratic_to_name,
                       &add_opt_to_name, &add_lifted_to_name)) {
        cut_generated = false;
        break;  // Exit the event loop.
      }
    }

    if (cut_generated) {
      std::string full_name = cut_name;
      if (add_opt_to_name) full_name.append("_optional");
      if (add_quadratic_to_name) full_name.append("_quadratic");
      if (add_lifted_to_name) full_name.append("_lifted");
      if (add_energy_to_name) full_name.append("_energy");
      top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
    }
  }

  top_n_cuts.TransferToManager(manager);
}

CutGenerator CreateCumulativeEnergyCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity,
    const std::optional<AffineExpression>& makespan, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  if (makespan.has_value() && !makespan.value().IsConstant()) {
    result.vars.push_back(makespan.value().var);
  }
  gtl::STLSortAndRemoveDuplicates(&result.vars);
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();

  result.generate_cuts = [makespan, capacity, demands_helper, helper,
                          integer_trail, time_limit,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!demands_helper->CacheAllEnergyValues()) return true;

    const auto& lp_values = manager->LpValues();
    std::vector<EnergyEvent> events;
    for (int i = 0; i < helper->NumTasks(); ++i) {
      if (helper->IsAbsent(i)) continue;
      // TODO(user): use level 0 bounds ?
      if (demands_helper->DemandMax(i) == 0 || helper->SizeMin(i) == 0) {
        continue;
      }

      EnergyEvent e(i, helper);
      e.y_size = demands_helper->Demands()[i];
      e.y_size_min = demands_helper->DemandMin(i);
      e.decomposed_energy = demands_helper->DecomposedEnergies()[i];
      e.energy_min = demands_helper->EnergyMin(i);
      e.energy_is_quadratic = demands_helper->EnergyIsQuadratic(i);
      if (!helper->IsPresent(i)) {
        e.presence_literal_index = helper->PresenceLiteral(i).Index();
      }
      // We can always skip events.
      if (!e.FillEnergyLp(helper->Sizes()[i], lp_values, model)) continue;
      events.push_back(e);
    }

    if (makespan.has_value() && integer_trail->IsFixed(capacity)) {
      GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity(
          "CumulativeEnergyM", lp_values, events,
          integer_trail->FixedValue(capacity), makespan.value(), time_limit,
          model, manager);

    } else {
      GenerateCumulativeEnergeticCuts("CumulativeEnergy", lp_values, events,
                                      capacity, time_limit, model, manager);
    }
    return true;
  };

  return result;
}

CutGenerator CreateNoOverlapEnergyCutGenerator(
    SchedulingConstraintHelper* helper,
    const std::optional<AffineExpression>& makespan, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  if (makespan.has_value() && !makespan.value().IsConstant()) {
    result.vars.push_back(makespan.value().var);
  }
  gtl::STLSortAndRemoveDuplicates(&result.vars);
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();

  result.generate_cuts = [makespan, helper, time_limit,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;

    const auto& lp_values = manager->LpValues();
    std::vector<EnergyEvent> events;
    for (int i = 0; i < helper->NumTasks(); ++i) {
      if (helper->IsAbsent(i)) continue;
      if (helper->SizeMin(i) == 0) continue;

      EnergyEvent e(i, helper);
      e.y_size = IntegerValue(1);
      e.y_size_min = IntegerValue(1);
      e.energy_min = e.x_size_min;
      if (!helper->IsPresent(i)) {
        e.presence_literal_index = helper->PresenceLiteral(i).Index();
      }
      // We can always skip events.
      if (!e.FillEnergyLp(helper->Sizes()[i], lp_values, model)) continue;
      events.push_back(e);
    }

    if (makespan.has_value()) {
      GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity(
          "NoOverlapEnergyM", lp_values, events,
          /*capacity=*/IntegerValue(1), makespan.value(), time_limit, model,
          manager);
    } else {
      GenerateCumulativeEnergeticCuts("NoOverlapEnergy", lp_values, events,
                                      /*capacity=*/IntegerValue(1), time_limit,
                                      model, manager);
    }
    return true;
  };
  return result;
}

CutGenerator CreateCumulativeTimeTableCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  struct TimeTableEvent {
    int interval_index;
    IntegerValue time;
    LinearExpression demand;
    double demand_lp = 0.0;
    bool is_positive = false;
    bool use_energy = false;
    bool is_optional = false;
  };

  result.generate_cuts = [helper, capacity, demands_helper,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!demands_helper->CacheAllEnergyValues()) return true;

    TopNCuts top_n_cuts(5);
    std::vector<TimeTableEvent> events;
    const auto& lp_values = manager->LpValues();
    if (lp_values.empty()) return true;  // No linear relaxation.
    const double capacity_lp = capacity.LpValue(lp_values);

    // Iterate through the intervals. If start_max < end_min, the demand
    // is mandatory.
    for (int i = 0; i < helper->NumTasks(); ++i) {
      if (helper->IsAbsent(i)) continue;

      const IntegerValue start_max = helper->StartMax(i);
      const IntegerValue end_min = helper->EndMin(i);

      if (start_max >= end_min) continue;

      TimeTableEvent e1;
      e1.interval_index = i;
      e1.time = start_max;
      {
        LinearConstraintBuilder builder(model);
        // Ignore the interval if the linearized demand fails.
        if (!demands_helper->AddLinearizedDemand(i, &builder)) continue;
        e1.demand = builder.BuildExpression();
      }
      e1.demand_lp = e1.demand.LpValue(lp_values);
      e1.is_positive = true;
      e1.use_energy = !demands_helper->DecomposedEnergies()[i].empty();
      e1.is_optional = !helper->IsPresent(i);

      TimeTableEvent e2 = e1;
      e2.time = end_min;
      e2.is_positive = false;

      events.push_back(e1);
      events.push_back(e2);
    }

    // Sort events by time.
    // It is also important that all positive event with the same time as
    // negative events appear after for the correctness of the algo below.
    std::sort(events.begin(), events.end(),
              [](const TimeTableEvent& i, const TimeTableEvent& j) {
                if (i.time == j.time) {
                  if (i.is_positive == j.is_positive) {
                    return i.interval_index < j.interval_index;
                  }
                  return !i.is_positive;
                }
                return i.time < j.time;
              });

    double sum_of_demand_lp = 0.0;
    bool positive_event_added_since_last_check = false;
    for (int i = 0; i < events.size(); ++i) {
      const TimeTableEvent& e = events[i];
      if (e.is_positive) {
        positive_event_added_since_last_check = true;
        sum_of_demand_lp += e.demand_lp;
        continue;
      }

      if (positive_event_added_since_last_check) {
        // Reset positive event added. We do not want to create cuts for
        // each negative event in sequence.
        positive_event_added_since_last_check = false;

        if (sum_of_demand_lp >= capacity_lp + kMinCutViolation) {
          // Create cut.
          bool use_energy = false;
          bool use_optional = false;
          LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));
          cut.AddTerm(capacity, IntegerValue(-1));
          // The i-th event, which is a negative event, follows a positive
          // event. We must ignore it in our cut generation.
          DCHECK(!events[i].is_positive);
          const IntegerValue time_point = events[i - 1].time;

          for (int j = 0; j < i; ++j) {
            const TimeTableEvent& cut_event = events[j];
            const int t = cut_event.interval_index;
            DCHECK_LE(helper->StartMax(t), time_point);
            if (!cut_event.is_positive || helper->EndMin(t) <= time_point) {
              continue;
            }

            cut.AddLinearExpression(cut_event.demand, IntegerValue(1));
            use_energy |= cut_event.use_energy;
            use_optional |= cut_event.is_optional;
          }

          std::string cut_name = "CumulativeTimeTable";
          if (use_optional) cut_name += "_optional";
          if (use_energy) cut_name += "_energy";
          top_n_cuts.AddCut(cut.Build(), cut_name, lp_values);
        }
      }

      // The demand_lp was added in case of a positive event. We need to
      // remove it for a negative event.
      sum_of_demand_lp -= e.demand_lp;
    }
    top_n_cuts.TransferToManager(manager);
    return true;
  };
  return result;
}

// Cached Information about one interval.
// Note that everything must correspond to level zero bounds, otherwise the
// generated cut are not valid.

struct CachedIntervalData {
  CachedIntervalData(int t, SchedulingConstraintHelper* helper)
      : start_min(helper->StartMin(t)),
        start_max(helper->StartMax(t)),
        start(helper->Starts()[t]),
        end_min(helper->EndMin(t)),
        end_max(helper->EndMax(t)),
        end(helper->Ends()[t]),
        size_min(helper->SizeMin(t)) {}

  IntegerValue start_min;
  IntegerValue start_max;
  AffineExpression start;
  IntegerValue end_min;
  IntegerValue end_max;
  AffineExpression end;
  IntegerValue size_min;

  IntegerValue demand_min;
};

void GenerateCutsBetweenPairOfNonOverlappingTasks(
    absl::string_view cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    std::vector<CachedIntervalData> events, IntegerValue capacity_max,
    Model* model, LinearConstraintManager* manager) {
  TopNCuts top_n_cuts(5);
  const int num_events = events.size();
  if (num_events <= 1) return;

  std::sort(events.begin(), events.end(),
            [](const CachedIntervalData& e1, const CachedIntervalData& e2) {
              return e1.start_min < e2.start_min ||
                     (e1.start_min == e2.start_min && e1.end_max < e2.end_max);
            });

  // Balas disjunctive cuts on 2 tasks a and b:
  //   start_1 * (duration_1 + start_min_1 - start_min_2) +
  //   start_2 * (duration_2 + start_min_2 - start_min_1) >=
  //       duration_1 * duration_2 +
  //       start_min_1 * duration_2 +
  //       start_min_2 * duration_1
  // From: David L. Applegate, William J. Cook:
  //   A Computational Study of the Job-Shop Scheduling Problem. 149-156
  //   INFORMS Journal on Computing, Volume 3, Number 1, Winter 1991
  const auto add_balas_disjunctive_cut =
      [&](absl::string_view local_cut_name, IntegerValue start_min_1,
          IntegerValue duration_min_1, AffineExpression start_1,
          IntegerValue start_min_2, IntegerValue duration_min_2,
          AffineExpression start_2) {
        // Checks hypothesis from the cut.
        if (start_min_2 >= start_min_1 + duration_min_1 ||
            start_min_1 >= start_min_2 + duration_min_2) {
          return;
        }
        const IntegerValue coeff_1 = duration_min_1 + start_min_1 - start_min_2;
        const IntegerValue coeff_2 = duration_min_2 + start_min_2 - start_min_1;
        const IntegerValue rhs = duration_min_1 * duration_min_2 +
                                 duration_min_1 * start_min_2 +
                                 duration_min_2 * start_min_1;

        if (ToDouble(coeff_1) * start_1.LpValue(lp_values) +
                ToDouble(coeff_2) * start_2.LpValue(lp_values) <=
            ToDouble(rhs) - kMinCutViolation) {
          LinearConstraintBuilder cut(model, rhs, kMaxIntegerValue);
          cut.AddTerm(start_1, coeff_1);
          cut.AddTerm(start_2, coeff_2);
          top_n_cuts.AddCut(cut.Build(), local_cut_name, lp_values);
        }
      };

  for (int i = 0; i + 1 < num_events; ++i) {
    const CachedIntervalData& e1 = events[i];
    for (int j = i + 1; j < num_events; ++j) {
      const CachedIntervalData& e2 = events[j];
      if (e2.start_min >= e1.end_max) break;  // Break out of the index2 loop.

      // Encode only the interesting pairs.
      if (e1.demand_min + e2.demand_min <= capacity_max) continue;

      const bool interval_1_can_precede_2 = e1.end_min <= e2.start_max;
      const bool interval_2_can_precede_1 = e2.end_min <= e1.start_max;

      if (interval_1_can_precede_2 && !interval_2_can_precede_1 &&
          e1.end.LpValue(lp_values) >=
              e2.start.LpValue(lp_values) + kMinCutViolation) {
        // interval_1.end <= interval_2.start
        LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));
        cut.AddTerm(e1.end, IntegerValue(1));
        cut.AddTerm(e2.start, IntegerValue(-1));
        top_n_cuts.AddCut(cut.Build(),
                          absl::StrCat(cut_name, "DetectedPrecedence"),
                          lp_values);
      } else if (interval_2_can_precede_1 && !interval_1_can_precede_2 &&
                 e2.end.LpValue(lp_values) >=
                     e1.start.LpValue(lp_values) + kMinCutViolation) {
        // interval_2.end <= interval_1.start
        LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));
        cut.AddTerm(e2.end, IntegerValue(1));
        cut.AddTerm(e1.start, IntegerValue(-1));
        top_n_cuts.AddCut(cut.Build(),
                          absl::StrCat(cut_name, "DetectedPrecedence"),
                          lp_values);
      } else {
        add_balas_disjunctive_cut(absl::StrCat(cut_name, "DisjunctionOnStart"),
                                  e1.start_min, e1.size_min, e1.start,
                                  e2.start_min, e2.size_min, e2.start);
        add_balas_disjunctive_cut(absl::StrCat(cut_name, "DisjunctionOnEnd"),
                                  -e1.end_max, e1.size_min, e1.end.Negated(),
                                  -e2.end_max, e2.size_min, e2.end.Negated());
      }
    }
  }

  top_n_cuts.TransferToManager(manager);
}

CutGenerator CreateCumulativePrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  result.generate_cuts = [integer_trail, helper, demands_helper, capacity,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;

    std::vector<CachedIntervalData> events;
    for (int t = 0; t < helper->NumTasks(); ++t) {
      if (!helper->IsPresent(t)) continue;
      CachedIntervalData event(t, helper);
      event.demand_min = demands_helper->DemandMin(t);
      events.push_back(event);
    }

    const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
    GenerateCutsBetweenPairOfNonOverlappingTasks(
        "Cumulative", manager->LpValues(), std::move(events), capacity_max,
        model, manager);
    return true;
  };
  return result;
}

CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  result.generate_cuts = [helper, model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;

    std::vector<CachedIntervalData> events;
    for (int t = 0; t < helper->NumTasks(); ++t) {
      if (!helper->IsPresent(t)) continue;
      CachedIntervalData event(t, helper);
      event.demand_min = IntegerValue(1);
      events.push_back(event);
    }

    GenerateCutsBetweenPairOfNonOverlappingTasks(
        "NoOverlap", manager->LpValues(), std::move(events), IntegerValue(1),
        model, manager);
    return true;
  };

  return result;
}

CtEvent::CtEvent(int t, SchedulingConstraintHelper* x_helper)
    : BaseEvent(t, x_helper) {}

std::string CtEvent::DebugString() const {
  return absl::StrCat("CtEvent(x_end = ", x_end.DebugString(),
                      ", x_start_min = ", x_start_min.value(),
                      ", x_start_max = ", x_start_max.value(),
                      ", x_size_min = ", x_size_min.value(),
                      ", x_lp_end = ", x_lp_end,
                      ", y_size_min = ", y_size_min.value(),
                      ", energy_min = ", energy_min.value(),
                      ", use_energy = ", use_energy, ", lifted = ", lifted);
}

namespace {

// This functions packs all events in a cumulative of capacity 'capacity_max'
// following the given permutation. It returns the sum of end mins and the sum
// of end mins weighted by event.weight.
//
// It ensures that if event_j is after event_i in the permutation, then event_j
// starts exactly at the same time or after event_i.
//
// It returns false if one event cannot start before event.start_max.
bool ComputeWeightedSumOfEndMinsForOnePermutation(
    absl::Span<const PermutableEvent> events, IntegerValue capacity_max,
    IntegerValue& sum_of_ends, IntegerValue& sum_of_weighted_ends,
    std::vector<std::pair<IntegerValue, IntegerValue>>& profile,
    std::vector<std::pair<IntegerValue, IntegerValue>>& new_profile) {
  sum_of_ends = 0;
  sum_of_weighted_ends = 0;

  // The profile (and new profile) is a set of (time, capa_left) pairs, ordered
  // by increasing time and capa_left.
  profile.clear();
  profile.emplace_back(kMinIntegerValue, capacity_max);
  profile.emplace_back(kMaxIntegerValue, capacity_max);
  IntegerValue start_of_previous_task = kMinIntegerValue;
  for (const PermutableEvent& event : events) {
    const IntegerValue start_min =
        std::max(event.start_min, start_of_previous_task);

    // Iterate on the profile to find the step that contains start_min.
    // Then push until we find a step with enough capacity.
    int current = 0;
    while (profile[current + 1].first <= start_min ||
           profile[current].second < event.demand) {
      ++current;
    }

    const IntegerValue actual_start =
        std::max(start_min, profile[current].first);
    start_of_previous_task = actual_start;

    // Compatible with the event.start_max ?
    if (actual_start > event.start_max) return false;

    const IntegerValue actual_end = actual_start + event.size;
    sum_of_ends += actual_end;
    sum_of_weighted_ends += event.weight * actual_end;

    // No need to update the profile on the last loop.
    if (&event == &events.back()) break;

    // Update the profile.
    new_profile.clear();
    new_profile.push_back(
        {actual_start, profile[current].second - event.demand});
    ++current;

    while (profile[current].first < actual_end) {
      new_profile.push_back(
          {profile[current].first, profile[current].second - event.demand});
      ++current;
    }

    if (profile[current].first > actual_end) {
      new_profile.push_back(
          {actual_end, new_profile.back().second + event.demand});
    }
    while (current < profile.size()) {
      new_profile.push_back(profile[current]);
      ++current;
    }
    profile.swap(new_profile);
  }
  return true;
}

}  // namespace

bool ComputeMinSumOfWeightedEndMins(std::vector<PermutableEvent>& events,
                                    IntegerValue capacity_max,
                                    IntegerValue& min_sum_of_end_mins,
                                    IntegerValue& min_sum_of_weighted_end_mins,
                                    IntegerValue unweighted_threshold,
                                    IntegerValue weighted_threshold) {
  int num_explored = 0;
  int num_pruned = 0;
  min_sum_of_end_mins = kMaxIntegerValue;
  min_sum_of_weighted_end_mins = kMaxIntegerValue;

  // Reusable storage for ComputeWeightedSumOfEndMinsForOnePermutation().
  std::vector<std::pair<IntegerValue, IntegerValue>> profile;
  std::vector<std::pair<IntegerValue, IntegerValue>> new_profile;
  do {
    IntegerValue sum_of_ends(0);
    IntegerValue sum_of_weighted_ends(0);
    if (ComputeWeightedSumOfEndMinsForOnePermutation(
            events, capacity_max, sum_of_ends, sum_of_weighted_ends, profile,
            new_profile)) {
      min_sum_of_end_mins = std::min(sum_of_ends, min_sum_of_end_mins);
      min_sum_of_weighted_end_mins =
          std::min(sum_of_weighted_ends, min_sum_of_weighted_end_mins);
      num_explored++;
      if (min_sum_of_end_mins <= unweighted_threshold &&
          min_sum_of_weighted_end_mins <= weighted_threshold) {
        break;
      }
    } else {
      num_pruned++;
    }
  } while (std::next_permutation(events.begin(), events.end()));
  VLOG(3) << "DP: size=" << events.size() << ", explored = " << num_explored
          << ", pruned = " << num_pruned
          << ", min_sum_of_end_mins = " << min_sum_of_end_mins
          << ", min_sum_of_weighted_end_mins = "
          << min_sum_of_weighted_end_mins;
  return num_explored > 0;
}

// TODO(user): Improve performance
//   - detect disjoint tasks (no need to crossover to the second part)
//   - better caching of explored states
void GenerateShortCompletionTimeCutsWithExactBound(
    const std::string& cut_name, std::vector<CtEvent> events,
    IntegerValue capacity_max, Model* model, LinearConstraintManager* manager) {
  TopNCuts top_n_cuts(5);
  // Sort by start min to bucketize by start_min.
  std::sort(events.begin(), events.end(),
            [](const CtEvent& e1, const CtEvent& e2) {
              return std::tie(e1.x_start_min, e1.y_size_min, e1.x_lp_end) <
                     std::tie(e2.x_start_min, e2.y_size_min, e2.x_lp_end);
            });
  std::vector<PermutableEvent> permutable_events;
  for (int start = 0; start + 1 < events.size(); ++start) {
    // Skip to the next start_min value.
    if (start > 0 &&
        events[start].x_start_min == events[start - 1].x_start_min) {
      continue;
    }

    const IntegerValue sequence_start_min = events[start].x_start_min;
    std::vector<CtEvent> residual_tasks(events.begin() + start, events.end());

    // We look at event that start before sequence_start_min, but are forced
    // to cross this time point. In that case, we replace this event by a
    // truncated event starting at sequence_start_min. To do this, we reduce
    // the size_min, and align the start_min with the sequence_start_min.
    for (int before = 0; before < start; ++before) {
      if (events[before].x_start_min + events[before].x_size_min >
          sequence_start_min) {
        residual_tasks.push_back(events[before]);  // Copy.
        residual_tasks.back().lifted = true;
      }
    }

    std::sort(residual_tasks.begin(), residual_tasks.end(),
              [](const CtEvent& e1, const CtEvent& e2) {
                return e1.x_lp_end < e2.x_lp_end;
              });

    IntegerValue sum_of_durations(0);
    IntegerValue sum_of_energies(0);
    double sum_of_ends_lp = 0.0;
    double sum_of_weighted_ends_lp = 0.0;
    IntegerValue sum_of_demands(0);

    permutable_events.clear();
    for (int i = 0; i < std::min<int>(residual_tasks.size(), 7); ++i) {
      const CtEvent& event = residual_tasks[i];
      permutable_events.emplace_back(i, event);
      sum_of_ends_lp += event.x_lp_end;
      sum_of_weighted_ends_lp += event.x_lp_end * ToDouble(event.y_size_min);
      sum_of_demands += event.y_size_min;
      sum_of_durations += event.x_size_min;
      sum_of_energies += event.x_size_min * event.y_size_min;

      // Both cases with 1 or 2 tasks are trivial and independent of the order.
      // Also, if capacity is not exceeded, pushing all ends left is a valid LP
      // assignment.
      if (i <= 1 || sum_of_demands <= capacity_max) continue;

      IntegerValue min_sum_of_end_mins = kMaxIntegerValue;
      IntegerValue min_sum_of_weighted_end_mins = kMaxIntegerValue;
      for (int j = 0; j <= i; ++j) {
        // We re-index the elements, so we will start enumerating the
        // permutation from there. Note that if the previous i caused an abort
        // because of the threshold, we might abort right away again!
        permutable_events[j].index = j;
      }
      if (!ComputeMinSumOfWeightedEndMins(
              permutable_events, capacity_max, min_sum_of_end_mins,
              min_sum_of_weighted_end_mins,
              /*unweighted_threshold=*/
              std::floor(sum_of_ends_lp + kMinCutViolation),
              /*weighted_threshold=*/
              std::floor(sum_of_weighted_ends_lp + kMinCutViolation))) {
        break;
      }

      const double unweigthed_violation =
          (ToDouble(min_sum_of_end_mins) - sum_of_ends_lp) /
          ToDouble(sum_of_durations);
      const double weighted_violation =
          (ToDouble(min_sum_of_weighted_end_mins) - sum_of_weighted_ends_lp) /
          ToDouble(sum_of_energies);

      // Unweighted cuts.
      if (unweigthed_violation > weighted_violation &&
          unweigthed_violation > kMinCutViolation) {
        LinearConstraintBuilder cut(model, min_sum_of_end_mins,
                                    kMaxIntegerValue);
        bool is_lifted = false;
        for (int j = 0; j <= i; ++j) {
          const CtEvent& event = residual_tasks[j];
          is_lifted |= event.lifted;
          cut.AddTerm(event.x_end, IntegerValue(1));
        }
        std::string full_name = cut_name;
        top_n_cuts.AddCut(cut.Build(), full_name, manager->LpValues());
      }

      // Weighted cuts.
      if (weighted_violation >= unweigthed_violation &&
          weighted_violation > kMinCutViolation) {
        LinearConstraintBuilder cut(model, min_sum_of_weighted_end_mins,
                                    kMaxIntegerValue);
        bool is_lifted = false;
        for (int j = 0; j <= i; ++j) {
          const CtEvent& event = residual_tasks[j];
          is_lifted |= event.lifted;
          cut.AddTerm(event.x_end, event.y_size_min);
        }
        std::string full_name = cut_name + "_weighted";
        if (is_lifted) full_name.append("_lifted");
        top_n_cuts.AddCut(cut.Build(), full_name, manager->LpValues());
      }
    }
  }
  top_n_cuts.TransferToManager(manager);
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
void GenerateCompletionTimeCutsWithEnergy(absl::string_view cut_name,
                                          std::vector<CtEvent> events,
                                          IntegerValue capacity_max,
                                          bool skip_low_sizes, Model* model,
                                          LinearConstraintManager* manager) {
  TopNCuts top_n_cuts(5);

  // Sort by start min to bucketize by start_min.
  std::sort(events.begin(), events.end(),
            [](const CtEvent& e1, const CtEvent& e2) {
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
    std::vector<CtEvent> residual_tasks(events.begin() + start, events.end());
    const VariablesAssignment& assignment =
        model->GetOrCreate<Trail>()->Assignment();

    // We look at event that start before sequence_start_min, but are forced
    // to cross this time point. In that case, we replace this event by a
    // truncated event starting at sequence_start_min. To do this, we reduce
    // the size_min, align the start_min with the sequence_start_min, and
    // scale the energy down accordingly.
    for (int before = 0; before < start; ++before) {
      if (events[before].x_start_min + events[before].x_size_min >
          sequence_start_min) {
        // Build the vector of energies as the vector of sizes.
        CtEvent event = events[before];  // Copy.
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

    std::sort(residual_tasks.begin(), residual_tasks.end(),
              [](const CtEvent& e1, const CtEvent& e2) {
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

    MaxBoundedSubsetSum dp(capacity_max.value());
    std::vector<int64_t> possible_demands;
    for (int i = 0; i < residual_tasks.size(); ++i) {
      const CtEvent& event = residual_tasks[i];
      DCHECK_GE(event.x_start_min, sequence_start_min);
      const IntegerValue energy = event.energy_min;
      sum_duration += energy;
      if (!AddProductTo(energy, energy, &sum_square_duration)) break;

      unscaled_lp_contrib += event.x_lp_end * ToDouble(energy);
      current_start_min = std::min(current_start_min, event.x_start_min);

      if (dp.CurrentMax() != capacity_max) {
        if (event.y_size_is_fixed) {
          dp.Add(event.y_size_min.value());
        } else if (!event.decomposed_energy.empty()) {
          possible_demands.clear();
          for (const auto& [literal, size, demand] : event.decomposed_energy) {
            if (assignment.LiteralIsFalse(literal)) continue;
            possible_demands.push_back(demand.value());
          }
          dp.AddChoices(possible_demands);
        } else {
          dp.Add(capacity_max.value());
        }
      }

      // This is competing with the brute force approach. Skip cases covered
      // by the other code.
      if (skip_low_sizes && i < 7) continue;

      // We compute the cuts like if it was a disjunctive cut with all the
      // duration actually equal to energy / capacity. But to keep the
      // computation in the integer domain, we multiply by capacity
      // everywhere instead.
      const IntegerValue reachable_capacity = dp.CurrentMax();
      IntegerValue min_contrib = 0;
      if (!AddProductTo(sum_duration, sum_duration, &min_contrib)) break;
      if (!AddTo(sum_square_duration, &min_contrib)) break;
      min_contrib = min_contrib / 2;  // The above is the double of the area.

      const IntegerValue intermediate =
          CapProdI(sum_duration, reachable_capacity);
      if (AtMinOrMaxInt64I(intermediate)) break;
      const IntegerValue offset = CapProdI(current_start_min, intermediate);
      if (AtMinOrMaxInt64I(offset)) break;
      if (!AddTo(offset, &min_contrib)) break;

      // We compute the efficacity in the unscaled domain where the l2 norm of
      // the cuts is exactly the sqrt of  the sum of squared duration.
      const double efficacy =
          (ToDouble(min_contrib) / ToDouble(reachable_capacity) -
           unscaled_lp_contrib) /
          std::sqrt(ToDouble(sum_square_duration));

      // TODO(user): Check overflow and ignore if too big.
      if (efficacy > best_efficacy) {
        best_efficacy = efficacy;
        best_end = i;
        best_min_contrib = min_contrib;
        best_capacity = reachable_capacity;
      }
    }
    if (best_end != -1) {
      LinearConstraintBuilder cut(model, best_min_contrib, kMaxIntegerValue);
      bool is_lifted = false;
      bool add_energy_to_name = false;
      for (int i = 0; i <= best_end; ++i) {
        const CtEvent& event = residual_tasks[i];
        is_lifted |= event.lifted;
        add_energy_to_name |= event.use_energy;
        cut.AddTerm(event.x_end, event.energy_min * best_capacity);
      }
      std::string full_name(cut_name);
      if (is_lifted) full_name.append("_lifted");
      if (add_energy_to_name) full_name.append("_energy");
      if (best_capacity < capacity_max) {
        full_name.append("_subsetsum");
        VLOG(2) << full_name << ": capacity = " << best_capacity << "/"
                << capacity_max;
      }
      top_n_cuts.AddCut(cut.Build(), full_name, manager->LpValues());
    }
  }
  top_n_cuts.TransferToManager(manager);
}

CutGenerator CreateNoOverlapCompletionTimeCutGenerator(
    SchedulingConstraintHelper* helper, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  result.generate_cuts = [helper, model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;

    auto generate_cuts = [model, manager, helper](bool mirror) {
      std::vector<CtEvent> events;
      const auto& lp_values = manager->LpValues();
      for (int index = 0; index < helper->NumTasks(); ++index) {
        if (!helper->IsPresent(index)) continue;
        const IntegerValue size_min = helper->SizeMin(index);
        if (size_min > 0) {
          const AffineExpression end_expr = helper->Ends()[index];
          CtEvent event(index, helper);
          event.x_end = end_expr;
          event.x_lp_end = end_expr.LpValue(lp_values);
          event.y_size_min = IntegerValue(1);
          event.energy_min = size_min;
          events.push_back(event);
        }
      }

      const std::string mirror_str = mirror ? "Mirror" : "";
      GenerateShortCompletionTimeCutsWithExactBound(
          absl::StrCat("NoOverlapCompletionTimeExhaustive", mirror_str), events,
          /*capacity_max=*/IntegerValue(1), model, manager);

      GenerateCompletionTimeCutsWithEnergy(
          absl::StrCat("NoOverlapCompletionTimeQueyrane", mirror_str),
          std::move(events), /*capacity_max=*/IntegerValue(1),
          /*skip_low_sizes=*/true, model, manager);
    };
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    generate_cuts(false);
    if (!helper->SynchronizeAndSetTimeDirection(false)) return false;
    generate_cuts(true);
    return true;
  };
  return result;
}

CutGenerator CreateCumulativeCompletionTimeCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  result.generate_cuts = [integer_trail, helper, demands_helper, capacity,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!demands_helper->CacheAllEnergyValues()) return true;

    auto generate_cuts = [integer_trail, model, manager, helper, demands_helper,
                          capacity](bool mirror) {
      std::vector<CtEvent> events;
      const auto& lp_values = manager->LpValues();
      for (int index = 0; index < helper->NumTasks(); ++index) {
        if (!helper->IsPresent(index)) continue;
        if (helper->SizeMin(index) > 0 &&
            demands_helper->DemandMin(index) > 0) {
          CtEvent event(index, helper);
          event.x_end = helper->Ends()[index];
          event.x_lp_end = event.x_end.LpValue(lp_values);
          event.y_size_min = demands_helper->DemandMin(index);
          event.energy_min = demands_helper->EnergyMin(index);
          event.decomposed_energy = demands_helper->DecomposedEnergies()[index];
          event.y_size_is_fixed = demands_helper->DemandIsFixed(index);
          events.push_back(event);
        }
      }

      const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
      const std::string mirror_str = mirror ? "Mirror" : "";
      GenerateShortCompletionTimeCutsWithExactBound(
          absl::StrCat("CumulativeCompletionTimeExhaustive", mirror_str),
          events, capacity_max, model, manager);

      GenerateCompletionTimeCutsWithEnergy(
          absl::StrCat("CumulativeCompletionTimeQueyrane", mirror_str),
          std::move(events), capacity_max,
          /*skip_low_sizes=*/true, model, manager);
    };
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    generate_cuts(false);
    if (!helper->SynchronizeAndSetTimeDirection(false)) return false;
    generate_cuts(true);
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research
