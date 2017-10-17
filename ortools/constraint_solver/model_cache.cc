// Copyright 2010-2017 Google
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


#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

DECLARE_int32(cache_initial_size);
DEFINE_bool(cp_disable_cache, false, "Disable caching of model objects");

namespace operations_research {
// ----- ModelCache -----

ModelCache::ModelCache(Solver* const s) : solver_(s) {}

ModelCache::~ModelCache() {}

Solver* ModelCache::solver() const { return solver_; }

namespace {
// ----- Helpers -----

template <class T>
bool IsEqual(const T& a1, const T& a2) {
  return a1 == a2;
}

template <class T>
bool IsEqual(const std::vector<T*>& a1, const std::vector<T*>& a2) {
  if (a1.size() != a2.size()) {
    return false;
  }
  for (int i = 0; i < a1.size(); ++i) {
    if (a1[i] != a2[i]) {
      return false;
    }
  }
  return true;
}

template <class A1, class A2>
uint64 Hash2(const A1& a1, const A2& a2) {
  uint64 a = Hash1(a1);
  uint64 b = GG_ULONGLONG(0xe08c1d668b756f82);  // more of the golden ratio
  uint64 c = Hash1(a2);
  mix(a, b, c);
  return c;
}

template <class A1, class A2, class A3>
uint64 Hash3(const A1& a1, const A2& a2, const A3& a3) {
  uint64 a = Hash1(a1);
  uint64 b = Hash1(a2);
  uint64 c = Hash1(a3);
  mix(a, b, c);
  return c;
}

template <class A1, class A2, class A3, class A4>
uint64 Hash4(const A1& a1, const A2& a2, const A3& a3, const A4& a4) {
  uint64 a = Hash1(a1);
  uint64 b = Hash1(a2);
  uint64 c = Hash2(a3, a4);
  mix(a, b, c);
  return c;
}

template <class C>
void Double(C*** array_ptr, int* size_ptr) {
  DCHECK(array_ptr != nullptr);
  DCHECK(size_ptr != nullptr);
  C** const old_cell_array = *array_ptr;
  const int old_size = *size_ptr;
  (*size_ptr) *= 2;
  (*array_ptr) = new C* [(*size_ptr)];
  memset(*array_ptr, 0, (*size_ptr) * sizeof(**array_ptr));
  for (int i = 0; i < old_size; ++i) {
    C* tmp = old_cell_array[i];
    while (tmp != nullptr) {
      C* const to_reinsert = tmp;
      tmp = tmp->next();
      const uint64 position = to_reinsert->Hash() % (*size_ptr);
      to_reinsert->set_next((*array_ptr)[position]);
      (*array_ptr)[position] = to_reinsert;
    }
  }
  delete[](old_cell_array);
}

// ----- Cache objects built with 1 object -----

template <class C, class A1>
class Cache1 {
 public:
  Cache1()
      : array_(new Cell* [FLAGS_cache_initial_size]),
        size_(FLAGS_cache_initial_size),
        num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache1() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1) const {
    uint64 code = Hash1(a1) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, C* const c) {
    const int position = Hash1(a1) % size_;
    Cell* const cell = new Cell(a1, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, C* const container, Cell* const next)
        : a1_(a1), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1) const {
      if (IsEqual(a1_, a1)) {
        return container_;
      }
      return nullptr;
    }

