// Copyright 2010-2018 Google LLC
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

// Reader and solver for the shift minimization personnel task
// scheduling problem (see
// https://publications.csiro.au/rpr/download?pid=csiro:EP104071&dsid=DS2)/
//
// Data files are in
//    examples/data/shift_scheduling/minization
//
// The problem is the following:
//   - There is a list of jobs. Each job has a start date and an end date. They
//     must all be performed.
//   - There is a set of workers. Each worker can perform one or more jobs among
//     a subset of job. One worker cannot perform two jobs that overlaps.
//   - The objective it to minimize the number of active workers, while
//     performing all the jobs.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/filelineiter.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

DEFINE_string(input, "", "Input file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");

namespace operations_research {
namespace sat {
class ShiftMinimizationParser {
 public:
  struct Job {
    int start;
    int end;
  };

  struct Assignment {
    int worker_id;
    int job_index;
  };

  ShiftMinimizationParser()
      : load_status_(NOT_STARTED),
        declared_num_jobs_(0),
        declared_num_workers_(0),
        num_workers_read_(0) {}

  const std::vector<Job>& jobs() const { return jobs_; }
  const std::vector<std::vector<int>>& possible_jobs_per_worker() const {
    return possible_jobs_per_worker_;
  }
  const std::vector<std::vector<Assignment>>& possible_assignments_per_job()
      const {
    return possible_assignments_per_job_;
  }

  // The file format is the following
  // # comments...
  // Type = 1
  // Jobs = <n>
  // <start> <end>  // Repeated n times
  // Qualifications = <k>
  // c: job_1 .. job_c  // repeated k times (a counter and job ids after).
  bool LoadFile(const std::string& file_name) {
    if (load_status_ != NOT_STARTED) {
      return false;
    }

    load_status_ = STARTED;

    for (const std::string& line :
         FileLines(file_name, FileLineIterator::REMOVE_LINEFEED |
                                  FileLineIterator::REMOVE_INLINE_CR)) {
      ProcessLine(line);
    }

    LOG(INFO) << "Read file " << file_name << " with " << declared_num_jobs_
              << " jobs, and " << declared_num_workers_ << " workers.";
    return declared_num_jobs_ != 0 && jobs_.size() == declared_num_jobs_ &&
           declared_num_workers_ != 0 &&
           declared_num_workers_ == num_workers_read_;
  }

 private:
  enum LoadStatus { NOT_STARTED, STARTED, JOBS_SEEN, WORKERS_SEEN };

  int strtoint32(const std::string& word) {
    int result;
    CHECK(absl::SimpleAtoi(word, &result));
    return result;
  }

  void ProcessLine(const std::string& line) {
    if (line.empty() || line[0] == '#') {
      return;
    }

    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());

    switch (load_status_) {
      case NOT_STARTED: {
        LOG(FATAL) << "Wrong status: NOT_STARTED";
        break;
      }
      case STARTED: {
        if (words.size() == 3 && words[0] == "Type") {
          CHECK_EQ(1, strtoint32(words[2]));
        } else if (words.size() == 3 && words[0] == "Jobs") {
          declared_num_jobs_ = strtoint32(words[2]);
          possible_assignments_per_job_.resize(declared_num_jobs_);
          load_status_ = JOBS_SEEN;
        } else {
          LOG(FATAL) << "Wrong state STARTED with line " << line;
        }
        break;
      }
      case JOBS_SEEN: {
        if (words.size() == 2) {
          jobs_.push_back({strtoint32(words[0]), strtoint32(words[1])});
        } else if (words.size() == 3 && words[0] == "Qualifications") {
          declared_num_workers_ = strtoint32(words[2]);
          possible_jobs_per_worker_.resize(declared_num_workers_);
          load_status_ = WORKERS_SEEN;
        } else {
          LOG(FATAL) << "Wrong state JOBS_SEEN with line " << line;
        }
        break;
      }
      case WORKERS_SEEN: {
        CHECK_EQ(strtoint32(words[0]), words.size() - 1);
        for (int i = 1; i < words.size(); ++i) {
          const int job = strtoint32(words[i]);
          const int pos = possible_jobs_per_worker_[num_workers_read_].size();
          possible_jobs_per_worker_[num_workers_read_].push_back(job);
          possible_assignments_per_job_[job].push_back(
              {num_workers_read_, pos});
        }
        num_workers_read_++;
        break;
      }
    }
  }

