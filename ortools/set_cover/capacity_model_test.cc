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
#include "ortools/base/message_matchers.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

using ::testing::EqualsProto;

TEST(CapacityModel, ConstructorRequiresOneBound) {
  EXPECT_DEATH(CapacityModel(std::numeric_limits<CapacityWeight>::min(),
                             std::numeric_limits<CapacityWeight>::max()),
               "min");
}

TEST(CapacityModel, ConstructorRejectsNaN) {
  EXPECT_DEATH(CapacityModel(std::numeric_limits<CapacityWeight>::quiet_NaN(),
                             std::numeric_limits<CapacityWeight>::quiet_NaN()),
               "isnan");
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

TEST(CapacityModel, WithMinimumWeightRejectsNaN) {
  EXPECT_DEATH(CapacityModel::WithMinimumWeight(
                   std::numeric_limits<double>::quiet_NaN()),
               "isnan");
}

TEST(CapacityModel, WithMaximumWeightRejectsNaN) {
  EXPECT_DEATH(CapacityModel::WithMaximumWeight(
                   std::numeric_limits<double>::quiet_NaN()),
               "isnan");
}

TEST(CapacityModel, SetMinimumCapacityRejectsNaN) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.SetMinimumCapacity(std::numeric_limits<double>::quiet_NaN()),
               "isnan");
}

TEST(CapacityModel, SetMinimumCapacityRejectsPlusInfinity) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.SetMinimumCapacity(std::numeric_limits<CapacityWeight>::max()),
               "max");
}

TEST(CapacityModel, SetMaximumCapacityRejectsNaN) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.SetMaximumCapacity(std::numeric_limits<double>::quiet_NaN()),
               "isnan");
}

TEST(CapacityModel, SetMaximumCapacityRejectsMinusInfinity) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.SetMaximumCapacity(std::numeric_limits<CapacityWeight>::min()),
               "min");
}

TEST(CapacityModel, AddTermRejectsNaN) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.AddTerm(SubsetIndex(0), ElementIndex(0),
                         std::numeric_limits<double>::quiet_NaN()),
               "isfinite");
}

TEST(CapacityModel, AddTermRejectsPlusInf) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.AddTerm(SubsetIndex(0), ElementIndex(0),
                         std::numeric_limits<double>::infinity()),
               "isfinite");
}

TEST(CapacityModel, AddTermRejectsMinusInf) {
  CapacityModel m(0.0, 1.0);
  EXPECT_DEATH(m.AddTerm(SubsetIndex(0), ElementIndex(0),
                         -std::numeric_limits<double>::infinity()),
               "isfinite");
}

TEST(CapacityModel, ComputeFeasibilityWithNoTerms) {
  CapacityModel m(0.0, 1.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-1.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(0.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-2.0);
  m.SetMaximumCapacity(-1.0);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyPositiveWeights) {
  CapacityModel m(0.0, 1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), 1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), 2.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), 3.0);
  // Activation bounds: [0.0, 6.0].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-1.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-1.0);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMaximumCapacity(7.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(7.0);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyNegativeWeights) {
  CapacityModel m(0.0, 1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), -1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), -2.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), -3.0);
  // Activation bounds: [-6.0, 0.0].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(1.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(1.0);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-7.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-7.0);
  EXPECT_FALSE(m.ComputeFeasibility());
}

TEST(CapacityModel, ComputeFeasibilityWithOnlyMixedWeights) {
  CapacityModel m(0.0, 1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), -1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(1), 2.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(2), -3.0);
  // Activation bounds: [-4.0, 2.0].
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(3.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMinimumCapacity(3.0);
  EXPECT_FALSE(m.ComputeFeasibility());

  m.SetMinimumCapacity(-5.0);
  EXPECT_TRUE(m.ComputeFeasibility());

  m.SetMaximumCapacity(-5.0);
  EXPECT_FALSE(m.ComputeFeasibility());
}

// TEST(CapacityModel, ImportModelFromProto) {
//   CapacityModel m(0.0, 1.0);
//   EXPECT_THAT(m.ExportModelAsProto(), EqualsProto(R"pb(min_capacity: 0.0
//                                                        max_capacity: 1.0)pb"));

//   m.AddTerm(SubsetIndex(0), ElementIndex(0), 1.0);
//   EXPECT_THAT(m.ExportModelAsProto(),
//               EqualsProto(R"pb(min_capacity: 0.0
//                                max_capacity: 1.0
//                                capacity_term {
//                                  subset: 0
//                                  element_weights { element: 0 weight: 1.0 }
//                                })pb"));

//   m.AddTerm(SubsetIndex(0), ElementIndex(1), 1.0);
//   EXPECT_THAT(m.ExportModelAsProto(),
//               EqualsProto(
//                   R"pb(min_capacity: 0.0
//                        max_capacity: 1.0
//                        capacity_term {
//                          subset: 0
//                          element_weights { element: 0 weight: 1.0 }
//                          element_weights { element: 1 weight: 1.0 }
//                        })pb"));
// }

// TEST(CapacityModel, ImportModelFromProtoHasCanonicalOrder) {
//   // Reverse order for the terms compared to
//   // CapacityModel_ImportModelFromProto, same order in the proto.
//   CapacityModel m(0.0, 1.0);
//   m.AddTerm(SubsetIndex(0), ElementIndex(1), 1.0);
//   m.AddTerm(SubsetIndex(0), ElementIndex(0), 1.0);
//   EXPECT_THAT(m.ExportModelAsProto(),
//               EqualsProto(
//                   R"pb(min_capacity: 0.0
//                        max_capacity: 1.0
//                        capacity_term {
//                          subset: 0
//                          element_weights { element: 0 weight: 1.0 }
//                          element_weights { element: 1 weight: 1.0 }
//                        })pb"));
// }

}  // namespace
}  // namespace operations_research
