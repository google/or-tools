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

#include "ortools/constraint_solver/expr_cst.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/expressions.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(int, cache_initial_size, 1024,
          "Initial size of the array of the hash "
          "table of caches for objects of type Var(x == 3)");

namespace operations_research {

//-----------------------------------------------------------------------------
// Equality

EqualityExprCst::EqualityExprCst(Solver* s, IntExpr* e, int64_t v)
    : Constraint(s), expr_(e), value_(v) {}

EqualityExprCst::~EqualityExprCst() {}

void EqualityExprCst::Post() {
  if (!expr_->IsVar()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(d);
  }
}

void EqualityExprCst::InitialPropagate() { expr_->SetValue(value_); }

IntVar* EqualityExprCst::Var() {
  return solver()->MakeIsEqualCstVar(expr_->Var(), value_);
}

std::string EqualityExprCst::DebugString() const {
  return absl::StrFormat("(%s == %d)", expr_->DebugString(), value_);
}

void EqualityExprCst::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
}

//-----------------------------------------------------------------------------
// Greater or equal constraint

GreaterEqExprCst::GreaterEqExprCst(Solver* s, IntExpr* e, int64_t v)
    : Constraint(s), expr_(e), value_(v), demon_(nullptr) {}

GreaterEqExprCst::~GreaterEqExprCst() {}

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

IntVar* GreaterEqExprCst::Var() {
  return solver()->MakeIsGreaterOrEqualCstVar(expr_->Var(), value_);
}

void GreaterEqExprCst::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
}

//-----------------------------------------------------------------------------
// Less or equal constraint

LessEqExprCst::LessEqExprCst(Solver* s, IntExpr* e, int64_t v)
    : Constraint(s), expr_(e), value_(v), demon_(nullptr) {}

LessEqExprCst::~LessEqExprCst() {}

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

IntVar* LessEqExprCst::Var() {
  return solver()->MakeIsLessOrEqualCstVar(expr_->Var(), value_);
}

void LessEqExprCst::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
}

//-----------------------------------------------------------------------------
// Different constraints

DiffCst::DiffCst(Solver* s, IntVar* var, int64_t value)
    : Constraint(s), var_(var), value_(value), demon_(nullptr) {}

DiffCst::~DiffCst() {}

void DiffCst::Post() {}

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
  const int64_t var_min = var_->Min();
  const int64_t var_max = var_->Max();
  if (var_min > value_ || var_max < value_) {
    demon_->inhibit(solver());
  } else if (var_min == value_) {
    var_->SetMin(CapAdd(value_, 1));
  } else if (var_max == value_) {
    var_->SetMax(CapSub(value_, 1));
  } else if (!HasLargeDomain(var_)) {
    demon_->inhibit(solver());
    var_->RemoveValue(value_);
  }
}

std::string DiffCst::DebugString() const {
  return absl::StrFormat("(%s != %d)", var_->DebugString(), value_);
}

IntVar* DiffCst::Var() { return solver()->MakeIsDifferentCstVar(var_, value_); }

void DiffCst::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kNonEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitConstraint(ModelVisitor::kNonEqual, this);
}

bool DiffCst::HasLargeDomain(IntVar* var) {
  return CapSub(var->Max(), var->Min()) > 0xFFFFFF;
}

// ----- is_equal_cst Constraint -----

IsEqualCstCt::IsEqualCstCt(Solver* s, IntVar* v, int64_t c, IntVar* b)
    : CastConstraint(s, b), var_(v), cst_(c), demon_(nullptr) {}

void IsEqualCstCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  var_->WhenDomain(demon_);
  target_var_->WhenBound(demon_);
}

void IsEqualCstCt::InitialPropagate() {
  bool inhibit = var_->Bound();
  int64_t u = var_->Contains(cst_);
  int64_t l = inhibit ? u : 0;
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

std::string IsEqualCstCt::DebugString() const {
  return absl::StrFormat("IsEqualCstCt(%s, %d, %s)", var_->DebugString(), cst_,
                         target_var_->DebugString());
}

void IsEqualCstCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsEqual, this);
}

// ----- is_diff_cst Constraint -----

IsDiffCstCt::IsDiffCstCt(Solver* s, IntVar* v, int64_t c, IntVar* b)
    : CastConstraint(s, b), var_(v), cst_(c), demon_(nullptr) {}

void IsDiffCstCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  var_->WhenDomain(demon_);
  target_var_->WhenBound(demon_);
}

void IsDiffCstCt::InitialPropagate() {
  bool inhibit = var_->Bound();
  int64_t l = 1 - var_->Contains(cst_);
  int64_t u = inhibit ? l : 1;
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

std::string IsDiffCstCt::DebugString() const {
  return absl::StrFormat("IsDiffCstCt(%s, %d, %s)", var_->DebugString(), cst_,
                         target_var_->DebugString());
}

void IsDiffCstCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsDifferent, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsDifferent, this);
}

