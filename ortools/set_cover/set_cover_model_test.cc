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

#include "ortools/set_cover/set_cover_model.h"

#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(SetCoverModelEmptyTest, EmptyModel) {
  SetCoverModel model;
  EXPECT_TRUE(model.IsEmpty());
  EXPECT_EQ(model.num_elements(), 0);
  EXPECT_EQ(model.num_subsets(), 0);
  EXPECT_EQ(model.num_nonzeros(), 0);
  EXPECT_EQ(model.name(), "SetCoverModel");
  model.SetName("MyEmptyModel");
  EXPECT_EQ(model.name(), "MyEmptyModel");
}

SetCoverModel CreateSmallModel() {
  SetCoverModel model;
  // Universe U = {0, 1, 2}
  // Subsets:
  // S0 = {0, 1}, cost 1.0
  // S1 = {1, 2}, cost 2.0
  // S2 = {0, 2}, cost 3.0
  model.AddEmptySubset(1.0);  // S0
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);

  model.AddEmptySubset(2.0);  // S1
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);

  model.AddEmptySubset(3.0);  // S2
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(2);

  return model;
}

TEST(SetCoverModelTest, InitialState) {
  SetCoverModel model = CreateSmallModel();
  EXPECT_FALSE(model.IsEmpty());
  EXPECT_EQ(model.num_elements(), 3);
  EXPECT_EQ(model.num_subsets(), 3);
  EXPECT_EQ(model.num_nonzeros(), 6);
  EXPECT_NEAR(model.FillRate(), 6.0 / 9.0, 1e-9);

  const auto& costs = model.subset_costs();
  ASSERT_EQ(costs.size(), 3);
  EXPECT_EQ(costs[SubsetIndex(0)], 1.0);
  EXPECT_EQ(costs[SubsetIndex(1)], 2.0);
  EXPECT_EQ(costs[SubsetIndex(2)], 3.0);
}

TEST(SetCoverModelTest, CreateSparseRowView) {
  SetCoverModel model = CreateSmallModel();
  model.CreateSparseRowView();
  EXPECT_TRUE(model.row_view_is_valid());

  const auto& rows = model.rows();
  ASSERT_EQ(rows.size(), 3);

  EXPECT_THAT(rows[ElementIndex(0)],
              ElementsAre(SubsetIndex(0), SubsetIndex(2)));
  EXPECT_THAT(rows[ElementIndex(1)],
              ElementsAre(SubsetIndex(0), SubsetIndex(1)));
  EXPECT_THAT(rows[ElementIndex(2)],
              ElementsAre(SubsetIndex(1), SubsetIndex(2)));
}

TEST(SetCoverModelTest, SortElementsInSubsets) {
  SetCoverModel unsorted_model;
  unsorted_model.AddEmptySubset(1.0);
  unsorted_model.AddElementToLastSubset(1);
  unsorted_model.AddElementToLastSubset(0);

  unsorted_model.SortElementsInSubsets();

  const auto& columns = unsorted_model.columns();
  ASSERT_EQ(columns.size(), 1);
  EXPECT_THAT(columns[SubsetIndex(0)],
              ElementsAre(ElementIndex(0), ElementIndex(1)));
}

TEST(SetCoverModelTest, AddElementToSubset) {
  SetCoverModel model = CreateSmallModel();
  model.AddElementToSubset(2, 0);  // Add element 2 to subset 0
  model.SortElementsInSubsets();
  EXPECT_THAT(model.columns()[SubsetIndex(0)],
              ElementsAre(ElementIndex(0), ElementIndex(1), ElementIndex(2)));

  model.AddElementToSubset(ElementIndex(1),
                           SubsetIndex(2));  // Add element 1 to subset 2
  model.SortElementsInSubsets();
  EXPECT_THAT(model.columns()[SubsetIndex(2)],
              ElementsAre(ElementIndex(0), ElementIndex(1), ElementIndex(2)));
  EXPECT_EQ(model.num_nonzeros(), 8);
}

TEST(SetCoverModelTest, Feasibility) {
  SetCoverModel model = CreateSmallModel();
  EXPECT_TRUE(model.ComputeFeasibility());
}

