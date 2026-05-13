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

#include "ortools/sat/scheduling_model.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/graph_base/topologicalsorter.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

namespace {

LinearExpression2 GetLinearExpression2FromProto(int a, int64_t coeff_a, int b,
                                                int64_t coeff_b) {
  LinearExpression2 result;
  DCHECK_GE(a, 0);
  DCHECK_GE(b, 0);
  result.vars[0] = IntegerVariable(2 * a);
  result.vars[1] = IntegerVariable(2 * b);
  result.coeffs[0] = IntegerValue(coeff_a);
  result.coeffs[1] = IntegerValue(coeff_b);
  return result;
}

int64_t GetExprMin(const LinearExpressionProto& expr,
                   const CpModelProto& model_proto) {
  int64_t val = expr.offset();
  for (int i = 0; i < expr.vars().size(); i++) {
    if (expr.coeffs(i) > 0) {
      val += expr.coeffs(i) * model_proto.variables(expr.vars(i)).domain(0);
    } else {
      const IntegerVariableProto& v = model_proto.variables(expr.vars(i));
      val += expr.coeffs(i) * v.domain(v.domain().size() - 1);
    }
  }
  return val;
}

}  // namespace

SchedulingProblemAndMapping BuildSchedulingProblemAndMapping(
    const std::vector<std::vector<int>>& machine_to_tasks,
    const std::vector<std::pair<int, int>>& task_precedences,
    const BestBinaryRelationBounds& vars_precedences,
    const CpModelProto& model_proto) {
  // Detect the makespan.
  int64_t problem_start = kint64max;
  for (const std::vector<int>& machine_intervals : machine_to_tasks) {
    for (int interval_idx : machine_intervals) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      problem_start =
          std::min(problem_start, GetExprMin(interval.start(), model_proto));
    }
  }

  // First, let's check all the intervals that are not succeeded by any other
  // interval.
  absl::flat_hash_set<int> seen_tasks;
  for (const auto& precedence : task_precedences) {
    seen_tasks.insert(precedence.first);
  }
  std::vector<int> terminal_intervals;
  for (const std::vector<int>& machine_intervals : machine_to_tasks) {
    for (int interval_idx : machine_intervals) {
      if (seen_tasks.insert(interval_idx).second) {
        terminal_intervals.push_back(interval_idx);
      }
    }
  }

  VLOG(2) << "Detected " << terminal_intervals.size()
          << " intervals that can be last.";

  CompactVectorVector<IntegerVariable, int> intervals_by_end_var;
  {
    CompactVectorVectorBuilder<IntegerVariable, int>
        intervals_by_end_var_builder;
    intervals_by_end_var_builder.ReserveNumItems(terminal_intervals.size());
    for (const int interval_idx : terminal_intervals) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      if (interval.end().vars().size() == 1) {
        const IntegerVariable end_var =
            IntegerVariable(2 * interval.end().vars(0));
        if (interval.end().coeffs(0) > 0) {
          intervals_by_end_var_builder.Add(end_var, interval_idx);
        } else {
          intervals_by_end_var_builder.Add(NegationOf(end_var), interval_idx);
        }
      }
    }
    intervals_by_end_var.ResetFromBuilder(intervals_by_end_var_builder,
                                          model_proto.variables().size());
  }

  struct Bounds {
    // intervals[interval_idx].end() <= var * coeff + offset
    int interval_idx;
    int var;
    int64_t coeff;
    int64_t offset;
  };
  std::vector<Bounds> bounds;
  for (const std::pair<LinearExpression2, IntegerValue>& bound :
       vars_precedences.GetSortedNonTrivialUpperBounds()) {
    if (bound.first.vars[0] == kNoIntegerVariable ||
        bound.first.vars[1] == kNoIntegerVariable) {
      continue;
    }
    for (int i = 0; i < 2; ++i) {
      const IntegerVariable var = bound.first.vars[i];
      const int proto_var = GetPositiveOnlyIndex(var).value();
      const IntegerValue var_proto_coeff = VariableIsPositive(var)
                                               ? bound.first.coeffs[i]
                                               : -bound.first.coeffs[i];
      const IntegerVariable other_var = bound.first.vars[1 - i];
      const IntegerValue other_coeff = bound.first.coeffs[i];
      const int other_proto_var = GetPositiveOnlyIndex(other_var).value();
      const int64_t other_proto_coeff = VariableIsPositive(other_var)
                                            ? other_coeff.value()
                                            : -other_coeff.value();
      if (var >= intervals_by_end_var.size()) continue;
      for (int interval_idx : intervals_by_end_var[var]) {
        const IntervalConstraintProto& interval =
            model_proto.constraints(interval_idx).interval();
        if (!model_proto.constraints(interval_idx)
                 .enforcement_literal()
                 .empty()) {
          continue;
        }
        if (interval.end().vars().size() != 1) continue;
        const int64_t interval_end_coeff = interval.end().coeffs(0);
        if (interval_end_coeff % var_proto_coeff != 0) continue;
        const int64_t factor = interval_end_coeff / var_proto_coeff.value();
        DCHECK_EQ(interval.end().vars(0), proto_var);
        bounds.push_back(
            Bounds{interval_idx, other_proto_var, -other_proto_coeff * factor,
                   bound.second.value() * factor + interval.end().offset()});
      }
    }
  }
  absl::flat_hash_map<std::pair<int, int64_t>, int>
      potential_var_coeff_bounds_to_interval_counts;
  absl::flat_hash_set<std::tuple<int, int64_t, int>> seen_bounds;
  for (const Bounds& bound : bounds) {
    if (seen_bounds.insert({bound.var, bound.coeff, bound.interval_idx})
            .second) {
      potential_var_coeff_bounds_to_interval_counts[{bound.var, bound.coeff}]++;
    }
  }
  VLOG(2) << "Detected " << bounds.size() << " upper bounds.";
  std::optional<std::pair<int, int64_t>> makespan_var_coeff;
  for (const auto& [var_coeff, count] :
       potential_var_coeff_bounds_to_interval_counts) {
    if (count == terminal_intervals.size()) {
      makespan_var_coeff = var_coeff;
    }
  }
  SchedulingProblemAndMapping problem_and_mapping;
  problem_and_mapping.problem.type = SchedulingProblem::kSatisfaction;
  if (makespan_var_coeff.has_value()) {
    int64_t offset = kint64min;
    for (const Bounds& bound : bounds) {
      if (bound.var == makespan_var_coeff->first &&
          bound.coeff == makespan_var_coeff->second) {
        offset = std::max(offset, bound.offset);
      }
    }
    problem_and_mapping.makespan_expr = {makespan_var_coeff->first,
                                         makespan_var_coeff->second,
                                         offset - problem_start};
    VLOG(2) << "Detected makespan: " << makespan_var_coeff->first << " * "
            << makespan_var_coeff->second << " + " << offset - problem_start;
    for (int i = 0; i < model_proto.objective().vars().size(); ++i) {
      if (model_proto.objective().vars(i) == makespan_var_coeff->first) {
        if (model_proto.objective().coeffs(i) > 0 ==
            makespan_var_coeff->second > 0) {
          problem_and_mapping.problem.type =
              SchedulingProblem::kMinimizeMakespan;
          VLOG(2) << "Detected minimize makespan.";
        }
      }
    }
  }

  absl::flat_hash_map<int, int> interval_to_task_index;
  for (int m = 0; m < machine_to_tasks.size(); ++m) {
    const std::vector<int>& machine_intervals = machine_to_tasks[m];
    for (int interval_idx : machine_intervals) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      const int64_t duration =
          std::max(int64_t{0}, GetExprMin(interval.size(), model_proto));
      const int task_idx = problem_and_mapping.problem.tasks.size();
      if (!interval_to_task_index.insert({interval_idx, task_idx}).second) {
        // TODO(user): support "recipes" where a task must occupy more than
        // one machine to complete.
        continue;
      }
      if (interval.start().vars().size() != 1) {
        continue;
      }
      problem_and_mapping.task_to_start_time_model_var.push_back(
          {interval.start().vars(0), interval.start().coeffs(0),
           interval.start().offset() - problem_start});
      SchedulingProblem::Task& task =
          problem_and_mapping.problem.tasks.emplace_back();
      task.machine = m;
      task.duration = duration;
    }
  }
  for (const auto& precedence : task_precedences) {
    const int from_task = interval_to_task_index[precedence.first];
    const int to_task = interval_to_task_index[precedence.second];
    problem_and_mapping.problem.tasks[to_task]
        .tasks_that_must_complete_before_this.push_back(from_task);
  }
  for (int i = 0; i < problem_and_mapping.problem.tasks.size(); ++i) {
    gtl::STLSortAndRemoveDuplicates(&problem_and_mapping.problem.tasks[i]
                                         .tasks_that_must_complete_before_this);
  }
  return problem_and_mapping;
}

