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

#include "examples/cpp/jobshop_sat_model.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/scheduling/jobshop_scheduling.pb.h"
#include "ortools/scheduling/jobshop_scheduling_parser.h"
#include "ortools/util/sorted_interval_list.h"

using operations_research::scheduling::jssp::Job;
using operations_research::scheduling::jssp::JobPrecedence;
using operations_research::scheduling::jssp::JsspInputProblem;
using operations_research::scheduling::jssp::Machine;
using operations_research::scheduling::jssp::Task;
using operations_research::scheduling::jssp::TransitionTimeMatrix;

namespace operations_research {
namespace sat {

namespace {

// Compute a valid horizon from a problem.
int64_t ComputeHorizon(const JsspInputProblem& problem) {
  int64_t sum_of_durations = 0;
  int64_t max_latest_end = 0;
  int64_t max_earliest_start = 0;
  for (const Job& job : problem.jobs()) {
    if (job.has_latest_end()) {
      max_latest_end = std::max(max_latest_end, job.latest_end().value());
    } else {
      max_latest_end = kint64max;
    }
    if (job.has_earliest_start()) {
      max_earliest_start =
          std::max(max_earliest_start, job.earliest_start().value());
    }
    for (const Task& task : job.tasks()) {
      int64_t max_duration = 0;
      for (int64_t d : task.duration()) {
        max_duration = std::max(max_duration, d);
      }
      sum_of_durations += max_duration;
    }
  }

  const int num_jobs = problem.jobs_size();
  int64_t sum_of_transitions = 0;
  for (const Machine& machine : problem.machines()) {
    if (!machine.has_transition_time_matrix()) continue;
    const TransitionTimeMatrix& matrix = machine.transition_time_matrix();
    for (int i = 0; i < num_jobs; ++i) {
      int64_t max_transition = 0;
      for (int j = 0; j < num_jobs; ++j) {
        max_transition =
            std::max(max_transition, matrix.transition_time(i * num_jobs + j));
      }
      sum_of_transitions += max_transition;
    }
  }
  return std::min(max_latest_end,
                  sum_of_durations + sum_of_transitions + max_earliest_start);
}

// Create the job structure as a chain of tasks. Fills in the job_to_tasks
// vector.
void CreateJobs(const JsspInputProblem& problem, int64_t horizon,
                std::vector<std::vector<JobTaskData>>& job_to_tasks,
                CpModelBuilder& cp_model) {
  const int num_jobs = problem.jobs_size();

  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    std::vector<JobTaskData>& task_data = job_to_tasks[j];

    const int64_t hard_start =
        job.has_earliest_start() ? job.earliest_start().value() : 0L;
    const int64_t hard_end =
        job.has_latest_end() ? job.latest_end().value() : horizon;

    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();
      CHECK_EQ(num_alternatives, task.duration_size());

      // Add the "main" task interval.
      std::vector<int64_t> durations;
      int64_t min_duration = task.duration(0);
      int64_t max_duration = task.duration(0);
      durations.push_back(task.duration(0));
      for (int a = 1; a < num_alternatives; ++a) {
        min_duration = std::min(min_duration, task.duration(a));
        max_duration = std::max(max_duration, task.duration(a));
        durations.push_back(task.duration(a));
      }

      // Hack: we force the end to be below the horizon if the job has no hard
      // limit defined.
      //
      // The correct formula should use min_duration, but this will break the
      // makespan detection inside the solver. Luckily, the horizon computation
      // is very loose and has a lot of slack, so we should not loose the
      // optimal solution.
      //
      // TODO(user): remove the makespan interval when makespan detection is
      // based on the dependency graph and not on the creation of the makespan
      // interval.
      const IntVar start = cp_model.NewIntVar(Domain(
          hard_start,
          job.has_latest_end() || problem.makespan_cost_per_time_unit() == 0
              ? hard_end
              : hard_end - max_duration));
      if (min_duration == max_duration) {
        const IntervalVar interval =
            cp_model.NewFixedSizeIntervalVar(start, min_duration);
        task_data.push_back(
            {interval, start, min_duration, start + min_duration});
      } else {
        const IntVar duration =
            cp_model.NewIntVar(Domain::FromValues(durations));
        const IntVar end = cp_model.NewIntVar(Domain(hard_start, hard_end));
        const IntervalVar interval =
            cp_model.NewIntervalVar(start, duration, end);
        task_data.push_back({interval, start, duration, end});
      }

      // Chain the task belonging to the same job.
      if (t > 0) {
        cp_model.AddLessOrEqual(task_data[t - 1].end, task_data[t].start);
      }
    }
  }
}

// Each task in a job can have multiple alternative ways of being performed.
// This structure stores the start, end, and presence variables attached to one
// alternative for a given task.
struct AlternativeTaskData {
  int machine;
  IntervalVar interval;
  BoolVar presence;
};

// For each task of each jobs, create the alternative tasks and link them to the
// main task of the job.
void CreateAlternativeTasks(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<JobTaskData>> job_to_tasks, int64_t horizon,
    std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives,
    CpModelBuilder& cp_model, const JobshopModelOptions& options) {
  const int num_jobs = problem.jobs_size();
  const BoolVar true_var = cp_model.TrueVar();

  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    job_task_to_alternatives[j].resize(num_tasks_in_job);
    const std::vector<JobTaskData>& tasks = job_to_tasks[j];

    const int64_t hard_start =
        job.has_earliest_start() ? job.earliest_start().value() : 0L;
    const int64_t hard_end =
        job.has_latest_end() ? job.latest_end().value() : horizon;

    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();
      CHECK_EQ(num_alternatives, task.duration_size());
      std::vector<AlternativeTaskData>& alternatives =
          job_task_to_alternatives[j][t];

      if (num_alternatives == 1) {
        if (options.use_variable_duration_to_encode_transition &&
            problem.machines(task.machine(0)).has_transition_time_matrix()) {
          const IntVar variable_duration = cp_model.NewIntVar(
              Domain(task.duration(0), hard_end - hard_start));
          const IntVar alt_end =
              cp_model.NewIntVar(Domain(hard_start, hard_end));
          const IntervalVar alt_interval = cp_model.NewIntervalVar(
              tasks[t].start, variable_duration, alt_end);
          alternatives.push_back({task.machine(0), alt_interval, true_var});
        } else {
          alternatives.push_back(
              {task.machine(0), tasks[t].interval, true_var});
        }
      } else {
        for (int a = 0; a < num_alternatives; ++a) {
          const BoolVar alt_presence = cp_model.NewBoolVar();
          const int64_t alt_duration = task.duration(a);
          const int alt_machine = task.machine(a);
          DCHECK_GE(hard_end - hard_start, alt_duration);
          const LinearExpr alt_start =
              options.use_optional_variables
                  ? cp_model.NewIntVar(
                        Domain(hard_start, hard_end - alt_duration))
                  : tasks[t].start;
          IntervalVar alt_interval;
          if (options.use_variable_duration_to_encode_transition &&
              problem.machines(alt_machine).has_transition_time_matrix()) {
            const IntVar variable_duration =
                cp_model.NewIntVar(Domain(alt_duration, hard_end - hard_start));
            const IntVar alt_end =
                cp_model.NewIntVar(Domain(hard_start, hard_end));
            alt_interval = cp_model.NewOptionalIntervalVar(
                alt_start, variable_duration, alt_end, alt_presence);
          } else {
            alt_interval = cp_model.NewOptionalFixedSizeIntervalVar(
                alt_start, alt_duration, alt_presence);
            if (!tasks[t].duration.IsConstant()) {
              cp_model.AddEquality(tasks[t].duration, alt_duration)
                  .OnlyEnforceIf(alt_presence);
            }
          }

          // Link local and global variables.
          if (options.use_optional_variables) {
            cp_model.AddEquality(tasks[t].start, alt_start)
                .OnlyEnforceIf(alt_presence);
          }

          alternatives.push_back({alt_machine, alt_interval, alt_presence});
        }

        // Exactly one alternative interval is present.
        std::vector<BoolVar> interval_presences;
        interval_presences.reserve(alternatives.size());
        for (const AlternativeTaskData& alternative : alternatives) {
          interval_presences.push_back(alternative.presence);
        }
        cp_model.AddExactlyOne(interval_presences);
      }
    }
  }
}

