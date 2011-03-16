// Copyright 2010 Google
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

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"
#include "examples/global_arith.h"

class MySearchLog {
 public:
  MySearchLog(){}

  string Display() {
    string response;
    response += "****** Solution *******";
    return response;
  }
};

int main() {
  operations_research::Solver solver("bin_packing");
  MySearchLog myLog;
  ResultCallback<string>* display = NewPermanentCallback(&myLog, &MySearchLog::Display);
  operations_research::SearchMonitor* const log = solver.MakeSearchLog(1000, display);
  return 0;
}

/*DEFINE_int32(size, 20, "Size of the problem");

namespace operations_research {
// ---------- Examples ----------

void DeepSearchTreeArith(int size) {
  LOG(INFO) << "DeepSearchTreeArith: size = " << size;
  const int64 rmax = 1 << size;

  Solver solver("DeepSearchTreeArith");
  IntVar* const v1 = solver.MakeIntVar(1, rmax, "v1");
  IntVar* const v2 = solver.MakeIntVar(0, rmax, "v2");
  IntVar* const v3 = solver.MakeIntVar(0, rmax, "v3");

  GlobalArithmeticConstraint* const global =
      solver.RevAlloc(new GlobalArithmeticConstraint(&solver));

  global->Add(global->MakeRowConstraint(0, v1, 1, v2, -1, 0));
  global->Add(global->MakeRowConstraint(0, v2, 1, v3, -1, 0));

  global->Add(global->MakeOrConstraint(
      global->MakeRowConstraint(0, v1, -1, v2, -1, v3, 1, kint64max),
      global->MakeRowConstraint(0, v1, -1, v2, 1, v3, -1, kint64max)));

  global->Post();
}

void SlowPropagationArith(int size) {
  LOG(INFO) << "SlowPropagationArith: size = " << size;
  const int64 rmin = -(1 << size);
  const int64 rmax = 1 << size;

  Solver solver("SlowPropagationArith");
  IntVar* const v1 = solver.MakeIntVar(rmin, rmax, "v1");
  IntVar* const v2 = solver.MakeIntVar(rmin, rmax, "v2");

  GlobalArithmeticConstraint* const global =
      solver.RevAlloc(new GlobalArithmeticConstraint(&solver));

  global->Add(global->MakeRowConstraint(1, v1, 1, v2, -1, kint64max));
  global->Add(global->MakeRowConstraint(0, v1, -1, v2, 1, kint64max));

  global->Post();
}

void DeepSearchTree(int size){
  LOG(INFO) << "DeepSearchTree: size = " << size;
  Solver s("DeepSearchTree");
  const int64 rmax = 1 << size;
  IntVar* const i = s.MakeIntVar(1, rmax, "i");
  IntVar* const j = s.MakeIntVar(0, rmax, "j");
  IntVar* const k = s.MakeIntVar(0, rmax, "k");

  s.AddConstraint(s.MakeEquality(i, j));
  s.AddConstraint(s.MakeEquality(j, k));
  IntVar* const left = s.MakeIsLessOrEqualVar(s.MakeSum(i, j), k);
  IntVar* const right = s.MakeIsLessOrEqualVar(s.MakeSum(i, k), j);

  s.AddConstraint(s.MakeGreater(s.MakeSum(left, right), Zero()));

  // search decision
  DecisionBuilder* const db = s.MakePhase(i, j, k,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* const log = s.MakeSearchLog(100000);

  s.Solve(db, log);  // go!
}


void SlowPropagation(int size) {
  LOG(INFO) << "SlowPropagation: size = " << size;
  Solver s("SlowPropagation");
  const int64 rmin = -(1 << size);
  const int64 rmax = 1 << size;
  IntVar* const i = s.MakeIntVar(rmin, rmax, "i");
  IntVar* const j = s.MakeIntVar(rmin, rmax, "j");
  s.AddConstraint(s.MakeGreater(i, j));
  s.AddConstraint(s.MakeLessOrEqual(i, j));

  DecisionBuilder* const db = s.MakePhase(i, j,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);

  SearchMonitor* const log = s.MakeSearchLog(100000);

  s.Solve(db, log);
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::DeepSearchTreeArith(FLAGS_size);
  operations_research::SlowPropagationArith(FLAGS_size);
  // operations_research::DeepSearchTree(FLAGS_size);
  // operations_research::SlowPropagation(FLAGS_size);
  return 0;
}
*/
