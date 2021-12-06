#define CATCH_CONFIG_RUNNER

#include "ortools/linear_solver/sirius_interface.cc"
#include "ortools/linear_solver/unittests/common.h"

namespace operations_research {

  class SRSGetter : public InterfaceGetter {
  public:
    using InterfaceGetter::InterfaceGetter;

    int getNumVariables() override {
      return prob()->problem_mip->NombreDeVariables;
    }

    double getLb(int n) override {
      REQUIRE(n < getNumVariables());
      return prob()->problem_mip->Xmin[n];
    }

    double getUb(int n) override {
      REQUIRE(n < getNumVariables());
      return prob()->problem_mip->Xmax[n];
    }

  private:
    SRS_PROBLEM* prob() {
      return (SRS_PROBLEM*)solver_->underlying_solver();
    }
  };

  TEST_CASE("makeVar") {
    MPSolver solver("SIRIUS_MIP", MPSolver::SIRIUS_MIXED_INTEGER_PROGRAMMING);
    SRSGetter getter(&solver);
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
