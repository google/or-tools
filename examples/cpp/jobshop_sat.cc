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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/scheduling/jobshop_scheduling.pb.h"
#include "ortools/scheduling/jobshop_scheduling_parser.h"

ABSL_FLAG(std::string, input, "", "Jobshop data file name.");
ABSL_FLAG(std::string, params, "", "Sat parameters in text proto format.");
ABSL_FLAG(bool, use_optional_variables, false,
          "Whether we use optional variables for bounds of an optional "
          "interval or not.");
ABSL_FLAG(bool, use_interval_makespan, true,
          "Whether we encode the makespan using an interval or not.");
ABSL_FLAG(bool, use_variable_duration_to_encode_transition, false,
          "Whether we move the transition cost to the alternative duration.");
ABSL_FLAG(
    bool, use_cumulative_relaxation, true,
    "Whether we regroup multiple machines to create a cumulative relaxation.");
ABSL_FLAG(bool, display_model, false, "Display jobshop proto before solving.");
ABSL_FLAG(bool, display_sat_model, false, "Display sat proto before solving.");
ABSL_FLAG(int, horizon, -1, "Override horizon computation.");

using operations_research::scheduling::jssp::Job;
using operations_research::scheduling::jssp::JobPrecedence;
using operations_research::scheduling::jssp::JsspInputProblem;
using operations_research::scheduling::jssp::Machine;
using operations_research::scheduling::jssp::Task;
using operations_research::scheduling::jssp::TransitionTimeMatrix;

namespace operations_research {
namespace sat {

// Compute a valid horizon from a problem.
int64_t ComputeHorizon(const JsspInputProblem& problem) {
  int64_t sum_of_durations = 0;
  int64_t max_latest_end = 0;
  int64_t max_earliest_start = 0;
  for (const Job& job : problem.jobs()) {
    if (job.has_latest_end()) {
      max_latest_end = std::max(max_latest_end, job.latest_end().value());
    } else {
      max_latest_end = std::numeric_limits<int64_t>::max();
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

// A job is a sequence of tasks. For each task, we store the main interval, as
// well as its start, size, and end variables.
struct JobTaskData {
  IntervalVar interval;
  IntVar start;
  IntVar duration;
  IntVar end;
};

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

      const IntVar start = cp_model.NewIntVar(Domain(hard_start, hard_end));
      const IntVar duration = cp_model.NewIntVar(Domain::FromValues(durations));
      const IntVar end = cp_model.NewIntVar(Domain(hard_start, hard_end));
      const IntervalVar interval =
          cp_model.NewIntervalVar(start, duration, end);

      // Fill in job_to_tasks.
      task_data.push_back({interval, start, duration, end});

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
    const std::vector<std::vector<JobTaskData>>& job_to_tasks, int64_t horizon,
    std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives,
    CpModelBuilder& cp_model) {
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
        if (absl::GetFlag(FLAGS_use_variable_duration_to_encode_transition) &&
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
          const IntVar alt_start =
              absl::GetFlag(FLAGS_use_optional_variables)
                  ? cp_model.NewIntVar(
                        Domain(hard_start, hard_end - alt_duration))
                  : tasks[t].start;
          IntervalVar alt_interval;
          if (absl::GetFlag(FLAGS_use_variable_duration_to_encode_transition) &&
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
          }

          // Link local and global variables.
          if (absl::GetFlag(FLAGS_use_optional_variables)) {
            cp_model.AddEquality(tasks[t].start, alt_start)
                .OnlyEnforceIf(alt_presence);
            cp_model.AddEquality(tasks[t].duration, alt_duration)
                .OnlyEnforceIf(alt_presence);
          }

          alternatives.push_back({alt_machine, alt_interval, alt_presence});
        }
        // Exactly one alternative interval is present.
        std::vector<BoolVar> interval_presences;
        for (const AlternativeTaskData& alternative : alternatives) {
          interval_presences.push_back(alternative.presence);
        }
        cp_model.AddEquality(LinearExpr::BooleanSum(interval_presences), 1);
      }
    }
  }
}

// Add a linear equation that links the duration of a task with all the
// alternative durations and presence literals.
void AddAlternativeTaskDurationRelaxation(
    const JsspInputProblem& problem,
    const std::vector<std::vector<JobTaskData>>& job_to_tasks,
    std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives,
    CpModelBuilder& cp_model) {
  const int num_jobs = problem.jobs_size();

  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    const std::vector<JobTaskData>& tasks = job_to_tasks[j];
    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();

      int64_t min_duration = std::numeric_limits<int64_t>::max();
      int64_t max_duration = std::numeric_limits<int64_t>::min();
      for (const int64_t alt_duration : task.duration()) {
        min_duration = std::min(min_duration, alt_duration);
        max_duration = std::max(max_duration, alt_duration);
      }

      // If all all_duration are equals, then the equation is redundant with the
      // interval constraint of the main task.
      if (min_duration == max_duration) return;

      // Shifting all durations by their min value, improves the propagation
      // of the linear equation.
      std::vector<BoolVar> presence_literals;
      std::vector<int64_t> shifted_durations;
      for (int a = 0; a < num_alternatives; ++a) {
        const int64_t alt_duration = task.duration(a);
        if (alt_duration != min_duration) {
          shifted_durations.push_back(alt_duration - min_duration);
          presence_literals.push_back(
              job_task_to_alternatives[j][t][a].presence);
        }
      }
      // end == start + min_duration +
      //        sum(shifted_duration[i] * presence_literals[i])
      cp_model.AddEquality(
          LinearExpr::ScalProd({tasks[t].end, tasks[t].start}, {1, -1}),
          LinearExpr::BooleanScalProd(presence_literals, shifted_durations)
              .AddConstant(min_duration));
    }
  }
}

