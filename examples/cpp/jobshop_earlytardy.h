// Copyright 2010-2017 Google
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

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/random.h"
#include "ortools/base/split.h"
#include "ortools/util/filelineiter.h"

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
        early_cost(ew),
        tardy_cost(tw) {}
  int release_date;
  int due_date;
  int early_cost;
  int tardy_cost;
  std::vector<Task> all_tasks;
};

class EtJobShopData {
 public:
  EtJobShopData()
      : machine_count_(0),
        job_count_(0),
        horizon_(0) {}

  ~EtJobShopData() {}

  void LoadJetFile(const std::string& filename) {
    LOG(INFO) << "Reading jet file " << filename;
    name_ = StringPrintf("JetData(%s)", filename.c_str());
    for (const std::string& line : FileLines(filename)) {
      if (line.empty()) {
        continue;
      }
      ProcessNewJetLine(line);
    }
  }

  void GenerateRandomData(int machine_count,
                          int job_count,
                          int max_release_date,
                          int max_early_cost,
                          int max_tardy_cost,
                          int max_duration,
                          int scale_factor,
                          int seed) {
    name_ = StringPrintf("EtJobshop(m%d-j%d-mrd%d-mew%d-mtw%d-md%d-sf%d-s%d)",
                         machine_count,
                         job_count,
                         max_release_date,
                         max_early_cost,
                         max_tardy_cost,
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
                              random.Uniform(max_early_cost),
                              random.Uniform(max_tardy_cost)));
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
  const std::string& name() const { return name_; }

  // The horizon of the workshop (the sum of all durations), which is
  // a trivial upper bound of the optimal make_span.
  int horizon() const { return horizon_; }

  // Returns the tasks of a job, ordered by precedence.
  const Job& GetJob(int job_id) const {
    return all_jobs_[job_id];
  }

 private:
  void ProcessNewJetLine(const std::string& line) {
    // TODO(user): more robust logic to support single-task jobs.
    static const char kWordDelimiters[] = " ";
    std::vector<std::string> words =
        strings::Split(line, " ", strings::SkipEmpty());

    if (words.size() == 2) {
      job_count_ = atoi32(words[0]);
      machine_count_ = atoi32(words[1]);
      CHECK_GT(machine_count_, 0);
      CHECK_GT(job_count_, 0);
      LOG(INFO) << machine_count_ << "  - machines and "
                << job_count_ << " jobs";
    } else if (words.size() > 2 && machine_count_ != 0) {
      const int job_id = all_jobs_.size();
      CHECK_EQ(words.size(), machine_count_ * 2 + 3);
      const int due_date = atoi32(words[2 * machine_count_]);
      const int early_cost = atoi32(words[2 * machine_count_ + 1]);
      const int late_cost = atoi32(words[2 * machine_count_ + 2]);
      LOG(INFO) << "Add job with due date = " << due_date
                << ", early cost = " << early_cost
                << ", and late cost = " << late_cost;
      all_jobs_.push_back(Job(0, due_date, early_cost, late_cost));
      for (int i = 0; i < machine_count_; ++i) {
        const int machine_id = atoi32(words[2 * i]);
        const int duration = atoi32(words[2 * i + 1]);
        all_jobs_.back().all_tasks.push_back(
            Task(job_id, machine_id, duration));
        horizon_ += duration;
      }
    }
  }

  std::string name_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<Job> all_jobs_;
};
}  // namespace operations_research
#endif // OR_TOOLS_EXAMPLES_JOBSHOP_EARLYTARDY_H_
