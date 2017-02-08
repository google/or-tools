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
#include "sat/cumulative.h"
#include "sat/disjunctive.h"
#include "sat/integer_expr.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/optimization.h"
#include "sat/precedences.h"
#include "util/rcpsp_parser.h"

DEFINE_string(input, "", "Input file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");

namespace operations_research {
namespace sat {
int ComputeNaiveHorizon(const RcpspParser& parser) {
  int horizon = 0;
  for (const RcpspParser::Task& task : parser.tasks()) {
    int max_duration = 0;
    for (const RcpspParser::Recipe& recipe : task.recipes) {
      max_duration = std::max(max_duration, recipe.duration);
    }
    horizon += max_duration;
  }
  return horizon;
}

int64 VectorSum(const std::vector<int64>& v) {
  int64 res = 0;
  for (const int64 c : v) res += c;
  return res;
}

void LoadAndSolve(const std::string& file_name) {
  RcpspParser parser;
  CHECK(parser.LoadFile(file_name));
  LOG(INFO) << "Successfully read '" << file_name << "'.";
  const std::string problem_type =
      parser.is_rcpsp_max()
          ? (parser.is_resource_investment() ? "Resource investment/Max"
                                             : "RCPSP/Max")
          : (parser.is_resource_investment() ? "Resource investment" : "RCPSP");
  LOG(INFO) << problem_type << " problem with " << parser.resources().size()
            << " resources, and " << parser.tasks().size() << " tasks.";

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const int num_tasks = parser.tasks().size();
  const int num_resources = parser.resources().size();
  const int horizon =
      parser.deadline() == -1
          ? (parser.horizon() == -1 ? ComputeNaiveHorizon(parser)
                                    : parser.horizon())
          : parser.deadline();
  LOG(INFO) << "Horizon = " << horizon;

  std::vector<std::vector<IntervalVariable>> intervals_per_resources(
      num_resources);
  std::vector<std::vector<IntegerVariable>> demands_per_resources(
      num_resources);
  std::vector<std::vector<int64>> consumptions_per_resources(num_resources);
  std::vector<std::vector<IntegerVariable>> presences_per_resources(
      num_resources);
  std::vector<IntegerVariable> task_starts(num_tasks);
  std::vector<IntegerVariable> task_ends(num_tasks);
  std::vector<std::vector<IntervalVariable>> alternatives_per_task(num_tasks);

  for (int t = 1; t < num_tasks - 1; ++t) {  // Ignore both sentinels.
    const RcpspParser::Task& task = parser.tasks()[t];

    if (task.recipes.size() == 1) {
      // Create the master interval.
      const RcpspParser::Recipe& recipe = task.recipes.front();
      CHECK_EQ(num_resources, recipe.demands_per_resource.size());
      const IntervalVariable interval =
          model.Add(NewInterval(0, horizon, recipe.duration));
      task_starts[t] = model.Get(StartVar(interval));
      task_ends[t] = model.Get(EndVar(interval));
      alternatives_per_task[t].push_back(interval);

      // Add intervals to the resources.
      for (int r = 0; r < num_resources; ++r) {
        const int demand = recipe.demands_per_resource[r];
        if (demand == 0) continue;

        consumptions_per_resources[r].push_back(demand);
        if (parser.resources()[r].renewable) {
          intervals_per_resources[r].push_back(interval);
          demands_per_resources[r].push_back(
              model.Add(ConstantIntegerVariable(demand)));
        } else {
          presences_per_resources[r].push_back(
              model.Add(ConstantIntegerVariable(1)));
        }
      }
    } else {
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
        alternatives_per_task[t].push_back(interval);
        const IntegerVariable presence_var =
            model.Add(NewIntegerVariableFromLiteral(is_present));

        for (int r = 0; r < num_resources; ++r) {
          const int demand = recipe.demands_per_resource[r];
          if (demand == 0) continue;

          consumptions_per_resources[r].push_back(demand);
          if (parser.resources()[r].renewable) {
            intervals_per_resources[r].push_back(interval);
            demands_per_resources[r].push_back(
                model.Add(ConstantIntegerVariable(demand)));
          } else {
            presences_per_resources[r].push_back(presence_var);
          }
        }
      }

      // Fill in the master interval.
      CHECK_GT(alternatives_per_task[t].size(), 1);
      const IntervalVariable master = model.Add(
          NewIntervalWithVariableSize(0, horizon, min_size, max_size));
      model.Add(IntervalWithAlternatives(master, alternatives_per_task[t]));
      task_starts[t] = model.Get(StartVar(master));
      task_ends[t] = model.Get(EndVar(master));
    }
  }

