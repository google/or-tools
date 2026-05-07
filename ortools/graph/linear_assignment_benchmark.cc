// Copyright 2010-2025 Google LLC
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

#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {

// Same as ConstructRandomAssignment, but for the new API.
template <typename GraphType>
std::pair<std::unique_ptr<GraphType>,
          std::unique_ptr<LinearSumAssignment<GraphType, int64_t>>>
ConstructRandomAssignmentForNewGraphApi(const int left_nodes,
                                        const int average_degree,
                                        const int cost_limit) {
  const int kNodes = 2 * left_nodes;
  const int kArcs = left_nodes * average_degree;
  const int kRandomSeed = 0;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<int64_t> arc_costs;
  arc_costs.reserve(kArcs);
  typename GraphType::Builder builder(kNodes, kArcs);
  for (int i = 0; i < kArcs; ++i) {
    const int left = absl::Uniform(randomizer, 0, left_nodes);
    const int right = left_nodes + absl::Uniform(randomizer, 0, left_nodes);
    const int64_t cost = absl::Uniform(randomizer, 0, cost_limit);
    builder.AddArc(left, right);
    arc_costs.push_back(cost);
  }

  // Finalize the graph.
  std::vector<typename GraphType::ArcIndex> permutation;
  auto graph = std::move(builder).BuildAndPermute(permutation, arc_costs);

  // Create the assignment.
  auto assignment = std::make_unique<LinearSumAssignment<GraphType, int64_t>>(
      *graph, left_nodes);
  for (int arc = 0; arc < kArcs; ++arc) {
    assignment->SetArcCost(arc, arc_costs[arc]);
  }
  return {std::move(graph), std::move(assignment)};
}

template <typename GraphType>
void BM_ConstructRandomAssignmentProblemWithNewGraphApi(
    benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const int64_t kCostLimit = 1000000;
  for (auto _ : state) {
    const auto [graph, assignment] =
        ConstructRandomAssignmentForNewGraphApi<GraphType>(
            kLeftNodes, kAverageDegree, kCostLimit);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_ConstructRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_ConstructRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

template <typename GraphType>
void BM_SolveRandomAssignmentProblemWithNewGraphApi(benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const int64_t kCostLimit = 1000000;
  const auto [graph, assignment] =
      ConstructRandomAssignmentForNewGraphApi<GraphType>(
          kLeftNodes, kAverageDegree, kCostLimit);
  for (auto _ : state) {
    assignment->ComputeAssignment();
    CHECK_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_SolveRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_SolveRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

template <typename GraphType>
void BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi(
    benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const int64_t kCostLimit = 1000000;
  for (auto _ : state) {
    const auto [graph, assignment] =
        ConstructRandomAssignmentForNewGraphApi<GraphType>(
            kLeftNodes, kAverageDegree, kCostLimit);
    assignment->ComputeAssignment();
    CHECK_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

}  // namespace operations_research