std::vector<std::vector<MachineTaskData>> GetDataPerMachine(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<std::vector<AlternativeTaskData>>>
        job_task_to_alternatives) {
  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();
  std::vector<std::vector<MachineTaskData>> machine_to_tasks(num_machines);
  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();
      CHECK_EQ(num_alternatives, task.duration_size());
      const std::vector<AlternativeTaskData>& alt_data =
          job_task_to_alternatives[j][t];
      for (int a = 0; a < num_alternatives; ++a) {
        machine_to_tasks[task.machine(a)].push_back(
            {j, alt_data[a].interval, task.duration(a)});
      }
    }
  }
  return machine_to_tasks;
}

void CreateMachines(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<std::vector<AlternativeTaskData>>>
        job_task_to_alternatives,
    IntervalVar makespan_interval, CpModelBuilder& cp_model,
    const JobshopModelOptions& options) {
  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();
  const std::vector<std::vector<MachineTaskData>> machine_to_tasks =
      GetDataPerMachine(problem, job_task_to_alternatives);

  // Add one no_overlap constraint per machine.
  for (int m = 0; m < num_machines; ++m) {
    std::vector<IntervalVar> intervals;
    for (const MachineTaskData& task : machine_to_tasks[m]) {
      intervals.push_back(task.interval);
    }
    if (options.use_interval_makespan &&
        problem.makespan_cost_per_time_unit() != 0L) {
      intervals.push_back(makespan_interval);
    }
    cp_model.AddNoOverlap(intervals);
  }

  // Add transition times if needed.
  //
  // TODO(user): If there is just a few non-zero transition, there is probably
  // a better way than this quadratic blowup.
  // TODO(user): Check for triangular inequalities.
  for (int m = 0; m < num_machines; ++m) {
    if (!problem.machines(m).has_transition_time_matrix()) continue;

    int64_t num_non_zero_transitions = 0;
    const int num_intervals = machine_to_tasks[m].size();
    const TransitionTimeMatrix& machine_transitions =
        problem.machines(m).transition_time_matrix();

    // Create circuit constraint on a machine. Node 0 is both the source and
    // sink, i.e. the first and last job.
    CircuitConstraint circuit = cp_model.AddCircuitConstraint();

    // If all intervals are optional, a solution without any performed
    // interval in this resource requires an empty circuit.
    BoolVar empty_circuit = cp_model.NewBoolVar();
    circuit.AddArc(0, 0, empty_circuit);

    for (int i = 0; i < num_intervals; ++i) {
      const int job_i = machine_to_tasks[m][i].job;
      const MachineTaskData& tail = machine_to_tasks[m][i];

      // TODO(user): simplify the code!
      CHECK_EQ(i, job_i);

      // Source to nodes.
      circuit.AddArc(0, i + 1, cp_model.NewBoolVar());
      // Node to sink.
      circuit.AddArc(i + 1, 0, cp_model.NewBoolVar());

      // If the circuit is empty, the interval cannot be performed.
      cp_model.AddImplication(empty_circuit, ~tail.interval.PresenceBoolVar());

      // Used to constrain the size of the tail interval.
      std::vector<BoolVar> literals;
      std::vector<int64_t> transitions;

      // Node to node.
      for (int j = 0; j < num_intervals; ++j) {
        if (i == j) {
          circuit.AddArc(i + 1, i + 1, ~tail.interval.PresenceBoolVar());
        } else {
          const MachineTaskData& head = machine_to_tasks[m][j];
          const int job_j = head.job;

          // TODO(user): simplify the code!
          CHECK_EQ(j, job_j);
          const int64_t transition =
              machine_transitions.transition_time(job_i * num_jobs + job_j);
          if (transition != 0) ++num_non_zero_transitions;

          const BoolVar lit = cp_model.NewBoolVar();
          circuit.AddArc(i + 1, j + 1, lit);

          if (options.use_variable_duration_to_encode_transition) {
            // Store the delays and the literals for the linear expression of
            // the size of the tail interval.
            literals.push_back(lit);
            transitions.push_back(transition);
            // This is redundant with the linear expression below, but makes
            // much shorter explanations.
            cp_model
                .AddEquality(tail.interval.SizeExpr(),
                             tail.fixed_duration + transition)
                .OnlyEnforceIf(lit);
          }

          // Make sure the interval follow the circuit in time.
          // Note that we use the start + duration + transition  as this is more
          // precise than the non-propagated end.
          cp_model
              .AddLessOrEqual(
                  tail.interval.StartExpr() + tail.fixed_duration + transition,
                  head.interval.StartExpr())
              .OnlyEnforceIf(lit);
        }
      }

      // Add a linear equation to define the size of the tail interval.
      if (options.use_variable_duration_to_encode_transition) {
        cp_model.AddEquality(tail.interval.SizeExpr(),
                             LinearExpr::WeightedSum(literals, transitions) +
                                 tail.fixed_duration);
      }
    }
    LOG(INFO) << "Machine " << m
              << ": #non_zero_transitions: " << num_non_zero_transitions << "/"
              << num_intervals * (num_intervals - 1)
              << ", #intervals: " << num_intervals;
  }
}

