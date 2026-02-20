// Copyright Artelys for RTE.
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

// This example that shows how to use Xpress Solver.

#include <cstdlib>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

using namespace operations_research;

/**
 * This method shows two ways to initialize a Xpress solver instance.
 * Two environment variables are used to specify the Xpress installation paths:
 *  * XPRESSDIR : Path to the Xpress root directory containing bin and lib
 *                folders
 *  * XPRESS : Path to the directory containing Xpress license
 */
void useXpressSolver(bool solveAsMip, bool useFactory) {
  std::unique_ptr<MPSolver> solver = nullptr;
  if (useFactory) {
    /* This is the preferred way as the program won't stop if anything went
       wrong. In such a case, `solver` will take value `nullptr` */
    std::string xpressName = (solveAsMip ? "XPRESS" : "XPRESS_LP");
    solver.reset(MPSolver::CreateSolver(xpressName));
  } else {
    MPSolver::OptimizationProblemType problemType =
        (solveAsMip ? MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING
                    : MPSolver::XPRESS_LINEAR_PROGRAMMING);
    /* MPSolver::SupportsProblemType(problem_type) will test if Xpress is
       correctly loaded and has a valid license. This check is important to keep
       the program running if Xpress is not correctly installed. With the
       constructor usage, if Xpress is badly loaded or if there is a problem
       with the license, the program will abort (SIGABRT)
    */
    if (MPSolver::SupportsProblemType(problemType)) {
      solver.reset(new MPSolver("IntegerProgrammingExample", problemType));
    }
  }
  if (solver == nullptr) {
    LOG(WARNING) << "Xpress solver is not available";
    return;
  }
  // Use the solver
  /*
    max -100 x1 + 10 x2
    s.t. x2 <= 20 x1;
         30 x1 + 3.5 x2 <= 350
         0 <= x1 <= 5
         0 <= x2
   */
  const double infinity = MPSolver::infinity();
  const MPVariable* x1 = solver->MakeIntVar(0, 5, "x1");
  const MPVariable* x2 = solver->MakeNumVar(0.0, infinity, "x2");

  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x1, -100);
  objective->SetCoefficient(x2, 10);
  objective->SetMaximization();

  MPConstraint* const c0 = solver->MakeRowConstraint(-infinity, 0.0);
  c0->SetCoefficient(x1, -20.0);
  c0->SetCoefficient(x2, 1);

  MPConstraint* const c1 = solver->MakeRowConstraint(-infinity, 350.0);
  c1->SetCoefficient(x1, 30.0);
  c1->SetCoefficient(x2, 3.5);

  const MPSolver::ResultStatus result_status = solver->Solve();

  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "Solver returned with non-optimal status.";
  } else {
    LOG(WARNING) << "Optimal solution found: obj=" << objective->Value();
  }
}
#define ABSL_MIN_LOG_LEVEL INFO;
int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);
  std::cout << "start\n";
  LOG(WARNING) << "start";
  for (bool solveAsMip : {true, false}) {
    for (bool useFactory : {true, false}) {
      useXpressSolver(solveAsMip, useFactory);
    }
  }
  return EXIT_SUCCESS;
}
