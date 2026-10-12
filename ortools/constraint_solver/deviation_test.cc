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

#include <stddef.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
class TestDeviationFailingProgagation;
}
namespace {
class TestDeviationTestAcceptableDeviation;
}
namespace {
class TestDeviationTestMaxVar;
}
namespace {
class TestDeviationTestMaxVarBis;
}
namespace {
class TestDeviationTestMaxVarTer;
}
namespace {
class TestDeviationTestMinDeviation;
}
namespace {
class TestDeviationTestNegativeVarMax;
}
namespace {
class TestDeviationTestNegativeVarMaxBis;
}
namespace {
class TestDeviationTestNullDeviation;
}
namespace {
class TestDeviationTestRangeVar;
}
namespace {
class TestDeviationTestRangeVarBis;
}
namespace {
class TestDeviationTestRangeVarTer;
}
namespace {
class TestDeviationTestRangeVarTerSwap;
}
namespace {
class TestDeviationTightProgagation;
}

namespace {
TEST_IN_PROPAGATION(TestDeviation, NullDeviation, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(4, -2, 2, &x);
  IntVar* const nd = solver->MakeIntVar(0, 0);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 0));
  for (int i = 0; i < x.size(); ++i) {
    EXPECT_BOUND_TO(x[i], 0);
  }
  return nullptr;
}

TEST(TestDeviation, DebugString) {
  Solver solver("TestDeviation");
  std::vector<IntVar*> x;
  solver.MakeIntVarArray(4, -2, 2, "x", &x);
  IntVar* const nd = solver.MakeIntVar(0, 6, "deviation");
  Constraint* const ct = solver.MakeDeviation(x, nd, 1);
  const std::string kExpectedOutput =
      "Deviation([x0(-2..2), x1(-2..2), x2(-2..2), "
      "x3(-2..2)], deviation_var = deviation(0..6), sum = 1)";
  EXPECT_EQ(kExpectedOutput, ct->DebugString());
}

TEST_IN_PROPAGATION(TestDeviation, AcceptableDeviation, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(4, -2, 2, &x);
  IntVar* const nd = solver->MakeIntVar(0, 6);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 1));
  EXPECT_BOUND_TO(nd, 6);
  EXPECT_EQ_RANGE(x[0], 0, 1);
  EXPECT_EQ_RANGE(x[1], 0, 1);
  EXPECT_EQ_RANGE(x[2], 0, 1);
  EXPECT_EQ_RANGE(x[3], 0, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, MaxVar, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(3, 7));
  x.push_back(solver->MakeIntVar(0, 5));
  x.push_back(solver->MakeIntVar(5, 6));
  x.push_back(solver->MakeIntVar(5, 7));
  IntVar* const nd = solver->MakeIntVar(0, 18);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ_RANGE(x[0], 3, 5);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, MaxVarBis, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(3, 7));
  x.push_back(solver->MakeIntVar(0, 5));
  x.push_back(solver->MakeIntVar(5, 6));
  x.push_back(solver->MakeIntVar(5, 7));
  IntVar* const nd = solver->MakeIntVar(0, 12);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ(4, x[0]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, RangeVar, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(3, 10));
  x.push_back(solver->MakeIntVar(4, 5));
  x.push_back(solver->MakeIntVar(3, 6));
  x.push_back(solver->MakeIntVar(0, 2));
  IntVar* const nd = solver->MakeIntVar(0, 45);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ_RANGE(x[0], 4, 9);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, RangeVarBis, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(3, 10));
  x.push_back(solver->MakeIntVar(4, 5));
  x.push_back(solver->MakeIntVar(3, 6));
  x.push_back(solver->MakeIntVar(0, 2));
  IntVar* const nd = solver->MakeIntVar(0, 22);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ_RANGE(x[0], 4, 7);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, RangeVarTer, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(3, 10));
  x.push_back(solver->MakeIntVar(4, 5));
  x.push_back(solver->MakeIntVar(3, 6));
  x.push_back(solver->MakeIntVar(0, 2));
  IntVar* const nd = solver->MakeIntVar(0, 30);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ_RANGE(x[0], 4, 8);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, RangeVarTerSwap, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(4, 5));
  x.push_back(solver->MakeIntVar(3, 6));
  x.push_back(solver->MakeIntVar(0, 2));
  x.push_back(solver->MakeIntVar(3, 10));
  IntVar* const nd = solver->MakeIntVar(0, 30);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 17));
  EXPECT_EQ_RANGE(x[3], 4, 8);
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, MinDeviation, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(11, 16));
  x.push_back(solver->MakeIntVar(9, 11));
  x.push_back(solver->MakeIntVar(12, 14));
  x.push_back(solver->MakeIntVar(13, 14));
  x.push_back(solver->MakeIntVar(10, 12));
  x.push_back(solver->MakeIntVar(12, 15));
  IntVar* const nd = solver->MakeIntVar(0, 1000);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 74));
  EXPECT_EQ(24, nd->Min());
  return nullptr;
}

