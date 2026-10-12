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

#include "ortools/constraint_solver/search.h"

#include <stddef.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/simulated_clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/local_search.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/graph/index.h"

namespace operations_research {
namespace {

using ::testing::DoDefault;
using ::testing::Return;

class ComposeTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("ComposeTest"); }

  std::unique_ptr<Solver> solver_;
};

class AddToInt : public DecisionBuilder {
 public:
  AddToInt(int* const ptr, int val) : ptr_(ptr), val_(val) {}
  ~AddToInt() override {}
  Decision* Next(Solver* const) override {
    (*ptr_) += val_;
    return nullptr;
  }
  std::string DebugString() const override {
    return absl::StrFormat("AddToInt(%p, %i)", ptr_, val_);
  }

 private:
  int* const ptr_;
  int val_;
};

TEST_F(ComposeTest, Compose2) {
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const dbs = solver_->Compose(db1, db2);
  ASSERT_TRUE(solver_->Solve(dbs));
  EXPECT_EQ(3, result);
}

TEST_F(ComposeTest, Compose3) {
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const db3 = solver_->RevAlloc(new AddToInt(&result, 3));
  DecisionBuilder* const dbs = solver_->Compose(db1, db2, db3);
  ASSERT_TRUE(solver_->Solve(dbs));
  EXPECT_EQ(6, result);
}

TEST_F(ComposeTest, Compose4) {
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const db3 = solver_->RevAlloc(new AddToInt(&result, 3));
  DecisionBuilder* const db4 = solver_->RevAlloc(new AddToInt(&result, 4));
  DecisionBuilder* const dbs = solver_->Compose(db1, db2, db3, db4);
  ASSERT_TRUE(solver_->Solve(dbs));
  EXPECT_EQ(10, result);
}

TEST_F(ComposeTest, ComposeVector) {
  int result = 0;
  std::vector<DecisionBuilder*> dbv;

  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 1)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 2)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 3)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 4)));
  DecisionBuilder* const dbs = solver_->Compose(dbv);
  ASSERT_TRUE(solver_->Solve(dbs));
  EXPECT_EQ(10, result);
}

TEST_F(ComposeTest, DebugString) {
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const dbs = solver_->Compose(db1, db2);
  EXPECT_FALSE(dbs->DebugString().empty());
}

// ----- Try Test -----

class TryTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("TryTest"); }

  std::unique_ptr<Solver> solver_;
};

TEST_F(TryTest, Try2) {
  const int kExpectedResults[] = {1, 3};
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const dbs = solver_->Try(db1, db2);
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

TEST_F(TryTest, Try3) {
  const int kExpectedResults[] = {1, 3, 6};
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const db3 = solver_->RevAlloc(new AddToInt(&result, 3));
  DecisionBuilder* const dbs = solver_->Try(db1, db2, db3);
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

TEST_F(TryTest, Try4) {
  const int kExpectedResults[] = {1, 3, 6, 10};
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const db3 = solver_->RevAlloc(new AddToInt(&result, 3));
  DecisionBuilder* const db4 = solver_->RevAlloc(new AddToInt(&result, 4));
  DecisionBuilder* const dbs = solver_->Try(db1, db2, db3, db4);
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

TEST_F(TryTest, TryVector) {
  const int kExpectedResults[] = {1, 3, 6, 10};
  int result = 0;
  std::vector<DecisionBuilder*> dbv;
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 1)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 2)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 3)));
  dbv.push_back(solver_->RevAlloc(new AddToInt(&result, 4)));
  DecisionBuilder* const dbs = solver_->Try(dbv);
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

TEST_F(TryTest, Try4Balanced) {
  const int kExpectedResults[] = {1, 3, 6, 10};
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const db3 = solver_->RevAlloc(new AddToInt(&result, 3));
  DecisionBuilder* const db4 = solver_->RevAlloc(new AddToInt(&result, 4));
  DecisionBuilder* const dbs =
      solver_->Try(solver_->Try(db1, db2), solver_->Try(db3, db4));
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

TEST_F(TryTest, DebugString) {
  int result = 0;
  DecisionBuilder* const db1 = solver_->RevAlloc(new AddToInt(&result, 1));
  DecisionBuilder* const db2 = solver_->RevAlloc(new AddToInt(&result, 2));
  DecisionBuilder* const dbs = solver_->Try(db1, db2);
  EXPECT_FALSE(dbs->DebugString().empty());
}

TEST_F(TryTest, TryWithCompose) {
  const int kExpectedResults[] = {4, 8, 13, 17};
  int result = 0;
  std::vector<DecisionBuilder*> dbv1;
  dbv1.push_back(solver_->RevAlloc(new AddToInt(&result, 1)));
  dbv1.push_back(solver_->RevAlloc(new AddToInt(&result, 2)));
  std::vector<DecisionBuilder*> dbv2;
  dbv2.push_back(solver_->RevAlloc(new AddToInt(&result, 3)));
  dbv2.push_back(solver_->RevAlloc(new AddToInt(&result, 4)));
  DecisionBuilder* const dbs =
      solver_->Compose(solver_->Try(dbv1), solver_->Try(dbv2));
  solver_->NewSearch(dbs);
  int solution_counter = 0;
  while (solver_->NextSolution()) {
    EXPECT_EQ(kExpectedResults[solution_counter], result);
    ++solution_counter;
  }
  EXPECT_EQ(std::size(kExpectedResults), solution_counter);
}

// ----- Phase Test -----

int64_t EvalFunction(int64_t, int64_t j) { return j; }
int64_t EvalFunctionConstantPerVar(int64_t i, int64_t) { return i; }
int64_t EvalFunctionEqualZero(int64_t, int64_t j) { return j == 0; }
int64_t EvalNearZero(int64_t, int64_t val) { return val < 0 ? 100LL : val; }
int64_t TieBreakerFirst(int64_t) { return 0; }
int64_t TieBreakerLast(int64_t i) { return i - 1; }
bool SecondArgCloserToFirstThanThird(int64_t i, int64_t j, int64_t k) {
  return std::abs(j - i) < std::abs(k - i);
}

class PhaseTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("PhaseTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_F(PhaseTest, ConstructionAndDebugString) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const ph1 = solver_->MakePhase(
      vars[1], Solver::INT_VAR_SIMPLE, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph2 = solver_->MakePhase(
      vars[1], vars[2], Solver::INT_VAR_DEFAULT, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph3 = solver_->MakePhase(vars[1], vars[2], vars[3],
                                                  Solver::CHOOSE_FIRST_UNBOUND,
                                                  Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph4 =
      solver_->MakePhase(vars[1], vars[2], vars[3], vars[4],
                         Solver::CHOOSE_RANDOM, Solver::ASSIGN_MAX_VALUE);
  DecisionBuilder* const ph5 = solver_->MakePhase(
      vars[1], vars[2], vars[3], vars[4], Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
      Solver::ASSIGN_RANDOM_VALUE);
  DecisionBuilder* const ph6 = solver_->MakePhase(
      vars[1], vars[2], vars[3], vars[4], Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN,
      Solver::ASSIGN_CENTER_VALUE);
  DecisionBuilder* const ph7 =
      solver_->MakePhase(vars, EvalFunction, Solver::CHOOSE_STATIC_GLOBAL_BEST);
  DecisionBuilder* const ph8 = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MAX, &EvalFunction);
  DecisionBuilder* const ph9 =
      solver_->MakePhase(vars, Solver::CHOOSE_PATH, EvalFunction);
  DecisionBuilder* const ph10 = solver_->MakePhase(
      vars, EvalFunction, Solver::CHOOSE_DYNAMIC_GLOBAL_BEST);
  DecisionBuilder* const ph11 = solver_->MakePhase(
      vars[1], vars[2], vars[3], vars[4], Solver::CHOOSE_MIN_SIZE_LOWEST_MAX,
      Solver::ASSIGN_RANDOM_VALUE);
  DecisionBuilder* const ph12 = solver_->MakePhase(
      vars[1], vars[2], vars[3], vars[4], Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX,
      Solver::ASSIGN_CENTER_VALUE);
  DecisionBuilder* const ph13 =
      solver_->MakePhase(vars[1], vars[2], vars[3], Solver::CHOOSE_LOWEST_MIN,
                         Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph14 =
      solver_->MakePhase(vars[1], vars[2], vars[3], Solver::CHOOSE_HIGHEST_MAX,
                         Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph15 =
      solver_->MakePhase(vars[1], vars[2], vars[3], Solver::CHOOSE_MIN_SIZE,
                         Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph16 =
      solver_->MakePhase(vars[1], vars[2], vars[3], Solver::CHOOSE_MAX_SIZE,
                         Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph17 = solver_->MakePhase(
      vars[1], vars[2], vars[3], Solver::CHOOSE_MAX_REGRET_ON_MIN,
      Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const ph18 = solver_->MakePhase(vars[1], vars[2], vars[3],
                                                   Solver::CHOOSE_FIRST_UNBOUND,
                                                   Solver::SPLIT_LOWER_HALF);
  DecisionBuilder* const ph19 = solver_->MakePhase(vars[1], vars[2], vars[3],
                                                   Solver::CHOOSE_FIRST_UNBOUND,
                                                   Solver::SPLIT_UPPER_HALF);
  DecisionBuilder* const ph20 = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, SecondArgCloserToFirstThanThird);

  EXPECT_FALSE(ph1->DebugString().empty());
  EXPECT_FALSE(ph2->DebugString().empty());
  EXPECT_FALSE(ph3->DebugString().empty());
  EXPECT_FALSE(ph4->DebugString().empty());
  EXPECT_FALSE(ph5->DebugString().empty());
  EXPECT_FALSE(ph6->DebugString().empty());
  EXPECT_FALSE(ph7->DebugString().empty());
  EXPECT_FALSE(ph8->DebugString().empty());
  EXPECT_FALSE(ph9->DebugString().empty());
  EXPECT_FALSE(ph10->DebugString().empty());
  EXPECT_FALSE(ph11->DebugString().empty());
  EXPECT_FALSE(ph12->DebugString().empty());
  EXPECT_FALSE(ph13->DebugString().empty());
  EXPECT_FALSE(ph14->DebugString().empty());
  EXPECT_FALSE(ph15->DebugString().empty());
  EXPECT_FALSE(ph16->DebugString().empty());
  EXPECT_FALSE(ph17->DebugString().empty());
  EXPECT_FALSE(ph18->DebugString().empty());
  EXPECT_FALSE(ph19->DebugString().empty());
  EXPECT_FALSE(ph20->DebugString().empty());
}

TEST_F(PhaseTest, DisplayDecision) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Decision* const d = db->Next(solver_.get());
  EXPECT_FALSE(d->DebugString().empty());
}

TEST_F(PhaseTest, DeathVar) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  EXPECT_DEATH(
      solver_->MakePhase(vars[1], static_cast<Solver::IntVarStrategy>(-1),
                         Solver::ASSIGN_MIN_VALUE),
      "");
}

TEST_F(PhaseTest, DeathValue) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  EXPECT_DEATH(solver_->MakePhase(vars[1], Solver::CHOOSE_FIRST_UNBOUND,
                                  static_cast<Solver::IntValueStrategy>(-1)),
               "");
}

TEST_F(PhaseTest, ChooseFirstAssignMin) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{0}, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstAssignMax) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(10, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstAssignRandom) {
  std::vector<IntVar*> vars;
  const int size = 10;
  solver_->MakeIntVarArray(size, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_RANDOM_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  int64_t sum = 0;
  for (int i = 0; i < size; ++i) {
    sum += vars[i]->Value();
  }
  const int64_t avg_value = sum / size;
  EXPECT_GE(5 + 2, avg_value);
  EXPECT_LE(5 - 2, avg_value);
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstAssignCenter) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_CENTER_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(5, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseRandomAssignMin) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{0}, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseRandomAssignMax) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MAX_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(10, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseRandomAssignRandom) {
  std::vector<IntVar*> vars;
  const int size = 10;
  const int64_t ub = 10;
  solver_->MakeIntVarArray(size, 0, ub, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(vars, Solver::CHOOSE_RANDOM,
                                                 Solver::ASSIGN_RANDOM_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  int64_t sum = 0;
  for (int i = 0; i < size; ++i) {
    sum += vars[i]->Value();
  }
  const int64_t avg_value = sum / size;
  EXPECT_GE(ub / 2 + 2, avg_value);
  EXPECT_LE(ub / 2 - 2, avg_value);
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseRandomAssignCenter) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(vars, Solver::CHOOSE_RANDOM,
                                                 Solver::ASSIGN_CENTER_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(5, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeLowestMinAssignCenter) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-3, 3, "x0"));
  vars.push_back(solver_->MakeIntVar(-5, 5, "x1"));
  vars.push_back(solver_->MakeIntVar(-7, 7, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN, Solver::ASSIGN_CENTER_VALUE);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(0, vars[0]->Value());
  EXPECT_EQ(-1, vars[1]->Value());
  EXPECT_EQ(1, vars[2]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeLowestMinAssignNearZero) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-4, 2, "x0"));
  vars.push_back(solver_->MakeIntVar(-3, 3, "x1"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN, EvalNearZero);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(0, vars[0]->Value());
  EXPECT_EQ(1, vars[1]->Value());
  EXPECT_EQ(2, vars[2]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeHighestMinAssignNearZero) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-4, 2, "x0"));
  vars.push_back(solver_->MakeIntVar(-3, 3, "x1"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN, EvalNearZero);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(0, vars[2]->Value());
  EXPECT_EQ(1, vars[1]->Value());
  EXPECT_EQ(2, vars[0]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeLowestMaxAssignNearZero) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-4, 2, "x0"));
  vars.push_back(solver_->MakeIntVar(-3, 3, "x1"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MAX, EvalNearZero);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(0, vars[0]->Value());
  EXPECT_EQ(1, vars[1]->Value());
  EXPECT_EQ(2, vars[2]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeHighestMaxAssignNearZero) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-4, 2, "x0"));
  vars.push_back(solver_->MakeIntVar(-3, 3, "x1"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX, EvalNearZero);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(0, vars[2]->Value());
  EXPECT_EQ(1, vars[1]->Value());
  EXPECT_EQ(2, vars[0]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseMinSizeLowestMinAssignLast) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(-4, 2, "x0"));
  vars.push_back(solver_->MakeIntVar(-3, 3, "x1"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x2"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                         EvalFunctionConstantPerVar, TieBreakerLast);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(2, vars[0]->Value());
  EXPECT_EQ(3, vars[1]->Value());
  EXPECT_EQ(4, vars[2]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstUnboundAssignBestValueByComparison) {
  std::vector<IntVar*> vars;
  vars.push_back(solver_->MakeIntVar(3, 3, "x0"));
  vars.push_back(solver_->MakeIntVar(-2, 4, "x1"));
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, true));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, SecondArgCloserToFirstThanThird);
  solver_->NewSearch(db);
  solver_->NextSolution();
  EXPECT_EQ(3, vars[0]->Value());
  EXPECT_EQ(1, vars[1]->Value());
  // Test the tie-breaking: the lowest value should be taken first.
  solver_->NextSolution();
  EXPECT_EQ(0, vars[1]->Value());
  solver_->NextSolution();
  EXPECT_EQ(2, vars[1]->Value());
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseGlobalBest) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, EvalFunction, Solver::CHOOSE_STATIC_GLOBAL_BEST);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(0, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseDynamicGlobalBest) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, EvalFunction, Solver::CHOOSE_DYNAMIC_GLOBAL_BEST);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(0, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstUnboundAssignBest) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND, EvalFunction);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(0, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstUnboundAssignBestUseTieBreakerFirst) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         EvalFunctionConstantPerVar, TieBreakerFirst);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(0, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstUnboundAssignBestUseTieBreakerFirstBug) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         EvalFunctionEqualZero, TieBreakerFirst);
  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(1, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChooseFirstUnboundAssignBestUseTieBreakerLast) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         EvalFunctionConstantPerVar, &TieBreakerLast);

  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(10, vars[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(PhaseTest, ChoosePathAssignBest) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  // Requires some kind of constraint to prevent cycles
  std::vector<IntVar*> ranks;
  solver_->MakeIntVarArray(11, 0, 11, "ranks", &ranks);
  for (int i = 0; i < 10; ++i) {
    IntVar* const rank_transit = solver_->MakeSum(ranks[i], 1)->Var();
    IntVar* const elem = solver_->MakeElement(ranks, vars[i])->Var();
    solver_->AddConstraint(solver_->MakeEquality(elem, rank_transit));
  }
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::CHOOSE_PATH, EvalFunction);

  solver_->NewSearch(db);
  solver_->NextSolution();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i + 1, vars[i]->Value());
  }
  solver_->EndSearch();
}

// SplitDomain decision test

class SplitDomainDecisionTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SplitDomainDecisionTest");
  }
  std::unique_ptr<Solver> solver_;
};

class SplitDomainDecisionBuilder : public DecisionBuilder {
 public:
  SplitDomainDecisionBuilder(IntVar* const var, int64_t value, int64_t lower)
      : var_(var), value_(value), lower_(lower), done_(false) {}

