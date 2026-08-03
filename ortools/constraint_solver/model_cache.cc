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

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "ortools/base/stl_util.h"

ABSL_DECLARE_FLAG(int, cache_initial_size);
ABSL_FLAG(bool, cp_disable_cache, false, "Disable caching of model objects");

namespace operations_research {
ModelCache::ModelCache(std::function<bool()> is_outside_search)
    : is_outside_search_(std::move(is_outside_search)),
      void_constraints_(VOID_CONSTRAINT_MAX, nullptr) {
  const int initial_size = absl::GetFlag(FLAGS_cache_initial_size);
  var_constant_constraints_.reserve(VAR_CONSTANT_CONSTRAINT_MAX);
  for (int i = 0; i < VAR_CONSTANT_CONSTRAINT_MAX; ++i) {
    var_constant_constraints_.push_back(
        new VarConstantConstraintCache(initial_size));
  }
  expr_expr_constraints_.reserve(EXPR_EXPR_CONSTRAINT_MAX);
  for (int i = 0; i < EXPR_EXPR_CONSTRAINT_MAX; ++i) {
    expr_expr_constraints_.push_back(new ExprExprConstraintCache(initial_size));
  }
  var_constant_constant_constraints_.reserve(
      VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX);
  for (int i = 0; i < VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX; ++i) {
    var_constant_constant_constraints_.push_back(
        new VarConstantConstantConstraintCache(initial_size));
  }
  expr_expressions_.reserve(EXPR_EXPRESSION_MAX);
  for (int i = 0; i < EXPR_EXPRESSION_MAX; ++i) {
    expr_expressions_.push_back(new ExprIntExprCache(initial_size));
  }
  expr_constant_expressions_.reserve(EXPR_CONSTANT_EXPRESSION_MAX);
  for (int i = 0; i < EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
    expr_constant_expressions_.push_back(
        new ExprConstantIntExprCache(initial_size));
  }
  expr_expr_expressions_.reserve(EXPR_EXPR_EXPRESSION_MAX);
  for (int i = 0; i < EXPR_EXPR_EXPRESSION_MAX; ++i) {
    expr_expr_expressions_.push_back(new ExprExprIntExprCache(initial_size));
  }
  var_constant_constant_expressions_.reserve(
      VAR_CONSTANT_CONSTANT_EXPRESSION_MAX);
  for (int i = 0; i < VAR_CONSTANT_CONSTANT_EXPRESSION_MAX; ++i) {
    var_constant_constant_expressions_.push_back(
        new VarConstantConstantIntExprCache(initial_size));
  }
  var_constant_array_expressions_.reserve(VAR_CONSTANT_ARRAY_EXPRESSION_MAX);
  for (int i = 0; i < VAR_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
    var_constant_array_expressions_.push_back(
        new VarConstantArrayIntExprCache(initial_size));
  }
  var_array_expressions_.reserve(VAR_ARRAY_EXPRESSION_MAX);
  for (int i = 0; i < VAR_ARRAY_EXPRESSION_MAX; ++i) {
    var_array_expressions_.push_back(new VarArrayIntExprCache(initial_size));
  }
  var_array_constant_array_expressions_.reserve(
      VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX);
  for (int i = 0; i < VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
    var_array_constant_array_expressions_.push_back(
        new VarArrayConstantArrayIntExprCache(initial_size));
  }
  var_array_constant_expressions_.reserve(VAR_ARRAY_CONSTANT_EXPRESSION_MAX);
  for (int i = 0; i < VAR_ARRAY_CONSTANT_EXPRESSION_MAX; ++i) {
    var_array_constant_expressions_.push_back(
        new VarArrayConstantIntExprCache(initial_size));
  }
  expr_expr_constant_expressions_.reserve(EXPR_EXPR_CONSTANT_EXPRESSION_MAX);
  for (int i = 0; i < EXPR_EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
    expr_expr_constant_expressions_.push_back(
        new ExprExprConstantIntExprCache(initial_size));
  }
}

ModelCache::~ModelCache() {
  gtl::STLDeleteElements(&var_constant_constraints_);
  gtl::STLDeleteElements(&expr_expr_constraints_);
  gtl::STLDeleteElements(&var_constant_constant_constraints_);
  gtl::STLDeleteElements(&expr_expressions_);
  gtl::STLDeleteElements(&expr_constant_expressions_);
  gtl::STLDeleteElements(&expr_expr_expressions_);
  gtl::STLDeleteElements(&var_constant_constant_expressions_);
  gtl::STLDeleteElements(&var_constant_array_expressions_);
  gtl::STLDeleteElements(&var_array_expressions_);
  gtl::STLDeleteElements(&var_array_constant_array_expressions_);
  gtl::STLDeleteElements(&var_array_constant_expressions_);
  gtl::STLDeleteElements(&expr_expr_constant_expressions_);
}

void ModelCache::Clear() {
  for (auto& cache : var_constant_constraints_) {
    cache->clear();
  }
  for (auto& cache : expr_expr_constraints_) {
    cache->clear();
  }
  for (auto& cache : var_constant_constant_constraints_) {
    cache->clear();
  }
  for (auto& cache : expr_expressions_) {
    cache->clear();
  }
  for (auto& cache : expr_constant_expressions_) {
    cache->clear();
  }
  for (auto& cache : expr_expr_expressions_) {
    cache->clear();
  }
  for (auto& cache : var_constant_constant_expressions_) {
    cache->clear();
  }
  for (auto& cache : var_constant_array_expressions_) {
    cache->clear();
  }
  for (auto& cache : var_array_expressions_) {
    cache->clear();
  }
  for (auto& cache : var_array_constant_array_expressions_) {
    cache->clear();
  }
  for (auto& cache : var_array_constant_expressions_) {
    cache->clear();
  }
  for (auto& cache : expr_expr_constant_expressions_) {
    cache->clear();
  }
}

// Void Constraint.

Constraint* ModelCache::FindVoidConstraint(VoidConstraintType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VOID_CONSTRAINT_MAX);
  return void_constraints_[type];
}

