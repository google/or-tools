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

#include "ortools/constraint_solver/model_cache.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

ModelCache* BuildModelCache(Solver* solver) {
  return new ModelCache(
      [solver]() { return solver->state() == Solver::OUTSIDE_SEARCH; });
}

namespace {
class FakeConstraint : public Constraint {
 public:
  explicit FakeConstraint(Solver* solver) : Constraint(solver) {}
  ~FakeConstraint() override {}

  void Post() override {}
  void InitialPropagate() override {}
};

Constraint* MakeFakeConstraint(Solver* solver) {
  return solver->RevAlloc(new FakeConstraint(solver));
}

class FakeExpr : public IntExpr {
 public:
  explicit FakeExpr(Solver* s) : IntExpr(s) {}
  ~FakeExpr() override {}
  int64_t Min() const override { return 0; }
  void SetMin(int64_t) override {}
  int64_t Max() const override { return 0; }
  void SetMax(int64_t) override {}
  void WhenRange(Demon*) override {}
  IntVar* Var() override { return nullptr; }
};

IntExpr* MakeFakeExpression(Solver* solver) {
  return solver->RevAlloc(new FakeExpr(solver));
}

class FakeConstraintWithNested : public Constraint {
 public:
  FakeConstraintWithNested(Solver* solver, IntVar* var, int64_t constant,
                           Constraint* constraint, ModelCache* cache,
                           bool* passed)
      : Constraint(solver),
        var_(var),
        constant_(constant),
        constraint_(constraint),
        cache_(cache),
        passed_(passed) {}

  ~FakeConstraintWithNested() override {}

  void Post() override {
    cache_->InsertVarConstantConstraint(constraint_, var_, constant_,
                                        ModelCache::VAR_CONSTANT_EQUALITY);
    EXPECT_FALSE(*passed_);
    *passed_ = true;
  }

  void InitialPropagate() override {}

 private:
  IntVar* const var_;
  const int64_t constant_;
  Constraint* const constraint_;
  ModelCache* const cache_;
  bool* const passed_;
};

Constraint* MakeFakeConstraintWithNested(Solver* solver, IntVar* var,
                                         int64_t constant,
                                         Constraint* constraint,
                                         ModelCache* cache, bool* passed) {
  return solver->RevAlloc(new FakeConstraintWithNested(
      solver, var, constant, constraint, cache, passed));
}

class NullDecisionBuilder : public DecisionBuilder {
 public:
  Decision* Next(Solver*) override { return nullptr; }
};

DecisionBuilder* MakeNullDecisionBuilder(Solver* solver) {
  return solver->RevAlloc(new NullDecisionBuilder);
}

TEST(ModelCacheTest, VoidConstraint) {
  Solver solver("ModelCacheTest::TestVoidConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  Constraint* const fake_constraint = MakeFakeConstraint(&solver);
  EXPECT_FALSE(cache->FindVoidConstraint(ModelCache::VOID_TRUE_CONSTRAINT));
  cache->InsertVoidConstraint(fake_constraint,
                              ModelCache::VOID_TRUE_CONSTRAINT);
  EXPECT_TRUE(fake_constraint ==
              cache->FindVoidConstraint(ModelCache::VOID_TRUE_CONSTRAINT));
  EXPECT_FALSE(cache->FindVoidConstraint(ModelCache::VOID_FALSE_CONSTRAINT));
}

TEST(ModelCacheTest, VarConstantConstraint) {
  Solver solver("ModelCacheTest::TestVarConstantConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  Constraint* const fake_constraint = MakeFakeConstraint(&solver);
  const int64_t kConstant = 123;
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      var, kConstant, ModelCache::VAR_CONSTANT_EQUALITY));
  cache->InsertVarConstantConstraint(fake_constraint, var, kConstant,
                                     ModelCache::VAR_CONSTANT_EQUALITY);
  EXPECT_TRUE(fake_constraint ==
              cache->FindVarConstantConstraint(
                  var, kConstant, ModelCache::VAR_CONSTANT_EQUALITY));
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      var, kConstant + 1, ModelCache::VAR_CONSTANT_EQUALITY));
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      other, kConstant, ModelCache::VAR_CONSTANT_EQUALITY));
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      var, kConstant, ModelCache::VAR_CONSTANT_GREATER_OR_EQUAL));
}

