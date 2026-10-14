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

#include <cstdint>
#include <vector>

#include "benchmark/benchmark.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
namespace {

static const int64_t kMediumAssignments = 1 << 11;
static const int64_t kLargeAssignments = 1 << 12;
static const int64_t kMin = 0;
static const int64_t kMax = 10;

#define CP_BM_AssignmentAction(action)                            \
  std::vector<IntVar*> vars;                                      \
  solver.MakeIntVarArray(assignment_size, kMin, kMax, "", &vars); \
  for (auto _ : state) {                                          \
    state.PauseTiming();                                          \
    Assignment assignment(&solver);                               \
    state.ResumeTiming();                                         \
    for (int j = 0; j < assignment_size; ++j) {                   \
      assignment.action(vars[j]);                                 \
    }                                                             \
  }

void BM_AssignmentAdd(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentAdd");
  CP_BM_AssignmentAction(Add);
}

BENCHMARK(BM_AssignmentAdd)->Range(1, kLargeAssignments);

void BM_AssignmentFastAdd(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentFastAdd");
  CP_BM_AssignmentAction(FastAdd);
}

BENCHMARK(BM_AssignmentFastAdd)->Range(1, kLargeAssignments);

void BM_AssignmentElement(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentFastAdd");
  Assignment assignment(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(assignment_size, kMin, kMax, "", &vars);
  assignment.Add(vars);
  const Assignment::IntContainer& container = assignment.IntVarContainer();
  for (auto s : state) {
    for (int j = 0; j < assignment_size; ++j) {
      container.Element(vars[j]);
    }
  }
}

BENCHMARK(BM_AssignmentElement)->Range(1, kLargeAssignments);
BENCHMARK(BM_AssignmentElement)
    ->Arg(1)
    ->Arg(3)
    ->Arg(7)
    ->Arg(9)
    ->Arg(10)
    ->Arg(11)
    ->Arg(12)
    ->Arg(13);

void BM_AssignmentCopyIntersection(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentCopyIntersection");
  Assignment assignment1(&solver);
  Assignment assignment2(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(assignment_size, kMin, kMax, "", &vars);
  assignment1.Add(vars);
  assignment2.Add(vars);
  for (auto s : state) {
    assignment1.CopyIntersection(&assignment2);
  }
}

BENCHMARK(BM_AssignmentCopyIntersection)->Range(1, kMediumAssignments);

void BM_AssignmentClone(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentClone");
  Assignment assignment1(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(assignment_size, kMin, kMax, "", &vars);
  assignment1.Add(vars);
  for (auto s : state) {
    Assignment assignment2(&assignment1);
  }
}

BENCHMARK(BM_AssignmentClone)->Range(1, kMediumAssignments);

void BM_AssignmentCopy(benchmark::State& state) {
  const int assignment_size = state.range(0);
  Solver solver("BM_AssignmentCopy");
  Assignment assignment1(&solver);
  Assignment assignment2(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(assignment_size, kMin, kMax, "", &vars);
  assignment1.Add(vars);
  for (auto s : state) {
    assignment2.Copy(&assignment1);
  }
}

BENCHMARK(BM_AssignmentCopy)->Range(1, kMediumAssignments);

#undef CP_BM_AssignmentAction

}  // namespace
}  // namespace operations_research
