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


#include <string>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
void BuildLinearProgrammingMaxExample(MPSolver::OptimizationProblemType type) {
  const double kObjCoef[] = {10.0, 6.0, 4.0};
  const std::string kVarName[] = {"x1", "x2", "x3"};
  const int numVars = 3;
  const int kNumConstraints = 3;
  const std::string kConstraintName[] = {"c1", "c2", "c3"};
  const double kConstraintCoef1[] = {1.0, 1.0, 1.0};
  const double kConstraintCoef2[] = {10.0, 4.0, 5.0};
  const double kConstraintCoef3[] = {2.0, 2.0, 6.0};
  const double* kConstraintCoef[] = {kConstraintCoef1, kConstraintCoef2,
                                     kConstraintCoef3};
  const double kConstraintUb[] = {100.0, 600.0, 300.0};

  const double infinity = MPSolver::infinity();
  MPModelProto model_proto;
  model_proto.set_name("Max_Example");

  // Create variables and objective function
  for (int j = 0; j < numVars; ++j) {
    MPVariableProto* x = model_proto.add_variable();
    x->set_name(kVarName[j]);  // Could be skipped (optional).
    x->set_lower_bound(0.0);
    x->set_upper_bound(infinity);  // Could be skipped (default value).
    x->set_is_integer(false);      // Could be skipped (default value).
    x->set_objective_coefficient(kObjCoef[j]);
  }
  model_proto.set_maximize(true);

  // Create constraints
  for (int i = 0; i < kNumConstraints; ++i) {
    MPConstraintProto* constraint_proto = model_proto.add_constraint();
    constraint_proto->set_name(kConstraintName[i]);  // Could be skipped.
    constraint_proto->set_lower_bound(-infinity);    // Could be skipped.
    constraint_proto->set_upper_bound(kConstraintUb[i]);
    for (int j = 0; j < numVars; ++j) {
      // These two lines may be skipped when the coefficient is zero.
      constraint_proto->add_var_index(j);
      constraint_proto->add_coefficient(kConstraintCoef[i][j]);
    }
  }

  MPModelRequest model_request;
  *model_request.mutable_model() = model_proto;
  #if defined(USE_GLOP)
  if (type == MPSolver::GLOP_LINEAR_PROGRAMMING) {
    model_request.set_solver_type(MPModelRequest::GLOP_LINEAR_PROGRAMMING);
  }
  #endif  // USE_GLOP
  #if defined(USE_CLP)
  if (type == MPSolver::CLP_LINEAR_PROGRAMMING) {
    model_request.set_solver_type(MPModelRequest::CLP_LINEAR_PROGRAMMING);
  }
  #endif  // USE_CLP

  MPSolutionResponse solution_response;
  MPSolver::SolveWithProto(model_request, &solution_response);

  // The problem has an optimal solution.
  CHECK_EQ(MPSOLVER_OPTIMAL, solution_response.status());

  LOG(INFO) << "objective = " << solution_response.objective_value();
  for (int j = 0; j < numVars; ++j) {
    LOG(INFO) << model_proto.variable(j).name() << " = "
              << solution_response.variable_value(j);
  }
}

void RunAllExamples() {
  #if defined(USE_GLOP)
  LOG(INFO) << "----- Running Max Example with GLOP -----";
  BuildLinearProgrammingMaxExample(MPSolver::GLOP_LINEAR_PROGRAMMING);
  #endif  // USE_GLOP
  #if defined(USE_CLP)
  LOG(INFO) << "----- Running Max Example with Coin LP -----";
  BuildLinearProgrammingMaxExample(MPSolver::CLP_LINEAR_PROGRAMMING);
  #endif  // USE_CLP
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::RunAllExamples();
  return 0;
}
