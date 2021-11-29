// Copyright 2010-2021 Google LLC
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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

// Returns the lp value of a Literal.
double GetLiteralLpValue(
    const Literal lit,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    const IntegerEncoder* encoder) {
  const IntegerVariable direct_view = encoder->GetLiteralView(lit);
  if (direct_view != kNoIntegerVariable) {
    return lp_values[direct_view];
  }
  const IntegerVariable opposite_view = encoder->GetLiteralView(lit.Negated());
  DCHECK_NE(opposite_view, kNoIntegerVariable);
  return 1.0 - lp_values[opposite_view];
}

void AddIntegerVariableFromIntervals(SchedulingConstraintHelper* helper,
                                     Model* model,
                                     std::vector<IntegerVariable>* vars) {
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
  for (int t = 0; t < helper->NumTasks(); ++t) {
    if (helper->Starts()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Starts()[t].var);
    }
    if (helper->Sizes()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Sizes()[t].var);
    }
    if (helper->Ends()[t].var != kNoIntegerVariable) {
      vars->push_back(helper->Ends()[t].var);
    }
    if (helper->IsOptional(t) && !helper->IsAbsent(t) &&
        !helper->IsPresent(t)) {
      const Literal l = helper->PresenceLiteral(t);
      if (encoder->GetLiteralView(l) == kNoIntegerVariable &&
          encoder->GetLiteralView(l.Negated()) == kNoIntegerVariable) {
        model->Add(NewIntegerVariableFromLiteral(l));
      }
      const IntegerVariable direct_view = encoder->GetLiteralView(l);
      if (direct_view != kNoIntegerVariable) {
        vars->push_back(direct_view);
      } else {
        vars->push_back(encoder->GetLiteralView(l.Negated()));
        DCHECK_NE(vars->back(), kNoIntegerVariable);
      }
    }
  }
  gtl::STLSortAndRemoveDuplicates(vars);
}

}  // namespace

std::function<bool(const absl::StrongVector<IntegerVariable, double>&,
                   LinearConstraintManager*)>
