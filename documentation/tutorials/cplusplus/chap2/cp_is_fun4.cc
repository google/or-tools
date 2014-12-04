// Copyright 2011-2014 Google
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
//
// Cryptoarithmetic puzzle
//
// First attempt to solve equation CP + IS + FUN = TRUE
// where each letter represents a unique digit.
//
// This problem has 72 different solutions in base 10.
//
// Use of SolutionCollectors.
// Use of Solve().
// Use of gflags to choose the base.
// Change the time limit of the solver.

#include <vector>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_int64(base, 10, "Base used to solve the problem.");
DEFINE_bool(print_all_solutions, false, "Print all solutions?");
DEFINE_int64(time_limit, 10000, "Time limit in milliseconds");

namespace operations_research {

// helper functions
IntVar* const MakeBaseLine2(Solver*  s,
                            IntVar* const v1,
                            IntVar* const v2,
                            const int64 base) {
  return s->MakeSum(s->MakeProd(v1, base), v2)->Var();
}

IntVar* const MakeBaseLine3(Solver* s,
                            IntVar* const v1,
                            IntVar* const v2,
                            IntVar* const v3,
                            const int64 base) {
  std::vector<IntVar*> tmp_vars;
  std::vector<int64> coefficients;
  tmp_vars.push_back(v1);
  coefficients.push_back(base * base);
  tmp_vars.push_back(v2);
  coefficients.push_back(base);
  tmp_vars.push_back(v3);
  coefficients.push_back(1);

  return s->MakeScalProd(tmp_vars, coefficients)->Var();
}

IntVar* const MakeBaseLine4(Solver* s,
                            IntVar* const v1,
                            IntVar* const v2,
                            IntVar* const v3,
                            IntVar* const v4,
                            const int64 base) {
  std::vector<IntVar*> tmp_vars;
  std::vector<int64> coefficients;
  tmp_vars.push_back(v1);
  coefficients.push_back(base * base * base);
  tmp_vars.push_back(v2);
  coefficients.push_back(base * base);
  tmp_vars.push_back(v3);
  coefficients.push_back(base);
  tmp_vars.push_back(v4);
  coefficients.push_back(1);

  return s->MakeScalProd(tmp_vars, coefficients)->Var();
}

void CPIsFun() {
  // Use some profiling and change the default parameters of the solver
  SolverParameters solver_params = SolverParameters();
  // Change the profile level
  solver_params.profile_level = SolverParameters::NORMAL_PROFILING;

  // Constraint programming engine
  Solver solver("CP is fun!", solver_params);

  const int64 kBase = FLAGS_base;

  // Decision variables
  IntVar* const c = solver.MakeIntVar(1, kBase - 1, "C");
  IntVar* const p = solver.MakeIntVar(0, kBase - 1, "P");
  IntVar* const i = solver.MakeIntVar(1, kBase - 1, "I");
  IntVar* const s = solver.MakeIntVar(0, kBase - 1, "S");
  IntVar* const f = solver.MakeIntVar(1, kBase - 1, "F");
  IntVar* const u = solver.MakeIntVar(0, kBase - 1, "U");
  IntVar* const n = solver.MakeIntVar(0, kBase - 1, "N");
  IntVar* const t = solver.MakeIntVar(1, kBase - 1, "T");
  IntVar* const r = solver.MakeIntVar(0, kBase - 1, "R");
  IntVar* const e = solver.MakeIntVar(0, kBase - 1, "E");

  // We need to group variables in a vector to be able to use
  // the global constraint AllDifferent
  std::vector<IntVar*> letters;
  letters.push_back(c);
  letters.push_back(p);
  letters.push_back(i);
  letters.push_back(s);
  letters.push_back(f);
  letters.push_back(u);
  letters.push_back(n);
  letters.push_back(t);
  letters.push_back(r);
  letters.push_back(e);

  // Check if we have enough digits
  CHECK_GE(kBase, letters.size());

  // Constraints
  solver.AddConstraint(solver.MakeAllDifferent(letters, false));

  // CP + IS + FUN = FUN
  IntVar* const term1 = MakeBaseLine2(&solver, c, p, kBase);
  IntVar* const term2 = MakeBaseLine2(&solver, i, s, kBase);
  IntVar* const term3 = MakeBaseLine3(&solver, f, u, n, kBase);
  IntVar* const sum_terms = solver.MakeSum(solver.MakeSum(term1,
                                                          term2),
                                           term3)->Var();

  IntVar* const sum   = MakeBaseLine4(&solver, t, r, u, e, kBase);

  solver.AddConstraint(solver.MakeEquality(sum_terms, sum));


  SolutionCollector* const all_solutions = solver.MakeAllSolutionCollector();
  //  Add the interesting variables to the SolutionCollector
  all_solutions->Add(letters);

  DecisionBuilder* const db = solver.MakePhase(letters,
                                               Solver::CHOOSE_FIRST_UNBOUND,
                                               Solver::ASSIGN_MIN_VALUE);

  // Add some time limit
  SearchLimit* const time_limit = solver.MakeTimeLimit(FLAGS_time_limit);
  
  solver.Solve(db, all_solutions,time_limit);

  //  Retrieve the solutions
  const int numberSolutions = all_solutions->solution_count();
  LOG(INFO) << "Number of solutions: " << numberSolutions << std::endl;

  if (FLAGS_print_all_solutions) {
    for (int index = 0; index < numberSolutions; ++index) {
      LOG(INFO) << "C=" << all_solutions->Value(index, c) << " "
      << "P=" << all_solutions->Value(index, p) << " "
      << "I=" << all_solutions->Value(index, i) << " "
      << "S=" << all_solutions->Value(index, s) << " "
      << "F=" << all_solutions->Value(index, f) << " "
      << "U=" << all_solutions->Value(index, u) << " "
      << "N=" << all_solutions->Value(index, n) << " "
      << "T=" << all_solutions->Value(index, t) << " "
      << "R=" << all_solutions->Value(index, r) << " "
      << "E=" << all_solutions->Value(index, e);

      // Is CP + IS + FUN = TRUE?
      CHECK_EQ(all_solutions->Value(index, p) + all_solutions->Value(index, s) + all_solutions->Value(index, n) +
        kBase * (all_solutions->Value(index, c) + all_solutions->Value(index, i) + all_solutions->Value(index, u)) +
        kBase * kBase * all_solutions->Value(index, f),
                 all_solutions->Value(index, e) +
                 kBase * all_solutions->Value(index, u) +
                 kBase * kBase * all_solutions->Value(index, r) +
                 kBase * kBase * kBase * all_solutions->Value(index, t));
    }
  }

  // Save profile in file
  solver.ExportProfilingOverview("profile.txt");
}

}   // namespace operations_research

// ----- MAIN -----
int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::CPIsFun();
  return 0;
}