    uint64 Hash() const { return Hash1(a1_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

// ----- Cache objects built with 2 objects -----

template <class C, class A1, class A2>
class Cache2 {
 public:
  Cache2()
      : array_(new Cell* [FLAGS_cache_initial_size]),
        size_(FLAGS_cache_initial_size),
        num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache2() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1, const A2& a2) const {
    uint64 code = Hash2(a1, a2) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1, a2);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, const A2& a2, C* const c) {
    const int position = Hash2(a1, a2) % size_;
    Cell* const cell = new Cell(a1, a2, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, const A2& a2, C* const container, Cell* const next)
        : a1_(a1), a2_(a2), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1, const A2& a2) const {
      if (IsEqual(a1_, a1) && IsEqual(a2_, a2)) {
        return container_;
      }
      return nullptr;
    }

    uint64 Hash() const { return Hash2(a1_, a2_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    const A2 a2_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

// ----- Cache objects built with 2 objects -----

template <class C, class A1, class A2, class A3>
class Cache3 {
 public:
  Cache3()
      : array_(new Cell* [FLAGS_cache_initial_size]),
        size_(FLAGS_cache_initial_size),
        num_items_(0) {
    memset(array_, 0, sizeof(*array_) * size_);
  }

  ~Cache3() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
    }
    delete[] array_;
  }

  void Clear() {
    for (int i = 0; i < size_; ++i) {
      Cell* tmp = array_[i];
      while (tmp != nullptr) {
        Cell* const to_delete = tmp;
        tmp = tmp->next();
        delete to_delete;
      }
      array_[i] = nullptr;
    }
  }

  C* Find(const A1& a1, const A2& a2, const A3& a3) const {
    uint64 code = Hash3(a1, a2, a3) % size_;
    Cell* tmp = array_[code];
    while (tmp) {
      C* const result = tmp->ReturnsIfEqual(a1, a2, a3);
      if (result != nullptr) {
        return result;
      }
      tmp = tmp->next();
    }
    return nullptr;
  }

  void UnsafeInsert(const A1& a1, const A2& a2, const A3& a3, C* const c) {
    const int position = Hash3(a1, a2, a3) % size_;
    Cell* const cell = new Cell(a1, a2, a3, c, array_[position]);
    array_[position] = cell;
    if (++num_items_ > 2 * size_) {
      Double(&array_, &size_);
    }
  }

 private:
  class Cell {
   public:
    Cell(const A1& a1, const A2& a2, const A3& a3, C* const container,
         Cell* const next)
        : a1_(a1), a2_(a2), a3_(a3), container_(container), next_(next) {}

    C* ReturnsIfEqual(const A1& a1, const A2& a2, const A3& a3) const {
      if (IsEqual(a1_, a1) && IsEqual(a2_, a2) && IsEqual(a3_, a3)) {
        return container_;
      }
      return nullptr;
    }

    uint64 Hash() const { return Hash3(a1_, a2_, a3_); }

    void set_next(Cell* const next) { next_ = next; }

    Cell* next() const { return next_; }

   private:
    const A1 a1_;
    const A2 a2_;
    const A3 a3_;
    C* const container_;
    Cell* next_;
  };

  Cell** array_;
  int size_;
  int num_items_;
};

// ----- Model Cache -----

class NonReversibleCache : public ModelCache {
 public:
  typedef Cache1<IntExpr, IntExpr*> ExprIntExprCache;
  typedef Cache1<IntExpr, std::vector<IntVar*> > VarArrayIntExprCache;

  typedef Cache2<Constraint, IntVar*, int64> VarConstantConstraintCache;
  typedef Cache2<Constraint, IntExpr*, IntExpr*> ExprExprConstraintCache;
  typedef Cache2<IntExpr, IntVar*, int64> VarConstantIntExprCache;
  typedef Cache2<IntExpr, IntExpr*, int64> ExprConstantIntExprCache;
  typedef Cache2<IntExpr, IntExpr*, IntExpr*> ExprExprIntExprCache;
  typedef Cache2<IntExpr, IntVar*, const std::vector<int64>&>
      VarConstantArrayIntExprCache;
  typedef Cache2<IntExpr, std::vector<IntVar*>, const std::vector<int64>&>
      VarArrayConstantArrayIntExprCache;
  typedef Cache2<IntExpr, std::vector<IntVar*>, int64>
      VarArrayConstantIntExprCache;

  typedef Cache3<IntExpr, IntVar*, int64, int64>
      VarConstantConstantIntExprCache;
  typedef Cache3<Constraint, IntVar*, int64, int64>
      VarConstantConstantConstraintCache;
  typedef Cache3<IntExpr, IntExpr*, IntExpr*, int64>
      ExprExprConstantIntExprCache;

  explicit NonReversibleCache(Solver* const solver)
      : ModelCache(solver), void_constraints_(VOID_CONSTRAINT_MAX, nullptr) {
    for (int i = 0; i < VAR_CONSTANT_CONSTRAINT_MAX; ++i) {
      var_constant_constraints_.push_back(new VarConstantConstraintCache);
    }
    for (int i = 0; i < EXPR_EXPR_CONSTRAINT_MAX; ++i) {
      expr_expr_constraints_.push_back(new ExprExprConstraintCache);
    }
    for (int i = 0; i < VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX; ++i) {
      var_constant_constant_constraints_.push_back(
          new VarConstantConstantConstraintCache);
    }
    for (int i = 0; i < EXPR_EXPRESSION_MAX; ++i) {
      expr_expressions_.push_back(new ExprIntExprCache);
    }
    for (int i = 0; i < EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
      expr_constant_expressions_.push_back(new ExprConstantIntExprCache);
    }
    for (int i = 0; i < EXPR_EXPR_EXPRESSION_MAX; ++i) {
      expr_expr_expressions_.push_back(new ExprExprIntExprCache);
    }
    for (int i = 0; i < VAR_CONSTANT_CONSTANT_EXPRESSION_MAX; ++i) {
      var_constant_constant_expressions_.push_back(
          new VarConstantConstantIntExprCache);
    }
    for (int i = 0; i < VAR_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
      var_constant_array_expressions_.push_back(
          new VarConstantArrayIntExprCache);
    }
    for (int i = 0; i < VAR_ARRAY_EXPRESSION_MAX; ++i) {
      var_array_expressions_.push_back(new VarArrayIntExprCache);
    }
    for (int i = 0; i < VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
      var_array_constant_array_expressions_.push_back(
          new VarArrayConstantArrayIntExprCache);
    }
    for (int i = 0; i < VAR_ARRAY_CONSTANT_EXPRESSION_MAX; ++i) {
      var_array_constant_expressions_.push_back(
          new VarArrayConstantIntExprCache);
    }
    for (int i = 0; i < EXPR_EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
      expr_expr_constant_expressions_.push_back(
          new ExprExprConstantIntExprCache);
    }
  }

  ~NonReversibleCache() override {
    STLDeleteElements(&var_constant_constraints_);
    STLDeleteElements(&expr_expr_constraints_);
    STLDeleteElements(&var_constant_constant_constraints_);
    STLDeleteElements(&expr_expressions_);
    STLDeleteElements(&expr_constant_expressions_);
    STLDeleteElements(&expr_expr_expressions_);
    STLDeleteElements(&var_constant_constant_expressions_);
    STLDeleteElements(&var_constant_array_expressions_);
    STLDeleteElements(&var_array_expressions_);
    STLDeleteElements(&var_array_constant_array_expressions_);
    STLDeleteElements(&var_array_constant_expressions_);
    STLDeleteElements(&expr_expr_constant_expressions_);
  }

  void Clear() override {
    for (int i = 0; i < VAR_CONSTANT_CONSTRAINT_MAX; ++i) {
      var_constant_constraints_[i]->Clear();
    }
    for (int i = 0; i < EXPR_EXPR_CONSTRAINT_MAX; ++i) {
      expr_expr_constraints_[i]->Clear();
    }
    for (int i = 0; i < VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX; ++i) {
      var_constant_constant_constraints_[i]->Clear();
    }
    for (int i = 0; i < EXPR_EXPRESSION_MAX; ++i) {
      expr_expressions_[i]->Clear();
    }
    for (int i = 0; i < EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
      expr_constant_expressions_[i]->Clear();
    }
    for (int i = 0; i < EXPR_EXPR_EXPRESSION_MAX; ++i) {
      expr_expr_expressions_[i]->Clear();
    }
    for (int i = 0; i < VAR_CONSTANT_CONSTANT_EXPRESSION_MAX; ++i) {
      var_constant_constant_expressions_[i]->Clear();
    }
    for (int i = 0; i < VAR_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
      var_constant_array_expressions_[i]->Clear();
    }
    for (int i = 0; i < VAR_ARRAY_EXPRESSION_MAX; ++i) {
      var_array_expressions_[i]->Clear();
    }
    for (int i = 0; i < VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX; ++i) {
      var_array_constant_array_expressions_[i]->Clear();
    }
    for (int i = 0; i < VAR_ARRAY_CONSTANT_EXPRESSION_MAX; ++i) {
      var_array_constant_expressions_[i]->Clear();
    }
    for (int i = 0; i < EXPR_EXPR_CONSTANT_EXPRESSION_MAX; ++i) {
      expr_expr_constant_expressions_[i]->Clear();
    }
  }

  // Void Constraint.-

  Constraint* FindVoidConstraint(VoidConstraintType type) const override {
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VOID_CONSTRAINT_MAX);
    return void_constraints_[type];
  }

  void InsertVoidConstraint(Constraint* const ct,
                            VoidConstraintType type) override {
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VOID_CONSTRAINT_MAX);
    DCHECK(ct != nullptr);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache) {
      void_constraints_[type] = ct;
    }
  }