// Collect all objective terms and add them to the model.
void CreateObjective(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<JobTaskData>> job_to_tasks,
    absl::Span<const std::vector<std::vector<AlternativeTaskData>>>
        job_task_to_alternatives,
    int64_t horizon, IntVar makespan, CpModelBuilder& cp_model) {
  LinearExpr objective;
  const int num_jobs = problem.jobs_size();
  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();

    // Add the cost associated to each task.
    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();

      for (int a = 0; a < num_alternatives; ++a) {
        // Add cost if present.
        if (task.cost_size() > 0) {
          objective +=
              job_task_to_alternatives[j][t][a].presence * task.cost(a);
        }
      }
    }

    // Job lateness cost.
    const int64_t lateness_penalty = job.lateness_cost_per_time_unit();
    if (lateness_penalty != 0L) {
      const int64_t due_date = job.late_due_date();
      const LinearExpr job_end = job_to_tasks[j].back().end;
      if (due_date == 0) {
        objective += job_end * lateness_penalty;
      } else {
        const IntVar lateness_var = cp_model.NewIntVar(Domain(0, horizon));
        cp_model.AddMaxEquality(lateness_var, {0, job_end - due_date});
        objective += lateness_var * lateness_penalty;
      }
    }

    // Job earliness cost.
    const int64_t earliness_penalty = job.earliness_cost_per_time_unit();
    if (earliness_penalty != 0L) {
      const int64_t due_date = job.early_due_date();
      const LinearExpr job_end = job_to_tasks[j].back().end;

      if (due_date > 0) {
        const IntVar earliness_var = cp_model.NewIntVar(Domain(0, horizon));
        cp_model.AddMaxEquality(earliness_var, {0, due_date - job_end});
        objective += earliness_var * earliness_penalty;
      }
    }
  }

  // Makespan objective.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    objective += makespan * problem.makespan_cost_per_time_unit();
  }

  // Add the objective to the model.
  cp_model.Minimize(objective);
  if (problem.has_scaling_factor()) {
    // We use the protobuf API to set the scaling factor.
    cp_model.MutableProto()->mutable_objective()->set_scaling_factor(
        1.0 / problem.scaling_factor().value());
  }
}

