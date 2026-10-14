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

#include "ortools/constraint_solver/local_search.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {

class LocalSearchPhaseParameters;

namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Lt;

class NamedLocalSearchOperator : public LocalSearchOperator {
 public:
  explicit NamedLocalSearchOperator(const std::string& name)
      : name_(name), called_(false) {}
  bool MakeNextNeighbor(Assignment*, Assignment*) override {
    if (called_) return false;
    called_ = true;
    return true;
  }
  void Start(const Assignment*) override {}
  std::string DebugString() const override { return name_; }

 private:
  const std::string name_;
  bool called_;
};

TEST(LocalSearchOperatorTest, DebugString) {
  NamedLocalSearchOperator op1("op1");
  EXPECT_EQ("op1", op1.DebugString());
  NamedLocalSearchOperator op2("op2");
  {
    Solver solver("Test");
    LocalSearchOperator* op = solver.ConcatenateOperators({&op1, &op2});
    Assignment* assignment = solver.MakeAssignment();
    Assignment* delta = solver.MakeAssignment();
    op->Start(assignment);
    op->MakeNextNeighbor(delta, nullptr);
    EXPECT_EQ("op1", op->DebugString());
    op->MakeNextNeighbor(delta, nullptr);
    EXPECT_EQ("op2", op->DebugString());
  }
}

TEST(LocalSearchOperatorTest, Self) {
  NamedLocalSearchOperator op1("op1");
  EXPECT_EQ(&op1, op1.Self());
  NamedLocalSearchOperator op2("op2");
  {
    Solver solver("Test");
    LocalSearchOperator* op = solver.ConcatenateOperators({&op1, &op2});
    Assignment* assignment = solver.MakeAssignment();
    Assignment* delta = solver.MakeAssignment();
    op->Start(assignment);
    op->MakeNextNeighbor(delta, nullptr);
    EXPECT_EQ(&op1, op->Self());
    op->MakeNextNeighbor(delta, nullptr);
    EXPECT_EQ(&op2, op->Self());
  }
}

class SetAllToValue : public DecisionBuilder {
 public:
  SetAllToValue(const std::vector<IntVar*>& vars, int64_t value)
      : vars_(vars), value_(value) {}
  ~SetAllToValue() override {}
  Decision* Next(Solver* const) override {
    for (int i = 0; i < vars_.size(); ++i) vars_[i]->SetValue(value_);
    return nullptr;
  }
  std::string DebugString() const override { return "SetAllToValue"; }

 private:
  const std::vector<IntVar*> vars_;
  int64_t value_;
};

class LocalSearchTest
    : public testing::TestWithParam<::testing::tuple<int, int>> {
 protected:
  static const int kNumberOfVariables;

  void MakeModel(int64_t max_value, int64_t initial_value, bool maximize,
                 int step, bool scal_prod = false);
  void SetUp() override;

  std::unique_ptr<Solver> solver_;
  IntVar* objective_;
  DecisionBuilder* first_solution_;
  ObjectiveMonitor* optimize_;
  SolutionCollector* collect_assignments_;
  SearchLimit* limit_;
  std::vector<IntVar*> vars_;
};

const int LocalSearchTest::kNumberOfVariables(100);

void LocalSearchTest::MakeModel(int64_t max_value, int64_t initial_value,
                                bool maximize, int step, bool scal_prod) {
  int64_t cost_bound = 0;
  std::vector<IntVar*> cost_vars;
  for (int i = 0; i < kNumberOfVariables; ++i) {
    IntVar* const var = solver_->MakeIntVar(int64_t{0}, max_value);
    if (scal_prod) {
      cost_vars.push_back(solver_->MakeProd(var, i + 1)->Var());
      cost_bound += max_value * (i + 1);
    } else {
      cost_vars.push_back(var);
      cost_bound += max_value;
    }
    vars_.push_back(var);
  }
  objective_ = solver_->MakeIntVar(0, cost_bound);
  IntExpr* const sum = solver_->MakeSum(cost_vars);
  solver_->AddConstraint(solver_->MakeEquality(objective_, sum->Var()));

  first_solution_ = solver_->RevAlloc(new SetAllToValue(vars_, initial_value));
  switch (::testing::get<1>(GetParam())) {
    case 0:
      optimize_ = solver_->MakeOptimize(maximize, objective_, step);
      break;
    case 1:
      optimize_ = solver_->MakeGuidedLocalSearch(
          maximize, objective_, [](int64_t, int64_t) { return 0; }, step,
          cost_vars, 0.1);
      break;
    case 2:
      optimize_ = solver_->MakeTabuSearch(maximize, objective_, step, cost_vars,
                                          1, 1, 0.5);
      break;
    default:
      optimize_ = solver_->MakeOptimize(maximize, objective_, step);
  }
  Assignment collector_reference(solver_.get());
  collector_reference.AddObjective(objective_);
  collect_assignments_ =
      solver_->MakeAllSolutionCollector(&collector_reference);
  limit_ = solver_->MakeSolutionsLimit(kNumberOfVariables * 4);
}

void LocalSearchTest::SetUp() {
  ConstraintSolverParameters parameters = Solver::DefaultSolverParameters();
  parameters.set_check_solution_period(::testing::get<0>(GetParam()));
  solver_ = std::make_unique<Solver>("LocalSearchTest", parameters);
}

TEST_P(LocalSearchTest, ImproveAssignmentWithDecrementValues) {
  MakeModel(int64_t{1}, int64_t{1}, false, 1LL);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->Add(vars_);
  assignment->AddObjective(objective_);

  DecisionBuilder* const store = solver_->MakeStoreAssignment(assignment);
  DecisionBuilder* const first_solution_and_store =
      solver_->Compose(first_solution_, store);

  LocalSearchOperator* const decrement_value =
      solver_->MakeOperator(vars_, Solver::DECREMENT);
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, decrement_value,
                                              nullptr);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(assignment, ls_params);

  EXPECT_TRUE(solver_->Solve(first_solution_and_store));
  EXPECT_EQ(kNumberOfVariables, assignment->ObjectiveValue());

  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
  EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
  EXPECT_EQ(0, collect_assignments_->objective_value(
                   collect_assignments_->solution_count() - 1));
}

TEST_P(LocalSearchTest, ImproveAssignmentWithIncrementValues) {
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->Add(vars_);
  assignment->AddObjective(objective_);

  DecisionBuilder* const store = solver_->MakeStoreAssignment(assignment);
  DecisionBuilder* const first_solution_and_store =
      solver_->Compose(first_solution_, store);

  LocalSearchOperator* const increment_value =
      solver_->MakeOperator(vars_, Solver::INCREMENT);
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, increment_value,
                                              nullptr);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(assignment, ls_params);

  EXPECT_TRUE(solver_->Solve(first_solution_and_store));
  EXPECT_EQ(0, assignment->ObjectiveValue());

  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
  EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
  EXPECT_EQ(kNumberOfVariables,
            collect_assignments_->objective_value(
                collect_assignments_->solution_count() - 1));
}

TEST_P(LocalSearchTest, ImproveFirstSolutionWithDecrementValues) {
  MakeModel(int64_t{1}, int64_t{1}, false, 1LL);
  for (bool use_sub_db : {true, false}) {
    LocalSearchOperator* const decrement_value =
        solver_->MakeOperator(vars_, Solver::DECREMENT);
    std::vector<LocalSearchFilter*> filters = {
        solver_->MakeVariableDomainFilter()};
    LocalSearchFilterManager* const ls_manager =
        solver_->RevAlloc(new LocalSearchFilterManager(std::move(filters)));
    LocalSearchPhaseParameters* const ls_params =
        solver_->MakeLocalSearchPhaseParameters(nullptr, decrement_value,
                                                nullptr, nullptr, ls_manager);
    DecisionBuilder* const local_search =
        use_sub_db
            ? solver_->MakeLocalSearchPhase(vars_, first_solution_,
                                            first_solution_, ls_params)
            : solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);

    EXPECT_TRUE(
        solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
    const int64_t check_solution_period =
        solver_->parameters().check_solution_period();
    if (check_solution_period > 1) {
      // Some solutions are skipped and are therefore not collected.
      EXPECT_EQ((kNumberOfVariables + 1) / check_solution_period + 3,
                collect_assignments_->solution_count());
    } else {
      EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
    }
  }
}

TEST_P(LocalSearchTest, ImproveFirstSolutionWithIncrementValues) {
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL);
  LocalSearchOperator* const increment_value =
      solver_->MakeOperator(vars_, Solver::INCREMENT);
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, increment_value,
                                              nullptr);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);

  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
  EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
}

TEST_P(LocalSearchTest,
       ImproveFirstSolutionWithIncrementValuesAndSolutionLimitAndFilter) {
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL);
  LocalSearchOperator* const increment_value =
      solver_->MakeOperator(vars_, Solver::INCREMENT);
  LocalSearchFilterManager* ls_manager =
      solver_->RevAlloc(new LocalSearchFilterManager(
          {solver_->MakeVariableDomainFilter(),
           solver_->MakeSumObjectiveFilter(
               vars_, [](int64_t, int64_t value) { return value; },
               Solver::GE)}));
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, increment_value,
                                              nullptr, nullptr, ls_manager);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);
  RegularLimit* const limit =
      solver_->MakeSolutionsLimit(kNumberOfVariables / 2);
  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit));
  EXPECT_EQ(kNumberOfVariables / 2 - 1,
            collect_assignments_->objective_value(
                collect_assignments_->solution_count() - 1));
}

TEST_P(LocalSearchTest, ImproveFirstSolutionWithIncrementDecrementValues) {
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL);
  for (bool use_sub_db : {true, false}) {
    LocalSearchOperator* const decrement_value =
        solver_->MakeOperator(vars_, Solver::DECREMENT);
    LocalSearchOperator* const increment_value =
        solver_->MakeOperator(vars_, Solver::INCREMENT);
    std::vector<LocalSearchFilter*> filters = {
        solver_->MakeVariableDomainFilter()};
    LocalSearchOperator* const ls_operators =
        solver_->ConcatenateOperators({decrement_value, increment_value});
    LocalSearchFilterManager* ls_manager =
        solver_->RevAlloc(new LocalSearchFilterManager(std::move(filters)));
    LocalSearchPhaseParameters* const ls_params =
        solver_->MakeLocalSearchPhaseParameters(objective_, ls_operators,
                                                nullptr, nullptr, ls_manager);
    DecisionBuilder* const local_search =
        use_sub_db
            ? solver_->MakeLocalSearchPhase(vars_, first_solution_,
                                            first_solution_, ls_params)
            : solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);

    EXPECT_TRUE(
        solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
    const int64_t check_solution_period =
        solver_->parameters().check_solution_period();
    if (check_solution_period > 1) {
      // Some solutions are skipped and are therefore not collected.
      EXPECT_EQ((kNumberOfVariables + 1) / check_solution_period + 5,
                collect_assignments_->solution_count());
    } else {
      EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
    }
  }
}

TEST_P(LocalSearchTest, BestImproveFirstSolutionWithIncrementValues) {
  const int best_of = 4;
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL, true);
  LocalSearchOperator* const increment_value =
      solver_->MakeOperator(vars_, Solver::INCREMENT);
  RegularLimit* const limit = solver_->MakeSolutionsLimit(best_of);
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, increment_value,
                                              nullptr, limit);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);

  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
  EXPECT_EQ(best_of + 1,
            collect_assignments_->solution(best_of + 1)->ObjectiveValue());
  const int solution_count = (kNumberOfVariables - best_of + 1) * best_of + 1 +
                             (best_of - 1) * best_of / 2;
  EXPECT_EQ(solution_count, collect_assignments_->solution_count());
}

TEST_P(LocalSearchTest, ImproveFirstSolutionWithSimpleLns) {
  // Only test LNS with greedy descent with XSan. Metaheuristics are going to be
  // terribly slow with LNS.
  if (kAnyXsanEnabled && ::testing::get<1>(GetParam()) != 0) return;
  MakeModel(int64_t{1}, int64_t{0}, true, 1LL);
  LocalSearchOperator* const lns =
      solver_->MakeOperator(vars_, Solver::SIMPLELNS);
  DecisionBuilder* const lns_db = solver_->MakePhase(
      vars_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  LocalSearchPhaseParameters* const ls_params =
      solver_->MakeLocalSearchPhaseParameters(objective_, lns, lns_db);
  DecisionBuilder* const local_search =
      solver_->MakeLocalSearchPhase(vars_, first_solution_, ls_params);

  EXPECT_TRUE(
      solver_->Solve(local_search, collect_assignments_, optimize_, limit_));
  EXPECT_EQ(kNumberOfVariables + 1, collect_assignments_->solution_count());
}

INSTANTIATE_TEST_SUITE_P(CheckPeriodLocalSearchTests, LocalSearchTest,
                         ::testing::Combine(::testing::Values(1, 5, 20),
                                            ::testing::Values(0, 1, 2)));

// Test that the FindOneNeighbor::EnterSearch() method is properly called
// when doing consecutive solves with a LocalSearch decision builder.
// The verification is done by making sure that the neighborhood operator is
// properly synchronized at the beginning of each solve.
// TRICKY: For the test to fail without the call to
// FindOneNeighbor::EnterSearch(), the optimal solution must be reached by the
// end of the first solve, making the first solve end early without the last
// synchronization, which itself sets neighbor_found_ to false.
TEST(LocalSearchFindOneNeighborTest, ConsecutiveSolves) {
  class TestOperator : public LocalSearchOperator {
   public:
    explicit TestOperator(const std::vector<IntVar*>& vars)
        : vars_(vars), num_neighbors_(0), num_start_calls_(0) {}
    bool MakeNextNeighbor(Assignment* delta, Assignment*) override {
      if (num_neighbors_ < vars_.size()) {
        delta->Add(vars_[num_neighbors_])->SetValue(0);
        num_neighbors_++;
        return true;
      }
      return false;
    }
    void Start(const Assignment*) override { num_start_calls_++; }
    int num_neighbors() const { return num_neighbors_; }
    int num_start_calls() const { return num_start_calls_; }

   private:
    const std::vector<IntVar*>& vars_;
    int num_neighbors_ = 0;
    int num_start_calls_ = 0;
  };

  Solver solver("LSFindOneNeighborTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 1000, "op_vars", &vars);

  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(vars);
  for (int i = 0; i < vars.size(); ++i) {
    assignment->SetValue(vars[i], 1000);
  }
  IntVar* sum = solver.MakeSum(vars)->Var();

  TestOperator* op = solver.RevAlloc(new TestOperator(vars));
  DecisionBuilder* ls_db = solver.MakeLocalSearchPhase(
      assignment, solver.MakeLocalSearchPhaseParameters(sum, op, nullptr));
  SearchMonitor* monitor = solver.MakeMinimize(sum, 1);

  ASSERT_TRUE(solver.Solve(ls_db, monitor));
  EXPECT_EQ(op->num_neighbors(), 10);
  EXPECT_EQ(op->num_start_calls(), 10);
  // Second solve: There shouldn't be any new neighbor found but the operator
  // should still be synchronized at the beginning of the search.
  ASSERT_TRUE(solver.Solve(ls_db, monitor));
  EXPECT_EQ(op->num_neighbors(), 10);
  EXPECT_EQ(op->num_start_calls(), 11);
}

enum class NeighborOption {
  kIncomingNeighbors,
  kOutgoingNeighbors,
  kIncomingAndOutgoingNeighbors
};

class OperatorTest : public BaseOperatorTest {
 protected:
  void SolveAndTest(Solver::LocalSearchOperators operator_type,
                    const std::vector<int>& neighbors = {},
                    NeighborOption neighbor_option =
                        NeighborOption::kIncomingAndOutgoingNeighbors) {
    operator_type_ = operator_type;

    if (neighbor_option == NeighborOption::kIncomingNeighbors ||
        neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
      incoming_neighbors_ = neighbors;
    }
    if (neighbor_option == NeighborOption::kOutgoingNeighbors ||
        neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
      outgoing_neighbors_ = neighbors;
    }
    BaseSolveAndTest(/*match_all=*/true);
  }
  LocalSearchOperator* BuildOperatorWithSecondaryVars(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      Solver* solver) const override {
    if (var_callback_) var_callback_(vars);
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors;
    if (!incoming_neighbors_.empty()) {
      get_incoming_neighbors = [this](int, int) -> const std::vector<int>& {
        return incoming_neighbors_;
      };
    }
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors;
    if (!outgoing_neighbors_.empty()) {
      get_outgoing_neighbors = [this](int, int) -> const std::vector<int>& {
        return outgoing_neighbors_;
      };
    }
    return solver->MakeOperator(vars, secondary_vars, operator_type_,
                                std::move(get_incoming_neighbors),
                                std::move(get_outgoing_neighbors));
  }
  void SetVarCallback(
      std::function<void(const std::vector<IntVar*>& vars)> callback) {
    var_callback_ = std::move(callback);
  }

  Solver::LocalSearchOperators operator_type_;
  std::vector<int> incoming_neighbors_;
  std::vector<int> outgoing_neighbors_;
  std::function<void(const std::vector<IntVar*>& vars)> var_callback_;
};

// TwoOpt tests

// Initial solution: 0->1->2->3
// Neighbor: 0->2->1->3
TEST_F(OperatorTest, TwoOptTest1) {
  SCOPED_TRACE("TwoOptTest1");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  SolveAndTest(Solver::TWOOPT);
}

// Initial solution: 0->1->2->3->4
// Neighbors:
// 0->2->1->3->4
// 0->3->2->1->4
// 0->1->3->2->4
TEST_F(OperatorTest, TwoOptTest1LongChain) {
  SCOPED_TRACE("TwoOptTest1");
  SetFirstSolution({1, 2, 3, 4});
  AddExpectedDelta({2, 3, 1, -1}, {});
  AddExpectedDelta({3, 4, 1, 2}, {3, 4, -1, 2});
  AddExpectedDelta({-1, 3, 4, 2}, {});
  SolveAndTest(Solver::TWOOPT);
}

class OperatorTestWithNeighbors
    : public OperatorTest,
      public ::testing::WithParamInterface<NeighborOption> {};

// Variants with neighbors.
TEST_P(OperatorTestWithNeighbors, TwoOptTest1WithNeighbors1) {
  SCOPED_TRACE("TwoOptTest1N1");
  SetFirstSolution({1, 2, 3});
  SolveAndTest(Solver::TWOOPT, {1}, GetParam());
}
TEST_P(OperatorTestWithNeighbors, TwoOptTest1WithNeighbors2) {
  SCOPED_TRACE("TwoOptTest1N2");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 2, 3});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({2, 3, 1});
  }
  SolveAndTest(Solver::TWOOPT, {2}, neighbor_option);
}

// Initial solution: 0->1->2->4, 3->5
// Neighbor: 0->2->1->4, 3->5
TEST_F(OperatorTest, TwoOptTest2) {
  SCOPED_TRACE("TwoOptTest2");
  SetFirstSolution({1, 2, 4, 5});
  AddExpectedDelta({2, 4, 1, -1});
  SolveAndTest(Solver::TWOOPT);
}

TEST_P(OperatorTestWithNeighbors, TwoOptTest2WithNeighbors1) {
  SCOPED_TRACE("TwoOptTest2N1");
  SetFirstSolution({1, 2, 4, 5});
  SolveAndTest(Solver::TWOOPT, {1}, GetParam());
}

TEST_P(OperatorTestWithNeighbors, TwoOptTest2WithNeighbors2) {
  SCOPED_TRACE("TwoOptTest2N2");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 2, 4, 5});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({2, 4, 1, -1});
  }
  SolveAndTest(Solver::TWOOPT, {2}, neighbor_option);
}

// Initial solution: 0->4, 1->2->3->5
// Neighbor: 0->4, 1->3->2->5
TEST_F(OperatorTest, TwoOptTest3) {
  SCOPED_TRACE("TwoOptTest3");
  SetFirstSolution({4, 2, 3, 5});
  AddExpectedDelta({-1, 3, 5, 2});
  SolveAndTest(Solver::TWOOPT);
}

TEST_P(OperatorTestWithNeighbors, TwoOptTest3WithNeighbors1) {
  SCOPED_TRACE("TwoOptTest3N1");
  SetFirstSolution({4, 2, 3, 5});
  SolveAndTest(Solver::TWOOPT, {1}, GetParam());
}

TEST_P(OperatorTestWithNeighbors, TwoOptTest3WithNeighbors2) {
  SCOPED_TRACE("TwoOptTest3N2");
  SetFirstSolution({4, 2, 3, 5});
  SolveAndTest(Solver::TWOOPT, {2}, GetParam());
}

TEST_P(OperatorTestWithNeighbors, TwoOptTest3WithNeighbors3) {
  SCOPED_TRACE("TwoOptTest3N3");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({4, 2, 3, 5});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({-1, 3, 5, 2});
  }
  SolveAndTest(Solver::TWOOPT, {3}, neighbor_option);
}

