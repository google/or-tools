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

#include "ortools/sat/cumulative.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cumulative_energy.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/timetable.h"
#include "ortools/sat/timetable_edgefinding.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Cumulative(
    const std::vector<IntervalVariable>& vars,
    absl::Span<const AffineExpression> demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=, demands = std::vector<AffineExpression>(
                 demands.begin(), demands.end())](Model* model) mutable {
    auto* intervals = model->GetOrCreate<IntervalsRepository>();
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();

    if (!integer_trail->SafeEnqueue(capacity.GreaterOrEqual(0), {})) {
      sat_solver->NotifyThatModelIsUnsat();
    }
    if (demands.empty()) {
      // If there is no demand, since we already added a constraint that the
      // capacity is not negative above, we can stop here.
      return;
    }

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
      if (intervals->MinSize(vars[i]) <= 0) {
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
    if (parameters.use_disjunctive_constraint_in_cumulative()) {
      // TODO(user): We need to exclude intervals that can be of size zero
      // because the disjunctive do not "ignore" them like the cumulative
      // does. That is, the interval [2,2) will be assumed to be in
      // disjunction with [1, 3) for instance. We need to uniformize the
      // handling of interval with size zero.
      std::vector<IntervalVariable> in_disjunction;
      IntegerValue min_of_demands = kMaxIntegerValue;
      const IntegerValue capa_max = integer_trail->UpperBound(capacity);
      for (int i = 0; i < vars.size(); ++i) {
        const IntegerValue size_min = intervals->MinSize(vars[i]);
        if (size_min == 0) continue;
        const IntegerValue demand_min = integer_trail->LowerBound(demands[i]);
        if (2 * demand_min > capa_max) {
          in_disjunction.push_back(vars[i]);
          min_of_demands = std::min(min_of_demands, demand_min);
        }
      }

      // Liftable? We might be able to add one more interval!
      if (!in_disjunction.empty()) {
        IntervalVariable lift_var;
        IntegerValue lift_size(0);
        for (int i = 0; i < vars.size(); ++i) {
          const IntegerValue size_min = intervals->MinSize(vars[i]);
          if (size_min == 0) continue;
          const IntegerValue demand_min = integer_trail->LowerBound(demands[i]);
          if (2 * demand_min > capa_max) continue;
          if (min_of_demands + demand_min > capa_max && size_min > lift_size) {
            lift_var = vars[i];
            lift_size = size_min;
          }
        }
        if (lift_size > 0) {
          in_disjunction.push_back(lift_var);
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
      if (in_disjunction.size() > 1) AddDisjunctive(in_disjunction, model);
      if (in_disjunction.size() == vars.size()) return;
    }

    if (helper == nullptr) {
      helper = intervals->GetOrCreateHelper(vars);
    }
    SchedulingDemandHelper* demands_helper =
        intervals->GetOrCreateDemandHelper(helper, demands);
    intervals->RegisterCumulative({capacity, helper, demands_helper});

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
    if (parameters.use_hard_precedences_in_cumulative()) {
      // The CumulativeIsAfterSubsetConstraint() always reset the helper to the
      // forward time direction, so it is important to also precompute the
      // precedence relation using the same direction! This is needed in case
      // the helper has already been used and set in the other direction.
      if (!helper->SynchronizeAndSetTimeDirection(true)) {
        model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
        return;
      }

      std::vector<IntegerVariable> index_to_end_vars;
      std::vector<int> index_to_task;
      index_to_end_vars.clear();
      for (int t = 0; t < helper->NumTasks(); ++t) {
        const AffineExpression& end_exp = helper->Ends()[t];

        // TODO(user): Handle generic affine relation?
        if (end_exp.var == kNoIntegerVariable || end_exp.coeff != 1) continue;
        index_to_end_vars.push_back(end_exp.var);
        index_to_task.push_back(t);
      }

      // TODO(user): This can lead to many constraints. By analyzing a bit more
      // the precedences, we could restrict that. In particular for cases were
      // the cumulative is always (bunch of tasks B), T, (bunch of tasks A) and
      // task T always in the middle, we never need to explicit list the
      // precedence of a task in B with a task in A.
      //
      // TODO(user): If more than one variable are after the same set of
      // intervals, we should regroup them in a single constraint rather than
      // having two independent constraint doing the same propagation.
      std::vector<FullIntegerPrecedence> full_precedences;
      if (parameters.exploit_all_precedences()) {
        model->GetOrCreate<PrecedenceRelations>()->ComputeFullPrecedences(
            index_to_end_vars, &full_precedences);
      }
      for (const FullIntegerPrecedence& data : full_precedences) {
        const int size = data.indices.size();
        if (size <= 1) continue;

        const IntegerVariable var = data.var;
        std::vector<int> subtasks;
        std::vector<IntegerValue> offsets;
        IntegerValue sum_of_demand_max(0);
        for (int i = 0; i < size; ++i) {
          const int t = index_to_task[data.indices[i]];
          subtasks.push_back(t);
          sum_of_demand_max += integer_trail->LevelZeroUpperBound(demands[t]);

          // We have var >= end_exp.var + offset, so
          // var >= (end_exp.var + end_exp.cte) + (offset - end_exp.cte)
          // var >= task end + new_offset.
          const AffineExpression& end_exp = helper->Ends()[t];
          offsets.push_back(data.offsets[i] - end_exp.constant);
        }
        if (sum_of_demand_max > integer_trail->LevelZeroLowerBound(capacity)) {
          VLOG(2) << "Cumulative precedence constraint! var= " << var
                  << " #task: " << absl::StrJoin(subtasks, ",");
          CumulativeIsAfterSubsetConstraint* constraint =
              new CumulativeIsAfterSubsetConstraint(var, capacity, subtasks,
                                                    offsets, helper,
                                                    demands_helper, model);
          constraint->RegisterWith(watcher);
          model->TakeOwnership(constraint);
        }
      }
    }

    // Propagator responsible for applying Timetabling filtering rule. It
    // increases the minimum of the start variables, decrease the maximum of the
    // end variables, and increase the minimum of the capacity variable.
    TimeTablingPerTask* time_tabling =
        new TimeTablingPerTask(capacity, helper, demands_helper, model);
    time_tabling->RegisterWith(watcher);
    model->TakeOwnership(time_tabling);

    // Propagator responsible for applying the Overload Checking filtering rule.
    // It increases the minimum of the capacity variable.
    if (parameters.use_overload_checker_in_cumulative()) {
      AddCumulativeOverloadChecker(capacity, helper, demands_helper, model);
    }
    if (parameters.use_conservative_scale_overload_checker()) {
      // Since we use the potential DFF conflict on demands to apply the
      // heuristic, only do so if any demand is greater than 1.
      bool any_demand_greater_than_one = false;
      for (int i = 0; i < vars.size(); ++i) {
        const IntegerValue demand_min = integer_trail->LowerBound(demands[i]);
        if (demand_min > 1) {
          any_demand_greater_than_one = true;
          break;
        }
      }
      if (any_demand_greater_than_one) {
        AddCumulativeOverloadCheckerDff(capacity, helper, demands_helper,
                                        model);
      }
    }

    // Propagator responsible for applying the Timetable Edge finding filtering
    // rule. It increases the minimum of the start variables and decreases the
    // maximum of the end variables,
    if (parameters.use_timetable_edge_finding_in_cumulative() &&
        helper->NumTasks() <=
            parameters.max_num_intervals_for_timetable_edge_finding()) {
      TimeTableEdgeFinding* time_table_edge_finding =
          new TimeTableEdgeFinding(capacity, helper, demands_helper, model);
      time_table_edge_finding->RegisterWith(watcher);
      model->TakeOwnership(time_table_edge_finding);
    }
  };
}

std::function<void(Model*)> CumulativeTimeDecomposition(
    absl::Span<const IntervalVariable> vars,
    absl::Span<const AffineExpression> demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=, vars = std::vector<IntervalVariable>(vars.begin(), vars.end()),
          demands = std::vector<AffineExpression>(
              demands.begin(), demands.end())](Model* model) {
    if (vars.empty()) return;

    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    CHECK(integer_trail->IsFixed(capacity));
    const Coefficient fixed_capacity(
        integer_trail->UpperBound(capacity).value());

    const int num_tasks = vars.size();
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

    std::vector<AffineExpression> start_exprs;
    std::vector<AffineExpression> end_exprs;
    std::vector<IntegerValue> fixed_demands;

    for (int t = 0; t < num_tasks; ++t) {
      start_exprs.push_back(repository->Start(vars[t]));
      end_exprs.push_back(repository->End(vars[t]));
      CHECK(integer_trail->IsFixed(demands[t]));
      fixed_demands.push_back(integer_trail->LowerBound(demands[t]));
    }

    // Compute time range.
    IntegerValue min_start = kMaxIntegerValue;
    IntegerValue max_end = kMinIntegerValue;
    for (int t = 0; t < num_tasks; ++t) {
      min_start =
          std::min(min_start, integer_trail->LowerBound(start_exprs[t]));
      max_end = std::max(max_end, integer_trail->UpperBound(end_exprs[t]));
    }

    for (IntegerValue time = min_start; time < max_end; ++time) {
      std::vector<LiteralWithCoeff> literals_with_coeff;
      for (int t = 0; t < num_tasks; ++t) {
        if (!sat_solver->Propagate()) return;
        const IntegerValue start_min =
            integer_trail->LowerBound(start_exprs[t]);
        const IntegerValue end_max = integer_trail->UpperBound(end_exprs[t]);
        if (end_max <= time || time < start_min || fixed_demands[t] == 0) {
          continue;
        }

        // Task t consumes the resource at time if consume_condition is true.
        std::vector<Literal> consume_condition;
        const Literal consume = Literal(model->Add(NewBooleanVariable()), true);

        // Task t consumes the resource at time if it is present.
        if (repository->IsOptional(vars[t])) {
          consume_condition.push_back(repository->PresenceLiteral(vars[t]));
        }

        // Task t overlaps time.
        consume_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
            start_exprs[t].LowerOrEqual(IntegerValue(time))));
        consume_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
            end_exprs[t].GreaterOrEqual(IntegerValue(time + 1))));

        model->Add(ReifiedBoolAnd(consume_condition, consume));

        // this is needed because we currently can't create a boolean variable
        // if the model is unsat.
        if (sat_solver->ModelIsUnsat()) return;

        literals_with_coeff.push_back(
            LiteralWithCoeff(consume, Coefficient(fixed_demands[t].value())));
      }
      // The profile cannot exceed the capacity at time.
      sat_solver->AddLinearConstraint(false, Coefficient(0), true,
                                      fixed_capacity, &literals_with_coeff);

      // Abort if UNSAT.
      if (sat_solver->ModelIsUnsat()) return;
    }
  };
}

