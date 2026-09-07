// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/log_severity.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver_callback.h"
#include "ortools/third_party_solvers/xpress_environment.h"

#define XPRS_NAMELENGTH 1028

namespace operations_research {

#define EXPECT_STATUS(s)                              \
  do {                                                \
    int const status_ = s;                            \
    EXPECT_EQ(0, status_) << "Nonzero return status"; \
  } while (0)

class XPRSGetter {
 public:
  explicit XPRSGetter(MPSolver* solver) : solver_(solver) {}

  int GetNumVariables() {
    int num_vars;
    EXPECT_STATUS(XPRSgetintattrib(Prob(), XPRS_COLS, &num_vars));
    return num_vars;
  }

  int GetNumConstraints() {
    int num_rows;
    EXPECT_STATUS(XPRSgetintattrib(Prob(), XPRS_ROWS, &num_rows));
    return num_rows;
  }

  std::string GetRowName(int n) {
    EXPECT_LT(n, GetNumConstraints());
    return GetName(n, XPRS_NAMES_ROW);
  }

  double GetLb(int n) {
    EXPECT_LT(n, GetNumVariables());
    double lb;
    EXPECT_STATUS(XPRSgetlb(Prob(), &lb, n, n));
    return lb;
  }

  double GetUb(int n) {
    EXPECT_LT(n, GetNumVariables());
    double ub;
    EXPECT_STATUS(XPRSgetub(Prob(), &ub, n, n));
    return ub;
  }

  std::string GetColName(int n) {
    EXPECT_LT(n, GetNumVariables());
    return GetName(n, XPRS_NAMES_COLUMN);
  }

  char GetVariableType(int n) {
    EXPECT_LT(n, GetNumVariables());
    char type;
    EXPECT_STATUS(XPRSgetcoltype(Prob(), &type, n, n));
    return type;
  }

  char GetConstraintType(int n) {
    EXPECT_LT(n, GetNumConstraints());
    char type;
    EXPECT_STATUS(XPRSgetrowtype(Prob(), &type, n, n));
    return type;
  }

  double GetConstraintRhs(int n) {
    EXPECT_LT(n, GetNumConstraints());
    double rhs;
    EXPECT_STATUS(XPRSgetrhs(Prob(), &rhs, n, n));
    return rhs;
  }

  double GetConstraintRange(int n) {
    EXPECT_LT(n, GetNumConstraints());
    double range;
    EXPECT_STATUS(XPRSgetrhsrange(Prob(), &range, n, n));
    return range;
  }

  double GetConstraintCoef(int row, int col) {
    EXPECT_LT(col, GetNumVariables());
    EXPECT_LT(row, GetNumConstraints());
    double coef;
    EXPECT_STATUS(XPRSgetcoef(Prob(), row, col, &coef));
    return coef;
  }

  double GetObjectiveCoef(int n) {
    EXPECT_LT(n, GetNumVariables());
    double obj_coef;
    EXPECT_STATUS(XPRSgetobj(Prob(), &obj_coef, n, n));
    return obj_coef;
  }

  double GetObjectiveOffset() {
    double offset;
    EXPECT_STATUS(XPRSgetdblattrib(Prob(), XPRS_OBJRHS, &offset));
    return offset;
  }

  double GetObjectiveSense() {
    double sense;
    EXPECT_STATUS(XPRSgetdblattrib(Prob(), XPRS_OBJSENSE, &sense));
    return sense;
  }

  std::string GetStringControl(int control) {
    std::string value(280, '\0');
    int value_size;
    EXPECT_STATUS(XPRSgetstringcontrol(Prob(), control, &value[0], value.size(),
                                       &value_size));
    value.resize(value_size - 1);
    return value;
  }

  double GetDoubleControl(int control) {
    double value;
    EXPECT_STATUS(XPRSgetdblcontrol(Prob(), control, &value));
    return value;
  }

  int GetIntegerControl(int control) {
    int value;
    EXPECT_STATUS(XPRSgetintcontrol(Prob(), control, &value));
    return value;
  }

  int GetInteger64Control(int control) {
    XPRSint64 value;
    EXPECT_STATUS(XPRSgetintcontrol64(Prob(), control, &value));
    return value;
  }

  std::string GetStringAttribute(int attrib) {
    std::string value(280, '\0');
    int value_size;
    EXPECT_STATUS(XPRSgetstringattrib(Prob(), attrib, &value[0], value.size(),
                                      &value_size));
    value.resize(value_size - 1);
    return value;
  }

 private:
  MPSolver* solver_;

  XPRSprob Prob() { return (XPRSprob)solver_->underlying_solver(); }

  std::string GetName(int n, int type) {
    int namelength;
    EXPECT_STATUS(XPRSgetintattrib(Prob(), XPRS_NAMELENGTH, &namelength));

    std::string name;
    name.resize(8 * namelength + 1);
    EXPECT_STATUS(XPRSgetnames(Prob(), type, name.data(), n, n));

    name.erase(std::find_if(name.rbegin(), name.rend(),
                            [](unsigned char ch) {
                              return !std::isspace(ch) && ch != '\0';
                            })
                   .base(),
               name.end());

    return name;
  }
};

// See
// https://github.com/google/googletest/blob/main/docs/primer.md#test-fixtures-using-the-same-data-configuration-for-multiple-tests-same-data-multiple-tests
class XpressFixture : public testing::Test {
 protected:
  XpressFixture(const char* solver_name, MPSolver::OptimizationProblemType type)
      : solver_(solver_name, type), getter_(&solver_) {}
  ~XpressFixture() override = default;
  MPSolver solver_;
  XPRSGetter getter_;
};

class XpressFixtureLP : public XpressFixture {
 public:
  XpressFixtureLP()
      : XpressFixture("XPRESS_LP", MPSolver::XPRESS_LINEAR_PROGRAMMING) {}
};

class XpressFixtureMIP : public XpressFixture {
 public:
  XpressFixtureMIP()
      : XpressFixture("XPRESS_MIP",
                      MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING) {}
};

void VerifyVar(XPRSGetter* getter, MPVariable* x, char type, double lb,
               double ub) {
  EXPECT_EQ(getter->GetVariableType(x->index()), type);
  EXPECT_EQ(getter->GetLb(x->index()), lb);
  EXPECT_EQ(getter->GetUb(x->index()), ub);
}

void VerifyConstraint(XPRSGetter* getter, MPConstraint* c, char type, double lb,
                      double ub) {
  int idx = c->index();
  EXPECT_EQ(getter->GetConstraintType(idx), type);
  switch (type) {
    case 'L':
      EXPECT_EQ(getter->GetConstraintRhs(idx), ub);
      break;
    case 'U':
      EXPECT_EQ(getter->GetConstraintRhs(idx), lb);
      break;
    case 'E':
      EXPECT_EQ(getter->GetConstraintRhs(idx), ub);
      EXPECT_EQ(getter->GetConstraintRhs(idx), lb);
      break;
    case 'R':
      EXPECT_EQ(getter->GetConstraintRhs(idx), ub);
      EXPECT_EQ(getter->GetConstraintRange(idx), ub - lb);
      break;
  }
}

void BuildLargeMip(MPSolver& solver, int num_vars, int max_time) {
  // Build a random but big and complicated MIP with num_vars integer variables
  // And every variable has a coupling constraint with all previous ones
  std::mt19937 bitgen(123);
  MPObjective* obj = solver.MutableObjective();
  obj->SetMaximization();
  for (int i = 0; i < num_vars; ++i) {
    MPVariable* x = solver.MakeIntVar(absl::Uniform(bitgen, -199, 1),
                                      absl::Uniform(bitgen, 0, 200),
                                      "x_" + std::to_string(i));
    obj->SetCoefficient(x, absl::Uniform(bitgen, -100, 100));
    if (i == 0) {
      continue;
    }
    int rand1 = absl::Uniform(bitgen, -1999, 1);
    int rand2 = absl::Uniform(bitgen, 0, 2000);
    int min = std::min(rand1, rand2);
    int max = std::max(rand1, rand2);
    MPConstraint* c = solver.MakeRowConstraint(min, max);
    c->SetCoefficient(x, absl::Uniform(bitgen, -100, 100));
    for (int j = 0; j < i; ++j) {
      c->SetCoefficient(solver.variable(j), absl::Uniform(bitgen, -100, 100));
    }
  }
  solver.SetSolverSpecificParametersAsString("PRESOLVE 0 MAXTIME " +
                                             std::to_string(max_time));
  solver.EnableOutput();
}

void BuildLargeLp(MPSolver& solver, int num_vars) {
  MPObjective* obj = solver.MutableObjective();
  obj->SetMaximization();
  for (int i = 0; i < num_vars; ++i) {
    MPVariable* x = solver.MakeNumVar(-(i * i) % 21, (i * i) % 55,
                                      "x_" + std::to_string(i));
    obj->SetCoefficient(x, (i * i) % 23);
    int min = -50;
    int max = (i * i) % 664 + 55;
    MPConstraint* c = solver.MakeRowConstraint(min, max);
    c->SetCoefficient(x, i % 331);
    for (int j = 0; j < i; ++j) {
      c->SetCoefficient(solver.variable(j), i + j);
    }
  }
  solver.EnableOutput();
}

class MyMPCallback : public MPCallback {
 private:
  MPSolver* solver_;
  int n_solutions_ = 0;
  std::vector<double> last_variable_values_;
  bool should_throw_;