// Initial solution: 0->4->5, 1->2->3->6
TEST_P(OperatorTestWithNeighbors, TwoOptTest4WithNeighbors) {
  SCOPED_TRACE("TwoOptTest4N");
  SetFirstSolution({4, 2, 3, 6, 5});
  SolveAndTest(Solver::TWOOPT, {4}, GetParam());
}

// OrOpt tests

TEST(OperatorNameTest, OrOpt) {
  Solver solver("Test");
  EXPECT_EQ(MakeOrOpt(&solver, {}, {}, nullptr)->DebugString(), "OrOpt<1>");
  EXPECT_EQ(solver.MakeOperator({}, Solver::OROPT)->DebugString(), "OrOpt<1>");
}

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->1->3
// 0->2->1->3
TEST_F(OperatorTest, OrOptTest1) {
  SCOPED_TRACE("OrOptTest1");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  AddExpectedDelta({2, 3, 1});
  SolveAndTest(Solver::OROPT);
}

// Initial solution: 0->1->2->4, 3->5
// Neighbors:
// 0->2->1->4, 3->5
// 0->2->1->4, 3->5
TEST_F(OperatorTest, OrOptTest2) {
  SCOPED_TRACE("OrOptTest2");
  SetFirstSolution({1, 2, 4, 5});
  AddExpectedDelta({2, 4, 1, -1});
  AddExpectedDelta({2, 4, 1, -1});
  SolveAndTest(Solver::OROPT);
}

// Initial solution: 0->4, 1->2->3->5
// Neighbors:
// 0->4, 1->3->2->5
// 0->4, 1->3->2->5
TEST_F(OperatorTest, OrOptTest3) {
  SCOPED_TRACE("OrOptTest3");
  SetFirstSolution({4, 2, 3, 5});
  AddExpectedDelta({-1, 3, 5, 2});
  AddExpectedDelta({-1, 3, 5, 2});
  SolveAndTest(Solver::OROPT);
}

// Initial solution: 0->1->2->3->4
// Neighbors:
// 0->2->1->3->4
// 0->2->3->1->4
// 0->2->1->3->4
// 0->1->3->2->4
// 0->3->1->2->4
// 0->1->3->2->4
// 0->3->1->2->4
// 0->2->3->1->4
TEST_F(OperatorTest, OrOptTest4) {
  SCOPED_TRACE("OrOptTest4");
  SetFirstSolution({1, 2, 3, 4});
  AddExpectedDelta({2, 3, 1, -1});
  AddExpectedDelta({2, 4, -1, 1});
  AddExpectedDelta({2, 3, 1, -1});
  AddExpectedDelta({-1, 3, 4, 2});
  AddExpectedDelta({3, -1, 4, 1});
  AddExpectedDelta({-1, 3, 4, 2});
  AddExpectedDelta({3, -1, 4, 1});
  AddExpectedDelta({2, 4, -1, 1});
  SolveAndTest(Solver::OROPT);
}

// Relocate tests

TEST(OperatorNameTest, Relocate) {
  Solver solver("Test");
  EXPECT_EQ(
      MakeRelocate(&solver, {}, {}, nullptr, nullptr, nullptr)->DebugString(),
      "Relocate");
  EXPECT_EQ(solver.MakeOperator({}, Solver::RELOCATE)->DebugString(),
            "Relocate");
}

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->1->3
// 0->2->1->3
TEST_F(OperatorTest, Relocate1) {
  SCOPED_TRACE("Relocate1");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  AddExpectedDelta({2, 3, 1});
  SolveAndTest(Solver::RELOCATE);
}

TEST_P(OperatorTestWithNeighbors, Relocate1WithNeighbors) {
  SCOPED_TRACE("Relocate1N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  if (neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
    AddExpectedDelta({2, 3, 1});
  }
  SolveAndTest(Solver::RELOCATE, {2}, neighbor_option);
}

// Initial solution: 0->1->3, 2->4
// Neighbor: 0->3, 2->1->4
TEST_F(OperatorTest, Relocate2) {
  SCOPED_TRACE("Relocate2");
  SetFirstSolution({1, 3, 4});
  AddExpectedDelta({3, 4, 1});
  SolveAndTest(Solver::RELOCATE);
}

TEST_F(OperatorTest, Relocate2WithAllowedPaths) {
  SCOPED_TRACE("Relocate2WithAllowedPaths");
  SetFirstSolution({1, 3, 4}, {0, 0, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {0}, {}, {}, {}});
  SolveAndTest(Solver::RELOCATE);
}

TEST_P(OperatorTestWithNeighbors, Relocate2WithNeighbors) {
  SCOPED_TRACE("Relocate2N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 3, 4});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({3, 4, 1});
  }
  SolveAndTest(Solver::RELOCATE, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Relocate2WithNeighborsAndAllowedPaths) {
  SCOPED_TRACE("Relocate2WithNeighborsAndAllowedPaths");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 3, 4}, {0, 0, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {0}, {}, {}, {}});
  SolveAndTest(Solver::RELOCATE, {1}, neighbor_option);
}

// Initial solution: 0->3, 1->2->4
// Neighbor: 0->2->3, 1->4
TEST_F(OperatorTest, Relocate3) {
  SCOPED_TRACE("Relocate3");
  SetFirstSolution({3, 2, 4});
  AddExpectedDelta({2, 4, 3});
  SolveAndTest(Solver::RELOCATE);
}

// Initial solution: 0->2->4, 1->3->5
// Neighbors:
// 0->3->2->4, 1->5  // For outgoing neighbor of 0.
// 0->3->2->4, 1->5  // For incoming neighbor of 2.
// 0->2->3->4, 1->5  // For outgoing neighbor of 2.
TEST_P(OperatorTestWithNeighbors, Relocate4WithNeighbors) {
  SCOPED_TRACE("Relocate4N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({2, 3, 4, 5});
  AddExpectedDelta({3, 5, -1, 2});
  if (neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
    AddExpectedDelta({3, 5, -1, 2});
  }
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    AddExpectedDelta({-1, 5, 3, 4});
  }
  SolveAndTest(Solver::RELOCATE, {3}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Relocate4WithNeighborsAllowedPaths) {
  SCOPED_TRACE("Relocate4NAllowedPaths");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({2, 3, 4, 5}, {0, 1, 0, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {}, {}, {1}, {}, {}});
  SolveAndTest(Solver::RELOCATE, {3}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Relocate4WithNeighbors2) {
  SCOPED_TRACE("Relocate4NAllowedPaths2");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({2, 3, 4, 5}, {0, 1, 0, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {}, {0}, {}, {}, {}});
  AddExpectedDelta({3, 5, -1, 2});
  if (neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
    AddExpectedDelta({3, 5, -1, 2});
  }
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    AddExpectedDelta({-1, 5, 3, 4});
  }
  SolveAndTest(Solver::RELOCATE, {3}, neighbor_option);
}

// Initial solution: 0->3, 2->4, 1 unperformed
// No Neighbors
TEST_P(OperatorTestWithNeighbors, Relocate5WithNeighbors) {
  SCOPED_TRACE("Relocate5N");
  SetFirstSolution({3, 1, 4});
  SolveAndTest(Solver::RELOCATE, {1}, GetParam());
}

// Exchange tests

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->1->3
// 0->2->1->3
TEST_F(OperatorTest, Exchange1) {
  SCOPED_TRACE("Exchange1");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  AddExpectedDelta({2, 3, 1});
  SolveAndTest(Solver::EXCHANGE);
}

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->1->3  // 2 outgoing neighbor of 0.
// 0->2->1->3  // 2 incoming neighbor of 1.
TEST_P(OperatorTestWithNeighbors, Exchange1WithNeighbors) {
  SCOPED_TRACE("Exchange1N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 3, 1});
  if (neighbor_option == NeighborOption::kIncomingAndOutgoingNeighbors) {
    AddExpectedDelta({2, 3, 1});
  }
  SolveAndTest(Solver::EXCHANGE, {2}, neighbor_option);
}

// Initial solution: 0->1->4, 2->3->5
// Neighbors:
// 0->3->4, 2->1->5
// 0->3->4, 2->1->5
TEST_F(OperatorTest, Exchange2) {
  SCOPED_TRACE("Exchange2");
  SetFirstSolution({1, 4, 3, 5});
  AddExpectedDelta({3, 5, 1, 4});
  AddExpectedDelta({3, 5, 1, 4});
  SolveAndTest(Solver::EXCHANGE);
}

TEST_F(OperatorTest, Exchange2WithPathCompatibility1) {
  SCOPED_TRACE("Exchange2WithPathCompatibility1");
  SetFirstSolution({1, 4, 3, 5}, {0, 0, 1, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {0}, {}, {}, {}, {}});
  SolveAndTest(Solver::EXCHANGE);
}

TEST_F(OperatorTest, Exchange2WithPathCompatibility2) {
  SCOPED_TRACE("Exchange2WithPathCompatibility2");
  SetFirstSolution({1, 4, 3, 5}, {0, 0, 1, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {}, {}, {1}, {}, {}});
  SolveAndTest(Solver::EXCHANGE);
}

TEST_P(OperatorTestWithNeighbors, Exchange2WithNeighbors) {
  SCOPED_TRACE("Exchange2N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 4, 3, 5});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({3, 5, 1, 4});
  }
  SolveAndTest(Solver::EXCHANGE, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Exchange2WithNeighborsAllowedPaths1) {
  SCOPED_TRACE("Exchange2NAllowedPaths1");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 4, 3, 5}, {0, 0, 1, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {0}, {}, {}, {}, {}});
  SolveAndTest(Solver::EXCHANGE, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Exchange2WithNeighborsAllowedPaths2) {
  SCOPED_TRACE("Exchange2NAllowedPaths2");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 4, 3, 5}, {0, 0, 1, 1, 0, 1});
  SetAllowedSecondaryValues({{}, {}, {}, {1}, {}, {}});
  SolveAndTest(Solver::EXCHANGE, {1}, neighbor_option);
}

// Initial solution: 0->1->4->5, 2->3->6
// Neighbors:
// 0->4->1->5, 2->3->6
// 0->3->4->5, 2->1->6
TEST_P(OperatorTestWithNeighbors, Exchange3WithNeighbors) {
  SCOPED_TRACE("Exchange3N");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 4, 3, 6, 5});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({4, 5, -1, -1, 1});
    AddExpectedDelta({3, 6, 1, 4, -1});
  }
  SolveAndTest(Solver::EXCHANGE, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Exchange3WithNeighborsAllowedPaths) {
  SCOPED_TRACE("Exchange3NAllowedPaths");
  const NeighborOption neighbor_option = GetParam();
  SetFirstSolution({1, 4, 3, 6, 5}, {0, 0, 1, 1, 0, 0, 1});
  SetAllowedSecondaryValues({{}, {0}, {}, {}, {}, {}, {}});
  if (neighbor_option != NeighborOption::kIncomingNeighbors) {
    // We shouldn't have any deltas when only considering incoming neighbors.
    AddExpectedDelta({4, 5, -1, -1, 1});
  }
  SolveAndTest(Solver::EXCHANGE, {1}, neighbor_option);
}

// Initial solution: 0->4, 2->3->5, 1 unperformed
// No neighbors
TEST_F(OperatorTest, Exchange4) {
  SCOPED_TRACE("Exchange4");
  SetFirstSolution({4, 1, 3, 5});
  SolveAndTest(Solver::EXCHANGE);
}

class ExchangeAndMakeActiveOperatorWithPathClassTest : public BaseOperatorTest {
 protected:
  void SolveAndTest(std::function<int(int64_t)> path_class) {
    path_class_ = std::move(path_class);
    BaseSolveAndTest(true);
  }
  LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,
                                     Solver* solver) const override {
    return ExchangeAndMakeActive(solver, vars, {}, path_class_);
  }

  std::function<int(int64_t)> path_class_;
};

// Initial solution: 0->1->5, 4->2->6, 3 unperformed
// Neighbors:
// 0->2->5, 4->3->1->6
// 0->2->3->5, 4->1->6
// 0->3->2->5, 4->1->6
// 0->2->5, 4->1->3->6
TEST_F(ExchangeAndMakeActiveOperatorWithPathClassTest, OneInactiveNode) {
  SetFirstSolution({1, 5, 6, 3, 2});
  AddExpectedDelta({2, 6, 5, 1, 3});
  AddExpectedDelta({2, 6, 3, 5, 1});
  AddExpectedDelta({3, 6, 5, 2, 1});
  AddExpectedDelta({2, 3, 5, 6, 1});
  SolveAndTest([](int64_t) { return 0; });
}

// Initial solution: 0->1->4, 3->2->5
// No neighbors.
TEST_F(ExchangeAndMakeActiveOperatorWithPathClassTest, NoInactiveNode) {
  SetFirstSolution({1, 4, 5, 2});
  SolveAndTest([](int64_t) { return 0; });
}

// Initial solution: 0->3, 2->4, 1 unperformed
// No neighbors.
TEST_F(ExchangeAndMakeActiveOperatorWithPathClassTest, NoActiveNode) {
  SetFirstSolution({3, 1, 4});
  SolveAndTest([](int64_t) { return 0; });
}

class ExchangePathStartEndsAndMakeActiveOperatorWithPathClassTest
    : public BaseOperatorTest {
 protected:
  void SolveAndTest(std::function<int(int64_t)> path_class) {
    path_class_ = std::move(path_class);
    BaseSolveAndTest(true);
  }
  LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,
                                     Solver* solver) const override {
    return ExchangePathStartEndsAndMakeActive(solver, vars, {}, path_class_);
  }

  std::function<int(int64_t)> path_class_;
};

// Initial solution: 0->1->2->7, 6->3->4->8, 5 unperformed
// Neighbors:
// 0->3->4->7, 6->1->2->8
// 0->5->3->4->7, 6->1->2->8
// 0->3->4->7, 6->1->5->2->8
// 0->3->4->7, 6->1->2->5->8
// 0->3->4->7, 6->5->1->2->8
// 0->3->5->4->7, 6->1->2->8
// 0->3->4->5->7, 6->1->2->8

TEST_F(ExchangePathStartEndsAndMakeActiveOperatorWithPathClassTest,
       OneInactiveNode) {
  SetFirstSolution({1, 2, 7, 4, 8, 5, 3});
  AddExpectedDelta({5, -1, 8, -1, 7, 3, 1});
  AddExpectedDelta({3, 5, 8, -1, 7, 2, 1});
  AddExpectedDelta({3, -1, 5, -1, 7, 8, 1});
  AddExpectedDelta({3, -1, 8, -1, 7, 1, 5});
  AddExpectedDelta({3, -1, 8, 5, 7, 4, 1});
  AddExpectedDelta({3, -1, 8, -1, 5, 7, 1});
  SolveAndTest([](int64_t) { return 0; });
}

// Initial solution: 0->1->2->6, 5->3->4->7
// No neighbors.
TEST_F(ExchangePathStartEndsAndMakeActiveOperatorWithPathClassTest,
       NoInactiveNode) {
  SetFirstSolution({1, 2, 6, 4, 7, 3});
  SolveAndTest([](int64_t) { return 0; });
}

// Initial solution: 0->3, 2->4, 1 unperformed
// No neighbors.
TEST_F(ExchangePathStartEndsAndMakeActiveOperatorWithPathClassTest,
       NoActiveNode) {
  SetFirstSolution({3, 1, 4});
  SolveAndTest([](int64_t) { return 0; });
}

// Cross tests.

class CrossWithPathClassTest : public BaseOperatorTest {
 protected:
  void SolveAndTest(std::function<int(int64_t)> path_class,
                    const std::vector<int>& neighbors = {}) {
    path_class_ = std::move(path_class);
    neighbors_ = neighbors;
    BaseSolveAndTest(true);
  }
  LocalSearchOperator* BuildOperatorWithSecondaryVars(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      Solver* solver) const override {
    if (neighbors_.empty()) {
      return MakeCross(solver, vars, secondary_vars, path_class_);
    } else {
      return MakeCross(
          solver, vars, secondary_vars, path_class_,
          [this](int, int) -> const std::vector<int>& { return neighbors_; },
          [this](int, int) -> const std::vector<int>& { return neighbors_; });
    }
  }

  std::function<int(int64_t)> path_class_;
  std::vector<int> neighbors_;
};

// We make sure we generate all possible candidates, in order.
// Cross enumerates pairs of paths ordered by pair of starts.
// Then it enumerates all pairs of nodes of these paths, in path order, skipping
// the start nodes.
// The operator translates those enumerated tuples to actual neighbors.
// It skips neighbors that would be trivial, duplicates or equivalent
// to neighbors generated with another tuple.
// The tests show all neighbors that would be generated, and comments out
// those that should be skipped.

TEST_F(OperatorTest, Cross1) {
  SCOPED_TRACE("Cross1");
  SetFirstPaths(3, {{0, 1, 3}, {2, 4}});
  // Cross path starts.
  // 1 with 4.
  AddExpectedPaths({{0, 3}, {2, 1, 4}});  // 0
  // 3 with 4.
  // AddExpectedPaths({{0, 1, 3}, {2, 4}});  // 1

  // Cross path ends.
  // 4 with 1.
  // AddExpectedPaths({{0, 3}, {2, 1, 4}});  // 2 = 0
  // 4 with 3.
  // AddExpectedPaths({{0, 1, 3}, {2, 4}});  // 3 = 1

  SolveAndTest(Solver::CROSS);
}

TEST_F(CrossWithPathClassTest, Cross1WithPathCompatibility) {
  SCOPED_TRACE("Cross1WithPathCompatibility");
  SetFirstPaths(3, {{0, 1, 3}, {2, 4}}, /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {0}, {}});
  SolveAndTest([](int64_t) { return 0; });
}

TEST_P(OperatorTestWithNeighbors, Cross1WithNeighbors) {
  SCOPED_TRACE("Cross1N");
  SetFirstPaths(3, {{0, 1, 3}, {2, 4}});
  // Cross path starts.
  // 1 with 4.
  // AddExpectedPaths({{0, 3}, {2, 1, 4}});  // 0
  // 3 with 4.
  // AddExpectedPaths({{0, 1, 3}, {2, 4}});  // 1

  // Cross path ends.
  // 4 with 1.
  // AddExpectedPaths({{0, 3}, {2, 1, 4}});  // 2 = 0
  // 4 with 3.
  // AddExpectedPaths({{0, 1, 3}, {2, 4}});  // 3 = 1

  SolveAndTest(Solver::CROSS, {1}, GetParam());
}

TEST_F(OperatorTest, Cross2NotEquivalent) {
  SCOPED_TRACE("Cross2NotEquivalent");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});

  // Cross path starts.
  // 1 with 5.
  AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  // 2 with 5.
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 4 with 5.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  // Cross path ends.
  // 5 with 1.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 5 with 2.
  AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  // 5 with 4.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  SolveAndTest(Solver::CROSS);
}

TEST_F(CrossWithPathClassTest, Cross2WithPathCompatibility) {
  SCOPED_TRACE("Cross2WithPathCompatibility");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}}, /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {}, {0}, {}});
  AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  SolveAndTest([](int64_t) { return 0; });
}

TEST_F(CrossWithPathClassTest, Cross2WithPathCompatibilityNonHomogeneous) {
  SCOPED_TRACE("Cross2WithPathCompatibility");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}}, /*fill_secondary_vars=*/true);
  SetAllowedSecondaryValues({{}, {0}, {}, {}});
  AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  SolveAndTest([](int64_t v) { return v; });
}

// Initial solution: 0->1->2->4, 3->5
TEST_P(OperatorTestWithNeighbors, Cross2NotEquivalentWithNeighbors1) {
  SCOPED_TRACE("Cross2NotEquivalentN1");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});
  const NeighborOption neighbor_option = GetParam();

  // Cross path starts.
  // 1 with 5.
  // AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  // 2 with 5.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 4 with 5.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  // Cross path ends.
  // 5 with 1.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 5 with 2 (with 1 as incoming neighbor of 5).
  if (neighbor_option != NeighborOption::kOutgoingNeighbors) {
    AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  }
  // 5 with 4.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  SolveAndTest(Solver::CROSS, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Cross2NotEquivalentWithNeighbors2) {
  SCOPED_TRACE("Cross2NotEquivalentN2");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});

  // Cross path starts.
  // 1 with 5.
  // AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  // 2 with 5.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 4 with 5.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  // Cross path ends.
  // 5 with 1.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 5 with 2.
  // AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  // 5 with 4.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  SolveAndTest(Solver::CROSS, {2}, GetParam());
}

// Initial solution: 0->1->4, 3->2->5
TEST_P(OperatorTestWithNeighbors, Cross2NotEquivalentWithNeighbors3) {
  SCOPED_TRACE("Cross2NotEquivalentN3");
  SetFirstPaths(4, {{0, 1, 4}, {3, 2, 5}});

  const NeighborOption neighbor_option = GetParam();
  // Cross path ends.
  // 4 with 2.
  if (neighbor_option != NeighborOption::kOutgoingNeighbors) {
    AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});
  }

  SolveAndTest(Solver::CROSS, {1}, neighbor_option);
}