std::vector<std::pair<int, int>> DetectIntervalPrecedences(
    const CpModelProto& model_proto,
    const BestBinaryRelationBounds& precedences,
    absl::Span<const int> interval_indices) {
  CompactVectorVector<IntegerVariable, int> intervals_by_end_var;
  {
    CompactVectorVectorBuilder<IntegerVariable, int>
        intervals_by_end_var_builder;
    for (const int interval_idx : interval_indices) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      if (interval.end().vars().size() != 1) continue;
      const IntegerVariable end_var =
          IntegerVariable(2 * interval.end().vars(0));
      if (interval.end().coeffs(0) > 0) {
        intervals_by_end_var_builder.Add(end_var, interval_idx);
      } else {
        intervals_by_end_var_builder.Add(NegationOf(end_var), interval_idx);
      }
    }
    intervals_by_end_var.ResetFromBuilder(intervals_by_end_var_builder,
                                          model_proto.variables().size());
  }

  CompactVectorVector<IntegerVariable, int> intervals_by_start_var;
  {
    CompactVectorVectorBuilder<IntegerVariable, int>
        intervals_by_start_var_builder;
    for (const int interval_idx : interval_indices) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      if (interval.start().vars().size() != 1) continue;
      const IntegerVariable start_var =
          IntegerVariable(2 * interval.start().vars(0));
      if (interval.start().coeffs(0) > 0) {
        intervals_by_start_var_builder.Add(start_var, interval_idx);
      } else {
        intervals_by_start_var_builder.Add(NegationOf(start_var), interval_idx);
      }
    }
    intervals_by_start_var.ResetFromBuilder(intervals_by_start_var_builder,
                                            model_proto.variables().size());
  }

  std::vector<std::pair<int, int>> interval_precedences;

  for (const std::pair<LinearExpression2, IntegerValue>& bound :
       precedences.GetSortedNonTrivialUpperBounds()) {
    if (bound.first.vars[0] == kNoIntegerVariable ||
        bound.first.vars[1] == kNoIntegerVariable) {
      continue;
    }
    for (int i = 0; i < 2; ++i) {
      // Let's look for interval1.start > interval2.end, thus
      // interval2.end - interval1.start <= 0.
      const IntegerVariable var = bound.first.vars[i];
      const IntegerVariable other_var = bound.first.vars[1 - i];
      if (NegationOf(var) >= intervals_by_start_var.size() ||
          other_var >= intervals_by_end_var.size()) {
        continue;
      }
      for (int interval_idx : intervals_by_start_var[NegationOf(var)]) {
        const IntervalConstraintProto& interval =
            model_proto.constraints(interval_idx).interval();
        if (interval.start().vars().size() != 1) continue;
        if (!model_proto.constraints(interval_idx)
                 .enforcement_literal()
                 .empty()) {
          continue;
        }
        const LinearExpressionProto& interval_start = interval.start();
        for (int other_interval_idx : intervals_by_end_var[other_var]) {
          if (other_interval_idx == interval_idx) continue;
          const LinearExpressionProto& other_interval_end =
              model_proto.constraints(other_interval_idx).interval().end();
          if (!model_proto.constraints(other_interval_idx)
                   .enforcement_literal()
                   .empty()) {
            continue;
          }
          if (other_interval_end.vars().size() != 1) continue;

          // Do we know if other_interval.end <= interval.start ?
          const LinearExpression2 expr2 = GetLinearExpression2FromProto(
              other_interval_end.vars(0), other_interval_end.coeffs(0),
              interval_start.vars(0), -interval_start.coeffs(0));
          const IntegerValue lb = kMinIntegerValue;
          const IntegerValue ub(interval_start.offset() -
                                other_interval_end.offset());
          const RelationStatus status = precedences.GetStatus(expr2, lb, ub);
          if (status == RelationStatus::IS_TRUE) {
            // We have a precedence relation.
            interval_precedences.push_back({other_interval_idx, interval_idx});
          }
        }
      }
    }
  }

  return interval_precedences;
}

