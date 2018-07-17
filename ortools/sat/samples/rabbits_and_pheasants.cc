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

void RabbitsAndPheasants() {
  CpModelProto cp_model;

  // Trivial model with just one variable and no constraint.
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto add_linear_constraint = [&cp_model](const std::vector<int>& vars,
                                           const std::vector<int64>& coeffs,
                                           int64 lb, int64 ub) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    for (const int v : vars) {
      lin->add_vars(v);
    }
    for (const int64 c : coeffs) {
      lin->add_coeffs(c);
    }
    lin->add_domain(lb);
    lin->add_domain(ub);
  };

  // Creates variables.
  const int r = new_variable(0, 100);
  const int p = new_variable(0, 100);

  // 20 heads.
  add_linear_constraint({r, p}, {1, 1}, 20, 20);
  // 56 legs.
  add_linear_constraint({r, p}, {4, 2}, 56, 56);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << response.solution(r) << " rabbits, and "
              << response.solution(p) << " pheasants";
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::RabbitsAndPheasants();

  return EXIT_SUCCESS;
}
