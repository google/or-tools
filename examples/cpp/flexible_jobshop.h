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

#ifndef OR_TOOLS_EXAMPLES_FLEXIBLE_JOBSHOP_H_
#define OR_TOOLS_EXAMPLES_FLEXIBLE_JOBSHOP_H_

#include <cstdio>
#include <cstdlib>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/util/string_array.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
// A FlexibleJobShopData parses data files and stores all data internally for
// easy retrieval.
class FlexibleJobShopData {
 public:
  // A task is the basic block of a jobshop.
  struct Task {
    Task(int j, const std::vector<int>& m, const std::vector<int>& d)
        : job_id(j), machines(m), durations(d) {}

    string DebugString() const {
      string out = StringPrintf("Task(", job_id);
      for (int k = 0; k < machines.size(); ++k) {
        out.append(StringPrintf("<m%d,%d>", machines[k], durations[k]));
        if (k < machines.size() - 1) {
          out.append(" | ");
        }
      }
      out.append(")");
      return out;
    }

    int job_id;
    std::vector<int> machines;
    std::vector<int> durations;
  };

  FlexibleJobShopData()
      : name_(""),
        machine_count_(-1),
        job_count_(-1),
        horizon_(0),
        current_job_index_(0) {}

  ~FlexibleJobShopData() {}

  // Parses a file in jssp or taillard format and loads the model. See the flag
  // --data_file for a description of the format. Note that the format
  // is only partially checked: bad inputs might cause undefined
  // behavior.
  void Load(const string& filename) {
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
  const string& name() const { return name_; }

  // The horizon of the workshop (the sum of all durations), which is
  // a trivial upper bound of the optimal make_span.
  int horizon() const { return horizon_; }

  // Returns the tasks of a job, ordered by precedence.
  const std::vector<Task>& TasksOfJob(int job_id) const {
    return all_tasks_[job_id];
  }

  string DebugString() const {
    string out =
        StringPrintf("FlexibleJobshop(name = %s, %d machines, %d jobs)\n",
                     name_.c_str(), machine_count_, job_count_);
    for (int j = 0; j < all_tasks_.size(); ++j) {
      out.append(StringPrintf("  job %d: ", j));
      for (int k = 0; k < all_tasks_[j].size(); ++k) {
        out.append(all_tasks_[j][k].DebugString());
        if (k < all_tasks_[j].size() - 1) {
          out.append(" -> ");
        } else {
          out.append("\n");
        }
      }
    }
    return out;
  }

 private:
  void ProcessNewLine(const std::string& line) {
    static const char kWordDelimiters[] = " ";
    std::vector<string> words =
        strings::Split(line, " ", strings::SkipEmpty());
    if (machine_count_ == -1 && words.size() > 1) {
      job_count_ = atoi32(words[0]);
      machine_count_ = atoi32(words[1]);
      CHECK_GT(machine_count_, 0);
      CHECK_GT(job_count_, 0);
      LOG(INFO) << machine_count_ << " machines and " << job_count_ << " jobs";
      all_tasks_.resize(job_count_);
    } else if (words.size() > 1) {
      const int operations_count = atoi32(words[0]);
      int index = 1;
      for (int operation = 0; operation < operations_count; ++operation) {
        std::vector<int> machines;
        std::vector<int> durations;
        const int alternatives_count = atoi32(words[index++]);
        for (int alt = 0; alt < alternatives_count; alt++) {
          // Machine id are 1 based.
          const int machine_id = atoi32(words[index++]) - 1;
          const int duration = atoi32(words[index++]);
          machines.push_back(machine_id);
          durations.push_back(duration);
        }
        AddTask(current_job_index_, machines, durations);
      }
      CHECK_EQ(index, words.size());
      current_job_index_++;
    }
  }

  static int SumOfDurations(const std::vector<int>& durations) {
    int result = 0;
    for (int i = 0; i < durations.size(); ++i) {
      result += durations[i];
    }
    return result;
  }

  void AddTask(int job_id, const std::vector<int>& machines,
               const std::vector<int>& durations) {
    all_tasks_[job_id].push_back(Task(job_id, machines, durations));
    horizon_ += SumOfDurations(durations);
  }

  string name_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<std::vector<Task> > all_tasks_;
  int current_job_index_;
};
}       // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FLEXIBLE_JOBSHOP_H_