TEST(SetCoverModelTest, ResizeNumSubsets) {
  SetCoverModel model = CreateSmallModel();
  model.ResizeNumSubsets(5);
  EXPECT_EQ(model.num_subsets(), 5);
  EXPECT_EQ(model.subset_costs().size(), 5);
  EXPECT_EQ(model.columns().size(), 5);
  EXPECT_EQ(model.subset_costs()[SubsetIndex(3)], 0.0);
  EXPECT_EQ(model.subset_costs()[SubsetIndex(4)], 0.0);
  EXPECT_TRUE(model.columns()[SubsetIndex(3)].empty());
  EXPECT_TRUE(model.columns()[SubsetIndex(4)].empty());

  // Resizing to a smaller size should not do anything.
  model.ResizeNumSubsets(2);
  EXPECT_EQ(model.num_subsets(), 5);

  model.ResizeNumSubsets(SubsetIndex(6));
  EXPECT_EQ(model.num_subsets(), 6);
}

TEST(SetCoverModelLogicTest, InfeasibleModel) {
  SetCoverModel model;
  model.AddEmptySubset(1.0);        // S0
  model.AddElementToLastSubset(0);  // element 0 is covered

  model.AddEmptySubset(1.0);        // S1
  model.AddElementToLastSubset(2);  // element 2 is covered

  // num_elements will be 3. Element 1 is not covered by any subset.
  EXPECT_FALSE(model.ComputeFeasibility());
}

TEST(SetCoverModelTest, IsUnicost) {
  SetCoverModel model = CreateSmallModel();
  EXPECT_FALSE(model.is_unicost());

  SetCoverModel unicost_model;
  unicost_model.AddEmptySubset(1.0);
  unicost_model.AddEmptySubset(1.0);
  EXPECT_TRUE(unicost_model.is_unicost());
  unicost_model.SetSubsetCost(SubsetIndex(0), 2.0);
  EXPECT_FALSE(unicost_model.is_unicost());
}

TEST(SetCoverModelTest, SetSubsetCosts) {
  SetCoverModel model = CreateSmallModel();
  SubsetCostVector new_costs(3);
  new_costs[SubsetIndex(0)] = 10.0;
  new_costs[SubsetIndex(1)] = 20.0;
  new_costs[SubsetIndex(2)] = 30.0;
  model.set_subset_costs(new_costs);
  const auto& costs = model.subset_costs();
  ASSERT_EQ(costs.size(), 3);
  EXPECT_EQ(costs[SubsetIndex(0)], 10.0);
  EXPECT_EQ(costs[SubsetIndex(1)], 20.0);
  EXPECT_EQ(costs[SubsetIndex(2)], 30.0);
}

TEST(SetCoverModelTest, SetSubsetCost) {
  SetCoverModel model = CreateSmallModel();
  model.SetSubsetCost(0, 10.0);
  EXPECT_EQ(model.subset_costs()[SubsetIndex(0)], 10.0);
  model.SetSubsetCost(SubsetIndex(1), 20.0);
  EXPECT_EQ(model.subset_costs()[SubsetIndex(1)], 20.0);
}

TEST(SetCoverModelTest, ProtoExportImport) {
  SetCoverModel model = CreateSmallModel();
  model.SortElementsInSubsets();
  SetCoverProto proto = model.ExportModelAsProto();

  SetCoverModel new_model;
  new_model.ImportModelFromProto(proto);

  EXPECT_EQ(model.num_elements(), new_model.num_elements());
  EXPECT_EQ(model.num_subsets(), new_model.num_subsets());
  EXPECT_EQ(model.num_nonzeros(), new_model.num_nonzeros());
  EXPECT_EQ(model.subset_costs(), new_model.subset_costs());

  const auto& cols1 = model.columns();
  const auto& cols2 = new_model.columns();
  ASSERT_EQ(cols1.size(), cols2.size());
  for (SubsetIndex i(0); i.value() < model.num_subsets(); ++i) {
    EXPECT_EQ(cols1[i], cols2[i]);
  }
}

TEST(SetCoverModelLogicTest, Singletons) {
  SetCoverModel model;
  // S0 = {0, 1}
  model.AddEmptySubset(1.0);
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);
  // S1 = {2}
  model.AddEmptySubset(1.0);
  model.AddElementToLastSubset(2);

  model.CreateSparseRowView();

  EXPECT_EQ(model.ComputeNumSingletonColumns(), 1);
  EXPECT_EQ(model.ComputeNumSingletonRows(), 3);
}

