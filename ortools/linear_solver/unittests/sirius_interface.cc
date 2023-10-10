#include "ortools/linear_solver/sirius_interface.cc"
#include "gtest/gtest.h"

#include <fstream>

namespace operations_research {

  class SRSGetter {
  public:
    SRSGetter(MPSolver* solver) : solver_(solver) {}

    bool isMip() {
      return prob()->is_mip;
    }

    int getNumVariables() {
      return SRSgetnbcols(prob());
    }

    std::string getVariableName(int n) {
      return prob()->problem_mps->LabelDeLaVariable[n];
    }

    int getNumConstraints() { return SRSgetnbrows(prob()); }

    std::string getConstraintName(int n) {
      return prob()->problem_mps->LabelDeLaContrainte[n];
    }

    double getLb(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mps->Umin[n];
    }

    double getUb(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mps->Umax[n];
    }

    int getVariableType(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mps->TypeDeVariable[n];
    }

    char getConstraintType(int n) {
      EXPECT_LT(n, getNumConstraints());
      return prob()->problem_mps->SensDeLaContrainte[n];
    }

    double getConstraintRhs(int n) {
      EXPECT_LT(n, getNumConstraints());
      return prob()->problem_mps->B[n];
    }

    double getConstraintCoef(int row, int col) {
      EXPECT_LT(col, getNumVariables());
      EXPECT_LT(row, getNumConstraints());
      PROBLEME_MPS* problem_mps = prob()->problem_mps;
      int rowBeg = problem_mps->Mdeb[row];
      for (int i = 0; i < problem_mps->NbTerm[row]; ++i) {
        if (problem_mps->Nuvar[rowBeg + i] == col) {
          return problem_mps->A[rowBeg + i];
        }
      }
      return 0.;
    }

    double getObjectiveCoef(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mps->L[n];
    }

    bool getObjectiveSense() {
      return prob()->maximize;
    }

    int getPresolve() {
      return prob()->presolve;
    }

    int getScaling() {
      return prob()->scaling;
    }

    double getRelativeMipGap() {
      return prob()->relativeGap;
    }

    int getVarBoundType(int n) {
      EXPECT_LT(n, getNumVariables());
      return prob()->problem_mps->TypeDeBorneDeLaVariable[n];
    }

  private:
    MPSolver* solver_;

    SRS_PROBLEM* prob() {
      return (SRS_PROBLEM*)solver_->underlying_solver();
    }
  };

#define UNITTEST_INIT_MIP() \
  MPSolver solver("SIRIUS_MIP", MPSolver::SIRIUS_MIXED_INTEGER_PROGRAMMING);\
  SRSGetter getter(&solver)
#define UNITTEST_INIT_LP() \
  MPSolver solver("SIRIUS_LP", MPSolver::SIRIUS_LINEAR_PROGRAMMING);\
  SRSGetter getter(&solver)

  void _unittest_verify_var(SRSGetter* getter, MPVariable* x, int type, double lb, double ub) {
    EXPECT_EQ(getter->getVariableType(x->index()), type);
    EXPECT_EQ(getter->getLb(x->index()), lb);
    EXPECT_EQ(getter->getUb(x->index()), ub);
  }

  void _unittest_verify_constraint(SRSGetter* getter, MPConstraint* c, char type, double lb, double ub) {
    int idx = c->index();
    EXPECT_EQ(getter->getConstraintType(idx), type);
    switch (type) {
      case SRS_LESSER_THAN:
        EXPECT_EQ(getter->getConstraintRhs(idx), ub);
        break;
      case SRS_GREATER_THAN:
        EXPECT_EQ(getter->getConstraintRhs(idx), lb);
        break;
      case SRS_EQUAL:
        EXPECT_EQ(getter->getConstraintRhs(idx), ub);
        EXPECT_EQ(getter->getConstraintRhs(idx), lb);
        break;
    }
  }

  TEST(SiriusInterface, isMIP) {
    UNITTEST_INIT_MIP();
    EXPECT_EQ(solver.IsMIP(), true);
  }

  TEST(SiriusInterface, isLP) {
    UNITTEST_INIT_LP();
    EXPECT_EQ(solver.IsMIP(), false);
  }

