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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_EXPRESSIONS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_EXPRESSIONS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/util/piecewise_linear_function.h"

namespace operations_research {

/// This is the base class for all expressions that are not variables.
/// It provides a basic 'CastToVar()' implementation.
///
/// The class of expressions represent two types of objects: variables
/// and subclasses of BaseIntExpr. Variables are stateful objects that
/// provide a rich API (remove values, WhenBound...). On the other hand,
/// subclasses of BaseIntExpr represent range-only stateless objects.
/// That is, min(A + B) is recomputed each time as min(A) + min(B).
///
/// Furthermore, sometimes, the propagation on an expression is not complete,
/// and Min(), Max() are not monotonic with respect to SetMin() and SetMax().
/// For instance, if A is a var with domain [0 .. 5], and B another variable
/// with domain [0 .. 5], then Plus(A, B) has domain [0, 10].
///
/// If we apply SetMax(Plus(A, B), 4)), we will deduce that both A
/// and B have domain [0 .. 4]. In that case, Max(Plus(A, B)) is 8
/// and not 4.  To get back monotonicity, we 'cast' the expression
/// into a variable using the Var() method (that will call CastToVar()
/// internally). The resulting variable will be stateful and monotonic.
///
/// Finally, one should never store a pointer to a IntExpr, or
/// BaseIntExpr in the code. The safe code should always call Var() on an
/// expression built by the solver, and store the object as an IntVar*.
/// This is a consequence of the stateless nature of the expressions that
/// makes the code error-prone.
class BaseIntExpr : public IntExpr {
 public:
  explicit BaseIntExpr(Solver* const s) : IntExpr(s), var_(nullptr) {}
  ~BaseIntExpr() override {}

  IntVar* Var() override;
  virtual IntVar* CastToVar();

 private:
  IntVar* var_;
};

// ----- PlusIntExpr -----

class PlusIntExpr : public BaseIntExpr {
 public:
  PlusIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~PlusIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  void Range(int64_t* mi, int64_t* ma) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void ExpandPlusIntExpr(IntExpr* expr, std::vector<IntExpr*>* subs);
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

class SafePlusIntExpr : public BaseIntExpr {
 public:
  SafePlusIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~SafePlusIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- PlusIntCstExpr -----

class PlusIntCstExpr : public BaseIntExpr {
 public:
  PlusIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~PlusIntCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t value_;
};

// ----- SubIntExpr -----

class SubIntExpr : public BaseIntExpr {
 public:
  SubIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~SubIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetRange(int64_t l, int64_t u) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;
  IntExpr* left() const { return left_; }
  IntExpr* right() const { return right_; }

 protected:
  IntExpr* const left_;
  IntExpr* const right_;
};

class SafeSubIntExpr : public SubIntExpr {
 public:
  SafeSubIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~SafeSubIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  void Range(int64_t* mi, int64_t* ma) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// l - r

// ----- SubIntCstExpr -----

class SubIntCstExpr : public BaseIntExpr {
 public:
  SubIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~SubIntCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t value_;
};

// ----- OppIntExpr -----

class OppIntExpr : public BaseIntExpr {
 public:
  OppIntExpr(Solver* s, IntExpr* e);
  ~OppIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  IntVar* CastToVar() override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
};

// ----- TimesIntCstExpr -----

class TimesIntCstExpr : public BaseIntExpr {
 public:
  TimesIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~TimesIntCstExpr() override = default;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  IntExpr* Expr() const { return expr_; }
  int64_t Constant() const { return value_; }
  IntVar* CastToVar() override;
  void Accept(ModelVisitor* visitor) const override;