  // VarConstantConstraint.

  Constraint* FindVarConstantConstraint(
      IntVar* const var, int64 value,
      VarConstantConstraintType type) const override {
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTRAINT_MAX);
    return var_constant_constraints_[type]->Find(var, value);
  }

  void InsertVarConstantConstraint(Constraint* const ct, IntVar* const var,
                                   int64 value,
                                   VarConstantConstraintType type) override {
    DCHECK(ct != nullptr);
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTRAINT_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        var_constant_constraints_[type]->Find(var, value) == nullptr) {
      var_constant_constraints_[type]->UnsafeInsert(var, value, ct);
    }
  }

  // Var Constant Constant Constraint.

  Constraint* FindVarConstantConstantConstraint(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) const override {
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX);
    return var_constant_constant_constraints_[type]->Find(var, value1, value2);
  }

  void InsertVarConstantConstantConstraint(
      Constraint* const ct, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantConstraintType type) override {
    DCHECK(ct != nullptr);
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTANT_CONSTRAINT_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        var_constant_constant_constraints_[type]->Find(var, value1, value2) ==
            nullptr) {
      var_constant_constant_constraints_[type]
          ->UnsafeInsert(var, value1, value2, ct);
    }
  }

  // Var Var Constraint.

  Constraint* FindExprExprConstraint(
      IntExpr* const var1, IntExpr* const var2,
      ExprExprConstraintType type) const override {
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_CONSTRAINT_MAX);
    return expr_expr_constraints_[type]->Find(var1, var2);
  }

  void InsertExprExprConstraint(Constraint* const ct, IntExpr* const var1,
                                IntExpr* const var2,
                                ExprExprConstraintType type) override {
    DCHECK(ct != nullptr);
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_CONSTRAINT_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        expr_expr_constraints_[type]->Find(var1, var2) == nullptr) {
      expr_expr_constraints_[type]->UnsafeInsert(var1, var2, ct);
    }
  }

  // Expr Expression.

  IntExpr* FindExprExpression(IntExpr* const expr,
                              ExprExpressionType type) const override {
    DCHECK(expr != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPRESSION_MAX);
    return expr_expressions_[type]->Find(expr);
  }

  void InsertExprExpression(IntExpr* const expression, IntExpr* const expr,
                            ExprExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(expr != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        expr_expressions_[type]->Find(expr) == nullptr) {
      expr_expressions_[type]->UnsafeInsert(expr, expression);
    }
  }

  // Expr Constant Expressions.

  IntExpr* FindExprConstantExpression(
      IntExpr* const expr, int64 value,
      ExprConstantExpressionType type) const override {
    DCHECK(expr != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_CONSTANT_EXPRESSION_MAX);
    return expr_constant_expressions_[type]->Find(expr, value);
  }

  void InsertExprConstantExpression(IntExpr* const expression,
                                    IntExpr* const expr, int64 value,
                                    ExprConstantExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(expr != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_CONSTANT_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        expr_constant_expressions_[type]->Find(expr, value) == nullptr) {
      expr_constant_expressions_[type]->UnsafeInsert(expr, value, expression);
    }
  }

  // Expr Expr Expression.

  IntExpr* FindExprExprExpression(IntExpr* const var1, IntExpr* const var2,
                                  ExprExprExpressionType type) const override {
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_EXPRESSION_MAX);
    return expr_expr_expressions_[type]->Find(var1, var2);
  }

  void InsertExprExprExpression(IntExpr* const expression, IntExpr* const var1,
                                IntExpr* const var2,
                                ExprExprExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        expr_expr_expressions_[type]->Find(var1, var2) == nullptr) {
      expr_expr_expressions_[type]->UnsafeInsert(var1, var2, expression);
    }
  }

  // Expr Expr Constant Expression.

  IntExpr* FindExprExprConstantExpression(
      IntExpr* const var1, IntExpr* const var2, int64 constant,
      ExprExprConstantExpressionType type) const override {
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_CONSTANT_EXPRESSION_MAX);
    return expr_expr_constant_expressions_[type]->Find(var1, var2, constant);
  }

  void InsertExprExprConstantExpression(
      IntExpr* const expression, IntExpr* const var1, IntExpr* const var2,
      int64 constant, ExprExprConstantExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(var1 != nullptr);
    DCHECK(var2 != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, EXPR_EXPR_CONSTANT_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache && expr_expr_constant_expressions_[type]->Find(
                                       var1, var2, constant) == nullptr) {
      expr_expr_constant_expressions_[type]
          ->UnsafeInsert(var1, var2, constant, expression);
    }
  }

  // Var Constant Constant Expression.

  IntExpr* FindVarConstantConstantExpression(
      IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) const override {
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTANT_EXPRESSION_MAX);
    return var_constant_constant_expressions_[type]->Find(var, value1, value2);
  }

  void InsertVarConstantConstantExpression(
      IntExpr* const expression, IntVar* const var, int64 value1, int64 value2,
      VarConstantConstantExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_CONSTANT_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        var_constant_constant_expressions_[type]->Find(var, value1, value2) ==
            nullptr) {
      var_constant_constant_expressions_[type]
          ->UnsafeInsert(var, value1, value2, expression);
    }
  }

  // Var Constant Array Expression.

  IntExpr* FindVarConstantArrayExpression(
      IntVar* const var, const std::vector<int64>& values,
      VarConstantArrayExpressionType type) const override {
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_ARRAY_EXPRESSION_MAX);
    return var_constant_array_expressions_[type]->Find(var, values);
  }

  void InsertVarConstantArrayExpression(
      IntExpr* const expression, IntVar* const var,
      const std::vector<int64>& values,
      VarConstantArrayExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK(var != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_CONSTANT_ARRAY_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        var_constant_array_expressions_[type]->Find(var, values) == nullptr) {
      var_constant_array_expressions_[type]
          ->UnsafeInsert(var, values, expression);
    }
  }

  // Var Array Expression.

  IntExpr* FindVarArrayExpression(const std::vector<IntVar*>& vars,
                                  VarArrayExpressionType type) const override {
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_EXPRESSION_MAX);
    return var_array_expressions_[type]->Find(vars);
  }

  void InsertVarArrayExpression(IntExpr* const expression,
                                const std::vector<IntVar*>& vars,
                                VarArrayExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_EXPRESSION_MAX);
    if (solver()->state() == Solver::OUTSIDE_SEARCH &&
        !FLAGS_cp_disable_cache &&
        var_array_expressions_[type]->Find(vars) == nullptr) {
      var_array_expressions_[type]->UnsafeInsert(vars, expression);
    }
  }

  // Var Array Constant Array Expressions.

  IntExpr* FindVarArrayConstantArrayExpression(
      const std::vector<IntVar*>& vars, const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) const override {
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX);
    return var_array_constant_array_expressions_[type]->Find(vars, values);
  }

  void InsertVarArrayConstantArrayExpression(
      IntExpr* const expression, const std::vector<IntVar*>& vars,
      const std::vector<int64>& values,
      VarArrayConstantArrayExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_CONSTANT_ARRAY_EXPRESSION_MAX);
    if (solver()->state() != Solver::IN_SEARCH &&
        var_array_constant_array_expressions_[type]->Find(vars, values) ==
            nullptr) {
      var_array_constant_array_expressions_[type]
          ->UnsafeInsert(vars, values, expression);
    }
  }

  // Var Array Constant Expressions.

  IntExpr* FindVarArrayConstantExpression(
      const std::vector<IntVar*>& vars, int64 value,
      VarArrayConstantExpressionType type) const override {
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_CONSTANT_EXPRESSION_MAX);
    return var_array_constant_expressions_[type]->Find(vars, value);
  }

  void InsertVarArrayConstantExpression(
      IntExpr* const expression, const std::vector<IntVar*>& vars, int64 value,
      VarArrayConstantExpressionType type) override {
    DCHECK(expression != nullptr);
    DCHECK_GE(type, 0);
    DCHECK_LT(type, VAR_ARRAY_CONSTANT_EXPRESSION_MAX);
    if (solver()->state() != Solver::IN_SEARCH &&
        var_array_constant_expressions_[type]->Find(vars, value) == nullptr) {
      var_array_constant_expressions_[type]
          ->UnsafeInsert(vars, value, expression);
    }
  }

 private:
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
}  // namespace

ModelCache* BuildModelCache(Solver* const solver) {
  return new NonReversibleCache(solver);
}

ModelCache* Solver::Cache() const { return model_cache_.get(); }
}  // namespace operations_research
