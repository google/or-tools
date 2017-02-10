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
// ACP 2014 challenge

#include <cstdio>

#include "base/commandlineflags.h"
#include "base/file.h"
#include "base/filelinereader.h"
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/split.h"
#include "base/map_util.h"
#include "base/stringprintf.h"
#include "base/strtoint.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/routing.h"
#include "util/tuple_set.h"

DEFINE_string(input, "", "");
DEFINE_string(solution, "", "");
DEFINE_int32(ls_seed, 0, "ls seed");
DEFINE_int32(ls_size, 8, "ls size");
DEFINE_int32(ls_perm, 11, "ls perm");

DECLARE_string(routing_first_solution);
DECLARE_bool(routing_no_lns);
DECLARE_bool(routing_trace);
DECLARE_bool(routing_guided_local_search);

namespace operations_research {

class AcpData {
 public:
  AcpData()
      : num_periods_(-1), num_products_(-1), inventory_cost_(0), state_(0) {}

  void Load(const std::string& filename) {
    FileLineReader reader(filename.c_str());
    reader.set_line_callback(
        NewPermanentCallback(this, &AcpData::ProcessNewLine));
    reader.Reload();
    if (!reader.loaded_successfully()) {
      LOG(ERROR) << "Could not open acp challenge file";
    }
  }

  void ProcessNewLine(char* const line) {
    const std::vector<std::string> words =
        strings::Split(line, " ", strings::SkipEmpty());
    if (words.empty()) return;
    switch (state_) {
      case 0: {
        num_periods_ = atoi32(words[0]);
        state_ = 1;
        break;
      }
      case 1: {
        num_products_ = atoi32(words[0]);
        state_ = 2;
        break;
      }
      case 2: {
        due_dates_per_product_.resize(due_dates_per_product_.size() + 1);
        CHECK_EQ(words.size(), num_periods_) << "Error with line " << line;
        for (int i = 0; i < num_periods_; ++i) {
          if (atoi32(words[i]) == 1) {
            due_dates_per_product_.back().push_back(i);
          }
        }
        if (due_dates_per_product_.size() == num_products_) {
          state_ = 3;
        }
        break;
      }
      case 3: {
        inventory_cost_ = atoi32(words[0]);
        state_ = 4;
        break;
      }
      case 4: {
        transitions_.resize(transitions_.size() + 1);
        CHECK_EQ(words.size(), num_products_);
        for (int i = 0; i < num_products_; ++i) {
          transitions_.back().push_back(atoi32(words[i]));
        }
        break;
      }
      default: {
        LOG(ERROR) << "Should not be here";
      }
    }
  }

  std::string DebugString() const {
    return StringPrintf("AcpData(%d periods, %d products, %d cost)",
                        num_periods_, num_products_, inventory_cost_);
  }

  const std::vector<std::vector<int>>& due_dates_per_product() const {
    return due_dates_per_product_;
  }

  const std::vector<std::vector<int>>& transitions() const {
    return transitions_;
  }

  int num_periods() const { return num_periods_; }
  int num_products() const { return num_products_; }
  int inventory_cost() const { return inventory_cost_; }

 private:
  int num_periods_;
  int num_products_;
  int inventory_cost_;
  std::vector<std::vector<int>> due_dates_per_product_;
  std::vector<std::vector<int>> transitions_;
  int state_;
};

void LoadSolution(const std::string& filename, std::vector<int>* vec) {
  File* const file = File::OpenOrDie(filename, "r");
  std::string line;
  file->ReadToString(&line, 10000);
  const std::vector<std::string> words =
      strings::Split(line, " ", strings::SkipEmpty());
  LOG(INFO) << "Solution file has " << words.size() << " entries";
  vec->clear();
  for (const std::string& word : words) {
    vec->push_back(atoi32(word));
  }
  LOG(INFO) << "  - loaded " << strings::Join(*vec, " ");
}

int Evaluate(const AcpData& data, const std::vector<int>& schedule) {
  std::vector<int> indices(data.num_products(), 0);
  int early_days = 0;
  for (int i = 0; i < schedule.size(); ++i) {
    const int product = schedule[i];
    if (product >= data.num_products() || product < -1) {
      return kint32max;
    }
    if (product != -1) {
      const int index = indices[product];
      if (index >= data.due_dates_per_product()[product].size()) {
        LOG(INFO) << "Strange";
        return kint32max;
      }
      indices[product]++;
      const int due_date = data.due_dates_per_product()[product][index];
      if (i > due_date) {
        return kint32max;
      }
      early_days += due_date - i;
    }
  }

  int previous = -1;
  int transition_cost = 0;
  for (const int product : schedule) {
    if (previous != -1  && product != -1 && previous != product) {
      transition_cost += data.transitions()[previous][product];
    }
    if (product != -1) {
      previous = product;
    }
  }
  return transition_cost + early_days * data.inventory_cost();
}

int64 OneDistance(RoutingModel::NodeIndex from, RoutingModel::NodeIndex to) {
  return 1;
}


class ProductMatrix {
 public:
  ProductMatrix(const AcpData& data, const std::vector<int>& item_to_product)
      : data_(data), item_to_product_(item_to_product) {}

