#include "base/commandlineflags.h"
#include "base/logging.h"
#include "linear_solver/linear_solver.h"

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
  for (int i = 0; i < 50000; i++) {
    SolveLP();
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::BreakLoop();
  return 0;
}
