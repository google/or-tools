// Copyright 2010-2018 Google LLC
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
#include <vector>

#include "absl/strings/match.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/data/jobshop_scheduling.pb.h"
#include "ortools/data/jobshop_scheduling_parser.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"

DEFINE_string(input, "", "Jobshop data file name.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
DEFINE_bool(use_optional_variables, true,
            "Whether we use optional variables for bounds of an optional "
            "interval or not.");
DEFINE_bool(display_model, false, "Display jobshop proto before solving.");
DEFINE_bool(display_sat_model, false, "Display sat proto before solving.");

using operations_research::data::jssp::Job;
using operations_research::data::jssp::JobPrecedence;
using operations_research::data::jssp::JsspInputProblem;
using operations_research::data::jssp::Machine;
using operations_research::data::jssp::Task;
using operations_research::data::jssp::TransitionTimeMatrix;

namespace operations_research {
namespace sat {

// Compute a valid horizon from a problem.
int64 ComputeHorizon(const JsspInputProblem& problem) {
  int64 sum_of_durations = 0;
  int64 max_latest_end = 0;
  int64 max_earliest_start = 0;
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
      int64 max_duration = 0;
      for (int64 d : task.duration()) {
        max_duration = std::max(max_duration, d);
      }
      sum_of_durations += max_duration;
    }
  }

  const int num_jobs = problem.jobs_size();
  int64 sum_of_transitions = 0;
  for (const Machine& machine : problem.machines()) {
    if (!machine.has_transition_time_matrix()) continue;
    const TransitionTimeMatrix& matrix = machine.transition_time_matrix();
    for (int i = 0; i < num_jobs; ++i) {
      int64 max_transition = 0;
      for (int j = 0; j < num_jobs; ++j) {
        max_transition =
            std::max(max_transition, matrix.transition_time(i * num_jobs + j));
      }
      sum_of_transitions += max_transition;
    }
  }
  return std::min(max_latest_end,
                  sum_of_durations + sum_of_transitions + max_earliest_start);
  // TODO(user): Uses transitions.
}

// Solve a JobShop scheduling problem using SAT.
void Solve(const JsspInputProblem& problem) {
  if (FLAGS_display_model) {
    LOG(INFO) << problem.DebugString();
  }

  CpModelBuilder cp_model;

  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();
  const int64 horizon = ComputeHorizon(problem);

  std::vector<int> starts;
  std::vector<int> ends;

  const Domain all_horizon(0, horizon);

  const IntVar makespan = cp_model.NewIntVar(all_horizon);

  std::vector<std::vector<IntervalVar>> machine_to_intervals(num_machines);
  std::vector<std::vector<int>> machine_to_jobs(num_machines);
  std::vector<std::vector<IntVar>> machine_to_starts(num_machines);
  std::vector<std::vector<IntVar>> machine_to_ends(num_machines);
  std::vector<std::vector<BoolVar>> machine_to_presences(num_machines);
  std::vector<IntVar> job_starts(num_jobs);
  std::vector<IntVar> job_ends(num_jobs);
  std::vector<IntVar> task_starts;
  int64 objective_offset = 0;
  std::vector<IntVar> objective_vars;
  std::vector<int64> objective_coeffs;

  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    IntVar previous_end;
    const int64 hard_start =
        job.has_earliest_start() ? job.earliest_start().value() : 0L;
    const int64 hard_end =
        job.has_latest_end() ? job.latest_end().value() : horizon;

    for (int t = 0; t < job.tasks_size(); ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();
      CHECK_EQ(num_alternatives, task.duration_size());

      // Add the "main" task interval. It will englobe all the alternative ones
      // if there is many, or be a normal task otherwise.
      int64 min_duration = task.duration(0);
      int64 max_duration = task.duration(0);
      for (int i = 1; i < num_alternatives; ++i) {
        min_duration = std::min(min_duration, task.duration(i));
        max_duration = std::max(max_duration, task.duration(i));
      }
      const IntVar start = cp_model.NewIntVar(Domain(hard_start, hard_end));
      const IntVar duration =
          cp_model.NewIntVar(Domain(min_duration, max_duration));
      const IntVar end = cp_model.NewIntVar(Domain(hard_start, hard_end));
      const IntervalVar interval =
          cp_model.NewIntervalVar(start, duration, end);

      // Store starts and ends of jobs for precedences.
      if (t == 0) {
        job_starts[j] = start;
      }
      if (t == job.tasks_size() - 1) {
        job_ends[j] = end;
      }
      task_starts.push_back(start);

      // Chain the task belonging to the same job.
      if (t > 0) {
        cp_model.AddLessOrEqual(previous_end, start);
      }
      previous_end = end;

      if (num_alternatives == 1) {
        const int m = task.machine(0);
        machine_to_intervals[m].push_back(interval);
        machine_to_jobs[m].push_back(j);
        machine_to_starts[m].push_back(start);
        machine_to_ends[m].push_back(end);
        machine_to_presences[m].push_back(cp_model.TrueVar());
        if (task.cost_size() > 0) {
          objective_offset += task.cost(0);
        }
      } else {
        std::vector<BoolVar> presences;
        for (int a = 0; a < num_alternatives; ++a) {
          const BoolVar presence = cp_model.NewBoolVar();
          const IntVar local_start =
              FLAGS_use_optional_variables
                  ? cp_model.NewIntVar(Domain(hard_start, hard_end))
                  : start;
          const IntVar local_duration = cp_model.NewConstant(task.duration(a));
          const IntVar local_end =
              FLAGS_use_optional_variables
                  ? cp_model.NewIntVar(Domain(hard_start, hard_end))
                  : end;
          const IntervalVar local_interval = cp_model.NewOptionalIntervalVar(
              local_start, local_duration, local_end, presence);

          // Link local and global variables.
          if (FLAGS_use_optional_variables) {
            cp_model.AddEquality(start, local_start).OnlyEnforceIf(presence);
            cp_model.AddEquality(end, local_end).OnlyEnforceIf(presence);

            // TODO(user): Experiment with the following implication.
            cp_model.AddEquality(duration, local_duration)
                .OnlyEnforceIf(presence);
          }

          // Record relevant variables for later use.
          const int m = task.machine(a);
          machine_to_intervals[m].push_back(local_interval);
          machine_to_jobs[m].push_back(j);
          machine_to_starts[m].push_back(local_start);
          machine_to_ends[m].push_back(local_end);
          machine_to_presences[m].push_back(presence);

          // Add cost if present.
          if (task.cost_size() > 0) {
            objective_vars.push_back(presence);
            objective_coeffs.push_back(task.cost(a));
          }
          // Collect presence variables.
          presences.push_back(presence);
        }
        // Exactly one alternative interval is present.
        cp_model.AddEquality(LinearExpr::BooleanSum(presences), 1);
      }
    }

    // The makespan will be greater than the end of each job.
    if (problem.makespan_cost_per_time_unit() != 0L) {
      cp_model.AddLessOrEqual(previous_end, makespan);
    }

    const int64 lateness_penalty = job.lateness_cost_per_time_unit();
    // Lateness cost.
    if (lateness_penalty != 0L) {
      const int64 due_date = job.late_due_date();
      if (due_date == 0) {
        objective_vars.push_back(previous_end);
        objective_coeffs.push_back(lateness_penalty);
      } else {
        const IntVar shifted_var =
            cp_model.NewIntVar(Domain(-due_date, horizon - due_date));
        cp_model.AddEquality(shifted_var,
                             LinearExpr(previous_end).AddConstant(-due_date));
        const IntVar lateness_var = cp_model.NewIntVar(all_horizon);
        cp_model.AddMaxEquality(lateness_var,
                                {cp_model.NewConstant(0), shifted_var});
        objective_vars.push_back(lateness_var);
        objective_coeffs.push_back(lateness_penalty);
      }
    }
    const int64 earliness_penalty = job.earliness_cost_per_time_unit();
    // Earliness cost.
    if (earliness_penalty != 0L) {
      const int64 due_date = job.early_due_date();
      if (due_date > 0) {
        const IntVar shifted_var =
            cp_model.NewIntVar(Domain(due_date - horizon, due_date));
        cp_model.AddEquality(LinearExpr::Sum({shifted_var, previous_end}),
                             due_date);
        const IntVar earliness_var = cp_model.NewIntVar(all_horizon);
        cp_model.AddMaxEquality(earliness_var,
                                {cp_model.NewConstant(0), shifted_var});
        objective_vars.push_back(earliness_var);
        objective_coeffs.push_back(earliness_penalty);
      }
    }
  }

  // Add one no_overlap constraint per machine.
  for (int m = 0; m < num_machines; ++m) {
    cp_model.AddNoOverlap(machine_to_intervals[m]);

    if (problem.machines(m).has_transition_time_matrix()) {
      const TransitionTimeMatrix& transitions =
          problem.machines(m).transition_time_matrix();
      const int num_intervals = machine_to_intervals[m].size();

      // Create circuit constraint on a machine.
      // Node 0 and num_intervals + 1 are source and sink.
      CircuitConstraint circuit = cp_model.AddCircuitConstraint();
      for (int i = 0; i < num_intervals; ++i) {
        const int job_i = machine_to_jobs[m][i];
        // Source to nodes.
        circuit.AddArc(0, i + 1, cp_model.NewBoolVar());
        // Node to sink.
        circuit.AddArc(i + 1, 0, cp_model.NewBoolVar());
        // Node to node.
        for (int j = 0; j < num_intervals; ++j) {
          if (i == j) {
            circuit.AddArc(i + 1, i + 1, Not(machine_to_presences[m][i]));
          } else {
            const int job_j = machine_to_jobs[m][j];
            const int64 transition =
                transitions.transition_time(job_i * num_jobs + job_j);
            const BoolVar lit = cp_model.NewBoolVar();
            const IntVar start = machine_to_starts[m][j];
            const IntVar end = machine_to_ends[m][i];
            circuit.AddArc(i + 1, j + 1, lit);
            // Push the new start with an extra transition.
            cp_model
                .AddLessOrEqual(LinearExpr(end).AddConstant(transition), start)
                .OnlyEnforceIf(lit);
          }
        }
      }
    }
  }

  // Add job precedences.
  for (const JobPrecedence& precedence : problem.precedences()) {
    const IntVar start = job_starts[precedence.second_job_index()];
    const IntVar end = job_ends[precedence.first_job_index()];
    cp_model.AddLessOrEqual(LinearExpr(end).AddConstant(precedence.min_delay()),
                            start);
  }

  // Add objective.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    objective_coeffs.push_back(problem.makespan_cost_per_time_unit());
    objective_vars.push_back(makespan);
  }
  cp_model.Minimize(LinearExpr::ScalProd(objective_vars, objective_coeffs)
                        .AddConstant(objective_offset));
  if (problem.has_scaling_factor()) {
    cp_model.ScaleObjectiveBy(problem.scaling_factor().value());
  }

  // Decision strategy.
  cp_model.AddDecisionStrategy(task_starts,
                               DecisionStrategyProto::CHOOSE_LOWEST_MIN,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  LOG(INFO) << "#machines:" << num_machines;
  LOG(INFO) << "#jobs:" << num_jobs;
  LOG(INFO) << "horizon:" << horizon;

  if (FLAGS_display_sat_model) {
    LOG(INFO) << cp_model.Proto().DebugString();
  }

  LOG(INFO) << CpModelStats(cp_model.Proto());

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const CpSolverResponse response = SolveWithModel(cp_model.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);

  // Abort if we don't have any solution.
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return;

  // Check cost, recompute it from scratch.
  int64 final_cost = 0;
  if (problem.makespan_cost_per_time_unit() != 0) {
    int64 makespan = 0;
    for (IntVar v : job_ends) {
      makespan = std::max(makespan, SolutionIntegerValue(response, v));
    }
    final_cost += makespan * problem.makespan_cost_per_time_unit();
  }

  for (int i = 0; i < job_ends.size(); ++i) {
    const int64 early_due_date = problem.jobs(i).early_due_date();
    const int64 late_due_date = problem.jobs(i).late_due_date();
    const int64 early_penalty = problem.jobs(i).earliness_cost_per_time_unit();
    const int64 late_penalty = problem.jobs(i).lateness_cost_per_time_unit();
    const int64 end = SolutionIntegerValue(response, job_ends[i]);
    if (end < early_due_date && early_penalty != 0) {
      final_cost += (early_due_date - end) * early_penalty;
    }
    if (end > late_due_date && late_penalty != 0) {
      final_cost += (end - late_due_date) * late_penalty;
    }
  }

  // TODO(user): Support alternative cost in check.
  const double tolerance = 1e-6;
  CHECK_GE(response.objective_value(), final_cost - tolerance);
  CHECK_LE(response.objective_value(), final_cost + tolerance);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::data::jssp::JsspParser parser;
  CHECK(parser.ParseFile(FLAGS_input));
  operations_research::sat::Solve(parser.problem());
  return EXIT_SUCCESS;
}
