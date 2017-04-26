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
// This model implements a simple jobshop problem.
//
// A jobshop is a standard scheduling problem where you must schedule a
// set of jobs on a set of machines.  Each job is a sequence of tasks
// (a task can only start when the preceding task finished), each of
// which occupies a single specific machine during a specific
// duration. Therefore, a job is simply given by a sequence of pairs
// (machine id, duration).

// The objective is to minimize the 'makespan', which is the duration
// between the start of the first task (across all machines) and the
// completion of the last task (across all machines).
//
// This will be modelled by sets of intervals variables (see class
// IntervalVar in constraint_solver/constraint_solver.h), one per
// task, representing the [start_time, end_time] of the task.  Tasks
// in the same job will be linked by precedence constraints.  Tasks on
// the same machine will be covered by Sequence constraints.

#ifndef OR_TOOLS_EXAMPLES_JOBSHOP_H_
#define OR_TOOLS_EXAMPLES_JOBSHOP_H_

#include <cstdio>
#include <cstdlib>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
// ----- JobShopData -----

// A JobShopData parses data files and stores all data internally for
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

  enum ProblemType {
    UNDEFINED,
    JSSP,
    TAILLARD
  };

  enum TaillardState {
    START,
    JOBS_READ,
    MACHINES_READ,
    SEED_READ,
    JOB_ID_READ,
    JOB_LENGTH_READ,
    JOB_READ
  };

  JobShopData()
      : name_(""),
        machine_count_(0),
        job_count_(0),
        horizon_(0),
        current_job_index_(0),
        problem_type_(UNDEFINED),
        taillard_state_(START) {}

  ~JobShopData() {}

  // Parses a file in jssp or taillard format and loads the model. See the flag
  // --data_file for a description of the format. Note that the format
  // is only partially checked: bad inputs might cause undefined
  // behavior.
  void Load(const std::string& filename) {
    for (const std::string& line : FileLines(filename)) {
      if (line.empty()) {
        continue;
      }
      ProcessNewLine(line);
    }
  }

  // The number of machines in the jobshop.
  int machine_count() const { return machine_count_; }

  // The number of jobs in the jobshop.
  int job_count() const { return job_count_; }

  // The name of the jobshop instance.
  const std::string& name() const { return name_; }

  // The horizon of the workshop (the sum of all durations), which is
  // a trivial upper bound of the optimal make_span.
  int horizon() const { return horizon_; }

  // Returns the tasks of a job, ordered by precedence.
  const std::vector<Task>& TasksOfJob(int job_id) const {
    return all_tasks_[job_id];
  }

 private:
  void ProcessNewLine(const std::string& line) {
    // TODO(user): more robust logic to support single-task jobs.
    const std::vector<std::string> words =
        strings::Split(line, ' ', strings::SkipEmpty());
    switch (problem_type_) {
      case UNDEFINED: {
        if (words.size() == 2 && words[0] == "instance") {
          problem_type_ = JSSP;
          LOG(INFO) << "Reading jssp instance " << words[1];
          name_ = words[1];
        } else if (words.size() == 1 && atoi32(words[0]) > 0) {
          problem_type_ = TAILLARD;
          taillard_state_ = JOBS_READ;
          job_count_ = atoi32(words[0]);
          CHECK_GT(job_count_, 0);
          all_tasks_.resize(job_count_);
        }
        break;
      }
      case JSSP: {
        if (words.size() == 2) {
          job_count_ = atoi32(words[0]);
          machine_count_ = atoi32(words[1]);
          CHECK_GT(machine_count_, 0);
          CHECK_GT(job_count_, 0);
          LOG(INFO) << machine_count_ << " machines and " << job_count_
                    << " jobs";
          all_tasks_.resize(job_count_);
        }

        if (words.size() > 2 && machine_count_ != 0) {
          CHECK_EQ(words.size(), machine_count_ * 2);
          for (int i = 0; i < machine_count_; ++i) {
            const int machine_id = atoi32(words[2 * i]);
            const int duration = atoi32(words[2 * i + 1]);
            AddTask(current_job_index_, machine_id, duration);
          }
          current_job_index_++;
        }
        break;
      }
      case TAILLARD: {
        switch (taillard_state_) {
          case START: {
            LOG(FATAL) << "Should not be here";
            break;
          }
          case JOBS_READ: {
            CHECK_EQ(1, words.size());
            machine_count_ = atoi32(words[0]);
            CHECK_GT(machine_count_, 0);
            taillard_state_ = MACHINES_READ;
            break;
          }
          case MACHINES_READ: {
            CHECK_EQ(1, words.size());
            const int seed = atoi32(words[0]);
            LOG(INFO) << "Taillard instance with " << job_count_
                      << " jobs, and " << machine_count_
                      << " machines, generated with a seed of " << seed;
            taillard_state_ = SEED_READ;
            break;
          }
          case SEED_READ:
          case JOB_READ: {
            CHECK_EQ(1, words.size());
            current_job_index_ = atoi32(words[0]);
            taillard_state_ = JOB_ID_READ;
            break;
          }
          case JOB_ID_READ: {
            CHECK_EQ(1, words.size());
            taillard_state_ = JOB_LENGTH_READ;
            break;
          }
          case JOB_LENGTH_READ: {
            CHECK_EQ(machine_count_, words.size());
            for (int i = 0; i < machine_count_; ++i) {
              const int duration = atoi32(words[i]);
              AddTask(current_job_index_, i, duration);
            }
            taillard_state_ = JOB_READ;
            break;
          }
        }
        break;
      }
    }
  }

  void AddTask(int job_id, int machine_id, int duration) {
    all_tasks_[job_id].push_back(Task(job_id, machine_id, duration));
    horizon_ += duration;
  }

  std::string name_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<std::vector<Task> > all_tasks_;
  int current_job_index_;
  ProblemType problem_type_;
  TaillardState taillard_state_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_JOBSHOP_H_
