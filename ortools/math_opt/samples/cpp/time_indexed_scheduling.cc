// Copyright 2010-2025 Google LLC
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

// Problem statement.
//
// Data:
//  * n jobs
//  * processing time p_i for i = 1,...,n
//  * release time r_i for i = 1,...,n
//  * Implied: T = max_i r_i + sum_i p_i, the time horizon, all jobs must start
//    in [0, T].
//
// Problem: schedule the jobs sequentially (on a single machine) to minimize the
// sum of the completion times, where each job cannot start until the release
// time. In the scheduling literature, this problem is 1|r_i|sum_i C_i. This
// problem is known to be NP-Hard (e.g. see "Elements of Scheduling" by Lenstra
// and Shmoys 2020, Chapter 4).
//
// Variables:
//  * x_it for job i = 1,...,n and time t = 1,...,T, if job i starts at time t.
//
// Model:
//   min   sum_i sum_t (t + p_i) * x_it
//   s.t.  sum_t x_it = 1                     for all i = 1,...,n     (1)
//         sum_i sum_{s=t-p_i+1}^t x_is <= 1  for all t = 0,...,T     (2)
//         x_it = 0                           for all i, for t < r_i  (3)
//         x_it in {0, 1}                     for all i and t
//
// In the objective, t + p_i is the time the job is completed if it starts at t.
// Constraint (1) ensures that each job is scheduled once, constraint (2)
// ensures that no two jobs overlap in when they are running, and constraint (3)
// enforces the release dates.

#include <algorithm>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

ABSL_FLAG(operations_research::math_opt::SolverType, solver_type,
          operations_research::math_opt::SolverType::kGscip,
          "The solver needs to support binary IP.");

ABSL_FLAG(int, num_jobs, 30, "How many jobs to schedule");
ABSL_FLAG(bool, use_test_data, false,
          "Solve a small hard coded instance instead of a large random one.");

namespace {

namespace math_opt = operations_research::math_opt;

struct Job {
  int processing_time = 0;
  int release_time = 0;
};

std::vector<Job> RandomJobs(int num_jobs) {
  // Processing times are uniform [1, kProcessingTimeUb].
  constexpr int kProcessingTimeUb = 20;

  // Release times are uniform in [0, kReleaseTimeUb].
  const int kReleaseTimeUb = num_jobs * kProcessingTimeUb / 2;

  absl::BitGen bit_gen;
  std::vector<Job> result;
  result.reserve(num_jobs);
  for (int i = 0; i < num_jobs; ++i) {
    result.push_back(
        {.processing_time = absl::Uniform(bit_gen, 1, kProcessingTimeUb),
         .release_time = absl::Uniform(bit_gen, 0, kReleaseTimeUb)});
  }
  return result;
}

// A small instance for testing. The optimal solution is to run:
//   Job 1 at time 1
//   Job 2 at time 2
//   Job 0 at time 7
// This gives a sum of completion times of 2 + 7 + 17 = 26
//
// Note that the above schedule idles at time 0. If instead, we did
//   Job 2 at time 0
//   Job 1 at time 5
//   Job 0 at time 6
// This gives a sum of completion times of 5 + 6 + 16 = 27
std::vector<Job> TestInstance() {
  return {{.processing_time = 10, .release_time = 0},
          {.processing_time = 1, .release_time = 1},
          {.processing_time = 5, .release_time = 0}};
}

int TimeHorizon(absl::Span<const Job> jobs) {
  int max_release = 0;
  int sum_processing = 0;
  for (const Job& job : jobs) {
    max_release = std::max(job.release_time, max_release);
    sum_processing += job.processing_time;
  }
  return max_release + sum_processing;
}

struct Schedule {
  std::vector<int> start_times;
  int sum_of_completion_times = 0;
};

absl::StatusOr<Schedule> Solve(absl::Span<const Job> jobs,
                               const math_opt::SolverType solver_type) {
  const int kTimeHorizon = TimeHorizon(jobs);
  math_opt::Model model;
  // x[i][t] indicates that we start job i at time t.
  std::vector<std::vector<math_opt::Variable>> x(jobs.size());
  math_opt::LinearExpression sum_completion_times;
  for (int i = 0; i < jobs.size(); ++i) {
    for (int t = 0; t < kTimeHorizon; ++t) {
      math_opt::Variable v = model.AddBinaryVariable();
      const int completion_time = t + jobs[i].processing_time;
      sum_completion_times += completion_time * v;
      if (t < jobs[i].release_time) {
        model.set_upper_bound(v, 0.0);
      }
      x[i].push_back(v);
    }
    // Pick one time to run job i.
    model.AddLinearConstraint(math_opt::Sum(x[i]) == 1.0);
  }
  model.Minimize(sum_completion_times);
  // Run at most one job at a time
  for (int t = 0; t < kTimeHorizon; ++t) {
    math_opt::LinearExpression conflicts;
    for (int i = 0; i < jobs.size(); ++i) {
      for (int s = std::max(0, t - jobs[i].processing_time + 1); s <= t; s++) {
        conflicts += x[i][s];
      }
    }
    model.AddLinearConstraint(conflicts <= 1.0);
  }
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   math_opt::Solve(model, solver_type,
                                   {.parameters = {.enable_output = true}}));
  if (!result.has_primal_feasible_solution()) {
    return util::InvalidArgumentErrorBuilder()
           << "no primal feasible solution, termination: "
           << result.termination;
  }
  Schedule schedule;
  schedule.sum_of_completion_times =
      sum_completion_times.Evaluate(result.variable_values());
  for (int i = 0; i < jobs.size(); ++i) {
    for (int t = 0; t < kTimeHorizon; ++t) {
      const double var_value = result.variable_values().at(x[i][t]);
      if (var_value > 0.5) {
        schedule.start_times.push_back(t);
        break;
      }
    }
  }
  return schedule;
}

void PrintSchedule(absl::Span<const Job> jobs, const Schedule& schedule) {
  std::cout << "sum of completion times: " << schedule.sum_of_completion_times
            << std::endl;
  std::vector<std::pair<int, Job>> jobs_by_start_time;
  for (int i = 0; i < jobs.size(); ++i) {
    jobs_by_start_time.push_back({schedule.start_times[i], jobs[i]});
  }
  absl::c_sort(jobs_by_start_time, [](const auto& left, const auto& right) {
    return left.first < right.first;
  });
  std::cout << "start time, processing time, release time" << std::endl;
  for (const auto [start_time, job] : jobs_by_start_time) {
    std::cout << start_time << ", " << job.processing_time << ", "
              << job.release_time << std::endl;
  }
}

absl::Status Main() {
  std::vector<Job> jobs;
  if (absl::GetFlag(FLAGS_use_test_data)) {
    jobs = TestInstance();
  } else {
    const int num_jobs = absl::GetFlag(FLAGS_num_jobs);
    jobs = RandomJobs(num_jobs);
  }
  ASSIGN_OR_RETURN(const Schedule schedule,
                   Solve(jobs, absl::GetFlag(FLAGS_solver_type)));
  PrintSchedule(jobs, schedule);
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