TEST(SetCoverModelTest, ToString) {
  SetCoverModel model = CreateSmallModel();
  EXPECT_EQ(model.ToString(", "), "3, 3, 6, 0.666667");
}

TEST(SetCoverModelTest, ToVerboseString) {
  SetCoverModel model = CreateSmallModel();
  model.SetName("MyModel");
  EXPECT_EQ(
      model.ToVerboseString(", "),
      "num_elements, 3, num_subsets, 3, num_nonzeros, 6, fill_rate, 0.666667");
}

TEST(SetCoverModelTest, IntersectingSubsets) {
  SetCoverModel model = CreateSmallModel();
  model.CreateSparseRowView();
  IntersectingSubsetsRange range(model, SubsetIndex(0));
  std::vector<SubsetIndex> intersecting;
  for (const SubsetIndex subset : range) {
    intersecting.push_back(subset);
  }
  EXPECT_THAT(intersecting,
              UnorderedElementsAre(SubsetIndex(1), SubsetIndex(2)));
}

TEST(SetCoverModelTest, ComputeCostStats) {
  SetCoverModel model = CreateSmallModel();
  const auto stats = model.ComputeCostStats();
  EXPECT_EQ(stats.min, 1.0);
  EXPECT_EQ(stats.max, 3.0);
  EXPECT_EQ(stats.median, 2.0);
  EXPECT_EQ(stats.mean, 2.0);
  EXPECT_NEAR(stats.stddev, 0.81649658, 1e-6);
  EXPECT_EQ(stats.iqr, 1.0);
  EXPECT_EQ(stats.ToString(", "), "1, 3, 2, 2, 0.816497, 1");
  EXPECT_EQ(stats.ToVerboseString(", "),
            "min, 1, max, 3, median, 2, mean, 2, stddev, 0.816497, iqr, 1");
}

TEST(SetCoverModelTest, ComputeRowStats) {
  SetCoverModel model = CreateSmallModel();
  model.CreateSparseRowView();
  const auto stats = model.ComputeRowStats();
  EXPECT_EQ(stats.min, 2);
  EXPECT_EQ(stats.max, 2);
  EXPECT_EQ(stats.median, 2);
  EXPECT_EQ(stats.mean, 2);
  EXPECT_EQ(stats.stddev, 0);
  EXPECT_EQ(stats.iqr, 0);
}

TEST(SetCoverModelTest, ComputeColumnStats) {
  SetCoverModel model = CreateSmallModel();
  const auto stats = model.ComputeColumnStats();
  EXPECT_EQ(stats.min, 2);
  EXPECT_EQ(stats.max, 2);
  EXPECT_EQ(stats.median, 2);
  EXPECT_EQ(stats.mean, 2);
  EXPECT_EQ(stats.stddev, 0);
  EXPECT_EQ(stats.iqr, 0);
}

TEST(SetCoverModelTest, ComputeRowDeciles) {
  SetCoverModel model = CreateSmallModel();
  model.CreateSparseRowView();
  EXPECT_THAT(model.ComputeRowDeciles(),
              ElementsAre(2, 2, 2, 2, 2, 2, 2, 2, 2));
}

TEST(SetCoverModelTest, ComputeColumnDeciles) {
  SetCoverModel model = CreateSmallModel();
  EXPECT_THAT(model.ComputeColumnDeciles(),
              ElementsAre(2, 2, 2, 2, 2, 2, 2, 2, 2));
}

TEST(SetCoverModelTest, ReserveNumElementsInSubset) {
  SetCoverModel model = CreateSmallModel();
  model.ReserveNumElementsInSubset(10, 0);
  // This is a smoke test to ensure ReserveNumElementsInSubset runs.
  // The actual capacity check is harder to verify without large allocations.
  SUCCEED();
}

