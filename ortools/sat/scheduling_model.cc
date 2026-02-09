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
#include <iterator>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/cp_model.pb.h"
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
    std::vector<std::pair<IntegerVariable, int>> edges;
    for (const int interval_idx : terminal_intervals) {
      const IntervalConstraintProto& interval =
          model_proto.constraints(interval_idx).interval();
      if (interval.end().vars().size() == 1) {
        const IntegerVariable end_var =
            IntegerVariable(2 * interval.end().vars(0));
        if (interval.end().coeffs(0) > 0) {
          edges.push_back({end_var, interval_idx});
        } else {
          edges.push_back({NegationOf(end_var), interval_idx});
        }
      }
    }
    intervals_by_end_var.ResetFromPairs(edges, model_proto.variables().size());
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
    int64_t offset = std::numeric_limits<int64_t>::min();
    for (const Bounds& bound : bounds) {
      if (bound.var == makespan_var_coeff->first &&
          bound.coeff == makespan_var_coeff->second) {
        offset = std::max(offset, bound.offset);
      }
    }
    problem_and_mapping.makespan_expr = {makespan_var_coeff->first,
                                         makespan_var_coeff->second, offset};
    VLOG(2) << "Detected makespan: " << makespan_var_coeff->first << " * "
            << makespan_var_coeff->second << " + " << offset;
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
      const int64_t duration = GetExprMin(interval.size(), model_proto);
      if (duration <= 0) {
        continue;
      }
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
           interval.start().offset()});
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
  return problem_and_mapping;
}

SchedulingRelaxation DetectSchedulingProblems(const CpModelProto& model_proto) {
  // We will first detect a single large satisfaction job-shop problem, then
  // we will try to detect one or more makespans that appear in the objective
  // and split the problem accordingly.
  std::vector<std::vector<int>> machine_to_tasks;
  BestBinaryRelationBounds precedences;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.has_no_overlap()) {
      machine_to_tasks.emplace_back(ct.no_overlap().intervals().begin(),
                                    ct.no_overlap().intervals().end());
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

  CompactVectorVector<IntegerVariable, int> intervals_by_end_var;
  {
    std::vector<std::pair<IntegerVariable, int>> edges;
    for (const std::vector<int>& intervals : machine_to_tasks) {
      for (const int interval_idx : intervals) {
        const IntervalConstraintProto& interval =
            model_proto.constraints(interval_idx).interval();
        if (interval.end().vars().size() != 1) continue;
        const IntegerVariable end_var =
            IntegerVariable(2 * interval.end().vars(0));
        if (interval.end().coeffs(0) > 0) {
          edges.push_back({end_var, interval_idx});
        } else {
          edges.push_back({NegationOf(end_var), interval_idx});
        }
      }
    }
    intervals_by_end_var.ResetFromPairs(edges, model_proto.variables().size());
  }

  CompactVectorVector<IntegerVariable, int> intervals_by_start_var;
  {
    std::vector<std::pair<IntegerVariable, int>> edges;
    for (const std::vector<int>& intervals : machine_to_tasks) {
      for (const int interval_idx : intervals) {
        const IntervalConstraintProto& interval =
            model_proto.constraints(interval_idx).interval();
        if (interval.start().vars().size() != 1) continue;
        const IntegerVariable start_var =
            IntegerVariable(2 * interval.start().vars(0));
        if (interval.start().coeffs(0) > 0) {
          edges.push_back({start_var, interval_idx});
        } else {
          edges.push_back({NegationOf(start_var), interval_idx});
        }
      }
    }
    intervals_by_start_var.ResetFromPairs(edges,
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

  VLOG(2) << "Detected " << interval_precedences.size()
          << " interval_precedences.";

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
  VLOG(2) << "Detected " << component_roots.size() << " job-shop sub-problems.";

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
    if (relaxation.problems_and_mappings.back().makespan_expr.has_value()) {
      makespan_vars.insert(
          relaxation.problems_and_mappings.back().makespan_expr->var);
    }
  }
  SchedulingRelaxation::RelaxedObjective& relaxed_objective =
      relaxation.relaxed_objective;
  relaxed_objective.offset = model_proto.objective().offset();
  for (int i = 0; i < model_proto.objective().vars().size(); ++i) {
    if (makespan_vars.contains(model_proto.objective().vars(i))) {
      relaxed_objective.var_in_problem_makespan.push_back(
          model_proto.objective().vars(i));
      relaxed_objective.coeff.push_back(model_proto.objective().coeffs(i));
    } else {
      const int var = model_proto.objective().vars(i);
      if (model_proto.objective().coeffs(i) > 0) {
        relaxed_objective.offset += model_proto.objective().coeffs(i) *
                                    model_proto.variables(var).domain(0);
      } else {
        relaxed_objective.offset +=
            model_proto.objective().coeffs(i) *
            model_proto.variables(var).domain(
                model_proto.variables(var).domain().size() - 1);
      }
    }
  }
  relaxation.relaxed_objective.offset = model_proto.objective().offset();
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
        return false;
      }
    }
  }

  // Now, check that at no time a machine is used by more than one task.
  absl::flat_hash_map<int, absl::btree_map<int64_t, int64_t>>
      machine_to_intervals;
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
    if (!machine_to_intervals[task.machine]
             .insert({start_time, end_time})
             .second) {
      return false;
    }
  }
  for (const auto& [machine, intervals] : machine_to_intervals) {
    auto it = intervals.begin();
    while (it != intervals.end()) {
      const int64_t end_time = it->second;
      const auto next_it = std::next(it);
      if (next_it == intervals.end()) break;
      if (next_it->first < end_time) {
        return false;
      }
      it = next_it;
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
    const int64_t coeff = relaxation.relaxed_objective.coeff[i];
    *relaxed_objective_value += coeff * solution[var];
  }
  VLOG(2) << "Scheduling relaxation verified with objective value: "
          << *relaxed_objective_value;
  return true;
}

}  // namespace sat
}  // namespace operations_research