void ModelCache::InsertVoidConstraint(Constraint* ct, VoidConstraintType type) {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VOID_CONSTRAINT_MAX);
  DCHECK(ct != nullptr);
  if (is_outside_search_() && !absl::GetFlag(FLAGS_cp_disable_cache)) {
    void_constraints_[type] = ct;
  }
}

// VarConstantConstraint.

Constraint* ModelCache::FindVarConstantConstraint(
    IntVar* var, int64_t value, VarConstantConstraintType type) const {
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTRAINT_MAX);
  const auto it = var_constant_constraints_[type]->find({var, value});
  return it == var_constant_constraints_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertVarConstantConstraint(Constraint* ct, IntVar* var,
                                             int64_t value,
                                             VarConstantConstraintType type) {
  DCHECK(ct != nullptr);
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTRAINT_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_constant_constraints_[type]->insert({{var, value}, ct});
}

// Var Constant Constant Constraint.

Constraint* ModelCache::FindVarConstantConstantConstraint(
    IntVar* var, int64_t value1, int64_t value2,
    VarConstantConstantConstraintType type) const {
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX);
  const auto it =
      var_constant_constant_constraints_[type]->find({var, value1, value2});
  return it == var_constant_constant_constraints_[type]->end() ? nullptr
                                                               : it->second;
}

void ModelCache::InsertVarConstantConstantConstraint(
    Constraint* ct, IntVar* var, int64_t value1, int64_t value2,
    VarConstantConstantConstraintType type) {
  DCHECK(ct != nullptr);
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_constant_constant_constraints_[type]->insert({{var, value1, value2}, ct});
}

// Var Var Constraint.

Constraint* ModelCache::FindExprExprConstraint(
    IntExpr* expr1, IntExpr* expr2, ExprExprConstraintType type) const {
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_CONSTRAINT_MAX);
  const auto it = expr_expr_constraints_[type]->find({expr1, expr2});
  return it == expr_expr_constraints_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertExprExprConstraint(Constraint* ct, IntExpr* expr1,
                                          IntExpr* expr2,
                                          ExprExprConstraintType type) {
  DCHECK(ct != nullptr);
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_CONSTRAINT_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  expr_expr_constraints_[type]->insert({{expr1, expr2}, ct});
}

// Expr Expression.

