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

#include "ortools/set_cover/assignment.h"

#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

SetCoverModel MakeBasicModel() {
  // 3 elements, 4 subsets (all of unitary cost).
  // Optimal cost: 2 (subsets #0 and #1).
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
  CHECK(model.ComputeFeasibility());
  return model;
}

TEST(SetCoverAssignment, EmbryonicModelHasZeroCost) {
  SetCoverModel model;
  model.AddEmptySubset(1);
  model.AddElementToLastSubset(0);
  SetCoverAssignment assignment(model);

  EXPECT_TRUE(assignment.CheckConsistency());
  EXPECT_EQ(assignment.cost(), 0.0);
  EXPECT_EQ(assignment.assignment(), SubsetBoolVector(1, false));
}

TEST(SetCoverAssignment, BasicModelHasCost) {
  SetCoverModel model = MakeBasicModel();
  ASSERT_EQ(model.num_subsets(), 4);
  ASSERT_EQ(model.num_elements(), 3);
  SetCoverAssignment assignment(model);

  EXPECT_TRUE(assignment.CheckConsistency());
  EXPECT_EQ(assignment.cost(), 0.0);
  EXPECT_EQ(assignment.assignment(), SubsetBoolVector(4, false));

  assignment.SetValue(SubsetIndex(0), true,
                      SetCoverInvariant::ConsistencyLevel::kInconsistent);
  assignment.SetValue(SubsetIndex(1), true,
                      SetCoverInvariant::ConsistencyLevel::kInconsistent);

  EXPECT_TRUE(assignment.CheckConsistency());
  EXPECT_EQ(assignment.cost(), 2.0);
  EXPECT_EQ(assignment.assignment(),
            SubsetBoolVector({true, true, false, false}));

  assignment.SetValue(SubsetIndex(1), false,
                      SetCoverInvariant::ConsistencyLevel::kInconsistent);

  EXPECT_TRUE(assignment.CheckConsistency());
  EXPECT_EQ(assignment.cost(), 1.0);
  EXPECT_EQ(assignment.assignment(),
            SubsetBoolVector({true, false, false, false}));
}

TEST(SetCoverAssignment, BasicModelWorksWithSetCoverInvariant) {
  // Changes to the solution imply changes in the invariant.
  SetCoverModel model = MakeBasicModel();
  ASSERT_EQ(model.num_subsets(), 4);
  ASSERT_EQ(model.num_elements(), 3);
  SetCoverAssignment assignment(model);

  SetCoverInvariant inv(&model);
  assignment.AttachInvariant(&inv);

  ASSERT_EQ(assignment.assignment(), SubsetBoolVector(4, false));
  EXPECT_EQ(inv.num_uncovered_elements(), 3);

  assignment.SetValue(SubsetIndex(0), true,
                      SetCoverInvariant::ConsistencyLevel::kRedundancy);
  assignment.SetValue(SubsetIndex(1), true,
                      SetCoverInvariant::ConsistencyLevel::kRedundancy);

  EXPECT_EQ(inv.num_uncovered_elements(), 0);
}

TEST(SetCoverAssignment, ImportFromVector) {
  SetCoverModel model = MakeBasicModel();
  ASSERT_EQ(model.num_subsets(), 4);
  ASSERT_EQ(model.num_elements(), 3);
  const SubsetBoolVector reference_assignment = {true, false, false, false};

  SetCoverAssignment assignment(model);
  assignment.LoadAssignment(reference_assignment);
  EXPECT_TRUE(assignment.CheckConsistency());
  EXPECT_EQ(assignment.cost(), 1.0);
  EXPECT_EQ(assignment.assignment(), reference_assignment);
}

TEST(SetCoverAssignment, ExportImportAllPullTogetherAsATeam) {
  SetCoverModel model = MakeBasicModel();
  ASSERT_EQ(model.num_subsets(), 4);
  ASSERT_EQ(model.num_elements(), 3);

  SetCoverAssignment assignment_1(model);
  assignment_1.SetValue(SubsetIndex(0), true,
                        SetCoverInvariant::ConsistencyLevel::kInconsistent);
  assignment_1.SetValue(SubsetIndex(1), true,
                        SetCoverInvariant::ConsistencyLevel::kInconsistent);
  ASSERT_EQ(assignment_1.cost(), 2.0);
  ASSERT_EQ(assignment_1.assignment(),
            SubsetBoolVector({true, true, false, false}));
  ASSERT_TRUE(assignment_1.CheckConsistency());

  SetCoverSolutionResponse message = assignment_1.ExportSolutionAsProto();
  SetCoverAssignment assignment_2(model);
  assignment_2.ImportSolutionFromProto(message);

  EXPECT_EQ(assignment_1.cost(), assignment_2.cost());
  EXPECT_EQ(assignment_1.assignment(), assignment_2.assignment());
  EXPECT_TRUE(assignment_2.CheckConsistency());
}

}  // namespace
}  // namespace operations_research
