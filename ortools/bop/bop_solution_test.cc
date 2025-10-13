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

#include "ortools/bop/bop_solution.h"

#include <string>
#include <vector>

#include "absl/log/check.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/bop/bop_types.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace bop {
namespace {

using ::operations_research::sat::LinearBooleanProblem;

// Tests Bop solutions using a problem with no constraints.
// The solution is always feasible, but the cost can vary.
TEST(BopSolution, NoConstraints) {
  const std::string kProblem =
      "name: \"Test\" "
      "num_variables: 3 "
      "var_names: \"x\" "
      "var_names: \"y\" "
      "var_names: \"z\" "
      "objective { "                     // 4 * (x + 2 * y - z + 3)
      "  literals: 1 coefficients: 1 "   // x
      "  literals: 2 coefficients: 2 "   // 2 * y
      "  literals: 3 coefficients: -1 "  // - z
      "  offset: 3 scaling_factor: 4 "
      "} ";

  LinearBooleanProblem problem;
  CHECK(google::protobuf::TextFormat::ParseFromString(kProblem, &problem));

  // Empty solution: all variables are set depending on the coefficient sign.
  BopSolution solution_001(problem, "NoConstraints");
  EXPECT_TRUE(solution_001.IsFeasible());
  EXPECT_EQ(-1, solution_001.GetCost());
  EXPECT_EQ(4 * (-1 + 3), solution_001.GetScaledCost());

  // Check accessors.
  EXPECT_EQ(3, solution_001.Size());
  EXPECT_EQ("NoConstraints", solution_001.name());
  VariableIndex var(0);
  const std::vector<bool> kValues = {false, false, true};
  for (const bool value : solution_001) {
    EXPECT_EQ(value, solution_001.Value(var));
    EXPECT_EQ(kValues[var.value()], solution_001.Value(var));
    ++var;
  }

  // All-true solution.
  BopSolution solution_111 = solution_001;
  for (VariableIndex var(0); var < solution_111.Size(); ++var) {
    solution_111.SetValue(var, true);
  }

  // Solution_000 should not have changed.
  EXPECT_TRUE(solution_001.IsFeasible());
  EXPECT_EQ(-1, solution_001.GetCost());
  EXPECT_EQ(4 * (-1 + 3), solution_001.GetScaledCost());
  EXPECT_EQ(solution_001.Size(), solution_111.Size());
  for (VariableIndex var(0); var < solution_111.Size(); ++var) {
    EXPECT_EQ(kValues[var.value()], solution_001.Value(var));
    EXPECT_TRUE(solution_111.Value(var));
  }

  EXPECT_TRUE(solution_111.IsFeasible());
  EXPECT_EQ(2, solution_111.GetCost());
  EXPECT_EQ((2 + 3) * 4, solution_111.GetScaledCost());

  // All false.
  BopSolution solution_000 = solution_001;
  solution_000.SetValue(VariableIndex(2), false);

  EXPECT_TRUE(solution_000.IsFeasible());
  EXPECT_EQ(0, solution_000.GetCost());
  EXPECT_EQ(3 * 4, solution_000.GetScaledCost());
}

// Tests using a Two-constraints problem. Constraints can be broken
// independently. Note that any feasible solution has a cost of 1 (because of
// the
// first constraint).
TEST(BopSolution, TwoConstraints) {
  const std::string kProblem =
      "name: \"Test\" "
      "num_variables: 3 "
      "var_names: \"x\" "
      "var_names: \"y\" "
      "var_names: \"z\" "
      "constraints { "  // x + y == 1
      "  literals: 1 coefficients: 1 "
      "  literals: 2 coefficients: 1 "
      "  lower_bound: 1 "
      "  upper_bound: 1 "
      "  name: \"Ct_1\" "
      "} "
      "constraints { "  // y + z <= 1
      "  literals: 2 coefficients: 1 "
      "  literals: 3 coefficients: 1 "
      "  upper_bound: 1 "
      "  name: \"Ct_2\" "
      "} "
      "objective { "  // x + y
      "  literals: 1 coefficients: 1 "
      "  literals: 2 coefficients: 1 "
      "} ";

  LinearBooleanProblem problem;
  CHECK(google::protobuf::TextFormat::ParseFromString(kProblem, &problem));

  // Empty solution: all variables are set to false.
  BopSolution solution_000(problem, "TwoConstraints");
  EXPECT_FALSE(solution_000.IsFeasible());
  EXPECT_EQ(0, solution_000.GetCost());
  EXPECT_EQ(0, solution_000.GetScaledCost());

  // All-true solution.
  BopSolution solution_111 = solution_000;
  for (VariableIndex var(0); var < solution_111.Size(); ++var) {
    solution_111.SetValue(var, true);
  }
  EXPECT_FALSE(solution_111.IsFeasible());
  EXPECT_EQ(2, solution_111.GetCost());
  EXPECT_EQ(2, solution_111.GetScaledCost());

  // Feasible solution with x true.
  BopSolution solution_100 = solution_000;
  solution_100.SetValue(VariableIndex(0), true);
  EXPECT_TRUE(solution_100.IsFeasible());
  EXPECT_EQ(1, solution_100.GetCost());
  EXPECT_EQ(1, solution_100.GetScaledCost());

  // Feasible solution with x and z true.
  BopSolution solution_101 = solution_100;
  solution_101.SetValue(VariableIndex(2), true);
  EXPECT_TRUE(solution_101.IsFeasible());
  EXPECT_EQ(1, solution_101.GetCost());
  EXPECT_EQ(1, solution_101.GetScaledCost());

  // Infeasible solution with y and z true.
  BopSolution solution_two_true = solution_111;
  solution_two_true.SetValue(VariableIndex(0), false);
  EXPECT_FALSE(solution_two_true.IsFeasible());
  EXPECT_EQ(1, solution_two_true.GetCost());
  EXPECT_EQ(1, solution_two_true.GetScaledCost());

  // Make solution_two_true feasible by swapping x and y values.
  solution_two_true.SetValue(VariableIndex(0), true);
  solution_two_true.SetValue(VariableIndex(1), false);
  EXPECT_TRUE(solution_two_true.IsFeasible());
  EXPECT_EQ(1, solution_two_true.GetCost());
  EXPECT_EQ(1, solution_two_true.GetScaledCost());
}
}  // anonymous namespace
}  // namespace bop
}  // namespace operations_research
