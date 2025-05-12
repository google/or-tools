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

#include "ortools/graph/graph_generator.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

template <typename Graph>
std::vector<int> OutgoingNodes(const Graph& graph,
                               typename Graph::NodeIndex src) {
  std::vector<int> nodes;
  for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(src)) {
    nodes.push_back(graph.Head(arc));
  }
  return nodes;
}

template <class T>
class GenerateCompleteUndirectedGraphTest : public testing::Test {};

using GenerateCompleteUndirectedGraphTypes =
    ::testing::Types<util::ListGraph<int, int>, util::StaticGraph<int, int>>;
TYPED_TEST_SUITE(GenerateCompleteUndirectedGraphTest,
                 GenerateCompleteUndirectedGraphTypes);

TYPED_TEST(GenerateCompleteUndirectedGraphTest, SimpleGraph) {
  TypeParam graph = GenerateCompleteUndirectedGraph<TypeParam>(3);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 3);
  EXPECT_EQ(graph.num_arcs(), 6);
  EXPECT_THAT(graph.OutDegree(0), 2);
  EXPECT_THAT(graph.OutDegree(1), 2);
  EXPECT_THAT(graph.OutDegree(2), 2);
  EXPECT_THAT(OutgoingNodes(graph, 0), UnorderedElementsAre(1, 2));
  EXPECT_THAT(OutgoingNodes(graph, 1), UnorderedElementsAre(0, 2));
  EXPECT_THAT(OutgoingNodes(graph, 2), UnorderedElementsAre(0, 1));
}

TYPED_TEST(GenerateCompleteUndirectedGraphTest, SmallestGraph) {
  TypeParam graph = GenerateCompleteUndirectedGraph<TypeParam>(1);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 1);
  EXPECT_EQ(graph.num_arcs(), 0);
  EXPECT_THAT(OutgoingNodes(graph, 0), IsEmpty());
}

template <class T>
class GenerateCompleteUndirectedBipartiteGraphTest : public testing::Test {};

using GenerateCompleteUndirectedBipartiteGraphTypes =
    ::testing::Types<util::ListGraph<int, int>, util::StaticGraph<int, int>>;
TYPED_TEST_SUITE(GenerateCompleteUndirectedBipartiteGraphTest,
                 GenerateCompleteUndirectedBipartiteGraphTypes);

TYPED_TEST(GenerateCompleteUndirectedBipartiteGraphTest, SimpleGraph) {
  TypeParam graph = GenerateCompleteUndirectedBipartiteGraph<TypeParam>(2, 3);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 5);
  EXPECT_EQ(graph.num_arcs(), 12);
  EXPECT_THAT(OutgoingNodes(graph, 0), UnorderedElementsAre(2, 3, 4));
  EXPECT_THAT(OutgoingNodes(graph, 1), UnorderedElementsAre(2, 3, 4));
  EXPECT_THAT(OutgoingNodes(graph, 2), UnorderedElementsAre(0, 1));
  EXPECT_THAT(OutgoingNodes(graph, 3), UnorderedElementsAre(0, 1));
  EXPECT_THAT(OutgoingNodes(graph, 4), UnorderedElementsAre(0, 1));
}

template <class T>
class GenerateCompleteDirectedBipartiteGraphTest : public testing::Test {};

using GenerateCompleteDirectedBipartiteGraphTypes =
    ::testing::Types<util::ListGraph<int, int>, util::StaticGraph<int, int>>;
TYPED_TEST_SUITE(GenerateCompleteDirectedBipartiteGraphTest,
                 GenerateCompleteDirectedBipartiteGraphTypes);

TYPED_TEST(GenerateCompleteDirectedBipartiteGraphTest, SimpleGraph) {
  TypeParam graph = GenerateCompleteDirectedBipartiteGraph<TypeParam>(2, 3);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 5);
  EXPECT_EQ(graph.num_arcs(), 6);
  EXPECT_THAT(OutgoingNodes(graph, 0), UnorderedElementsAre(2, 3, 4));
  EXPECT_THAT(OutgoingNodes(graph, 1), UnorderedElementsAre(2, 3, 4));
  EXPECT_THAT(OutgoingNodes(graph, 2), IsEmpty());
  EXPECT_THAT(OutgoingNodes(graph, 3), IsEmpty());
  EXPECT_THAT(OutgoingNodes(graph, 4), IsEmpty());
}

TYPED_TEST(GenerateCompleteDirectedBipartiteGraphTest, SimplSmallestGraphLeft) {
  TypeParam graph = GenerateCompleteDirectedBipartiteGraph<TypeParam>(1, 0);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 1);
  EXPECT_EQ(graph.num_arcs(), 0);
  EXPECT_THAT(OutgoingNodes(graph, 0), IsEmpty());
}

TYPED_TEST(GenerateCompleteDirectedBipartiteGraphTest,
           SimplSmallestGraphRight) {
  TypeParam graph = GenerateCompleteDirectedBipartiteGraph<TypeParam>(0, 1);
  graph.Build();

  EXPECT_EQ(graph.num_nodes(), 1);
  EXPECT_EQ(graph.num_arcs(), 0);
  EXPECT_THAT(OutgoingNodes(graph, 0), IsEmpty());
}

}  // namespace
