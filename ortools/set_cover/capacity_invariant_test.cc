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

#include "ortools/set_cover/capacity_invariant.h"

#include "gtest/gtest.h"
#include "ortools/set_cover/capacity_model.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

TEST(CapacityModel, ChecksConstraintViolation) {
  // Compatibility constraint: choose either of the two subsets.
  SetCoverModel sc;
  sc.AddEmptySubset(1.0);
  sc.AddElementToLastSubset(ElementIndex(0));
  sc.AddEmptySubset(1.0);
  sc.AddElementToLastSubset(ElementIndex(0));
  CapacityModel m(0.0, 1.0);
  m.AddTerm(SubsetIndex(0), ElementIndex(0), CapacityWeight(1.0));
  m.AddTerm(SubsetIndex(1), ElementIndex(0), CapacityWeight(1.0));
  EXPECT_TRUE(m.ComputeFeasibility());

  CapacityInvariant cinv(&m, &sc);
  // Current assignment: [false, false]. Current activation: 0.
  EXPECT_TRUE(cinv.CanSelect(SubsetIndex(0)));  // All moves are possible.
  EXPECT_TRUE(cinv.CanSelect(SubsetIndex(1)));

  EXPECT_TRUE(cinv.Select(SubsetIndex(0)));
  EXPECT_TRUE(cinv.CanDeselect(SubsetIndex(0)));  // Undoing: still valid.
  EXPECT_FALSE(cinv.CanSelect(SubsetIndex(1)));   // Impossible move.
  EXPECT_FALSE(cinv.Select(SubsetIndex(1)));
  // Current assignment: [true, false]. Current activation: 1.

  EXPECT_TRUE(cinv.Deselect(SubsetIndex(0)));
  EXPECT_TRUE(cinv.CanSelect(SubsetIndex(0)));  // Undoing: still valid.
  EXPECT_TRUE(cinv.CanSelect(SubsetIndex(1)));  // Valid when 0 not selected.
  EXPECT_TRUE(cinv.Select(SubsetIndex(1)));
  EXPECT_FALSE(cinv.CanSelect(SubsetIndex(0)));   // Impossible move.
  EXPECT_TRUE(cinv.CanDeselect(SubsetIndex(1)));  // Undoing: still valid.
}

}  // namespace
}  // namespace operations_research
