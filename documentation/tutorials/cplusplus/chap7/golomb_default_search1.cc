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
//
// Simple use of DefaultPhase to solve the Golomb Rule Problem.

#include <vector>
#include <sstream>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_int32(n, 0, "Number of marks. If 0 will test different values of n.");
DEFINE_bool(print, false, "Print solution or not?");

// kG[n] = G(n).
static const int kG[] = {
  -1, 0, 1, 3, 6, 11, 17, 25, 34, 44, 55, 72, 85,
  106, 127, 151, 177, 199, 216, 246
};

static const int kKnownSolutions = 19;

namespace operations_research {

void GolombRuler(int n) {
  CHECK_GE(n, 1);

  Solver s("golomb");

  // Upper bound on G(n), only valid for n <= 65 000
  CHECK_LE(n, 65000);
  const int64 max = n * n - 1;

  // Variables
  std::vector<IntVar*> X(n + 1);
  X[0] = s.MakeIntConst(-1);  // The solver doesn't allow NULL pointers
  X[1] = s.MakeIntConst(0);   // X(1) = 0

  for (int i = 2; i <= n; ++i) {
    X[i] = s.MakeIntVar(1, max, StringPrintf("X%03d", i));
  }

  std::vector<IntVar*> Y;
  for (int i = 1; i <= n; ++i) {
    for (int j = i + 1; j <= n; ++j) {
      IntVar* const diff = s.MakeDifference(X[j], X[i])->Var();
      Y.push_back(diff);
      diff->SetMin(1);
    }
  }

  s.AddConstraint(s.MakeAllDifferent(Y));

  OptimizeVar* const length = s.MakeMinimize(X[n], 1);

  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->Add(X);
  DecisionBuilder* const db = s.MakeDefaultPhase(X);

  s.Solve(db, collector, length);  // go!

  CHECK_EQ(collector->solution_count(), 1);
  const int64 result = collector->Value(0, X[n]);
  LOG(INFO) << "G(" << n << ") = " << result;
  LOG(INFO) << "Time: " << s.wall_time()/1000.0 << " s";

  if (FLAGS_print) {
    std::ostringstream solution_str;
    solution_str << "Solution: ";
    for (int i = 1; i <= n; ++i) {
      solution_str << collector->Value(0, X[i]) << " ";
    }
    LOG(INFO) << solution_str.str();
  }

  if (n < kKnownSolutions) {
    CHECK_EQ(result, kG[n]);
  }
}

}   // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_n != 0) {
    operations_research::GolombRuler(FLAGS_n);
  } else {
    for (int n = 4; n < 11; ++n) {
      operations_research::GolombRuler(n);
    }
  }
  return 0;
}
