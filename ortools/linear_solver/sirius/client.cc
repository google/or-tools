#include "sirius_interface.h"

// This file is intended to demonstrate the integration of 3rd-party interfaces.
// By providing BuildSiriusInterface, we inject this 3rd party interface
// into a new MPSolver object.



int main() {
  using namespace operations_research;

  MPSolver solver("sirius_test",
		  &BuildSiriusInterfaceLP);

  auto x = solver.MakeVar(-10, 10, false, "x");
  auto y = solver.MakeVar(-10, 10, false, "y");

  auto objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 2);
  objective->SetMaximization();

  auto cnt = solver.MakeRowConstraint(-MPSolver::infinity(), 10);
  cnt->SetCoefficient(x, 2);
  cnt->SetCoefficient(y, 1);

  solver.Solve();
}
