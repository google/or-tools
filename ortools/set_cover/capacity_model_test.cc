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

#include "ortools/set_cover/capacity_model.h"

#include <limits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ::testing::EqualsProto;

TEST(CapacityModel, ConstructorRequiresOneBound) {
  EXPECT_DEATH(CapacityModel(std::numeric_limits<CapacityWeight>::min(),
                             std::numeric_limits<CapacityWeight>::max()),
               "min");
}

TEST(CapacityModel, WithMinimumWeightRequiresNonVacuousMinimum) {
  EXPECT_DEATH(CapacityModel::WithMinimumWeight(
                   std::numeric_limits<CapacityWeight>::min()),
               "min");
}

TEST(CapacityModel, WithMaximumWeightRequiresNonVacuousMaximum) {
  EXPECT_DEATH(CapacityModel::WithMaximumWeight(
                   std::numeric_limits<CapacityWeight>::max()),
               "min");
}

TEST(CapacityModel, SetMinimumCapacityRejectsPlusInfinity) {
  CapacityModel m(0, 1);
  EXPECT_DEATH(m.SetMinimumCapacity(std::numeric_limits<CapacityWeight>::max()),
               "max");
}

TEST(CapacityModel, SetMaximumCapacityRejectsMinusInfinity) {
  CapacityModel m(0, 1);
  EXPECT_DEATH(m.SetMaximumCapacity(std::numeric_limits<CapacityWeight>::min()),
               "min");
}

TEST(CapacityModel, ComputeFeasibilityWithNoTerms) {
  CapacityModel m(0, 1);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-1);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-2);
  m.SetMaximumCapacity(-1);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyPositiveWeights) {
  CapacityModel m(0, 1);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), 1);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), 2);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), 3);
  // Activation bounds: [0, 6].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-1);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-1);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMaximumCapacity(7);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(7);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyNegativeWeights) {
  CapacityModel m(0, 1);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), -1);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), -2);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), -3);
  // Activation bounds: [-6, 0].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(1);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(1);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-7);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-7);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyMixedWeights) {
  CapacityModel m(0, 1);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), -1);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), 2);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), -3);
  // Activation bounds: [-4, 2].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(3);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(3);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-5);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-5);
  EXPECT_FALSE(m.ComputeFeasibility());
}

}  // namespace
}  // namespace operations_research
