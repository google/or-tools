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

#include "ortools/sat/cp_model.proto.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.proto.h"

namespace operations_research {
namespace sat {

void ChannelingSample() {
  // Model.
  CpModelProto cp_model;

  // Helpers.
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  // literal => (lb <= sum(vars) <= ub).
  auto add_half_reified_sum = [&cp_model](const std::vector<int>& vars,
                                          int64 lb, int64 ub, int literal) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(literal);
    LinearConstraintProto* const lin = ct->mutable_linear();
    for (const int v : vars) {
      lin->add_vars(v);
      lin->add_coeffs(1);
    }
    lin->add_domain(lb);
    lin->add_domain(ub);
  };

  // Main variables.
  const int x = new_variable(0, 10);
  const int y = new_variable(0, 10);
  const int b = new_variable(0, 1);

  // Implements b == (x >= 5).
  add_half_reified_sum({x}, 5, kint64max, b);
  add_half_reified_sum({x}, kint64min, 4, NegatedRef(b));

  // b implies (y == 10 - x).
  add_half_reified_sum({x, y}, 10, 10, b);
  // not(b) implies y == 0.
  add_half_reified_sum({y}, 0, 0, NegatedRef(b));

  // Search for x values in increasing order.
  DecisionStrategyProto* const strategy = cp_model.add_search_strategy();
  strategy->add_variables(x);
  strategy->set_variable_selection_strategy(
      DecisionStrategyProto::CHOOSE_FIRST);
  strategy->set_domain_reduction_strategy(
      DecisionStrategyProto::SELECT_MIN_VALUE);

  // Solving part.
  Model model;

  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << r.solution(x) << " y=" << r.solution(y)
              << " b=" << r.solution(b);
  }));
  SolveCpModel(cp_model, &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ChannelingSample();

  return EXIT_SUCCESS;
}