std::function<void(Model*)> CumulativeUsingReservoir(
    absl::Span<const IntervalVariable> vars,
    absl::Span<const AffineExpression> demands, AffineExpression capacity,
    SchedulingConstraintHelper* helper) {
  return [=, vars = std::vector<IntervalVariable>(vars.begin(), vars.end()),
          demands = std::vector<AffineExpression>(
              demands.begin(), demands.end())](Model* model) {
    if (vars.empty()) return;

    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* encoder = model->GetOrCreate<IntegerEncoder>();
    auto* repository = model->GetOrCreate<IntervalsRepository>();

    CHECK(integer_trail->IsFixed(capacity));
    const IntegerValue fixed_capacity(
        integer_trail->UpperBound(capacity).value());

    std::vector<AffineExpression> times;
    std::vector<AffineExpression> deltas;
    std::vector<Literal> presences;

    const int num_tasks = vars.size();
    for (int t = 0; t < num_tasks; ++t) {
      CHECK(integer_trail->IsFixed(demands[t]));
      times.push_back(repository->Start(vars[t]));
      deltas.push_back(demands[t]);
      times.push_back(repository->End(vars[t]));
      deltas.push_back(demands[t].Negated());
      if (repository->IsOptional(vars[t])) {
        presences.push_back(repository->PresenceLiteral(vars[t]));
        presences.push_back(repository->PresenceLiteral(vars[t]));
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
