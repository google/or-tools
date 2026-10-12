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

#include "ortools/constraint_solver/assignment.h"

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/path.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/sequence_var.h"

namespace operations_research {
namespace {

class AssignmentTest : public testing::Test {
 protected:
  enum ActivateState { DEFAULT, ACTIVATED, DEACTIVATED };

  static const int kAssignmentSize;

  void SetUp() override {
    solver_ = std::make_unique<Solver>("AssignmentTest");
    assignment_ = std::make_unique<Assignment>(solver_.get());
  }
  void Activate(IntVar* var, ActivateState activate);
  void ActivateObjective(ActivateState activate);
  void ActivateObjectiveFromIndex(int index, ActivateState activate);
  void MakeVars(int64_t var_min, int64_t var_max, ActivateState activate,
                std::vector<IntVar*>* vars);
  void TestAssignmentFromSolver(int64_t var_min, int64_t var_max,
                                ActivateState activate);
  void TestAssignmentFromDecision(int64_t var_min, int64_t var_max,
                                  ActivateState activate);
  void TestAssignmentFromVar(int64_t var_min, int64_t var_max,
                             ActivateState activate);
  void TestAssignmentFromVarArray(int64_t var_min, int64_t var_max,
                                  ActivateState activate);
  void TestAssignmentFromVarVector(int64_t var_min, int64_t var_max,
                                   ActivateState activate);
  void TestAssignmentObjective(int64_t obj_min, int64_t obj_max,
                               ActivateState activate);
  void TestAssignmentObjectives(int64_t obj_min, int64_t obj_max,
                                ActivateState activate);
  void TestAssignmentObjectiveFromSolver(int64_t obj_min, int64_t obj_max,
                                         ActivateState activate);
  void TestAssignmentObjectivesFromSolver(int64_t obj_min, int64_t obj_max,
                                          ActivateState activate);
  void TestRestoreAssignment(int64_t var_min, int64_t var_max,
                             ActivateState activate);
  void TestRestoreObjective(int64_t obj_min, int64_t obj_max,
                            ActivateState activate);
  void TestRestoreObjectives(int64_t obj_min, int64_t obj_max,
                             ActivateState activate);
  void TestStoredAssignment(int64_t min_value, int64_t max_value,
                            bool activated);
  void TestUnstoredAssignment(bool activated);

