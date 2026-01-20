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
#include <cstdlib>
#include <vector>

#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {

using NodeIndex = int32_t;
using ArcIndex = int32_t;
using CostValue = int64_t;
using Graph = ::util::ListGraph<NodeIndex, ArcIndex>;

// Test assignment on a 4x4 matrix. Example taken from
// http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm with kCost[0][1]
// modified so the optimum solution is unique.
void AssignmentOn4x4Matrix() {
  LOG(INFO) << "Assignment on 4x4 Matrix";
  const int kNumSources = 4;
  const int kNumTargets = 4;
  const CostValue kCost[kNumSources][kNumTargets] = {{90, 76, 75, 80},
                                                     {35, 85, 55, 65},
                                                     {125, 95, 90, 105},
                                                     {45, 110, 95, 115}};
  const CostValue kExpectedCost =
      kCost[0][3] + kCost[1][2] + kCost[2][1] + kCost[3][0];
  Graph graph(kNumSources + kNumTargets, kNumSources * kNumTargets);
  LinearSumAssignment<Graph, CostValue> assignment(graph, kNumSources);
  for (NodeIndex source = 0; source < kNumSources; ++source) {
    for (NodeIndex target = 0; target < kNumTargets; ++target) {
      ArcIndex arc = graph.AddArc(source, kNumSources + target);
      assignment.SetArcCost(arc, kCost[source][target]);
    }
  }
  CHECK(assignment.ComputeAssignment());
  CostValue total_cost = assignment.GetCost();
  CHECK_EQ(kExpectedCost, total_cost);
}

void AnotherAssignment() {
  LOG(INFO) << "Another assignment on 4x4 matrix";
  std::vector<std::vector<int>> matrice(
      {{8, 7, 9, 9}, {5, 2, 7, 8}, {6, 1, 4, 9}, {2, 3, 2, 6}});
  const int kSize = matrice.size();
  Graph graph(2 * kSize, kSize * kSize);
  LinearSumAssignment<Graph, CostValue> assignment(graph, kSize);
  for (int i = 0; i < kSize; ++i) {
    CHECK_EQ(kSize, matrice[i].size());
    for (int j = 0; j < kSize; ++j) {
      int arcIndex = graph.AddArc(i, j + kSize);
      assignment.SetArcCost(arcIndex, matrice[i][j]);
    }
  }

  assignment.ComputeAssignment();
  LOG(INFO) << "Cost : " << assignment.GetCost();
}

}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  operations_research::AssignmentOn4x4Matrix();
  operations_research::AnotherAssignment();
  return EXIT_SUCCESS;
}