SchedulingRelaxation DetectSchedulingProblems(const CpModelProto& model_proto) {
  // We will first detect a single large satisfaction job-shop problem, then
  // we will try to detect one or more makespans that appear in the objective
  // and split the problem accordingly.
  std::vector<std::vector<int>> machine_to_tasks;
  std::vector<int> interval_indices;
  BestBinaryRelationBounds precedences;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.has_no_overlap()) {
      std::vector<int>& tasks = machine_to_tasks.emplace_back();
      for (int interval_idx : ct.no_overlap().intervals()) {
        if (model_proto.constraints(interval_idx)
                .enforcement_literal()
                .empty()) {
          tasks.push_back(interval_idx);
          interval_indices.push_back(interval_idx);
        }
      }
      if (tasks.empty()) machine_to_tasks.pop_back();
    }
    if (ct.has_linear() && ct.linear().vars().size() == 2) {
      const LinearConstraintProto& lin = ct.linear();
      const LinearExpression2 expr2 = GetLinearExpression2FromProto(
          lin.vars(0), lin.coeffs(0), lin.vars(1), lin.coeffs(1));
      const IntegerValue lb(lin.domain(0));
      const IntegerValue ub(lin.domain(lin.domain().size() - 1));

      precedences.Add(expr2, lb, ub);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&interval_indices);

  const std::vector<std::pair<int, int>> interval_precedences =
      DetectIntervalPrecedences(model_proto, precedences, interval_indices);

  VLOG(2) << "Detected " << interval_precedences.size()
          << " interval precedences.";
  if (interval_precedences.empty()) {
    return SchedulingRelaxation();
  }

  // Now that we have the interval precedences, we can imagine a graph where the
  // nodes are the intervals and the edges are the precedences. We can then
  // detect cycles in the graph (which means that the problem is unsat!).
  // Moreover, if we add edges connecting the tasks executed on the same
  // machine and split the graph into connected components, every component
  // will be an independent job-shop problem.
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(model_proto.constraints().size());
  for (const auto& precedence : interval_precedences) {
    union_find.AddEdge(precedence.first, precedence.second);
  }
  for (const std::vector<int>& intervals : machine_to_tasks) {
    for (int i = 1; i < intervals.size(); ++i) {
      union_find.AddEdge(intervals[i], intervals[0]);
    }
  }
  absl::btree_set<int> component_roots;
  for (const std::vector<int>& intervals : machine_to_tasks) {
    component_roots.insert(union_find.FindRoot(intervals[0]));
  }

  struct PerComponentData {
    std::vector<std::vector<int>> machine_to_tasks;
    std::vector<std::pair<int, int>> precedences;
  };
  std::vector<PerComponentData> per_component_data;
  per_component_data.reserve(component_roots.size());
  absl::flat_hash_map<int, int> root_to_index;
  for (int root : component_roots) {
    root_to_index[root] = per_component_data.size();
    per_component_data.emplace_back();
  }
  for (const auto& precedence : interval_precedences) {
    const int root = union_find.FindRoot(precedence.first);
    const auto it = root_to_index.find(root);
    if (it == root_to_index.end()) continue;
    per_component_data[it->second].precedences.push_back(precedence);
  }
  for (const std::vector<int>& intervals : machine_to_tasks) {
    const int root = union_find.FindRoot(intervals[0]);
    const auto it = root_to_index.find(root);
    CHECK(it != root_to_index.end());
    per_component_data[it->second].machine_to_tasks.push_back(intervals);
  }

  SchedulingRelaxation relaxation;
  absl::flat_hash_set<int> makespan_vars;
  for (int i = 0; i < per_component_data.size(); ++i) {
    const PerComponentData& data = per_component_data[i];
    relaxation.problems_and_mappings.emplace_back(
        BuildSchedulingProblemAndMapping(
            data.machine_to_tasks, data.precedences, precedences, model_proto));
    const SchedulingProblemAndMapping& problem_and_mapping =
        relaxation.problems_and_mappings.back();
    const int num_machines =
        absl::c_max_element(problem_and_mapping.problem.tasks,
                            [](const SchedulingProblem::Task& a,
                               const SchedulingProblem::Task& b) {
                              return a.machine < b.machine;
                            })
            ->machine +
        1;
    if (problem_and_mapping.problem.tasks.size() < 3 || num_machines < 2) {
      relaxation.problems_and_mappings.pop_back();
      continue;
    }
    if (relaxation.problems_and_mappings.back().makespan_expr.has_value()) {
      makespan_vars.insert(
          relaxation.problems_and_mappings.back().makespan_expr->var);
    }
  }

  VLOG(2) << "Detected " << relaxation.problems_and_mappings.size()
          << " job-shop sub-problems.";

  if (relaxation.problems_and_mappings.empty()) {
    return relaxation;
  }

  SchedulingRelaxation::RelaxedObjective& relaxed_objective =
      relaxation.relaxed_objective;
  relaxed_objective.offset = 0;
  for (int i = 0; i < model_proto.objective().vars().size(); ++i) {
    const int var = model_proto.objective().vars(i);
    const int64_t coeff = model_proto.objective().coeffs(i);
    if (makespan_vars.contains(var)) {
      relaxed_objective.var_in_problem_makespan.push_back(var);
      relaxed_objective.coeff.push_back(coeff);
    } else {
      if (coeff > 0) {
        relaxed_objective.offset +=
            coeff * model_proto.variables(var).domain(0);
      } else {
        relaxed_objective.offset +=
            coeff * model_proto.variables(var).domain(
                        model_proto.variables(var).domain().size() - 1);
      }
    }
  }
  return relaxation;
}