// ----- is_greater_equal_cst Constraint -----

IsGreaterEqualCstCt::IsGreaterEqualCstCt(Solver* s, IntExpr* v, int64_t c,
                                         IntVar* b)
    : CastConstraint(s, b), expr_(v), cst_(c), demon_(nullptr) {}

void IsGreaterEqualCstCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  expr_->WhenRange(demon_);
  target_var_->WhenBound(demon_);
}

void IsGreaterEqualCstCt::InitialPropagate() {
  bool inhibit = false;
  int64_t u = expr_->Max() >= cst_;
  int64_t l = expr_->Min() >= cst_;
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

std::string IsGreaterEqualCstCt::DebugString() const {
  return absl::StrFormat("IsGreaterEqualCstCt(%s, %d, %s)",
                         expr_->DebugString(), cst_,
                         target_var_->DebugString());
}

void IsGreaterEqualCstCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsGreaterOrEqual, this);
}

// ----- is_lesser_equal_cst Constraint -----

IsLessEqualCstCt::IsLessEqualCstCt(Solver* s, IntExpr* v, int64_t c, IntVar* b)
    : CastConstraint(s, b), expr_(v), cst_(c), demon_(nullptr) {}

void IsLessEqualCstCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  expr_->WhenRange(demon_);
  target_var_->WhenBound(demon_);
}

void IsLessEqualCstCt::InitialPropagate() {
  bool inhibit = false;
  int64_t u = expr_->Min() <= cst_;
  int64_t l = expr_->Max() <= cst_;
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

std::string IsLessEqualCstCt::DebugString() const {
  return absl::StrFormat("IsLessEqualCstCt(%s, %d, %s)", expr_->DebugString(),
                         cst_, target_var_->DebugString());
}

void IsLessEqualCstCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, cst_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
}

// ----- BetweenCt -----

BetweenCt::BetweenCt(Solver* s, IntExpr* v, int64_t l, int64_t u)
    : Constraint(s), expr_(v), min_(l), max_(u), demon_(nullptr) {}

void BetweenCt::Post() {
  if (!expr_->IsVar()) {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    expr_->WhenRange(demon_);
  }
}

void BetweenCt::InitialPropagate() {
  expr_->SetRange(min_, max_);
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  if (demon_ != nullptr && emin >= min_ && emax <= max_) {
    demon_->inhibit(solver());
  }
}

std::string BetweenCt::DebugString() const {
  return absl::StrFormat("BetweenCt(%s, %d, %d)", expr_->DebugString(), min_,
                         max_);
}

void BetweenCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kBetween, this);
  visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
  visitor->EndVisitConstraint(ModelVisitor::kBetween, this);
}

// ----- NonMember constraint -----

NotBetweenCt::NotBetweenCt(Solver* s, IntExpr* v, int64_t l, int64_t u)
    : Constraint(s), expr_(v), min_(l), max_(u), demon_(nullptr) {}

void NotBetweenCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  expr_->WhenRange(demon_);
}

void NotBetweenCt::InitialPropagate() {
  int64_t emin = 0;
  int64_t emax = 0;
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

std::string NotBetweenCt::DebugString() const {
  return absl::StrFormat("NotBetweenCt(%s, %d, %d)", expr_->DebugString(), min_,
                         max_);
}

void NotBetweenCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kNotBetween, this);
  visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
  visitor->EndVisitConstraint(ModelVisitor::kBetween, this);
}

int64_t ExtractExprProductCoeff(IntExpr** expr) {
  int64_t prod = 1;
  int64_t coeff = 1;

  while (IsExprProduct(*expr, expr, &coeff)) prod *= coeff;
  return prod;
}

// ----- is_between_cst Constraint -----

IsBetweenCt::IsBetweenCt(Solver* s, IntExpr* e, int64_t l, int64_t u, IntVar* b)
    : Constraint(s), expr_(e), min_(l), max_(u), boolvar_(b), demon_(nullptr) {}

void IsBetweenCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  expr_->WhenRange(demon_);
  boolvar_->WhenBound(demon_);
}

void IsBetweenCt::InitialPropagate() {
  bool inhibit = false;
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  int64_t u = 1 - (emin > max_ || emax < min_);
  int64_t l = emax <= max_ && emin >= min_;
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

std::string IsBetweenCt::DebugString() const {
  return absl::StrFormat("IsBetweenCt(%s, %d, %d, %s)", expr_->DebugString(),
                         min_, max_, boolvar_->DebugString());
}

void IsBetweenCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsBetween, this);
  visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, min_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, max_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          boolvar_);
  visitor->EndVisitConstraint(ModelVisitor::kIsBetween, this);
}

// ---------- Member ----------

// ----- Member(IntVar, IntSet) -----

MemberCt::MemberCt(Solver* s, IntVar* v,
                   const std::vector<int64_t>& sorted_values)
    : Constraint(s), var_(v), values_(sorted_values) {
  DCHECK(v != nullptr);
  DCHECK(s != nullptr);
}

