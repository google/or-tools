
// Copyright 2025 Francesco Cavaliere
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

#include <absl/log/initialize.h>
#include <absl/status/status.h>

#include <cstdlib>
#include <random>

#include "ortools/base/init_google.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_cft.h"
#include "ortools/set_cover/samples/bin_packing.h"

using namespace operations_research;
ABSL_FLAG(std::string, instance, "", "BPP instance int RAIL format.");
ABSL_FLAG(int, bins, 1000, "Number of bins to generate.");

template <typename Iterable>
std::string Stringify(const Iterable& col) {
  std::string result;
  for (auto i : col) {
    absl::StrAppend(&result, " ", i);
  }
  return result;
}

bool operator==(const SparseColumn& lhs, const std::vector<BaseInt>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  auto lit = lhs.begin();
  auto rit = rhs.begin();
  while (lit != lhs.end() && rit != rhs.end()) {
    if (static_cast<BaseInt>(*lit) != *rit) {
      return false;
    }
    ++lit;
    ++rit;
  }
  return true;
}

void RunTest(const ElementCostVector& weights, const ElementCostVector& profits,
             const std::vector<BaseInt>& expected) {
  ExpKnap knap_solver;

  for (ElementIndex i; i < ElementIndex(weights.size()); ++i) {
    std::cout << "Item " << i << " -- profit: " << profits[i]
              << " weight: " << weights[i]
              << " efficiency: " << profits[i] / weights[i] << "\n";
  }

  knap_solver.InitSolver(profits, weights, 6, 100000000);
  knap_solver.Heuristic();
  std::cout << "Heur solution cost " << knap_solver.best_cost() << " -- "
            << Stringify(knap_solver.collected_bins()[0]) << "\n";

  knap_solver.EleBranch();
  std::cout << "B&b solution cost " << knap_solver.best_cost() << " -- "
            << Stringify(knap_solver.collected_bins()[0]) << "\n";

  const auto& result = knap_solver.collected_bins()[0];
  if (!(result == expected)) {
    std::cout << "Error: expected " << Stringify(expected) << " but got "
              << Stringify(result) << "\n";
  }
  std::cout << std::endl;
}

void KnapsackTest() {
  std::cout << "Testing knapsack\n";
  ExpKnap knap_solver;
  ElementCostVector ws = {1, 2, 3, 4, 5};
  RunTest(ws, {10, 20, 30, 40, 51}, {0, 4});
  RunTest(ws, {10, 20, 30, 41, 50}, {1, 3});
  RunTest(ws, {10, 20, 31, 40, 50}, {0, 1, 2});
  RunTest(ws, {10, 21, 30, 41, 50}, {1, 3});
  RunTest(ws, {11, 21, 30, 40, 50}, {0, 1, 2});
  RunTest(ws, {11, 20, 31, 40, 50}, {0, 1, 2});
  RunTest(ws, {11, 20, 30, 41, 50}, {0, 4});
  RunTest(ws, {11, 20, 30, 40, 51}, {0, 4});
  RunTest(ws, {11, 21, 31, 40, 50}, {0, 1, 2});
  RunTest({4.1, 2, 2, 2}, {8.5, 3, 3, 3}, {1, 2, 3});
}

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // KnapsackTest();
  // return 0;

  BinPackingModel model = ReadBpp(absl::GetFlag(FLAGS_instance));

  // Quick run with a minimal set of bins
  BinPackingSetCoverModel scp_model = GenerateInitialBins(model);
  scp::PrimalDualState best_result = scp::RunCftHeuristic(scp_model);

  if (absl::GetFlag(FLAGS_bins) > 0) {
    // Run the CFT again with more bins to get a better solution
    std::mt19937 rnd(0);
    AddRandomizedBins(model, absl::GetFlag(FLAGS_bins), scp_model, rnd);
    scp::PrimalDualState result =
        scp::RunCftHeuristic(scp_model, best_result.solution);
    if (result.solution.cost() < best_result.solution.cost()) {
      best_result = result;
    }
  }

  auto [solution, dual] = best_result;
  if (solution.subsets().empty()) {
    std::cerr << "Error: failed to find any solution\n";
  } else {
    std::cout << "Solution:         " << solution.cost() << '\n';
  }

  if (dual.multipliers().empty()) {
    std::cerr << "Error: failed to find any dual\n";
  } else {
    std::cout << "Core Lower bound: " << dual.lower_bound() << '\n';
  }

  // The lower bound computed on the full model is not a real lower bound unless
  // the knapsack subproblem failed to fined any negative reduced cost bin to
  // add to the set cover model.
  // TODO(anyone): add a flag to indicate if a valid LB has been found or not.
  if (scp_model.best_dual_state().multipliers().empty()) {
    std::cerr << "Error: no real dual state has been computed\n";
  } else {
    std::cout << "Restricted Lower bound: "
              << scp_model.best_dual_state().lower_bound() << '\n';
  }

  return EXIT_SUCCESS;
}