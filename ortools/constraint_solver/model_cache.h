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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_MODEL_CACHE_H_
#define ORTOOLS_CONSTRAINT_SOLVER_MODEL_CACHE_H_

#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

#include "ortools/base/safe_hash_map.h"

namespace operations_research {

class Constraint;
class IntExpr;
class IntVar;

/// Implements a complete cache for model elements: expressions and
/// constraints.  Caching is based on the signatures of the elements, as
/// well as their types.  This class is used internally to avoid creating
/// duplicate objects.
class ModelCache {
 public:
  template <typename K, typename V>
  using cache_map = safe_hash_map<K, V>;

  // Cache templates.
  typedef cache_map<IntExpr*, IntExpr*> ExprIntExprCache;
  typedef cache_map<std::vector<IntVar*>, IntExpr*> VarArrayIntExprCache;

  typedef cache_map<std::pair<IntVar*, int64_t>, Constraint*>
      VarConstantConstraintCache;
  typedef cache_map<std::pair<IntExpr*, IntExpr*>, Constraint*>
      ExprExprConstraintCache;
  typedef cache_map<std::pair<IntVar*, int64_t>, IntExpr*>
      VarConstantIntExprCache;
  typedef cache_map<std::pair<IntExpr*, int64_t>, IntExpr*>
      ExprConstantIntExprCache;
  typedef cache_map<std::pair<IntExpr*, IntExpr*>, IntExpr*>
      ExprExprIntExprCache;
  typedef cache_map<std::pair<IntVar*, std::vector<int64_t>>, IntExpr*>
      VarConstantArrayIntExprCache;
  typedef cache_map<std::pair<std::vector<IntVar*>, std::vector<int64_t>>,
                    IntExpr*>
      VarArrayConstantArrayIntExprCache;
  typedef cache_map<std::pair<std::vector<IntVar*>, int64_t>, IntExpr*>
      VarArrayConstantIntExprCache;

  typedef cache_map<std::tuple<IntVar*, int64_t, int64_t>, IntExpr*>
      VarConstantConstantIntExprCache;
  typedef cache_map<std::tuple<IntVar*, int64_t, int64_t>, Constraint*>
      VarConstantConstantConstraintCache;
  typedef cache_map<std::tuple<IntExpr*, IntExpr*, int64_t>, IntExpr*>
      ExprExprConstantIntExprCache;

  enum VoidConstraintType {
    VOID_FALSE_CONSTRAINT,
    VOID_TRUE_CONSTRAINT,
    VOID_CONSTRAINT_MAX,
  };

  enum VarConstantConstraintType {
    VAR_CONSTANT_EQUALITY,
    VAR_CONSTANT_GREATER_OR_EQUAL,
    VAR_CONSTANT_LESS_OR_EQUAL,
    VAR_CONSTANT_NON_EQUALITY,
    VAR_CONSTANT_CONSTRAINT_MAX,
  };

  enum VarConstantConstantConstraintType {
    VAR_CONSTANT_CONSTANT_BETWEEN,
    VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX,
  };

  enum ExprExprConstraintType {
    EXPR_EXPR_EQUALITY,
    EXPR_EXPR_GREATER,
    EXPR_EXPR_GREATER_OR_EQUAL,
    EXPR_EXPR_LESS,
    EXPR_EXPR_LESS_OR_EQUAL,
    EXPR_EXPR_NON_EQUALITY,
    EXPR_EXPR_CONSTRAINT_MAX,
  };

  enum ExprExpressionType {
    EXPR_OPPOSITE,
    EXPR_ABS,
    EXPR_SQUARE,
    EXPR_EXPRESSION_MAX,
  };

  enum ExprExprExpressionType {
    EXPR_EXPR_DIFFERENCE,
    EXPR_EXPR_PROD,
    EXPR_EXPR_DIV,
    EXPR_EXPR_MAX,
    EXPR_EXPR_MIN,
    EXPR_EXPR_SUM,
    EXPR_EXPR_IS_LESS,
    EXPR_EXPR_IS_LESS_OR_EQUAL,
    EXPR_EXPR_IS_EQUAL,
    EXPR_EXPR_IS_NOT_EQUAL,
    EXPR_EXPR_EXPRESSION_MAX,
  };

  enum ExprExprConstantExpressionType {
    EXPR_EXPR_CONSTANT_CONDITIONAL,
    EXPR_EXPR_CONSTANT_EXPRESSION_MAX,
  };

  enum ExprConstantExpressionType {
    EXPR_CONSTANT_DIFFERENCE,
    EXPR_CONSTANT_DIVIDE,
    EXPR_CONSTANT_PROD,
    EXPR_CONSTANT_MAX,
    EXPR_CONSTANT_MIN,
    EXPR_CONSTANT_SUM,
    EXPR_CONSTANT_IS_EQUAL,
    EXPR_CONSTANT_IS_NOT_EQUAL,
    EXPR_CONSTANT_IS_GREATER_OR_EQUAL,
    EXPR_CONSTANT_IS_LESS_OR_EQUAL,
    EXPR_CONSTANT_EXPRESSION_MAX,
  };
  enum VarConstantConstantExpressionType {
    VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS,
    VAR_CONSTANT_CONSTANT_EXPRESSION_MAX,
  };

