// Copyright 2010-2012 Google
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
// This model implements a simple jobshop problem with
// earlyness-tardiness costs.
//
// A earlyness-tardinessjobshop is a standard scheduling problem where
// you must schedule a set of jobs on a set of machines.  Each job is
// a sequence of tasks (a task can only start when the preceding task
// finished), each of which occupies a single specific machine during
// a specific duration. Therefore, a job is a sequence of pairs
// (machine id, duration), along with a release data (minimum start
// date of the first task of the job, and due data (end time of the
// last job) with a tardiness linear penalty.

// The objective is to minimize the sum of early-tardy penalties for each job.
//
// This will be modelled by sets of intervals variables (see class
// IntervalVar in constraint_solver/constraint_solver.h), one per
// task, representing the [start_time, end_time] of the task.  Tasks
// in the same job will be linked by precedence constraints.  Tasks on
// the same machine will be covered by Sequence constraints.

#include <cstdio>
#include <cstdlib>

#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "cpp/jobshop_earlytardy.h"

DEFINE_string(
    data_file,
    "",
    "Required: input file description the scheduling problem to solve, "
    "in our jssp format:\n"
    "  - the first line is \"instance <instance name>\"\n"
    "  - the second line is \"<number of jobs> <number of machines>\"\n"
    "  - then one line per job, with a single space-separated "
    "list of \"<machine index> <duration>\"\n"
    "note: jobs with one task are not supported");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, 0 means no limit.");

namespace operations_research {
void EtJobShop(const EtJobShopData& data) {
  Solver solver("et_jobshop");
  const int machine_count = data.machine_count();
  const int job_count = data.job_count();
  const int horizon = data.horizon();

  // ----- Creates all Intervals and vars -----

  // Stores all tasks attached interval variables per job.
  std::vector<std::vector<IntervalVar*> > jobs_to_tasks(job_count);
  // machines_to_tasks stores the same interval variables as above, but
  // grouped my machines instead of grouped by jobs.
  std::vector<std::vector<IntervalVar*> > machines_to_tasks(machine_count);

  // Creates all individual interval variables.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const Job& job = data.GetJob(job_id);
    const std::vector<Task>& tasks = job.all_tasks;
    for (int task_index = 0; task_index < tasks.size(); ++task_index) {
      const Task& task = tasks[task_index];
      CHECK_EQ(job_id, task.job_id);
      const string name = StringPrintf("J%dM%dI%dD%d",
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

  // ----- Creates model -----

  // Creates precedences inside jobs.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const int task_count = jobs_to_tasks[job_id].size();
    for (int task_index = 0; task_index < task_count - 1; ++task_index) {
      IntervalVar* const t1 = jobs_to_tasks[job_id][task_index];
      IntervalVar* const t2 = jobs_to_tasks[job_id][task_index + 1];
      Constraint* const prec =
          solver.MakeIntervalVarRelation(t2, Solver::STARTS_AFTER_END, t1);
      solver.AddConstraint(prec);
    }
  }

  // Add release date.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const Job& job = data.GetJob(job_id);
    IntervalVar* const t = jobs_to_tasks[job_id][0];
    Constraint* const prec =
        solver.MakeIntervalVarRelation(t,
                                       Solver::STARTS_AFTER,
                                       job.release_date);
    solver.AddConstraint(prec);
  }

  std::vector<IntVar*> penalties;
  for (int job_id = 0; job_id < job_count; ++job_id) {
    const Job& job = data.GetJob(job_id);
    IntervalVar* const t = jobs_to_tasks[job_id][machine_count - 1];
    IntVar* const penalty =
        solver.MakeConvexPiecewiseExpr(t->EndExpr(),
                                       job.earlyness_weight,
                                       job.due_date,
                                       job.due_date,
                                       job.tardiness_weight)->Var();
    penalties.push_back(penalty);
  }

  // Adds disjunctive constraints on unary resources.
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    solver.AddConstraint(
        solver.MakeDisjunctiveConstraint(machines_to_tasks[machine_id]));
  }

  // Creates sequences variables on machines. A sequence variable is a
  // dedicated variable whose job is to sequence interval variables.
  std::vector<SequenceVar*> all_sequences;
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    const string name = StringPrintf("Machine_%d", machine_id);
    SequenceVar* const sequence =
        solver.MakeSequenceVar(machines_to_tasks[machine_id], name);
    all_sequences.push_back(sequence);
  }

  // Objective: minimize the weighted penalties.
  IntVar* const objective_var = solver.MakeSum(penalties)->Var();
  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);

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
  DecisionBuilder* const obj_phase =
      solver.MakePhase(objective_var,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  // The main decision builder (ranks all tasks, then fixes the
  // objective_variable).
  DecisionBuilder* const main_phase =
      solver.Compose(sequence_phase, obj_phase);

  // Search log.
  const int kLogFrequency = 1000000;
  SearchMonitor* const search_log =
      solver.MakeSearchLog(kLogFrequency, objective_monitor);

  SearchLimit* limit = NULL;
  if (FLAGS_time_limit_in_ms > 0) {
    limit = solver.MakeTimeLimit(FLAGS_time_limit_in_ms);
  }

  // Search.
  solver.Solve(main_phase, search_log, objective_monitor, limit);
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs a simple job shop optimization "
    "output besides the debug LOGs of the solver.";

int main(int argc, char **argv) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(INFO) << "Please supply a data file with --data_file=";
    return 1;
  }
  operations_research::EtJobShopData data;
  data.Load(FLAGS_data_file);
  operations_research::EtJobShop(data);
  return 0;
}
