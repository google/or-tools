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

//
// This model implements a multidimensional knapsack problem.

#include <cstdio>
#include <cstdlib>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/hybrid.h"
#include "ortools/util/filelineiter.h"

DEFINE_string(
    data_file, "",
    "Required: input file description the muldi-dimensional knapsack problem\n "
    "to solve. It supports two file format as described in:\n"
    "  - http://elib.zib.de/pub/Packages/mp-testdata/ip/sac94-suite/readme\n"
    "  - http://hces.bus.olemiss.edu/tools.html\n");
DEFINE_int32(time_limit_in_ms, 0, "Time limit in ms, <= 0 means no limit.");
DEFINE_int32(simplex_frequency, 0,
             "Number of nodes explored between each"
             " call to the simplex optimizer.");
DEFINE_bool(display_search_log, true, "Display search log.");

namespace operations_research {

// ----- Data -----

class MultiDimKnapsackData {
 public:
  MultiDimKnapsackData()
      : name_(""),
        line_read_(0),
        mode_(0),
        num_dims_(-1),
        num_items_(-1),
        current_bin_(0),
        current_item_(0),
        optimal_value_(0),
        problem_type_(-1) {}

  void Load(const std::string& filename) {
    for (const std::string& line : FileLines(filename)) {
      if (line.empty()) {
        continue;
      }
      ProcessNewLine(line);
    }
    if (optimal_value_ == 0) {
      LOG(INFO) << "Successfully loaded problem " << name_ << " with "
                << items() << " items, " << dims() << " dimensions";
    } else {
      LOG(INFO) << "Successfully loaded problem " << name_ << " with "
                << items() << " items, " << dims()
                << " dimensions and an optimal value of " << optimal_value_;
    }
  }

  // Number of items of the problem.
  int items() const { return num_items_; }

  // Number of dimensions of the problem.
  int dims() const { return num_dims_; }

  // Name of the problem.
  const std::string& name() const { return name_; }

  int capacity(int i) const { return dims_[i]; }
  int profit(int j) const { return profit_[j]; }
  int weight(int i, int j) const { return weight_[i][j]; }
  int optimal_value() const { return optimal_value_; }

  // Used internally.
  void ProcessNewLine(const std::string& line) {
    const std::vector<std::string> words =
        strings::Split(line, ' ', strings::SkipEmpty());
    line_read_++;
    if (problem_type_ == -1) {
      if (words.size() == 1) {
        LOG(INFO) << "New data format";
        problem_type_ = 1;
      } else if (words.size() == 2) {
        LOG(INFO) << "Original data format";
        problem_type_ = 0;
      }
    }
    if (problem_type_ == 0) {
      // 0 = init
      // 1 = size passed
      // 2 = profit passed
      // 3 = capacity passed
      // 4 = constraint passed
      // 5 = optimum passed
      // 6 = name passed
      switch (mode_) {
        case 0: {
          if (!words.empty()) {
            CHECK_EQ(2, words.size());
            num_dims_ = atoi32(words[0]);
            num_items_ = atoi32(words[1]);
            weight_.resize(num_dims_);
            mode_ = 1;
          }
          break;
        }
        case 1: {
          for (int i = 0; i < words.size(); ++i) {
            const int val = atoi32(words[i]);
            profit_.push_back(val);
          }
          CHECK_LE(profit_.size(), num_items_);
          if (profit_.size() == num_items_) {
            mode_ = 2;
          }
          break;
        }
        case 2: {
          for (int i = 0; i < words.size(); ++i) {
            const int val = atoi32(words[i]);
            dims_.push_back(val);
          }
          CHECK_LE(dims_.size(), num_dims_);
          if (dims_.size() == num_dims_) {
            mode_ = 3;
          }
          break;
        }
        case 3: {
          for (int i = 0; i < words.size(); ++i) {
            const int val = atoi32(words[i]);
            weight_[current_bin_].push_back(val);
            if (weight_[current_bin_].size() == num_items_) {
              current_bin_++;
            }
          }
          if (current_bin_ == num_dims_) {
            mode_ = 4;
          }
          break;
        }
        case 4: {
          if (!words.empty()) {
            CHECK_EQ(1, words.size());
            optimal_value_ = atoi32(words[0]);
            mode_ = 5;
          }
          break;
        }
        case 5: {
          if (!words.empty()) {
            name_ = line;
            mode_ = 6;
          }
          break;
        }
        case 6: {
          break;
        }
      }
    } else {
      // 0 = init
      // 1 = name passed
      // 2 = size passed
      // 3 = data passed
      // 4 = capacity passed
      switch (mode_) {
        case 0: {
          name_ = words[0];
          mode_ = 1;
          current_bin_ = -1;
          break;
        }
        case 1: {
          if (!words.empty()) {
            CHECK_EQ(2, words.size());
            num_items_ = atoi32(words[0]);
            num_dims_ = atoi32(words[1]);
            weight_.resize(num_dims_);
            mode_ = 2;
          }
          break;
        }
        case 2: {
          for (int i = 0; i < words.size(); ++i) {
            const int val = atoi32(words[i]);
            if (current_bin_ == -1) {
              profit_.push_back(val);
            } else {
              weight_[current_bin_].push_back(val);
              CHECK_EQ(current_item_, weight_[current_bin_].size() - 1);
            }
            current_bin_++;
            if (current_bin_ == num_dims_) {
              current_bin_ = -1;
              current_item_++;
            }
            if (current_item_ == num_items_) {
              mode_ = 3;
            }
          }
          break;
        }
        case 3: {
          for (int i = 0; i < words.size(); ++i) {
            const int val = atoi32(words[i]);
            dims_.push_back(val);
          }
          CHECK_LE(dims_.size(), num_dims_);
          if (dims_.size() == num_dims_) {
            mode_ = 4;
          }
          break;
        }
        case 4:
          break;
      }
    }
  }

