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

#include "ortools/constraint_solver/constraint_solver.h"
// [END import]

namespace operations_research {

// Helper functions.
IntVar* const MakeBaseLine2(Solver* s, IntVar* const v1, IntVar* const v2,
                            const int64_t base) {
  return s->MakeSum(s->MakeProd(v1, base), v2)->Var();
}

IntVar* const MakeBaseLine3(Solver* s, IntVar* const v1, IntVar* const v2,
                            IntVar* const v3, const int64_t base) {
  std::vector<IntVar*> tmp_vars;
  std::vector<int64_t> coefficients;
  tmp_vars.push_back(v1);
  coefficients.push_back(base * base);
  tmp_vars.push_back(v2);
  coefficients.push_back(base);
  tmp_vars.push_back(v3);
  coefficients.push_back(1);

  return s->MakeScalProd(tmp_vars, coefficients)->Var();
}

IntVar* const MakeBaseLine4(Solver* s, IntVar* const v1, IntVar* const v2,
                            IntVar* const v3, IntVar* const v4,
                            const int64_t base) {
  std::vector<IntVar*> tmp_vars;
  std::vector<int64_t> coefficients;
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

void CPIsFunCp() {
  // Instantiate the solver.
  // [START solver]
  Solver solver("CP is fun!");
  // [END solver]

  // [START variables]
  const int64_t kBase = 10;

  // Define decision variables.
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
  // [END variables]

  // [START constraints]
  // Define constraints.
  solver.AddConstraint(solver.MakeAllDifferent(letters));

  // CP + IS + FUN = TRUE
  IntVar* const term1 = MakeBaseLine2(&solver, c, p, kBase);
  IntVar* const term2 = MakeBaseLine2(&solver, i, s, kBase);
  IntVar* const term3 = MakeBaseLine3(&solver, f, u, n, kBase);
  IntVar* const sum_terms =
      solver.MakeSum(solver.MakeSum(term1, term2), term3)->Var();

  IntVar* const sum = MakeBaseLine4(&solver, t, r, u, e, kBase);

  solver.AddConstraint(solver.MakeEquality(sum_terms, sum));
  // [END constraints]

  // [START solve]
  int num_solutions = 0;
  // Create decision builder to search for solutions.
  DecisionBuilder* const db = solver.MakePhase(
      letters, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << "C=" << c->Value() << " "
              << "P=" << p->Value() << " "
              << "I=" << i->Value() << " "
              << "S=" << s->Value() << " "
              << "F=" << f->Value() << " "
              << "U=" << u->Value() << " "
              << "N=" << n->Value() << " "
              << "T=" << t->Value() << " "
              << "R=" << r->Value() << " "
              << "E=" << e->Value();

    // Is CP + IS + FUN = TRUE?
    CHECK_EQ(p->Value() + s->Value() + n->Value() +
                 kBase * (c->Value() + i->Value() + u->Value()) +
                 kBase * kBase * f->Value(),
             e->Value() + kBase * u->Value() + kBase * kBase * r->Value() +
                 kBase * kBase * kBase * t->Value());
    num_solutions++;
  }
  solver.EndSearch();
  LOG(INFO) << "Number of solutions found: " << num_solutions;
  // [END solve]
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::CPIsFunCp();
  return EXIT_SUCCESS;
}
// [END program]