TEST_F(CrossWithPathClassTest, Cross2Equivalent) {
  SCOPED_TRACE("Cross2Equivalent");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});

  // Cross path starts.
  // 1 with 5.
  AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  // 2 with 5.
  AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // 4 with 5.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  // Do not cross path ends.
  SolveAndTest([](int64_t) { return 0; });
}

TEST_F(CrossWithPathClassTest, Cross2EquivalentWithNeighbors) {
  SCOPED_TRACE("Cross2EquivalentN");
  SetFirstPaths(4, {{0, 1, 2, 4}, {3, 5}});

  // Cross path starts.
  // 1 with 5.
  // AddExpectedPaths({{0, 2, 4}, {3, 1, 5}});
  // 2 with 5.
  // AddExpectedPaths({{0, 4}, {3, 1, 2, 5}});
  // AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  // 4 with 5.
  // AddExpectedPaths({{0, 1, 2, 4}, {3, 5}});

  // Cross path ends (in the presence of neighbors).
  // 5 with 1 as incoming neighbor -> Cross 2 with 5.
  AddExpectedPaths({{0, 1, 4}, {3, 2, 5}});
  SolveAndTest([](int64_t) { return 0; }, {1});
}

TEST_F(OperatorTest, Cross3NotEquivalent) {
  SCOPED_TRACE("Cross3NotEquivalent");
  SetFirstPaths(4, {{0, 1, 4}, {2, 3, 5}});

  // Cross path starts.
  // 1 with 3, 5.
  AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  AddExpectedPaths({{0, 4}, {2, 1, 3, 5}});
  // 4 with 3, 5.
  AddExpectedPaths({{0, 3, 1, 4}, {2, 5}});
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  // Cross path ends.
  // 2 with 3, 4.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  AddExpectedPaths({{0, 1, 3, 4}, {2, 5}});
  // 5 with 3, 4.
  AddExpectedPaths({{0, 4}, {2, 3, 1, 5}});
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  SolveAndTest(Solver::CROSS);
}

TEST_P(OperatorTestWithNeighbors, Cross3NotEquivalentWithNeighbors1) {
  SCOPED_TRACE("Cross3NotEquivalentN1");
  SetFirstPaths(4, {{0, 1, 4}, {2, 3, 5}});

  const NeighborOption neighbor_option = GetParam();
  // Cross path starts.
  // 1 with 3, 5.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  // AddExpectedPaths({{0, 4}, {2, 1, 3, 5}});
  // 4 with 3, 5.
  // AddExpectedPaths({{0, 3, 1, 4}, {2, 5}});
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  // Cross path ends.
  // 2 with 3, 4.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  if (neighbor_option != NeighborOption::kOutgoingNeighbors) {
    AddExpectedPaths({{0, 1, 3, 4}, {2, 5}});
  }
  // 5 with 3, 4.
  // AddExpectedPaths({{0, 4}, {2, 3, 1, 5}});
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  SolveAndTest(Solver::CROSS, {1}, neighbor_option);
}

TEST_P(OperatorTestWithNeighbors, Cross3NotEquivalentWithNeighbors2) {
  SCOPED_TRACE("Cross3NotEquivalentN2");
  SetFirstPaths(4, {{0, 1, 4}, {2, 3, 5}});

  const NeighborOption neighbor_option = GetParam();
  // Cross path starts.
  // 1 with 3, 5.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  // AddExpectedPaths({{0, 4}, {2, 1, 3, 5}});
  // 4 with 3, 5.
  // AddExpectedPaths({{0, 3, 1, 4}, {2, 5}});
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  // Cross path ends.
  // 2 with 3, 4.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});
  // AddExpectedPaths({{0, 1, 3, 4}, {2, 5}});
  // 5 with 3, 4.
  // 1 with 3 as incoming neighbor.
  if (neighbor_option != NeighborOption::kOutgoingNeighbors) {
    AddExpectedPaths({{0, 4}, {2, 3, 1, 5}});
  }
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});

  SolveAndTest(Solver::CROSS, {3}, neighbor_option);
}

TEST_F(CrossWithPathClassTest, Cross3Equivalent) {
  SCOPED_TRACE("Cross3Equivalent");
  SetFirstPaths(4, {{0, 1, 4}, {2, 3, 5}});  // initial.

  // Cross path starts.
  // 1 with 3, 5.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});  // 0 ~ initial.
  AddExpectedPaths({{0, 4}, {2, 1, 3, 5}});  // 1
  // 4 with 3, 5.
  AddExpectedPaths({{0, 3, 1, 4}, {2, 5}});  // 2
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});  // 3 = initial.

  // Do not cross path ends.
  SolveAndTest([](int64_t) { return 0; });
}

TEST_F(CrossWithPathClassTest, Cross3EquivalentWithNeighbors) {
  SCOPED_TRACE("Cross3EquivalentN");
  SetFirstPaths(4, {{0, 1, 4}, {2, 3, 5}});  // initial.

  // Cross path starts.
  // 1 with 3, 5.
  // AddExpectedPaths({{0, 3, 4}, {2, 1, 5}});  // 0 ~ initial.
  // AddExpectedPaths({{0, 4}, {2, 1, 3, 5}});  // 1
  // 4 with 3, 5.
  // AddExpectedPaths({{0, 3, 1, 4}, {2, 5}});  // 2
  // AddExpectedPaths({{0, 1, 4}, {2, 3, 5}});  // 3 = initial.

  // Cross path ends.
  // 3 and 4.
  AddExpectedPaths({{0, 1, 3, 4}, {2, 5}});
  SolveAndTest([](int64_t) { return 0; }, {1});
}

TEST_F(OperatorTest, Cross4NotEquivalent) {
  SCOPED_TRACE("Cross4NotEquivalent");
  SetFirstPaths(6, {{0, 1, 2, 6}, {3, 4, 5, 7}});
  // Cross path starts.
  // Cross 1 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 2, 6}, {3, 1, 5, 7}});
  AddExpectedPaths({{0, 4, 5, 2, 6}, {3, 1, 7}});
  AddExpectedPaths({{0, 2, 6}, {3, 1, 4, 5, 7}});
  // Cross 2 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 6}, {3, 1, 2, 5, 7}});
  AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});
  AddExpectedPaths({{0, 6}, {3, 1, 2, 4, 5, 7}});
  // Cross 6 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 1, 2, 6}, {3, 5, 7}});
  AddExpectedPaths({{0, 4, 5, 1, 2, 6}, {3, 7}});
  // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});

  // Cross path ends.
  // 4 with 1, 2, 6.
  // AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});
  AddExpectedPaths({{0, 1, 4, 5, 6}, {3, 2, 7}});
  AddExpectedPaths({{0, 1, 2, 4, 5, 6}, {3, 7}});
  // 5 with 1, 2, 6.
  AddExpectedPaths({{0, 5, 6}, {3, 4, 1, 2, 7}});
  AddExpectedPaths({{0, 1, 5, 6}, {3, 4, 2, 7}});
  AddExpectedPaths({{0, 1, 2, 5, 6}, {3, 4, 7}});
  // 7 with 1, 2, 6.
  AddExpectedPaths({{0, 6}, {3, 4, 5, 1, 2, 7}});
  AddExpectedPaths({{0, 1, 6}, {3, 4, 5, 2, 7}});
  // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});

  SolveAndTest(Solver::CROSS);
}

TEST_P(OperatorTestWithNeighbors, Cross4NotEquivalentWithNeighbors) {
  SCOPED_TRACE("Cross4NotEquivalentN");
  SetFirstPaths(6, {{0, 1, 2, 6}, {3, 4, 5, 7}});
  const NeighborOption neighbor_option = GetParam();

  // Cross path starts.
  // Cross 1 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 2, 6}, {3, 1, 5, 7}});
  // AddExpectedPaths({{0, 4, 5, 2, 6}, {3, 1, 7}});
  // AddExpectedPaths({{0, 2, 6}, {3, 1, 4, 5, 7}});
  // Cross 2 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 6}, {3, 1, 2, 5, 7}});
  // AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});
  // AddExpectedPaths({{0, 6}, {3, 1, 2, 4, 5, 7}});
  // Cross 6 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 1, 2, 6}, {3, 5, 7}});
  // AddExpectedPaths({{0, 4, 5, 1, 2, 6}, {3, 7}});
  // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});

  // Cross path ends.
  if (neighbor_option != NeighborOption::kOutgoingNeighbors) {
    // 4 with 1, 2, 6.
    // AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});
    AddExpectedPaths({{0, 1, 4, 5, 6}, {3, 2, 7}});
    // AddExpectedPaths({{0, 1, 2, 4, 5, 6}, {3, 7}});
    // 5 with 1, 2, 6.
    // AddExpectedPaths({{0, 5, 6}, {3, 4, 1, 2, 7}});
    AddExpectedPaths({{0, 1, 5, 6}, {3, 4, 2, 7}});
    // AddExpectedPaths({{0, 1, 2, 5, 6}, {3, 4, 7}});
    // 7 with 1, 2, 6.
    // AddExpectedPaths({{0, 6}, {3, 4, 5, 1, 2, 7}});
    // 7 with 1 as incoming neighbor -> Cross 7 with 2.
    AddExpectedPaths({{0, 1, 6}, {3, 4, 5, 2, 7}});
    // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});
  }

  SolveAndTest(Solver::CROSS, {1}, neighbor_option);
}

TEST_F(CrossWithPathClassTest, Cross4Equivalent) {
  SCOPED_TRACE("Cross4NotEquivalent");
  SetFirstPaths(6, {{0, 1, 2, 6}, {3, 4, 5, 7}});  // initial.
  // Cross path starts.
  // Cross 1 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 2, 6}, {3, 1, 5, 7}});  // 0
  AddExpectedPaths({{0, 4, 5, 2, 6}, {3, 1, 7}});  // 1
  AddExpectedPaths({{0, 2, 6}, {3, 1, 4, 5, 7}});  // 2
  // Cross 2 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 6}, {3, 1, 2, 5, 7}});  // 3
  // AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});  // 4 ~= initial.
  AddExpectedPaths({{0, 6}, {3, 1, 2, 4, 5, 7}});  // 5
  // Cross 6 with 4, 5, 7.
  AddExpectedPaths({{0, 4, 1, 2, 6}, {3, 5, 7}});  // 6
  AddExpectedPaths({{0, 4, 5, 1, 2, 6}, {3, 7}});  // 7
  // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});  // 8 == initial.

  // Do not cross path ends.
  SolveAndTest([](int64_t) { return 0; });
}

TEST_F(CrossWithPathClassTest, Cross4EquivalentWithNeighbors) {
  SCOPED_TRACE("Cross4NotEquivalentN");
  SetFirstPaths(6, {{0, 1, 2, 6}, {3, 4, 5, 7}});  // initial.
  // Cross path starts.
  // Cross 1 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 2, 6}, {3, 1, 5, 7}});  // 0
  // AddExpectedPaths({{0, 4, 5, 2, 6}, {3, 1, 7}});  // 1
  // AddExpectedPaths({{0, 2, 6}, {3, 1, 4, 5, 7}});  // 2
  // Cross 2 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 6}, {3, 1, 2, 5, 7}});  // 3
  // AddExpectedPaths({{0, 4, 5, 6}, {3, 1, 2, 7}});  // 4 ~= initial.
  // AddExpectedPaths({{0, 6}, {3, 1, 2, 4, 5, 7}});  // 5
  // Cross 6 with 4, 5, 7.
  // AddExpectedPaths({{0, 4, 1, 2, 6}, {3, 5, 7}});  // 6
  // AddExpectedPaths({{0, 4, 5, 1, 2, 6}, {3, 7}});  // 7
  // AddExpectedPaths({{0, 1, 2, 6}, {3, 4, 5, 7}});  // 8 == initial.
  // Cross path ends.
  // Cross 2 and 4.
  AddExpectedPaths({{0, 1, 4, 5, 6}, {3, 2, 7}});
  // Cross 2 and 5.
  AddExpectedPaths({{0, 1, 5, 6}, {3, 4, 2, 7}});

  // Cross path ends.
  // 7 with 1 as incoming neighbor -> Cross 2 with 7.
  AddExpectedPaths({{0, 1, 6}, {3, 4, 5, 2, 7}});
  SolveAndTest([](int64_t) { return 0; }, {1});
}

INSTANTIATE_TEST_SUITE_P(
    OperatorTestWithNeighborsInstance, OperatorTestWithNeighbors,
    ::testing::ValuesIn({NeighborOption::kIncomingNeighbors,
                         NeighborOption::kOutgoingNeighbors,
                         NeighborOption::kIncomingAndOutgoingNeighbors}));

TEST(OperatorBugTest, SkippedNeighborsInCross) {
  // Solution 1 paths: 0 -> 5, 1 -> 3 -> 6, 2 -> 4 -> 7.
  // It has 7 neighbors: if only node 3 moves, it can go to three places:
  // after 0, 2 or 4. Similarly if only node 4 moves, three places.
  // If both move, they are exchanged, there is only one choice.
  const std::vector<int> first_solution = {5, 3, 4, 6, 7};
  // Solution 2 paths: 0 -> 5, 1 -> 6, 2 -> 3 -> 4 -> 7.
  // It has 6 neighbors: two where only node 3 moves (after 0 or 1), two where
  // only node 4 moves, two where chain 3 -> 4 moves.
  const std::vector<int> second_solution = {5, 6, 3, 4, 7};
  Solver solver("Cross");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(first_solution.size(), 0, 7, "", &vars);
  Assignment assignment(&solver);
  assignment.Add(vars);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  for (int count = 1; count <= 7; ++count) {
    LocalSearchOperator* op = solver.MakeOperator(vars, Solver::CROSS);
    for (int i = 0; i < first_solution.size(); ++i) {
      assignment.SetValue(vars[i], first_solution[i]);
    }
    op->Start(&assignment);
    for (int i = 0; i < count; ++i) {
      EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
      delta.Clear();
    }
    for (int i = 0; i < first_solution.size(); ++i) {
      assignment.SetValue(vars[i], second_solution[i]);
    }
    op->Start(&assignment);
    int neighbor_count = 0;
    while (op->MakeNextNeighbor(&delta, &deltadelta)) {
      ++neighbor_count;
      delta.Clear();
    }
    EXPECT_EQ(6, neighbor_count);
  }
}

// Non-regression test. Check that when the number of class-equivalent non-empty
// paths increases during local search, path operators properly handle base
// nodes which have been moved to the new path.
TEST(OperatorBugTest, SkippedNodeWhenIncreasingPaths) {
  // 1 and 2 on path 0, 2 empty paths.
  const std::vector<int> first_solution = {1, 2, 5, 6, 7};
  // 1 on path 0, 2 on path 2 and one empty path.
  const std::vector<int> second_solution = {1, 5, 7, 6, 2};
  Solver solver("Relocate");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(first_solution.size(), 0, 7, "", &vars);
  Assignment assignment(&solver);
  assignment.Add(vars);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  LocalSearchOperator* op =
      MakeRelocate(&solver, vars, {}, [](int64_t) { return 0; });
  for (int i = 0; i < first_solution.size(); ++i) {
    assignment.SetValue(vars[i], first_solution[i]);
  }
  // Sync with first solution, do a move and resync to make sure path reduction
  // has been seen by the operator.
  op->Start(&assignment);
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  delta.Clear();
  op->Start(&assignment);
  // Make a move the make 2 a base node.
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  delta.Clear();
  // Sync with second solution which moves 2 to a path which was previously
  // empty.
  for (int i = 0; i < first_solution.size(); ++i) {
    assignment.SetValue(vars[i], second_solution[i]);
  }
  op->Start(&assignment);
  // Following calls will eventually crash when base node 2 is moved if the new
  // none empty path was not handled properly.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  }
}

// Inactive/Active neighborhood tests

// Initial solution: 0->1->3, 2 inactive
// Neighbors:
// 0->2->1->3
// 0->1->2->3
TEST_F(OperatorTest, MakeActive) {
  SCOPED_TRACE("MakeActiveTest");
  SetFirstSolution({1, 3, 2});
  AddExpectedDelta({2, -1, 1});
  AddExpectedDelta({-1, 2, 3});
  SolveAndTest(Solver::MAKEACTIVE);
}

// Initial solution: 0->1->4, 2,3 inactive
// Neighbors:
// 0->2->1->4, 3 inactive
// 0->1->2->4, 3 inactive
// 0->3->1->4, 2 inactive
// 0->1->3->4, 2 inactive
TEST_F(OperatorTest, MultipleMakeActive) {
  SCOPED_TRACE("MakeActiveTest");
  SetFirstSolution({1, 4, 2, 3});
  AddExpectedDelta({2, -1, 1, -1});
  AddExpectedDelta({-1, 2, 4, -1});
  AddExpectedDelta({3, -1, -1, 1});
  AddExpectedDelta({-1, 3, -1, 4});
  SolveAndTest(Solver::MAKEACTIVE);
}

#define OPERATOR_TEST(operator_name)                                       \
  class operator_name##Test : public BaseOperatorTest {                    \
   protected:                                                              \
    virtual LocalSearchOperator* BuildOperator(                            \
        const std::vector<IntVar*>& vars, Solver* solver) const {          \
      return operator_name(solver, vars, std::vector<IntVar*>(), nullptr); \
    }                                                                      \
  }

OPERATOR_TEST(MakeActiveAndRelocate);

// Initial solution: 0->4, 1->2->5, 3 inactive
// Neighbor: 0->3->2->4, 1->5
TEST_F(MakeActiveAndRelocateTest, MakeActiveAndRelocate) {
  SCOPED_TRACE("MakeActiveAndRelocate");
  SetFirstSolution({4, 2, 5, 3});
  AddExpectedDelta({3, 5, 4, 2});
  BaseSolveAndTest(true);
}

OPERATOR_TEST(RelocateAndMakeActive);

// Initial solution: 0->4, 1->2->5, 3 inactive
// Neighbor: 0->2->4, 1->3->5
TEST_F(RelocateAndMakeActiveTest, RelocateAndMakeActive1) {
  SCOPED_TRACE("RelocateAndMakeActive1");
  SetFirstSolution({4, 2, 5, 3});
  AddExpectedDelta({2, 3, 4, 5});
  BaseSolveAndTest(true);
}

// Initial solution: 0->2->4, 1->5, 3 inactive
// Neighbor: 0->3->4, 1->2->5
TEST_F(RelocateAndMakeActiveTest, RelocateAndMakeActive2) {
  SCOPED_TRACE("RelocateAndMakeActive2");
  SetFirstSolution({2, 5, 4, 3});
  AddExpectedDelta({3, 2, 5, 4});
  BaseSolveAndTest(true);
}

// Initial solution: 0->2->5, 1->3->6, 4 unperformed
// Neighbors:
// 0->3->2->5, 1->4->6
// 0->2->3->5, 1->4->6
// 0->4->5, 1->2->3->6
// 0->4->5, 1->3->2->6
TEST_F(RelocateAndMakeActiveTest, RelocateAndMakeActive3) {
  SCOPED_TRACE("RelocateAndMakeActive2");
  SetFirstSolution({2, 3, 5, 6, 4});
  AddExpectedDelta({3, 4, -1, 2, 6});
  AddExpectedDelta({-1, 4, 3, 5, 6});
  AddExpectedDelta({4, 2, 3, -1, 5});
  AddExpectedDelta({4, -1, 6, 2, 5});
  BaseSolveAndTest(true);
}

OPERATOR_TEST(RelocateAndMakeInactive);

// Initial solution: 0->2->4, 1->3->5
// Neighbors:
// 0->4, 1->2->5, 3 inactive
// 0->3->4, 1->5, 2 inactive
TEST_F(RelocateAndMakeInactiveTest, RelocateAndMakeInactiveOperator1) {
  SCOPED_TRACE("RelocateAndMakeInactiveOperator1");
  SetFirstSolution({2, 3, 4, 5});
  AddExpectedDelta({4, 2, 5, 3});
  AddExpectedDelta({3, 5, 2, 4});
  BaseSolveAndTest(true);
}

// Initial solution: 0->3->2->5, 1->4->6
// Neighbors:
// 0->2->5, 1->3->6, 4 unperformed
// 0->3->5, 1->2->6, 4 unperformed
// 0->4->2->5, 1->6, 3 unperformed
// 0->3->4->5, 1->6, 2 unperformed
TEST_F(RelocateAndMakeInactiveTest, RelocateAndMakeInactiveOperator2) {
  SCOPED_TRACE("RelocateAndMakeInactiveOperator2");
  SetFirstSolution({3, 4, 5, 2, 6});
  AddExpectedDelta({2, 3, -1, 6, 4});
  AddExpectedDelta({-1, 2, 6, 5, 4});
  AddExpectedDelta({4, 6, -1, 3, 2});
  AddExpectedDelta({-1, 6, 2, 4, 5});
  BaseSolveAndTest(true);
}

#undef OPERATOR_TEST

