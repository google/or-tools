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
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/timer.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/join.h"
#include "ortools/base/split.h"
#include "ortools/base/strutil.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/util/filelineiter.h"

DEFINE_string(input, "examples/data/weighted_tardiness/wt40.txt",
              "wt data file name.");
DEFINE_int32(size, 40, "Size of the problem in the wt file.");
DEFINE_int32(n, 28, "1-based instance number in the wt file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
DEFINE_bool(use_boolean_precedences, false,
            "Whether we create Boolean variables for all the possible "
            "precedences between tasks on the same machine, or not.");
DEFINE_int32(upper_bound, -1, "If positive, look for a solution <= this.");

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

  // An simple heuristic solution: We choose the tasks from last to first, and
  // always take the one with smallest cost.
  std::vector<bool> is_taken(num_tasks, false);
  int heuristic_bound = 0;
  int end = horizon;
  for (int i = 0; i < num_tasks; ++i) {
    int next_task = -1;
    int64 next_cost;
    for (int j = 0; j < num_tasks; ++j) {
      if (is_taken[j]) continue;
      const int64 cost = weights[j] * std::max(0, end - due_dates[j]);
      if (next_task == -1 || cost < next_cost) {
        next_task = j;
        next_cost = cost;
      }
    }
    CHECK_NE(-1, next_task);
    is_taken[next_task] = true;
    end -= durations[next_task];
    heuristic_bound += next_cost;
  }
  LOG(INFO) << "num_tasks: " << num_tasks;
  LOG(INFO) << "The time horizon is " << horizon;
  LOG(INFO) << "Trival cost bound = " << heuristic_bound;

  // Create the model.
  Model model;
  std::vector<IntegerVariable> decision_vars;
  std::vector<IntervalVariable> tasks(num_tasks);
  std::vector<IntegerVariable> tardiness_vars(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    tasks[i] = model.Add(NewInterval(0, horizon, durations[i]));
    if (due_dates[i] == 0) {
      tardiness_vars[i] = model.Get(EndVar(tasks[i]));
    } else {
      tardiness_vars[i] =
          model.Add(NewIntegerVariable(0, std::max(0, horizon - due_dates[i])));
      model.Add(LowerOrEqualWithOffset(model.Get(EndVar(tasks[i])),
                                       tardiness_vars[i], -due_dates[i]));
    }

    // Experiments showed that the heuristic of choosing first the task that
    // comes last (because of the NegationOf()) works a lot better. This make
    // sense because these are the task with the most influence on the cost.
    decision_vars.push_back(NegationOf(model.Get(StartVar(tasks[i]))));
  }
  if (FLAGS_use_boolean_precedences) {
    model.Add(DisjunctiveWithBooleanPrecedences(tasks));
  } else {
    model.Add(Disjunctive(tasks));
  }

  // Set a known upper bound (or use the flag). This has a bigger impact than
  // can be expected at first:
  // - It avoid spending time finding not so good solution.
  // - More importantly, because we lazily create the associated Boolean
  //   variables, we end up creating less of them, and that speed up the search
  //   for the optimal and the proof of optimality.
  //
  // Note however than for big problem, this will drastically augment the time
  // to get a first feasible solution (but then the heuristic gave one to us).
  const IntegerVariable objective_var =
      model.Add(NewWeightedSum(weights, tardiness_vars));
  if (FLAGS_upper_bound >= 0) {
    model.Add(LowerOrEqual(objective_var, FLAGS_upper_bound));
  } else {
    model.Add(LowerOrEqual(objective_var, heuristic_bound));
  }

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
        // If two jobs have exactly the same specs, we don't add both
        // precedences!
        if (due_dates[i] == due_dates[j] && durations[i] == durations[j] &&
            weights[i] == weights[j] && i > j) {
          continue;
        }

        ++num_added_precedences;
        model.Add(LowerOrEqual(model.Get(EndVar(tasks[i])),
                               model.Get(StartVar(tasks[j]))));
      }
    }
  }
  LOG(INFO) << "Added " << num_added_precedences
            << " precedences that will not affect the optimal solution value.";

  if (FLAGS_use_boolean_precedences) {
    // We disable the lazy encoding in this case.
    decision_vars.clear();
  }

  // Solve it.
  //
  // Note that we only fully instanciate the start/end and only look at the
  // lower bound for the objective and the tardiness variables.
  model.Add(NewSatParameters(FLAGS_params));
  MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      /*log_info=*/true, objective_var,
      /*next_decision=*/
      UnassignedVarWithLowestMinAtItsMinHeuristic(decision_vars, &model),
      /*feasible_solution_observer=*/
      [&](const Model& model) {
        const int64 objective = model.Get(LowerBound(objective_var));
        LOG(INFO) << "Cost " << objective;

        // Debug code.
        {
          int64 tardiness_objective = 0;
          for (int i = 0; i < num_tasks; ++i) {
            tardiness_objective +=
                weights[i] *
                std::max(0ll, model.Get(Value(model.Get(EndVar(tasks[i])))) -
                                  due_dates[i]);
          }
          CHECK_EQ(objective, tardiness_objective);

          tardiness_objective = 0;
          for (int i = 0; i < num_tasks; ++i) {
            tardiness_objective +=
                weights[i] * model.Get(LowerBound(tardiness_vars[i]));
          }
          CHECK_EQ(objective, tardiness_objective);
        }

        // Print the current solution.
        std::vector<IntervalVariable> sorted_tasks = tasks;
        std::sort(sorted_tasks.begin(), sorted_tasks.end(),
                  [&model](IntervalVariable v1, IntervalVariable v2) {
                    return model.Get(Value(model.Get(StartVar(v1)))) <
                           model.Get(Value(model.Get(StartVar(v2))));
                  });
        std::string solution = "0";
        int end = 0;
        for (const IntervalVariable v : sorted_tasks) {
          const int64 cost = weights[v.value()] *
                             model.Get(LowerBound(tardiness_vars[v.value()]));
          solution += StringPrintf("| #%d ", v.value());
          if (cost > 0) {
            // Display the cost in red.
            solution += StringPrintf("\033[1;31m(+%lld) \033[0m", cost);
          }
          solution +=
              StringPrintf("|%lld", model.Get(Value(model.Get(EndVar(v)))));
          CHECK_EQ(end, model.Get(Value(model.Get(StartVar(v)))));
          end += durations[v.value()];
          CHECK_EQ(end, model.Get(Value(model.Get(EndVar(v)))));
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
    entries = strings::Split(line, ' ', strings::SkipEmpty());
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
