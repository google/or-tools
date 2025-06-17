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

#include "ortools/sat/scheduling_cuts.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
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
#include "absl/strings/str_join.h"
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
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
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

// Checks that the literals of the decomposed energy (if present) and the size
// and demand of a cumulative task are in sync.
// In some rare instances, this is not the case. In that case, it is better not
// to try to generate cuts.
bool DecomposedEnergyIsPropagated(const VariablesAssignment& assignment, int t,
                                  SchedulingConstraintHelper* helper,
                                  SchedulingDemandHelper* demands_helper) {
  const std::vector<LiteralValueValue>& decomposed_energy =
      demands_helper->DecomposedEnergies()[t];
  if (decomposed_energy.empty()) return true;

  // Checks the propagation of the exactly_one constraint on literals.
  int num_false_literals = 0;
  int num_true_literals = 0;
  for (const auto [lit, fixed_size, fixed_demand] : decomposed_energy) {
    if (assignment.LiteralIsFalse(lit)) ++num_false_literals;
    if (assignment.LiteralIsTrue(lit)) ++num_true_literals;
  }
  if (num_true_literals > 1) return false;
  if (num_true_literals == 1 &&
      num_false_literals < decomposed_energy.size() - 1) {
    return false;
  }
  if (num_false_literals == decomposed_energy.size()) return false;
  if (decomposed_energy.size() == 1 && num_true_literals != 1) return false;

  // Checks the propagations of the bounds of the size and the demand.
  IntegerValue propagated_size_min = kMaxIntegerValue;
  IntegerValue propagated_size_max = kMinIntegerValue;
  IntegerValue propagated_demand_min = kMaxIntegerValue;
  IntegerValue propagated_demand_max = kMinIntegerValue;
  for (const auto [lit, fixed_size, fixed_demand] : decomposed_energy) {
    if (assignment.LiteralIsFalse(lit)) continue;

    if (assignment.LiteralIsTrue(lit)) {
      if (fixed_size != helper->SizeMin(t) ||
          fixed_size != helper->SizeMax(t) ||
          fixed_demand != demands_helper->DemandMin(t) ||
          fixed_demand != demands_helper->DemandMax(t)) {
        return false;
      }
      return true;
    }

    if (fixed_size < helper->SizeMin(t) || fixed_size > helper->SizeMax(t) ||
        fixed_demand < demands_helper->DemandMin(t) ||
        fixed_demand > demands_helper->DemandMax(t)) {
      return false;
    }
    propagated_size_min = std::min(propagated_size_min, fixed_size);
    propagated_size_max = std::max(propagated_size_max, fixed_size);
    propagated_demand_min = std::min(propagated_demand_min, fixed_demand);
    propagated_demand_max = std::max(propagated_demand_max, fixed_demand);
  }

  if (propagated_size_min != helper->SizeMin(t) ||
      propagated_size_max != helper->SizeMax(t) ||
      propagated_demand_min != demands_helper->DemandMin(t) ||
      propagated_demand_max != demands_helper->DemandMax(t)) {
    return false;
  }

  return true;
}

}  // namespace

struct EnergyEvent {
  EnergyEvent(int t, SchedulingConstraintHelper* x_helper)
      : start_min(x_helper->StartMin(t)),
        start_max(x_helper->StartMax(t)),
        end_min(x_helper->EndMin(t)),
        end_max(x_helper->EndMax(t)),
        size_min(x_helper->SizeMin(t)) {}

  // Cache of the bounds of the interval
  IntegerValue start_min;
  IntegerValue start_max;
  IntegerValue end_min;
  IntegerValue end_max;
  IntegerValue size_min;

  // Cache of the bounds of the demand.
  IntegerValue demand_min;

  // The energy min of this event.
  IntegerValue energy_min;

  // If non empty, a decomposed view of the energy of this event.
  // First value in each pair is size, second is demand.
  std::vector<LiteralValueValue> decomposed_energy;
  bool use_decomposed_energy = false;

