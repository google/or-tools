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

#include "ortools/sat/cumulative.h"

#include <algorithm>
#include <memory>

#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cumulative_energy.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/timetable.h"
#include "ortools/sat/timetable_edgefinding.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=](Model* model) mutable {
    if (vars.empty()) return;

    auto* intervals = model->GetOrCreate<IntervalsRepository>();
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();

    // Redundant constraints to ensure that the resource capacity is high enough
    // for each task. Also ensure that no task consumes more resource than what
    // is available. This is useful because the subsequent propagators do not
    // filter the capacity variable very well.
    for (int i = 0; i < demands.size(); ++i) {
      if (intervals->MaxSize(vars[i]) == 0) continue;

      LinearConstraintBuilder builder(model, kMinIntegerValue, IntegerValue(0));
      builder.AddTerm(demands[i], IntegerValue(1));
      builder.AddTerm(capacity, IntegerValue(-1));
      LinearConstraint ct = builder.Build();

      std::vector<Literal> enforcement_literals;
      if (intervals->IsOptional(vars[i])) {
        enforcement_literals.push_back(intervals->PresenceLiteral(vars[i]));
      }

      // If the interval can be of size zero, it currently do not count towards
      // the capacity. TODO(user): Change that since we have optional interval
      // for this.
      if (intervals->MinSize(vars[i]) == 0) {
        enforcement_literals.push_back(encoder->GetOrCreateAssociatedLiteral(
            intervals->Size(vars[i]).GreaterOrEqual(IntegerValue(1))));
      }

      if (enforcement_literals.empty()) {
        LoadLinearConstraint(ct, model);
      } else {
        LoadConditionalLinearConstraint(enforcement_literals, ct, model);
      }
    }

    if (vars.size() == 1) return;

    const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

    // Detect a subset of intervals that needs to be in disjunction and add a
    // Disjunctive() constraint over them.
    if (parameters.use_disjunctive_constraint_in_cumulative_constraint()) {
      // TODO(user): We need to exclude intervals that can be of size zero
      // because the disjunctive do not "ignore" them like the cumulative
      // does. That is, the interval [2,2) will be assumed to be in
      // disjunction with [1, 3) for instance. We need to uniformize the
      // handling of interval with size zero.
      //
      // TODO(user): improve the condition (see CL147454185).
      std::vector<IntervalVariable> in_disjunction;
      for (int i = 0; i < vars.size(); ++i) {
        if (intervals->MinSize(vars[i]) > 0 &&
            2 * integer_trail->LowerBound(demands[i]) >
                integer_trail->UpperBound(capacity)) {
          in_disjunction.push_back(vars[i]);
        }
      }

      // Add a disjunctive constraint on the intervals in in_disjunction. Do not
      // create the cumulative at all when all intervals must be in disjunction.
      //
      // TODO(user): Do proper experiments to see how beneficial this is, the
      // disjunctive will propagate more but is also using slower algorithms.
      // That said, this is more a question of optimizing the disjunctive
      // propagation code.
      //
      // TODO(user): Another "known" idea is to detect pair of tasks that must
      // be in disjunction and to create a Boolean to indicate which one is
      // before the other. It shouldn't change the propagation, but may result
      // in a faster one with smaller explanations, and the solver can also take
      // decision on such Boolean.
      //
      // TODO(user): A better place for stuff like this could be in the
      // presolver so that it is easier to disable and play with alternatives.
      if (in_disjunction.size() > 1) model->Add(Disjunctive(in_disjunction));
      if (in_disjunction.size() == vars.size()) return;
    }

    if (helper == nullptr) {
      helper = new SchedulingConstraintHelper(vars, model);
      model->TakeOwnership(helper);
    }

    // For each variables that is after a subset of task ends (i.e. like a
    // makespan objective), we detect it and add a special constraint to
    // propagate it.
    //
    // TODO(user): Models that include the makespan as a special interval might
    // be better, but then not everyone does that. In particular this code
    // allows to have decent lower bound on the large cumulative minizinc
    // instances.
    //
    // TODO(user): this require the precedence constraints to be already loaded,
    // and there is no guarantee of that currently. Find a more robust way.
    //
    // TODO(user): There is a bit of code duplication with the disjunctive
    // precedence propagator. Abstract more?
    {
      std::vector<IntegerVariable> index_to_end_vars;
      std::vector<int> index_to_task;
      std::vector<PrecedencesPropagator::IntegerPrecedences> before;
      index_to_end_vars.clear();
      for (int t = 0; t < helper->NumTasks(); ++t) {
        const AffineExpression& end_exp = helper->Ends()[t];

        // TODO(user): Handle generic affine relation?
        if (end_exp.var == kNoIntegerVariable || end_exp.coeff != 1) continue;
        index_to_end_vars.push_back(end_exp.var);
        index_to_task.push_back(t);
      }
      model->GetOrCreate<PrecedencesPropagator>()->ComputePrecedences(
          index_to_end_vars, &before);
      const int size = before.size();
      for (int i = 0; i < size;) {
        const IntegerVariable var = before[i].var;
        DCHECK_NE(var, kNoIntegerVariable);

        IntegerValue min_offset = kMaxIntegerValue;
        std::vector<int> subtasks;
        for (; i < size && before[i].var == var; ++i) {
          const int t = index_to_task[before[i].index];
          subtasks.push_back(t);

          // We have var >= end_exp.var + offset, so
          // var >= (end_exp.var + end_exp.cte) + (offset - end_exp.cte)
          // var >= task end + new_offset.
          const AffineExpression& end_exp = helper->Ends()[t];
          min_offset =
              std::min(min_offset, before[i].offset - end_exp.constant);
        }

        if (subtasks.size() > 1) {
          CumulativeIsAfterSubsetConstraint* constraint =
              new CumulativeIsAfterSubsetConstraint(var, min_offset, capacity,
                                                    demands, subtasks,
                                                    integer_trail, helper);
          constraint->RegisterWith(watcher);
          model->TakeOwnership(constraint);
        }
      }
    }

    // Propagator responsible for applying Timetabling filtering rule. It
    // increases the minimum of the start variables, decrease the maximum of the
    // end variables, and increase the minimum of the capacity variable.
    TimeTablingPerTask* time_tabling =
        new TimeTablingPerTask(demands, capacity, integer_trail, helper);
    time_tabling->RegisterWith(watcher);
    model->TakeOwnership(time_tabling);

    // Propagator responsible for applying the Overload Checking filtering rule.
    // It increases the minimum of the capacity variable.
    if (parameters.use_overload_checker_in_cumulative_constraint()) {
      AddCumulativeOverloadChecker(demands, capacity, helper, model);
    }

    // Propagator responsible for applying the Timetable Edge finding filtering
    // rule. It increases the minimum of the start variables and decreases the
    // maximum of the end variables,
    if (parameters.use_timetable_edge_finding_in_cumulative_constraint()) {
      TimeTableEdgeFinding* time_table_edge_finding =
          new TimeTableEdgeFinding(demands, capacity, helper, integer_trail);
      time_table_edge_finding->RegisterWith(watcher);
      model->TakeOwnership(time_table_edge_finding);
    }
  };
}

