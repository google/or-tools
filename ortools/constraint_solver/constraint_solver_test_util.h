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

// This file provides utilities for writing unit tests for the constraint solver
// and local search library.

#ifndef ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_TEST_UTIL_H_
#define ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_TEST_UTIL_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
// These macros allow writing unit tests with expectations on the
// state during search, when propagation is active.
// They are necessary for testing the implementation of constraint
// propagation.

// Creates a TEST() macro that executes the code in the test case during
// the search phase, when propagation is active. The implementation executes
// the user code when the search needs a new decision from the DecisionBuilder.
// Any call to this macro should finish with return nullptr.
//
// Example:
// Solver* solver = new Solver;
// TEST_IN_PROPAGATION(TestMakeGreater, TestPropagation, solver, true) {
//   IntVar* x = solver->MakeIntVar(0, 100, "var");
//   solver->AddConstraint(solver->MakeGreater(x, 10));
//   EXPECT_EQ(x->Min(), 11);
//   return nullptr;
// }
#define TEST_IN_PROPAGATION(class_name, test_name, solver, expected_result) \
  class class_name##test_name : public DecisionBuilder {                    \
   public:                                                                  \
    virtual Decision* Next(Solver* const solver);                           \
  };                                                                        \
                                                                            \
  TEST(class_name, test_name) {                                             \
    Solver solver("test");                                                  \
    DecisionBuilder* const db = solver.RevAlloc(new class_name##test_name); \
    EXPECT_EQ(expected_result, solver.Solve(db));                           \
  }                                                                         \
                                                                            \
  Decision* class_name##test_name::Next(Solver* const solver)

// Creates a TEST_F() macro that executes the code in the test case during
// the search phase, when propagation is active. The base test class can
// be used for set-up (create variables, add constraints) but its members
// can not be used in the user code. Use the TEST_IN_PROPAGATION1-3 for
// this.
// The implementation executes the user code when the search needs a new
// decision from the DecisionBuilder.
// Any call to this macro should finish with return nullptr.
//
// Example:
// class SimpleTest : public testing::Test {
//  protected:
//   virtual void SetUp() {
//     solver_.reset(new Solver("SampleSolver"));
//     IntVar* x = solver->MakeIntVar(0, 100, "var");
//     solver->AddConstraint(solver->MakeGreater(x, 10));
//     IntVar* y = solver->MakeIntVar(0, 100, "second ");
//     solver->AddConstraint(solver->MakeEquality(x, y));
//   }
//   std::unique_ptr<Solver> solver_;
// };
//
// TEST_IN_PROPAGATION0(TestEquality, TestWithBoundedVariable, solver, true) {
//   EXPECT_EQ(y->Min(), 11);
//   return nullptr;
// }
#define TEST_IN_PROPAGATION0(class_name, test_name, solver, expected_result)  \
  class class_name##test_name : public DecisionBuilder {                      \
   public:                                                                    \
    virtual Decision* Next(Solver* const solver);                             \
  };                                                                          \
                                                                              \
  TEST_F(class_name, test_name) {                                             \
    DecisionBuilder* const db = solver_->RevAlloc(new class_name##test_name); \
    EXPECT_EQ(expected_result, solver_->Solve(db));                           \
  }                                                                           \
                                                                              \
  Decision* class_name##test_name::Next(Solver* const solver)

// Creates a TEST_F() macro that executes the code in the test case during
// the search phase, when propagation is active. The parameter param of type
// type will be available to be used in the user code.
// The implementation executes the user code when the search needs a new
// decision from the DecisionBuilder.
// Any call to this macro should finish with return nullptr.
//
// Example:
// class SimpleTest : public testing::Test {
//  protected:
//   virtual void SetUp() {
//     solver_.reset(new Solver("SampleSolver"));
//     x = solver->MakeIntVar(0, 100, "var");
//     solver->AddConstraint(solver->MakeGreater(x, 10));
//   }
//   IntVar* x;
//   std::unique_ptr<Solver> solver_;
// };
//
// TEST_IN_PROPAGATION1(TestEquality, TestWithBoundedVariable, solver, true,
//                      IntVar*, x) {
//   IntVar* y = solver->MakeIntVar(0, 100, "second ");
//   solver->AddConstraint(solver->MakeEquality(x, y));
//   EXPECT_EQ(y->Min(), x->Min());
//   return nullptr;
// }
#define TEST_IN_PROPAGATION1(class_name, test_name, solver, expected_result, \
                             type1, param1)                                  \
  class class_name##test_name : public DecisionBuilder {                     \
   public:                                                                   \
    explicit class_name##test_name(type1 param1##param1)                     \
        : param1(param1##param1) {}                                          \
    virtual Decision* Next(Solver* const solver);                            \
                                                                             \
   private:                                                                  \
    type1 param1;                                                            \
  };                                                                         \
                                                                             \
  TEST_F(class_name, test_name) {                                            \
    DecisionBuilder* const db =                                              \
        solver_->RevAlloc(new class_name##test_name(param1));                \
    EXPECT_EQ(expected_result, solver_->Solve(db));                          \
  }                                                                          \
                                                                             \
  Decision* class_name##test_name::Next(Solver* const solver)

// Just like above, but two parameters param1 and param2 of types type1 and
// type2 can be passed to be used in the user code.
#define TEST_IN_PROPAGATION2(class_name, test_name, solver, expected_result, \
                             type1, param1, type2, param2)                   \
  class class_name##test_name : public DecisionBuilder {                     \
   public:                                                                   \
    class_name##test_name(type1 param1##param1, type2 param2##param2)        \
        : param1(param1##param1), param2(param2##param2) {}                  \
    virtual Decision* Next(Solver* const solver);                            \
                                                                             \
   private:                                                                  \
    type1 param1;                                                            \
    type2 param2;                                                            \
  };                                                                         \
                                                                             \
  TEST_F(class_name, test_name) {                                            \
    DecisionBuilder* const db =                                              \
        solver_->RevAlloc(new class_name##test_name(param1, param2));        \
    EXPECT_EQ(expected_result, solver_->Solve(db));                          \
  }                                                                          \
                                                                             \
  Decision* class_name##test_name::Next(Solver* const solver)

// Just like above, but three parameters param1, param2, param3 of types type1,
// type2, type3 can be passed to be used in the user code.
#define TEST_IN_PROPAGATION3(class_name, test_name, solver, expected_result,  \
                             type1, param1, type2, param2, type3, param3)     \
  class class_name##test_name : public DecisionBuilder {                      \
   public:                                                                    \
    class_name##test_name(type1 param1##param1, type2 param2##param2,         \
                          type3 param3##param3)                               \
        : param1(param1##param1),                                             \
          param2(param2##param2),                                             \
          param3(param3##param3) {}                                           \
    virtual Decision* Next(Solver* const solver);                             \
                                                                              \
   private:                                                                   \
    type1 param1;                                                             \
    type2 param2;                                                             \
    type3 param3;                                                             \
  };                                                                          \
                                                                              \
  TEST_F(class_name, test_name) {                                             \
    DecisionBuilder* const db =                                               \
        solver_->RevAlloc(new class_name##test_name(param1, param2, param3)); \
    EXPECT_EQ(expected_result, solver_->Solve(db));                           \
  }                                                                           \
                                                                              \
  Decision* class_name##test_name::Next(Solver* const solver)

// Creates a TEST_F() macro that executes the code in the test case during
// the search phase, when propagation is active. The parameter param
// can works like an enum that takes values between param_min and param_max
// and can be used to parametrize user code.
//
// Example:
// class SimpleTest : public testing::Test {
//  protected:
//   virtual void SetUp() {
//     solver_.reset(new Solver("SampleSolver"));
//     IntVar* x = solver->MakeIntVar(0, 100, "var");
//     solver->AddConstraint(solver->MakeGreater(x, 10));
//     IntVar* y = solver->MakeIntVar(0, 100, "second ");
//     solver->AddConstraint(solver->MakeEquality(x, y));
//   }
//   std::unique_ptr<Solver> solver_;
// };
//
// TEST_IN_PROPAGATION_LOOP0(TestMin, TestWithAllBounds, solver,
//                           false, variable_min, 0, kint64max) {
//   IntVar* x = solver->MakeIntVar(0, 100, "var");
//   solver->AddConstraint(solver->MakeGreater(x, variable_min));
//   EXPECT_EQ(x->Min(), variable_bound);
//   return nullptr;
// }
#define TEST_IN_PROPAGATION_LOOP0(class_name, test_name, solver,     \
                                  expected_result, param, param_min, \
                                  param_max)                         \
  class class_name##test_name : public DecisionBuilder {             \
   public:                                                           \
    explicit class_name##test_name(int param1) : param(param1) {}    \
    virtual Decision* Next(Solver* const solver);                    \
                                                                     \
   private:                                                          \
    int param;                                                       \
  };                                                                 \
                                                                     \
  TEST_F(class_name, test_name) {                                    \
    for (int i = param_min; i <= param_max; ++i) {                   \
      DecisionBuilder* const db =                                    \
          solver_->RevAlloc(new class_name##test_name(i));           \
      EXPECT_EQ(expected_result, solver_->Solve(db));                \
    }                                                                \
  }                                                                  \
                                                                     \
  Decision* class_name##test_name::Next(Solver* const solver)

// These methods provide shorthands for setting expectations on the domains
// of integer variables.
#define EXPECT_EXPR_EQ_RANGE(var, vmin, vmax) \
  EXPECT_EQ(vmin, var->Min());                \
  EXPECT_EQ(vmax, var->Max())

#define EXPECT_BOUND_TO(var, value) \
  EXPECT_TRUE(var->Bound());        \
  EXPECT_EQ(value, var->Min())

#define EXPECT_EQ_RANGE(var, vmin, vmax)     \
  EXPECT_EQ(vmax - (vmin) + 1, var->Size()); \
  EXPECT_EQ(vmin, var->Min());               \
  EXPECT_EQ(vmax, var->Max())

#define EXPECT_EQ_DOMAIN(var, value1, ...)                                     \
  {                                                                            \
    const std::vector<decltype(value1)> tail = {__VA_ARGS__};                  \
    EXPECT_EQ(tail.size() + 1, var->Size());                                   \
    EXPECT_TRUE(var->Contains(value1)) << " #0: " << value1;                   \
    for (int i = 0; i < tail.size(); ++i) {                                    \
      EXPECT_TRUE(var->Contains(tail[i])) << " #" << i + 1 << ": " << tail[i]; \
    }                                                                          \
  }

namespace operations_research {

// Class for testing how a Local Search Operators generate deltas from an
// initial solution.
// The test case needs to:
//  - define the BuildOperator method,
//  - initialize the first solution via SetFirstSolution
//  - add the expectations via AddExpectedDeltas,
//  - call BaseSolveAndTest to test the expectations. The ordered_match
//    parameter specifies if the expectations are tested in the order
//    they were added or if the order does not matter.
//
// Example:
// class OperatorTest : public BaseOperatorTest {
//  protected:
//   void SolveAndTest(Solver::LocalSearchOperators operator_type) {
//     operator_type_ = operator_type;
//     BaseSolveAndTest(true);
//   }
//   virtual LocalSearchOperator* BuildOperator(const vector<IntVar*>& vars,
//                                              Solver* solver) const {
//     return solver->MakeOperator(vars, operator_type_);
//   }
//   Solver::LocalSearchOperators operator_type_;
// };
// TEST_F(OperatorTest, TwoOptTest1) {
//   const int first_solution[] = {1, 2, 3};
//   SetFirstSolution(first_solution, ABSL_ARRAYSIZE(first_solution));
//   AddExpectedDelta(nullptr, 0);
//   const int delta[] = {2, 3, 1};
//   AddExpectedDelta(delta, ABSL_ARRAYSIZE(delta));
//   AddExpectedDelta(nullptr, 0);
//   SolveAndTest(Solver::TWOOPT);
// }
class BaseOperatorTest : public testing::Test {
 protected:
  virtual LocalSearchOperator* BuildOperator(
      const std::vector<IntVar*>& /*vars*/, Solver* /*solver*/) const {
    return nullptr;
  }
  virtual LocalSearchOperator* BuildOperatorWithSecondaryVars(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& /*secondary_vars*/, Solver* solver) const {
    return BuildOperator(vars, solver);
  }
  virtual void MakeVars(Solver* solver, std::vector<IntVar*>* primary_vars,
                        std::vector<IntVar*>* secondary_vars) const;
  void BaseSolveAndTest(
      bool match_all,
      const std::function<bool()>& on_completion = nullptr) const;
  void SetFirstSolution(const std::vector<int>& first_solution_primary_vars,
                        const std::vector<int>& first_solution_secondary_vars);
  void SetFirstSolution(const std::vector<int>& first_solution) {
    SetFirstSolution(first_solution, {});
  }
  // Note: pass empty vectors to allow all paths.
  void SetAllowedSecondaryValues(
      const std::vector<std::vector<int64_t>>& allowed_secondary_values);
  void SetFirstPaths(int num_nodes, absl::Span<const std::vector<int>> paths,
                     bool fill_secondary_vars = false);
  void AddExpectedDelta(absl::Span<const int> expected_delta,
                        absl::Span<const int> expected_delta_delta = {});
  void AddExpectedSolution(absl::Span<const int> expected_solution);
  void AddExpectedPaths(absl::Span<const std::vector<int>> expected_paths);
  void ClearDeltas() {
    expected_deltas_.clear();
    expected_delta_deltas_.clear();
  }

  // To help you build unit tests, this LOG(INFO) the sequence of actual
  // neihgbors visited, without checking anything.
  // If log_delta_only, logs only the variables in the "delta" assignments
  // instead of all variables.
  void SolveAndLogNeighbors(bool log_delta_only) const;

  std::vector<int> first_solution_primary_vars_;
  std::vector<int> first_solution_secondary_vars_;
  std::vector<std::vector<int64_t>> allowed_secondary_values_;
  std::vector<std::vector<int>> expected_deltas_;
  std::vector<std::vector<int>> expected_delta_deltas_;

 private:
  void OrderedMatch(const std::vector<IntVar*>& vars, Solver* solver,
                    LocalSearchOperator* localsearch_operator) const;
  void UnorderedMatch(const std::vector<IntVar*>& vars, Solver* solver,
                      LocalSearchOperator* localsearch_operator) const;
  void FillSolutionWithPaths(std::vector<int>* solution,
                             std::vector<int>* secondary_solution,
                             int num_nodes,
                             absl::Span<const std::vector<int>> paths);
};

// Class for testing if a search discovers given solutions in a given order.
// The test case needs to add expected solutions via the AddExpectedSolution
// method and then add the search monitor to the search monitors.
//
// Example:
// class ObjectiveTest : public testing::Test {
//  protected:
//   virtual void SetUp() {
//     solver_.reset(new operations_research::Solver("test"));
//     var_ = solver_->MakeIntVar(0, 10, "Test Var");
//     OptimizeVar* const objective_monitor
//          = solver_->MakeMinimize(var, true);
//     test_search_monitor_ = solver_->RevAlloc(
//         new ExpectedSolutionsSearchMonitor(solver_.get()));
//     monitors_.push_back(objective_monitor);
//     monitors_.push_back(test_search_monitor_);
//   }
//   std::unique_ptr<Solver> solver_;
//   ExpectedSolutionsSearchMonitor* test_search_monitor_;
//   IntVar* objective_;
//   vector<SearchMonitor*> monitors_;
// };
//
// TEST_F(ObjectiveTest, SingleVariableTest) {
//   for (int i = 0; i <= 10; ++i) {
//     Assignment* solution = solver_->MakeAssignment();
//     solution->Add(var);
//     solution->SetValue(var, i);
//     test_search_monitor_->AddExpectedSolution(solution);
//   }
//   DecisionBuilder* db = solver_->MakePhase(var,
//                                            Solver::CHOOSE_FIRST_UNBOUND,
//                                            Solver::ASSIGN_MAX_VALUE);
//   solver_->Solve(db, monitors_);
//   EXPECT_TRUE(test_search_monitor_->AllExpectedSolutionsReceived());
// }
class ExpectedSolutionsSearchMonitor : public SearchMonitor {
 public:
  ExpectedSolutionsSearchMonitor(
      Solver* s, const std::vector<const Assignment*>& expected_solutions);
  explicit ExpectedSolutionsSearchMonitor(Solver* s);

  // Add an expected solution.
  void AddExpectedSolution(const Assignment* solution);
  bool AtSolution() override;
  // Returns whether all the expected solutions have returned, and if in the
  // order specified by either the constructor and/or the calls to
  // AddExpectedSolutions.
  bool AllExpectedSolutionsReceived();

 private:
  bool IsExpectedAssignment(const Assignment* assignment);

  std::vector<const Assignment*> expected_solutions_;
  int next_solution_index_;
};

}  // namespace operations_research
#endif  // ORTOOLS_CONSTRAINT_SOLVER_CONSTRAINT_SOLVER_TEST_UTIL_H_
