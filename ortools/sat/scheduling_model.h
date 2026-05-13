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

#ifndef ORTOOLS_SAT_SCHEDULING_MODEL_H_
#define ORTOOLS_SAT_SCHEDULING_MODEL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// A generic representation of a scheduling problem. It is described as a set of
// tasks. Each task must be executed on a single machine. During the duration of
// a task the machine executing it is not available for other tasks. The
// problem can specify task precedences, for example if task 1 can only start
// after task 0 completes, we will have
// tasks[1].tasks_that_must_complete_before_this = {0}. The solution of the
// problem is a start time for each task.
struct SchedulingProblem {
  struct Task {
    int machine;
    int64_t duration;
    std::vector<int> tasks_that_must_complete_before_this;

    template <typename Sink>
    friend void AbslStringify(Sink& sink, const Task& task) {
      absl::Format(
          &sink, "Task(machine: %v, duration: %v, run_after: %v)", task.machine,
          task.duration,
          absl::StrJoin(task.tasks_that_must_complete_before_this, ","));
    }
  };
  std::vector<Task> tasks;

  enum Type { kMinimizeMakespan, kSatisfaction } type;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingProblem& problem) {
    absl::Format(
        &sink, "SchedulingProblem(%s, tasks: %s)",
        (problem.type == kMinimizeMakespan ? "makespan minimization"
                                           : "satisfaction"),
        absl::StrJoin(
            problem.tasks, ",", [](std::string* out, const Task& task) {
              absl::Format(out,
                           "Task(machine: %v, duration: %v, run_after: %v)",
                           task.machine, task.duration,
                           absl::StrJoin(
                               task.tasks_that_must_complete_before_this, ","));
            }));
  }
};

// The description of a scheduling problem and a mapping allowing to compute the
// solution to the problem from the solution of the CpModelProto it was
// extracted from.
struct SchedulingProblemAndMapping {
  SchedulingProblem problem;

  struct ShiftedVar {
    int var;  // The variable in the CpModelProto.
    int64_t coeff;
    int64_t offset;
  };
  std::vector<ShiftedVar> task_to_start_time_model_var;
  std::optional<ShiftedVar> makespan_expr;
};

// A relaxation of the CpModelProto as a set of independent scheduling problems
// that are only potentially connected through the objective function.
struct SchedulingRelaxation {
  std::vector<SchedulingProblemAndMapping> problems_and_mappings;

  struct RelaxedObjective {
    // `var_in_problem_makespan` must match at least one of the
    // makespan_expr.var. If it matches the makespan_expr of several
    // problems_and_mappings, one must pick the largest value to get the
    // objective value of the relaxation.
    std::vector<int> var_in_problem_makespan;
    std::vector<int64_t> coeff;
    int64_t offset;
  };
  RelaxedObjective relaxed_objective;
};

// Detects all the scheduling sub-problems in the model and returns a set of
// scheduling problems that together correspond to a relaxation of the original
// problem. In other terms, any solution of the original problem will be a
// solution of each individual scheduling problem and the objective computed
// from the makespan of the taks using `relaxed_objective` will be lower or
// equal to the original objective of the solution.
SchedulingRelaxation DetectSchedulingProblems(const CpModelProto& model_proto);

// Build a CpModelProto to solve the given scheduling problem. By convention,
// the start time of task i will be variable i and the makespan will the
// variable with index num_tasks.
CpModelProto BuildSchedulingModel(const SchedulingProblem& problem);

bool VerifySchedulingRelaxation(const SchedulingRelaxation& relaxation,
                                absl::Span<const int64_t> solution,
                                int64_t* relaxed_objective_value);

// Detects all the precedences between intervals from `precedences`.
// This return pairs of "interval constraint indices" in the given proto where
// we are sure that end(pair.first) <= start(pair.second) in all feasible
// solution.
std::vector<std::pair<int, int>> DetectIntervalPrecedences(
    const CpModelProto& model_proto,
    const BestBinaryRelationBounds& precedences,
    absl::Span<const int> interval_indices);

