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

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// A scheduling resource representing either a unary machine (no_overlap) or a
// renewable cumulative resource.
class SchedulingResource {
 public:
  enum Type { kNoOverlap, kCumulative };

  static SchedulingResource NoOverlap(
      int constraint_index,
      std::optional<int> barrier_interval = std::nullopt) {
    SchedulingResource r;
    r.type_ = kNoOverlap;
    r.constraint_index_ = constraint_index;
    r.capacity_ = AffineExpr::Const(1);
    r.barrier_interval_ = barrier_interval;
    return r;
  }

  static SchedulingResource Cumulative(
      int constraint_index, AffineExpr capacity,
      std::optional<int> barrier_interval = std::nullopt,
      bool is_redundant = false) {
    SchedulingResource r;
    r.type_ = kCumulative;
    r.constraint_index_ = constraint_index;
    r.capacity_ = capacity;
    r.is_redundant_ = is_redundant;
    r.barrier_interval_ = barrier_interval;
    return r;
  }

  static SchedulingResource Cumulative(
      int constraint_index, int64_t capacity,
      std::optional<int> barrier_interval = std::nullopt,
      bool is_redundant = false) {
    return Cumulative(constraint_index, AffineExpr::Const(capacity),
                      barrier_interval, is_redundant);
  }

  Type type() const { return type_; }
  bool is_no_overlap() const { return type_ == kNoOverlap; }
  bool is_cumulative() const { return type_ == kCumulative; }
  int constraint_index() const { return constraint_index_; }
  AffineExpr capacity() const { return capacity_; }

  // Whether this resource is redundant (e.g. cumulative clique relaxation
  // inferred from mutually exclusive tasks).
  bool is_redundant() const { return is_redundant_; }

  // An optional interval that acts as a barrier enforcing a horizon or makespan
  // boundary on the resource. When present, all other intervals on this
  // resource must complete before this barrier interval starts (or start after
  // it ends).
  std::optional<int> barrier_interval() const { return barrier_interval_; }

  bool operator==(const SchedulingResource& other) const {
    return type_ == other.type_ &&
           constraint_index_ == other.constraint_index_ &&
           capacity_ == other.capacity_ &&
           is_redundant_ == other.is_redundant_ &&
           barrier_interval_ == other.barrier_interval_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingResource& r) {
    absl::Format(&sink,
                 "SchedulingResource(type: %s, constraint: %d, capacity: %v, "
                 "redundant: %v, barrier_interval: %s)",
                 r.type_ == kNoOverlap ? "no_overlap" : "cumulative",
                 r.constraint_index_, r.capacity_, r.is_redundant_,
                 r.barrier_interval_.has_value()
                     ? absl::StrCat(*r.barrier_interval_)
                     : "none");
  }

 private:
  // Type of the resource (no_overlap or cumulative).
  Type type_ = kNoOverlap;

  // Index of the constraint in the CpModelProto defining this resource.
  int constraint_index_ = -1;

  // Capacity of the resource (1 for no_overlap, or affine expression for
  // cumulative).
  AffineExpr capacity_ = AffineExpr::Const(1);

  // Whether this resource constraint is redundant (e.g. cumulative clique
  // relaxation).
  bool is_redundant_ = false;

  // Optional barrier interval enforcing horizon / makespan boundary.
  std::optional<int> barrier_interval_;
};

// Scans `model_proto` (or the subset of constraints in `constraint_indices` if
// non-empty) for `no_overlap` and `cumulative` resources, including variable
// capacities and barrier intervals.
std::vector<SchedulingResource> ScanSchedulingResources(
    const CpModelProto& model_proto,
    absl::Span<const int> constraint_indices = {});

// Represents a resource demand required by a task recipe.
class ResourceDemand {
 public:
  ResourceDemand() = default;
  ResourceDemand(int resource_index, AffineExpr demand)
      : resource_index_(resource_index), demand_(demand) {}
  ResourceDemand(int resource_index, int64_t constant_demand)
      : resource_index_(resource_index),
        demand_(AffineExpr::Const(constant_demand)) {}

  int resource_index() const { return resource_index_; }
  AffineExpr demand() const { return demand_; }

  bool operator==(const ResourceDemand& other) const {
    return resource_index_ == other.resource_index_ && demand_ == other.demand_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ResourceDemand& rd) {
    absl::Format(&sink, "ResourceDemand(resource: %d, demand: %v)",
                 rd.resource_index_, rd.demand_);
  }

 private:
  // Index of the resource in the scheduling problem.
  int resource_index_ = -1;

  // Demand expression (1 for no_overlap, or affine expression for cumulative).
  AffineExpr demand_ = AffineExpr::Const(1);
};