GenerateCumulativeEnergyCuts(const std::string& cut_name,
                             SchedulingConstraintHelper* helper,
                             const std::vector<IntegerVariable>& demands,
                             const std::vector<LinearExpression>& energies,
                             AffineExpression capacity, Model* model) {
  Trail* trail = model->GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();

  return [capacity, demands, energies, trail, integer_trail, helper, model,
          cut_name,
          encoder](const absl::StrongVector<IntegerVariable, double>& lp_values,
                   LinearConstraintManager* manager) {
    if (trail->CurrentDecisionLevel() > 0) return true;

    const auto demand_is_fixed = [integer_trail, &demands](int i) {
      return demands.empty() || integer_trail->IsFixed(demands[i]);
    };
    const auto demand_min = [integer_trail, &demands](int i) {
      return demands.empty() ? IntegerValue(1)
                             : integer_trail->LowerBound(demands[i]);
    };
    const auto demand_max = [integer_trail, &demands](int i) {
      return demands.empty() ? IntegerValue(1)
                             : integer_trail->UpperBound(demands[i]);
    };

    std::vector<int> active_intervals;
    for (int i = 0; i < helper->NumTasks(); ++i) {
      if (!helper->IsAbsent(i) && demand_max(i) > 0 && helper->SizeMin(i) > 0) {
        active_intervals.push_back(i);
      }
    }

    if (active_intervals.size() < 2) return true;

    std::sort(active_intervals.begin(), active_intervals.end(),
              [helper](int a, int b) {
                return helper->StartMin(a) < helper->StartMin(b) ||
                       (helper->StartMin(a) == helper->StartMin(b) &&
                        helper->EndMax(a) < helper->EndMax(b));
              });

    const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
    IntegerValue processed_start = kMinIntegerValue;
    for (int i1 = 0; i1 + 1 < active_intervals.size(); ++i1) {
      const int start_index = active_intervals[i1];
      DCHECK(!helper->IsAbsent(start_index));

      // We want maximal cuts. For any start_min value, we only need to create
      // cuts starting from the first interval having this start_min value.
      if (helper->StartMin(start_index) == processed_start) {
        continue;
      } else {
        processed_start = helper->StartMin(start_index);
      }

      // For each start time, we will keep the most violated cut generated while
      // scanning the residual intervals.
      int end_index_of_max_violation = -1;
      double max_relative_violation = 1.01;
      IntegerValue start_of_max_violation(0);
      IntegerValue end_of_max_violation(0);
      std::vector<int> lifted_intervals_of_max_violation;

      // Accumulate intervals and check for potential cuts.
      double energy_lp = 0.0;
      IntegerValue min_of_starts = kMaxIntegerValue;
      IntegerValue max_of_ends = kMinIntegerValue;

      // We sort all tasks (start_min(task) >= start_min(start_index) by
      // increasing end max.
      std::vector<int> residual_intervals(active_intervals.begin() + i1,
                                          active_intervals.end());
      // Keep track of intervals not included in the potential cut.
      // TODO(user): remove ?
      std::set<int> intervals_not_visited(active_intervals.begin(),
                                          active_intervals.end());
      std::sort(
          residual_intervals.begin(), residual_intervals.end(),
          [&](int a, int b) { return helper->EndMax(a) < helper->EndMax(b); });

      // Let's process residual tasks and evaluate the cut violation of the cut
      // at each step. We follow the same structure as the cut creation code
      // below.
      for (int i2 = 0; i2 < residual_intervals.size(); ++i2) {
        const int t = residual_intervals[i2];
        intervals_not_visited.erase(t);
        if (helper->IsPresent(t)) {
          if (demand_is_fixed(t)) {
            if (helper->SizeIsFixed(t)) {
              energy_lp += ToDouble(helper->SizeMin(t) * demand_min(t));
            } else {
              energy_lp += ToDouble(demand_min(t)) *
                           helper->Sizes()[t].LpValue(lp_values);
            }
          } else if (helper->SizeIsFixed(t)) {
            DCHECK(!demands.empty());
            energy_lp += lp_values[demands[t]] * ToDouble(helper->SizeMin(t));
          } else if (!energies.empty()) {
            energy_lp += energies[t].LpValue(lp_values);
          } else {  // demand and size are not fixed.
            DCHECK(!demands.empty());
            energy_lp +=
                ToDouble(demand_min(t)) * helper->Sizes()[t].LpValue(lp_values);
            energy_lp += lp_values[demands[t]] * ToDouble(helper->SizeMin(t));
            energy_lp -= ToDouble(demand_min(t) * helper->SizeMin(t));
          }
        } else {
          // TODO(user): Use the energy min if better than size_min *
          // demand_min, here and when building the cut.
          energy_lp += GetLiteralLpValue(helper->PresenceLiteral(t), lp_values,
                                         encoder) *
                       ToDouble(helper->SizeMin(t) * demand_min(t));
        }

        min_of_starts = std::min(min_of_starts, helper->StartMin(t));
        max_of_ends = std::max(max_of_ends, helper->EndMax(t));

        // Dominance rule. If the next interval also fits in
        // [min_of_starts, max_of_ends], the cut will be stronger with the
        // next interval.
        if (i2 + 1 < residual_intervals.size() &&
            helper->StartMin(residual_intervals[i2 + 1]) >= min_of_starts &&
            helper->EndMax(residual_intervals[i2 + 1]) <= max_of_ends) {
          continue;
        }

        // Compute forced contributions from intervals not included in
        // [min_of_starts..max_of_ends].
        //
        // TODO(user): We could precompute possible intervals and store them
        // by start_max, end_min to reduce the complexity.
        std::vector<int> lifted_intervals;
        std::vector<IntegerValue> lifted_min_overlap;
        double forced_contrib_lp = 0.0;
        for (const int t : intervals_not_visited) {
          // It should not happen because of the 2 dominance rules above.
          if (helper->StartMin(t) >= min_of_starts &&
              helper->EndMax(t) <= max_of_ends) {
            continue;
          }

          const IntegerValue min_overlap =
              helper->GetMinOverlap(t, min_of_starts, max_of_ends);

          if (min_overlap <= 0) continue;

          lifted_intervals.push_back(t);

          if (helper->IsPresent(t)) {
            if (demand_is_fixed(t)) {
              forced_contrib_lp += ToDouble(min_overlap * demand_min(t));
            } else {
              DCHECK(!demands.empty());
              forced_contrib_lp +=
                  lp_values[demands[t]] * ToDouble(min_overlap);
            }
          } else {
            forced_contrib_lp += GetLiteralLpValue(helper->PresenceLiteral(t),
                                                   lp_values, encoder) *
                                 ToDouble(min_overlap * demand_min(t));
          }
        }

        // Compute the violation of the potential cut.
        const double relative_violation =
            (energy_lp + forced_contrib_lp) /
            ToDouble((max_of_ends - min_of_starts) * capacity_max);
        if (relative_violation > max_relative_violation) {
          end_index_of_max_violation = i2;
          max_relative_violation = relative_violation;
          start_of_max_violation = min_of_starts;
          end_of_max_violation = max_of_ends;
          lifted_intervals_of_max_violation = lifted_intervals;
        }
      }

      if (end_index_of_max_violation == -1) continue;

      // A maximal violated cut has been found.
      bool cut_generated = true;
      bool has_opt_cuts = false;
      bool lifted = false;
      bool has_quadratic_cuts = false;
      bool use_energy = false;

      LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));

      // Build the cut.
      cut.AddTerm(capacity, start_of_max_violation - end_of_max_violation);
      for (int i2 = 0; i2 <= end_index_of_max_violation; ++i2) {
        const int t = residual_intervals[i2];
        if (helper->IsPresent(t)) {
          if (demand_is_fixed(t)) {
            cut.AddTerm(helper->Sizes()[t], demand_min(t));
          } else if (!helper->SizeIsFixed(t) && !energies.empty()) {
            // We favor the energy info instead of the McCormick relaxation.
            cut.AddLinearExpression(energies[t]);
            use_energy = true;
          } else {
            // This will add linear term if the size is fixed.
            cut.AddQuadraticLowerBound(helper->Sizes()[t], demands[t],
                                       integer_trail);
            if (!helper->SizeIsFixed(t)) {
              has_quadratic_cuts = true;
            }
          }
        } else {
          // TODO(user): use the offset of the energy expression if better
          // than size_min * demand_min.
          has_opt_cuts = true;
          if (!helper->SizeIsFixed(t) || !demand_is_fixed(t)) {
            has_quadratic_cuts = true;
          }
          if (!cut.AddLiteralTerm(helper->PresenceLiteral(t),
                                  helper->SizeMin(t) * demand_min(t))) {
            cut_generated = false;
            break;
          }
        }
      }

      for (int i2 = 0; i2 < lifted_intervals_of_max_violation.size(); ++i2) {
        const int t = lifted_intervals_of_max_violation[i2];
        const IntegerValue min_overlap = helper->GetMinOverlap(
            t, start_of_max_violation, end_of_max_violation);
        lifted = true;

        if (helper->IsPresent(t)) {
          if (demand_is_fixed(t)) {
            cut.AddConstant(min_overlap * demand_min(t));
          } else {
            DCHECK(!demands.empty());
            cut.AddTerm(demands[t], min_overlap);
          }
        } else {
          has_opt_cuts = true;
          if (!cut.AddLiteralTerm(helper->PresenceLiteral(t),
                                  min_overlap * demand_min(t))) {
            cut_generated = false;
            break;
          }
        }
      }

      if (cut_generated) {
        std::string full_name = cut_name;
        if (has_opt_cuts) full_name.append("_opt");
        if (has_quadratic_cuts) full_name.append("_quad");
        if (lifted) full_name.append("_lifted");
        if (use_energy) full_name.append("_energy");

        manager->AddCut(cut.Build(), full_name, lp_values);
      }
    }
    return true;
  };
}

CutGenerator CreateCumulativeEnergyCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const IntegerVariable capacity, const std::vector<IntegerVariable>& demands,
    const std::vector<LinearExpression>& energies, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  result.vars = demands;
  result.vars.push_back(capacity);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);
  for (const LinearExpression& energy : energies) {
    result.vars.insert(result.vars.end(), energy.vars.begin(),
                       energy.vars.end());
  }
  gtl::STLSortAndRemoveDuplicates(&result.vars);

  // TODO(user): Do not create the cut generator if the capacity is fixed,
  // all demands are fixed, and the intervals are always performed with a fixed
  // size.
  result.generate_cuts =
      GenerateCumulativeEnergyCuts("CumulativeEnergy", helper, demands,
                                   energies, AffineExpression(capacity), model);
  return result;
}

CutGenerator CreateNoOverlapEnergyCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  // TODO(user): Do not create the cut generator if all intervals are
  // performed with a fixed size as it will not propagate more than
  // the overload checker.
  result.generate_cuts = GenerateCumulativeEnergyCuts(
      "NoOverlapEnergy", helper,
      /*demands=*/{}, /*energies=*/{},
      /*capacity=*/AffineExpression(IntegerValue(1)), model);
  return result;
}

CutGenerator CreateCumulativeTimeTableCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const IntegerVariable capacity, const std::vector<IntegerVariable>& demands,
    Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  result.vars = demands;
  result.vars.push_back(capacity);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  struct Event {
    int interval_index;
    IntegerValue time;
    bool positive;
    IntegerVariable demand;
  };

  Trail* trail = model->GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  result.generate_cuts =
      [helper, capacity, demands, trail, integer_trail, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        std::vector<Event> events;
        // Iterate through the intervals. If start_max < end_min, the demand
        // is mandatory.
        for (int i = 0; i < helper->NumTasks(); ++i) {
          if (helper->IsAbsent(i)) continue;

          const IntegerValue start_max = helper->StartMax(i);
          const IntegerValue end_min = helper->EndMin(i);

          if (start_max >= end_min) continue;

          Event e1;
          e1.interval_index = i;
          e1.time = start_max;
          e1.demand = demands[i];
          e1.positive = true;

          Event e2 = e1;
          e2.time = end_min;
          e2.positive = false;
          events.push_back(e1);
          events.push_back(e2);
        }

        // Sort events by time.
        // It is also important that all positive event with the same time as
        // negative events appear after for the correctness of the algo below.
        std::sort(events.begin(), events.end(),
                  [](const Event i, const Event j) {
                    if (i.time == j.time) {
                      if (i.positive == j.positive) {
                        return i.interval_index < j.interval_index;
                      }
                      return !i.positive;
                    }
                    return i.time < j.time;
                  });

        std::vector<Event> cut_events;
        bool added_positive_event = false;
        for (const Event& e : events) {
          if (e.positive) {
            added_positive_event = true;
            cut_events.push_back(e);
            continue;
          }
          if (added_positive_event && cut_events.size() > 1) {
            // Create cut.
            bool cut_generated = true;
            LinearConstraintBuilder cut(model, kMinIntegerValue,
                                        IntegerValue(0));
            cut.AddTerm(capacity, IntegerValue(-1));
            for (const Event& cut_event : cut_events) {
              if (helper->IsPresent(cut_event.interval_index)) {
                cut.AddTerm(cut_event.demand, IntegerValue(1));
              } else {
                cut_generated &= cut.AddLiteralTerm(
                    helper->PresenceLiteral(cut_event.interval_index),
                    integer_trail->LowerBound(cut_event.demand));
                if (!cut_generated) break;
              }
            }
            if (cut_generated) {
              // Violation of the cut is checked by AddCut so we don't check
              // it here.
              manager->AddCut(cut.Build(), "CumulativeTimeTable", lp_values);
            }
          }
          // Remove the event.
          int new_size = 0;
          for (int i = 0; i < cut_events.size(); ++i) {
            if (cut_events[i].interval_index == e.interval_index) {
              continue;
            }
            cut_events[new_size] = cut_events[i];
            new_size++;
          }
          cut_events.resize(new_size);
          added_positive_event = false;
        }
        return true;
      };
  return result;
}