// Initial solution: 0->1->2
// Neighbor: 0->2, 1 inactive
TEST_F(OperatorTest, MakeInactive) {
  SCOPED_TRACE("MakeInactiveTest");
  SetFirstSolution({1, 2});
  AddExpectedDelta({2, 1});
  SolveAndTest(Solver::MAKEINACTIVE);
}

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->3, 1 inactive
// 0->1->3, 2 inactive
TEST_F(OperatorTest, MultipleMakeInactive) {
  SCOPED_TRACE("MakeInactiveTest");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 1, -1});
  AddExpectedDelta({-1, 3, 2});
  SolveAndTest(Solver::MAKEINACTIVE);
}

// Initial solution: 0->1->2->3->4
// 2 unskippable
// Neighbors:
// 0->2->3->4, 1 inactive
// 0->1->2->4, 3 inactive
TEST_F(OperatorTest, MakeChainInactiveWithUnskippable) {
  SCOPED_TRACE("MakeChainInactiveTest");
  SetFirstSolution({1, 2, 3, 4});
  AddExpectedDelta({2, 1, -1, -1});
  AddExpectedDelta({-1, -1, 4, 3});
  SetVarCallback(
      [](const std::vector<IntVar*>& vars) { vars[2]->RemoveValue(2); });
  SolveAndTest(Solver::MAKECHAININACTIVE);
}

// Initial solution: 0->1->2->3
// Neighbors:
// 0->2->3, 1 inactive
// 0->3, 1,2 inactive
// 0->1->3, 2 inactive
TEST_F(OperatorTest, MakeChainInactive) {
  SCOPED_TRACE("MakeChainInactiveTest");
  SetFirstSolution({1, 2, 3});
  AddExpectedDelta({2, 1, -1});
  AddExpectedDelta({3, 1, 2});
  AddExpectedDelta({-1, 3, 2});
  SolveAndTest(Solver::MAKECHAININACTIVE);
}

// Initial solution: 0->1->3, 2 inactive
// Neighbor: 0->2->3, 1 inactive
TEST_F(OperatorTest, SwapActive) {
  SCOPED_TRACE("SwapActiveTest");
  SetFirstSolution({1, 3, 2});
  AddExpectedDelta({2, 1, 3});
  SolveAndTest(Solver::SWAPACTIVE);
}

// Initial solution: 0->1->4, 2,3 inactive
// Neighbors:
// 0->2->4, 1,3 inactive
// 0->3->4, 1,2 inactive
TEST_F(OperatorTest, MultipleSwapActive) {
  SCOPED_TRACE("SwapActiveTest");
  SetFirstSolution({1, 4, 2, 3});
  AddExpectedDelta({2, 1, 4, -1});
  AddExpectedDelta({3, 1, -1, 4});
  SolveAndTest(Solver::SWAPACTIVE);
}

// Initial solution: 0->1->2->3
// No neighbors
TEST_F(OperatorTest, SwapActiveNoInactive) {
  SCOPED_TRACE("SwapActiveTest");
  SetFirstSolution({1, 2, 3});
  SolveAndTest(Solver::SWAPACTIVE);
}

#define OPERATOR_WITH_ARG_TEST(operator_name, arg_value)                  \
  class operator_name##arg_value##Test : public BaseOperatorTest {        \
   protected:                                                             \
    virtual LocalSearchOperator* BuildOperator(                           \
        const std::vector<IntVar*>& vars, Solver* solver) const {         \
      return operator_name(solver, vars, std::vector<IntVar*>(), nullptr, \
                           arg_value);                                    \
    }                                                                     \
  }

OPERATOR_WITH_ARG_TEST(MakeSwapActiveChain, kint32max);

// Initial solution: 0->1->2->4, 3 inactive
// Neighbors:
// 0->3->2->4, 1 inactive
// 0->3->4, 1,2 inactive
// 0->1->3->4, 2 inactive
TEST_F(MakeSwapActiveChainkint32maxTest, SwapActiveChain) {
  SCOPED_TRACE("SwapActiveChainTest");
  SetFirstSolution({1, 2, 4, 3});
  AddExpectedDelta({3, 1, -1, 2}, {});
  AddExpectedDelta({3, 1, 2, 4}, {-1, -1, 2, 4});
  AddExpectedDelta({-1, 3, 2, 4}, {});
  BaseSolveAndTest(true);
}

// Initial solution: 0->1->2->5, 3,4 inactive
// Neighbors:
// 0->3->2->5, 1,4 inactive
// 0->3->5, 1,2,4 inactive
// 0->1->3->5, 2,4 inactive
// 0->4->2->5, 1,3 inactive
// 0->4->5, 1,2,3 inactive
// 0->1->4->5, 2,3 inactive
TEST_F(MakeSwapActiveChainkint32maxTest, MultipleSwapActiveChain) {
  SCOPED_TRACE("SwapActiveChainTest");
  SetFirstSolution({1, 2, 5, 3, 4});
  AddExpectedDelta({3, 1, -1, 2, -1}, {});
  AddExpectedDelta({3, 1, 2, 5, -1}, {-1, -1, 2, 5, -1});
  AddExpectedDelta({-1, 3, 2, 5, -1}, {});
  AddExpectedDelta({4, 1, -1, -1, 2}, {});
  AddExpectedDelta({4, 1, 2, -1, 5}, {-1, -1, 2, -1, 5});
  AddExpectedDelta({-1, 4, 2, -1, 5}, {});
  BaseSolveAndTest(true);
}

// Initial solution: 0->1->2->3
// No neighbors
TEST_F(MakeSwapActiveChainkint32maxTest, SwapActiveChainNoInactive) {
  SCOPED_TRACE("SwapActiveChainTest");
  SetFirstSolution({1, 2, 3});
  BaseSolveAndTest(true);
}

OPERATOR_WITH_ARG_TEST(MakeSwapActiveChain, 1);

// Initial solution: 0->1->2->4, 3 inactive
// Neighbors:
// 0->3->2->4, 1 inactive
// 0->1->3->4, 2 inactive
TEST_F(MakeSwapActiveChain1Test, SwapActiveChain) {
  SCOPED_TRACE("SwapActiveChainTest");
  SetFirstSolution({1, 2, 4, 3});
  AddExpectedDelta({3, 1, -1, 2}, {});
  AddExpectedDelta({-1, 3, 2, 4}, {});
  BaseSolveAndTest(true);
}

OPERATOR_WITH_ARG_TEST(MakeSwapActiveChain, 2);

// Initial solution: 0->1->2->4, 3 inactive
// Neighbors:
// 0->3->2->4, 1 inactive
// 0->3->4, 1,2 inactive
// 0->1->3->4, 2 inactive
TEST_F(MakeSwapActiveChain2Test, SwapActiveChain) {
  SCOPED_TRACE("SwapActiveChainTest");
  SetFirstSolution({1, 2, 4, 3});
  AddExpectedDelta({3, 1, -1, 2}, {});
  AddExpectedDelta({3, 1, 2, 4}, {-1, -1, 2, 4});
  AddExpectedDelta({-1, 3, 2, 4}, {});
  BaseSolveAndTest(true);
}

#undef OPERATOR_WITH_ARG_TEST

// Initial solution: 0->1->2->4, 3 inactive
// Neighbors:
// 0->3->2->4, 1 inactive
// 0->2->3->4, 1 inactive
// 0->3->1->4, 2 inactive
// 0->1->3->4, 2 inactive
TEST_F(OperatorTest, ExtendedSwapActive) {
  SCOPED_TRACE("ExtendedSwapActiveTest");
  SetFirstSolution({1, 2, 4, 3});
  AddExpectedDelta({3, 1, -1, 2});
  AddExpectedDelta({2, 1, 3, 4});
  AddExpectedDelta({3, 4, 2, 1});
  AddExpectedDelta({-1, 3, 2, 4});
  SolveAndTest(Solver::EXTENDEDSWAPACTIVE);
}

TEST_F(OperatorTest, PathLns) {
  Solver solver("OperatorTest");
  Assignment assignment(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(3, 0, 3, "nodes", &vars);
  assignment.Add(vars);
  assignment.SetValue(vars[0], 1);
  assignment.SetValue(vars[1], 2);
  assignment.SetValue(vars[2], 3);
  LocalSearchOperator* const op = solver.MakeOperator(vars, Solver::PATHLNS);
  op->Start(&assignment);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
    EXPECT_FALSE(delta.Activated(vars[0]));
    EXPECT_FALSE(delta.Activated(vars[1]));
    EXPECT_FALSE(delta.Activated(vars[2]));
    delta.Clear();
  }
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[0]));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
    EXPECT_FALSE(delta.Activated(vars[1]));
    EXPECT_FALSE(delta.Activated(vars[2]));
    delta.Clear();
  }
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[0]));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
    EXPECT_FALSE(delta.Activated(vars[2]));
    delta.Clear();
  }
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[0]));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_TRUE(delta.Empty());
  EXPECT_FALSE(op->MakeNextNeighbor(&delta, &deltadelta));
}

TEST_F(OperatorTest, FullPathLns) {
  Solver solver("OperatorTest");
  // Assign the variables to form 3 paths of various lengths, and add a few
  // vars outside of any path. Verify that the neighborhood of the FULLPATHLNS
  // operator is as expected: deactivating a full path and all vars that aren't
  // part of a path.
  // TODO(user): similarly refactor PathLNS and UnactiveLNS.
  const int kNumNodes = 17;
  // Note: Paths are sorted deterministically by the PathOperator
  // preprocessing.
  // TODO(user): clarify that order here (lexicographic?).
  const std::vector<int> kPaths[3] = {
      {3, 5, 15, 8},
      {4, 9, 0, 11, 10, 12, 13, 14},
      {6, 1},
  };
  Assignment assignment(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(
      /*var_count=*/kNumNodes, /*vmin=*/0, /*vmax=*/kNumNodes, "nodes", &vars);
  assignment.Add(vars);
  std::vector<bool> in_a_path(kNumNodes, false);
  for (const std::vector<int>& path : kPaths) {
    for (int node : path) {
      in_a_path[node] = true;
    }
    for (int i = 1; i < path.size(); ++i) {
      assignment.SetValue(vars[path[i - 1]], path[i]);
    }
    assignment.SetValue(vars[path.back()], kNumNodes);
  }
  // Mark variables "not on any path" as such, by assigning their index to
  // themselves. Also remember them in the "not_in_a_path" set.
  std::set<int> not_in_a_path;
  for (int i = 0; i < kNumNodes; ++i) {
    if (!in_a_path[i]) {
      assignment.SetValue(vars[i], i);
      not_in_a_path.insert(i);
    }
  }

  // Explore the neighborhood and verify that it's as expected.
  LocalSearchOperator* const op =
      solver.MakeOperator(vars, Solver::FULLPATHLNS);
  op->Start(&assignment);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  for (const std::vector<int>& expected_unactivated_path : kPaths) {
    EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
    std::set<int> expected_unactivated = not_in_a_path;
    expected_unactivated.insert(expected_unactivated_path.begin(),
                                expected_unactivated_path.end());
    std::set<int> unactivated;
    for (int node = 0; node < kNumNodes; ++node) {
      // The neighbor assignment should only contain the deactivated variables:
      // this is LNS.
      if (delta.Contains(vars[node])) {
        ASSERT_FALSE(delta.Activated(vars[node]));
        unactivated.insert(node);
      }
    }
    EXPECT_THAT(unactivated, Eq(expected_unactivated));
    delta.Clear();
  }
  EXPECT_FALSE(op->MakeNextNeighbor(&delta, &deltadelta));
}

TEST_F(OperatorTest, UnactiveLns) {
  Solver solver("OperatorTest");
  Assignment assignment(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(3, 0, 3, "nodes", &vars);
  assignment.Add(vars);
  assignment.SetValue(vars[0], 1);
  assignment.SetValue(vars[1], 3);
  assignment.SetValue(vars[2], 2);
  LocalSearchOperator* const op =
      solver.MakeOperator(vars, Solver::UNACTIVELNS);
  op->Start(&assignment);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[0]));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[1]));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
  EXPECT_FALSE(delta.Activated(vars[2]));
  delta.Clear();
  EXPECT_FALSE(op->MakeNextNeighbor(&delta, &deltadelta));
}

class EvaluatorOperatorTest : public BaseOperatorTest {
 protected:
  void SolveAndTest(Solver::IndexEvaluator3 evaluator,
                    Solver::EvaluatorLocalSearchOperators operator_type,
                    bool match_all) {
    evaluator_ = std::move(evaluator);
    operator_type_ = operator_type;
    BaseSolveAndTest(match_all);
  }
  LocalSearchOperator* BuildOperator(const std::vector<IntVar*>& vars,
                                     Solver* solver) const override {
    return solver->MakeOperator(vars, evaluator_, operator_type_);
  }

  Solver::IndexEvaluator3 evaluator_;
  Solver::EvaluatorLocalSearchOperators operator_type_;
};

int64_t TestEval(int64_t i, int64_t j, int64_t) {
  return (i < j ? j - i : i - j);
}

// Initial solution: 0->2->1->3
// Neighbor: 0->1->2->3
TEST_F(EvaluatorOperatorTest, TSPOpt) {
  SCOPED_TRACE("TSPOpt");
  SetFirstSolution({2, 3, 1});
  AddExpectedDelta({1, 2, 3});
  SolveAndTest(TestEval, Solver::TSPOPT, true);
}

// Initial solution: 0->1->2->3->4->5->6->7->8->9->10->12->11->13
// Neighbors:
// 0->1->2->3->4->5->6->7->8->9->10->11->12->13
// 0->1->2->3->4->5->6->7->8->9->11->10->12->13
TEST_F(EvaluatorOperatorTest, TSPLns) {
  SCOPED_TRACE("TSPLNS");
  SetFirstSolution({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 11});
  AddExpectedDelta({-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 11, 12, 13});
  AddExpectedDelta({-1, -1, -1, -1, -1, -1, -1, -1, -1, 11, -1, 10, 13});
  SolveAndTest(TestEval, Solver::TSPLNS, false);
}

// TSPLns is an endless operator, making sure it respects search limits.
TEST(TSPLnsOperatorTest, WithSearchLimit) {
  Solver solver("TSPLns");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(13, 0, 13, "vars", &vars);
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(vars);
  const std::vector<int64_t> first_solution = {1, 2, 3,  4,  5,  6, 7,
                                               8, 9, 10, 12, 13, 11};
  for (int i = 0; i < vars.size(); ++i) {
    assignment->SetValue(vars[i], first_solution[i]);
  }

  const absl::Duration kTimeLimit = absl::Seconds(1);
  const int64_t start_time = solver.wall_time();
  IntVar* sum = solver.MakeSum(vars)->Var();
  EXPECT_TRUE(solver.Solve(
      solver.MakeLocalSearchPhase(
          assignment,
          solver.MakeLocalSearchPhaseParameters(
              sum, solver.MakeOperator(vars, TestEval, Solver::TSPLNS),
              nullptr)),
      solver.MakeTimeLimit(kTimeLimit), solver.MakeOptimize(false, sum, 1)));
  EXPECT_LT(absl::Milliseconds(solver.wall_time() - start_time),
            2 * kTimeLimit);
}

// Initial solution: 0->2->1->3
// Neighbor: 0->1->2->3
TEST_F(EvaluatorOperatorTest, LKTest1) {
  SCOPED_TRACE("LKTest1");
  SetFirstSolution({2, 3, 1});
  AddExpectedDelta({1, 2, 3});
  SolveAndTest(TestEval, Solver::LK, true);
}

// TODO(user): Not supported by LK yet
// TEST_F(OperatorTest, LKTest2) {
//   std::unique_ptr<Solver> solver(new Solver("OperatorTest"));
//   std::unique_ptr<Assignment> assignment(new Assignment(solver.get()));
//   vector<IntVar*> vars;
//   solver->MakeIntVarArray(4, 0, 4, "nodes", &vars);
//   assignment->Add(vars);
//   assignment->SetValue(vars[0], 3);
//   assignment->SetValue(vars[1], 2);
//   assignment->SetValue(vars[2], 4);
//   assignment->SetValue(vars[3], 1);
//   std::unique_ptr<Evaluator> eval(new TestEvaluator);
//   LocalSearchOperator* const op = solver->MakeOperator(vars, eval.get(),
//                                                  Solver::LK);
//   op->Start(assignment.get());
//   std::unique_ptr<Assignment> delta(new Assignment(solver.get()));
//   EXPECT_TRUE(op->MakeNextNeighbor(delta.get()));
//   EXPECT_EQ(delta->Value(vars[0]), 1);
//   EXPECT_EQ(delta->Value(vars[1]), 3);
//   EXPECT_EQ(delta->Value(vars[3]), 2);
//   delta.reset(new Assignment(solver.get()));
//   EXPECT_TRUE(op->MakeNextNeighbor(delta.get()));
//   EXPECT_EQ(delta->Value(vars[0]), 1);
//   EXPECT_EQ(delta->Value(vars[1]), 2);
//   EXPECT_EQ(delta->Value(vars[2]), 3);
//   EXPECT_EQ(delta->Value(vars[3]), 4);
//   delta.reset(new Assignment(solver.get()));
//   EXPECT_FALSE(op->MakeNextNeighbor(delta.get()));
// }

class TestPathOperator : public PathOperator</*ignore_path_vars=*/true> {
 public:
  TestPathOperator(const std::vector<IntVar*>& next_vars,
                   bool restart_at_path_start_on_synchronize,
                   bool restart_at_previous_base_node)
      : PathOperator(next_vars, /*path_vars=*/{}, /*number_of_base_nodes=*/2,
                     /*skip_locally_optimal_paths=*/true,
                     /*accept_path_end_base=*/true,
                     /*start_empty_path_class*/ nullptr,
                     /*get_incoming_neighbors=*/nullptr,
                     /*get_outgoing_neighbors=*/nullptr),
        restart_at_previous_base_node_(restart_at_previous_base_node),
        restart_at_path_start_on_synchronize_(
            restart_at_path_start_on_synchronize) {}
  ~TestPathOperator() override = default;
  bool MakeNeighbor() override { return true; }
  bool AreBaseNodesEqualTo(int64_t base0, int64_t base1) const {
    return BaseNode(0) == base0 && BaseNode(1) == base1;
  }
  void AddAlternative(int64_t node, int64_t alternative) {
    AddAlternativeSet({node, alternative});
  }
  std::string DebugString() const override { return "TestPathOperator"; }

 protected:
  int64_t GetBaseNodeRestartPosition(int base_index) override {
    return (base_index == 0 || !restart_at_previous_base_node_) ? StartNode(0)
                                                                : BaseNode(0);
  }
  bool RestartAtPathStartOnSynchronize() override {
    return restart_at_path_start_on_synchronize_;
  }

 private:
  const bool restart_at_previous_base_node_;
  const bool restart_at_path_start_on_synchronize_;
};

TEST(PathOperatorTest, RestartAtPathStartOnSynchronize) {
  for (bool restart_at_previous_base_node : {true, false}) {
    for (bool restart_at_path_start_on_synchronize : {true, false}) {
      Solver solver("PathOperatorTest");
      std::vector<IntVar*> vars;
      solver.MakeIntVarArray(5, 0, 5, "nexts", &vars);
      Assignment assignment(&solver);
      assignment.Add(vars);
      assignment.SetValue(vars[0], 1);
      assignment.SetValue(vars[1], 2);
      assignment.SetValue(vars[2], 3);
      assignment.SetValue(vars[3], 4);
      assignment.SetValue(vars[4], 5);
      TestPathOperator* const op = solver.RevAlloc(
          new TestPathOperator(vars, restart_at_path_start_on_synchronize,
                               restart_at_previous_base_node));
      op->Start(&assignment);
      Assignment delta(&solver);
      Assignment deltadelta(&solver);
      // Generate neighbors until base nodes have changed enough.
      while (!op->AreBaseNodesEqualTo(2, 4)) {
        EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
      }
      op->Start(&assignment);
      if (restart_at_path_start_on_synchronize) {
        EXPECT_TRUE(op->AreBaseNodesEqualTo(0, 0));
      } else {
        EXPECT_TRUE(op->AreBaseNodesEqualTo(2, 4));
      }
    }
  }
}

TEST(PathOperatorTest, RestartAtPathStartOnSynchronizeWithUnperformed) {
  for (bool restart_at_previous_base_node : {true, false}) {
    for (bool restart_at_path_start_on_synchronize : {true, false}) {
      Solver solver("PathOperatorTest");
      std::vector<IntVar*> vars;
      solver.MakeIntVarArray(5, 0, 5, "nexts", &vars);
      Assignment assignment(&solver);
      assignment.Add(vars);
      assignment.SetValue(vars[0], 1);
      assignment.SetValue(vars[1], 2);
      assignment.SetValue(vars[2], 3);
      assignment.SetValue(vars[3], 4);
      assignment.SetValue(vars[4], 5);
      TestPathOperator* const op = solver.RevAlloc(
          new TestPathOperator(vars, restart_at_path_start_on_synchronize,
                               restart_at_previous_base_node));
      op->Start(&assignment);
      Assignment delta(&solver);
      Assignment deltadelta(&solver);
      // Generate neighbors until base nodes have changed enough.
      while (!op->AreBaseNodesEqualTo(2, 4)) {
        EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
      }
      // Making second base node (4) unperformed.
      assignment.SetValue(vars[4], 4);
      assignment.SetValue(vars[3], 5);
      op->Start(&assignment);
      if (restart_at_path_start_on_synchronize) {
        EXPECT_TRUE(op->AreBaseNodesEqualTo(0, 0));
      } else {
        EXPECT_TRUE(
            op->AreBaseNodesEqualTo(2, restart_at_previous_base_node ? 2 : 0));
      }
    }
  }
}

