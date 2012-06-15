// Copyright 2010-2012 Google
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

#include <stddef.h>
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/const_int_array.h"

DEFINE_int32(cache_initial_size, 1024, "Initial size of the array of the hash "
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
  virtual string DebugString() const;

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

void EqualityExprCst::InitialPropagate() {
  expr_->SetValue(value_);
}

string EqualityExprCst::DebugString() const {
  return StringPrintf("(%s == %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeEquality(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new EqualityExprCst(this, e, v));
}

Constraint* Solver::MakeEquality(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new EqualityExprCst(this, e, v));
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
  virtual string DebugString() const;
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
};

GreaterEqExprCst::GreaterEqExprCst(Solver* const s, IntExpr* const e, int64 v)
  : Constraint(s), expr_(e), value_(v) {}

void GreaterEqExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void GreaterEqExprCst::InitialPropagate() {
  expr_->SetMin(value_);
}

string GreaterEqExprCst::DebugString() const {
  return StringPrintf("(%s >= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v));
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v));
}

Constraint* Solver::MakeGreater(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
}

Constraint* Solver::MakeGreater(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new GreaterEqExprCst(this, e, v + 1));
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
  virtual string DebugString() const;
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
};

LessEqExprCst::LessEqExprCst(Solver* const s, IntExpr* const e, int64 v)
  : Constraint(s), expr_(e), value_(v) {}

void LessEqExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void LessEqExprCst::InitialPropagate() {
  expr_->SetMax(value_);
}

string LessEqExprCst::DebugString() const {
  return StringPrintf("(%s <= %" GG_LL_FORMAT "d)",
                      expr_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v));
}

Constraint* Solver::MakeLessOrEqual(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v));
}

Constraint* Solver::MakeLess(IntExpr* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v - 1));
}

Constraint* Solver::MakeLess(IntExpr* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new LessEqExprCst(this, e, v - 1));
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
  virtual string DebugString() const;
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
    : Constraint(s), var_(var), value_(value), demon_(NULL) {}

