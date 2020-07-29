#include <iostream>
#include <ortools/linear_solver/linear_solver.h>
#include <ortools/linear_solver/linear_solver.pb.h>

namespace operations_research {
void RunLinearExample(
  MPSolver::OptimizationProblemType optimization_problem_type) {
  MPSolver solver("LinearExample", optimization_problem_type);
  const double infinity = solver.infinity();
  // x and y are non-negative variables.
  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");
  // Objective function: 3x + 4y.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 3);
  objective->SetCoefficient(y, 4);
  objective->SetMaximization();
  // x + 2y <= 14.
  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 14.0);
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 2);

  // 3x - y >= 0.
  MPConstraint* const c1 = solver.MakeRowConstraint(0.0, infinity);
  c1->SetCoefficient(x, 3);
  c1->SetCoefficient(y, -1);

  // x - y <= 2.
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 2.0);
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, -1);
	std::cout << "Number of variables = " << solver.NumVariables() << std::endl;
  std::cout << "Number of constraints = " << solver.NumConstraints() << std::endl;
  solver.Solve();
  // The value of each variable in the solution.
  std::cout << "Solution:" << std::endl
  << "x = " << x->solution_value() << std::endl
  << "y = " << y->solution_value() << std::endl;

  // The objective value of the solution.
	std::cout << "Optimal objective value = " << objective->Value() << std::endl;
}

void RunExample() {
  RunLinearExample(MPSolver::GLOP_LINEAR_PROGRAMMING);
}
}  // namespace operations_research

int main(int argc, char** argv) {
	operations_research::RunExample();
	return 0;
}