TEST(SetCoverModelTest, Timestamp) {
  SetCoverModel model = CreateSmallModel();
  int64_t current_timestamp = model.timestamp();
  model.SetSubsetCost(SubsetIndex(0), 10.0);
  EXPECT_GT(model.timestamp(), current_timestamp);
  current_timestamp = model.timestamp();

  SubsetCostVector new_costs = {1.0, 2.0, 3.0};
  model.set_subset_costs(new_costs);
  EXPECT_GT(model.timestamp(), current_timestamp);
  current_timestamp = model.timestamp();

  model.AddEmptySubset(1.0);
  EXPECT_GT(model.timestamp(), current_timestamp);
  current_timestamp = model.timestamp();

  model.AddElementToLastSubset(0);
  EXPECT_GT(model.timestamp(), current_timestamp);
  current_timestamp = model.timestamp();

  model.AddElementToSubset(1, 0);
  EXPECT_GT(model.timestamp(), current_timestamp);
  current_timestamp = model.timestamp();

  model.ResizeNumSubsets(10);
  EXPECT_GT(model.timestamp(), current_timestamp);
}

SetCoverModel CreateMidSizeModel() {
  // Sets up a small, well-defined set cover problem for testing.
  // Model Description:
  // Elements: 3 (0, 1, 2)
  // Subsets: 4 (S0, S1, S2, S3)
  // S0 = {0, 2}, cost = 2.5
  // S1 = {1}, cost = 1.0
  // S2 = {0, 1}, cost = 3.0
  // S3 = {2}, cost = 1.0
  SetCoverModel model;
  model.AddEmptySubset(2.5);  // Subset 0
  model.AddElementToLastSubset(ElementIndex(0));
  model.AddElementToLastSubset(ElementIndex(2));

  model.AddEmptySubset(1.0);  // Subset 1
  model.AddElementToLastSubset(ElementIndex(1));

  model.AddEmptySubset(3.0);  // Subset 2
  model.AddElementToLastSubset(ElementIndex(0));
  model.AddElementToLastSubset(ElementIndex(1));

  model.AddEmptySubset(1.0);  // Subset 3
  model.AddElementToLastSubset(ElementIndex(2));

  return model;
}

// Construction and Basic Property Tests

TEST(SetCoverModelEmptyTest, DefaultConstructor) {
  SetCoverModel empty_model;
  EXPECT_TRUE(empty_model.IsEmpty());
  EXPECT_EQ(empty_model.num_elements(), 0);
  EXPECT_EQ(empty_model.num_subsets(), 0);
  EXPECT_EQ(empty_model.num_nonzeros(), 0);
  EXPECT_EQ(empty_model.name(), "SetCoverModel");
  EXPECT_TRUE(empty_model.is_unicost());
}

TEST(SetCoverModelTest, InitialProperties) {
  SetCoverModel model = CreateMidSizeModel();
  EXPECT_FALSE(model.IsEmpty());
  EXPECT_EQ(model.num_elements(), 3);
  EXPECT_EQ(model.num_subsets(), 4);
  EXPECT_EQ(model.num_nonzeros(), 6);
  EXPECT_DOUBLE_EQ(model.FillRate(), 6.0 / (3.0 * 4.0));
  EXPECT_EQ(model.name(), "SetCoverModel");

  model.SetName("TestProblem");
  EXPECT_EQ(model.name(), "TestProblem");
}

TEST(SetCoverModelTest, ColumnViewAndCosts) {
  SetCoverModel model = CreateMidSizeModel();
  const SparseColumnView& columns = model.columns();
  ASSERT_THAT(columns, SizeIs(4));
  EXPECT_THAT(columns[SubsetIndex(0)],
              ElementsAre(ElementIndex(0), ElementIndex(2)));
  EXPECT_THAT(columns[SubsetIndex(1)], ElementsAre(ElementIndex(1)));
  EXPECT_THAT(columns[SubsetIndex(2)],
              ElementsAre(ElementIndex(0), ElementIndex(1)));
  EXPECT_THAT(columns[SubsetIndex(3)], ElementsAre(ElementIndex(2)));

  const SubsetCostVector& costs = model.subset_costs();
  ASSERT_THAT(costs, SizeIs(4));
  EXPECT_THAT(costs, ElementsAre(Cost(2.5), Cost(1.0), Cost(3.0), Cost(1.0)));
}

// View Generation and Validity Tests

