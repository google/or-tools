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
//  A simple program to solve the Job-Shop Problem with Local Search/Large Neighborhood Search.
//  See jobshop_ls.h for the Local Search operators.
//
//  We use the disjunctive model and specialized IntervalVars and SequenceVars.
//
//  Use of two Local Search operators:
//  - swap_operator: Exchanging two IntervalVars on a SequenceVar.
//  - suffle_operator: Exchanging an arbitratry number of contiguous
//    IntervalVars on a SequenceVar.
//
//  We also use Local Search to find an initial solution.


#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"
#include "jobshop.h"
#include "jobshop_ls.h"
#include "jobshop_lns.h"
#include "limits.h"


DEFINE_string(
  data_file,
  "",
  "Input file with a description of the job-shop problem instance to solve "
  "in JSSP or Taillard's format.\n");

DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, 0 means no limit.");
DEFINE_int32(shuffle_length, 4, "Length of sub-sequences to shuffle LS.");
DEFINE_int64(initial_time_limit_in_ms, 20000,
             "Time limit in ms to find the initial solution by LS.");
DEFINE_int32(solutions_nbr_tolerance, 1,
  "initial_time_limit_in_ms is applied except if the number of solutions"
  "produced since last check is greater of equal to solutions_nbr_tolerance.");
DEFINE_int32(sub_sequence_length, 4,
             "Length of sub-sequences to relax in LNS.");
DEFINE_int32(lns_seed, 1, "Seed of the LNS random search");
DEFINE_int32(lns_limit, 30,
             "Limit the size of the search tree in a LNS fragment");

namespace operations_research {

void Jobshop(const JobShopData& data) {
  //  *************************************************
  //  MODEL
  //  *************************************************
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

  //  *************************************************
  //  First solution
  //  *************************************************
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
  DecisionBuilder* const first_solution_store_db =
              solver.MakeStoreAssignment(first_solution);

  // The main decision builder (ranks all tasks, then fixes the
  // objective_variable).
  DecisionBuilder* const first_solution_phase =
      solver.Compose(sequence_phase, obj_phase, first_solution_store_db);

  LOG(INFO) << "Looking for the first solution to initialize "
               "the LS to find the initial solution...";
  const bool first_solution_found = solver.Solve(first_solution_phase);
  if (first_solution_found) {
    LOG(INFO) << "First solution found with makespan = "
              << first_solution->ObjectiveValue();
  } else {
    LOG(INFO) << "No first solution found!";
    return;
  }


  //  *************************************************
  //  Initial solution
  //  *************************************************
  LOG(INFO) << "Switching to local search to find a good initial solution...";

  //  Swap Operator with shuffle length 2.
  LocalSearchOperator* const initial_shuffle_operator =
      solver.RevAlloc(new ShuffleIntervals(all_sequences,
                                           2));
  //  Complementary DecisionBuilder.
  DecisionBuilder* const random_sequence_phase =
      solver.MakePhase(all_sequences, Solver::CHOOSE_RANDOM_RANK_FORWARD);
  DecisionBuilder* const complementary_ls_db =
      solver.Compose(random_sequence_phase, obj_phase);

  //  LS Parameters.
  LocalSearchPhaseParameters* const initial_ls_param =
      solver.MakeLocalSearchPhaseParameters(initial_shuffle_operator,
                                            complementary_ls_db);

  //  LS DecisionBuilder.
  DecisionBuilder* const initial_ls_db =
      solver.MakeLocalSearchPhase(first_solution, initial_ls_param);

  //  Custom SearchLimit
  SearchLimit * initial_search_limit =
          solver.MakeCustomLimit(
            new LSInitialSolLimit(&solver,
                                  FLAGS_initial_time_limit_in_ms,
                                  FLAGS_solutions_nbr_tolerance));

  Assignment* const initial_solution = solver.MakeAssignment();
  initial_solution->Add(all_sequences);
  initial_solution->AddObjective(objective_var);
  // Store the initial solution in the 'solution' object.
  DecisionBuilder* const initial_solution_store_db =
               solver.MakeStoreAssignment(initial_solution);

  DecisionBuilder* const initial_solution_phase =
      solver.Compose(initial_ls_db, initial_solution_store_db);

  LOG(INFO) << "Looking for the initial solution...";
  const bool initial_solution_found =
     solver.Solve(initial_solution_phase,
                  objective_monitor,
                  initial_search_limit);
  if (initial_solution_found) {
    LOG(INFO) << "Initial solution found with makespan = "
              << initial_solution->ObjectiveValue();
  } else {
    LOG(INFO) << "No initial solution found!";
    return;
  }

  //  *************************************************
  //  Real Local Search
  //  *************************************************
  LOG(INFO) << "Switching to local search to find a good solution...";
  std::vector<LocalSearchOperator*> operators;
  LOG(INFO) << "  - use swap operator";
  LocalSearchOperator* const swap_operator =
      solver.RevAlloc(new SwapIntervals(all_sequences));
  operators.push_back(swap_operator);
  LOG(INFO) << "  - use shuffle operator with a max length of "
            << FLAGS_shuffle_length;
  LocalSearchOperator* const shuffle_operator =
      solver.RevAlloc(new ShuffleIntervals(all_sequences,
                                           FLAGS_shuffle_length));
  operators.push_back(shuffle_operator);

  LOG(INFO) << "  - use sequence_lns operator with seed = "
            << FLAGS_lns_seed << " and sub sequence length of " << FLAGS_sub_sequence_length;
  //  SequenceLns Operator.
  LocalSearchOperator* const sequence_lns =
      solver.RevAlloc(new SequenceLns(all_sequences,
                                      FLAGS_lns_seed,
                                      FLAGS_sub_sequence_length));

  operators.push_back(sequence_lns);

  // Creates the local search decision builder.
  LocalSearchOperator* const ls_concat =
      solver.ConcatenateOperators(operators, true);

        SearchLimit* const lns_limit =
      solver.MakeLimit(kint64max, FLAGS_lns_limit, kint64max, kint64max);

      DecisionBuilder* const ls_db =
      solver.MakeSolveOnce(solver.Compose(random_sequence_phase, obj_phase), lns_limit);

  LocalSearchPhaseParameters* const parameters =
      solver.MakeLocalSearchPhaseParameters(ls_concat, ls_db);
  DecisionBuilder* const final_db =
      solver.MakeLocalSearchPhase(initial_solution, parameters);

  SearchLimit* const limit = FLAGS_time_limit_in_ms > 0 ?
      solver.MakeTimeLimit(FLAGS_time_limit_in_ms) :
      NULL;

  // Search log.
  const int kLogFrequency = 1000000;
  SearchMonitor* const search_log =
      solver.MakeSearchLog(kLogFrequency, objective_monitor);

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

  std::vector<SearchMonitor *> search_monitors;
  search_monitors.push_back(search_log);
  search_monitors.push_back(objective_monitor);
  search_monitors.push_back(limit);
  search_monitors.push_back(collector);

#if defined(__GNUC__)  // Linux
  SearchLimit * ctrl_catch_limit = MakeCatchCTRLBreakLimit(&solver);
  search_monitors.push_back(ctrl_catch_limit);
#endif

  // Search.
  if (solver.Solve(final_db,
    search_monitors)) {
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
  return;
}

}  //  namespace operations_research

static const char kUsage[] =
"Usage: jobshop --data_file=instance.txt.\n\n"
"This program solves the job-shop problem in JSSP or "
"Taillard's format with two basic local search operators and Large Neighborhood Search.\n";

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