// Cached Information about one interval.
struct PrecedenceEvent {
  IntegerValue start_min;
  IntegerValue start_max;
  AffineExpression start;
  IntegerValue end_min;
  IntegerValue end_max;
  AffineExpression end;
  IntegerValue demand_min;
};

void GeneratePrecedenceCuts(
    const std::string& cut_name,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    std::vector<PrecedenceEvent> events, IntegerValue capacity_max,
    Model* model, LinearConstraintManager* manager) {
  const int num_events = events.size();
  if (num_events <= 1) return;

  std::sort(events.begin(), events.end(),
            [](const PrecedenceEvent& e1, const PrecedenceEvent& e2) {
              return e1.start_min < e2.start_min ||
                     (e1.start_min == e2.start_min && e1.end_max < e2.end_max);
            });

  const double tolerance = 1e-4;

  for (int i = 0; i + 1 < num_events; ++i) {
    const PrecedenceEvent& e1 = events[i];
    for (int j = i + 1; j < num_events; ++j) {
      const PrecedenceEvent& e2 = events[j];
      if (e2.start_min >= e1.end_max) break;  // Break out of the index2 loop.

      // Encode only the interesting pairs.
      if (e1.demand_min + e2.demand_min <= capacity_max) continue;

      const bool interval_1_can_precede_2 = e1.end_min <= e2.start_max;
      const bool interval_2_can_precede_1 = e2.end_min <= e1.start_max;

      if (interval_1_can_precede_2 && !interval_2_can_precede_1 &&
          e1.end.LpValue(lp_values) >=
              e2.start.LpValue(lp_values) + tolerance) {
        // interval1.end <= interval2.start
        LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));
        cut.AddTerm(e1.end, IntegerValue(1));
        cut.AddTerm(e2.start, IntegerValue(-1));
      } else if (interval_2_can_precede_1 && !interval_1_can_precede_2 &&
                 e2.end.LpValue(lp_values) >=
                     e1.start.LpValue(lp_values) + tolerance) {
        // interval2.end <= interval1.start
        LinearConstraintBuilder cut(model, kMinIntegerValue, IntegerValue(0));
        cut.AddTerm(e2.end, IntegerValue(1));
        cut.AddTerm(e1.start, IntegerValue(-1));
        manager->AddCut(cut.Build(), cut_name, lp_values);
      }
    }
  }
}