 public:
  MyMPCallback(MPSolver* solver, bool should_throw)
      : MPCallback(false, false),
        solver_(solver),
        should_throw_(should_throw) {}

  ~MyMPCallback() override {}

  void RunCallback(MPCallbackContext* callback_context) override {
    if (should_throw_) {
      throw std::runtime_error("This is a mocked exception in MyMPCallback");
    }
    // XpressMPCallbackContext* context_ =
    // static_cast<XpressMPCallbackContext*>(callback_context);
    ++n_solutions_;
    EXPECT_TRUE(callback_context->CanQueryVariableValues());
    EXPECT_EQ(callback_context->Event(), MPCallbackEvent::kMipSolution);
    last_variable_values_.resize(solver_->NumVariables(), 0.0);
    for (int i = 0; i < solver_->NumVariables(); i++) {
      last_variable_values_[i] =
          callback_context->VariableValue(solver_->variable(i));
    }
  }

  int GetNSolutions() const { return n_solutions_; }
  double GetLastVariableValue(int index) const {
    return last_variable_values_[index];
  }
};

MyMPCallback* BuildLargeMipWithCallback(MPSolver& solver, int num_vars,
                                        int max_time) {
  BuildLargeMip(solver, num_vars, max_time);
  MPCallback* mp_callback = new MyMPCallback(&solver, false);
  solver.SetCallback(nullptr);  // just to test that this does not cause failure
  solver.SetCallback(mp_callback);
  return static_cast<MyMPCallback*>(mp_callback);
}

TEST_F(XpressFixtureMIP, isMIP) { EXPECT_EQ(solver_.IsMIP(), true); }

TEST_F(XpressFixtureLP, isLP) { EXPECT_EQ(solver_.IsMIP(), false); }

TEST_F(XpressFixtureLP, LpStartingBasis) {
  BuildLargeLp(solver_, 1000);
  // First, we record the number of iterations without an initial basis
  solver_.Solve();
  const auto iterInit = solver_.iterations();
  EXPECT_GE(iterInit, 1000);

  // Here, we retrieve the final basis
  std::vector<MPSolver::BasisStatus> varStatus, constrStatus;
  for (auto* var : solver_.variables()) {
    varStatus.push_back(var->basis_status());
  }
  for (auto* constr : solver_.constraints()) {
    constrStatus.push_back(constr->basis_status());
  }

  // Then we slightly modify the problem...
  MPObjective* obj = solver_.MutableObjective();
  obj->SetCoefficient(solver_.variable(1), 100);
  // Here, we provide the final basis of the previous (similar) problem
  solver_.SetStartingLpBasis(varStatus, constrStatus);
  solver_.Solve();
  const auto iterWithBasis = solver_.iterations();
  // ...and check that few iterations have been performed
  EXPECT_LT(iterWithBasis, 10);
}

TEST_F(XpressFixtureLP, LpStartingBasisNoIterationsIfBasisIsProvided) {
  BuildLargeLp(solver_, 1000);
  // First, we record the number of iterations without an initial basis
  solver_.Solve();

  // Then, we retrieve the final basis
  std::vector<MPSolver::BasisStatus> varStatus, constrStatus;
  for (auto* var : solver_.variables()) {
    varStatus.push_back(var->basis_status());
  }
  for (auto* constr : solver_.constraints()) {
    constrStatus.push_back(constr->basis_status());
  }

  MPSolver solver_BasisProvided("XPRESS_LP",
                                MPSolver::XPRESS_LINEAR_PROGRAMMING);
  BuildLargeLp(solver_BasisProvided, 1000);
  solver_BasisProvided.SetStartingLpBasis(varStatus, constrStatus);
  solver_BasisProvided.Solve();
  const auto iterWithBasis = solver_BasisProvided.iterations();
  // ...and finally check that no iteration has been performed
  EXPECT_EQ(iterWithBasis, 0);
}

TEST_F(XpressFixtureMIP, NumVariables) {
  solver_.MakeNumVar(-1., 5.1, "x1");
  solver_.MakeNumVar(3.14, 5.1, "x2");
  std::vector<MPVariable*> xs;
  solver_.MakeBoolVarArray(500, "xs", &xs);
  solver_.Solve();
  EXPECT_EQ(getter_.GetNumVariables(), 502);
}

TEST_F(XpressFixtureMIP, NumConstraints) {
  solver_.MakeRowConstraint(12., 100.0);
  solver_.MakeRowConstraint(13., 13.1);
  solver_.MakeRowConstraint(12.1, 1000.0);
  solver_.Solve();
  EXPECT_EQ(getter_.GetNumConstraints(), 3);
}

TEST_F(XpressFixtureMIP, Reset) {
  solver_.MakeBoolVar("x1");
  solver_.MakeBoolVar("x2");
  solver_.MakeRowConstraint(12., 100.0);
  solver_.MutableObjective()->SetMaximization();
  solver_.Solve();
  EXPECT_EQ(getter_.GetNumConstraints(), 1);
  EXPECT_EQ(getter_.GetNumVariables(), 2);
  auto oldProbUuid = getter_.GetStringAttribute(XPRS_UUID);
  solver_.Reset();
  EXPECT_EQ(getter_.GetStringAttribute(XPRS_UUID), oldProbUuid);
  EXPECT_EQ(getter_.GetNumConstraints(), 0);
  EXPECT_EQ(getter_.GetNumVariables(), 0);
  EXPECT_EQ(getter_.GetObjectiveSense(), XPRS_OBJ_MAXIMIZE);
}

TEST_F(XpressFixtureMIP, MakeIntVar) {
  int lb = 0, ub = 10;
  MPVariable* x = solver_.MakeIntVar(lb, ub, "x");
  solver_.Solve();
  VerifyVar(&getter_, x, 'I', lb, ub);
}

TEST_F(XpressFixtureMIP, MakeNumVar) {
  double lb = 1.5, ub = 158.2;
  MPVariable* x = solver_.MakeNumVar(lb, ub, "x");
  solver_.Solve();
  VerifyVar(&getter_, x, 'C', lb, ub);
}

TEST_F(XpressFixtureMIP, MakeBoolVar) {
  MPVariable* x = solver_.MakeBoolVar("x");
  solver_.Solve();
  VerifyVar(&getter_, x, 'B', 0, 1);
}

TEST_F(XpressFixtureMIP, MakeIntVarArray) {
  int n1 = 25, lb1 = -7, ub1 = 18;
  std::vector<MPVariable*> xs1;
  solver_.MakeIntVarArray(n1, lb1, ub1, "xs1", &xs1);
  int n2 = 37, lb2 = 19, ub2 = 189;
  std::vector<MPVariable*> xs2;
  solver_.MakeIntVarArray(n2, lb2, ub2, "xs2", &xs2);
  solver_.Solve();
  for (int i = 0; i < n1; ++i) {
    VerifyVar(&getter_, xs1[i], 'I', lb1, ub1);
  }
  for (int i = 0; i < n2; ++i) {
    VerifyVar(&getter_, xs2[i], 'I', lb2, ub2);
  }
}

TEST_F(XpressFixtureMIP, MakeNumVarArray) {
  int n1 = 1;
  double lb1 = 5.1, ub1 = 8.1;
  std::vector<MPVariable*> xs1;
  solver_.MakeNumVarArray(n1, lb1, ub1, "xs1", &xs1);
  int n2 = 13;
  double lb2 = -11.5, ub2 = 189.9;
  std::vector<MPVariable*> xs2;
  solver_.MakeNumVarArray(n2, lb2, ub2, "xs2", &xs2);
  solver_.Solve();
  for (int i = 0; i < n1; ++i) {
    VerifyVar(&getter_, xs1[i], 'C', lb1, ub1);
  }
  for (int i = 0; i < n2; ++i) {
    VerifyVar(&getter_, xs2[i], 'C', lb2, ub2);
  }
}

TEST_F(XpressFixtureMIP, MakeBoolVarArray) {
  int n = 43;
  std::vector<MPVariable*> xs;
  solver_.MakeBoolVarArray(n, "xs", &xs);
  solver_.Solve();
  for (int i = 0; i < n; ++i) {
    VerifyVar(&getter_, xs[i], 'B', 0, 1);
  }
}

TEST_F(XpressFixtureMIP, SetVariableBounds) {
  int lb1 = 3, ub1 = 4;
  MPVariable* x1 = solver_.MakeIntVar(lb1, ub1, "x1");
  double lb2 = 3.7, ub2 = 4;
  MPVariable* x2 = solver_.MakeNumVar(lb2, ub2, "x2");
  solver_.Solve();
  VerifyVar(&getter_, x1, 'I', lb1, ub1);
  VerifyVar(&getter_, x2, 'C', lb2, ub2);
  lb1 = 12, ub1 = 15;
  x1->SetBounds(lb1, ub1);
  lb2 = -1.1, ub2 = 0;
  x2->SetBounds(lb2, ub2);
  solver_.Solve();
  VerifyVar(&getter_, x1, 'I', lb1, ub1);
  VerifyVar(&getter_, x2, 'C', lb2, ub2);
}

TEST_F(XpressFixtureMIP, SetVariableInteger) {
  int lb = -1, ub = 7;
  MPVariable* x = solver_.MakeIntVar(lb, ub, "x");
  solver_.Solve();
  VerifyVar(&getter_, x, 'I', lb, ub);
  x->SetInteger(false);
  solver_.Solve();
  VerifyVar(&getter_, x, 'C', lb, ub);
}

TEST_F(XpressFixtureMIP, ConstraintL) {
  double lb = -solver_.infinity(), ub = 10.;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'L', lb, ub);
}

