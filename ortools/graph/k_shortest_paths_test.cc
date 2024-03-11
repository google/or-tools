// Copyright 2010-2024 Google LLC
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

#include "ortools/graph/k_shortest_paths.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/shortest_paths.h"

namespace operations_research {
namespace {

using testing::ElementsAre;
using util::StaticGraph;

TEST(KShortestPathsYenDeathTest, EmptyGraph) {
  StaticGraph<> graph;
  std::vector<PathDistance> lengths;

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1, /*k=*/10),
               "graph.num_nodes\\(\\) > 0");
}

TEST(KShortestPathsYenDeathTest, NoArcGraph) {
  StaticGraph<> graph;
  graph.AddNode(1);
  (void)graph.Build();
  std::vector<PathDistance> lengths;

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1, /*k=*/10),
               "graph.num_arcs\\(\\) > 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingSourceBecauseNegative) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/-1,
                                 /*destination=*/1, /*k=*/10),
               "source >= 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingSourceBecauseTooLarge) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/1'000,
                                 /*destination=*/1, /*k=*/10),
               "source < graph.num_nodes()");
}

TEST(KShortestPathsYenDeathTest, NonExistingDestinationBecauseNegative) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/-1, /*k=*/10),
               "destination >= 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingDestinationBecauseTooLarge) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1'000, /*k=*/10),
               "destination < graph.num_nodes()");
}

TEST(KShortestPathsYenDeathTest, KEqualsZero) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/2, /*k=*/0),
               "k != 0");
}

TEST(KShortestPathsYenTest, ReducesToShortestPath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
                                                 /*destination=*/2, /*k=*/1);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2));
}

TEST(KShortestPathsYenTest, OnlyHasOnePath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
                                                 /*destination=*/2, /*k=*/10);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2));
}

TEST(KShortestPathsYenTest, HasTwoPaths) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 2);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 30, 1};

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
                                                 /*destination=*/2, /*k=*/10);
  EXPECT_THAT(paths.paths,
              ElementsAre(std::vector<int>{0, 1, 2}, std::vector<int>{0, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2, 30));
}

TEST(KShortestPathsYenTest, HasTwoPathsWithLongerPath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 4);
  graph.AddArc(1, 2);
  graph.AddArc(2, 3);
  graph.AddArc(3, 4);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 30, 1, 1, 1};

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
                                                 /*destination=*/4, /*k=*/10);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2, 3, 4},
                                       std::vector<int>{0, 4}));
  EXPECT_THAT(paths.distances, ElementsAre(4, 30));
}

// TODO(user): randomized tests? Check validity with exhaustive
// exploration/IP formulation?

}  // namespace
}  // namespace operations_research
