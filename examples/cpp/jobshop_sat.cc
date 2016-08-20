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

#include <math.h>
#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/timer.h"
#include "google/protobuf/text_format.h"
#include "base/join.h"
#include "base/strutil.h"
#include "cpp/flexible_jobshop.h"
#include "cpp/jobshop.h"
#include "sat/disjunctive.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/optimization.h"
#include "sat/precedences.h"
#include "sat/sat_solver.h"

DEFINE_string(input, "", "Jobshop data file name.");
DEFINE_string(params, "", "Sat parameters in text proto format.");

namespace {
struct Task {
  int id;   // dense unique id in [1, num_tasks + 1)
  int job;  // dense unique id in [0, num_jobs)

  // The possible alternative to run this task.
  std::vector<int> machines;
  std::vector<int> durations;
};
struct MachineTask {
  int id;  // The task id (note that it is shared between alternative tasks).
  int job;
  int duration;

  // -1 if no alternative or the index of the Boolean variable indicating if
  //  this alternative was taken.
  int alternative_var;
};
}  // namespace

namespace operations_research {
namespace sat {

// Solve a flexible/normal JobShop scheduling problem using SAT.
void Solve(const std::vector<std::vector<Task>>& tasks_per_job, int horizon) {
  int num_machines = 0;
  for (const std::vector<Task>& tasks : tasks_per_job) {
    for (const Task& task : tasks) {
      for (const int machine : task.machines) {
        num_machines = std::max(num_machines, machine + 1);
      }
    }
  }

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const IntegerVariable makespan = model.Add(NewIntegerVariable(0, horizon));
  std::vector<std::vector<IntervalVariable>> machine_to_intervals(num_machines);
  for (const std::vector<Task>& tasks : tasks_per_job) {
    IntervalVariable previous_interval = kNoIntervalVariable;
    for (const Task& task : tasks) {
      const int num_alternatives = task.machines.size();
      CHECK_EQ(num_alternatives, task.durations.size());

      // Add the "main" task interval. It will englobe all the alternative ones
      // if there is many, or be a normal task otherwise.
      int min_duration = task.durations[0];
      int max_duration = task.durations[0];
      for (int i = 0; i < num_alternatives; ++i) {
        min_duration = std::min(min_duration, task.durations[i]);
        max_duration = std::max(max_duration, task.durations[i]);
      }
      const IntervalVariable interval =
          model.Add(NewIntervalWithVariableSize(min_duration, max_duration));

      // Chain the task belonging to the same job.
      if (previous_interval != kNoIntervalVariable) {
        model.Add(EndBeforeStart(previous_interval, interval));
      }
      previous_interval = interval;

      if (num_alternatives == 1) {
        machine_to_intervals[task.machines[0]].push_back(interval);
      } else {
        std::vector<IntervalVariable> alternatives;
        for (int i = 0; i < num_alternatives; ++i) {
          const Literal is_present(model.Add(NewBooleanVariable()), true);
          const IntervalVariable alternative =
              model.Add(NewOptionalInterval(task.durations[i], is_present));
          alternatives.push_back(alternative);
          machine_to_intervals[task.machines[i]].push_back(alternative);
        }
        model.Add(IntervalWithAlternatives(interval, alternatives));
      }
    }

    // The makespan will be greater than the end of each job.
    model.Add(EndBefore(previous_interval, makespan));
  }

  // Add all the potential precedences between tasks on the same machine.
  for (int m = 0; m < num_machines; ++m) {
    model.Add(DisjunctiveWithBooleanPrecedences(machine_to_intervals[m]));
  }

  LOG(INFO) << "#machines:" << num_machines;
  LOG(INFO) << "#jobs:" << tasks_per_job.size();
  LOG(INFO) << "#tasks:" << model.Get<IntervalsRepository>()->NumIntervals();

  MinimizeIntegerVariableWithLinearScan(
      makespan,
      /*feasible_solution_observer=*/
      [makespan](const Model& model) {
        const IntegerTrail* integer_trail = model.Get<IntegerTrail>();
        LOG(INFO) << "Makespan " << integer_trail->LowerBound(makespan);
      },
      &model);
}

}  // namespace sat

void LoadAndSolve() {
  // Read a flexible/normal job shop problem based on the file extension.
  int new_task_id = 0;
  int horizon = 0;
  std::vector<std::vector<Task>> data;
  if (HasSuffixString(FLAGS_input, ".fjs")) {
    LOG(INFO) << "Reading flexible jobshop instance '" << FLAGS_input << "'.";
    operations_research::FlexibleJobShopData fjs;
    fjs.Load(FLAGS_input);
    horizon = fjs.horizon();
    data.resize(fjs.job_count());
    if (fjs.job_count() == 0) {
      LOG(FATAL) << "No jobs in '" << FLAGS_input << "'.";
    }
    for (int i = 0; i < fjs.job_count(); ++i) {
      for (const operations_research::FlexibleJobShopData::Task& task :
           fjs.TasksOfJob(i)) {
        CHECK_EQ(task.machines.size(), task.durations.size());
        data[i].push_back(
            {new_task_id++, task.job_id, task.machines, task.durations});
      }
    }
  } else {
    operations_research::JobShopData jssp;
    jssp.Load(FLAGS_input);
    horizon = jssp.horizon();
    data.resize(jssp.job_count());
    if (jssp.job_count() == 0) {
      LOG(FATAL) << "No jobs in '" << FLAGS_input << "'.";
    }
    for (int i = 0; i < jssp.job_count(); ++i) {
      for (const operations_research::JobShopData::Task& task :
           jssp.TasksOfJob(i)) {
        data[i].push_back(
            {new_task_id++, task.job_id, {task.machine_id}, {task.duration}});
      }
    }
  }

  // Solve it.
  operations_research::sat::Solve(data, horizon);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  operations_research::LoadAndSolve();
  return EXIT_SUCCESS;
}