TEST_F(XpressFixtureMIP, ConstraintR) {
  double lb = -2, ub = -1;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'R', lb, ub);
}

TEST_F(XpressFixtureMIP, ConstraintG) {
  double lb = 8.1, ub = solver_.infinity();
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'G', lb, ub);
}

TEST_F(XpressFixtureMIP, ConstraintE) {
  double lb = 18947.3, ub = lb;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'E', lb, ub);
}

TEST_F(XpressFixtureMIP, SetConstraintBoundsL) {
  double lb = 18947.3, ub = lb;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'E', lb, ub);
  lb = -solver_.infinity(), ub = 16.6;
  c->SetBounds(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'L', lb, ub);
}

TEST_F(XpressFixtureMIP, SetConstraintBoundsR) {
  double lb = -solver_.infinity(), ub = 15;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'L', lb, ub);
  lb = 0, ub = 0.1;
  c->SetBounds(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'R', lb, ub);
}

TEST_F(XpressFixtureMIP, SetConstraintBoundsG) {
  double lb = 1, ub = 2;
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'R', lb, ub);
  lb = 5, ub = solver_.infinity();
  c->SetBounds(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'G', lb, ub);
}

TEST_F(XpressFixtureMIP, SetConstraintBoundsE) {
  double lb = -1, ub = solver_.infinity();
  MPConstraint* c = solver_.MakeRowConstraint(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'G', lb, ub);
  lb = 128, ub = lb;
  c->SetBounds(lb, ub);
  solver_.Solve();
  VerifyConstraint(&getter_, c, 'E', lb, ub);
}

TEST_F(XpressFixtureMIP, ConstraintCoef) {
  MPVariable* x1 = solver_.MakeBoolVar("x1");
  MPVariable* x2 = solver_.MakeBoolVar("x2");
  MPConstraint* c1 = solver_.MakeRowConstraint(4.1, solver_.infinity());
  MPConstraint* c2 = solver_.MakeRowConstraint(-solver_.infinity(), 0.1);
  double c11 = -15.6, c12 = 0.4, c21 = -11, c22 = 4.5;
  c1->SetCoefficient(x1, c11);
  c1->SetCoefficient(x2, c12);
  c2->SetCoefficient(x1, c21);
  c2->SetCoefficient(x2, c22);
  solver_.Solve();
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x1->index()), c11);
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x2->index()), c12);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x1->index()), c21);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x2->index()), c22);
  c11 = 0.11, c12 = 0.12, c21 = 0.21, c22 = 0.22;
  c1->SetCoefficient(x1, c11);
  c1->SetCoefficient(x2, c12);
  c2->SetCoefficient(x1, c21);
  c2->SetCoefficient(x2, c22);
  solver_.Solve();
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x1->index()), c11);
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x2->index()), c12);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x1->index()), c21);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x2->index()), c22);
}

TEST_F(XpressFixtureMIP, ClearConstraint) {
  MPVariable* x1 = solver_.MakeBoolVar("x1");
  MPVariable* x2 = solver_.MakeBoolVar("x2");
  MPConstraint* c1 = solver_.MakeRowConstraint(4.1, solver_.infinity());
  MPConstraint* c2 = solver_.MakeRowConstraint(-solver_.infinity(), 0.1);
  double c11 = -1533.6, c12 = 3.4, c21 = -11000, c22 = 0.0001;
  c1->SetCoefficient(x1, c11);
  c1->SetCoefficient(x2, c12);
  c2->SetCoefficient(x1, c21);
  c2->SetCoefficient(x2, c22);
  solver_.Solve();
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x1->index()), c11);
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x2->index()), c12);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x1->index()), c21);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x2->index()), c22);
  c1->Clear();
  c2->Clear();
  solver_.Solve();
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x1->index()), 0);
  EXPECT_EQ(getter_.GetConstraintCoef(c1->index(), x2->index()), 0);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x1->index()), 0);
  EXPECT_EQ(getter_.GetConstraintCoef(c2->index(), x2->index()), 0);
}

TEST_F(XpressFixtureMIP, ObjectiveCoef) {
  MPVariable* x = solver_.MakeBoolVar("x");
  MPObjective* obj = solver_.MutableObjective();
  double coef = 3112.4;
  obj->SetCoefficient(x, coef);
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveCoef(x->index()), coef);
  coef = 0.2;
  obj->SetCoefficient(x, coef);
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveCoef(x->index()), coef);
}

TEST_F(XpressFixtureMIP, ObjectiveOffset) {
  solver_.MakeBoolVar("x");
  MPObjective* obj = solver_.MutableObjective();
  double offset = 4.3;
  obj->SetOffset(offset);
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveOffset(), offset);
  offset = 3.6;
  obj->SetOffset(offset);
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveOffset(), offset);
}

TEST_F(XpressFixtureMIP, ClearObjective) {
  MPVariable* x = solver_.MakeBoolVar("x");
  MPObjective* obj = solver_.MutableObjective();
  double coef = -15.6;
  obj->SetCoefficient(x, coef);
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveCoef(x->index()), coef);
  obj->Clear();
  solver_.Solve();
  EXPECT_EQ(getter_.GetObjectiveCoef(x->index()), 0);
}

TEST_F(XpressFixtureMIP, ObjectiveSense) {
  MPObjective* const objective = solver_.MutableObjective();
  objective->SetMinimization();
  EXPECT_EQ(getter_.GetObjectiveSense(), XPRS_OBJ_MINIMIZE);
  objective->SetMaximization();
  EXPECT_EQ(getter_.GetObjectiveSense(), XPRS_OBJ_MAXIMIZE);
}

TEST_F(XpressFixtureLP, interactions) {
  int nc = 100, nv = 100;
  std::vector<MPConstraint*> cs(nc);
  for (int ci = 0; ci < nc; ++ci) {
    cs[ci] = solver_.MakeRowConstraint(ci, ci + 1);
  }
  MPObjective* const objective = solver_.MutableObjective();
  for (int vi = 0; vi < nv; ++vi) {
    MPVariable* v = solver_.MakeNumVar(0, nv, "x" + std::to_string(vi));
    for (int ci = 0; ci < nc; ++ci) {
      cs[ci]->SetCoefficient(v, vi + ci);
    }
    objective->SetCoefficient(v, 1);
  }
  solver_.Solve();
  EXPECT_GT(solver_.iterations(), 0);
}

TEST_F(XpressFixtureMIP, nodes) {
  int nc = 100, nv = 100;
  std::vector<MPConstraint*> cs(nc);
  for (int ci = 0; ci < nc; ++ci) {
    cs[ci] = solver_.MakeRowConstraint(ci, ci + 1);
  }
  MPObjective* const objective = solver_.MutableObjective();
  for (int vi = 0; vi < nv; ++vi) {
    MPVariable* v = solver_.MakeIntVar(0, nv, "x" + std::to_string(vi));
    for (int ci = 0; ci < nc; ++ci) {
      cs[ci]->SetCoefficient(v, vi + ci);
    }
    objective->SetCoefficient(v, 1);
  }
  solver_.Solve();
  EXPECT_GT(solver_.nodes(), 0);
}

