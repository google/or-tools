// Copyright 2010-2014 Google
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
// earliness-tardiness costs.
//
// A earliness-tardiness jobshop is a standard scheduling problem where
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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/util/string_array.h"
#include "examples/cpp/jobshop_earlytardy.h"
#include "examples/cpp/jobshop_ls.h"

DEFINE_string(
    jet_file,
    "",
    "Required: input file description the scheduling problem to solve, "
    "in our jet format:\n"
    "  - the first line is \"<number of jobs> <number of machines>\"\n"
    "  - then one line per job, with a single space-separated "
    "list of \"<machine index> <duration>\", ended by due_date, early_cost,"
    "late_cost\n"
    "note: jobs with one task are not supported");
DEFINE_int32(machine_count, 10, "Machine count");
DEFINE_int32(job_count, 10, "Job count");
DEFINE_int32(max_release_date, 0, "Max release date");
DEFINE_int32(max_early_cost, 0, "Max earliness weight");
DEFINE_int32(max_tardy_cost, 3, "Max tardiness weight");
DEFINE_int32(max_duration, 10, "Max duration of a task");
DEFINE_int32(scale_factor, 130, "Scale factor (in percent)");
DEFINE_int32(seed, 1, "Random seed");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, 0 means no limit.");
DEFINE_bool(time_placement, false, "Use MIP based time placement");
DEFINE_int32(shuffle_length, 4, "Length of sub-sequences to shuffle LS.");
DEFINE_int32(sub_sequence_length, 4,
             "Length of sub-sequences to relax in LNS.");
DEFINE_int32(lns_seed, 1, "Seed of the LNS random search");
DEFINE_int32(lns_limit, 30,
             "Limit the size of the search tree in a LNS fragment");
DEFINE_bool(use_ls, false, "Use ls");
DECLARE_bool(log_prefix);

namespace operations_research {
class TimePlacement : public DecisionBuilder {
 public:
  TimePlacement(const EtJobShopData& data,
                const std::vector<SequenceVar*>& all_sequences,
                const std::vector<std::vector<IntervalVar*> >& jobs_to_tasks)
      : data_(data),
        all_sequences_(all_sequences),
        jobs_to_tasks_(jobs_to_tasks),
        mp_solver_("TimePlacement", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING) {}

  virtual ~TimePlacement() {}

