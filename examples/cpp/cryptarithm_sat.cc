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

// Use CP-SAT to solve a simple cryptarithmetic problem: SEND+MORE=MONEY.

#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void SendMoreMoney() {
  CpModelBuilder cp_model;

  // Possible domains for variables.
  Domain all_digits(0, 9);
  Domain non_zero_digits(1, 9);
  // Create variables.
  // Since s is a leading digit, it can't be 0.
  const IntVar s = cp_model.NewIntVar(non_zero_digits);
  const IntVar e = cp_model.NewIntVar(all_digits);
  const IntVar n = cp_model.NewIntVar(all_digits);
  const IntVar d = cp_model.NewIntVar(all_digits);
  // Since m is a leading digit, it can't be 0.
  const IntVar m = cp_model.NewIntVar(non_zero_digits);
  const IntVar o = cp_model.NewIntVar(all_digits);
  const IntVar r = cp_model.NewIntVar(all_digits);
  const IntVar y = cp_model.NewIntVar(all_digits);

  // Create carry variables. c0 is true if the first column of addends carries
  // a 1, c2 is true if the second column carries a 1, and so on.
  const BoolVar c0 = cp_model.NewBoolVar();
  const BoolVar c1 = cp_model.NewBoolVar();
  const BoolVar c2 = cp_model.NewBoolVar();
  const BoolVar c3 = cp_model.NewBoolVar();

  // Force all letters to take on different values.
  cp_model.AddAllDifferent({s, e, n, d, m, o, r, y});

  // Column 0:
  cp_model.AddEquality(c0, m);

  // Column 1:
  cp_model.AddEquality(c1 + s + m + o, 10 * c0);

  // Column 2:
  cp_model.AddEquality(c2 + e + o, n + 10 * c1);

  // Column 3:
  cp_model.AddEquality(c3 + n + r, e + 10 * c2);

  // Column 4:
  cp_model.AddEquality(d + e, y + 10 * c3);

  // Declare the model, solve it, and display the results.
  const CpSolverResponse response = Solve(cp_model.Build());
  LOG(INFO) << CpSolverResponseStats(response);
  LOG(INFO) << "s: " << SolutionIntegerValue(response, s);
  LOG(INFO) << "e: " << SolutionIntegerValue(response, e);
  LOG(INFO) << "n: " << SolutionIntegerValue(response, n);
  LOG(INFO) << "d: " << SolutionIntegerValue(response, d);
  LOG(INFO) << "m: " << SolutionIntegerValue(response, m);
  LOG(INFO) << "o: " << SolutionIntegerValue(response, o);
  LOG(INFO) << "r: " << SolutionIntegerValue(response, r);
  LOG(INFO) << "y: " << SolutionIntegerValue(response, y);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SendMoreMoney();
  return EXIT_SUCCESS;
}
