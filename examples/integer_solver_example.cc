// Copyright 2010-2011 Google
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

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "linear_solver/linear_solver.h"

namespace operations_research {
void BuildMixedIntegerProgrammingBoundedExample(
    MPSolver::OptimizationProblemType type) {
  MPSolver solver("Mixed Integer_Example", type);
  const double infinity = solver.infinity();
  MPVariable* const x1 = solver.MakeIntVar(0.0, infinity, "x1");
  MPVariable* const x2 = solver.MakeIntVar(0.0, infinity, "x2");

  // minimize x1 + 2 * x2
  solver.AddObjectiveTerm(x1, 1);
  solver.AddObjectiveTerm(x2, 2);

  // 2 * x2 + 3 * x1 >= 17
  MPConstraint* const c0 = solver.MakeRowConstraint(17, infinity);
  c0->AddTerm(x1, 3);
  c0->AddTerm(x2, 2);

  CHECK_EQ(MPSolver::OPTIMAL, solver.Solve());
  LOG(INFO) << "objective = " << solver.objective_value();
}


void RunAllExamples() {
  #if defined(USE_GLPK)
  LOG(INFO) << "----- Running MIP Example with GLPK -----";
  BuildMixedIntegerProgrammingBoundedExample(
      MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING);
  #endif
  #if defined(USE_CBC)
  LOG(INFO) << "----- Running MIP Example with Coin Branch and Cut -----";
  BuildMixedIntegerProgrammingBoundedExample(
      MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  #endif
  #if defined(USE_SCIP)
  LOG(INFO) << "----- Running MIP Example with SCIP -----";
  BuildMixedIntegerProgrammingBoundedExample(
      MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  #endif
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::RunAllExamples();
  return 0;
}