 private:
  std::string name_;
  std::vector<int> dims_;
  std::vector<int> profit_;
  std::vector<std::vector<int> > weight_;
  int line_read_;
  int mode_;
  int num_dims_;
  int num_items_;
  int current_bin_;
  int current_item_;
  int optimal_value_;
  int problem_type_;  // -1 = undefined, 0 = original, 1 = new format
};

int64 EvaluateItem(const MultiDimKnapsackData& data, int64 var, int64 val) {
  if (val == 0) {
    return 0LL;
  }
  const int profit = data.profit(var);
  int max_weight = 0;
  for (int i = 0; i < data.dims(); ++i) {
    const int weight = data.weight(i, var);
    if (weight > max_weight) {
      max_weight = weight;
    }
  }
  return -(profit * 100 / max_weight);
}

void SolveKnapsack(MultiDimKnapsackData* const data) {
  Solver solver("MultiDim Knapsack");
  std::vector<IntVar*> assign;
  solver.MakeBoolVarArray(data->items(), "assign", &assign);
  for (int i = 0; i < data->dims(); ++i) {
    const int capacity = data->capacity(i);
    std::vector<int64> coefs;
    for (int j = 0; j < data->items(); ++j) {
      coefs.push_back(data->weight(i, j));
    }
    solver.AddConstraint(
        solver.MakeScalProdLessOrEqual(assign, coefs, capacity));
  }

  // Build objective.
  std::vector<int64> profits;
  for (int i = 0; i < data->items(); ++i) {
    profits.push_back(data->profit(i));
  }

  IntVar* const objective = solver.MakeScalProd(assign, profits)->Var();

  std::vector<SearchMonitor*> monitors;
  OptimizeVar* const objective_monitor = solver.MakeMaximize(objective, 1);
  monitors.push_back(objective_monitor);

  // Add a search collector of assign variables
  SolutionCollector* const assign_solution_collector =
      solver.MakeLastSolutionCollector();
  assign_solution_collector->Add(assign);
  monitors.push_back(assign_solution_collector);
  if (FLAGS_display_search_log) {
    SearchMonitor* const search_log = solver.MakeSearchLog(1000000, objective);
    monitors.push_back(search_log);
  }
  DecisionBuilder* const db = solver.MakePhase(
      assign, [data](int64 var,
                     int64 value) { return EvaluateItem(*data, var, value); },
      Solver::CHOOSE_STATIC_GLOBAL_BEST);
  if (FLAGS_time_limit_in_ms != 0) {
    LOG(INFO) << "adding time limit of " << FLAGS_time_limit_in_ms << " ms";
    SearchLimit* const limit = solver.MakeLimit(
        FLAGS_time_limit_in_ms, kint64max, kint64max, kint64max);
    monitors.push_back(limit);
  }

  if (FLAGS_simplex_frequency > 0) {
    SearchMonitor* const simplex =
        MakeSimplexConstraint(&solver, FLAGS_simplex_frequency);
    monitors.push_back(simplex);
  }

  if (solver.Solve(db, monitors)) {
    LOG(INFO) << "Best solution found = " << objective_monitor->best();
    std::string assigned_items = "";
    for (int i = 0; i < assign.size(); i++) {
      IntVar* assign_var = assign[i];
      if (assign_solution_collector->Value(0, assign_var) == 1) {
        assigned_items += ", " + std::to_string(i);
      }
    }
    if (assigned_items == "") {
      LOG(INFO) << "No items were assigned";
    } else {
      assigned_items.erase(0, 2);
      LOG(INFO) << "Assigned items : " << assigned_items << ".";
    }
  }
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program runs a multi-dimensional knapsack problem.";

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(FATAL) << "Please supply a data file with --data_file=";
  }
  operations_research::MultiDimKnapsackData data;
  data.Load(FLAGS_data_file);
  operations_research::SolveKnapsack(&data);
  return 0;
}
