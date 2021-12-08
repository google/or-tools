#include "ortools/linear_solver/xpress_interface.cc"
#include "gtest/gtest.h"

namespace operations_research {

  class XPRSGetter {
  public:
    XPRSGetter(MPSolver* solver) : solver_(solver) {}

    int getNumVariables() {
      int cols;
      XPRSgetintattrib(prob(), XPRS_COLS, &cols);
      return cols;
    }

    double getLb(int n) {
      EXPECT_LT(n, getNumVariables());
      double lb;
      XPRSgetlb(prob(), &lb, n, n);
      return lb;
    }

    double getUb(int n) {
      EXPECT_LT(n, getNumVariables());
      double ub;
      XPRSgetub(prob(), &ub, n, n);
      return ub;
    }

  private:
    MPSolver* solver_;

    XPRSprob prob() {
      return (XPRSprob)solver_->underlying_solver();
    }
  };

  TEST(XpressInterface, MakeVar) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);

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
