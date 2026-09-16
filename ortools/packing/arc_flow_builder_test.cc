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

// Test the arc_flow builder algorithm.
// We only have golden tests here.
// We will test the correctness of the generated arc-flow graph along the
// vector bin packing solver.

#include "ortools/packing/arc_flow_builder.h"

#include <vector>

#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace packing {
namespace {

TEST(ArcFlowBuilderTest, Small) {
  std::vector<int> capacities = {9, 3};
  std::vector<int> demands = {1, 3, 1};
  std::vector<std::vector<int>> shapes = {{4, 1}, {3, 1}, {2, 1}};

  const ArcFlowGraph result = BuildArcFlowGraph(capacities, shapes, demands);
  // The results are those of the arc_flow poster.
  EXPECT_EQ(6, result.nodes.size());
  EXPECT_EQ(14, result.arcs.size());
  EXPECT_EQ(19, result.num_dp_states);
}

TEST(ArcFlowBuilderTest, Medium) {
  // Contains reused states, need the topological sorted.
  std::vector<int> capacities = {1000, 1000};
  std::vector<int> demands(25, 1);
  std::vector<std::vector<int>> shapes = {
      {359, 225}, {353, 388}, {334, 236}, {333, 396}, {313, 169},
      {306, 123}, {303, 314}, {301, 105}, {298, 131}, {285, 367},
      {281, 109}, {272, 297}, {249, 123}, {232, 175}, {232, 105},
      {229, 343}, {203, 315}, {191, 334}, {160, 182}, {139, 110},
      {137, 132}, {136, 220}, {135, 132}, {125, 111}, {113, 166}};

  const ArcFlowGraph result = BuildArcFlowGraph(capacities, shapes, demands);
  EXPECT_EQ(271, result.nodes.size());
  EXPECT_EQ(1657, result.arcs.size());
  EXPECT_EQ(43896, result.num_dp_states);
}

TEST(ArcFlowBuilderTest, Large) {
  std::vector<int> capacities = {150};
  std::vector<int> demands(120, 1);
  std::vector<std::vector<int>> shapes = {
      {98}, {98}, {98}, {96}, {96}, {94}, {93}, {93}, {92}, {91}, {91}, {90},
      {87}, {86}, {85}, {85}, {84}, {84}, {84}, {84}, {84}, {83}, {83}, {82},
      {82}, {81}, {80}, {80}, {80}, {79}, {79}, {78}, {78}, {78}, {78}, {76},
      {74}, {74}, {73}, {73}, {73}, {73}, {72}, {71}, {70}, {70}, {70}, {69},
      {69}, {69}, {67}, {66}, {64}, {62}, {62}, {60}, {60}, {59}, {58}, {58},
      {58}, {57}, {57}, {57}, {57}, {55}, {55}, {55}, {50}, {49}, {49}, {49},
      {47}, {46}, {46}, {45}, {45}, {44}, {44}, {43}, {43}, {43}, {43}, {42},
      {42}, {42}, {42}, {42}, {41}, {41}, {41}, {39}, {39}, {38}, {38}, {38},
      {37}, {36}, {36}, {36}, {35}, {33}, {33}, {33}, {32}, {32}, {30}, {30},
      {30}, {29}, {28}, {27}, {27}, {26}, {25}, {25}, {24}, {23}, {23}, {20}};

  const ArcFlowGraph result = BuildArcFlowGraph(capacities, shapes, demands);
  EXPECT_EQ(94, result.nodes.size());
  EXPECT_EQ(3316, result.arcs.size());
  EXPECT_EQ(11241, result.num_dp_states);
}

}  // namespace
}  // namespace packing
}  // namespace operations_research