 protected:
  IntExpr* const expr_;
  const int64_t value_;
};

// ----- TimesPosIntCstExpr -----

class TimesPosIntCstExpr : public TimesIntCstExpr {
 public:
  TimesPosIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~TimesPosIntCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// This expressions adds safe arithmetic (w.r.t. overflows) compared
// to the previous one.
class SafeTimesPosIntCstExpr : public TimesIntCstExpr {
 public:
  SafeTimesPosIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~SafeTimesPosIntCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// ----- TimesIntNegCstExpr -----

class TimesIntNegCstExpr : public TimesIntCstExpr {
 public:
  TimesIntNegCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~TimesIntNegCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// ----- TimesIntExpr -----

class TimesIntExpr : public BaseIntExpr {
 public:
  TimesIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~TimesIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  IntExpr* const minus_left_;
  IntExpr* const minus_right_;
};

// ----- TimesPosIntExpr -----

class TimesPosIntExpr : public BaseIntExpr {
 public:
  TimesPosIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~TimesPosIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- SafeTimesPosIntExpr -----

class SafeTimesPosIntExpr : public BaseIntExpr {
 public:
  SafeTimesPosIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~SafeTimesPosIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- TimesBooleanPosIntExpr -----

class TimesBooleanPosIntExpr : public BaseIntExpr {
 public:
  TimesBooleanPosIntExpr(Solver* s, BooleanVar* b, IntExpr* e);
  ~TimesBooleanPosIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

// ----- TimesBooleanIntExpr -----

class TimesBooleanIntExpr : public BaseIntExpr {
 public:
  TimesBooleanIntExpr(Solver* s, BooleanVar* b, IntExpr* e);
  ~TimesBooleanIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void Range(int64_t* mi, int64_t* ma) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  BooleanVar* const boolvar_;
  IntExpr* const expr_;
};

// ----- DivPosIntCstExpr -----

class DivPosIntCstExpr : public BaseIntExpr {
 public:
  DivPosIntCstExpr(Solver* s, IntExpr* e, int64_t v);
  ~DivPosIntCstExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t value_;
};

// DivPosIntExpr

class DivPosIntExpr : public BaseIntExpr {
 public:
  DivPosIntExpr(Solver* s, IntExpr* num, IntExpr* denom);
  ~DivPosIntExpr() override = default;
  int64_t Min() const override;
  int64_t Max() const override;
  static void SetPosMin(IntExpr* num, IntExpr* denom, int64_t m);
  static void SetPosMax(IntExpr* num, IntExpr* denom, int64_t m);
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const num_;
  IntExpr* const denom_;
  IntExpr* const opp_num_;
};

class DivPosPosIntExpr : public BaseIntExpr {
 public:
  DivPosPosIntExpr(Solver* s, IntExpr* num, IntExpr* denom);
  ~DivPosPosIntExpr() override = default;
  int64_t Min() const override;
  int64_t Max() const override;
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const num_;
  IntExpr* const denom_;
};

// DivIntExpr

class DivIntExpr : public BaseIntExpr {
 public:
  DivIntExpr(Solver* s, IntExpr* num, IntExpr* denom);
  ~DivIntExpr() override = default;
  int64_t Min() const override;
  int64_t Max() const override;
  void SetMin(int64_t m) override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  void AdjustDenominator();
  static void SetPosMin(IntExpr* num, IntExpr* denom, int64_t m);
  static void SetPosMax(IntExpr* num, IntExpr* denom, int64_t m);
  IntExpr* const num_;
  IntExpr* const denom_;
  IntExpr* const opp_num_;
};

// ----- IntAbs And IntAbsConstraint ------

class IntAbsConstraint : public CastConstraint {
 public:
  IntAbsConstraint(Solver* s, IntVar* sub, IntVar* target);
  ~IntAbsConstraint() override = default;
  void Post() override;
  void InitialPropagate() override;
  void PropagateSub();
  void PropagateTarget();
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const sub_;
};

class IntAbs : public BaseIntExpr {
 public:
  IntAbs(Solver* s, IntExpr* e);
  ~IntAbs() override {}
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  void Range(int64_t* mi, int64_t* ma) override;
  bool Bound() const override;
  void WhenRange(Demon* d) override;
  std::string name() const override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;
  IntVar* CastToVar() override;

 private:
  IntExpr* const expr_;
};

// ----- Square -----

// TODO(user): shouldn't we compare to kint32max^2 instead of kint64max?
class IntSquare : public BaseIntExpr {
 public:
  IntSquare(Solver* s, IntExpr* e);
  ~IntSquare() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  void WhenRange(Demon* d) override;
  std::string name() const override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;
  IntExpr* expr() const { return expr_; }

 protected:
  IntExpr* const expr_;
};

class PosIntSquare : public IntSquare {
 public:
  PosIntSquare(Solver* s, IntExpr* e);
  ~PosIntSquare() override {}
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// ----- Power -----

class BasePower : public BaseIntExpr {
 public:
  BasePower(Solver* s, IntExpr* e, int64_t n);
  ~BasePower() override = default;
  bool Bound() const override;
  IntExpr* expr() const { return expr_; }
  int64_t exponant() const { return pow_; }
  void WhenRange(Demon* d) override;
  std::string name() const override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 protected:
  int64_t Pown(int64_t value) const;
  int64_t SqrnDown(int64_t value) const;
  int64_t SqrnUp(int64_t value) const;