// An alternative execution recipe for a task.
class TaskRecipe {
 public:
  TaskRecipe() = default;
  TaskRecipe(int interval_index, std::optional<int> literal, int64_t duration,
             std::vector<ResourceDemand> resources,
             bool literal_is_presence_literal = false)
      : interval_index_(interval_index),
        literal_(literal),
        duration_(duration),
        resources_(std::move(resources)),
        literal_is_presence_literal_(literal_is_presence_literal) {}

  int interval_index() const { return interval_index_; }
  std::optional<int> literal() const { return literal_; }
  int64_t duration() const { return duration_; }
  absl::Span<const ResourceDemand> resources() const { return resources_; }

  bool literal_is_presence_literal() const {
    return literal_is_presence_literal_;
  }

  bool operator==(const TaskRecipe& other) const {
    return interval_index_ == other.interval_index_ &&
           literal_ == other.literal_ && duration_ == other.duration_ &&
           resources_ == other.resources_ &&
           literal_is_presence_literal_ == other.literal_is_presence_literal_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TaskRecipe& r) {
    absl::Format(
        &sink,
        "TaskRecipe(interval: %d, literal: %s, duration: %d, resources: [%s], "
        "presence_literal: %v)",
        r.interval_index_,
        r.literal_.has_value() ? absl::StrCat(*r.literal_) : "none",
        r.duration_, absl::StrJoin(r.resources_, ", "),
        r.literal_is_presence_literal_);
  }

 private:
  // Interval constraint index in the model.
  int interval_index_ = -1;

  // Optional controlling literal (choice or presence).
  std::optional<int> literal_;

  // Fixed duration of the task under this recipe.
  int64_t duration_ = 0;

  // List of resource demands for this recipe.
  std::vector<ResourceDemand> resources_;

  // Whether the literal acts as the interval's presence literal.
  bool literal_is_presence_literal_ = false;
};

// Represents a unit of execution in a scheduling problem.
class SchedulingTask {
 public:
  SchedulingTask() = default;
  SchedulingTask(AffineExpr start_expr, AffineExpr end_expr,
                 std::vector<TaskRecipe> recipes = {},
                 std::optional<int> representative_interval = std::nullopt)
      : start_expr_(start_expr),
        end_expr_(end_expr),
        representative_interval_(representative_interval),
        recipes_(std::move(recipes)),
        multiple_recipes_(recipes_.size() > 1),
        recipes_contains_parallel_intervals_(absl::c_any_of(
            recipes_,
            [](const TaskRecipe& r) { return r.resources().size() > 1; })),
        all_literals_are_presence_literals_(
            !recipes_.empty() &&
            absl::c_all_of(recipes_, [](const TaskRecipe& r) {
              return r.literal_is_presence_literal();
            })) {}

  AffineExpr start_expr() const { return start_expr_; }
  AffineExpr end_expr() const { return end_expr_; }

  // The index of the representative interval constraint in the model if
  // one was identified (e.g. an un-enforced interval matching the task's
  // start and end expressions).
  std::optional<int> representative_interval() const {
    return representative_interval_;
  }

  absl::Span<const TaskRecipe> recipes() const { return recipes_; }

  bool multiple_recipes() const { return multiple_recipes_; }
  bool recipes_contains_parallel_intervals() const {
    return recipes_contains_parallel_intervals_;
  }
  bool all_literals_are_presence_literals() const {
    return all_literals_are_presence_literals_;
  }

  bool operator==(const SchedulingTask& other) const {
    return start_expr_ == other.start_expr_ && end_expr_ == other.end_expr_ &&
           representative_interval_ == other.representative_interval_ &&
           recipes_ == other.recipes_ &&
           multiple_recipes_ == other.multiple_recipes_ &&
           recipes_contains_parallel_intervals_ ==
               other.recipes_contains_parallel_intervals_ &&
           all_literals_are_presence_literals_ ==
               other.all_literals_are_presence_literals_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingTask& t) {
    absl::Format(&sink,
                 "SchedulingTask(start: %v, end: %v, rep_interval: %s, "
                 "recipes: [%s], multiple_recipes: %v, parallel: %v, "
                 "all_presence: %v)",
                 t.start_expr_, t.end_expr_,
                 t.representative_interval_.has_value()
                     ? absl::StrCat(*t.representative_interval_)
                     : "none",
                 absl::StrJoin(t.recipes_, ", "), t.multiple_recipes_,
                 t.recipes_contains_parallel_intervals_,
                 t.all_literals_are_presence_literals_);
  }

 private:
  // Start affine expression of the task.
  AffineExpr start_expr_;

  // End affine expression of the task.
  AffineExpr end_expr_;

  // Optional representative interval constraint index.
  std::optional<int> representative_interval_;

  // Alternative recipe options for executing this task.
  std::vector<TaskRecipe> recipes_;

