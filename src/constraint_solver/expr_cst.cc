// Copyright 2010-2013 Google
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
//
//  Expression constraints

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

DEFINE_int32(cache_initial_size, 1024,
             "Initial size of the array of the hash "
             "table of caches for objects of type Var(x == 3)");

namespace operations_research {

//-----------------------------------------------------------------------------
// Equality

namespace {
class EqualityExprCst : public Constraint {
 public:
  EqualityExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~EqualityExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual IntVar* Var() {
    return solver()->MakeIsEqualCstVar(expr_->Var(), value_);
  }
  virtual std::string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
  }

 private:
  IntExpr* const expr_;
  int64 value_;
};

EqualityExprCst::EqualityExprCst(Solver* const s, IntExpr* const e, int64 v)
    : Constraint(s), expr_(e), value_(v) {}

void EqualityExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void EqualityExprCst::InitialPropagate() { expr_->SetValue(value_); }

std::string EqualityExprCst::DebugString() const {
  return StringPrintf("(%s == %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeEquality(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

Constraint* Solver::MakeEquality(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeFalseConstraint();
  } else if (e->Min() == e->Max() && e->Min() == v) {
    return MakeTrueConstraint();
  } else {
    return RevAlloc(new EqualityExprCst(this, e, v));
  }
}

//-----------------------------------------------------------------------------
// Greater or equal constraint

namespace {
class GreaterEqExprCst : public Constraint {
 public:
  GreaterEqExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~GreaterEqExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual std::string DebugString() const;
  virtual IntVar* Var() {
    return solver()->MakeIsGreaterOrEqualCstVar(expr_->Var(), value_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
  }

 private:
  IntExpr* const expr_;
  int64 value_;
  Demon* demon_;
};

GreaterEqExprCst::GreaterEqExprCst(Solver* const s, IntExpr* const e, int64 v)
    : Constraint(s), expr_(e), value_(v), demon_(nullptr) {}

void GreaterEqExprCst::Post() {
  if (!expr_->IsVar() && expr_->Min() < value_) {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
  } else {
    // Let's clean the demon in case the constraint is posted during search.
    demon_ = nullptr;
  }
}

void GreaterEqExprCst::InitialPropagate() {
  expr_->SetMin(value_);
  if (demon_ != nullptr && expr_->Min() >= value_) {
    demon_->inhibit(solver());
  }
}

std::string GreaterEqExprCst::DebugString() const {
  return StringPrintf("(%s >= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() >= v) {
    return MakeTrueConstraint();
  } else if (e->Max() < v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeGreater(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

Constraint* Solver::MakeGreater(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Min() > v) {
    return MakeTrueConstraint();
  } else if (e->Max() <= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
  }
}

//-----------------------------------------------------------------------------
// Less or equal constraint

namespace {
class LessEqExprCst : public Constraint {
 public:
  LessEqExprCst(Solver* const s, IntExpr* const e, int64 v);
  virtual ~LessEqExprCst() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual std::string DebugString() const;
  virtual IntVar* Var() {
    return solver()->MakeIsLessOrEqualCstVar(expr_->Var(), value_);
  }
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  IntExpr* const expr_;
  int64 value_;
  Demon* demon_;
};

LessEqExprCst::LessEqExprCst(Solver* const s, IntExpr* const e, int64 v)
    : Constraint(s), expr_(e), value_(v), demon_(nullptr) {}

void LessEqExprCst::Post() {
  if (!expr_->IsVar() && expr_->Max() > value_) {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
  } else {
    // Let's clean the demon in case the constraint is posted during search.
    demon_ = nullptr;
  }
}

void LessEqExprCst::InitialPropagate() {
  expr_->SetMax(value_);
  if (demon_ != nullptr && expr_->Max() <= value_) {
    demon_->inhibit(solver());
  }
}

std::string LessEqExprCst::DebugString() const {
  return StringPrintf("(%s <= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() <= v) {
    return MakeTrueConstraint();
  } else if (e->Min() > v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v));
  }
}

Constraint* Solver::MakeLess(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

Constraint* Solver::MakeLess(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  if (e->Max() < v) {
    return MakeTrueConstraint();
  } else if (e->Min() >= v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new LessEqExprCst(this, e, v - 1));
  }
}

//-----------------------------------------------------------------------------
// Different constraints

namespace {
class DiffCst : public Constraint {
 public:
  DiffCst(Solver* const s, IntVar* const var, int64 value);
  virtual ~DiffCst() {}
  virtual void Post() {}
  virtual void InitialPropagate();
  void BoundPropagate();
  virtual std::string DebugString() const;
  virtual IntVar* Var() {
    return solver()->MakeIsDifferentCstVar(var_, value_);
  }
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kNonEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitConstraint(ModelVisitor::kNonEqual, this);
  }

 private:
  IntVar* const var_;
  int64 value_;
  Demon* demon_;
};

DiffCst::DiffCst(Solver* const s, IntVar* const var, int64 value)
    : Constraint(s), var_(var), value_(value), demon_(nullptr) {}

void DiffCst::InitialPropagate() {
  if (var_->Size() >= 0xFFFFFF) {
    demon_ = MakeConstraintDemon0(solver(), this, &DiffCst::BoundPropagate,
                                  "BoundPropagate");
    var_->WhenRange(demon_);
  } else {
    var_->RemoveValue(value_);
  }
}

void DiffCst::BoundPropagate() {
  const int64 var_min = var_->Min();
  const int64 var_max = var_->Max();
  if (var_min > value_ || var_max < value_) {
    demon_->inhibit(solver());
  } else if (var_min == value_) {
    var_->SetMin(value_ + 1);
  } else if (var_max == value_) {
    var_->SetMax(value_ - 1);
  } else if (var_->Size() <= 0xFFFFFF) {
    demon_->inhibit(solver());
    var_->RemoveValue(value_);
  }
}

std::string DiffCst::DebugString() const {
  return StringPrintf("(%s != %" GG_LL_FORMAT "d)", var_->DebugString().c_str(),
                      value_);
}
}  // namespace

Constraint* Solver::MakeNonEquality(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}

Constraint* Solver::MakeNonEquality(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(e, &left, &right)) {
    return MakeNonEquality(left, MakeSum(right, v));
  } else if (e->IsVar() && !e->Var()->Contains(v)) {
    return MakeTrueConstraint();
  } else if (e->Bound() && e->Min() == v) {
    return MakeFalseConstraint();
  } else {
    return RevAlloc(new DiffCst(this, e->Var(), v));
  }
}
// ----- is_equal_cst Constraint -----

namespace {
class IsEqualCstCt : public CastConstraint {
 public:
  IsEqualCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(nullptr) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    target_var_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = var_->Bound();
    int64 u = var_->Contains(cst_);
    int64 l = inhibit ? u : 0;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 0) {
        var_->RemoveValue(cst_);
      } else {
        var_->SetValue(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  std::string DebugString() const {
    return StringPrintf("IsEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(), cst_,
                        target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsEqual, this);
  }

 private:
  IntVar* const var_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace

IntVar* Solver::MakeIsEqualCstVar(IntExpr* const var, int64 value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualVar(left, MakeSum(right, value));
  }
  if (var->Max() - var->Min() == 1) {
    if (value == var->Min()) {
      return MakeDifference(value + 1, var)->Var();
    } else if (value == var->Max()) {
      return MakeSum(var, -value + 1)->Var();
    } else {
      return MakeIntConst(0);
    }
  }
  if (var->IsVar()) {
    return var->Var()->IsEqual(value);
  } else {
    IntVar* const boolvar = MakeBoolVar(StringPrintf(
        "Is(%s == %" GG_LL_FORMAT "d)", var->DebugString().c_str(), value));
    AddConstraint(MakeIsEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCstCt(IntExpr* const var, int64 value,
                                     IntVar* const boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    if (var->Max() - var->Min() == 1) {
      return MakeEquality(MakeDifference(value + 1, var), boolvar);
    }
    return MakeIsLessOrEqualCstCt(var, value, boolvar);
  }
  if (value == var->Max()) {
    if (var->Max() - var->Min() == 1) {
      return MakeEquality(MakeSum(var, -value + 1), boolvar);
    }
    return MakeIsGreaterOrEqualCstCt(var, value, boolvar);
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeNonEquality(var, value);
    } else {
      return MakeEquality(var, value);
    }
  }
  // TODO(user) : what happens if the constraint is not posted?
  // The cache becomes tainted.
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsEqualCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsEqualCstCt(this, var->Var(), value, boolvar));
  }
}

// ----- is_diff_cst Constraint -----

namespace {
class IsDiffCstCt : public CastConstraint {
 public:
  IsDiffCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(nullptr) {}

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    target_var_->WhenBound(demon_);
  }

  virtual void InitialPropagate() {
    bool inhibit = var_->Bound();
    int64 l = 1 - var_->Contains(cst_);
    int64 u = inhibit ? l : 1;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 1) {
        var_->RemoveValue(cst_);
      } else {
        var_->SetValue(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("IsDiffCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(), cst_,
                        target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsDifferent, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsDifferent, this);
  }

 private:
  IntVar* const var_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace

IntVar* Solver::MakeIsDifferentCstVar(IntExpr* const var, int64 value) {
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentVar(left, MakeSum(right, value));
  }
  return var->Var()->IsDifferent(value);
}

Constraint* Solver::MakeIsDifferentCstCt(IntExpr* const var, int64 value,
                                         IntVar* const boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstCt(var, value + 1, boolvar);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstCt(var, value - 1, boolvar);
  }
  if (var->IsVar() && !var->Var()->Contains(value)) {
    return MakeEquality(boolvar, 1LL);
  }
  if (var->Bound() && var->Min() == value) {
    return MakeEquality(boolvar, Zero());
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeEquality(var, value);
    } else {
      return MakeNonEquality(var, value);
    }
  }
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_NOT_EQUAL);
  IntExpr* left = nullptr;
  IntExpr* right = nullptr;
  if (IsADifference(var, &left, &right)) {
    return MakeIsDifferentCt(left, MakeSum(right, value), boolvar);
  } else {
    return RevAlloc(new IsDiffCstCt(this, var->Var(), value, boolvar));
  }
}

// ----- is_greater_equal_cst Constraint -----

namespace {
class IsGreaterEqualCstCt : public CastConstraint {
 public:
  IsGreaterEqualCstCt(Solver* const s, IntExpr* const v, int64 c,
                      IntVar* const b)
      : CastConstraint(s, b), expr_(v), cst_(c), demon_(nullptr) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = expr_->Max() >= cst_;
    int64 l = expr_->Min() >= cst_;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 0) {
        expr_->SetMax(cst_ - 1);
      } else {
        expr_->SetMin(cst_);
      }
    }
    if (inhibit && ((target_var_->Max() == 0 && expr_->Max() < cst_) ||
                    (target_var_->Min() == 1 && expr_->Min() >= cst_))) {
      // Can we safely inhibit? Sometimes an expression is not
      // persistent, just monotonic.
      demon_->inhibit(solver());
    }
  }
  virtual std::string DebugString() const {
    return StringPrintf("IsGreaterEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        expr_->DebugString().c_str(), cst_,
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
  }

 private:
  IntExpr* const expr_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace

IntVar* Solver::MakeIsGreaterOrEqualCstVar(IntExpr* const var, int64 value) {
  if (var->Min() >= value) {
    return MakeIntConst(1LL);
  }
  if (var->Max() < value) {
    return MakeIntConst(0LL);
  }
  if (var->IsVar()) {
    return var->Var()->IsGreaterOrEqual(value);
  } else {
    IntVar* const boolvar = MakeBoolVar(StringPrintf(
        "Is(%s >= %" GG_LL_FORMAT "d)", var->DebugString().c_str(), value));
    AddConstraint(MakeIsGreaterOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsGreaterCstVar(IntExpr* const var, int64 value) {
  return MakeIsGreaterOrEqualCstVar(var, value + 1);
}

Constraint* Solver::MakeIsGreaterOrEqualCstCt(IntExpr* const var, int64 value,
                                              IntVar* const boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeLess(var, value);
    } else {
      return MakeGreaterOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_GREATER_OR_EQUAL);
  return RevAlloc(new IsGreaterEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsGreaterCstCt(IntExpr* const v, int64 c,
                                       IntVar* const b) {
  return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
}

// ----- is_lesser_equal_cst Constraint -----

namespace {
class IsLessEqualCstCt : public CastConstraint {
 public:
  IsLessEqualCstCt(Solver* const s, IntExpr* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), expr_(v), cst_(c), demon_(nullptr) {}

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }

  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = expr_->Min() <= cst_;
    int64 l = expr_->Max() <= cst_;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 0) {
        expr_->SetMin(cst_ + 1);
      } else {
        expr_->SetMax(cst_);
      }
    }
    if (inhibit && ((target_var_->Max() == 0 && expr_->Min() > cst_) ||
                    (target_var_->Min() == 1 && expr_->Max() <= cst_))) {
      // Can we safely inhibit? Sometimes an expression is not
      // persistent, just monotonic.
      demon_->inhibit(solver());
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("IsLessEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        expr_->DebugString().c_str(), cst_,
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
  }

 private:
  IntExpr* const expr_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace

IntVar* Solver::MakeIsLessOrEqualCstVar(IntExpr* const var, int64 value) {
  if (var->Max() <= value) {
    return MakeIntConst(1LL);
  }
  if (var->Min() > value) {
    return MakeIntConst(0LL);
  }
  if (var->IsVar()) {
    return var->Var()->IsLessOrEqual(value);
  } else {
    IntVar* const boolvar = MakeBoolVar(StringPrintf(
        "Is(%s <= %" GG_LL_FORMAT "d)", var->DebugString().c_str(), value));
    AddConstraint(MakeIsLessOrEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

IntVar* Solver::MakeIsLessCstVar(IntExpr* const var, int64 value) {
  return MakeIsLessOrEqualCstVar(var, value - 1);
}

Constraint* Solver::MakeIsLessOrEqualCstCt(IntExpr* const var, int64 value,
                                           IntVar* const boolvar) {
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeGreater(var, value);
    } else {
      return MakeLessOrEqual(var, value);
    }
  }
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  model_cache_->InsertExprConstantExpression(
      boolvar, var, value, ModelCache::EXPR_CONSTANT_IS_LESS_OR_EQUAL);
  return RevAlloc(new IsLessEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsLessCstCt(IntExpr* const v, int64 c,
                                    IntVar* const b) {
  return MakeIsLessOrEqualCstCt(v, c - 1, b);
}

// ----- BetweenCt -----

namespace {
class BetweenCt : public Constraint {
 public:
  BetweenCt(Solver* const s, IntExpr* const v, int64 l, int64 u)
      : Constraint(s), expr_(v), min_(l), max_(u) {}

  virtual void Post() {
    if (!expr_->IsVar()) {
      Demon* const d = solver()->MakeConstraintInitialPropagateCallback(this);
      expr_->WhenRange(d);
    }
  }

  virtual void InitialPropagate() { expr_->SetRange(min_, max_); }

  virtual std::string DebugString() const {
    return StringPrintf("BetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d)",
                        expr_->DebugString().c_str(), min_, max_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kBetween, this);
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
    visitor->EndVisitConstraint(ModelVisitor::kBetween, this);
  }

 private:
  IntExpr* const expr_;
  int64 min_;
  int64 max_;
};
}  // namespace

Constraint* Solver::MakeBetweenCt(IntExpr* const v, int64 l, int64 u) {
  CHECK_EQ(this, v->solver());
  if (v->Min() < l || v->Max() > u) {
    return RevAlloc(new BetweenCt(this, v, l, u));
  } else {
    return MakeTrueConstraint();
  }
}

// ----- is_between_cst Constraint -----

namespace {
class IsBetweenCt : public Constraint {
 public:
  IsBetweenCt(Solver* const s, IntExpr* const e, int64 l, int64 u,
              IntVar* const b)
      : Constraint(s),
        expr_(e),
        min_(l),
        max_(u),
        boolvar_(b),
        demon_(nullptr) {}

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }

  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    int64 u = 1 - (emin > max_ || emax < min_);
    int64 l = emax <= max_ && emin >= min_;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        if (expr_->IsVar()) {
          expr_->Var()->RemoveInterval(min_, max_);
          inhibit = true;
        } else if (emin > min_) {
          expr_->SetMin(max_ + 1);
        } else if (emax < max_) {
          expr_->SetMax(min_ - 1);
        }
      } else {
        expr_->SetRange(min_, max_);
        inhibit = true;
      }
      if (inhibit && expr_->IsVar()) {
        demon_->inhibit(solver());
      }
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("IsBetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT
                        "d, %s)",
                        expr_->DebugString().c_str(), min_, max_,
                        boolvar_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsBetween, this);
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            expr_);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            boolvar_);
    visitor->EndVisitConstraint(ModelVisitor::kIsBetween, this);
  }

 private:
  IntExpr* const expr_;
  int64 min_;
  int64 max_;
  IntVar* const boolvar_;
  Demon* demon_;
};
}  // namespace
Constraint* Solver::MakeIsBetweenCt(IntExpr* const v, int64 l, int64 u,
                                    IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  return RevAlloc(new IsBetweenCt(this, v, l, u, b));
}

IntVar* Solver::MakeIsBetweenVar(IntExpr* const v, int64 l, int64 u) {
  CHECK_EQ(this, v->solver());
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsBetweenCt(v, l, u, b));
  return b;
}

// ---------- Member ----------

// ----- Member(IntVar, IntSet) -----

namespace {
// TODO(user): Do not create holes on expressions.
class MemberCt : public Constraint {
 public:
  MemberCt(Solver* const s, IntVar* const v, const std::vector<int64>& sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != nullptr);
    DCHECK(s != nullptr);
  }

  virtual void Post() {}

  virtual void InitialPropagate() { var_->SetValues(values_); }

  virtual std::string DebugString() const {
    return StringPrintf("Member(%s, %s)", var_->DebugString().c_str(),
                        strings::Join(values_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->EndVisitConstraint(ModelVisitor::kMember, this);
  }

 private:
  IntVar* const var_;
  const std::vector<int64> values_;
};

class NotMemberCt : public Constraint {
 public:
  NotMemberCt(Solver* const s, IntVar* const v, const std::vector<int64>& sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != nullptr);
    DCHECK(s != nullptr);
  }

  virtual void Post() {}

  virtual void InitialPropagate() { var_->RemoveValues(values_); }

  virtual std::string DebugString() const {
    return StringPrintf("NotMember(%s, %s)", var_->DebugString().c_str(),
                        strings::Join(values_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->EndVisitConstraint(ModelVisitor::kMember, this);
  }

 private:
  IntVar* const var_;
  const std::vector<int64> values_;
};
}  // namespace

Constraint* Solver::MakeMemberCt(IntExpr* const var,
                                 const std::vector<int64>& values) {
  std::vector<int64> sorted = SortedNoDuplicates(values);
  if (IsIncreasingContiguous(sorted)) {
    return MakeBetweenCt(var, sorted.front(), sorted.back());
  } else {
    // Let's build the reverse vector.
    if (var->Max() - var->Min() < 2 * values.size()) {
      hash_set<int64> value_set(values.begin(), values.end());
      std::vector<int64> remaining;
      for (int64 value = var->Min(); value <= var->Max(); ++value) {
        if (!ContainsKey(value_set, value)) {
          remaining.push_back(value);
        }
      }
      if (remaining.empty()) {
        return MakeTrueConstraint();
      } else if (remaining.size() == 1) {
        return MakeNonEquality(var, remaining.back());
      } else if (remaining.size() < values.size()) {
        return RevAlloc(new NotMemberCt(this, var->Var(), remaining));
      }
    }
    return RevAlloc(new MemberCt(this, var->Var(), sorted));
  }
}

Constraint* Solver::MakeMemberCt(IntExpr* const var,
                                 const std::vector<int>& values) {
  std::vector<int64> sorted = SortedNoDuplicates(ToInt64Vector(values));
  if (IsIncreasingContiguous(sorted)) {
    return MakeBetweenCt(var, sorted.front(), sorted.back());
  } else {
    return RevAlloc(new MemberCt(this, var->Var(), sorted));
  }
}

// ----- IsMemberCt -----

namespace {
class IsMemberCt : public Constraint {
 public:
  IsMemberCt(Solver* const s, IntVar* const v,
             const std::vector<int64>& sorted_values, IntVar* const b)
      : Constraint(s),
        var_(v),
        values_as_set_(sorted_values.begin(), sorted_values.end()),
        values_(sorted_values),
        boolvar_(b),
        support_(0),
        demon_(nullptr),
        domain_(var_->MakeDomainIterator(true)),
        neg_support_(kint64min) {
    DCHECK(v != nullptr);
    DCHECK(s != nullptr);
    DCHECK(b != nullptr);
    while (ContainsKey(values_as_set_, neg_support_)) {
      neg_support_++;
    }
  }

  virtual void Post() {
    demon_ = MakeConstraintDemon0(solver(), this, &IsMemberCt::VarDomain,
                                  "VarDomain");
    if (!var_->Bound()) {
      var_->WhenDomain(demon_);
    }
    if (!boolvar_->Bound()) {
      Demon* const bdemon = MakeConstraintDemon0(
          solver(), this, &IsMemberCt::TargetBound, "TargetBound");
      boolvar_->WhenBound(bdemon);
    }
  }

  virtual void InitialPropagate() {
    boolvar_->SetRange(0, 1);
    if (boolvar_->Bound()) {
      TargetBound();
    } else {
      VarDomain();
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("IsMemberCt(%s, %s, %s)", var_->DebugString().c_str(),
                        strings::Join(values_, ", ").c_str(),
                        boolvar_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            boolvar_);
    visitor->EndVisitConstraint(ModelVisitor::kIsMember, this);
  }

 private:
  void VarDomain() {
    if (boolvar_->Bound()) {
      TargetBound();
    } else {
      for (int offset = 0; offset < values_.size(); ++offset) {
        const int candidate = (support_ + offset) % values_.size();
        if (var_->Contains(values_[candidate])) {
          support_ = candidate;
          if (var_->Bound()) {
            demon_->inhibit(solver());
            boolvar_->SetValue(1);
            return;
          }
          // We have found a positive support. Let's check the
          // negative support.
          if (var_->Contains(neg_support_)) {
            return;
          } else {
            // Look for a new negative support.
            for (const int64 value : InitAndGetValues(domain_)) {
              if (!ContainsKey(values_as_set_, value)) {
                neg_support_ = value;
                return;
              }
            }
          }
          // No negative support, setting boolvar to true.
          demon_->inhibit(solver());
          boolvar_->SetValue(1);
          return;
        }
      }
      // No positive support, setting boolvar to false.
      demon_->inhibit(solver());
      boolvar_->SetValue(0);
    }
  }

  void TargetBound() {
    DCHECK(boolvar_->Bound());
    if (boolvar_->Min() == 1LL) {
      demon_->inhibit(solver());
      var_->SetValues(values_);
    } else {
      demon_->inhibit(solver());
      var_->RemoveValues(values_);
    }
  }

  IntVar* const var_;
  hash_set<int64> values_as_set_;
  std::vector<int64> values_;
  IntVar* const boolvar_;
  int support_;
  Demon* demon_;
  IntVarIterator* const domain_;
  int64 neg_support_;
};

template <class T>
Constraint* BuildIsMemberCt(Solver* const solver, IntExpr* const expr,
                            const std::vector<T>& values, IntVar* const boolvar) {
  std::set<T> set_of_values(values.begin(), values.end());
  std::vector<int64> filtered_values;
  bool all_values = false;
  if (expr->IsVar()) {
    IntVar* const var = expr->Var();
    for (const T value : set_of_values) {
      if (var->Contains(value)) {
        filtered_values.push_back(value);
      }
    }
    all_values = (filtered_values.size() == var->Size());
  } else {
    int64 emin = 0;
    int64 emax = 0;
    expr->Range(&emin, &emax);
    for (const T value : set_of_values) {
      if (value >= emin && value <= emax) {
        filtered_values.push_back(value);
      }
    }
    all_values = (filtered_values.size() == emax - emin + 1);
  }
  if (filtered_values.empty()) {
    return solver->MakeEquality(boolvar, Zero());
  } else if (all_values) {
    return solver->MakeEquality(boolvar, 1);
  } else if (filtered_values.size() == 1) {
    return solver->MakeIsEqualCstCt(expr, filtered_values.back(), boolvar);
  } else if (filtered_values.back() ==
             filtered_values.front() + filtered_values.size() - 1) {
    // Contiguous
    return solver->MakeIsBetweenCt(expr, filtered_values.front(),
                                   filtered_values.back(), boolvar);
  } else {
    return solver->RevAlloc(
        new IsMemberCt(solver, expr->Var(), filtered_values, boolvar));
  }
}
}  // namespace

Constraint* Solver::MakeIsMemberCt(IntExpr* const expr,
                                   const std::vector<int64>& values,
                                   IntVar* const boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

Constraint* Solver::MakeIsMemberCt(IntExpr* const expr,
                                   const std::vector<int>& values,
                                   IntVar* const boolvar) {
  return BuildIsMemberCt(this, expr, values, boolvar);
}

IntVar* Solver::MakeIsMemberVar(IntExpr* const expr,
                                const std::vector<int64>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntExpr* const expr,
                                const std::vector<int>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(expr, values, b));
  return b;
}

}  // namespace operations_research
