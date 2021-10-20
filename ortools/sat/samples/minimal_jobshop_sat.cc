// Copyright 2010-2021 Google LLC
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

// [START program]
// Nurse scheduling problem with shift requests.
// [START import]
#include <atomic>
#include <map>
#include <numeric>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
// [END import]

namespace operations_research {
namespace sat {

void MinimalJobshopSat() {
  // [START data]
  using Task = std::tuple<int64_t, int64_t>;  // (machine_id, processing_time)
  using Job = std::vector<Task>;
  std::vector<Job> jobs_data = {
      {{0, 3}, {1, 2}, {2, 2}},  // Job_0: Task_0 Task_1 Task_2
      {{0, 2}, {2, 1}, {1, 4}},  // Job_1: Task_0 Task_1 Task_2
      {{1, 4}, {2, 3}},          // Job_2: Task_0 Task_1
  };

  int64_t num_machines = 0;
  for (const auto& job : jobs_data) {
    for (const auto& [machine, _] : job) {
      num_machines = std::max(num_machines, 1 + machine);
    }
  }

  std::vector<int> all_machines(num_machines);
  std::iota(all_machines.begin(), all_machines.end(), 0);

  // Computes horizon dynamically as the sum of all durations.
  int64_t horizon = 0;
  for (const auto& job : jobs_data) {
    for (const auto& [_, time] : job) {
      horizon += time;
    }
  }
  // [END data]

  // Creates the model.
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  struct TaskType {
    IntVar start;
    IntVar end;
    IntervalVar interval;
  };

  using TaskID = std::tuple<int, int>;  // (job_id, task_id)
  std::map<TaskID, TaskType> all_tasks;
  std::map<int64_t, std::vector<IntervalVar>> machine_to_intervals;
  for (int job_id = 0; job_id < jobs_data.size(); ++job_id) {
    const auto& job = jobs_data[job_id];
    for (int task_id = 0; task_id < job.size(); ++task_id) {
      const auto [machine, duration] = job[task_id];
      std::string suffix = absl::StrFormat("_%d_%d", job_id, task_id);
      IntVar start = cp_model.NewIntVar({0, horizon})
                         .WithName(std::string("start") + suffix);
      IntVar end = cp_model.NewIntVar({0, horizon})
                       .WithName(std::string("end") + suffix);
      IntervalVar interval = cp_model.NewIntervalVar(start, duration, end)
                                 .WithName(std::string("interval") + suffix);

      TaskID key = std::make_tuple(job_id, task_id);
      all_tasks.emplace(key, TaskType{/*.start=*/start,
                                      /*.end=*/end,
                                      /*.interval=*/interval});
      machine_to_intervals[machine].push_back(interval);
    }
  }
  // [END variables]

  // [START constraints]
  // Create and add disjunctive constraints.
  for (const auto machine : all_machines) {
    cp_model.AddNoOverlap(machine_to_intervals[machine]);
  }

  // Precedences inside a job.
  for (int job_id = 0; job_id < jobs_data.size(); ++job_id) {
    const auto& job = jobs_data[job_id];
    for (int task_id = 0; task_id < job.size() - 1; ++task_id) {
      TaskID key = std::make_tuple(job_id, task_id);
      TaskID next_key = std::make_tuple(job_id, task_id + 1);
      cp_model.AddGreaterOrEqual(all_tasks[next_key].start, all_tasks[key].end);
    }
  }
  // [END constraints]

  // [START objective]
  // Makespan objective.
  IntVar obj_var = cp_model.NewIntVar({0, horizon}).WithName("makespan");

  std::vector<IntVar> ends;
  for (int job_id = 0; job_id < jobs_data.size(); ++job_id) {
    const auto& job = jobs_data[job_id];
    TaskID key = std::make_tuple(job_id, job.size() - 1);
    ends.push_back(all_tasks[key].end);
  }
  cp_model.AddMaxEquality(obj_var, ends);
  cp_model.Minimize(obj_var);
  // [END objective]

  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // [START print_solution]
  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    LOG(INFO) << "Solution:";
    // create one list of assigned tasks per machine.
    struct AssignedTaskType {
      int job_id;
      int task_id;
      int64_t start;
      int64_t duration;

      bool operator<(const AssignedTaskType& rhs) const {
        return std::tie(this->start, this->duration) <
               std::tie(rhs.start, rhs.duration);
      }
    };

    std::map<int64_t, std::vector<AssignedTaskType>> assigned_jobs;
    for (int job_id = 0; job_id < jobs_data.size(); ++job_id) {
      const auto& job = jobs_data[job_id];
      for (int task_id = 0; task_id < job.size(); ++task_id) {
        const auto [machine, duration] = job[task_id];
        TaskID key = std::make_tuple(job_id, task_id);
        int64_t start = SolutionIntegerValue(response, all_tasks[key].start);
        assigned_jobs[machine].push_back(
            AssignedTaskType{/*.job_id=*/job_id,
                             /*.task_id=*/task_id,
                             /*.start=*/start,
                             /*.duration=*/duration});
      }
    }

    // Create per machine output lines.
    std::string output = "";
    for (const auto machine : all_machines) {
      // Sort by starting time.
      std::sort(assigned_jobs[machine].begin(), assigned_jobs[machine].end());
      std::string sol_line_tasks = "Machine " + std::to_string(machine) + ": ";
      std::string sol_line = "           ";

      for (const auto& assigned_task : assigned_jobs[machine]) {
        std::string name = absl::StrFormat(
            "job_%d_task_%d", assigned_task.job_id, assigned_task.task_id);
        // Add spaces to output to align columns.
        sol_line_tasks += absl::StrFormat("%-15s", name);

        int64_t start = assigned_task.start;
        int64_t duration = assigned_task.duration;
        std::string sol_tmp =
            absl::StrFormat("[%i,%i]", start, start + duration);
        // Add spaces to output to align columns.
        sol_line += absl::StrFormat("%-15s", sol_tmp);
      }
      output += sol_line_tasks + "\n";
      output += sol_line + "\n";
    }
    // Finally print the solution found.
    LOG(INFO) << "Optimal Schedule Length: " << response.objective_value();
    LOG(INFO) << "\n" << output;
  } else {
    LOG(INFO) << "No solution found.";
  }
  // [END print_solution]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::MinimalJobshopSat();
  return EXIT_SUCCESS;
}
// [END program]
