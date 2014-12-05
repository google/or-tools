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
// First implementation of the Golomb Ruler Problem.
//
// Same as golomb2.cc with some global statistics and 
// the use of SearchLimits.
#include <vector>
#include <sstream>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_int32(n, 0, "Number of marks. If 0 will test different values of n.");
DEFINE_bool(print, false, "Print solution or not?");

// kG(n) = G(n+1)
static const int kG[] = {
  0, 1, 3, 6, 11, 17, 25, 34, 44, 55, 72, 85,
  106, 127, 151, 177, 199, 216, 246
};

static const int kKnownSolutions = 19;

namespace operations_research {

  void Golomb(const int n) {
    CHECK_GE(n, 2);

    Solver s("golomb");

    // Upper bound on G(n), only valid for n <= 65 000
    CHECK_LE(n, 65000);
    const int64 max = n * n - 1;

    // Variables
    const int64 num_vars = (n*(n - 1))/2;
    std::vector<IntVar*> Y;
    s.MakeIntVarArray(num_vars, 1, max, "Y_", &Y);

    // Constraints
    s.AddConstraint(s.MakeAllDifferent(Y));

    int index = n - 2;
    IntVar* v2 = NULL;
    Constraint* c = NULL;
    for (int i = 2; i <= n - 1; ++i) {
       for (int j = 0; j < n-i; ++j) {
         ++index;
         v2 = Y[j];
         for (int l = j + 1; l <=  j + i - 1 ; ++l) {
           v2 = s.MakeSum(Y[l], v2)->Var();
         }
         c = s.MakeEquality(Y[index], v2);
         s.AddConstraint(c);
       }
    }
    CHECK_EQ(index, num_vars - 1);

    OptimizeVar* const length = s.MakeMinimize(Y[num_vars-1], 1);
    SolutionCollector* const collector = s.MakeLastSolutionCollector();
    collector->Add(Y);

    DecisionBuilder* const db = s.MakePhase(Y,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);

    // Max 10 seconds
    SearchLimit* const time_limit = s.MakeTimeLimit(10000);

    s.Solve(db, collector, length, time_limit);  // go!
    CHECK_EQ(collector->solution_count(), 1);
    const int64 result = collector->Value(0, Y[num_vars - 1]);
    LG << "G(" << n << ") = " << result;
    LOG(INFO) << "Time: " << s.wall_time()/1000.0 << " s";
    LOG(INFO) << "Failures: " << s.failures();
    LOG(INFO) << "Fail stamps: " << s.fail_stamp();
    LOG(INFO) << "Branches: " << s.branches() << std::endl;

    if (FLAGS_print) {
      int64 tick = 0;
      std::ostringstream solution_str;
      solution_str << "Solution: ";
      for (int i = 0; i <= n - 1; ++i) {
        solution_str << tick << " ";
        tick += collector->Value(0, Y[i]);
      }
      LOG(INFO) << solution_str.str();
    }
    if (n <= kKnownSolutions) {
      CHECK_EQ(result, kG[n-1]);
    }
    return;
  }  // void Golomb(const int n)

}   // namespace operations_research


// ----- MAIN -----
int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_n != 0) {
    operations_research::Golomb(FLAGS_n);
  } else {
    for (int n = 4; n < 11; ++n) {
      operations_research::Golomb(n);
    }
  }
  return 0;
}
