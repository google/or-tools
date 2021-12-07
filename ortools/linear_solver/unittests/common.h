#include "ortools/linear_solver/linear_solver.h"

#include "gtest/gtest.h"

namespace operations_research {

  class InterfaceGetter {
  public:
    InterfaceGetter(MPSolver* solver) : solver_(solver) {}

    virtual int getNumVariables() = 0;
    virtual double getLb(int n) = 0;
    virtual double getUb(int n) = 0;

  protected:
    MPSolver* solver_;
  };

  class LinearSolverTests {
  public:
    LinearSolverTests(MPSolver* solver, InterfaceGetter* getter)
      : solver_(solver), getter_(getter) {}

    void testMakeVar(double lb, double ub, bool incremental=false, bool clear=true) {
      MPVariable* x = solver_->MakeIntVar(lb, ub, "x");
      if (!incremental) solver_->Solve();
      EXPECT_EQ(getter_->getLb(0), lb);
      EXPECT_EQ(getter_->getUb(0), ub);
      if (clear) solver_->Clear();
    }

  private:
    MPSolver* solver_;
    InterfaceGetter* getter_;
  };

} // namespace operations_research
