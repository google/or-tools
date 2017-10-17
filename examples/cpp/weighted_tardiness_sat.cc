// Copyright 2010-2017 Google
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
#include <numeric>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/join.h"
#include "ortools/base/split.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/strutil.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/util/filelineiter.h"

DEFINE_string(input, "examples/data/weighted_tardiness/wt40.txt",
              "wt data file name.");
DEFINE_int32(size, 40, "Size of the problem in the wt file.");
DEFINE_int32(n, 28, "1-based instance number in the wt file.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
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
  CpModelProto cp_model;
  cp_model.set_name("weighted_tardiness");
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    const int index = cp_model.variables_size();
    IntegerVariableProto* var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };
  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    ConstraintProto* ct = cp_model.add_constraints();
    ct->mutable_interval()->set_start(start);
    ct->mutable_interval()->set_size(duration);
    ct->mutable_interval()->set_end(end);
    return index;
  };

  std::vector<int> tasks_interval(num_tasks);
  std::vector<int> tasks_start(num_tasks);
  std::vector<int> tasks_duration(num_tasks);
  std::vector<int> tasks_end(num_tasks);
  std::vector<int> tardiness_vars(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    tasks_start[i] = new_variable(0, horizon - durations[i]);
    tasks_duration[i] = new_variable(durations[i], durations[i]);
    tasks_end[i] = new_variable(durations[i], horizon);
    tasks_interval[i] =
        new_interval(tasks_start[i], tasks_duration[i], tasks_end[i]);
    if (due_dates[i] == 0) {
      tardiness_vars[i] = tasks_end[i];
    } else {
      tardiness_vars[i] = new_variable(0, std::max(0, horizon - due_dates[i]));

      // tardiness_vars >= end - due_date
      LinearConstraintProto* arg = cp_model.add_constraints()->mutable_linear();
      arg->add_vars(tardiness_vars[i]);
      arg->add_coeffs(1);
      arg->add_vars(tasks_end[i]);
      arg->add_coeffs(-1);
      arg->add_domain(-due_dates[i]);
      arg->add_domain(kint64max);
    }
  }

  // Decision heuristic. Note that we don't instantiate all the variables. As a
  // consequence, in the values returned by the solution observer for the
  // non-fully instantiated variable will be the variable lower bounds after
  // propagation.
  {
    DecisionStrategyProto* strategy = cp_model.add_search_strategy();
    for (int i = 0; i < num_tasks; ++i) strategy->add_variables(tasks_start[i]);

    // Experiments showed that the heuristic of choosing first the task that
    // comes last works a lot better. This make sense because these are the task
    // with the most influence on the cost.
    strategy->set_variable_selection_strategy(
        DecisionStrategyProto::CHOOSE_HIGHEST_MAX);
    strategy->set_domain_reduction_strategy(
        DecisionStrategyProto::SELECT_MAX_VALUE);
  }

  // Disjunction between all the task intervals
  {
    ConstraintProto* ct = cp_model.add_constraints();
    NoOverlapConstraintProto* arg = ct->mutable_no_overlap();
    for (const int interval : tasks_interval) {
      arg->add_intervals(interval);
    }
  }

  // TODO(user): We can't set an objective upper bound with the current cp_model
  // interface, so we can't use heuristic or FLAGS_upper_bound here. The best is
  // probably to provide a "solution hint" instead.
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
  CpObjectiveProto* objective = cp_model.mutable_objective();
  for (int i = 0; i < num_tasks; ++i) {
    objective->add_vars(tardiness_vars[i]);
    objective->add_coeffs(weights[i]);
  }

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
        ConstraintProto* ct = cp_model.add_constraints();
        LinearConstraintProto* arg = ct->mutable_linear();
        arg->add_vars(tasks_start[j]);
        arg->add_coeffs(1);
        arg->add_vars(tasks_end[i]);
        arg->add_coeffs(-1);
        arg->add_domain(0);
        arg->add_domain(kint64max);
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
  model.Add(NewSatParameters(FLAGS_params));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    // Note that we conpute the "real" cost here and do not use the tardiness
    // variables. This is because in the core based approach, the tardiness
    // variable might be fixed before the end date, and we just have a >=
    // relation.
    int64 objective = 0;
    for (int i = 0; i < num_tasks; ++i) {
      objective +=
          weights[i] * std::max<int64>(0ll, r.solution(tasks_end[i]) - due_dates[i]);
    }
    LOG(INFO) << "Cost " << objective;

    // Print the current solution.
    std::vector<int> sorted_tasks(num_tasks);
    std::iota(sorted_tasks.begin(), sorted_tasks.end(), 0);
    std::sort(sorted_tasks.begin(), sorted_tasks.end(), [&](int v1, int v2) {
      return r.solution(tasks_start[v1]) < r.solution(tasks_start[v2]);
    });
    std::string solution = "0";
    int end = 0;
    for (const int i : sorted_tasks) {
      const int64 cost = weights[i] * r.solution(tardiness_vars[i]);
      StrAppend(&solution, "| #", i, " ");
      if (cost > 0) {
        // Display the cost in red.
        StrAppend(&solution, "\033[1;31m(+", cost, ") \033[0m");
      }
      StrAppend(&solution, "|", r.solution(tasks_end[i]));
      CHECK_EQ(end, r.solution(tasks_start[i]));
      end += durations[i];
      CHECK_EQ(end, r.solution(tasks_end[i]));
    }
    LOG(INFO) << "solution: " << solution;
  }));

  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  std::vector<int> numbers;
  std::vector<std::string> entries;
  for (const std::string& line : operations_research::FileLines(FLAGS_input)) {
    entries = strings::Split(line, ' ', strings::SkipEmpty());
    for (const std::string& entry : entries) {
      numbers.push_back(operations_research::atoi32(entry));
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

  operations_research::sat::Solve(durations, due_dates, weights);
  return EXIT_SUCCESS;
}
