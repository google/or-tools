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

#include "ortools/graph/line_graph.h"

#include "absl/base/macros.h"
#include "gtest/gtest.h"
#include "ortools/graph/graph.h"

namespace operations_research {
namespace {

// Empty fixture templates to collect the types of graphs on which we want to
// base the shortest paths template instances that we test.
template <typename GraphType>
class LineGraphDeathTest : public testing::Test {};
template <typename GraphType>
class LineGraphTest : public testing::Test {};

typedef testing::Types<::util::ListGraph<>, ::util::ReverseArcListGraph<>>
    GraphTypesForLineGraphTesting;

TYPED_TEST_SUITE(LineGraphDeathTest, GraphTypesForLineGraphTesting);
TYPED_TEST_SUITE(LineGraphTest, GraphTypesForLineGraphTesting);

TYPED_TEST(LineGraphDeathTest, NullLineGraph) {
  TypeParam graph;
#ifndef NDEBUG
  EXPECT_DEATH(BuildLineGraph<TypeParam>(graph, nullptr),
               "line_graph must not be NULL");
#else
  EXPECT_FALSE(BuildLineGraph<TypeParam>(graph, nullptr));
#endif
}

TYPED_TEST(LineGraphDeathTest, NonEmptyLineGraph) {
  TypeParam graph;
  TypeParam line_graph(1, 1);
  line_graph.AddArc(0, 0);
#ifndef NDEBUG
  EXPECT_DEATH(BuildLineGraph<TypeParam>(graph, &line_graph),
               "line_graph must be empty");
#else
  EXPECT_FALSE(BuildLineGraph<TypeParam>(graph, &line_graph));
#endif
}

TYPED_TEST(LineGraphDeathTest, LineGraphOfEmptyGraph) {
  TypeParam graph;
  TypeParam line_graph;
  EXPECT_TRUE(BuildLineGraph<TypeParam>(graph, &line_graph));
  EXPECT_EQ(0, line_graph.num_nodes());
  EXPECT_EQ(0, line_graph.num_arcs());
}

TYPED_TEST(LineGraphTest, LineGraphOfSingleton) {
  TypeParam graph(1, 1);
  graph.AddArc(0, 0);
  TypeParam line_graph;
  EXPECT_TRUE(BuildLineGraph<TypeParam>(graph, &line_graph));
  EXPECT_EQ(1, line_graph.num_nodes());
  EXPECT_EQ(1, line_graph.num_arcs());
}

TYPED_TEST(LineGraphTest, LineGraph) {
  const typename TypeParam::NodeIndex kNodes = 4;
  const typename TypeParam::ArcIndex kArcs[][2] = {
      {0, 1}, {0, 2}, {1, 2}, {2, 0}, {2, 3}};
  const typename TypeParam::ArcIndex kExpectedLineArcs[][2] = {
      {0, 2}, {2, 3}, {3, 0}, {3, 1}, {2, 4}, {1, 4}, {1, 3}};
  TypeParam graph(kNodes, ABSL_ARRAYSIZE(kArcs));
  for (int i = 0; i < ABSL_ARRAYSIZE(kArcs); ++i) {
    graph.AddArc(kArcs[i][0], kArcs[i][1]);
  }
  TypeParam line_graph;
  EXPECT_TRUE(BuildLineGraph<TypeParam>(graph, &line_graph));
  EXPECT_EQ(ABSL_ARRAYSIZE(kArcs), line_graph.num_nodes());
  EXPECT_EQ(ABSL_ARRAYSIZE(kExpectedLineArcs), line_graph.num_arcs());
  for (int i = 0; i < ABSL_ARRAYSIZE(kExpectedLineArcs); ++i) {
    const typename TypeParam::NodeIndex expected_tail = kExpectedLineArcs[i][0];
    const typename TypeParam::NodeIndex expected_head = kExpectedLineArcs[i][1];
    bool found = false;
    for (const auto arc : line_graph.OutgoingArcs(expected_tail)) {
      if (line_graph.Head(arc) == expected_head) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << expected_tail << " " << expected_head;
  }
}

}  // namespace
}  // namespace operations_research
