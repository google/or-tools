// Copyright 2010-2013 Google
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

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver.pb.h"

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
  const double* kConstraintCoef[] = {kConstraintCoef1,
                                     kConstraintCoef2,
                                     kConstraintCoef3};
  const double kConstraintUb[] = {100.0, 600.0, 300.0};

  const double infinity = MPSolver::infinity();
  MPModelProto model_proto;
  model_proto.set_name("Max_Example");

  // Create variables and objective function
  for (int j = 0; j < numVars; ++j) {
    MPVariableProto* x = model_proto.add_variables();
    x->set_id(kVarName[j]);
    x->set_lb(0.0);
    x->set_ub(infinity);
    x->set_integer(false);
    MPTermProto* obj_term = model_proto.add_objective_terms();
    obj_term->set_variable_id(kVarName[j]);
    obj_term->set_coefficient(kObjCoef[j]);
  }
  model_proto.set_maximize(true);

  // Create constraints
  for (int i = 0; i < kNumConstraints; ++i) {
    MPConstraintProto* constraint_proto = model_proto.add_constraints();
    constraint_proto->set_id(kConstraintName[i]);
    constraint_proto->set_lb(-infinity);
    constraint_proto->set_ub(kConstraintUb[i]);
    for (int j = 0; j < numVars; ++j) {
      MPTermProto* term = constraint_proto->add_terms();
      term->set_variable_id(kVarName[j]);
      term->set_coefficient(kConstraintCoef[i][j]);
    }
  }

  MPModelRequest model_request;
  model_request.mutable_model()->CopyFrom(model_proto);
#if defined(USE_GLPK)
  if (type == MPSolver::GLPK_LINEAR_PROGRAMMING) {
    model_request.set_problem_type(MPModelRequest::GLPK_LINEAR_PROGRAMMING);
  }
#endif  // USE_GLPK
#if defined(USE_CLP)
  if (type == MPSolver::CLP_LINEAR_PROGRAMMING) {
    model_request.set_problem_type(MPModelRequest::CLP_LINEAR_PROGRAMMING);
  }
#endif  // USE_CLP

  MPSolutionResponse solution_response;
  MPSolver::SolveWithProtocolBuffers(model_request, &solution_response);

  // The problem has an optimal solution.
  CHECK_EQ(MPSolutionResponse::OPTIMAL, solution_response.result_status());

  LOG(INFO) << "objective = " <<  solution_response.objective_value();
  const int num_non_zeros = solution_response.solution_values_size();
  for (int j = 0; j < num_non_zeros; ++j) {
    MPSolutionValue solution_value = solution_response.solution_values(j);
    LOG(INFO) << solution_value.variable_id() << " = "
              << solution_value.value();
  }
  if (num_non_zeros != numVars) {
    LOG(INFO) << "All other variables have zero value";
  }
}

void RunAllExamples() {
#if defined(USE_GLPK)
  LOG(INFO) << "----- Running Max Example with GLPK -----";
  BuildLinearProgrammingMaxExample(MPSolver::GLPK_LINEAR_PROGRAMMING);
#endif  // USE_GLPK
#if defined(USE_CLP)
  LOG(INFO) << "----- Running Max Example with Coin LP -----";
  BuildLinearProgrammingMaxExample(MPSolver::CLP_LINEAR_PROGRAMMING);
#endif  // USE_CLP
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::RunAllExamples();
  return 0;
}
