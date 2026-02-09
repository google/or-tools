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
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"

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
    // `var_in_problem_makespan` must match one of the makespan_expr.var.
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

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SCHEDULING_MODEL_H_
