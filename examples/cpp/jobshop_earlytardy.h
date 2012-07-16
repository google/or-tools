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
#include "base/random.h"
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

  void GenerateRandomData(int machine_count,
                          int job_count,
                          int max_release_date,
                          int max_earlyness_weight,
                          int max_tardiness_weight,
                          int max_duration,
                          int scale_factor,
                          int seed) {
    name_ = StringPrintf("EtJobshop(m%d-j%d-mrd%d-mew%d-mtw%d-md%d-sf%d-s%d)",
                         machine_count,
                         job_count,
                         max_release_date,
                         max_earlyness_weight,
                         max_tardiness_weight,
                         max_duration,
                         scale_factor,
                         seed);
    LOG(INFO) << "Generating random problem " << name_;
    ACMRandom random(seed);
    machine_count_ = machine_count;
    job_count_ = job_count;
    for (int j = 0; j < job_count_; ++j) {
      const int release_date = random.Uniform(max_release_date);
      int sum_of_durations = max_release_date;
      all_jobs_.push_back(Job(release_date,
                              0,  // due date, to be filled later.
                              random.Uniform(max_earlyness_weight),
                              random.Uniform(max_tardiness_weight)));
      for (int m = 0; m < machine_count_; ++m) {
        const int duration = random.Uniform(max_duration);
        all_jobs_.back().all_tasks.push_back(Task(j, m, duration));
        sum_of_durations += duration;
      }
      all_jobs_.back().due_date = sum_of_durations * scale_factor / 100;
      horizon_ += all_jobs_.back().due_date;
      // Scramble jobs.
      for (int m = 0; m < machine_count_; ++m) {
        Task t = all_jobs_.back().all_tasks[m];
        const int target = random.Uniform(machine_count_);
        all_jobs_.back().all_tasks[m] = all_jobs_.back().all_tasks[target];
        all_jobs_.back().all_tasks[target] = t;
      }
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