TEST(PathOperatorTest, RestartAtPathStartOnSynchronizeWithAlternatives) {
  for (bool restart_at_previous_base_node : {true, false}) {
    for (bool restart_at_path_start_on_synchronize : {true, false}) {
      Solver solver("PathOperatorTest");
      std::vector<IntVar*> vars;
      solver.MakeIntVarArray(5, 0, 5, "nexts", &vars);
      Assignment assignment(&solver);
      assignment.Add(vars);
      assignment.SetValue(vars[0], 1);
      assignment.SetValue(vars[1], 2);
      assignment.SetValue(vars[2], 3);
      assignment.SetValue(vars[3], 5);
      assignment.SetValue(vars[4], 4);
      TestPathOperator* const op = solver.RevAlloc(
          new TestPathOperator(vars, restart_at_path_start_on_synchronize,
                               restart_at_previous_base_node));
      op->AddAlternative(3, 4);
      op->Start(&assignment);
      Assignment delta(&solver);
      Assignment deltadelta(&solver);
      // Generate neighbors until base nodes have changed enough.
      while (!op->AreBaseNodesEqualTo(2, 3)) {
        EXPECT_TRUE(op->MakeNextNeighbor(&delta, &deltadelta));
      }
      // Change alternative of node 3.
      assignment.SetValue(vars[3], 3);
      assignment.SetValue(vars[2], 4);
      assignment.SetValue(vars[4], 5);
      op->Start(&assignment);
      if (restart_at_path_start_on_synchronize) {
        EXPECT_TRUE(op->AreBaseNodesEqualTo(0, 0));
      } else {
        EXPECT_TRUE(
            op->AreBaseNodesEqualTo(2, restart_at_previous_base_node ? 2 : 0));
      }
    }
  }
}

int64_t Cost(int64_t, int64_t j) {
  return j;  // return value
}

// Test with/without an objective variable, with/without deltadelta.
class SumObjectiveFilterTest
    : public ::testing::TestWithParam<::testing::tuple<bool, bool>> {
 protected:
  bool TestObjective() { return ::testing::get<0>(GetParam()); }
  bool TestDeltadelta() { return ::testing::get<1>(GetParam()); }
};

void ResetAssignmentIntVars(Assignment* assignment, std::vector<IntVar*> vars,
                            std::vector<int64_t> values) {
  assignment->Clear();
  assignment->SetObjectiveRange(kint64min, kint64max);
  for (int i = 0; i < vars.size(); ++i) {
    assignment->FastAdd(vars[i])->SetValue(values[i]);
  }
}

TEST_P(SumObjectiveFilterTest, ObjectiveValuesAreCorrect) {
  Solver solver("SumObjectiveFilterTest");
  Assignment assignment(&solver);
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(3, 0, 5, "nodes", &vars);
  // {3, 4, 5} -> objective_value = 12
  ResetAssignmentIntVars(&assignment, vars, {3, 4, 5});

  IntVar* const objective = solver.MakeIntVar(0, kint32max);

  LocalSearchFilter* const filter =
      solver.MakeSumObjectiveFilter(vars, Cost, Solver::EQ);
  filter->Synchronize(&assignment, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 12);

  std::vector<int64_t> objective_value = {13, 11, 12, 15, 6};
  std::vector<int64_t> objective_max = {kint64max};
  if (TestObjective()) objective_max = {15, 14, 13, 12, 11, 10, 6, 5};

  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  for (int i = 0; i < objective_max.size(); ++i) {
    objective->SetMax(objective_max[i]);
    {
      // {4, 4, 5} -> objective_value = 13
      ResetAssignmentIntVars(&delta, {vars[0]}, {4});
      deltadelta.Clear();
      const bool expected_accept = objective_value[0] <= objective_max[i];
      EXPECT_EQ(
          filter->Accept(&delta, &deltadelta, kint64min, objective_max[i]),
          expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), objective_value[0]);
      }
    }

    {
      // {4, 2, 5} -> objective_value = 11
      ResetAssignmentIntVars(&delta, {vars[0], vars[1]}, {4, 2});
      if (TestDeltadelta()) ResetAssignmentIntVars(&deltadelta, {vars[1]}, {2});
      const bool expected_accept = objective_value[1] <= objective_max[i];
      EXPECT_EQ(
          filter->Accept(&delta, &deltadelta, kint64min, objective_max[i]),
          expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), objective_value[1]);
      }
    }

    {
      // {4, 3, 5} -> objective_value = 12
      ResetAssignmentIntVars(&delta, {vars[0], vars[1]}, {4, 3});
      if (TestDeltadelta()) ResetAssignmentIntVars(&deltadelta, {vars[1]}, {3});
      const bool expected_accept = objective_value[2] <= objective_max[i];
      EXPECT_EQ(
          filter->Accept(&delta, &deltadelta, kint64min, objective_max[i]),
          expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), objective_value[2]);
      }
    }

    {
      // {5, 5, 5} -> objective_value = 15
      ResetAssignmentIntVars(&delta, {vars[0], vars[1]}, {5, 5});
      if (TestDeltadelta()) {
        ResetAssignmentIntVars(&deltadelta, {vars[0], vars[1]}, {5, 5});
      }
      const bool expected_accept = objective_value[3] <= objective_max[i];
      EXPECT_EQ(
          filter->Accept(&delta, &deltadelta, kint64min, objective_max[i]),
          expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), objective_value[3]);
      }
    }

    {
      // {2, 2, 2} -> objective_value = 6
      ResetAssignmentIntVars(&delta, vars, {2, 2, 2});
      if (TestDeltadelta())
        ResetAssignmentIntVars(&deltadelta, vars, {2, 2, 2});
      const bool expected_accept = objective_value[4] <= objective_max[i];
      EXPECT_EQ(
          filter->Accept(&delta, &deltadelta, kint64min, objective_max[i]),
          expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), objective_value[4]);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SumObjectiveFilterTestInstantiation,
                         SumObjectiveFilterTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST(FilterTest, AcceptFilter) {
  Solver solver("AcceptFilterTest");
  LocalSearchFilter* filter = solver.MakeAcceptFilter();
  Assignment delta(&solver);
  filter->Synchronize(&delta, &delta);
  EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());

  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(3, 0, 10, "x", &vars);
  for (int i = 0; i < vars.size(); ++i) {
    Assignment deltadelta(&solver);
    delta.Add(vars[i]);
    delta.SetValue(vars[i], i);
    deltadelta.Add(vars[i]);
    deltadelta.SetValue(vars[i], i);
    EXPECT_TRUE(filter->Accept(&delta, &deltadelta, 0, 10));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
  }
}

TEST(FilterTest, RejectFilter) {
  Solver solver("RejectFilterTest");
  LocalSearchFilter* filter = solver.MakeRejectFilter();
  Assignment delta(&solver);
  filter->Synchronize(&delta, &delta);
  EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());

  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(3, 0, 10, "x", &vars);
  for (int i = 0; i < vars.size(); ++i) {
    Assignment deltadelta(&solver);
    delta.Add(vars[i]);
    delta.SetValue(vars[i], i);
    deltadelta.Add(vars[i]);
    deltadelta.SetValue(vars[i], i);
    EXPECT_FALSE(filter->Accept(&delta, &deltadelta, 0, 10));
  }
}

TEST(FilterTest, DomainFilter) {
  Solver solver("DomainFilterTest");
  std::vector<IntVar*> vars;
  const int kSize = 3;
  solver.MakeIntVarArray(kSize, 0, 2, "vars", &vars);
  vars[1]->SetValue(0);
  vars[2]->RemoveValue(1);
  Assignment deltadelta(&solver);
  const int kValue = 1;
  LocalSearchFilter* const filter = solver.MakeVariableDomainFilter();
  for (int i = 0; i < kSize; ++i) {
    Assignment delta(&solver);
    delta.Add(vars[i]);
    delta.SetValue(vars[i], kValue);
    EXPECT_EQ(vars[i]->Contains(kValue),
              filter->Accept(&delta, &deltadelta, kint64min, kint64max));
  }
}

class TestIntVarLocalSearchFilter : public IntVarLocalSearchFilter {
 public:
  explicit TestIntVarLocalSearchFilter(const std::vector<IntVar*>& vars)
      : IntVarLocalSearchFilter(vars) {}
  ~TestIntVarLocalSearchFilter() override {}
  bool Accept(const Assignment*, const Assignment*, int64_t, int64_t) override {
    return true;
  }
};

TEST(FilterTest, IntVarLocalSearchFilter) {
  const int kNumVars = 3;
  Solver solver("IntVarLocalSearchFilterTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kNumVars, 0, 5, "vars", &vars);
  std::unique_ptr<IntVarLocalSearchFilter> filter(
      new TestIntVarLocalSearchFilter(vars));
  EXPECT_EQ(kNumVars, filter->Size());
  for (int i = 0; i < kNumVars; ++i) {
    EXPECT_EQ(vars[i], filter->Var(i));
    int64_t index = -1;
    EXPECT_TRUE(filter->FindIndex(vars[i], &index));
    EXPECT_EQ(i, index);
  }
  Assignment assignment(&solver);
  filter->Synchronize(&assignment, nullptr);
  for (int i = 0; i < kNumVars; ++i) {
    EXPECT_FALSE(filter->IsVarSynced(i));
  }
  for (int i = 0; i < kNumVars; ++i) {
    assignment.FastAdd(vars[i])->SetValue(i);
  }
  filter->Synchronize(&assignment, nullptr);
  for (int i = 0; i < kNumVars; ++i) {
    EXPECT_TRUE(filter->IsVarSynced(i));
    EXPECT_EQ(i, filter->Value(i));
  }
  std::vector<IntVar*> extra_var(1, solver.MakeBoolVar());
  filter->AddVars(extra_var);
  EXPECT_EQ(kNumVars + 1, filter->Size());
  filter->Synchronize(&assignment, nullptr);
  EXPECT_FALSE(filter->IsVarSynced(kNumVars));
  assignment.FastAdd(extra_var.back())->SetValue(1);
  filter->Synchronize(&assignment, nullptr);
  EXPECT_TRUE(filter->IsVarSynced(kNumVars));
  EXPECT_EQ(1, filter->Value(kNumVars));
}

class LocalSearchTestOperator : public LocalSearchOperator {
 public:
  LocalSearchTestOperator(const std::string& id, std::string* log)
      : id_(id), log_(log), called_(false) {}
  ~LocalSearchTestOperator() override {}
  bool MakeNextNeighbor(Assignment*, Assignment*) override {
    if (!called_) {
      log_->append(id_);
      called_ = true;
      return true;
    }
    called_ = false;
    return false;
  }
  void Start(const Assignment*) override {}

 private:
  const std::string id_;
  std::string* const log_;
  bool called_;
};

class ConcatenateTest : public testing::Test {
 public:
  virtual LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) = 0;

 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("Test"); }
  void SetupOperators() {
    const int kNumberOfOperators = 4;
    std::vector<LocalSearchOperator*> operators;
    for (int i = 0; i < kNumberOfOperators; ++i) {
      operators.push_back(solver_->RevAlloc(
          new LocalSearchTestOperator(absl::StrCat(i), &log_)));
    }
    concat_ = MakeConcatenate(operators);
  }
  virtual void Run(const std::string& expected_log) {
    SetupOperators();
    // TODO(user): modify test operators in a way that the search stops.
    const int size = expected_log.size();
    Assignment delta(solver_.get());
    Assignment deltadelta(solver_.get());
    if (size == 0) {
      concat_->Start(&delta);
      EXPECT_FALSE(concat_->MakeNextNeighbor(&delta, &deltadelta));
      EXPECT_EQ(concat_->DebugString(), "");
      EXPECT_EQ(concat_->Self(), concat_);
    } else {
      for (int i = 0; i < size; ++i) {
        concat_->Start(&delta);
        concat_->MakeNextNeighbor(&delta, &deltadelta);
      }
      EXPECT_NE(concat_->DebugString(), "");
      EXPECT_NE(concat_->Self(), concat_);
    }
    EXPECT_EQ(expected_log, log_);
  }

  std::unique_ptr<Solver> solver_;
  LocalSearchOperator* concat_;
  std::string log_;
};

class EmptyConcatenateTest : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>&) override {
    std::vector<LocalSearchOperator*> empty_operators;
    return solver_->ConcatenateOperators(empty_operators);
  }
};

TEST_F(EmptyConcatenateTest, EmptyConcatenate) { Run(""); }

class NullOperatorConcatenateDeathTest : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    std::vector<LocalSearchOperator*> with_null_operators = operators;
    with_null_operators.insert(with_null_operators.begin(), nullptr);
    with_null_operators.push_back(nullptr);
    return solver_->ConcatenateOperators(with_null_operators);
  }
};

TEST_F(NullOperatorConcatenateDeathTest, NullOperatorConcatenate) {
  EXPECT_DEBUG_DEATH(Run(""), "Null operator in ConcatenateOperators");
}

class ConcatenateTest1 : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    return solver_->ConcatenateOperators(operators);
  }
};

TEST_F(ConcatenateTest1, Concatenate1) { Run("012301230123"); }

class ConcatenateTest2 : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    return solver_->ConcatenateOperators(operators, true);
  }
};

TEST_F(ConcatenateTest2, Concatenate2) { Run("010201030102010"); }

class ConcatenateTest3 : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    return solver_->ConcatenateOperators(
        operators, [this](int i, int j) { return Evaluate(i, j); });
  }
  int64_t Evaluate(int, int operator_index) {
    const int kWeights[] = {3, 2, 1, 0};
    return kWeights[operator_index];
  }
};

TEST_F(ConcatenateTest3, Concatenate3) { Run("323132303231323"); }

class RandomConcatenateTest : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    return solver_->RandomConcatenateOperators(operators);
  }
  void Run(const std::string& expected_log) override {
    SetupOperators();
    const int kNumberOfOperators = 4;
    const int size = expected_log.size();
    Assignment delta(solver_.get());
    Assignment deltadelta(solver_.get());
    for (int i = 0; i < size; ++i) {
      concat_->Start(&delta);
      concat_->MakeNextNeighbor(&delta, &deltadelta);
    }
    for (int i = 0; i < kNumberOfOperators; ++i) {
      EXPECT_NE(std::string::npos, log_.find(absl::StrFormat("%d", i)));
    }
  }
};

TEST_F(RandomConcatenateTest, RandomConcatenate) {
  Run("-------------------------------------");
}

class StagnationLimitedConcatenateTest : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>& operators) override {
    return solver_->MultiArmedBanditConcatenateOperators(
        operators, /*memory_coefficient=*/1, /*exploration_coefficient=*/1,
        /*maximize=*/false);
  }
};

TEST_F(StagnationLimitedConcatenateTest, Concatenate) { Run("0123"); }

class EmptyStagnationLimitedConcatenateTest : public ConcatenateTest {
 public:
  LocalSearchOperator* MakeConcatenate(
      const std::vector<LocalSearchOperator*>&) override {
    std::vector<LocalSearchOperator*> empty_operators;
    return solver_->MultiArmedBanditConcatenateOperators(
        empty_operators, /*memory_coefficient=*/1,
        /*exploration_coefficient=*/1, /*maximize=*/false);
  }
};

TEST_F(EmptyStagnationLimitedConcatenateTest, EmptyConcatenate) { Run(""); }

class EnterSearchTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("TestEnterSearch");
    solver_->MakeIntVarArray(kNumVars, 0, 1000, "Variable", &vars_);

    original_ = solver_->MakeAssignment();
    original_->Add(vars_);
    for (int i = 0; i < kNumVars; ++i) {
      original_->SetValue(vars_.at(i), 1000);
    }
    delta_ = solver_->MakeAssignment();
  }

  void Run(std::function<LocalSearchOperator*(
               Solver* solver, std::vector<LocalSearchOperator*>&)>
               concatenate,
           bool check_values) {
    int first_op_neighbors = 1;
    for (int base_nodes1 = 1; base_nodes1 < 3; ++base_nodes1) {
      first_op_neighbors *= kNumVars;
      int second_op_neighbors = 1;
      for (int base_nodes2 = 1; base_nodes2 < 3; ++base_nodes2) {
        SCOPED_TRACE(absl::StrFormat("base_nodes1: %d, base_nodes2: %d",
                                     base_nodes1, base_nodes2));
        second_op_neighbors *= kNumVars;
        std::vector<LocalSearchOperator*> ops;
        ops.push_back(
            solver_->RevAlloc(new CustomPathOperator(vars_, base_nodes1)));
        ops.push_back(
            solver_->RevAlloc(new CustomPathOperator(vars_, base_nodes2)));
        // Only explore the second operator halfway, to make sure the base
        // and compound indices are reset after calling EnterSearch() even when
        // the neighborhood wasn't fully explored in the previous round.
        TestEnterSearch(concatenate(solver_.get(), ops),
                        first_op_neighbors + (second_op_neighbors / 2),
                        check_values);
      }
    }
  }

 private:
  class CustomPathOperator : public PathOperator<true> {
   public:
    CustomPathOperator(const std::vector<IntVar*>& vars, int num_base_nodes)
        : PathOperator<true>(vars, {},
                             {.number_of_base_nodes = num_base_nodes}),
          num_base_nodes_(num_base_nodes) {}
    ~CustomPathOperator() override {}
    bool MakeNeighbor() override {
      SetValue(num_base_nodes_, BaseNode(num_base_nodes_ - 1));
      return true;
    }

   private:
    const int num_base_nodes_;
  };

  void TestEnterSearch(LocalSearchOperator* op, int max_num_neighbors,
                       bool check_values) {
    delta_->Clear();
    op->Start(original_);
    int num_neighbors = 0;
    std::vector<int> initial_values;
    while (num_neighbors < max_num_neighbors) {
      EXPECT_TRUE(op->MakeNextNeighbor(delta_, nullptr));
      num_neighbors++;
      ASSERT_EQ(delta_->IntVarContainer().Size(), 1);
      initial_values.push_back(delta_->IntVarContainer().Element(0).Value());
    }

    // Start a "new" search.
    op->EnterSearch();
    op->Start(original_);
    num_neighbors = 0;
    std::vector<int> new_values;
    while (num_neighbors < max_num_neighbors) {
      EXPECT_TRUE(op->MakeNextNeighbor(delta_, nullptr));
      num_neighbors++;
      ASSERT_EQ(delta_->IntVarContainer().Size(), 1);
      new_values.push_back(delta_->IntVarContainer().Element(0).Value());
    }
    EXPECT_EQ(new_values.size(), initial_values.size());
    if (check_values) {
      EXPECT_THAT(new_values, testing::ElementsAreArray(initial_values));
    }
  }

  static constexpr int kNumVars = 10;
  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Assignment* original_;
  Assignment* delta_;
};

TEST_F(EnterSearchTest, ConcatenateOperators) {
  Run(
      [](Solver* solver, std::vector<LocalSearchOperator*>& ops) {
        return solver->ConcatenateOperators(ops);
      },
      /*check_values=*/true);
}

TEST_F(EnterSearchTest, RandomConcatenateOperators) {
  Run(
      [](Solver* solver, std::vector<LocalSearchOperator*>& ops) {
        return solver->RandomConcatenateOperators(ops);
      },
      /*check_values=*/false);
}

TEST_F(EnterSearchTest, MABConcatenateOperators) {
  Run(
      [](Solver* solver, std::vector<LocalSearchOperator*>& ops) {
        return solver->MultiArmedBanditConcatenateOperators(
            ops, /*memory_coefficient=*/1, /*exploration_coefficient=*/1,
            /*maximize=*/false);
      },
      /*check_values=*/false);
}

class IntVarLocalSearchTestOperator : public IntVarLocalSearchOperator {
 public:
  explicit IntVarLocalSearchTestOperator(const std::vector<IntVar*>& vars)
      : IntVarLocalSearchOperator(vars) {}
  ~IntVarLocalSearchTestOperator() override {}
  bool MakeNextNeighbor(Assignment*, Assignment*) override { return false; }
};