  ~SplitDomainDecisionBuilder() override {}

  Decision* Next(Solver* const s) override {
    if (done_) {
      return nullptr;
    } else {
      s->SaveAndSetValue(&done_, true);
      return s->MakeSplitVariableDomain(var_, value_, lower_);
    }
  }

 private:
  IntVar* const var_;
  const int64_t value_;
  const bool lower_;
  bool done_;
};

TEST_F(SplitDomainDecisionTest, testLower) {
  IntVar* const var = solver_->MakeIntVar(0, 10);
  SplitDomainDecisionBuilder* const db =
      solver_->RevAlloc(new SplitDomainDecisionBuilder(var, 3, true));
  solver_->NewSearch(db);
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, var->Min());
  EXPECT_EQ(3, var->Max());
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(4, var->Min());
  EXPECT_EQ(10, var->Max());
  EXPECT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

TEST_F(SplitDomainDecisionTest, testUpper) {
  IntVar* const var = solver_->MakeIntVar(0, 10);
  SplitDomainDecisionBuilder* const db =
      solver_->RevAlloc(new SplitDomainDecisionBuilder(var, 6, false));
  solver_->NewSearch(db);
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(7, var->Min());
  EXPECT_EQ(10, var->Max());
  EXPECT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, var->Min());
  EXPECT_EQ(6, var->Max());
  EXPECT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

// AssignVariablesValues decision test.

class AssignVariablesValuesDecisionTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("AssignVariablesValuesDecisionTest");
  }
  std::unique_ptr<Solver> solver_;
};

class CallbackOnceDecisionBuilder : public DecisionBuilder {
 public:
  CallbackOnceDecisionBuilder(std::function<Decision*()> decision_callback,
                              const std::string& expected_debug_string)
      : decision_callback_(std::move(decision_callback)),
        expected_debug_string_(expected_debug_string) {}

  ~CallbackOnceDecisionBuilder() override {}

  Decision* Next(Solver* const s) override {
    if (called_once_) {
      return nullptr;
    } else {
      s->SaveAndSetValue(&called_once_, true);
      Decision* decision = decision_callback_();
      EXPECT_EQ(expected_debug_string_, decision->DebugString());
      return decision;
    }
  }

 private:
  std::function<Decision*()> decision_callback_;
  const std::string expected_debug_string_;
  bool called_once_ = false;
};

// Empty assignment is a corner case.
TEST_F(AssignVariablesValuesDecisionTest, EmptyAssignment) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(/*num_vars*/ 3, /*min*/ 1, /*max*/ 3, "x", &vars);
  auto assign_callback = [=, this]() -> Decision* {
    return solver_->MakeAssignVariablesValues({}, {});
  };
  const std::string expected_debug_string = "do nothing or forbid assignment";
  DecisionBuilder* const assign = solver_->RevAlloc(
      new CallbackOnceDecisionBuilder(assign_callback, expected_debug_string));
  DecisionBuilder* const first_min = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(solver_->Compose(assign, first_min));

  // Enumerate the 3*3*3 solutions.
  std::set<std::vector<int64_t>> solutions;
  for (int i = 0; i < 27; ++i) {
    ASSERT_TRUE(solver_->NextSolution());
    solutions.insert({vars[0]->Value(), vars[1]->Value(), vars[2]->Value()});
  }
  EXPECT_FALSE(solver_->NextSolution());
  std::set<std::vector<int64_t>> expected_solutions;
  for (int i = 1; i <= 3; ++i) {
    for (int j = 1; j <= 3; ++j) {
      for (int k = 1; k <= 3; ++k) {
        expected_solutions.insert({i, j, k});
      }
    }
  }
  EXPECT_THAT(solutions, testing::ContainerEq(expected_solutions));
  solver_->EndSearch();
}

TEST_F(AssignVariablesValuesDecisionTest, DefaultRefutationIsForbid) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(/*num_vars*/ 3, /*min*/ 1, /*max*/ 3, "", &vars);
  const std::vector<int64_t> first_solution{1, 3, 2};
  auto assign_callback = [=, this]() -> Decision* {
    return solver_->MakeAssignVariablesValues(vars, first_solution);
  };
  const std::string expected_debug_string = absl::StrFormat(
      "[%s == 1][%s == 3][%s == 2] or forbid assignment",
      vars[0]->DebugString(), vars[1]->DebugString(), vars[2]->DebugString());
  DecisionBuilder* const assign = solver_->RevAlloc(
      new CallbackOnceDecisionBuilder(assign_callback, expected_debug_string));
  DecisionBuilder* const first_min = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(solver_->Compose(assign, first_min));

  // First solution exists and is values defined above: 1, 3, 2.
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(1, vars[0]->Value());
  EXPECT_EQ(3, vars[1]->Value());
  EXPECT_EQ(2, vars[2]->Value());
  // Enumerate the remaining 3*3*3 -1 solutions.
  std::set<std::vector<int64_t>> solutions{first_solution};
  for (int i = 0; i < 26; ++i) {
    ASSERT_TRUE(solver_->NextSolution());
    solutions.insert({vars[0]->Value(), vars[1]->Value(), vars[2]->Value()});
  }
  EXPECT_FALSE(solver_->NextSolution());
  // The list of all solutions should be all possible assignments.
  std::set<std::vector<int64_t>> expected_solutions;
  for (int i = 1; i <= 3; ++i) {
    for (int j = 1; j <= 3; ++j) {
      for (int k = 1; k <= 3; ++k) {
        expected_solutions.insert({i, j, k});
      }
    }
  }
  EXPECT_THAT(solutions, testing::ContainerEq(expected_solutions));
  solver_->EndSearch();
}

TEST_F(AssignVariablesValuesDecisionTest, DoNothingRefutation) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(/*num_vars*/ 3, /*min*/ 1, /*max*/ 3, "", &vars);
  const std::vector<int64_t> first_solution{1, 3, 2};
  auto assign_callback = [=, this]() -> Decision* {
    return solver_->MakeAssignVariablesValuesOrDoNothing(vars, first_solution);
  };
  const std::string expected_debug_string = absl::StrFormat(
      "[%s == 1][%s == 3][%s == 2] or do nothing", vars[0]->DebugString(),
      vars[1]->DebugString(), vars[2]->DebugString());
  DecisionBuilder* const assign = solver_->RevAlloc(
      new CallbackOnceDecisionBuilder(assign_callback, expected_debug_string));
  DecisionBuilder* const first_min = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(solver_->Compose(assign, first_min));

  // First solution exists and is values defined above: 1, 3, 2.
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(1, vars[0]->Value());
  EXPECT_EQ(3, vars[1]->Value());
  EXPECT_EQ(2, vars[2]->Value());
  // kDoNothing does not forbid anything in the remainder of the search,
  // there are 3*3*3 solutions left.
  std::multiset<std::vector<int64_t>> solutions{first_solution};
  for (int i = 0; i < 27; ++i) {
    ASSERT_TRUE(solver_->NextSolution());
    solutions.insert({vars[0]->Value(), vars[1]->Value(), vars[2]->Value()});
  }
  EXPECT_FALSE(solver_->NextSolution());
  // The list of all solutions should have all possible assignments once,
  // except for first_solution, which should appear twice.
  std::multiset<std::vector<int64_t>> expected_solutions{first_solution};
  for (int i = 1; i <= 3; ++i) {
    for (int j = 1; j <= 3; ++j) {
      for (int k = 1; k <= 3; ++k) {
        expected_solutions.insert({i, j, k});
      }
    }
  }
  EXPECT_THAT(solutions, testing::ContainerEq(expected_solutions));
  solver_->EndSearch();
}

TEST_F(AssignVariablesValuesDecisionTest, FailRefutation) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(/*num_vars*/ 3, /*min*/ 1, /*max*/ 3, "", &vars);
  const std::vector<int64_t> first_solution{1, 3, 2};
  auto assign_callback = [=, this]() -> Decision* {
    return solver_->MakeAssignVariablesValuesOrFail(vars, first_solution);
  };
  const std::string expected_debug_string = absl::StrFormat(
      "[%s == 1][%s == 3][%s == 2] or fail", vars[0]->DebugString(),
      vars[1]->DebugString(), vars[2]->DebugString());
  DecisionBuilder* const assign = solver_->RevAlloc(
      new CallbackOnceDecisionBuilder(assign_callback, expected_debug_string));
  DecisionBuilder* const first_min = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(solver_->Compose(assign, first_min));

  // First solution exists and is values defined above: 1, 3, 2.
  std::multiset<std::vector<int64_t>> solutions{first_solution};
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(1, vars[0]->Value());
  EXPECT_EQ(3, vars[1]->Value());
  EXPECT_EQ(2, vars[2]->Value());

  // kFail forbids any other solution.
  EXPECT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

// ----- Solution Collector -----

class SolutionCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SolutionCollectorTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(SolutionCollectorTest, FirstSolutionCollector) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  IntVar* const obj = solver_->MakeIntVar(2, 10, "obj");
  DecisionBuilder* const db = solver_->MakePhase(
      {var, obj}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  a->AddObjective(obj);
  SolutionCollector* const collector = solver_->MakeFirstSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeFirstSolutionCollector();
  collector2->Add(var);
  collector2->AddObjective(obj);
  SolutionCollector* const c2 = solver_->MakeFirstSolutionCollector(nullptr);
  EXPECT_FALSE(c2->DebugString().empty());
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(0, collector->solution(0)->Value(var));
  EXPECT_EQ(2, collector->objective_value(0));
  EXPECT_EQ(0, collector->failures(0));
  EXPECT_EQ(2, collector->branches(0));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(1, collector2->solution_count());
  EXPECT_EQ(0, collector2->solution(0)->Value(var));
  EXPECT_EQ(2, collector2->objective_value(0));
  EXPECT_EQ(0, collector2->failures(0));
  EXPECT_EQ(2, collector2->branches(0));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, LexicographicFirstSolutionCollector) {
  const int kNumObj = 10;
  std::vector<IntVar*> obj;
  solver_->MakeIntVarArray(kNumObj, 2, 10, &obj);
  DecisionBuilder* const db = solver_->MakePhase(
      obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjectives(obj);
  SolutionCollector* const collector = solver_->MakeFirstSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeFirstSolutionCollector();
  collector2->AddObjectives(obj);
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(1, collector->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(2, collector->ObjectiveValueFromIndex(0, i));
  }
  EXPECT_EQ(0, collector->failures(0));
  EXPECT_EQ(kNumObj, collector->branches(0));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(1, collector2->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(2, collector2->ObjectiveValueFromIndex(0, i));
  }
  EXPECT_EQ(0, collector2->failures(0));
  EXPECT_EQ(kNumObj, collector2->branches(0));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, LastSolutionCollector) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  IntVar* const obj = solver_->MakeIntVar(2, 10, "obj");
  DecisionBuilder* const db = solver_->MakePhase(
      {var, obj}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  a->AddObjective(obj);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeLastSolutionCollector();
  collector2->Add(var);
  collector2->AddObjective(obj);
  SolutionCollector* const c2 = solver_->MakeLastSolutionCollector(nullptr);
  EXPECT_FALSE(c2->DebugString().empty());
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(10, collector->solution(0)->Value(var));
  EXPECT_EQ(10, collector->objective_value(0));
  EXPECT_EQ(98, collector->failures(0));
  EXPECT_EQ(196, collector->branches(0));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(1, collector2->solution_count());
  EXPECT_EQ(10, collector2->solution(0)->Value(var));
  EXPECT_EQ(10, collector2->objective_value(0));
  EXPECT_EQ(98, collector2->failures(0));
  EXPECT_EQ(196, collector2->branches(0));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, LexicographicLastSolutionCollector) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj;
  solver_->MakeIntVarArray(kNumObj, 2, 5, &obj);
  DecisionBuilder* const db = solver_->MakePhase(
      obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjectives(obj);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeLastSolutionCollector();
  collector2->AddObjectives(obj);
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(1, collector->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(5, collector->ObjectiveValueFromIndex(0, i));
  }
  EXPECT_EQ(63, collector->failures(0));
  EXPECT_EQ(126, collector->branches(0));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(1, collector2->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(5, collector2->ObjectiveValueFromIndex(0, i));
  }
  EXPECT_EQ(63, collector2->failures(0));
  EXPECT_EQ(126, collector2->branches(0));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, AllSolutionCollector) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjective(var);
  SolutionCollector* const collector = solver_->MakeAllSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeAllSolutionCollector();
  collector2->AddObjective(var);
  SolutionCollector* const c2 = solver_->MakeAllSolutionCollector(nullptr);
  EXPECT_FALSE(c2->DebugString().empty());
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(11, collector->solution_count());
  EXPECT_EQ(3, collector->solution(3)->ObjectiveValue());
  EXPECT_EQ(3, collector->objective_value(3));
  EXPECT_EQ(3, collector->failures(3));
  EXPECT_EQ(7, collector->branches(3));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(11, collector2->solution_count());
  EXPECT_EQ(3, collector2->solution(3)->ObjectiveValue());
  EXPECT_EQ(3, collector2->objective_value(3));
  EXPECT_EQ(3, collector2->failures(3));
  EXPECT_EQ(7, collector2->branches(3));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, LexicographicAllSolutionCollector) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj;
  solver_->MakeIntVarArray(kNumObj, 2, 5, &obj);
  DecisionBuilder* const db = solver_->MakePhase(
      obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjectives(obj);
  SolutionCollector* const collector = solver_->MakeAllSolutionCollector(a);
  SolutionCollector* const collector2 = solver_->MakeAllSolutionCollector();
  collector2->AddObjectives(obj);
  SolutionCollector* const c2 = solver_->MakeAllSolutionCollector(nullptr);
  EXPECT_FALSE(c2->DebugString().empty());
  ASSERT_TRUE(solver_->Solve(db, collector, collector2));
  EXPECT_EQ(64, collector->solution_count());
  std::vector<int64_t> expected_objective_values = {2, 2, 5};
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(expected_objective_values[i],
              collector->ObjectiveValueFromIndex(3, i));
  }
  EXPECT_EQ(3, collector->failures(3));
  EXPECT_EQ(8, collector->branches(3));
  EXPECT_FALSE(collector->DebugString().empty());
  EXPECT_EQ(64, collector2->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(expected_objective_values[i],
              collector->ObjectiveValueFromIndex(3, i));
  }
  EXPECT_EQ(3, collector2->failures(3));
  EXPECT_EQ(8, collector2->branches(3));
  EXPECT_FALSE(collector2->DebugString().empty());
}

TEST_F(SolutionCollectorTest, BestValueSolutionCollector) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjective(var);
  std::vector<SearchMonitor*> collectors;
  SolutionCollector* const c1_max =
      solver_->MakeBestValueSolutionCollector(a, true);
  collectors.push_back(c1_max);
  SolutionCollector* const c1_min =
      solver_->MakeBestValueSolutionCollector(a, false);
  collectors.push_back(c1_min);
  SolutionCollector* const c1b_max =
      solver_->MakeBestValueSolutionCollector(true);
  c1b_max->AddObjective(var);
  collectors.push_back(c1b_max);
  SolutionCollector* const c1b_min =
      solver_->MakeBestValueSolutionCollector(false);
  c1b_min->AddObjective(var);
  collectors.push_back(c1b_min);
  SolutionCollector* const c2_max =
      solver_->MakeBestValueSolutionCollector(nullptr, true);
  collectors.push_back(c2_max);
  SolutionCollector* const c2_min =
      solver_->MakeBestValueSolutionCollector(nullptr, false);
  collectors.push_back(c2_min);
  Assignment* const a2 = solver_->MakeAssignment();
  SolutionCollector* const c3_max =
      solver_->MakeBestValueSolutionCollector(a2, true);
  collectors.push_back(c3_max);
  SolutionCollector* const c3_min =
      solver_->MakeBestValueSolutionCollector(a2, false);
  collectors.push_back(c3_min);
  ASSERT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c1_max->solution_count());
  EXPECT_EQ(1, c1_min->solution_count());
  EXPECT_EQ(1, c1b_max->solution_count());
  EXPECT_EQ(1, c1b_min->solution_count());
  EXPECT_EQ(0, c2_max->solution_count());
  EXPECT_EQ(0, c2_min->solution_count());
  EXPECT_EQ(0, c3_max->solution_count());
  EXPECT_EQ(0, c3_min->solution_count());
  EXPECT_EQ(10, c1_max->solution(0)->ObjectiveValue());
  EXPECT_EQ(0, c1_min->solution(0)->ObjectiveValue());
  EXPECT_EQ(10, c1_max->objective_value(0));
  EXPECT_EQ(0, c1_min->objective_value(0));
  EXPECT_EQ(10, c1_max->failures(0));
  EXPECT_EQ(0, c1_min->failures(0));
  EXPECT_EQ(20, c1_max->branches(0));
  EXPECT_EQ(1, c1_min->branches(0));
  EXPECT_FALSE(c1_min->DebugString().empty());
  EXPECT_FALSE(c1_max->DebugString().empty());
  EXPECT_EQ(10, c1b_max->solution(0)->ObjectiveValue());
  EXPECT_EQ(0, c1b_min->solution(0)->ObjectiveValue());
  EXPECT_EQ(10, c1b_max->objective_value(0));
  EXPECT_EQ(0, c1b_min->objective_value(0));
  EXPECT_EQ(10, c1b_max->failures(0));
  EXPECT_EQ(0, c1b_min->failures(0));
  EXPECT_EQ(20, c1b_max->branches(0));
  EXPECT_EQ(1, c1b_min->branches(0));
  EXPECT_FALSE(c1b_min->DebugString().empty());
  EXPECT_FALSE(c1b_max->DebugString().empty());
  EXPECT_FALSE(c2_min->DebugString().empty());
  EXPECT_FALSE(c2_max->DebugString().empty());
  EXPECT_FALSE(c3_min->DebugString().empty());
  EXPECT_FALSE(c3_max->DebugString().empty());
}

