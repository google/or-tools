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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/join.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"
#include "examples/cpp/flexible_jobshop.h"
#include "examples/cpp/jobshop.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_solver.h"

DEFINE_string(input, "", "Jobshop data file name.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
DEFINE_bool(use_boolean_precedences, false,
            "Whether we create Boolean variables for all the possible "
            "precedences between tasks on the same machine, or not.");

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

  // This is only used with --nouse_boolean_precedences.
  std::vector<IntegerVariable> decision_variables;

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
      const IntervalVariable interval = model.Add(
          NewIntervalWithVariableSize(0, horizon, min_duration, max_duration));
      decision_variables.push_back(model.Get(StartVar(interval)));

      // Chain the task belonging to the same job.
      if (previous_interval != kNoIntervalVariable) {
        model.Add(LowerOrEqual(model.Get(EndVar(previous_interval)),
                               model.Get(StartVar(interval))));
      }
      previous_interval = interval;

      if (num_alternatives == 1) {
        machine_to_intervals[task.machines[0]].push_back(interval);
      } else {
        std::vector<IntervalVariable> alternatives;
        for (int i = 0; i < num_alternatives; ++i) {
          const Literal is_present(model.Add(NewBooleanVariable()), true);
          const IntervalVariable alternative = model.Add(
              NewOptionalInterval(0, horizon, task.durations[i], is_present));
          alternatives.push_back(alternative);
          machine_to_intervals[task.machines[i]].push_back(alternative);
        }
        model.Add(IntervalWithAlternatives(interval, alternatives));
      }
    }

    // The makespan will be greater than the end of each job.
    model.Add(LowerOrEqual(model.Get(EndVar(previous_interval)), makespan));
  }

  // Add all the potential precedences between tasks on the same machine.
  for (int m = 0; m < num_machines; ++m) {
    if (FLAGS_use_boolean_precedences) {
      model.Add(DisjunctiveWithBooleanPrecedences(machine_to_intervals[m]));
    } else {
      model.Add(Disjunctive(machine_to_intervals[m]));
    }
  }

  LOG(INFO) << "#machines:" << num_machines;
  LOG(INFO) << "#jobs:" << tasks_per_job.size();
  LOG(INFO) << "#tasks:" << model.Get<IntervalsRepository>()->NumIntervals();

  if (FLAGS_use_boolean_precedences) {
    // We disable the lazy encoding in this case.
    decision_variables.clear();
  }
  MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      /*log_info=*/true, makespan,
      FirstUnassignedVarAtItsMinHeuristic(decision_variables, &model),
      /*feasible_solution_observer=*/
      [makespan](const Model& model) {
        LOG(INFO) << "Makespan " << model.Get(LowerBound(makespan));
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

  // Read a flexible/normal job shop problem based on the file extension.
  int new_task_id = 0;
  int horizon = 0;
  std::vector<std::vector<Task>> data;
  if (strings::EndsWith(FLAGS_input, ".fjs")) {
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
  return EXIT_SUCCESS;
}