TEST(IntVarLocalSearchOperatorTest, IntVarMismatch) {
  const int kAssignment[] = {0, 1, 2, 3, 4};
  const int kSubset[] = {1, 3, 4};
  Solver solver("IntVarLocalSearchOperatorTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(std::size(kAssignment), 0, 4, "Variable", &vars);
  std::vector<IntVar*> subvars(std::size(kSubset));
  for (int i = 0; i < std::size(kSubset); ++i) {
    subvars[i] = vars[kSubset[i]];
  }
  IntVarLocalSearchOperator* const op =
      solver.RevAlloc(new IntVarLocalSearchTestOperator(subvars));
  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(vars);
  for (int i = 0; i < std::size(kAssignment); ++i) {
    assignment->SetValue(vars[i], kAssignment[i]);
  }
  op->Start(assignment);
  for (int i = 0; i < std::size(kSubset); ++i) {
    EXPECT_EQ(kAssignment[kSubset[i]], op->Value(i)) << "for variable " << i;
  }
}

TEST(IntVarLocalSearchOperatorTest, IntVarOverflow) {
  const int kAssignment[] = {0, 1, 2};
  const int kVars = 5;
  Solver solver("IntVarLocalSearchOperatorTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kVars, 0, 4, "Variable", &vars);
  IntVarLocalSearchOperator* const op =
      solver.RevAlloc(new IntVarLocalSearchTestOperator(vars));
  Assignment* const assignment = solver.MakeAssignment();
  for (int i = 0; i < std::size(kAssignment); ++i) {
    assignment->Add(vars[i]);
    assignment->SetValue(vars[i], kAssignment[i]);
  }
  EXPECT_DEATH(op->Start(assignment),
               "Assignment contains fewer variables than operator");
}

TEST(IntVarLocalSearchOperatorTest, MissingIntVar) {
  const int kAssignment[] = {0, 1, 2};
  const int kVars = std::size(kAssignment);
  Solver solver("IntVarLocalSearchOperatorTest");
  std::vector<IntVar*> vars1;
  solver.MakeIntVarArray(kVars, 0, 4, "Variable1", &vars1);
  std::vector<IntVar*> vars2;
  solver.MakeIntVarArray(kVars, 0, 4, "Variable2", &vars2);
  IntVarLocalSearchOperator* const op =
      solver.RevAlloc(new IntVarLocalSearchTestOperator(vars1));
  Assignment* const assignment = solver.MakeAssignment();
  for (int i = 0; i < kVars; ++i) {
    assignment->Add(vars2[i]);
    assignment->SetValue(vars2[i], kAssignment[i]);
  }
  EXPECT_DEATH(
      op->Start(assignment),
      "Assignment does not contain operator variable " + vars1[0]->name());
}

TEST(RandomLnsTest, DeterministicRandomLns) {
  const int kNumIterations = 4;
  const int kRandomVariables = 4;
  const int kNumberOfVariables = 10;
  Solver solver("TestRandomLns");
  std::vector<IntVar*> vars;
  solver.MakeBoolVarArray(kNumberOfVariables, "", &vars);
  LocalSearchOperator* const random_lns =
      solver.MakeRandomLnsOperator(vars, kRandomVariables, /*seed=*/4444);
  std::vector<std::vector<int64_t>> vars_in_deltas;
  for (int i = 0; i < kNumIterations; ++i) {
    SCOPED_TRACE(absl::StrFormat("On iteration #%d", i));
    Assignment delta(&solver);
    EXPECT_TRUE(random_lns->MakeNextNeighbor(&delta, nullptr));
    std::vector<int64_t> vars_in_delta;
    for (int64_t var_index = 0; var_index < vars.size(); ++var_index) {
      if (delta.Contains(vars[var_index])) {
        vars_in_delta.push_back(var_index);
      }
    }
    vars_in_deltas.push_back(vars_in_delta);
  }
  EXPECT_THAT(vars_in_deltas,
              ElementsAre(ElementsAre(1, 4, 5, 9), ElementsAre(4, 6, 8, 9),
                          ElementsAre(5, 6, 9), ElementsAre(1, 2, 7)))
      << "NOTE: This is a golden test. It may legitimately fail if you changed "
         "the Lns implementation.";
}

// ----- Single Move Undoer test -----

class MoveTowardTargetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("TestSingleMoveUndoerTest");
    vars_.clear();
    solver_->MakeIntVarArray(kNumIntVars, 0, 3, &vars_);
    original_ = solver_->MakeAssignment();
    original_->Add(vars_);
    for (int i = 0; i < kNumIntVars; ++i) {
      original_->SetValue(vars_.at(i), 1);
    }
    undoer_ = solver_->MakeMoveTowardTargetOperator(*original_);
    // Copy the original assignment. Tests will modify the current one.
    current_ = solver_->MakeAssignment(original_);
    delta_ = solver_->MakeAssignment();
  }
  static constexpr int kNumIntVars = 8;
  std::vector<IntVar*> vars_;
  std::unique_ptr<Solver> solver_;
  Assignment* original_;
  Assignment* current_;
  LocalSearchOperator* undoer_;
  Assignment* delta_;
};

// Tests that no neighbors are generated when the current assignment equals
// the original.
TEST_F(MoveTowardTargetTest, NothingHasChanged) {
  undoer_->Start(current_);
  bool neighbor_generated = undoer_->MakeNextNeighbor(delta_, nullptr);
  EXPECT_FALSE(neighbor_generated);
}

// Tests that one neighbor is generated when the current assignment equals
// the original for all variable but one, with the neighbor being rejected.
TEST_F(MoveTowardTargetTest, OneChangedRestoreRejected) {
  const int kChangedVarIndex = 3;
  const int64_t kNewValue = 2;
  current_->SetValue(vars_.at(kChangedVarIndex), kNewValue);
  undoer_->Start(current_);
  bool neighbor_generated = undoer_->MakeNextNeighbor(delta_, nullptr);
  EXPECT_TRUE(neighbor_generated);
  EXPECT_EQ(1, delta_->Size());
  const IntVar* const changed_var = vars_.at(kChangedVarIndex);
  EXPECT_EQ(original_->Value(changed_var), delta_->Value(changed_var));
  // Reject the neighbor = not call Start again
  neighbor_generated = undoer_->MakeNextNeighbor(delta_, nullptr);
  EXPECT_FALSE(neighbor_generated);
}

// Tests that one neighbor is generated when the current assignment equals
// the original for all variable but one, with the neighbor being accepted.
TEST_F(MoveTowardTargetTest, OneChangedRestoreAccepted) {
  const int kChangedVarIndex = 3;
  const int64_t kNewValue = 2;
  current_->SetValue(vars_.at(kChangedVarIndex), kNewValue);
  undoer_->Start(current_);
  bool neighbor_generated = undoer_->MakeNextNeighbor(delta_, nullptr);
  EXPECT_TRUE(neighbor_generated);
  EXPECT_EQ(1, delta_->Size());
  const IntVar* const changed_var = vars_.at(kChangedVarIndex);
  EXPECT_EQ(original_->Value(changed_var), delta_->Value(changed_var));
  // Accept the neighbor = modify current + call start again
  current_->SetValue(changed_var, original_->Value(changed_var));
  undoer_->Start(current_);
  neighbor_generated = undoer_->MakeNextNeighbor(delta_, nullptr);
  EXPECT_FALSE(neighbor_generated);
}

// In this tests, all variables have changed, but only the odd ones should be
// restored: restoring the even ones would cause the objective to degrade.
//
// We check that the number of neighbors needed to reach to optimal solution
// is indeed linear in the number of variables. An implementation of the
// undoer that would reset the variable counter would produce a quadratic
// number of neighbors.
TEST_F(MoveTowardTargetTest, AllChangedOddRestored) {
  for (int i = 0; i < kNumIntVars; ++i) {
    current_->SetValue(vars_.at(i), (i % 2 == 0) ? 0 : 3);
  }
  IntVar* const sum = solver_->MakeSum(vars_)->Var();
  OptimizeVar* const optimize = solver_->MakeOptimize(false, sum, 1);
  DecisionBuilder* const sub = solver_->MakePhase(vars_, Solver::INT_VAR_SIMPLE,
                                                  Solver::INT_VALUE_SIMPLE);
  Assignment* const assignment = solver_->MakeAssignment();
  assignment->Add(vars_);
  SolutionCollector* const collector =
      solver_->MakeLastSolutionCollector(assignment);
  LocalSearchPhaseParameters* const param =
      solver_->MakeLocalSearchPhaseParameters(sum, undoer_,
                                              solver_->MakeSolveOnce(sub));
  DecisionBuilder* const db = solver_->MakeLocalSearchPhase(current_, param);
  // Restore the current_ assignment, then do the local search
  bool solution_found = solver_->Solve(db, optimize, collector);
  EXPECT_TRUE(solution_found);
  ASSERT_EQ(1, collector->solution_count());
  // Check the solution
  // Even variables should be like in current_ (= 0) while odd variable should
  // be like in original_ (= 1)
  const Assignment* const solution = collector->solution(0);
  for (int i = 0; i < kNumIntVars; ++i) {
    ASSERT_TRUE(solution->Contains(vars_.at(i)));
    EXPECT_EQ((i % 2 == 0) ? 0 : 1, solution->Value(vars_.at(i)));
  }
  // There should have been 8 neighbors generated in the first pass, 4 accepted,
  // then another four in the second pass, all rejected.
  EXPECT_EQ(0, kNumIntVars % 2);  // The checks are easier with an even number
  EXPECT_EQ(kNumIntVars + kNumIntVars / 2, solver_->neighbors());
  EXPECT_EQ(kNumIntVars / 2, solver_->accepted_neighbors());
}

// ----- Neighborhood limit test -----

class UpOne : public IntVarLocalSearchOperator {
 public:
  explicit UpOne(const std::vector<IntVar*>& vars)
      : IntVarLocalSearchOperator(vars), value_(0) {}

  ~UpOne() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override {
    CHECK(delta != nullptr);
    RevertChanges(true);
    SetValue(0, ++value_);
    ApplyChanges(delta, deltadelta);
    return true;
  }

 private:
  void OnStart() override { value_ = 0; }

  int value_;
};

TEST(NeighborhoodLimitTest, SimpleLimit) {
  const int kVarMax = 10;
  const int kLimit = 5;
  Solver solver("NeighborhoodLimitTest::TestSimpleLimit");
  IntVar* const var = solver.MakeIntVar(0, kVarMax);
  std::vector<IntVar*> one_var = {var};
  LocalSearchOperator* const up_one = solver.RevAlloc(new UpOne(one_var));
  LocalSearchOperator* const limited =
      solver.MakeNeighborhoodLimit(up_one, kLimit);
  Assignment solution(&solver);
  solution.Add(var);
  solution.SetValue(var, 0);
  limited->Start(&solution);
  for (int i = 0; i < kLimit; ++i) {
    Assignment delta(&solver);
    EXPECT_TRUE(limited->MakeNextNeighbor(&delta, nullptr));
    EXPECT_TRUE(delta.Contains(var));
    EXPECT_EQ(i + 1, delta.Value(var));
  }
  Assignment delta(&solver);
  EXPECT_FALSE(limited->MakeNextNeighbor(&delta, nullptr));
  limited->Start(&solution);
  EXPECT_TRUE(limited->MakeNextNeighbor(&delta, nullptr));
  EXPECT_TRUE(delta.Contains(var));
  EXPECT_EQ(1, delta.Value(var));
}

TEST(NeighborhoodLimitTest, DeathNegativeParameter) {
  const int kVarMax = 10;
  Solver solver("NeighborhoodLimitTest::TestDeathNegativeParameter");
  IntVar* const var = solver.MakeIntVar(0, kVarMax);
  std::vector<IntVar*> one_var = {var};
  LocalSearchOperator* const up_one = solver.RevAlloc(new UpOne(one_var));
  EXPECT_DEATH(solver.MakeNeighborhoodLimit(up_one, 0), "");
  EXPECT_DEATH(solver.MakeNeighborhoodLimit(up_one, -1), "");
}

TEST(LocalSearchProfilerTest, Profiling) {
  ConstraintSolverParameters param = Solver::DefaultSolverParameters();
  for (bool profile : {true, false}) {
    param.set_profile_local_search(profile);
    Solver solver("test", param);
    IntVar* const var = solver.MakeIntVar(0, 10);
    OptimizeVar* const optimize = solver.MakeOptimize(false, var, 1);
    DecisionBuilder* const fs = solver.RevAlloc(new SetAllToValue({var}, 10));
    fs->set_name("AllToValue");
    LocalSearchOperator* const decrement_value =
        solver.MakeOperator({var}, Solver::DECREMENT);
    LocalSearchFilterManager* ls_manager = solver.RevAlloc(
        new LocalSearchFilterManager({solver.MakeVariableDomainFilter()}));
    LocalSearchPhaseParameters* const ls_params =
        solver.MakeLocalSearchPhaseParameters(var, decrement_value, nullptr,
                                              nullptr, ls_manager);
    DecisionBuilder* const local_search =
        solver.MakeLocalSearchPhase({var}, fs, ls_params);
    const absl::Time start = absl::Now();
    EXPECT_TRUE(solver.Solve(local_search, optimize));
    const double duration_seconds = absl::ToDoubleSeconds(absl::Now() - start);
    if (profile) {
      double times[5];
      const std::string profile_str = solver.LocalSearchProfile();
      const int num_times_parsed =
          sscanf(profile_str.c_str(),
                 R"re(First solution statistics:
           | Time (s)
AllToValue | %lf
Local search operator statistics:
               | Neighbors | Filtered | Accepted | Time (s)
DecrementValue |        10 |       10 |       10 | %lf
         Total |        10 |       10 |       10 | %lf
Local search filter statistics:
                     |     Calls |   Rejects | Time (s) | Rejects/s
VariableDomainFilter |        10 |         0 | %lf |       0
               Total |        10 |         0 | %lf |       0
)re",
                 &times[0], &times[1], &times[2], &times[3], &times[4]);
      EXPECT_EQ(num_times_parsed, 5) << "profile_str:\n" << profile_str;
      // PrintOverview formats durations with %7.2g (2 significant digits).
      for (int i = 0; i < 5; ++i) {
        EXPECT_THAT(times[i], AllOf(Gt(0), Lt(duration_seconds * 1.1))) << i;
      }

      const LocalSearchStatistics stats = solver.GetLocalSearchStatistics();
      EXPECT_EQ(1, stats.first_solution_statistics_size());
      const LocalSearchStatistics::FirstSolutionStatistics& fs_stats =
          stats.first_solution_statistics(0);
      EXPECT_EQ("AllToValue", fs_stats.strategy());
      EXPECT_THAT(fs_stats.duration_seconds(),
                  AllOf(Gt(0), Lt(duration_seconds)));
      EXPECT_EQ(1, stats.local_search_operator_statistics_size());
      const LocalSearchStatistics::LocalSearchOperatorStatistics& op_stats =
          stats.local_search_operator_statistics(0);
      EXPECT_EQ("DecrementValue", op_stats.local_search_operator());
      EXPECT_LT(0, op_stats.num_neighbors());
      EXPECT_LT(0, op_stats.num_filtered_neighbors());
      EXPECT_LT(0, op_stats.num_accepted_neighbors());
      EXPECT_THAT(op_stats.duration_seconds(),
                  AllOf(Gt(0), Lt(duration_seconds)));
    } else {
      EXPECT_TRUE(solver.LocalSearchProfile().empty());
      EXPECT_EQ(0, solver.GetLocalSearchStatistics()
                       .local_search_operator_statistics_size());
    }
  }
}

TEST(SubDagComputerTest, EmptyDAG) {
  for (int num_nodes = 0; num_nodes < 8; ++num_nodes) {
    SubDagComputer computer;
    computer.BuildGraph(num_nodes);
    for (SubDagComputer::NodeId n(0); n.value() < num_nodes; ++n) {
      EXPECT_TRUE(computer.ComputeSortedSubDagArcs(n).empty());
    }
  }
}

TEST(SubDagComputerTest, OneArcDAG) {
  for (int num_nodes = 0; num_nodes < 8; ++num_nodes) {
    for (SubDagComputer::NodeId n1(0); n1.value() < num_nodes; ++n1) {
      for (SubDagComputer::NodeId n2(0); n2.value() < num_nodes; ++n2) {
        if (n1 == n2) continue;
        SubDagComputer computer;
        computer.AddArc(n1, n2);
        computer.BuildGraph(num_nodes);
        for (SubDagComputer::NodeId n(0); n.value() < num_nodes; ++n) {
          if (n == n1) {
            ASSERT_EQ(computer.ComputeSortedSubDagArcs(n).size(), 1);
            EXPECT_EQ(computer.ComputeSortedSubDagArcs(n).front().value(), 0);
          } else {
            EXPECT_TRUE(computer.ComputeSortedSubDagArcs(n).empty());
          }
        }
      }
    }
  }
}

TEST(SubDagComputerTest, PathPermutedNodes) {
  using NodeId = SubDagComputer::NodeId;
  for (int num_nodes = 1; num_nodes < 8; ++num_nodes) {
    // Test all possible paths of num_nodes nodes.
    std::vector<NodeId> nodes(num_nodes);
    std::iota(nodes.begin(), nodes.end(), NodeId(0));
    do {
      SubDagComputer computer;
      // Set DAG to path nodes[0] -> ... -> nodes[num_nodes-1].
      for (int n = 1; n < num_nodes; ++n) {
        computer.AddArc(nodes[n - 1], nodes[n]);
      }
      computer.BuildGraph(num_nodes);
      // SubDAG from position p should be
      // nodes[p] -> nodes[p+1] -> ... -> nodes[num_nodes-1].
      for (int p = 0; p < num_nodes; ++p) {
        const auto& arcs = computer.ComputeSortedSubDagArcs(nodes[p]);
        const int expected_num_arcs = num_nodes - p - 1;
        EXPECT_EQ(arcs.size(), expected_num_arcs);
        for (int a = 0; a < arcs.size(); ++a) {
          EXPECT_EQ(arcs[a].value(), a + p);
        }
      }
    } while (std::next_permutation(nodes.begin(), nodes.end()));
  }
}

TEST(SubDagComputerTest, PathPermutedArcs) {
  using NodeId = SubDagComputer::NodeId;
  using ArcId = SubDagComputer::ArcId;
  for (int num_nodes = 1; num_nodes < 8; ++num_nodes) {
    // Test DAG computation for DAG 0 -> ... -> num_nodes - 1,
    // with arcs passed in every possible order.
    // Arc with ArcId(a) has:
    // - tail tail_of_arc[a]
    // - head tail_of_arc[a]+1.
    util_intops::StrongVector<ArcId, NodeId> tail_of_arc(num_nodes - 1);
    std::iota(tail_of_arc.begin(), tail_of_arc.end(), NodeId(0));
    do {
      SubDagComputer computer;
      for (const ArcId a : tail_of_arc.index_range()) {
        const NodeId tail = tail_of_arc[a];
        const NodeId head = NodeId(tail_of_arc[a].value() + 1);
        EXPECT_EQ(a, computer.AddArc(tail, head));
      }
      computer.BuildGraph(num_nodes);
      // SubDAG from node n should be n -> ... -> (num_nodes - 1).
      for (NodeId n(0); n.value() < num_nodes; ++n) {
        const auto& arcs = computer.ComputeSortedSubDagArcs(n);
        const int expected_num_arcs = num_nodes - n.value() - 1;
        EXPECT_EQ(arcs.size(), expected_num_arcs);
        for (int a = 0; a < arcs.size(); ++a) {
          EXPECT_EQ(tail_of_arc[arcs[a]], NodeId(n.value() + a));
        }
      }
    } while (std::next_permutation(tail_of_arc.begin(), tail_of_arc.end()));
  }
}

TEST(SubDAGComputerTest, TournamentGraph) {
  using NodeId = SubDagComputer::NodeId;
  using ArcId = SubDagComputer::ArcId;
  for (int num_nodes = 1; num_nodes < 16; ++num_nodes) {
    SubDagComputer computer;
    util_intops::StrongVector<ArcId, std::pair<NodeId, NodeId>> arcs;
    // Build DAG with arcs i -> j for all i < j.
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = i + 1; j < num_nodes; ++j) {
        computer.AddArc(NodeId(i), NodeId(j));
        arcs.push_back(std::make_pair(NodeId(i), NodeId(j)));
      }
    }
    computer.BuildGraph(num_nodes);
    std::vector<ArcId> ordering = computer.ComputeSortedSubDagArcs(NodeId(0));
    // When i -> j is visited, all k -> i must have been visited.
    util_intops::StrongVector<NodeId, int> num_visited(num_nodes, 0);
    for (const ArcId a : ordering) {
      const auto& [tail, head] = arcs[a];
      EXPECT_EQ(num_visited[tail], tail.value());
      num_visited[head]++;
    }
    EXPECT_EQ(num_visited[NodeId(num_nodes - 1)], num_nodes - 1);
    // Check that all arcs are different.
    std::sort(ordering.begin(), ordering.end());
    EXPECT_EQ(std::unique(ordering.begin(), ordering.end()), ordering.end());
  }
}