bool VerifySingleSchedulingProblem(
    const SchedulingProblemAndMapping& relaxation,
    absl::Span<const int64_t> solution) {
  VLOG(2) << "Verifying Scheduling problem with "
          << relaxation.problem.tasks.size() << " tasks.";

  // First, check task precedences.
  for (int task_idx = 0; task_idx < relaxation.problem.tasks.size();
       ++task_idx) {
    const SchedulingProblem::Task& task = relaxation.problem.tasks[task_idx];
    const int start_time_var =
        relaxation.task_to_start_time_model_var[task_idx].var;
    const int64_t start_time =
        solution[start_time_var] *
            relaxation.task_to_start_time_model_var[task_idx].coeff +
        relaxation.task_to_start_time_model_var[task_idx].offset;
    for (int before_task : task.tasks_that_must_complete_before_this) {
      const int before_task_end_var =
          relaxation.task_to_start_time_model_var[before_task].var;
      const int64_t before_task_end_time =
          solution[before_task_end_var] *
              relaxation.task_to_start_time_model_var[before_task].coeff +
          relaxation.task_to_start_time_model_var[before_task].offset +
          relaxation.problem.tasks[before_task].duration;
      if (start_time < before_task_end_time) {
        VLOG(2) << "Task " << relaxation.problem.tasks[before_task]
                << " does not complete before task " << task;
        return false;
      }
    }
  }

  // Now, check that at no time a machine is used by more than one task.
  std::vector<std::vector<std::pair<int64_t, int64_t>>> machine_intervals(
      relaxation.problem.tasks.size());
  for (int task_idx = 0; task_idx < relaxation.problem.tasks.size();
       ++task_idx) {
    const SchedulingProblem::Task& task = relaxation.problem.tasks[task_idx];
    const int start_time_var =
        relaxation.task_to_start_time_model_var[task_idx].var;
    const int64_t start_time =
        solution[start_time_var] *
            relaxation.task_to_start_time_model_var[task_idx].coeff +
        relaxation.task_to_start_time_model_var[task_idx].offset;
    const int64_t end_time = start_time + task.duration;
    machine_intervals[task.machine].push_back({start_time, end_time});
  }
  for (int machine = 0; machine < machine_intervals.size(); ++machine) {
    auto& intervals = machine_intervals[machine];
    absl::c_sort(intervals);  // Sorts by start_time ascending
    for (int i = 1; i < intervals.size(); ++i) {
      if (intervals[i - 1].second > intervals[i].first) {
        VLOG(2) << "Overlap detected on machine " << machine
                << " at task with "
                   "time interval ["
                << intervals[i - 1].first << ", " << intervals[i - 1].second
                << "] and task with time interval [" << intervals[i].first
                << ", " << intervals[i].second << "]";
        return false;
      }
    }
  }
  VLOG(2) << "Scheduling problem verified.";
  return true;
}