// This is a relaxation of the problem where we only consider the main tasks,
// and not the alternate copies.
void AddCumulativeRelaxation(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<JobTaskData>> job_to_tasks,
    IntervalVar makespan_interval, CpModelBuilder& cp_model,
    const JobshopModelOptions& options) {
  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();

  // Connect two machines if a task has alternatives on these machines.
  DenseConnectedComponentsFinder finder;
  finder.SetNumberOfNodes(num_machines);
  int num_tasks = 0;
  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    num_tasks += num_tasks_in_job;
    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      for (int a = 1; a < task.machine_size(); ++a) {
        finder.AddEdge(task.machine(0), task.machine(a));
      }
    }
  }

  // Construct the set of machine components.
  const std::vector<int> machine_to_component = finder.GetComponentIds();
  CHECK_EQ(machine_to_component.size(), num_machines);
  std::vector<std::vector<int>> component_to_machines(
      finder.GetNumberOfComponents());
  for (int m = 0; m < num_machines; ++m) {
    component_to_machines[machine_to_component[m]].push_back(m);
  }
  LOG(INFO) << "Found " << component_to_machines.size()
            << " connected machine component";

  for (const std::vector<int>& machines : component_to_machines) {
    const absl::flat_hash_set<int> machine_set(machines.begin(),
                                               machines.end());
    std::vector<IntervalVar> connected_intervals;
    for (int j = 0; j < num_jobs; ++j) {
      const Job& job = problem.jobs(j);
      const int num_tasks_in_job = job.tasks_size();
      for (int t = 0; t < num_tasks_in_job; ++t) {
        const Task& task = job.tasks(t);
        for (const int m : task.machine()) {
          if (machine_set.contains(m)) {
            connected_intervals.push_back(job_to_tasks[j][t].interval);
            break;
          }
        }
      }
    }

    // Ignore trivial cases with at most one interval, or all intervals, or only
    // one machine.
    if (connected_intervals.size() <= 1 || machines.size() <= 1 ||
        machines.size() == num_tasks) {
      continue;
    }

    LOG(INFO) << "Interesting machine connected component: ["
              << absl::StrJoin(machines, ", ") << "] with "
              << connected_intervals.size() << " intervals";

    CumulativeConstraint cumul = cp_model.AddCumulative(machines.size());
    for (const IntervalVar& interval : connected_intervals) {
      cumul.AddDemand(interval, 1);
    }
    if (options.use_interval_makespan) {
      cumul.AddDemand(makespan_interval, machines.size());
    }
  }
}

