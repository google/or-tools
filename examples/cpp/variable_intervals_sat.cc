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

#include <atomic>

#include "absl/log/log.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

void Solve() {
  CpModelBuilder cp_model;

  const IntVar start_ins = cp_model.NewIntVar(Domain(660, 755));
  const IntVar duration_ins = cp_model.NewConstant(25);
  const IntVar end_ins = cp_model.NewIntVar(Domain(685, 780));
  const IntervalVar ins =
      cp_model.NewIntervalVar(start_ins, duration_ins, end_ins);

  const IntVar start_p1 = cp_model.NewIntVar(Domain(500, 800));
  const IntVar duration_p1 = cp_model.NewIntVar(Domain(1, 360));
  const IntVar end_p1 = cp_model.NewIntVar(Domain(500, 1000));
  const IntervalVar p1 = cp_model.NewIntervalVar(start_p1, duration_p1, end_p1);

  const IntVar start_p2 = cp_model.NewIntVar(Domain(500, 800));
  const IntVar duration_p2 = cp_model.NewIntVar(Domain(1, 360));
  const IntVar end_p2 = cp_model.NewIntVar(Domain(500, 1000));
  const IntervalVar p2 = cp_model.NewIntervalVar(start_p2, duration_p2, end_p2);

  cp_model.AddEquality(duration_p1 + duration_p2, 360);
  cp_model.AddLessOrEqual(end_p1, start_p2);

  cp_model.AddNoOverlap({ins, p1, p2});

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  const int kSolutionLimit = 100;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  start_p1 = " << SolutionIntegerValue(r, start_p1);
    LOG(INFO) << "  duration_p1 = " << SolutionIntegerValue(r, duration_p1);
    LOG(INFO) << "  start_p2 = " << SolutionIntegerValue(r, start_p2);
    LOG(INFO) << "  duration_p2 = " << SolutionIntegerValue(r, duration_p2);
    LOG(INFO) << "  start_ins = " << SolutionIntegerValue(r, start_ins);
    num_solutions++;
    if (num_solutions >= kSolutionLimit) {
      StopSearch(&model);
      LOG(INFO) << "Stop search after " << kSolutionLimit << " solutions.";
    }
  }));

  SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::Solve();

  return EXIT_SUCCESS;
}
