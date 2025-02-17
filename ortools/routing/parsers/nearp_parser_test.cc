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

#include "ortools/routing/parsers/nearp_parser.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/path.h"
#include "ortools/routing/parsers/simple_graph.h"

#define ROOT_DIR "_main/"

namespace operations_research::routing {
namespace {
TEST(NearpParserTest, Constructor) {
  NearpParser parser;
  EXPECT_EQ(parser.name(), "");
  EXPECT_EQ(parser.comment(), "");
  EXPECT_EQ(parser.NumberOfNodes(), 0);
  EXPECT_EQ(parser.NumberOfEdgesWithServicing(), 0);
  EXPECT_EQ(parser.NumberOfEdgesWithoutServicing(), 0);
  EXPECT_EQ(parser.NumberOfEdges(), 0);
  EXPECT_EQ(parser.NumberOfVehicles(), 0);
  EXPECT_EQ(parser.capacity(), 0);
  EXPECT_EQ(parser.depot(), 0);
}

TEST(NearpParserTest, LoadEmptyFileName) {
  std::string empty_file_name;
  NearpParser parser;
  EXPECT_FALSE(parser.LoadFile(empty_file_name));
}

TEST(NearpParserTest, LoadNonExistingFile) {
  NearpParser parser;
  EXPECT_FALSE(parser.LoadFile("google2/nonexistent.dat"));
}

TEST(NearpParserTest, LoadBHW1) {
  std::string file_name = file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                         "ortools/routing/parsers/testdata/"
                                         "nearp_BHW1.dat");
  NearpParser parser;
  EXPECT_TRUE(parser.LoadFile(file_name));
  EXPECT_EQ(parser.name(), "BHW1");
  EXPECT_EQ(parser.comment(), "-1");
  EXPECT_EQ(parser.NumberOfNodes(), 12);
  EXPECT_EQ(parser.NumberOfNodesWithServicing(), 7);
  EXPECT_EQ(parser.NumberOfNodesWithoutServicing(), 5);
  EXPECT_EQ(parser.NumberOfEdges(), 11);
  EXPECT_EQ(parser.NumberOfEdgesWithServicing(), 11);
  EXPECT_EQ(parser.NumberOfEdgesWithoutServicing(), 0);
  EXPECT_EQ(parser.NumberOfArcs(), 22);
  EXPECT_EQ(parser.NumberOfArcsWithServicing(), 11);
  EXPECT_EQ(parser.NumberOfArcsWithoutServicing(), 11);
  EXPECT_EQ(parser.NumberOfVehicles(), -1);
  EXPECT_EQ(parser.capacity(), 5);
  EXPECT_EQ(parser.depot(), 0);

  EXPECT_EQ(parser.arc_traversing_costs().size(), 22);
  EXPECT_EQ(parser.arc_servicing_costs().size(), 11);
  EXPECT_EQ(parser.arc_servicing_demands().size(), 11);
  EXPECT_EQ(parser.edge_traversing_costs().size(), 11);
  EXPECT_EQ(parser.edge_servicing_demands().size(), 11);
  EXPECT_EQ(parser.edge_servicing_costs().size(), 11);
  EXPECT_EQ(parser.node_servicing_demands().size(), 7);
  EXPECT_EQ(parser.node_servicing_costs().size(), 7);

  EXPECT_EQ(parser.GetArcName(0, 1), "A1");
  EXPECT_EQ(parser.GetArcName(Arc(0, 1)), "A1");
  EXPECT_EQ(parser.GetArcName(3, 0), "NrA2");
  EXPECT_EQ(parser.GetArcName(Arc(3, 0)), "NrA2");
  EXPECT_EQ(parser.GetEdgeName(2, 1), "E1");
  EXPECT_EQ(parser.GetEdgeName(Edge(2, 1)), "E1");
  EXPECT_EQ(parser.GetEdgeName(1, 2), "E1");
  EXPECT_EQ(parser.GetEdgeName(Edge(1, 2)), "E1");
  EXPECT_EQ(parser.GetNodeName(3), "N4");
}

TEST(NearpParserTest, LoadToy) {
  std::string file_name = file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                         "ortools/routing/parsers/"
                                         "testdata/nearp_toy.dat");
  NearpParser parser;
  EXPECT_TRUE(parser.LoadFile(file_name));
  EXPECT_EQ(parser.name(), "Toy");
  EXPECT_EQ(parser.comment(), "-1");
  EXPECT_EQ(parser.NumberOfNodes(), 4);
  EXPECT_EQ(parser.NumberOfNodesWithServicing(), 1);
  EXPECT_EQ(parser.NumberOfNodesWithoutServicing(), 3);
  EXPECT_EQ(parser.NumberOfEdges(), 3);
  EXPECT_EQ(parser.NumberOfEdgesWithServicing(), 2);
  EXPECT_EQ(parser.NumberOfEdgesWithoutServicing(), 1);
  EXPECT_EQ(parser.NumberOfArcs(), 3);
  EXPECT_EQ(parser.NumberOfArcsWithServicing(), 2);
  EXPECT_EQ(parser.NumberOfArcsWithoutServicing(), 1);
  EXPECT_EQ(parser.NumberOfVehicles(), -1);
  EXPECT_EQ(parser.capacity(), 5);
  EXPECT_EQ(parser.depot(), 0);

  EXPECT_EQ(parser.arc_traversing_costs().size(), 3);
  EXPECT_EQ(parser.arc_servicing_costs().size(), 2);
  EXPECT_EQ(parser.arc_servicing_demands().size(), 2);
  EXPECT_EQ(parser.edge_traversing_costs().size(), 3);
  EXPECT_EQ(parser.edge_servicing_demands().size(), 2);
  EXPECT_EQ(parser.edge_servicing_costs().size(), 2);
  EXPECT_EQ(parser.node_servicing_demands().size(), 1);
  EXPECT_EQ(parser.node_servicing_costs().size(), 1);

  EXPECT_DEATH(parser.GetArcName(0, 1), "");
  EXPECT_DEATH(parser.GetArcName(3, 0), "");
  EXPECT_DEATH(parser.GetEdgeName(3, 1), "");
  EXPECT_DEATH(parser.GetEdgeName(1, 3), "");

  EXPECT_EQ(parser.GetArcName(1, 3), "A1");
  EXPECT_EQ(parser.GetArcName(3, 1), "NrA1");
  EXPECT_EQ(parser.GetEdgeName(2, 1), "E2");
  EXPECT_EQ(parser.GetEdgeName(1, 2), "E2");
  EXPECT_EQ(parser.GetNodeName(3), "N4");
}
}  // namespace
}  // namespace operations_research::routing
