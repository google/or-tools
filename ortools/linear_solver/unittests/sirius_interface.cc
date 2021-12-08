#include "ortools/linear_solver/sirius_interface.cc"
#include "gtest/gtest.h"

namespace operations_research {

  class SRSGetter {
  public:
    SRSGetter(MPSolver* solver) : solver_(solver) {}

    int getNumVariables() {
      return prob()->problem_mip->NombreDeVariables;
    }

    double getLb(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mip->Xmin[n];
    }

    double getUb(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mip->Xmax[n];
    }

  private:
    MPSolver* solver_;

    SRS_PROBLEM* prob() {
      return (SRS_PROBLEM*)solver_->underlying_solver();
    }
  };

  TEST(SiriusInterface, MakeVar) {
    MPSolver solver("SIRIUS_MIP", MPSolver::SIRIUS_MIXED_INTEGER_PROGRAMMING);
    SRSGetter getter(&solver);

    int lb = 0, ub = 10;
    MPVariable* x = solver.MakeIntVar(lb, ub, "x");
    solver.Solve();
    EXPECT_EQ(getter.getLb(0), lb);
    EXPECT_EQ(getter.getUb(0), ub);
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
