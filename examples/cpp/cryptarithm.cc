// Copyright 2010-2017 Google
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
// Cryptoarithmetics problem
//
// Solves equation SEND + MORE = MONEY among numbers
// where each digit is represented by a letter.
//
// Solution:
// S=9; M=1; O=0; E=5; N=6; D=7; R=8; Y=2.

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

void Cryptoarithmetics() {
  Solver solver("cryptarithm");

  // model with carry
  IntVar* s = solver.MakeIntVar(1, 9, "s");
  IntVar* m = solver.MakeIntVar(1, 9, "m");
  IntVar* o = solver.MakeIntVar(0, 9, "o");
  IntVar* e = solver.MakeIntVar(0, 9, "e");
  IntVar* n = solver.MakeIntVar(0, 9, "n");
  IntVar* d = solver.MakeIntVar(0, 9, "d");
  IntVar* r = solver.MakeIntVar(0, 9, "r");
  IntVar* y = solver.MakeIntVar(0, 9, "y");

  std::vector<IntVar*> letters;
  letters.push_back(s);
  letters.push_back(m);
  letters.push_back(o);
  letters.push_back(e);
  letters.push_back(n);
  letters.push_back(d);
  letters.push_back(r);
  letters.push_back(y);

  solver.AddConstraint(solver.MakeAllDifferent(letters));

  // carry variables
  IntVar* c1 = solver.MakeIntVar(0, 1, "c1");
  IntVar* c2 = solver.MakeIntVar(0, 1, "c2");
  IntVar* c3 = solver.MakeIntVar(0, 1, "c3");

  // initial constraint is separated into four small constraints
  IntVar* v1 = solver.MakeSum(e, d)->Var();
  IntVar* v2 = solver.MakeSum(y, solver.MakeProd(c1, 10))->Var();
  solver.AddConstraint(solver.MakeEquality(v1, v2));

  v1 = solver.MakeSum(solver.MakeSum(c1, n), r)->Var();
  v2 = solver.MakeSum(e, solver.MakeProd(c2, 10))->Var();
  solver.AddConstraint(solver.MakeEquality(v1, v2));

  v1 = solver.MakeSum(solver.MakeSum(c2, e), o)->Var();
  v2 = solver.MakeSum(n, solver.MakeProd(c3, 10))->Var();
  solver.AddConstraint(solver.MakeEquality(v1, v2));

  v1 = solver.MakeSum(solver.MakeSum(c3, s), m)->Var();
  v2 = solver.MakeSum(o, solver.MakeProd(m, 10))->Var();
  solver.AddConstraint(solver.MakeEquality(v1, v2));

  DecisionBuilder* const db = solver.MakePhase(
      letters, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  if (solver.NextSolution()) {
    CHECK_EQ(s->Value(), 9);
    CHECK_EQ(m->Value(), 1);
    CHECK_EQ(o->Value(), 0);
    CHECK_EQ(e->Value(), 5);
    CHECK_EQ(n->Value(), 6);
    CHECK_EQ(d->Value(), 7);
    CHECK_EQ(r->Value(), 8);
    CHECK_EQ(y->Value(), 2);

    LOG(INFO) << "S=" << s->Value();
    LOG(INFO) << "M=" << m->Value();
    LOG(INFO) << "O=" << o->Value();
    LOG(INFO) << "E=" << e->Value();
    LOG(INFO) << "N=" << n->Value();
    LOG(INFO) << "D=" << d->Value();
    LOG(INFO) << "R=" << r->Value();
    LOG(INFO) << "Y=" << y->Value();
  } else {
    LOG(INFO) << "Cannot solve problem: number of failures "
              << solver.failures();
  }
  solver.EndSearch();
}

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::Cryptoarithmetics();
  return 0;
}