  // Create the makespan variable.
  const IntegerVariable makespan = model.Add(NewIntegerVariable(0, horizon));

  // Add precedences.
  if (parser.is_rcpsp_max()) {
    for (int t = 1; t < num_tasks - 1; ++t) {
      const RcpspParser::Task& task = parser.tasks()[t];
      const int num_modes = task.recipes.size();

      for (int s = 0; s < task.successors.size(); ++s) {
        const int n = task.successors[s];
        const std::vector<std::vector<int>>& delay_matrix = task.delays[s];
        CHECK_EQ(delay_matrix.size(), num_modes);
        const int num_other_modes = parser.tasks()[n].recipes.size();

        for (int m1 = 0; m1 < num_modes; ++m1) {
          const IntegerVariable s1 =
              model.Get(StartVar(alternatives_per_task[t][m1]));
          CHECK_EQ(num_other_modes, delay_matrix[m1].size());

          if (n == num_tasks - 1) {
            CHECK_EQ(1, num_other_modes);
            const int delay = delay_matrix[m1][0];
            model.Add(LowerOrEqualWithOffset(s1, makespan, delay));
          } else {
            for (int m2 = 0; m2 < num_other_modes; ++m2) {
              const int delay = delay_matrix[m1][m2];
              const IntegerVariable s2 =
                  model.Get(StartVar(alternatives_per_task[n][m2]));
              model.Add(LowerOrEqualWithOffset(s1, s2, delay));
            }
          }
        }
      }
    }
  } else {
    for (int t = 1; t < num_tasks - 1; ++t) {
      for (int n : parser.tasks()[t].successors) {
        if (n == num_tasks - 1) {
          // By construction, we do not need to add the precedence
          // constraint between all tasks and the makespan, just the one
          // described in the problem.
          model.Add(LowerOrEqual(task_ends[t], makespan));
        } else {
          model.Add(LowerOrEqual(task_ends[t], task_starts[n]));
        }
      }
    }
  }

  std::vector<int> weights;
  std::vector<IntegerVariable> capacities;

  // Create resources.
  for (int r = 0; r < num_resources; ++r) {
    const RcpspParser::Resource& res = parser.resources()[r];
    const int64 c = res.max_capacity == -1
                        ? VectorSum(consumptions_per_resources[r])
                        : res.max_capacity;

    const IntegerVariable capacity = model.Add(ConstantIntegerVariable(c));
    model.Add(Cumulative(intervals_per_resources[r], demands_per_resources[r],
                         capacity));
    if (parser.is_resource_investment()) {
      const IntegerVariable capacity = model.Add(NewIntegerVariable(0, c));
      model.Add(Cumulative(intervals_per_resources[r], demands_per_resources[r],
                           capacity));
      capacities.push_back(capacity);
      weights.push_back(res.unit_cost);
    } else if (res.renewable) {
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

  // Create objective var.
  IntegerVariable objective_var;
  if (parser.is_resource_investment()) {
    objective_var = model.Add(NewWeightedSum(weights, capacities));
  } else {
    objective_var = makespan;
  }

  // Search.
  std::vector<IntegerVariable> decision_variables;
  for (int t = 1; t < num_tasks - 1; ++t) {
    decision_variables.push_back(task_starts[t]);
  }

  MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      /*log_info=*/true, objective_var, decision_variables,
      /*feasible_solution_observer=*/
      [objective_var](const Model& model) {
        LOG(INFO) << "Objective " << model.Get(LowerBound(objective_var));
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
