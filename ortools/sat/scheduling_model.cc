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
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"

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

struct IntervalAffine {
  int interval_index;
  SchedulingProblemAndMapping::AffineExpr affine;

  bool operator<(const IntervalAffine& o) const {
    return std::tie(affine.var, affine.coeff, interval_index, affine.offset) <
           std::tie(o.affine.var, o.affine.coeff, o.interval_index,
                    o.affine.offset);
  }

  bool operator==(const IntervalAffine& o) const {
    return std::tie(affine.var, affine.coeff, interval_index, affine.offset) ==
           std::tie(o.affine.var, o.affine.coeff, o.interval_index,
                    o.affine.offset);
  }
};

// Detects when the makespan is modeled as a physical "barrier" interval at the
// end of a NoOverlap constraint. Returns the interval indexes (to skip during
// task building) alongside the affine expression of the makespan.
std::vector<IntervalAffine> DetectImplicitBarrierMakespans(
    const CpModelProto& model_proto) {
  int64_t horizon = kint64min;
  for (const auto& var : model_proto.variables()) {
    if (!var.domain().empty()) {
      horizon = std::max(horizon, var.domain(var.domain().size() - 1));
    }
  }

  std::vector<IntervalAffine> barriers;
  for (int i = 0; i < model_proto.constraints_size(); ++i) {
    const auto& ct = model_proto.constraints(i);
    if (!ct.has_no_overlap()) continue;

    for (int interval_idx : ct.no_overlap().intervals()) {
      const auto& interval_ct = model_proto.constraints(interval_idx);
      if (!interval_ct.enforcement_literal().empty()) continue;

      const auto& interval = interval_ct.interval();
      if (GetExprMin(interval.end(), model_proto) >= horizon &&
          GetExprMin(interval.size(), model_proto) > 0) {
        if (interval.start().vars().size() == 1) {
          barriers.push_back(
              IntervalAffine{interval_idx,
                             {.var = interval.start().vars(0),
                              .coeff = interval.start().coeffs(0),
                              .offset = interval.start().offset()}});
        }
      }
    }
  }
  return barriers;
}

struct VariableBoundsGroup {
  int var;
  int64_t coeff;
  int64_t max_offset;
  std::vector<int> bounded_intervals;
};

// Detects a makespan as a variable that is made to precede the end of all tasks
// by using precedence constraints.
std::optional<SchedulingProblemAndMapping::AffineExpr>
DetectMakespanFromPrecedenceGraph(
    const std::vector<VariableBoundsGroup>& grouped_bounds,
    const SchedulingProblemAndMapping& problem_and_mapping,
    int64_t problem_start) {
  SparseBitset<int> visited(problem_and_mapping.problem.tasks.size());
  std::vector<int> stack;

  for (const VariableBoundsGroup& group : grouped_bounds) {
    if (group.coeff <= 0) continue;

    std::vector<int> explicitly_bounded_tasks;
    for (int t = 0; t < problem_and_mapping.problem.tasks.size(); ++t) {
      const auto& task_intervals = problem_and_mapping.task_to_intervals[t];

      auto is_bounded = [&](int idx) {
        return absl::c_binary_search(group.bounded_intervals, idx);
      };

      bool is_task_bounded = false;
      if (absl::c_any_of(task_intervals.unconditional_intervals, is_bounded)) {
        is_task_bounded = true;
      } else if (!task_intervals.alternative_intervals.empty()) {
        is_task_bounded = true;
        for (int alt : task_intervals.alternative_intervals) {
          if (!is_bounded(alt)) {
            is_task_bounded = false;
            break;
          }
        }
      }

      if (is_task_bounded) explicitly_bounded_tasks.push_back(t);
    }

    if (explicitly_bounded_tasks.empty()) continue;

    visited.ResetAllToFalse();
    stack = explicitly_bounded_tasks;
    for (int t : stack) visited.Set(t);

    int reached_count = 0;
    while (!stack.empty()) {
      const int curr = stack.back();
      stack.pop_back();
      reached_count++;

      for (int prev : problem_and_mapping.problem.tasks[curr]
                          .tasks_that_must_complete_before_this) {
        if (!visited[prev]) {
          visited.Set(prev);
          stack.push_back(prev);
        }
      }
    }

    if (reached_count == problem_and_mapping.problem.tasks.size()) {
      const int64_t exact_offset =
          group.max_offset == kint64min ? problem_start : group.max_offset;

      return SchedulingProblemAndMapping::AffineExpr{
          .var = group.var, .coeff = group.coeff, .offset = exact_offset};
    }
  }
  return std::nullopt;
}