  std::vector<Job> jobs_;
  std::vector<std::vector<int>> possible_jobs_per_worker_;
  std::vector<std::vector<Assignment>> possible_assignments_per_job_;
  LoadStatus load_status_;
  int declared_num_jobs_;
  int declared_num_workers_;
  int num_workers_read_;
};

bool Overlaps(const ShiftMinimizationParser::Job& j1,
              const ShiftMinimizationParser::Job& j2) {
  // TODO(user): Are end date inclusive or exclusive? To check.
  // For now, we assume that they are exclusive.
  return !(j1.start > j2.end || j2.start > j1.end);
}

void LoadAndSolve(const std::string& file_name) {
  ShiftMinimizationParser parser;
  CHECK(parser.LoadFile(file_name));

  CpModelBuilder cp_model;

  const int num_workers = parser.possible_jobs_per_worker().size();
  const std::vector<ShiftMinimizationParser::Job>& jobs = parser.jobs();
  const int num_jobs = jobs.size();

  std::vector<BoolVar> active_workers(num_workers);
  std::vector<std::vector<BoolVar>> worker_job_vars(num_workers);
  std::vector<std::vector<BoolVar>> possible_workers_per_job(num_jobs);

  for (int w = 0; w < num_workers; ++w) {
    // Status variables for workers, are they active or not?
    active_workers[w] = cp_model.NewBoolVar();

    // Job-Worker variable. worker_job_vars[w][i] is true iff worker w
    // performs it's ith possible job.
    const std::vector<int>& possible = parser.possible_jobs_per_worker()[w];
    for (int p : possible) {
      const BoolVar var = cp_model.NewBoolVar();
      worker_job_vars[w].push_back(var);
      possible_workers_per_job[p].push_back(var);
    }

    // Add conflicts on overlapping jobs for the same worker.
    for (int i = 0; i < possible.size() - 1; ++i) {
      for (int j = i + 1; j < possible.size(); ++j) {
        const int job1 = possible[i];
        const int job2 = possible[j];
        if (Overlaps(jobs[job1], jobs[job2])) {
          const BoolVar v1 = worker_job_vars[w][i];
          const BoolVar v2 = worker_job_vars[w][j];
          cp_model.AddBoolOr({Not(v1), Not(v2)});
        }
      }
    }

    // Maintain active_workers variable.
    cp_model.AddBoolOr(worker_job_vars[w]).OnlyEnforceIf(active_workers[w]);
    for (const BoolVar& var : worker_job_vars[w]) {
      cp_model.AddImplication(var, active_workers[w]);
    }
  }

  // All jobs must be performed.
  for (int j = 0; j < num_jobs; ++j) {
    // this does not enforce that at most one worker performs one job.
    // It should not change the solution cost.
    // TODO(user): Checks if sum() == 1 improves the solving speed.
    cp_model.AddBoolOr(possible_workers_per_job[j]);
  }

  // Redundant constraint:
  //   For each time point, count the number of active jobs at that time,
  //   then the number of active workers on these jobs is equal to the number of
  //   active jobs.
  std::set<int> time_points;
  std::set<std::vector<int>> visited_job_lists;

  for (int j = 0; j < num_jobs; ++j) {
    time_points.insert(parser.jobs()[j].start);
    time_points.insert(parser.jobs()[j].end);
  }

  int num_count_constraints = 0;
  int max_intersection_size = 0;

  // Add one counting constraint per time point.
  for (int t : time_points) {
    // Collect all jobs that intersects with this time point.
    std::vector<int> intersecting_jobs;
    for (int j = 0; j < num_jobs; ++j) {
      const ShiftMinimizationParser::Job& job = parser.jobs()[j];
      // Assumption: End date are inclusive.
      if (t >= job.start && t <= job.end) {
        intersecting_jobs.push_back(j);
      }
    }

    // Check that we have not already visited this exact set of candidate jobs.
    if (gtl::ContainsKey(visited_job_lists, intersecting_jobs)) continue;
    visited_job_lists.insert(intersecting_jobs);

    // Collect the relevant worker job vars.
    std::vector<BoolVar> overlapping_worker_jobs;
    for (int j : intersecting_jobs) {
      for (const auto& p : parser.possible_assignments_per_job()[j]) {
        const BoolVar var = worker_job_vars[p.worker_id][p.job_index];
        overlapping_worker_jobs.push_back(var);
      }
    }

    // Add the count constraints: We have as many active workers as jobs.
    const int num_jobs = intersecting_jobs.size();
    cp_model.AddEquality(LinearExpr::BooleanSum(overlapping_worker_jobs),
                         num_jobs);
    // Book keeping.
    max_intersection_size = std::max(max_intersection_size, num_jobs);
    num_count_constraints++;
  }

  LOG(INFO) << "Added " << num_count_constraints
            << " count constraints while processing " << time_points.size()
            << " time points.";
  LOG(INFO) << "Lower bound = " << max_intersection_size;

  // Objective.
  const IntVar objective_var =
      cp_model.NewIntVar(Domain(max_intersection_size, num_workers));
  cp_model.AddEquality(LinearExpr::BooleanSum(active_workers), objective_var);
  cp_model.Minimize(objective_var);

  // Solve.
  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const CpSolverResponse response = SolveWithModel(cp_model.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);
}
}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::sat::LoadAndSolve(FLAGS_input);
  return EXIT_SUCCESS;
}