TEST(SubDAGComputerTest, MultipleArcs) {
  using NodeId = SubDagComputer::NodeId;
  using ArcId = SubDagComputer::ArcId;
  for (int num_nodes = 1; num_nodes < 16; ++num_nodes) {
    SubDagComputer computer;
    util_intops::StrongVector<ArcId, std::pair<NodeId, NodeId>> arcs;
    // For each k, make k arcs 2k -> 2k+1.
    for (int k = 0; k < num_nodes; ++k) {
      for (int i = 0; i < k; ++i) {
        computer.AddArc(NodeId(2 * k), NodeId(2 * k + 1));
        arcs.push_back(std::make_pair(NodeId(2 * k), NodeId(2 * k + 1)));
      }
    }
    computer.BuildGraph(2 * num_nodes);
    for (int k = 0; k < num_nodes; ++k) {
      std::vector<ArcId> ordering =
          computer.ComputeSortedSubDagArcs(NodeId(2 * k));
      EXPECT_EQ(ordering.size(), k);
      for (const ArcId a : computer.ComputeSortedSubDagArcs(NodeId(0))) {
        const auto& [tail, head] = arcs[a];
        EXPECT_EQ(tail.value(), 2 * k);
        EXPECT_EQ(head.value(), 2 * k + 1);
      }
      std::sort(ordering.begin(), ordering.end());
      EXPECT_EQ(std::unique(ordering.begin(), ordering.end()), ordering.end());
    }
  }
}

TEST(SubDAGComputerDeathTest, Loop) {
  using NodeId = SubDagComputer::NodeId;
  SubDagComputer computer;
  computer.AddArc(NodeId(1), NodeId(3));
  computer.AddArc(NodeId(2), NodeId(2));
  EXPECT_DEBUG_DEATH(computer.BuildGraph(4), "Graph has a directed cycle");
}

TEST(SubDAGComputerDeathTest, SimpleDirectedCycle) {
  using NodeId = SubDagComputer::NodeId;
  SubDagComputer computer;
  computer.AddArc(NodeId(1), NodeId(2));
  computer.AddArc(NodeId(2), NodeId(1));
  EXPECT_DEBUG_DEATH(computer.BuildGraph(3), "Graph has a directed cycle");
}

TEST(SubDAGComputerDeathTest, DeepDirectedCycle) {
  using NodeId = SubDagComputer::NodeId;
  SubDagComputer computer;
  computer.AddArc(NodeId(0), NodeId(1));
  computer.AddArc(NodeId(1), NodeId(2));
  computer.AddArc(NodeId(2), NodeId(1));
  EXPECT_DEBUG_DEATH(computer.BuildGraph(3), "Graph has a directed cycle");
}

TEST(SubDAGComputerDeathTest, DeeperDirectedCycle) {
  using NodeId = SubDagComputer::NodeId;
  SubDagComputer computer;
  computer.AddArc(NodeId(0), NodeId(1));
  computer.AddArc(NodeId(1), NodeId(2));

  computer.AddArc(NodeId(3), NodeId(4));
  computer.AddArc(NodeId(3), NodeId(5));
  computer.AddArc(NodeId(4), NodeId(6));
  computer.AddArc(NodeId(5), NodeId(6));

  computer.AddArc(NodeId(6), NodeId(7));
  computer.AddArc(NodeId(7), NodeId(5));
  EXPECT_DEBUG_DEATH(computer.BuildGraph(8), "Graph has a directed cycle");
}

TEST(LocalSearchStateTest, DummyVariableInterface) {
  LocalSearchState::Variable variable = LocalSearchState::DummyVariable();
  EXPECT_FALSE(variable.Exists());
  EXPECT_TRUE(variable.SetMin(kint64max));
  EXPECT_TRUE(variable.SetMax(kint64min));
  variable.Relax();  // Does not crash.
}

TEST(LocalSearchStateTest, DummyVariableAccessorsDeathOnMin) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  LocalSearchState::Variable variable = LocalSearchState::DummyVariable();
  EXPECT_DEBUG_DEATH(variable.Min(), ".*");
}

TEST(LocalSearchStateTest, DummyVariableAccessorsDeathOnMax) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  LocalSearchState::Variable variable = LocalSearchState::DummyVariable();
  EXPECT_DEBUG_DEATH(variable.Max(), ".*");
}

using VariableDomainId = LocalSearchState::VariableDomainId;
std::pair<std::unique_ptr<LocalSearchState>, std::vector<VariableDomainId>>
MakeStateWithVariableDomains(
    absl::Span<const std::pair<int64_t, int64_t>> domains) {
  // Make a state and some domains, given their bounds.
  auto state = std::make_unique<LocalSearchState>();
  std::vector<VariableDomainId> domain_ids;
  domain_ids.reserve(domains.size());
  for (const auto [min, max] : domains) {
    domain_ids.push_back(state->AddVariableDomain(min, max));
  }
  return std::make_pair(std::move(state), domain_ids);
}

std::pair<std::unique_ptr<LocalSearchState>,
          std::vector<LocalSearchState::Variable>>
MakeStateWithVariables(absl::Span<const std::pair<int64_t, int64_t>> domains) {
  // Make a state and some variables, given their bounds.
  // The state must be heap-allocated, because variables have references to it.
  auto state = std::make_unique<LocalSearchState>();
  std::vector<LocalSearchState::Variable> variables;
  variables.reserve(domains.size());
  for (const auto [min, max] : domains) {
    const VariableDomainId domain_id = state->AddVariableDomain(min, max);
    variables.push_back(state->MakeVariable(domain_id));
  }
  state->CompileConstraints();  // No constraints.
  return std::make_pair(std::move(state), variables);
}

TEST(LocalSearchStateTest, InitialVariableRangeIsWholeInt64Range) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (const LocalSearchState::Variable& variable : variables) {
    EXPECT_TRUE(variable.Exists());
    EXPECT_EQ(variable.Min(), kint64min);
    EXPECT_EQ(variable.Max(), kint64max);
  }
  EXPECT_TRUE(state->StateIsFeasible());
}

TEST(LocalSearchStateTest, TighteningModifiesVariables) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  EXPECT_TRUE(state->StateIsFeasible());

  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
  EXPECT_TRUE(state->StateIsFeasible());
}

TEST(LocalSearchStateTest, TighteningMinTooFarReturnFalse) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  variables[0].Relax();
  EXPECT_TRUE(variables[0].SetMax(1000));
  EXPECT_TRUE(state->StateIsFeasible());
  EXPECT_FALSE(variables[0].SetMin(1001));
  EXPECT_FALSE(state->StateIsFeasible());
}

TEST(LocalSearchStateTest, TighteningMaxTooFarReturnFalse) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  variables[0].Relax();
  EXPECT_TRUE(variables[0].SetMin(1000));
  EXPECT_TRUE(state->StateIsFeasible());
  EXPECT_FALSE(variables[0].SetMax(999));
  EXPECT_FALSE(state->StateIsFeasible());
}

TEST(LocalSearchStateTest, ChangeRelaxedIntervalWidensInitialDomain) {
  auto state = std::make_unique<LocalSearchState>();
  const VariableDomainId domain_id = state->AddVariableDomain(0, 1);
  state->CompileConstraints();
  LocalSearchState::Variable variable = state->MakeVariable(domain_id);
  // Initial bounds prevent variable from going outside of [0, 1].
  {
    variable.Relax();
    EXPECT_FALSE(variable.SetMin(2));
    state->Revert();
  }
  {
    variable.Relax();
    EXPECT_FALSE(variable.SetMax(-1));
    state->Revert();
  }
  // ChangeRelaxedVariableDomain() widens the target domain of Relax().
  state->ChangeRelaxedVariableDomain(domain_id, -1, 2);
  {
    variable.Relax();
    EXPECT_EQ(variable.Min(), -1);
    EXPECT_EQ(variable.Max(), 2);
    EXPECT_TRUE(variable.SetMin(2));
    EXPECT_FALSE(variable.SetMin(3));
    state->Revert();
  }
  {
    variable.Relax();
    EXPECT_EQ(variable.Min(), -1);
    EXPECT_EQ(variable.Max(), 2);
    EXPECT_TRUE(variable.SetMax(-1));
    EXPECT_FALSE(variable.SetMax(-2));
    state->Revert();
  }
}

TEST(LocalSearchStateTest, ChangeRelaxedIntervalsTightensInitialDomain) {
  auto state = std::make_unique<LocalSearchState>();
  const VariableDomainId domain_id = state->AddVariableDomain(0, 10);
  state->CompileConstraints();
  LocalSearchState::Variable variable = state->MakeVariable(domain_id);
  // Initial bounds prevent variable from going outside of [0, 10].
  {
    variable.Relax();
    EXPECT_TRUE(variable.SetMin(10));
    state->Revert();
  }
  {
    variable.Relax();
    EXPECT_TRUE(variable.SetMax(0));
    state->Revert();
  }
  // ChangeRelaxedVariableDomain() tightens the target domain of Relax().
  state->ChangeRelaxedVariableDomain(domain_id, 2, 8);
  {
    variable.Relax();
    EXPECT_EQ(variable.Min(), 2);
    EXPECT_EQ(variable.Max(), 8);
    EXPECT_TRUE(variable.SetMin(8));
    EXPECT_FALSE(variable.SetMin(9));
    state->Revert();
  }
  {
    variable.Relax();
    EXPECT_EQ(variable.Min(), 2);
    EXPECT_EQ(variable.Max(), 8);
    EXPECT_TRUE(variable.SetMax(2));
    EXPECT_FALSE(variable.SetMax(1));
    state->Revert();
  }
}

TEST(LocalSearchStateTest, ChangeRelaxedIntervalsMakeDomainInfeasible) {
  auto state = std::make_unique<LocalSearchState>();
  const VariableDomainId domain_id = state->AddVariableDomain(0, 10);
  state->CompileConstraints();
  LocalSearchState::Variable variable = state->MakeVariable(domain_id);
  // Commit variable to [4, 6], within relaxed bounds.
  {
    variable.Relax();
    variable.SetMin(4);
    variable.SetMax(6);
    EXPECT_TRUE(state->StateIsFeasible());
    state->Commit();
  }
  // Relax/Revert within relaxed bounds keeps state feasible.
  {
    variable.Relax();
    EXPECT_TRUE(state->StateIsFeasible());
    state->Revert();
    EXPECT_TRUE(state->StateIsFeasible());
  }
  // New relaxed bounds overlap committed bounds below: feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 2, 4);
    EXPECT_TRUE(state->StateIsFeasible());
  }
  // New relaxed bounds strictly below committed bounds: not feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 1, 3);
    EXPECT_FALSE(state->StateIsFeasible());
    variable.Relax();
    EXPECT_TRUE(state->StateIsFeasible());
    state->Revert();
    EXPECT_FALSE(state->StateIsFeasible());
  }
  // New relaxed bounds overlap committed bounds above: feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 6, 8);
    EXPECT_TRUE(state->StateIsFeasible());
  }
  // New relaxed bounds strictly above committed bounds: not feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 7, 9);
    EXPECT_FALSE(state->StateIsFeasible());
    variable.Relax();
    EXPECT_TRUE(state->StateIsFeasible());
    state->Revert();
    EXPECT_FALSE(state->StateIsFeasible());
  }
  // New relaxed bounds strictly contain committed bounds: feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 3, 7);
    EXPECT_TRUE(state->StateIsFeasible());
  }
  // New relaxed bounds strictly contained by committed bounds: feasible.
  {
    state->ChangeRelaxedVariableDomain(domain_id, 5, 5);
    EXPECT_TRUE(state->StateIsFeasible());
  }
}

TEST(LocalSearchStateTest, RevertRestoresInitialState) {
  const std::vector<std::pair<int64_t, int64_t>> domains{
      {-1, 1}, {-2, 2}, {-3, 3}, {-4, 4}, {-5, 5}};
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Revert();
  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(domains[i].first, variables[i].Min());
    EXPECT_EQ(domains[i].second, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, RevertRestoresInitialStateAfterFailure) {
  const std::vector<std::pair<int64_t, int64_t>> domains{
      {-1, 1}, {-2, 2}, {-3, 3}, {-4, 4}, {-5, 5}};
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  EXPECT_FALSE(variables[0].SetMin(1));
  EXPECT_FALSE(state->StateIsFeasible());
  state->Revert();
  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(domains[i].first, variables[i].Min());
    EXPECT_EQ(domains[i].second, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, CommitKeepsCurrentValues) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  EXPECT_TRUE(state->StateIsFeasible());
  state->Commit();
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
  EXPECT_TRUE(state->StateIsFeasible());
}

TEST(LocalSearchStateTest, RevertFromCommitKeepsCurrentValues) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Commit();
  state->Revert();
  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, RevertFromValidStateReturnsToLastSavedState) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Commit();

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i + 10));
    EXPECT_TRUE(variables[i].SetMax(i + 10));
  }

  state->Revert();
  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, RevertFromInvalidStateReturnsToLastSavedState) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Commit();

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i + 10));
    EXPECT_TRUE(variables[i].SetMax(i + 10));
  }
  EXPECT_FALSE(variables[0].SetMin(0 + 10 + 1));
  EXPECT_FALSE(state->StateIsFeasible());

  state->Revert();
  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, RevertFromValidStateLeavesUnmodifiedVariables) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Commit();

  variables[1].Relax();
  variables[1].SetMin(0);
  variables[1].SetMin(2);
  state->Revert();

  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, RevertFromInvalidStateLeavesUnmodifiedVariables) {
  const std::vector<std::pair<int64_t, int64_t>> domains(
      5, {kint64min, kint64max});
  auto [state, variables] = MakeStateWithVariables(domains);

  for (int i = 0; i < variables.size(); ++i) {
    variables[i].Relax();
    EXPECT_TRUE(variables[i].SetMin(-i));
    EXPECT_TRUE(variables[i].SetMax(i));
  }
  state->Commit();

  variables[1].Relax();
  EXPECT_TRUE(variables[1].SetMin(0));
  EXPECT_TRUE(variables[1].SetMin(2));
  state->Revert();

  EXPECT_TRUE(state->StateIsFeasible());
  for (int i = 0; i < variables.size(); ++i) {
    EXPECT_EQ(-i, variables[i].Min());
    EXPECT_EQ(i, variables[i].Max());
  }
}

TEST(LocalSearchStateTest, BasicConstraintPropagation) {
  const std::vector<std::pair<int64_t, int64_t>> bounds = {
      {0, 2}, {0, 2}, {0, 1}};
  auto [state, domains] = MakeStateWithVariableDomains(bounds);
  // 1 * domains[0] + 0 == domains[1].
  state->AddWeightedSumConstraint({domains[0]}, {1}, 0, domains[1]);
  // 1 * domains[1] + 0 == domains[2].
  state->AddWeightedSumConstraint({domains[1]}, {1}, 0, domains[2]);
  state->CompileConstraints();

  state->RelaxVariableDomain(domains[0]);
  state->PropagateRelax(domains[0]);
  // Setting domains[0] to {1, 2}.
  EXPECT_TRUE(state->TightenVariableDomainMin(domains[0], 1));
  EXPECT_TRUE(state->PropagateTighten(domains[0]));
  EXPECT_EQ(state->VariableDomainMin(domains[1]), 1);
  EXPECT_EQ(state->VariableDomainMin(domains[2]), 1);
  // Setting domains[0] to {2, 2} fails at propagation step.
  EXPECT_TRUE(state->TightenVariableDomainMin(domains[0], 2));
  EXPECT_FALSE(state->PropagateTighten(domains[0]));
}

TEST(LocalStateSearchTest, WeightedSumWithNegativeWeights) {
  const std::vector<std::pair<int64_t, int64_t>> bounds(3,
                                                        {kint64min, kint64max});
  auto [state, domains] = MakeStateWithVariableDomains(bounds);
  // 2 * domains[0] - 3 * domains[1] == domains[2].
  state->AddWeightedSumConstraint({domains[0], domains[1]}, {2, -3}, 0,
                                  domains[2]);
  state->CompileConstraints();
  // Prepare to change bounds.
  state->RelaxVariableDomain(domains[0]);
  state->PropagateRelax(domains[0]);
  state->RelaxVariableDomain(domains[1]);
  state->PropagateRelax(domains[1]);
  // Increasing domains[0] min does not change domains[2] min or max.
  EXPECT_TRUE(state->TightenVariableDomainMin(domains[0], 10));
  EXPECT_TRUE(state->PropagateTighten(domains[0]));
  EXPECT_EQ(state->VariableDomainMin(domains[2]), kint64min);
  EXPECT_EQ(state->VariableDomainMax(domains[2]), kint64max);
  // Decreasing domains[1] max changes domains[2] min but not max.
  EXPECT_TRUE(state->TightenVariableDomainMax(domains[1], 5));
  EXPECT_TRUE(state->PropagateTighten(domains[1]));
  EXPECT_EQ(state->VariableDomainMin(domains[2]), 2 * 10 - 3 * 5);
  EXPECT_EQ(state->VariableDomainMax(domains[2]), kint64max);
  // Decreasing domains[0] max does not change domains[2] min or max.
  EXPECT_TRUE(state->TightenVariableDomainMax(domains[0], 12));
  EXPECT_TRUE(state->PropagateTighten(domains[0]));
  EXPECT_EQ(state->VariableDomainMin(domains[2]), 2 * 10 - 3 * 5);
  EXPECT_EQ(state->VariableDomainMax(domains[2]), kint64max);
  // Increasing domains[1] min changes domains[2] max but not min.
  EXPECT_TRUE(state->TightenVariableDomainMin(domains[1], 2));
  EXPECT_TRUE(state->PropagateTighten(domains[1]));
  EXPECT_EQ(state->VariableDomainMin(domains[2]), 2 * 10 - 3 * 5);
  EXPECT_EQ(state->VariableDomainMax(domains[2]), 2 * 12 - 3 * 2);
}

TEST(LocalStateSearchTest, WeightedSumWithOffsets) {
  const std::vector<std::pair<int64_t, int64_t>> bounds = {
      {0, 100}, {0, 100}, {0, 300}};
  auto [state, domains] = MakeStateWithVariableDomains(bounds);
  // domains[0] + domains[1] + 20 == domains[2].
  state->AddWeightedSumConstraint({domains[0], domains[1]}, {1, 1}, 20,
                                  domains[2]);
  state->CompileConstraints();
  state->RelaxVariableDomain(domains[0]);
  state->PropagateRelax(domains[0]);
  state->RelaxVariableDomain(domains[1]);
  state->PropagateRelax(domains[1]);
  // Changing a variable should propagate the sum offset along with the change.
  EXPECT_TRUE(state->TightenVariableDomainMin(domains[0], 1));
  EXPECT_TRUE(state->PropagateTighten(domains[0]));
  EXPECT_EQ(state->VariableDomainMin(domains[2]), 21);
  EXPECT_EQ(state->VariableDomainMax(domains[2]), 220);
}

TEST(LocalSearchStateTest, WeightedSumBackAndForth) {
  // Two variables, vars[2] == vars[0] + 2 * vars[1] + 5
  const std::vector<std::pair<int64_t, int64_t>> bounds(3, {0, 100});
  auto [state, vars] = MakeStateWithVariableDomains(bounds);
  state->AddWeightedSumConstraint({vars[0], vars[1]}, {1, 2}, 5, vars[2]);
  state->CompileConstraints();
  // Initial state: vars[0] and vars[1] min = 0.
  state->RelaxVariableDomain(vars[0]);
  state->PropagateRelax(vars[0]);
  state->RelaxVariableDomain(vars[1]);
  state->PropagateRelax(vars[1]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[0], 0));
  EXPECT_TRUE(state->PropagateTighten(vars[0]));
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[1], 0));
  EXPECT_TRUE(state->PropagateTighten(vars[1]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 5);
  state->Commit();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 5);
  // Try increasing vars[0].min to 1, revert.
  state->RelaxVariableDomain(vars[0]);
  state->PropagateRelax(vars[0]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[0], 1));
  EXPECT_TRUE(state->PropagateTighten(vars[0]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 1);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 6);
  state->Revert();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 5);
  // Try increasing vars[1].min to 2, commit.
  state->RelaxVariableDomain(vars[1]);
  state->PropagateRelax(vars[1]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[1], 2));
  EXPECT_TRUE(state->PropagateTighten(vars[1]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 9);
  state->Commit();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 9);
  // Try increasing vars[0] to 100, revert.
  state->RelaxVariableDomain(vars[0]);
  state->PropagateRelax(vars[0]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[0], 100));
  EXPECT_FALSE(state->PropagateTighten(vars[0]));
  state->Revert();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 0);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 9);
  // Try increasing vars[0] to 10, commit.
  state->RelaxVariableDomain(vars[0]);
  state->PropagateRelax(vars[0]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[0], 10));
  EXPECT_TRUE(state->PropagateTighten(vars[0]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 19);
  state->Commit();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 19);
  // Try increasing vars[2] to 20, commit.
  state->RelaxVariableDomain(vars[2]);
  state->PropagateRelax(vars[2]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[2], 20));
  EXPECT_TRUE(state->PropagateTighten(vars[2]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 20);
  state->Commit();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 20);
  // Try increasing vars[1] to 10, revert.
  state->RelaxVariableDomain(vars[1]);
  state->PropagateRelax(vars[1]);
  EXPECT_TRUE(state->TightenVariableDomainMin(vars[1], 10));
  EXPECT_TRUE(state->PropagateTighten(vars[1]));
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 35);
  state->Revert();
  EXPECT_EQ(state->VariableDomainMin(vars[0]), 10);
  EXPECT_EQ(state->VariableDomainMin(vars[1]), 2);
  EXPECT_EQ(state->VariableDomainMin(vars[2]), 20);
}