CpModelProto BuildSchedulingModel(const SchedulingProblem& problem) {
  CpModelProto model_proto;
  int64_t horizon = 0;
  int num_machines = 0;
  for (const SchedulingProblem::Task& task : problem.tasks) {
    num_machines = std::max(num_machines, task.machine + 1);
    horizon += task.duration;
  }
  std::vector<NoOverlapConstraintProto*> no_overlap_constraints(num_machines);
  for (int i = 0; i < num_machines; ++i) {
    no_overlap_constraints[i] =
        model_proto.add_constraints()->mutable_no_overlap();
  }
  const int num_tasks = problem.tasks.size();
  std::vector<int> task_idx_to_interval_idx(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    IntegerVariableProto* var = model_proto.add_variables();
    var->add_domain(0);
    var->add_domain(horizon);
    task_idx_to_interval_idx[i] = model_proto.constraints().size();
    IntervalConstraintProto* interval =
        model_proto.add_constraints()->mutable_interval();
    interval->mutable_start()->add_vars(i);
    interval->mutable_start()->add_coeffs(1);
    interval->mutable_size()->set_offset(problem.tasks[i].duration);
    interval->mutable_end()->add_vars(i);
    interval->mutable_end()->add_coeffs(1);
    interval->mutable_end()->set_offset(problem.tasks[i].duration);
    no_overlap_constraints[problem.tasks[i].machine]->add_intervals(
        task_idx_to_interval_idx[i]);
  }
  for (int task_idx = 0; task_idx < num_tasks; ++task_idx) {
    const SchedulingProblem::Task& current_task = problem.tasks[task_idx];
    for (int preceding_task_idx :
         current_task.tasks_that_must_complete_before_this) {
      // preceding_task.start + preceding_task.duration <= current_task.start
      LinearConstraintProto* linear =
          model_proto.add_constraints()->mutable_linear();
      linear->add_vars(task_idx);
      linear->add_coeffs(-1);
      linear->add_vars(preceding_task_idx);
      linear->add_coeffs(1);
      linear->add_domain(-2 * horizon);
      linear->add_domain(-problem.tasks[preceding_task_idx].duration);
    }
  }
  const int makespan_var = model_proto.variables().size();
  IntegerVariableProto* var = model_proto.add_variables();
  var->add_domain(0);
  var->add_domain(horizon);
  for (int task_idx = 0; task_idx < num_tasks; ++task_idx) {
    // task.start + task.duration <= makespan
    LinearConstraintProto* linear =
        model_proto.add_constraints()->mutable_linear();
    linear->add_vars(task_idx);
    linear->add_coeffs(1);
    linear->add_vars(makespan_var);
    linear->add_coeffs(-1);
    linear->add_domain(-2 * horizon);
    linear->add_domain(-problem.tasks[task_idx].duration);
  }
  if (problem.type == SchedulingProblem::kMinimizeMakespan) {
    model_proto.mutable_objective()->add_vars(makespan_var);
    model_proto.mutable_objective()->add_coeffs(1);
  }
  return model_proto;
}

bool VerifySchedulingRelaxation(const SchedulingRelaxation& relaxation,
                                absl::Span<const int64_t> solution,
                                int64_t* relaxed_objective_value) {
  // First, check task precedences.
  for (const SchedulingProblemAndMapping& problem_and_mapping :
       relaxation.problems_and_mappings) {
    if (!VerifySingleSchedulingProblem(problem_and_mapping, solution)) {
      return false;
    }
  }
  *relaxed_objective_value = relaxation.relaxed_objective.offset;
  for (int i = 0;
       i < relaxation.relaxed_objective.var_in_problem_makespan.size(); ++i) {
    const int var = relaxation.relaxed_objective.var_in_problem_makespan[i];
    const int64_t global_coeff = relaxation.relaxed_objective.coeff[i];

    int64_t var_lower_bound = kint64min;

    for (const auto& problem_and_mapping : relaxation.problems_and_mappings) {
      if (!problem_and_mapping.makespan_expr.has_value() ||
          problem_and_mapping.makespan_expr->var != var) {
        continue;
      }
      // 1. Calculate the true local makespan directly from the tasks
      int64_t problem_makespan = 0;
      for (int task_idx = 0;
           task_idx < problem_and_mapping.problem.tasks.size(); ++task_idx) {
        const int start_time_var =
            problem_and_mapping.task_to_start_time_model_var[task_idx].var;
        const int64_t start_time =
            solution[start_time_var] *
                problem_and_mapping.task_to_start_time_model_var[task_idx]
                    .coeff +
            problem_and_mapping.task_to_start_time_model_var[task_idx].offset;
        problem_makespan = std::max(
            problem_makespan,
            start_time + problem_and_mapping.problem.tasks[task_idx].duration);
      }

      // 2. Translate the local task makespan to the global variable space.
      // global_var * coeff + offset >= problem_makespan
      // global_var >= (problem_makespan - offset) / coeff
      const int64_t diff =
          problem_makespan - problem_and_mapping.makespan_expr->offset;
      const int64_t local_coeff = problem_and_mapping.makespan_expr->coeff;

      // Use integer ceiling division to ensure the lower bound remains
      // strictly valid (assuming local_coeff > 0 for a minimize makespan
      // relation)
      const int64_t translated_bound =
          (diff > 0) ? (diff + local_coeff - 1) / local_coeff
                     : diff / local_coeff;

      var_lower_bound = std::max(var_lower_bound, translated_bound);
    }
    *relaxed_objective_value += global_coeff * var_lower_bound;
  }
  VLOG(2) << "Scheduling relaxation verified with objective value: "
          << *relaxed_objective_value;
  return true;
}