  std::unique_ptr<Assignment> assignment_;
  std::unique_ptr<Solver> solver_;
  absl::flat_hash_set<IntVar*> unique_vars_;
};

const int AssignmentTest::kAssignmentSize = 10000;

void AssignmentTest::Activate(IntVar* const var, ActivateState activate) {
  if (ACTIVATED == activate) {
    assignment_->Activate(var);
  } else if (DEACTIVATED == activate) {
    assignment_->Deactivate(var);
  }
}

void AssignmentTest::ActivateObjective(ActivateState activate) {
  if (ACTIVATED == activate) {
    assignment_->ActivateObjective();
  } else if (DEACTIVATED == activate) {
    assignment_->DeactivateObjective();
  }
}

void AssignmentTest::ActivateObjectiveFromIndex(int index,
                                                ActivateState activate) {
  if (ACTIVATED == activate) {
    assignment_->ActivateObjectiveFromIndex(index);
  } else if (DEACTIVATED == activate) {
    assignment_->DeactivateObjectiveFromIndex(index);
  }
}

void AssignmentTest::MakeVars(int64_t var_min, int64_t var_max,
                              ActivateState activate,
                              std::vector<IntVar*>* const vars) {
  unique_vars_.clear();
  for (int i = 0; i < kAssignmentSize; ++i) {
    IntVar* const var = solver_->MakeIntVar(var_min, var_max);
    unique_vars_.insert(var);
    vars->push_back(var);
    assignment_->Add(var);
    Activate(var, activate);
  }
  EXPECT_EQ(unique_vars_.size(), assignment_->Size());
}

void AssignmentTest::TestAssignmentFromSolver(int64_t var_min, int64_t var_max,
                                              ActivateState activate) {
  std::vector<IntVar*> vars;
  MakeVars(var_min, var_max, activate, &vars);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  assignment_->Store();
  TestStoredAssignment(var_min, var_min, DEACTIVATED != activate);
  solver_->EndSearch();
}

void AssignmentTest::TestAssignmentFromDecision(int64_t var_min,
                                                int64_t var_max,
                                                ActivateState activate) {
  std::vector<IntVar*> vars;
  MakeVars(var_min, var_max, activate, &vars);
  DecisionBuilder* store = solver_->MakeStoreAssignment(assignment_.get());
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* db_and_store = solver_->Compose(db, store);
  solver_->NewSearch(db_and_store);
  ASSERT_TRUE(solver_->NextSolution());
  TestStoredAssignment(var_min, var_min, DEACTIVATED != activate);
  solver_->EndSearch();
}

void AssignmentTest::TestAssignmentFromVar(int64_t var_min, int64_t var_max,
                                           ActivateState activate) {
  std::vector<IntVar*> vars;
  MakeVars(var_min, var_max, activate, &vars);
  TestUnstoredAssignment(DEACTIVATED != activate);
  const int64_t min_value = 0;
  const int64_t max_value = 5;
  for (IntVar* const var : vars) {
    assignment_->SetMin(var, min_value);
  }
  TestStoredAssignment(min_value, kint64max, DEACTIVATED != activate);
  for (IntVar* const var : vars) {
    assignment_->SetMax(var, max_value);
  }
  TestStoredAssignment(min_value, max_value, DEACTIVATED != activate);
  for (IntVar* const var : vars) {
    assignment_->SetMin(var, max_value);
  }
  // Variable bound
  TestStoredAssignment(max_value, max_value, DEACTIVATED != activate);
}

void AssignmentTest::TestAssignmentFromVarVector(int64_t var_min,
                                                 int64_t var_max,
                                                 ActivateState activate) {
  unique_vars_.clear();
  std::vector<IntVar*> vars;
  for (int i = 0; i < kAssignmentSize; ++i) {
    IntVar* const var = solver_->MakeIntVar(var_min, var_max);
    vars.push_back(var);
    unique_vars_.insert(var);
  }
  assignment_->Add(vars);
  EXPECT_EQ(unique_vars_.size(), assignment_->Size());
  for (int i = 0; i < kAssignmentSize; ++i) {
    Activate(vars[i], activate);
  }
  TestUnstoredAssignment(DEACTIVATED != activate);
  const int64_t min_value = 0;
  const int64_t max_value = 5;
  for (int i = 0; i < kAssignmentSize; ++i) {
    assignment_->SetMin(vars[i], min_value);
  }
  TestStoredAssignment(min_value, kint64max, DEACTIVATED != activate);
  for (int i = 0; i < kAssignmentSize; ++i) {
    assignment_->SetMax(vars[i], max_value);
  }
  TestStoredAssignment(min_value, max_value, DEACTIVATED != activate);
  for (int i = 0; i < kAssignmentSize; ++i) {
    assignment_->SetMin(vars[i], max_value);
  }
  // Variable bound
  TestStoredAssignment(max_value, max_value, DEACTIVATED != activate);
}

void AssignmentTest::TestAssignmentObjective(int64_t obj_min, int64_t obj_max,
                                             ActivateState activate) {
  ASSERT_FALSE(assignment_->HasObjective());
  IntVar* objective = solver_->MakeIntVar(obj_min, obj_max);
  assignment_->AddObjective(objective);
  ActivateObjective(activate);
  ASSERT_TRUE(assignment_->HasObjective());
  const Assignment::IntContainer& container = assignment_->IntVarContainer();
  EXPECT_EQ(0, container.Size());
  EXPECT_EQ(kint64min, assignment_->ObjectiveMin());
  EXPECT_EQ(kint64max, assignment_->ObjectiveMax());
  EXPECT_FALSE(assignment_->ObjectiveBound());
  EXPECT_EQ(DEACTIVATED != activate, assignment_->ActivatedObjective());
  const int64_t min_value = 0;
  const int64_t max_value = 5;
  assignment_->SetObjectiveMin(min_value);
  EXPECT_EQ(min_value, assignment_->ObjectiveMin());
  EXPECT_EQ(kint64max, assignment_->ObjectiveMax());
  EXPECT_FALSE(assignment_->ObjectiveBound());
  assignment_->SetObjectiveMax(max_value);
  EXPECT_EQ(min_value, assignment_->ObjectiveMin());
  EXPECT_EQ(max_value, assignment_->ObjectiveMax());
  EXPECT_FALSE(assignment_->ObjectiveBound());
  assignment_->SetObjectiveMin(max_value);
  EXPECT_EQ(max_value, assignment_->ObjectiveMin());
  EXPECT_EQ(max_value, assignment_->ObjectiveMax());
  EXPECT_TRUE(assignment_->ObjectiveBound());
}

void AssignmentTest::TestAssignmentObjectives(int64_t obj_min, int64_t obj_max,
                                              ActivateState activate) {
  ASSERT_FALSE(assignment_->HasObjective());
  const int kNumObj = 10;
  std::vector<IntVar*> objectives;
  for (int i = 0; i < kNumObj; ++i) {
    ASSERT_FALSE(assignment_->HasObjectiveFromIndex(i));
    objectives.push_back(solver_->MakeIntVar(obj_min, obj_max));
  }
  assignment_->AddObjectives(objectives);
  for (int i = 0; i < kNumObj; ++i) {
    ActivateObjectiveFromIndex(i, activate);
    ASSERT_TRUE(assignment_->HasObjectiveFromIndex(i));
    const Assignment::IntContainer& container = assignment_->IntVarContainer();
    EXPECT_EQ(0, container.Size());
    EXPECT_EQ(kint64min, assignment_->ObjectiveMinFromIndex(i));
    EXPECT_EQ(kint64max, assignment_->ObjectiveMaxFromIndex(i));
    EXPECT_FALSE(assignment_->ObjectiveBoundFromIndex(i));
    EXPECT_EQ(DEACTIVATED != activate,
              assignment_->ActivatedObjectiveFromIndex(i));
    const int64_t min_value = 0;
    const int64_t max_value = 5;
    assignment_->SetObjectiveMinFromIndex(i, min_value);
    EXPECT_EQ(min_value, assignment_->ObjectiveMinFromIndex(i));
    EXPECT_EQ(kint64max, assignment_->ObjectiveMaxFromIndex(i));
    EXPECT_FALSE(assignment_->ObjectiveBoundFromIndex(i));
    assignment_->SetObjectiveMaxFromIndex(i, max_value);
    EXPECT_EQ(min_value, assignment_->ObjectiveMinFromIndex(i));
    EXPECT_EQ(max_value, assignment_->ObjectiveMaxFromIndex(i));
    EXPECT_FALSE(assignment_->ObjectiveBoundFromIndex(i));
    assignment_->SetObjectiveMinFromIndex(i, max_value);
    EXPECT_EQ(max_value, assignment_->ObjectiveMinFromIndex(i));
    EXPECT_EQ(max_value, assignment_->ObjectiveMaxFromIndex(i));
    EXPECT_TRUE(assignment_->ObjectiveBoundFromIndex(i));
  }
}

void AssignmentTest::TestAssignmentObjectiveFromSolver(int64_t obj_min,
                                                       int64_t obj_max,
                                                       ActivateState activate) {
  IntVar* objective = solver_->MakeIntVar(obj_min, obj_max);
  assignment_->AddObjective(objective);
  ActivateObjective(activate);
  ASSERT_TRUE(assignment_->HasObjective());
  DecisionBuilder* db = solver_->MakePhase(
      objective, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  assignment_->Store();
  EXPECT_EQ(obj_min, assignment_->ObjectiveMin());
  EXPECT_EQ(obj_min, assignment_->ObjectiveMax());
  EXPECT_TRUE(assignment_->ObjectiveBound());
  if (obj_min == obj_max) {
    EXPECT_EQ(obj_min, assignment_->ObjectiveValue());
  }
  EXPECT_EQ(DEACTIVATED != activate, assignment_->ActivatedObjective());
  solver_->EndSearch();
}

void AssignmentTest::TestAssignmentObjectivesFromSolver(
    int64_t obj_min, int64_t obj_max, ActivateState activate) {
  const int kNumObj = 10;
  std::vector<IntVar*> objectives;
  for (int i = 0; i < kNumObj; ++i) {
    objectives.push_back(solver_->MakeIntVar(obj_min, obj_max));
  }
  assignment_->AddObjectives(objectives);
  for (int i = 0; i < kNumObj; ++i) {
    ActivateObjectiveFromIndex(i, activate);
    ASSERT_TRUE(assignment_->HasObjectiveFromIndex(i));
  }
  DecisionBuilder* db = solver_->MakePhase(
      objectives, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  assignment_->Store();
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(obj_min, assignment_->ObjectiveMinFromIndex(i));
    EXPECT_EQ(obj_min, assignment_->ObjectiveMaxFromIndex(i));
    EXPECT_TRUE(assignment_->ObjectiveBoundFromIndex(i));
    if (obj_min == obj_max) {
      EXPECT_EQ(obj_min, assignment_->ObjectiveValueFromIndex(i));
    }
    EXPECT_EQ(DEACTIVATED != activate,
              assignment_->ActivatedObjectiveFromIndex(i));
  }
  solver_->EndSearch();
}

void AssignmentTest::TestRestoreAssignment(int64_t var_min, int64_t var_max,
                                           ActivateState activate) {
  std::vector<IntVar*> vars;
  MakeVars(var_min, var_max, activate, &vars);
  for (int i = 0; i < kAssignmentSize; ++i) {
    assignment_->SetValue(vars[i], var_min);
  }
  DecisionBuilder* const restore =
      solver_->MakeRestoreAssignment(assignment_.get());
  solver_->NewSearch(restore);
  ASSERT_TRUE(solver_->NextSolution());
  const Assignment::IntContainer& container = assignment_->IntVarContainer();
  for (int i = 0; i < container.Size(); ++i) {
    const IntVar* const var = container.Element(i).Var();
    if (DEACTIVATED != activate) {
      EXPECT_EQ(var_min, var->Min());
      EXPECT_EQ(var_min, var->Max());
      EXPECT_TRUE(var->Bound());
      EXPECT_EQ(var_min, var->Value());
    } else {
      EXPECT_EQ(var_min, var->Min());
      EXPECT_EQ(var_max, var->Max());
    }
  }
  solver_->EndSearch();
}

void AssignmentTest::TestRestoreObjective(int64_t obj_min, int64_t obj_max,
                                          ActivateState activate) {
  IntVar* objective = solver_->MakeIntVar(obj_min, obj_max);
  assignment_->AddObjective(objective);
  ActivateObjective(activate);
  ASSERT_TRUE(assignment_->HasObjective());
  assignment_->SetObjectiveValue(obj_min);
  DecisionBuilder* restore = solver_->MakeRestoreAssignment(assignment_.get());
  solver_->NewSearch(restore);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(obj_min, objective->Min());
  EXPECT_EQ(obj_max, objective->Max());
  solver_->EndSearch();
}

void AssignmentTest::TestRestoreObjectives(int64_t obj_min, int64_t obj_max,
                                           ActivateState activate) {
  const int kNumObj = 10;
  std::vector<IntVar*> objectives;
  for (int i = 0; i < kNumObj; ++i) {
    objectives.push_back(solver_->MakeIntVar(obj_min, obj_max));
  }
  assignment_->AddObjectives(objectives);
  for (int i = 0; i < kNumObj; ++i) {
    ActivateObjectiveFromIndex(i, activate);
    ASSERT_TRUE(assignment_->HasObjectiveFromIndex(i));
    assignment_->SetObjectiveValueFromIndex(i, obj_min);
  }
  DecisionBuilder* restore = solver_->MakeRestoreAssignment(assignment_.get());
  solver_->NewSearch(restore);
  ASSERT_TRUE(solver_->NextSolution());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(obj_min, objectives[i]->Min());
    EXPECT_EQ(obj_max, objectives[i]->Max());
  }
  solver_->EndSearch();
}

void AssignmentTest::TestStoredAssignment(int64_t min_value, int64_t max_value,
                                          bool activated) {
  const Assignment::IntContainer& container = assignment_->IntVarContainer();
  EXPECT_EQ(unique_vars_.size(), container.Size());
  for (int i = 0; i < container.Size(); ++i) {
    const IntVar* const var = container.Element(i).Var();
    EXPECT_EQ(min_value, assignment_->Min(var));
    EXPECT_EQ(max_value, assignment_->Max(var));
    EXPECT_EQ(min_value == max_value, assignment_->Bound(var));
    if (min_value == max_value) {
      EXPECT_EQ(min_value, assignment_->Value(var));
    }
    EXPECT_EQ(activated, assignment_->Activated(var));
  }
}

void AssignmentTest::TestUnstoredAssignment(bool activated) {
  const Assignment::IntContainer& container = assignment_->IntVarContainer();
  EXPECT_EQ(unique_vars_.size(), container.Size());
  for (int i = 0; i < container.Size(); ++i) {
    const IntVar* const var = container.Element(i).Var();
    EXPECT_EQ(kint64min, assignment_->Min(var));
    EXPECT_EQ(kint64max, assignment_->Max(var));
    EXPECT_FALSE(assignment_->Bound(var));
    EXPECT_EQ(activated, assignment_->Activated(var));
  }
}

TEST_F(AssignmentTest, AssignmentFromConst) {
  TestAssignmentFromVar(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentFromBoolVar) {
  TestAssignmentFromVar(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentFromIntVar) {
  TestAssignmentFromVar(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedFromConst) {
  TestAssignmentFromVar(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedFromBoolVar) {
  TestAssignmentFromVar(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedFromIntVar) {
  TestAssignmentFromVar(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromConst) {
  TestAssignmentFromVar(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromBoolVar) {
  TestAssignmentFromVar(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromIntVar) {
  TestAssignmentFromVar(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentFromConstVector) {
  TestAssignmentFromVarVector(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentFromBoolVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentFromIntVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedFromConstVector) {
  TestAssignmentFromVarVector(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedFromBoolVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedFromIntVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromConstVector) {
  TestAssignmentFromVarVector(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromBoolVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedFromIntVarVector) {
  TestAssignmentFromVarVector(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstFromSolver) {
  TestAssignmentFromSolver(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentIntVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstFromSolver) {
  TestAssignmentFromSolver(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedIntVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstFromSolver) {
  TestAssignmentFromSolver(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedIntVarFromSolver) {
  TestAssignmentFromSolver(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstFromDecision) {
  TestAssignmentFromDecision(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentIntVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstFromDecision) {
  TestAssignmentFromDecision(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedIntVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstFromDecision) {
  TestAssignmentFromDecision(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedIntVarFromDecision) {
  TestAssignmentFromDecision(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentRestoreConst) {
  TestRestoreAssignment(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolVarRestore) {
  TestRestoreAssignment(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentIntVarRestore) {
  TestRestoreAssignment(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstRestore) {
  TestRestoreAssignment(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolVarRestore) {
  TestRestoreAssignment(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedIntVarRestore) {
  TestRestoreAssignment(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedRestore) {
  TestRestoreAssignment(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolVarRestore) {
  TestRestoreAssignment(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedIntVarRestore) {
  TestRestoreAssignment(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjective) {
  TestAssignmentObjective(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjective) {
  TestAssignmentObjective(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjective) {
  TestAssignmentObjective(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjective) {
  TestAssignmentObjective(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjective) {
  TestAssignmentObjective(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjective) {
  TestAssignmentObjective(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjective) {
  TestAssignmentObjective(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjective) {
  TestAssignmentObjective(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjective) {
  TestAssignmentObjective(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjectives) {
  TestAssignmentObjectives(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjectives) {
  TestAssignmentObjectives(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjectives) {
  TestAssignmentObjectives(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjectives) {
  TestAssignmentObjectives(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjectives) {
  TestAssignmentObjectives(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjectives) {
  TestAssignmentObjectives(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjectives) {
  TestAssignmentObjectives(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjectives) {
  TestAssignmentObjectives(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjectives) {
  TestAssignmentObjectives(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjectiveFromSolver) {
  TestAssignmentObjectiveFromSolver(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjectivesFromSolver) {
  TestAssignmentObjectivesFromSolver(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjectiveRestore) {
  TestRestoreObjective(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjectiveRestore) {
  TestRestoreObjective(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjectiveRestore) {
  TestRestoreObjective(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjectiveRestore) {
  TestRestoreObjective(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentConstObjectivesRestore) {
  TestRestoreObjectives(int64_t{2}, int64_t{2}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentBoolObjectivesRestore) {
  TestRestoreObjectives(int64_t{0}, int64_t{1}, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentObjectivesRestore) {
  TestRestoreObjectives(int64_t{0}, kAssignmentSize, DEFAULT);
}

TEST_F(AssignmentTest, AssignmentActivatedConstObjectivesRestore) {
  TestRestoreObjectives(int64_t{2}, int64_t{2}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedBoolObjectiveRestores) {
  TestRestoreObjectives(int64_t{0}, int64_t{1}, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentActivatedObjectivesRestore) {
  TestRestoreObjectives(int64_t{0}, kAssignmentSize, ACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedConstObjectivesRestore) {
  TestRestoreObjectives(int64_t{2}, int64_t{2}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedBoolObjectivesRestore) {
  TestRestoreObjectives(int64_t{0}, int64_t{1}, DEACTIVATED);
}

TEST_F(AssignmentTest, AssignmentDeactivatedObjectivesRestore) {
  TestRestoreObjectives(int64_t{0}, kAssignmentSize, DEACTIVATED);
}

void CheckAssignment(const Assignment& assignment,
                     const std::vector<IntVar*>& variables,
                     absl::Span<const int64_t> min_values,
                     absl::Span<const int64_t> max_values,
                     absl::Span<const bool> is_active,
                     const IntVar* objective_variable,
                     int64_t min_objective_value, int64_t max_objective_value) {
  for (int i = 0; i < variables.size(); ++i) {
    IntVar* var = variables[i];
    EXPECT_TRUE(assignment.Contains(var));
    EXPECT_EQ(assignment.Min(var), min_values[i]);
    EXPECT_EQ(assignment.Max(var), max_values[i]);
    EXPECT_EQ(assignment.Activated(var), is_active[i]);
  }
  EXPECT_EQ(assignment.IntVarContainer().Size(), variables.size());

  EXPECT_EQ(assignment.Objective(), objective_variable);
  EXPECT_EQ(assignment.ObjectiveMin(), min_objective_value);
  EXPECT_EQ(assignment.ObjectiveMax(), max_objective_value);
}

TEST_F(AssignmentTest, CopyEmptyToEmpty) {
  Assignment copy(solver_.get());
  copy.Copy(assignment_.get());
  CheckAssignment(copy, {}, {}, {}, {}, nullptr, 0, 0);
  copy.CopyIntersection(assignment_.get());
  CheckAssignment(copy, {}, {}, {}, {}, nullptr, 0, 0);
}

TEST_F(AssignmentTest, CopyEmptyToNonEmpty) {
  Assignment copy(solver_.get());
  IntVar* v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* v2 = solver_->MakeIntVar(2, 3, "v2");
  copy.Add(std::vector<IntVar*>({v1, v2}));
  IntVar* objective_var = solver_->MakeIntVar(0, 5, "objective");
  copy.AddObjective(objective_var);
  copy.Copy(assignment_.get());
  CheckAssignment(copy, {}, {}, {}, {}, nullptr, 0, 0);
  copy.Clear();
  copy.Add(std::vector<IntVar*>({v1, v2}));
  copy.AddObjective(objective_var);
  copy.CopyIntersection(assignment_.get());
  CheckAssignment(copy, {v1, v2}, {kint64min, kint64min},
                  {kint64max, kint64max}, {true, true}, objective_var,
                  kint64min, kint64max);
}

TEST_F(AssignmentTest, CopyNonEmptyToEmpty) {
  Assignment copy(solver_.get());
  IntVar* v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* v2 = solver_->MakeIntVar(2, 3, "v2");
  assignment_->Add(std::vector<IntVar*>({v1, v2}));
  IntVar* objective_var = solver_->MakeIntVar(0, 5, "objective");
  assignment_->AddObjective(objective_var);
  copy.Copy(assignment_.get());
  CheckAssignment(copy, {v1, v2}, {kint64min, kint64min},
                  {kint64max, kint64max}, {true, true}, objective_var,
                  kint64min, kint64max);
  copy.Clear();
  copy.CopyIntersection(assignment_.get());
  CheckAssignment(copy, {}, {}, {}, {}, nullptr, 0, 0);
}

TEST_F(AssignmentTest, CopyIntersecting) {
  Assignment copy(solver_.get());
  IntVar* v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* v2 = solver_->MakeIntVar(2, 3, "v2");
  IntVar* v3 = solver_->MakeIntVar(4, 5, "v3");
  assignment_->Add(std::vector<IntVar*>({v1, v2}));
  assignment_->SetValue(v1, 10);
  assignment_->Deactivate(v1);
  IntVar* objective_var = solver_->MakeIntVar(0, 5, "objective");
  assignment_->AddObjective(objective_var);
  assignment_->SetObjectiveValue(15);
  copy.Add(std::vector<IntVar*>({v1, v3}));
  copy.Copy(assignment_.get());
  CheckAssignment(copy, {v1, v2}, {10, kint64min}, {10, kint64max},
                  {false, true}, objective_var, 15, 15);
  copy.Clear();
  copy.Add(std::vector<IntVar*>({v1, v3}));
  copy.AddObjective(objective_var);
  copy.CopyIntersection(assignment_.get());
  CheckAssignment(copy, {v1, v3}, {10, kint64min}, {10, kint64max},
                  {false, true}, objective_var, 15, 15);
}

TEST_F(AssignmentTest, CopyNonIntersecting) {
  Assignment copy(solver_.get());
  IntVar* v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* v2 = solver_->MakeIntVar(2, 3, "v2");
  IntVar* v3 = solver_->MakeIntVar(4, 5, "v3");
  assignment_->Add(std::vector<IntVar*>({v1, v2}));
  assignment_->SetValue(v1, 10);
  assignment_->Deactivate(v1);
  IntVar* objective_var1 = solver_->MakeIntVar(0, 5, "objective1");
  assignment_->AddObjective(objective_var1);
  assignment_->SetObjectiveValue(15);
  IntVar* objective_var2 = solver_->MakeIntVar(0, 5, "objective2");
  copy.AddObjective(objective_var2);
  copy.Add(v3);
  copy.Copy(assignment_.get());
  CheckAssignment(copy, {v1, v2}, {10, kint64min}, {10, kint64max},
                  {false, true}, objective_var1, 15, 15);
  copy.Clear();
  copy.Add(v3);
  copy.AddObjective(objective_var2);
  copy.SetObjectiveValue(25);
  copy.CopyIntersection(assignment_.get());
  CheckAssignment(copy, {v3}, {kint64min}, {kint64max}, {true}, objective_var2,
                  25, 25);
}

TEST_F(AssignmentTest, ContainerResizeAndFill) {
  IntVar* const v1 = solver_->MakeIntVar(0, 1, "v1");
  IntVar* const v2 = solver_->MakeIntVar(2, 3, "v2");
  assignment_->Add(v1);
  Assignment::IntContainer* const int_container =
      assignment_->MutableIntVarContainer();
  const int kNewSize = 10;
  int_container->Resize(kNewSize);
  EXPECT_EQ(kNewSize, int_container->Size());
  EXPECT_EQ(v1, int_container->Element(0).Var());
  for (int i = 1; i < kNewSize; ++i) {
    EXPECT_EQ(nullptr, int_container->Element(i).Var());
  }
  int_container->AddAtPosition(v2, kNewSize - 1);
  EXPECT_EQ(v2, int_container->Element(kNewSize - 1).Var());
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  assignment_->Add(t1);
  Assignment::IntervalContainer* const interval_container =
      assignment_->MutableIntervalVarContainer();
  interval_container->Resize(kNewSize);
  EXPECT_EQ(kNewSize, interval_container->Size());
  EXPECT_EQ(t1, interval_container->Element(0).Var());
  for (int i = 1; i < kNewSize; ++i) {
    EXPECT_EQ(nullptr, interval_container->Element(i).Var());
  }
  interval_container->AddAtPosition(t2, kNewSize - 1);
  EXPECT_EQ(t2, interval_container->Element(kNewSize - 1).Var());
  std::vector<IntervalVar*> vt1 = {t1};
  std::vector<IntervalVar*> vt2 = {t2};
  SequenceVar* const s1 =
      solver_->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  SequenceVar* const s2 =
      solver_->MakeDisjunctiveConstraint(vt2, "s2")->MakeSequenceVar();
  assignment_->Add(s1);
  Assignment::SequenceContainer* const sequence_container =
      assignment_->MutableSequenceVarContainer();
  sequence_container->Resize(kNewSize);
  EXPECT_EQ(kNewSize, sequence_container->Size());
  EXPECT_EQ(s1, sequence_container->Element(0).Var());
  for (int i = 1; i < kNewSize; ++i) {
    EXPECT_EQ(nullptr, sequence_container->Element(i).Var());
  }
  sequence_container->AddAtPosition(s2, kNewSize - 1);
  EXPECT_EQ(s2, sequence_container->Element(kNewSize - 1).Var());
}

TEST_F(AssignmentTest, IntervalVarAdder) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t5");
  std::vector<IntervalVar*> vt;
  vt.push_back(t2);
  vt.push_back(t3);
  assignment_->Add(t1);
  assignment_->Add(vt);
  EXPECT_EQ(3, assignment_->Size());
}

TEST_F(AssignmentTest, IntervalVarElementGetter) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  std::vector<IntervalVar*> vt;
  vt.push_back(t1);
  vt.push_back(t2);
  vt.push_back(t3);
  vt.push_back(t4);
  assignment_->Add(vt);
  assignment_->Deactivate(t4);
  assignment_->Store();
  EXPECT_EQ(0, assignment_->StartMin(t1));
  EXPECT_EQ(10, assignment_->StartMax(t1));
  EXPECT_EQ(5, assignment_->DurationMin(t1));
  EXPECT_EQ(5, assignment_->DurationMax(t1));
  EXPECT_EQ(5, assignment_->EndMin(t1));
  EXPECT_EQ(15, assignment_->EndMax(t1));
  EXPECT_EQ(0, assignment_->PerformedMin(t1));
  EXPECT_EQ(1, assignment_->PerformedMax(t1));
  EXPECT_EQ(1, assignment_->PerformedMin(t3));
  EXPECT_EQ(1, assignment_->PerformedMax(t3));
  EXPECT_TRUE(assignment_->Activated(t1));
  EXPECT_FALSE(assignment_->Activated(t4));
}

TEST_F(AssignmentTest, IntervalVarElementStartSetter) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t4");
  std::vector<IntervalVar*> vt;
  vt.push_back(t1);
  vt.push_back(t2);
  vt.push_back(t3);
  vt.push_back(t4);
  assignment_->Add(vt);
  assignment_->SetStartMin(t1, 3);
  assignment_->SetStartMax(t2, 7);
  assignment_->SetStartRange(t3, 2, 6);
  assignment_->SetStartValue(t4, 4);
  assignment_->Restore();
  EXPECT_EQ(3, t1->StartMin());
  EXPECT_EQ(10, t1->StartMax());
  EXPECT_EQ(0, t2->StartMin());
  EXPECT_EQ(7, t2->StartMax());
  EXPECT_EQ(2, t3->StartMin());
  EXPECT_EQ(6, t3->StartMax());
  EXPECT_EQ(4, t4->StartMin());
  EXPECT_EQ(4, t4->StartMax());
}

TEST_F(AssignmentTest, IntervalVarElementDurationSetter) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t4");
  std::vector<IntervalVar*> vt;
  vt.push_back(t1);
  vt.push_back(t2);
  vt.push_back(t3);
  vt.push_back(t4);
  assignment_->Add(vt);
  assignment_->SetDurationMin(t1, 4);
  assignment_->SetDurationMax(t2, 10);
  assignment_->SetDurationRange(t3, 3, 11);
  assignment_->SetDurationValue(t4, 5);
  assignment_->Restore();
  EXPECT_EQ(5, t1->DurationMin());
  EXPECT_EQ(5, t1->DurationMax());
  EXPECT_EQ(5, t2->DurationMin());
  EXPECT_EQ(5, t2->DurationMax());
  EXPECT_EQ(5, t3->DurationMin());
  EXPECT_EQ(5, t3->DurationMax());
  EXPECT_EQ(5, t4->DurationMin());
  EXPECT_EQ(5, t4->DurationMax());
}

TEST_F(AssignmentTest, IntervalVarElementEndSetter) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t4");
  std::vector<IntervalVar*> vt;
  vt.push_back(t1);
  vt.push_back(t2);
  vt.push_back(t3);
  vt.push_back(t4);
  assignment_->Add(vt);
  assignment_->SetEndMin(t1, 8);
  assignment_->SetEndMax(t2, 12);
  assignment_->SetEndRange(t3, 7, 11);
  assignment_->SetEndValue(t4, 9);
  assignment_->Restore();
  EXPECT_EQ(8, t1->EndMin());
  EXPECT_EQ(15, t1->EndMax());
  EXPECT_EQ(5, t2->EndMin());
  EXPECT_EQ(12, t2->EndMax());
  EXPECT_EQ(7, t3->EndMin());
  EXPECT_EQ(11, t3->EndMax());
  EXPECT_EQ(9, t4->EndMin());
  EXPECT_EQ(9, t4->EndMax());
}

TEST_F(AssignmentTest, IntervalVarElementPerformedSetter) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  std::vector<IntervalVar*> vt;
  vt.push_back(t1);
  vt.push_back(t2);
  vt.push_back(t3);
  vt.push_back(t4);
  assignment_->Add(vt);
  assignment_->SetPerformedMin(t1, 1);
  assignment_->SetPerformedMax(t2, 0);
  assignment_->SetPerformedRange(t3, 1, 1);
  assignment_->SetPerformedValue(t4, 0);
  assignment_->Restore();
  EXPECT_TRUE(t1->MustBePerformed());
  EXPECT_TRUE(t1->MayBePerformed());
  EXPECT_FALSE(t2->MustBePerformed());
  EXPECT_FALSE(t2->MayBePerformed());
  EXPECT_TRUE(t3->MustBePerformed());
  EXPECT_TRUE(t3->MayBePerformed());
  EXPECT_FALSE(t4->MustBePerformed());
  EXPECT_FALSE(t4->MayBePerformed());
}

// ----- Sequence Support -----

TEST_F(AssignmentTest, SequenceVarAdder) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  IntervalVar* const t5 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t5");
  IntervalVar* const t6 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t6");
  std::vector<IntervalVar*> vt1;
  vt1.push_back(t4);
  vt1.push_back(t5);
  std::vector<IntervalVar*> vt2;
  vt2.push_back(t2);
  vt2.push_back(t3);
  std::vector<IntervalVar*> vt3;
  vt3.push_back(t1);
  std::vector<IntervalVar*> vt4;
  vt4.push_back(t6);
  SequenceVar* const s1 =
      solver_->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  SequenceVar* const s2 =
      solver_->MakeDisjunctiveConstraint(vt2, "s2")->MakeSequenceVar();
  SequenceVar* const s3 =
      solver_->MakeDisjunctiveConstraint(vt3, "s3")->MakeSequenceVar();
  SequenceVar* const s4 =
      solver_->MakeDisjunctiveConstraint(vt4, "s4")->MakeSequenceVar();
  std::vector<SequenceVar*> sequence_vector;
  sequence_vector.push_back(s2);
  sequence_vector.push_back(s3);
  sequence_vector.push_back(s4);
  assignment_->Add(s1);
  assignment_->Add(sequence_vector);
  EXPECT_EQ(4, assignment_->Size());
}

TEST_F(AssignmentTest, SequenceVarElementGetterForward) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  IntervalVar* const t5 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t5");
  IntervalVar* const t6 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t6");
  std::vector<IntervalVar*> vt1;
  vt1.push_back(t4);
  vt1.push_back(t5);
  vt1.push_back(t6);
  std::vector<IntervalVar*> vt2;
  vt2.push_back(t1);
  vt2.push_back(t2);
  vt2.push_back(t3);

  SequenceVar* const s1 =
      solver_->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  s1->RankFirst(2);
  s1->RankFirst(0);
  s1->RankFirst(1);
  SequenceVar* const s2 =
      solver_->MakeDisjunctiveConstraint(vt2, "s2")->MakeSequenceVar();
  s2->RankFirst(1);

  assignment_->Add(s1);
  assignment_->Add(s2);
  assignment_->Deactivate(s2);
  assignment_->Store();
  const std::vector<int>& iv1 = assignment_->ForwardSequence(s1);
  EXPECT_EQ(3, iv1.size());
  EXPECT_EQ(2, iv1[0]);
  EXPECT_EQ(0, iv1[1]);
  EXPECT_EQ(1, iv1[2]);
  const std::vector<int>& iv2 = assignment_->ForwardSequence(s2);
  EXPECT_EQ(1, iv2.size());
  EXPECT_EQ(1, iv2[0]);
  EXPECT_EQ(0, assignment_->BackwardSequence(s1).size());
  EXPECT_EQ(0, assignment_->BackwardSequence(s2).size());
  EXPECT_EQ(0, assignment_->Unperformed(s1).size());
  EXPECT_EQ(0, assignment_->Unperformed(s2).size());
  EXPECT_TRUE(assignment_->Activated(s1));
  EXPECT_FALSE(assignment_->Activated(s2));
}

TEST_IN_PROPAGATION0(AssignmentTest, SequenceVarElementGetterBackward, solver,
                     true) {
  IntervalVar* const t1 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "t4");
  IntervalVar* const t5 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "t5");
  IntervalVar* const t6 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "t6");
  std::vector<IntervalVar*> vt1;
  vt1.push_back(t4);
  vt1.push_back(t5);
  vt1.push_back(t6);
  std::vector<IntervalVar*> vt2;
  vt2.push_back(t1);
  vt2.push_back(t2);
  vt2.push_back(t3);

  SequenceVar* const s1 =
      solver->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  s1->RankLast(2);
  s1->RankLast(0);
  s1->RankLast(1);
  SequenceVar* const s2 =
      solver->MakeDisjunctiveConstraint(vt2, "s2")->MakeSequenceVar();
  s2->RankLast(1);

  Assignment* const assignment = solver->MakeAssignment();

  assignment->Add(s1);
  assignment->Add(s2);
  assignment->Deactivate(s2);
  assignment->Store();
  const std::vector<int>& iv1 = assignment->BackwardSequence(s1);
  EXPECT_EQ(0, iv1.size());
  const std::vector<int>& iv2 = assignment->BackwardSequence(s2);
  EXPECT_EQ(1, iv2.size());
  EXPECT_EQ(1, iv2[0]);
  const std::vector<int>& iv3 = assignment->ForwardSequence(s1);
  EXPECT_EQ(3, iv3.size());
  EXPECT_EQ(2, iv3[2]);
  EXPECT_EQ(0, iv3[1]);
  EXPECT_EQ(1, iv3[0]);
  EXPECT_EQ(0, assignment->ForwardSequence(s2).size());
  EXPECT_EQ(0, assignment->Unperformed(s1).size());
  EXPECT_EQ(0, assignment->Unperformed(s2).size());
  EXPECT_TRUE(assignment->Activated(s1));
  EXPECT_FALSE(assignment->Activated(s2));
  return nullptr;
}

TEST_F(AssignmentTest, SequenceVarSequenceSetterForward) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t4");
  std::vector<IntervalVar*> vt1;
  vt1.push_back(t1);
  vt1.push_back(t2);
  vt1.push_back(t3);
  vt1.push_back(t4);

  SequenceVar* const s1 =
      solver_->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  assignment_->Add(s1);
  std::vector<int> forward_sequence;
  forward_sequence.push_back(2);
  forward_sequence.push_back(0);
  std::vector<int> backward_sequence;
  std::vector<int> unperformed;
  assignment_->SetSequence(s1, forward_sequence, backward_sequence,
                           unperformed);
  assignment_->Restore();
  int ranked = 0;
  int not_ranked = 0;
  int un_performed = 0;
  s1->ComputeStatistics(&ranked, &not_ranked, &un_performed);
  EXPECT_EQ(2, ranked);
  EXPECT_EQ(2, not_ranked);
}

TEST_F(AssignmentTest, SequenceVarSequenceSetterBackward) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t3");
  IntervalVar* const t4 =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, false, "t4");
  std::vector<IntervalVar*> vt1;
  vt1.push_back(t1);
  vt1.push_back(t2);
  vt1.push_back(t3);
  vt1.push_back(t4);

  SequenceVar* const s1 =
      solver_->MakeDisjunctiveConstraint(vt1, "s1")->MakeSequenceVar();
  assignment_->Add(s1);
  std::vector<int> forward_sequence;
  std::vector<int> backward_sequence;
  std::vector<int> unperformed;
  backward_sequence.push_back(2);
  backward_sequence.push_back(0);
  assignment_->SetSequence(s1, forward_sequence, backward_sequence,
                           unperformed);
  assignment_->Restore();
  int ranked = 0;
  int not_ranked = 0;
  int un_performed = 0;
  s1->ComputeStatistics(&ranked, &not_ranked, &un_performed);
  EXPECT_EQ(2, ranked);
  EXPECT_EQ(2, not_ranked);
}

// ----- IO Support -----

TEST_F(AssignmentTest, SaveLoad) {
  const std::string filename =
      file::JoinPath(::testing::TempDir(), "assignment_test_temp.bin");
  std::vector<IntVar*> int_vars;
  int_vars.push_back(solver_->MakeIntVar(0, 1, "var0"));
  int_vars.push_back(solver_->MakeIntVar(0, 1, "var1"));
  assignment_->Add(int_vars);
  assignment_->SetValue(int_vars[0], 1);
  assignment_->SetRange(int_vars[1], 0, 1);
  assignment_->Deactivate(int_vars[1]);
  std::vector<IntervalVar*> interval_vars;
  interval_vars.push_back(
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t0"));
  interval_vars.push_back(
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "t1"));
  assignment_->Add(interval_vars);
  EXPECT_EQ(4, assignment_->Size());
  assignment_->SetStartValue(interval_vars[0], 0);
  assignment_->SetDurationValue(interval_vars[0], 5);
  assignment_->SetEndValue(interval_vars[0], 5);
  assignment_->SetPerformedValue(interval_vars[0], 1);
  assignment_->SetStartRange(interval_vars[1], 0, 10);
  assignment_->SetDurationRange(interval_vars[1], 0, 5);
  assignment_->SetEndRange(interval_vars[1], 0, 15);
  assignment_->SetPerformedRange(interval_vars[1], 0, 1);
  assignment_->Deactivate(interval_vars[1]);
  EXPECT_TRUE(assignment_->Save(filename));
  auto assignment = std::make_unique<Assignment>(solver_.get());
  assignment->Add(int_vars);
  assignment->Add(interval_vars);
  EXPECT_EQ(4, assignment_->Size());
  EXPECT_TRUE(assignment->Load(filename));
  EXPECT_EQ(1, assignment->Value(int_vars[0]));
  EXPECT_TRUE(assignment->Activated(int_vars[0]));
  EXPECT_EQ(0, assignment->Min(int_vars[1]));
  EXPECT_EQ(1, assignment->Max(int_vars[1]));
  EXPECT_FALSE(assignment->Activated(int_vars[1]));
  EXPECT_EQ(0, assignment->StartMin(interval_vars[0]));
  EXPECT_EQ(0, assignment->StartMax(interval_vars[0]));
  EXPECT_EQ(5, assignment->DurationMin(interval_vars[0]));
  EXPECT_EQ(5, assignment->DurationMax(interval_vars[0]));
  EXPECT_EQ(5, assignment->EndMin(interval_vars[0]));
  EXPECT_EQ(5, assignment->EndMax(interval_vars[0]));
  EXPECT_EQ(1, assignment->PerformedMin(interval_vars[0]));
  EXPECT_EQ(1, assignment->PerformedMax(interval_vars[0]));
  EXPECT_TRUE(assignment->Activated(interval_vars[0]));
  EXPECT_EQ(0, assignment->StartMin(interval_vars[1]));
  EXPECT_EQ(10, assignment->StartMax(interval_vars[1]));
  EXPECT_EQ(0, assignment->DurationMin(interval_vars[1]));
  EXPECT_EQ(5, assignment->DurationMax(interval_vars[1]));
  EXPECT_EQ(0, assignment->EndMin(interval_vars[1]));
  EXPECT_EQ(15, assignment->EndMax(interval_vars[1]));
  EXPECT_EQ(0, assignment->PerformedMin(interval_vars[1]));
  EXPECT_EQ(1, assignment->PerformedMax(interval_vars[1]));
  EXPECT_FALSE(assignment->Activated(interval_vars[1]));
  auto mixed_assignment = std::make_unique<Assignment>(solver_.get());
  for (int i = int_vars.size() - 1; i >= 0; --i) {
    mixed_assignment->Add(int_vars[i]);
  }
  for (int i = interval_vars.size() - 1; i >= 0; --i) {
    mixed_assignment->Add(interval_vars[i]);
  }
  EXPECT_TRUE(mixed_assignment->Load(filename));
  EXPECT_EQ(1, mixed_assignment->Value(int_vars[0]));
  EXPECT_TRUE(mixed_assignment->Activated(int_vars[0]));
  EXPECT_EQ(0, mixed_assignment->Min(int_vars[1]));
  EXPECT_EQ(1, mixed_assignment->Max(int_vars[1]));
  EXPECT_FALSE(mixed_assignment->Activated(int_vars[1]));
  EXPECT_EQ(0, mixed_assignment->StartMin(interval_vars[0]));
  EXPECT_EQ(0, mixed_assignment->StartMax(interval_vars[0]));
  EXPECT_EQ(5, mixed_assignment->DurationMin(interval_vars[0]));
  EXPECT_EQ(5, mixed_assignment->DurationMax(interval_vars[0]));
  EXPECT_EQ(5, mixed_assignment->EndMin(interval_vars[0]));
  EXPECT_EQ(5, mixed_assignment->EndMax(interval_vars[0]));
  EXPECT_EQ(1, mixed_assignment->PerformedMin(interval_vars[0]));
  EXPECT_EQ(1, mixed_assignment->PerformedMax(interval_vars[0]));
  EXPECT_TRUE(mixed_assignment->Activated(interval_vars[0]));
  EXPECT_EQ(0, mixed_assignment->StartMin(interval_vars[1]));
  EXPECT_EQ(10, mixed_assignment->StartMax(interval_vars[1]));
  EXPECT_EQ(0, mixed_assignment->DurationMin(interval_vars[1]));
  EXPECT_EQ(5, mixed_assignment->DurationMax(interval_vars[1]));
  EXPECT_EQ(0, mixed_assignment->EndMin(interval_vars[1]));
  EXPECT_EQ(15, mixed_assignment->EndMax(interval_vars[1]));
  EXPECT_EQ(0, mixed_assignment->PerformedMin(interval_vars[1]));
  EXPECT_EQ(1, mixed_assignment->PerformedMax(interval_vars[1]));
  EXPECT_FALSE(mixed_assignment->Activated(interval_vars[1]));
}

TEST_F(AssignmentTest, IntervalVarCopy) {
  IntervalVar* const t1 =
      solver_->MakeFixedDurationIntervalVar(0, 1, 2, true, "t1");
  IntervalVar* const t2 =
      solver_->MakeFixedDurationIntervalVar(0, 3, 4, false, "t2");
  IntervalVar* const t3 =
      solver_->MakeFixedDurationIntervalVar(0, 5, 6, true, "t3");
  assignment_->Add(t1);
  assignment_->Add(t2);
  assignment_->Deactivate(t2);
  EXPECT_EQ(2, assignment_->Size());
  std::unique_ptr<Assignment> copy(new Assignment(solver_.get()));
  copy->Add(t1);
  copy->Add(t2);
  copy->Add(t3);
  EXPECT_EQ(3, copy->Size());
  assignment_->SetStartValue(t1, 10);
  assignment_->SetPerformedValue(t1, 1);
  assignment_->SetStartValue(t2, 20);
  assignment_->SetPerformedValue(t2, 0);
  EXPECT_EQ(copy->StartMin(t1), kint64min);
  EXPECT_EQ(copy->StartMax(t1), kint64max);
  EXPECT_EQ(copy->PerformedMin(t1), 0);
  EXPECT_EQ(copy->PerformedMax(t1), 1);
  EXPECT_TRUE(copy->Activated(t1));
  EXPECT_EQ(copy->StartMin(t2), kint64min);
  EXPECT_EQ(copy->StartMax(t2), kint64max);
  EXPECT_EQ(copy->PerformedMin(t2), 0);
  EXPECT_EQ(copy->PerformedMax(t2), 1);
  EXPECT_TRUE(copy->Activated(t2));
  EXPECT_EQ(copy->StartMin(t3), kint64min);
  EXPECT_EQ(copy->StartMax(t3), kint64max);
  EXPECT_EQ(copy->PerformedMin(t3), 0);
  EXPECT_EQ(copy->PerformedMax(t3), 1);
  EXPECT_TRUE(copy->Activated(t3));
  copy->CopyIntersection(assignment_.get());
  EXPECT_EQ(copy->StartMin(t1), 10);
  EXPECT_EQ(copy->StartMax(t1), 10);
  EXPECT_EQ(copy->PerformedMin(t1), 1);
  EXPECT_EQ(copy->PerformedMax(t1), 1);
  EXPECT_TRUE(copy->Activated(t1));
  EXPECT_EQ(copy->StartMin(t2), 20);
  EXPECT_EQ(copy->StartMax(t2), 20);
  EXPECT_EQ(copy->PerformedMin(t2), 0);
  EXPECT_EQ(copy->PerformedMax(t2), 0);
  EXPECT_FALSE(copy->Activated(t2));
  EXPECT_EQ(copy->StartMin(t3), kint64min);
  EXPECT_EQ(copy->StartMax(t3), kint64max);
  EXPECT_EQ(copy->PerformedMin(t3), 0);
  EXPECT_EQ(copy->PerformedMax(t3), 1);
  EXPECT_TRUE(copy->Activated(t3));
}

TEST_F(AssignmentTest, EqualityAndLogging) {
  std::vector<IntVar*> variables;
  const int kNumVars = 3;
  const int kMin = 2;
  const int kMax = 9;
  solver_->MakeIntVarArray(kNumVars, kMin, kMax, "v", &variables);
  const Assignment empty(solver_.get());
  EXPECT_EQ(empty, empty);
  Assignment assignment(solver_.get());
  // Adds a variable: initially, it maps to an open domain
  assignment.Add(variables[0]);
  EXPECT_EQ(assignment, assignment);
  EXPECT_NE(empty, assignment);
  // Assign it to a value
  assignment.SetValue(variables[0], 4);
  EXPECT_EQ(assignment, assignment);
  EXPECT_NE(empty, assignment);
  // Adds another variable
  assignment.Add(variables[2]);
  EXPECT_EQ(assignment, assignment);
  EXPECT_NE(empty, assignment);
  // Now add the same variables, but in a different order
  assignment_->Add(variables[2]);
  assignment_->Add(variables[0]);
  assignment_->SetValue(variables[0], 4);
  EXPECT_EQ(assignment, *assignment_);
  // Change a value
  assignment.SetValue(variables[0], 3);
  EXPECT_NE(assignment, *assignment_);
  // Try with a range instead of a value
  assignment.SetRange(variables[0], 5, 7);
  assignment_->SetRange(variables[0], 5, 7);
  EXPECT_EQ(assignment, *assignment_);
  // Adds an interval variable
  IntervalVar* interval =
      solver_->MakeFixedDurationIntervalVar(13, 17, 24, false, "interval");
  assignment.Add(interval);
  EXPECT_NE(assignment, *assignment_);
  assignment_->Add(interval);
  EXPECT_EQ(assignment, *assignment_);
  assignment.SetStartValue(interval, 16);
  EXPECT_NE(assignment, *assignment_);
  assignment_->SetStartValue(interval, 16);
  EXPECT_EQ(assignment, *assignment_);
  // Test logging
  std::ostringstream stream;
  stream << assignment;
  const std::string kExpected =
      "Assignment(v0 (5..7) | "
      "v2 (-9223372036854775808..9223372036854775807) | "
      "interval (start = 16, "
      "duration = -9223372036854775808..9223372036854775807, "
      "status = 0..1) | )";
  const std::string actual = stream.str();
  EXPECT_EQ(kExpected, actual);
  // Deactivate a int variable
  assignment.Deactivate(variables[0]);
  EXPECT_NE(assignment, *assignment_);
  assignment_->Deactivate(variables[0]);
  EXPECT_EQ(assignment, *assignment_);
  // Deactivate a interval variable
  assignment.Deactivate(interval);
  EXPECT_NE(assignment, *assignment_);
  assignment_->Deactivate(interval);
  EXPECT_EQ(assignment, *assignment_);
}

TEST(CreateNewAssignmentTest, AdaptAssignment) {
  Solver solver("CreateNewAssignmentTest");
  const int kNumVars = 5;
  const int kMin = 0;
  const int kMax = 15;
  std::vector<IntVar*> first_vars;
  solver.MakeIntVarArray(kNumVars, kMin, kMax, "v1", &first_vars);
  std::vector<IntVar*> second_vars;
  solver.MakeIntVarArray(kNumVars, kMin, kMax, "v2", &second_vars);

  Assignment first_assignment(&solver);
  std::vector<int64_t> var_values = {10, 5, 3, 7, 12};
  for (int i = 0; i < kNumVars; i++) {
    first_assignment.Add(first_vars[i]);
    first_assignment.SetValue(first_vars[i], var_values[i]);
  }
  Assignment second_assignment(&solver);
  SetAssignmentFromAssignment(&second_assignment, second_vars,
                              &first_assignment, first_vars);

  for (int i = 0; i < kNumVars; i++) {
    EXPECT_EQ(second_assignment.Value(second_vars[i]), var_values[i]);
  }
}

}  // namespace
}  // namespace operations_research