CutGenerator CreateCumulativePrecedenceCutGenerator(
    const std::vector<IntervalVariable>& intervals, IntegerVariable capacity,
    const std::vector<IntegerVariable>& demands, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  result.vars = demands;
  result.vars.push_back(capacity);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  Trail* trail = model->GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  result.generate_cuts =
      [trail, integer_trail, helper, demands, capacity, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
        std::vector<PrecedenceEvent> events;
        for (int t = 0; t < helper->NumTasks(); ++t) {
          if (!helper->IsPresent(t)) continue;
          PrecedenceEvent event;
          event.start_min = helper->StartMin(t);
          event.start_max = helper->StartMax(t);
          event.start = helper->Starts()[t];
          event.end_min = helper->EndMin(t);
          event.end_max = helper->EndMax(t);
          event.end = helper->Ends()[t];
          event.demand_min = integer_trail->LowerBound(demands[t]);
          events.push_back(event);
        }
        GeneratePrecedenceCuts("CumulativePrecedence", lp_values,
                               std::move(events), capacity_max, model, manager);
        return true;
      };
  return result;
}

CutGenerator CreateNoOverlapPrecedenceCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  Trail* trail = model->GetOrCreate<Trail>();

  result.generate_cuts =
      [trail, helper, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        std::vector<PrecedenceEvent> events;
        for (int t = 0; t < helper->NumTasks(); ++t) {
          if (!helper->IsPresent(t)) continue;
          PrecedenceEvent event;
          event.start_min = helper->StartMin(t);
          event.start_max = helper->StartMax(t);
          event.start = helper->Starts()[t];
          event.end_min = helper->EndMin(t);
          event.end_max = helper->EndMax(t);
          event.end = helper->Ends()[t];
          event.demand_min = IntegerValue(1);
          events.push_back(event);
        }
        GeneratePrecedenceCuts("NoOverlapPrecedence", lp_values,
                               std::move(events), IntegerValue(1), model,
                               manager);
        return true;
      };

  return result;
}

// Stores the event for a box along the two axis x and y.
//   For a no_overlap constraint, y is always of size 1 between 0 and 1.
//   For a cumulative constraint, y is the demand that must be between 0 and
//       capacity_max.
//   For a no_overlap_2d constraint, y the other dimension of the box.
struct CtEvent {
  // The start min of the x interval.
  IntegerValue x_start_min;

  // The size min of the x interval.
  IntegerValue x_size_min;

  // The end of the x interval.
  AffineExpression x_end;

  // The lp value of the end of the x interval.
  double x_lp_end;

  // The start min of the y interval.
  IntegerValue y_start_min;

  // The end max of the y interval.
  IntegerValue y_end_max;