TEST_F(XpressFixtureMIP, SolverVersion) {
  EXPECT_GE(solver_.SolverVersion().size(), 30);
}

TEST_F(XpressFixtureMIP, Write) {
  MPVariable* x1 = solver_.MakeIntVar(-1.2, 9.3, "C1");
  MPVariable* x2 = solver_.MakeNumVar(-1, 5.147593849384714, "SomeColumnName");
  MPConstraint* c1 = solver_.MakeRowConstraint(-solver_.infinity(), 1, "R1");
  c1->SetCoefficient(x1, 3);
  c1->SetCoefficient(x2, 1.5);
  MPConstraint* c2 = solver_.MakeRowConstraint(3, 5, "SomeRowName");
  c2->SetCoefficient(x2, -1.1122334455667788);
  MPObjective* obj = solver_.MutableObjective();
  obj->SetMaximization();
  obj->SetCoefficient(x1, 1);
  obj->SetCoefficient(x2, 2);

  const std::string tmpName = absl::StrCat(testing::TempDir(), "/dummy.mps");
  solver_.Write(tmpName);

  std::ifstream tmpFile(tmpName);
  std::stringstream tmpBuffer;
  tmpBuffer << tmpFile.rdbuf();
  tmpFile.close();
  std::remove(tmpName.c_str());

  std::string expectedMps = R"(NAME          
OBJSENSE  MAXIMIZE
ROWS
 N  __OBJ___        
 L  R1              
 L  SomeRowName     
COLUMNS
    C1                __OBJ___          1
    C1                R1                3
    SomeColumnName    __OBJ___          2
    SomeColumnName    R1                1.5
    SomeColumnName    SomeRowName       -1.1122334455667788
RHS
    RHS00001          R1                1
    RHS00001          SomeRowName       5
RANGES
    RNG00001          SomeRowName       2
BOUNDS
 UI BND00001          C1                9
 LO BND00001          C1                -1
 UP BND00001          SomeColumnName    5.147593849384714
 LO BND00001          SomeColumnName    -1
ENDATA
)";
  EXPECT_EQ(tmpBuffer.str(), expectedMps);
}

TEST_F(XpressFixtureLP, SetPrimalTolerance) {
  MPSolverParameters params;
  double tol = 1e-4;
  params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, tol);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_FEASTOL), tol);
}

TEST_F(XpressFixtureLP, SetPrimalToleranceNotOverriddenByMPSolverParameters) {
  double tol = 1e-4;  // Choose a value different from kDefaultPrimalTolerance
  std::string xpressParamString = "FEASTOL " + std::to_string(tol);
  solver_.SetSolverSpecificParametersAsString(xpressParamString);
  solver_.Solve();
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_FEASTOL), tol);
}

TEST_F(XpressFixtureLP, SetDualTolerance) {
  MPSolverParameters params;
  double tol = 1e-2;
  params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, tol);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_OPTIMALITYTOL), tol);
}

TEST_F(XpressFixtureLP, SetDualToleranceNotOverriddenByMPSolverParameters) {
  double tol = 1e-4;  // Choose a value different from kDefaultDualTolerance
  std::string xpressParamString = "OPTIMALITYTOL " + std::to_string(tol);
  solver_.SetSolverSpecificParametersAsString(xpressParamString);
  solver_.Solve();
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_OPTIMALITYTOL), tol);
}

TEST_F(XpressFixtureMIP, SetPresolveMode) {
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_OFF);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_PRESOLVE), 0);
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_ON);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_PRESOLVE), 1);
}

TEST_F(XpressFixtureMIP, SetPresolveModeNotOverriddenByMPSolverParameters) {
  // Test all presolve modes of Xpress
  std::vector<int> presolveModes{-1, 0, 1, 2, 3};
  for (int presolveMode : presolveModes) {
    std::string xpressParamString = "PRESOLVE " + std::to_string(presolveMode);
    solver_.SetSolverSpecificParametersAsString(xpressParamString);
    solver_.Solve();
    EXPECT_EQ(getter_.GetIntegerControl(XPRS_PRESOLVE), presolveMode);
  }
}

TEST_F(XpressFixtureLP, SetLpAlgorithm) {
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::DUAL);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_DEFAULTALG), 2);
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::PRIMAL);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_DEFAULTALG), 3);
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::BARRIER);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_DEFAULTALG), 4);
}

TEST_F(XpressFixtureLP, SetLPAlgorithmNotOverriddenByMPSolverParameters) {
  std::vector<int> defaultAlgs{1, 2, 3, 4};
  for (int defaultAlg : defaultAlgs) {
    std::string xpressParamString = "DEFAULTALG " + std::to_string(defaultAlg);
    solver_.SetSolverSpecificParametersAsString(xpressParamString);
    solver_.Solve();
    EXPECT_EQ(getter_.GetIntegerControl(XPRS_DEFAULTALG), defaultAlg);
  }
}

TEST_F(XpressFixtureMIP, SetScaling) {
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::SCALING,
                         MPSolverParameters::SCALING_OFF);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_SCALING), 0);
  params.SetIntegerParam(MPSolverParameters::SCALING,
                         MPSolverParameters::SCALING_ON);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_SCALING), 163);
}

TEST_F(XpressFixtureMIP, SetScalingNotOverriddenByMPSolverParameters) {
  // Scaling is a bitmap on 16 bits in Xpress, test only a random value among
  // all possible
  int scaling = 2354;

  std::string xpressParamString = "SCALING " + std::to_string(scaling);
  solver_.SetSolverSpecificParametersAsString(xpressParamString);
  solver_.Solve();
  EXPECT_EQ(getter_.GetIntegerControl(XPRS_SCALING), scaling);
}

TEST_F(XpressFixtureMIP, SetRelativeMipGap) {
  MPSolverParameters params;
  double relativeMipGap = 1e-3;
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, relativeMipGap);
  solver_.Solve(params);
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_MIPRELSTOP), relativeMipGap);
}

TEST_F(XpressFixtureMIP, SetRelativeMipGapNotOverriddenByMPSolverParameters) {
  double gap = 1e-2;  // Choose a value different from kDefaultRelativeMipGap
  std::string xpressParamString = "MIPRELSTOP " + std::to_string(gap);
  solver_.SetSolverSpecificParametersAsString(xpressParamString);
  solver_.Solve();
  EXPECT_EQ(getter_.GetDoubleControl(XPRS_MIPRELSTOP), gap);
}

TEST(XpressInterface, setStringControls) {
  std::vector<std::tuple<std::string, int, std::string>> params = {
      {"MPSRHSNAME", XPRS_MPSRHSNAME, "default_value"},
      {"MPSOBJNAME", XPRS_MPSOBJNAME, "default_value"},
      {"MPSRANGENAME", XPRS_MPSRANGENAME, "default_value"},
      {"MPSBOUNDNAME", XPRS_MPSBOUNDNAME, "default_value"},
      {"OUTPUTMASK", XPRS_OUTPUTMASK, "default_value"},
      {"TUNERMETHODFILE", XPRS_TUNERMETHODFILE, "default_value"},
      {"TUNEROUTPUTPATH", XPRS_TUNEROUTPUTPATH, "default_value"},
      {"TUNERSESSIONNAME", XPRS_TUNERSESSIONNAME, "default_value"},
      {"COMPUTEEXECSERVICE", XPRS_COMPUTEEXECSERVICE, "default_value"},
  };
  for (const auto& [param_string, control, param_value] : params) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    std::string xpress_param_string = param_string + " " + param_value;
    solver.SetSolverSpecificParametersAsString(xpress_param_string);
    EXPECT_EQ(param_value, getter.GetStringControl(control));
  }
}

