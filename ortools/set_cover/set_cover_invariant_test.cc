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

#include "ortools/set_cover/set_cover_invariant.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

using CL = SetCoverInvariant::ConsistencyLevel;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class SetCoverInvariantTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Universe U = {0, 1, 2}
    // Subsets:
    // S0 = {0, 1}, cost 1.0
    // S1 = {1, 2}, cost 2.0
    // S2 = {0, 2}, cost 3.0
    model_.AddEmptySubset(1.0);  // S0
    model_.AddElementToLastSubset(0);
    model_.AddElementToLastSubset(1);

    model_.AddEmptySubset(2.0);  // S1
    model_.AddElementToLastSubset(1);
    model_.AddElementToLastSubset(2);

    model_.AddEmptySubset(3.0);  // S2
    model_.AddElementToLastSubset(0);
    model_.AddElementToLastSubset(2);

    invariant_ = std::make_unique<SetCoverInvariant>(&model_);
    model_.CreateSparseRowView();
  }

  SetCoverModel model_;
  std::unique_ptr<SetCoverInvariant> invariant_;
};

class SetCoverInvariantWithCostsTest
    : public ::testing::TestWithParam<std::vector<double>> {
 protected:
  void SetUp() override {
    const std::vector<double>& costs = GetParam();
    // Universe U = {0, 1, 2}
    // Subsets:
    // S0 = {0, 1}
    // S1 = {1, 2}
    // S2 = {0, 2}
    model_.AddEmptySubset(costs[0]);  // S0
    model_.AddElementToLastSubset(0);
    model_.AddElementToLastSubset(1);

    model_.AddEmptySubset(costs[1]);  // S1
    model_.AddElementToLastSubset(1);
    model_.AddElementToLastSubset(2);

    model_.AddEmptySubset(costs[2]);  // S2
    model_.AddElementToLastSubset(0);
    model_.AddElementToLastSubset(2);

    invariant_ = std::make_unique<SetCoverInvariant>(&model_);
    model_.CreateSparseRowView();
  }

  SetCoverModel model_;
  std::unique_ptr<SetCoverInvariant> invariant_;
};

TEST_P(SetCoverInvariantWithCostsTest, SelectDeselectCostAndCoverage) {
  const std::vector<double>& costs = GetParam();
  ASSERT_TRUE(invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), costs[0]);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(1, 1, 0));
  EXPECT_FALSE(invariant_->is_selected()[SubsetIndex(1)]);

  ASSERT_TRUE(invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), costs[0] + costs[1]);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(1, 2, 1));

  // Already selected
  EXPECT_FALSE(invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage));

  ASSERT_TRUE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), costs[1]);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(0, 1, 1));

  // Already deselected
  EXPECT_FALSE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));
}

TEST_P(SetCoverInvariantWithCostsTest, LoadSolution) {
  const std::vector<double>& costs = GetParam();
  SubsetBoolVector solution = {true, false, true};
  invariant_->LoadSolution(solution);
  invariant_->Recompute(CL::kRedundancy);

  EXPECT_EQ(invariant_->cost(), costs[0] + costs[2]);
  EXPECT_THAT(invariant_->is_selected(), ElementsAre(true, false, true));
  EXPECT_EQ(invariant_->num_uncovered_elements(), 0);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(2, 1, 1));
}

INSTANTIATE_TEST_SUITE_P(AllCosts, SetCoverInvariantWithCostsTest,
                         ::testing::Values(std::vector<double>{1.0, 2.0, 3.0},
                                           std::vector<double>{10.0, 1.0,
                                                               5.0}));

TEST_F(SetCoverInvariantTest, InitialState) {
  EXPECT_EQ(invariant_->cost(), 0.0);
  EXPECT_TRUE(invariant_->is_cost_consistent());
  EXPECT_EQ(invariant_->num_uncovered_elements(), 3);
  EXPECT_THAT(invariant_->is_selected(), ElementsAre(false, false, false));
  EXPECT_THAT(invariant_->coverage(), ElementsAre(0, 0, 0));
  EXPECT_THAT(invariant_->num_free_elements(), ElementsAre(2, 2, 2));
  EXPECT_THAT(invariant_->num_coverage_le_1_elements(), ElementsAre(2, 2, 2));
  EXPECT_THAT(invariant_->is_redundant(), ElementsAre(false, false, false));
  EXPECT_TRUE(invariant_->trace().empty());
}

TEST_F(SetCoverInvariantTest, SelectDeselectCostAndCoverage) {
  ASSERT_TRUE(invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), 1.0);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(1, 1, 0));
  EXPECT_FALSE(invariant_->is_selected()[SubsetIndex(1)]);

  ASSERT_TRUE(invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), 3.0);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(1, 2, 1));

  // Already selected
  EXPECT_FALSE(invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage));

  ASSERT_TRUE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));
  EXPECT_EQ(invariant_->cost(), 2.0);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(0, 1, 1));

  // Already deselected
  EXPECT_FALSE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));
}

TEST_F(SetCoverInvariantTest, DeselectReturnsFalseWhenAlreadyDeselected) {
  // Initially, subset 0 is not selected.
  ASSERT_FALSE(invariant_->is_selected()[SubsetIndex(0)]);
  // Calling Deselect on an unselected subset should return false.
  EXPECT_FALSE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));

  // Select and then deselect subset 0.
  ASSERT_TRUE(invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage));
  ASSERT_TRUE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));

  // The second call to Deselect should return false.
  EXPECT_FALSE(invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage));
}

