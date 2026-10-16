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

#include "ortools/graph/random_graph.h"

#include <cstdint>
#include <memory>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/log_severity.h"
#include "ortools/graph/graph.h"

namespace util {
namespace {

TEST(RandomUndirectedSimpleGraphTest, RandomizedSmallGraphGeneration) {
  absl::BitGen random;
  const int num_tests = DEBUG_MODE ? 300 : 5000;
  for (int test = 0; test < num_tests; ++test) {
    const int num_nodes = absl::Uniform(random, 0, 100);
    const int num_edges =
        absl::Uniform(random, 0, num_nodes * (num_nodes - 1) / 2 + 1);
    std::unique_ptr<StaticGraph<>> graph =
        GenerateRandomUndirectedSimpleGraph(num_nodes, num_edges, random)
            .Build(nullptr);
    ASSERT_EQ(2 * num_edges, graph->num_arcs());

    // Validate arc pairing implicitly.
    int64_t counter = 0;
    for (const auto arc : graph->AllForwardArcs()) {
      const auto head = graph->Head(arc);
      const auto tail = graph->Tail(arc);
      ASSERT_NE(head, tail);
      const int64_t edge_key = static_cast<int64_t>(head + 1) * (tail + 1);
      counter += (head > tail) ? edge_key : -edge_key;
    }
    EXPECT_EQ(counter, 0);
  }
}

TEST(RandomUndirectedSimpleGraphTest, RandomizedSparseLargeGraphGeneration) {
  absl::BitGen random;
  const int num_tests = DEBUG_MODE ? 10 : 100;
  for (int test = 0; test < num_tests; ++test) {
    const int num_nodes = absl::Uniform(random, 100000, 10000000);
    const int num_edges = absl::Uniform(random, 2, 10);
    std::unique_ptr<StaticGraph<>> graph =
        GenerateRandomUndirectedSimpleGraph(num_nodes, num_edges, random)
            .Build(nullptr);
    EXPECT_EQ(2 * num_edges, graph->num_arcs());
  }
}

}  // namespace
}  // namespace util