TEST(XpressInterface, setDoubleControls) {
  std::vector<std::tuple<std::string, int, double>> params = {
      {"MAXCUTTIME", XPRS_MAXCUTTIME, 1.},
      {"MAXSTALLTIME", XPRS_MAXSTALLTIME, 1.},
      {"TUNERMAXTIME", XPRS_TUNERMAXTIME, 1.},
      {"MATRIXTOL", XPRS_MATRIXTOL, 1.},
      {"PIVOTTOL", XPRS_PIVOTTOL, 1.},
      {"FEASTOL", XPRS_FEASTOL, 1.},
      {"OUTPUTTOL", XPRS_OUTPUTTOL, 1.},
      {"SOSREFTOL", XPRS_SOSREFTOL, 1.},
      {"OPTIMALITYTOL", XPRS_OPTIMALITYTOL, 1.},
      {"ETATOL", XPRS_ETATOL, 1.},
      {"RELPIVOTTOL", XPRS_RELPIVOTTOL, 1.},
      {"MIPTOL", XPRS_MIPTOL, 1.},
      {"MIPTOLTARGET", XPRS_MIPTOLTARGET, 1.},
      {"BARPERTURB", XPRS_BARPERTURB, 1.},
      {"MIPADDCUTOFF", XPRS_MIPADDCUTOFF, 1.},
      {"MIPABSCUTOFF", XPRS_MIPABSCUTOFF, 1.},
      {"MIPRELCUTOFF", XPRS_MIPRELCUTOFF, 1.},
      {"PSEUDOCOST", XPRS_PSEUDOCOST, 1.},
      {"PENALTY", XPRS_PENALTY, 1.},
      {"BIGM", XPRS_BIGM, 1.},
      {"MIPABSSTOP", XPRS_MIPABSSTOP, 1.},
      {"MIPRELSTOP", XPRS_MIPRELSTOP, 1.},
      {"CROSSOVERACCURACYTOL", XPRS_CROSSOVERACCURACYTOL, 1.},
      {"PRIMALPERTURB", XPRS_PRIMALPERTURB, 1.},
      {"DUALPERTURB", XPRS_DUALPERTURB, 1.},
      {"BAROBJSCALE", XPRS_BAROBJSCALE, 1.},
      {"BARRHSSCALE", XPRS_BARRHSSCALE, 1.},
      {"CHOLESKYTOL", XPRS_CHOLESKYTOL, 1.},
      {"BARGAPSTOP", XPRS_BARGAPSTOP, 1.},
      {"BARDUALSTOP", XPRS_BARDUALSTOP, 1.},
      {"BARPRIMALSTOP", XPRS_BARPRIMALSTOP, 1.},
      {"BARSTEPSTOP", XPRS_BARSTEPSTOP, 1.},
      {"ELIMTOL", XPRS_ELIMTOL, 1.},
      {"MARKOWITZTOL", XPRS_MARKOWITZTOL, 1.},
      {"MIPABSGAPNOTIFY", XPRS_MIPABSGAPNOTIFY, 1.},
      {"MIPRELGAPNOTIFY", XPRS_MIPRELGAPNOTIFY, 1.},
      {"BARLARGEBOUND", XPRS_BARLARGEBOUND, 1.},
      {"PPFACTOR", XPRS_PPFACTOR, 1.},
      {"REPAIRINDEFINITEQMAX", XPRS_REPAIRINDEFINITEQMAX, 1.},
      {"BARGAPTARGET", XPRS_BARGAPTARGET, 1.},
      {"DUMMYCONTROL", XPRS_DUMMYCONTROL, 1.},
      {"BARSTARTWEIGHT", XPRS_BARSTARTWEIGHT, 1.},
      {"BARFREESCALE", XPRS_BARFREESCALE, 1.},
      {"SBEFFORT", XPRS_SBEFFORT, 1.},
      {"HEURDIVERANDOMIZE", XPRS_HEURDIVERANDOMIZE, 1.},
      {"HEURSEARCHEFFORT", XPRS_HEURSEARCHEFFORT, 1.},
      {"CUTFACTOR", XPRS_CUTFACTOR, 1.},
      {"EIGENVALUETOL", XPRS_EIGENVALUETOL, 1.},
      {"INDLINBIGM", XPRS_INDLINBIGM, 1.},
      {"TREEMEMORYSAVINGTARGET", XPRS_TREEMEMORYSAVINGTARGET, 1.},
      {"INDPRELINBIGM", XPRS_INDPRELINBIGM, 1.},
      {"RELAXTREEMEMORYLIMIT", XPRS_RELAXTREEMEMORYLIMIT, 1.},
      {"MIPABSGAPNOTIFYOBJ", XPRS_MIPABSGAPNOTIFYOBJ, 1.},
      {"MIPABSGAPNOTIFYBOUND", XPRS_MIPABSGAPNOTIFYBOUND, 1.},
      {"PRESOLVEMAXGROW", XPRS_PRESOLVEMAXGROW, 1.},
      {"HEURSEARCHTARGETSIZE", XPRS_HEURSEARCHTARGETSIZE, 1.},
      {"CROSSOVERRELPIVOTTOL", XPRS_CROSSOVERRELPIVOTTOL, 1.},
      {"CROSSOVERRELPIVOTTOLSAFE", XPRS_CROSSOVERRELPIVOTTOLSAFE, 1.},
      {"DETLOGFREQ", XPRS_DETLOGFREQ, 1.},
      {"MAXIMPLIEDBOUND", XPRS_MAXIMPLIEDBOUND, 1.},
      {"FEASTOLTARGET", XPRS_FEASTOLTARGET, 1.},
      {"OPTIMALITYTOLTARGET", XPRS_OPTIMALITYTOLTARGET, 1.},
      {"PRECOMPONENTSEFFORT", XPRS_PRECOMPONENTSEFFORT, 1.},
      {"LPLOGDELAY", XPRS_LPLOGDELAY, 1.},
      {"HEURDIVEITERLIMIT", XPRS_HEURDIVEITERLIMIT, 1.},
      {"BARKERNEL", XPRS_BARKERNEL, 1.},
      {"FEASTOLPERTURB", XPRS_FEASTOLPERTURB, 1.},
      {"CROSSOVERFEASWEIGHT", XPRS_CROSSOVERFEASWEIGHT, 1.},
      {"LUPIVOTTOL", XPRS_LUPIVOTTOL, 1.},
      {"MIPRESTARTGAPTHRESHOLD", XPRS_MIPRESTARTGAPTHRESHOLD, 1.},
      {"NODEPROBINGEFFORT", XPRS_NODEPROBINGEFFORT, 1.},
      {"INPUTTOL", XPRS_INPUTTOL, 1.},
      {"MIPRESTARTFACTOR", XPRS_MIPRESTARTFACTOR, 1.},
      {"BAROBJPERTURB", XPRS_BAROBJPERTURB, 1.},
      {"CPIALPHA", XPRS_CPIALPHA, 1.},
      {"GLOBALBOUNDINGBOX", XPRS_GLOBALBOUNDINGBOX, 1.},
      {"TIMELIMIT", XPRS_TIMELIMIT, 1.},
      {"SOLTIMELIMIT", XPRS_SOLTIMELIMIT, 1.},
      {"REPAIRINFEASTIMELIMIT", XPRS_REPAIRINFEASTIMELIMIT, 1.},
  };
  for (const auto& [param_string, control, param_value] : params) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    std::string xpress_param_string =
        param_string + " " + std::to_string(param_value);
    solver.SetSolverSpecificParametersAsString(xpress_param_string);
    EXPECT_EQ(param_value, getter.GetDoubleControl(control));
  }
}