// Parses CP-SAT constraints and returns a flat, grouped structure of all
// variables acting as an upper bound to intervals.
std::vector<VariableBoundsGroup> ExtractUpperBounds(
    const CpModelProto& model_proto,
    const BestBinaryRelationBounds& vars_precedences,
    const absl::flat_hash_map<int, int>& interval_to_machine) {
  CompactVectorVector<IntegerVariable, int> intervals_by_end_var;
  {
    CompactVectorVectorBuilder<IntegerVariable, int> builder;
    builder.ReserveNumItems(interval_to_machine.size() * 2);
    for (int i = 0; i < model_proto.constraints().size(); ++i) {
      if (!model_proto.constraints(i).has_interval()) continue;
      if (!model_proto.constraints(i).enforcement_literal().empty()) continue;

      const auto& interval = model_proto.constraints(i).interval();
      if (interval.end().vars().size() == 1) {
        const IntegerVariable end_var(2 * interval.end().vars(0));
        if (interval.end().coeffs(0) > 0)
          builder.Add(end_var, i);
        else
          builder.Add(NegationOf(end_var), i);
      }
    }
    intervals_by_end_var.ResetFromBuilder(builder,
                                          model_proto.variables().size());
  }

  std::vector<IntervalAffine> raw_bounds;
  for (const auto& [expr, offset] :
       vars_precedences.GetSortedNonTrivialUpperBounds()) {
    if (expr.vars[0] == kNoIntegerVariable ||
        expr.vars[1] == kNoIntegerVariable) {
      continue;
    }
    for (int i = 0; i < 2; ++i) {
      const IntegerVariable var = expr.vars[i];
      const int proto_var = GetPositiveOnlyIndex(var).value();
      const IntegerValue var_proto_coeff =
          VariableIsPositive(var) ? expr.coeffs[i] : -expr.coeffs[i];

      const IntegerVariable other_var = expr.vars[1 - i];
      const IntegerValue other_coeff = expr.coeffs[1 - i];
      const int other_proto_var = GetPositiveOnlyIndex(other_var).value();
      const int64_t other_proto_coeff = VariableIsPositive(other_var)
                                            ? other_coeff.value()
                                            : -other_coeff.value();

      if (var >= intervals_by_end_var.size()) continue;

      for (int interval_idx : intervals_by_end_var[var]) {
        const auto& interval = model_proto.constraints(interval_idx).interval();
        if (interval.end().vars().size() != 1) continue;

        const int64_t interval_end_coeff = interval.end().coeffs(0);
        if (interval_end_coeff % var_proto_coeff.value() != 0) continue;

        const int64_t factor = interval_end_coeff / var_proto_coeff.value();
        DCHECK_EQ(interval.end().vars(0), proto_var);

        raw_bounds.push_back(IntervalAffine{
            interval_idx,
            {.var = other_proto_var,
             .coeff = -other_proto_coeff * factor,
             .offset = offset.value() * factor + interval.end().offset()}});
      }
    }
  }

  gtl::STLSortAndRemoveDuplicates(&raw_bounds);

  // Group all intervals bounded by a specific variable
  std::vector<VariableBoundsGroup> groups;
  for (const IntervalAffine& bound : raw_bounds) {
    if (groups.empty() || groups.back().var != bound.affine.var ||
        groups.back().coeff != bound.affine.coeff) {
      groups.push_back({bound.affine.var, bound.affine.coeff, kint64min, {}});
    }

    // Only push unique interval indices into the contiguous group block
    if (groups.back().bounded_intervals.empty() ||
        groups.back().bounded_intervals.back() != bound.interval_index) {
      groups.back().bounded_intervals.push_back(bound.interval_index);
    }

    groups.back().max_offset =
        std::max(groups.back().max_offset, bound.affine.offset);
  }

  return groups;
}

// Helper to verify if two CP-SAT linear expressions are the same.
bool IsSameExpr(const LinearExpressionProto& a,
                const LinearExpressionProto& b) {
  if (a.offset() != b.offset()) return false;
  if (a.vars_size() != b.vars_size()) return false;
  for (int i = 0; i < a.vars_size(); ++i) {
    if (a.vars(i) != b.vars(i) || a.coeffs(i) != b.coeffs(i)) return false;
  }
  return true;
}