// Tasks or alternative tasks are added to machines one by one.
// This structure records the characteristics of each task added on a machine.
// This information is indexed on each vector by the order of addition.
struct MachineTaskData {
  int job;
  IntervalVar interval;
  int64_t fixed_duration;
};

std::vector<std::vector<MachineTaskData>> GetDataPerMachine(
    const JsspInputProblem& problem,
    const std::vector<std::vector<std::vector<AlternativeTaskData>>>&
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
    const std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives,
    IntervalVar makespan_interval, CpModelBuilder& cp_model) {
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
    if (absl::GetFlag(FLAGS_use_interval_makespan) &&
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
    for (int i = 0; i < num_intervals; ++i) {
      const int job_i = machine_to_tasks[m][i].job;
      const MachineTaskData& tail = machine_to_tasks[m][i];

      // TODO(user, lperron): simplify the code!
      CHECK_EQ(i, job_i);

      // Source to nodes.
      circuit.AddArc(0, i + 1, cp_model.NewBoolVar());
      // Node to sink.
      const BoolVar sink_literal = cp_model.NewBoolVar();
      circuit.AddArc(i + 1, 0, sink_literal);

      // Used to constrain the size of the tail interval.
      std::vector<BoolVar> literals;
      std::vector<int64_t> transitions;

      // Node to node.
      for (int j = 0; j < num_intervals; ++j) {
        if (i == j) {
          circuit.AddArc(i + 1, i + 1, Not(tail.interval.PresenceBoolVar()));
        } else {
          const MachineTaskData& head = machine_to_tasks[m][j];
          const int job_j = head.job;

          // TODO(user, lperron): simplify the code!
          CHECK_EQ(j, job_j);
          const int64_t transition =
              machine_transitions.transition_time(job_i * num_jobs + job_j);
          if (transition != 0) ++num_non_zero_transitions;

          const BoolVar lit = cp_model.NewBoolVar();
          circuit.AddArc(i + 1, j + 1, lit);

          if (absl::GetFlag(FLAGS_use_variable_duration_to_encode_transition)) {
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
          // Note that we use the start + delay as this is more precise than
          // the non-propagated end.
          cp_model
              .AddLessOrEqual(tail.interval.StartExpr().AddConstant(
                                  tail.fixed_duration + transition),
                              head.interval.StartExpr())
              .OnlyEnforceIf(lit);
        }
      }

      // Add a linear equation to define the size of the tail interval.
      if (absl::GetFlag(FLAGS_use_variable_duration_to_encode_transition)) {
        cp_model.AddEquality(tail.interval.SizeExpr(),
                             LinearExpr::BooleanScalProd(literals, transitions)
                                 .AddConstant(tail.fixed_duration));
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
    const std::vector<std::vector<JobTaskData>>& job_to_tasks,
    const std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives,
    int64_t horizon, IntVar makespan, CpModelBuilder& cp_model) {
  int64_t objective_offset = 0;
  std::vector<IntVar> objective_vars;
  std::vector<int64_t> objective_coeffs;

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
          objective_vars.push_back(job_task_to_alternatives[j][t][a].presence);
          objective_coeffs.push_back(task.cost(a));
        }
      }
    }

    // Job lateness cost.
    const int64_t lateness_penalty = job.lateness_cost_per_time_unit();
    if (lateness_penalty != 0L) {
      const int64_t due_date = job.late_due_date();
      const IntVar job_end = job_to_tasks[j].back().end;
      if (due_date == 0) {
        objective_vars.push_back(job_end);
        objective_coeffs.push_back(lateness_penalty);
      } else {
        const IntVar lateness_var = cp_model.NewIntVar(Domain(0, horizon));
        cp_model.AddLinMaxEquality(
            lateness_var, {LinearExpr(0), job_end.AddConstant(-due_date)});
        objective_vars.push_back(lateness_var);
        objective_coeffs.push_back(lateness_penalty);
      }
    }

    // Job earliness cost.
    const int64_t earliness_penalty = job.earliness_cost_per_time_unit();
    if (earliness_penalty != 0L) {
      const int64_t due_date = job.early_due_date();
      const IntVar job_end = job_to_tasks[j].back().end;

      if (due_date > 0) {
        const IntVar earliness_var = cp_model.NewIntVar(Domain(0, horizon));
        cp_model.AddLinMaxEquality(
            earliness_var,
            {LinearExpr(0),
             LinearExpr::Term(job_end, -1).AddConstant(due_date)});
        objective_vars.push_back(earliness_var);
        objective_coeffs.push_back(earliness_penalty);
      }
    }
  }

  // Makespan objective.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    objective_coeffs.push_back(problem.makespan_cost_per_time_unit());
    objective_vars.push_back(makespan);
  }

  // Add the objective to the model.
  cp_model.Minimize(LinearExpr::ScalProd(objective_vars, objective_coeffs)
                        .AddConstant(objective_offset));
  if (problem.has_scaling_factor()) {
    cp_model.ScaleObjectiveBy(problem.scaling_factor().value());
  }
}

