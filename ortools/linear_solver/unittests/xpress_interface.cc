#include "ortools/linear_solver/unittests/common.h"
#include "ortools/linear_solver/xpress_interface.cc"

namespace operations_research {

  class XPRSGetter : public InterfaceGetter {
  public:
    using InterfaceGetter::InterfaceGetter;

    int getNumVariables() override {
      int cols;
      XPRSgetintattrib(prob(), XPRS_COLS, &cols);
      return cols;
    }

    double getLb(int n) override {
      EXPECT_LT(n, getNumVariables());
      double lb;
      XPRSgetlb(prob(), &lb, n, n);
      return lb;
    }

    double getUb(int n) override {
      EXPECT_LT(n, getNumVariables());
      double ub;
      XPRSgetub(prob(), &ub, n, n);
      return ub;
    }

  private:
    XPRSprob prob() {
      return (XPRSprob)solver_->underlying_solver();
    }
  };

  TEST(XpressInterface, MakeVar) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    LinearSolverTests tests(&solver, &getter);

    tests.testMakeVar(1, 10);
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
