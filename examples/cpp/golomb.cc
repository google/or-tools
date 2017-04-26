// Copyright 2010-2014 Google
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
// Golomb ruler problem
//
//  find minimal ruler so that the differences between ticks are unique.
//
// First solutions:
//   0, 1
//   0, 1, 3
//   0, 1, 4,  6
//   0, 1, 4,  9, 11
//   0, 1, 4, 10, 12, 17
//   0, 1, 4, 10, 18, 23, 25

#include <cstdio>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"

DEFINE_bool(print, false, "If true, print the minimal solution.");
DEFINE_int32(
    size, 0,
    "Size of the problem. If equal to 0, will test several increasing sizes.");

static const int kBestSolutions[] = {0,   1,   3,   6,   11,  17,  25,
                                     34,  44,  55,  72,  85,
                                     // just for the optimistics ones, the rest:
                                     106, 127, 151, 177, 199, 216, 246};

static const int kKnownSolutions = 19;

namespace operations_research {

void GolombRuler(int size) {
  CHECK_GE(size, 1);
  Solver s("golomb");

  // model
  std::vector<IntVar*> ticks(size);
  ticks[0] = s.MakeIntConst(0);  // X(0) = 0
  const int64 max = 1 + size * size * size;
  for (int i = 1; i < size; ++i) {
    ticks[i] = s.MakeIntVar(1, max, StringPrintf("X%02d", i));
  }
  std::vector<IntVar*> diffs;
  for (int i = 0; i < size; ++i) {
    for (int j = i + 1; j < size; ++j) {
      IntVar* const diff = s.MakeDifference(ticks[j], ticks[i])->Var();
      diffs.push_back(diff);
      diff->SetMin(1);
    }
  }
  s.AddConstraint(s.MakeAllDifferent(diffs));

  OptimizeVar* const length = s.MakeMinimize(ticks[size - 1], 1);
  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->Add(ticks);
  DecisionBuilder* const db = s.MakePhase(ticks, Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);
  s.Solve(db, collector, length);  // go!
  CHECK_EQ(collector->solution_count(), 1);
  const int64 result = collector->Value(0, ticks[size - 1]);
  const int num_failures = collector->failures(0);
  printf("N = %d, optimal length = %d (fails:%d)\n", size,
         static_cast<int>(result), num_failures);
  if (size - 1 < kKnownSolutions) {
    CHECK_EQ(result, kBestSolutions[size - 1]);
  }
  if (FLAGS_print) {
    for (int i = 0; i < size; ++i) {
      const int64 tick = collector->Value(0, ticks[i]);
      printf("%d ", static_cast<int>(tick));
    }
    printf("\n");
  }
}

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::GolombRuler(FLAGS_size);
  } else {
    for (int n = 1; n < 11; ++n) {
      operations_research::GolombRuler(n);
    }
  }
  return 0;
}