  TEST(SiriusInterface, NumVariables) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x1 = solver.MakeNumVar(-1., 5.1, "x1");
    MPVariable* x2 = solver.MakeNumVar(3.14, 5.1, "x2");
    std::vector<MPVariable*> xs;
    solver.MakeBoolVarArray(500, "xs", &xs);
    solver.Solve();
    EXPECT_EQ(getter.getNumVariables(), 502);
  }

  TEST(SiriusInterface, VariablesName) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    std::string pi("Pi");
    std::string secondVar("Name");
    MPVariable* x1 = solver.MakeNumVar(-1., 5.1, pi);
    MPVariable* x2 = solver.MakeNumVar(3.14, 5.1, secondVar);
    solver.Solve();
    EXPECT_EQ(getter.getVariableName(0), pi);
    EXPECT_EQ(getter.getVariableName(1), secondVar);
  }

  TEST(SiriusInterface, NumConstraints) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(100.0, 100.0);
    solver.MakeRowConstraint(-solver.infinity(), 13.1);
    solver.MakeRowConstraint(12.1, solver.infinity());
    solver.Solve();
    EXPECT_EQ(getter.getNumConstraints(), 3);
  }

  TEST(SiriusInterface, ConstraintsName) {
    UNITTEST_INIT_MIP();

    std::string phi("Phi");
    std::string otherCnt("constraintName");
    solver.MakeRowConstraint(100.0, 100.0, phi);
    solver.MakeRowConstraint(-solver.infinity(), 13.1, otherCnt);
    solver.Solve();
    EXPECT_EQ(getter.getConstraintName(0), phi);
    EXPECT_EQ(getter.getConstraintName(1), otherCnt);
  }
  
  TEST(SiriusInterface, Reset) {
    UNITTEST_INIT_MIP();
    solver.MakeBoolVar("x1");
    solver.MakeBoolVar("x2");
    solver.MakeRowConstraint(-solver.infinity(), 100.0);
    solver.Solve();
    EXPECT_EQ(getter.getNumConstraints(), 1);
    EXPECT_EQ(getter.getNumVariables(), 2);
    solver.Reset();
    EXPECT_EQ(getter.getNumConstraints(), 0);
    EXPECT_EQ(getter.getNumVariables(), 0);
  }

  TEST(SiriusInterface, MakeIntVar) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    int lb = 0, ub = 10;
    MPVariable* x = solver.MakeIntVar(lb, ub, "x");
    solver.Solve();
    _unittest_verify_var(&getter, x, SRS_INTEGER_VAR, lb, ub);
  }

  TEST(SiriusInterface, MakeNumVar) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    double lb = 1.5, ub = 158.2;
    MPVariable* x = solver.MakeNumVar(lb, ub, "x");
    solver.Solve();
    _unittest_verify_var(&getter, x, SRS_CONTINUOUS_VAR, lb, ub);
  }

  TEST(SiriusInterface, MakeBoolVar) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x = solver.MakeBoolVar("x");
    solver.Solve();
    _unittest_verify_var(&getter, x, SRS_INTEGER_VAR, 0, 1);
  }

  TEST(SiriusInterface, MakeIntVarArray) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    int n1 = 25, lb1 = -7, ub1 = 18;
    solver.MakeRowConstraint(-solver.infinity(), 0);
    std::vector<MPVariable*> xs1;
    solver.MakeIntVarArray(n1, lb1, ub1, "xs1", &xs1);
    int n2 = 37, lb2 = 19, ub2 = 189;
    std::vector<MPVariable*> xs2;
    solver.MakeIntVarArray(n2, lb2, ub2, "xs2", &xs2);
    solver.Solve();
    for (int i = 0; i < n1; ++i) {
      _unittest_verify_var(&getter, xs1[i], SRS_INTEGER_VAR, lb1, ub1);
    }
    for (int i = 0; i < n2; ++i) {
      _unittest_verify_var(&getter, xs2[i], SRS_INTEGER_VAR, lb2, ub2);
    }
  }

  TEST(SiriusInterface, MakeNumVarArray) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    int n1 = 1;
    double lb1 = 5.1, ub1 = 8.1;
    std::vector<MPVariable*> xs1;
    solver.MakeNumVarArray(n1, lb1, ub1, "xs1", &xs1);
    int n2 = 13;
    double lb2 = -11.5, ub2 = 189.9;
    std::vector<MPVariable*> xs2;
    solver.MakeNumVarArray(n2, lb2, ub2, "xs2", &xs2);
    solver.Solve();
    for (int i = 0; i < n1; ++i) {
      _unittest_verify_var(&getter, xs1[i], SRS_CONTINUOUS_VAR, lb1, ub1);
    }
    for (int i = 0; i < n2; ++i) {
      _unittest_verify_var(&getter, xs2[i], SRS_CONTINUOUS_VAR, lb2, ub2);
    }
  }

  TEST(SiriusInterface, MakeBoolVarArray) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    double n = 43;
    std::vector<MPVariable*> xs;
    solver.MakeBoolVarArray(n, "xs", &xs);
    solver.Solve();
    for (int i = 0; i < n; ++i) {
      _unittest_verify_var(&getter, xs[i], SRS_INTEGER_VAR, 0, 1);
    }
  }

  TEST(SiriusInterface, SetVariableBounds) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    int lb1 = 3, ub1 = 4;
    MPVariable* x1 = solver.MakeIntVar(lb1, ub1, "x1");
    double lb2 = 3.7, ub2 = 4;
    MPVariable* x2 = solver.MakeNumVar(lb2, ub2, "x2");
    solver.Solve();
    _unittest_verify_var(&getter, x1, SRS_INTEGER_VAR, lb1, ub1);
    _unittest_verify_var(&getter, x2, SRS_CONTINUOUS_VAR, lb2, ub2);
    lb1 = 12, ub1 = 15;
    x1->SetBounds(lb1, ub1);
    lb2 = -1.1, ub2 = 0;
    x2->SetBounds(lb2, ub2);
    solver.Solve();
    _unittest_verify_var(&getter, x1, SRS_INTEGER_VAR, lb1, ub1);
    _unittest_verify_var(&getter, x2, SRS_CONTINUOUS_VAR, lb2, ub2);
  }

   TEST(SiriusInterface, DISABLED_SetVariableInteger) {
    // Here we test a badly definied behaviour
    // depending on the sirius version the sirius-workflow breaks at:
    // either the call of x->SetInteger(false) like the test suggest
    // or at solver.Solve() because integer variables are not supported
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    int lb = -1, ub = 7;
    MPVariable* x = solver.MakeIntVar(lb, ub, "x");
    solver.Solve();
    _unittest_verify_var(&getter, x, SRS_INTEGER_VAR, lb, ub);
    EXPECT_THROW(x->SetInteger(false), std::logic_error);
  }

  TEST(SiriusInterface, ConstraintL) {
    UNITTEST_INIT_MIP();
    double lb = -solver.infinity(), ub = 10.;
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_LESSER_THAN, lb, ub);
  }

  TEST(SiriusInterface, ConstraintR) {
    UNITTEST_INIT_MIP();
    double lb = -2, ub = -1;
    solver.MakeRowConstraint(lb, ub);
    EXPECT_THROW(solver.Solve(), std::logic_error);
  }

  TEST(SiriusInterface, ConstraintG) {
    UNITTEST_INIT_MIP();
    double lb = 8.1, ub = solver.infinity();
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_GREATER_THAN, lb, ub);
  }

  TEST(SiriusInterface, ConstraintE) {
    UNITTEST_INIT_MIP();
    double lb = 18947.3, ub = lb;
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_EQUAL, lb, ub);
  }

  TEST(SiriusInterface, SetConstraintBoundsL) {
    UNITTEST_INIT_MIP();
    double lb = 18947.3, ub = lb;
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_EQUAL, lb, ub);
    lb = -solver.infinity(), ub = 16.6;
    c->SetBounds(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_LESSER_THAN, lb, ub);
  }

  TEST(SiriusInterface, SetConstraintBoundsG) {
    UNITTEST_INIT_MIP();
    double lb = 18947.3, ub = lb;
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_EQUAL, lb, ub);
    lb = 5, ub = solver.infinity();
    c->SetBounds(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_GREATER_THAN, lb, ub);
  }

  TEST(SiriusInterface, SetConstraintBoundsE) {
    UNITTEST_INIT_MIP();
    double lb = -1, ub = solver.infinity();
    MPConstraint* c = solver.MakeRowConstraint(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_GREATER_THAN, lb, ub);
    lb = 128, ub = lb;
    c->SetBounds(lb, ub);
    solver.Solve();
    _unittest_verify_constraint(&getter, c, SRS_EQUAL, lb, ub);
  }

  TEST(SiriusInterface, DISABLED_ConstraintCoef) {
    UNITTEST_INIT_MIP();
    MPVariable* x1 = solver.MakeBoolVar("x1");
    MPVariable* x2 = solver.MakeBoolVar("x2");
    MPConstraint* c1 = solver.MakeRowConstraint(4.1, solver.infinity());
    MPConstraint* c2 = solver.MakeRowConstraint(-solver.infinity(), 0.1);
    double c11 = -15.6, c12 = 0.4, c21 = -11, c22 = 4.5;
    c1->SetCoefficient(x1, c11);
    c1->SetCoefficient(x2, c12);
    c2->SetCoefficient(x1, c21);
    c2->SetCoefficient(x2, c22);
    solver.Solve();
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x1->index()), c11);
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x2->index()), c12);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x1->index()), c21);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x2->index()), c22);

    // Next part causes sirius to crash ("free(): invalid next size (fast)")
    c11 = 0.11, c12 = 0.12, c21 = 0.21, c22 = 0.22;
    c1->SetCoefficient(x1, c11);
    c1->SetCoefficient(x2, c12);
    c2->SetCoefficient(x1, c21);
    c2->SetCoefficient(x2, c22);
    solver.Solve();
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x1->index()), c11);
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x2->index()), c12);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x1->index()), c21);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x2->index()), c22);
  }

  TEST(SiriusInterface, DISABLED_ClearConstraint) {
    UNITTEST_INIT_MIP();
    MPVariable* x1 = solver.MakeBoolVar("x1");
    MPVariable* x2 = solver.MakeBoolVar("x2");
    MPConstraint* c1 = solver.MakeRowConstraint(4.1, solver.infinity());
    MPConstraint* c2 = solver.MakeRowConstraint(-solver.infinity(), 0.1);
    double c11 = -1533.6, c12 = 3.4, c21 = -11000, c22 = 0.0001;
    c1->SetCoefficient(x1, c11);
    c1->SetCoefficient(x2, c12);
    c2->SetCoefficient(x1, c21);
    c2->SetCoefficient(x2, c22);
    solver.Solve();
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x1->index()), c11);
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x2->index()), c12);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x1->index()), c21);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x2->index()), c22);
    c1->Clear();
    c2->Clear();

    // next part causes sirius to crash ("free(): invalid next size (fast)")
    solver.Solve();
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x1->index()), 0);
    EXPECT_EQ(getter.getConstraintCoef(c1->index(), x2->index()), 0);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x1->index()), 0);
    EXPECT_EQ(getter.getConstraintCoef(c2->index(), x2->index()), 0);
  }

  TEST(SiriusInterface, ObjectiveCoef) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x = solver.MakeBoolVar("x");
    MPObjective* obj = solver.MutableObjective();
    double coef = 3112.4;
    obj->SetCoefficient(x, coef);
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveCoef(x->index()), coef);
    coef = 0.2;
    obj->SetCoefficient(x, coef);
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveCoef(x->index()), coef);
  }

  TEST(SiriusInterface, DISABLED_ObjectiveOffset) {
    // ObjectiveOffset not implemented for sirius_interface
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x = solver.MakeBoolVar("x");
    MPObjective* obj = solver.MutableObjective();
    double offset = 4.3;
    obj->SetOffset(offset);
    solver.Solve();
    // EXPECT_EQ(getter.getObjectiveOffset(), offset);
    offset = 3.6;
    obj->SetOffset(offset);
    solver.Solve();
    // EXPECT_EQ(getter.getObjectiveOffset(), offset);
  }

  TEST(SiriusInterface, ObjectiveOffset) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x = solver.MakeBoolVar("x");
    MPObjective* obj = solver.MutableObjective();
    double offset = 4.3;
    EXPECT_THROW(obj->SetOffset(offset), std::logic_error);
  }

  TEST(SiriusInterface, ClearObjective) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPVariable* x = solver.MakeBoolVar("x");
    MPObjective* obj = solver.MutableObjective();
    double coef = -15.6;
    obj->SetCoefficient(x, coef);
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveCoef(x->index()), coef);
    obj->Clear();
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveCoef(x->index()), 0);
  }

  TEST(SiriusInterface, ObjectiveSense) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPObjective* const objective = solver.MutableObjective();
    objective->SetMinimization();
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveSense(), false);
    objective->SetMaximization();
    solver.Solve();
    EXPECT_EQ(getter.getObjectiveSense(), true);
  }

  TEST(SiriusInterface, interations) {
    UNITTEST_INIT_LP();
    int nc = 100, nv = 100;
    std::vector<MPConstraint*> cs(nc);
    for (int ci = 0; ci < nc; ++ci) {
      cs[ci] = solver.MakeRowConstraint(-solver.infinity(), ci + 1);
    }
    MPObjective* const objective = solver.MutableObjective();
    for (int vi = 0; vi < nv; ++vi) {
      MPVariable* v = solver.MakeNumVar(0, nv, "x" + std::to_string(vi));
      for (int ci = 0; ci < nc; ++ci) {
        cs[ci]->SetCoefficient(v, vi + ci);
      }
      objective->SetCoefficient(v, 1);
    }
    solver.Solve();
    EXPECT_GT(solver.iterations(), 0);
  }

  TEST(SiriusInterface, DISABLED_nodes) {
    // The problem seems to be incorrectly returned as infeasible
    UNITTEST_INIT_MIP();
    int nc = 100, nv = 100;
    std::vector<MPConstraint*> cs(2*nc);
    for (int ci = 0; ci < nc; ++ci) {
      cs[2*ci    ] = solver.MakeRowConstraint(-solver.infinity(), ci + 1);
      cs[2*ci + 1] = solver.MakeRowConstraint(ci, solver.infinity());
    }
    MPObjective* const objective = solver.MutableObjective();
    for (int vi = 0; vi < nv; ++vi) {
      MPVariable* v = solver.MakeIntVar(0, nv, "x" + std::to_string(vi));
      for (int ci = 0; ci < nc; ++ci) {
        cs[2*ci    ]->SetCoefficient(v, vi + ci);
        cs[2*ci + 1]->SetCoefficient(v, vi + ci);
      }
      objective->SetCoefficient(v, 1);
    }
    VLOG(0) << solver.Solve();
    EXPECT_GT(solver.nodes(), 0);
  }

  TEST(SiriusInterface, SolverVersion) {
    UNITTEST_INIT_MIP();
    EXPECT_GE(solver.SolverVersion().size(), 36);
  }

  TEST(SiriusInterface, DISABLED_Write) {
    // SRSwritempsprob has different implementations on the metrix branch
    UNITTEST_INIT_MIP();
    MPVariable* x1 = solver.MakeIntVar(-1.2, 9.3, "x1");
    MPVariable* x2 = solver.MakeNumVar(-1, 5, "x2");
    MPConstraint* c1 = solver.MakeRowConstraint(-solver.infinity(), 1);
    c1->SetCoefficient(x1, 3);
    c1->SetCoefficient(x2, 1.5);
    MPConstraint* c2 = solver.MakeRowConstraint(3, solver.infinity());
    c2->SetCoefficient(x2, -1.1);
    MPObjective* obj = solver.MutableObjective();
    obj->SetMaximization();
    obj->SetCoefficient(x1, 1);
    obj->SetCoefficient(x2, 2);

    std::string tmpName = std::string(std::tmpnam(nullptr)) + ".mps";
    solver.Write(tmpName);

    std::ifstream tmpFile(tmpName);
    std::stringstream tmpBuffer;
    tmpBuffer << tmpFile.rdbuf();
    tmpFile.close();
    std::remove(tmpName.c_str());

    EXPECT_EQ(tmpBuffer.str(), R"(* Number of variables:   2
* Number of constraints: 2
NAME          Pb Solve
ROWS
 N  OBJECTIF
 L  R0000000
 G  R0000001
COLUMNS
    C0000000  OBJECTIF  1.0000000000
    C0000000  R0000000  3.0000000000
    C0000001  OBJECTIF  2.0000000000
    C0000001  R0000000  1.5000000000
    C0000001  R0000001  -1.1000000000
RHS
    RHSVAL    R0000000  1.000000000
    RHSVAL    R0000001  3.000000000
BOUNDS
 LI BNDVALUE  C0000000  -1
 UI BNDVALUE  C0000000  9
 LO BNDVALUE  C0000001  -1.000000000
 UP BNDVALUE  C0000001  5.000000000
ENDATA
)");
  }

  TEST(SiriusInterface, DISABLED_SetPrimalTolerance) {
    // SetPrimalTolerance not implemented for sirius_interface
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    double tol = 1e-4;
    params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, tol);
    solver.Solve(params);
    // EXPECT_EQ(getter.getPrimalTolerance(), tol);
  }


  TEST(SiriusInterface, SetPrimalTolerance) {
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    double tol = 1e-4;
    params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, tol);
    solver.Solve(params);
  }

  TEST(SiriusInterface, DISABLED_SetDualTolerance) {
    // SetDualTolerance not implemented for sirius_interface
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    double tol = 1e-2;
    params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, tol);
    solver.Solve(params);
    // EXPECT_EQ(getter.getDualTolerance(), tol) << "Not available";
  }

  TEST(SiriusInterface, SetDualTolerance) {
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    double tol = 1e-2;
    params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, tol);
    solver.Solve(params);
  }

  TEST(SiriusInterface, SetPresolveMode) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::PRESOLVE, MPSolverParameters::PRESOLVE_OFF);
    solver.Solve(params);
    EXPECT_EQ(getter.getPresolve(), 0);
    params.SetIntegerParam(MPSolverParameters::PRESOLVE, MPSolverParameters::PRESOLVE_ON);
    solver.Solve(params);
    EXPECT_EQ(getter.getPresolve(), 1);
  }

  TEST(SiriusInterface, DISABLED_SetLpAlgorithm) {
    // SetLpAlgorithm not implemented for sirius_interface
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::DUAL);
    solver.Solve(params);
    // EXPECT_EQ(getter.getLpAlgorithm(), 2);
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::PRIMAL);
    solver.Solve(params);
    // EXPECT_EQ(getter.getLpAlgorithm(), 3);
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::BARRIER);
    solver.Solve(params);
    // EXPECT_EQ(getter.getLpAlgorithm(), 4);
  }

  TEST(SiriusInterface, SetLpAlgorithm) {
    UNITTEST_INIT_LP();
    MPConstraint* c =  solver.MakeRowConstraint(-solver.infinity(), 0.5);
    MPVariable* x = solver.MakeNumVar(0, 1, "x");
    MPObjective* obj = solver.MutableObjective();
    c->SetCoefficient(x, 1);
    obj->SetCoefficient(x, 1);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::DUAL);
    solver.Solve(params);
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::PRIMAL);
    solver.Solve(params);
    params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, MPSolverParameters::BARRIER);
    solver.Solve(params);
  }

  TEST(SiriusInterface, DISABLED_SetScaling) {
    // SetScaling not implemented for sirius_interface
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::SCALING, MPSolverParameters::SCALING_OFF);
    solver.Solve(params);
    EXPECT_EQ(getter.getScaling(), 0);
    params.SetIntegerParam(MPSolverParameters::SCALING, MPSolverParameters::SCALING_ON);
    solver.Solve(params);
    EXPECT_EQ(getter.getScaling(), 1);
  }

  TEST(SiriusInterface, SetScaling) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::SCALING, MPSolverParameters::SCALING_OFF);
    solver.Solve(params);
    params.SetIntegerParam(MPSolverParameters::SCALING, MPSolverParameters::SCALING_ON);
    solver.Solve(params);
  }

  TEST(SiriusInterface, DISABLED_SetRelativeMipGap) {
    // SetRelativeMipGap not implemented for sirius_interface
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPSolverParameters params;
    double relativeMipGap = 1e-3;
    params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, relativeMipGap);
    solver.Solve(params);
    EXPECT_EQ(getter.getRelativeMipGap(), relativeMipGap);
  }

  TEST(SiriusInterface, SetRelativeMipGap) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);

    MPSolverParameters params;
    double relativeMipGap = 1e-3;
    params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, relativeMipGap);
    solver.Solve(params);
  }

  TEST(SiriusInterface, setVarBoundType) {
    UNITTEST_INIT_MIP();
    solver.MakeRowConstraint(-solver.infinity(), 0);
    double infty = solver.infinity();
    solver.MakeIntVar(2, 2, "VARIABLE_FIXE");
    solver.MakeIntVar(-10, -1, "VARIABLE_BORNEE_DES_DEUX_COTES");
    solver.MakeIntVar(3, infty, "VARIABLE_BORNEE_INFERIEUREMENT");
    solver.MakeIntVar(-infty, -1, "VARIABLE_BORNEE_SUPERIEUREMENT");
    solver.MakeIntVar(-infty, infty, "VARIABLE_NON_BORNEE");

    std::array<int, 5> varBoundTypes = {
      VARIABLE_FIXE,
      VARIABLE_BORNEE_DES_DEUX_COTES,
      VARIABLE_BORNEE_INFERIEUREMENT,
      VARIABLE_BORNEE_SUPERIEUREMENT,
      VARIABLE_NON_BORNEE
    };
    std::string xpressParamString = "VAR_BOUNDS_TYPE";
    for (int boundType : varBoundTypes)
      xpressParamString += " " + std::to_string(boundType);

    solver.SetSolverSpecificParametersAsString(xpressParamString);
    solver.Solve();

    for (int i = 0; i < varBoundTypes.size(); ++i)
      EXPECT_EQ(getter.getVarBoundType(i), varBoundTypes[i]);
  }

  TEST(SiriusInterface, DISABLED_SolveMIP) {
    // The problem seems to incorrectly be returned as infeasible
    UNITTEST_INIT_MIP();

    // max   x + 2y
    // st.  -x +  y <= 1
    //      2x + 3y <= 12
    //      3x + 2y <= 12
    //       x ,  y >= 0
    //       x ,  y \in Z

    double inf = solver.infinity();
    MPVariable* x = solver.MakeIntVar(0, inf, "x");
    MPVariable* y = solver.MakeIntVar(0, inf, "y");
    MPObjective* obj = solver.MutableObjective();
    obj->SetCoefficient(x, 1);
    obj->SetCoefficient(y, 2);
    obj->SetMaximization();
    MPConstraint* c1 = solver.MakeRowConstraint(-inf, 1);
    c1->SetCoefficient(x, -1);
    c1->SetCoefficient(y, 1);
    MPConstraint* c2 = solver.MakeRowConstraint(-inf, 12);
    c2->SetCoefficient(x, 3);
    c2->SetCoefficient(y, 2);
    MPConstraint* c3 = solver.MakeRowConstraint(-inf, 12);
    c3->SetCoefficient(x, 2);
    c3->SetCoefficient(y, 3);

    solver.Solve();
    EXPECT_EQ(obj->Value(), 6);
    EXPECT_EQ(obj->BestBound(), 6);
    EXPECT_EQ(x->solution_value(), 2);
    EXPECT_EQ(y->solution_value(), 2);
  }

  TEST(SiriusInterface, DISABLED_SolveLP) {
    // Sign of dual values seems to be off
    // This sign problem occurs with presolve on and presolve off
    UNITTEST_INIT_LP();

    // max   x + 2y
    // st.  -x +  y <= 1
    //      2x + 3y <= 12
    //      3x + 2y <= 12
    //       x ,  y \in R+

    double inf = solver.infinity();
    MPVariable* x = solver.MakeNumVar(0, inf, "x");
    MPVariable* y = solver.MakeNumVar(0, inf, "y");
    MPObjective* obj = solver.MutableObjective();
    obj->SetCoefficient(x, 1);
    obj->SetCoefficient(y, 2);
    obj->SetMaximization();
    MPConstraint* c1 = solver.MakeRowConstraint(-inf, 1);
    c1->SetCoefficient(x, -1);
    c1->SetCoefficient(y, 1);
    MPConstraint* c2 = solver.MakeRowConstraint(-inf, 12);
    c2->SetCoefficient(x, 3);
    c2->SetCoefficient(y, 2);
    MPConstraint* c3 = solver.MakeRowConstraint(-inf, 12);
    c3->SetCoefficient(x, 2);
    c3->SetCoefficient(y, 3);

    MPSolverParameters params;
    params.SetIntegerParam(MPSolverParameters::PRESOLVE, MPSolverParameters::PRESOLVE_OFF);
    solver.Solve(params);

    EXPECT_NEAR(obj->Value(), 7.4, 1e-8);
    EXPECT_NEAR(x->solution_value(), 1.8, 1e-8);
    EXPECT_NEAR(y->solution_value(), 2.8, 1e-8);
    EXPECT_NEAR(x->reduced_cost(), 0, 1e-8);
    EXPECT_NEAR(y->reduced_cost(), 0, 1e-8);
    EXPECT_NEAR(c1->dual_value(), 0.2, 1e-8);
    EXPECT_NEAR(c2->dual_value(), 0, 1e-8);
    EXPECT_NEAR(c3->dual_value(), 0.6, 1e-8);
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
