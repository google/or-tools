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

namespace operations_research {
namespace sat {

void BinpackingProblem() {
  // Data.
  const int kBinCapacity = 100;
  const int kSlackCapacity = 20;
  const int kNumBins = 5;

  const std::vector<std::vector<int>> items = {
      {20, 6}, {15, 6}, {30, 4}, {45, 3}};
  const int num_items = items.size();

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

  auto add_reified_variable_bounds = [&cp_model](int var, int64 lb, int64 ub,
                                                 int lit) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(lit);
    LinearConstraintProto* const lin = ct->mutable_linear();
    lin->add_vars(var);
    lin->add_coeffs(1);
    lin->add_domain(lb);
    lin->add_domain(ub);
  };

  auto maximize = [&cp_model](const std::vector<int>& vars) {
    CpObjectiveProto* const obj = cp_model.mutable_objective();
    for (const int v : vars) {
      obj->add_vars(v);
      obj->add_coeffs(-1);  // Maximize.
    }
    obj->set_scaling_factor(-1.0);  // Maximize.
  };

  // Main variables.
  std::vector<std::vector<int>> x(num_items);
  for (int i = 0; i < num_items; ++i) {
    const int num_copies = items[i][1];
    for (int b = 0; b < kNumBins; ++b) {
      x[i].push_back(new_variable(0, num_copies));
    }
  }

  // Load variables.
  std::vector<int> load(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    load[b] = new_variable(0, kBinCapacity);
  }

  // Slack variables.
  std::vector<int> slack(kNumBins);
  for (int b = 0; b < kNumBins; ++b) {
    slack[b] = new_variable(0, 1);
  }

  // Links load and x.
  for (int b = 0; b < kNumBins; ++b) {
    std::vector<int> vars;
    std::vector<int64> coeffs;
    vars.push_back(load[b]);
    coeffs.push_back(-1);
    for (int i = 0; i < num_items; ++i) {
      vars.push_back(x[i][b]);
      coeffs.push_back(items[i][0]);
    }
    add_linear_constraint(vars, coeffs, 0, 0);
  }

  // Place all items.
  for (int i = 0; i < num_items; ++i) {
    std::vector<int> vars;
    std::vector<int64> coeffs;
    for (int b = 0; b < kNumBins; ++b) {
      vars.push_back(x[i][b]);
      coeffs.push_back(1);
    }
    add_linear_constraint(vars, coeffs, items[i][1], items[i][1]);
  }

  // Links load and slack through an equivalence relation.
  const int safe_capacity = kBinCapacity - kSlackCapacity;
  for (int b = 0; b < kNumBins; ++b) {
    // slack[b] => load[b] <= safe_capacity.
    add_reified_variable_bounds(load[b], kint64min, safe_capacity, slack[b]);
    // not(slack[b]) => load[b] > safe_capacity.
    add_reified_variable_bounds(load[b], safe_capacity + 1, kint64max,
                                NegatedRef(slack[b]));
  }

  // Maximize sum of slacks.
  maximize(slack);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::BinpackingProblem();

  return EXIT_SUCCESS;
}