// Check that triggering constraints in topological order allows to compute
// DAG-linked values in polynomial time, where a DFS would use exponential time.
TEST(LocalSearchStateTest, BinomialCoefficients) {
  for (int num_rows = 1; num_rows <= 60; ++num_rows) {
    LocalSearchState state;
    // Make a Pascal triangle-full of domains.
    std::vector<std::vector<VariableDomainId>> rows(num_rows);
    for (int r = 0; r < num_rows; ++r) {
      rows[r].resize(r + 1);
      for (VariableDomainId& cell : rows[r]) {
        cell = state.AddVariableDomain(kint64min, kint64max);
      }
    }
    // Add constraints: triangle[r+1][c] = triangle[r][c-1] + triangle[r][c].
    // Also, compute Pascal triangle values. Keep all values for convenience.
    std::vector<std::vector<int64_t>> triangle(num_rows);
    triangle[0] = {1};
    for (int r = 1; r < num_rows; ++r) {
      triangle[r].resize(r + 1, 0);
      for (int c = 0; c <= r; ++c) {
        std::vector<VariableDomainId> vars;
        std::vector<int64_t> coeffs;
        if (c > 0) {
          vars.push_back(rows[r - 1][c - 1]);
          coeffs.push_back(1);
          triangle[r][c] += triangle[r - 1][c - 1];
        }
        if (c < r) {
          vars.push_back(rows[r - 1][c]);
          coeffs.push_back(1);
          triangle[r][c] += triangle[r - 1][c];
        }
        state.AddWeightedSumConstraint(vars, coeffs, 0, rows[r][c]);
      }
    }
    state.CompileConstraints();
    // Change bounds of last row to 1, the corresponding bounds of domains
    // should be Pascal's triangle.
    // Min bounds.
    state.RelaxVariableDomain(rows[0][0]);
    state.PropagateRelax(rows[0][0]);
    EXPECT_TRUE(state.TightenVariableDomainMin(rows[0][0], 1));
    EXPECT_TRUE(state.PropagateTighten(rows[0][0]));
    EXPECT_EQ(state.VariableDomainMax(rows[0][0]), kint64max);
    for (int r = 0; r < num_rows; ++r) {
      for (int c = 0; c <= r; ++c) {
        EXPECT_EQ(triangle[r][c], state.VariableDomainMin(rows[r][c]));
      }
    }
    state.Revert();
    // Max bounds.
    state.RelaxVariableDomain(rows[0][0]);
    state.PropagateRelax(rows[0][0]);
    EXPECT_TRUE(state.TightenVariableDomainMax(rows[0][0], 1));
    EXPECT_TRUE(state.PropagateTighten(rows[0][0]));
    EXPECT_EQ(state.VariableDomainMin(rows[0][0]), kint64min);
    for (int r = 0; r < num_rows; ++r) {
      for (int c = 0; c <= r; ++c) {
        EXPECT_EQ(triangle[r][c], state.VariableDomainMax(rows[r][c]));
      }
    }
    state.Revert();
  }
}

class RunUncheckedLocalSearchTest : public testing::Test,
                                    public ::testing::WithParamInterface<bool> {
 protected:
  ~RunUncheckedLocalSearchTest() override {}
  void SetUp() override {
    const bool maximize = GetParam();
    solver_ = std::make_unique<Solver>("test");
    IntVar* objective = solver_->MakeIntVar(0, 100);
    solver_->MakeIntVarArray(2, 0, 5, &vars_);

    initial_solution_ = solver_->MakeAssignment();
    initial_solution_->AddObjective(objective);
    for (IntVar* var : vars_) {
      initial_solution_->Add(var)->SetValue(maximize ? 0 : 5);
    }

    std::vector<LocalSearchFilter*> filters = {
        solver_->MakeVariableDomainFilter(),
        solver_->MakeSumObjectiveFilter(
            vars_, [](int64_t /*var*/, int64_t value) { return value; },
            maximize ? Solver::GE : Solver::LE)};
    filter_manager_ =
        solver_->RevAlloc(new LocalSearchFilterManager(std::move(filters)));
    ls_operator_ = solver_->MakeOperator(
        vars_, maximize ? Solver::INCREMENT : Solver::DECREMENT);
    monitors_.push_back(solver_->MakeOptimize(maximize, objective, /*step=*/1));
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Assignment* initial_solution_;
  LocalSearchFilterManager* filter_manager_;
  LocalSearchOperator* ls_operator_;
  std::vector<SearchMonitor*> monitors_;
};

TEST_P(RunUncheckedLocalSearchTest, EmptyInitialSolution) {
  initial_solution_->MutableIntVarContainer()->Clear();
  ls_operator_ = solver_->MakeOperator({}, Solver::INCREMENT);
  absl::flat_hash_set<IntVar*> touched;
  Assignment* solution = solver_->RunUncheckedLocalSearch(
      initial_solution_, filter_manager_, ls_operator_, monitors_, nullptr,
      &touched);
  EXPECT_TRUE(solution->Empty());
  EXPECT_TRUE(touched.empty());
}

TEST_P(RunUncheckedLocalSearchTest, OptimizeSolutionWithLogs) {
  monitors_.push_back(solver_->MakeSearchLog(/*branch_period=*/10000));
  absl::flat_hash_set<IntVar*> touched;
  Assignment* solution = solver_->RunUncheckedLocalSearch(
      initial_solution_, filter_manager_, ls_operator_, monitors_, nullptr,
      &touched);
  EXPECT_TRUE(!solution->Empty());
  if (GetParam()) {
    EXPECT_EQ(10, solution->ObjectiveMin());
  } else {
    EXPECT_EQ(0, solution->ObjectiveMax());
  }
  EXPECT_EQ(2, touched.size());
}

TEST_P(RunUncheckedLocalSearchTest, OptimizeSolutionWithSolutionLimit) {
  RegularLimit* limit = solver_->MakeSolutionsLimit(3);
  absl::flat_hash_set<IntVar*> touched;
  Assignment* solution = solver_->RunUncheckedLocalSearch(
      initial_solution_, filter_manager_, ls_operator_, monitors_, limit,
      &touched);
  EXPECT_TRUE(!solution->Empty());
  // Note: the first solution counts as one solution, we then improve twice.
  if (GetParam()) {
    EXPECT_EQ(2, solution->ObjectiveMin());
  } else {
    EXPECT_EQ(8, solution->ObjectiveMax());
  }
  ASSERT_EQ(1, touched.size());
  for (IntVar* var : touched) {
    EXPECT_EQ(vars_[0], var);
  }
}

INSTANTIATE_TEST_SUITE_P(UncheckedMaximizeOrMinimize,
                         RunUncheckedLocalSearchTest, testing::Bool());

TEST(IncrementalLocalSearchTest, ResettingTwoOpt) {
  Solver solver("test");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(7, 0, 7, "var", &vars);
  LocalSearchOperator* ls_operator = solver.MakeOperator(vars, Solver::TWOOPT);
  // Initial solution: 0 - 1 - 2 - 3 - 4 - 5 - 6 - 7
  Assignment* initial_solution = solver.MakeAssignment();
  initial_solution->Add(vars[0])->SetValue(1);
  initial_solution->Add(vars[1])->SetValue(2);
  initial_solution->Add(vars[2])->SetValue(3);
  initial_solution->Add(vars[3])->SetValue(4);
  initial_solution->Add(vars[4])->SetValue(5);
  initial_solution->Add(vars[5])->SetValue(6);
  initial_solution->Add(vars[6])->SetValue(7);
  ls_operator->Start(initial_solution);
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  int count = 0;
  // Count the number of possible moves to position the operator elsewhere than
  // at a starting position.
  while (ls_operator->MakeNextNeighbor(delta, deltadelta)) {
    ++count;
    deltadelta->Clear();
  }
  ls_operator->Reset();
  // Position the operator.
  for (int i = 0; i < count / 2; ++i) {
    deltadelta->Clear();
    ls_operator->MakeNextNeighbor(delta, deltadelta);
  }
  initial_solution->CopyIntersection(delta);
  ls_operator->Start(initial_solution);
  // Perform a first pass
  deltadelta->Clear();
  while (ls_operator->MakeNextNeighbor(delta, deltadelta)) {
    deltadelta->Clear();
  }
  // Reset the operator. This should disable incrementality as Reset() also
  // changes the state of the operator (if it's not disabled a DCHECK will
  // fail).
  ls_operator->Reset();
  deltadelta->Clear();
  while (ls_operator->MakeNextNeighbor(delta, deltadelta)) {
    deltadelta->Clear();
  }
}

TEST(LocalSearchFilterManagerTest, EmptyManagerBehaviour) {
  Solver solver("dummy");
  Assignment delta(&solver);
  LocalSearchFilterManager manager(std::vector<LocalSearchFilter*>{});
  manager.Synchronize(&delta, nullptr);
  EXPECT_EQ(manager.GetSynchronizedObjectiveValue(), 0);
  EXPECT_TRUE(manager.Accept(nullptr, &delta, nullptr, kint64min, kint64max));
  EXPECT_EQ(manager.GetAcceptedObjectiveValue(), 0);
}

class LocalSearchFilterStub : public LocalSearchFilter {
 public:
  LocalSearchFilterStub(bool accept, int64_t synchronized_value,
                        int64_t accepted_value, bool is_incremental)
      : num_synchronize_calls_(0),
        num_accept_calls_(0),
        accept_(accept),
        synchronized_value_(synchronized_value),
        accepted_value_(accepted_value),
        is_incremental_(is_incremental) {}

  bool Accept(const Assignment* /*delta*/, const Assignment* /*deltadelta*/,
              int64_t /*objective_min*/, int64_t /*objective_max*/) override {
    ++num_accept_calls_;
    return accept_;
  }
  void Synchronize(const Assignment* /*solution*/,
                   const Assignment* /*delta*/) override {
    ++num_synchronize_calls_;
  }
  int64_t GetSynchronizedObjectiveValue() const override {
    return synchronized_value_;
  }
  int64_t GetAcceptedObjectiveValue() const override { return accepted_value_; }
  bool IsIncremental() const override { return is_incremental_; }
  int num_synchronize_calls_;
  int num_accept_calls_;

 private:
  bool accept_;
  int64_t synchronized_value_;
  int64_t accepted_value_;
  bool is_incremental_;
};

TEST(LocalSearchFilterManagerTest, ManagerComputesSum) {
  LocalSearchFilterStub filter1(true, 1, 10, false);
  LocalSearchFilterStub filter2(true, 2, 20, false);
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2});
  manager.Synchronize(nullptr, nullptr);
  EXPECT_EQ(manager.GetSynchronizedObjectiveValue(), 3);
  EXPECT_TRUE(manager.Accept(nullptr, nullptr, nullptr, kint64min, kint64max));
  EXPECT_EQ(manager.GetAcceptedObjectiveValue(), 30);
  EXPECT_FALSE(manager.Accept(nullptr, nullptr, nullptr, kint64min, 29));
}

TEST(LocalSearchFilterManagerTest, ManagerAlwaysCallsIncrementalFilters) {
  LocalSearchFilterStub filter1(false, 1, 10, false);
  LocalSearchFilterStub filter2(true, 2, 20, true);
  LocalSearchFilterStub filter3(false, 3, 30, false);
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2, &filter3});
  manager.Synchronize(nullptr, nullptr);
  EXPECT_EQ(manager.GetSynchronizedObjectiveValue(), 6);
  EXPECT_FALSE(manager.Accept(nullptr, nullptr, nullptr, kint64min, kint64max));
  EXPECT_EQ(1, filter2.num_accept_calls_);
}

TEST(LocalSearchFilterManagerTest, ManagerForwardsSynchronizeCalls) {
  LocalSearchFilterStub filter1(false, 1, 10, false);
  LocalSearchFilterStub filter2(true, 2, 20, true);
  LocalSearchFilterStub filter3(false, 3, 30, false);
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2, &filter3});
  manager.Synchronize(nullptr, nullptr);
  EXPECT_EQ(manager.GetSynchronizedObjectiveValue(), 6);
  EXPECT_EQ(filter1.num_synchronize_calls_, 1);
  EXPECT_EQ(filter2.num_synchronize_calls_, 1);
  EXPECT_EQ(filter3.num_synchronize_calls_, 1);
}

TEST(LocalSearchFilterManagerTest, ManagerComparesToObjectiveMax) {
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  LocalSearchFilterStub filter1(true, 1, 10, false);
  LocalSearchFilterStub filter2(true, 2, 20, false);
  LocalSearchFilterStub filter3(true, 3, 30, false);
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2, &filter3});
  manager.Synchronize(delta, nullptr);
  EXPECT_EQ(manager.GetSynchronizedObjectiveValue(), 6);
  EXPECT_TRUE(manager.Accept(nullptr, delta, nullptr, kint64min, 60));
  EXPECT_FALSE(manager.Accept(nullptr, delta, nullptr, kint64min, 59));
}

class MockLocalSearchFilter : public LocalSearchFilter {
 public:
  MOCK_METHOD(void, Relax,
              (const Assignment* delta, const Assignment* deltadelta),
              (override));
  MOCK_METHOD(bool, Accept,
              (const Assignment* delta, const Assignment* deltadelta,
               int64_t objective_min, int64_t objective_max),
              (override));
  MOCK_METHOD(void, Synchronize,
              (const Assignment* assignment, const Assignment* delta),
              (override));
  MOCK_METHOD(void, Commit,
              (const Assignment* assignment, const Assignment* delta),
              (override));
  MOCK_METHOD(void, Revert, (), (override));
};

TEST(LocalSearchFilterManager, ForwardsRelax) {
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2});
  ::testing::InSequence in_sequence;
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Relax(delta, deltadelta));
  manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max);
}

TEST(LocalSearchFilterManager, ForwardsRevert) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max);
  ::testing::InSequence in_sequence;
  EXPECT_CALL(filter2, Revert());
  EXPECT_CALL(filter1, Revert());
  manager.Revert();
}

using ::testing::Return;
// When constructor is vector of filters, event ordering is Relax*Accept*.
TEST(LocalSearchFilterManager, VectorFilterOrderingIsRelaxAccept) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilter*>{&filter1, &filter2});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  ::testing::InSequence in_sequence;
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Relax(delta, deltadelta));
  EXPECT_CALL(filter1, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter2, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max));
}

// When constructor is event ordering, the manager should respect it.
TEST(LocalSearchFilterManager, EventOrderingIsRespected) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilterManager::FilterEvent>{
          {&filter2, kAccept, 1},
          {&filter1, kRelax, 0},
          {&filter1, kAccept, 2}});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  ::testing::InSequence in_sequence;
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter1, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max));
}

class MockLocalSearchFilterIncremental : public MockLocalSearchFilter {
 public:
  MOCK_METHOD(bool, IsIncremental, (), (const, override));
};

// Forced events after an Accept returns false should be called.
TEST(LocalSearchFilterManager, ForcedEventsCalledAfterNoAccept) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  MockLocalSearchFilterIncremental filter3;
  MockLocalSearchFilter filter4;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  ::testing::InSequence in_sequence;
  using ::testing::_;
  EXPECT_CALL(filter3, IsIncremental()).WillOnce(Return(true));
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilterManager::FilterEvent>{
          {&filter1, kRelax, 0},
          {&filter2, kRelax, 2},  // Forced.
          {&filter3, kRelax, 4},  // Forced.
          {&filter4, kRelax, 6},
          {&filter1, kAccept, 1},  // Rejects.
          {&filter2, kAccept, 3},
          {&filter3, kAccept, 5},  // Incremental.
          {&filter4, kAccept, 7}});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  // After filter1's Accept() returns false, filter3 is Incremental,
  // so its events must be called along with in-between Relax events.
  // Filter4 does not have to be called.
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter1, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(false));
  EXPECT_CALL(filter2, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Accept(_, _, _, _)).Times(0);
  EXPECT_CALL(filter3, Relax(delta, deltadelta));
  EXPECT_CALL(filter3, IsIncremental()).WillOnce(Return(true));
  EXPECT_CALL(filter3, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter4, Relax(_, _)).Times(0);
  EXPECT_CALL(filter4, Accept(_, _, _, _)).Times(0);
  EXPECT_FALSE(
      manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max));
}

// Revert should be called in reverse order of Relax events.
TEST(LocalSearchFilterManager, RevertInReverseOrderOfRelaxAfterAccept) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  MockLocalSearchFilter filter3;
  MockLocalSearchFilter filter4;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  ::testing::InSequence in_sequence;
  // Relax order: 1324.
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilterManager::FilterEvent>{
          {&filter1, kRelax, 0},
          {&filter1, kAccept, 1},
          {&filter3, kRelax, 2},
          {&filter2, kRelax, 3},
          {&filter3, kAccept, 4},
          {&filter4, kRelax, 5},
          {&filter4, kAccept, 6}});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter1, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter3, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Relax(delta, deltadelta));
  EXPECT_CALL(filter3, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter4, Relax(delta, deltadelta));
  EXPECT_CALL(filter4, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_TRUE(manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max));
  // Revert order: 1324 reverted = 4231.
  EXPECT_CALL(filter4, Revert());
  EXPECT_CALL(filter2, Revert());
  EXPECT_CALL(filter3, Revert());
  EXPECT_CALL(filter1, Revert());
  manager.Revert();
}

TEST(LocalSearchFilterManager, RevertInReverseOrderOfRelaxAfterNoAccept) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  MockLocalSearchFilter filter3;
  MockLocalSearchFilter filter4;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  ::testing::InSequence in_sequence;
  using ::testing::_;
  // Relax order: 1324.
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilterManager::FilterEvent>{
          {&filter1, kRelax, 0},
          {&filter1, kAccept, 1},
          {&filter3, kRelax, 2},
          {&filter2, kRelax, 3},
          {&filter3, kAccept, 4},
          {&filter4, kRelax, 5},
          {&filter4, kAccept, 6}});
  Solver solver("s");
  Assignment* delta = solver.MakeAssignment();
  Assignment* deltadelta = solver.MakeAssignment();
  EXPECT_CALL(filter1, Relax(delta, deltadelta));
  EXPECT_CALL(filter1, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(true));
  EXPECT_CALL(filter3, Relax(delta, deltadelta));
  EXPECT_CALL(filter2, Relax(delta, deltadelta));
  EXPECT_CALL(filter3, Accept(delta, deltadelta, kint64min, kint64max))
      .WillOnce(Return(false));
  EXPECT_CALL(filter4, Relax(_, _)).Times(0);
  EXPECT_CALL(filter4, Accept(_, _, _, _)).Times(0);
  EXPECT_FALSE(
      manager.Accept(nullptr, delta, deltadelta, kint64min, kint64max));
  // Revert order: 132 reverted = 231.
  EXPECT_CALL(filter4, Revert()).Times(0);
  EXPECT_CALL(filter2, Revert());
  EXPECT_CALL(filter3, Revert());
  EXPECT_CALL(filter1, Revert());
  manager.Revert();
}

// Synchronize and Commit should be called in reverse order of Accept and Relax.
TEST(LocalSearchFilterManager,
     SynchronizeAndCommitInReverseOrderOfAcceptAndRelax) {
  MockLocalSearchFilter filter1;
  MockLocalSearchFilter filter2;
  MockLocalSearchFilter filter3;
  MockLocalSearchFilter filter4;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  ::testing::InSequence in_sequence;
  // Relax order: 1324.
  LocalSearchFilterManager manager(
      std::vector<LocalSearchFilterManager::FilterEvent>{
          {&filter1, kRelax, 0},
          {&filter1, kAccept, 1},
          {&filter3, kRelax, 2},
          {&filter2, kRelax, 3},
          {&filter3, kAccept, 4},
          {&filter4, kRelax, 5},
          {&filter4, kAccept, 6}});
  Solver solver("s");
  Assignment* assignment = solver.MakeAssignment();
  Assignment* delta = solver.MakeAssignment();
  // Relax in forward order.
  EXPECT_CALL(filter1, Relax(assignment, nullptr));
  EXPECT_CALL(filter3, Relax(assignment, nullptr));
  EXPECT_CALL(filter2, Relax(assignment, nullptr));
  EXPECT_CALL(filter4, Relax(assignment, nullptr));
  // Synchronize/Commit in backward order.
  EXPECT_CALL(filter4, Synchronize(assignment, delta));
  EXPECT_CALL(filter4, Commit(assignment, delta));
  EXPECT_CALL(filter3, Synchronize(assignment, delta));
  EXPECT_CALL(filter2, Commit(assignment, delta));
  EXPECT_CALL(filter3, Commit(assignment, delta));
  EXPECT_CALL(filter1, Synchronize(assignment, delta));
  EXPECT_CALL(filter1, Commit(assignment, delta));
  manager.Synchronize(assignment, delta);
}

}  // namespace
}  // namespace operations_research
