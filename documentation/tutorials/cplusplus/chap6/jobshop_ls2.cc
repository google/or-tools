// Copyright 2011-2014 Google
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
//
//
//  A simple program to solve the job-shop problem with local search.
//  See jobshop_ls.h for the local operators.
//
//  We use the disjunctive model and specialized IntervalVars and SequenceVars.
//
//  Exchanging an arbitratry number of contiguous IntervalVars on a SequenceVar.


#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "jobshop.h"
#include "jobshop_ls.h"
#include "util/string_array.h"


DEFINE_string(
  data_file,
  "",
  "Input file with a description of the job-shop problem instance to solve "
  "in JSSP or Taillard's format.\n");

DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, 0 means no limit.");
DEFINE_int32(shuffle_length, 4, "Length of sub-sequences to shuffle LS.");

namespace operations_research {

void Jobshop(const JobShopData& data) {
  Solver solver("jobshop");
  const int machine_count = data.machine_count();
  const int job_count = data.job_count();
  const int horizon = data.horizon();

  // Stores all tasks per job.
  std::vector<std::vector<IntervalVar*> > jobs_to_tasks(job_count);
  // Stores all tasks per machine.
  std::vector<std::vector<IntervalVar*> > machines_to_tasks(machine_count);

  // Creates all interval variables.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const std::vector<JobShopData::Task>& tasks = data.TasksOfJob(job_id);
    for (int task_index = 0; task_index < tasks.size(); ++task_index) {
      const JobShopData::Task& task = tasks[task_index];
      CHECK_EQ(job_id, task.job_id);
      const std::string name = StringPrintf("J%dM%dI%dD%d",
                                       task.job_id,
                                       task.machine_id,
                                       task_index,
                                       task.duration);
      IntervalVar* const one_task =
      solver.MakeFixedDurationIntervalVar(0,
                                          horizon,
                                          task.duration,
                                          false,
                                          name);
      jobs_to_tasks[task.job_id].push_back(one_task);
      machines_to_tasks[task.machine_id].push_back(one_task);
    }
  }