IntExpr* ModelCache::FindExprExpression(IntExpr* expr,
                                        ExprExpressionType type) const {
  DCHECK(expr != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPRESSION_MAX);
  const auto it = expr_expressions_[type]->find(expr);
  return it == expr_expressions_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertExprExpression(IntExpr* expression, IntExpr* expr,
                                      ExprExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(expr != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  expr_expressions_[type]->insert({expr, expression});
}

// Expr Constant Expressions.

IntExpr* ModelCache::FindExprConstantExpression(
    IntExpr* expr, int64_t value, ExprConstantExpressionType type) const {
  DCHECK(expr != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_CONSTANT_EXPRESSION_MAX);
  const auto it = expr_constant_expressions_[type]->find({expr, value});
  return it == expr_constant_expressions_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertExprConstantExpression(IntExpr* expression,
                                              IntExpr* expr, int64_t value,
                                              ExprConstantExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(expr != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_CONSTANT_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  expr_constant_expressions_[type]->insert({{expr, value}, expression});
}

// Expr Expr Expression.

IntExpr* ModelCache::FindExprExprExpression(IntExpr* expr1, IntExpr* expr2,
                                            ExprExprExpressionType type) const {
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_EXPRESSION_MAX);
  const auto it = expr_expr_expressions_[type]->find({expr1, expr2});
  return it == expr_expr_expressions_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertExprExprExpression(IntExpr* expression, IntExpr* expr1,
                                          IntExpr* expr2,
                                          ExprExprExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  expr_expr_expressions_[type]->insert({{expr1, expr2}, expression});
}

// Expr Expr Constant Expression.

IntExpr* ModelCache::FindExprExprConstantExpression(
    IntExpr* expr1, IntExpr* expr2, int64_t constant,
    ExprExprConstantExpressionType type) const {
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_CONSTANT_EXPRESSION_MAX);
  const auto it =
      expr_expr_constant_expressions_[type]->find({expr1, expr2, constant});
  return it == expr_expr_constant_expressions_[type]->end() ? nullptr
                                                            : it->second;
}

void ModelCache::InsertExprExprConstantExpression(
    IntExpr* expression, IntExpr* expr1, IntExpr* expr2, int64_t constant,
    ExprExprConstantExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(expr1 != nullptr);
  DCHECK(expr2 != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, EXPR_EXPR_CONSTANT_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  expr_expr_constant_expressions_[type]->insert(
      {{expr1, expr2, constant}, expression});
}

// Var Constant Constant Expression.

IntExpr* ModelCache::FindVarConstantConstantExpression(
    IntVar* var, int64_t value1, int64_t value2,
    VarConstantConstantExpressionType type) const {
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTANT_EXPRESSION_MAX);
  const auto it =
      var_constant_constant_expressions_[type]->find({var, value1, value2});
  return it == var_constant_constant_expressions_[type]->end() ? nullptr
                                                               : it->second;
}

void ModelCache::InsertVarConstantConstantExpression(
    IntExpr* expression, IntVar* var, int64_t value1, int64_t value2,
    VarConstantConstantExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_CONSTANT_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_constant_constant_expressions_[type]->insert(
      {{var, value1, value2}, expression});
}

// Var Constant Array Expression.

IntExpr* ModelCache::FindVarConstantArrayExpression(
    IntVar* var, const std::vector<int64_t>& values,
    VarConstantArrayExpressionType type) const {
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_ARRAY_EXPRESSION_MAX);
  const auto it = var_constant_array_expressions_[type]->find({var, values});
  return it == var_constant_array_expressions_[type]->end() ? nullptr
                                                            : it->second;
}

void ModelCache::InsertVarConstantArrayExpression(
    IntExpr* expression, IntVar* var, const std::vector<int64_t>& values,
    VarConstantArrayExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK(var != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_CONSTANT_ARRAY_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_constant_array_expressions_[type]->insert({{var, values}, expression});
}

// Var Array Expression.

IntExpr* ModelCache::FindVarArrayExpression(const std::vector<IntVar*>& vars,
                                            VarArrayExpressionType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_EXPRESSION_MAX);
  const auto it = var_array_expressions_[type]->find(vars);
  return it == var_array_expressions_[type]->end() ? nullptr : it->second;
}

void ModelCache::InsertVarArrayExpression(IntExpr* expression,
                                          const std::vector<IntVar*>& vars,
                                          VarArrayExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_array_expressions_[type]->insert({vars, expression});
}

// Var Array Constant Array Expressions.

IntExpr* ModelCache::FindVarArrayConstantArrayExpression(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& values,
    VarArrayConstantArrayExpressionType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX);
  const auto it =
      var_array_constant_array_expressions_[type]->find({vars, values});
  return it == var_array_constant_array_expressions_[type]->end() ? nullptr
                                                                  : it->second;
}

void ModelCache::InsertVarArrayConstantArrayExpression(
    IntExpr* expression, const std::vector<IntVar*>& vars,
    const std::vector<int64_t>& values,
    VarArrayConstantArrayExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_array_constant_array_expressions_[type]->insert(
      {{vars, values}, expression});
}

// Var Array Constant Expressions.

IntExpr* ModelCache::FindVarArrayConstantExpression(
    const std::vector<IntVar*>& vars, int64_t value,
    VarArrayConstantExpressionType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_CONSTANT_EXPRESSION_MAX);
  const auto it = var_array_constant_expressions_[type]->find({vars, value});
  return it == var_array_constant_expressions_[type]->end() ? nullptr
                                                            : it->second;
}

void ModelCache::InsertVarArrayConstantExpression(
    IntExpr* expression, const std::vector<IntVar*>& vars, int64_t value,
    VarArrayConstantExpressionType type) {
  DCHECK(expression != nullptr);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, VAR_ARRAY_CONSTANT_EXPRESSION_MAX);
  if (!is_outside_search_() || absl::GetFlag(FLAGS_cp_disable_cache)) return;
  var_array_constant_expressions_[type]->insert({{vars, value}, expression});
}

}  // namespace operations_research
