// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void StopAfterNSolutions() {
  CpModelProto cp_model;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto add_different = [&cp_model](const int left_var, const int right_var) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(left_var);
    lin->add_coeffs(1);
    lin->add_vars(right_var);
    lin->add_coeffs(-1);
    lin->add_domain(kint64min);
    lin->add_domain(-1);
    lin->add_domain(1);
    lin->add_domain(kint64max);
  };

  const int kNumVals = 3;
  const int x = new_variable(0, kNumVals - 1);
  const int y = new_variable(0, kNumVals - 1);
  const int z = new_variable(0, kNumVals - 1);

  add_different(x, y);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  // Create an atomic Boolean that will be periodically checked by the limit.
  std::atomic<bool> stopped;
  model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
      &stopped);

  const int kSolutionLimit = 5;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << r.solution(x);
    LOG(INFO) << "  y = " << r.solution(y);
    LOG(INFO) << "  z = " << r.solution(z);
    num_solutions++;
    if (num_solutions >= kSolutionLimit) {
      stopped = true;
      LOG(INFO) << "Stop search after " << kSolutionLimit << " solutions.";
    }
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::StopAfterNSolutions();

  return EXIT_SUCCESS;
}
