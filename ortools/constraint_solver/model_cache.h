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
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

inline uint64_t Hash1(uint64_t value) {
  value = (~value) + (value << 21);  /// value = (value << 21) - value - 1;
  value ^= value >> 24;
  value += (value << 3) + (value << 8);  /// value * 265
  value ^= value >> 14;
  value += (value << 2) + (value << 4);  /// value * 21
  value ^= value >> 28;
  value += (value << 31);
  return value;
}

inline uint64_t Hash1(uint32_t value) {
  uint64_t a = value;
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint64_t Hash1(int64_t value) {
  return Hash1(static_cast<uint64_t>(value));
}

inline uint64_t Hash1(int value) { return Hash1(static_cast<uint32_t>(value)); }

inline uint64_t Hash1(void* const ptr) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__powerpc64__) || \
    defined(__aarch64__) || (defined(_MIPS_SZPTR) && (_MIPS_SZPTR == 64))
  return Hash1(reinterpret_cast<uint64_t>(ptr));
#else
  return Hash1(reinterpret_cast<uint32_t>(ptr));
#endif
}

template <class T>
uint64_t Hash1(const std::vector<T*>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64_t hash = Hash1(ptrs[0]);
  for (int i = 1; i < ptrs.size(); ++i) {
    hash = hash * i + Hash1(ptrs[i]);
  }
  return hash;
}

inline uint64_t Hash1(const std::vector<int64_t>& ptrs) {
  if (ptrs.empty()) return 0;
  if (ptrs.size() == 1) return Hash1(ptrs[0]);
  uint64_t hash = Hash1(ptrs[0]);
  for (int i = 1; i < ptrs.size(); ++i) {
    hash = hash * i + Hash1(ptrs[i]);
  }
  return hash;
}

/// Implements a complete cache for model elements: expressions and
/// constraints.  Caching is based on the signatures of the elements, as
/// well as their types.  This class is used internally to avoid creating
/// duplicate objects.
class ModelCache {
 public:
  enum VoidConstraintType {
    VOID_FALSE_CONSTRAINT = 0,
    VOID_TRUE_CONSTRAINT,
    VOID_CONSTRAINT_MAX,
  };

  enum VarConstantConstraintType {
    VAR_CONSTANT_EQUALITY = 0,
    VAR_CONSTANT_GREATER_OR_EQUAL,
    VAR_CONSTANT_LESS_OR_EQUAL,
    VAR_CONSTANT_NON_EQUALITY,
    VAR_CONSTANT_CONSTRAINT_MAX,
  };

  enum VarConstantConstantConstraintType {
    VAR_CONSTANT_CONSTANT_BETWEEN = 0,
    VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX,
  };

  enum ExprExprConstraintType {
    EXPR_EXPR_EQUALITY = 0,
    EXPR_EXPR_GREATER,
    EXPR_EXPR_GREATER_OR_EQUAL,
    EXPR_EXPR_LESS,
    EXPR_EXPR_LESS_OR_EQUAL,
    EXPR_EXPR_NON_EQUALITY,
    EXPR_EXPR_CONSTRAINT_MAX,
  };

  enum ExprExpressionType {
    EXPR_OPPOSITE = 0,
    EXPR_ABS,
    EXPR_SQUARE,
    EXPR_EXPRESSION_MAX,
  };

  enum ExprExprExpressionType {
    EXPR_EXPR_DIFFERENCE = 0,
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
    EXPR_EXPR_CONSTANT_CONDITIONAL = 0,
    EXPR_EXPR_CONSTANT_EXPRESSION_MAX,
  };

  enum ExprConstantExpressionType {
    EXPR_CONSTANT_DIFFERENCE = 0,
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
    VAR_CONSTANT_CONSTANT_SEMI_CONTINUOUS = 0,
    VAR_CONSTANT_CONSTANT_EXPRESSION_MAX,
  };