  int64 Distance(RoutingModel::NodeIndex from,
                 RoutingModel::NodeIndex to) const {
    if (from.value() == 0 || to.value() == 0) {
      return 0;
    }
    const int index1 = item_to_product_[from.value() - 1];
    const int index2 = item_to_product_[to.value() - 1];
    return data_.transitions()[index1][index2];
  }

 private:
  const AcpData& data_;
  const std::vector<int>& item_to_product_;
};

void Solve(const std::string& filename, const std::string& solution_file) {

  const char* kTime = "Time";

  LOG(INFO) << "Load " << filename;
  AcpData data;
  data.Load(filename);

  std::vector<int> best;
  int best_cost = kint32max;
  if (!solution_file.empty()) {
    LoadSolution(solution_file, &best);
    best.resize(data.num_periods());
    best_cost = Evaluate(data, best);
    LOG(INFO) << "Initial solution cost = " << best_cost;
  }

  int num_active_periods = 0;
  std::vector<int> due_dates_per_period(data.num_periods(), 0);
  for (const std::vector<int>& d : data.due_dates_per_product()) {
    for (const int v : d) {
      due_dates_per_period[v]++;
      num_active_periods++;
    }
  }
  LOG(INFO) << "num active periods = " << num_active_periods;
  std::vector<bool> active_periods(num_active_periods);
  std::vector<int> modified_dates_to_dates;
  std::vector<int> dates_to_modified_dates;
  int count = 0;
  for (int period = data.num_periods() - 1; period >= 0; --period) {
    count += due_dates_per_period[period];
    if (count > 0) {
      count--;
      active_periods[period] = true;
    } else {
      active_periods[period] = false;
    }
  }
  for (int i = 0; i < data.num_periods(); ++i) {
    if (active_periods[i]) {
      dates_to_modified_dates.push_back(modified_dates_to_dates.size());
      modified_dates_to_dates.push_back(i);
    } else {
      dates_to_modified_dates.push_back(-1);
    }
  }
  LOG(INFO) << "original: " << strings::Join(dates_to_modified_dates, " ");
  LOG(INFO) << "modified: " << strings::Join(modified_dates_to_dates, " ");

  std::vector<int> item_to_product;
  std::vector<int> modified_due_dates;
  for (int i = 0; i < data.num_products(); ++i) {
    for (const int j : data.due_dates_per_product()[i]) {
      item_to_product.push_back(i);
      modified_due_dates.push_back(dates_to_modified_dates[j]);
    }
  }

  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(num_active_periods + 1, 1, kDepot);

  // Setting first solution heuristic (cheapest addition).
  FLAGS_routing_first_solution = "Savings";
  // Disabling Large Neighborhood Search, comment out to activate it.
  FLAGS_routing_no_lns = true;
  FLAGS_routing_trace = true;
  FLAGS_routing_guided_local_search = true;

  ProductMatrix matrix(data, item_to_product);

  routing.SetArcCostEvaluatorOfAllVehicles(
      NewPermanentCallback(&matrix, &ProductMatrix::Distance));

  routing.AddDimension(NewPermanentCallback(OneDistance), 0,
                       num_active_periods + 2, true, kTime);
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  for (int i = 0; i < num_active_periods; ++i) {
    const int due_date = modified_due_dates[i];
    LOG(INFO) << i << ": " << due_date;
    time_dimension.CumulVar(1 + i)->SetMax(due_date + 1);
  }
  // Solve, returns a solution if any (owned by RoutingModel).
  const Assignment* solution = routing.Solve();
  if (solution != NULL) {
    LOG(INFO) << solution->DebugString();
  } else {
    LOG(INFO) << "No solution";
  }
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs the ACP 2014 summer school "
    "competition";

int main(int argc, char** argv) {
  FLAGS_log_prefix = false;
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }
  operations_research::Solve(FLAGS_input, FLAGS_solution);
  return 0;
}
