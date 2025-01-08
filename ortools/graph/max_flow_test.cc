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

#include "ortools/graph/max_flow.h"

#include <limits>

#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"
#include "ortools/graph/flow_problem.pb.h"
#include "ortools/util/file_util.h"

namespace operations_research {
namespace {

#if not defined(ROOT_DIR)
#define ROOT_DIR "_main/"
#endif

TEST(SimpleMaxFlowTest, EmptyWithValidSourceAndSink) {
  SimpleMaxFlow max_flow;
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 1));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(1, 0));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(0, max_flow.NumArcs());
  EXPECT_EQ(0, max_flow.NumNodes());
}

TEST(SimpleMaxFlowTest, ArcFlowIsZeroOnDisconnectedGraph) {
  SimpleMaxFlow max_flow;
  max_flow.AddArcWithCapacity(0, 1, 10);
  max_flow.AddArcWithCapacity(1, 0, 10);
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 2));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(0, max_flow.Flow(0));
  EXPECT_EQ(0, max_flow.Flow(1));
}

TEST(SimpleMaxFlowTest, EmptyWithInvalidSourceAndSink) {
  SimpleMaxFlow max_flow;
  EXPECT_EQ(SimpleMaxFlow::BAD_INPUT, max_flow.Solve(0, 0));
  EXPECT_EQ(SimpleMaxFlow::BAD_INPUT, max_flow.Solve(-1, 10));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(0, max_flow.NumArcs());
  EXPECT_EQ(0, max_flow.NumNodes());
}

TEST(SimpleMaxFlowTest, TrivialGraphWithMaxCapacity) {
  SimpleMaxFlow max_flow;
  const FlowQuantity kCapacityMax = std::numeric_limits<FlowQuantity>::max();
  EXPECT_EQ(0, max_flow.AddArcWithCapacity(0, 1, kCapacityMax));
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(1, 0));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 1));
  EXPECT_EQ(kCapacityMax, max_flow.OptimalFlow());
  EXPECT_EQ(1, max_flow.NumArcs());
  EXPECT_EQ(2, max_flow.NumNodes());
  EXPECT_EQ(kCapacityMax, max_flow.Flow(0));
}

TEST(SimpleMaxFlowTest, TrivialGraphWithRepeatedArc) {
  SimpleMaxFlow max_flow;
  EXPECT_EQ(0, max_flow.AddArcWithCapacity(0, 1, 10));
  EXPECT_EQ(1, max_flow.AddArcWithCapacity(0, 1, 10));
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(1, 0));
  EXPECT_EQ(0, max_flow.OptimalFlow());
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 1));
  EXPECT_EQ(20, max_flow.OptimalFlow());
  EXPECT_EQ(2, max_flow.NumArcs());
  EXPECT_EQ(2, max_flow.NumNodes());
  EXPECT_EQ(10, max_flow.Flow(0));
  EXPECT_EQ(10, max_flow.Flow(1));
}

TEST(SimpleMaxFlowTest, SelfLoop) {
  SimpleMaxFlow max_flow;
  EXPECT_EQ(0, max_flow.AddArcWithCapacity(0, 0, 10));
  EXPECT_EQ(1, max_flow.AddArcWithCapacity(0, 1, 10));
  EXPECT_EQ(2, max_flow.AddArcWithCapacity(1, 1, 10));
  EXPECT_EQ(3, max_flow.AddArcWithCapacity(1, 2, 10));
  EXPECT_EQ(4, max_flow.AddArcWithCapacity(2, 2, 10));
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 2));
  EXPECT_EQ(10, max_flow.OptimalFlow());
}

TEST(SimpleMaxFlowTest, SetArcCapacity) {
  SimpleMaxFlow max_flow;
  // Graph:  0------[10]-->1--[15]->3
  //          \            ^        ^
  //           \          [3]       |
  //            \          |        |
  //             `--[15]-->2--[10]--'
  //
  // Max flow is saturated by the 2->1 arc: it's 23.
  max_flow.AddArcWithCapacity(0, 1, 10);
  max_flow.AddArcWithCapacity(0, 2, 15);
  const int arc21 = max_flow.AddArcWithCapacity(2, 1, 3);
  max_flow.AddArcWithCapacity(2, 3, 10);
  max_flow.AddArcWithCapacity(1, 3, 15);
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 3));
  EXPECT_EQ(23, max_flow.OptimalFlow());
  max_flow.SetArcCapacity(arc21, 10);
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, max_flow.Solve(0, 3));
  EXPECT_EQ(25, max_flow.OptimalFlow());
}

SimpleMaxFlow::Status LoadAndSolveFlowModel(const FlowModelProto& model,
                                            SimpleMaxFlow* solver) {
  for (int a = 0; a < model.arcs_size(); ++a) {
    solver->AddArcWithCapacity(model.arcs(a).tail(), model.arcs(a).head(),
                               model.arcs(a).capacity());
  }
  int source, sink;
  for (int n = 0; n < model.nodes_size(); ++n) {
    if (model.nodes(n).supply() == 1) source = model.nodes(n).id();
    if (model.nodes(n).supply() == -1) sink = model.nodes(n).id();
  }
  return solver->Solve(source, sink);
}

TEST(SimpleMaxFlowTest, CreateFlowModelProto) {
  SimpleMaxFlow solver1;
  solver1.AddArcWithCapacity(0, 1, 10);
  solver1.AddArcWithCapacity(0, 2, 10);
  solver1.AddArcWithCapacity(1, 2, 5);
  solver1.AddArcWithCapacity(2, 3, 15);

  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, solver1.Solve(0, 3));
  EXPECT_EQ(15, solver1.OptimalFlow());
  const FlowModelProto model_proto = solver1.CreateFlowModelProto(0, 3);

  // Load the model_proto in another solver, and check that the solve and that
  // the new dump is the same.
  SimpleMaxFlow solver2;
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL,
            LoadAndSolveFlowModel(model_proto, &solver2));
  EXPECT_EQ(15, solver2.OptimalFlow());
  EXPECT_THAT(model_proto,
              ::testing::EqualsProto(solver2.CreateFlowModelProto(0, 3)));

  // Check that the proto is what we expect it is.
  FlowModelProto expected;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        problem_type: MAX_FLOW
        nodes { id: 0 supply: 1 }
        nodes { id: 1 }
        nodes { id: 2 }
        nodes { id: 3 supply: -1 }
        arcs { tail: 0 head: 1 capacity: 10 }
        arcs { tail: 0 head: 2 capacity: 10 }
        arcs { tail: 1 head: 2 capacity: 5 }
        arcs { tail: 2 head: 3 capacity: 15 }
      )pb",
      &expected);
  EXPECT_THAT(model_proto, testing::EqualsProto(expected));
}

// A problem that was triggering a issue on 28/11/2014 (now fixed).
TEST(SimpleMaxFlowTest, ProblematicProblemWithMaxCapacity) {
  ASSERT_OK_AND_ASSIGN(
      FlowModelProto model,
      ReadFileToProto<FlowModelProto>(file::JoinPathRespectAbsolute(
          ::testing::SrcDir(), ROOT_DIR "ortools/graph/testdata/"
                                        "max_flow_test1.pb.txt")));
  SimpleMaxFlow solver;
  EXPECT_EQ(SimpleMaxFlow::OPTIMAL, LoadAndSolveFlowModel(model, &solver));
  EXPECT_EQ(10290243, solver.OptimalFlow());
}

}  // namespace
}  // namespace operations_research
