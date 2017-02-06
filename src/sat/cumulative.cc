// Copyright 2010-2014 Google
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

#include <algorithm>

#include "sat/cumulative.h"
#include "sat/disjunctive.h"
#include "sat/overload_checker.h"
#include "sat/sat_solver.h"
#include "sat/timetable.h"
#include "sat/timetable_edgefinding.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    const std::vector<IntegerVariable>& demands,
    const IntegerVariable& capacity) {
  return [=](Model* model) {
    if (vars.empty()) return;
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();

    if (vars.size() == 1) {
      if (intervals->IsOptional(vars[0])) {
        model->Add(ConditionalLowerOrEqualWithOffset(
            demands[0], capacity, 0, intervals->IsPresentLiteral(vars[0])));
      } else {
        model->Add(LowerOrEqual(demands[0], capacity));
      }
      return;
    }

    // Detect a subset of intervals that needs to be in disjunction.
    //
    // TODO(user): We need to exclude intervals that can be of size zero because
    // the disjunctive do not "ignore" them like the cumulative does. That is,
    // the interval [2,2) will be assumed to be in disjunction with [1, 3) for
    // instance. We need to uniformize the handling of interval with size zero.
    std::vector<IntervalVariable> in_disjunction;
    for (int i = 0; i < vars.size(); ++i) {
      if (intervals->MinSize(vars[i]) > 0 &&
          2 * model->Get(LowerBound(demands[i])) >
              model->Get(UpperBound(capacity))) {
        in_disjunction.push_back(vars[i]);
      }
    }

    // Add a disjunctive constraint on the intervals in in_disjunction. Do not
    // create the cumulative at all when all intervals must be in disjunction.
    //
    // TODO(user): Do proper experiments to see how beneficial this is, the
    // disjunctive will propagate more but is also using slower algorithms. That
    // said, this is more a question of optimizing the disjunctive propagation
    // code.
    //
    // TODO(user): Another "known" idea is to detect pair of tasks that must be
    // in disjunction and to create a Boolean to indicate which one is before
    // the other. It shouldn't change the propagation, but may result in a
    // faster one with smaller explanations, and the solver can also take
    // decision on such Boolean.
    //
    // TODO(user): A better place for stuff like this could be in the presolver
    // so that it is easier to disable and play with alternatives.
    if (in_disjunction.size() > 1) {
      model->Add(Disjunctive(in_disjunction));
    }
    if (in_disjunction.size() == vars.size()) {
      // We still need to propagate the minimum capacity though. Note that
      // this works since we don't have interval of size zero here.
      for (int i = 0; i < vars.size(); ++i) {
        if (intervals->IsOptional(vars[i])) {
          model->Add(ConditionalLowerOrEqualWithOffset(
              demands[i], capacity, 0, intervals->IsPresentLiteral(vars[i])));
        } else {
          model->Add(LowerOrEqual(demands[i], capacity));
        }
      }
      return;
    }

    Trail* trail = model->GetOrCreate<Trail>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    // Propagator responsible for applying Timetabling filtering rule. It
    // increases the minimum of the start variables, decrease the maximum of the
    // end variables, and increase the minimum of the capacity variable.
    TimeTablingPerTask* time_tabling = new TimeTablingPerTask(
        vars, demands, capacity, trail, integer_trail, intervals);
    time_tabling->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(time_tabling);

    const SatParameters& parameters = model->Get<SatSolver>()->parameters();

    // Propagator responsible for applying the Overload Checking filtering rule.
    // It increases the minimum of the capacity variable.
    if (parameters.use_overload_checker_in_cumulative_constraint()) {
      OverloadChecker* overload_checker = new OverloadChecker(
          vars, demands, capacity, trail, integer_trail, intervals);
      overload_checker->RegisterWith(
          model->GetOrCreate<GenericLiteralWatcher>());
      model->TakeOwnership(overload_checker);
    }

    // Propagator responsible for applying the Timetable Edge finding filtering
    // rule. It increases the minimum of the start variables and decreases the
    // maximum of the end variables,
    if (parameters.use_timetable_edge_finding_in_cumulative_constraint()) {
      TimeTableEdgeFinding* time_table_edge_finding = new TimeTableEdgeFinding(
          vars, demands, capacity, trail, integer_trail, intervals);
      time_table_edge_finding->RegisterWith(
          model->GetOrCreate<GenericLiteralWatcher>());
      model->TakeOwnership(time_table_edge_finding);
    }
  };
}

std::function<void(Model*)> CumulativeTimeDecomposition(
    const std::vector<IntervalVariable>& vars,
    const std::vector<IntegerVariable>& demand_vars,
    const IntegerVariable& capacity_var) {
  return [=](Model* model) {
    CHECK(model->Get(IsFixed(capacity_var)));

    const int num_tasks = vars.size();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();

    std::vector<IntegerVariable> start_vars;
    std::vector<IntegerVariable> end_vars;
    std::vector<IntegerValue> demands;

    for (int t = 0; t < num_tasks; ++t) {
      const IntervalVariable i = vars[t];

      CHECK(!intervals->IsOptional(i));
      CHECK(intervals->SizeVar(i) == kNoIntegerVariable);
      CHECK(model->Get(IsFixed(demand_vars[t])));

      start_vars.push_back(intervals->StartVar(i));
      end_vars.push_back(intervals->EndVar(i));
      demands.push_back(integer_trail->LowerBound(demand_vars[t]));
    }

    // Compute time range.
    IntegerValue min_start = kMaxIntegerValue;
    IntegerValue max_end = kMinIntegerValue;
    for (int t = 0; t < num_tasks; ++t) {
      min_start = std::min(min_start, integer_trail->LowerBound(start_vars[t]));
      max_end = std::max(max_end, integer_trail->UpperBound(end_vars[t]));
    }

    const IntegerValue capacity = integer_trail->UpperBound(capacity_var);

    for (IntegerValue time = min_start; time <= max_end; ++time) {
      std::vector<LiteralWithCoeff> literals_with_coeff;
      for (int t = 0; t < num_tasks; ++t) {
        const IntegerValue start_min = integer_trail->LowerBound(start_vars[t]);
        const IntegerValue end_max = integer_trail->UpperBound(end_vars[t]);
        if (end_max <= time || time < start_min || demands[t] == 0) continue;

        // True if the task overlap time.
        const Literal overlap_true =
            Literal(model->Add(NewBooleanVariable()), true);
        // True if the task does not start after time.
        const Literal start_true = encoder->CreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(start_vars[t], IntegerValue(time)));
        // True if the task ends after time.
        const Literal end_true =
            encoder->CreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
                end_vars[t], IntegerValue(time + 1)));

        bool sat = true;
        // if overlap_true is true, then start_true is true.
        sat &= sat_solver->AddBinaryClause(overlap_true.Negated(), start_true);
        // if overlap is true, then end_true is true.
        sat &= sat_solver->AddBinaryClause(overlap_true.Negated(), end_true);
        // if start_true and end_true are true, then overlap_true is true.
        sat &= sat_solver->AddTernaryClause(start_true.Negated(),
                                            end_true.Negated(), overlap_true);

        if (!sat) return;

        literals_with_coeff.push_back(
            LiteralWithCoeff(overlap_true, Coefficient(demands[t].value())));
      }
      // The profile cannot exceed the capacity at time.
      sat_solver->AddLinearConstraint(false, Coefficient(0), true,
                                      Coefficient(capacity.value()),
                                      &literals_with_coeff);
    }
  };
}

}  // namespace sat
}  // namespace operations_research