TEST(XpressInterface, setIntControl) {
  std::vector<std::tuple<std::string, int, int>> params = {
      {"EXTRAROWS", XPRS_EXTRAROWS, 1},
      {"EXTRACOLS", XPRS_EXTRACOLS, 1},
      {"LPITERLIMIT", XPRS_LPITERLIMIT, 1},
      {"LPLOG", XPRS_LPLOG, 1},
      {"SCALING", XPRS_SCALING, 1},
      {"PRESOLVE", XPRS_PRESOLVE, 1},
      {"CRASH", XPRS_CRASH, 1},
      {"PRICINGALG", XPRS_PRICINGALG, 1},
      {"INVERTFREQ", XPRS_INVERTFREQ, 1},
      {"INVERTMIN", XPRS_INVERTMIN, 1},
      {"MAXNODE", XPRS_MAXNODE, 1},
      {"MAXTIME", XPRS_MAXTIME, 1},
      {"MAXMIPSOL", XPRS_MAXMIPSOL, 1},
      {"SIFTPASSES", XPRS_SIFTPASSES, 1},
      {"DEFAULTALG", XPRS_DEFAULTALG, 1},
      {"VARSELECTION", XPRS_VARSELECTION, 1},
      {"NODESELECTION", XPRS_NODESELECTION, 1},
      {"BACKTRACK", XPRS_BACKTRACK, 1},
      {"MIPLOG", XPRS_MIPLOG, 1},
      {"KEEPNROWS", XPRS_KEEPNROWS, 1},
      {"MPSECHO", XPRS_MPSECHO, 1},
      {"MAXPAGELINES", XPRS_MAXPAGELINES, 1},
      {"OUTPUTLOG", XPRS_OUTPUTLOG, 1},
      {"BARSOLUTION", XPRS_BARSOLUTION, 1},
      {"CACHESIZE", XPRS_CACHESIZE, 1},
      {"CROSSOVER", XPRS_CROSSOVER, 1},
      {"BARITERLIMIT", XPRS_BARITERLIMIT, 1},
      {"CHOLESKYALG", XPRS_CHOLESKYALG, 1},
      {"BAROUTPUT", XPRS_BAROUTPUT, 1},
      {"EXTRAMIPENTS", XPRS_EXTRAMIPENTS, 1},
      {"REFACTOR", XPRS_REFACTOR, 1},
      {"BARTHREADS", XPRS_BARTHREADS, 1},
      {"KEEPBASIS", XPRS_KEEPBASIS, 1},
      {"CROSSOVEROPS", XPRS_CROSSOVEROPS, 1},
      {"VERSION", XPRS_VERSION, 1},
      {"CROSSOVERTHREADS", XPRS_CROSSOVERTHREADS, 1},
      {"BIGMMETHOD", XPRS_BIGMMETHOD, 1},
      {"MPSNAMELENGTH", XPRS_MPSNAMELENGTH, 1},
      {"ELIMFILLIN", XPRS_ELIMFILLIN, 1},
      {"PRESOLVEOPS", XPRS_PRESOLVEOPS, 1},
      {"MIPPRESOLVE", XPRS_MIPPRESOLVE, 1},
      {"MIPTHREADS", XPRS_MIPTHREADS, 1},
      {"BARORDER", XPRS_BARORDER, 1},
      {"BREADTHFIRST", XPRS_BREADTHFIRST, 1},
      {"AUTOPERTURB", XPRS_AUTOPERTURB, 1},
      {"DENSECOLLIMIT", XPRS_DENSECOLLIMIT, 1},
      {"CALLBACKFROMMASTERTHREAD", XPRS_CALLBACKFROMMASTERTHREAD, 1},
      {"MAXMCOEFFBUFFERELEMS", XPRS_MAXMCOEFFBUFFERELEMS, 1},
      {"REFINEOPS", XPRS_REFINEOPS, 1},
      {"LPREFINEITERLIMIT", XPRS_LPREFINEITERLIMIT, 1},
      {"MIPREFINEITERLIMIT", XPRS_MIPREFINEITERLIMIT, 1},
      {"DUALIZEOPS", XPRS_DUALIZEOPS, 1},
      {"CROSSOVERITERLIMIT", XPRS_CROSSOVERITERLIMIT, 1},
      {"PREBASISRED", XPRS_PREBASISRED, 1},
      {"PRESORT", XPRS_PRESORT, 1},
      {"PREPERMUTE", XPRS_PREPERMUTE, 1},
      {"PREPERMUTESEED", XPRS_PREPERMUTESEED, 1},
      {"MAXMEMORYSOFT", XPRS_MAXMEMORYSOFT, 1},
      {"CUTFREQ", XPRS_CUTFREQ, 1},
      {"SYMSELECT", XPRS_SYMSELECT, 1},
      {"SYMMETRY", XPRS_SYMMETRY, 1},
      {"MAXMEMORYHARD", XPRS_MAXMEMORYHARD, 1},
      {"MIQCPALG", XPRS_MIQCPALG, 1},
      {"QCCUTS", XPRS_QCCUTS, 1},
      {"QCROOTALG", XPRS_QCROOTALG, 1},
      {"PRECONVERTSEPARABLE", XPRS_PRECONVERTSEPARABLE, 1},
      {"ALGAFTERNETWORK", XPRS_ALGAFTERNETWORK, 1},
      {"TRACE", XPRS_TRACE, 1},
      {"MAXIIS", XPRS_MAXIIS, 1},
      {"CPUTIME", XPRS_CPUTIME, 1},
      {"COVERCUTS", XPRS_COVERCUTS, 1},
      {"GOMCUTS", XPRS_GOMCUTS, 1},
      {"LPFOLDING", XPRS_LPFOLDING, 1},
      {"MPSFORMAT", XPRS_MPSFORMAT, 1},
      {"CUTSTRATEGY", XPRS_CUTSTRATEGY, 1},
      {"CUTDEPTH", XPRS_CUTDEPTH, 1},
      {"TREECOVERCUTS", XPRS_TREECOVERCUTS, 1},
      {"TREEGOMCUTS", XPRS_TREEGOMCUTS, 1},
      {"CUTSELECT", XPRS_CUTSELECT, 1},
      {"TREECUTSELECT", XPRS_TREECUTSELECT, 1},
      {"DUALIZE", XPRS_DUALIZE, 1},
      {"DUALGRADIENT", XPRS_DUALGRADIENT, 1},
      {"SBITERLIMIT", XPRS_SBITERLIMIT, 1},
      {"SBBEST", XPRS_SBBEST, 1},
      {"BARINDEFLIMIT", XPRS_BARINDEFLIMIT, 1},
      {"HEURFREQ", XPRS_HEURFREQ, 1},
      {"HEURDEPTH", XPRS_HEURDEPTH, 1},
      {"HEURMAXSOL", XPRS_HEURMAXSOL, 1},
      {"HEURNODES", XPRS_HEURNODES, 1},
      {"LNPBEST", XPRS_LNPBEST, 1},
      {"LNPITERLIMIT", XPRS_LNPITERLIMIT, 1},
      {"BRANCHCHOICE", XPRS_BRANCHCHOICE, 1},
      {"BARREGULARIZE", XPRS_BARREGULARIZE, 1},
      {"SBSELECT", XPRS_SBSELECT, 1},
      {"LOCALCHOICE", XPRS_LOCALCHOICE, 1},
      {"LOCALBACKTRACK", XPRS_LOCALBACKTRACK, 1},
      {"DUALSTRATEGY", XPRS_DUALSTRATEGY, 1},
      {"L1CACHE", XPRS_L1CACHE, 1},
      {"HEURDIVESTRATEGY", XPRS_HEURDIVESTRATEGY, 1},
      {"HEURSELECT", XPRS_HEURSELECT, 1},
      {"BARSTART", XPRS_BARSTART, 1},
      {"PRESOLVEPASSES", XPRS_PRESOLVEPASSES, 1},
      {"BARNUMSTABILITY", XPRS_BARNUMSTABILITY, 1},
      {"BARORDERTHREADS", XPRS_BARORDERTHREADS, 1},
      {"EXTRASETS", XPRS_EXTRASETS, 1},
      {"FEASIBILITYPUMP", XPRS_FEASIBILITYPUMP, 1},
      {"PRECOEFELIM", XPRS_PRECOEFELIM, 1},
      {"PREDOMCOL", XPRS_PREDOMCOL, 1},
      {"HEURSEARCHFREQ", XPRS_HEURSEARCHFREQ, 1},
      {"HEURDIVESPEEDUP", XPRS_HEURDIVESPEEDUP, 1},
      {"SBESTIMATE", XPRS_SBESTIMATE, 1},
      {"BARCORES", XPRS_BARCORES, 1},
      {"MAXCHECKSONMAXTIME", XPRS_MAXCHECKSONMAXTIME, 1},
      {"MAXCHECKSONMAXCUTTIME", XPRS_MAXCHECKSONMAXCUTTIME, 1},
      {"HISTORYCOSTS", XPRS_HISTORYCOSTS, 1},
      {"ALGAFTERCROSSOVER", XPRS_ALGAFTERCROSSOVER, 1},
      {"MUTEXCALLBACKS", XPRS_MUTEXCALLBACKS, 1},
      {"BARCRASH", XPRS_BARCRASH, 1},
      {"HEURDIVESOFTROUNDING", XPRS_HEURDIVESOFTROUNDING, 1},
      {"HEURSEARCHROOTSELECT", XPRS_HEURSEARCHROOTSELECT, 1},
      {"HEURSEARCHTREESELECT", XPRS_HEURSEARCHTREESELECT, 1},
      {"MPS18COMPATIBLE", XPRS_MPS18COMPATIBLE, 1},
      {"ROOTPRESOLVE", XPRS_ROOTPRESOLVE, 1},
      {"CROSSOVERDRP", XPRS_CROSSOVERDRP, 1},
      {"FORCEOUTPUT", XPRS_FORCEOUTPUT, 1},
      {"PRIMALOPS", XPRS_PRIMALOPS, 1},
      {"DETERMINISTIC", XPRS_DETERMINISTIC, 1},
      {"PREPROBING", XPRS_PREPROBING, 1},
      {"TREEMEMORYLIMIT", XPRS_TREEMEMORYLIMIT, 1},
      {"TREECOMPRESSION", XPRS_TREECOMPRESSION, 1},
      {"TREEDIAGNOSTICS", XPRS_TREEDIAGNOSTICS, 1},
      {"MAXTREEFILESIZE", XPRS_MAXTREEFILESIZE, 1},
      {"PRECLIQUESTRATEGY", XPRS_PRECLIQUESTRATEGY, 1},
      {"REPAIRINFEASMAXTIME", XPRS_REPAIRINFEASMAXTIME, 1},
      {"IFCHECKCONVEXITY", XPRS_IFCHECKCONVEXITY, 1},
      {"PRIMALUNSHIFT", XPRS_PRIMALUNSHIFT, 1},
      {"REPAIRINDEFINITEQ", XPRS_REPAIRINDEFINITEQ, 1},
      {"MIPRAMPUP", XPRS_MIPRAMPUP, 1},
      {"MAXLOCALBACKTRACK", XPRS_MAXLOCALBACKTRACK, 1},
      {"USERSOLHEURISTIC", XPRS_USERSOLHEURISTIC, 1},
      {"FORCEPARALLELDUAL", XPRS_FORCEPARALLELDUAL, 1},
      {"BACKTRACKTIE", XPRS_BACKTRACKTIE, 1},
      {"BRANCHDISJ", XPRS_BRANCHDISJ, 1},
      {"MIPFRACREDUCE", XPRS_MIPFRACREDUCE, 1},
      {"CONCURRENTTHREADS", XPRS_CONCURRENTTHREADS, 1},
      {"MAXSCALEFACTOR", XPRS_MAXSCALEFACTOR, 1},
      {"HEURTHREADS", XPRS_HEURTHREADS, 1},
      {"THREADS", XPRS_THREADS, 1},
      {"HEURBEFORELP", XPRS_HEURBEFORELP, 1},
      {"PREDOMROW", XPRS_PREDOMROW, 1},
      {"BRANCHSTRUCTURAL", XPRS_BRANCHSTRUCTURAL, 1},
      {"QUADRATICUNSHIFT", XPRS_QUADRATICUNSHIFT, 1},
      {"BARPRESOLVEOPS", XPRS_BARPRESOLVEOPS, 1},
      {"QSIMPLEXOPS", XPRS_QSIMPLEXOPS, 1},
      {"MIPRESTART", XPRS_MIPRESTART, 1},
      {"CONFLICTCUTS", XPRS_CONFLICTCUTS, 1},
      {"PREPROTECTDUAL", XPRS_PREPROTECTDUAL, 1},
      {"CORESPERCPU", XPRS_CORESPERCPU, 1},
      {"RESOURCESTRATEGY", XPRS_RESOURCESTRATEGY, 1},
      {"CLAMPING", XPRS_CLAMPING, 1},
      {"SLEEPONTHREADWAIT", XPRS_SLEEPONTHREADWAIT, 1},
      {"PREDUPROW", XPRS_PREDUPROW, 1},
      {"CPUPLATFORM", XPRS_CPUPLATFORM, 1},
      {"BARALG", XPRS_BARALG, 1},
      {"SIFTING", XPRS_SIFTING, 1},
      {"LPLOGSTYLE", XPRS_LPLOGSTYLE, 1},
      {"RANDOMSEED", XPRS_RANDOMSEED, 1},
      {"TREEQCCUTS", XPRS_TREEQCCUTS, 1},
      {"PRELINDEP", XPRS_PRELINDEP, 1},
      {"DUALTHREADS", XPRS_DUALTHREADS, 1},
      {"PREOBJCUTDETECT", XPRS_PREOBJCUTDETECT, 1},
      {"PREBNDREDQUAD", XPRS_PREBNDREDQUAD, 1},
      {"PREBNDREDCONE", XPRS_PREBNDREDCONE, 1},
      {"PRECOMPONENTS", XPRS_PRECOMPONENTS, 1},
      {"MAXMIPTASKS", XPRS_MAXMIPTASKS, 1},
      {"MIPTERMINATIONMETHOD", XPRS_MIPTERMINATIONMETHOD, 1},
      {"PRECONEDECOMP", XPRS_PRECONEDECOMP, 1},
      {"HEURFORCESPECIALOBJ", XPRS_HEURFORCESPECIALOBJ, 1},
      {"HEURSEARCHROOTCUTFREQ", XPRS_HEURSEARCHROOTCUTFREQ, 1},
      {"PREELIMQUAD", XPRS_PREELIMQUAD, 1},
      {"PREIMPLICATIONS", XPRS_PREIMPLICATIONS, 1},
      {"TUNERMODE", XPRS_TUNERMODE, 1},
      {"TUNERMETHOD", XPRS_TUNERMETHOD, 1},
      {"TUNERTARGET", XPRS_TUNERTARGET, 1},
      {"TUNERTHREADS", XPRS_TUNERTHREADS, 1},
      {"TUNERHISTORY", XPRS_TUNERHISTORY, 1},
      {"TUNERPERMUTE", XPRS_TUNERPERMUTE, 1},
      {"TUNERVERBOSE", XPRS_TUNERVERBOSE, 1},
      {"TUNEROUTPUT", XPRS_TUNEROUTPUT, 1},
      {"PREANALYTICCENTER", XPRS_PREANALYTICCENTER, 1},
      {"NETCUTS", XPRS_NETCUTS, 1},
      {"LPFLAGS", XPRS_LPFLAGS, 1},
      {"MIPKAPPAFREQ", XPRS_MIPKAPPAFREQ, 1},
      {"OBJSCALEFACTOR", XPRS_OBJSCALEFACTOR, 1},
      {"TREEFILELOGINTERVAL", XPRS_TREEFILELOGINTERVAL, 1},
      {"IGNORECONTAINERCPULIMIT", XPRS_IGNORECONTAINERCPULIMIT, 1},
      {"IGNORECONTAINERMEMORYLIMIT", XPRS_IGNORECONTAINERMEMORYLIMIT, 1},
      {"MIPDUALREDUCTIONS", XPRS_MIPDUALREDUCTIONS, 1},
      {"GENCONSDUALREDUCTIONS", XPRS_GENCONSDUALREDUCTIONS, 1},
      {"PWLDUALREDUCTIONS", XPRS_PWLDUALREDUCTIONS, 1},
      {"BARFAILITERLIMIT", XPRS_BARFAILITERLIMIT, 1},
      {"AUTOSCALING", XPRS_AUTOSCALING, 1},
      {"GENCONSABSTRANSFORMATION", XPRS_GENCONSABSTRANSFORMATION, 1},
      {"COMPUTEJOBPRIORITY", XPRS_COMPUTEJOBPRIORITY, 1},
      {"PREFOLDING", XPRS_PREFOLDING, 1},
      {"NETSTALLLIMIT", XPRS_NETSTALLLIMIT, 1},
      {"SERIALIZEPREINTSOL", XPRS_SERIALIZEPREINTSOL, 1},
      {"NUMERICALEMPHASIS", XPRS_NUMERICALEMPHASIS, 1},
      {"PWLNONCONVEXTRANSFORMATION", XPRS_PWLNONCONVEXTRANSFORMATION, 1},
      {"MIPCOMPONENTS", XPRS_MIPCOMPONENTS, 1},
      {"MIPCONCURRENTNODES", XPRS_MIPCONCURRENTNODES, 1},
      {"MIPCONCURRENTSOLVES", XPRS_MIPCONCURRENTSOLVES, 1},
      {"OUTPUTCONTROLS", XPRS_OUTPUTCONTROLS, 1},
      {"SIFTSWITCH", XPRS_SIFTSWITCH, 1},
      {"HEUREMPHASIS", XPRS_HEUREMPHASIS, 1},
      {"COMPUTEMATX", XPRS_COMPUTEMATX, 1},
      {"COMPUTEMATX_IIS", XPRS_COMPUTEMATX_IIS, 1},
      {"COMPUTEMATX_IISMAXTIME", XPRS_COMPUTEMATX_IISMAXTIME, 1},
      {"BARREFITER", XPRS_BARREFITER, 1},
      {"COMPUTELOG", XPRS_COMPUTELOG, 1},
      {"SIFTPRESOLVEOPS", XPRS_SIFTPRESOLVEOPS, 1},
      {"CHECKINPUTDATA", XPRS_CHECKINPUTDATA, 1},
      {"ESCAPENAMES", XPRS_ESCAPENAMES, 1},
      {"IOTIMEOUT", XPRS_IOTIMEOUT, 1},
      {"AUTOCUTTING", XPRS_AUTOCUTTING, 1},
      {"CALLBACKCHECKTIMEDELAY", XPRS_CALLBACKCHECKTIMEDELAY, 1},
      {"MULTIOBJOPS", XPRS_MULTIOBJOPS, 1},
      {"MULTIOBJLOG", XPRS_MULTIOBJLOG, 1},
      {"GLOBALSPATIALBRANCHIFPREFERORIG", XPRS_GLOBALSPATIALBRANCHIFPREFERORIG,
       1},
      {"PRECONFIGURATION", XPRS_PRECONFIGURATION, 1},
      {"FEASIBILITYJUMP", XPRS_FEASIBILITYJUMP, 1},
  };
  for (const auto& [param_string, control, param_value] : params) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    std::string xpress_param_string =
        param_string + " " + std::to_string(param_value);
    solver.SetSolverSpecificParametersAsString(xpress_param_string);
    EXPECT_EQ(param_value, getter.GetIntegerControl(control));
  }
}