// This redundant linear constraints states that the sum of durations of all
// tasks is a lower bound of the makespan * number of machines.
void AddMakespanRedundantConstraints(
    const JsspInputProblem& problem,
    absl::Span<const std::vector<JobTaskData>> job_to_tasks, IntVar makespan,
    CpModelBuilder& cp_model) {
  const int num_machines = problem.machines_size();

  // Global energetic reasoning.
  LinearExpr sum_of_duration;
  for (const std::vector<JobTaskData>& tasks : job_to_tasks) {
    for (const JobTaskData& task : tasks) {
      sum_of_duration += task.duration;
    }
  }
  cp_model.AddLessOrEqual(sum_of_duration, makespan * num_machines);
}

void DisplayJobStatistics(
    const JsspInputProblem& problem, int64_t horizon,
    absl::Span<const std::vector<JobTaskData>> job_to_tasks,
    absl::Span<const std::vector<std::vector<AlternativeTaskData>>>
        job_task_to_alternatives) {
  const int num_jobs = job_to_tasks.size();
  int num_tasks = 0;
  int num_tasks_with_variable_duration = 0;
  int num_tasks_with_alternatives = 0;
  for (const std::vector<JobTaskData>& job : job_to_tasks) {
    num_tasks += job.size();
    for (const JobTaskData& task : job) {
      if (!task.duration.IsConstant()) {
        num_tasks_with_variable_duration++;
      }
    }
  }
  for (const std::vector<std::vector<AlternativeTaskData>>&
           task_to_alternatives : job_task_to_alternatives) {
    for (const std::vector<AlternativeTaskData>& alternatives :
         task_to_alternatives) {
      if (alternatives.size() > 1) num_tasks_with_alternatives++;
    }
  }

  LOG(INFO) << "#machines:" << problem.machines_size();
  LOG(INFO) << "#jobs:" << num_jobs;
  LOG(INFO) << "horizon:" << horizon;
  LOG(INFO) << "#tasks: " << num_tasks;
  LOG(INFO) << "#tasks with alternative: " << num_tasks_with_alternatives;
  LOG(INFO) << "#tasks with variable duration: "
            << num_tasks_with_variable_duration;
}

}  // namespace

