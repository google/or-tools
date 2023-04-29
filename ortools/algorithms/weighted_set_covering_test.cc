// Copyright 2010-2022 Google LLC
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

#include "ortools/algorithms/weighted_set_covering.h"

#include "gtest/gtest.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"

namespace operations_research {
namespace {

TEST(SetCoveringTest, InitialValues) {
  WeightedSetCovering set_covering;
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(0);
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(1);
  set_covering.AddElementToLastSubset(2);
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(1);
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(2);
  set_covering.Init();
  set_covering.GenerateGreedySolution();
  set_covering.Steepest(500);
  set_covering.GuidedTabuSearch(500);
  set_covering.RestoreSolution();
}

TEST(SetCoveringTest, Infeasible) {
  WeightedSetCovering set_covering;
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(0);
  set_covering.AddEmptySubset(1);
  set_covering.AddElementToLastSubset(3);
  set_covering.Init();
  EXPECT_FALSE(set_covering.CheckFeasibility());
}

TEST(SetCoveringTest, KnightsCover) {
  const int knight_row_move[] = {2, 1, -1, -2, -2, -1, 1, 2};
  const int knight_col_move[] = {1, 2, 2, 1, -1, -2, -2, -1};
  WeightedSetCovering set_covering;
  const int num_rows = 25;
  const int num_cols = 25;
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      set_covering.AddEmptySubset(1);
      set_covering.AddElementToLastSubset(row * num_cols + col);
      for (int i = 0; i < 8; ++i) {
        const int new_row = row + knight_row_move[i];
        const int new_col = col + knight_col_move[i];
        if (new_row >= 0 && new_row < num_rows && new_col >= 0 &&
            new_col < num_cols) {
          set_covering.AddElementToLastSubset(new_row * num_cols + new_col);
        }
      }
    }
  }
  EXPECT_TRUE(set_covering.CheckFeasibility());
  set_covering.Init();
  set_covering.UseEverything();
  set_covering.RestoreSolution();
  EXPECT_TRUE(set_covering.CheckSolution());
  set_covering.Init();
  set_covering.GenerateGreedySolution();
  set_covering.Steepest(500);
  for (int i = 0; i < 50; ++i) {
    set_covering.GuidedTabuSearch(100000);
    set_covering.ResetGuidedTabuSearch();
  }
  set_covering.RestoreSolution();
  EXPECT_TRUE(set_covering.CheckSolution());
}

}  // namespace
}  // namespace operations_research
