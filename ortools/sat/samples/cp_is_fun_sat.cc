// Copyright 2010-2021 Google LLC
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

// [START program]
// Cryptarithmetic puzzle
//
// First attempt to solve equation CP + IS + FUN = TRUE
// where each letter represents a unique digit.
//
// This problem has 72 different solutions in base 10.
// [START import]
#include <cstdint>
#include <vector>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
// [END import]

namespace operations_research {
namespace sat {

void CPIsFunSat() {
  // Instantiate the solver.
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  const int64_t kBase = 10;

  // Define decision variables.
  Domain digit(0, kBase - 1);
  Domain non_zero_digit(1, kBase - 1);

  IntVar c = cp_model.NewIntVar(non_zero_digit).WithName("C");
  IntVar p = cp_model.NewIntVar(digit).WithName("P");
  IntVar i = cp_model.NewIntVar(non_zero_digit).WithName("I");
  IntVar s = cp_model.NewIntVar(digit).WithName("S");
  IntVar f = cp_model.NewIntVar(non_zero_digit).WithName("F");
  IntVar u = cp_model.NewIntVar(digit).WithName("U");
  IntVar n = cp_model.NewIntVar(digit).WithName("N");
  IntVar t = cp_model.NewIntVar(non_zero_digit).WithName("T");
  IntVar r = cp_model.NewIntVar(digit).WithName("R");
  IntVar e = cp_model.NewIntVar(digit).WithName("E");
  // [END variables]

  // [START constraints]
  // Define constraints.
  cp_model.AddAllDifferent({c, p, i, s, f, u, n, t, r, e});

  // CP + IS + FUN = TRUE
  cp_model.AddEquality(
      c * kBase + p + i * kBase + s + f * kBase * kBase + u * kBase + n,
      kBase * kBase * kBase * t + kBase * kBase * r + kBase * u + e);
  // [END constraints]

  // [START solution_printer]
  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& response) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "C=" << SolutionIntegerValue(response, c) << " "
              << "P=" << SolutionIntegerValue(response, p) << " "
              << "I=" << SolutionIntegerValue(response, i) << " "
              << "S=" << SolutionIntegerValue(response, s) << " "
              << "F=" << SolutionIntegerValue(response, f) << " "
              << "U=" << SolutionIntegerValue(response, u) << " "
              << "N=" << SolutionIntegerValue(response, n) << " "
              << "T=" << SolutionIntegerValue(response, t) << " "
              << "R=" << SolutionIntegerValue(response, r) << " "
              << "E=" << SolutionIntegerValue(response, e);
    num_solutions++;
  }));
  // [END solution_printer]

  // [START solve]
  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
  // [END solve]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::sat::CPIsFunSat();
  return EXIT_SUCCESS;
}
// [END program]
