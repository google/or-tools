#define CATCH_CONFIG_RUNNER

#include "ortools/linear_solver/xpress_interface.cc"
#include "ortools/linear_solver/unittests/common.h"

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
      REQUIRE(n < getNumVariables());
      double lb;
      XPRSgetlb(prob(), &lb, n, n);
      return lb;
    }

    double getUb(int n) override {
      REQUIRE(n < getNumVariables());
      double ub;
      XPRSgetub(prob(), &ub, n, n);
      return ub;
    }

  private:
    XPRSprob prob() {
      return (XPRSprob)solver_->underlying_solver();
    }
  };

  TEST_CASE("makeVar") {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    LinearSolverTests tests(&solver, &getter);

    tests.testMakeVar(1, 10);
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  int result = Catch::Session().run(argc, argv);
  return result < 0xff ? result : 0xff;
}
