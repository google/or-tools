// Copyright 2010-2025 Google LLC
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

// Pure speed measurement of the AllDifferent constraint

#include <stdio.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "ortools/base/init_google.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"

ABSL_FLAG(bool, use_range, false, "If true, use AllDifferenceRange.");
ABSL_FLAG(bool, print, false, "If true, print one of the solution.");
ABSL_FLAG(int, nb_loops, 1,
          "Number of solving loops to perform, for performance timing.");
ABSL_FLAG(
    int, size, 0,
    "Max size of the problem. If equal to 0, will test few increasing sizes.");

namespace operations_research {
static int fact(int N) { return (N > 0) ? N * fact(N - 1) : 1; }

void TestAllDiff(int size) {
  CHECK_GE(size, 1);
  for (int K = 1; K <= size + 2; ++K) {
    std::unique_ptr<Solver> s(new Solver("nqueens"));

    // model: we ask K variables with range [0,size) to be all different
    std::vector<IntVar*> vars;
    for (int i = 0; i < K; ++i) {
      vars.push_back(s->MakeIntVar(0, size - 1, absl::StrFormat("var%04d", i)));
    }
    s->AddConstraint(s->MakeAllDifferent(vars, absl::GetFlag(FLAGS_use_range)));

    std::unique_ptr<Assignment> a(new Assignment(s.get()));
    a->Add(vars);
    SolutionCollector* const c = s->MakeAllSolutionCollector(a.get());
    DecisionBuilder* const db = s->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                             Solver::ASSIGN_MIN_VALUE);

    s->Solve(db, c);
    const int64_t num_solutions = c->solution_count();
    int64_t num_failures = 0;
    for (int i = 0; i < num_solutions; ++i) {
      num_failures += c->failures(i);
    }
    if (absl::GetFlag(FLAGS_print)) {
      for (int k = 0; k < num_solutions; ++k) {
        const Assignment* const b = c->solution(k);
        for (int i = 0; i < K; ++i) {
          absl::PrintF("[%d]", static_cast<int>(b->Value(vars[i])));
        }
        absl::PrintF("\n");
      }
    }
    absl::PrintF(
        "=== Size = %d, %d variables: %d solutions, "
        "%d fails, %d ms\n",
        size, K, num_solutions, num_failures, s->wall_time());
    if (K <= size) {
      CHECK_EQ(fact(size) / fact(size - K), num_solutions);
    } else {
      CHECK_EQ(0, num_solutions);
    }
  }
}

}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  if (absl::GetFlag(FLAGS_size) != 0) {
    operations_research::TestAllDiff(absl::GetFlag(FLAGS_size));
  } else {
    for (int n = 1; n < 9; ++n) {
      operations_research::TestAllDiff(n);
    }
  }
  return 0;
}