absl::flat_hash_map<int, int> BuildTasksAndIntervalMapping(
    const CpModelProto& model_proto,
    const std::vector<std::vector<int>>& task_to_intervals,
    const absl::flat_hash_map<int, int>& interval_to_machine,
    const absl::flat_hash_set<int>& barrier_intervals, int64_t problem_start,
    SchedulingProblemAndMapping* problem_and_mapping) {
  absl::flat_hash_map<int, int> interval_to_task_index;

  absl::flat_hash_map<int, std::vector<int>> start_var_to_intervals;
  for (int i = 0; i < model_proto.constraints_size(); ++i) {
    const auto& ct = model_proto.constraints(i);
    // Unrestricted collection: we discover dummy intervals even if they have
    // enforcement literals (critical for optional industrial tasks).
    if (ct.has_interval() && ct.interval().start().vars().size() == 1) {
      start_var_to_intervals[ct.interval().start().vars(0)].push_back(i);
    }
  }

  for (const std::vector<int>& alts : task_to_intervals) {
    if (absl::c_any_of(
            alts, [&](int idx) { return barrier_intervals.contains(idx); })) {
      continue;
    }

    const int task_idx = problem_and_mapping->problem.tasks.size();
    auto& task = problem_and_mapping->problem.tasks.emplace_back();
    problem_and_mapping->task_to_intervals.push_back({});
    auto& task_intervals = problem_and_mapping->task_to_intervals.back();
    std::vector<int> presence_literals;

    for (int interval_idx : alts) {
      interval_to_task_index[interval_idx] = task_idx;
      const auto& ct = model_proto.constraints(interval_idx);

      const bool is_conditional = !ct.enforcement_literal().empty();
      if (is_conditional) {
        task_intervals.alternative_intervals.push_back(interval_idx);
      } else {
        task_intervals.unconditional_intervals.push_back(interval_idx);
      }

      auto it = interval_to_machine.find(interval_idx);
      if (it != interval_to_machine.end()) {
        task.compatible_machine.push_back(it->second);
        task.duration_for_machine.push_back(std::max(
            int64_t{0}, GetExprMin(ct.interval().size(), model_proto)));
        presence_literals.push_back(is_conditional ? ct.enforcement_literal(0)
                                                   : kint32max);
      }
    }
    problem_and_mapping->task_to_presence_literals.push_back(presence_literals);

    const auto& first_interval = model_proto.constraints(alts[0]).interval();
    task.min_start =
        std::max(int64_t{0}, GetExprMin(first_interval.start(), model_proto) -
                                 problem_start);

    if (first_interval.start().vars().size() == 1) {
      int base_start_var = first_interval.start().vars(0);
      problem_and_mapping->task_to_start_time_model_var.push_back(
          {base_start_var, first_interval.start().coeffs(0),
           first_interval.start().offset() - problem_start});

      if (auto it = start_var_to_intervals.find(base_start_var);
          it != start_var_to_intervals.end()) {
        for (int i : it->second) {
          if (!barrier_intervals.contains(i) &&
              absl::c_find(alts, i) == alts.end()) {
            // GUARD 1: Never claim a physical machine interval
            if (interval_to_machine.contains(i)) continue;

            // GUARD 2: Restored from your original code! Never steal an
            // interval already claimed by another task. Prevents industrial
            // cannibalization.
            if (interval_to_task_index.contains(i)) continue;

            const auto& dummy_interval = model_proto.constraints(i).interval();

            // GUARD 3: Only check the start expression. CP-SAT gives main
            // intervals distinct end variables, so we cannot check
            // IsSameExpr(end).
            if (IsSameExpr(dummy_interval.start(), first_interval.start())) {
              task_intervals.unconditional_intervals.push_back(i);
              interval_to_task_index[i] = task_idx;
            }
          }
        }
      }
    } else if (first_interval.start().vars().empty()) {
      problem_and_mapping->task_to_start_time_model_var.push_back(
          {0, 0, first_interval.start().offset() - problem_start});
    } else {
      problem_and_mapping->task_to_start_time_model_var.push_back({0, 1, 0});
    }
  }
  return interval_to_task_index;
}

// Extracts direct precedence edges between actual scheduled tasks.
// This performs a localized DFS through the interval adjacency graph to route
// seamlessly around any dummy or routing intervals inserted by the presolver.
// Crucially, the search stops immediately upon reaching the next valid task,
// computing a fast graph contraction rather than an expensive $O(N^2)$ full
// transitive closure.
void ExtractDirectTaskPrecedences(
    const CpModelProto& model_proto,
    const std::vector<std::pair<int, int>>& interval_precedences,
    const absl::flat_hash_map<int, int>& interval_to_task_index,
    SchedulingProblemAndMapping* problem_and_mapping) {
  CompactVectorVectorBuilder<int> builder;
  builder.ReserveNumItems(interval_precedences.size());
  for (const auto& precedence : interval_precedences) {
    builder.Add(precedence.first, precedence.second);
  }
  CompactVectorVector<int> interval_adj;
  interval_adj.ResetFromBuilder(builder, model_proto.constraints_size());

  SparseBitset<int> visited(model_proto.constraints_size());
  std::vector<int> stack;

  for (int task_idx = 0; task_idx < problem_and_mapping->problem.tasks.size();
       ++task_idx) {
    visited.ResetAllToFalse();
    stack.clear();

    const auto& task_intervals =
        problem_and_mapping->task_to_intervals[task_idx];

    // Seed DFS from ALL intervals properly mapped to the task
    for (int interval_idx : task_intervals.alternative_intervals) {
      stack.push_back(interval_idx);
      visited.Set(interval_idx);
    }
    for (const int interval_idx : task_intervals.unconditional_intervals) {
      stack.push_back(interval_idx);
      visited.Set(interval_idx);
    }

    while (!stack.empty()) {
      const int curr = stack.back();
      stack.pop_back();

      for (int next : interval_adj[curr]) {
        if (visited[next]) continue;
        visited.Set(next);

        auto it = interval_to_task_index.find(next);
        if (it != interval_to_task_index.end()) {
          int target_task_idx = it->second;

          if (target_task_idx != task_idx) {
            problem_and_mapping->problem.tasks[target_task_idx]
                .tasks_that_must_complete_before_this.push_back(task_idx);
          }
          continue;  // Stop DFS gracefully at task boundaries
        }

        // Unmapped node: Safely traverse it! Since we protected against
        // cannibalization, the graph is trustworthy and won't cross-contaminate
        // bounds.
        stack.push_back(next);
      }
    }
  }

  for (int i = 0; i < problem_and_mapping->problem.tasks.size(); ++i) {
    gtl::STLSortAndRemoveDuplicates(&problem_and_mapping->problem.tasks[i]
                                         .tasks_that_must_complete_before_this);
  }
}

}  // namespace