TEST_F(SolutionCollectorTest, BestValueSolutionCollectorWithLimitCosts) {
  std::vector<SearchMonitor*> collectors;
  IntVar* const var_max = solver_->MakeIntVar(kint64max, kint64max, "max");
  Assignment* const a_max = solver_->MakeAssignment();
  a_max->AddObjective(var_max);
  SolutionCollector* const c_max =
      solver_->MakeBestValueSolutionCollector(a_max, false);
  collectors.push_back(c_max);
  IntVar* const var_min = solver_->MakeIntVar(kint64min, kint64min, "min");
  Assignment* const a_min = solver_->MakeAssignment();
  a_min->AddObjective(var_min);
  SolutionCollector* const c_min =
      solver_->MakeBestValueSolutionCollector(a_min, true);
  collectors.push_back(c_min);
  DecisionBuilder* const db =
      solver_->MakePhase({var_min, var_max}, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MIN_VALUE);
  EXPECT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c_max->solution_count());
  EXPECT_EQ(kint64max, c_max->solution(0)->ObjectiveValue());
  EXPECT_EQ(1, c_min->solution_count());
  EXPECT_EQ(kint64min, c_min->solution(0)->ObjectiveValue());
}

TEST_F(SolutionCollectorTest, LexicographicBestValueSolutionCollector) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj;
  solver_->MakeIntVarArray(kNumObj, 2, 5, &obj);
  DecisionBuilder* const db = solver_->MakePhase(
      obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  std::vector<bool> maximize = {true, false, false};
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjectives(obj);
  std::vector<SearchMonitor*> collectors;
  SolutionCollector* const c1 =
      solver_->MakeBestLexicographicValueSolutionCollector(a, maximize);
  collectors.push_back(c1);
  // More objectives in the assignment than in the collector.
  std::vector<bool> small_maximize = {true, false};
  SolutionCollector* const c1b =
      solver_->MakeBestLexicographicValueSolutionCollector(a, small_maximize);
  collectors.push_back(c1b);
  // Fewer objectives in the assignment than in the collector.
  std::vector<bool> big_maximize = {true, false, false, true};
  SolutionCollector* const c1t =
      solver_->MakeBestLexicographicValueSolutionCollector(a, big_maximize);
  collectors.push_back(c1t);
  Assignment* const a2 = solver_->MakeAssignment();
  SolutionCollector* const c2 =
      solver_->MakeBestLexicographicValueSolutionCollector(a2, maximize);
  collectors.push_back(c2);
  SolutionCollector* const c3 =
      solver_->MakeBestLexicographicValueSolutionCollector(maximize);
  collectors.push_back(c3);
  ASSERT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c1->solution_count());
  EXPECT_EQ(1, c1b->solution_count());
  EXPECT_EQ(1, c1t->solution_count());
  EXPECT_EQ(0, c2->solution_count());
  EXPECT_EQ(0, c3->solution_count());
  EXPECT_EQ(5, c1->solution(0)->ObjectiveValueFromIndex(0));
  EXPECT_EQ(2, c1->solution(0)->ObjectiveValueFromIndex(1));
  EXPECT_EQ(2, c1->solution(0)->ObjectiveValueFromIndex(2));
  EXPECT_EQ(5, c1->ObjectiveValueFromIndex(0, 0));
  EXPECT_EQ(2, c1->ObjectiveValueFromIndex(0, 1));
  EXPECT_EQ(2, c1->ObjectiveValueFromIndex(0, 2));
  EXPECT_EQ(48, c1->failures(0));
  EXPECT_EQ(98, c1->branches(0));
  EXPECT_EQ(5, c1b->solution(0)->ObjectiveValueFromIndex(0));
  EXPECT_EQ(2, c1b->solution(0)->ObjectiveValueFromIndex(1));
  EXPECT_EQ(5, c1b->ObjectiveValueFromIndex(0, 0));
  EXPECT_EQ(2, c1b->ObjectiveValueFromIndex(0, 1));
  EXPECT_EQ(48, c1b->failures(0));
  EXPECT_EQ(98, c1b->branches(0));
  EXPECT_EQ(5, c1t->solution(0)->ObjectiveValueFromIndex(0));
  EXPECT_EQ(2, c1t->solution(0)->ObjectiveValueFromIndex(1));
  EXPECT_EQ(2, c1t->solution(0)->ObjectiveValueFromIndex(2));
  EXPECT_EQ(5, c1t->ObjectiveValueFromIndex(0, 0));
  EXPECT_EQ(2, c1t->ObjectiveValueFromIndex(0, 1));
  EXPECT_EQ(2, c1t->ObjectiveValueFromIndex(0, 2));
  EXPECT_EQ(48, c1t->failures(0));
  EXPECT_EQ(98, c1t->branches(0));
  EXPECT_FALSE(c1->DebugString().empty());
  EXPECT_FALSE(c1b->DebugString().empty());
  EXPECT_FALSE(c1t->DebugString().empty());
  EXPECT_FALSE(c2->DebugString().empty());
  EXPECT_FALSE(c3->DebugString().empty());
}

TEST_F(SolutionCollectorTest,
       LexicographicBestValueSolutionCollectorWithLimitCosts) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj_max;
  solver_->MakeIntVarArray(kNumObj, kint64max, kint64max, &obj_max);
  std::vector<IntVar*> obj_min;
  solver_->MakeIntVarArray(kNumObj, kint64min, kint64min, &obj_min);
  std::vector<SearchMonitor*> collectors;
  Assignment* const a_max = solver_->MakeAssignment();
  a_max->AddObjectives(obj_max);
  SolutionCollector* const c_max =
      solver_->MakeBestLexicographicValueSolutionCollector(
          a_max, {false, false, false});
  collectors.push_back(c_max);
  Assignment* const a_min = solver_->MakeAssignment();
  a_min->AddObjectives(obj_min);
  SolutionCollector* const c_min =
      solver_->MakeBestLexicographicValueSolutionCollector(a_min,
                                                           {true, true, true});
  collectors.push_back(c_min);
  std::vector<IntVar*> all_vars = obj_min;
  all_vars.insert(all_vars.end(), obj_max.begin(), obj_max.end());
  DecisionBuilder* const db = solver_->MakePhase(
      all_vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  EXPECT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c_max->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(kint64max, c_max->solution(0)->ObjectiveValueFromIndex(i));
  }
  EXPECT_EQ(1, c_min->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(kint64min, c_min->solution(0)->ObjectiveValueFromIndex(i));
  }
}

TEST_F(SolutionCollectorTest, NBestValueSolutionCollector) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjective(var);
  std::vector<SearchMonitor*> collectors;
  SolutionCollector* const c1_max =
      solver_->MakeNBestValueSolutionCollector(a, 3, true);
  collectors.push_back(c1_max);
  SolutionCollector* const c1_min =
      solver_->MakeNBestValueSolutionCollector(a, 3, false);
  collectors.push_back(c1_min);
  SolutionCollector* const c1b_max =
      solver_->MakeNBestValueSolutionCollector(3, true);
  c1b_max->AddObjective(var);
  collectors.push_back(c1b_max);
  SolutionCollector* const c1b_min =
      solver_->MakeNBestValueSolutionCollector(3, false);
  c1b_min->AddObjective(var);
  collectors.push_back(c1b_min);
  SolutionCollector* const c2_max =
      solver_->MakeNBestValueSolutionCollector(nullptr, 3, true);
  collectors.push_back(c2_max);
  SolutionCollector* const c2_min =
      solver_->MakeNBestValueSolutionCollector(nullptr, 3, false);
  collectors.push_back(c2_min);
  Assignment* const a2 = solver_->MakeAssignment();
  SolutionCollector* const c3_max =
      solver_->MakeNBestValueSolutionCollector(a2, 3, true);
  collectors.push_back(c3_max);
  SolutionCollector* const c3_min =
      solver_->MakeNBestValueSolutionCollector(a2, 3, false);
  collectors.push_back(c3_min);
  ASSERT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(3, c1_max->solution_count());
  EXPECT_EQ(3, c1_min->solution_count());
  EXPECT_EQ(3, c1b_max->solution_count());
  EXPECT_EQ(3, c1b_min->solution_count());
  EXPECT_EQ(0, c2_max->solution_count());
  EXPECT_EQ(0, c2_min->solution_count());
  EXPECT_EQ(0, c3_max->solution_count());
  EXPECT_EQ(0, c3_min->solution_count());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(8 + i, c1_max->solution(i)->ObjectiveValue());
    EXPECT_EQ(2 - i, c1_min->solution(i)->ObjectiveValue());
    EXPECT_EQ(8 + i, c1_max->objective_value(i));
    EXPECT_EQ(2 - i, c1_min->objective_value(i));
    EXPECT_EQ(8 + i, c1_max->failures(i));
    EXPECT_EQ(2 - i, c1_min->failures(i));
    if (i < 2) {
      EXPECT_EQ(2 * (8 + i) + 1, c1_max->branches(i));
    } else {
      EXPECT_EQ(20, c1_max->branches(i));
    }
    EXPECT_EQ(2 * (2 - i) + 1, c1_min->branches(i));
  }
  EXPECT_FALSE(c1_min->DebugString().empty());
  EXPECT_FALSE(c1_max->DebugString().empty());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(8 + i, c1b_max->solution(i)->ObjectiveValue());
    EXPECT_EQ(2 - i, c1b_min->solution(i)->ObjectiveValue());
    EXPECT_EQ(8 + i, c1b_max->objective_value(i));
    EXPECT_EQ(2 - i, c1b_min->objective_value(i));
    EXPECT_EQ(8 + i, c1b_max->failures(i));
    EXPECT_EQ(2 - i, c1b_min->failures(i));
    if (i < 2) {
      EXPECT_EQ(2 * (8 + i) + 1, c1b_max->branches(i));
    } else {
      EXPECT_EQ(20, c1_max->branches(i));
    }
    EXPECT_EQ(2 * (2 - i) + 1, c1b_min->branches(i));
  }
  EXPECT_FALSE(c1b_min->DebugString().empty());
  EXPECT_FALSE(c1b_max->DebugString().empty());
  EXPECT_FALSE(c2_min->DebugString().empty());
  EXPECT_FALSE(c2_max->DebugString().empty());
  EXPECT_FALSE(c3_min->DebugString().empty());
  EXPECT_FALSE(c3_max->DebugString().empty());
}

TEST_F(SolutionCollectorTest, LexicographicNBestValueSolutionCollector) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj;
  solver_->MakeIntVarArray(kNumObj, 2, 5, &obj);
  DecisionBuilder* const db = solver_->MakePhase(
      obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  std::vector<bool> maximize = {true, false, false};
  Assignment* const a = solver_->MakeAssignment();
  a->AddObjectives(obj);
  std::vector<SearchMonitor*> collectors;
  SolutionCollector* const c1 =
      solver_->MakeNBestLexicographicValueSolutionCollector(a, 3, maximize);
  collectors.push_back(c1);
  // More objectives in the assignment than in the collector.
  std::vector<bool> small_maximize = {true, false};
  SolutionCollector* const c1b =
      solver_->MakeNBestLexicographicValueSolutionCollector(a, 3,
                                                            small_maximize);
  collectors.push_back(c1b);
  // Fewer objectives in the assignment than in the collector.
  std::vector<bool> big_maximize = {true, false, false, true};
  SolutionCollector* const c1t =
      solver_->MakeNBestLexicographicValueSolutionCollector(a, 3, big_maximize);
  collectors.push_back(c1t);
  Assignment* const a2 = solver_->MakeAssignment();
  SolutionCollector* const c2 =
      solver_->MakeNBestLexicographicValueSolutionCollector(a2, 3, maximize);
  collectors.push_back(c2);
  SolutionCollector* const c3 =
      solver_->MakeNBestLexicographicValueSolutionCollector(3, maximize);
  collectors.push_back(c3);
  ASSERT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(3, c1->solution_count());
  EXPECT_EQ(3, c1b->solution_count());
  EXPECT_EQ(3, c1t->solution_count());
  EXPECT_EQ(0, c2->solution_count());
  EXPECT_EQ(0, c3->solution_count());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(5, c1->solution(i)->ObjectiveValueFromIndex(0));
    EXPECT_EQ(2, c1->solution(i)->ObjectiveValueFromIndex(1));
    EXPECT_EQ(4 - i, c1->solution(i)->ObjectiveValueFromIndex(2));
    EXPECT_EQ(5, c1->ObjectiveValueFromIndex(i, 0));
    EXPECT_EQ(2, c1->ObjectiveValueFromIndex(i, 1));
    EXPECT_EQ(4 - i, c1->ObjectiveValueFromIndex(i, 2));
  }
  EXPECT_EQ(50, c1->failures(0));
  EXPECT_EQ(102, c1->branches(0));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(5, c1b->solution(i)->ObjectiveValueFromIndex(0));
    EXPECT_EQ(2, c1b->solution(i)->ObjectiveValueFromIndex(1));
    EXPECT_EQ(5, c1b->ObjectiveValueFromIndex(i, 0));
    EXPECT_EQ(2, c1b->ObjectiveValueFromIndex(i, 1));
  }
  EXPECT_EQ(48, c1b->failures(0));
  EXPECT_EQ(98, c1b->branches(0));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(5, c1t->solution(i)->ObjectiveValueFromIndex(0));
    EXPECT_EQ(2, c1t->solution(i)->ObjectiveValueFromIndex(1));
    EXPECT_EQ(4 - i, c1t->solution(i)->ObjectiveValueFromIndex(2));
    EXPECT_EQ(5, c1t->ObjectiveValueFromIndex(i, 0));
    EXPECT_EQ(2, c1t->ObjectiveValueFromIndex(i, 1));
    EXPECT_EQ(4 - i, c1t->ObjectiveValueFromIndex(i, 2));
  }
  EXPECT_EQ(50, c1t->failures(0));
  EXPECT_EQ(102, c1t->branches(0));
  EXPECT_FALSE(c1->DebugString().empty());
  EXPECT_FALSE(c1b->DebugString().empty());
  EXPECT_FALSE(c1t->DebugString().empty());
  EXPECT_FALSE(c2->DebugString().empty());
  EXPECT_FALSE(c3->DebugString().empty());
}

TEST_F(SolutionCollectorTest, NBestValueSolutionCollectorWithLimitCosts) {
  std::vector<SearchMonitor*> collectors;
  IntVar* const var_max = solver_->MakeIntVar(kint64max, kint64max, "max");
  Assignment* const a_max = solver_->MakeAssignment();
  a_max->AddObjective(var_max);
  SolutionCollector* const c_max =
      solver_->MakeNBestValueSolutionCollector(a_max, 2, false);
  collectors.push_back(c_max);
  IntVar* const var_min = solver_->MakeIntVar(kint64min, kint64min, "min");
  Assignment* const a_min = solver_->MakeAssignment();
  a_min->AddObjective(var_min);
  SolutionCollector* const c_min =
      solver_->MakeNBestValueSolutionCollector(a_min, 2, true);
  collectors.push_back(c_min);
  DecisionBuilder* const db =
      solver_->MakePhase({var_min, var_max}, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MIN_VALUE);
  EXPECT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c_max->solution_count());
  EXPECT_EQ(kint64max, c_max->solution(0)->ObjectiveValue());
  EXPECT_EQ(1, c_min->solution_count());
  EXPECT_EQ(kint64min, c_min->solution(0)->ObjectiveValue());
}

