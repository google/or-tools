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

#include <math.h>

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/filelineiter.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

ABSL_FLAG(std::string, input, "examples/data/weighted_tardiness/wt40.txt",
          "wt data file name.");
ABSL_FLAG(int, size, 40, "Size of the problem in the wt file.");
ABSL_FLAG(int, n, 28, "1-based instance number in the wt file.");
ABSL_FLAG(std::string, params, "", "Sat parameters in text proto format.");
ABSL_FLAG(int, upper_bound, -1, "If positive, look for a solution <= this.");

namespace operations_research {
namespace sat {

// Solve a single machine problem with weighted tardiness cost.
void Solve(const std::vector<int64_t>& durations,
           const std::vector<int64_t>& due_dates,
           const std::vector<int64_t>& weights) {
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
  int64_t heuristic_bound = 0;
  int64_t end = horizon;
  for (int i = 0; i < num_tasks; ++i) {
    int next_task = -1;
    int64_t next_cost;
    for (int j = 0; j < num_tasks; ++j) {
      if (is_taken[j]) continue;
      const int64_t cost =
          weights[j] * std::max<int64_t>(0, end - due_dates[j]);
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
  CpModelBuilder cp_model;

  std::vector<IntervalVar> task_intervals(num_tasks);
  std::vector<IntVar> task_starts(num_tasks);
  std::vector<LinearExpr> tardiness_expressions(num_tasks);
  LinearExpr objective;

  for (int i = 0; i < num_tasks; ++i) {
    task_starts[i] = cp_model.NewIntVar(Domain(0, horizon - durations[i]));
    task_intervals[i] =
        cp_model.NewFixedSizeIntervalVar(task_starts[i], durations[i]);

    if (due_dates[i] == 0) {
      tardiness_expressions[i] = task_starts[i] + durations[i];
    } else {
      tardiness_expressions[i] = cp_model.NewIntVar(
          Domain(0, std::max<int64_t>(0, horizon - due_dates[i])));

      // tardiness_vars >= end - due_date
      cp_model.AddGreaterOrEqual(tardiness_expressions[i],
                                 task_starts[i] + durations[i] - due_dates[i]);
    }
    objective += weights[i] * tardiness_expressions[i];
  }

  // Decision heuristic. Note that we don't instantiate all the variables. As a
  // consequence, in the values returned by the solution observer for the
  // non-fully instantiated variable will be the variable lower bounds after
  // propagation.
  cp_model.AddDecisionStrategy(task_starts,
                               DecisionStrategyProto::CHOOSE_HIGHEST_MAX,
                               DecisionStrategyProto::SELECT_MAX_VALUE);

  cp_model.AddNoOverlap(task_intervals);

  // TODO(user): We can't set an objective upper bound with the current cp_model
  // interface, so we can't use heuristic or absl::GetFlag(FLAGS_upper_bound)
  // here. The best is probably to provide a "solution hint" instead.
  //
  // Set a known upper bound (or use the flag). This has a bigger impact than
  // can be expected at first:
  // - It avoid spending time finding not so good solution.
  // - More importantly, because we lazily create the associated Boolean
  //   variables, we end up creating less of them, and that speed up the search
  //   for the optimal and the proof of optimality.
  //
  // Note however than for big problem, this will drastically augment the time
  // to get a first feasible solution (but then the heuristic gave one to us).
  cp_model.Minimize(objective);

  // Optional preprocessing: add precedences that don't change the optimal
  // solution value.
  //
  // Proof: in any schedule, if such precedence between task A and B is not
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
        cp_model.AddLessOrEqual(task_starts[i] + durations[i], task_starts[j]);
      }
    }
  }
  LOG(INFO) << "Added " << num_added_precedences
            << " precedences that will not affect the optimal solution value.";

  // Solve it.
  //
  // Note that we only fully instantiate the start/end and only look at the
  // lower bound for the objective and the tardiness variables.
  Model model;
  model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  model.GetOrCreate<SatParameters>()->set_log_search_progress(true);
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    // Note that we compute the "real" cost here and do not use the tardiness
    // variables. This is because in the core based approach, the tardiness
    // variable might be fixed before the end date, and we just have a >=
    // relation.

    int64_t objective = 0;
    for (int i = 0; i < num_tasks; ++i) {
      const int64_t end =
          SolutionIntegerValue(r, task_starts[i]) + durations[i];
      objective += weights[i] * std::max<int64_t>(0, end - due_dates[i]);
    }
    LOG(INFO) << "Cost " << objective;

    // Print the current solution.
    std::vector<int> sorted_tasks(num_tasks);
    std::iota(sorted_tasks.begin(), sorted_tasks.end(), 0);
    std::sort(sorted_tasks.begin(), sorted_tasks.end(), [&](int v1, int v2) {
      return SolutionIntegerValue(r, task_starts[v1]) <
             SolutionIntegerValue(r, task_starts[v2]);
    });
    std::string solution = "0";
    int end = 0;
    for (const int i : sorted_tasks) {
      const int64_t cost =
          weights[i] * SolutionIntegerValue(r, tardiness_expressions[i]);
      absl::StrAppend(&solution, "| #", i, " ");
      if (cost > 0) {
        // Display the cost in red.
        absl::StrAppend(&solution, "\033[1;31m(+", cost, ") \033[0m");
      }
      absl::StrAppend(&solution, "|",
                      SolutionIntegerValue(r, task_starts[i]) + durations[i]);
      end += durations[i];
    }
    LOG(INFO) << "solution: " << solution;
  }));

  // Solve.
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
}

void ParseAndSolve() {
  std::vector<int> numbers;
  std::vector<std::string> entries;
  for (const std::string& line : FileLines(absl::GetFlag(FLAGS_input))) {
    entries = absl::StrSplit(line, ' ', absl::SkipEmpty());
    for (const std::string& entry : entries) {
      numbers.push_back(0);
      CHECK(absl::SimpleAtoi(entry, &numbers.back()));
    }
  }

  const int instance_size = absl::GetFlag(FLAGS_size) * 3;
  LOG(INFO) << numbers.size() << " numbers in '" << absl::GetFlag(FLAGS_input)
            << "'.";
  LOG(INFO) << "This correspond to " << numbers.size() / instance_size
            << " instances of size " << absl::GetFlag(FLAGS_size);
  LOG(INFO) << "Loading instance #" << absl::GetFlag(FLAGS_n);
  CHECK_GE(absl::GetFlag(FLAGS_n), 0);
  CHECK_LE(absl::GetFlag(FLAGS_n) * instance_size, numbers.size());

  // The order in a wt file is: duration, tardiness weights and then due_dates.
  int index = (absl::GetFlag(FLAGS_n) - 1) * instance_size;
  std::vector<int64_t> durations;
  for (int j = 0; j < absl::GetFlag(FLAGS_size); ++j)
    durations.push_back(numbers[index++]);
  std::vector<int64_t> weights;
  for (int j = 0; j < absl::GetFlag(FLAGS_size); ++j)
    weights.push_back(numbers[index++]);
  std::vector<int64_t> due_dates;
  for (int j = 0; j < absl::GetFlag(FLAGS_size); ++j)
    due_dates.push_back(numbers[index++]);

  Solve(durations, due_dates, weights);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  operations_research::sat::ParseAndSolve();
  return EXIT_SUCCESS;
}