  IntExpr* const expr_;
  const int64_t pow_;
  const int64_t limit_;
};

class IntEvenPower : public BasePower {
 public:
  IntEvenPower(Solver* s, IntExpr* e, int64_t n);
  ~IntEvenPower() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

class PosIntEvenPower : public BasePower {
 public:
  PosIntEvenPower(Solver* s, IntExpr* e, int64_t n);
  ~PosIntEvenPower() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

class IntOddPower : public BasePower {
 public:
  IntOddPower(Solver* s, IntExpr* e, int64_t n);
  ~IntOddPower() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
};

// ----- Min(expr, expr) -----

class MinIntExpr : public BaseIntExpr {
 public:
  MinIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~MinIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Min(expr, constant) -----

class MinCstIntExpr : public BaseIntExpr {
 public:
  MinCstIntExpr(Solver* s, IntExpr* e, int64_t v);
  ~MinCstIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t value_;
};

// ----- Max(expr, expr) -----

class MaxIntExpr : public BaseIntExpr {
 public:
  MaxIntExpr(Solver* s, IntExpr* l, IntExpr* r);
  ~MaxIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

// ----- Max(expr, constant) -----

class MaxCstIntExpr : public BaseIntExpr {
 public:
  MaxCstIntExpr(Solver* s, IntExpr* e, int64_t v);
  ~MaxCstIntExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  bool Bound() const override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t value_;
};

// ----- Convex Piecewise -----

// This class is a very simple convex piecewise linear function.  The
// argument of the function is the expression.  Between early_date and
// late_date, the value of the function is 0.  Before early date, it
// is affine and the cost is early_cost * (early_date - x). After
// late_date, the cost is late_cost * (x - late_date).

class SimpleConvexPiecewiseExpr : public BaseIntExpr {
 public:
  SimpleConvexPiecewiseExpr(Solver* s, IntExpr* e, int64_t ec, int64_t ed,
                            int64_t ld, int64_t lc);
  ~SimpleConvexPiecewiseExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t early_cost_;
  const int64_t early_date_;
  const int64_t late_date_;
  const int64_t late_cost_;
};

class PiecewiseLinearExpr : public BaseIntExpr {
 public:
  PiecewiseLinearExpr(Solver* solver, IntExpr* expr,
                      const PiecewiseLinearFunction& f);
  ~PiecewiseLinearExpr() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t l, int64_t u) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* expr_;
  const PiecewiseLinearFunction f_;
};

class SemiContinuousExpr : public BaseIntExpr {
 public:
  SemiContinuousExpr(Solver* s, IntExpr* e, int64_t fixed_charge, int64_t step);
  ~SemiContinuousExpr() override = default;
  int64_t Value(int64_t x) const;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t fixed_charge_;
  const int64_t step_;
};

class SemiContinuousStepOneExpr : public BaseIntExpr {
 public:
  SemiContinuousStepOneExpr(Solver* s, IntExpr* e, int64_t fixed_charge);
  ~SemiContinuousStepOneExpr() override = default;
  int64_t Value(int64_t x) const;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t fixed_charge_;
};

class SemiContinuousStepZeroExpr : public BaseIntExpr {
 public:
  SemiContinuousStepZeroExpr(Solver* s, IntExpr* e, int64_t fixed_charge);
  ~SemiContinuousStepZeroExpr() override = default;
  int64_t Value(int64_t x) const;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  std::string name() const override;
  std::string DebugString() const override;
  void WhenRange(Demon* d) override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntExpr* const expr_;
  const int64_t fixed_charge_;
};

// ----- Conditional Expression -----

class ExprWithEscapeValue : public BaseIntExpr {
 public:
  ExprWithEscapeValue(Solver* s, IntVar* c, IntExpr* e,
                      int64_t unperformed_value);
  // This type is neither copyable nor movable.
  ExprWithEscapeValue(const ExprWithEscapeValue&) = delete;
  ExprWithEscapeValue& operator=(const ExprWithEscapeValue&) = delete;

  ~ExprWithEscapeValue() override = default;
  int64_t Min() const override;
  void SetMin(int64_t m) override;
  int64_t Max() const override;
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  void SetValue(int64_t v) override;
  bool Bound() const override;
  void WhenRange(Demon* d) override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const condition_;
  IntExpr* const expression_;
  const int64_t unperformed_value_;
};

/// Returns true if expr represents a product of a expr and a
/// constant.  In that case, it fills inner_expr and coefficient with
/// these, and returns true. In the other case, it fills inner_expr
/// with expr, coefficient with 1, and returns false.
/// Note that this method will call IsVarProduct but will treat everything as
/// expressions.
bool IsExprProduct(IntExpr* expr, IntExpr** inner_expr, int64_t* coefficient);
bool IsADifference(IntExpr* expr, IntExpr** left, IntExpr** right);
bool IsExprPower(IntExpr* expr, IntExpr** sub_expr, int64_t* exponant);

// Helpers for power expressions.
int64_t IntPowerValue(int64_t value, int64_t power);
int64_t IntPowerOverflowLimit(int64_t power);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_EXPRESSIONS_H_
