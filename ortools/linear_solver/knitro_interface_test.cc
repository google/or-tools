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

#include <filesystem>
#include <fstream>
#include <locale>

#include "gtest/gtest.h"
#include "ortools/base/init_google.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/knitro/environment.h"

namespace operations_research {

#define EXPECT_STATUS(s)                              \
  do {                                                \
    int const status_ = s;                            \
    EXPECT_EQ(0, status_) << "Nonzero return status"; \
  } while (0)

class KNITROGetter {
 public:
  KNITROGetter(MPSolver* solver) : solver_(solver) {}

  // Add public methods here

 private:
  MPSolver* solver_;

  // XPRSprob prob() { return (XPRSprob)solver_->underlying_solver(); }

};

TEST(Env, CheckEnv){
  EXPECT_EQ(KnitroIsCorrectlyInstalled(),true);
}

TEST(KnitroInterface, SolveMIP){
  // max  x -  y + 5z
  // st.  x + 2y -  z <= 19.5
  //      x +  y +  z >= 3.14
  //      x           <= 10
  //           y +  z <= 6
  //      x,   y,   z \in R+

  auto solver = operations_research::MPSolver::CreateSolver("KNITRO");
  const double infinity = solver->infinity();
  // x and y are integer non-negative variables.
  MPVariable* const x = solver->MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver->MakeNumVar(0.0, infinity, "y");
  MPVariable* const z = solver->MakeIntVar(0.0, infinity, "z");

  // x + 2 * y - z <= 19.5
  MPConstraint* const c0 = solver->MakeRowConstraint(-infinity, 19.5, "c0");
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 2);
  c0->SetCoefficient(z, -1);

  // x + y + z >= 3.14
  MPConstraint* const c1 = solver->MakeRowConstraint(3.14, infinity, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  c1->SetCoefficient(z, 1);

  // x <= 10.
  MPConstraint* const c2 = solver->MakeRowConstraint(-infinity, 10.0, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 0);
  c2->SetCoefficient(z, 0);

  // y + z <= 6.
  MPConstraint* const c3 = solver->MakeRowConstraint(-infinity, 6.0, "c3");
  c3->SetCoefficient(x, 0);
  c3->SetCoefficient(y, 1);
  c3->SetCoefficient(z, 1);

  // Maximize x - y + 5 * z.
  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, -1);
  objective->SetCoefficient(z, 5);
  objective->SetMaximization();

  const MPSolver::ResultStatus result_status = solver->Solve();
  DCHECK_EQ(objective->Value(), 40);
  DCHECK_EQ(x->solution_value(), 10);
  DCHECK_EQ(y->solution_value(), 0);
  DCHECK_EQ(z->solution_value(), 6);
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  // auto solver = operations_research::MPSolver::CreateSolver("KNITRO_LP");
  return RUN_ALL_TESTS();
}
