// Copyright 2010-2018 Google LLC
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

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

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
  ~EqualityExprCst() override {}
  void Post() override;
  void InitialPropagate() override;
  IntVar* Var() override {
    return solver()->MakeIsEqualCstVar(expr_->Var(), value_);
  }
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
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
  return absl::StrFormat("(%s == %d)", expr_->DebugString(), value_);
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
  ~GreaterEqExprCst() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsGreaterOrEqualCstVar(expr_->Var(), value_);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  return absl::StrFormat("(%s >= %d)", expr_->DebugString(), value_);
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
  ~LessEqExprCst() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsLessOrEqualCstVar(expr_->Var(), value_);
  }
  void Accept(ModelVisitor* const visitor) const override {
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
  return absl::StrFormat("(%s <= %d)", expr_->DebugString(), value_);
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
  ~DiffCst() override {}
  void Post() override {}
  void InitialPropagate() override;
  void BoundPropagate();
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsDifferentCstVar(var_, value_);
  }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNonEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
    visitor->EndVisitConstraint(ModelVisitor::kNonEqual, this);
  }

 private:
  bool HasLargeDomain(IntVar* var);

  IntVar* const var_;
  int64 value_;
  Demon* demon_;
};

DiffCst::DiffCst(Solver* const s, IntVar* const var, int64 value)
    : Constraint(s), var_(var), value_(value), demon_(nullptr) {}

void DiffCst::InitialPropagate() {
  if (HasLargeDomain(var_)) {
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
  } else if (!HasLargeDomain(var_)) {
    demon_->inhibit(solver());
    var_->RemoveValue(value_);
  }
}

std::string DiffCst::DebugString() const {
  return absl::StrFormat("(%s != %d)", var_->DebugString(), value_);
}

