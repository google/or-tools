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

// Integer programming example that shows how to use the API.

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
void RunIntegerProgrammingExample(const std::string& solver_id) {
  LOG(INFO) << "---- Integer programming example with " << solver_id << " ----";

  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver(solver_id));
  if (!solver) {
    LOG(INFO) << "Unable to create solver : " << solver_id;
    return;
  }

  const double infinity = solver->infinity();
  // x and y are integer non-negative variables.
  MPVariable* const x = solver->MakeIntVar(0.0, infinity, "x");
  MPVariable* const y = solver->MakeIntVar(0.0, infinity, "y");

  // Maximize x + 10 * y.
  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 10);
  objective->SetMaximization();

  // x + 7 * y <= 17.5.
  MPConstraint* const c0 = solver->MakeRowConstraint(-infinity, 17.5);
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 7);

  // x <= 3.5
  MPConstraint* const c1 = solver->MakeRowConstraint(-infinity, 3.5);
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 0);

  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  LOG(INFO) << "Number of constraints = " << solver->NumConstraints();

  const MPSolver::ResultStatus result_status = solver->Solve();
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution!";
  }
  LOG(INFO) << "Solution:";
  LOG(INFO) << "x = " << x->solution_value();
  LOG(INFO) << "y = " << y->solution_value();
  LOG(INFO) << "Optimal objective value = " << objective->Value();
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver->wall_time() << " milliseconds";
  LOG(INFO) << "Problem solved in " << solver->iterations() << " iterations";
  LOG(INFO) << "Problem solved in " << solver->nodes()
            << " branch-and-bound nodes";
}

void RunAllExamples() {
  std::vector<MPSolver::OptimizationProblemType> supported_problem_types =
      MPSolverInterfaceFactoryRepository::GetInstance()
          ->ListAllRegisteredProblemTypes();
  for (MPSolver::OptimizationProblemType type : supported_problem_types) {
    const std::string type_name = MPModelRequest::SolverType_Name(
        static_cast<MPModelRequest::SolverType>(type));
    if (!SolverTypeIsMip(type)) continue;
    if (absl::StrContains(type_name, "KNAPSACK")) continue;
    if (absl::StrContains(type_name, "BOP")) continue;
    if (absl::StrContains(type_name, "HIGHS")) continue;
// ASAN issues a warning in CBC code which cannot be avoided for now:
// AddressSanitizer: float-cast-overflow
// third_party/cbc/Cgl/src/CglPreProcess/CglPreProcess.cpp:1717:36
#ifdef ADDRESS_SANITIZER
    if (type_name.find("CBC") != std::string::npos) {
      continue;
    }
#endif
    RunIntegerProgrammingExample(type_name);
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);
  operations_research::RunAllExamples();
  return EXIT_SUCCESS;
}