std::function<void(Model*)> CumulativeTimeDecomposition(
    const std::vector<IntervalVariable>& vars,
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=](Model* model) {
    if (vars.empty()) return;

    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    CHECK(integer_trail->IsFixed(capacity));
    const Coefficient fixed_capacity(
        integer_trail->UpperBound(capacity).value());

    const int num_tasks = vars.size();
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();

    std::vector<IntegerVariable> start_vars;
    std::vector<IntegerVariable> end_vars;
    std::vector<IntegerValue> fixed_demands;

    for (int t = 0; t < num_tasks; ++t) {
      start_vars.push_back(intervals->StartVar(vars[t]));
      end_vars.push_back(intervals->EndVar(vars[t]));
      CHECK(integer_trail->IsFixed(demands[t]));
      fixed_demands.push_back(integer_trail->LowerBound(demands[t]));
    }

    // Compute time range.
    IntegerValue min_start = kMaxIntegerValue;
    IntegerValue max_end = kMinIntegerValue;
    for (int t = 0; t < num_tasks; ++t) {
      min_start = std::min(min_start, integer_trail->LowerBound(start_vars[t]));
      max_end = std::max(max_end, integer_trail->UpperBound(end_vars[t]));
    }

    for (IntegerValue time = min_start; time < max_end; ++time) {
      std::vector<LiteralWithCoeff> literals_with_coeff;
      for (int t = 0; t < num_tasks; ++t) {
        sat_solver->Propagate();
        const IntegerValue start_min = integer_trail->LowerBound(start_vars[t]);
        const IntegerValue end_max = integer_trail->UpperBound(end_vars[t]);
        if (end_max <= time || time < start_min || fixed_demands[t] == 0) {
          continue;
        }

        // Task t consumes the resource at time if consume_condition is true.
        std::vector<Literal> consume_condition;
        const Literal consume = Literal(model->Add(NewBooleanVariable()), true);

        // Task t consumes the resource at time if it is present.
        if (intervals->IsOptional(vars[t])) {
          consume_condition.push_back(intervals->PresenceLiteral(vars[t]));
        }

        // Task t overlaps time.
        consume_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(start_vars[t], IntegerValue(time))));
        consume_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(end_vars[t],
                                           IntegerValue(time + 1))));

        model->Add(ReifiedBoolAnd(consume_condition, consume));

        // TODO(user): this is needed because we currently can't create a
        // boolean variable if the model is unsat.
        if (sat_solver->IsModelUnsat()) return;

        literals_with_coeff.push_back(
            LiteralWithCoeff(consume, Coefficient(fixed_demands[t].value())));
      }
      // The profile cannot exceed the capacity at time.
      sat_solver->AddLinearConstraint(false, Coefficient(0), true,
                                      fixed_capacity, &literals_with_coeff);

      // Abort if UNSAT.
      if (sat_solver->IsModelUnsat()) return;
    }
  };
}