TEST(XpressInterface, setInt64Control) {
  std::vector<std::tuple<std::string, int, int>> params = {
      {"EXTRAELEMS", XPRS_EXTRAELEMS, 1},
      {"EXTRASETELEMS", XPRS_EXTRASETELEMS, 1},
  };
  for (const auto& [param_string, control, param_value] : params) {
    MPSolver solver("XPRESS_MIP", MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING);
    XPRSGetter getter(&solver);
    std::string xpress_param_string =
        param_string + " " + std::to_string(param_value);
    solver.SetSolverSpecificParametersAsString(xpress_param_string);
    EXPECT_EQ(param_value, getter.GetInteger64Control(control));
  }
}

TEST_F(XpressFixtureMIP, SolveMIP) {
  // max   x + 2y
  // st.  -x +  y <= 1
  //      2x + 3y <= 12
  //      3x + 2y <= 12
  //       x ,  y >= 0
  //       x ,  y \in Z

  double inf = solver_.infinity();
  MPVariable* x = solver_.MakeIntVar(0, inf, "x");
  MPVariable* y = solver_.MakeIntVar(0, inf, "y");
  MPObjective* obj = solver_.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 2);
  obj->SetMaximization();
  MPConstraint* c1 = solver_.MakeRowConstraint(-inf, 1);
  c1->SetCoefficient(x, -1);
  c1->SetCoefficient(y, 1);
  MPConstraint* c2 = solver_.MakeRowConstraint(-inf, 12);
  c2->SetCoefficient(x, 3);
  c2->SetCoefficient(y, 2);
  MPConstraint* c3 = solver_.MakeRowConstraint(-inf, 12);
  c3->SetCoefficient(x, 2);
  c3->SetCoefficient(y, 3);
  solver_.Solve();

  EXPECT_EQ(obj->Value(), 6);
  EXPECT_EQ(obj->BestBound(), 6);
  EXPECT_EQ(x->solution_value(), 2);
  EXPECT_EQ(y->solution_value(), 2);
}

