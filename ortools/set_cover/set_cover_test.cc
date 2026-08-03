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

#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_mip.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {
using google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using CL = SetCoverInvariant::ConsistencyLevel;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::Pointwise;

TEST(SetCoverTest, GuidedLocalSearchVerySmall) {
  SetCoverProto proto = ParseTextProtoOrDie(R"pb(
    subset { cost: 1 element: 1 element: 2 }
    subset { cost: 1 element: 0 })pb");

  SetCoverModel model;
  model.ImportModelFromProto(proto);
  CHECK(model.ComputeFeasibility());
  SetCoverInvariant inv(&model);
  GreedySolutionOptimizer greedy_search(&inv);
  CHECK(greedy_search.Optimize());
  CHECK(inv.CheckConsistency(CL::kFreeAndUncovered));
  GuidedLocalSearch search(&inv);
  CHECK(search.SetMaxIterations(100).Optimize());
  CHECK(inv.CheckConsistency(CL::kRedundancy));
}

TEST(SetCoverTest, InitialValues) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(2);
  EXPECT_TRUE(model.ComputeFeasibility());

  SetCoverInvariant inv(&model);
  TrivialSolutionGenerator trivial(&inv);
  CHECK(trivial.Optimize());
  LOG(INFO) << "TrivialSolutionGenerator cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kCostAndCoverage));

  GreedySolutionOptimizer greedy(&inv);
  EXPECT_TRUE(greedy.Optimize());
  LOG(INFO) << "GreedySolutionOptimizer cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));

  EXPECT_EQ(inv.num_uncovered_elements(), 0);
  SteepestSearch steepest(&inv);
  CHECK(steepest.SetMaxIterations(500).Optimize());
  LOG(INFO) << "SteepestSearch cost: " << inv.cost();
  EXPECT_TRUE(inv.CheckConsistency(CL::kFreeAndUncovered));
}

TEST(SetCoverTest, Infeasible) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(3);
  EXPECT_FALSE(model.ComputeFeasibility());
}

TEST(SetCoverTest, FractionalSolution) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(2);
  model.AddElementToLastSubset(0);
  SetCoverInvariant inv(&model);
  SetCoverMip mip(&inv);

  mip.UseMipSolver(SetCoverMipSolver::GLOP).SetTimeLimit(absl::Seconds(1));
  mip.Optimize();

  EXPECT_THAT(mip.solution_weights(), Pointwise(DoubleEq(), {0.5, 0.5, 0.5}));
  EXPECT_THAT(inv.LowerBound(), DoubleEq(1.5));
}

TEST(SetCoverTest, MipErasePreviousSubsets) {
  SetCoverProto proto = ParseTextProtoOrDie(R"pb(
    subset { cost: 1 element: 0 element: 1 }
    subset { cost: 1 element: 0 }
    subset { cost: 1 element: 1 })pb");
  SetCoverModel model;
  model.ImportModelFromProto(proto);
  SetCoverInvariant inv(&model);
  inv.Select(SubsetIndex(1), CL::kCostAndCoverage);
  inv.Select(SubsetIndex(2), CL::kCostAndCoverage);
  SetCoverMip mip(&inv);
  mip.UseIntegers(true).SetTimeLimit(absl::Milliseconds(500));
  mip.Optimize();

  EXPECT_THAT(inv.is_selected(), ElementsAre(true, false, false));
}

}  // namespace
}  // namespace operations_research
