#include "examples/tests/linear_programming_unittests.h"
#include "ortools/linear_solver/sirius_interface.cc"

namespace operations_research {

  class SRSGetter : public InterfaceGetter {
  public:
    using InterfaceGetter::InterfaceGetter;

    int getNumVariables() override {
      return prob()->problem_mip->NombreDeVariables;
    }

    double getLb(int n) override {
      assert(n < getNumVariables());
      return prob()->problem_mip->Xmin[n];
    }

    double getUb(int n) override {
      assert(n < getNumVariables());
      return prob()->problem_mip->Xmax[n];
    }

  private:
    SRS_PROBLEM* prob() {
      return (SRS_PROBLEM*)solver_->underlying_solver();
    }
  };

  void runAllTests() {
    MPSolver solver("SIRIUS_MIP", MPSolver::SIRIUS_MIXED_INTEGER_PROGRAMMING);
    SRSGetter getter(&solver);
    LinearProgrammingTests tests(&solver, &getter);

    tests.testMakeVar(1, 10);
    // tests.testMakeVar(0, 1);
    // tests.testMakeVar(-10, 140);
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::runAllTests();
  return 0;
}