bool ProbableSplitExists(const GraphForPartition& graph) {
  const int kSamples = 64;

  if (graph.num_primary_nodes <= kSamples) {
    return true;  // Too small for the heuristic to be useful.
  }

  std::vector<int> primary_topo_indices;
  primary_topo_indices.reserve(graph.num_primary_nodes);
  for (int i = 0; i < graph.num_nodes; ++i) {
    if (graph.topological_order[i] < graph.num_primary_nodes) {
      primary_topo_indices.push_back(i);
    }
  }

  std::vector<int> sampled_nodes;
  sampled_nodes.reserve(kSamples);
  const int num_p = primary_topo_indices.size();

  // Pick kNumSamples nodes following the following rules:
  // 1. Always pick the first and last primary nodes in topological order. This
  //    ensures we cannot miss something that always follows or always precedes
  //    the sampled nodes.
  // 2. Sample remaining nodes well distributed across the topological
  //    order, biased towards lower degree nodes (note that a node of degree
  //    zero immediately proves the graph is unsplittable).

  sampled_nodes.push_back(primary_topo_indices.front());
  for (int i = 1; i < kSamples - 1; ++i) {
    const int start_idx = (i * (num_p - 1)) / (kSamples - 1);
    const int end_idx =
        std::max(((i + 1) * (num_p - 1)) / (kSamples - 1), start_idx + 1);

    int best_topo = primary_topo_indices[start_idx];
    int min_degree = graph.adj[graph.topological_order[best_topo]].size();

    for (int j = start_idx + 1; j < end_idx; ++j) {
      const int node_topo = primary_topo_indices[j];
      const int node_degree =
          graph.adj[graph.topological_order[node_topo]].size();
      if (node_degree < min_degree) {
        min_degree = node_degree;
        best_topo = node_topo;
      }
    }
    sampled_nodes.push_back(best_topo);
  }
  sampled_nodes.push_back(primary_topo_indices.back());

  // Map original node IDs to their new sampled node IDs.
  std::vector<int> node_to_sampled(graph.num_nodes, -1);
  for (int i = 0; i < kSamples; ++i) {
    node_to_sampled[graph.topological_order[sampled_nodes[i]]] = i;
  }

  // Build the connectivity of the sampled graph by computing reachability
  // within the original graph.
  CompactVectorVector<int> sampled_adj;
  sampled_adj.reserve(kSamples, kSamples * 4);
  std::vector<bool> visited;

  for (int i = 0; i < kSamples; ++i) {
    sampled_adj.Add({});
    visited.assign(graph.num_nodes, false);

    const int start_node = graph.topological_order[sampled_nodes[i]];
    visited[start_node] = true;

    for (int t = sampled_nodes[i]; t < graph.num_nodes; ++t) {
      const int u = graph.topological_order[t];
      if (!visited[u]) continue;

      if (u != start_node && node_to_sampled[u] != -1) {
        sampled_adj.AppendToLastVector(node_to_sampled[u]);
        continue;  // We don't need to build the transitive closure.
      }

      for (const int c : graph.adj[u]) {
        visited[c] = true;
      }
    }
  }

  std::vector<int> sampled_topo(kSamples);
  std::iota(sampled_topo.begin(), sampled_topo.end(), 0);

  GraphForPartition sampled_graph{/*num_nodes=*/kSamples,
                                  /*num_primary_nodes=*/kSamples,
                                  /*adj=*/sampled_adj,
                                  /*topological_order=*/sampled_topo};

  return PartitionByIncomparabilityExact(sampled_graph).size() > 1;
}

