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

#include <string>

#include "ortools/base/base_export.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/utilities.h"

namespace operations_research {

/// This enum is used internally to do dynamic typing on subclasses of integer
/// variables.
enum VarTypes {
  UNSPECIFIED,
  DOMAIN_INT_VAR,
  BOOLEAN_VAR,
  CONST_VAR,
  VAR_ADD_CST,
  VAR_TIMES_CST,
  CST_SUB_VAR,
  OPP_VAR,
  TRACE_VAR
};

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

class OR_DLL BooleanVar : public IntVar {
 public:
  static const int kUnboundBooleanVarValue;

  explicit BooleanVar(Solver* const s, const std::string& name = "")
      : IntVar(s, name), value_(kUnboundBooleanVarValue) {}

  ~BooleanVar() override {}

  int64_t Min() const override { return (value_ == 1); }
  void SetMin(int64_t m) override;
  int64_t Max() const override { return (value_ != 0); }
  void SetMax(int64_t m) override;
  void SetRange(int64_t mi, int64_t ma) override;
  bool Bound() const override { return (value_ != kUnboundBooleanVarValue); }
  int64_t Value() const override {
    CHECK_NE(value_, kUnboundBooleanVarValue) << "variable is not bound";
    return value_;
  }
  void RemoveValue(int64_t v) override;
  void RemoveInterval(int64_t l, int64_t u) override;
  void WhenBound(Demon* d) override;
  void WhenRange(Demon* d) override { WhenBound(d); }
  void WhenDomain(Demon* d) override { WhenBound(d); }
  uint64_t Size() const override;
  bool Contains(int64_t v) const override;
  IntVarIterator* MakeHoleIterator(bool reversible) const override;
  IntVarIterator* MakeDomainIterator(bool reversible) const override;
  std::string DebugString() const override;
  int VarType() const override { return BOOLEAN_VAR; }

  IntVar* IsEqual(int64_t constant) override;
  IntVar* IsDifferent(int64_t constant) override;
  IntVar* IsGreaterOrEqual(int64_t constant) override;
  IntVar* IsLessOrEqual(int64_t constant) override;

  virtual void RestoreValue() = 0;
  std::string BaseName() const override { return "BooleanVar"; }

  int RawValue() const { return value_; }

 protected:
  int value_;
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_EXPRESSIONS_H_