  // The min energy of the task (this is always larger or equal to x_size_min *
  // y_size_min).
  IntegerValue energy_min;

  // Indicates if the events used the optional energy information from the
  // model.
  bool use_energy = false;

  // Indicates if the cut is lifted, that is if it includes tasks that are not
  // strictly contained in the current time window.
  bool lifted = false;

  std::string DebugString() const {
    return absl::StrCat("CtEvent(x_end = ", x_end.DebugString(),
                        ", x_start_min = ", x_start_min.value(),
                        ", x_size_min = ", x_size_min.value(),
                        ", x_lp_end = ", x_lp_end,
                        ", y_start_min = ", y_start_min.value(),
                        ", y_end_max = ", y_end_max.value(),
                        ", energy_min = ", energy_min.value(),
                        ", use_energy = ", use_energy, ", lifted = ", lifted);
  }
};

// We generate the cut from the Smith's rule from:
// M. Queyranne, Structure of a simple scheduling polyhedron,
// Mathematical Programming 58 (1993), 263–285
//
// The original cut is:
//    sum(end_min_i * duration_min_i) >=
//        (sum(duration_min_i^2) + sum(duration_min_i)^2) / 2
// We strenghten this cuts by noticing that if all tasks starts after S,
// then replacing end_min_i by (end_min_i - S) is still valid.
//
// A second difference is that we look at a set of intervals starting
// after a given start_min, sorted by relative (end_lp - start_min).
void GenerateCompletionTimeCuts(
    const std::string& cut_name,
    const absl::StrongVector<IntegerVariable, double>& lp_values,
    std::vector<CtEvent> events, bool use_lifting, Model* model,
    LinearConstraintManager* manager) {
  TopNCuts top_n_cuts(15);

  // Sort by start min to bucketize by start_min.
  std::sort(events.begin(), events.end(),
            [](const CtEvent& e1, const CtEvent& e2) {
              return e1.x_start_min < e2.x_start_min;
            });
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
    // the size_min, align the start_min with the sequence_start_min, and
    // scale the energy down accordingly.
    if (use_lifting) {
      for (int before = 0; before < start; ++before) {
        if (events[before].x_start_min + events[before].x_size_min >
            sequence_start_min) {
          CtEvent event = events[before];  // Copy.
          event.lifted = true;
          const IntegerValue old_size_min = event.x_size_min;
          event.x_size_min =
              event.x_size_min + event.x_start_min - sequence_start_min;
          event.x_start_min = sequence_start_min;
          // We can rescale the energy min correctly.
          //
          // Let's take the example of a box of size 2 * 20 that overlaps
          // sequence start min by 1, and that can rotate by 90 degrees.
          // The energy min is 40, size min is 2, size_max is 20.
          // If the box is horizontal, the lifted energy is (20 - 1) * 2 = 38.
          // If the box is vertical, the lifted energy is (2 - 1) * 20 = 20.
          // The min of the two is always reached when size = size_min.
          event.energy_min = event.energy_min * event.x_size_min / old_size_min;
          residual_tasks.push_back(event);
        }
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
    IntegerValue best_size_divisor(0);
    double unscaled_lp_contrib = 0;
    IntegerValue current_start_min(kMaxIntegerValue);
    IntegerValue y_start_min = kMaxIntegerValue;
    IntegerValue y_end_max = kMinIntegerValue;

    for (int i = 0; i < residual_tasks.size(); ++i) {
      const CtEvent& event = residual_tasks[i];
      DCHECK_GE(event.x_start_min, sequence_start_min);
      const IntegerValue energy = event.energy_min;
      sum_duration += energy;
      sum_square_duration += energy * energy;
      unscaled_lp_contrib += event.x_lp_end * ToDouble(energy);
      current_start_min = std::min(current_start_min, event.x_start_min);
      y_start_min = std::min(y_start_min, event.y_start_min);
      y_end_max = std::max(y_end_max, event.y_end_max);

      const IntegerValue size_divisor = y_end_max - y_start_min;

      // We compute the cuts with all the sizes actually equal to
      //     size_min * demand_min / size_divisor
      // but to keep the computation in the integer domain, we multiply by
      // size_divisor where needed instead.
      const IntegerValue min_contrib =
          (sum_duration * sum_duration + sum_square_duration) / 2 +
          current_start_min * sum_duration * size_divisor;
      const double efficacy = (ToDouble(min_contrib) -
                               unscaled_lp_contrib * ToDouble(size_divisor)) /
                              std::sqrt(ToDouble(sum_square_duration));
      // TODO(user): Check overflow and ignore if too big.
      if (efficacy > best_efficacy) {
        best_efficacy = efficacy;
        best_end = i;
        best_min_contrib = min_contrib;
        best_size_divisor = size_divisor;
      }
    }
    if (best_end != -1) {
      LinearConstraintBuilder cut(model, best_min_contrib, kMaxIntegerValue);
      bool is_lifted = false;
      bool use_energy = false;
      for (int i = 0; i <= best_end; ++i) {
        const CtEvent& event = residual_tasks[i];
        is_lifted |= event.lifted;
        use_energy |= event.use_energy;
        cut.AddTerm(event.x_end, event.energy_min * best_size_divisor);
      }
      std::string full_name = cut_name;
      if (is_lifted) full_name.append("_lifted");
      if (use_energy) full_name.append("_energy");
      top_n_cuts.AddCut(cut.Build(), full_name, lp_values);
    }
  }
  top_n_cuts.TransferToManager(lp_values, manager);
}

CutGenerator CreateNoOverlapCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& intervals, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  Trail* trail = model->GetOrCreate<Trail>();

  result.generate_cuts =
      [trail, helper, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        auto generate_cuts = [&lp_values, model, manager,
                              helper](const std::string& cut_name) {
          std::vector<CtEvent> events;
          for (int index = 0; index < helper->NumTasks(); ++index) {
            if (!helper->IsPresent(index)) continue;
            const IntegerValue size_min = helper->SizeMin(index);
            if (size_min > 0) {
              const AffineExpression end_expr = helper->Ends()[index];
              CtEvent event;
              event.x_start_min = helper->StartMin(index);
              event.x_size_min = size_min;
              event.x_end = end_expr;
              event.x_lp_end = end_expr.LpValue(lp_values);
              event.y_start_min = IntegerValue(0);
              event.y_end_max = IntegerValue(1);
              event.energy_min = size_min;
              events.push_back(event);
            }
          }
          GenerateCompletionTimeCuts(cut_name, lp_values, std::move(events),
                                     /*use_lifting=*/false, model, manager);
        };
        if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
        generate_cuts("NoOverlapCompletionTime");
        if (!helper->SynchronizeAndSetTimeDirection(false)) return false;
        generate_cuts("NoOverlapCompletionTimeMirror");
        return true;
      };
  return result;
}