TEST_F(SetCoverInvariantTest, SelectFreeAndUncovered) {
  ASSERT_TRUE(invariant_->Select(SubsetIndex(0), CL::kFreeAndUncovered));
  EXPECT_EQ(invariant_->num_uncovered_elements(), 1);  // Element 2
  EXPECT_THAT(invariant_->num_free_elements(), ElementsAre(0, 1, 1));

  ASSERT_TRUE(invariant_->Select(SubsetIndex(2), CL::kFreeAndUncovered));
  EXPECT_EQ(invariant_->num_uncovered_elements(), 0);
  EXPECT_THAT(invariant_->num_free_elements(), ElementsAre(0, 0, 0));
}

TEST_F(SetCoverInvariantTest, SelectRedundancy) {
  ASSERT_TRUE(invariant_->Select(SubsetIndex(0), CL::kRedundancy));
  ASSERT_TRUE(invariant_->Select(SubsetIndex(1), CL::kRedundancy));

  // S2 covers {0, 2}. Element 0 is covered by S0, Element 2 is covered by S1.
  // After selecting S2, all elements are covered twice.
  ASSERT_TRUE(invariant_->Select(SubsetIndex(2), CL::kRedundancy));
  EXPECT_THAT(invariant_->is_redundant(), ElementsAre(true, true, true));
  EXPECT_THAT(
      invariant_->newly_removable_subsets(),
      UnorderedElementsAre(SubsetIndex(0), SubsetIndex(1), SubsetIndex(2)));

  ASSERT_TRUE(invariant_->Deselect(SubsetIndex(0), CL::kRedundancy));
  EXPECT_THAT(invariant_->is_redundant(), ElementsAre(false, false, false));
  EXPECT_THAT(invariant_->newly_non_removable_subsets(),
              UnorderedElementsAre(SubsetIndex(1), SubsetIndex(2)));
}

TEST_F(SetCoverInvariantTest, LoadSolution) {
  SubsetBoolVector solution = {true, false, true};
  invariant_->LoadSolution(solution);
  invariant_->Recompute(CL::kRedundancy);

  EXPECT_EQ(invariant_->cost(), 4.0);
  EXPECT_THAT(invariant_->is_selected(), ElementsAre(true, false, true));
  EXPECT_EQ(invariant_->num_uncovered_elements(), 0);
  EXPECT_THAT(invariant_->coverage(), ElementsAre(2, 1, 1));
}

TEST_F(SetCoverInvariantTest, CompressTrace) {
  invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage);
  invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage);
  invariant_->Deselect(SubsetIndex(0), CL::kCostAndCoverage);
  invariant_->Select(SubsetIndex(2), CL::kCostAndCoverage);
  invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage);

  invariant_->CompressTrace();
  const auto& trace = invariant_->trace();
  ASSERT_EQ(trace.size(), 3);
  std::vector<SubsetIndex> subsets_in_trace;
  for (const auto& decision : trace) {
    EXPECT_TRUE(decision.decision());
    subsets_in_trace.push_back(decision.subset());
  }
  EXPECT_THAT(
      subsets_in_trace,
      UnorderedElementsAre(SubsetIndex(0), SubsetIndex(1), SubsetIndex(2)));
}

TEST_F(SetCoverInvariantTest, CheckConsistency) {
  invariant_->Select(SubsetIndex(0), CL::kRedundancy);
  EXPECT_TRUE(invariant_->CheckConsistency(CL::kRedundancy));

  invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage);
  EXPECT_TRUE(invariant_->CheckConsistency(CL::kCostAndCoverage));
  // Not fully consistent at higher level after a partial update.
  EXPECT_DEATH(invariant_->CheckConsistency(CL::kFreeAndUncovered), "");
}

TEST_F(SetCoverInvariantTest, CheckTimestamp) {
  int64_t current_timestamp = model_.timestamp();

  // Test SetSubsetCost, which changes a single cost.
  model_.SetSubsetCost(SubsetIndex(0), 100.0);
  EXPECT_GT(model_.timestamp(), current_timestamp);
  current_timestamp = model_.timestamp();

  // Test set_subset_costs, which changes the whole vector.
  SubsetCostVector new_costs = model_.subset_costs();
  new_costs[SubsetIndex(1)] = 200.0;
  model_.set_subset_costs(new_costs);
  EXPECT_GT(model_.timestamp(), current_timestamp);
  current_timestamp = model_.timestamp();

  // Test AddEmptySubset, which adds a new subset with a cost.
  model_.AddEmptySubset(300.0);
  EXPECT_GT(model_.timestamp(), current_timestamp);
}

TEST_F(SetCoverInvariantTest, Recompute) {
  invariant_->Select(SubsetIndex(0), CL::kCostAndCoverage);
  invariant_->Select(SubsetIndex(1), CL::kCostAndCoverage);

  // At this point, only cost and coverage are guaranteed to be consistent.
  EXPECT_DEATH(invariant_->CheckConsistency(CL::kFreeAndUncovered), "");

  invariant_->Recompute(CL::kRedundancy);
  EXPECT_TRUE(invariant_->CheckConsistency(CL::kRedundancy));
  EXPECT_EQ(invariant_->num_uncovered_elements(), 0);
  EXPECT_THAT(invariant_->num_free_elements(), ElementsAre(0, 0, 0));
  EXPECT_THAT(invariant_->is_redundant(), ElementsAre(false, false, false));
}

TEST_F(SetCoverInvariantTest, ModelAndInvariantTimestampTest) {
  SetCoverInvariant inv1(&model_);
  EXPECT_EQ(inv1.timestamp(), model_.timestamp());
  model_.SetSubsetCost(SubsetIndex(0), 10.0);
  SetCoverInvariant inv2(&model_);
  EXPECT_EQ(inv2.timestamp(), model_.timestamp());
  EXPECT_NE(inv1.timestamp(), inv2.timestamp());
}

}  // namespace
}  // namespace operations_research