CompactVectorVector<int> PartitionByIncomparabilityExact(
    const GraphForPartition& graph) {
  // The first step to solve this problem is to notice that the partitions must
  // be contiguous in the topological order. Thus, we need to find right "cut"
  // nodes to split the topological order into our partitions. To do that, we
  // write a helper lambda that for a given node finds the last node that is
  // unreachable from it downstream in the topological order. The important
  // trick is that this last unreachable node is also a lower bound of where the
  // next cut can be made. The straightforward solution would be then to iterate
  // on current_node starting from 0 and get the maximum of those lower bounds
  // until we get to the point where lower_bound >= current_node; when that
  // happens, we make a cut and start a new partition. We do something that is
  // similar to that, but searching backwards: we start from the first node,
  // compute the current lower_bound from its last unreachable primary node, and
  // then iterate backward from lower_bound-1 to 0. If we find a new
  // lower_bound, jump again to the new lower_bound-1 and iterate backward. To
  // avoid exploring the same region twice we keep a vector of
  // next_to_check[node] that holds how far backward from node has already been
  // checked. The advantage of searching backwards is to optimistically prove
  // that the graph in unsplittable with a few large increments of lower_bound
  // or, if the graph is splittable, to quickly jump to the next split position.

  if (graph.num_primary_nodes == 0) {
    return CompactVectorVector<int>();
  }
  // For simplicity, we re-index the graph so its indexes correspond to the
  // topological order.
  std::vector<bool> is_primary(graph.num_nodes, false);
  int last_primary_node = -1;
  CompactVectorVector<int> adj;  // Adjacency list indexed by topological order.
  {
    std::vector<int> inverse_topo(graph.num_nodes);
    for (int i = 0; i < graph.num_nodes; ++i) {
      const bool is_primary_node =
          graph.topological_order[i] < graph.num_primary_nodes;
      is_primary[i] = is_primary_node;
      inverse_topo[graph.topological_order[i]] = i;

      if (is_primary_node) {
        last_primary_node = i;
      }
    }

    adj.reserve(graph.num_nodes, graph.adj.num_entries());
    for (int i = 0; i < graph.num_nodes; ++i) {
      adj.Add({});
      for (const int c : graph.adj[graph.topological_order[i]]) {
        adj.AppendToLastVector(inverse_topo[c]);
      }
    }
  }

  std::vector<bool> is_reachable(graph.num_nodes, false);
  int work_done = 0;

  // Computes the last unreachable primary node starting from `node` and
  // assuming all primary nodes in the range [node, assume_reachable_up_to] can
  // reach the whole graph below them in the topological order. Returns `node`
  // if it can reach the whole graph below it or the last unreachable primary
  // node otherwise.
  auto compute_last_unreachable_primary = [&](int node,
                                              int assume_reachable_up_to) {
    is_reachable.assign(graph.num_nodes, false);
    is_reachable[node] = true;

    // Populate is_reachable
    for (int j = node; j <= last_primary_node; ++j) {
      if (!is_reachable[j]) continue;
      for (const int c : adj[j]) {
        if (c <= assume_reachable_up_to && is_primary[c]) {
          // We hit a child that can reach the rest of the graph, so we can
          // reach all those nodes from `node`. Stop.
          return assume_reachable_up_to;
        }
        is_reachable[c] = true;
      }
      work_done += 1 + adj[j].size();
    }

    // Find the last primary among the unreachable.
    int last_unreachable_primary = node;
    for (int j = last_primary_node; j > node; --j) {
      if (!is_reachable[j] && is_primary[j]) {
        last_unreachable_primary = j;
        break;
      }
    }
    return last_unreachable_primary;
  };

  std::vector<int> primary_to_check;
  primary_to_check.reserve(graph.num_primary_nodes);

  CompactVectorVector<int> result;
  int last_cut_position = -1;
  int start_node = 0;

  // Limit to avoid quadratic explosion on large inputs.
  constexpr int kWorkLimit = 100'000'000;

  while (start_node <= last_primary_node) {
    // Fast-forward to the next primary node
    while (start_node <= last_primary_node && !is_primary[start_node]) {
      start_node++;
    }
    if (start_node > last_primary_node) break;

    int proposed_cut = compute_last_unreachable_primary(start_node, -1);

    // For each partition we start by checking all the primary nodes up to
    // proposed_cut. Note that elements in primary_to_check are always in
    // increasing order.
    primary_to_check.clear();
    for (int i = start_node + 1; i <= proposed_cut; ++i) {
      if (is_primary[i]) {
        primary_to_check.push_back(i);
      }
    }

    // Verify backwards down to start_node
    while (!primary_to_check.empty() && proposed_cut < last_primary_node) {
      const int node_to_check = primary_to_check.back();
      primary_to_check.pop_back();

      // Note that we already checked all the nodes in the range
      // [node_to_check, proposed_cut] since we are iterating backwards from
      // proposed_cut.
      const int lup =
          compute_last_unreachable_primary(node_to_check, proposed_cut);

      if (work_done > kWorkLimit) {
        // Jump to the end to write the current partition and stop.
        proposed_cut = last_primary_node;
      }

      if (lup > proposed_cut) {
        // The boundary expanded! Push the newly exposed primary nodes onto the
        // stack after the existing ones.
        for (int i = proposed_cut + 1; i <= lup; ++i) {
          if (is_primary[i]) {
            primary_to_check.push_back(i);
          }
        }
        proposed_cut = lup;
      }
    }

    // Write the partition translating back to original node IDs.
    result.Add({});
    for (int i = last_cut_position + 1; i <= proposed_cut; ++i) {
      if (is_primary[i]) {
        result.AppendToLastVector(graph.topological_order[i]);
      }
    }

    // Advance the lower_bound and prepare for the next partition.
    last_cut_position = proposed_cut;
    start_node = proposed_cut + 1;
  }

  return result;
}

// Runs the DAG Incomparability Partitioning heuristics.
CompactVectorVector<int> PartitionByIncomparability(
    int num_nodes, int num_primary_nodes, const CompactVectorVector<int>& adj) {
  auto maybe_topo = util::graph::FastTopologicalSort(adj);
  if (!maybe_topo.ok()) {
    VLOG(2) << "PartitionByIncomparability found a cycle!";
    CompactVectorVector<int> trivial_partition;
    trivial_partition.Add({});
    for (int i = 0; i < num_primary_nodes; ++i) {
      trivial_partition.AppendToLastVector(i);
    }

    return trivial_partition;
  }

  const GraphForPartition graph{num_nodes, num_primary_nodes, adj,
                                maybe_topo.value()};

  // Quick heuristic to avoid expensive quadratic computation for most
  // unsplittable graphs.
  if (!ProbableSplitExists(graph)) {
    VLOG(2) << "Quick heuristic deemed graph unsplittable.";
    CompactVectorVector<int> result;
    result.Add({});
    for (int i = 0; i < num_primary_nodes; ++i) {
      result.AppendToLastVector(i);
    }
    return result;
  }

  const CompactVectorVector<int> result =
      PartitionByIncomparabilityExact(graph);

  if (result.size() == 1 && result[0].size() == num_primary_nodes) {
    VLOG(2) << "PartitionByIncomparability found graph is non-splittable.";
  } else {
    VLOG(2) << "PartitionByIncomparability found: ["
            << absl::StrJoin(result.AsVectorOfSpan(), ",",
                             [](std::string* out, absl::Span<const int> v) {
                               absl::StrAppend(out, v.size());
                             })
            << "]";
  }
  return result;
}