CutGenerator CreateCumulativeCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& intervals,
    const IntegerVariable capacity, const std::vector<IntegerVariable>& demands,
    const std::vector<LinearExpression>& energies, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* helper =
      new SchedulingConstraintHelper(intervals, model);
  model->TakeOwnership(helper);

  result.vars = demands;
  result.vars.push_back(capacity);
  AddIntegerVariableFromIntervals(helper, model, &result.vars);

  Trail* trail = model->GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

  result.generate_cuts =
      [trail, integer_trail, helper, demands, energies, capacity, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        const IntegerValue capacity_max = integer_trail->UpperBound(capacity);
        auto generate_cuts = [&lp_values, model, manager, helper, capacity_max,
                              integer_trail, &demands,
                              &energies](const std::string& cut_name) {
          std::vector<CtEvent> events;
          for (int index = 0; index < helper->NumTasks(); ++index) {
            if (!helper->IsPresent(index)) continue;
            if (helper->SizeMin(index) > 0 &&
                integer_trail->LowerBound(demands[index]) > 0) {
              const AffineExpression end_expr = helper->Ends()[index];
              IntegerValue energy_min =
                  energies.empty()
                      ? IntegerValue(0)
                      : LinExprLowerBound(energies[index], *integer_trail);

              const IntegerValue size_min = helper->SizeMin(index);
              const IntegerValue demand_min =
                  integer_trail->LowerBound(demands[index]);
              CtEvent event;
              event.x_start_min = helper->StartMin(index);
              event.x_size_min = size_min;
              event.x_end = end_expr;
              event.x_lp_end = end_expr.LpValue(lp_values);
              event.y_start_min = IntegerValue(0);
              event.y_end_max = IntegerValue(capacity_max);
              if (energy_min > size_min * demand_min) {
                event.energy_min = energy_min;
                event.use_energy = true;
              } else {
                event.energy_min = size_min * demand_min;
              }
              events.push_back(event);
            }
          }
          GenerateCompletionTimeCuts(cut_name, lp_values, std::move(events),
                                     /*use_lifting=*/true, model, manager);
        };
        if (!helper->SynchronizeAndSetTimeDirection(true)) return false;
        generate_cuts("CumulativeCompletionTime");
        if (!helper->SynchronizeAndSetTimeDirection(false)) return false;
        generate_cuts("CumulativeCompletionTimeMirror");
        return true;
      };
  return result;
}