  // True if the task has more than one recipe option.
  bool multiple_recipes_ = false;

  // True if any recipe uses multiple resources simultaneously.
  bool recipes_contains_parallel_intervals_ = false;

  // True if all recipe literals act as presence literals for intervals.
  bool all_literals_are_presence_literals_ = false;
};

// Represents an ordered sequence of tasks (job).
class SchedulingJob {
 public:
  SchedulingJob() = default;
  explicit SchedulingJob(std::vector<int> task_indices)
      : task_indices_(std::move(task_indices)) {}

  absl::Span<const int> task_indices() const { return task_indices_; }

  bool operator==(const SchedulingJob& other) const {
    return task_indices_ == other.task_indices_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingJob& j) {
    absl::Format(&sink, "SchedulingJob(tasks: [%s])",
                 absl::StrJoin(j.task_indices_, ", "));
  }

 private:
  // Ordered sequence of task indices comprising the job chain.
  std::vector<int> task_indices_;
};

// Generalized precedence relationship between two endpoints.
class SchedulingPrecedence {
 public:
  enum EndpointType { kTaskStart, kTaskEnd, kModelExpression };

  struct Endpoint {
    // Type of the endpoint (task start, task end, or model expression).
    EndpointType type = kTaskStart;

    // Task index in the problem, or -1 if type is kModelExpression.
    int task_index = -1;

    // Affine expression representing the endpoint in the model.
    AffineExpr expr;

    static Endpoint TaskStart(int task_index, AffineExpr expr) {
      Endpoint ep;
      ep.type = kTaskStart;
      ep.task_index = task_index;
      ep.expr = expr;
      return ep;
    }

    static Endpoint TaskEnd(int task_index, AffineExpr expr) {
      Endpoint ep;
      ep.type = kTaskEnd;
      ep.task_index = task_index;
      ep.expr = expr;
      return ep;
    }

    static Endpoint ModelExpression(AffineExpr expr) {
      Endpoint ep;
      ep.type = kModelExpression;
      ep.task_index = -1;
      ep.expr = expr;
      return ep;
    }

    bool operator==(const Endpoint& other) const {
      return type == other.type && task_index == other.task_index &&
             expr == other.expr;
    }

    template <typename Sink>
    friend void AbslStringify(Sink& sink, const Endpoint& ep) {
      const char* type_str = "start";
      if (ep.type == kTaskEnd) type_str = "end";
      if (ep.type == kModelExpression) type_str = "expr";
      absl::Format(&sink, "Endpoint(%s, task: %d, expr: %v)", type_str,
                   ep.task_index, ep.expr);
    }
  };

  SchedulingPrecedence() = default;
  SchedulingPrecedence(Endpoint before, Endpoint after, int64_t delay,
                       std::vector<int> enforcement_literals = {})
      : before_(before),
        after_(after),
        delay_(delay),
        enforcement_literals_(std::move(enforcement_literals)) {}

  const Endpoint& before() const { return before_; }
  const Endpoint& after() const { return after_; }
  int64_t delay() const { return delay_; }
  bool is_min_delay() const { return delay_ >= 0; }
  bool is_max_delay() const { return delay_ < 0; }
  int64_t min_delay() const { return delay_; }
  int64_t max_delay() const { return delay_; }

  absl::Span<const int> enforcement_literals() const {
    return enforcement_literals_;
  }
  bool is_unconditional() const { return enforcement_literals_.empty(); }

  bool is_end_to_start() const {
    return before_.type == EndpointType::kTaskEnd &&
           after_.type == EndpointType::kTaskStart;
  }
  bool is_start_to_start() const {
    return before_.type == EndpointType::kTaskStart &&
           after_.type == EndpointType::kTaskStart;
  }
  bool is_end_to_end() const {
    return before_.type == EndpointType::kTaskEnd &&
           after_.type == EndpointType::kTaskEnd;
  }
  bool is_start_to_end() const {
    return before_.type == EndpointType::kTaskStart &&
           after_.type == EndpointType::kTaskEnd;
  }
  bool is_to_model_expression() const {
    return after_.type == EndpointType::kModelExpression;
  }

  int before_task() const { return before_.task_index; }
  int after_task() const { return after_.task_index; }

  bool operator==(const SchedulingPrecedence& other) const {
    return before_ == other.before_ && after_ == other.after_ &&
           delay_ == other.delay_ &&
           enforcement_literals_ == other.enforcement_literals_;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingPrecedence& p) {
    absl::Format(&sink, "SchedulingPrecedence(%v -> %v + %d, literals: [%s])",
                 p.before_, p.after_, p.delay_,
                 absl::StrJoin(p.enforcement_literals_, ", "));
  }

 private:
  // Before endpoint.
  Endpoint before_;

  // After endpoint.
  Endpoint after_;

