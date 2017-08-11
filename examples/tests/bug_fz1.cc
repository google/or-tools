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

#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/random.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"

namespace operations_research {
void ShoppingBasketBug() {
  Solver s("ShoppingBasketBug");
  IntVar* const x15 = s.MakeIntVar(0, 2, "x15");
  IntVar* const x18 = s.MakeIntVar(0, 2, "x18");
  IntVar* const is1 = s.MakeIsEqualCstVar(x15, 2);
  IntVar* const is2 = s.MakeIsEqualCstVar(x18, 2);
  IntVar* const is_less = s.MakeIsLessOrEqualCstVar(
      s.MakeSum(s.MakeProd(is1, 2), s.MakeProd(is2, 2)), 1);
  std::vector<int64> values1;
  values1.push_back(10);
  values1.push_back(2);
  values1.push_back(12);
  IntVar* const elem1 = s.MakeElement(values1, x15)->Var();
  std::vector<int64> values2;
  values2.push_back(2);
  values2.push_back(10);
  values2.push_back(5);
  IntVar* const elem2 = s.MakeElement(values2, x18)->Var();
  std::vector<IntVar*> vars;
  vars.push_back(elem1);
  vars.push_back(is_less);
  vars.push_back(elem2);
  std::vector<int64> coefs;
  coefs.push_back(1);
  coefs.push_back(90);
  coefs.push_back(1);
  IntVar* const obj = s.MakeScalProd(vars, coefs)->Var();
  OptimizeVar* const optimize = s.MakeMinimize(obj, 1);
  SearchMonitor* const log = s.MakeSearchLog(10, optimize);
  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->Add(x15);
  collector->Add(x18);
  collector->Add(is_less);
  collector->Add(elem1);
  collector->Add(elem2);
  collector->Add(is1);
  collector->Add(is2);
  DecisionBuilder* const db1 =
      s.MakePhase(x15, x18, Solver::CHOOSE_MAX_SIZE, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db2 = s.MakePhase(obj, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = s.Compose(db1, db2);
  s.Solve(db, optimize, log, collector);
  LOG(INFO) << collector->solution(0)->DebugString();
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::ShoppingBasketBug();
  return 0;
}