TEST_F(SolutionCollectorTest,
       LexicographicNBestValueSolutionCollectorWithLimitCosts) {
  const int kNumObj = 3;
  std::vector<IntVar*> obj_max;
  solver_->MakeIntVarArray(kNumObj, kint64max, kint64max, &obj_max);
  std::vector<IntVar*> obj_min;
  solver_->MakeIntVarArray(kNumObj, kint64min, kint64min, &obj_min);
  std::vector<SearchMonitor*> collectors;
  Assignment* const a_max = solver_->MakeAssignment();
  a_max->AddObjectives(obj_max);
  SolutionCollector* const c_max =
      solver_->MakeNBestLexicographicValueSolutionCollector(
          a_max, 2, {false, false, false});
  collectors.push_back(c_max);
  Assignment* const a_min = solver_->MakeAssignment();
  a_min->AddObjectives(obj_min);
  SolutionCollector* const c_min =
      solver_->MakeNBestLexicographicValueSolutionCollector(a_min, 2,
                                                            {true, true, true});
  collectors.push_back(c_min);
  std::vector<IntVar*> all_vars = obj_min;
  all_vars.insert(all_vars.end(), obj_max.begin(), obj_max.end());
  DecisionBuilder* const db = solver_->MakePhase(
      all_vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  EXPECT_TRUE(solver_->Solve(db, collectors));
  EXPECT_EQ(1, c_max->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(kint64max, c_max->solution(0)->ObjectiveValueFromIndex(i));
  }
  EXPECT_EQ(1, c_min->solution_count());
  for (int i = 0; i < kNumObj; ++i) {
    EXPECT_EQ(kint64min, c_min->solution(0)->ObjectiveValueFromIndex(i));
  }
}

// ----- Optimize Var -----

class OptimizeVarTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("OptimizeVarTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(OptimizeVarTest, MinimizeVar) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* db = solver_->MakePhase(var, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  OptimizeVar* const opt = solver_->MakeMinimize(var, 2);
  int64_t optimal_value = kint64max;
  opt->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, opt));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(0, collector->solution(0)->Value(var));
  EXPECT_EQ(0, opt->best());
  EXPECT_EQ(opt->var(), var);
  EXPECT_FALSE(opt->DebugString().empty());
  EXPECT_EQ(optimal_value, 0);
}

TEST_F(OptimizeVarTest, LexicographicObjective) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(2, 0, 10, "var", &vars);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->AddObjectives(vars);
  SolutionCollector* const collector =
      solver_->MakeAllSolutionCollector(assignment);
  OptimizeVar* const objective =
      solver_->MakeLexicographicOptimize({false, true}, vars, {1, 2});
  int64_t optimal_value = kint64max;
  objective->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, objective));
  EXPECT_EQ(6, collector->solution_count());
  for (int i = 0; i < collector->solution_count(); ++i) {
    EXPECT_EQ(0, collector->solution(i)->ObjectiveValueFromIndex(0));
    EXPECT_EQ(2 * i, collector->solution(i)->ObjectiveValueFromIndex(1));
  }
  EXPECT_EQ(0, objective->BestValue(0));
  EXPECT_EQ(10, objective->BestValue(1));
  EXPECT_EQ(objective->ObjectiveVar(0), vars[0]);
  EXPECT_EQ(objective->ObjectiveVar(1), vars[1]);
  EXPECT_FALSE(objective->DebugString().empty());
  EXPECT_EQ(optimal_value, 0);
}

TEST_F(OptimizeVarTest, LexicographicTabuSearch) {
  IntVar* var0 = solver_->MakeIntVar(kint64min, kint64max, "var0");
  IntVar* var1 = solver_->MakeIntVar(0, 10, "var1");
  // var0 = (var1 - 1) * (var1 - 3) * (var1 - 5)
  solver_->AddConstraint(solver_->MakeEquality(
      var0, solver_->MakeProd(solver_->MakeSum(var1, -1),
                              solver_->MakeProd(solver_->MakeSum(var1, -3),
                                                solver_->MakeSum(var1, -5)))));
  DecisionBuilder* db = solver_->MakePhase(
      {var1, var0}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->AddObjectives({var0, var1});
  SolutionCollector* const collector =
      solver_->MakeAllSolutionCollector(assignment);
  ObjectiveMonitor* const objective = solver_->MakeLexicographicTabuSearch(
      {false, true}, {var0, var1}, {1, 1}, {var1}, 1, 1, 0.5);
  RegularLimit* const limit = solver_->MakeSolutionsLimit(20);
  ASSERT_TRUE(solver_->Solve(db, collector, objective, limit));
  EXPECT_EQ(8, collector->solution_count());
  const int kOptIterations = 7;
  for (int i = 0; i < collector->solution_count(); ++i) {
    if (i < kOptIterations) {
      EXPECT_EQ((9 - i) * (7 - i) * (5 - i),
                collector->solution(i)->ObjectiveValueFromIndex(0));
      EXPECT_EQ(10 - i, collector->solution(i)->ObjectiveValueFromIndex(1));
    } else {
      EXPECT_GE(-15, collector->solution(i)->ObjectiveValueFromIndex(0));
      EXPECT_LE(0, collector->solution(i)->ObjectiveValueFromIndex(1));
    }
  }
  EXPECT_EQ(-15, objective->BestValue(0));
  EXPECT_EQ(0, objective->BestValue(1));
  EXPECT_EQ(objective->ObjectiveVar(0), var0);
  EXPECT_EQ(objective->ObjectiveVar(1), var1);
  EXPECT_FALSE(objective->DebugString().empty());
}

TEST_F(OptimizeVarTest, LexicographicSimulatedAnnealing) {
  IntVar* var0 = solver_->MakeIntVar(kint64min, kint64max, "var0");
  IntVar* var1 = solver_->MakeIntVar(0, 10, "var1");
  // var0 = (var1 - 1) * (var1 - 3) * (var1 - 5)
  solver_->AddConstraint(solver_->MakeEquality(
      var0, solver_->MakeProd(solver_->MakeSum(var1, -1),
                              solver_->MakeProd(solver_->MakeSum(var1, -3),
                                                solver_->MakeSum(var1, -5)))));
  DecisionBuilder* db = solver_->MakePhase(
      {var1, var0}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->AddObjectives({var0, var1});
  SolutionCollector* const collector =
      solver_->MakeAllSolutionCollector(assignment);
  ObjectiveMonitor* const objective =
      solver_->MakeLexicographicSimulatedAnnealing({false, true}, {var0, var1},
                                                   {1, 1}, {10, 5});
  RegularLimit* const limit = solver_->MakeSolutionsLimit(20);
  ASSERT_TRUE(solver_->Solve(db, collector, objective, limit));
  EXPECT_EQ(8, collector->solution_count());
  const int kOptIterations = 7;
  for (int i = 0; i < collector->solution_count(); ++i) {
    if (i < kOptIterations) {
      EXPECT_EQ((9 - i) * (7 - i) * (5 - i),
                collector->solution(i)->ObjectiveValueFromIndex(0));
      EXPECT_EQ(10 - i, collector->solution(i)->ObjectiveValueFromIndex(1));
    } else {
      EXPECT_GE(-15, collector->solution(i)->ObjectiveValueFromIndex(0));
      EXPECT_LE(0, collector->solution(i)->ObjectiveValueFromIndex(1));
    }
  }
  EXPECT_EQ(-15, objective->BestValue(0));
  EXPECT_EQ(0, objective->BestValue(1));
  EXPECT_EQ(objective->ObjectiveVar(0), var0);
  EXPECT_EQ(objective->ObjectiveVar(1), var1);
  EXPECT_FALSE(objective->DebugString().empty());
}

TEST_F(OptimizeVarTest, MaximizeVar) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  OptimizeVar* const opt = solver_->MakeMaximize(var, 3);
  int64_t optimal_value = kint64max;
  opt->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, opt));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(9, collector->solution(0)->Value(var));
  EXPECT_EQ(9, opt->best());
  EXPECT_EQ(opt->var(), var);
  EXPECT_FALSE(opt->DebugString().empty());
  EXPECT_EQ(optimal_value, 9);
}

TEST_F(OptimizeVarTest, MaximizeVarWithFirstSolution) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeFirstSolutionCollector(a);
  OptimizeVar* const opt = solver_->MakeMaximize(var, 3);
  int64_t optimal_value = kint64max;
  opt->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, opt));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(0, collector->solution(0)->Value(var));
  EXPECT_EQ(0, collector->objective_value(0));
  EXPECT_EQ(9, opt->best());  // Search has not been stopped after the
                              // collector has found a solution.
  EXPECT_EQ(opt->var(), var);
  EXPECT_FALSE(opt->DebugString().empty());
  EXPECT_EQ(optimal_value, 9);
}

TEST_F(OptimizeVarTest, OptimizeVarFalse) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  OptimizeVar* const opt = solver_->MakeOptimize(false, var, 2);
  int64_t optimal_value = kint64max;
  opt->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, opt));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(0, collector->solution(0)->Value(var));
  EXPECT_EQ(0, opt->best());
  EXPECT_EQ(opt->var(), var);
  EXPECT_FALSE(opt->DebugString().empty());
  EXPECT_EQ(optimal_value, 0);
}

TEST_F(OptimizeVarTest, OptimizeVarTrue) {
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  OptimizeVar* const opt = solver_->MakeOptimize(true, var, 3);
  int64_t optimal_value = kint64max;
  opt->SetOnOptimalFoundcallback(
      [&optimal_value](int64_t value) { optimal_value = value; });
  ASSERT_TRUE(solver_->Solve(db, collector, opt));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(9, collector->solution(0)->Value(var));
  EXPECT_EQ(9, opt->best());
  EXPECT_EQ(opt->var(), var);
  EXPECT_FALSE(opt->DebugString().empty());
  EXPECT_EQ(optimal_value, 9);
}

// ----- WeightedOptimizeVarTest -----

template <class T>
class WeightedOptimizeVarTest : public OptimizeVarTest {
 protected:
  void SetUp() override {
    OptimizeVarTest::SetUp();
    int sign = -1;
    Assignment* const a = solver_->MakeAssignment();
    solver_->MakeIntVarArray(kNumVars, 0, kMaxValue, "v", &vars_);
    for (int i = 0; i < kNumVars; ++i) {
      const int64_t weight = sign * (i + 1) * (i + 1);
      weights_.push_back(weight);
      sign *= -1;
    }
    a->Add(vars_);
    db_ = solver_->MakePhase(vars_, Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_MIN_VALUE);
    collector_ = solver_->MakeLastSolutionCollector(a);
  }
  void CheckExpectedValues(OptimizeVar* const optimize_var,
                           int64_t positive_weight_expected_value,
                           int64_t negative_weights_expected_value,
                           bool check_positive_best) {
    ASSERT_TRUE(solver_->Solve(db_, collector_, optimize_var));
    ASSERT_EQ(1, collector_->solution_count());
    int64_t positive_best = 0;
    int64_t negative_best = 0;
    for (int i = 0; i < kNumVars; ++i) {
      if (weights_[i] > 0) {
        EXPECT_EQ(positive_weight_expected_value,
                  collector_->solution(0)->Value(vars_[i]));
        positive_best += kMaxValue * weights_[i];
      } else {
        EXPECT_EQ(negative_weights_expected_value,
                  collector_->solution(0)->Value(vars_[i]));
        negative_best += kMaxValue * weights_[i];
      }
    }
    EXPECT_EQ(check_positive_best ? positive_best : negative_best,
              optimize_var->best());
    EXPECT_FALSE(optimize_var->DebugString().empty());
  }

  std::vector<IntVar*> vars_;
  std::vector<T> weights_;
  SolutionCollector* collector_;
  DecisionBuilder* db_;
  static constexpr int kNumVars = 10;
  static constexpr int64_t kMaxValue = 10;
};

typedef testing::Types<int, int64_t> IntegerTypes;
TYPED_TEST_SUITE(WeightedOptimizeVarTest, IntegerTypes);

TYPED_TEST(WeightedOptimizeVarTest, MakeWeightedMaximize) {
  OptimizeVar* const opt = TestFixture::solver_->MakeWeightedMaximize(
      TestFixture::vars_, TestFixture::weights_, 1);
  TestFixture::CheckExpectedValues(opt, TestFixture::kMaxValue, 0, true);
}

TYPED_TEST(WeightedOptimizeVarTest, MakeWeightedMinimize) {
  OptimizeVar* const opt = TestFixture::solver_->MakeWeightedMinimize(
      TestFixture::vars_, TestFixture::weights_, 1);
  TestFixture::CheckExpectedValues(opt, 0, TestFixture::kMaxValue, false);
}

TYPED_TEST(WeightedOptimizeVarTest, WeightedOptimize) {
  OptimizeVar* const opt = TestFixture::solver_->MakeWeightedOptimize(
      true, TestFixture::vars_, TestFixture::weights_, 1);
  TestFixture::CheckExpectedValues(opt, TestFixture::kMaxValue, 0, true);
}

TYPED_TEST(WeightedOptimizeVarTest, DeathTest) {
  TestFixture::weights_.push_back(1LL);
  EXPECT_DEATH(TestFixture::solver_->MakeWeightedMinimize(
                   TestFixture::vars_, TestFixture::weights_, 1),
               "");
}

// ----- Search Limit -----

void ExpectLimits(RegularLimit* limit, absl::Duration time_limit,
                  int64_t branch_limit, int64_t failure_limit,
                  int64_t solution_limit) {
  EXPECT_EQ(time_limit, limit->duration_limit());
  EXPECT_EQ(branch_limit, limit->branches());
  EXPECT_EQ(failure_limit, limit->failures());
  EXPECT_EQ(solution_limit, limit->solutions());
}

class SearchLimitTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SearchLimitTest");
  }
  std::unique_ptr<Solver> solver_;
};

// A search limit mock.
class MockSearchLimit : public SearchLimit {
 public:
  explicit MockSearchLimit(Solver* solver) : SearchLimit(solver) {}
  MOCK_METHOD(bool, CheckWithOffset, (absl::Duration), (override));
  MOCK_METHOD(void, Init, (), (override));
  void Copy(const SearchLimit* const) override {}
  SearchLimit* MakeClone() const override { return nullptr; }
};

TEST_F(SearchLimitTest, Combination) {
  MockSearchLimit limit_1(solver_.get());
  MockSearchLimit limit_2(solver_.get());
  EXPECT_CALL(limit_1, Init()).WillOnce(DoDefault());
  EXPECT_CALL(limit_2, Init()).WillOnce(DoDefault());
  EXPECT_CALL(limit_1, CheckWithOffset(absl::ZeroDuration()))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(limit_2, CheckWithOffset(absl::ZeroDuration()))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  SearchLimit* limit = solver_->MakeLimit(&limit_1, &limit_2);

  limit->EnterSearch();          // Should call init for both limits
  EXPECT_FALSE(limit->Check());  // Both return false
  EXPECT_TRUE(limit->Check());   // Limit 2 returns true
  EXPECT_TRUE(limit->Check());   // Limit 1 returns true
  EXPECT_TRUE(limit->Check());   // Both return true
  EXPECT_FALSE(limit->Check());  // Both returns false
}

bool StopAtCountLimit(IntVar* const var, int64_t value) {
  return var->Bound() && var->Value() > value;
}

void TestSearchLimit(SearchLimit* limit, IntVar* var, int limit_value) {
  Solver* const solver = limit->solver();
  DecisionBuilder* const db = solver->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver->MakeLastSolutionCollector(a);
  ASSERT_TRUE(solver->Solve(db, collector, limit));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(limit_value, collector->solution(0)->Value(var));
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_TRUE(limit->crossed());
}

TEST_F(SearchLimitTest, CustomLimit) {
  const int64_t kLimitValue = 5;
  IntVar* const var = solver_->MakeIntVar(0, 10, "var");
  TestSearchLimit(solver_->MakeCustomLimit(
                      [var] { return StopAtCountLimit(var, kLimitValue); }),
                  var, kLimitValue);
  TestSearchLimit(
      solver_->MakeCustomLimit(std::bind(StopAtCountLimit, var, kLimitValue)),
      var, kLimitValue);
  TestSearchLimit(solver_->MakeCustomLimit([var]() {
    const int64_t kLimitValue = 5;
    return StopAtCountLimit(var, kLimitValue);
  }),
                  var, kLimitValue);
}

class RegularLimitTest : public SearchLimitTest {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("RegularLimitTest");
  }
  void TestBranchLimit(RegularLimit* limit, int branch_limit);
  void TestTimeLimit(RegularLimit* limit, int num_solutions);
  void TestFailureLimit(RegularLimit* limit, int failure_limit);
  void TestSolutionLimit(RegularLimit* limit, int solution_limit);
};

