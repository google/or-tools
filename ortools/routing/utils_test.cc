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

#include "ortools/routing/utils.h"

#include <vector>

#include "gtest/gtest.h"

namespace operations_research::routing {

TEST(BinCapacitiesTest, InitializationIsCorrectOneDimension) {
  BinCapacities bin_capacities(2);
  auto demand = [](int, int) { return 0; };
  std::vector<BinCapacities::LoadLimit> limits = {
      {.max_load = 3, .soft_max_load = -2, .cost_above_soft_max_load = 5},
      {.max_load = 3, .soft_max_load = -3, .cost_above_soft_max_load = 7}};
  bin_capacities.AddDimension(demand, limits);
  EXPECT_EQ(bin_capacities.NumDimensions(), 1);
  EXPECT_EQ(bin_capacities.TotalCost(), 2 * 5 + 3 * 7);
}

TEST(BinCapacitiesTest, InitializationIsCorrectTwoDimensions) {
  BinCapacities bin_capacities(3);
  auto dim = [](int, int) { return 0; };
  std::vector<BinCapacities::LoadLimit> limits = {
      {.max_load = 3, .soft_max_load = -1, .cost_above_soft_max_load = 10},
      {.max_load = 3, .soft_max_load = -2, .cost_above_soft_max_load = 10},
      {.max_load = 3, .soft_max_load = -3, .cost_above_soft_max_load = 10}};
  bin_capacities.AddDimension(dim, limits);
  EXPECT_EQ(bin_capacities.NumDimensions(), 1);
  limits[0].soft_max_load = -10;
  limits[1].cost_above_soft_max_load = 100;
  limits[2].soft_max_load = -30;
  bin_capacities.AddDimension(dim, limits);
  EXPECT_EQ(bin_capacities.NumDimensions(), 2);
  EXPECT_EQ(bin_capacities.TotalCost(), 660);
}

TEST(BinCapacitiesTest, ZeroDimensions) {
  // With no dimensions, everything is feasible, cost is 0.
  BinCapacities bin_capacities(5);
  EXPECT_EQ(bin_capacities.TotalCost(), 0);
  EXPECT_EQ(bin_capacities.NumDimensions(), 0);
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 4));
  EXPECT_TRUE(bin_capacities.AddItemToBin(7, 1));
  EXPECT_TRUE(bin_capacities.AddItemToBin(5, 0));
  EXPECT_TRUE(bin_capacities.AddItemToBin(3, 2));
  EXPECT_TRUE(bin_capacities.AddItemToBin(5000, 3));
  EXPECT_TRUE(bin_capacities.AddItemToBin(3414134, 1));
  EXPECT_TRUE(bin_capacities.AddItemToBin(737, 4));
  EXPECT_EQ(bin_capacities.TotalCost(), 0);
}

TEST(BinCapacitiesTest, FeasibilityIsCorrect) {
  BinCapacities bin_capacities(2);
  auto dim = [](int item, int bin) { return (item == bin) ? 0 : 1; };
  std::vector<BinCapacities::LoadLimit> limits1 = {
      {.max_load = 2, .soft_max_load = 2, .cost_above_soft_max_load = 0},
      {.max_load = 4, .soft_max_load = 4, .cost_above_soft_max_load = 0},
  };
  std::vector<BinCapacities::LoadLimit> limits2 = {
      {.max_load = 4, .soft_max_load = 4, .cost_above_soft_max_load = 0},
      {.max_load = 2, .soft_max_load = 2, .cost_above_soft_max_load = 0},
  };
  bin_capacities.AddDimension(dim, limits1);
  bin_capacities.AddDimension(dim, limits2);
  EXPECT_EQ(bin_capacities.NumDimensions(), 2);
  // Items {1, 2} on bin 0 fit, but not more.
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 0));  // {1}
  EXPECT_TRUE(bin_capacities.AddItemToBin(2, 0));  // {1, 2}
  EXPECT_FALSE(bin_capacities.CheckAdditionFeasibility(3, 0));
  EXPECT_FALSE(bin_capacities.AddItemToBin(3, 0));      // {1, 2, 3}
  EXPECT_TRUE(bin_capacities.RemoveItemFromBin(3, 0));  // {1, 2}
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 0));       // {1, 2, 0}
  // Clear bin 0, expect it to fit item 3.
  bin_capacities.ClearItemsOfBin(0);
  EXPECT_TRUE(bin_capacities.CheckAdditionFeasibility(3, 0));
  bin_capacities.ClearItemsOfBin(0);
  // Items {0, 3} on bin 1 fit, but not more.
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 1));  // {0}
  EXPECT_TRUE(bin_capacities.AddItemToBin(3, 1));  // {0, 3}
  EXPECT_FALSE(bin_capacities.CheckAdditionFeasibility(2, 1));
  EXPECT_FALSE(bin_capacities.AddItemToBin(2, 1));      // {0, 3, 2}
  EXPECT_TRUE(bin_capacities.RemoveItemFromBin(2, 1));  // {0, 3}
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 1));       // {0, 3, 1}
  // Test multiple additions.
  bin_capacities.ClearItems();
  EXPECT_FALSE(bin_capacities.CheckAdditionsFeasibility({1, 2, 3}, 0));
  EXPECT_FALSE(bin_capacities.CheckAdditionsFeasibility({0, 2, 3}, 1));
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 0));  // {1}
  EXPECT_TRUE(bin_capacities.AddItemToBin(2, 0));  // {1, 2}
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 0));  // {1, 2, 0}
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 1));  // {0}
  EXPECT_TRUE(bin_capacities.AddItemToBin(3, 1));  // {0, 3}
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 1));  // {0, 3, 1}
}

TEST(BinCapacitiesTest, CostsAreCorrect) {
  BinCapacities bin_capacities(2);
  auto dim1 = [](int item, int bin) { return (item == bin) ? 1 : 2; };
  auto dim2 = [](int item, int bin) { return (item == bin) ? 10 : 11; };
  std::vector<BinCapacities::LoadLimit> limits1 = {
      {.max_load = 999, .soft_max_load = 1, .cost_above_soft_max_load = 1},
      {.max_load = 999, .soft_max_load = 1, .cost_above_soft_max_load = 10},
  };
  std::vector<BinCapacities::LoadLimit> limits2 = {
      {.max_load = 999, .soft_max_load = 10, .cost_above_soft_max_load = 100},
      {.max_load = 999, .soft_max_load = 10, .cost_above_soft_max_load = 1000},
  };
  bin_capacities.AddDimension(dim1, limits1);
  bin_capacities.AddDimension(dim2, limits2);
  EXPECT_EQ(bin_capacities.NumDimensions(), 2);
  EXPECT_EQ(bin_capacities.TotalCost(), 0);
  // Setting the items on their worst bin costs is expensive.
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 1));
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 0));
  EXPECT_EQ(bin_capacities.TotalCost(), 1111);
  // Setting the items on their preferred bin costs nothing.
  bin_capacities.ClearItems();
  EXPECT_TRUE(bin_capacities.AddItemToBin(0, 0));
  EXPECT_TRUE(bin_capacities.AddItemToBin(1, 1));
  EXPECT_EQ(bin_capacities.TotalCost(), 0);
  // Adding another item overloads soft costs.
  EXPECT_TRUE(bin_capacities.AddItemToBin(2, 0));
  EXPECT_TRUE(bin_capacities.AddItemToBin(3, 1));
  EXPECT_EQ(bin_capacities.TotalCost(), 1111 + 11 + 11000);
}

}  // namespace operations_research::routing