// This is a relaxation of the problem where we only consider the main tasks,
// and not the alternate copies.
void AddCumulativeRelaxation(
    const JsspInputProblem& problem,
    const std::vector<std::vector<JobTaskData>>& job_to_tasks,
    IntervalVar makespan_interval, CpModelBuilder& cp_model) {
  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();

  // Build a graph where two machines are connected if they appear in the same
  // set of alternate machines for a given task.
  int num_tasks = 0;
  std::vector<absl::flat_hash_set<int>> neighbors(num_machines);
  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    const int num_tasks_in_job = job.tasks_size();
    num_tasks += num_tasks_in_job;
    for (int t = 0; t < num_tasks_in_job; ++t) {
      const Task& task = job.tasks(t);
      for (int a = 1; a < task.machine_size(); ++a) {
        neighbors[task.machine(0)].insert(task.machine(a));
      }
    }
  }

  // Search for connected components in the above graph.
  std::vector<int> components =
      util::GetConnectedComponents(num_machines, neighbors);
  absl::flat_hash_map<int, std::vector<int>> machines_per_component;
  for (int c = 0; c < components.size(); ++c) {
    machines_per_component[components[c]].push_back(c);
  }
  LOG(INFO) << "Found " << machines_per_component.size()
            << " connected machine components.";

  for (const auto& it : machines_per_component) {
    // Ignore the trivial cases.
    if (it.second.size() < 2 || it.second.size() == num_machines) continue;
    absl::flat_hash_set<int> component(it.second.begin(), it.second.end());
    std::vector<IntervalVar> intervals;
    for (int j = 0; j < num_jobs; ++j) {
      const Job& job = problem.jobs(j);
      const int num_tasks_in_job = job.tasks_size();
      for (int t = 0; t < num_tasks_in_job; ++t) {
        const Task& task = job.tasks(t);
        for (const int m : task.machine()) {
          if (component.contains(m)) {
            intervals.push_back(job_to_tasks[j][t].interval);
            break;
          }
        }
      }
    }

    LOG(INFO) << "Found machine connected component: ["
              << absl::StrJoin(it.second, ", ") << "] with " << intervals.size()
              << " intervals";
    // Ignore trivial case with all intervals.
    if (intervals.size() == 1 || intervals.size() == num_tasks) continue;

    const IntVar capacity = cp_model.NewConstant(component.size());
    const IntVar one = cp_model.NewConstant(1);
    CumulativeConstraint cumul = cp_model.AddCumulative(capacity);
    for (int i = 0; i < intervals.size(); ++i) {
      cumul.AddDemand(intervals[i], one);
    }
    if (absl::GetFlag(FLAGS_use_interval_makespan)) {
      cumul.AddDemand(makespan_interval, capacity);
    }
  }
}

