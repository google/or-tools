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

#include "base/hash.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

namespace operations_research {
void OverflowTest() {
  // It works on mac-clang, but fails with gcc.
  Solver solver("OverflowTest");
  IntVar* const x = solver.MakeIntVar(kint64min, kint64max, "x");
  IntVar* const y = solver.MakeIntVar(kint64min, kint64max, "y");
  IntExpr* const z = solver.MakeDifference(x, y);
  LOG(INFO) << z->DebugString();
  Constraint* const ct = solver.MakeGreaterOrEqual(z, 10);
  LOG(INFO) << ct->DebugString();
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::OverflowTest();
  return 0;
}
