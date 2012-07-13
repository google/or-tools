// Copyright 2011-2012 Google
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


#include "base/hash.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/model.pb.h"
#include "util/string_array.h"
#include "core/Solver.h"

namespace operations_research {
class SatPropagator;
bool AddBoolEq(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right);

bool AddBoolLe(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right);

bool AddBoolNot(SatPropagator* const sat,
                IntExpr* const left,
                IntExpr* const right);

bool AddBoolAndArrayEqVar(SatPropagator* const sat,
                          const std::vector<IntVar*>& vars,
                          IntExpr* const target);

bool AddBoolOrArrayEqVar(SatPropagator* const sat,
                         const std::vector<IntVar*>& vars,
                         IntExpr* const target);

bool AddBoolAndEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target);

bool AddBoolIsNEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target);

bool AddBoolOrEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target);

bool AddBoolIsEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target);


bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars);

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars);

SatPropagator* MakeSatPropagator(Solver* const solver);

void TestConversions() {
  LOG(INFO) << "lbool(false) -> " << toInt(Minisat::lbool(false));
  LOG(INFO) << "lbool(true) -> " << toInt(Minisat::lbool(true));
}

void TestBoolLe(int rotation) {
  LOG(INFO) << "TestBoolLe(" << rotation << ")";
  Solver solver("TestBoolLe");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  CHECK(AddBoolLe(sat, x, y));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value() << ", y = " << y->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolEq(int rotation) {
  LOG(INFO) << "TestBoolEq(" << rotation << ")";
  Solver solver("TestBoolEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  CHECK(AddBoolEq(sat, x, y));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value() << ", y = " << y->Value();
  }
  solver.EndSearch();
  CHECK_EQ(2, solver.solutions());
  LOG(INFO) << solver.DebugString();
}

void TestBoolNot(int rotation) {
  LOG(INFO) << "TestBoolNot(" << rotation << ")";
  Solver solver("TestBoolNot");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  CHECK(AddBoolNot(sat, x, y));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value() << ", y = " << y->Value();
  }
  solver.EndSearch();
  CHECK_EQ(2, solver.solutions());
  LOG(INFO) << solver.DebugString();
}

void TestBoolAndEq(int rotation) {
  LOG(INFO) << "TestBoolAndEq(" << rotation << ")";
  Solver solver("TestBoolAndEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  CHECK(AddBoolAndEqVar(sat, x, y, z));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(z, y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolOrEq(int rotation) {
  LOG(INFO) << "TestBoolOrEq(" << rotation << ")";
  Solver solver("TestBoolOrEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  CHECK(AddBoolOrEqVar(sat, x, y, z));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(z, y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolArrayAndEq(int rotation) {
  LOG(INFO) << "TestBoolArrayAndEq(" << rotation << ")";
  Solver solver("TestBoolArrayAndEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  IntVar* const t = solver.MakeBoolVar("t");
  std::vector<IntVar*> vars;
  vars.push_back(x);
  vars.push_back(y);
  vars.push_back(z);
  CHECK(AddBoolAndArrayEqVar(sat, vars, t));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z, t,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(t, y, x, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value()
              << ", t = " << t->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolArrayOrEq(int rotation) {
  LOG(INFO) << "TestBoolArrayOrEq(" << rotation << ")";
  Solver solver("TestBoolArrayOrdEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  IntVar* const t = solver.MakeBoolVar("t");
  std::vector<IntVar*> vars;
  vars.push_back(x);
  vars.push_back(y);
  vars.push_back(z);
  CHECK(AddBoolOrArrayEqVar(sat, vars, t));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z, t,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(t, y, x, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value()
              << ", t = " << t->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolIsEq(int rotation) {
  LOG(INFO) << "TestBoolIsEq(" << rotation << ")";
  Solver solver("TestBoolIsEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  CHECK(AddBoolIsEqVar(sat, x, y, z));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(z, y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolIsNEq(int rotation) {
  LOG(INFO) << "TestBoolIsNEq(" << rotation << ")";
  Solver solver("TestBoolIsNEq");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  CHECK(AddBoolIsNEqVar(sat, x, y, z));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(z, y, x,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolArrayAndEqFalse(int rotation) {
  LOG(INFO) << "TestBoolArrayAndEqFalse(" << rotation << ")";
  Solver solver("TestBoolArrayAndEqFalse");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  std::vector<IntVar*> vars;
  vars.push_back(x);
  vars.push_back(y);
  vars.push_back(z);
  CHECK(AddBoolAndArrayEqualFalse(sat, vars));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(y, x, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestBoolArrayOrEqTrue(int rotation) {
  LOG(INFO) << "TestBoolArrayOrEqTrue(" << rotation << ")";
  Solver solver("TestBoolArrayOrdEqTrue");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  IntVar* const z = solver.MakeBoolVar("z");
  std::vector<IntVar*> vars;
  vars.push_back(x);
  vars.push_back(y);
  vars.push_back(z);
  CHECK(AddBoolOrArrayEqualTrue(sat, vars));
  DecisionBuilder* const db = rotation == 1 ?
      solver.MakePhase(x, y, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE) :
      solver.MakePhase(y, x, z,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value()
              << ", y = " << y->Value()
              << ", z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();
}

void TestInconsistent() {
  LOG(INFO) << "TestInconsistent";
  Solver solver("TestInconsistent");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
  CHECK(AddBoolEq(sat, x, y));
  CHECK(AddBoolEq(sat, x, solver.MakeDifference(1, y)));
  DecisionBuilder* const db =
      solver.MakePhase(x, y,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << " x = " << x->Value() << ", y = " << y->Value();
  }
  solver.EndSearch();
  LOG(INFO) << solver.DebugString();

}
}  // namespace operations_research


int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestConversions();
  operations_research::TestBoolLe(1);
  operations_research::TestBoolLe(2);
  operations_research::TestBoolEq(1);
  operations_research::TestBoolEq(2);
  operations_research::TestBoolNot(1);
  operations_research::TestBoolNot(2);
  operations_research::TestBoolAndEq(1);
  operations_research::TestBoolAndEq(2);
  operations_research::TestBoolOrEq(1);
  operations_research::TestBoolOrEq(2);
  operations_research::TestBoolArrayAndEq(1);
  operations_research::TestBoolArrayAndEq(2);
  operations_research::TestBoolArrayOrEq(1);
  operations_research::TestBoolArrayOrEq(2);
  operations_research::TestBoolIsEq(1);
  operations_research::TestBoolIsEq(2);
  operations_research::TestBoolIsNEq(1);
  operations_research::TestBoolIsNEq(2);
  operations_research::TestBoolArrayAndEqFalse(1);
  operations_research::TestBoolArrayAndEqFalse(2);
  operations_research::TestBoolArrayOrEqTrue(1);
  operations_research::TestBoolArrayOrEqTrue(2);
  operations_research::TestInconsistent();
  return 0;
}