  // We need this for linearizing the energy in some cases.
  AffineExpression demand;

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
    return std::max(
        std::min({end_min - start, end - start_max, size_min, end - start}),
        IntegerValue(0));
  }

  // This method expects all the other fields to have been filled before.
  // It must be called before the EnergyEvent is used.
  ABSL_MUST_USE_RESULT bool FillEnergyLp(
      AffineExpression size,
      const util_intops::StrongVector<IntegerVariable, double>& lp_values,
      Model* model) {
    LinearConstraintBuilder tmp_energy(model);
    if (IsPresent()) {
      if (!decomposed_energy.empty()) {
        if (!tmp_energy.AddDecomposedProduct(decomposed_energy)) return false;
      } else {
        tmp_energy.AddQuadraticLowerBound(size, demand,
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
        "EnergyEvent(start_min = ", start_min, ", start_max = ", start_max,
        ", end_min = ", end_min, ", end_max = ", end_max,
        ", demand = ", demand.DebugString(), ", energy = ",
        decomposed_energy.empty()
            ? "{}"
            : absl::StrCat(decomposed_energy.size(), " terms"),
        ", presence_literal_index = ", presence_literal_index, ")");
  }
};

namespace {

// Compute the energetic contribution of a task in a given time window, and
// add it to the cut. It returns false if it tried to generate the cut, and
// failed.
ABSL_MUST_USE_RESULT bool AddOneEvent(
    const EnergyEvent& event, IntegerValue window_start,
    IntegerValue window_end, LinearConstraintBuilder& cut,
    bool* add_energy_to_name = nullptr, bool* add_quadratic_to_name = nullptr,
    bool* add_opt_to_name = nullptr, bool* add_lifted_to_name = nullptr) {
  if (event.end_min <= window_start || event.start_max >= window_end) {
    return true;  // Event can move outside the time window.
  }

  if (event.start_min >= window_start && event.end_max <= window_end) {
    // Event is always contained by the time window.
    cut.AddLinearExpression(event.linearized_energy);

    if (event.energy_is_quadratic && add_quadratic_to_name != nullptr) {
      *add_quadratic_to_name = true;
    }
    if (add_energy_to_name != nullptr &&
        event.energy_min > event.size_min * event.demand_min) {
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
        cut.AddTerm(event.demand, min_overlap);
      } else {
        const IntegerValue window_size = window_end - window_start;
        for (const auto [lit, fixed_size, fixed_demand] : energy) {
          const IntegerValue alt_end_min =
              std::max(event.end_min, event.start_min + fixed_size);
          const IntegerValue alt_start_max =
              std::min(event.start_max, event.end_max - fixed_size);
          const IntegerValue energy_min =
              fixed_demand *
              std::min({alt_end_min - window_start, window_end - alt_start_max,
                        fixed_size, window_size});
          if (energy_min == 0) continue;
          if (!cut.AddLiteralTerm(lit, energy_min)) return false;
        }
        if (add_energy_to_name != nullptr) *add_energy_to_name = true;
      }
    } else {
      if (add_opt_to_name != nullptr) *add_opt_to_name = true;
      const IntegerValue min_energy = ComputeEnergyMinInWindow(
          event.start_min, event.start_max, event.end_min, event.end_max,
          event.size_min, event.demand_min, event.decomposed_energy,
          window_start, window_end);
      if (min_energy > event.size_min * event.demand_min &&
          add_energy_to_name != nullptr) {
        *add_energy_to_name = true;
      }
      if (!cut.AddLiteralTerm(Literal(event.presence_literal_index),
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
    if (integer_trail->IsFixed(event.demand)) {
      possible_demands.push_back(
          integer_trail->FixedValue(event.demand).value());
    } else {
      if (integer_trail->InitialVariableDomain(event.demand.var).Size() >
          1000000) {
        return {};
      }
      for (const int64_t var_value :
           integer_trail->InitialVariableDomain(event.demand.var).Values()) {
        possible_demands.push_back(event.demand.ValueAt(var_value).value());
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
    absl::Span<EnergyEvent> events, IntegerValue capacity,
    AffineExpression makespan, TimeLimit* time_limit, Model* model,
    TopNCuts& top_n_cuts) {
  // Checks the precondition of the code.
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  DCHECK(integer_trail->IsFixed(capacity));

  const VariablesAssignment& assignment =
      model->GetOrCreate<Trail>()->Assignment();

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
    if (event.start_min < makespan_min) {
      time_points.push_back(event.start_min);
    }
    if (event.start_max < makespan_min) {
      time_points.push_back(event.start_max);
    }
    if (event.end_min < makespan_min) {
      time_points.push_back(event.end_min);
    }
    if (event.end_max < makespan_min) {
      time_points.push_back(event.end_max);
    }
    max_end_min = std::max(max_end_min, event.end_min);
    max_end_max = std::max(max_end_max, event.end_max);
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
      if (event.start_min >= window_end || event.end_max <= window_start) {
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
      bool cut_generated = true;
      bool add_opt_to_name = false;
      bool add_lifted_to_name = false;
      bool add_quadratic_to_name = false;
      bool add_energy_to_name = false;

      temp_builder.Clear();
      const bool use_makespan =
          max_energy_up_to_makespan_lp <=
          ToDouble(cumulated_max_energy) + kMinCutViolation;
      const bool use_subset_sum_in_cut =
          use_makespan ? use_subset_sum_before_makespan_min : use_subset_sum;

      if (use_makespan) {  // Add the energy from makespan_min to makespan.
        temp_builder.AddConstant(makespan_min * capacity);
        temp_builder.AddTerm(makespan, -capacity);
      }

      // Add contributions from all events.
      for (const EnergyEvent& event : events) {
        if (!AddOneEvent(event, window_start, window_end, temp_builder,
                         &add_energy_to_name, &add_quadratic_to_name,
                         &add_opt_to_name, &add_lifted_to_name)) {
          cut_generated = false;
          break;  // Exit the event loop.
        }
      }

      // We can break here are any further iteration on j will hit the same
      // issue.
      if (!cut_generated) break;

      // Build the cut to evaluate its efficacy.
      const IntegerValue energy_rhs =
          use_makespan ? cumulated_max_energy_before_makespan_min
                       : cumulated_max_energy;
      LinearConstraint potential_cut =
          temp_builder.BuildConstraint(kMinIntegerValue, energy_rhs);

      if (potential_cut.NormalizedViolation(lp_values) >= kMinCutViolation) {
        std::string full_name(cut_name);
        if (add_energy_to_name) full_name.append("_energy");
        if (add_lifted_to_name) full_name.append("_lifted");
        if (use_makespan) full_name.append("_makespan");
        if (add_opt_to_name) full_name.append("_optional");
        if (add_quadratic_to_name) full_name.append("_quadratic");
        if (use_subset_sum_in_cut) full_name.append("_subsetsum");
        top_n_cuts.AddCut(std::move(potential_cut), full_name, lp_values);
      }
    }
  }
}

void GenerateCumulativeEnergeticCuts(
    absl::string_view cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    absl::Span<EnergyEvent> events, const AffineExpression& capacity,
    TimeLimit* time_limit, Model* model, TopNCuts& top_n_cuts) {
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
  const double capacity_lp = capacity.LpValue(lp_values);

  // Compute relevant time points.
  // TODO(user): We could reduce this set.
  absl::btree_set<IntegerValue> time_points_set;
  IntegerValue max_end_min = kMinIntegerValue;
  for (const EnergyEvent& event : events) {
    time_points_set.insert(event.start_min);
    time_points_set.insert(event.start_max);
    time_points_set.insert(event.end_min);
    time_points_set.insert(event.end_max);
    max_end_min = std::max(max_end_min, event.end_min);
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

      bool cut_generated = true;
      bool add_opt_to_name = false;
      bool add_lifted_to_name = false;
      bool add_quadratic_to_name = false;
      bool add_energy_to_name = false;
      temp_builder.Clear();

      // Compute the max energy available for the tasks.
      temp_builder.AddTerm(capacity, window_start - window_end);

      // Add all contributions.
      for (const EnergyEvent& event : events) {
        if (!AddOneEvent(event, window_start, window_end, temp_builder,
                         &add_energy_to_name, &add_quadratic_to_name,
                         &add_opt_to_name, &add_lifted_to_name)) {
          cut_generated = false;
          break;  // Exit the event loop.
        }
      }

      // We can break here are any further iteration on j will hit the same
      // issue.
      if (!cut_generated) break;

      // Build the cut to evaluate its efficacy.
      LinearConstraint potential_cut =
          temp_builder.BuildConstraint(kMinIntegerValue, 0);

      if (potential_cut.NormalizedViolation(lp_values) >= kMinCutViolation) {
        std::string full_name(cut_name);
        if (add_energy_to_name) full_name.append("_energy");
        if (add_lifted_to_name) full_name.append("_lifted");
        if (add_opt_to_name) full_name.append("_optional");
        if (add_quadratic_to_name) full_name.append("_quadratic");
        top_n_cuts.AddCut(std::move(potential_cut), full_name, lp_values);
      }
    }
  }
}

CutGenerator CreateCumulativeEnergyCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity,
    const std::optional<AffineExpression>& makespan, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(
      helper, model, &result.vars,
      IntegerVariablesToAddMask::kSize | IntegerVariablesToAddMask::kPresence);
  if (makespan.has_value() && !makespan.value().IsConstant()) {
    result.vars.push_back(makespan.value().var);
  }
  gtl::STLSortAndRemoveDuplicates(&result.vars);
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();

  result.generate_cuts = [makespan, capacity, demands_helper, helper,
                          integer_trail, time_limit, sat_solver,
                          model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
    if (!demands_helper->CacheAllEnergyValues()) return true;

    const auto& lp_values = manager->LpValues();
    const VariablesAssignment& assignment = sat_solver->Assignment();

    std::vector<EnergyEvent> events;
    for (int i = 0; i < helper->NumTasks(); ++i) {
      if (helper->IsAbsent(i)) continue;
      // TODO(user): use level 0 bounds ?
      if (demands_helper->DemandMax(i) == 0 || helper->SizeMin(i) == 0) {
        continue;
      }
      if (!DecomposedEnergyIsPropagated(assignment, i, helper,
                                        demands_helper)) {
        return true;  // Propagation did not reach a fixed point. Abort.
      }

      EnergyEvent e(i, helper);
      e.demand = demands_helper->Demands()[i];
      e.demand_min = demands_helper->DemandMin(i);
      e.decomposed_energy = demands_helper->DecomposedEnergies()[i];
      if (e.decomposed_energy.size() == 1) {
        // We know it was propagated correctly. We can remove this field.
        e.decomposed_energy.clear();
      }
      e.energy_min = demands_helper->EnergyMin(i);
      e.energy_is_quadratic = demands_helper->EnergyIsQuadratic(i);
      if (!helper->IsPresent(i)) {
        e.presence_literal_index = helper->PresenceLiteral(i).Index();
      }
      // We can always skip events.
      if (!e.FillEnergyLp(helper->Sizes()[i], lp_values, model)) continue;
      events.push_back(e);
    }

    TopNCuts top_n_cuts(5);
    std::vector<absl::Span<EnergyEvent>> disjoint_events =
        SplitEventsInIndendentSets(absl::MakeSpan(events));
    // Can we pass cluster as const. It would mean sorting before.
    for (const absl::Span<EnergyEvent> cluster : disjoint_events) {
      if (makespan.has_value() && integer_trail->IsFixed(capacity)) {
        GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity(
            "CumulativeEnergyM", lp_values, cluster,
            integer_trail->FixedValue(capacity), makespan.value(), time_limit,
            model, top_n_cuts);

      } else {
        GenerateCumulativeEnergeticCuts("CumulativeEnergy", lp_values, cluster,
                                        capacity, time_limit, model,
                                        top_n_cuts);
      }
    }
    top_n_cuts.TransferToManager(manager);
    return true;
  };

  return result;
}

CutGenerator CreateNoOverlapEnergyCutGenerator(
    SchedulingConstraintHelper* helper,
    const std::optional<AffineExpression>& makespan, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(
      helper, model, &result.vars,
      IntegerVariablesToAddMask::kSize | IntegerVariablesToAddMask::kPresence);
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
      e.demand = IntegerValue(1);
      e.demand_min = IntegerValue(1);
      e.energy_min = e.size_min;
      if (!helper->IsPresent(i)) {
        e.presence_literal_index = helper->PresenceLiteral(i).Index();
      }
      // We can always skip events.
      if (!e.FillEnergyLp(helper->Sizes()[i], lp_values, model)) continue;
      events.push_back(e);
    }

    TopNCuts top_n_cuts(5);
    std::vector<absl::Span<EnergyEvent>> disjoint_events =
        SplitEventsInIndendentSets(absl::MakeSpan(events));
    for (const absl::Span<EnergyEvent> cluster : disjoint_events) {
      if (makespan.has_value()) {
        GenerateCumulativeEnergeticCutsWithMakespanAndFixedCapacity(
            "NoOverlapEnergyM", lp_values, cluster,
            /*capacity=*/IntegerValue(1), makespan.value(), time_limit, model,
            top_n_cuts);
      } else {
        GenerateCumulativeEnergeticCuts("NoOverlapEnergy", lp_values, cluster,
                                        /*capacity=*/IntegerValue(1),
                                        time_limit, model, top_n_cuts);
      }
    }
    top_n_cuts.TransferToManager(manager);
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
  AddIntegerVariableFromIntervals(helper, model, &result.vars,
                                  IntegerVariablesToAddMask::kPresence);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  struct TimeTableEvent {
    int interval_index;
    IntegerValue time;
    LinearExpression demand;
    double demand_lp = 0.0;
    bool is_positive = false;
    bool use_decomposed_energy_min = false;
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
      e1.use_decomposed_energy_min =
          !demands_helper->DecomposedEnergies()[i].empty();
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
    std::stable_sort(events.begin(), events.end(),
                     [](const TimeTableEvent& i, const TimeTableEvent& j) {
                       return std::tie(i.time, i.is_positive) <
                              std::tie(j.time, j.is_positive);
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
          bool use_decomposed_energy_min = false;
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
            use_decomposed_energy_min |= cut_event.use_decomposed_energy_min;
            use_optional |= cut_event.is_optional;
          }

          std::string cut_name = "CumulativeTimeTable";
          if (use_optional) cut_name += "_optional";
          if (use_decomposed_energy_min) cut_name += "_energy";
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
    absl::string_view cut_name, bool ignore_zero_size_intervals,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    absl::Span<CachedIntervalData> events, IntegerValue capacity_max,
    Model* model, TopNCuts& top_n_cuts) {
  const int num_events = events.size();
  if (num_events <= 1) return;

  std::stable_sort(
      events.begin(), events.end(),
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

      if (ignore_zero_size_intervals &&
          (e1.size_min <= 0 || e2.size_min <= 0)) {
        continue;
      }

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
}

CutGenerator CreateCumulativePrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, SchedulingDemandHelper* demands_helper,
    const AffineExpression& capacity, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AppendVariablesFromCapacityAndDemands(capacity, demands_helper, model,
                                        &result.vars);
  AddIntegerVariableFromIntervals(
      helper, model, &result.vars,
      IntegerVariablesToAddMask::kEnd | IntegerVariablesToAddMask::kStart);
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

    TopNCuts top_n_cuts(5);
    std::vector<absl::Span<CachedIntervalData>> disjoint_events =
        SplitEventsInIndendentSets(absl::MakeSpan(events));
    for (const absl::Span<CachedIntervalData> cluster : disjoint_events) {
      GenerateCutsBetweenPairOfNonOverlappingTasks(
          "Cumulative", /* ignore_zero_size_intervals= */ true,
          manager->LpValues(), cluster, capacity_max, model, top_n_cuts);
    }
    top_n_cuts.TransferToManager(manager);
    return true;
  };
  return result;
}

CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    SchedulingConstraintHelper* helper, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(
      helper, model, &result.vars,
      IntegerVariablesToAddMask::kEnd | IntegerVariablesToAddMask::kStart);
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

    TopNCuts top_n_cuts(5);
    std::vector<absl::Span<CachedIntervalData>> disjoint_events =
        SplitEventsInIndendentSets(absl::MakeSpan(events));
    for (const absl::Span<CachedIntervalData> cluster : disjoint_events) {
      GenerateCutsBetweenPairOfNonOverlappingTasks(
          "NoOverlap", /* ignore_zero_size_intervals= */ false,
          manager->LpValues(), cluster, IntegerValue(1), model, top_n_cuts);
    }
    top_n_cuts.TransferToManager(manager);
    return true;
  };

  return result;
}

CompletionTimeEvent::CompletionTimeEvent(int t,
                                         SchedulingConstraintHelper* helper,
                                         SchedulingDemandHelper* demands_helper)
    : task_index(t),
      start_min(helper->StartMin(t)),
      start_max(helper->StartMax(t)),
      end_min(helper->EndMin(t)),
      end_max(helper->EndMax(t)),
      size_min(helper->SizeMin(t)),
      start(helper->Starts()[t]),
      end(helper->Ends()[t]) {
  if (demands_helper == nullptr) {
    demand_min = 1;
    demand_is_fixed = true;
    energy_min = size_min;
    use_decomposed_energy_min = false;
  } else {
    demand_min = demands_helper->DemandMin(t);
    demand_is_fixed = demands_helper->DemandIsFixed(t);
    // Default values for energy. Will be updated if decomposed energy is
    // not empty.
    energy_min = demand_min * size_min;
    use_decomposed_energy_min = false;
    decomposed_energy = demands_helper->DecomposedEnergies()[t];
    if (decomposed_energy.size() == 1) {
      // We know everything is propagated, we can remove this field.
      decomposed_energy.clear();
    }
  }
}

std::string CompletionTimeEvent::DebugString() const {
  return absl::StrCat(
      "CompletionTimeEvent(task_index = ", task_index,
      ", start_min = ", start_min, ", start_max = ", start_max,
      ", size_min = ", size_min, ", end = ", end.DebugString(),
      ", lp_end = ", lp_end, ", size_min = ", size_min,
      " demand_min = ", demand_min, ", demand_is_fixed = ", demand_is_fixed,
      ", energy_min = ", energy_min,
      ", use_decomposed_energy_min = ", use_decomposed_energy_min,
      ", lifted = ", lifted, ", decomposed_energy = [",
      absl::StrJoin(decomposed_energy, ", ",
                    [](std::string* out, const LiteralValueValue& e) {
                      absl::StrAppend(out, e.left_value, " * ", e.right_value);
                    }),
      "]");
}

void CtExhaustiveHelper::Init(
    const absl::Span<const CompletionTimeEvent> events, Model* model) {
  max_task_index_ = 0;
  if (events.empty()) return;

  // We compute the max_task_index_ from the events early to avoid sorting
  // the events if there are too many of them.
  for (const auto& event : events) {
    max_task_index_ = std::max(max_task_index_, event.task_index);
  }
  BuildPredecessors(events, model);
  VLOG(2) << "num_tasks:" << max_task_index_ + 1
          << " num_precedences:" << predecessors_.num_entries()
          << " predecessors size:" << predecessors_.size();
}

void CtExhaustiveHelper::BuildPredecessors(
    const absl::Span<const CompletionTimeEvent> events, Model* model) {
  predecessors_.clear();
  if (events.size() > 100) return;

  ReifiedLinear2Bounds* binary_relations =
      model->GetOrCreate<ReifiedLinear2Bounds>();

  std::vector<CompletionTimeEvent> sorted_events(events.begin(), events.end());
  std::sort(sorted_events.begin(), sorted_events.end(),
            [](const CompletionTimeEvent& a, const CompletionTimeEvent& b) {
              return a.task_index < b.task_index;
            });

  predecessors_.reserve(max_task_index_ + 1);
  for (const auto& e1 : sorted_events) {
    for (const auto& e2 : sorted_events) {
      if (e2.task_index == e1.task_index) continue;
      if (binary_relations->GetLevelZeroPrecedenceStatus(e2.end, e1.start) ==
          RelationStatus::IS_TRUE) {
        while (predecessors_.size() <= e1.task_index) predecessors_.Add({});
        predecessors_.AppendToLastVector(e2.task_index);
      }
    }
  }
}

bool CtExhaustiveHelper::PermutationIsCompatibleWithPrecedences(
    absl::Span<const CompletionTimeEvent> events,
    absl::Span<const int> permutation) {
  if (predecessors_.num_entries() == 0) return true;
  visited_.assign(max_task_index_ + 1, false);
  for (int i = permutation.size() - 1; i >= 0; --i) {
    const CompletionTimeEvent& event = events[permutation[i]];
    if (event.task_index >= predecessors_.size()) continue;
    for (const int predecessor : predecessors_[event.task_index]) {
      if (visited_[predecessor]) return false;
    }
    visited_[event.task_index] = true;
  }
  return true;
}

namespace {

bool ComputeWeightedSumOfEndMinsOfOnePermutationForNoOverlap(
    absl::Span<const CompletionTimeEvent> events,
    absl::Span<const int> permutation, IntegerValue& sum_of_ends,
    IntegerValue& sum_of_weighted_ends) {
  // Reset the two sums.
  sum_of_ends = 0;
  sum_of_weighted_ends = 0;

  // Loop over the permutation.
  IntegerValue end_min_of_previous_task = kMinIntegerValue;
  for (const int index : permutation) {
    const CompletionTimeEvent& event = events[index];
    const IntegerValue task_start_min =
        std::max(event.start_min, end_min_of_previous_task);
    if (event.start_max < task_start_min) return false;  // Infeasible.

    end_min_of_previous_task = task_start_min + event.size_min;
    sum_of_ends += end_min_of_previous_task;
    sum_of_weighted_ends += event.energy_min * end_min_of_previous_task;
  }
  return true;
}

// This functions packs all events in a cumulative of capacity 'capacity_max'
// following the given permutation. It returns the sum of end mins and the sum
// of end mins weighted by event.energy_min.
//
// It ensures that if event_j is after event_i in the permutation, then
// event_j starts exactly at the same time or after event_i.
//
// It returns false if one event cannot start before event.start_max.
bool ComputeWeightedSumOfEndMinsOfOnePermutation(
    absl::Span<const CompletionTimeEvent> events,
    absl::Span<const int> permutation, IntegerValue capacity_max,
    CtExhaustiveHelper& helper, IntegerValue& sum_of_ends,
    IntegerValue& sum_of_weighted_ends, bool& cut_use_precedences) {
  DCHECK_EQ(permutation.size(), events.size());

  if (capacity_max == 1) {
    return ComputeWeightedSumOfEndMinsOfOnePermutationForNoOverlap(
        events, permutation, sum_of_ends, sum_of_weighted_ends);
  }

  // Reset the two sums.
  sum_of_ends = 0;
  sum_of_weighted_ends = 0;

  // Quick check to see if the permutation feasible:
  // ei = events[permutation[i]], ej = events[permutation[j]], i < j
  // - start_max(ej) >= start_min(ei)
  IntegerValue demand_min_of_previous_task = 0;
  IntegerValue start_min_of_previous_task = kMinIntegerValue;
  IntegerValue end_min_of_previous_task = kMinIntegerValue;
  for (const int index : permutation) {
    const CompletionTimeEvent& event = events[index];
    const IntegerValue threshold =
        std::max(event.start_min,
                 (event.demand_min + demand_min_of_previous_task > capacity_max)
                     ? end_min_of_previous_task
                     : start_min_of_previous_task);
    if (event.start_max < threshold) {
      return false;
    }

    start_min_of_previous_task = threshold;
    end_min_of_previous_task = threshold + event.size_min;
    demand_min_of_previous_task = event.demand_min;
  }

  // The profile (and new profile) is a set of (time, capa_left) pairs,
  // ordered by increasing time and capa_left.
  helper.profile_.clear();
  helper.profile_.emplace_back(kMinIntegerValue, capacity_max);
  helper.profile_.emplace_back(kMaxIntegerValue, capacity_max);

  // Loop over the permutation.
  helper.assigned_ends_.assign(helper.max_task_index() + 1, kMinIntegerValue);
  IntegerValue start_of_previous_task = kMinIntegerValue;
  for (const int index : permutation) {
    const CompletionTimeEvent& event = events[index];
    const IntegerValue start_min =
        std::max(event.start_min, start_of_previous_task);

    // Iterate on the profile to find the step that contains start_min.
    // Then push until we find a step with enough capacity.
    auto profile_it = helper.profile_.begin();
    while ((profile_it + 1)->first <= start_min ||
           profile_it->second < event.demand_min) {
      ++profile_it;
    }

    IntegerValue actual_start = std::max(start_min, profile_it->first);
    const IntegerValue initial_start_min = actual_start;

    // Propagate precedences.
    //
    // helper.predecessors() can be truncated. We need to be careful here.
    if (event.task_index < helper.predecessors().size()) {
      for (const int predecessor : helper.predecessors()[event.task_index]) {
        if (helper.assigned_ends_[predecessor] == kMinIntegerValue) continue;
        actual_start =
            std::max(actual_start, helper.assigned_ends_[predecessor]);
      }
    }

    if (actual_start > initial_start_min) {
      cut_use_precedences = true;
      // Catch up the position on the profile w.r.t. the actual start.
      while ((profile_it + 1)->first <= actual_start) ++profile_it;
      VLOG(3) << "push from " << initial_start_min << " to " << actual_start;
    }

    // Compatible with the event.start_max ?
    if (actual_start > event.start_max) {
      return false;
    }

    const IntegerValue actual_end = actual_start + event.size_min;

    // Bookkeeping.
    helper.assigned_ends_[event.task_index] = actual_end;
    sum_of_ends += actual_end;
    sum_of_weighted_ends += event.energy_min * actual_end;
    start_of_previous_task = actual_start;

    // No need to update the profile on the last loop.
    if (event.task_index == events[permutation.back()].task_index) break;

    // Update the profile.
    helper.new_profile_.clear();
    const IntegerValue demand_min = event.demand_min;

    // Insert the start of the shifted profile.
    helper.new_profile_.push_back(
        {actual_start, profile_it->second - demand_min});
    ++profile_it;

    // Copy and modify the part of the profile impacted by the current event.
    while (profile_it->first < actual_end) {
      helper.new_profile_.push_back(
          {profile_it->first, profile_it->second - demand_min});
      ++profile_it;
    }

    // Insert a new event in the profile at the end of the task if needed.
    if (profile_it->first > actual_end) {
      helper.new_profile_.push_back({actual_end, (profile_it - 1)->second});
    }

    // Insert the tail of the current profile.
    helper.new_profile_.insert(helper.new_profile_.end(), profile_it,
                               helper.profile_.end());

    helper.profile_.swap(helper.new_profile_);
  }
  return true;
}

const int kCtExhaustiveTargetSize = 6;
// This correspond to the number of permutations the system will explore when
// fully exploring all possible sizes and all possible permutations for up to 6
// tasks, without any precedence.
const int kExplorationLimit = 873;  // 1! + 2! + 3! + 4! + 5! + 6!

}  // namespace

CompletionTimeExplorationStatus ComputeMinSumOfWeightedEndMins(
    absl::Span<const CompletionTimeEvent> events, IntegerValue capacity_max,
    double unweighted_threshold, double weighted_threshold,
    CtExhaustiveHelper& helper, double& min_sum_of_ends,
    double& min_sum_of_weighted_ends, bool& cut_use_precedences,
    int& exploration_credit) {
  // Reset the events based sums.
  min_sum_of_ends = std::numeric_limits<double>::max();
  min_sum_of_weighted_ends = std::numeric_limits<double>::max();
  helper.task_to_index_.assign(helper.max_task_index() + 1, -1);
  for (int i = 0; i < events.size(); ++i) {
    helper.task_to_index_[events[i].task_index] = i;
  }
  helper.valid_permutation_iterator_.Reset(events.size());
  const auto& predecessors = helper.predecessors();
  for (int i = 0; i < events.size(); ++i) {
    const int task_i = events[i].task_index;
    if (task_i >= predecessors.size()) continue;
    for (const int task_j : predecessors[task_i]) {
      const int j = helper.task_to_index_[task_j];
      if (j != -1) {
        helper.valid_permutation_iterator_.AddArc(j, i);
      }
    }
  }

  bool is_dag = false;
  int num_valid_permutations = 0;
  for (const auto& permutation : helper.valid_permutation_iterator_) {
    is_dag = true;
    if (--exploration_credit < 0) break;

    IntegerValue sum_of_ends = 0;
    IntegerValue sum_of_weighted_ends = 0;

    if (ComputeWeightedSumOfEndMinsOfOnePermutation(
            events, permutation, capacity_max, helper, sum_of_ends,
            sum_of_weighted_ends, cut_use_precedences)) {
      min_sum_of_ends = std::min(ToDouble(sum_of_ends), min_sum_of_ends);
      min_sum_of_weighted_ends =
          std::min(ToDouble(sum_of_weighted_ends), min_sum_of_weighted_ends);
      ++num_valid_permutations;

      if (min_sum_of_ends <= unweighted_threshold &&
          min_sum_of_weighted_ends <= weighted_threshold) {
        break;
      }
    }
  }
  if (!is_dag) {
    return CompletionTimeExplorationStatus::NO_VALID_PERMUTATION;
  }
  const CompletionTimeExplorationStatus status =
      exploration_credit < 0 ? CompletionTimeExplorationStatus::ABORTED
      : num_valid_permutations > 0
          ? CompletionTimeExplorationStatus::FINISHED
          : CompletionTimeExplorationStatus::NO_VALID_PERMUTATION;
  VLOG(2) << "DP: size:" << events.size()
          << ", num_valid_permutations:" << num_valid_permutations
          << ", min_sum_of_end_mins:" << min_sum_of_ends
          << ", min_sum_of_weighted_end_mins:" << min_sum_of_weighted_ends
          << ", unweighted_threshold:" << unweighted_threshold
          << ", weighted_threshold:" << weighted_threshold
          << ", exploration_credit:" << exploration_credit
          << ", status:" << status;
  return status;
}

// TODO(user): Improve performance
//   - detect disjoint tasks (no need to crossover to the second part)
//   - better caching of explored states
ABSL_MUST_USE_RESULT bool GenerateShortCompletionTimeCutsWithExactBound(
    absl::string_view cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    absl::Span<CompletionTimeEvent> events, IntegerValue capacity_max,
    CtExhaustiveHelper& helper, Model* model, TopNCuts& top_n_cuts) {
  // Sort by start min to bucketize by start_min.
  std::sort(
      events.begin(), events.end(),
      [](const CompletionTimeEvent& e1, const CompletionTimeEvent& e2) {
        return std::tie(e1.start_min, e1.energy_min, e1.lp_end, e1.task_index) <
               std::tie(e2.start_min, e2.energy_min, e2.lp_end, e2.task_index);
      });
  for (int start = 0; start + 1 < events.size(); ++start) {
    // Skip to the next start_min value.
    if (start > 0 && events[start].start_min == events[start - 1].start_min) {
      continue;
    }

    bool cut_use_precedences = false;  // Used for naming the cut.
    const IntegerValue sequence_start_min = events[start].start_min;
    helper.residual_events_.assign(events.begin() + start, events.end());

    // We look at events that start before sequence_start_min, but are forced
    // to cross this time point.
    for (int before = 0; before < start; ++before) {
      if (events[before].start_min + events[before].size_min >
          sequence_start_min) {
        helper.residual_events_.push_back(events[before]);  // Copy.
        helper.residual_events_.back().lifted = true;
      }
    }

    std::sort(helper.residual_events_.begin(), helper.residual_events_.end(),
              [](const CompletionTimeEvent& e1, const CompletionTimeEvent& e2) {
                return std::tie(e1.lp_end, e1.task_index) <
                       std::tie(e2.lp_end, e2.task_index);
              });

    double sum_of_ends_lp = 0.0;
    double sum_of_weighted_ends_lp = 0.0;
    IntegerValue sum_of_demands = 0;
    double sum_of_square_energies = 0;
    double min_sum_of_ends = std::numeric_limits<double>::max();
    double min_sum_of_weighted_ends = std::numeric_limits<double>::max();
    int exploration_limit = kExplorationLimit;
    const int kMaxSize = std::min<int>(helper.residual_events_.size(), 12);

    for (int i = 0; i < kMaxSize; ++i) {
      const CompletionTimeEvent& event = helper.residual_events_[i];
      const double energy = ToDouble(event.energy_min);
      sum_of_ends_lp += event.lp_end;
      sum_of_weighted_ends_lp += event.lp_end * energy;
      sum_of_demands += event.demand_min;
      sum_of_square_energies += energy * energy;

      // Both cases with 1 or 2 tasks are trivial and independent of the order.
      // Also, if the sum of demands is less than or equal to the capacity,
      // pushing all ends left is a valid LP assignment. And this assignment
      // should be propagated by the lp model.
      if (i <= 1 || sum_of_demands <= capacity_max) continue;

      absl::Span<const CompletionTimeEvent> tasks_to_explore =
          absl::MakeSpan(helper.residual_events_).first(i + 1);
      const CompletionTimeExplorationStatus status =
          ComputeMinSumOfWeightedEndMins(
              tasks_to_explore, capacity_max,
              /* unweighted_threshold= */ sum_of_ends_lp + kMinCutViolation,
              /* weighted_threshold= */ sum_of_weighted_ends_lp +
                  kMinCutViolation,
              helper, min_sum_of_ends, min_sum_of_weighted_ends,
              cut_use_precedences, exploration_limit);
      if (status == CompletionTimeExplorationStatus::NO_VALID_PERMUTATION) {
        // TODO(user): We should return false here but there is a bug.
        break;
      } else if (status == CompletionTimeExplorationStatus::ABORTED) {
        break;
      }

      const double unweigthed_violation =
          (min_sum_of_ends - sum_of_ends_lp) / std::sqrt(ToDouble(i + 1));
      const double weighted_violation =
          (min_sum_of_weighted_ends - sum_of_weighted_ends_lp) /
          std::sqrt(sum_of_square_energies);

      // Unweighted cuts.
      if (unweigthed_violation > weighted_violation &&
          unweigthed_violation > kMinCutViolation) {
        LinearConstraintBuilder cut(model, min_sum_of_ends, kMaxIntegerValue);
        bool is_lifted = false;
        for (int j = 0; j <= i; ++j) {
          const CompletionTimeEvent& event = helper.residual_events_[j];
          is_lifted |= event.lifted;
          cut.AddTerm(event.end, IntegerValue(1));
        }
        std::string full_name(cut_name);
        if (cut_use_precedences) full_name.append("_prec");
        if (is_lifted) full_name.append("_lifted");
        top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
      }

      // Weighted cuts.
      if (weighted_violation >= unweigthed_violation &&
          weighted_violation > kMinCutViolation) {
        LinearConstraintBuilder cut(model, min_sum_of_weighted_ends,
                                    kMaxIntegerValue);
        bool is_lifted = false;
        for (int j = 0; j <= i; ++j) {
          const CompletionTimeEvent& event = helper.residual_events_[j];
          is_lifted |= event.lifted;
          cut.AddTerm(event.end, event.energy_min);
        }
        std::string full_name(cut_name);
        if (is_lifted) full_name.append("_lifted");
        if (cut_use_precedences) full_name.append("_prec");
        full_name.append("_weighted");
        top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
      }
    }
  }
  return true;
}

namespace {

// Returns a copy of the event with the start time increased to time.
// Energy (min and decomposed) are updated accordingly.
CompletionTimeEvent CopyAndTrimEventAfter(const CompletionTimeEvent& old_event,
                                          IntegerValue time) {
  CHECK_GT(time, old_event.start_min);
  CHECK_GT(old_event.start_min + old_event.size_min, time);
  CompletionTimeEvent event = old_event;  // Copy.
  event.lifted = true;

  // Trim the decomposed energy and compute the energy min in the window.
  const IntegerValue shift = time - event.start_min;
  CHECK_GT(shift, IntegerValue(0));
  event.size_min -= shift;
  event.energy_min = event.size_min * event.demand_min;
  if (!event.decomposed_energy.empty()) {
    // Trim fixed sizes and recompute the decomposed energy min.
    IntegerValue propagated_energy_min = kMaxIntegerValue;
    for (auto& [literal, fixed_size, fixed_demand] : event.decomposed_energy) {
      CHECK_GT(fixed_size, shift);
      fixed_size -= shift;
      propagated_energy_min =
          std::min(propagated_energy_min, fixed_demand * fixed_size);
    }

    DCHECK_GT(propagated_energy_min, 0);
    if (propagated_energy_min > event.energy_min) {
      event.use_decomposed_energy_min = true;
      event.energy_min = propagated_energy_min;
    } else {
      event.use_decomposed_energy_min = false;
    }
  }
  event.start_min = time;
  return event;
}

// Collects all possible demand values for the event, and adds them to the
// subset sum of the reachable capacity of the cumulative constraint.
void AddEventDemandsToCapacitySubsetSum(
    const CompletionTimeEvent& event, const VariablesAssignment& assignment,
    IntegerValue capacity_max, std::vector<int64_t>& tmp_possible_demands,
    MaxBoundedSubsetSum& dp) {
  if (dp.CurrentMax() != capacity_max) {
    if (event.demand_is_fixed) {
      dp.Add(event.demand_min.value());
    } else if (!event.decomposed_energy.empty()) {
      tmp_possible_demands.clear();
      for (const auto& [literal, size, demand] : event.decomposed_energy) {
        if (assignment.LiteralIsFalse(literal)) continue;
        if (assignment.LiteralIsTrue(literal)) {
          tmp_possible_demands.assign({demand.value()});
          break;
        }
        tmp_possible_demands.push_back(demand.value());
      }
      dp.AddChoices(tmp_possible_demands);
    } else {  // event.demand_min is not fixed, we abort.
      // TODO(user): We could still add all events in the range if the range
      // is small.
      dp.Add(capacity_max.value());
    }
  }
}

}  // namespace

// We generate the cut from the Smith's rule from:
// M. Queyranne, Structure of a simple scheduling polyhedron,
// Mathematical Programming 58 (1993), 263–285
//
// The original cut is:
//    sum(end_min_i * size_min_i) >=
//        (sum(size_min_i^2) + sum(size_min_i)^2) / 2
// We strengthen this cuts by noticing that if all tasks starts after S,
// then replacing end_min_i by (end_min_i - S) is still valid.
//
// A second difference is that we lift intervals that starts before a given
// value, but are forced to cross it. This lifting procedure implies trimming
// interval to its part that is after the given value.
//
// In the case of a cumulative constraint with a capacity of C, we compute a
// valid equation by splitting the task (size_min si, demand_min di) into di
// tasks of size si and demand 1, that we spread across C no_overlap
// constraint. When doing so, the lhs of the equation is the same, the first
// term of the rhs is also unchanged. A lower bound of the second term of the
// rhs is reached when the split is exact (each no_overlap sees a long demand of
// sum(si * di / C). Thus the second term is greater or equal to
//   (sum (si * di) ^ 2) / (2 * C)
//
// Sometimes, the min energy of the task i is greater than si * di.
// Let's introduce ai the minimum energy of the task and rewrite the previous
// equation. In that new setting, we can rewrite the cumulative transformation
// by splitting each tasks into at least di tasks of size at least si and demand
// 1.
//
// In that setting, the lhs is rewritten as sum(ai * ei) and the second term of
// the rhs is rewritten as sum(ai) ^ 2 / (2 * C).
//
// The question is how to rewrite the term `sum(di * si * si). The minimum
// contribution is when the task has size si and demand ai / si. (note that si
// is the minimum size of the task, and di its minimum demand). We can
// replace the small rectangle area term by ai * si.
//
//     sum (ai * ei) - sum (ai) * current_start_min >=
//         sum(si * ai) / 2 + (sum (ai) ^ 2) / (2 * C)
//
// The separation procedure is implemented using two loops:
//   - first, we loop on potential start times in increasing order.
//   - second loop, we add tasks that must contribute after this start time
//     ordered by increasing end time in the LP relaxation.
void GenerateCompletionTimeCutsWithEnergy(
    absl::string_view cut_name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    absl::Span<CompletionTimeEvent> events, IntegerValue capacity_max,
    Model* model, TopNCuts& top_n_cuts) {
  const VariablesAssignment& assignment =
      model->GetOrCreate<Trail>()->Assignment();
  std::vector<int64_t> tmp_possible_demands;

  // Sort by start min to bucketize by start_min.
  std::stable_sort(
      events.begin(), events.end(),
      [](const CompletionTimeEvent& e1, const CompletionTimeEvent& e2) {
        return std::tie(e1.start_min, e1.demand_min, e1.lp_end) <
               std::tie(e2.start_min, e2.demand_min, e2.lp_end);
      });

  // First loop: we loop on potential start times.
  for (int start = 0; start + 1 < events.size(); ++start) {
    // Skip to the next start_min value.
    if (start > 0 && events[start].start_min == events[start - 1].start_min) {
      continue;
    }

    const IntegerValue sequence_start_min = events[start].start_min;
    std::vector<CompletionTimeEvent> residual_tasks(events.begin() + start,
                                                    events.end());

    // We look at events that start before sequence_start_min, but are forced
    // to cross this time point. In that case, we replace this event by a
    // truncated event starting at sequence_start_min. To do this, we reduce
    // the size_min, align the start_min with the sequence_start_min, and
    // scale the energy down accordingly.
    for (int before = 0; before < start; ++before) {
      if (events[before].start_min + events[before].size_min >
          sequence_start_min) {
        CompletionTimeEvent event =
            CopyAndTrimEventAfter(events[before], sequence_start_min);
        if (event.energy_min <= 0) continue;
        residual_tasks.push_back(std::move(event));
      }
    }

    // If we have less than kCtExhaustiveTargetSize tasks, we are already
    // covered by the exhaustive cut generator.
    if (residual_tasks.size() <= kCtExhaustiveTargetSize) continue;

    // Best cut so far for this loop.
    int best_end = -1;
    double best_efficacy = 0.01;
    IntegerValue best_min_contrib = 0;
    bool best_uses_subset_sum = false;

    // Used in the first term of the rhs of the equation.
    IntegerValue sum_event_contributions = 0;
    // Used in the second term of the rhs of the equation.
    IntegerValue sum_energy = 0;
    // For normalization.
    IntegerValue sum_square_energy = 0;

    double lp_contrib = 0.0;
    IntegerValue current_start_min(kMaxIntegerValue);

    MaxBoundedSubsetSum dp(capacity_max.value());

    // We will add tasks one by one, sorted by end time, and evaluate the
    // potential cut at each step.
    std::stable_sort(
        residual_tasks.begin(), residual_tasks.end(),
        [](const CompletionTimeEvent& e1, const CompletionTimeEvent& e2) {
          return e1.lp_end < e2.lp_end;
        });

    // Second loop: we add tasks one by one.
    for (int i = 0; i < residual_tasks.size(); ++i) {
      const CompletionTimeEvent& event = residual_tasks[i];
      DCHECK_GE(event.start_min, sequence_start_min);

      // Make sure we do not overflow.
      if (!AddTo(event.energy_min, &sum_energy)) break;
      // In the no_overlap case, we have:
      //   area = event.size_min ^ 2
      // In the simple cumulative case, we split split the task
      // (demand_min, size_min) into demand_min tasks in the no_overlap case.
      //   area = event.demand_min * event.size_min * event.size_min
      // In the cumulative case, we can have energy_min > side_min * demand_min.
      // In that case, we use energy_min * size_min.
      if (!AddProductTo(event.energy_min, event.size_min,
                        &sum_event_contributions)) {
        break;
      }
      if (!AddSquareTo(event.energy_min, &sum_square_energy)) break;

      lp_contrib += event.lp_end * ToDouble(event.energy_min);
      current_start_min = std::min(current_start_min, event.start_min);

      // Maintain the reachable capacity with a bounded complexity subset sum.
      AddEventDemandsToCapacitySubsetSum(event, assignment, capacity_max,
                                         tmp_possible_demands, dp);

      // Ignore cuts covered by the exhaustive cut generator.
      if (i < kCtExhaustiveTargetSize) continue;

      const IntegerValue reachable_capacity = dp.CurrentMax();

      // Do we have a violated cut ?
      const IntegerValue large_rectangle_contrib =
          CapProdI(sum_energy, sum_energy);
      if (AtMinOrMaxInt64I(large_rectangle_contrib)) break;
      const IntegerValue mean_large_rectangle_contrib =
          CeilRatio(large_rectangle_contrib, reachable_capacity);

      IntegerValue min_contrib =
          CapAddI(sum_event_contributions, mean_large_rectangle_contrib);
      if (AtMinOrMaxInt64I(min_contrib)) break;
      min_contrib = CeilRatio(min_contrib, 2);

      // shift contribution by current_start_min.
      if (!AddProductTo(sum_energy, current_start_min, &min_contrib)) break;

      // The efficacy of the cut is the normalized violation of the above
      // equation. We will normalize by the sqrt of the sum of squared energies.
      const double efficacy = (ToDouble(min_contrib) - lp_contrib) /
                              std::sqrt(ToDouble(sum_square_energy));

      // For a given start time, we only keep the best cut.
      // The reason is that is the cut is strongly violated, we can get a
      // sequence of violated cuts as we add more tasks. These new cuts will
      // be less violated, but will not bring anything useful to the LP
      // relaxation. At the same time, this sequence of cuts can push out
      // other cuts from a disjoint set of tasks.
      if (efficacy > best_efficacy) {
        best_efficacy = efficacy;
        best_end = i;
        best_min_contrib = min_contrib;
        best_uses_subset_sum = reachable_capacity < capacity_max;
      }
    }

    // We have inserted all tasks. Have we found a violated cut ?
    // If so, add the most violated one to the top_n cut container.
    if (best_end != -1) {
      LinearConstraintBuilder cut(model, best_min_contrib, kMaxIntegerValue);
      bool is_lifted = false;
      bool add_energy_to_name = false;
      for (int i = 0; i <= best_end; ++i) {
        const CompletionTimeEvent& event = residual_tasks[i];
        is_lifted |= event.lifted;
        add_energy_to_name |= event.use_decomposed_energy_min;
        cut.AddTerm(event.end, event.energy_min);
      }
      std::string full_name(cut_name);
      if (add_energy_to_name) full_name.append("_energy");
      if (is_lifted) full_name.append("_lifted");
      if (best_uses_subset_sum) full_name.append("_subsetsum");
      top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
    }
  }
}

CutGenerator CreateNoOverlapCompletionTimeCutGenerator(
    SchedulingConstraintHelper* helper, Model* model) {
  CutGenerator result;
  result.only_run_at_level_zero = true;
  AddIntegerVariableFromIntervals(helper, model, &result.vars,
                                  IntegerVariablesToAddMask::kEnd);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  result.generate_cuts = [helper, model](LinearConstraintManager* manager) {
    if (!helper->SynchronizeAndSetTimeDirection(true)) return false;

    auto generate_cuts = [model, manager,
                          helper](bool time_is_forward) -> bool {
      if (!helper->SynchronizeAndSetTimeDirection(time_is_forward)) {
        return false;
      }
      std::vector<CompletionTimeEvent> events;
      const auto& lp_values = manager->LpValues();
      for (int index = 0; index < helper->NumTasks(); ++index) {
        if (!helper->IsPresent(index)) continue;
        const IntegerValue size_min = helper->SizeMin(index);
        if (size_min > 0) {
          CompletionTimeEvent event(index, helper, nullptr);
          event.lp_end = event.end.LpValue(lp_values);
          events.push_back(event);
        }
      }

      CtExhaustiveHelper helper;
      helper.Init(events, model);

      TopNCuts top_n_cuts(5);
      std::vector<absl::Span<CompletionTimeEvent>> disjoint_events =
          SplitEventsInIndendentSets(absl::MakeSpan(events));
      for (const absl::Span<CompletionTimeEvent> cluster : disjoint_events) {
        if (!GenerateShortCompletionTimeCutsWithExactBound(
                "NoOverlapCompletionTimeExhaustive", lp_values, cluster,
                /*capacity_max=*/IntegerValue(1), helper, model, top_n_cuts)) {
          return false;
        }

        GenerateCompletionTimeCutsWithEnergy(
            "NoOverlapCompletionTimeQueyrane", lp_values, cluster,
            /*capacity_max=*/IntegerValue(1), model, top_n_cuts);
      }
      top_n_cuts.TransferToManager(manager);
      return true;
    };
    if (!generate_cuts(/*time_is_forward=*/true)) return false;
    if (!generate_cuts(/*time_is_forward=*/false)) return false;
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
  AddIntegerVariableFromIntervals(helper, model, &result.vars,
                                  IntegerVariablesToAddMask::kEnd);
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  result.generate_cuts = [integer_trail, helper, demands_helper, capacity,
                          sat_solver,
                          model](LinearConstraintManager* manager) -> bool {
    auto generate_cuts = [integer_trail, sat_solver, model, manager, helper,
                          demands_helper,
                          capacity](bool time_is_forward) -> bool {
      DCHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);
      if (!helper->SynchronizeAndSetTimeDirection(time_is_forward)) {
        return false;
      }
      if (!demands_helper->CacheAllEnergyValues()) return true;

      std::vector<CompletionTimeEvent> events;
      const auto& lp_values = manager->LpValues();
      const VariablesAssignment& assignment = sat_solver->Assignment();
      for (int index = 0; index < helper->NumTasks(); ++index) {
        if (!helper->IsPresent(index)) continue;
        if (!DecomposedEnergyIsPropagated(assignment, index, helper,
                                          demands_helper)) {
          return true;  // Propagation did not reach a fixed point. Abort.
        }
        if (helper->SizeMin(index) > 0 &&
            demands_helper->DemandMin(index) > 0) {
          CompletionTimeEvent event(index, helper, demands_helper);
          event.lp_end = event.end.LpValue(lp_values);
          events.push_back(event);
        }
      }

      CtExhaustiveHelper helper;
      helper.Init(events, model);

      const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
      TopNCuts top_n_cuts(5);
      std::vector<absl::Span<CompletionTimeEvent>> disjoint_events =
          SplitEventsInIndendentSets(absl::MakeSpan(events));
      for (const absl::Span<CompletionTimeEvent> cluster : disjoint_events) {
        if (!GenerateShortCompletionTimeCutsWithExactBound(
                "CumulativeCompletionTimeExhaustive", lp_values, cluster,
                capacity_max, helper, model, top_n_cuts)) {
          return false;
        }

        GenerateCompletionTimeCutsWithEnergy("CumulativeCompletionTimeQueyrane",
                                             lp_values, cluster, capacity_max,
                                             model, top_n_cuts);
      }
      top_n_cuts.TransferToManager(manager);
      return true;
    };

    if (!generate_cuts(/*time_is_forward=*/true)) return false;
    if (!generate_cuts(/*time_is_forward=*/false)) return false;
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research
