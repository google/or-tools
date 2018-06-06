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

#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void CodeSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  const int x = new_boolean_variable();
  LOG(INFO) << x;
}

void LiteralSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  const int x = new_boolean_variable();
  const int not_x = NegatedRef(x);
  LOG(INFO) << "x = " << x << ", not(x) = " << not_x;
}

void BoolOrSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  const int x = new_boolean_variable();
  const int y = new_boolean_variable();
  add_bool_or({x, NegatedRef(y)});
}

void ReifiedSample() {
  CpModelProto cp_model;

  auto new_boolean_variable = [&cp_model]() {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return index;
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  auto add_reified_bool_and = [&cp_model](const std::vector<int>& literals,
                                          const int literal) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(literal);
    for (const int lit : literals) {
      ct->mutable_bool_and()->add_literals(lit);
    }
  };

  const int x = new_boolean_variable();
  const int y = new_boolean_variable();
  const int b = new_boolean_variable();

  // First version using a half-reified bool and.
  add_reified_bool_and({x, NegatedRef(y)}, b);

  // Second version using bool or.
  add_bool_or({NegatedRef(b), x});
  add_bool_or({NegatedRef(b), NegatedRef(y)});
}

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

  if (response.status() == CpSolverStatus::MODEL_SAT) {
    // Get the value of x in the solution.
    LOG(INFO) << response.solution(r) << " rabbits, and "
              << response.solution(p) << " pheasants";
  }
}

void BinpackingProblem() {
  // Data.
  const int kBinCapacity = 100;
  const int kSlackCapacity = 20;
  const int kNumBins = 10;

  const std::vector<std::vector<int>> items =
  { {20, 12}, {15, 12}, {30, 8}, {45, 5} };
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

  auto add_reified_variable_bounds = [&cp_model](
      int var, int64 lb, int64 ub, int lit) {
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
    add_reified_variable_bounds(
        load[b], safe_capacity + 1, kint64max, NegatedRef(slack[b]));
  }

  // Maximize sum of slacks.
  maximize(slack);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

void IntervalSample() {
  CpModelProto cp_model;
  const int kHorizon = 100;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&cp_model, &new_variable](int64 v) {
    return new_variable(v, v);
  };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  const int start_var = new_variable(0, kHorizon);
  const int duration_var = new_constant(10);
  const int end_var = new_variable(0, kHorizon);
  const int interval_var = new_interval(start_var, duration_var, end_var);
  LOG(INFO) << "start_var = " << start_var
            << ", duration_var = " << duration_var << ", end_var = " << end_var
            << ", interval_var = " << interval_var;
}

void SimpleSolve() {
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

  const int x = new_variable(0, 3);

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::MODEL_SAT) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "x = " << value_x;
  }
}

void SolveWithTimeLimit() {
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

  const int x = new_variable(0, 3);

  // Solving part.
  Model model;

  // Sets a time limit of 10 seconds.
  SatParameters parameters;
  parameters.set_max_time_in_seconds(10.0);
  model.Add(NewSatParameters(parameters));

  // Solve.
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::MODEL_SAT) {
    // Get the value of x in the solution.
    const int64 value_x = response.solution(x);
    LOG(INFO) << "value_x = " << value_x;
  }
}

void MinimalSatPrintIntermediateSolutions() {
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

  auto maximize = [&cp_model](const std::vector<int>& vars,
                              const std::vector<int64>& coeffs) {
    CpObjectiveProto* const obj = cp_model.mutable_objective();
    for (const int v : vars) {
      obj->add_vars(v);
    }
    for (const int64 c : coeffs) {
      obj->add_coeffs(-c);  // Maximize.
    }
    obj->set_scaling_factor(-1.0);  // Maximize.
  };

  const int kNumVals = 3;
  const int x = new_variable(0, kNumVals - 1);
  const int y = new_variable(0, kNumVals - 1);
  const int z = new_variable(0, kNumVals - 1);

  add_different(x, y);

  maximize({x, y, z}, {1, 2, 3});

  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  objective value = " << r.objective_value();
    LOG(INFO) << "  x = " << r.solution(x);
    LOG(INFO) << "  y = " << r.solution(y);
    LOG(INFO) << "  z = " << r.solution(z);
    num_solutions++;
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

void MinimalSatSearchForAllSolutions() {
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

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  x = " << r.solution(x);
    LOG(INFO) << "  y = " << r.solution(y);
    LOG(INFO) << "  z = " << r.solution(z);
    num_solutions++;
  }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  LOG(INFO) << "--- CodeSample ---";
  operations_research::sat::CodeSample();
  LOG(INFO) << "--- LiteralSample ---";
  operations_research::sat::LiteralSample();
  LOG(INFO) << "--- BoolOrSample ---";
  operations_research::sat::BoolOrSample();
  LOG(INFO) << "--- ReifiedSample ---";
  operations_research::sat::ReifiedSample();
  LOG(INFO) << "--- RabbitsAndPheasants ---";
  operations_research::sat::RabbitsAndPheasants();
  LOG(INFO) << "--- BinpackingProblem ---";
  operations_research::sat::BinpackingProblem();
  LOG(INFO) << "--- IntervalSample ---";
  operations_research::sat::IntervalSample();
  LOG(INFO) << "--- SimpleSolve ---";
  operations_research::sat::SimpleSolve();
  LOG(INFO) << "--- SolveWithTimeLimit ---";
  operations_research::sat::SolveWithTimeLimit();
  LOG(INFO) << "--- MinimalSatPrintIntermediateSolutions ---";
  operations_research::sat::MinimalSatPrintIntermediateSolutions();
  LOG(INFO) << "--- MinimalSatSearchForAllSolutions ---";
  operations_research::sat::MinimalSatSearchForAllSolutions();

  return EXIT_SUCCESS;
}