void DiffCst::InitialPropagate() {
  if (var_->Size() >= 0xFFFFFFFF) {
    demon_ = MakeConstraintDemon0(solver(),
                                  this,
                                  &DiffCst::BoundPropagate,
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

string DiffCst::DebugString() const {
  return StringPrintf("(%s != %" GG_LL_FORMAT "d)",
                      var_->DebugString().c_str(), value_);
}
}  // namespace

Constraint* Solver::MakeNonEquality(IntVar* const e, int64 v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new DiffCst(this, e, v));
}

Constraint* Solver::MakeNonEquality(IntVar* const e, int v) {
  CHECK_EQ(this, e->solver());
  return RevAlloc(new DiffCst(this, e, v));
}
// ----- is_equal_cst Constraint -----

namespace {
class IsEqualCstCt : public CastConstraint {
 public:
  IsEqualCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(NULL) {}
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
  string DebugString() const {
    return StringPrintf("IsEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
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

IntVar* Solver::MakeIsEqualCstVar(IntVar* const var, int64 value) {
  return var->IsEqual(value);
}

Constraint* Solver::MakeIsEqualCstCt(IntVar* const var,
                                     int64 value,
                                     IntVar* const boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    return MakeIsLessOrEqualCstCt(var, value, boolvar);
  }
  if (value == var->Max()) {
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
  model_cache_->InsertVarConstantExpression(
      boolvar,
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_EQUAL);
  return RevAlloc(new IsEqualCstCt(this, var, value, boolvar));
}

// ----- is_diff_cst Constraint -----

namespace {
class IsDiffCstCt : public CastConstraint {
 public:
  IsDiffCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(NULL) {}

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

  virtual string DebugString() const {
    return StringPrintf("IsDiffCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
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

IntVar* Solver::MakeIsDifferentCstVar(IntVar* const var, int64 value) {
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstVar(var, value + 1);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstVar(var, value - 1);
  }
  if (!var->Contains(value)) {
    return MakeIntConst(1LL);
  }
  if (var->Bound() && var->Value() == value) {
    return MakeIntConst(0LL);
  }
  IntExpr* const cache = model_cache_->FindVarConstantExpression(
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_NOT_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name = var->name();
    if (name.empty()) {
      name = var->DebugString();
    }
    IntVar* const boolvar = MakeBoolVar(
        StringPrintf("Var<%s != %" GG_LL_FORMAT "d>",
                     name.c_str(), value));
    CastConstraint* const maintain =
        RevAlloc(new IsDiffCstCt(this, var, value, boolvar));
    AddConstraint(maintain);
    model_cache_->InsertVarConstantExpression(
        boolvar,
        var,
        value,
        ModelCache::VAR_CONSTANT_IS_NOT_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsDifferentCstCt(IntVar* const var,
                                         int64 value,
                                         IntVar* const boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    return MakeIsGreaterOrEqualCstCt(var, value + 1, boolvar);
  }
  if (value == var->Max()) {
    return MakeIsLessOrEqualCstCt(var, value - 1, boolvar);
  }
  if (boolvar->Bound()) {
    if (boolvar->Min() == 0) {
      return MakeEquality(var, value);
    } else {
      return MakeNonEquality(var, value);
    }
  }
  model_cache_->InsertVarConstantExpression(
      boolvar,
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_NOT_EQUAL);
  return RevAlloc(new IsDiffCstCt(this, var, value, boolvar));
}

// ----- is_greater_equal_cst Constraint -----

namespace {
class IsGreaterEqualCstCt : public CastConstraint {
 public:
  IsGreaterEqualCstCt(Solver* const s, IntVar* const v, int64 c,
                      IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(NULL) {}
  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }
  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = var_->Max() >= cst_;
    int64 l = var_->Min() >= cst_;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 0) {
        var_->SetMax(cst_ - 1);
      } else {
        var_->SetMin(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  virtual string DebugString() const {
    return StringPrintf("IsGreaterEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
  }

 private:
  IntVar* const var_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace

IntVar* Solver::MakeIsGreaterOrEqualCstVar(IntVar* const var, int64 value) {
  if (var->Min() >= value) {
    return MakeIntConst(1LL);
  }
  if (var->Max() < value) {
    return MakeIntConst(0LL);
  }
  IntExpr* const cache = model_cache_->FindVarConstantExpression(
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_GREATER_OR_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name = var->name();
    if (name.empty()) {
      name = var->DebugString();
    }
    IntVar* const boolvar = MakeBoolVar(
        StringPrintf("Var<%s >= %" GG_LL_FORMAT "d>",
                     name.c_str(), value));
    CastConstraint* const maintain =
        RevAlloc(new IsGreaterEqualCstCt(this, var, value, boolvar));
    AddConstraint(maintain);
    model_cache_->InsertVarConstantExpression(
        boolvar,
        var,
        value,
        ModelCache::VAR_CONSTANT_IS_GREATER_OR_EQUAL);
    return boolvar;
  }
}

IntVar* Solver::MakeIsGreaterCstVar(IntVar* const var, int64 value) {
  return MakeIsGreaterOrEqualCstVar(var, value + 1);
}

Constraint* Solver::MakeIsGreaterOrEqualCstCt(IntVar* const var,
                                              int64 value,
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
  model_cache_->InsertVarConstantExpression(
      boolvar,
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_GREATER_OR_EQUAL);
  return RevAlloc(new IsGreaterEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsGreaterCstCt(IntVar* const v, int64 c,
                                       IntVar* const b) {
  return MakeIsGreaterOrEqualCstCt(v, c + 1, b);
}

// ----- is_lesser_equal_cst Constraint -----

namespace {
class IsLessEqualCstCt : public CastConstraint {
 public:
  IsLessEqualCstCt(Solver* const s, IntVar* const v, int64 c, IntVar* const b)
      : CastConstraint(s, b), var_(v), cst_(c), demon_(NULL) {}

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }

  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = var_->Min() <= cst_;
    int64 l = var_->Max() <= cst_;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      inhibit = true;
      if (target_var_->Min() == 0) {
        var_->SetMin(cst_ + 1);
      } else {
        var_->SetMax(cst_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }

  virtual string DebugString() const {
    return StringPrintf("IsLessEqualCstCt(%s, %" GG_LL_FORMAT "d, %s)",
                        var_->DebugString().c_str(),
                        cst_,
                        target_var_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
  }

 private:
  IntVar* const var_;
  int64 cst_;
  Demon* demon_;
};
}  // namespace


IntVar* Solver::MakeIsLessOrEqualCstVar(IntVar* const var, int64 value) {
  if (var->Max() <= value) {
    return MakeIntConst(1LL);
  }
  if (var->Min() > value) {
    return MakeIntConst(0LL);
  }
  IntExpr* const cache = model_cache_->FindVarConstantExpression(
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_LESS_OR_EQUAL);
  if (cache != NULL) {
    return cache->Var();
  } else {
    string name = var->name();
    if (name.empty()) {
      name = var->DebugString();
    }
    IntVar* const boolvar = MakeBoolVar(
        StringPrintf("Var<%s <= %" GG_LL_FORMAT "d>",
                     name.c_str(), value));
    CastConstraint* const maintain =
        RevAlloc(new IsLessEqualCstCt(this, var, value, boolvar));
    AddConstraint(maintain);
    model_cache_->InsertVarConstantExpression(
        boolvar,
        var,
        value,
        ModelCache::VAR_CONSTANT_IS_LESS_OR_EQUAL);
    return boolvar;
  }
}

IntVar* Solver::MakeIsLessCstVar(IntVar* const var, int64 value) {
  return MakeIsLessOrEqualCstVar(var, value - 1);
}

Constraint* Solver::MakeIsLessOrEqualCstCt(IntVar* const var,
                                           int64 value,
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
  model_cache_->InsertVarConstantExpression(
      boolvar,
      var,
      value,
      ModelCache::VAR_CONSTANT_IS_LESS_OR_EQUAL);
  return RevAlloc(new IsLessEqualCstCt(this, var, value, boolvar));
}

Constraint* Solver::MakeIsLessCstCt(IntVar* const v, int64 c,
                                    IntVar* const b) {
  return MakeIsLessOrEqualCstCt(v, c - 1, b);
}

// ----- BetweenCt -----

namespace {
class BetweenCt : public Constraint {
 public:
  BetweenCt(Solver* const s, IntVar* const v, int64 l, int64 u)
      : Constraint(s), var_(v), min_(l), max_(u) {}

  virtual void Post() {}

  virtual void InitialPropagate() {
    var_->SetRange(min_, max_);
  }

  virtual string DebugString() const {
    return StringPrintf("BetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), min_, max_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kBetween, this);
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
    visitor->EndVisitConstraint(ModelVisitor::kBetween, this);
  }

 private:
  IntVar* const var_;
  int64 min_;
  int64 max_;
};
}  // namespace

Constraint* Solver::MakeBetweenCt(IntVar* const v, int64 l, int64 u) {
  CHECK_EQ(this, v->solver());
  return RevAlloc(new BetweenCt(this, v, l, u));
}

// ----- is_between_cst Constraint -----

namespace {
class IsBetweenCt : public Constraint {
 public:
  IsBetweenCt(Solver* const s, IntVar* const v, int64 l, int64 u,
              IntVar* const b)
      : Constraint(s), var_(v), min_(l), max_(u), boolvar_(b), demon_(NULL) {}

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }

  virtual void InitialPropagate() {
    bool inhibit = false;
    int64 u = 1 - (var_->Min() > max_ || var_->Max() < min_);
    int64 l = var_->Max() <= max_ && var_->Min() >= min_;
    boolvar_->SetRange(l, u);
    if (boolvar_->Bound()) {
      inhibit = true;
      if (boolvar_->Min() == 0) {
        var_->RemoveInterval(min_, max_);
      } else {
        var_->SetRange(min_, max_);
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }

  virtual string DebugString() const {
    return StringPrintf(
        "IsBetweenCt(%s, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d, %s)",
        var_->DebugString().c_str(), min_, max_,
        boolvar_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsBetween, this);
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            boolvar_);
    visitor->EndVisitConstraint(ModelVisitor::kIsBetween, this);
  }

 private:
  IntVar* const var_;
  int64 min_;
  int64 max_;
  IntVar* const boolvar_;
  Demon* demon_;
};
}  // namespace

Constraint* Solver::MakeIsBetweenCt(IntVar* const v,
                                    int64 l,
                                    int64 u,
                                    IntVar* const b) {
  CHECK_EQ(this, v->solver());
  CHECK_EQ(this, b->solver());
  return RevAlloc(new IsBetweenCt(this, v, l, u, b));
}

// ---------- Member ----------

// ----- Member(IntVar, IntSet) -----

namespace {
class MemberCt : public Constraint {
 public:
  MemberCt(Solver* const s,
           IntVar* const v,
           std::vector<int64>* const sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != NULL);
    DCHECK(s != NULL);
    DCHECK(sorted_values != NULL);
  }

  virtual void Post() {}

  virtual void InitialPropagate() {
    var_->SetValues(values_.RawData(), values_.size());
  }

  virtual string DebugString() const {
    return StringPrintf("Member(%s, %s)",
                        var_->DebugString().c_str(),
                        values_.DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kValuesArgument,
                                        values_);
    visitor->EndVisitConstraint(ModelVisitor::kMember, this);
  }

 private:
  IntVar* const var_;
  ConstIntArray values_;
};
}  // namespace

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const std::vector<int64>& values) {
  ConstIntArray local_values(values);
  return RevAlloc(
      new MemberCt(this, var, local_values.SortedCopyWithoutDuplicates(true)));
}

Constraint* Solver::MakeMemberCt(IntVar* const var,
                                 const std::vector<int>& values) {
  ConstIntArray local_values(values);
  return RevAlloc(
      new MemberCt(this, var, local_values.SortedCopyWithoutDuplicates(true)));
}

// ----- IsMemberCt -----

namespace {
class IsMemberCt : public Constraint {
 public:
  IsMemberCt(Solver* const s,
             IntVar* const v,
             std::vector<int64>* const sorted_values,
             IntVar* const b)
      : Constraint(s),
        var_(v),
        values_(sorted_values),
        boolvar_(b),
        support_pos_(0),
        demon_(NULL) {
    DCHECK(v != NULL);
    DCHECK(s != NULL);
    DCHECK(b != NULL);
    DCHECK(sorted_values != NULL);
  }

  virtual void Post() {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    if (!var_->Bound()) {
      var_->WhenDomain(demon_);
    }
    if (!boolvar_->Bound()) {
      boolvar_->WhenBound(demon_);
    }
  }

  virtual void InitialPropagate() {
    if (boolvar_->Min() == 1LL) {
      demon_->inhibit(solver());
      var_->SetValues(values_.RawData(), values_.size());
    } else if (boolvar_->Max() == 1LL) {
      int support = support_pos_.Value();
      const int64 vmin = var_->Min();
      const int64 vmax = var_->Max();
      while (support < values_.size() &&
             (values_[support] < vmin ||
              !var_->Contains(values_[support]))) {
        if (values_[support] <= vmax) {
          support++;
        } else {
          support = values_.size();
        }
      }
      support_pos_.SetValue(solver(), support);
      if (support >= values_.size()) {
        demon_->inhibit(solver());
        boolvar_->SetValue(0LL);
      } else if (var_->Bound()) {
        demon_->inhibit(solver());
        boolvar_->SetValue(1LL);
      }
    } else {  // boolvar_ set to 0.
      demon_->inhibit(solver());
      var_->RemoveValues(values_.RawData(), values_.size());
    }
  }

  virtual string DebugString() const {
    return StringPrintf("IsMemberCt(%s, %s, %s)",
                        var_->DebugString().c_str(),
                        values_.DebugString().c_str(),
                        boolvar_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kIsMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitConstIntArrayArgument(ModelVisitor::kValuesArgument,
                                        values_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            boolvar_);
    visitor->EndVisitConstraint(ModelVisitor::kIsMember, this);
  }

 private:
  IntVar* const var_;
  ConstIntArray values_;
  IntVar* const boolvar_;
  Rev<int> support_pos_;
  Demon* demon_;
};
}  // namespace

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const std::vector<int64>& values,
                                   IntVar* const boolvar) {
  if (values.size() == 0) {
    return MakeFalseConstraint();
  } else {
    ConstIntArray local_values(values);
    return RevAlloc(
        new IsMemberCt(this,
                       var,
                       local_values.SortedCopyWithoutDuplicates(true),
                       boolvar));
  }
}

Constraint* Solver::MakeIsMemberCt(IntVar* const var,
                                   const std::vector<int>& values,
                                   IntVar* const boolvar) {
  if (values.size() == 0) {
    return MakeFalseConstraint();
  } else {
    ConstIntArray local_values(values);
    return RevAlloc(
        new IsMemberCt(this,
                       var,
                       local_values.SortedCopyWithoutDuplicates(true),
                       boolvar));
  }
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var,
                                const std::vector<int64>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(var, values, b));
  return b;
}

IntVar* Solver::MakeIsMemberVar(IntVar* const var, const std::vector<int>& values) {
  IntVar* const b = MakeBoolVar();
  AddConstraint(MakeIsMemberCt(var, values, b));
  return b;
}

}  // namespace operations_research
