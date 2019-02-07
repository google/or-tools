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

void SetProblemParameters(PWLSolver& solver, PWLSolver::MatrixOfDoubles& x, 
        PWLSolver::VectorOfDoubles& y, PWLSolver::MatrixOfDoubles& A, 
        PWLSolver::MatrixOfDoubles& B, PWLSolver::VectorOfDoubles& b,
        PWLSolver::VectorOfDoubles& c, PWLSolver::VectorOfDoubles& d) {

  solver.SetXValues(x);
  solver.SetYValues(y);
  solver.SetParameter(b, PWLSolver::VectorParameterType::bVector);
  solver.SetParameter(c, PWLSolver::VectorParameterType::cVector);
  solver.SetParameter(d, PWLSolver::VectorParameterType::dVector);
  solver.SetParameter(A, PWLSolver::MatrixParameterType::AMatrix);
  solver.SetParameter(B, PWLSolver::MatrixParameterType::BMatrix);

}

void PrintProblemHeader(PWLSolver& solver, const std::string& name, 
        PWLSolver::OptimizationSuite optimization_suite)  {
    const std::string titlePrefix = "Mixed integer programming example with PWL function in ";
    const std::string title = titlePrefix + name;
    auto titleLen = title.length();
    std::string solverType;
    PWLSolver::OptSuiteToString(optimization_suite, solverType);
    std::string dashed_line(titleLen, '-');
    LOG(INFO) << dashed_line;
    LOG(INFO) << title;
    LOG(INFO) << dashed_line;
    LOG(INFO) << "Solver type: " << solverType;
    LOG(INFO) << "Number of points = " << solver.numb_of_x_points();
    LOG(INFO) << "Dimension of a point = " << solver.dim_of_x_point();
    LOG(INFO) << "Total number of variables = " << solver.numb_of_vars();
    LOG(INFO) << "Number of continuous variables = " << solver.numb_of_real_vars();
    LOG(INFO) << "Number of integer variables = " << solver.numb_of_real_vars();
    LOG(INFO) << "Number of constraints = " << solver.numb_of_constr();
}

void SolveAndPrintSolution(PWLSolver& solver) {
    LOG(INFO) << "Total number of integer and continuous variables = " << solver.numb_of_vars();
    LOG(INFO) << "Number of continuous variables = " << solver.numb_of_real_vars();

    const MPSolver::ResultStatus result_status = solver.Solve();
    // Check that the problem has an optimal solution.
    if (result_status != MPSolver::OPTIMAL) {
      LOG(FATAL) << "The problem does not have an optimal solution!";
    }

    if (!solver.VerifySolution(1e-7, true)) {
      LOG(FATAL) << "The solution is infeasible with respect to the projected tolerance " << 1e-7;
    }

    LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";

    auto & vars = solver.variables();
    LOG(INFO) << "Solution values for the integer variables:";
    auto intVars = solver.numb_of_vars() - solver.numb_of_real_vars();
    for (int i=0; i < intVars; ++i) {
       LOG(INFO) << "lambda_" << (i+1) << " = " << vars[i]->solution_value(); 
    }

    LOG(INFO) << "Solution values for the continuous variables:";
    auto contVars = solver.numb_of_real_vars();
    for (int j=0; j < contVars; ++j) {
       LOG(INFO) << "z_" << (j+1) << " = " << vars[j+intVars]->solution_value();
    }

    LOG(INFO) << "Advanced usage:";
    LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";
    LOG(INFO) << "Problem solved in " << solver.nodes() << " branch-and-bound nodes";

}

void RunPWLExampleWithScalarXDomainAndTwoContinuousVars(
    PWLSolver::OptimizationSuite optimization_suite) {
  std::string problemName = "scalar x domain and two continuous variables";
  PWLSolver solver(problemName, optimization_suite);

  PWLSolver::MatrixOfDoubles x = { { 1.0, 2.0, 4.0 } };
  PWLSolver::VectorOfDoubles y = { 1.0, 1.5, 2.0 };
  PWLSolver::MatrixOfDoubles A = { { 2.0 }, { 1.0 } };
  PWLSolver::MatrixOfDoubles B = { { -1.0, 2.8 }, { 2.8, 1.0 } };
  PWLSolver::VectorOfDoubles b = { 1.0, 2.0 };
  PWLSolver::VectorOfDoubles c = {  0.5  };
  PWLSolver::VectorOfDoubles d =  { 10.8, 13.8 };

  SetProblemParameters(solver, x, y, A, B, b, c, d);

  PrintProblemHeader(solver, problemName, optimization_suite);
 
  SolveAndPrintSolution(solver);

}

void RunPWLExampleWithScalarXDomainAndThreeContinuousVars(
        PWLSolver::OptimizationSuite optimization_suite) {

  std::string problemName = "scalar x domain and three continuous variables";
  PWLSolver solver(problemName, optimization_suite);
  PWLSolver::MatrixOfDoubles x = { { 1.0, 2.0, 4.0 } };
  PWLSolver::VectorOfDoubles y = { 1.0, 1.5, 2.0 };
  PWLSolver::MatrixOfDoubles A = { { 1.0 }, { 1.0 }, { 1.0 } };
  PWLSolver::MatrixOfDoubles B = { { 1.0, 1.0, 0.0 }, { 1.0, 0.0, 1.0 }, { 0.0, 1.0, 1.0 } };
  PWLSolver::VectorOfDoubles b = { 1.0, 0.25, 0.25 };
  PWLSolver::VectorOfDoubles c = { 0.5 };
  PWLSolver::VectorOfDoubles d = { 10.0, 10.0, 10.0 };

  SetProblemParameters(solver, x, y, A, B, b, c, d);
  
  PrintProblemHeader(solver, problemName, optimization_suite);
  
  SolveAndPrintSolution(solver);
}

