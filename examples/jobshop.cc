// Copyright 2010-2011 Google
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
//This model implements a simple jobshop problem.
//
// A jobshop is a standard scheduling problem when you must sequence a
// series of tasks on a set of machines. Each job contains one task per
// machine. The order of execution and the length of each job on each
// machine is task dependent.
//
// The objective is to minimize the maximum completion time of all
// jobs. This is called the makespan.


#include <cstdio>
#include <cstdlib>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strtoint.h"
#include "base/file.h"
#include "base/split.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_string(data_file,
              "",
              "Filename of the data instance in the jssp formap.");

namespace operations_research {

// ----- JobShopData -----

// A Jobshop data parses data files and stores all data internally for
// easy retrieval.
class JobShopData {
 public:
  // A task is the basic block of a jobshop.
  struct Task {
    Task(int j, int m, int d) : job_id(j), machine_id(m), duration(d) {}
    int job_id;
    int machine_id;
    int duration;
  };

  JobShopData()
      : name_(""), machine_count_(0), job_count_(0), horizon_(0), job_index_(0) {}

  ~JobShopData() {}

  // Parses a file a loads data.
  void Load(const string& filename) {
    const uint64 kMaxLineLength = 1024;
    File* const data_file = File::Open(filename, "r");
    string line;
    while (data_file->ReadToString(&line, kMaxLineLength)) {
      ProcessNewLine(line);
    }
    data_file->Close();
  }

  // The number of machines in the jobshop.
  int machines() const { return machine_count_; }

  // The number of jobs in the jobshop.
  int jobs() const { return job_count_; }

  // The name of the jobshop instance.
  const string& name() const { return name_; }

  // The horizon of the workshop (the sum of all durations).
  int horizon() const { return horizon_; }

  // Query an individual task in a job (i.e. a sequence of tasks).
  const Task& TaskByJob(int job_id, int index) const {
    return all_tasks_[job_id][index];
  }

 private:
  void ProcessNewLine(const string& line) {
    const char* const kWordDelimiters(" ");
    std::vector<string> words;
    SplitStringUsing(line, kWordDelimiters, &words);
    if (words.size() == 2) {
      if (words[0].compare("instance") == 0) {
        LOG(INFO) << "Name = " << words[1];
        name_ = words[1];
      } else {
        job_count_ = atoi32(words[0]);
        machine_count_ = atoi32(words[1]);
        CHECK_GT(machine_count_, 0);
        CHECK_GT(job_count_, 0);
        LOG(INFO) << machine_count_ << " machines and " << job_count_ << " jobs";
        all_tasks_.resize(job_count_);
      }
    }
    if (words.size() > 2 && machine_count_ != 0) {
      CHECK_EQ(words.size(), machine_count_ * 2);
      for (int i = 0; i < machine_count_; ++i) {
        const int machine_id = atoi32(words[2 * i]);
        const int duration = atoi32(words[2 * i + 1]);
        AddTask(job_index_, machine_id, duration);
      }
      job_index_++;
    }
  }

  void AddTask(int job_id, int machine_id, int duration) {
    all_tasks_[job_id].push_back(Task(job_id, machine_id, duration));
    horizon_ += duration;
  }

  string name_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<std::vector<Task> > all_tasks_;
  int job_index_;
};

void Jobshop(const JobShopData& data) {
  Solver solver("jobshop");
  const int machine_count = data.machines();
  const int job_count = data.jobs();
  const int horizon = data.horizon();

  // Creates all Intervals and vars
  std::vector<std::vector<IntervalVar*> > tasks_per_jobs(job_count);
  std::vector<std::vector<IntervalVar*> > tasks_per_machine(machine_count);

  for (int job_id = 0; job_id < job_count; ++job_id) {
    for (int task_index = 0; task_index < machine_count; ++task_index) {
      const JobShopData::Task& task = data.TaskByJob(job_id, task_index);
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
      tasks_per_jobs[task.job_id].push_back(one_task);
      tasks_per_machine[task.machine_id].push_back(one_task);
    }
  }

  // Creates precedences inside jobs.
  for (int job_id = 0; job_id < job_count; ++job_id) {
    for (int task_index = 0; task_index < machine_count - 1; ++task_index) {
      IntervalVar* const t1 = tasks_per_jobs[job_id][task_index];
      IntervalVar* const t2 = tasks_per_jobs[job_id][task_index + 1];
      Constraint* const prec =
          solver.MakeIntervalVarRelation(t2, Solver::STARTS_AFTER_END, t1);
      solver.AddConstraint(prec);
    }
  }

  // Creates MakeSpan.
  std::vector<IntVar*> all_ends;
  for (int job_id = 0; job_id < job_count; ++job_id) {
    IntervalVar* const task = tasks_per_jobs[job_id][machine_count - 1];
    all_ends.push_back(task->EndExpr()->Var());
  }

  // Creates sequences constraints on machines.
  std::vector<Sequence*> all_sequences;
  for (int machine_id = 0; machine_id < machine_count; ++machine_id) {
    const string name = StringPrintf("Machine_%d", machine_id);
    Sequence* const sequence =
        solver.MakeSequence(tasks_per_machine[machine_id], name);
    all_sequences.push_back(sequence);
    solver.AddConstraint(sequence);
  }

  IntVar* const objective_var = solver.MakeMax(all_ends)->Var();
  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);

  DecisionBuilder* const obj_phase =
      solver.MakePhase(objective_var,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const sequence_phase =
      solver.MakePhase(all_sequences, Solver::SEQUENCE_DEFAULT);
  DecisionBuilder* const main_phase =
      solver.Compose(sequence_phase, obj_phase);

  SearchMonitor* const search_log =
      solver.MakeSearchLog(1000000, objective_monitor);
  solver.Solve(main_phase, search_log, objective_monitor);
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(FATAL) << "Please supply a data file with --data_file=";
  }
  operations_research::JobShopData data;
  data.Load(FLAGS_data_file);
  operations_research::Jobshop(data);
  return 0;
}
