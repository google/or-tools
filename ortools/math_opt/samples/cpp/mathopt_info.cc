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

// Check MathOpt available solvers.
#include <iostream>

#include "ortools/base/init_google.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/cpp/math_opt.h"

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);

  std::cout << "MathOpt is configured to support the following solvers:\n";
  for (const operations_research::math_opt::SolverTypeProto& solver_type_proto :
       operations_research::math_opt::AllSolversRegistry::Instance()
           ->RegisteredSolvers()) {
    std::cout << " - " << EnumFromProto(solver_type_proto) << std::endl;
  }

  return 0;
}