std::function<void(Model*)> CumulativeUsingReservoir(
    const std::vector<IntervalVariable>& vars,
    const std::vector<AffineExpression>& demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=](Model* model) {
    if (vars.empty()) return;

    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    auto* intervals = model->GetOrCreate<IntervalsRepository>();

    CHECK(integer_trail->IsFixed(capacity));
    const IntegerValue fixed_capacity(
        integer_trail->UpperBound(capacity).value());

    std::vector<AffineExpression> times;
    std::vector<IntegerValue> deltas;
    std::vector<Literal> presences;

    const int num_tasks = vars.size();
    for (int t = 0; t < num_tasks; ++t) {
      CHECK(integer_trail->IsFixed(demands[t]));
      times.push_back(intervals->StartVar(vars[t]));
      deltas.push_back(integer_trail->LowerBound(demands[t]));
      times.push_back(intervals->EndVar(vars[t]));
      deltas.push_back(-integer_trail->LowerBound(demands[t]));
      if (intervals->IsOptional(vars[t])) {
        presences.push_back(intervals->PresenceLiteral(vars[t]));
        presences.push_back(intervals->PresenceLiteral(vars[t]));
      } else {
        presences.push_back(encoder->GetTrueLiteral());
        presences.push_back(encoder->GetTrueLiteral());
      }
    }
    AddReservoirConstraint(times, deltas, presences, 0, fixed_capacity.value(),
                           model);
  };
}

}  // namespace sat
}  // namespace operations_research
