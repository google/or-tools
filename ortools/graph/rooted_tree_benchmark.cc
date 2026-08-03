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

#include <algorithm>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/rooted_tree.h"

namespace operations_research {
namespace {

std::vector<int> RandomTreeRootedZero(int num_nodes) {
  absl::BitGen bit_gen;
  std::vector<int> nodes(num_nodes);
  absl::c_iota(nodes, 0);
  std::shuffle(nodes.begin() + 1, nodes.end(), bit_gen);
  std::vector<int> result(num_nodes);
  result[0] = -1;
  for (int i = 1; i < num_nodes; ++i) {
    int target = absl::Uniform<int>(bit_gen, 0, i);
    result[i] = target;
  }
  return result;
}

void BM_RootedTreeShortestPath(benchmark::State& state) {
  const int num_nodes = state.range(0);
  std::vector<int> random_tree_data = RandomTreeRootedZero(num_nodes);
  for (auto s : state) {
    const RootedTree<int> tree =
        RootedTree<int>::Create(0, random_tree_data).value();
    std::vector<int> path = tree.PathToRoot(num_nodes - 1);
    CHECK_GE(path.size(), 2);
  }
}

BENCHMARK(BM_RootedTreeShortestPath)->Arg(100)->Arg(10'000)->Arg(1'000'000);

}  // namespace
}  // namespace operations_research