bool DiffCst::HasLargeDomain(IntVar* var) {
  return CapSub(var->Max(), var->Min()) > 0xFFFFFF;
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
  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    target_var_->WhenBound(demon_);
  }
  void InitialPropagate() override {
    bool inhibit = var_->Bound();
    int64 u = var_->Contains(cst_);
    int64 l = inhibit ? u : 0;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      if (target_var_->Min() == 0) {
        if (var_->Size() <= 0xFFFFFF) {
          var_->RemoveValue(cst_);
          inhibit = true;
        }
      } else {
        var_->SetValue(cst_);
        inhibit = true;
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }
  std::string DebugString() const override {
    return absl::StrFormat("IsEqualCstCt(%s, %d, %s)", var_->DebugString(),
                           cst_, target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  if (CapSub(var->Max(), var->Min()) == 1) {
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
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s == %d)", var->DebugString(), value));
    AddConstraint(MakeIsEqualCstCt(var, value, boolvar));
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCstCt(IntExpr* const var, int64 value,
                                     IntVar* const boolvar) {
  CHECK_EQ(this, var->solver());
  CHECK_EQ(this, boolvar->solver());
  if (value == var->Min()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
      return MakeEquality(MakeDifference(value + 1, var), boolvar);
    }
    return MakeIsLessOrEqualCstCt(var, value, boolvar);
  }
  if (value == var->Max()) {
    if (CapSub(var->Max(), var->Min()) == 1) {
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

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenDomain(demon_);
    target_var_->WhenBound(demon_);
  }

  void InitialPropagate() override {
    bool inhibit = var_->Bound();
    int64 l = 1 - var_->Contains(cst_);
    int64 u = inhibit ? l : 1;
    target_var_->SetRange(l, u);
    if (target_var_->Bound()) {
      if (target_var_->Min() == 1) {
        if (var_->Size() <= 0xFFFFFF) {
          var_->RemoveValue(cst_);
          inhibit = true;
        }
      } else {
        var_->SetValue(cst_);
        inhibit = true;
      }
    }
    if (inhibit) {
      demon_->inhibit(solver());
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("IsDiffCstCt(%s, %d, %s)", var_->DebugString(), cst_,
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
    return MakeEquality(boolvar, int64{1});
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
  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }
  void InitialPropagate() override {
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
  std::string DebugString() const override {
    return absl::StrFormat("IsGreaterEqualCstCt(%s, %d, %s)",
                           expr_->DebugString(), cst_,
                           target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
    return MakeIntConst(int64{1});
  }
  if (var->Max() < value) {
    return MakeIntConst(int64{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsGreaterOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s >= %d)", var->DebugString(), value));
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

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return absl::StrFormat("IsLessEqualCstCt(%s, %d, %s)", expr_->DebugString(),
                           cst_, target_var_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
    return MakeIntConst(int64{1});
  }
  if (var->Min() > value) {
    return MakeIntConst(int64{0});
  }
  if (var->IsVar()) {
    return var->Var()->IsLessOrEqual(value);
  } else {
    IntVar* const boolvar =
        MakeBoolVar(absl::StrFormat("Is(%s <= %d)", var->DebugString(), value));
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
      : Constraint(s), expr_(v), min_(l), max_(u), demon_(nullptr) {}

  void Post() override {
    if (!expr_->IsVar()) {
      demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
      expr_->WhenRange(demon_);
    }
  }

  void InitialPropagate() override {
    expr_->SetRange(min_, max_);
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (demon_ != nullptr && emin >= min_ && emax <= max_) {
      demon_->inhibit(solver());
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("BetweenCt(%s, %d, %d)", expr_->DebugString(), min_,
                           max_);
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  Demon* demon_;
};

// ----- NonMember constraint -----

class NotBetweenCt : public Constraint {
 public:
  NotBetweenCt(Solver* const s, IntExpr* const v, int64 l, int64 u)
      : Constraint(s), expr_(v), min_(l), max_(u), demon_(nullptr) {}

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
  }

  void InitialPropagate() override {
    int64 emin = 0;
    int64 emax = 0;
    expr_->Range(&emin, &emax);
    if (emin >= min_) {
      expr_->SetMin(max_ + 1);
    } else if (emax <= max_) {
      expr_->SetMax(min_ - 1);
    }

    if (!expr_->IsVar() && (emax < min_ || emin > max_)) {
      demon_->inhibit(solver());
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("NotBetweenCt(%s, %d, %d)", expr_->DebugString(),
                           min_, max_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNotBetween, this);
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
  Demon* demon_;
};

int64 ExtractExprProductCoeff(IntExpr** expr) {
  int64 prod = 1;
  int64 coeff = 1;
  while ((*expr)->solver()->IsProduct(*expr, expr, &coeff)) prod *= coeff;
  return prod;
}
}  // namespace

Constraint* Solver::MakeBetweenCt(IntExpr* expr, int64 l, int64 u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeFalseConstraint();
    return MakeEquality(expr, l);
  }
  int64 emin = 0;
  int64 emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeFalseConstraint();
  if (emin >= l && emax <= u) return MakeTrueConstraint();
  // Catch one-sided constraints.
  if (emax <= u) return MakeGreaterOrEqual(expr, l);
  if (emin >= l) return MakeLessOrEqual(expr, u);
  // Simplify the common factor, if any.
  int64 coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff));
  } else {
    // No further reduction is possible.
    return RevAlloc(new BetweenCt(this, expr, l, u));
  }
}

Constraint* Solver::MakeNotBetweenCt(IntExpr* expr, int64 l, int64 u) {
  DCHECK_EQ(this, expr->solver());
  // Catch empty interval.
  if (l > u) {
    return MakeTrueConstraint();
  }

  int64 emin = 0;
  int64 emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeTrueConstraint();
  if (emin >= l && emax <= u) return MakeFalseConstraint();
  // Catch one-sided constraints.
  if (emin >= l) return MakeGreater(expr, u);
  if (emax <= u) return MakeLess(expr, l);
  // TODO(user): Add back simplification code if expr is constant *
  // other_expr.
  return RevAlloc(new NotBetweenCt(this, expr, l, u));
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

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
    boolvar_->WhenBound(demon_);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return absl::StrFormat("IsBetweenCt(%s, %d, %d, %s)", expr_->DebugString(),
                           min_, max_, boolvar_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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

Constraint* Solver::MakeIsBetweenCt(IntExpr* expr, int64 l, int64 u,
                                    IntVar* const b) {
  CHECK_EQ(this, expr->solver());
  CHECK_EQ(this, b->solver());
  // Catch empty and singleton intervals.
  if (l >= u) {
    if (l > u) return MakeEquality(b, Zero());
    return MakeIsEqualCstCt(expr, l, b);
  }
  int64 emin = 0;
  int64 emax = 0;
  expr->Range(&emin, &emax);
  // Catch the trivial cases first.
  if (emax < l || emin > u) return MakeEquality(b, Zero());
  if (emin >= l && emax <= u) return MakeEquality(b, 1);
  // Catch one-sided constraints.
  if (emax <= u) return MakeIsGreaterOrEqualCstCt(expr, l, b);
  if (emin >= l) return MakeIsLessOrEqualCstCt(expr, u, b);
  // Simplify the common factor, if any.
  int64 coeff = ExtractExprProductCoeff(&expr);
  if (coeff != 1) {
    CHECK_NE(coeff, 0);  // Would have been caught by the trivial cases already.
    if (coeff < 0) {
      std::swap(u, l);
      u = -u;
      l = -l;
      coeff = -coeff;
    }
    return MakeIsBetweenCt(expr, PosIntDivUp(l, coeff), PosIntDivDown(u, coeff),
                           b);
  } else {
    // No further reduction is possible.
    return RevAlloc(new IsBetweenCt(this, expr, l, u, b));
  }
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
  MemberCt(Solver* const s, IntVar* const v,
           const std::vector<int64>& sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != nullptr);
    DCHECK(s != nullptr);
  }

  void Post() override {}

  void InitialPropagate() override { var_->SetValues(values_); }

  std::string DebugString() const override {
    return absl::StrFormat("Member(%s, %s)", var_->DebugString(),
                           absl::StrJoin(values_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  NotMemberCt(Solver* const s, IntVar* const v,
              const std::vector<int64>& sorted_values)
      : Constraint(s), var_(v), values_(sorted_values) {
    DCHECK(v != nullptr);
    DCHECK(s != nullptr);
  }

  void Post() override {}

  void InitialPropagate() override { var_->RemoveValues(values_); }

  std::string DebugString() const override {
    return absl::StrFormat("NotMember(%s, %s)", var_->DebugString(),
                           absl::StrJoin(values_, ", "));
  }

  void Accept(ModelVisitor* const visitor) const override {
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

Constraint* Solver::MakeMemberCt(IntExpr* expr,
                                 const std::vector<int64>& values) {
  const int64 coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return std::find(values.begin(), values.end(), 0) == values.end()
               ? MakeFalseConstraint()
               : MakeTrueConstraint();
  }
  std::vector<int64> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64 v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64 emin;
  int64 emax;
  expr->Range(&emin, &emax);
  for (const int64 v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeFalseConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    // Note: MakeBetweenCt() has a fast-track for trivially true constraints.
    return MakeBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // NotMemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to list the values *not* allowed.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64 v : copied_values) is_among_input_values[v - emin] = true;
    // We use the zero valued indices of is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64 v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeNonEquality(expr, copied_values[0]);
    }
    return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use MemberCt. No further reduction is possible.
  return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeMemberCt(IntExpr* const expr,
                                 const std::vector<int>& values) {
  return MakeMemberCt(expr, ToInt64Vector(values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    const std::vector<int64>& values) {
  const int64 coeff = ExtractExprProductCoeff(&expr);
  if (coeff == 0) {
    return std::find(values.begin(), values.end(), 0) == values.end()
               ? MakeTrueConstraint()
               : MakeFalseConstraint();
  }
  std::vector<int64> copied_values = values;
  // If the expression is a non-trivial product, we filter out the values that
  // aren't multiples of "coeff", and divide them.
  if (coeff != 1) {
    int num_kept = 0;
    for (const int64 v : copied_values) {
      if (v % coeff == 0) copied_values[num_kept++] = v / coeff;
    }
    copied_values.resize(num_kept);
  }
  // Filter out the values that are outside the [Min, Max] interval.
  int num_kept = 0;
  int64 emin;
  int64 emax;
  expr->Range(&emin, &emax);
  for (const int64 v : copied_values) {
    if (v >= emin && v <= emax) copied_values[num_kept++] = v;
  }
  copied_values.resize(num_kept);
  // Catch empty set.
  if (copied_values.empty()) return MakeTrueConstraint();
  // Sort and remove duplicates.
  gtl::STLSortAndRemoveDuplicates(&copied_values);
  // Special case for singleton.
  if (copied_values.size() == 1) return MakeNonEquality(expr, copied_values[0]);
  // Catch contiguous intervals.
  if (copied_values.size() ==
      copied_values.back() - copied_values.front() + 1) {
    return MakeNotBetweenCt(expr, copied_values.front(), copied_values.back());
  }
  // If the set of values in [expr.Min(), expr.Max()] that are *not* in
  // "values" is smaller than "values", then it's more efficient to use
  // MemberCt. Catch that case here.
  if (emax - emin < 2 * copied_values.size()) {
    // Convert "copied_values" to a dense boolean vector.
    std::vector<bool> is_among_input_values(emax - emin + 1, false);
    for (const int64 v : copied_values) is_among_input_values[v - emin] = true;
    // Use zero valued indices for is_among_input_values to build the
    // complement of copied_values.
    copied_values.clear();
    for (int64 v_off = 0; v_off < is_among_input_values.size(); ++v_off) {
      if (!is_among_input_values[v_off]) copied_values.push_back(v_off + emin);
    }
    // The empty' case (all values in range [expr.Min(), expr.Max()] are in the
    // "values" input) was caught earlier, by the "contiguous interval" case.
    DCHECK_GE(copied_values.size(), 1);
    if (copied_values.size() == 1) {
      return MakeEquality(expr, copied_values[0]);
    }
    return RevAlloc(new MemberCt(this, expr->Var(), copied_values));
  }
  // Otherwise, just use NotMemberCt. No further reduction is possible.
  return RevAlloc(new NotMemberCt(this, expr->Var(), copied_values));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* const expr,
                                    const std::vector<int>& values) {
  return MakeNotMemberCt(expr, ToInt64Vector(values));
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
    while (gtl::ContainsKey(values_as_set_, neg_support_)) {
      neg_support_++;
    }
  }

  void Post() override {
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

  void InitialPropagate() override {
    boolvar_->SetRange(0, 1);
    if (boolvar_->Bound()) {
      TargetBound();
    } else {
      VarDomain();
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("IsMemberCt(%s, %s, %s)", var_->DebugString(),
                           absl::StrJoin(values_, ", "),
                           boolvar_->DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
              if (!gtl::ContainsKey(values_as_set_, value)) {
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
  absl::flat_hash_set<int64> values_as_set_;
  std::vector<int64> values_;
  IntVar* const boolvar_;
  int support_;
  Demon* demon_;
  IntVarIterator* const domain_;
  int64 neg_support_;
};

template <class T>
Constraint* BuildIsMemberCt(Solver* const solver, IntExpr* const expr,
                            const std::vector<T>& values,
                            IntVar* const boolvar) {
  // TODO(user): optimize this by copying the code from MakeMemberCt.
  // Simplify and filter if expr is a product.
  IntExpr* sub = nullptr;
  int64 coef = 1;
  if (solver->IsProduct(expr, &sub, &coef) && coef != 0 && coef != 1) {
    std::vector<int64> new_values;
    new_values.reserve(values.size());
    for (const int64 value : values) {
      if (value % coef == 0) {
        new_values.push_back(value / coef);
      }
    }
    return BuildIsMemberCt(solver, sub, new_values, boolvar);
  }

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

namespace {
class SortedDisjointForbiddenIntervalsConstraint : public Constraint {
 public:
  SortedDisjointForbiddenIntervalsConstraint(
      Solver* const solver, IntVar* const var,
      SortedDisjointIntervalList intervals)
      : Constraint(solver), var_(var), intervals_(std::move(intervals)) {}

  ~SortedDisjointForbiddenIntervalsConstraint() override {}

  void Post() override {
    Demon* const demon = solver()->MakeConstraintInitialPropagateCallback(this);
    var_->WhenRange(demon);
  }

  void InitialPropagate() override {
    const int64 vmin = var_->Min();
    const int64 vmax = var_->Max();
    const auto first_interval_it = intervals_.FirstIntervalGreaterOrEqual(vmin);
    if (first_interval_it == intervals_.end()) {
      // No interval intersects the variable's range. Nothing to do.
      return;
    }
    const auto last_interval_it = intervals_.LastIntervalLessOrEqual(vmax);
    if (last_interval_it == intervals_.end()) {
      // No interval intersects the variable's range. Nothing to do.
      return;
    }
    // TODO(user): Quick fail if first_interval_it == last_interval_it, which
    // would imply that the interval contains the entire range of the variable?
    if (vmin >= first_interval_it->start) {
      // The variable's minimum is inside a forbidden interval. Move it to the
      // interval's end.
      var_->SetMin(CapAdd(first_interval_it->end, 1));
    }
    if (vmax <= last_interval_it->end) {
      // Ditto, on the other side.
      var_->SetMax(CapSub(last_interval_it->start, 1));
    }
  }

  std::string DebugString() const override {
    return absl::StrFormat("ForbiddenIntervalCt(%s, %s)", var_->DebugString(),
                           intervals_.DebugString());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNotMember, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            var_);
    std::vector<int64> starts;
    std::vector<int64> ends;
    for (auto& interval : intervals_) {
      starts.push_back(interval.start);
      ends.push_back(interval.end);
    }
    visitor->VisitIntegerArrayArgument(ModelVisitor::kStartsArgument, starts);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kEndsArgument, ends);
    visitor->EndVisitConstraint(ModelVisitor::kNotMember, this);
  }

 private:
  IntVar* const var_;
  const SortedDisjointIntervalList intervals_;
};
}  // namespace

Constraint* Solver::MakeNotMemberCt(IntExpr* const expr,
                                    std::vector<int64> starts,
                                    std::vector<int64> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* const expr,
                                    std::vector<int> starts,
                                    std::vector<int> ends) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), {starts, ends}));
}

Constraint* Solver::MakeNotMemberCt(IntExpr* expr,
                                    SortedDisjointIntervalList intervals) {
  return RevAlloc(new SortedDisjointForbiddenIntervalsConstraint(
      this, expr->Var(), std::move(intervals)));
}
}  // namespace operations_research
