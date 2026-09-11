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

#include "ortools/packing/arc_flow_solver.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/packing/vector_bin_packing.pb.h"

namespace operations_research {
namespace packing {
namespace {

using google::protobuf::contrib::parse_proto::ParseTextProto;
using testing::EqualsProto;

TEST(ArcFlowSolverTest, TestMinimizesBinsByDefault) {
  // With no objective information, minimize the number of bins.
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 5, 2 ]
                         item {
                           resource_usage: [ 1, 1 ]
                           num_copies: 5
                         }
                         item {
                           resource_usage: [ 2, 0 ]
                           num_copies: 10
                         }
                       )pb"));

  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/0);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 5);
  EXPECT_EQ(solution.objective_value(), 5);
  EXPECT_THAT(solution.bins(), testing::Each(EqualsProto(R"pb(
                item_indices: [ 0, 1 ]
                item_copies: [ 1, 2 ]
              )pb")));
}

TEST(ArcFlowSolverTest, TestMaxNumBins) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 5, 2 ]
                         cost_per_bin: 0
                         item {
                           resource_usage: [ 1, 1 ]
                           num_optional_copies: 6
                           penalty_per_missing_copy: 2
                         }
                         item {
                           resource_usage: [ 2, 0 ]
                           num_optional_copies: 10
                           penalty_per_missing_copy: 1
                         }
                       )pb"));

  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/3);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 3);
  EXPECT_EQ(solution.objective_value(), 7);
  EXPECT_THAT(solution.bins(), testing::Each(EqualsProto(R"pb(
                item_indices: [ 0, 1 ]
                item_copies: [ 2, 1 ]
              )pb")));
}

TEST(ArcFlowSolverTest, TestMaxNumBinsInfeasible) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 5, 2 ]
                         item {
                           resource_usage: [ 1, 1 ]
                           num_copies: 6
                         }
                         item {
                           resource_usage: [ 2, 0 ]
                           num_copies: 10
                         }
                       )pb"));

  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/5);

  EXPECT_EQ(solution.status(), vbp::INFEASIBLE);
  EXPECT_EQ(solution.bins_size(), 0);
}

TEST(ArcFlowSolverTest, TestMaxCopiesPerBin) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 5 ]
                         cost_per_bin: 1
                         item {
                           resource_usage: [ 1 ]
                           num_copies: 6
                           max_number_of_copies_per_bin: 2
                         }
                         item {
                           resource_usage: [ 2, 0 ]
                           num_copies: 1
                         }
                       )pb"));

  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/0);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 3);
  EXPECT_THAT(solution.bins(),
              testing::UnorderedElementsAre(EqualsProto(R"pb(
                                              item_indices: [ 0, 1 ]
                                              item_copies: [ 2, 1 ]
                                            )pb"),
                                            EqualsProto(R"pb(
                                              item_indices: [ 0 ]
                                              item_copies: [ 2 ]
                                            )pb"),
                                            EqualsProto(R"pb(
                                              item_indices: [ 0 ]
                                              item_copies: [ 2 ]
                                            )pb")));
}

TEST(ArcFlowSolverTest, TestWeightedItemAndBinCost) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 6, 3 ]
                         item {
                           resource_usage: [ 1, 1 ]
                           num_optional_copies: 5
                           penalty_per_missing_copy: 2
                         }
                         item {
                           resource_usage: [ 2, 0 ]
                           num_optional_copies: 3
                           penalty_per_missing_copy: 2
                         }
                         cost_per_bin: 7
                       )pb"));

  // It's not worth filling a bin with either item alone.
  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/0);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 2);
  EXPECT_EQ(solution.objective_value(), 2 * 7);
  EXPECT_THAT(solution.bins(),
              testing::UnorderedElementsAre(EqualsProto(R"pb(
                                              item_indices: [ 0, 1 ]
                                              item_copies: [ 3, 1 ]
                                            )pb"),
                                            EqualsProto(R"pb(
                                              item_indices: [ 0, 1 ]
                                              item_copies: [ 2, 2 ]
                                            )pb")));
}

TEST(ArcFlowSolverTest, TestCanEmitNonMaximalBins) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 6, 3 ]
                         item {
                           resource_usage: [ 1, 1 ]
                           num_optional_copies: 5
                           penalty_per_missing_copy: 2
                         }
                         item {
                           resource_usage: [ 3, 0 ]
                           num_optional_copies: 10
                           penalty_per_missing_copy: 1
                         }
                         cost_per_bin: 3
                       )pb"));

  // It's not worth putting any of the second item in any bin, even though there
  // is space.
  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/0);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 2);
  EXPECT_EQ(solution.objective_value(), 2 * 3 + 8);
}

TEST(ArcFlowSolverTest, TestMixedRequiredAndOptionalItem) {
  ASSERT_OK_AND_ASSIGN(const auto problem,
                       ParseTextProto<vbp::VectorBinPackingProblem>(R"pb(
                         resource_capacity: [ 6, 3 ]
                         item {
                           resource_usage: [ 1, 1 ]
                           num_copies: 3
                           penalty_per_missing_copy: 2
                         }
                         item {
                           resource_usage: [ 3, 0 ]
                           num_copies: 3
                           num_optional_copies: 10
                           penalty_per_missing_copy: 1
                         }
                         cost_per_bin: 3
                       )pb"));

  // It's not worth putting any of the second item in any bin, even though there
  // is space.
  auto solution = SolveVectorBinPackingWithArcFlow(
      problem, MPSolver::SAT_INTEGER_PROGRAMMING, /*mip_params = */ "",
      /*time_limit=*/30, /*num_threads=*/1, /*max_bins=*/0);

  EXPECT_EQ(solution.status(), vbp::OPTIMAL);
  EXPECT_EQ(solution.bins_size(), 2);
  EXPECT_EQ(solution.objective_value(), 2 * 3 + 10);
  EXPECT_THAT(solution.bins(),
              testing::UnorderedElementsAre(EqualsProto(R"pb(
                                              item_indices: [ 0, 1 ]
                                              item_copies: [ 3, 1 ]
                                            )pb"),
                                            EqualsProto(R"pb(
                                              item_indices: [ 1 ]
                                              item_copies: [ 2 ]
                                            )pb")));
}
}  // namespace
}  // namespace packing
}  // namespace operations_research