void RegularLimitTest::TestSolutionLimit(RegularLimit* limit,
                                         int solution_limit) {
  ExpectLimits(limit, absl::InfiniteDuration(), kint64max, kint64max,
               solution_limit);
  IntVar* const var = solver_->MakeIntVar(0, 2 * (solution_limit + 1), "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeAllSolutionCollector(a);
  bool solution_found = solver_->Solve(db, collector, limit);
  EXPECT_EQ(solution_limit > 0, solution_found);
  EXPECT_EQ(solution_limit, collector->solution_count());
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_TRUE(limit->crossed());
}

void RegularLimitTest::TestBranchLimit(RegularLimit* limit, int branch_limit) {
  ExpectLimits(limit, absl::InfiniteDuration(), branch_limit, kint64max,
               kint64max);
  IntVar* const var = solver_->MakeIntVar(0, 2 * (branch_limit + 1), "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);
  bool solution_found = solver_->Solve(db, collector, limit);
  if (branch_limit > 0) {
    EXPECT_TRUE(solution_found);
    EXPECT_EQ(1, collector->solution_count());
    EXPECT_EQ(branch_limit, solver_->branches());
  } else {
    EXPECT_FALSE(solution_found);
    EXPECT_EQ(0, collector->solution_count());
  }
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_TRUE(limit->crossed());
}

TEST_F(RegularLimitTest, BranchLimitZero) {
  const int kBranchLimit = 0;
  RegularLimit* const limit = solver_->MakeBranchesLimit(kBranchLimit);
  TestBranchLimit(limit, kBranchLimit);
}

TEST_F(RegularLimitTest, BranchLimitSeven) {
  const int kBranchLimit = 7;
  RegularLimit* const limit = solver_->MakeBranchesLimit(kBranchLimit);
  TestBranchLimit(limit, kBranchLimit);
}

TEST_F(RegularLimitTest, SolutionLimitZero) {
  const int kSolutionLimit = 0;
  RegularLimit* const limit = solver_->MakeSolutionsLimit(kSolutionLimit);
  TestSolutionLimit(limit, kSolutionLimit);
}

TEST_F(RegularLimitTest, SolutionLimitThree) {
  const int kSolutionLimit = 3;
  RegularLimit* const limit = solver_->MakeSolutionsLimit(kSolutionLimit);
  TestSolutionLimit(limit, kSolutionLimit);
}

TEST_F(RegularLimitTest, Cumulative) {
  const int kSolutionLimit = 5;
  IntVar* var = solver_->MakeIntVar(0, 2);
  Assignment* assignment = solver_->MakeAssignment();
  assignment->Add(var);
  SolutionCollector* collector = solver_->MakeAllSolutionCollector(assignment);
  RegularLimit* const limit = solver_->MakeLimit(
      absl::InfiniteDuration(), kint64max, kint64max, kSolutionLimit,
      /*smart_time_check=*/false, /*cumulative=*/true);
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  // A first search, with the limit
  bool solution_found = solver_->Solve(db, collector, limit);
  EXPECT_TRUE(solution_found);
  EXPECT_EQ(3, collector->solution_count());
  // A second search, without the limit
  collector = solver_->MakeAllSolutionCollector(assignment);
  solution_found = solver_->Solve(db, collector);
  EXPECT_TRUE(solution_found);
  EXPECT_EQ(3, collector->solution_count());
  // A last search, with the limit. This one should hit the limit
  collector = solver_->MakeAllSolutionCollector(assignment);
  solution_found = solver_->Solve(db, collector, limit);
  EXPECT_TRUE(solution_found);
  EXPECT_EQ(kSolutionLimit - 3, collector->solution_count());
}

class AdvanceClock : public SearchMonitor {
 public:
  AdvanceClock(Solver* const solver, absl::SimulatedClock* const clock)
      : SearchMonitor(solver), clock_(clock) {}
  ~AdvanceClock() override {}
  bool AtSolution() override {
    const absl::Duration kOneSecond = absl::Seconds(1);
    clock_->AdvanceTime(kOneSecond);
    return true;
  }

 private:
  absl::SimulatedClock* const clock_;
};

// Test the time limit. The given limit is expected to have a time limit
// equal to num_solutions * 1000.
void RegularLimitTest::TestTimeLimit(RegularLimit* limit, int num_solutions) {
  absl::SimulatedClock clock;
  solver_->SetClock(&clock);
  IntVar* const var = solver_->MakeIntVar(0, 2 * num_solutions, "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->Add(var);
  SolutionCollector* const collector =
      solver_->MakeAllSolutionCollector(assignment);

  AdvanceClock* const advance_clock =
      solver_->RevAlloc(new AdvanceClock(solver_.get(), &clock));
  ASSERT_TRUE(solver_->Solve(db, collector, limit, advance_clock));
  EXPECT_EQ(num_solutions, collector->solution_count());
  for (int i = 0; i < num_solutions; ++i) {
    EXPECT_EQ(i, collector->solution(i)->Value(var));
  }
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_TRUE(limit->crossed());
}

void RegularLimitTest::TestFailureLimit(RegularLimit* limit,
                                        int failure_limit) {
  ExpectLimits(limit, absl::InfiniteDuration(), kint64max, failure_limit,
               kint64max);
  IntVar* const var = solver_->MakeIntVar(0, 2 * (failure_limit + 1), "var");
  DecisionBuilder* const db = solver_->MakePhase(
      var, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  Assignment* const a = solver_->MakeAssignment();
  a->Add(var);
  SolutionCollector* const collector = solver_->MakeLastSolutionCollector(a);

  ASSERT_TRUE(solver_->Solve(db, collector, limit));
  EXPECT_EQ(1, collector->solution_count());
  EXPECT_EQ(failure_limit - 1, collector->solution(0)->Value(var));
  EXPECT_EQ(failure_limit - 1, collector->failures(0));
  EXPECT_EQ(failure_limit + 1, solver_->failures());
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_TRUE(limit->crossed());
}

TEST_F(RegularLimitTest, TimeLimit) {
  const int kNumberOfSolutions = 5;
  const absl::Duration kTimeLimit = absl::Seconds(kNumberOfSolutions);
  RegularLimit* const limit = solver_->MakeTimeLimit(kTimeLimit);
  ExpectLimits(limit, kTimeLimit, kint64max, kint64max, kint64max);
  TestTimeLimit(limit, kNumberOfSolutions);
}

TEST_F(RegularLimitTest, FailureLimit) {
  const int kFailureLimit = 4;
  RegularLimit* const limit = solver_->MakeFailuresLimit(kFailureLimit);
  TestFailureLimit(limit, kFailureLimit);
}

class MyCollector : public SearchMonitor {
 public:
  MyCollector(Solver* const s, IntVar* const var1, IntVar* const var2)
      : SearchMonitor(s), var1_(var1), var2_(var2) {}
  ~MyCollector() override {}

  bool AcceptSolution() override {
    const int64_t val1 = var1_->Value();
    const int64_t val2 = var2_->Value();
    return (val2 < 2 * val1 + 2);
  }

  bool AtSolution() override {
    const int64_t val1 = var1_->Value();
    const int64_t val2 = var2_->Value();
    pool_.insert(std::make_pair(val1, val2));
    return false;
  }

  const absl::flat_hash_set<std::pair<int64_t, int64_t>>& pool() const {
    return pool_;
  }

 private:
  absl::flat_hash_set<std::pair<int64_t, int64_t>> pool_;
  IntVar* const var1_;
  IntVar* const var2_;
};

TEST_F(RegularLimitTest, SolveOnceTest) {
  IntVar* const var1 = solver_->MakeIntVar(0, 10, "var1");
  IntVar* const var2 = solver_->MakeIntVar(0, 10, "var2");
  DecisionBuilder* const db1 = solver_->MakePhase(
      var1, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db2 = solver_->MakePhase(
      var2, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SearchLimit* const limit1 =
      solver_->MakeLimit(absl::InfiniteDuration(), kint64max, 40, kint64max);
  SearchLimit* const limit2 =
      solver_->MakeLimit(absl::InfiniteDuration(), kint64max, 15, kint64max);
  OptimizeVar* const max1 = solver_->MakeMaximize(var1, 1);
  OptimizeVar* const max2 = solver_->MakeMaximize(var2, 1);
  SearchMonitor* const trace1 = solver_->MakeSearchTrace("outer");
  SearchMonitor* const trace2 = solver_->MakeSearchTrace("inner");
  MyCollector* const collector =
      solver_->RevAlloc(new MyCollector(solver_.get(), var1, var2));
  DecisionBuilder* const inner =
      solver_->MakeSolveOnce(db2, max2, collector, limit2, trace2);

  solver_->Solve(solver_->Compose(db1, inner), max1, limit1, trace1);
  const absl::flat_hash_set<std::pair<int64_t, int64_t>>& pool =
      collector->pool();
  // The nested search has a fail limit of 15 (and this limit is actually never
  // reached because its search space is of size 10), the outer search has one
  // of 40. The first time the nested search is called it finds 2 solutions and
  // fails 12 times (10 = the size of the domain of var2, + 2 = backtracking
  // out into the outer search); on the second call, finds 4 solutions and fails
  // 12 times, on the third 6 solutions, 12 fails. At this point we have a total
  // of 36 fails. Now the search is stopped when reaching 40 fails, which allows
  // the nested search to only find 4 (= 40-36) extra solutions (total of 16).
  EXPECT_EQ(16, pool.size());
  EXPECT_TRUE(pool.contains(std::make_pair(0, 0)));
  EXPECT_TRUE(pool.contains(std::make_pair(0, 1)));
  EXPECT_TRUE(pool.contains(std::make_pair(1, 0)));
  EXPECT_TRUE(pool.contains(std::make_pair(1, 1)));
  EXPECT_TRUE(pool.contains(std::make_pair(1, 2)));
  EXPECT_TRUE(pool.contains(std::make_pair(1, 3)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 0)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 1)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 2)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 3)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 4)));
  EXPECT_TRUE(pool.contains(std::make_pair(2, 5)));
  EXPECT_TRUE(pool.contains(std::make_pair(3, 0)));
  EXPECT_TRUE(pool.contains(std::make_pair(3, 1)));
  EXPECT_TRUE(pool.contains(std::make_pair(3, 2)));
  EXPECT_TRUE(pool.contains(std::make_pair(3, 3)));
}

TEST_F(RegularLimitTest, ProtobufDefault) {
  RegularLimitParameters proto = solver_->MakeDefaultRegularLimitParameters();
  SearchLimit* const limit = solver_->MakeLimit(proto);
  EXPECT_FALSE(limit->DebugString().empty());
  EXPECT_FALSE(limit->Check());
}

TEST_F(RegularLimitTest, ProtobufTimeLimit) {
  const int kNumberOfSolutions = 3;
  const int64_t kTimeLimitMs = kNumberOfSolutions * 1000;
  RegularLimitParameters proto = solver_->MakeDefaultRegularLimitParameters();
  proto.set_time(kTimeLimitMs);
  RegularLimit* const limit = solver_->MakeLimit(proto);
  TestTimeLimit(limit, kNumberOfSolutions);
}

TEST_F(RegularLimitTest, ProtobufBranchLimit) {
  const int kBranchLimit = 4;
  RegularLimitParameters proto = solver_->MakeDefaultRegularLimitParameters();
  proto.set_branches(kBranchLimit);
  RegularLimit* const limit = solver_->MakeLimit(proto);
  TestBranchLimit(limit, kBranchLimit);
}

TEST_F(RegularLimitTest, ProtobufSolutionLimit) {
  const int kSolutionLimit = 2;
  RegularLimitParameters proto = solver_->MakeDefaultRegularLimitParameters();
  proto.set_solutions(kSolutionLimit);
  RegularLimit* const limit = solver_->MakeLimit(proto);
  TestSolutionLimit(limit, kSolutionLimit);
}

TEST_F(RegularLimitTest, ProtobufFailureLimit) {
  const int kFailureLimit = 9;
  RegularLimitParameters proto = solver_->MakeDefaultRegularLimitParameters();
  proto.set_failures(kFailureLimit);
  RegularLimit* const limit = solver_->MakeLimit(proto);
  TestFailureLimit(limit, kFailureLimit);
}

// ----- Search Improvement Limit -----

class ImprovementLimitTest : public SearchLimitTest {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("ImprovementLimitTest");
  }
  void TestImprovementLimit(const std::vector<int64_t>& values,
                            int64_t expected_result, bool maximize,
                            double objective_scaling_factor,
                            double objective_offset,
                            double improvement_rate_coefficient,
                            int improvement_rate_solutions_distance);
};

void ImprovementLimitTest::TestImprovementLimit(
    const std::vector<int64_t>& values, int64_t expected_result, bool maximize,
    double objective_scaling_factor, double objective_offset,
    double improvement_rate_coefficient,
    int improvement_rate_solutions_distance) {
  IntVar* const index = solver_->MakeIntVar(0, 10, "obj");
  IntVar* const objective = solver_->MakeElement(values, index)->Var();
  std::vector<IntVar*> vars = {index};

  DecisionBuilder* decision_builder = solver_->MakeLocalSearchPhase(
      vars,
      solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MIN_VALUE),
      solver_->MakeLocalSearchPhaseParameters(
          objective, solver_->MakeOperator(vars, Solver::INCREMENT),
          solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_MIN_VALUE)));

  SearchLimit* limit = solver_->MakeImprovementLimit(
      objective, maximize, objective_scaling_factor, objective_offset,
      improvement_rate_coefficient, improvement_rate_solutions_distance);

  SearchLimit* copied_limit = solver_->MakeImprovementLimit(
      objective, maximize, objective_scaling_factor, objective_offset,
      /*improvement_rate_coefficient=*/0,
      /*improvement_rate_solutions_distance=*/1);
  copied_limit->Copy(limit);

  SearchLimit* cloned_limit = copied_limit->MakeClone();

  for (SearchLimit* const search_limit : {limit, copied_limit, cloned_limit}) {
    Assignment* assignment = solver_->MakeAssignment();
    assignment->AddObjective(objective);
    SolutionCollector* collector =
        solver_->MakeLastSolutionCollector(assignment);

    EXPECT_TRUE(solver_->Solve(decision_builder, collector, search_limit));
    EXPECT_EQ(expected_result, collector->objective_value(0));
  }
}

TEST_F(ImprovementLimitTest, MinimizeImprovementLimit) {
  const std::vector<int64_t> values = {100, 98, 96, 93, 93, 80,
                                       79,  78, 50, 10, 0};

  const std::vector<int64_t> expected_results = {80, 78, 0, 0, 0};
  const std::vector<double> improvement_rate_coefficients = {0.1, 1, 10, 0.01,
                                                             2};
  const std::vector<int> improvement_rate_solutions_distance = {1, 2, 3, 4, 1};
  for (int i = 0; i < expected_results.size(); i++) {
    TestImprovementLimit(values, expected_results[i], /*maximize=*/false,
                         /*objective_scaling_factor=*/1, /*objective_offset=*/0,
                         improvement_rate_coefficients[i],
                         improvement_rate_solutions_distance[i]);
  }
}

TEST_F(ImprovementLimitTest, MaximizeImprovementLimit) {
  const std::vector<int64_t> values = {9, 11, 13, 9, 8, 7, 6, 5, 25, 26, 1000};

  const std::vector<int64_t> expected_results = {25, 26, 1000, 1000};
  const std::vector<double> improvement_rate_coefficients = {0.1, 1.9, 2, 0.01};
  const std::vector<int> improvement_rate_solutions_distance = {2, 1, 2, 3};
  for (int i = 0; i < expected_results.size(); i++) {
    TestImprovementLimit(values, expected_results[i], /*maximize=*/true,
                         /*objective_scaling_factor=*/1,
                         /*objective_offset=*/0,
                         improvement_rate_coefficients[i],
                         improvement_rate_solutions_distance[i]);
  }
}
class LexicographicImprovementLimitTest : public SearchLimitTest {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("LexicographicImprovementLimitTest");
  }
  void TestImprovementLimit(const std::vector<std::vector<int64_t>>& values,
                            const std::vector<int64_t>& expected_results,
                            const std::vector<bool>& maximize,
                            const std::vector<double>& objective_scaling_factor,
                            const std::vector<double>& objective_offset,
                            double improvement_rate_coefficient,
                            int improvement_rate_solutions_distance);
};

class AssignmentSequence : public DecisionBuilder {
 public:
  AssignmentSequence(const std::vector<IntVar*>& vars,
                     const std::vector<std::vector<int64_t>>& values)
      : DecisionBuilder(), vars_(vars), values_(values), step_(0) {
    CHECK(!vars.empty());
  }
  Decision* Next(Solver* solver) override {
    if (vars_.back()->Bound()) return nullptr;
    if (step_ >= values_.size()) solver->Fail();
    if (step_ > 0) solver->IncrementNeighbors();
    return solver->MakeAssignVariablesValuesOrDoNothing(vars_,
                                                        values_[step_++]);
  }

 private:
  std::vector<IntVar*> vars_;
  std::vector<std::vector<int64_t>> values_;
  int step_;
};