JobshopModelMapping CreateJobshopModel(const JsspInputProblem& problem,
                                       const JobshopModelOptions& options,
                                       CpModelBuilder* builder) {
  CpModelBuilder& cp_model = *builder;

  if (!problem.name().empty()) {
    cp_model.SetName(problem.name());
  }

  // Compute an over estimate of the horizon.
  const int64_t horizon =
      options.horizon != -1 ? options.horizon : ComputeHorizon(problem);

  // Create the main job structure.
  const int num_jobs = problem.jobs_size();
  std::vector<std::vector<JobTaskData>> job_to_tasks(num_jobs);
  CreateJobs(problem, horizon, job_to_tasks, cp_model);

  // For each task of each jobs, create the alternative copies if needed and
  // fill in the AlternativeTaskData vector.
  std::vector<std::vector<std::vector<AlternativeTaskData>>>
      job_task_to_alternatives(num_jobs);
  CreateAlternativeTasks(problem, job_to_tasks, horizon,
                         job_task_to_alternatives, cp_model, options);

  // Create the makespan variable and interval.
  // If this flag is true, we will add to each no overlap constraint a special
  // "makespan interval" that must necessarily be last by construction. This
  // gives us a better lower bound on the makespan because this way we known
  // that it must be after all other intervals in each no-overlap constraint.
  //
  // Otherwise, we will just add precedence constraints between the last task of
  // each job and the makespan variable. Alternatively, we could have added a
  // precedence relation between all tasks and the makespan for a similar
  // propagation thanks to our "precedence" propagator in the disjunctive but
  // that was slower than the interval trick when I tried.
  const IntVar makespan = cp_model.NewIntVar(Domain(0, horizon));
  IntervalVar makespan_interval;
  if (problem.makespan_cost_per_time_unit() != 0L) {
    if (options.use_interval_makespan) {
      makespan_interval = cp_model.NewIntervalVar(
          /*start=*/makespan,
          /*size=*/cp_model.NewIntVar(Domain(1, horizon)),
          /*end=*/cp_model.NewIntVar(Domain(horizon + 1)));
    }
    for (int j = 0; j < num_jobs; ++j) {
      // The makespan will be greater than the end of each job.
      cp_model.AddLessOrEqual(job_to_tasks[j].back().end, makespan);
    }
  }

  // Display model statistics before creating the machine as they may display
  // additional statistics.
  DisplayJobStatistics(problem, horizon, job_to_tasks,
                       job_task_to_alternatives);

  // Machine constraints.
  CreateMachines(problem, job_task_to_alternatives, makespan_interval, cp_model,
                 options);

  // Try to detect connected components of alternative machines.
  // If this is happens, we can add a cumulative constraint as a relaxation of
  // all no_ovelap constraints on the set of alternative machines.
  if (options.use_cumulative_relaxation &&
      problem.makespan_cost_per_time_unit() != 0) {
    AddCumulativeRelaxation(problem, job_to_tasks, makespan_interval, cp_model,
                            options);
  }

  // This redundant makespan constraint is here mostly to improve the LP
  // relaxation.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    AddMakespanRedundantConstraints(problem, job_to_tasks, makespan, cp_model);
  }

  // Add job precedences.
  for (const JobPrecedence& precedence : problem.precedences()) {
    const LinearExpr start =
        job_to_tasks[precedence.second_job_index()].front().start;
    const LinearExpr end =
        job_to_tasks[precedence.first_job_index()].back().end;
    cp_model.AddLessOrEqual(end + precedence.min_delay(), start);
  }

  // Objective.
  CreateObjective(problem, job_to_tasks, job_task_to_alternatives, horizon,
                  makespan, cp_model);

  return {
      .job_to_tasks = job_to_tasks,
      .machine_to_tasks = GetDataPerMachine(problem, job_task_to_alternatives)};
}

}  // namespace sat
}  // namespace operations_research