CompactVectorVector<int> IntervalsNonOverlappingComponents(
    absl::Span<const IndexedInterval> intervals,
    const std::vector<std::pair<int, int>>& precedences) {
  // We want to map the problem of splitting our intervals into a pure graph
  // theory problem. To do that, we augment the precedence graph with extra
  // nodes and edges to represent the fact that start and end times imposes a
  // precedence on the intervals.
  //
  // The construction is as follows:
  //
  // 1. Nodes:
  //    - [0, N-1]: The actual Intervals.
  //    - [N, N+K-1]: "Arrival" time nodes (t_in). Represents the exact moment
  //    of time t.
  //    - [N+K, N+2K-1]: "Departure" time nodes (t_out). Represents time moving
  //    forward from t.
  //
  // 2. The time backbone edges:
  //    We chain time points together: Arrival(t) -> Departure(t) ->
  //    Arrival(t+1)
  //
  // 3. The interval time edges:
  //    An interval spanning from start_time to end_time is hooked as:
  //    Departure(start_time) -> [Interval] -> Arrival(end_time)
  //
  // We need two nodes per time point (start and end) to handle zero-duration
  // intervals: a zero-duration interval Z at time t must happen after an
  // interval ending at t, and before an interval starting at t. By splitting
  // time t into Arr(t) and Dep(t), we can hook zero-duration intervals exactly
  // in the middle:
  //    Arrival(t) -> [Zero-Duration Interval] -> Departure(t)
  //
  // VISUALIZATION:
  // Consider I1 [t0..t1], I2 [t1..t2], and Z [t1..t1]
  //
  //         Arr(t0) -> Dep(t0) ---> Arr(t1) -> Dep(t1) ---> Arr(t2) -> Dep(t2)
  //                      |            ^  |       ^  |            ^
  //                      |    [I1]    |  |  [Z]  |  |    [I2]    |
  //                      +------------+  +-------+  +------------+
  //
  // Notice the paths:
  // - I1 reaches I2 via: I1 -> Arr(t1) -> Dep(t1) -> I2. (Guaranteed A finishes
  // before B starts).
  // - I1 reaches Z via: I1 -> Arr(t1) -> Z.
  // - Z reaches I2 via: Z -> Dep(t1) -> I2.
  // - If I3 was [t0..t2], it would bypass t1 entirely, meaning it cannot reach
  //   I1,  and I1 cannot reach it.

  const int N = intervals.size();
  if (N <= 1) {
    CompactVectorVector<int> res;
    for (const auto& inv : intervals) {
      res.Add({inv.index});
    }
    return res;
  }

  std::vector<int64_t> times;
  times.reserve(N * 2);
  for (const auto& inv : intervals) {
    times.push_back(inv.start.value());
    times.push_back(inv.end.value());
  }
  gtl::STLSortAndRemoveDuplicates(&times);

  const int K = times.size();
  const int first_t_in_index = N;
  const int first_t_out_index = N + K;
  const int total_nodes = N + 2 * K;

  absl::flat_hash_map<int64_t, int> time_to_index(times.size());
  for (int i = 0; i < times.size(); ++i) {
    time_to_index[times[i]] = i + first_t_in_index;
  }

  CompactVectorVectorBuilder<int> adj;
  adj.ReserveNumItems(2 * K + 2 * N + precedences.size());

  for (int i = 0; i < K; ++i) {
    const int t_in_index = first_t_in_index + i;
    const int t_out_index = first_t_out_index + i;
    adj.Add(t_in_index, t_out_index);
    if (i < K - 1) {
      const int next_t_in_index = first_t_in_index + i + 1;
      adj.Add(t_out_index, next_t_in_index);
    }
  }

  for (int i = 0; i < N; ++i) {
    const bool is_zero_duration =
        intervals[i].start.value() == intervals[i].end.value();
    const int t_in_start_index = time_to_index[intervals[i].start.value()];
    const int t_in_end_index = time_to_index[intervals[i].end.value()];
    const int t_out_start_index = t_in_start_index + K;
    const int t_out_end_index = t_in_end_index + K;

    if (is_zero_duration) {
      adj.Add(t_in_start_index, i);
      adj.Add(i, t_out_end_index);
    } else {
      adj.Add(t_out_start_index, i);
      adj.Add(i, t_in_end_index);
    }
  }

  absl::flat_hash_map<int, int> id_to_internal;
  for (int i = 0; i < N; ++i) {
    id_to_internal[intervals[i].index] = i;
  }
  for (const auto& prec : precedences) {
    auto it1 = id_to_internal.find(prec.first);
    auto it2 = id_to_internal.find(prec.second);
    if (it1 != id_to_internal.end() && it2 != id_to_internal.end()) {
      adj.Add(it1->second, it2->second);
    }
  }

  const CompactVectorVector<int> adj_vec(adj, total_nodes);

  const auto partitions = PartitionByIncomparability(total_nodes, N, adj_vec);

  CompactVectorVector<int> result;

  std::vector<int> mapped;
  for (int i = 0; i < partitions.size(); ++i) {
    mapped.clear();
    for (const int u : partitions[i]) {
      // Only keep nodes that represent intervals
      if (u < N) mapped.push_back(intervals[u].index);
    }
    if (!mapped.empty()) {
      result.Add(mapped);
    }
  }

  if (result.size() == 1 && result[0].size() == N) {
    VLOG(2) << "IntervalsNonOverlappingComponents: " << N << " intervals and "
            << precedences.size()
            << " precedences and the found graph is non-splittable.";
  } else {
    VLOG(2) << "IntervalsNonOverlappingComponents: " << N << " intervals and "
            << precedences.size() << " precedences, components=["
            << absl::StrJoin(result.AsVectorOfSpan(), ",",
                             [](std::string* out, absl::Span<const int> v) {
                               absl::StrAppend(out, v.size());
                             })
            << "]";
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
