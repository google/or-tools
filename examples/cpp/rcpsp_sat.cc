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

#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/timer.h"
#include "sat/disjunctive.h"
#include "sat/integer_expr.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/optimization.h"
#include "sat/precedences.h"
#include "sat/timetabling.h"
#include "util/rcpsp_parser.h"

DEFINE_string(input, "", "Input file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");

namespace operations_research {
namespace sat {

void LoadAndSolve(const std::string& file_name) {
  RcpspParser parser;
  CHECK(parser.LoadFile(file_name));
  LOG(INFO) << "Successfully read '" << file_name << "'.";

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const int num_tasks = parser.tasks().size();
  const int num_resources = parser.resources().size();
  const int horizon = parser.horizon();

  std::vector<std::vector<IntervalVariable>> intervals_per_resources(
      num_resources);
  std::vector<std::vector<IntegerVariable>> demands_per_resources(
      num_resources);
  std::vector<std::vector<int64>> consumptions_per_resources(num_resources);
  std::vector<std::vector<IntegerVariable>> presences_per_resources(
      num_resources);
  std::vector<IntervalVariable> master_interval_per_task(num_tasks);

  for (int t = 1; t < num_tasks - 1; ++t) {  // Ignore both sentinels.
    const RcpspParser::Task& task = parser.tasks()[t];

    if (task.recipes.size() == 1) {
      // Create the master interval.
      const RcpspParser::Recipe& recipe = task.recipes.front();
      CHECK_EQ(num_resources, recipe.demands_per_resource.size());
      const IntervalVariable interval =
          model.Add(NewInterval(0, horizon, recipe.duration));
      master_interval_per_task[t] = interval;

      // Add intervals to the resources.
      for (int r = 0; r < num_resources; ++r) {
        const int demand = recipe.demands_per_resource[r];
        if (demand == 0) continue;

        if (parser.resources()[r].renewable) {
          intervals_per_resources[r].push_back(interval);
          demands_per_resources[r].push_back(
              model.Add(ConstantIntegerVariable(demand)));
        } else {
          consumptions_per_resources[r].push_back(demand);
          presences_per_resources[r].push_back(
              model.Add(ConstantIntegerVariable(1)));
        }
      }
    } else {
      std::vector<IntervalVariable> alternatives;
      int min_size = kint32max;
      int max_size = 0;
      for (const RcpspParser::Recipe& recipe : task.recipes) {
        CHECK_EQ(num_resources, recipe.demands_per_resource.size());
        const int duration = recipe.duration;
        min_size = std::min(min_size, duration);
        max_size = std::max(max_size, duration);
        const Literal is_present =
            Literal(model.Add(NewBooleanVariable()), true);
        const IntervalVariable interval =
            model.Add(NewOptionalInterval(0, horizon, duration, is_present));
        alternatives.push_back(interval);
        const IntegerVariable presence_var =
            model.Add(NewIntegerVariableFromLiteral(is_present));

        for (int r = 0; r < num_resources; ++r) {
          const int demand = recipe.demands_per_resource[r];
          if (demand == 0) continue;

          if (parser.resources()[r].renewable) {
            intervals_per_resources[r].push_back(interval);
            demands_per_resources[r].push_back(
                model.Add(ConstantIntegerVariable(demand)));
          } else {
            consumptions_per_resources[r].push_back(demand);
            presences_per_resources[r].push_back(presence_var);
          }
        }
      }

      // Fill in the master interval.
      CHECK_GT(alternatives.size(), 0);
      if (alternatives.size() == 1) {
        master_interval_per_task[t] = alternatives.front();
      } else {
        const IntervalVariable master = model.Add(
            NewIntervalWithVariableSize(0, horizon, min_size, max_size));
        model.Add(IntervalWithAlternatives(master, alternatives));
        master_interval_per_task[t] = master;
      }
    }
  }

  // Create the makespan variable.
  const IntegerVariable makespan = model.Add(NewIntegerVariable(0, horizon));

  // Add precedences.
  for (int t = 1; t < num_tasks - 1; ++t) {
    const IntervalVariable main = master_interval_per_task[t];
    for (int n : parser.tasks()[t].successors) {
      if (n == num_tasks - 1) {
        // By construction, we do not need to add the precedence
        // constraint between all tasks and the makespan, just the one described
        // in the problem.
        model.Add(EndBefore(main, makespan));
      } else {
        model.Add(EndBeforeStart(main, master_interval_per_task[n]));
      }
    }
  }

  // Create resources.
  for (int r = 0; r < num_resources; ++r) {
    const RcpspParser::Resource& res = parser.resources()[r];
    const int c = res.capacity;
    if (res.renewable) {
      if (intervals_per_resources[r].empty()) continue;
      const IntegerVariable capacity = model.Add(ConstantIntegerVariable(c));
      model.Add(Cumulative(intervals_per_resources[r], demands_per_resources[r],
                           capacity));

    } else {
      if (presences_per_resources[r].empty()) continue;
      model.Add(WeightedSumLowerOrEqual(presences_per_resources[r],
                                        consumptions_per_resources[r], c));
    }
  }

  // Search.
  std::vector<IntegerVariable> decision_variables;
  for (int t = 1; t < num_tasks - 1; ++t) {
    decision_variables.push_back(
        model.Get(StartVar(master_interval_per_task[t])));
  }

  MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      /*log_info=*/true, makespan, decision_variables,
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

  operations_research::sat::LoadAndSolve(FLAGS_input);
  return EXIT_SUCCESS;
}