  enum VarConstantArrayExpressionType {
    VAR_CONSTANT_ARRAY_ELEMENT,
    VAR_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantArrayExpressionType {
    VAR_ARRAY_CONSTANT_ARRAY_SCAL_PROD,
    VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayExpressionType {
    VAR_ARRAY_MAX,
    VAR_ARRAY_MIN,
    VAR_ARRAY_SUM,
    VAR_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantExpressionType {
    VAR_ARRAY_CONSTANT_INDEX,
    VAR_ARRAY_CONSTANT_EXPRESSION_MAX,
  };

  explicit ModelCache(std::function<bool()> is_outside_search);
  ~ModelCache();

  void Clear();

  /// Void constraints.

  Constraint* FindVoidConstraint(VoidConstraintType type) const;

  void InsertVoidConstraint(Constraint* ct, VoidConstraintType type);

  /// Var Constant Constraints.
  Constraint* FindVarConstantConstraint(IntVar* var, int64_t value,
                                        VarConstantConstraintType type) const;

  void InsertVarConstantConstraint(Constraint* ct, IntVar* var, int64_t value,
                                   VarConstantConstraintType type);

  /// Var Constant Constant Constraints.

  Constraint* FindVarConstantConstantConstraint(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type) const;

  void InsertVarConstantConstantConstraint(
      Constraint* ct, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type);

  /// Expr Expr Constraints.

  Constraint* FindExprExprConstraint(IntExpr* expr1, IntExpr* expr2,
                                     ExprExprConstraintType type) const;

  void InsertExprExprConstraint(Constraint* ct, IntExpr* expr1, IntExpr* expr2,
                                ExprExprConstraintType type);

  /// Expr Expressions.

  IntExpr* FindExprExpression(IntExpr* expr, ExprExpressionType type) const;

  void InsertExprExpression(IntExpr* expression, IntExpr* expr,
                            ExprExpressionType type);

  /// Expr Constant Expressions.

  IntExpr* FindExprConstantExpression(IntExpr* expr, int64_t value,
                                      ExprConstantExpressionType type) const;

  void InsertExprConstantExpression(IntExpr* expression, IntExpr* expr,
                                    int64_t value,
                                    ExprConstantExpressionType type);

  /// Expr Expr Expressions.

  IntExpr* FindExprExprExpression(IntExpr* expr1, IntExpr* expr2,
                                  ExprExprExpressionType type) const;

  void InsertExprExprExpression(IntExpr* expression, IntExpr* expr1,
                                IntExpr* expr2, ExprExprExpressionType type);

  /// Expr Expr Constant Expressions.

  IntExpr* FindExprExprConstantExpression(
      IntExpr* expr1, IntExpr* expr2, int64_t constant,
      ExprExprConstantExpressionType type) const;

  void InsertExprExprConstantExpression(IntExpr* expression, IntExpr* expr1,
                                        IntExpr* expr2, int64_t constant,
                                        ExprExprConstantExpressionType type);

  /// Var Constant Constant Expressions.

  IntExpr* FindVarConstantConstantExpression(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type) const;

  void InsertVarConstantConstantExpression(
      IntExpr* expression, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type);

  /// Var Constant Array Expressions.

  IntExpr* FindVarConstantArrayExpression(
      IntVar* var, const std::vector<int64_t>& values,
      VarConstantArrayExpressionType type) const;

  void InsertVarConstantArrayExpression(IntExpr* expression, IntVar* var,
                                        const std::vector<int64_t>& values,
                                        VarConstantArrayExpressionType type);

  /// Var Array Expressions.

  IntExpr* FindVarArrayExpression(const std::vector<IntVar*>& vars,
                                  VarArrayExpressionType type) const;

  void InsertVarArrayExpression(IntExpr* expression,
                                const std::vector<IntVar*>& vars,
                                VarArrayExpressionType type);

  /// Var Array Constant Array Expressions.

  IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type) const;

  void InsertVarArrayConstantArrayExpression(
      IntExpr* expression, const std::vector<IntVar*>& var,
      const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type);

  /// Var Array Constant Expressions.

  IntExpr* FindVarArrayConstantExpression(
      const std::vector<IntVar*>& vars, int64_t value,
      VarArrayConstantExpressionType type) const;

  void InsertVarArrayConstantExpression(IntExpr* expression,
                                        const std::vector<IntVar*>& var,
                                        int64_t value,
                                        VarArrayConstantExpressionType type);

 private:
  std::function<bool()> is_outside_search_;
  std::vector<Constraint*> void_constraints_;
  std::vector<VarConstantConstraintCache*> var_constant_constraints_;
  std::vector<ExprExprConstraintCache*> expr_expr_constraints_;
  std::vector<VarConstantConstantConstraintCache*>
      var_constant_constant_constraints_;
  std::vector<ExprIntExprCache*> expr_expressions_;
  std::vector<ExprConstantIntExprCache*> expr_constant_expressions_;
  std::vector<ExprExprIntExprCache*> expr_expr_expressions_;
  std::vector<VarConstantConstantIntExprCache*>
      var_constant_constant_expressions_;
  std::vector<VarConstantArrayIntExprCache*> var_constant_array_expressions_;
  std::vector<VarArrayIntExprCache*> var_array_expressions_;
  std::vector<VarArrayConstantArrayIntExprCache*>
      var_array_constant_array_expressions_;
  std::vector<VarArrayConstantIntExprCache*> var_array_constant_expressions_;
  std::vector<ExprExprConstantIntExprCache*> expr_expr_constant_expressions_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_MODEL_CACHE_H_