// Splits the set of intervals into components that cannot overlap. More
// precisely, for two components C1 and C2 one of the following must be true:
// - every interval in C1 must end at or before any interval in C2 starts;
// - every interval in C2 must end at or before any interval in C1 starts.
//
// This function takes into account both the `precedences` and the trivial
// precedences from the interval start and end times. It runs in O(num_intervals
// + num_precedences) time for "easy" cases and O(num_intervals *
// (num_precedences) in the worst case. It always returns the largest number of
// components possible. It doesn't do anything if the number of intervals is too
// large to avoid spending too much time in this quadratic algorithm.
CompactVectorVector<int> IntervalsNonOverlappingComponents(
    absl::Span<const IndexedInterval> intervals,
    const std::vector<std::pair<int, int>>& precedences);

// -----------------------------------------------------------------------------
// Induced DAG Incomparability Partitioning
// -----------------------------------------------------------------------------
// Given a Directed Acyclic Graph (DAG) G = (V, E) and a contiguous subset of
// "Primary Nodes" P (where P comprises the indices [0, num_primary_nodes - 1]),
// the goal is to partition P into the maximum number of disjoint subsets
// S_1, ..., S_k such that for any two primary vertices u and v belonging to
// DIFFERENT subsets, there exists a directed path between them in G (either
// u ~> v or v ~> u).
//
// Equivalently: If there is no directed path between u and v in either
// direction, they must be placed in the SAME subset. In graph theory terms,
// this is computing the connected components of the induced incomparability
// subgraph on P.
//
// The resulting partition forms a strict total order of subsets, meaning all
// directed paths between distinct subsets flow in a single direction
// (S_i -> S_j where i < j).
//
// === Why the distinction between primary and non-primary nodes? ===
// In scheduling, explicitly drawing direct edges between every single pair of
// comparable tasks with incompatible start/end times creates a massive O(N^2)
// edge explosion. To avoid the quadratic size, we introduce auxiliary nodes to
// represent points in time and model the incompatibilities as precedences
// (edges) in the graph.
//
// However, these time nodes exist purely to pass paths forward. Because
// they don't have edges to *every* parallel task happening at the same time,
// the standard graph math sees them as "incomparable" to those parallel tasks.
// If we naively partition the entire graph, these routing nodes act as false
// "glue," accidentally merging completely independent sequences of tasks
// together.
//
// By strictly evaluating incomparability ONLY on the Primary Nodes, we achieve
// the memory savings of the auxiliary routing infrastructure without suffering
// from its mathematical side-effects.
//
// === The Algorithms ===
// We built two algorithms to tackle this efficiently:
// 1. ProbableSplitExists(): A fast O(V + E) probabilistic filter that samples a
//    subset of primary nodes and computes if the subset can be partitioned to
//    quickly detect unsplittable graphs.
// 2. PartitionByIncomparabilityExact(): An exact O(V * E) solver that
//    in the worst case computes the reachability for each primary node.
//
// We then combine these into PartitionByIncomparability() which runs the filter
// first and only runs the exact solver when the filter suggests the graph is
// splittable.
//
// Fun fact: if this sounds over-complicated, there is an algorithm [1] that can
// solve the general incomparability problem optimally in O(V + E) time.
// However, it's so complex that not even Gemini had the stomach to implement
// it.
//
// [1] McConnell, Ross M., and Fabien De Montgolfier. "Linear-time modular
// decomposition of directed graphs." Discrete Applied Mathematics 145.2 (2005):
// 198-209.

// Represents a DAG that has been validated cycle-free and topologically sorted.
struct GraphForPartition {
  int num_nodes;
  int num_primary_nodes;
  const CompactVectorVector<int>& adj;
  const std::vector<int>& topological_order;
};
CompactVectorVector<int> PartitionByIncomparability(
    int num_nodes, int num_primary_nodes, const CompactVectorVector<int>& adj);

// The two underlying algorithms are exposed for testing.
CompactVectorVector<int> PartitionByIncomparabilityExact(
    const GraphForPartition& graph);
bool ProbableSplitExists(const GraphForPartition& graph);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SCHEDULING_MODEL_H_
