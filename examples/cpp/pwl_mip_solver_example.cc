// Copyright 2010-2018 Google LLC
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

// Integer programming example that shows how to use the API.

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
void RunPwlMipExample(
    PWLSolver::OptimizationSuite optimization_suite) {
  PWLSolver solver("PieceWiseLinearMixedIntegerProgrammingExample", optimization_suite);

  PWLSolver::MatrixOfDoubles x = { { 1.0, 2.0, 4.0 } };
  PWLSolver::VectorOfDoubles y = { 1.0, 1.5, 2.0 };
  PWLSolver::MatrixOfDoubles A = { { 2.0 }, { 1.0 } };
  PWLSolver::MatrixOfDoubles B = { { -1.0, 2.8 }, { 2.8, 1.0 } };
  PWLSolver::VectorOfDoubles b = { 1.0, 2.0 };
  PWLSolver::VectorOfDoubles c = {  0.5  };
  PWLSolver::VectorOfDoubles d =  { 10.8, 13.8 };

  solver.SetXValues(x);
  solver.SetYValues(y);
  solver.SetParameter(b, PWLSolver::VectorParameterType::bVector);
  solver.SetParameter(c, PWLSolver::VectorParameterType::cVector);
  solver.SetParameter(d, PWLSolver::VectorParameterType::dVector);
  solver.SetParameter(A, PWLSolver::MatrixParameterType::AMatrix);
  solver.SetParameter(B, PWLSolver::MatrixParameterType::BMatrix);


  LOG(INFO) << "Number of points = " << solver.numb_of_x_points();
  LOG(INFO) << "Dimension of a point = " << solver.dim_of_x_point();
  LOG(INFO) << "Number of variables = " << solver.numb_of_vars();
  LOG(INFO) << "Number of constraints = " << solver.numb_of_constr();
  LOG(INFO) << "Number of continuous variables = " << solver.numb_of_real_vars();

  const MPSolver::ResultStatus result_status = solver.Solve();
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution!";
  }
  
  auto & vars = solver.variables();
  
  auto lambda_1 = vars[0];
  auto lambda_2 = vars[1];
  auto lambda_3 = vars[2];
  
  auto z_1 = vars[3];
  auto z_2 = vars[4];
  

  LOG(INFO) << "Solution:";

  LOG(INFO) << "lambda_1 = " << lambda_1->solution_value();
  LOG(INFO) << "lambda_2 = " << lambda_2->solution_value();
  LOG(INFO) << "lambda_3 = " << lambda_3->solution_value();

  LOG(INFO) << "z_1 = " << z_1->solution_value();
  LOG(INFO) << "z_2 = " << z_2->solution_value();

  LOG(INFO) << "Optimal objective value = " << solver.objective()->Value();
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";
  LOG(INFO) << "Problem solved in " << solver.nodes()
            << " branch-and-bound nodes";
}

void RunAllExamples() {
#if defined(USE_CBC)
  LOG(INFO) << "---- Integer programming example with CBC ----";
  RunPwlMipExample(PWLSolver::CBC);
#endif
#if defined(USE_GLPK)
  LOG(INFO) << "---- Integer programming example with GLPK ----";
  RunPwlMipExample(PWLSolver::GLPK);
#endif
#if defined(USE_SCIP)
  LOG(INFO) << "---- Integer programming example with SCIP ----";
  RunPwlMipExample(PWLSolver::SCIP);
#endif
#if defined(USE_GUROBI)
  LOG(INFO) << "---- Integer programming example with Gurobi ----";
  RunPwlMipExample(PWLSolver::GUROBI);
#endif  // USE_GUROBI
#if defined(USE_CPLEX)
  LOG(INFO) << "---- Integer programming example with CPLEX ----";
  RunPwlMipExample(PWLSolver::CPLEX);
#endif  // USE_CPLEX
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  //operations_research::RunAllExamples();
  RunPwlMipExample(operations_research::PWLSolver::GUROBI);
  return EXIT_SUCCESS;
}