  enum VarConstantArrayExpressionType {
    VAR_CONSTANT_ARRAY_ELEMENT = 0,
    VAR_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantArrayExpressionType {
    VAR_ARRAY_CONSTANT_ARRAY_SCAL_PROD = 0,
    VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayExpressionType {
    VAR_ARRAY_MAX = 0,
    VAR_ARRAY_MIN,
    VAR_ARRAY_SUM,
    VAR_ARRAY_EXPRESSION_MAX,
  };

  enum VarArrayConstantExpressionType {
    VAR_ARRAY_CONSTANT_INDEX = 0,
    VAR_ARRAY_CONSTANT_EXPRESSION_MAX,
  };

  explicit ModelCache(Solver* solver);
  virtual ~ModelCache();

  virtual void Clear() = 0;

  /// Void constraints.

  virtual Constraint* FindVoidConstraint(VoidConstraintType type) const = 0;

  virtual void InsertVoidConstraint(Constraint* ct,
                                    VoidConstraintType type) = 0;

  /// Var Constant Constraints.
  virtual Constraint* FindVarConstantConstraint(
      IntVar* var, int64_t value, VarConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstraint(Constraint* ct, IntVar* var,
                                           int64_t value,
                                           VarConstantConstraintType type) = 0;

  /// Var Constant Constant Constraints.

  virtual Constraint* FindVarConstantConstantConstraint(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type) const = 0;

  virtual void InsertVarConstantConstantConstraint(
      Constraint* ct, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantConstraintType type) = 0;

  /// Expr Expr Constraints.

  virtual Constraint* FindExprExprConstraint(
      IntExpr* expr1, IntExpr* expr2, ExprExprConstraintType type) const = 0;

  virtual void InsertExprExprConstraint(Constraint* ct, IntExpr* expr1,
                                        IntExpr* expr2,
                                        ExprExprConstraintType type) = 0;

  /// Expr Expressions.

  virtual IntExpr* FindExprExpression(IntExpr* expr,
                                      ExprExpressionType type) const = 0;

  virtual void InsertExprExpression(IntExpr* expression, IntExpr* expr,
                                    ExprExpressionType type) = 0;

  /// Expr Constant Expressions.

  virtual IntExpr* FindExprConstantExpression(
      IntExpr* expr, int64_t value, ExprConstantExpressionType type) const = 0;

  virtual void InsertExprConstantExpression(
      IntExpr* expression, IntExpr* var, int64_t value,
      ExprConstantExpressionType type) = 0;

  /// Expr Expr Expressions.

  virtual IntExpr* FindExprExprExpression(
      IntExpr* var1, IntExpr* var2, ExprExprExpressionType type) const = 0;

  virtual void InsertExprExprExpression(IntExpr* expression, IntExpr* var1,
                                        IntExpr* var2,
                                        ExprExprExpressionType type) = 0;

  /// Expr Expr Constant Expressions.

  virtual IntExpr* FindExprExprConstantExpression(
      IntExpr* var1, IntExpr* var2, int64_t constant,
      ExprExprConstantExpressionType type) const = 0;

  virtual void InsertExprExprConstantExpression(
      IntExpr* expression, IntExpr* var1, IntExpr* var2, int64_t constant,
      ExprExprConstantExpressionType type) = 0;

  /// Var Constant Constant Expressions.

  virtual IntExpr* FindVarConstantConstantExpression(
      IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type) const = 0;

  virtual void InsertVarConstantConstantExpression(
      IntExpr* expression, IntVar* var, int64_t value1, int64_t value2,
      VarConstantConstantExpressionType type) = 0;

  /// Var Constant Array Expressions.

  virtual IntExpr* FindVarConstantArrayExpression(
      IntVar* var, const std::vector<int64_t>& values,
      VarConstantArrayExpressionType type) const = 0;

  virtual void InsertVarConstantArrayExpression(
      IntExpr* expression, IntVar* var, const std::vector<int64_t>& values,
      VarConstantArrayExpressionType type) = 0;

  /// Var Array Expressions.

  virtual IntExpr* FindVarArrayExpression(
      const std::vector<IntVar*>& vars, VarArrayExpressionType type) const = 0;

  virtual void InsertVarArrayExpression(IntExpr* expression,
                                        const std::vector<IntVar*>& vars,
                                        VarArrayExpressionType type) = 0;

  /// Var Array Constant Array Expressions.

  virtual IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type) const = 0;

  virtual void InsertVarArrayConstantArrayExpression(
      IntExpr* expression, const std::vector<IntVar*>& var,
      const std::vector<int64_t>& values,
      VarArrayConstantArrayExpressionType type) = 0;

  /// Var Array Constant Expressions.

  virtual IntExpr* FindVarArrayConstantExpression(
      const std::vector<IntVar*>& vars, int64_t value,
      VarArrayConstantExpressionType type) const = 0;

  virtual void InsertVarArrayConstantExpression(
      IntExpr* expression, const std::vector<IntVar*>& var, int64_t value,
      VarArrayConstantExpressionType type) = 0;

  Solver* solver() const;

 private:
  Solver* const solver_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_MODEL_CACHE_H_