void LexicographicImprovementLimitTest::TestImprovementLimit(
    const std::vector<std::vector<int64_t>>& values,
    const std::vector<int64_t>& expected_results,
    const std::vector<bool>& maximize,
    const std::vector<double>& objective_scaling_factors,
    const std::vector<double>& objective_offsets,
    double improvement_rate_coefficient,
    int improvement_rate_solutions_distance) {
  std::vector<IntVar*> objectives;
  solver_->MakeIntVarArray(maximize.size(), 0, 5000, &objectives);

  SearchLimit* limit = solver_->MakeLexicographicImprovementLimit(
      objectives, maximize, objective_scaling_factors, objective_offsets,
      improvement_rate_coefficient, improvement_rate_solutions_distance);

  SearchLimit* copied_limit = solver_->MakeLexicographicImprovementLimit(
      objectives, maximize, objective_scaling_factors, objective_offsets,
      /*improvement_rate_coefficient=*/0,
      /*improvement_rate_solutions_distance=*/1);
  copied_limit->Copy(limit);

  SearchLimit* cloned_limit = copied_limit->MakeClone();

  for (SearchLimit* const search_limit : {limit, copied_limit, cloned_limit}) {
    solver_->ClearNeighbors();
    Assignment* assignment = solver_->MakeAssignment();
    assignment->AddObjectives(objectives);
    SolutionCollector* collector =
        solver_->MakeLastSolutionCollector(assignment);
    DecisionBuilder* decision_builder =
        solver_->RevAlloc(new AssignmentSequence(objectives, values));
    EXPECT_TRUE(solver_->Solve(decision_builder, collector, search_limit));
    for (int i = 0; i < maximize.size(); i++) {
      EXPECT_EQ(expected_results[i], collector->ObjectiveValueFromIndex(0, i));
    }
  }
}

// Mixing minimization and maximization in lexicographic improvements.
TEST_F(LexicographicImprovementLimitTest, LexicographicImprovementLimit) {
  // Descent phase ends at (900, 10) with a min threshold of (0, 10) for
  // distance 1 and (50, 5) for distance 2.
  const std::vector<std::vector<int64_t>> values = {
      {1000, 20}, {1000, 30}, {900, 10}, {1000, 10},
      {800, 5},   {2000, 25}, {1500, 25}};
  const std::vector<bool> maximize = {false, true};
  const std::vector<std::vector<int64_t>> expected_results = {{1500, 25},
                                                              {800, 5}};
  const std::vector<double> improvement_rate_coefficients = {1, 0.1};
  const std::vector<int> improvement_rate_solutions_distance = {1, 2};
  for (int i = 0; i < expected_results.size(); i++) {
    TestImprovementLimit(values, expected_results[i], maximize,
                         /*objective_scaling_factor=*/{1, 1},
                         /*objective_offset=*/{0, 0},
                         improvement_rate_coefficients[i],
                         improvement_rate_solutions_distance[i]);
  }
}

// First objective does not change, applying maximization to the second
// objective.
TEST_F(LexicographicImprovementLimitTest,
       LexicographicImprovementLimitMaximize) {
  const std::vector<std::vector<int64_t>> values = {
      {100, 9}, {100, 11}, {100, 13}, {100, 9},  {100, 8},   {100, 7},
      {100, 6}, {100, 5},  {100, 25}, {100, 26}, {100, 1000}};

  const std::vector<std::vector<int64_t>> expected_results = {
      {100, 25}, {100, 26}, {100, 1000}, {100, 1000}};
  const std::vector<bool> maximize = {false, true};
  const std::vector<double> improvement_rate_coefficients = {0.1, 1.9, 2, 0.01};
  const std::vector<int> improvement_rate_solutions_distance = {2, 1, 2, 3};
  for (int i = 0; i < expected_results.size(); i++) {
    TestImprovementLimit(values, expected_results[i], maximize,
                         /*objective_scaling_factor=*/{1, 1},
                         /*objective_offset=*/{0, 0},
                         improvement_rate_coefficients[i],
                         improvement_rate_solutions_distance[i]);
  }
}

// ----- NestedOptimize -----

namespace {
class TryTwoValues : public Decision {
 public:
  TryTwoValues(IntVar* const variable, int64_t value_a, int64_t value_b)
      : variable_(variable), value_a_(value_a), value_b_(value_b) {}
  void Apply(Solver* const) override { variable_->SetValue(value_a_); }
  void Refute(Solver* const) override { variable_->SetValue(value_b_); }

  std::string DebugString() const override {
    return absl::StrFormat("TryTwoValues(%d, %d)", value_a_, value_b_);
  }

 private:
  IntVar* const variable_;
  const int64_t value_a_;  // First value to try
  const int64_t value_b_;  // Second value to try
};

class SwapValuesDecisionBuilder : public DecisionBuilder {
 public:
  SwapValuesDecisionBuilder(IntVar* const* vars, int size)
      : vars_(vars), size_(size) {}
  Decision* Next(Solver* const s) override {
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        return s->RevAlloc(new TryTwoValues(vars_[i], i, size_ - i - 1));
      }
    }
    return nullptr;
  }
  std::string DebugString() const override {
    return "SwapValuesDecisionBuilder";
  }

 private:
  IntVar* const* vars_;
  const int size_;
};

// A Lns that only produces a single, precomputed, fragment.
class SingleUseLns : public BaseLns {
 public:
  SingleUseLns(const std::vector<IntVar*>& vars,
               const std::vector<bool>& to_release)
      : BaseLns(vars), to_release_(to_release), first_call_(true) {}

  bool NextFragment() override {
    if (first_call_) {
      // Relax all
      for (int i = 0; i < Size(); ++i) {
        if (to_release_[i]) {
          AppendToFragment(i);
        }
      }
      first_call_ = false;
      return true;
    } else {
      return false;
    }
  }

 private:
  const std::vector<bool>& to_release_;
  bool first_call_;
};
}  // namespace

class NestedOptimizeTest : public testing::Test {
 public:
  static constexpr int kNumVariables = 4;

 protected:
  void Solve(const std::vector<bool>& to_release, bool maximize) {
    // Prepare for the search
    DecisionBuilder* const swap = solver_->RevAlloc(
        new SwapValuesDecisionBuilder(variables_.data(), variables_.size()));

    SingleUseLns lns(variables_, to_release);
    DecisionBuilder* const inner =
        solver_->MakeNestedOptimize(swap, assignment_, maximize, 1);
    LocalSearchPhaseParameters* ls_params =
        solver_->MakeLocalSearchPhaseParameters(objective_, &lns, inner);
    DecisionBuilder* const decision_builder =
        solver_->MakeLocalSearchPhase(assignment_, ls_params);
    // Search
    solver_->Solve(decision_builder, collector_);
  }

  void SetUp() override {
    solver_ = std::make_unique<Solver>("NestedOptimizeTest");
    assignment_ = solver_->MakeAssignment();

    // Creates a simple model
    solver_->MakeIntVarArray(kNumVariables, 0, kNumVariables - 1, "var",
                             &variables_);
    solver_->AddConstraint(solver_->MakeAllDifferent(variables_, false));

    // Create an objective: Obj = Sum_i(i * var[i])
    std::vector<int64_t> weights;
    for (int i = 0; i < kNumVariables; ++i) {
      weights.push_back(i);
    }
    objective_ = solver_->MakeScalProd(variables_, weights)->Var();

    // Assignment
    for (int i = 0; i < kNumVariables; ++i) {
      assignment_->Add(variables_[i]);
      // We set the worst feasible values
      assignment_->SetValue(variables_[i], i);
    }
    assignment_->AddObjective(objective_);
    collector_ = solver_->MakeLastSolutionCollector(assignment_);
  }

  void TearDown() override { variables_.clear(); }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> variables_;
  Assignment* assignment_;
  IntVar* objective_;
  SolutionCollector* collector_;
  DecisionBuilder* decision_builder_;
};

TEST_F(NestedOptimizeTest, NoFreedom) {
  std::vector<bool> to_release(NestedOptimizeTest::kNumVariables);
  Solve(to_release, false);
  ASSERT_EQ(1, collector_->solution_count());
  EXPECT_EQ(14, collector_->objective_value(0));
}

TEST_F(NestedOptimizeTest, MinimizeFull) {
  std::vector<bool> to_release(NestedOptimizeTest::kNumVariables, true);
  Solve(to_release, false);
  ASSERT_EQ(1, collector_->solution_count());
  EXPECT_EQ(4, collector_->objective_value(0));
}

TEST_F(NestedOptimizeTest, MaximizeFull) {
  std::vector<bool> to_release(NestedOptimizeTest::kNumVariables, true);
  Solve(to_release, true);
  ASSERT_EQ(1, collector_->solution_count());
  EXPECT_EQ(14, collector_->objective_value(0));
}

TEST_F(NestedOptimizeTest, MinimizePartial) {
  std::vector<bool> to_release(NestedOptimizeTest::kNumVariables, true);
  to_release[0] = false;
  to_release.back() = false;
  Solve(to_release, false);
  ASSERT_EQ(1, collector_->solution_count());
  EXPECT_EQ(13, collector_->objective_value(0));
}

// ----- Branch Selector -----

class BranchSelectorTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BranchSelectorTest");
  }
  std::unique_ptr<Solver> solver_;
};

Solver::DecisionModification LeftDive() { return Solver::KEEP_LEFT; }

Solver::DecisionModification RightDive() { return Solver::KEEP_RIGHT; }

Solver::DecisionModification KillBoth() { return Solver::KILL_BOTH; }

Solver::DecisionModification SwitchBranches() {
  return Solver::SWITCH_BRANCHES;
}

Solver::DecisionModification NormalSearch() { return Solver::NO_CHANGE; }

TEST_F(BranchSelectorTest, LeftDive) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* const apply = solver_->MakeApplyBranchSelector(LeftDive);
  DecisionBuilder* const search = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = solver_->Compose(apply, search);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, v1->Value());
  EXPECT_EQ(0, v2->Value());
  EXPECT_EQ(0, v3->Value());
  EXPECT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

TEST_F(BranchSelectorTest, RightDive) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* const apply = solver_->MakeApplyBranchSelector(RightDive);
  DecisionBuilder* const search = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = solver_->Compose(apply, search);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(10, v1->Value());
  EXPECT_EQ(10, v2->Value());
  EXPECT_EQ(10, v3->Value());
  EXPECT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

TEST_F(BranchSelectorTest, KillBoth) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* const apply = solver_->MakeApplyBranchSelector(KillBoth);
  DecisionBuilder* const search = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = solver_->Compose(apply, search);
  solver_->NewSearch(db);
  ASSERT_FALSE(solver_->NextSolution());
  solver_->EndSearch();
}

TEST_F(BranchSelectorTest, NormalSearch) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* const apply = solver_->MakeApplyBranchSelector(NormalSearch);
  DecisionBuilder* const search = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = solver_->Compose(apply, search);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, v1->Value());
  EXPECT_EQ(0, v2->Value());
  EXPECT_EQ(0, v3->Value());
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, v1->Value());
  EXPECT_EQ(0, v2->Value());
  EXPECT_EQ(1, v3->Value());
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(0, v1->Value());
  EXPECT_EQ(0, v2->Value());
  EXPECT_EQ(2, v3->Value());
  solver_->EndSearch();
}

TEST_F(BranchSelectorTest, SwitchBranches) {
  IntVar* const v1 = solver_->MakeIntVar(0, 10, "v1");
  IntVar* const v2 = solver_->MakeIntVar(0, 10, "v2");
  IntVar* const v3 = solver_->MakeIntVar(0, 10, "v3");
  DecisionBuilder* const apply =
      solver_->MakeApplyBranchSelector(SwitchBranches);
  DecisionBuilder* const search = solver_->MakePhase(
      v1, v2, v3, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = solver_->Compose(apply, search);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(10, v1->Value());
  EXPECT_EQ(10, v2->Value());
  EXPECT_EQ(10, v3->Value());
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(10, v1->Value());
  EXPECT_EQ(10, v2->Value());
  EXPECT_EQ(9, v3->Value());
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(10, v1->Value());
  EXPECT_EQ(10, v2->Value());
  EXPECT_EQ(8, v3->Value());
  solver_->EndSearch();
}

// ----- Luby Restart -----

namespace {
const int kLubyFirstResults[] = {1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8,
                                 1, 1, 2, 1, 1, 2, 4, 1, 1, 2, 1, 1, 2, 4, 8};
}  // namespace
class LubyRestartTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("LubyRestartTest");
  }
  std::unique_ptr<Solver> solver_;
};

class LubyTest : public SearchMonitor {
 public:
  LubyTest(Solver* const s, int scale, int fail_limit)
      : SearchMonitor(s),
        scale_(scale),
        fail_limit_(fail_limit),
        previous_(0) {}
  ~LubyTest() override {}

  void BeginFail() override {
    if (solver()->failures() > fail_limit_) {
      solver()->FinishCurrentSearch();
    }
  }

  void RestartSearch() override {
    const int64_t failures = solver()->failures();
    lengths_.push_back(failures - previous_);
    previous_ = failures;
  }
  void ExitSearch() override {
    for (int i = 0; i < lengths_.size(); ++i) {
      EXPECT_EQ(kLubyFirstResults[i] * scale_, lengths_[i]);
    }
  }
  bool AtSolution() override { return true; }

 private:
  const int scale_;
  const int fail_limit_;
  std::vector<int> lengths_;
  int previous_;
};

TEST_F(LubyRestartTest, Strategy) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* const luby = solver_->MakeLubyRestart(2);
  SearchMonitor* const luby_test =
      solver_->RevAlloc(new LubyTest(solver_.get(), 2, 100));
  EXPECT_TRUE(solver_->Solve(db, luby, luby_test));
}

TEST_F(LubyRestartTest, Parameter) {
  EXPECT_DEATH(solver_->MakeLubyRestart(-1), "");
}

// ----- Constant Restart -----

class ConstantRestartTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("ConstantRestartTest");
  }
  std::unique_ptr<Solver> solver_;
};

class ConstantTest : public SearchMonitor {
 public:
  ConstantTest(Solver* const s, int scale, int fail_limit)
      : SearchMonitor(s), scale_(scale), fail_limit_(fail_limit) {}
  ~ConstantTest() override {}

  void BeginFail() override {
    if (solver()->failures() > fail_limit_) {
      solver()->FinishCurrentSearch();
    }
  }

  void RestartSearch() override { lengths_.push_back(solver()->failures()); }
  void ExitSearch() override {
    for (int i = 0; i < lengths_.size(); ++i) {
      EXPECT_EQ(i * scale_ + scale_, lengths_[i]);
    }
  }
  bool AtSolution() override { return true; }

 private:
  const int scale_;
  const int fail_limit_;
  std::vector<int> lengths_;
};

TEST_F(ConstantRestartTest, Strategy) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(10, 0, 10, "vars", &vars);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* const constant = solver_->MakeConstantRestart(5);
  SearchMonitor* const constant_test =
      solver_->RevAlloc(new ConstantTest(solver_.get(), 5, 100));
  EXPECT_TRUE(solver_->Solve(db, constant, constant_test));
}

TEST_F(ConstantRestartTest, Parameter) {
  EXPECT_DEATH(solver_->MakeConstantRestart(-1), "");
}

// ----- SymmetryManagerTest -----

class SymmetryManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SymmetryManagerTest");
  }
  std::unique_ptr<Solver> solver_;
};

class X1X4Symmetry : public SymmetryBreaker {
 public:
  X1X4Symmetry(Solver* const, const std::vector<IntVar*>& vars) {
    for (IntVar* var : vars) {
      CHECK(vars_.TryEmplace(var).second) << "Duplicate var: " << var->name();
    }
  }

  ~X1X4Symmetry() override {}

  void VisitSetVariableValue(IntVar* var, int64_t value) override {
    AddIntegerVariableEqualValueClause(GetSymmetricVar(var), value);
  }

  void VisitSplitVariableDomain(IntVar* var, int64_t value,
                                bool start_with_lower_half) override {
    IntVar* const other_var = GetSymmetricVar(var);
    if (start_with_lower_half) {
      AddIntegerVariableLessOrEqualValueClause(other_var, value);
    } else {
      AddIntegerVariableGreaterOrEqualValueClause(other_var, value + 1);
    }
  }

 private:
  IntVar* GetSymmetricVar(IntVar* var) {
    const std::optional<int> index = vars_.Lookup(var);
    DCHECK(index.has_value());
    return vars_[vars_.size() - 1 - index.value()];
  }

  util::graph::Index<IntVar*> vars_;
};

class X2X3Symmetry : public SymmetryBreaker {
 public:
  X2X3Symmetry(Solver* const, const std::vector<IntVar*>& vars) {
    for (IntVar* var : vars) {
      CHECK(vars_.TryEmplace(var).second) << "Duplicate var: " << var->name();
    }
  }

  ~X2X3Symmetry() override {}

  void VisitSetVariableValue(IntVar* var, int64_t value) override {
    const int index = vars_.Lookup(var).value();
    if (index == 1 || index == 2) {
      AddIntegerVariableEqualValueClause(vars_[vars_.size() - 1 - index],
                                         value);
    }
  }

 private:
  util::graph::Index<IntVar*> vars_;
};

TEST_F(SymmetryManagerTest, VarSymmetry) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(4, 0, 3, "vars", &vars);
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, false));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SymmetryBreaker* const sym =
      solver_->RevAlloc(new X1X4Symmetry(solver_.get(), vars));
  SearchMonitor* const breaker = solver_->MakeSymmetryManager(sym);
  int counter = 0;
  solver_->NewSearch(db, breaker);
  while (solver_->NextSolution()) {
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(12, counter);
}

