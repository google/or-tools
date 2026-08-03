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

// Simple integer programming example for users manual

#include <iostream>
#include <limits>
#include <ostream>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {
using ::operations_research::math_opt::LinearConstraint;
using ::operations_research::math_opt::Model;
using ::operations_research::math_opt::SolveResult;
using ::operations_research::math_opt::SolverType;
using ::operations_research::math_opt::Variable;
using ::operations_research::math_opt::VariableMap;

constexpr double kInf = std::numeric_limits<double>::infinity();

void SolveSimpleServerAllocationProblem() {
  // [START_build_model]
  Model server_model("server allocation");

  // Variables
  const Variable west = server_model.AddIntegerVariable(0.0, kInf, "west");
  const Variable central =
      server_model.AddIntegerVariable(0.0, kInf, "central");
  const Variable east = server_model.AddIntegerVariable(0.0, kInf, "east");

  // Constraints
  const LinearConstraint pacific =
      server_model.AddLinearConstraint(west + central >= 2, "pacific");
  const LinearConstraint atlantic =
      server_model.AddLinearConstraint(central + east >= 1, "atlantic");

  // Objective
  server_model.Minimize(west + 2 * central + 3 * east);
  // [END_build_model]

  // [START_solve]
  const absl::StatusOr<SolveResult> status_or_result =
      Solve(server_model, SolverType::kGscip);
  CHECK_OK(status_or_result.status());
  const SolveResult& result = status_or_result.value();
  // [END_solve]

  // [START_output_results]
  // Check that the problem has an optimal solution.
  QCHECK_OK(result.termination.EnsureIsOptimal());

  // Get some result information
  std::cout << "Problem solved in " << result.solve_time() << std::endl;
  std::cout << "Objective value: " << result.objective_value() << std::endl;

  // Get solution values
  const double west_value = result.variable_values().at(west);
  const double east_value = result.variable_values().at(east);
  const double central_value = result.variable_values().at(central);
  std::cout << "Variable values: [west=" << west_value
            << ", central=" << central_value << ", east=" << east_value << "]"
            << std::endl;
  // [END_output_results]

  // [START_output_ids]
  // Print variable and constraint ids.
  std::cout << "Variable ids: [west=" << west.id()
            << ", central=" << central.id() << ", east=" << east.id() << "]"
            << std::endl;
  std::cout << "Constraint ids: [pacific=" << pacific.id()
            << ", atlantic=" << atlantic.id() << "]" << std::endl;
  // [END_output_ids]
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  SolveSimpleServerAllocationProblem();
  return 0;
}
