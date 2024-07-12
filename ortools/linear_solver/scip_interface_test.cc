// Copyright 2010-2024 Google LLC
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

#include <fstream>

#include "gtest/gtest.h"
#include "ortools/base/init_google.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {

TEST(ScipInterface, IndicatorConstraint0) {
  MPSolver solver("SCIP", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  // Maximize x <= 100
  auto x = solver.MakeNumVar(0, 100, "x");
  solver.MutableObjective()->SetMaximization();
  solver.MutableObjective()->SetCoefficient(x, 1);
  // With indicator constraint
  // if var = 0, then x <= 10
  auto var = solver.MakeBoolVar("indicator_var");
  auto ct = solver.MakeIndicatorConstraint(0, 10, "test", var, false);
  ct->SetCoefficient(x, 1);

  // Leave var free ==> x = 100
  solver.Solve();
  EXPECT_EQ(var->solution_value(), 1);
  EXPECT_EQ(x->solution_value(), 100);

  // Force var to 0 ==> x = 10
  var->SetUB(0);
  solver.Solve();
  EXPECT_EQ(x->solution_value(), 10);
}

TEST(ScipInterface, IndicatorConstraint1) {
  MPSolver solver("SCIP", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  // Maximize x <= 100
  auto x = solver.MakeNumVar(0, 100, "x");
  solver.MutableObjective()->SetMaximization();
  solver.MutableObjective()->SetCoefficient(x, 1);
  // With indicator constraint
  // if var = 1, then x <= 10
  auto var = solver.MakeBoolVar("indicator_var");
  auto ct = solver.MakeIndicatorConstraint(0, 10, "test", var, true);
  ct->SetCoefficient(x, 1);

  // Leave var free ==> x = 100
  solver.Solve();
  EXPECT_EQ(var->solution_value(), 0);
  EXPECT_EQ(x->solution_value(), 100);

  // Force var to 0 ==> x = 10
  var->SetLB(1);
  solver.Solve();
  EXPECT_EQ(x->solution_value(), 10);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_stderrthreshold, 0);
  testing::InitGoogleTest(&argc, argv);
  auto solver = operations_research::MPSolver::CreateSolver("scip");
  if (solver == nullptr) {
    LOG(ERROR) << "SCIP solver is not available";
    return EXIT_SUCCESS;
  } else {
    return RUN_ALL_TESTS();
  }
}