TEST(SetCoverModelTest, CreateSparseRowView2) {
  SetCoverModel model = CreateMidSizeModel();
  EXPECT_FALSE(model.row_view_is_valid());
  model.CreateSparseRowView();
  EXPECT_TRUE(model.row_view_is_valid());

  const SparseRowView& rows = model.rows();
  ASSERT_THAT(rows, SizeIs(3));
  // Note: CreateSparseRowView sorts subsets within rows.
  EXPECT_THAT(rows[ElementIndex(0)],
              ElementsAre(SubsetIndex(0), SubsetIndex(2)));
  EXPECT_THAT(rows[ElementIndex(1)],
              ElementsAre(SubsetIndex(1), SubsetIndex(2)));
  EXPECT_THAT(rows[ElementIndex(2)],
              ElementsAre(SubsetIndex(0), SubsetIndex(3)));
}

namespace {
// Returns true if the two sparse columns are equal.
bool Equal(const SparseColumn& sparse_col, const SparseColumn& other_col) {
  EXPECT_EQ(sparse_col.empty(), other_col.empty());
  ColumnEntryIndex entry(0);
  for (const ElementIndex element : other_col) {
    if (element != sparse_col[entry]) {
      return false;
    }
    ++entry;
  }
  return true;
}
}  // namespace

// Model Analysis Tests

TEST(SetCoverModelTest, IsUnicost2) {
  SetCoverModel model = CreateMidSizeModel();
  EXPECT_FALSE(model.is_unicost());

  SetCoverModel unicost_model;
  unicost_model.AddEmptySubset(1.0);
  unicost_model.AddEmptySubset(1.0);
  EXPECT_TRUE(unicost_model.is_unicost());

  // Invalidate the unicost cache by changing a cost.
  unicost_model.SetSubsetCost(SubsetIndex(0), 2.0);
  EXPECT_FALSE(unicost_model.is_unicost());
}

TEST(SetCoverModelTest, SingletonComputations) {
  SetCoverModel model = CreateMidSizeModel();
  model.CreateSparseRowView();
  EXPECT_EQ(model.ComputeNumSingletonColumns(), 2);  // Subsets 1 and 3.
  EXPECT_EQ(model.ComputeNumSingletonRows(), 0);
}

TEST(SetCoverModelSingletonTest, SingletonRows) {
  SetCoverModel model;
  model.AddEmptySubset(1.0);  // S0 = {0}
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(1.0);  // S1 = {1, 2}
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);
  model.AddEmptySubset(1.0);  // S2 = {1}
  model.AddElementToLastSubset(1);
  model.ResizeNumElements(3);
  model.CreateSparseRowView();

  EXPECT_EQ(model.ComputeNumSingletonRows(),
            2);  // Elements 0 and 2 appear only once.
}

TEST(SetCoverModelTest, ComputeFeasibility) {
  SetCoverModel model = CreateMidSizeModel();
  EXPECT_TRUE(model.ComputeFeasibility());

  // Create an infeasible model where element 1 is not covered.
  SetCoverModel infeasible_model;
  infeasible_model.AddEmptySubset(1.0);
  infeasible_model.AddElementToLastSubset(0);
  infeasible_model.ResizeNumElements(2);

  EXPECT_FALSE(infeasible_model.ComputeFeasibility());
}

// Proto and Slicing Tests

TEST(SetCoverModelTest, ProtoImportExport) {
  SetCoverModel model = CreateMidSizeModel();
  // Sort elements to ensure deterministic proto export.
  model.SortElementsInSubsets();
  const SetCoverProto proto = model.ExportModelAsProto();

  ASSERT_EQ(proto.subset_size(), 4);

  SetCoverModel new_model;
  new_model.ImportModelFromProto(proto);

  EXPECT_EQ(new_model.num_elements(), model.num_elements());
  EXPECT_EQ(new_model.num_subsets(), model.num_subsets());
  EXPECT_EQ(new_model.num_nonzeros(), model.num_nonzeros());

  EXPECT_THAT(new_model.subset_costs(),
              ElementsAre(Cost(2.5), Cost(1.0), Cost(3.0), Cost(1.0)));

  const auto& new_columns = new_model.columns();
  ASSERT_THAT(new_columns, SizeIs(4));
  EXPECT_THAT(new_columns[SubsetIndex(0)],
              ElementsAre(ElementIndex(0), ElementIndex(2)));
  EXPECT_THAT(new_columns[SubsetIndex(1)], ElementsAre(ElementIndex(1)));
  EXPECT_THAT(new_columns[SubsetIndex(2)],
              ElementsAre(ElementIndex(0), ElementIndex(1)));
  EXPECT_THAT(new_columns[SubsetIndex(3)], ElementsAre(ElementIndex(2)));
}