void RunPWLExampleWith2DimXDomainAndTwoContinuousVars(
        PWLSolver::OptimizationSuite optimization_suite) {

    std::string problemName = "two-dimensional x domain and two continuous variables";
    PWLSolver solver(problemName, optimization_suite);
    PWLSolver::MatrixOfDoubles x = { { 1.0, 2.0, 4.0 }, { 1.0, 2.0, 4.0 } };
    PWLSolver::VectorOfDoubles y = { 1.0, 1.5, 2.0 };
    PWLSolver::MatrixOfDoubles A = { { 2.0, 1.0 }, { 1.0, 0.5 } };
    PWLSolver::MatrixOfDoubles B = { { -1.0, 2.8 }, { 2.8, 1.0 } };
    PWLSolver::VectorOfDoubles b = { 1.0, 2.0 };
    PWLSolver::VectorOfDoubles c = { 0.5, 0.25 };
    PWLSolver::VectorOfDoubles d = { 10.8, 13.8 };

    SetProblemParameters(solver, x, y, A, B, b, c, d);

    PrintProblemHeader(solver, problemName, optimization_suite);

    SolveAndPrintSolution(solver);
}


void RunPWLExampleWith2DimXDomainAndThreeContinuousVars(
        PWLSolver::OptimizationSuite optimization_suite)
{
    std::string problemName = "two-dimensional x domain and three continuous variables";
    PWLSolver solver(problemName, optimization_suite);
    PWLSolver::MatrixOfDoubles x = { { 1.0, 2.0, 4.0 }, { 1.0, 2.0, 4.0 } };
    PWLSolver::VectorOfDoubles y = { 1.0, 1.5, 2.0 };
    PWLSolver::MatrixOfDoubles A = { { 1.0, 0.5 }, { 1.0, 0.5 }, { 1.0, 0.5 } };
    PWLSolver::MatrixOfDoubles B = { { 1.0, 1.0, 0.0 }, { 1.0, 0.0, 1.0 }, { 0.0, 1.0, 1.0 } };
    PWLSolver::VectorOfDoubles b = { 1.0, 0.25, 0.25 };
    PWLSolver::VectorOfDoubles c = { 0.5, 0.25 };
    PWLSolver::VectorOfDoubles d = { 10.0, 10.0, 10.0 };

    SetProblemParameters(solver, x, y, A, B, b, c, d);

    PrintProblemHeader(solver, problemName, optimization_suite);
    
    SolveAndPrintSolution(solver);
}


void RunAllExamples() {
#if defined(USE_GUROBI)
  RunPWLExampleWithScalarXDomainAndTwoContinuousVars(PWLSolver::GUROBI);
  RunPWLExampleWithScalarXDomainAndThreeContinuousVars(PWLSolver::GUROBI);
  RunPWLExampleWith2DimXDomainAndTwoContinuousVars(PWLSolver::GUROBI);
  RunPWLExampleWith2DimXDomainAndThreeContinuousVars(PWLSolver::GUROBI);
#endif  // USE_GUROBI

#if 0 // for now only Gurobi is supported -- dimitarpg13 2/25/19 --
#if defined(USE_CBC)
  LOG(INFO) << "---- Integer programming example with CBC ----";
  RunPWLExampleWithScalarXDomainAndTwoContinuousVars(PWLSolver::CBC);
  RunPWLExampleWithScalarXDomainAndThreeContinuousVars(PWLSolver::CBC);
  RunPWLExampleWith2DimXDomainAndTwoContinuousVars(PWLSolver::CBC);
  RunPWLExampleWith2DimXDomainAndThreeContinuousVars(PWLSolver::CBC);
#endif
#if defined(USE_GLPK)
  LOG(INFO) << "---- Integer programming example with GLPK ----";
  RunPWLExampleWithScalarXDomainAndTwoContinuousVars(PWLSolver::GLPK);
  RunPWLExampleWithScalarXDomainAndThreeContinuousVars(PWLSolver::GLPK);
  RunPWLExampleWith2DimXDomainAndTwoContinuousVars(PWLSolver::GLPK);
  RunPWLExampleWith2DimXDomainAndThreeContinuousVars(PWLSolver::GLPK);
#endif
#if defined(USE_SCIP)
  LOG(INFO) << "---- Integer programming example with SCIP ----";
  RunPWLExampleWithScalarXDomainAndTwoContinuousVars(PWLSolver::SCIP);
  RunPWLExampleWithScalarXDomainAndThreeContinuousVars(PWLSolver::SCIP);
  RunPWLExampleWith2DimXDomainAndTwoContinuousVars(PWLSolver::SCIP);
  RunPWLExampleWith2DimXDomainAndThreeContinuousVars(PWLSolver::SCIP);
#endif
#if defined(USE_CPLEX)
  LOG(INFO) << "---- Integer programming example with CPLEX ----";
  RunPWLExampleWithScalarXDomainAndTwoContinuousVars(PWLSolver::CPLEX);
  RunPWLExampleWithScalarXDomainAndThreeContinuousVars(PWLSolver::CPLEX);
  RunPWLExampleWith2DimXDomainAndTwoContinuousVars(PWLSolver::CPLEX);
  RunPWLExampleWith2DimXDomainAndThreeContinuousVars(PWLSolver::CPLEX);
#endif  // USE_CPLEX
#endif
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  operations_research::RunAllExamples();
  return EXIT_SUCCESS;
}
