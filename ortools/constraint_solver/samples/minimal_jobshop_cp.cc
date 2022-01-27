// Copyright 2018 Google LLC
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

#include <iomanip>
#include <numeric>  // std::iota

#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"

// Solve a job shop problem:

namespace operations_research {
void SolveJobShopExample() {
  // Instantiate the solver.
  Solver solver("JobShopExample");
  std::array<int, 3> machines;
  std::iota(std::begin(machines), std::end(machines), 0);
  {
    std::ostringstream oss;
    for (auto i : machines) oss << ' ' << i;
    LOG(INFO) << "Machines: " << oss.str();
  }

  // Jobs definition
  using MachineIndex = int;
  using ProcessingTime = int;
  using Task = std::pair<MachineIndex, ProcessingTime>;
  using Job = std::vector<Task>;
  std::vector<Job> jobs = {
      {{0, 3}, {1, 2}, {2, 2}}, {{0, 2}, {2, 1}, {1, 4}}, {{1, 4}, {2, 3}}};
  LOG(INFO) << "Jobs:";
  for (int i = 0; i < jobs.size(); ++i) {
    std::ostringstream problem;
    problem << "Job " << i << ": [";
    for (const Task& task : jobs[i]) {
      problem << "(" << task.first << ", " << task.second << ")";
    }
    problem << "]" << std::endl;
    LOG(INFO) << problem.str();
  }

  // Computes horizon.
  ProcessingTime horizon = 0;
  for (const Job& job : jobs) {
    for (const Task& task : job) {
      horizon += task.second;
    }
  }
  LOG(INFO) << "Horizon: " << horizon;

  // Creates tasks.
  std::vector<std::vector<IntervalVar*>> tasks_matrix(jobs.size());
  for (int i = 0; i < jobs.size(); ++i) {
    for (int j = 0; j < jobs[i].size(); ++j) {
      std::ostringstream oss;
      oss << "Job_" << i << "_" << j;
      tasks_matrix[i].push_back(solver.MakeFixedDurationIntervalVar(
          0, horizon, jobs[i][j].second, false, oss.str()));
    }
  }

  // Add conjunctive constraints.
  for (int i = 0; i < jobs.size(); ++i) {
    for (int j = 0; j < jobs[i].size() - 1; ++j) {
      solver.AddConstraint(solver.MakeIntervalVarRelation(
          tasks_matrix[i][j + 1], Solver::STARTS_AFTER_END,
          tasks_matrix[i][j]));
    }
  }

  // Creates sequence variables and add disjunctive constraints.
  std::vector<SequenceVar*> all_sequences;
  std::vector<IntVar*> all_machines_jobs;
  for (const auto machine : machines) {
    std::vector<IntervalVar*> machines_jobs;
    for (int i = 0; i < jobs.size(); ++i) {
      for (int j = 0; j < jobs[i].size(); ++j) {
        if (jobs[i][j].first == machine)
          machines_jobs.push_back(tasks_matrix[i][j]);
      }
    }
    DisjunctiveConstraint* const disj = solver.MakeDisjunctiveConstraint(
        machines_jobs, "Machine_" + std::to_string(machine));
    solver.AddConstraint(disj);
    all_sequences.push_back(disj->MakeSequenceVar());
  }

  // Set the objective.
  std::vector<IntVar*> all_ends;
  for (const auto& job : tasks_matrix) {
    IntervalVar* const task = job.back();
    all_ends.push_back(task->EndExpr()->Var());
  }
  IntVar* const obj_var = solver.MakeMax(all_ends)->Var();
  OptimizeVar* const objective_monitor = solver.MakeMinimize(obj_var, 1);

  // ----- Search monitors and decision builder -----

  // This decision builder will rank all tasks on all machines.
  DecisionBuilder* const sequence_phase =
      solver.MakePhase(all_sequences, Solver::SEQUENCE_DEFAULT);

  // After the ranking of tasks, the schedule is still loose and any
  // task can be postponed at will. But, because the problem is now a PERT
  // (http://en.wikipedia.org/wiki/Program_Evaluation_and_Review_Technique),
  // we can schedule each task at its earliest start time. This is
  // conveniently done by fixing the objective variable to its
  // minimum value.
  DecisionBuilder* const obj_phase = solver.MakePhase(
      obj_var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

  // The main decision builder (ranks all tasks, then fixes the
  // objective_variable).
  DecisionBuilder* const main_phase = solver.Compose(sequence_phase, obj_phase);

  // Search log.
  const int kLogFrequency = 1000000;
  SearchMonitor* const search_log =
      solver.MakeSearchLog(kLogFrequency, objective_monitor);

  SearchLimit* limit = nullptr;

  // Create the solution collector.
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  collector->Add(all_sequences);
  collector->AddObjective(obj_var);
  for (const auto machine : machines) {
    SequenceVar* const sequence = all_sequences[machine];
    for (int i = 0; i < sequence->size(); ++i) {
      IntervalVar* const t = sequence->Interval(i);
      collector->Add(t->StartExpr()->Var());
      collector->Add(t->EndExpr()->Var());
    }
  }

  // Solve the problem.
  if (solver.Solve(main_phase, search_log, objective_monitor, limit,
                   collector)) {
    LOG(INFO) << "Optimal Schedule Length: " << collector->objective_value(0);
    LOG(INFO) << "";

    LOG(INFO) << "Optimal Schedule:";
    std::vector<std::string> machine_intervals_list;
    for (const auto machine : machines) {
      std::ostringstream machine_tasks;
      SequenceVar* seq = all_sequences[machine];
      machine_tasks << "Machine " << machine << ": ";
      for (const auto s : collector->ForwardSequence(0, seq)) {
        machine_tasks << seq->Interval(s)->name() << " ";
      }
      LOG(INFO) << machine_tasks.str();

      std::ostringstream machine_intervals;
      machine_intervals << "Machine " << machine << ": ";
      for (const auto s : collector->ForwardSequence(0, seq)) {
        IntervalVar* t = seq->Interval(s);
        machine_intervals << "[(" << std::setw(2)
                          << collector->solution(0)->Min(t->StartExpr()->Var())
                          << ", "
                          << collector->solution(0)->Max(t->StartExpr()->Var())
                          << "),(" << std::setw(2)
                          << collector->solution(0)->Min(t->EndExpr()->Var())
                          << ", "
                          << collector->solution(0)->Max(t->EndExpr()->Var())
                          << ")]";
      }
      machine_intervals_list.push_back(machine_intervals.str());
    }
    LOG(INFO) << "Time Intervals for Tasks: ";
    for (const auto& intervals : machine_intervals_list) {
      LOG(INFO) << intervals;
    }
    LOG(INFO) << "Advanced usage:";
    LOG(INFO) << "Time: " << solver.wall_time() << "ms";
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::SolveJobShopExample();
  return EXIT_SUCCESS;
}