// Statistics and Iterator Tests

TEST(SetCoverModelTest, StatsComputation) {
  SetCoverModel model = CreateMidSizeModel();
  model.CreateSparseRowView();

  // Costs: {2.5, 1.0, 3.0, 1.0}. Mean: 1.875, Median: 1.75.
  const SetCoverModel::Stats cost_stats = model.ComputeCostStats();
  EXPECT_EQ(cost_stats.min, 1.0);
  EXPECT_EQ(cost_stats.max, 3.0);
  EXPECT_DOUBLE_EQ(cost_stats.mean, 1.875);
  EXPECT_DOUBLE_EQ(cost_stats.median, 1.75);

  // Column cardinalities: {2, 1, 2, 1}. Mean: 1.5, Median: 1.5.
  const SetCoverModel::Stats col_stats = model.ComputeColumnStats();
  EXPECT_EQ(col_stats.min, 1.0);
  EXPECT_EQ(col_stats.max, 2.0);
  EXPECT_DOUBLE_EQ(col_stats.mean, 1.5);
  EXPECT_DOUBLE_EQ(col_stats.median, 1.5);

  // Row degrees: {2, 2, 2}. Mean: 2.0, Median: 2.0.
  const SetCoverModel::Stats row_stats = model.ComputeRowStats();
  EXPECT_EQ(row_stats.min, 2.0);
  EXPECT_EQ(row_stats.max, 2.0);
  EXPECT_DOUBLE_EQ(row_stats.mean, 2.0);
  EXPECT_DOUBLE_EQ(row_stats.median, 2.0);
}

// Miscellaneous Tests

TEST(SetCoverModelTest, Resize) {
  SetCoverModel model = CreateMidSizeModel();
  model.ResizeNumSubsets(6);
  EXPECT_EQ(model.num_subsets(), 6);
  ASSERT_THAT(model.columns(), SizeIs(6));
  ASSERT_THAT(model.subset_costs(), SizeIs(6));
  EXPECT_THAT(model.columns()[SubsetIndex(4)], IsEmpty());
  EXPECT_THAT(model.columns()[SubsetIndex(5)], IsEmpty());
  EXPECT_EQ(model.subset_costs()[SubsetIndex(4)], 0.0);

  // Resizing to a smaller number should have no effect.
  model.ResizeNumSubsets(3);
  EXPECT_EQ(model.num_subsets(), 6);

  model.ResizeNumElements(5);
  EXPECT_EQ(model.num_elements(), 5);
  model.CreateSparseRowView();
  ASSERT_THAT(model.rows(), SizeIs(5));
  EXPECT_THAT(model.rows()[ElementIndex(3)], IsEmpty());
  EXPECT_THAT(model.rows()[ElementIndex(4)], IsEmpty());
}

TEST(SetCoverModelTest, GenerateRandomModelFrom) {
  SetCoverModel model = CreateMidSizeModel();
  // This is a smoke test, as the output is random.
  const BaseInt kNumElements = 50;
  const BaseInt kNumSubsets = 100;

  model.CreateSparseRowView();

  const SetCoverModel random_model = SetCoverModel::GenerateRandomModelFrom(
      model, kNumElements, kNumSubsets, 1.0, 1.0, 1.0);

  // Generation can fail and return an empty model. If it succeeds, check sizes.
  if (!random_model.IsEmpty()) {
    EXPECT_EQ(random_model.num_elements(), kNumElements);
    EXPECT_EQ(random_model.num_subsets(), kNumSubsets);
    // Ensure the generated model is valid by creating its row view.
    SetCoverModel mutable_random_model = random_model;
    mutable_random_model.CreateSparseRowView();
    EXPECT_TRUE(mutable_random_model.row_view_is_valid());
  } else {
    // This is a possible and valid outcome.
    LOG(INFO) << "Random model generation produced an empty model.";
  }
}

}  // namespace
}  // namespace operations_research