SchedulingProblemAndMapping BuildSchedulingProblemAndMapping(
    const std::vector<std::vector<int>>& machine_to_intervals,
    const std::vector<std::vector<int>>& task_to_intervals,
    const std::vector<std::pair<int, int>>& interval_precedences,
    const BestBinaryRelationBounds& vars_precedences,
    const CpModelProto& model_proto) {
  const int64_t problem_start = 0;
  SchedulingProblemAndMapping problem_and_mapping;

  absl::flat_hash_map<int, int> interval_to_machine;
  for (int m = 0; m < machine_to_intervals.size(); ++m) {
    for (int interval_idx : machine_to_intervals[m]) {
      interval_to_machine[interval_idx] = m;
    }
  }

  // --- 1. Detect Implicit Barrier Makespans ---
  const std::vector<IntervalAffine> barriers =
      DetectImplicitBarrierMakespans(model_proto);
  absl::flat_hash_set<int> barrier_indices;
  std::optional<SchedulingProblemAndMapping::AffineExpr> proven_makespan;

  for (const auto& b : barriers) {
    barrier_indices.insert(b.interval_index);
    if (!proven_makespan) proven_makespan = b.affine;
  }

  // --- 2. Extract Upper Bounds ---
  std::vector<VariableBoundsGroup> extracted_bounds =
      ExtractUpperBounds(model_proto, vars_precedences, interval_to_machine);

  VLOG(2) << "Detected " << extracted_bounds.size() << " grouped upper bounds.";

  // --- 3. Build Tasks and Interval Mapping ---
  absl::flat_hash_map<int, int> interval_to_task_index =
      BuildTasksAndIntervalMapping(model_proto, task_to_intervals,
                                   interval_to_machine, barrier_indices,
                                   problem_start, &problem_and_mapping);

  // --- 4. Extract Direct Task Precedences ---
  ExtractDirectTaskPrecedences(model_proto, interval_precedences,
                               interval_to_task_index, &problem_and_mapping);

  // --- 5. Fallback: Precedence Graph Makespan Detection ---
  if (!proven_makespan.has_value()) {
    if (auto graph_result = DetectMakespanFromPrecedenceGraph(
            extracted_bounds, problem_and_mapping, problem_start)) {
      proven_makespan = *graph_result;
    }
  }

  // --- 6. Objective Classification & Mapping ---
  bool makespan_is_minimized = false;

  if (proven_makespan.has_value() && model_proto.has_objective()) {
    for (int i = 0; i < model_proto.objective().vars().size(); ++i) {
      const int obj_var = model_proto.objective().vars(i);
      const int64_t obj_coeff = model_proto.objective().coeffs(i);

      if (obj_var == proven_makespan->var) {
        makespan_is_minimized =
            ((obj_coeff > 0) == (proven_makespan->coeff > 0));
        break;
      }
    }
  }

  if (proven_makespan.has_value()) {
    problem_and_mapping.makespan_expr = *proven_makespan;
    problem_and_mapping.makespan_expr->offset -= problem_start;

    problem_and_mapping.problem.type =
        makespan_is_minimized ? SchedulingProblem::kMinimizeMakespan
                              : SchedulingProblem::kSatisfaction;

    VLOG(2) << "Makespan detected: I" << proven_makespan->var << " * "
            << proven_makespan->coeff << " + "
            << proven_makespan->offset - problem_start
            << (makespan_is_minimized ? " (minimized)"
                                      : " (not on the objective)");
  } else {
    VLOG(2) << "Could not map any variable to a makespan.";
    problem_and_mapping.problem.type = SchedulingProblem::kSatisfaction;
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
        const LinearExpressionProto& interval_start = interval.start();
        for (int other_interval_idx : intervals_by_end_var[other_var]) {
          if (other_interval_idx == interval_idx) continue;
          const LinearExpressionProto& other_interval_end =
              model_proto.constraints(other_interval_idx).interval().end();
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
  std::vector<std::vector<int>> machine_to_intervals;
  std::vector<int> interval_indices;
  absl::flat_hash_map<int, std::vector<int>> start_var_to_intervals;
  std::vector<int> all_intervals;
  BestBinaryRelationBounds precedences;

  std::vector<std::vector<int>> exact_ones_groups;

  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.has_no_overlap() && !ct.no_overlap().intervals().empty()) {
      std::vector<int>& intervals = machine_to_intervals.emplace_back();
      for (int interval_idx : ct.no_overlap().intervals()) {
        intervals.push_back(interval_idx);
        interval_indices.push_back(interval_idx);
      }
    }

    if (ct.has_linear() && ct.linear().vars().size() == 2) {
      const LinearConstraintProto& lin = ct.linear();
      const LinearExpression2 expr2 = GetLinearExpression2FromProto(
          lin.vars(0), lin.coeffs(0), lin.vars(1), lin.coeffs(1));
      const IntegerValue lb(lin.domain(0));
      const IntegerValue ub(lin.domain(lin.domain().size() - 1));

      precedences.Add(expr2, lb, ub);
    }

    if (ct.has_exactly_one()) {
      exact_ones_groups.push_back(
          std::vector<int>{ct.exactly_one().literals().begin(),
                           ct.exactly_one().literals().end()});
    }

    if (ct.has_interval()) {
      all_intervals.push_back(c);
      if (ct.enforcement_literal().empty() &&
          ct.interval().start().vars().size() == 1) {
        start_var_to_intervals[ct.interval().start().vars(0)].push_back(c);
      }
    }
  }
  gtl::STLSortAndRemoveDuplicates(&interval_indices);

  absl::flat_hash_map<int, int> literal_to_interval;
  for (int interval_idx : interval_indices) {
    const ConstraintProto& ct = model_proto.constraints(interval_idx);
    if (ct.enforcement_literal().size() == 1) {
      literal_to_interval[ct.enforcement_literal(0)] = interval_idx;
    }
  }

  std::vector<std::vector<int>> task_to_intervals;
  absl::flat_hash_set<int> interval_in_task;

  for (const auto& group : exact_ones_groups) {
    std::vector<int> alts;
    for (const int lit : group) {
      auto it = literal_to_interval.find(lit);
      if (it != literal_to_interval.end()) {
        alts.push_back(it->second);
      }
    }
    if (alts.size() > 1) {
      bool already_used = false;
      for (const int a : alts) {
        if (interval_in_task.contains(a)) already_used = true;
      }
      if (!already_used) {
        task_to_intervals.push_back(alts);
        for (const int a : alts) interval_in_task.insert(a);
      }
    }
  }

  for (auto& alts : task_to_intervals) {
    DCHECK(!alts.empty());
    const auto& first_interval = model_proto.constraints(alts[0]).interval();

    if (first_interval.start().vars().size() == 1) {
      const int base_start_var = first_interval.start().vars(0);

      if (auto it = start_var_to_intervals.find(base_start_var);
          it != start_var_to_intervals.end()) {
        for (const int i : it->second) {
          if (!interval_in_task.contains(i)) {
            alts.push_back(i);
            interval_in_task.insert(i);
          }
        }
      }
    }
  }

  // Handle the intervals that appears both with b and ¬b as a task that can be
  // executed on two machines.
  for (const auto& [lit, interval_idx] : literal_to_interval) {
    if (lit < 0) continue;
    auto it = literal_to_interval.find(NegatedRef(lit));
    if (it != literal_to_interval.end()) {
      int other_idx = it->second;
      if (!interval_in_task.contains(interval_idx) &&
          !interval_in_task.contains(other_idx)) {
        task_to_intervals.push_back({interval_idx, other_idx});
        interval_in_task.insert(interval_idx);
        interval_in_task.insert(other_idx);
      }
    }
  }

  // Handle the intervals that didn't appear in any exact one group as a task
  // with a single machine choice.
  for (int interval_idx : interval_indices) {
    if (!interval_in_task.contains(interval_idx)) {
      task_to_intervals.push_back({interval_idx});
      interval_in_task.insert(interval_idx);
    }
  }

  const std::vector<std::pair<int, int>> interval_precedences =
      DetectIntervalPrecedences(model_proto, precedences, all_intervals);

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
  for (const std::vector<int>& intervals : machine_to_intervals) {
    for (int i = 1; i < intervals.size(); ++i) {
      union_find.AddEdge(intervals[i], intervals[0]);
    }
  }
  for (const std::vector<int>& alts : task_to_intervals) {
    for (int i = 1; i < alts.size(); ++i) {
      union_find.AddEdge(alts[i], alts[0]);
    }
  }

  absl::btree_set<int> component_roots;
  for (const std::vector<int>& intervals : machine_to_intervals) {
    DCHECK(!intervals.empty());
    component_roots.insert(union_find.FindRoot(intervals[0]));
  }

  struct PerComponentData {
    std::vector<std::vector<int>> machine_to_intervals;
    std::vector<std::vector<int>> task_to_intervals;
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
  for (const std::vector<int>& intervals : machine_to_intervals) {
    const int root = union_find.FindRoot(intervals[0]);
    const auto it = root_to_index.find(root);
    CHECK(it != root_to_index.end());
    per_component_data[it->second].machine_to_intervals.push_back(intervals);
  }
  for (const std::vector<int>& alts : task_to_intervals) {
    const int root = union_find.FindRoot(alts[0]);
    const auto it = root_to_index.find(root);
    if (it != root_to_index.end()) {
      per_component_data[it->second].task_to_intervals.push_back(alts);
    }
  }

  SchedulingRelaxation relaxation;
  absl::flat_hash_set<int> makespan_vars;
  std::vector<int> num_machines_per_problem;
  std::vector<int> num_precedences_per_problem;
  std::vector<int> num_choices_per_problem;
  for (int i = 0; i < per_component_data.size(); ++i) {
    const PerComponentData& data = per_component_data[i];
    relaxation.problems_and_mappings.emplace_back(
        BuildSchedulingProblemAndMapping(
            data.machine_to_intervals, data.task_to_intervals, data.precedences,
            precedences, model_proto));
    const SchedulingProblemAndMapping& problem_and_mapping =
        relaxation.problems_and_mappings.back();

    int num_machines = 0;
    int num_precedences = 0;
    int num_choices = 0;
    for (const auto& task : problem_and_mapping.problem.tasks) {
      for (int m : task.compatible_machine) {
        num_machines = std::max(num_machines, m + 1);
      }
      num_precedences += task.tasks_that_must_complete_before_this.size();
      num_choices += task.compatible_machine.size();
    }

    if (problem_and_mapping.problem.tasks.size() < 3 || num_machines < 2) {
      relaxation.problems_and_mappings.pop_back();
      continue;
    }
    num_machines_per_problem.push_back(num_machines);
    num_precedences_per_problem.push_back(num_precedences);
    num_choices_per_problem.push_back(num_choices);
    if (relaxation.problems_and_mappings.back().makespan_expr.has_value()) {
      makespan_vars.insert(
          relaxation.problems_and_mappings.back().makespan_expr->var);
    }
  }

  VLOG(2) << "Detected " << relaxation.problems_and_mappings.size()
          << " job-shop sub-problems:";
  for (int i = 0; i < relaxation.problems_and_mappings.size(); ++i) {
    const SchedulingProblemAndMapping& problem_and_mapping =
        relaxation.problems_and_mappings[i];
    VLOG(2) << "  " << i << ": " << problem_and_mapping.problem.tasks.size()
            << " tasks, " << num_machines_per_problem[i] << " machines, "
            << num_precedences_per_problem[i] << " precedences and "
            << num_choices_per_problem[i] << " task-machine choices.";
  }

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

  auto lit_value = [&solution](int lit) {
    DCHECK_NE(lit, kint32max);
    const bool is_true = RefIsPositive(lit) ? (solution[lit] == 1)
                                            : (solution[NegatedRef(lit)] == 0);
    return is_true;
  };

  // First, check task precedences.
  for (int task_idx = 0; task_idx < relaxation.problem.tasks.size();
       ++task_idx) {
    const SchedulingProblem::Task& task = relaxation.problem.tasks[task_idx];

    // Skip if the task is not present.
    bool is_present = false;
    int active_machine_idx = 0;
    for (int a = 0; a < task.compatible_machine.size(); ++a) {
      int lit = relaxation.task_to_presence_literals[task_idx][a];
      if (lit == kint32max || lit_value(lit)) {
        active_machine_idx = a;
        is_present = true;
        break;
      }
    }
    if (!is_present) continue;

    const int start_time_var =
        relaxation.task_to_start_time_model_var[task_idx].var;
    const int64_t start_time =
        solution[start_time_var] *
            relaxation.task_to_start_time_model_var[task_idx].coeff +
        relaxation.task_to_start_time_model_var[task_idx].offset;

    for (int before_task : task.tasks_that_must_complete_before_this) {
      bool before_is_present = false;
      int before_active_machine_idx = 0;
      for (int a = 0;
           a < relaxation.problem.tasks[before_task].compatible_machine.size();
           ++a) {
        int lit = relaxation.task_to_presence_literals[before_task][a];
        if (lit == kint32max || lit_value(lit)) {
          before_active_machine_idx = a;
          before_is_present = true;
          break;
        }
      }
      if (!before_is_present) continue;

      const int before_task_end_var =
          relaxation.task_to_start_time_model_var[before_task].var;
      const int64_t before_task_end_time =
          solution[before_task_end_var] *
              relaxation.task_to_start_time_model_var[before_task].coeff +
          relaxation.task_to_start_time_model_var[before_task].offset +
          relaxation.problem.tasks[before_task]
              .duration_for_machine[before_active_machine_idx];
      if (start_time < before_task_end_time) {
        VLOG(2) << "Task " << relaxation.problem.tasks[before_task]
                << " does not complete before task " << task;
        return false;
      }
    }
  }

  int num_machines = 0;
  for (const auto& t : relaxation.problem.tasks) {
    for (int m : t.compatible_machine) {
      num_machines = std::max(num_machines, m + 1);
    }
  }

  // Now, check that at no time a machine is used by more than one task.
  std::vector<std::vector<std::pair<int64_t, int64_t>>> machine_intervals(
      num_machines);
  for (int task_idx = 0; task_idx < relaxation.problem.tasks.size();
       ++task_idx) {
    const SchedulingProblem::Task& task = relaxation.problem.tasks[task_idx];

    bool is_present = false;
    int active_machine_idx = 0;
    for (int a = 0; a < task.compatible_machine.size(); ++a) {
      int lit = relaxation.task_to_presence_literals[task_idx][a];
      if (lit == kint32max || lit_value(lit)) {
        active_machine_idx = a;
        is_present = true;
        break;
      }
    }
    if (!is_present) continue;

    const int start_time_var =
        relaxation.task_to_start_time_model_var[task_idx].var;
    const int64_t start_time =
        solution[start_time_var] *
            relaxation.task_to_start_time_model_var[task_idx].coeff +
        relaxation.task_to_start_time_model_var[task_idx].offset;

    const int64_t end_time =
        start_time + task.duration_for_machine[active_machine_idx];
    machine_intervals[task.compatible_machine[active_machine_idx]].push_back(
        {start_time, end_time});
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
    for (const int m : task.compatible_machine) {
      num_machines = std::max(num_machines, m + 1);
    }
    int64_t max_dur = 0;
    for (const int64_t d : task.duration_for_machine) {
      max_dur = std::max(max_dur, d);
    }
    horizon += max_dur;
  }

  std::vector<NoOverlapConstraintProto*> no_overlap_constraints(num_machines);
  for (int i = 0; i < num_machines; ++i) {
    no_overlap_constraints[i] =
        model_proto.add_constraints()->mutable_no_overlap();
  }

  const int num_tasks = problem.tasks.size();

  // Helper struct to unify task end expressions (var + offset)
  struct AffineExpr {
    int var;
    int64_t offset;
  };

  std::vector<int> task_start_vars(num_tasks);
  std::vector<AffineExpr> task_ends(num_tasks);

  // Track predecessors for the makespan "sink task" optimization
  std::vector<bool> is_predecessor(num_tasks, false);

  // Build variables and interval constraints for each task.
  for (int i = 0; i < num_tasks; ++i) {
    const auto& task = problem.tasks[i];
    int num_alts = task.compatible_machine.size();

    IntegerVariableProto* start_var = model_proto.add_variables();
    start_var->add_domain(0);
    start_var->add_domain(horizon);
    int start_var_idx = model_proto.variables().size() - 1;
    task_start_vars[i] = start_var_idx;

    // Mark any task that must complete *before* this one as a predecessor
    for (int p : task.tasks_that_must_complete_before_this) {
      is_predecessor[p] = true;
    }

    if (num_alts == 1) {
      // Unify: End = StartVar + Duration
      task_ends[i] = {start_var_idx, task.duration_for_machine[0]};

      IntervalConstraintProto* interval =
          model_proto.add_constraints()->mutable_interval();

      interval->mutable_start()->add_vars(start_var_idx);
      interval->mutable_start()->add_coeffs(1);

      interval->mutable_size()->set_offset(task.duration_for_machine[0]);

      interval->mutable_end()->add_vars(start_var_idx);
      interval->mutable_end()->add_coeffs(1);
      interval->mutable_end()->set_offset(task.duration_for_machine[0]);

      int interval_idx = model_proto.constraints().size() - 1;
      no_overlap_constraints[task.compatible_machine[0]]->add_intervals(
          interval_idx);

    } else if (num_alts > 1) {
      IntegerVariableProto* end_var = model_proto.add_variables();
      end_var->add_domain(0);
      end_var->add_domain(horizon);
      int end_var_idx = model_proto.variables().size() - 1;

      // Unify: End = EndVar + 0
      task_ends[i] = {end_var_idx, 0};

      int64_t min_dur = task.duration_for_machine[0];
      int64_t max_dur = task.duration_for_machine[0];
      for (int64_t d : task.duration_for_machine) {
        min_dur = std::min(min_dur, d);
        max_dur = std::max(max_dur, d);
      }

      IntegerVariableProto* dur_var = model_proto.add_variables();
      dur_var->add_domain(min_dur);
      dur_var->add_domain(max_dur);
      int dur_var_idx = model_proto.variables().size() - 1;

      IntervalConstraintProto* main_interval =
          model_proto.add_constraints()->mutable_interval();
      main_interval->mutable_start()->add_vars(start_var_idx);
      main_interval->mutable_start()->add_coeffs(1);
      main_interval->mutable_size()->add_vars(dur_var_idx);
      main_interval->mutable_size()->add_coeffs(1);
      main_interval->mutable_end()->add_vars(end_var_idx);
      main_interval->mutable_end()->add_coeffs(1);

      auto* exactly_one = model_proto.add_constraints()->mutable_exactly_one();

      for (int a = 0; a < num_alts; ++a) {
        IntegerVariableProto* bool_var = model_proto.add_variables();
        bool_var->add_domain(0);
        bool_var->add_domain(1);
        int bool_var_idx = model_proto.variables().size() - 1;

        exactly_one->add_literals(bool_var_idx);

        ConstraintProto* alt_ct = model_proto.add_constraints();
        alt_ct->add_enforcement_literal(bool_var_idx);
        IntervalConstraintProto* alt_interval = alt_ct->mutable_interval();
        alt_interval->mutable_start()->add_vars(start_var_idx);
        alt_interval->mutable_start()->add_coeffs(1);
        alt_interval->mutable_size()->set_offset(task.duration_for_machine[a]);
        alt_interval->mutable_end()->add_vars(end_var_idx);
        alt_interval->mutable_end()->add_coeffs(1);

        int alt_interval_idx = model_proto.constraints().size() - 1;
        no_overlap_constraints[task.compatible_machine[a]]->add_intervals(
            alt_interval_idx);
      }
    }
  }

  // Build precedence constraints.
  for (int task_idx = 0; task_idx < num_tasks; ++task_idx) {
    const auto& current_task = problem.tasks[task_idx];
    for (int preceding_idx :
         current_task.tasks_that_must_complete_before_this) {
      // current_start >= preceding_end
      // current_start - preceding_end.var >= preceding_end.offset
      LinearConstraintProto* linear =
          model_proto.add_constraints()->mutable_linear();

      linear->add_vars(task_start_vars[task_idx]);
      linear->add_coeffs(1);
      linear->add_vars(task_ends[preceding_idx].var);
      linear->add_coeffs(-1);

      linear->add_domain(task_ends[preceding_idx].offset);
      linear->add_domain(kint64max);
    }
  }

  // Build makespan constraints using sink tasks only.
  const int makespan_var = model_proto.variables().size();
  IntegerVariableProto* var = model_proto.add_variables();
  var->add_domain(0);
  var->add_domain(horizon);

  for (int task_idx = 0; task_idx < num_tasks; ++task_idx) {
    // Optimization: Only constrain tasks that have no successors!
    if (is_predecessor[task_idx]) continue;

    // makespan >= task_end
    // makespan - task_end.var >= task_end.offset
    LinearConstraintProto* linear =
        model_proto.add_constraints()->mutable_linear();

    linear->add_vars(makespan_var);
    linear->add_coeffs(1);
    linear->add_vars(task_ends[task_idx].var);
    linear->add_coeffs(-1);

    linear->add_domain(task_ends[task_idx].offset);
    linear->add_domain(kint64max);
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

        const SchedulingProblem::Task& task =
            problem_and_mapping.problem.tasks[task_idx];
        int active_machine_idx = 0;
        for (int a = 0; a < task.compatible_machine.size(); ++a) {
          int lit = problem_and_mapping.task_to_presence_literals[task_idx][a];
          if (lit == kint32max) {
            active_machine_idx = a;
            break;
          }
          const bool is_true = RefIsPositive(lit)
                                   ? (solution[lit] == 1)
                                   : (solution[NegatedRef(lit)] == 0);
          if (is_true) {
            active_machine_idx = a;
            break;
          }
        }

        int64_t duration = problem_and_mapping.problem.tasks[task_idx]
                               .duration_for_machine[active_machine_idx];
        problem_makespan = std::max(problem_makespan, start_time + duration);
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
    VLOG(3) << "Quick heuristic deemed graph unsplittable.";
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
    VLOG(3) << "PartitionByIncomparability found graph is non-splittable.";
  } else {
    VLOG(3) << "PartitionByIncomparability found: ["
            << absl::StrJoin(result, ",",
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
  for (const absl::Span<const int> partition : partitions) {
    mapped.clear();
    for (const int u : partition) {
      // Only keep nodes that represent intervals
      if (u < N) mapped.push_back(intervals[u].index);
    }
    if (!mapped.empty()) {
      result.Add(mapped);
    }
  }

  if (result.size() == 1 && result[0].size() == N) {
    VLOG(3) << "IntervalsNonOverlappingComponents: " << N << " intervals and "
            << precedences.size()
            << " precedences and the found graph is non-splittable.";
  } else {
    VLOG(3) << "IntervalsNonOverlappingComponents: " << N << " intervals and "
            << precedences.size() << " precedences, components=["
            << absl::StrJoin(result, ",",
                             [](std::string* out, absl::Span<const int> v) {
                               absl::StrAppend(out, v.size());
                             })
            << "]";
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
