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

#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
namespace {

// One very dumb constraint that does not propagate but fails such that it
// doesn't reduce the domain size but the variable has values whose impacts
// are not initted.
class DummyDiffCst : public Constraint {
 public:
  DummyDiffCst(Solver* s, IntVar* var, int64_t value)
      : Constraint(s), var_(var), value_(value) {}
  ~DummyDiffCst() override {}
  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &DummyDiffCst::ValuePropagate, "BoundPropagate");
    var_->WhenBound(demon);
  }
  void InitialPropagate() override {}
  void ValuePropagate() {
    int64_t var_value = var_->Min();
    if (var_value == value_) {
      solver()->Fail();
    }
  }
  void Accept(ModelVisitor* visitor) const override {
    const std::string kId = "Dummy";
    visitor->BeginVisitConstraint(kId, this);
    visitor->VisitIntegerExpressionArgument("var", var_);
    visitor->VisitIntegerArgument("value", value_);
    visitor->EndVisitConstraint(kId, this);
  }

 private:
  IntVar* var_;
  int64_t value_;
};

class SimpleImpactDecisionBuilderTest : public ::testing::Test {
 protected:
  SimpleImpactDecisionBuilderTest()
      : solver_("SimpleImpactDecisionBuilderTest"),
        decision_builder_(nullptr) {}
  void SetUp() override {
    solver_.MakeIntVarArray(kSize, 1, kSize, &vars_);
    decision_builder_ = solver_.MakeDefaultPhase(vars_);
  }

  static const int kSize;
  Solver solver_;
  std::vector<IntVar*> vars_;
  DecisionBuilder* decision_builder_;
};

const int SimpleImpactDecisionBuilderTest::kSize = 5;

// Test for a bug where the impact values were not reset between solve.
TEST_F(SimpleImpactDecisionBuilderTest, ImpactResetBug) {
  solver_.Solve(decision_builder_);
  Constraint* ct = solver_.RevAlloc(new DummyDiffCst(&solver_, vars_[0], 1));
  solver_.AddConstraint(ct);
  solver_.Solve(decision_builder_);
}

TEST_F(SimpleImpactDecisionBuilderTest, AllSolutions) {
  solver_.AddConstraint(solver_.MakeAllDifferent(vars_, false));
  operations_research::DefaultPhaseParameters parameters;
  parameters.heuristic_period = 1000;
  decision_builder_ = solver_.MakeDefaultPhase(vars_, parameters);
  int solution_count = 0;
  solver_.NewSearch(decision_builder_);
  while (solver_.NextSolution()) {
    solution_count++;
  }
  solver_.EndSearch();
  int factorial = 1;
  for (int index = 2; index <= kSize; ++index) {
    factorial *= index;
  }
  EXPECT_EQ(factorial, solution_count);
}

TEST(ImpactDecisionBuilderTest, AllSolutionsWithHeuristics) {
  const int kSize = 5;
  Solver solver("ImpactDecisionBuilderTest::TestAllSolutionsWithHeuristics");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kSize, 1, kSize, &vars);
  solver.AddConstraint(solver.MakeAllDifferent(vars, false));
  operations_research::DefaultPhaseParameters parameters;
  parameters.heuristic_period = 100;
  // We should have two heuristic calls during search.
  const int kHeuristicSolutions = 2;
  DecisionBuilder* decision_builder = solver.MakeDefaultPhase(vars, parameters);
  int solution_count = 0;
  solver.NewSearch(decision_builder);
  while (solver.NextSolution()) {
    solution_count++;
  }
  solver.EndSearch();
  int factorial = 1;
  for (int index = 2; index <= kSize; ++index) {
    factorial *= index;
  }
  // The run will explore all solutions (factorial).  In the same
  // period, it will run heuristics twice, thus generating two more
  // 'redundant' solutions.
  EXPECT_EQ(factorial + kHeuristicSolutions, solution_count);
}

TEST(ImpactDecisionBuilderTest, EasyProblem) {
  // This is magic_square of size 3. There are some values for some
  // variables which instantiation solves the problem. They were removed
  // by a previous version of impact search.
  const int kGridSize = 3;
  Solver solver("ImpactDecisionBuilderTest::TestEasyProblem");
  const int total_size = kGridSize * kGridSize;
  const int sum = kGridSize * (total_size + 1) / 2;
  // create the variables
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(total_size, 1, total_size, "", &vars);
  solver.AddConstraint(solver.MakeAllDifferent(vars, true));

  // create the constraints
  std::vector<IntVar*> diag1(kGridSize);
  std::vector<IntVar*> diag2(kGridSize);
  for (int n = 0; n < kGridSize; ++n) {
    std::vector<IntVar*> sub_set(kGridSize);

    for (int m = 0; m < kGridSize; ++m) {  // extract row indices
      sub_set[m] = vars[m + n * kGridSize];
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));

    for (int m = 0; m < kGridSize; ++m) {
      sub_set[m] = vars[m * kGridSize + n];  // extract column indices
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));
    diag1[n] = vars[n + n * kGridSize];  // extract first diagonal indices
    diag2[n] = vars[(kGridSize - 1 - n) + n * kGridSize];  // second diagonal
  }
  solver.AddConstraint(solver.MakeSumEquality(diag1, sum));
  solver.AddConstraint(solver.MakeSumEquality(diag2, sum));

  // To break a simple symmetry: the upper right corner
  // must be less than the lower left corner
  solver.AddConstraint(
      solver.MakeLess(vars[kGridSize - 1], vars[(kGridSize - 1) * kGridSize]));

  DecisionBuilder* decision_builder = solver.MakeDefaultPhase(vars);
  EXPECT_TRUE(solver.Solve(decision_builder));
}

TEST(ImpactDecisionBuilderTest, RemoveAllValues) {
  // Here is an infeasible problem for which binding any variable makes the
  // propagation to discover the infeasibility. This used to cause a memory
  // leak.
  const int kNumVars = 3;
  Solver solver("ImpactDecisionBuilderTest::RemoveAllValues");
  // create the variables
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(kNumVars, 0, 1, "vars_", &vars);
  // Pairwise different, without using the all-different constraint
  solver.AddConstraint(solver.MakeNonEquality(vars[0], vars[1]));
  solver.AddConstraint(solver.MakeNonEquality(vars[0], vars[2]));
  solver.AddConstraint(solver.MakeNonEquality(vars[1], vars[2]));

  // Use the default search
  DecisionBuilder* decision_builder = solver.MakeDefaultPhase(vars);
  EXPECT_FALSE(solver.Solve(decision_builder));
}

}  // namespace
}  // namespace operations_research
