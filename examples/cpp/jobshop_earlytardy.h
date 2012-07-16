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
// This model implements a simple jobshop problem.
//
// A jobshop is a standard scheduling problem where you must schedule
// a set of jobs on a set of machines.  Each job is a sequence of
// tasks (a task can only start when the preceding task finished),
// each of which occupies a single specific machine during a specific
// duration. Therefore, a job is a sequence of pairs (machine id,
// duration), along with a release data (minimum start date of the
// first task of the job, and due data (end time of the last job) with
// a tardiness linear penalty.

// The objective is to minimize the 'makespan', which is the duration
// between the start of the first task (across all machines) and the
// completion of the last task (across all machines).
//
// This will be modelled by sets of intervals variables (see class
// IntervalVar in constraint_solver/constraint_solver.h), one per
// task, representing the [start_time, end_time] of the task.  Tasks
// in the same job will be linked by precedence constraints.  Tasks on
// the same machine will be covered by Sequence constraints.

#ifndef OR_TOOLS_EXAMPLES_JOBSHOP_EARLYTARDY_H_
#define OR_TOOLS_EXAMPLES_JOBSHOP_EARLYTARDY_H_

#include <cstdio>
#include <cstdlib>

#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strtoint.h"
#include "base/file.h"
#include "base/filelinereader.h"
#include "base/split.h"

namespace operations_research {
struct Task {
  Task(int j, int m, int d) : job_id(j), machine_id(m), duration(d) {}
  int job_id;
  int machine_id;
  int duration;
};

struct Job {
  Job(int r, int d, int ew, int tw)
      : release_date(r),
        due_date(d),
        earlyness_weight(ew),
        tardiness_weight(tw) {}
  int release_date;
  int due_date;
  int earlyness_weight;
  int tardiness_weight;
  std::vector<Task> all_tasks;
};

class EtJobShopData {
 public:
  EtJobShopData()
      : machine_count_(0),
        job_count_(0),
        horizon_(0) {}

  ~EtJobShopData() {}

  void Load(const string& filename) {

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
  const Job& GetJob(int job_id) const {
    return all_jobs_[job_id];
  }

 private:
  string name_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<Job> all_jobs_;
};
}  // namespace operations_research
#endif OR_TOOLS_EXAMPLES_JOBSHOP_EARLYTARDY_H_