void MemberCt::Post() {}

void MemberCt::InitialPropagate() { var_->SetValues(values_); }

std::string MemberCt::DebugString() const {
  return absl::StrFormat("Member(%s, %s)", var_->DebugString(),
                         absl::StrJoin(values_, ", "));
}

void MemberCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMember, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->EndVisitConstraint(ModelVisitor::kMember, this);
}

NotMemberCt::NotMemberCt(Solver* s, IntVar* v,
                         const std::vector<int64_t>& sorted_values)
    : Constraint(s), var_(v), values_(sorted_values) {
  DCHECK(v != nullptr);
  DCHECK(s != nullptr);
}

void NotMemberCt::Post() {}

void NotMemberCt::InitialPropagate() { var_->RemoveValues(values_); }

std::string NotMemberCt::DebugString() const {
  return absl::StrFormat("NotMember(%s, %s)", var_->DebugString(),
                         absl::StrJoin(values_, ", "));
}

void NotMemberCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kMember, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->EndVisitConstraint(ModelVisitor::kMember, this);
}

// ----- IsMemberCt -----

IsMemberCt::IsMemberCt(Solver* s, IntVar* v,
                       const std::vector<int64_t>& sorted_values, IntVar* b)
    : Constraint(s),
      var_(v),
      values_as_set_(sorted_values.begin(), sorted_values.end()),
      values_(sorted_values),
      boolvar_(b),
      support_(0),
      demon_(nullptr),
      domain_(var_->MakeDomainIterator(true)),
      neg_support_(std::numeric_limits<int64_t>::min()) {
  DCHECK(v != nullptr);
  DCHECK(s != nullptr);
  DCHECK(b != nullptr);
  while (values_as_set_.contains(neg_support_)) {
    neg_support_++;
  }
}

void IsMemberCt::Post() {
  demon_ =
      MakeConstraintDemon0(solver(), this, &IsMemberCt::VarDomain, "VarDomain");
  if (!var_->Bound()) {
    var_->WhenDomain(demon_);
  }
  if (!boolvar_->Bound()) {
    Demon* const bdemon = MakeConstraintDemon0(
        solver(), this, &IsMemberCt::TargetBound, "TargetBound");
    boolvar_->WhenBound(bdemon);
  }
}

void IsMemberCt::InitialPropagate() {
  boolvar_->SetRange(0, 1);
  if (boolvar_->Bound()) {
    TargetBound();
  } else {
    VarDomain();
  }
}

std::string IsMemberCt::DebugString() const {
  return absl::StrFormat("IsMemberCt(%s, %s, %s)", var_->DebugString(),
                         absl::StrJoin(values_, ", "), boolvar_->DebugString());
}

void IsMemberCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsMember, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          boolvar_);
  visitor->EndVisitConstraint(ModelVisitor::kIsMember, this);
}

void IsMemberCt::VarDomain() {
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
          for (const int64_t value : InitAndGetValues(domain_)) {
            if (!values_as_set_.contains(value)) {
              neg_support_ = value;
              return;
            }
          }
        }
        // No negative support, setting bool-var to true.
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

void IsMemberCt::TargetBound() {
  DCHECK(boolvar_->Bound());
  if (boolvar_->Min() == 1LL) {
    demon_->inhibit(solver());
    var_->SetValues(values_);
  } else {
    demon_->inhibit(solver());
    var_->RemoveValues(values_);
  }
}

SortedDisjointForbiddenIntervalsConstraint::
    SortedDisjointForbiddenIntervalsConstraint(
        Solver* solver, IntVar* var, SortedDisjointIntervalList intervals)
    : Constraint(solver), var_(var), intervals_(std::move(intervals)) {}

void SortedDisjointForbiddenIntervalsConstraint::Post() {
  Demon* const demon = solver()->MakeConstraintInitialPropagateCallback(this);
  var_->WhenRange(demon);
}

void SortedDisjointForbiddenIntervalsConstraint::InitialPropagate() {
  const int64_t vmin = var_->Min();
  const int64_t vmax = var_->Max();
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

std::string SortedDisjointForbiddenIntervalsConstraint::DebugString() const {
  return absl::StrFormat("ForbiddenIntervalCt(%s, %s)", var_->DebugString(),
                         intervals_.DebugString());
}

void SortedDisjointForbiddenIntervalsConstraint::Accept(
    ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kNotMember, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  std::vector<int64_t> starts;
  std::vector<int64_t> ends;
  for (auto& interval : intervals_) {
    starts.push_back(interval.start);
    ends.push_back(interval.end);
  }
  visitor->VisitIntegerArrayArgument(ModelVisitor::kStartsArgument, starts);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kEndsArgument, ends);
  visitor->EndVisitConstraint(ModelVisitor::kNotMember, this);
}

}  // namespace operations_research