  // Delay value (>= 0 is min_delay, < 0 is max_delay).
  int64_t delay_ = 0;

  // Optional enforcement literals under which this precedence holds.
  std::vector<int> enforcement_literals_;
};

// A generic representation of a scheduling problem. It is described as a set of
// tasks. Each task must be executed on a single machine. During the duration of
// a task the machine executing it is not available for other tasks. The
// problem can specify task precedences, for example if task 1 can only start
// after task 0 completes, we will have
// tasks[1].tasks_that_must_complete_before_this = {0}. The solution of the
// problem is a start time for each task.
struct SchedulingProblem {
  struct Task {
    std::vector<int> compatible_machine;
    std::vector<int64_t> duration_for_machine;
    std::vector<int> tasks_that_must_complete_before_this;
    int64_t min_start = 0;

    // JSSP Constraint Mappings
    int main_interval = -1;
    std::vector<int> alternative_intervals;
    std::vector<int> unconditional_intervals;
    AffineExpr start_time_expr;
    std::vector<int> presence_literals;

    template <typename Sink>
    friend void AbslStringify(Sink& sink, const Task& task) {
      absl::Format(
          &sink, "Task(machines: [%v], durations: [%v], run_after: [%v])",
          absl::StrJoin(task.compatible_machine, ","),
          absl::StrJoin(task.duration_for_machine, ","),
          absl::StrJoin(task.tasks_that_must_complete_before_this, ","));
    }
  };
  std::vector<Task> tasks;

  enum Type { kMinimizeMakespan, kSatisfaction } type;

  // Job sequences (grouping tasks by job sequence)
  struct Job {
    std::vector<int> task_indices;
  };
  std::vector<Job> jobs;

  // Bidirectional Mapping Maps
  std::vector<int> interval_to_task_index;
  std::vector<int> interval_to_job;
  std::vector<int> interval_to_main_interval;

  // Objective / Makespan
  std::optional<AffineExpr> makespan_expr;
  int makespan_interval = -1;
  std::vector<int> redundant_cumulative;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const SchedulingProblem& problem) {
    absl::Format(
        &sink, "SchedulingProblem(%s, tasks: %s)",
        (problem.type == kMinimizeMakespan ? "makespan minimization"
                                           : "satisfaction"),
        absl::StrJoin(
            problem.tasks, ",", [](std::string* out, const Task& task) {
              absl::Format(
                  out, "Task(machines: [%v], durations: [%v], run_after: [%v])",
                  absl::StrJoin(task.compatible_machine, ","),
                  absl::StrJoin(task.duration_for_machine, ","),
                  absl::StrJoin(task.tasks_that_must_complete_before_this,
                                ","));
            }));
  }
};

// A relaxation of the CpModelProto as a set of independent scheduling problems
// that are only potentially connected through the objective function.
struct SchedulingRelaxation {
  std::vector<SchedulingProblem> problems;

  struct RelaxedObjective {
    // `var_in_problem_makespan` must match at least one of the
    // makespan_expr.var. If it matches the makespan_expr of several
    // problems, one must pick the largest value to get the
    // objective value of the relaxation.
    std::vector<int> var_in_problem_makespan;
    std::vector<int64_t> coeff;
    int64_t offset;
  };
  RelaxedObjective relaxed_objective;

  bool HasRedundantConstraints() const;
  std::string ToString(const CpModelProto* model_proto = nullptr) const;
};

// Detects all the scheduling sub-problems in the model and returns a set of
// scheduling problems that together correspond to a relaxation of the original
// problem. In other terms, any solution of the original problem will be a
// solution of each individual scheduling problem and the objective computed
// from the makespan of the tasks using `relaxed_objective` will be lower or
// equal to the original objective of the solution.
SchedulingRelaxation DetectSchedulingProblems(const CpModelProto& model_proto);

// Build a CpModelProto to solve the given scheduling problem. By convention,
// the start time of task i will be variable i and the makespan will be the
// variable with index num_tasks.
CpModelProto BuildSchedulingModel(const SchedulingProblem& problem);

bool VerifySchedulingRelaxation(const SchedulingRelaxation& relaxation,
                                absl::Span<const int64_t> solution,
                                int64_t* relaxed_objective_value);

// Detects all the precedences between intervals from `precedences`.
// This returns pairs of "interval constraint indices" in the given proto where
// we are sure that end(pair.first) <= start(pair.second) in all feasible
// solutions.
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
// num_precedences) in the worst case. It always returns the largest number of
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
// Fun fact: if this sounds overcomplicated, there is an algorithm [1] that can
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

std::vector<int> DetectRedundantCumulativeConstraints(
    const CpModelProto& model_proto, const SchedulingProblem& structure);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SCHEDULING_MODEL_H_
