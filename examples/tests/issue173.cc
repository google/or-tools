#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
void SolveLP() {
  MPSolver solver("test", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  const double kInfinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(-kInfinity, kInfinity, "x");

  MPObjective* const objective = solver.MutableObjective();
  objective->SetMaximization();
  objective->SetCoefficient(x, 1);

  MPConstraint* const constraint = solver.MakeRowConstraint(0, 5);
  constraint->SetCoefficient(x, 1);

  solver.Solve();
}

void BreakLoop() {
  for (int i = 0; i < 500; i++) {
    SolveLP();
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  operations_research::BreakLoop();
  return 0;
}