CutGenerator CreateNoOverlap2dCompletionTimeCutGenerator(
    const std::vector<IntervalVariable>& x_intervals,
    const std::vector<IntervalVariable>& y_intervals, Model* model) {
  CutGenerator result;

  SchedulingConstraintHelper* x_helper =
      new SchedulingConstraintHelper(x_intervals, model);
  model->TakeOwnership(x_helper);

  SchedulingConstraintHelper* y_helper =
      new SchedulingConstraintHelper(y_intervals, model);
  model->TakeOwnership(y_helper);
  AddIntegerVariableFromIntervals(x_helper, model, &result.vars);
  AddIntegerVariableFromIntervals(y_helper, model, &result.vars);

  Trail* trail = model->GetOrCreate<Trail>();

  result.generate_cuts =
      [trail, x_helper, y_helper, model](
          const absl::StrongVector<IntegerVariable, double>& lp_values,
          LinearConstraintManager* manager) {
        if (trail->CurrentDecisionLevel() > 0) return true;

        if (!x_helper->SynchronizeAndSetTimeDirection(true)) return false;
        if (!y_helper->SynchronizeAndSetTimeDirection(true)) return false;

        const int num_boxes = x_helper->NumTasks();
        std::vector<int> active_boxes;
        std::vector<IntegerValue> cached_areas(num_boxes);
        std::vector<Rectangle> cached_rectangles(num_boxes);
        for (int box = 0; box < num_boxes; ++box) {
          cached_areas[box] = x_helper->SizeMin(box) * y_helper->SizeMin(box);
          if (cached_areas[box] == 0) continue;
          if (!y_helper->IsPresent(box) || !y_helper->IsPresent(box)) continue;

          // TODO(user): It might be possible/better to use some shifted value
          // here, but for now this code is not in the hot spot, so better be
          // defensive and only do connected components on really disjoint
          // boxes.
          Rectangle& rectangle = cached_rectangles[box];
          rectangle.x_min = x_helper->StartMin(box);
          rectangle.x_max = x_helper->EndMax(box);
          rectangle.y_min = y_helper->StartMin(box);
          rectangle.y_max = y_helper->EndMax(box);

          active_boxes.push_back(box);
        }

        if (active_boxes.size() <= 1) return true;

        std::vector<absl::Span<int>> components =
            GetOverlappingRectangleComponents(cached_rectangles,
                                              absl::MakeSpan(active_boxes));
        for (absl::Span<int> boxes : components) {
          if (boxes.size() <= 1) continue;

          auto generate_cuts = [&lp_values, model, manager, &boxes,
                                &cached_areas](
                                   const std::string& cut_name,
                                   SchedulingConstraintHelper* x_helper,
                                   SchedulingConstraintHelper* y_helper) {
            std::vector<CtEvent> events;

            for (const int box : boxes) {
              const AffineExpression x_end_expr = x_helper->Ends()[box];
              CtEvent event;
              event.x_start_min = x_helper->ShiftedStartMin(box);
              event.x_size_min = x_helper->SizeMin(box);
              event.x_end = x_end_expr;
              event.x_lp_end = x_end_expr.LpValue(lp_values);
              event.y_start_min = y_helper->ShiftedStartMin(box);
              event.y_end_max = y_helper->ShiftedEndMax(box);
              event.energy_min =
                  x_helper->SizeMin(box) * y_helper->SizeMin(box);
              events.push_back(event);
            }

            GenerateCompletionTimeCuts(cut_name, lp_values, std::move(events),
                                       /*use_lifting=*/true, model, manager);
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
