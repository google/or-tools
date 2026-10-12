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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/util/tuple_set.h"

ABSL_DECLARE_FLAG(bool, cp_use_small_table);

// TODO(user) : use TYPED_TEST.
namespace operations_research {
namespace {
class TableTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("TableTest"); }

  std::unique_ptr<Solver> solver_;
};

void SetTableMode(int mode) {
  switch (mode) {
    case 0:
      absl::SetFlag(&FLAGS_cp_use_small_table, false);
      break;
    case 1:
      absl::SetFlag(&FLAGS_cp_use_small_table, true);
      break;
  }
}

TEST_IN_PROPAGATION_LOOP0(TableTest, BinaryPropagationTest, solver, true, mode,
                          0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(2);
  tuples.Insert2(0, 0);
  tuples.Insert2(1, 1);
  tuples.Insert2(2, 2);
  tuples.Insert2(3, 3);
  tuples.Insert2(4, 0);
  tuples.Insert2(5, 1);
  tuples.Insert2(6, 2);
  tuples.Insert2(7, 3);
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(2, 0, 10, "x", &vars);
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(3, vars[1]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1));
  EXPECT_EQ(4, vars[1]->Size());
  vars[0]->RemoveValue(5);
  EXPECT_FALSE(vars[1]->Contains(1));
  EXPECT_EQ(3, vars[1]->Size());
  vars[1]->RemoveValue(2);
  EXPECT_FALSE(vars[0]->Contains(2));
  EXPECT_FALSE(vars[0]->Contains(6));
  EXPECT_EQ(2, vars[1]->Size());
  EXPECT_EQ(4, vars[0]->Size());
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, BinaryPropagationDoubledTest, solver, true,
                          mode, 0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(2);
  tuples.Insert2(0, 0);
  tuples.Insert2(1, 1);
  tuples.Insert2(2, 2);
  tuples.Insert2(3, 3);
  tuples.Insert2(4, 0);
  tuples.Insert2(5, 1);
  tuples.Insert2(6, 2);
  tuples.Insert2(7, 3);
  tuples.Insert2(0, 0);
  tuples.Insert2(1, 1);
  tuples.Insert2(2, 2);
  tuples.Insert2(3, 3);
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(2, 0, 10, "x", &vars);
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(3, vars[1]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1));
  EXPECT_EQ(4, vars[1]->Size());
  vars[0]->RemoveValue(5);
  EXPECT_FALSE(vars[1]->Contains(1));
  EXPECT_EQ(3, vars[1]->Size());
  vars[1]->RemoveValue(2);
  EXPECT_FALSE(vars[0]->Contains(2));
  EXPECT_FALSE(vars[0]->Contains(6));
  EXPECT_EQ(2, vars[1]->Size());
  EXPECT_EQ(4, vars[0]->Size());
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, TernaryPropagationTest, solver, true, mode,
                          0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(3);
  tuples.Insert3(0, 0, 1);
  tuples.Insert3(1, 1, 1);
  tuples.Insert3(2, 2, 1);
  tuples.Insert3(3, 3, 1);
  tuples.Insert3(4, 0, 2);
  tuples.Insert3(5, 1, 2);
  tuples.Insert3(6, 2, 2);
  tuples.Insert3(7, 3, 2);
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(3, 0, 10, "x", &vars);
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(3, vars[1]->Max());
  EXPECT_EQ(1, vars[2]->Min());
  EXPECT_EQ(2, vars[2]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1));
  EXPECT_EQ(1, vars[2]->Min());
  EXPECT_EQ(2, vars[2]->Max());
  vars[2]->SetValue(1);
  EXPECT_FALSE(vars[1]->Contains(1));
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, EmptyTupleSet, solver, false, mode, 0, 1) {
  SetTableMode(mode);
  Constraint* ct = nullptr;
  {
    IntTupleSet tuples(2);
    std::vector<IntVar*> vars;
    solver->MakeIntVarArray(2, 0, 10, "x", &vars);
    ct = solver->MakeAllowedAssignments(vars, tuples);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_F(TableTest, VectorAPI) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  IntTupleSet tuples(3);
  std::vector<IntVar*> vars;
  vars.push_back(v1);
  vars.push_back(v2);
  vars.push_back(v3);

  for (int i = 0; i < 5; ++i) {
    std::vector<int> tuple;
    for (int j = 0; j < 3; ++j) {
      tuple.push_back(i);
    }
    tuples.Insert(tuple);
  }
  solver_->AddConstraint(solver_->MakeAllowedAssignments(vars, tuples));
  DecisionBuilder* db = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  int counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(counter, v1->Value());
    EXPECT_EQ(counter, v2->Value());
    EXPECT_EQ(counter, v3->Value());
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(5, counter);
}

TEST_IN_PROPAGATION_LOOP0(TableTest, BinaryPropagationLargeTest, solver, true,
                          mode, 0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(2);
  tuples.Insert2(0, 1000);
  tuples.Insert2(1, 1001);
  tuples.Insert2(2, 1002);
  tuples.Insert2(3, 1003);
  tuples.Insert2(4, 1000);
  tuples.Insert2(5, 1001);
  tuples.Insert2(6, 1002);
  tuples.Insert2(7, 1003);
  std::vector<IntVar*> vars(2);
  vars[0] = solver->MakeIntVar(0, 10, "x_0");
  vars[1] = solver->MakeIntVar(1000, 1010, "x_1");
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(1000, vars[1]->Min());
  EXPECT_EQ(1003, vars[1]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1001));
  EXPECT_EQ(4, vars[1]->Size());
  vars[0]->RemoveValue(5);
  EXPECT_FALSE(vars[1]->Contains(1001));
  EXPECT_EQ(3, vars[1]->Size());
  vars[1]->RemoveValue(1002);
  EXPECT_FALSE(vars[0]->Contains(2));
  EXPECT_FALSE(vars[0]->Contains(6));
  EXPECT_EQ(2, vars[1]->Size());
  EXPECT_EQ(4, vars[0]->Size());
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, BinaryPropagationDoubledLargeTest, solver,
                          true, mode, 0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(2);
  tuples.Insert2(0, 1000);
  tuples.Insert2(1, 1001);
  tuples.Insert2(2, 1002);
  tuples.Insert2(3, 1003);
  tuples.Insert2(4, 1000);
  tuples.Insert2(5, 1001);
  tuples.Insert2(6, 1002);
  tuples.Insert2(7, 1003);
  tuples.Insert2(0, 1000);
  tuples.Insert2(1, 1001);
  tuples.Insert2(2, 1002);
  tuples.Insert2(3, 1003);
  std::vector<IntVar*> vars(2);
  vars[0] = solver->MakeIntVar(0, 10, "x_0");
  vars[1] = solver->MakeIntVar(1000, 1010, "x_1");
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(1000, vars[1]->Min());
  EXPECT_EQ(1003, vars[1]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1001));
  EXPECT_EQ(4, vars[1]->Size());
  vars[0]->RemoveValue(5);
  EXPECT_FALSE(vars[1]->Contains(1001));
  EXPECT_EQ(3, vars[1]->Size());
  vars[1]->RemoveValue(1002);
  EXPECT_FALSE(vars[0]->Contains(2));
  EXPECT_FALSE(vars[0]->Contains(6));
  EXPECT_EQ(2, vars[1]->Size());
  EXPECT_EQ(4, vars[0]->Size());
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, TernaryPropagationLargeTest, solver, true,
                          mode, 0, 1) {
  SetTableMode(mode);
  IntTupleSet tuples(3);
  tuples.Insert3(0, 0, 1001);
  tuples.Insert3(1, 1, 1001);
  tuples.Insert3(2, 2, 1001);
  tuples.Insert3(3, 3, 1001);
  tuples.Insert3(4, 0, 1002);
  tuples.Insert3(5, 1, 1002);
  tuples.Insert3(6, 2, 1002);
  tuples.Insert3(7, 3, 1002);
  std::vector<IntVar*> vars(3);
  vars[0] = solver->MakeIntVar(0, 10, "x_0");
  vars[1] = solver->MakeIntVar(0, 10, "x_1");
  vars[2] = solver->MakeIntVar(1000, 1010, "x_2");
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(7, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(3, vars[1]->Max());
  EXPECT_EQ(1001, vars[2]->Min());
  EXPECT_EQ(1002, vars[2]->Max());
  vars[0]->RemoveValue(1);
  EXPECT_TRUE(vars[1]->Contains(1));
  EXPECT_EQ(1001, vars[2]->Min());
  EXPECT_EQ(1002, vars[2]->Max());
  vars[2]->SetValue(1001);
  EXPECT_FALSE(vars[1]->Contains(1));
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, SomeInvalidTuples, solver, true, mode, 0,
                          1) {
  SetTableMode(mode);
  IntTupleSet tuples(2);
  tuples.Insert2(0, 0);
  tuples.Insert2(3, 1);
  tuples.Insert2(1, 5);
  tuples.Insert2(1, 3);
  std::vector<IntVar*> vars(2);
  vars[0] = solver->MakeIntVar(0, 3, "x_0");
  vars[1] = solver->MakeIntVar(0, 2, "x_1");
  Constraint* ct = solver->MakeAllowedAssignments(vars, tuples);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(3, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(1, vars[1]->Max());
  vars[0]->RemoveValue(3);
  EXPECT_EQ(0, vars[0]->Max());
  EXPECT_EQ(0, vars[1]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION_LOOP0(TableTest, EmptyTupleLargeSet, solver, false, mode, 0,
                          1) {
  SetTableMode(mode);
  Constraint* ct = nullptr;
  {
    IntTupleSet tuples(2);
    std::vector<IntVar*> vars(2);
    vars[0] = solver->MakeIntVar(0, 10, "x_0");
    vars[1] = solver->MakeIntVar(1000, 1010, "x_1");
    ct = solver->MakeAllowedAssignments(vars, tuples);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_F(TableTest, VectorAPILarge) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(1000, 1010, "v3");
  IntTupleSet tuples(3);
  std::vector<IntVar*> vars;
  vars.push_back(v1);
  vars.push_back(v2);
  vars.push_back(v3);

  for (int i = 0; i < 5; ++i) {
    std::vector<int> tuple;
    for (int j = 0; j < 3; ++j) {
      tuple.push_back(i + 1000 * (j == 2));
    }
    tuples.Insert(tuple);
  }
  solver_->AddConstraint(solver_->MakeAllowedAssignments(vars, tuples));
  DecisionBuilder* db = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  int counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(counter, v1->Value());
    EXPECT_EQ(counter, v2->Value());
    EXPECT_EQ(counter + 1000, v3->Value());
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(5, counter);
}

TEST_F(TableTest, TupleMismatch) {
  IntVar* const v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* const v2 = solver_->MakeIntVar(1, 2, "v2");
  std::vector<IntVar*> vars;
  vars.push_back(v1);
  vars.push_back(v2);
  IntTupleSet tuples(2);
  tuples.Insert2(2, 1);
  tuples.Insert2(0, 0);
  tuples.Insert2(2, 0);
  solver_->AddConstraint(solver_->MakeAllowedAssignments(vars, tuples));
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  EXPECT_FALSE(solver_->Solve(db));
}

// ----- TransitionTest -----

TEST(TransitionTest, NonogramRule) {
  const int kVars = 10;
  Solver solver("TransitionTest");
  std::vector<IntVar*> vars;
  solver.MakeBoolVarArray(kVars, "vars", &vars);
  IntTupleSet tuples(3);
  tuples.Insert3(1, 0, 1);
  tuples.Insert3(1, 1, 2);
  tuples.Insert3(2, 1, 3);
  tuples.Insert3(3, 1, 4);
  tuples.Insert3(4, 0, 5);
  tuples.Insert3(5, 0, 5);
  tuples.Insert3(5, 1, 6);
  tuples.Insert3(6, 1, 7);
  tuples.Insert3(7, 0, 8);
  tuples.Insert3(8, 0, 8);
  tuples.Insert3(8, 1, 9);
  tuples.Insert3(9, 0, 9);
  // These tuples accept sequence with 3 '1', then 2 '1', then 1 '1',
  // separated by at least one '0'.

  const int kExpected[][kVars] = {{0, 0, 1, 1, 1, 0, 1, 1, 0, 1},  //
                                  {0, 1, 1, 1, 0, 0, 1, 1, 0, 1},  //
                                  {0, 1, 1, 1, 0, 1, 1, 0, 0, 1},  //
                                  {0, 1, 1, 1, 0, 1, 1, 0, 1, 0},  //
                                  {1, 1, 1, 0, 0, 0, 1, 1, 0, 1},  //
                                  {1, 1, 1, 0, 0, 1, 1, 0, 0, 1},  //
                                  {1, 1, 1, 0, 0, 1, 1, 0, 1, 0},  //
                                  {1, 1, 1, 0, 1, 1, 0, 0, 0, 1},  //
                                  {1, 1, 1, 0, 1, 1, 0, 0, 1, 0},  //
                                  {1, 1, 1, 0, 1, 1, 0, 1, 0, 0}};
  std::vector<int64_t> final;
  final.push_back(9);
  solver.AddConstraint(solver.MakeTransitionConstraint(vars, tuples, 1, final));
  DecisionBuilder* const decision_builder = solver.MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(decision_builder);
  int counter = 0;
  while (solver.NextSolution()) {
    for (int row = 0; row < kVars; ++row) {
      EXPECT_EQ(kExpected[counter][row], vars[row]->Value());
    }
    counter++;
  }
  solver.EndSearch();
  EXPECT_EQ(ABSL_ARRAYSIZE(kExpected), counter);
}
TEST(TransitionTest, LoopingAutomaton) {
  const int kVars = 10;
  Solver solver("TransitionTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kVars, 0, 2, "vars", &vars);
  IntTupleSet tuples(3);
  tuples.Insert3(1, 0, 1);
  tuples.Insert3(1, 1, 2);
  tuples.Insert3(2, 2, 3);
  tuples.Insert3(3, 1, 2);
  tuples.Insert3(3, 0, 4);
  tuples.Insert3(4, 0, 4);
  // These tuples accept 0*(12)+0+.
  const int kExpected[][kVars] = {{0, 0, 0, 0, 0, 0, 0, 1, 2, 0},  //
                                  {0, 0, 0, 0, 0, 0, 1, 2, 0, 0},  //
                                  {0, 0, 0, 0, 0, 1, 2, 0, 0, 0},  //
                                  {0, 0, 0, 0, 0, 1, 2, 1, 2, 0},  //
                                  {0, 0, 0, 0, 1, 2, 0, 0, 0, 0},  //
                                  {0, 0, 0, 0, 1, 2, 1, 2, 0, 0},  //
                                  {0, 0, 0, 1, 2, 0, 0, 0, 0, 0},  //
                                  {0, 0, 0, 1, 2, 1, 2, 0, 0, 0},  //
                                  {0, 0, 0, 1, 2, 1, 2, 1, 2, 0},  //
                                  {0, 0, 1, 2, 0, 0, 0, 0, 0, 0},  //
                                  {0, 0, 1, 2, 1, 2, 0, 0, 0, 0},  //
                                  {0, 0, 1, 2, 1, 2, 1, 2, 0, 0},  //
                                  {0, 1, 2, 0, 0, 0, 0, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 0, 0, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 1, 2, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 1, 2, 1, 2, 0},  //
                                  {1, 2, 0, 0, 0, 0, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 0, 0, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 1, 2, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 1, 2, 1, 2, 0, 0}};

  std::vector<int64_t> final;
  final.push_back(4);
  solver.AddConstraint(solver.MakeTransitionConstraint(vars, tuples, 1, final));
  DecisionBuilder* const decision_builder = solver.MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(decision_builder);
  int counter = 0;
  while (solver.NextSolution()) {
    for (int row = 0; row < kVars; ++row) {
      EXPECT_EQ(kExpected[counter][row], vars[row]->Value());
    }
    counter++;
  }
  solver.EndSearch();
  EXPECT_EQ(ABSL_ARRAYSIZE(kExpected), counter);
}

TEST(TransitionTest, LoopingAutomatonMultipleFinalStates) {
  const int kVars = 10;
  Solver solver("TransitionTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kVars, 0, 2, "vars", &vars);
  IntTupleSet tuples(3);
  tuples.Insert3(1, 0, 1);
  tuples.Insert3(1, 1, 2);
  tuples.Insert3(2, 2, 3);
  tuples.Insert3(3, 1, 2);
  tuples.Insert3(3, 0, 4);
  tuples.Insert3(4, 0, 4);
  // These tuples accept 0*(12)+0*.
  const int kExpected[][kVars] = {{0, 0, 0, 0, 0, 0, 0, 0, 1, 2},  //
                                  {0, 0, 0, 0, 0, 0, 0, 1, 2, 0},  //
                                  {0, 0, 0, 0, 0, 0, 1, 2, 0, 0},  //
                                  {0, 0, 0, 0, 0, 0, 1, 2, 1, 2},  //
                                  {0, 0, 0, 0, 0, 1, 2, 0, 0, 0},  //
                                  {0, 0, 0, 0, 0, 1, 2, 1, 2, 0},  //
                                  {0, 0, 0, 0, 1, 2, 0, 0, 0, 0},  //
                                  {0, 0, 0, 0, 1, 2, 1, 2, 0, 0},  //
                                  {0, 0, 0, 0, 1, 2, 1, 2, 1, 2},  //
                                  {0, 0, 0, 1, 2, 0, 0, 0, 0, 0},  //
                                  {0, 0, 0, 1, 2, 1, 2, 0, 0, 0},  //
                                  {0, 0, 0, 1, 2, 1, 2, 1, 2, 0},  //
                                  {0, 0, 1, 2, 0, 0, 0, 0, 0, 0},  //
                                  {0, 0, 1, 2, 1, 2, 0, 0, 0, 0},  //
                                  {0, 0, 1, 2, 1, 2, 1, 2, 0, 0},  //
                                  {0, 0, 1, 2, 1, 2, 1, 2, 1, 2},  //
                                  {0, 1, 2, 0, 0, 0, 0, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 0, 0, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 1, 2, 0, 0, 0},  //
                                  {0, 1, 2, 1, 2, 1, 2, 1, 2, 0},  //
                                  {1, 2, 0, 0, 0, 0, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 0, 0, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 1, 2, 0, 0, 0, 0},  //
                                  {1, 2, 1, 2, 1, 2, 1, 2, 0, 0},  //
                                  {1, 2, 1, 2, 1, 2, 1, 2, 1, 2}};

  std::vector<int64_t> final;
  final.push_back(4);
  final.push_back(3);
  solver.AddConstraint(solver.MakeTransitionConstraint(vars, tuples, 1, final));
  DecisionBuilder* const decision_builder = solver.MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(decision_builder);
  int counter = 0;
  while (solver.NextSolution()) {
    for (int row = 0; row < kVars; ++row) {
      EXPECT_EQ(kExpected[counter][row], vars[row]->Value());
    }
    counter++;
  }
  solver.EndSearch();
  EXPECT_EQ(ABSL_ARRAYSIZE(kExpected), counter);
}

TEST_IN_PROPAGATION(TransitionTest, UnitPropagationTest, solver, true) {
  const int kVars = 5;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kVars, 0, 2, "vars", &vars);
  IntTupleSet tuples(3);
  tuples.Insert3(1, 0, 1);
  tuples.Insert3(1, 1, 2);
  tuples.Insert3(2, 2, 3);
  tuples.Insert3(3, 1, 2);
  tuples.Insert3(3, 0, 4);
  tuples.Insert3(4, 0, 4);
  std::vector<int64_t> final;
  final.push_back(4);
  final.push_back(3);
  solver->AddConstraint(
      solver->MakeTransitionConstraint(vars, tuples, 1, final));
  // Init propagation.
  for (int i = 0; i < kVars; ++i) {
    LOG(INFO) << vars[i]->DebugString();
  }
  // [ [0, 1][0 .. 2][0 .. 2][0 .. 2][0, 2] ]
  EXPECT_EQ(2, vars[0]->Size());
  EXPECT_FALSE(vars[0]->Contains(2));
  EXPECT_EQ(3, vars[1]->Size());
  EXPECT_EQ(3, vars[2]->Size());
  EXPECT_EQ(3, vars[3]->Size());
  EXPECT_EQ(2, vars[4]->Size());
  EXPECT_FALSE(vars[4]->Contains(1));

  vars[0]->SetValue(1);
  // [ [1][2][0, 1][0 .. 2][0] ]
  EXPECT_EQ(2, vars[1]->Min());
  EXPECT_EQ(2, vars[2]->Size());
  EXPECT_FALSE(vars[2]->Contains(2));
  // 1 is no longer possible for vars[3], state is either 3 or 4.
  EXPECT_EQ(2, vars[3]->Size());
  EXPECT_FALSE(vars[3]->Contains(1));
  EXPECT_EQ(0, vars[4]->Max());

  vars[3]->SetValue(0);  // Forcing the last 3 vars to 0.
  // [ [1][2][0][0][0] ]
  EXPECT_EQ(2, vars[1]->Min());
  EXPECT_EQ(0, vars[2]->Max());
  EXPECT_EQ(0, vars[3]->Max());
  EXPECT_EQ(0, vars[4]->Max());
  return nullptr;
}
}  // namespace
}  // namespace operations_research