void AddDecomposedDeviation(Solver* const cp, const std::vector<IntVar*>& x,
                            int s, IntVar* const nd) {
  std::vector<IntVar*> dev;
  const int size = x.size();
  for (int i = 0; i < size; ++i) {
    dev.push_back(
        cp->MakeAbs(cp->MakeSum(cp->MakeProd(x[i], size), -s))->Var());
  }
  cp->AddConstraint(cp->MakeEquality(cp->MakeSum(dev)->Var(), nd));
  cp->AddConstraint(cp->MakeSumEquality(x, s));
}

int NumSolutions(int use_decomposition) {
  Solver cp("TestDeviation");

  const int kSum = 74;
  std::vector<IntVar*> x;
  x.push_back(cp.MakeIntVar(11, 16));
  x.push_back(cp.MakeIntVar(9, 11));
  x.push_back(cp.MakeIntVar(12, 14));
  x.push_back(cp.MakeIntVar(13, 14));
  x.push_back(cp.MakeIntVar(10, 12));
  x.push_back(cp.MakeIntVar(12, 15));
  IntVar* const nd = cp.MakeIntVar(0, 34);

  if (use_decomposition) {
    AddDecomposedDeviation(&cp, x, kSum, nd);
  } else {
    cp.AddConstraint(cp.MakeDeviation(x, nd, kSum));
  }

  DecisionBuilder* const db =
      cp.MakePhase(x, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  int num_solutions = 0;
  cp.NewSearch(db);
  while (cp.NextSolution()) {
    num_solutions++;
  }
  cp.EndSearch();
  return num_solutions;
}

TEST(TestDeviation, CompareMethods) {
  const int NumWithDeviation = NumSolutions(false);
  const int NumWithoutDeviation = NumSolutions(true);
  EXPECT_EQ(NumWithDeviation, NumWithoutDeviation);
}

TEST_IN_PROPAGATION(TestDeviation, NegativeVarMax, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(-27, -25));
  x.push_back(solver->MakeIntVar(-27, -27));
  x.push_back(solver->MakeIntVar(-27, -27));
  x.push_back(solver->MakeIntVar(-27, -25));
  x.push_back(solver->MakeIntVar(-30, -30));
  x.push_back(solver->MakeIntVar(-27, -25));
  x.push_back(solver->MakeIntVar(-27, -25));
  x.push_back(solver->MakeIntVar(-27, -23));
  IntVar* const nd = solver->MakeIntVar(0, 75);
  solver->AddConstraint(solver->MakeDeviation(x, nd, -213));
  EXPECT_EQ(-24, x[7]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, MaxVarTer, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(11, 16));
  x.push_back(solver->MakeIntVar(9, 11));
  x.push_back(solver->MakeIntVar(12, 14));
  x.push_back(solver->MakeIntVar(13, 14));
  x.push_back(solver->MakeIntVar(10, 12));
  x.push_back(solver->MakeIntVar(12, 15));
  IntVar* const nd = solver->MakeIntVar(0, 34);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 74));
  EXPECT_EQ(14, x[0]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, NegativeVarMaxBis, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(-14, -12));
  x.push_back(solver->MakeIntVar(-11, -9));
  x.push_back(solver->MakeIntVar(-14, -12));
  x.push_back(solver->MakeIntVar(-14, -13));
  x.push_back(solver->MakeIntVar(-12, -10));
  x.push_back(solver->MakeIntVar(-14, -12));
  IntVar* const nd = solver->MakeIntVar(0, 34);
  solver->AddConstraint(solver->MakeDeviation(x, nd, -74));
  EXPECT_EQ(-10, x[1]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(TestDeviation, TightProgagation, solver, true) {
  std::vector<IntVar*> x;
  x.push_back(solver->MakeIntVar(-4, -2));
  x.push_back(solver->MakeIntVar(-4, -2));
  x.push_back(solver->MakeIntVar(2, 4));
  x.push_back(solver->MakeIntVar(2, 4));
  IntVar* const nd = solver->MakeIntVar(0, 32);
  solver->AddConstraint(solver->MakeDeviation(x, nd, 0));
  EXPECT_BOUND_TO(nd, 32);
  EXPECT_BOUND_TO(x[0], -2);
  EXPECT_BOUND_TO(x[1], -2);
  EXPECT_BOUND_TO(x[2], 2);
  EXPECT_BOUND_TO(x[3], 2);
  return nullptr;
}
TEST_IN_PROPAGATION(TestDeviation, FailingProgagation, solver, false) {
  Constraint* ct = nullptr;
  {
    std::vector<IntVar*> x;
    x.push_back(solver->MakeIntVar(-4, -2));
    x.push_back(solver->MakeIntVar(-4, -2));
    x.push_back(solver->MakeIntVar(2, 4));
    x.push_back(solver->MakeIntVar(2, 4));
    IntVar* const nd = solver->MakeIntVar(0, 31);
    ct = solver->MakeDeviation(x, nd, 0);
  }
  solver->AddConstraint(ct);
  return nullptr;
}
}  // namespace
}  // namespace operations_research