  virtual Decision* Next(Solver* const solver) {
    mp_solver_.Clear();
    std::vector<std::vector<MPVariable*> > all_vars;
    std::unordered_map<IntervalVar*, MPVariable*> mapping;
    const double infinity = mp_solver_.infinity();
    all_vars.resize(all_sequences_.size());

    // Creates the MP Variables.
    for (int s = 0; s < jobs_to_tasks_.size(); ++s) {
      for (int t = 0; t < jobs_to_tasks_[s].size(); ++t) {
        IntervalVar* const task = jobs_to_tasks_[s][t];
        const std::string name = StringPrintf("J%dT%d", s, t);
        MPVariable* const var =
            mp_solver_.MakeIntVar(task->StartMin(), task->StartMax(), name);
        mapping[task] = var;
      }
    }

      // Adds the jobs precedence constraints.
    for (int j = 0; j < jobs_to_tasks_.size(); ++j) {
      for (int t = 0; t < jobs_to_tasks_[j].size() - 1; ++t) {
        IntervalVar* const first_task = jobs_to_tasks_[j][t];
        const int duration = first_task->DurationMax();
        IntervalVar* const second_task = jobs_to_tasks_[j][t + 1];
        MPVariable* const first_var = mapping[first_task];
        MPVariable* const second_var = mapping[second_task];
        MPConstraint* const ct =
            mp_solver_.MakeRowConstraint(duration, infinity);
        ct->SetCoefficient(second_var, 1.0);
        ct->SetCoefficient(first_var, -1.0);
      }
    }

    // Adds the ranked machines constraints.
    for (int s = 0; s < all_sequences_.size(); ++s) {
      SequenceVar* const sequence = all_sequences_[s];
      std::vector<int> rank_firsts;
      std::vector<int> rank_lasts;
      std::vector<int> unperformed;
      sequence->FillSequence(&rank_firsts, &rank_lasts, &unperformed);
      CHECK_EQ(0, rank_lasts.size());
      CHECK_EQ(0, unperformed.size());
      for (int i = 0; i < rank_firsts.size() - 1; ++i) {
        IntervalVar* const first_task = sequence->Interval(rank_firsts[i]);
        const int duration = first_task->DurationMax();
        IntervalVar* const second_task = sequence->Interval(rank_firsts[i + 1]);
        MPVariable* const first_var = mapping[first_task];
        MPVariable* const second_var = mapping[second_task];
        MPConstraint* const ct =
            mp_solver_.MakeRowConstraint(duration, infinity);
        ct->SetCoefficient(second_var, 1.0);
        ct->SetCoefficient(first_var, -1.0);
      }
    }

    // Creates penalty terms and objective.
    std::vector<MPVariable*> terms;
    mp_solver_.MakeIntVarArray(jobs_to_tasks_.size(),
                               0,
                               infinity,
                               "terms",
                               &terms);
    for (int j = 0; j < jobs_to_tasks_.size(); ++j) {
      mp_solver_.MutableObjective()->SetCoefficient(terms[j], 1.0);
    }
    mp_solver_.MutableObjective()->SetMinimization();

    // Forces penalty terms to be above late and early costs.
    for (int j = 0; j < jobs_to_tasks_.size(); ++j) {
      IntervalVar* const last_task = jobs_to_tasks_[j].back();
      const int duration = last_task->DurationMin();
      MPVariable* const mp_start = mapping[last_task];
      const Job& job = data_.GetJob(j);
      const int ideal_start = job.due_date - duration;
      const int early_offset = job.early_cost * ideal_start;
      MPConstraint* const early_ct =
          mp_solver_.MakeRowConstraint(early_offset, infinity);
      early_ct->SetCoefficient(terms[j], 1);
      early_ct->SetCoefficient(mp_start, job.early_cost);

      const int tardy_offset = job.tardy_cost * ideal_start;
      MPConstraint* const tardy_ct =
          mp_solver_.MakeRowConstraint(-tardy_offset, infinity);
      tardy_ct->SetCoefficient(terms[j], 1);
      tardy_ct->SetCoefficient(mp_start, -job.tardy_cost);
    }

    // Solve.
    CHECK_EQ(MPSolver::OPTIMAL, mp_solver_.Solve());

    // Inject MIP solution into the CP part.
    VLOG(1) << "MP cost = " << mp_solver_.Objective().Value();
    for (int j = 0; j < jobs_to_tasks_.size(); ++j) {
      for (int t = 0; t < jobs_to_tasks_[j].size(); ++t) {
        IntervalVar* const first_task = jobs_to_tasks_[j][t];
        MPVariable* const first_var = mapping[first_task];
        const int date = first_var->solution_value();
        first_task->SetStartRange(date, date);
      }
    }
    return NULL;
  }

  virtual std::string DebugString() const {
    return "TimePlacement";
  }

