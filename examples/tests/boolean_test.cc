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
                          IntVar* const target);

bool AddBoolAndEqVar(SatPropagator* const sat,
                     IntVar* const left,
                     IntVar* const right,
                     IntVar* const target);

bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars);

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars);

SatPropagator* MakeSatPropagator(Solver* const solver);

void TestApi() {
  Solver solver("TestApi");
  SatPropagator* const sat = MakeSatPropagator(&solver);
  solver.AddConstraint(reinterpret_cast<Constraint*>(sat));
  IntVar* const x = solver.MakeBoolVar("x");
  IntVar* const y = solver.MakeBoolVar("y");
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
}
}  // namespace operations_research


int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestApi();
  return 0;
}