TEST(ModelCacheTest, VerifyNoInsertionDuringRootNode) {
  Solver solver("ModelCacheTest::TestVarConstantConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  Constraint* const fake_constraint = MakeFakeConstraint(&solver);
  const int64_t kConstant = 123;
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  bool passed = false;
  Constraint* const primary = MakeFakeConstraintWithNested(
      &solver, var, kConstant, fake_constraint, cache.get(), &passed);
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      var, kConstant, ModelCache::VAR_CONSTANT_EQUALITY));
  solver.AddConstraint(primary);
  EXPECT_TRUE(solver.Solve(MakeNullDecisionBuilder(&solver)));
  EXPECT_FALSE(cache->FindVarConstantConstraint(
      var, kConstant, ModelCache::VAR_CONSTANT_EQUALITY));
  EXPECT_TRUE(passed);
}

TEST(ModelCacheTest, VarConstantConstantConstraint) {
  Solver solver("ModelCacheTest::TestVarConstantConstantConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  Constraint* const fake_constraint = MakeFakeConstraint(&solver);
  const int64_t kConstant = 123;
  const int64_t kConstant2 = 345;
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  EXPECT_FALSE(cache->FindVarConstantConstantConstraint(
      var, kConstant, kConstant2, ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN));
  cache->InsertVarConstantConstantConstraint(
      fake_constraint, var, kConstant, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN);
  EXPECT_TRUE(fake_constraint ==
              cache->FindVarConstantConstantConstraint(
                  var, kConstant, kConstant2,
                  ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN));
  EXPECT_FALSE(cache->FindVarConstantConstantConstraint(
      var, kConstant + 1, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN));
  EXPECT_FALSE(cache->FindVarConstantConstantConstraint(
      var, kConstant, kConstant2 + 1,
      ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN));
  EXPECT_FALSE(cache->FindVarConstantConstantConstraint(
      other, kConstant, kConstant2, ModelCache::VAR_CONSTANT_CONSTANT_BETWEEN));
}

TEST(ModelCacheTest, ExprExprConstraint) {
  Solver solver("ModelCacheTest::ExprExprConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  Constraint* const fake_constraint = MakeFakeConstraint(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const var2 = solver.MakeIntVar(0, 10, "var2");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  EXPECT_FALSE(
      cache->FindExprExprConstraint(var, var2, ModelCache::EXPR_EXPR_EQUALITY));
  cache->InsertExprExprConstraint(fake_constraint, var, var2,
                                  ModelCache::EXPR_EXPR_EQUALITY);
  EXPECT_TRUE(
      fake_constraint ==
      cache->FindExprExprConstraint(var, var2, ModelCache::EXPR_EXPR_EQUALITY));
  EXPECT_FALSE(cache->FindExprExprConstraint(var, other,
                                             ModelCache::EXPR_EXPR_EQUALITY));
  EXPECT_FALSE(cache->FindExprExprConstraint(other, var2,
                                             ModelCache::EXPR_EXPR_EQUALITY));
  EXPECT_FALSE(cache->FindExprExprConstraint(
      var, var2, ModelCache::EXPR_EXPR_GREATER_OR_EQUAL));
}

TEST(ModelCacheTest, VarExpression) {
  Solver solver("ModelCacheTest::TestVarExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  IntExpr* const fake_expression = MakeFakeExpression(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");

  EXPECT_FALSE(cache->FindExprExpression(var, ModelCache::EXPR_ABS));
  cache->InsertExprExpression(fake_expression, var, ModelCache::EXPR_ABS);
  EXPECT_EQ(fake_expression,
            cache->FindExprExpression(var, ModelCache::EXPR_ABS));
  EXPECT_FALSE(cache->FindExprExpression(other, ModelCache::EXPR_ABS));
  EXPECT_FALSE(cache->FindExprExpression(var, ModelCache::EXPR_OPPOSITE));
}

TEST(ModelCacheTest, VarConstantExpression) {
  Solver solver("ModelCacheTest::TestVarConstantExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  IntExpr* const fake_expression = MakeFakeExpression(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  const int kConstant = 123;

  EXPECT_FALSE(cache->FindExprConstantExpression(
      var, kConstant, ModelCache::EXPR_CONSTANT_PROD));
  cache->InsertExprConstantExpression(fake_expression, var, kConstant,
                                      ModelCache::EXPR_CONSTANT_PROD);
  EXPECT_EQ(fake_expression,
            cache->FindExprConstantExpression(var, kConstant,
                                              ModelCache::EXPR_CONSTANT_PROD));
  EXPECT_FALSE(cache->FindExprConstantExpression(
      other, kConstant, ModelCache::EXPR_CONSTANT_PROD));
  EXPECT_FALSE(cache->FindExprConstantExpression(
      var, kConstant + 1, ModelCache::EXPR_CONSTANT_PROD));

  EXPECT_FALSE(cache->FindExprConstantExpression(
      var, kConstant, ModelCache::EXPR_CONSTANT_MAX));
}

TEST(ModelCacheTest, ExprExprConstantExpression) {
  Solver solver("ModelCacheTest::ExprExprConstantExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  IntExpr* const fake_expression = MakeFakeExpression(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  const int kConstant = 123;

  EXPECT_FALSE(cache->FindExprExprConstantExpression(
      var, other, kConstant, ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL));
  cache->InsertExprExprConstantExpression(
      fake_expression, var, other, kConstant,
      ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL);
  EXPECT_EQ(fake_expression, cache->FindExprExprConstantExpression(
                                 var, other, kConstant,
                                 ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL));
  EXPECT_FALSE(cache->FindExprExprConstantExpression(
      other, var, kConstant, ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL));
  EXPECT_FALSE(cache->FindExprExprConstantExpression(
      var, other, kConstant + 1, ModelCache::EXPR_EXPR_CONSTANT_CONDITIONAL));
}

TEST(ModelCacheTest, VarConstantConstantExpression) {
  Solver solver("ModelCacheTest::TestVarConstantConstantExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  IntExpr* const fake_expression = MakeFakeExpression(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  const int kConstant = 123;
  const int kConstant2 = 234;

  EXPECT_FALSE(cache->FindVarConstantConstantExpression(
      var, kConstant, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS));
  cache->InsertVarConstantConstantExpression(
      fake_expression, var, kConstant, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS);
  EXPECT_EQ(fake_expression,
            cache->FindVarConstantConstantExpression(
                var, kConstant, kConstant2,
                ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS));
  EXPECT_FALSE(cache->FindVarConstantConstantExpression(
      other, kConstant, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS));
  EXPECT_FALSE(cache->FindVarConstantConstantExpression(
      var, kConstant + 1, kConstant2,
      ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS));
  EXPECT_FALSE(cache->FindVarConstantConstantExpression(
      var, kConstant, kConstant2 + 1,
      ModelCache::VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS));
}

TEST(ModelCacheTest, VarConstantArrayExpression) {
  Solver solver("ModelCacheTest::TestVarConstantArrayExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  IntExpr* const fake_expression = MakeFakeExpression(&solver);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  IntVar* const other = solver.MakeIntVar(0, 10, "other");
  const std::vector<int64_t> values = {1, 3, 5, 7};
  const std::vector<int64_t> values2 = {1, 3, 5, 9};

  EXPECT_FALSE(cache->FindVarConstantArrayExpression(
      var, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT));
  cache->InsertVarConstantArrayExpression(
      fake_expression, var, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT);
  EXPECT_EQ(fake_expression,
            cache->FindVarConstantArrayExpression(
                var, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT));
  EXPECT_FALSE(cache->FindVarConstantArrayExpression(
      other, values, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT));
  EXPECT_FALSE(cache->FindVarConstantArrayExpression(
      var, values2, ModelCache::VAR_CONSTANT_ARRAY_ELEMENT));
}

TEST(ModelCacheTest, VarArrayExpression) {
  Solver solver("ModelCacheTest::TestVarArrayExpression");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 100, "vars", &vars);
  std::vector<IntVar*> vars2;
  solver.MakeIntVarArray(10, 0, 100, "vars", &vars2);

  EXPECT_FALSE(cache->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM));
  IntExpr* const fake_sum = MakeFakeExpression(&solver);
  cache->InsertVarArrayExpression(fake_sum, vars, ModelCache::VAR_ARRAY_SUM);
  EXPECT_EQ(fake_sum,
            cache->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_SUM));
  EXPECT_TRUE(nullptr ==
              cache->FindVarArrayExpression(vars2, ModelCache::VAR_ARRAY_SUM));
  EXPECT_FALSE(cache->FindVarArrayExpression(vars, ModelCache::VAR_ARRAY_MIN));
}

TEST(ModelCacheTest, Growth) {
  const int kSize = 100000;
  Solver solver("ModelCacheTest::TestVarConstantConstraint");
  std::unique_ptr<ModelCache> cache(BuildModelCache(&solver));
  std::vector<Constraint*> constraints(kSize);
  IntVar* const var = solver.MakeIntVar(0, 10, "var");
  for (int i = 0; i < kSize; ++i) {
    EXPECT_FALSE(cache->FindVarConstantConstraint(
        var, i, ModelCache::VAR_CONSTANT_EQUALITY));
    Constraint* const fake_constraint = MakeFakeConstraint(&solver);
    cache->InsertVarConstantConstraint(fake_constraint, var, i,
                                       ModelCache::VAR_CONSTANT_EQUALITY);
    constraints[i] = fake_constraint;
  }
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(cache->FindVarConstantConstraint(
                  var, i, ModelCache::VAR_CONSTANT_EQUALITY),
              constraints[i]);
  }
}
}  // namespace
}  // namespace operations_research