 private:
  const EtJobShopData& data_;
  const std::vector<SequenceVar*>& all_sequences_;
  const std::vector<std::vector<IntervalVar*> >& jobs_to_tasks_;
  MPSolver mp_solver_;
};

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
                                       job.early_cost,
                                       job.due_date,
                                       job.due_date,
                                       job.tardy_cost)->Var();
    penalties.push_back(penalty);
  }

  // Adds disjunctive constraints on unary resources, and creates
  // sequence variables. A sequence variable is a dedicated variable
  // whose job is to sequence interval variables.
  std::vector<SequenceVar*> all_sequences;
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    const std::string name = StringPrintf("Machine_%d", machine_id);
    DisjunctiveConstraint* const ct =
        solver.MakeDisjunctiveConstraint(machines_to_tasks[machine_id], name);
    solver.AddConstraint(ct);
    all_sequences.push_back(ct->MakeSequenceVar());
  }

  // Objective: minimize the weighted penalties.
  IntVar* const objective_var = solver.MakeSum(penalties)->Var();
  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);

  // ----- Search monitors and decision builder -----

  // This decision builder will rank all tasks on all machines.
  DecisionBuilder* const sequence_phase =
      solver.MakePhase(all_sequences, Solver::CHOOSE_MIN_SLACK_RANK_FORWARD);

  // After the ranking of tasks, the schedule is still loose and any
  // task can be postponed at will. But, because the problem is now a PERT
  // (http://en.wikipedia.org/wiki/Program_Evaluation_and_Review_Technique),
  // we can schedule each task at its earliest start time. This is
  // conveniently done by fixing the objective variable to its
  // minimum value.
  DecisionBuilder* const obj_phase =
      FLAGS_time_placement ?
      solver.RevAlloc(new TimePlacement(data, all_sequences, jobs_to_tasks)) :
      solver.MakePhase(objective_var,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  if (FLAGS_use_ls) {
    Assignment* const first_solution = solver.MakeAssignment();
    first_solution->Add(all_sequences);
    first_solution->AddObjective(objective_var);
    // Store the first solution in the 'solution' object.
    DecisionBuilder* const store_db =
        solver.MakeStoreAssignment(first_solution);

    // The main decision builder (ranks all tasks, then fixes the
    // objective_variable).
    DecisionBuilder* const first_solution_phase =
        solver.Compose(sequence_phase, obj_phase, store_db);

    LOG(INFO) << "Looking for the first solution";
    const bool first_solution_found = solver.Solve(first_solution_phase);
    if (first_solution_found) {
      LOG(INFO) << "Solution found with penalty cost of = "
                << first_solution->ObjectiveValue();
    } else {
      LOG(INFO) << "No initial solution found!";
      return;
    }

    LOG(INFO) << "Switching to local search";
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
    LOG(INFO) << "  - use free sub sequences of length "
              << FLAGS_sub_sequence_length << " lns operator";
    LocalSearchOperator* const lns_operator =
        solver.RevAlloc(new SequenceLns(all_sequences,
                                        FLAGS_lns_seed,
                                        FLAGS_sub_sequence_length));
    operators.push_back(lns_operator);

    // Creates the local search decision builder.
    LocalSearchOperator* const concat =
        solver.ConcatenateOperators(operators, true);

    SearchLimit* const ls_limit =
        solver.MakeLimit(kint64max, FLAGS_lns_limit, kint64max, kint64max);
    DecisionBuilder* const random_sequence_phase =
        solver.MakePhase(all_sequences, Solver::CHOOSE_RANDOM_RANK_FORWARD);
    DecisionBuilder* const ls_db =
        solver.MakeSolveOnce(solver.Compose(random_sequence_phase, obj_phase),
                             ls_limit);

    LocalSearchPhaseParameters* const parameters =
        solver.MakeLocalSearchPhaseParameters(concat, ls_db);
    DecisionBuilder* const final_db =
        solver.MakeLocalSearchPhase(first_solution, parameters);

    OptimizeVar* const objective_monitor =
        solver.MakeMinimize(objective_var, 1);

    // Search log.
    const int kLogFrequency = 1000000;
    SearchMonitor* const search_log =
        solver.MakeSearchLog(kLogFrequency, objective_monitor);

    SearchLimit* const limit = FLAGS_time_limit_in_ms > 0 ?
        solver.MakeTimeLimit(FLAGS_time_limit_in_ms) :
        NULL;

    // Search.
    solver.Solve(final_db, search_log, objective_monitor, limit);
  } else {
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
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs a simple job shop optimization "
    "output besides the debug LOGs of the solver.";

int main(int argc, char **argv) {
  FLAGS_log_prefix = false;
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::EtJobShopData data;
  if (!FLAGS_jet_file.empty()) {
    data.LoadJetFile(FLAGS_jet_file);
  } else {
    data.GenerateRandomData(FLAGS_machine_count,
                            FLAGS_job_count,
                            FLAGS_max_release_date,
                            FLAGS_max_early_cost,
                            FLAGS_max_tardy_cost,
                            FLAGS_max_duration,
                            FLAGS_scale_factor,
                            FLAGS_seed);
  }
  operations_research::EtJobShop(data);
  return 0;
}