TEST_F(XpressFixtureLP, SolveLP) {
  // max   x + 2y
  // st.  -x +  y <= 1
  //      2x + 3y <= 12
  //      3x + 2y <= 12
  //       x ,  y \in R+

  double inf = solver_.infinity();
  MPVariable* x = solver_.MakeNumVar(0, inf, "x");
  MPVariable* y = solver_.MakeNumVar(0, inf, "y");
  MPObjective* obj = solver_.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 2);
  obj->SetMaximization();
  MPConstraint* c1 = solver_.MakeRowConstraint(-inf, 1);
  c1->SetCoefficient(x, -1);
  c1->SetCoefficient(y, 1);
  MPConstraint* c2 = solver_.MakeRowConstraint(-inf, 12);
  c2->SetCoefficient(x, 3);
  c2->SetCoefficient(y, 2);
  MPConstraint* c3 = solver_.MakeRowConstraint(-inf, 12);
  c3->SetCoefficient(x, 2);
  c3->SetCoefficient(y, 3);
  solver_.Solve();

  EXPECT_NEAR(obj->Value(), 7.4, 1e-8);
  EXPECT_NEAR(x->solution_value(), 1.8, 1e-8);
  EXPECT_NEAR(y->solution_value(), 2.8, 1e-8);
  EXPECT_NEAR(x->reduced_cost(), 0, 1e-8);
  EXPECT_NEAR(y->reduced_cost(), 0, 1e-8);
  EXPECT_NEAR(c1->dual_value(), 0.2, 1e-8);
  EXPECT_NEAR(c2->dual_value(), 0, 1e-8);
  EXPECT_NEAR(c3->dual_value(), 0.6, 1e-8);
}

// WARNING fragile test because it uses
// the random generator is used by
// buildLargeMip(solver, numVars, maxTime);
// called by
// buildLargeMipWithCallback(solver, 60, 2);
// This tests hints a solution to the solver that is only
// usable for the test generated under linux
#if defined(_MSC_VER)
// Ignore this test because the random generator is different
// for windows and linux.
#elif defined(__GNUC__)
TEST_F(XpressFixtureMIP, SetHint) {
  // Once a solution is added to XPRESS, it is actually impossible to get it
  // back using the API
  // In this test we send the (near) optimal solution as a hint (with
  // obj=56774). Usually XPRESS finds it in ~3000 seconds but in this case it
  // should be able to retain it in just a few seconds using the hint. Note
  // that the logs should mention \"User solution (USER_HINT) stored.\"
  BuildLargeMipWithCallback(solver_, 60, 2);

  std::vector<double> hintValues{
      -2,  -3,  -19, 8,    -1,  -1, 7,   9,   -20, -17,  7,    -7,
      9,   -27, 13,  14,   -6,  -3, -25, -9,  15,  13,   -10,  16,
      -34, 51,  39,  4,    -54, 19, -76, 1,   -17, -18,  -46,  -10,
      0,   -36, 9,   -29,  -6,  4,  -16, -45, -12, -45,  -25,  -70,
      -43, -63, 54,  -148, 79,  -2, 64,  92,  61,  -121, -174, -85};
  std::vector<std::pair<const MPVariable*, double>> hint;
  for (int i = 0; i < solver_.NumVariables(); ++i) {
    hint.push_back(std::make_pair(
        solver_.LookupVariableOrNull("x_" + std::to_string(i)), hintValues[i]));
  }
  solver_.SetHint(hint);
  solver_.Solve();

  // Test that we have at least the near optimal objective function value
  EXPECT_GE(solver_.Objective().Value(), 56774.0);
}
#endif

TEST_F(XpressFixtureMIP, SetCallBack) {
  auto my_mp_callback = BuildLargeMipWithCallback(solver_, 30, 30);
  solver_.Solve();

  int n_solutions = my_mp_callback->GetNSolutions();

  // This is a tough MIP, in 30 seconds XPRESS should have found at least 5
  // solutions (tested with XPRESS v9.0, may change in later versions)
  EXPECT_GT(n_solutions, 5);
  // Test variable values for the last solution found
  for (int i = 0; i < solver_.NumVariables(); ++i) {
    EXPECT_NEAR(my_mp_callback->GetLastVariableValue(i),
                solver_.LookupVariableOrNull("x_" + std::to_string(i))
                    ->solution_value(),
                1e-10);
  }
}

TEST_F(XpressFixtureMIP, SetAndUnsetCallBack) {
  // Test that when we unset a callback it is not called
  auto my_mp_callback = BuildLargeMipWithCallback(solver_, 100, 5);
  solver_.SetCallback(nullptr);
  solver_.Solve();
  EXPECT_EQ(my_mp_callback->GetNSolutions(), 0);
}

TEST_F(XpressFixtureMIP, SetAndResetCallBack) {
  // Test that when we set a new callback then it is called, and old one is not
  // called
  auto old_mp_callback = BuildLargeMipWithCallback(solver_, 100, 5);
  auto new_mp_callback = new MyMPCallback(&solver_, false);
  solver_.SetCallback((MPCallback*)new_mp_callback);
  solver_.Solve();
  EXPECT_EQ(old_mp_callback->GetNSolutions(), 0);
  EXPECT_GT(new_mp_callback->GetNSolutions(), 1);
}

TEST_F(XpressFixtureMIP, CallbackThrowsException) {
  // Test that when the callback throws an exception, it is caught and logged
  BuildLargeMipWithCallback(solver_, 30, 30);
  auto new_mp_callback = new MyMPCallback(&solver_, true);
  solver_.SetCallback((MPCallback*)new_mp_callback);
  testing::internal::CaptureStderr();
  EXPECT_NO_THROW(solver_.Solve());
  std::string errors = testing::internal::GetCapturedStderr();
  // Test that StdErr contains the following error message
  std::string expected_error =
      "Caught exception during user-defined call-back: This is a mocked "
      "exception in MyMPCallback";
  ASSERT_NE(errors.find(expected_error), std::string::npos);
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  testing::InitGoogleTest(&argc, argv);
  auto solver = operations_research::MPSolver::CreateSolver("XPRESS_LP");
  if (solver == nullptr) {
    LOG(ERROR) << "Xpress solver is not available";
    return EXIT_SUCCESS;
  } else {
    return RUN_ALL_TESTS();
  }
}