  // Add conjunctive constraintss.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const int task_count = jobs_to_tasks[job_id].size();
    if (task_count == 1) {continue;}
    for (int task_index = 0; task_index < task_count - 1; ++task_index) {
      IntervalVar* const t1 = jobs_to_tasks[job_id][task_index];
      IntervalVar* const t2 = jobs_to_tasks[job_id][task_index + 1];
      Constraint* const prec =
      solver.MakeIntervalVarRelation(t2, Solver::STARTS_AFTER_END, t1);
      solver.AddConstraint(prec);
    }
  }

  // Adds disjunctive constraints and creates sequence variables.
  std::vector<SequenceVar*> all_sequences;
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    const std::string name = StringPrintf("Machine_%d", machine_id);
    DisjunctiveConstraint* const ct =
    solver.MakeDisjunctiveConstraint(machines_to_tasks[machine_id], name);
    solver.AddConstraint(ct);
    all_sequences.push_back(ct->MakeSequenceVar());
  }

  // Creates array of end_times of jobs.
  std::vector<IntVar*> all_ends;
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const int task_count = jobs_to_tasks[job_id].size();
    IntervalVar* const task = jobs_to_tasks[job_id][task_count - 1];
    all_ends.push_back(task->EndExpr()->Var());
  }

  // Objective: minimize the makespan (maximum end times of all tasks).
  IntVar* const objective_var = solver.MakeMax(all_ends)->Var();
  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);

  // This decision builder will rank all tasks on all machines.
  DecisionBuilder* const sequence_phase =
  solver.MakePhase(all_sequences, Solver::SEQUENCE_DEFAULT);

  // After the ranking of tasks, the schedule is still loose.
  // We schedule each task at its earliest start time.
  DecisionBuilder* const obj_phase =
  solver.MakePhase(objective_var,
                   Solver::CHOOSE_FIRST_UNBOUND,
                   Solver::ASSIGN_MIN_VALUE);

  Assignment* const first_solution = solver.MakeAssignment();
  first_solution->Add(all_sequences);
  first_solution->AddObjective(objective_var);
  // Store the first solution in the 'solution' object.
  DecisionBuilder* const store_db = solver.MakeStoreAssignment(first_solution);

  // The main decision builder (ranks all tasks, then fixes the
  // objective_variable).
  DecisionBuilder* const first_solution_phase =
      solver.Compose(sequence_phase, obj_phase, store_db);

  LOG(INFO) << "Looking for the first solution";
  const bool first_solution_found = solver.Solve(first_solution_phase);
  if (first_solution_found) {
    LOG(INFO) << "Solution found with makespan = "
              << first_solution->ObjectiveValue();
  } else {
    LOG(INFO) << "No initial solution found!";
    return;
  }

  LOG(INFO) << "Switching to local search";

  //  Swap Operator.
  LocalSearchOperator* const shuffle_operator =
      solver.RevAlloc(new ShuffleIntervals(all_sequences,
                                           FLAGS_shuffle_length));
  //  Complementary DecisionBuilder.
  DecisionBuilder* const random_sequence_phase =
      solver.MakePhase(all_sequences, Solver::CHOOSE_RANDOM_RANK_FORWARD);
  DecisionBuilder* const complementary_ls_db =
       solver.Compose(random_sequence_phase, obj_phase);

  //  LS Parameters.
  LocalSearchPhaseParameters* const ls_param =
      solver.MakeLocalSearchPhaseParameters(shuffle_operator,
                                            complementary_ls_db);

  //  LS DecisionBuilder.
  DecisionBuilder* const ls_db =
      solver.MakeLocalSearchPhase(first_solution, ls_param);

  // Search log.
  const int kLogFrequency = 1000000;
  SearchMonitor* const search_log =
  solver.MakeSearchLog(kLogFrequency, objective_monitor);

  SearchLimit* limit = NULL;
  if (FLAGS_time_limit_in_ms > 0) {
    limit = solver.MakeTimeLimit(FLAGS_time_limit_in_ms);
  }

  SolutionCollector* const collector =
  solver.MakeLastSolutionCollector();
  collector->Add(all_sequences);
  collector->AddObjective(objective_var);
  // IntervalVar
  for (int seq = 0; seq < all_sequences.size(); ++seq) {
    const SequenceVar * sequence = all_sequences[seq];
    const int sequence_count = sequence->size();
    for (int i = 0; i < sequence_count; ++i) {
      IntervalVar * t = sequence->Interval(i);
      collector->Add(t->StartExpr()->Var());
      collector->Add(t->EndExpr()->Var());
    }
  }

  // Search.
  if (solver.Solve(ls_db,
    search_log,
    objective_monitor,
    limit,
    collector)) {
      LOG(INFO) << "Objective value: " << collector->objective_value(0);
      for (int m = 0; m < machine_count; ++m) {
        SequenceVar* const seq = all_sequences[m];
        std::ostringstream s;
        s << seq->name() << ": ";
        const std::vector<int> & sequence = collector->ForwardSequence(0, seq);
        const int seq_size = sequence.size();
        for (int i = 0; i < seq_size; ++i) {
          IntervalVar * t = seq->Interval(sequence[i]);
          s << "Job " << sequence[i] << " (";
          s << collector->Value(0, t->StartExpr()->Var());
          s << ",";
          s << collector->Value(0, t->EndExpr()->Var());
          s << ")  ";
        }
        s.flush();
        LOG(INFO) << s.str();
      }
    } else {
      LOG(INFO) << "No solution found...";
    }
}

}  // namespace operations_research

static const char kUsage[] =
"Usage: jobshop --data_file=instance.txt.\n\n"
"This program solves the job-shop problem in JSSP "
"or Taillard's format with a basic swap operator and Local Search.\n";

int main(int argc, char **argv) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(FATAL) << "Please supply a data file with --data_file=";
  }
  operations_research::JobShopData data(FLAGS_data_file);
  operations_research::Jobshop(data);

  return 0;
}