class SplitVars : public DecisionBuilder {
 public:
  SplitVars(const std::vector<IntVar*>& vars, bool start_low)
      : vars_(vars), start_low_(start_low) {}
  ~SplitVars() override {}

  Decision* Next(Solver* const solver) override {
    IntVar* var = nullptr;
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        var = vars_[i];
        break;
      }
    }
    if (var == nullptr) {
      return nullptr;
    }
    // Round down when starting low, round up when starting high.
    const int64_t med = (var->Min() + var->Max()) / 2;
    return start_low_ ? solver->MakeVariableLessOrEqualValue(var, med)
                      : solver->MakeVariableGreaterOrEqualValue(var, med);
  }

 private:
  std::vector<IntVar*> vars_;
  const bool start_low_;
};

TEST_F(SymmetryManagerTest, VarSymmetrySplitDomains) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(4, 0, 3, "vars", &vars);
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, false));
  DecisionBuilder* const split_low =
      solver_->RevAlloc(new SplitVars(vars, true));
  SymmetryBreaker* const sym =
      solver_->RevAlloc(new X1X4Symmetry(solver_.get(), vars));
  SearchMonitor* const breaker = solver_->MakeSymmetryManager(sym);
  int counter = 0;
  solver_->NewSearch(split_low, breaker);
  while (solver_->NextSolution()) {
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(12, counter);
  counter = 0;
  DecisionBuilder* const split_high =
      solver_->RevAlloc(new SplitVars(vars, false));
  solver_->NewSearch(split_high, breaker);
  while (solver_->NextSolution()) {
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(12, counter);
}

TEST_F(SymmetryManagerTest, PartialVarSymmetry) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(4, 0, 3, "vars", &vars);
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, false));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SymmetryBreaker* const sym =
      solver_->RevAlloc(new X2X3Symmetry(solver_.get(), vars));
  SearchMonitor* const breaker = solver_->MakeSymmetryManager(sym);
  int counter = 0;
  solver_->NewSearch(db, breaker);
  while (solver_->NextSolution()) {
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(12, counter);
}

TEST_F(SymmetryManagerTest, CombinedVarSymmetry) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(4, 0, 3, "vars", &vars);
  solver_->AddConstraint(solver_->MakeAllDifferent(vars, false));
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  SymmetryBreaker* const sym1 =
      solver_->RevAlloc(new X1X4Symmetry(solver_.get(), vars));
  SymmetryBreaker* const sym2 =
      solver_->RevAlloc(new X2X3Symmetry(solver_.get(), vars));
  SearchMonitor* const breaker = solver_->MakeSymmetryManager(sym1, sym2);
  int counter = 0;
  solver_->NewSearch(db, breaker);
  while (solver_->NextSolution()) {
    counter++;
  }
  solver_->EndSearch();
  EXPECT_EQ(6, counter);
}

// ----- SearchLog Test -----
// TODO(user): Improve search log tests; currently only tests coverage.

void TestLog(SearchMonitor* const searchlog) {
  searchlog->EnterSearch();
  searchlog->ExitSearch();
  searchlog->AcceptSolution();
  searchlog->AtSolution();
  searchlog->BeginFail();
  searchlog->NoMoreSolutions();
  searchlog->BeginInitialPropagation();
  searchlog->EndInitialPropagation();
}

TEST(SearchLogTest, SearchLog) {
  Solver solver("TestSearchLog");
  IntVar* const var = solver.MakeIntVar(1, 1, "obj");
  OptimizeVar* const objective = solver.MakeMinimize(var, 1);
  SearchMonitor* const searchlog = solver.MakeSearchLog(0, objective, nullptr);
  TestLog(searchlog);
  int count = 0;
  SearchMonitor* const searchlog_count =
      solver.MakeSearchLog(0, objective, [&count]() {
        count++;
        return "";
      });
  TestLog(searchlog_count);
  // Called in AtSolution.
  EXPECT_EQ(1, count);
}

// Log sink to count the occurrence of a given std::string.
class LocalSearchLog : public absl::LogSink {
 public:
  explicit LocalSearchLog(const std::string& count_str,
                          std::function<std::string(int)> value_from_count)
      : count_str_(count_str), value_from_count_(std::move(value_from_count)) {}
  void Send(const absl::LogEntry& e) override {
    if (absl::StrContains(e.text_message(), count_str_)) {
      EXPECT_TRUE(
          absl::StrContains(e.text_message(), value_from_count_(count_)));
      ++count_;
    }
  }
  int count() const { return count_; }
  void Reset() { count_ = 0; }

 private:
  int count_ = 0;
  const std::string count_str_;
  std::function<std::string(int)> value_from_count_;
};

TEST(SearchLogTest, SearchLogWithLocalSearch) {
  std::unique_ptr<LocalSearchLog> destination(new LocalSearchLog(
      "DecrementValue",
      [](int count) { return absl::StrCat("objective = ", 9 - count); }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var = solver.MakeIntVar(1, 10, "obj");
  std::vector<IntVar*> vars = {var};
  OptimizeVar* const objective = solver.MakeMinimize(var, 1);
  SearchMonitor* const searchlog = solver.MakeSearchLog(1, objective, nullptr);
  // Local search starting from a solution with an objective value of 10.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10 to 1.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest, SearchLogWithLocalSearchAndMultipleObjectives) {
  std::unique_ptr<LocalSearchLog> destination(
      new LocalSearchLog("DecrementValue", [](int count) {
        return absl::StrFormat("objective = %d, %d", 9 - count, 11 - count);
      }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var1 = solver.MakeIntVar(1, 10, "obj");
  IntVar* const var2 = solver.MakeSum(var1, 2)->Var();
  std::vector<IntVar*> vars = {var1};
  OptimizeVar* const objective = solver.MakeLexicographicOptimize(
      /*maximize=*/{false, false}, {var1, var2}, /*steps=*/{1, 1});
  SearchMonitor* const searchlog = solver.MakeSearchLog(1, objective, nullptr);
  // Local search starting from a solution with an objective value of 10/12.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var1, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase({var1, var2}, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10/12 to 1/3.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest, SearchLogWithLocalSearchAndMultipleVars) {
  std::unique_ptr<LocalSearchLog> destination(
      new LocalSearchLog("DecrementValue", [](int count) {
        return absl::StrFormat("%d, %d", 9 - count, 11 - count);
      }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var1 = solver.MakeIntVar(1, 10, "obj");
  IntVar* const var2 = solver.MakeSum(var1, 2)->Var();
  std::vector<IntVar*> vars = {var1};
  OptimizeVar* const objective = solver.MakeLexicographicOptimize(
      /*maximize=*/{false, false}, {var1, var2}, /*steps=*/{1, 1});
  Solver::SearchLogParameters parameters;
  parameters.variables = {var1, var2};
  SearchMonitor* const searchlog = solver.MakeSearchLog(parameters);
  // Local search starting from a solution with an objective value of 10/12.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var1, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase({var1, var2}, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10/12 to 1/3.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest, SearchLogWithLocalSearchAndCostScaling) {
  const double scaling_factor = 10.0;
  const double offset = 100.0;
  std::unique_ptr<LocalSearchLog> destination(
      new LocalSearchLog("DecrementValue", [scaling_factor, offset](int count) {
        const int64_t value = 9 - count;
        return absl::StrFormat("%d (%.8lf)", value,
                               (value + offset) * scaling_factor);
      }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var = solver.MakeIntVar(1, 10, "obj");
  std::vector<IntVar*> vars = {var};
  OptimizeVar* const objective = solver.MakeMinimize(var, 1);
  SearchMonitor* const searchlog = solver.MakeSearchLog(
      {1, nullptr, {var}, {scaling_factor}, {offset}, nullptr});
  // Local search starting from a solution with an objective value of 10.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10 to 1.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest, SearchLogWithLocalSearchAndMultipleVarsWithCostScaling) {
  std::unique_ptr<LocalSearchLog> destination(
      new LocalSearchLog("DecrementValue", [](int count) {
        const int64_t value = 9 - count;
        return absl::StrFormat("%d (%.8lf), %d (%.8lf)", value,
                               (value + 50.0) * 10.0, value + 2,
                               (value + 2 + 70.0) * 20.0);
      }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var1 = solver.MakeIntVar(1, 10, "obj");
  IntVar* const var2 = solver.MakeSum(var1, 2)->Var();
  std::vector<IntVar*> vars = {var1};
  OptimizeVar* const objective = solver.MakeLexicographicOptimize(
      /*maximize=*/{false, false}, {var1, var2}, /*steps=*/{1, 1});
  Solver::SearchLogParameters parameters;
  parameters.variables = {var1, var2};
  parameters.scaling_factors = {10.0, 20.0};
  parameters.offsets = {50.0, 70.0};
  SearchMonitor* const searchlog = solver.MakeSearchLog(parameters);
  // Local search starting from a solution with an objective value of 10/12.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var1, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase({var1, var2}, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10/12 to 1/3.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest, SearchLogWithLocalSearchDisplayCallback) {
  std::unique_ptr<LocalSearchLog> destination(
      new LocalSearchLog("DecrementValue", [](int) { return "Callback"; }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var = solver.MakeIntVar(1, 10, "obj");
  std::vector<IntVar*> vars = {var};
  OptimizeVar* const objective = solver.MakeMinimize(var, 1);
  SearchMonitor* const searchlog =
      solver.MakeSearchLog(1, objective, []() { return "Callback"; });
  // Local search starting from a solution with an objective value of 10.
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MAX_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          var, solver.MakeOperator(vars, Solver::DECREMENT),
          solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
  // Objective will be decremented 9 times, from 10 to 1.
  EXPECT_EQ(9, destination->count());
  absl::RemoveLogSink(destination.get());
}

TEST(SearchLogTest,
     SearchLogWithLocalSearchDisplayCallbackWhenStartedAnfFinished) {
  for (const std::string& message : {"Start search", "Finished search"}) {
    std::unique_ptr<LocalSearchLog> destination(
        new LocalSearchLog(message, [](int) { return "Callback"; }));
    absl::AddLogSink(destination.get());
    Solver solver("TestSearchLog");
    IntVar* const var = solver.MakeIntVar(1, 10, "obj");
    std::vector<IntVar*> vars = {var};
    OptimizeVar* const objective = solver.MakeMinimize(var, 1);
    Solver::SearchLogParameters parameters;
    parameters.branch_period = 1;
    parameters.objective = objective;
    parameters.display_callback = []() { return "Callback"; };
    parameters.display_on_new_solutions_only = false;
    SearchMonitor* const searchlog = solver.MakeSearchLog(parameters);
    // Local search starting from a solution with an objective value of 10.
    DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
        vars,
        solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MAX_VALUE),
        solver.MakeLocalSearchPhaseParameters(
            var, solver.MakeOperator(vars, Solver::DECREMENT),
            solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_MIN_VALUE)));
    EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
    EXPECT_EQ(1, destination->count());
    absl::RemoveLogSink(destination.get());
  }
}

TEST(SearchLogTest, SearchLogWithLocalSearchSolutionCountReset) {
  std::unique_ptr<LocalSearchLog> destination(new LocalSearchLog(
      "Solution", [](int count) { return absl::StrCat("Solution #", count); }));
  absl::AddLogSink(destination.get());
  Solver solver("TestSearchLog");
  IntVar* const var = solver.MakeIntVar(1, 10, "obj");
  std::vector<IntVar*> vars = {var};
  OptimizeVar* const objective = solver.MakeMinimize(var, 1);
  SearchMonitor* const searchlog = solver.MakeSearchLog(1, objective, nullptr);
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(absl::StrCat("Iteration#", i));
    // Local search starting from a solution with an objective value of 10.
    DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
        vars,
        solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_MAX_VALUE),
        solver.MakeLocalSearchPhaseParameters(
            var, solver.MakeOperator(vars, Solver::DECREMENT),
            solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_MIN_VALUE)));
    EXPECT_TRUE(solver.Solve(decision_builder, objective, searchlog));
    // Objective will be decremented 9 times, from 10 to 1 + initial solution.
    EXPECT_EQ(10, destination->count());
    destination->Reset();
  }
  absl::RemoveLogSink(destination.get());
}

// ----- Metaheuristic Test -----

int64_t ZeroCallback(int64_t, int64_t) { return 0; }

class MetaheuristicFirstSolutionTest : public testing::Test {
 public:
  SearchMonitor* MakeGreedyDescentMaximize(Solver* const solver,
                                           const std::vector<IntVar*>&,
                                           IntVar* const objective_var) const {
    return solver->MakeMaximize(objective_var, 1);
  }

  SearchMonitor* MakeGreedyDescentMinimize(Solver* const solver,
                                           const std::vector<IntVar*>&,
                                           IntVar* const objective_var) const {
    return solver->MakeMinimize(objective_var, 1);
  }

  SearchMonitor* MakeSimulatedAnnealingMaximize(
      Solver* const solver, const std::vector<IntVar*>&,
      IntVar* const objective_var) const {
    return solver->MakeSimulatedAnnealing(true, objective_var, 1, 100);
  }

  SearchMonitor* MakeSimulatedAnnealingMinimize(
      Solver* const solver, const std::vector<IntVar*>&,
      IntVar* const objective_var) const {
    return solver->MakeSimulatedAnnealing(false, objective_var, 1, 100);
  }

  SearchMonitor* MakeGuidedLocalSearchMinimize(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const {
    return solver->MakeGuidedLocalSearch(false, objective_var, ZeroCallback, 1,
                                         variables, 0.1);
  }

  SearchMonitor* MakeGuidedLocalSearchMaximize(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const {
    return solver->MakeGuidedLocalSearch(true, objective_var, ZeroCallback, 1,
                                         variables, 0.1);
  }

  SearchMonitor* MakeGuidedLocalSearchMinimizeWithReset(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const {
    return solver->MakeGuidedLocalSearch(
        false, objective_var, ZeroCallback, 1, variables, 0.1,
        /*get_equivalent_pairs=*/nullptr,
        /*reset_penalties_on_new_best_solution=*/true);
  }

  SearchMonitor* MakeGuidedLocalSearchMaximizeWithReset(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const {
    return solver->MakeGuidedLocalSearch(
        true, objective_var, ZeroCallback, 1, variables, 0.1,
        /*get_equivalent_pairs=*/nullptr,
        /*reset_penalties_on_new_best_solution=*/true);
  }

  SearchMonitor* MakeTabuSearchMinimize(Solver* const solver,
                                        const std::vector<IntVar*>& variables,
                                        IntVar* const objective_var) const {
    return solver->MakeTabuSearch(false, objective_var, 1, variables, 1, 1,
                                  0.0);
  }

  SearchMonitor* MakeTabuSearchMaximize(Solver* const solver,
                                        const std::vector<IntVar*>& variables,
                                        IntVar* const objective_var) const {
    return solver->MakeTabuSearch(true, objective_var, 1, variables, 1, 1, 0.0);
  }

  SearchMonitor* MakeRoundRobinCompoundObjectiveMonitor(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const {
    return solver->MakeRoundRobinCompoundObjectiveMonitor(
        {solver->MakeGuidedLocalSearch(false, objective_var, ZeroCallback, 1,
                                       variables, 0.1),
         solver->MakeTabuSearch(false, objective_var, 1, variables, 1, 1, 0.0)},
        10);
  }

 protected:
  typedef SearchMonitor* (MetaheuristicFirstSolutionTest::*MakeMetaheuristic)(
      Solver* const solver, const std::vector<IntVar*>& variables,
      IntVar* const objective_var) const;

  void RunTest(int64_t objective_value, MakeMetaheuristic make_metaheuristic) {
    LOG(INFO) << "Using objective value: " << objective_value;
    Solver solver("MetaheuristicFirstSolutionTest");

    std::vector<IntVar*> variables;
    variables.push_back(solver.MakeIntVar(0, 1));
    variables.push_back(solver.MakeIntVar(0, 1));

    IntVar* objective_var = solver.MakeIntConst(objective_value);

    std::vector<SearchMonitor*> search_monitors;
    search_monitors.push_back(
        solver.MakeLimit(absl::Seconds(1), kint64max, kint64max, 1));
    SolutionCollector* collector = solver.MakeLastSolutionCollector();
    collector->Add(variables);
    collector->AddObjective(objective_var);
    search_monitors.push_back(collector);

    search_monitors.push_back(
        (this->*make_metaheuristic)(&solver, variables, objective_var));

    DecisionBuilder* decision_builder = solver.MakePhase(
        variables, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

    EXPECT_TRUE(solver.Solve(decision_builder, search_monitors));

    EXPECT_EQ(1, collector->solution_count());
    EXPECT_EQ(objective_value, collector->objective_value(0));
  }

  void RunTests(MakeMetaheuristic make_metaheuristic) {
    for (int i = 0; i < kNumTestedObjectiveValues; ++i) {
      RunTest(kTestedObjecteValues[i], make_metaheuristic);
    }
  }

  static const int64_t kTestedObjecteValues[];

  static const int kNumTestedObjectiveValues;

 private:
};

const int64_t MetaheuristicFirstSolutionTest::kTestedObjecteValues[] = {
    kint64min, kint64min + 1, 0, kint64max - 1, kint64max};
const int MetaheuristicFirstSolutionTest::kNumTestedObjectiveValues = 5;

TEST_F(MetaheuristicFirstSolutionTest, GreedyDescentMaximize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGreedyDescentMaximize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, GreedyDescentMinimize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGreedyDescentMinimize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, SimulatedAnnealingMinimize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeSimulatedAnnealingMinimize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, SimulatedAnnealingMaximize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeSimulatedAnnealingMaximize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, GuidedLocalSearchMinimize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGuidedLocalSearchMinimize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, GuidedLocalSearchMaximize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGuidedLocalSearchMaximize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, GuidedLocalSearchMinimizeWithReset) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGuidedLocalSearchMinimizeWithReset;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, GuidedLocalSearchMaximizeWithReset) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeGuidedLocalSearchMaximizeWithReset;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, TabuSearchMinimize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeTabuSearchMinimize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, TabuSearchMaximize) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeTabuSearchMaximize;
  RunTests(make_metaheuristic);
}

TEST_F(MetaheuristicFirstSolutionTest, RoundRobinCompoundObjectiveMonitor) {
  const MakeMetaheuristic make_metaheuristic =
      &MetaheuristicFirstSolutionTest::MakeRoundRobinCompoundObjectiveMonitor;
  RunTests(make_metaheuristic);
}

class AtSolutionDecisionBuilder : public DecisionBuilder {
 public:
  AtSolutionDecisionBuilder(SearchMonitor* monitor, std::vector<IntVar*> vars,
                            std::vector<int64_t> values,
                            DecisionBuilder* finalizer = nullptr)
      : monitor_(monitor),
        vars_(std::move(vars)),
        values_(std::move(values)),
        finalizer_(finalizer) {}
  Decision* Next(Solver* s) override {
    for (int i = 0; i < vars_.size(); ++i) {
      vars_[i]->SetValue(values_[i]);
    }
    if (finalizer_ != nullptr) s->SolveAndCommit(finalizer_);
    if (!monitor_->AcceptSolution()) s->Fail();
    monitor_->AtSolution();
    return nullptr;
  }

 private:
  SearchMonitor* const monitor_;
  const std::vector<IntVar*> vars_;
  const std::vector<int64_t> values_;
  DecisionBuilder* const finalizer_;
};

// Test tabu search plateau avoidance on cases where the cost variable can
// artificially be stretched to avoid the plateau constraint.
TEST_IN_PROPAGATION(TabuSearchTest, CheckPlateau, s, true) {
  IntVar* const var1 = s->MakeBoolVar("var1");
  IntVar* const var2 = s->MakeBoolVar("var2");
  IntVar* const cost_var1 = s->MakeIntVar(0, 10, "cost1");
  s->AddConstraint(s->MakeGreaterOrEqual(
      cost_var1, s->MakeElement(std::vector<int64_t>{1, 2}, var1)));
  IntVar* const cost_var2 = s->MakeIntVar(0, 10, "cost2");
  s->AddConstraint(s->MakeGreaterOrEqual(
      cost_var2, s->MakeElement(std::vector<int64_t>{1, 2}, var2)));
  IntVar* const objective_var = s->MakeIntVar(0, 100, "obj");
  s->AddConstraint(s->MakeSumEquality({cost_var1, cost_var2}, objective_var));
  Assignment* const assignment = s->MakeAssignment();
  assignment->Add(var1);
  assignment->Add(var2);
  assignment->AddObjective(objective_var);
  SolutionCollector* const monitor = s->MakeBestValueSolutionCollector(
      /*assignment=*/nullptr, /*maximize=*/false);
  monitor->Add(var1);
  monitor->Add(var2);
  monitor->AddObjective(objective_var);
  DecisionBuilder* const finalizer =
      s->MakePhase({cost_var1, cost_var2}, Solver::CHOOSE_FIRST_UNBOUND,
                   Solver::ASSIGN_MIN_VALUE);
  SearchMonitor* const tabu =
      s->MakeTabuSearch(false, objective_var, 1, {var1, var2}, 10, 10, 0.6);
  tabu->EnterSearch();
  // Sync with solution 1,1, cost 4
  SolutionCollector* collector = s->MakeLastSolutionCollector(assignment);
  s->Solve(s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {1, 1},
                                                     finalizer)),
           collector);
  EXPECT_EQ(collector->solution_count(), 1);
  EXPECT_EQ(collector->objective_value(0), 4);
  // Age tabu lists
  tabu->AtLocalOptimum();
  // Sync with solution 1,0, cost 3
  collector = s->MakeLastSolutionCollector(assignment);
  s->Solve(s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {1, 0},
                                                     finalizer)),
           collector);
  EXPECT_EQ(collector->solution_count(), 1);
  EXPECT_EQ(collector->objective_value(0), 3);
  // Setup tabu constraints
  tabu->ApplyDecision(nullptr);
  // Sync with solution 0,1, which should reject the new solution since it's a
  // cost plateau. We don't want cost_var1 or cost_var2 to be stretched to avoid
  // it.
  collector = s->MakeLastSolutionCollector(assignment);
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {0, 1})),
      collector);
  EXPECT_EQ(collector->solution_count(), 0);
  return nullptr;
}

// Test which puts Tabu Search in a situation where the tabu constraints fail.
// This checks that this does not lead to leaks.
TEST_IN_PROPAGATION(TabuSearchTest, CheckLeaks, s, false) {
  IntVar* const var1 = s->MakeBoolVar("var1");
  IntVar* const var2 = s->MakeBoolVar("var2");
  IntVar* const objective_var = s->MakeIntVar(0, 1, "obj");
  SearchMonitor* const tabu =
      s->MakeTabuSearch(false, objective_var, 1, {var1, var2}, 10, 10, 0.6);
  tabu->EnterSearch();
  // Sync with solution 1,1
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {1, 1})));
  // Age tabu lists
  tabu->AtLocalOptimum();
  // Sync with solution 0,0
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {0, 0})));
  // Sync with solution 1,0
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(tabu, {var1, var2}, {1, 0})));
  // Setting variables to tabu values.
  var1->SetValue(1);
  var2->SetValue(1);
  objective_var->SetValue(1);
  // Setup tabu constraints, which fails
  tabu->ApplyDecision(s->MakeFailDecision());
  return nullptr;
}