// This redundant linear constraints states that the sum of durations of all
// tasks is a lower bound of the makespan * number of machines.
void AddMakespanRedundantConstraints(
    const JsspInputProblem& problem,
    const std::vector<std::vector<JobTaskData>>& job_to_tasks, IntVar makespan,
    CpModelBuilder& cp_model) {
  const int num_machines = problem.machines_size();

  // Global energetic reasoning.
  std::vector<IntVar> all_task_durations;
  for (const std::vector<JobTaskData>& tasks : job_to_tasks) {
    for (const JobTaskData& task : tasks) {
      all_task_durations.push_back(task.duration);
    }
  }
  cp_model.AddLessOrEqual(LinearExpr::Sum(all_task_durations),
                          LinearExpr::Term(makespan, num_machines));
}

void DisplayJobStatistics(
    const JsspInputProblem& problem, int64_t horizon,
    const std::vector<std::vector<JobTaskData>>& job_to_tasks,
    const std::vector<std::vector<std::vector<AlternativeTaskData>>>&
        job_task_to_alternatives) {
  const int num_jobs = job_to_tasks.size();
  int num_tasks = 0;
  int num_tasks_with_variable_duration = 0;
  int num_tasks_with_alternatives = 0;
  for (const std::vector<JobTaskData>& job : job_to_tasks) {
    num_tasks += job.size();
    for (const JobTaskData& task : job) {
      if (task.duration.Proto().domain_size() != 2 ||
          task.duration.Proto().domain(0) != task.duration.Proto().domain(1)) {
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

// Solve a JobShop scheduling problem using CP-SAT.
void Solve(const JsspInputProblem& problem) {
  if (absl::GetFlag(FLAGS_display_model)) {
    LOG(INFO) << problem.DebugString();
  }

  CpModelBuilder cp_model;

  // Compute an over estimate of the horizon.
  const int64_t horizon = absl::GetFlag(FLAGS_horizon) != -1
                              ? absl::GetFlag(FLAGS_horizon)
                              : ComputeHorizon(problem);

  // Create the main job structure.
  const int num_jobs = problem.jobs_size();
  std::vector<std::vector<JobTaskData>> job_to_tasks(num_jobs);
  CreateJobs(problem, horizon, job_to_tasks, cp_model);

  // For each task of each jobs, create the alternative copies if needed and
  // fill in the AlternativeTaskData vector.
  std::vector<std::vector<std::vector<AlternativeTaskData>>>
      job_task_to_alternatives(num_jobs);
  CreateAlternativeTasks(problem, job_to_tasks, horizon,
                         job_task_to_alternatives, cp_model);

  // Note that this is the only place where the duration of a task is linked
  // with the duration of its alternatives.
  AddAlternativeTaskDurationRelaxation(problem, job_to_tasks,
                                       job_task_to_alternatives, cp_model);

  // Create the makespan variable and interval.
  // If this flag is true, we will add to each no overlap constraint a special
  // "makespan interval" that must necessarily be last by construction. This
  // gives us a better lower bound on the makespan because this way we known
  // that it must be after all other intervals in each no-overlap constraint.
  //
  // Otherwise, we will just add precence constraints between the last task of
  // each job and the makespan variable. Alternatively, we could have added a
  // precedence relation between all tasks and the makespan for a similar
  // propagation thanks to our "precedence" propagator in the dijsunctive but
  // that was slower than the interval trick when I tried.
  const IntVar makespan = cp_model.NewIntVar(Domain(0, horizon));
  IntervalVar makespan_interval;
  if (absl::GetFlag(FLAGS_use_interval_makespan)) {
    makespan_interval = cp_model.NewIntervalVar(
        /*start=*/makespan,
        /*size=*/cp_model.NewIntVar(Domain(1, horizon)),
        /*end=*/cp_model.NewIntVar(Domain(horizon + 1)));
  } else if (problem.makespan_cost_per_time_unit() != 0L) {
    for (int j = 0; j < num_jobs; ++j) {
      // The makespan will be greater than the end of each job.
      // This is not needed if we add the makespan "interval" to each
      // disjunctive.
      cp_model.AddLessOrEqual(job_to_tasks[j].back().end, makespan);
    }
  }

  // Display model statistics before creating the machine as they may display
  // additional statistics.
  DisplayJobStatistics(problem, horizon, job_to_tasks,
                       job_task_to_alternatives);

  // Machine constraints.
  CreateMachines(problem, job_task_to_alternatives, makespan_interval,
                 cp_model);

  // Try to detect connected components of alternative machines.
  // If this is happens, we can add a cumulative constraint as a relaxation of
  // all no_ovelap constraints on the set of alternative machines.
  if (absl::GetFlag(FLAGS_use_cumulative_relaxation)) {
    AddCumulativeRelaxation(problem, job_to_tasks, makespan_interval, cp_model);
  }

  // This redundant makespan constraint is here mostly to improve the LP
  // relaxation.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    AddMakespanRedundantConstraints(problem, job_to_tasks, makespan, cp_model);
  }

  // Add job precedences.
  for (const JobPrecedence& precedence : problem.precedences()) {
    const IntVar start =
        job_to_tasks[precedence.second_job_index()].front().start;
    const IntVar end = job_to_tasks[precedence.first_job_index()].back().end;
    cp_model.AddLessOrEqual(end.AddConstant(precedence.min_delay()), start);
  }

  // Objective.
  CreateObjective(problem, job_to_tasks, job_task_to_alternatives, horizon,
                  makespan, cp_model);

  // Decision strategy.
  // CP-SAT now has a default strategy for scheduling problem that works best.

  if (absl::GetFlag(FLAGS_display_sat_model)) {
    LOG(INFO) << cp_model.Proto().DebugString();
  }

  // Setup parameters.
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  // Parse the --params flag.
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);

  // Abort if we don't have any solution.
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return;

  // Check transitions.
  {
    const int num_machines = problem.machines_size();
    const std::vector<std::vector<MachineTaskData>> machine_to_tasks =
        GetDataPerMachine(problem, job_task_to_alternatives);
    for (int m = 0; m < num_machines; ++m) {
      if (!problem.machines(m).has_transition_time_matrix()) continue;

      struct Data {
        int job;
        int64_t fixed_duration;
        int64_t start;
        int64_t end;
      };
      std::vector<Data> schedule;
      for (const MachineTaskData& d : machine_to_tasks[m]) {
        if (!SolutionBooleanValue(response, d.interval.PresenceBoolVar())) {
          continue;
        }
        schedule.push_back(
            {d.job, d.fixed_duration,
             SolutionIntegerValue(response, d.interval.StartExpr()),
             SolutionIntegerValue(response, d.interval.EndExpr())});
      }
      std::sort(schedule.begin(), schedule.end(),
                [](const Data& a, const Data& b) { return a.start < b.start; });

      const TransitionTimeMatrix& transitions =
          problem.machines(m).transition_time_matrix();

      int last_job = -1;
      int64_t last_start = std::numeric_limits<int64_t>::min();
      int64_t last_duration;
      for (const Data& d : schedule) {
        const int64_t transition =
            last_job == -1
                ? 0
                : transitions.transition_time(last_job * num_jobs + d.job);
        CHECK_LE(last_start + last_duration + transition, d.start);
        last_job = d.job;
        last_start = d.start;
        last_duration = d.fixed_duration;
      }
    }
  }

  // Check cost, recompute it from scratch.
  int64_t final_cost = 0;
  if (problem.makespan_cost_per_time_unit() != 0) {
    int64_t makespan = 0;
    for (const std::vector<JobTaskData>& tasks : job_to_tasks) {
      const IntVar job_end = tasks.back().end;
      makespan = std::max(makespan, SolutionIntegerValue(response, job_end));
    }
    final_cost += makespan * problem.makespan_cost_per_time_unit();
  }

  for (int j = 0; j < num_jobs; ++j) {
    const int64_t early_due_date = problem.jobs(j).early_due_date();
    const int64_t late_due_date = problem.jobs(j).late_due_date();
    const int64_t early_penalty =
        problem.jobs(j).earliness_cost_per_time_unit();
    const int64_t late_penalty = problem.jobs(j).lateness_cost_per_time_unit();
    const int64_t end =
        SolutionIntegerValue(response, job_to_tasks[j].back().end);
    if (end < early_due_date && early_penalty != 0) {
      final_cost += (early_due_date - end) * early_penalty;
    }
    if (end > late_due_date && late_penalty != 0) {
      final_cost += (end - late_due_date) * late_penalty;
    }
  }

  // Note that this the objective is a variable of the model, there is actually
  // no strong guarantee that in an intermediate solution, it is packed to its
  // minimum possible value. We do observe this from time to time. The DCHECK is
  // mainly to warn when this happen.
  //
  // TODO(user): Support alternative cost in check.
  const double tolerance = 1e-6;
  DCHECK_GE(response.objective_value(), final_cost - tolerance);
  DCHECK_LE(response.objective_value(), final_cost + tolerance);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::scheduling::jssp::JsspParser parser;
  CHECK(parser.ParseFile(absl::GetFlag(FLAGS_input)));
  operations_research::sat::Solve(parser.problem());
  return EXIT_SUCCESS;
}
