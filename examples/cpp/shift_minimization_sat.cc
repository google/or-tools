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

// Reader and solver for the shift minimization personnel task
// scheduling problem (see
// https://publications.csiro.au/rpr/download?pid=csiro:EP104071&dsid=DS2)/
//
// Data files are in
//    /cns/li-d/home/operations-research/shift_minization_scheduling
//
// The problem is the following:
//   - There is a list of jobs. Each job has a start date and an end date. They
//     must all be performed.
//   - There is a set of workers. Each worker can perform on or more jobs among
//     a subset of job. One worker cannot perform two jobs that overlaps.
//   - The objective it to minimize the number of active workers, while
//     performing all the jobs.

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/strtoint.h"
#include "ortools/util/filelineiter.h"
#include "ortools/base/split.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_solver.h"

DEFINE_string(input, "", "Input file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
DEFINE_bool(use_core, false, "Use the core based solver.");

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

    File* file = nullptr;
    if (!file::Open(file_name, "r", &file, file::Defaults()).ok()) {
      LOG(WARNING) << "Can't open " << file_name;
      return false;
    }

    load_status_ = STARTED;

    for (const std::string& line :
         FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
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

  void ProcessLine(const std::string& line) {
    if (line.empty() || line[0] == '#') {
      return;
    }

    const std::vector<std::string> words = strings::Split(
        line, strings::delimiter::AnyOf(" :\t"), strings::SkipEmpty());

    switch (load_status_) {
      case NOT_STARTED: {
        LOG(FATAL) << "Wrong status: NOT_STARTED";
        break;
      }
      case STARTED: {
        if (words.size() == 3 && words[0] == "Type") {
          CHECK_EQ(1, atoi32(words[2]));
        } else if (words.size() == 3 && words[0] == "Jobs") {
          declared_num_jobs_ = atoi32(words[2]);
          possible_assignments_per_job_.resize(declared_num_jobs_);
          load_status_ = JOBS_SEEN;
        } else {
          LOG(FATAL) << "Wrong state STARTED with line " << line;
        }
        break;
      }
      case JOBS_SEEN: {
        if (words.size() == 2) {
          jobs_.push_back({atoi32(words[0]), atoi32(words[1])});
        } else if (words.size() == 3 && words[0] == "Qualifications") {
          declared_num_workers_ = atoi32(words[2]);
          possible_jobs_per_worker_.resize(declared_num_workers_);
          load_status_ = WORKERS_SEEN;
        } else {
          LOG(FATAL) << "Wrong state JOBS_SEEN with line " << line;
        }
        break;
      }
      case WORKERS_SEEN: {
        CHECK_EQ(atoi32(words[0]), words.size() - 1);
        for (int i = 1; i < words.size(); ++i) {
          const int job = atoi32(words[i]);
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

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const int num_workers = parser.possible_jobs_per_worker().size();
  const std::vector<ShiftMinimizationParser::Job>& jobs = parser.jobs();
  const int num_jobs = jobs.size();

  std::vector<Literal> active_workers(num_workers);
  std::vector<std::vector<Literal>> worker_job_literals(num_workers);
  std::vector<std::vector<Literal>> selected_workers_per_job(num_jobs);

  for (int w = 0; w < num_workers; ++w) {
    // Status variables for workers, are they active or not?
    active_workers[w] = Literal(model.Add(NewBooleanVariable()), true);

    // Job-Worker literal. worker_job_literals[w][i] is true iff worker w
    // performs it's ith possible job.
    const std::vector<int>& possible = parser.possible_jobs_per_worker()[w];
    for (int p : possible) {
      worker_job_literals[w].push_back(
          Literal(model.Add(NewBooleanVariable()), true));
      selected_workers_per_job[p].push_back(worker_job_literals[w].back());
    }

    // Add conflicts on overlapping jobs for the same worker.
    for (int i = 0; i < possible.size() - 1; ++i) {
      for (int j = i + 1; j < possible.size(); ++j) {
        const int job1 = possible[i];
        const int job2 = possible[j];
        if (Overlaps(jobs[job1], jobs[job2])) {
          const Literal l1 = worker_job_literals[w][i];
          const Literal l2 = worker_job_literals[w][j];
          model.Add(ClauseConstraint({l1.Negated(), l2.Negated()}));
        }
      }
    }

    // Maintain active_workers variable.
    model.Add(ReifiedBoolOr(worker_job_literals[w], active_workers[w]));
  }

  // All jobs must be performed.
  for (int j = 0; j < num_jobs; ++j) {
    // this does not enforce that at most one worker performs one job.
    // It should not change the solution cost.
    // TODO(user): Checks if sum() == 1 improves the solving speed.
    model.Add(ClauseConstraint(selected_workers_per_job[j]));
  }

  // Redundant constraint:
  //   For each time point, count the number of active jobs at that time,
  //   then the number of active workers on these jobs is equal to the number of
  //   active jobs.
  std::set<int> time_points;
  std::set<std::vector<int>> visited_job_lists;
  std::map<std::vector<Literal>, Literal> active_literal_cache;

  for (int j = 0; j < num_jobs; ++j) {
    time_points.insert(parser.jobs()[j].start);
    time_points.insert(parser.jobs()[j].end);
  }

  int num_reused_literals = 0;
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
    if (ContainsKey(visited_job_lists, intersecting_jobs)) continue;
    visited_job_lists.insert(intersecting_jobs);

    // Collect the relevant literals, and regroup them per worker.
    std::map<int, std::vector<Literal>> active_literals_per_workers;
    for (int j : intersecting_jobs) {
      for (const auto& p : parser.possible_assignments_per_job()[j]) {
        const Literal lit = worker_job_literals[p.worker_id][p.job_index];
        active_literals_per_workers[p.worker_id].push_back(lit);
      }
    }

    // Create the worker activity variables.
    std::vector<Literal> active_worker_literals;
    for (const auto& it : active_literals_per_workers) {
      Literal active;
      if (ContainsKey(active_literal_cache, it.second)) {
        active = active_literal_cache[it.second];
        num_reused_literals++;
      } else {
        active = Literal(model.Add(NewBooleanVariable()), true);
        model.Add(Implication(active, active_workers[it.first]));
        model.Add(ReifiedBoolOr(it.second, active));
        active_literal_cache[it.second] = active;
      }

      active_worker_literals.push_back(active);
    }

    // Add the count constraints: We have as many active workers as jobs.
    num_count_constraints++;
    const int num_jobs = intersecting_jobs.size();
    max_intersection_size = std::max(max_intersection_size, num_jobs);
    model.Add(
        CardinalityConstraint(num_jobs, num_jobs, active_worker_literals));
  }

  LOG(INFO) << "Added " << num_count_constraints
            << " count constraints while processing " << time_points.size()
            << " time points.";
  LOG(INFO) << "This has created " << active_literal_cache.size()
            << " active worker literals, and reused them "
            << num_reused_literals << " times.";
  LOG(INFO) << "Lower bound = " << max_intersection_size;

  if (FLAGS_use_core) {
    const std::vector<int64> coeffs(num_workers, 1);
    MinimizeWeightedLiteralSumWithCoreAndLazyEncoding(
        /*log_info=*/true, active_workers, coeffs, nullptr, nullptr, &model);
    return;
  }

  // Objective.
  std::vector<int> weights(num_workers, 1);
  std::vector<IntegerVariable> worker_vars(num_workers);
  for (int w = 0; w < num_workers; ++w) {
    worker_vars[w] =
        model.Add(NewIntegerVariableFromLiteral(active_workers[w]));
  }
  const IntegerVariable objective_var =
      model.Add(NewIntegerVariable(max_intersection_size, num_workers));
  weights.push_back(-1);
  worker_vars.push_back(objective_var);

  model.Add(FixedWeightedSum(worker_vars, weights, 0));

  MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      /*log_info=*/true, objective_var, nullptr,
      /*feasible_solution_observer=*/
      [&](const Model& model) {
        LOG(INFO) << "Cost " << model.Get(Value(objective_var));
      },
      &model);
}
}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::sat::LoadAndSolve(FLAGS_input);
  return EXIT_SUCCESS;
}