// Test which puts Guided Local Search in a situation where the penalty
// constraints fail. This checks that this does not lead to leaks.
TEST_IN_PROPAGATION(GuidedLocalSearch, CheckLeaks, s, false) {
  IntVar* const var1 = s->MakeBoolVar("var1");
  IntVar* const var2 = s->MakeBoolVar("var2");
  IntVar* const objective_var = s->MakeIntConst(1, "obj");
  SearchMonitor* const gls = s->MakeGuidedLocalSearch(
      false, objective_var, [](int64_t, int64_t) { return 1; }, 1, {var1, var2},
      0.1);
  gls->EnterSearch();
  // Sync with solution 1,1
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(gls, {var1, var2}, {1, 1})));
  // Penalize arcs
  gls->AtLocalOptimum();
  // Sync with solution 0,0
  s->Solve(
      s->RevAlloc(new AtSolutionDecisionBuilder(gls, {var1, var2}, {0, 0})));
  // Setting variables to tabu values.
  var1->SetValue(1);
  var2->SetValue(1);
  // Setup gls constraints, which fails with objective <= 0 since objective == 1
  gls->ApplyDecision(s->MakeFailDecision());
  return nullptr;
}

TEST(GuidedLocalSearch, OverflowTest) {
  // This is a complicated test: trying to make GLS accept a cost-improving
  // move with overflowing penalties; if the overflow is not handled properly,
  // penalties will become negative and trigger an unwanted behavior of the
  // sum constraint: in presence of elements which can overflow the sum and of
  // others which are negative, the constraint fails to instantiate the sum
  // variable.
  Solver solver("GLS");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(4, 0, 5, "vars", &vars);
  IntVar* objective_var = solver.MakeSum(vars)->Var();
  std::vector<SearchMonitor*> search_monitors;
  Solver::IndexEvaluator2 callback = [](int64_t i, int64_t) {
    return i % 2 ? kint64max / 6 : kint64max / 3;
  };
  search_monitors.push_back(solver.MakeGuidedLocalSearch(
      false, objective_var, callback, 1, vars, 3.1));
  SolutionCollector* collector = solver.MakeLastSolutionCollector();
  collector->Add(vars);
  collector->AddObjective(objective_var);
  search_monitors.push_back(collector);
  search_monitors.push_back(
      solver.MakeLimit(absl::Seconds(3), kint64max, kint64max, 10));
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_CENTER_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          objective_var, solver.MakeOperator(vars, Solver::INCREMENT),
          solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, search_monitors));
}

TEST(GuidedLocalSearch, DirectionTest) {
  for (bool maximize : {true, false}) {
    Solver solver("GLS");
    std::vector<IntVar*> vars;
    solver.MakeIntVarArray(4, 0, 10, "vars", &vars);
    // Adding this constraint to make have multiple local optima.
    solver.AddConstraint(solver.MakeAllDifferent(vars));
    IntVar* objective_var = solver.MakeSum(vars)->Var();
    std::vector<SearchMonitor*> search_monitors;
    Solver::IndexEvaluator2 callback = [](int64_t, int64_t j) { return j; };
    search_monitors.push_back(solver.MakeGuidedLocalSearch(
        maximize, objective_var, callback, 1, vars, 3.1));
    SolutionCollector* collector =
        solver.MakeBestValueSolutionCollector(maximize);
    collector->Add(vars);
    collector->AddObjective(objective_var);
    search_monitors.push_back(collector);
    search_monitors.push_back(
        solver.MakeLimit(absl::Seconds(3), kint64max, kint64max, 100));
    DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
        vars,
        solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                         Solver::ASSIGN_CENTER_VALUE),
        solver.MakeLocalSearchPhaseParameters(
            objective_var,
            solver.ConcatenateOperators(
                {solver.MakeOperator(vars, Solver::INCREMENT),
                 solver.MakeOperator(vars, Solver::DECREMENT)}),
            solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_MIN_VALUE)));
    EXPECT_TRUE(solver.Solve(decision_builder, search_monitors));
    EXPECT_EQ(collector->objective_value(0),
              maximize ? 10 + 9 + 8 + 7 : 0 + 1 + 2 + 3);
  }
}

// Variant of IncrementValue with secondary variables.
class IncrementValue : public IntVarLocalSearchOperator {
 public:
  IncrementValue(const std::vector<IntVar*>& vars,
                 const std::vector<IntVar*>& secondaries)
      : IntVarLocalSearchOperator(vars), index_(0), base_size_(vars.size()) {
    AddVars(secondaries);
  }
  ~IncrementValue() override {}
  bool MakeOneNeighbor() override {
    while (index_ < base_size_) {
      SetValue(index_, Value(index_) + 1);
      SetValue(index_ + base_size_, Value(index_ + base_size_));
      ++index_;
      return true;
    }
    return false;
  }
  void OnStart() override { index_ = 0; }

 private:
  int index_;
  int base_size_;
};

TEST(GuidedLocalSearch, OverflowTestWithSecondaryVariables) {
  // Same as OverflowTest with the ternary version of GLS.
  Solver solver("GLS");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(4, 0, 5, "vars", &vars);
  std::vector<IntVar*> secondaries;
  solver.MakeIntVarArray(4, 1, 1, "secondaries", &secondaries);
  IntVar* objective_var = solver.MakeSum(vars)->Var();
  std::vector<IntVar*> all_vars = vars;
  for (auto v : secondaries) all_vars.push_back(v);
  std::vector<SearchMonitor*> search_monitors;
  Solver::IndexEvaluator3 callback = [](int64_t i, int64_t, int64_t) {
    return i % 2 ? kint64max / 6 : kint64max / 3;
  };
  search_monitors.push_back(solver.MakeGuidedLocalSearch(
      false, objective_var, callback, 1, vars, secondaries, 3.1));
  SolutionCollector* collector = solver.MakeLastSolutionCollector();
  collector->Add(vars);
  collector->AddObjective(objective_var);
  search_monitors.push_back(collector);
  search_monitors.push_back(
      solver.MakeLimit(absl::Seconds(3), kint64max, kint64max, 10));
  DecisionBuilder* decision_builder = solver.MakeLocalSearchPhase(
      all_vars,
      solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_CENTER_VALUE),
      solver.MakeLocalSearchPhaseParameters(
          objective_var, solver.RevAlloc(new IncrementValue(vars, secondaries)),
          solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_MIN_VALUE)));
  EXPECT_TRUE(solver.Solve(decision_builder, search_monitors));
}

// Callback-based search monitors

TEST(AtSolutionCallbackTest, LambdaFunction) {
  Solver solver("");
  bool touched = false;
  SearchMonitor* monitor =
      solver.MakeAtSolutionCallback([&touched]() { touched = true; });
  monitor->EnterSearch();
  EXPECT_FALSE(touched);
  monitor->RestartSearch();
  EXPECT_FALSE(touched);
  monitor->ExitSearch();
  EXPECT_FALSE(touched);
  monitor->AcceptSolution();
  EXPECT_FALSE(touched);
  monitor->BeginFail();
  EXPECT_FALSE(touched);
  monitor->EndFail();
  EXPECT_FALSE(touched);
  EXPECT_FALSE(monitor->AtSolution());
  EXPECT_TRUE(touched);
}

TEST(EnterSearchCallbackTest, LambdaFunction) {
  Solver solver("");
  bool touched = false;
  SearchMonitor* monitor =
      solver.MakeEnterSearchCallback([&touched]() { touched = true; });
  monitor->AtSolution();
  EXPECT_FALSE(touched);
  monitor->RestartSearch();
  EXPECT_FALSE(touched);
  monitor->ExitSearch();
  EXPECT_FALSE(touched);
  monitor->AcceptSolution();
  EXPECT_FALSE(touched);
  monitor->BeginFail();
  EXPECT_FALSE(touched);
  monitor->EndFail();
  EXPECT_FALSE(touched);
  monitor->EnterSearch();
  EXPECT_TRUE(touched);
}

TEST(ExitSearchCallbackTest, LambdaFunction) {
  Solver solver("");
  bool touched = false;
  SearchMonitor* monitor =
      solver.MakeExitSearchCallback([&touched]() { touched = true; });
  monitor->AtSolution();
  EXPECT_FALSE(touched);
  monitor->RestartSearch();
  EXPECT_FALSE(touched);
  monitor->EnterSearch();
  EXPECT_FALSE(touched);
  monitor->AcceptSolution();
  EXPECT_FALSE(touched);
  monitor->BeginFail();
  EXPECT_FALSE(touched);
  monitor->EndFail();
  EXPECT_FALSE(touched);
  monitor->ExitSearch();
  EXPECT_TRUE(touched);
}

// Test ClosureDecision

void SetToValue(int* ptr, int value) { *ptr = value; }

TEST(ClosureDecisionTest, testApi) {
  int x = 0;
  Solver::Action a = [&x](Solver*) { SetToValue(&x, 2); };
  Solver::Action b = [&x](Solver*) { SetToValue(&x, 4); };
  Solver solver("ClosureDecisionTest::testApi");
  Decision* const decision = solver.MakeDecision(a, b);
  decision->Apply(&solver);
  EXPECT_EQ(x, 2);
  decision->Refute(&solver);
  EXPECT_EQ(x, 4);
}

// Test search direction

TEST(SearchDirectionTest, SetDirectionDirectly) {
  Solver solver("Test");
  EXPECT_EQ(Solver::NOT_SET, solver.optimization_direction());
  for (Solver::OptimizationDirection direction :
       {Solver::NOT_SET, Solver::MAXIMIZATION, Solver::MINIMIZATION}) {
    solver.set_optimization_direction(direction);
    EXPECT_EQ(direction, solver.optimization_direction());
    solver.set_optimization_direction(Solver::NOT_SET);
  }
}

TEST(SearchDirectionTest, SetMinizationDirectionThroughMonitor) {
  Solver solver("Test");
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  solver.MakeMinimize(var, 2);
  EXPECT_EQ(Solver::MINIMIZATION, solver.optimization_direction());
}

TEST(SearchDirectionTest, SetMaximizationDirectionThroughMonitor) {
  Solver solver("Test");
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  solver.MakeMaximize(var, 2);
  EXPECT_EQ(Solver::MAXIMIZATION, solver.optimization_direction());
}

TEST(SearchDirectionTest, SetDirectionThroughMonitor) {
  for (bool maximize : {true, false}) {
    Solver solver("Test");
    IntVar* const var = solver.MakeIntVar(0, 10, "var");
    solver.MakeOptimize(maximize, var, 2);
    if (maximize) {
      EXPECT_EQ(Solver::MAXIMIZATION, solver.optimization_direction());
    } else {
      EXPECT_EQ(Solver::MINIMIZATION, solver.optimization_direction());
    }
  }
}

}  // namespace
}  // namespace operations_research
