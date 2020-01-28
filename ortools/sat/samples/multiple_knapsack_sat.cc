// Copyright 2010-2018 Google LLC
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

// [START program]
// [START import]
#include "ortools/sat/cp_model.h"
// [END import]
namespace operations_research {
namespace sat {

// [START data_model]
struct DataModel {
  const std::vector<int> weights = {{
      48,
      30,
      42,
      36,
      36,
      48,
      42,
      42,
      36,
      24,
      30,
      30,
      42,
      36,
      36,
  }};
  const std::vector<int> values = {
      {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25}};
  const int num_items = weights.size();
  const int total_value = accumulate(values.begin(), values.end(), 0);
  const std::vector<int> kBinCapacities = {{100, 100, 100, 100, 100}};
  const int kNumBins = 5;
};
// [END data_model]

// [START solution_printer]
void PrintSolution(const DataModel data,
                   const std::vector<std::vector<IntVar>> x,
                   const std::vector<IntVar> load,
                   const std::vector<IntVar> value,
                   const CpSolverResponse response) {
  for (int b = 0; b < data.kNumBins; ++b) {
    LOG(INFO) << "Bin " << b;
    LOG(INFO);
    for (int i = 0; i < data.num_items; ++i) {
      if (SolutionIntegerValue(response, x[i][b]) > 0) {
        LOG(INFO) << "Item " << i << "  -  Weight: " << data.weights[i]
                  << " Value: " << data.values[i];
      }
    }
    LOG(INFO) << "Packed bin weight: "
              << SolutionIntegerValue(response, load[b]);
    LOG(INFO) << "Packed bin value: "
              << SolutionIntegerValue(response, value[b]);
    LOG(INFO);
  }
  LOG(INFO) << "Total packed weight: "
            << SolutionIntegerValue(response, LinearExpr::Sum(load));
  LOG(INFO) << "Total packed value: "
            << SolutionIntegerValue(response, LinearExpr::Sum(value));
}
// [END solution_printer]

void MultipleKnapsackSat() {
  // [START data]
  DataModel data;
  // [END data]

  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  // Main variables.
  std::vector<std::vector<IntVar>> x(data.num_items);
  for (int i = 0; i < data.num_items; ++i) {
    for (int b = 0; b < data.kNumBins; ++b) {
      x[i].push_back(cp_model.NewIntVar({0, 1}));
    }
  }

  // Load variables.
  std::vector<IntVar> load(data.kNumBins);
  std::vector<IntVar> value(data.kNumBins);
  for (int b = 0; b < data.kNumBins; ++b) {
    load[b] = cp_model.NewIntVar({0, data.kBinCapacities[b]});
    value[b] = cp_model.NewIntVar({0, data.total_value});
  }

  // Links load and x.
  for (int b = 0; b < data.kNumBins; ++b) {
    LinearExpr weightsExpr;
    LinearExpr valuesExpr;
    for (int i = 0; i < data.num_items; ++i) {
      weightsExpr.AddTerm(x[i][b], data.weights[i]);
      valuesExpr.AddTerm(x[i][b], data.values[i]);
    }
    cp_model.AddEquality(weightsExpr, load[b]);
    cp_model.AddEquality(valuesExpr, value[b]);
  }
  // [END variables]

  // [START constraints]
  // Each item can be in at most one bin.
  for (int i = 0; i < data.num_items; ++i) {
    LinearExpr expr;
    for (int b = 0; b < data.kNumBins; ++b) {
      expr.AddTerm(x[i][b], 1);
    }
    cp_model.AddLessOrEqual(expr, 1);
  }
  // [END constraints]
  // Maximize sum of load.
  // [START objective]
  cp_model.Maximize(LinearExpr::Sum(value));
  // [END objective]

  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // [START print_solution]
  PrintSolution(data, x, load, value, response);
  // [END print_solution]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::MultipleKnapsackSat();

  return EXIT_SUCCESS;
}
// [END program]
