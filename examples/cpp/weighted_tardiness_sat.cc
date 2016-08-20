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
#include "base/split.h"
#include "base/strutil.h"
#include "base/strtoint.h"
#include "sat/disjunctive.h"
#include "sat/integer_sum.h"
#include "sat/intervals.h"
#include "sat/model.h"
#include "sat/optimization.h"
#include "sat/precedences.h"
#include "util/filelineiter.h"


DEFINE_string(input, "examples/data/weighted_tardiness/wt40.txt",
              "wt data file name.");
DEFINE_int32(size, 40, "Size of the problem in the wt file.");
DEFINE_int32(n, 28, "1-based instance number in the wt file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");

namespace operations_research {
namespace sat {

// Solve a single machine problem with weighted tardiness cost.
void Solve(const std::vector<int>& durations, const std::vector<int>& due_dates,
           const std::vector<int>& weights) {
  const int num_tasks = durations.size();
  CHECK_EQ(due_dates.size(), num_tasks);
  CHECK_EQ(weights.size(), num_tasks);

  // Display some statistics.
  int horizon = 0;
  for (int i = 0; i < num_tasks; ++i) {
    horizon += durations[i];
    LOG(INFO) << "#" << i << " duration:" << durations[i]
              << " due_date:" << due_dates[i] << " weight:" << weights[i];
  }
  int trivial_bound = 0;
  for (int i = 0; i < num_tasks; ++i) {
    trivial_bound += weights[i] * std::max(0, horizon - due_dates[i]);
  }
  LOG(INFO) << "num_tasks: " << num_tasks;
  LOG(INFO) << "The time horizon is " << horizon;
  LOG(INFO) << "Trival cost bound = " << trivial_bound;

  // Create the model.
  Model model;
  std::vector<IntervalVariable> tasks(num_tasks);
  std::vector<IntegerVariable> tardiness_vars(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    tasks[i] = model.Add(NewInterval(durations[i]));
    model.Add(LowerOrEqual(model.Get(EndVar(tasks[i])), horizon));
    if (due_dates[i] == 0) {
      tardiness_vars[i] = model.Get(EndVar(tasks[i]));
    } else {
      tardiness_vars[i] =
          model.Add(NewIntegerVariable(0, horizon - due_dates[i]));
      model.Add(
          EndBeforeWithOffset(tasks[i], tardiness_vars[i], -due_dates[i]));
    }
  }
  model.Add(DisjunctiveWithBooleanPrecedences(tasks));
  const IntegerVariable objective_var =
      model.Add(NewWeightedSum(weights, tardiness_vars));

  // Optional preprocessing: add precedences that don't change the optimal
  // solution value.
  //
  // Proof: in any schedule, if such precendece between task A and B is not
  // satisfied, then it is always better (or the same) to swap A and B. This is
  // because the tasks between A and B will be completed earlier (because the
  // duration of A is smaller), and the cost of the swap itself is also smaller.
  int num_added_precedences = 0;
  for (int i = 0; i < num_tasks; ++i) {
    for (int j = 0; j < num_tasks; ++j) {
      if (i == j) continue;
      if (due_dates[i] <= due_dates[j] && durations[i] <= durations[j] &&
          weights[i] >= weights[j]) {
        ++num_added_precedences;
        model.Add(EndBeforeStart(tasks[i], tasks[j]));
      }
    }
  }
  LOG(INFO) << "Added " << num_added_precedences
            << " precedences that will not affect the optimal solution value.";

  // Solve it.
  model.Add(NewSatParameters(FLAGS_params));
  MinimizeIntegerVariableWithLinearScan(
      objective_var,
      /*feasible_solution_observer=*/
      [&](const Model& model) {
        const IntegerTrail* integer_trail = model.Get<IntegerTrail>();
        const IntervalsRepository* intervals = model.Get<IntervalsRepository>();

        const int objective = integer_trail->LowerBound(objective_var);
        LOG(INFO) << "Cost " << objective;

        // Debug code.
        {
          int tardiness_objective = 0;
          for (int i = 0; i < num_tasks; ++i) {
            tardiness_objective +=
                weights[i] * std::max(0, integer_trail->LowerBound(
                                             intervals->EndVar(tasks[i])) -
                                             due_dates[i]);
          }
          CHECK_EQ(objective, tardiness_objective);

          tardiness_objective = 0;
          for (int i = 0; i < num_tasks; ++i) {
            tardiness_objective +=
                weights[i] * integer_trail->LowerBound(tardiness_vars[i]);
          }
          CHECK_EQ(objective, tardiness_objective);
        }

        // Print the current solution.
        std::vector<IntervalVariable> sorted_tasks = tasks;
        std::sort(sorted_tasks.begin(), sorted_tasks.end(),
                  [integer_trail, &intervals](IntervalVariable v1,
                                              IntervalVariable v2) {
                    return integer_trail->LowerBound(intervals->StartVar(v1)) <
                           integer_trail->LowerBound(intervals->StartVar(v2));
                  });
        std::string solution = "0";
        int end = 0;
        for (const IntervalVariable v : sorted_tasks) {
          const int cost = weights[v.value()] *
                           integer_trail->LowerBound(tardiness_vars[v.value()]);
          solution += StringPrintf("| #%d ", v.value());
          if (cost > 0) {
            // Display the cost in red.
            solution += StringPrintf("\033[1;31m(+%d) \033[0m", cost);
          }
          solution += StringPrintf(
              "|%d", integer_trail->LowerBound(intervals->EndVar(v)));
          CHECK_EQ(end, integer_trail->LowerBound(intervals->StartVar(v)));
          end += durations[v.value()];
          CHECK_EQ(end, integer_trail->LowerBound(intervals->EndVar(v)));
        }
        LOG(INFO) << "solution: " << solution;
      },
      &model);
}

}  // namespace sat

void LoadAndSolve() {
  std::vector<int> numbers;
  std::vector<std::string> entries;
  for (const std::string& line : operations_research::FileLines(FLAGS_input)) {
    entries = strings::Split(line, " ", strings::SkipEmpty());
    for (const std::string& entry : entries) {
      numbers.push_back(atoi32(entry));
    }
  }

  const int instance_size = FLAGS_size * 3;
  LOG(INFO) << numbers.size() << " numbers in '" << FLAGS_input << "'.";
  LOG(INFO) << "This correspond to " << numbers.size() / instance_size
            << " instances of size " << FLAGS_size;
  LOG(INFO) << "Loading instance #" << FLAGS_n;
  CHECK_GE(FLAGS_n, 0);
  CHECK_LE(FLAGS_n * instance_size, numbers.size());

  // The order in a wt file is: duration, tardiness weights and then due_dates.
  int index = (FLAGS_n - 1) * instance_size;
  std::vector<int> durations;
  for (int j = 0; j < FLAGS_size; ++j) durations.push_back(numbers[index++]);
  std::vector<int> weights;
  for (int j = 0; j < FLAGS_size; ++j) weights.push_back(numbers[index++]);
  std::vector<int> due_dates;
  for (int j = 0; j < FLAGS_size; ++j) due_dates.push_back(numbers[index++]);

  sat::Solve(durations, due_dates, weights);
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
