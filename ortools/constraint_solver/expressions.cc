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

#include "ortools/constraint_solver/expressions.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"

ABSL_DECLARE_FLAG(bool, cp_disable_expression_optimization);

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355)
#endif

namespace operations_research {

// ---------- BaseIntExpr ---------

IntVar* BaseIntExpr::Var() {
  if (var_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&var_));
    var_ = CastToVar();
  }
  return var_;
}

IntVar* BaseIntExpr::CastToVar() {
  int64_t vmin, vmax;
  Range(&vmin, &vmax);
  IntVar* const var = solver()->MakeIntVar(vmin, vmax);
  LinkVarExpr(solver(), this, var);
  return var;
}

// ----- PlusIntExpr -----

PlusIntExpr::PlusIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t PlusIntExpr::Min() const { return left_->Min() + right_->Min(); }

void PlusIntExpr::SetMin(int64_t m) {
  if (m > left_->Min() + right_->Min()) {
    // Catching potential overflow.
    if (m > right_->Max() + left_->Max()) solver()->Fail();
    left_->SetMin(m - right_->Max());
    right_->SetMin(m - left_->Max());
  }
}

void PlusIntExpr::SetRange(int64_t l, int64_t u) {
  const int64_t left_min = left_->Min();
  const int64_t right_min = right_->Min();
  const int64_t left_max = left_->Max();
  const int64_t right_max = right_->Max();
  if (l > left_min + right_min) {
    // Catching potential overflow.
    if (l > right_max + left_max) solver()->Fail();
    left_->SetMin(l - right_max);
    right_->SetMin(l - left_max);
  }
  if (u < left_max + right_max) {
    // Catching potential overflow.
    if (u < right_min + left_min) solver()->Fail();
    left_->SetMax(u - right_min);
    right_->SetMax(u - left_min);
  }
}

int64_t PlusIntExpr::Max() const { return left_->Max() + right_->Max(); }

void PlusIntExpr::SetMax(int64_t m) {
  if (m < left_->Max() + right_->Max()) {
    // Catching potential overflow.
    if (m < right_->Min() + left_->Min()) solver()->Fail();
    left_->SetMax(m - right_->Min());
    right_->SetMax(m - left_->Min());
  }
}

bool PlusIntExpr::Bound() const { return (left_->Bound() && right_->Bound()); }

void PlusIntExpr::Range(int64_t* mi, int64_t* ma) {
  *mi = left_->Min() + right_->Min();
  *ma = left_->Max() + right_->Max();
}

std::string PlusIntExpr::name() const {
  return absl::StrFormat("(%s + %s)", left_->name(), right_->name());
}

std::string PlusIntExpr::DebugString() const {
  return absl::StrFormat("(%s + %s)", left_->DebugString(),
                         right_->DebugString());
}

void PlusIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void PlusIntExpr::ExpandPlusIntExpr(IntExpr* expr,
                                    std::vector<IntExpr*>* subs) {
  PlusIntExpr* casted = dynamic_cast<PlusIntExpr*>(expr);
  if (casted != nullptr) {
    ExpandPlusIntExpr(casted->left_, subs);
    ExpandPlusIntExpr(casted->right_, subs);
  } else {
    subs->push_back(expr);
  }
}

IntVar* PlusIntExpr::CastToVar() {
  if (dynamic_cast<PlusIntExpr*>(left_) != nullptr ||
      dynamic_cast<PlusIntExpr*>(right_) != nullptr) {
    std::vector<IntExpr*> sub_exprs;
    ExpandPlusIntExpr(left_, &sub_exprs);
    ExpandPlusIntExpr(right_, &sub_exprs);
    if (sub_exprs.size() >= 3) {
      std::vector<IntVar*> sub_vars(sub_exprs.size());
      for (int i = 0; i < sub_exprs.size(); ++i) {
        sub_vars[i] = sub_exprs[i]->Var();
      }
      return solver()->MakeSum(sub_vars)->Var();
    }
  }
  return BaseIntExpr::CastToVar();
}

void PlusIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
}

SafePlusIntExpr::SafePlusIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t SafePlusIntExpr::Min() const {
  return CapAdd(left_->Min(), right_->Min());
}

void SafePlusIntExpr::SetMin(int64_t m) {
  left_->SetMin(CapSub(m, right_->Max()));
  right_->SetMin(CapSub(m, left_->Max()));
}

void SafePlusIntExpr::SetRange(int64_t l, int64_t u) {
  const int64_t left_min = left_->Min();
  const int64_t right_min = right_->Min();
  const int64_t left_max = left_->Max();
  const int64_t right_max = right_->Max();
  if (l > CapAdd(left_min, right_min)) {
    left_->SetMin(CapSub(l, right_max));
    right_->SetMin(CapSub(l, left_max));
  }
  if (u < CapAdd(left_max, right_max)) {
    left_->SetMax(CapSub(u, right_min));
    right_->SetMax(CapSub(u, left_min));
  }
}

int64_t SafePlusIntExpr::Max() const {
  return CapAdd(left_->Max(), right_->Max());
}

void SafePlusIntExpr::SetMax(int64_t m) {
  left_->SetMax(CapSub(m, right_->Min()));
  right_->SetMax(CapSub(m, left_->Min()));
}

bool SafePlusIntExpr::Bound() const {
  return (left_->Bound() && right_->Bound());
}

std::string SafePlusIntExpr::name() const {
  return absl::StrFormat("(%s + %s)", left_->name(), right_->name());
}

std::string SafePlusIntExpr::DebugString() const {
  return absl::StrFormat("(%s + %s)", left_->DebugString(),
                         right_->DebugString());
}

void SafePlusIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void SafePlusIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
}

// ----- PlusIntCstExpr -----

PlusIntCstExpr::PlusIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {}

int64_t PlusIntCstExpr::Min() const { return CapAdd(expr_->Min(), value_); }

void PlusIntCstExpr::SetMin(int64_t m) { expr_->SetMin(CapSub(m, value_)); }

int64_t PlusIntCstExpr::Max() const { return CapAdd(expr_->Max(), value_); }

void PlusIntCstExpr::SetMax(int64_t m) { expr_->SetMax(CapSub(m, value_)); }

bool PlusIntCstExpr::Bound() const { return (expr_->Bound()); }

std::string PlusIntCstExpr::name() const {
  return absl::StrFormat("(%s + %d)", expr_->name(), value_);
}

std::string PlusIntCstExpr::DebugString() const {
  return absl::StrFormat("(%s + %d)", expr_->DebugString(), value_);
}

void PlusIntCstExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void PlusIntCstExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSum, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSum, this);
}

IntVar* PlusIntCstExpr::CastToVar() {
  Solver* const s = solver();
  IntVar* const var = expr_->Var();
  if (AddOverflows(value_, expr_->Max()) ||
      AddOverflows(value_, expr_->Min())) {
    return BaseIntExpr::CastToVar();
  }
  return NewVarPlusInt(s, var, value_);
}

// ----- SubIntExpr -----

SubIntExpr::SubIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t SubIntExpr::Min() const { return left_->Min() - right_->Max(); }

void SubIntExpr::SetMin(int64_t m) {
  left_->SetMin(CapAdd(m, right_->Min()));
  right_->SetMax(CapSub(left_->Max(), m));
}

int64_t SubIntExpr::Max() const { return left_->Max() - right_->Min(); }

void SubIntExpr::SetMax(int64_t m) {
  left_->SetMax(CapAdd(m, right_->Max()));
  right_->SetMin(CapSub(left_->Min(), m));
}

void SubIntExpr::Range(int64_t* mi, int64_t* ma) {
  *mi = left_->Min() - right_->Max();
  *ma = left_->Max() - right_->Min();
}

void SubIntExpr::SetRange(int64_t l, int64_t u) {
  const int64_t left_min = left_->Min();
  const int64_t right_min = right_->Min();
  const int64_t left_max = left_->Max();
  const int64_t right_max = right_->Max();
  if (l > left_min - right_max) {
    left_->SetMin(CapAdd(l, right_min));
    right_->SetMax(CapSub(left_max, l));
  }
  if (u < left_max - right_min) {
    left_->SetMax(CapAdd(u, right_max));
    right_->SetMin(CapSub(left_min, u));
  }
}

bool SubIntExpr::Bound() const { return (left_->Bound() && right_->Bound()); }

std::string SubIntExpr::name() const {
  return absl::StrFormat("(%s - %s)", left_->name(), right_->name());
}

std::string SubIntExpr::DebugString() const {
  return absl::StrFormat("(%s - %s)", left_->DebugString(),
                         right_->DebugString());
}

void SubIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void SubIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
}

SafeSubIntExpr::SafeSubIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : SubIntExpr(s, l, r) {}

int64_t SafeSubIntExpr::Min() const {
  return CapSub(left_->Min(), right_->Max());
}

void SafeSubIntExpr::SetMin(int64_t m) {
  left_->SetMin(CapAdd(m, right_->Min()));
  right_->SetMax(CapSub(left_->Max(), m));
}

void SafeSubIntExpr::SetRange(int64_t l, int64_t u) {
  const int64_t left_min = left_->Min();
  const int64_t right_min = right_->Min();
  const int64_t left_max = left_->Max();
  const int64_t right_max = right_->Max();
  if (l > CapSub(left_min, right_max)) {
    left_->SetMin(CapAdd(l, right_min));
    right_->SetMax(CapSub(left_max, l));
  }
  if (u < CapSub(left_max, right_min)) {
    left_->SetMax(CapAdd(u, right_max));
    right_->SetMin(CapSub(left_min, u));
  }
}

void SafeSubIntExpr::Range(int64_t* mi, int64_t* ma) {
  *mi = CapSub(left_->Min(), right_->Max());
  *ma = CapSub(left_->Max(), right_->Min());
}

int64_t SafeSubIntExpr::Max() const {
  return CapSub(left_->Max(), right_->Min());
}

void SafeSubIntExpr::SetMax(int64_t m) {
  left_->SetMax(CapAdd(m, right_->Max()));
  right_->SetMin(CapSub(left_->Min(), m));
}

// l - r

// ----- SubIntCstExpr -----

SubIntCstExpr::SubIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {}
int64_t SubIntCstExpr::Min() const { return CapSub(value_, expr_->Max()); }
void SubIntCstExpr::SetMin(int64_t m) { expr_->SetMax(CapSub(value_, m)); }
int64_t SubIntCstExpr::Max() const { return CapSub(value_, expr_->Min()); }
void SubIntCstExpr::SetMax(int64_t m) { expr_->SetMin(CapSub(value_, m)); }
bool SubIntCstExpr::Bound() const { return (expr_->Bound()); }
std::string SubIntCstExpr::name() const {
  return absl::StrFormat("(%d - %s)", value_, expr_->name());
}
std::string SubIntCstExpr::DebugString() const {
  return absl::StrFormat("(%d - %s)", value_, expr_->DebugString());
}
void SubIntCstExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void SubIntCstExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDifference, this);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDifference, this);
}

IntVar* SubIntCstExpr::CastToVar() {
  if (SubOverflows(value_, expr_->Min()) ||
      SubOverflows(value_, expr_->Max())) {
    return BaseIntExpr::CastToVar();
  }
  return NewIntMinusVar(solver(), value_, expr_->Var());
}

// ----- OppIntExpr -----

OppIntExpr::OppIntExpr(Solver* s, IntExpr* e) : BaseIntExpr(s), expr_(e) {}

int64_t OppIntExpr::Min() const { return (CapOpp(expr_->Max())); }
void OppIntExpr::SetMin(int64_t m) { expr_->SetMax(CapOpp(m)); }
int64_t OppIntExpr::Max() const { return (CapOpp(expr_->Min())); }
void OppIntExpr::SetMax(int64_t m) { expr_->SetMin(CapOpp(m)); }
bool OppIntExpr::Bound() const { return (expr_->Bound()); }
std::string OppIntExpr::name() const {
  return absl::StrFormat("(-%s)", expr_->name());
}
std::string OppIntExpr::DebugString() const {
  return absl::StrFormat("(-%s)", expr_->DebugString());
}
void OppIntExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void OppIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kOpposite, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kOpposite, this);
}

IntVar* OppIntExpr::CastToVar() {
  return NewIntMinusVar(solver(), 0, expr_->Var());
}

// ----- TimesIntCstExpr -----

TimesIntCstExpr::TimesIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {}

bool TimesIntCstExpr::Bound() const { return (expr_->Bound()); }

std::string TimesIntCstExpr::name() const {
  return absl::StrFormat("(%s * %d)", expr_->name(), value_);
}

std::string TimesIntCstExpr::DebugString() const {
  return absl::StrFormat("(%s * %d)", expr_->DebugString(), value_);
}

void TimesIntCstExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

IntVar* TimesIntCstExpr::CastToVar() {
  return NewVarTimesInt(solver(), expr_->Var(), value_);
}

void TimesIntCstExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

// ----- TimesPosIntCstExpr -----

TimesPosIntCstExpr::TimesPosIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : TimesIntCstExpr(s, e, v) {
  CHECK_GT(v, 0);
}

int64_t TimesPosIntCstExpr::Min() const { return expr_->Min() * value_; }

void TimesPosIntCstExpr::SetMin(int64_t m) {
  expr_->SetMin(PosIntDivUp(m, value_));
}

int64_t TimesPosIntCstExpr::Max() const { return expr_->Max() * value_; }

void TimesPosIntCstExpr::SetMax(int64_t m) {
  expr_->SetMax(PosIntDivDown(m, value_));
}

// This expressions adds safe arithmetic (w.r.t. overflows) compared
// to the previous one.
SafeTimesPosIntCstExpr::SafeTimesPosIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : TimesIntCstExpr(s, e, v) {
  CHECK_GT(v, 0);
}

int64_t SafeTimesPosIntCstExpr::Min() const {
  return CapProd(expr_->Min(), value_);
}

void SafeTimesPosIntCstExpr::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    expr_->SetMin(PosIntDivUp(m, value_));
  }
}

int64_t SafeTimesPosIntCstExpr::Max() const {
  return CapProd(expr_->Max(), value_);
}

void SafeTimesPosIntCstExpr::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    expr_->SetMax(PosIntDivDown(m, value_));
  }
}

// ----- TimesIntNegCstExpr -----

TimesIntNegCstExpr::TimesIntNegCstExpr(Solver* s, IntExpr* e, int64_t v)
    : TimesIntCstExpr(s, e, v) {
  CHECK_LT(v, 0);
}

int64_t TimesIntNegCstExpr::Min() const {
  return CapProd(expr_->Max(), value_);
}

void TimesIntNegCstExpr::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    expr_->SetMax(PosIntDivDown(-m, -value_));
  }
}

int64_t TimesIntNegCstExpr::Max() const {
  return CapProd(expr_->Min(), value_);
}

void TimesIntNegCstExpr::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    expr_->SetMin(PosIntDivUp(-m, -value_));
  }
}

// ----- Utilities for product expression -----

// Propagates set_min on left * right, left and right >= 0.
void SetPosPosMinExpr(IntExpr* left, IntExpr* right, int64_t m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64_t lmax = left->Max();
  const int64_t rmax = right->Max();
  if (m > CapProd(lmax, rmax)) {
    left->solver()->Fail();
  }
  if (m > CapProd(left->Min(), right->Min())) {
    // Ok for m == 0 due to left and right being positive
    if (0 != rmax) {
      left->SetMin(PosIntDivUp(m, rmax));
    }
    if (0 != lmax) {
      right->SetMin(PosIntDivUp(m, lmax));
    }
  }
}

// Propagates set_max on left * right, left and right >= 0.
void SetPosPosMaxExpr(IntExpr* left, IntExpr* right, int64_t m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GE(right->Min(), 0);
  const int64_t lmin = left->Min();
  const int64_t rmin = right->Min();
  if (m < CapProd(lmin, rmin)) {
    left->solver()->Fail();
  }
  if (m < CapProd(left->Max(), right->Max())) {
    if (0 != lmin) {
      right->SetMax(PosIntDivDown(m, lmin));
    }
    if (0 != rmin) {
      left->SetMax(PosIntDivDown(m, rmin));
    }
    // else do nothing: 0 is supporting any value from other expr.
  }
}

// Propagates set_min on left * right, left >= 0, right across 0.
void SetPosGenMinExpr(IntExpr* left, IntExpr* right, int64_t m) {
  DCHECK_GE(left->Min(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64_t lmax = left->Max();
  const int64_t rmax = right->Max();
  if (m > CapProd(lmax, rmax)) {
    left->solver()->Fail();
  }
  if (left->Max() == 0) {  // left is bound to 0, product is bound to 0.
    DCHECK_EQ(0, left->Min());
    DCHECK_LE(m, 0);
  } else {
    if (m > 0) {  // We deduce right > 0.
      left->SetMin(PosIntDivUp(m, rmax));
      right->SetMin(PosIntDivUp(m, lmax));
    } else if (m == 0) {
      const int64_t lmin = left->Min();
      if (lmin > 0) {
        right->SetMin(0);
      }
    } else {  // m < 0
      const int64_t lmin = left->Min();
      if (0 != lmin) {  // We cannot deduce anything if 0 is in the domain.
        right->SetMin(-PosIntDivDown(-m, lmin));
      }
    }
  }
}

// Propagates set_min on left * right, left and right across 0.
void SetGenGenMinExpr(IntExpr* left, IntExpr* right, int64_t m) {
  DCHECK_LT(left->Min(), 0);
  DCHECK_GT(left->Max(), 0);
  DCHECK_GT(right->Max(), 0);
  DCHECK_LT(right->Min(), 0);
  const int64_t lmin = left->Min();
  const int64_t lmax = left->Max();
  const int64_t rmin = right->Min();
  const int64_t rmax = right->Max();
  if (m > std::max(CapProd(lmin, rmin), CapProd(lmax, rmax))) {
    left->solver()->Fail();
  }
  if (m >
      CapProd(lmin, rmin)) {  // Must be positive section * positive section.
    left->SetMin(PosIntDivUp(m, rmax));
    right->SetMin(PosIntDivUp(m, lmax));
  } else if (m > CapProd(lmax, rmax)) {  // Negative section * negative section.
    left->SetMax(CapOpp(PosIntDivUp(m, CapOpp(rmin))));
    right->SetMax(CapOpp(PosIntDivUp(m, CapOpp(lmin))));
  }
}

void TimesSetMin(IntExpr* left, IntExpr* right, IntExpr* minus_left,
                 IntExpr* minus_right, int64_t m) {
  if (left->Min() >= 0) {
    if (right->Min() >= 0) {
      SetPosPosMinExpr(left, right, m);
    } else if (right->Max() <= 0) {
      SetPosPosMaxExpr(left, minus_right, -m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(left, right, m);
    }
  } else if (left->Max() <= 0) {
    if (right->Min() >= 0) {
      SetPosPosMaxExpr(right, minus_left, -m);
    } else if (right->Max() <= 0) {
      SetPosPosMinExpr(minus_left, minus_right, m);
    } else {  // right->Min() < 0 && right->Max() > 0
      SetPosGenMinExpr(minus_left, minus_right, m);
    }
  } else if (right->Min() >= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(right, left, m);
  } else if (right->Max() <= 0) {  // left->Min() < 0 && left->Max() > 0
    SetPosGenMinExpr(minus_right, minus_left, m);
  } else {  // left->Min() < 0 && left->Max() > 0 &&
            // right->Min() < 0 && right->Max() > 0
    SetGenGenMinExpr(left, right, m);
  }
}

TimesIntExpr::TimesIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s),
      left_(l),
      right_(r),
      minus_left_(s->MakeOpposite(left_)),
      minus_right_(s->MakeOpposite(right_)) {}

int64_t TimesIntExpr::Min() const {
  const int64_t lmin = left_->Min();
  const int64_t lmax = left_->Max();
  const int64_t rmin = right_->Min();
  const int64_t rmax = right_->Max();
  return std::min(std::min(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                  std::min(CapProd(lmax, rmin), CapProd(lmin, rmax)));
}
int64_t TimesIntExpr::Max() const {
  const int64_t lmin = left_->Min();
  const int64_t lmax = left_->Max();
  const int64_t rmin = right_->Min();
  const int64_t rmax = right_->Max();
  return std::max(std::max(CapProd(lmin, rmin), CapProd(lmax, rmax)),
                  std::max(CapProd(lmax, rmin), CapProd(lmin, rmax)));
}
std::string TimesIntExpr::name() const {
  return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
}
std::string TimesIntExpr::DebugString() const {
  return absl::StrFormat("(%s * %s)", left_->DebugString(),
                         right_->DebugString());
}
void TimesIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void TimesIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

void TimesIntExpr::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    TimesSetMin(left_, right_, minus_left_, minus_right_, m);
  }
}

void TimesIntExpr::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    TimesSetMin(left_, minus_right_, minus_left_, right_, CapOpp(m));
  }
}

bool TimesIntExpr::Bound() const {
  const bool left_bound = left_->Bound();
  const bool right_bound = right_->Bound();
  return ((left_bound && left_->Max() == 0) ||
          (right_bound && right_->Max() == 0) || (left_bound && right_bound));
}

// ----- TimesPosIntExpr -----

TimesPosIntExpr::TimesPosIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t TimesPosIntExpr::Min() const { return (left_->Min() * right_->Min()); }
int64_t TimesPosIntExpr::Max() const { return (left_->Max() * right_->Max()); }
std::string TimesPosIntExpr::name() const {
  return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
}
std::string TimesPosIntExpr::DebugString() const {
  return absl::StrFormat("(%s * %s)", left_->DebugString(),
                         right_->DebugString());
}
void TimesPosIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void TimesPosIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

void TimesPosIntExpr::SetMin(int64_t m) { SetPosPosMinExpr(left_, right_, m); }

void TimesPosIntExpr::SetMax(int64_t m) { SetPosPosMaxExpr(left_, right_, m); }

bool TimesPosIntExpr::Bound() const {
  return (left_->Max() == 0 || right_->Max() == 0 ||
          (left_->Bound() && right_->Bound()));
}

// ----- SafeTimesPosIntExpr -----

SafeTimesPosIntExpr::SafeTimesPosIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t SafeTimesPosIntExpr::Min() const {
  return CapProd(left_->Min(), right_->Min());
}
void SafeTimesPosIntExpr::SetMin(int64_t m) {
  if (m != std::numeric_limits<int64_t>::min()) {
    SetPosPosMinExpr(left_, right_, m);
  }
}
int64_t SafeTimesPosIntExpr::Max() const {
  return CapProd(left_->Max(), right_->Max());
}
void SafeTimesPosIntExpr::SetMax(int64_t m) {
  if (m != std::numeric_limits<int64_t>::max()) {
    SetPosPosMaxExpr(left_, right_, m);
  }
}
bool SafeTimesPosIntExpr::Bound() const {
  return (left_->Max() == 0 || right_->Max() == 0 ||
          (left_->Bound() && right_->Bound()));
}
std::string SafeTimesPosIntExpr::name() const {
  return absl::StrFormat("(%s * %s)", left_->name(), right_->name());
}
std::string SafeTimesPosIntExpr::DebugString() const {
  return absl::StrFormat("(%s * %s)", left_->DebugString(),
                         right_->DebugString());
}
void SafeTimesPosIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void SafeTimesPosIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

// ----- TimesBooleanPosIntExpr -----

TimesBooleanPosIntExpr::TimesBooleanPosIntExpr(Solver* s, BooleanVar* b,
                                               IntExpr* e)
    : BaseIntExpr(s), boolvar_(b), expr_(e) {}

int64_t TimesBooleanPosIntExpr::Min() const {
  return (boolvar_->RawValue() == 1 ? expr_->Min() : 0);
}
int64_t TimesBooleanPosIntExpr::Max() const {
  return (boolvar_->RawValue() == 0 ? 0 : expr_->Max());
}
std::string TimesBooleanPosIntExpr::name() const {
  return absl::StrFormat("(%s * %s)", boolvar_->name(), expr_->name());
}
std::string TimesBooleanPosIntExpr::DebugString() const {
  return absl::StrFormat("(%s * %s)", boolvar_->DebugString(),
                         expr_->DebugString());
}
void TimesBooleanPosIntExpr::WhenRange(Demon* d) {
  boolvar_->WhenRange(d);
  expr_->WhenRange(d);
}

void TimesBooleanPosIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                          boolvar_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

void TimesBooleanPosIntExpr::SetMin(int64_t m) {
  if (m > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(m);
  }
}

void TimesBooleanPosIntExpr::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->RawValue() == 1) {
    expr_->SetMax(m);
  }
}

void TimesBooleanPosIntExpr::Range(int64_t* mi, int64_t* ma) {
  const int value = boolvar_->RawValue();
  if (value == 0) {
    *mi = 0;
    *ma = 0;
  } else if (value == 1) {
    expr_->Range(mi, ma);
  } else {
    *mi = 0;
    *ma = expr_->Max();
  }
}

void TimesBooleanPosIntExpr::SetRange(int64_t mi, int64_t ma) {
  if (ma < 0 || mi > ma) {
    solver()->Fail();
  }
  if (mi > 0) {
    boolvar_->SetValue(1);
    expr_->SetMin(mi);
  }
  if (ma < expr_->Min()) {
    boolvar_->SetValue(0);
  }
  if (boolvar_->RawValue() == 1) {
    expr_->SetMax(ma);
  }
}

bool TimesBooleanPosIntExpr::Bound() const {
  return (boolvar_->RawValue() == 0 || expr_->Max() == 0 ||
          (boolvar_->RawValue() != BooleanVar::kUnboundBooleanVarValue &&
           expr_->Bound()));
}

// ----- TimesBooleanIntExpr -----

TimesBooleanIntExpr::TimesBooleanIntExpr(Solver* s, BooleanVar* b, IntExpr* e)
    : BaseIntExpr(s), boolvar_(b), expr_(e) {}

int64_t TimesBooleanIntExpr::Min() const {
  switch (boolvar_->RawValue()) {
    case 0: {
      return 0LL;
    }
    case 1: {
      return expr_->Min();
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      return std::min(int64_t{0}, expr_->Min());
    }
  }
}
int64_t TimesBooleanIntExpr::Max() const {
  switch (boolvar_->RawValue()) {
    case 0: {
      return 0LL;
    }
    case 1: {
      return expr_->Max();
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      return std::max(int64_t{0}, expr_->Max());
    }
  }
}
std::string TimesBooleanIntExpr::name() const {
  return absl::StrFormat("(%s * %s)", boolvar_->name(), expr_->name());
}
std::string TimesBooleanIntExpr::DebugString() const {
  return absl::StrFormat("(%s * %s)", boolvar_->DebugString(),
                         expr_->DebugString());
}
void TimesBooleanIntExpr::WhenRange(Demon* d) {
  boolvar_->WhenRange(d);
  expr_->WhenRange(d);
}

void TimesBooleanIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kProduct, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument,
                                          boolvar_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kProduct, this);
}

void TimesBooleanIntExpr::SetMin(int64_t m) {
  switch (boolvar_->RawValue()) {
    case 0: {
      if (m > 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMin(m);
      break;
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      if (m > 0) {  // 0 is no longer possible for boolvar because min > 0.
        boolvar_->SetValue(1);
        expr_->SetMin(m);
      } else if (m <= 0 && expr_->Max() < m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::SetMax(int64_t m) {
  switch (boolvar_->RawValue()) {
    case 0: {
      if (m < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetMax(m);
      break;
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      if (m < 0) {  // 0 is no longer possible for boolvar because max < 0.
        boolvar_->SetValue(1);
        expr_->SetMax(m);
      } else if (m >= 0 && expr_->Min() > m) {
        boolvar_->SetValue(0);
      }
    }
  }
}

void TimesBooleanIntExpr::Range(int64_t* mi, int64_t* ma) {
  switch (boolvar_->RawValue()) {
    case 0: {
      *mi = 0;
      *ma = 0;
      break;
    }
    case 1: {
      *mi = expr_->Min();
      *ma = expr_->Max();
      break;
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      *mi = std::min(int64_t{0}, expr_->Min());
      *ma = std::max(int64_t{0}, expr_->Max());
      break;
    }
  }
}

void TimesBooleanIntExpr::SetRange(int64_t mi, int64_t ma) {
  if (mi > ma) {
    solver()->Fail();
  }
  switch (boolvar_->RawValue()) {
    case 0: {
      if (mi > 0 || ma < 0) {
        solver()->Fail();
      }
      break;
    }
    case 1: {
      expr_->SetRange(mi, ma);
      break;
    }
    default: {
      DCHECK_EQ(BooleanVar::kUnboundBooleanVarValue, boolvar_->RawValue());
      if (mi > 0) {
        boolvar_->SetValue(1);
        expr_->SetMin(mi);
      } else if (mi == 0 && expr_->Max() < 0) {
        boolvar_->SetValue(0);
      }
      if (ma < 0) {
        boolvar_->SetValue(1);
        expr_->SetMax(ma);
      } else if (ma == 0 && expr_->Min() > 0) {
        boolvar_->SetValue(0);
      }
      break;
    }
  }
}

bool TimesBooleanIntExpr::Bound() const {
  return (boolvar_->RawValue() == 0 ||
          (expr_->Bound() &&
           (boolvar_->RawValue() != BooleanVar::kUnboundBooleanVarValue ||
            expr_->Max() == 0)));
}

// ----- DivPosIntCstExpr -----

DivPosIntCstExpr::DivPosIntCstExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {
  CHECK_GE(v, 0);
}

int64_t DivPosIntCstExpr::Min() const { return expr_->Min() / value_; }

void DivPosIntCstExpr::SetMin(int64_t m) {
  if (m > 0) {
    expr_->SetMin(m * value_);
  } else {
    expr_->SetMin((m - 1) * value_ + 1);
  }
}
int64_t DivPosIntCstExpr::Max() const { return expr_->Max() / value_; }

void DivPosIntCstExpr::SetMax(int64_t m) {
  if (m >= 0) {
    expr_->SetMax((m + 1) * value_ - 1);
  } else {
    expr_->SetMax(m * value_);
  }
}

std::string DivPosIntCstExpr::name() const {
  return absl::StrFormat("(%s div %d)", expr_->name(), value_);
}

std::string DivPosIntCstExpr::DebugString() const {
  return absl::StrFormat("(%s div %d)", expr_->DebugString(), value_);
}

void DivPosIntCstExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void DivPosIntCstExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
}

// DivPosIntExpr

namespace {
void DivPosIntExpr_SetPosMin(IntExpr* num, IntExpr* denom, int64_t m) {
  num->SetMin(m * denom->Min());
  denom->SetMax(num->Max() / m);
}

void DivPosIntExpr_SetPosMax(IntExpr* num, IntExpr* denom, int64_t m) {
  num->SetMax((m + 1) * denom->Max() - 1);
  denom->SetMin(num->Min() / (m + 1) + 1);
}
}  // namespace

DivPosIntExpr::DivPosIntExpr(Solver* s, IntExpr* num, IntExpr* denom)
    : BaseIntExpr(s),
      num_(num),
      denom_(denom),
      opp_num_(s->MakeOpposite(num)) {}

int64_t DivPosIntExpr::Min() const {
  return num_->Min() >= 0
             ? num_->Min() / denom_->Max()
             : (denom_->Min() == 0 ? num_->Min() : num_->Min() / denom_->Min());
}

int64_t DivPosIntExpr::Max() const {
  return num_->Max() >= 0
             ? (denom_->Min() == 0 ? num_->Max() : num_->Max() / denom_->Min())
             : num_->Max() / denom_->Max();
}

void DivPosIntExpr::SetMin(int64_t m) {
  if (m > 0) {
    DivPosIntExpr_SetPosMin(num_, denom_, m);
  } else {
    DivPosIntExpr_SetPosMax(opp_num_, denom_, -m);
  }
}

void DivPosIntExpr::SetMax(int64_t m) {
  if (m >= 0) {
    DivPosIntExpr_SetPosMax(num_, denom_, m);
  } else {
    DivPosIntExpr_SetPosMin(opp_num_, denom_, -m);
  }
}

std::string DivPosIntExpr::name() const {
  return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
}
std::string DivPosIntExpr::DebugString() const {
  return absl::StrFormat("(%s div %s)", num_->DebugString(),
                         denom_->DebugString());
}
void DivPosIntExpr::WhenRange(Demon* d) {
  num_->WhenRange(d);
  denom_->WhenRange(d);
}

void DivPosIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, denom_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
}

DivPosPosIntExpr::DivPosPosIntExpr(Solver* s, IntExpr* num, IntExpr* denom)
    : BaseIntExpr(s), num_(num), denom_(denom) {}

int64_t DivPosPosIntExpr::Min() const {
  if (denom_->Max() == 0) {
    solver()->Fail();
  }
  return num_->Min() / denom_->Max();
}

int64_t DivPosPosIntExpr::Max() const {
  if (denom_->Min() == 0) {
    return num_->Max();
  } else {
    return num_->Max() / denom_->Min();
  }
}

void DivPosPosIntExpr::SetMin(int64_t m) {
  if (m > 0) {
    num_->SetMin(m * denom_->Min());
    denom_->SetMax(num_->Max() / m);
  }
}

void DivPosPosIntExpr::SetMax(int64_t m) {
  if (m >= 0) {
    num_->SetMax((m + 1) * denom_->Max() - 1);
    denom_->SetMin(num_->Min() / (m + 1) + 1);
  } else {
    solver()->Fail();
  }
}

std::string DivPosPosIntExpr::name() const {
  return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
}

std::string DivPosPosIntExpr::DebugString() const {
  return absl::StrFormat("(%s div %s)", num_->DebugString(),
                         denom_->DebugString());
}

void DivPosPosIntExpr::WhenRange(Demon* d) {
  num_->WhenRange(d);
  denom_->WhenRange(d);
}

void DivPosPosIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, denom_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
}

// DivIntExpr

namespace {
// m > 0.
void DivIntExpr_SetPosMin(IntExpr* num, IntExpr* denom, int64_t m) {
  DCHECK_GT(m, 0);
  const int64_t num_min = num->Min();
  const int64_t num_max = num->Max();
  const int64_t denom_min = denom->Min();
  const int64_t denom_max = denom->Max();
  DCHECK_NE(denom_min, 0);
  DCHECK_NE(denom_max, 0);
  if (denom_min > 0) {  // Denominator strictly positive.
    num->SetMin(m * denom_min);
    denom->SetMax(num_max / m);
  } else if (denom_max < 0) {  // Denominator strictly negative.
    num->SetMax(m * denom_max);
    denom->SetMin(num_min / m);
  } else {  // Denominator across 0.
    if (num_min >= 0) {
      num->SetMin(m);
      denom->SetRange(1, num_max / m);
    } else if (num_max <= 0) {
      num->SetMax(-m);
      denom->SetRange(num_min / m, -1);
    } else {
      if (m > -num_min) {  // Denominator is forced positive.
        num->SetMin(m);
        denom->SetRange(1, num_max / m);
      } else if (m > num_max) {  // Denominator is forced negative.
        num->SetMax(-m);
        denom->SetRange(num_min / m, -1);
      } else {
        denom->SetRange(num_min / m, num_max / m);
      }
    }
  }
}

// m >= 0.
void DivIntExpr_SetPosMax(IntExpr* num, IntExpr* denom, int64_t m) {
  DCHECK_GE(m, 0);
  const int64_t num_min = num->Min();
  const int64_t num_max = num->Max();
  const int64_t denom_min = denom->Min();
  const int64_t denom_max = denom->Max();
  DCHECK_NE(denom_min, 0);
  DCHECK_NE(denom_max, 0);
  if (denom_min > 0) {  // Denominator strictly positive.
    num->SetMax((m + 1) * denom_max - 1);
    denom->SetMin((num_min / (m + 1)) + 1);
  } else if (denom_max < 0) {
    num->SetMin((m + 1) * denom_min + 1);
    denom->SetMax(num_max / (m + 1) - 1);
  } else if (num_min > (m + 1) * denom_max - 1) {
    denom->SetMax(-1);
  } else if (num_max < (m + 1) * denom_min + 1) {
    denom->SetMin(1);
  }
}
}  // namespace

DivIntExpr::DivIntExpr(Solver* s, IntExpr* num, IntExpr* denom)
    : BaseIntExpr(s),
      num_(num),
      denom_(denom),
      opp_num_(s->MakeOpposite(num)) {}

int64_t DivIntExpr::Min() const {
  const int64_t num_min = num_->Min();
  const int64_t num_max = num_->Max();
  const int64_t denom_min = denom_->Min();
  const int64_t denom_max = denom_->Max();

  if (denom_min == 0 && denom_max == 0) {
    return std::numeric_limits<int64_t>::max();  // TODO(user): Check this
                                                 // convention.
  }

  if (denom_min >= 0) {  // Denominator strictly positive.
    DCHECK_GT(denom_max, 0);
    const int64_t adjusted_denom_min = denom_min == 0 ? 1 : denom_min;
    return num_min >= 0 ? num_min / denom_max : num_min / adjusted_denom_min;
  } else if (denom_max <= 0) {  // Denominator strictly negative.
    DCHECK_LT(denom_min, 0);
    const int64_t adjusted_denom_max = denom_max == 0 ? -1 : denom_max;
    return num_max >= 0 ? num_max / adjusted_denom_max : num_max / denom_min;
  } else {  // Denominator across 0.
    return std::min(num_min, -num_max);
  }
}

int64_t DivIntExpr::Max() const {
  const int64_t num_min = num_->Min();
  const int64_t num_max = num_->Max();
  const int64_t denom_min = denom_->Min();
  const int64_t denom_max = denom_->Max();

  if (denom_min == 0 && denom_max == 0) {
    return std::numeric_limits<int64_t>::min();  // TODO(user): Check this
                                                 // convention.
  }

  if (denom_min >= 0) {  // Denominator strictly positive.
    DCHECK_GT(denom_max, 0);
    const int64_t adjusted_denom_min = denom_min == 0 ? 1 : denom_min;
    return num_max >= 0 ? num_max / adjusted_denom_min : num_max / denom_max;
  } else if (denom_max <= 0) {  // Denominator strictly negative.
    DCHECK_LT(denom_min, 0);
    const int64_t adjusted_denom_max = denom_max == 0 ? -1 : denom_max;
    return num_min >= 0 ? num_min / denom_min : -num_min / -adjusted_denom_max;
  } else {  // Denominator across 0.
    return std::max(num_max, -num_min);
  }
}

void DivIntExpr::AdjustDenominator() {
  if (denom_->Min() == 0) {
    denom_->SetMin(1);
  } else if (denom_->Max() == 0) {
    denom_->SetMax(-1);
  }
}

void DivIntExpr::SetMin(int64_t m) {
  AdjustDenominator();
  if (m > 0) {
    DivIntExpr_SetPosMin(num_, denom_, m);
  } else {
    DivIntExpr_SetPosMax(opp_num_, denom_, -m);
  }
}

void DivIntExpr::SetMax(int64_t m) {
  AdjustDenominator();
  if (m >= 0) {
    DivIntExpr_SetPosMax(num_, denom_, m);
  } else {
    DivIntExpr_SetPosMin(opp_num_, denom_, -m);
  }
}

std::string DivIntExpr::name() const {
  return absl::StrFormat("(%s div %s)", num_->name(), denom_->name());
}
std::string DivIntExpr::DebugString() const {
  return absl::StrFormat("(%s div %s)", num_->DebugString(),
                         denom_->DebugString());
}
void DivIntExpr::WhenRange(Demon* d) {
  num_->WhenRange(d);
  denom_->WhenRange(d);
}

void DivIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kDivide, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, num_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, denom_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kDivide, this);
}

// ----- IntAbs And IntAbsConstraint ------

IntAbsConstraint::IntAbsConstraint(Solver* s, IntVar* sub, IntVar* target)
    : CastConstraint(s, target), sub_(sub) {}

void IntAbsConstraint::Post() {
  Demon* const sub_demon = MakeConstraintDemon0(
      solver(), this, &IntAbsConstraint::PropagateSub, "PropagateSub");
  sub_->WhenRange(sub_demon);
  Demon* const target_demon = MakeConstraintDemon0(
      solver(), this, &IntAbsConstraint::PropagateTarget, "PropagateTarget");
  target_var_->WhenRange(target_demon);
}

void IntAbsConstraint::InitialPropagate() {
  PropagateSub();
  PropagateTarget();
}

void IntAbsConstraint::PropagateSub() {
  const int64_t smin = sub_->Min();
  const int64_t smax = sub_->Max();
  if (smax <= 0) {
    target_var_->SetRange(-smax, -smin);
  } else if (smin >= 0) {
    target_var_->SetRange(smin, smax);
  } else {
    target_var_->SetRange(0, std::max(-smin, smax));
  }
}

void IntAbsConstraint::PropagateTarget() {
  const int64_t target_max = target_var_->Max();
  sub_->SetRange(-target_max, target_max);
  const int64_t target_min = target_var_->Min();
  if (target_min > 0) {
    if (sub_->Min() > -target_min) {
      sub_->SetMin(target_min);
    } else if (sub_->Max() < target_min) {
      sub_->SetMax(-target_min);
    }
  }
}

std::string IntAbsConstraint::DebugString() const {
  return absl::StrFormat("IntAbsConstraint(%s, %s)", sub_->DebugString(),
                         target_var_->DebugString());
}

void IntAbsConstraint::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kAbs, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          sub_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kAbs, this);
}

IntAbs::IntAbs(Solver* s, IntExpr* e) : BaseIntExpr(s), expr_(e) {}

int64_t IntAbs::Min() const {
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  if (emin >= 0) {
    return emin;
  }
  if (emax <= 0) {
    return -emax;
  }
  return 0;
}

void IntAbs::SetMin(int64_t m) {
  if (m > 0) {
    int64_t emin = 0;
    int64_t emax = 0;
    expr_->Range(&emin, &emax);
    if (emin > -m) {
      expr_->SetMin(m);
    } else if (emax < m) {
      expr_->SetMax(-m);
    }
  }
}

int64_t IntAbs::Max() const {
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  return std::max(-emin, emax);
}

void IntAbs::SetMax(int64_t m) { expr_->SetRange(-m, m); }

void IntAbs::Range(int64_t* mi, int64_t* ma) {
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  if (emin >= 0) {
    *mi = emin;
    *ma = emax;
  } else if (emax <= 0) {
    *mi = -emax;
    *ma = -emin;
  } else {
    *mi = 0;
    *ma = std::max(-emin, emax);
  }
}

void IntAbs::SetRange(int64_t mi, int64_t ma) {
  expr_->SetRange(-ma, ma);
  if (mi > 0) {
    int64_t emin = 0;
    int64_t emax = 0;
    expr_->Range(&emin, &emax);
    if (emin > -mi) {
      expr_->SetMin(mi);
    } else if (emax < mi) {
      expr_->SetMax(-mi);
    }
  }
}

bool IntAbs::Bound() const { return expr_->Bound(); }

std::string IntAbs::name() const {
  return absl::StrFormat("Abs(%s)", expr_->name());
}

std::string IntAbs::DebugString() const {
  return absl::StrFormat("Abs(%s)", expr_->DebugString());
}

void IntAbs::WhenRange(Demon* d) { expr_->WhenRange(d); }

void IntAbs::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kAbs, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kAbs, this);
}

IntVar* IntAbs::CastToVar() {
  int64_t min_value = 0;
  int64_t max_value = 0;
  Range(&min_value, &max_value);
  Solver* const s = solver();
  const std::string name = absl::StrFormat("AbsVar(%s)", expr_->name());
  IntVar* const target = s->MakeIntVar(min_value, max_value, name);
  CastConstraint* const ct =
      s->RevAlloc(new IntAbsConstraint(s, expr_->Var(), target));
  s->AddCastConstraint(ct, target, this);
  return target;
}

IntSquare::IntSquare(Solver* s, IntExpr* e) : BaseIntExpr(s), expr_(e) {}

int64_t IntSquare::Min() const {
  const int64_t emin = expr_->Min();
  if (emin >= 0) {
    return emin >= std::numeric_limits<int32_t>::max()
               ? std::numeric_limits<int64_t>::max()
               : emin * emin;
  }
  const int64_t emax = expr_->Max();
  if (emax < 0) {
    return emax <= -std::numeric_limits<int32_t>::max()
               ? std::numeric_limits<int64_t>::max()
               : emax * emax;
  }
  return 0LL;
}

void IntSquare::SetMin(int64_t m) {
  if (m <= 0) {
    return;
  }
  // TODO(user): What happens if m is kint64max?
  const int64_t emin = expr_->Min();
  const int64_t emax = expr_->Max();
  const int64_t root = static_cast<int64_t>(ceil(sqrt(static_cast<double>(m))));
  if (emin >= 0) {
    expr_->SetMin(root);
  } else if (emax <= 0) {
    expr_->SetMax(-root);
  } else if (expr_->IsVar()) {
    reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-root + 1, root - 1);
  }
}

int64_t IntSquare::Max() const {
  const int64_t emax = expr_->Max();
  const int64_t emin = expr_->Min();
  if (emax >= std::numeric_limits<int32_t>::max() ||
      emin <= -std::numeric_limits<int32_t>::max()) {
    return std::numeric_limits<int64_t>::max();
  }
  return std::max(emin * emin, emax * emax);
}

void IntSquare::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m == std::numeric_limits<int64_t>::max()) {
    return;
  }
  const int64_t root =
      static_cast<int64_t>(floor(sqrt(static_cast<double>(m))));
  expr_->SetRange(-root, root);
}

bool IntSquare::Bound() const { return expr_->Bound(); }

std::string IntSquare::name() const {
  return absl::StrFormat("Square(%s)", expr_->name());
}

std::string IntSquare::DebugString() const {
  return absl::StrFormat("Square(%s)", expr_->DebugString());
}

void IntSquare::WhenRange(Demon* d) { expr_->WhenRange(d); }

void IntSquare::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSquare, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSquare, this);
}

PosIntSquare::PosIntSquare(Solver* s, IntExpr* e) : IntSquare(s, e) {}

int64_t PosIntSquare::Min() const {
  const int64_t emin = expr_->Min();
  return emin >= std::numeric_limits<int32_t>::max()
             ? std::numeric_limits<int64_t>::max()
             : emin * emin;
}
void PosIntSquare::SetMin(int64_t m) {
  if (m <= 0) {
    return;
  }
  int64_t root = static_cast<int64_t>(ceil(sqrt(static_cast<double>(m))));
  if (CapProd(root, root) < m) {
    root++;
  }
  expr_->SetMin(root);
}

int64_t PosIntSquare::Max() const {
  const int64_t emax = expr_->Max();
  return emax >= std::numeric_limits<int32_t>::max()
             ? std::numeric_limits<int64_t>::max()
             : emax * emax;
}
void PosIntSquare::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m == std::numeric_limits<int64_t>::max()) {
    return;
  }
  int64_t root = static_cast<int64_t>(floor(sqrt(static_cast<double>(m))));
  if (CapProd(root, root) > m) {
    root--;
  }

  expr_->SetMax(root);
}

BasePower::BasePower(Solver* s, IntExpr* e, int64_t n)
    : BaseIntExpr(s), expr_(e), pow_(n), limit_(IntPowerOverflowLimit(n)) {}

bool BasePower::Bound() const { return expr_->Bound(); }

void BasePower::WhenRange(Demon* d) { expr_->WhenRange(d); }
std::string BasePower::name() const {
  return absl::StrFormat("Power(%s, %d)", expr_->name(), pow_);
}
std::string BasePower::DebugString() const {
  return absl::StrFormat("Power(%s, %d)", expr_->DebugString(), pow_);
}
void BasePower::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kPower, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, pow_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kPower, this);
}

int64_t BasePower::Pown(int64_t value) const {
  if (value >= limit_) {
    return std::numeric_limits<int64_t>::max();
  }
  if (value <= -limit_) {
    if (pow_ % 2 == 0) {
      return std::numeric_limits<int64_t>::max();
    } else {
      return std::numeric_limits<int64_t>::min();
    }
  }
  return IntPowerValue(value, pow_);
}

int64_t BasePower::SqrnDown(int64_t value) const {
  if (value == std::numeric_limits<int64_t>::min()) {
    return std::numeric_limits<int64_t>::min();
  }
  if (value == std::numeric_limits<int64_t>::max()) {
    return std::numeric_limits<int64_t>::max();
  }
  int64_t res = 0;
  const double d_value = static_cast<double>(value);
  if (value >= 0) {
    const double sq = exp(log(d_value) / pow_);
    res = static_cast<int64_t>(floor(sq));
  } else {
    CHECK_EQ(1, pow_ % 2);
    const double sq = exp(log(-d_value) / pow_);
    res = -static_cast<int64_t>(ceil(sq));
  }
  const int64_t pow_res = Pown(res + 1);
  if (pow_res <= value) {
    return res + 1;
  } else {
    return res;
  }
}

int64_t BasePower::SqrnUp(int64_t value) const {
  if (value == std::numeric_limits<int64_t>::min()) {
    return std::numeric_limits<int64_t>::min();
  }
  if (value == std::numeric_limits<int64_t>::max()) {
    return std::numeric_limits<int64_t>::max();
  }
  int64_t res = 0;
  const double d_value = static_cast<double>(value);
  if (value >= 0) {
    const double sq = exp(log(d_value) / pow_);
    res = static_cast<int64_t>(ceil(sq));
  } else {
    CHECK_EQ(1, pow_ % 2);
    const double sq = exp(log(-d_value) / pow_);
    res = -static_cast<int64_t>(floor(sq));
  }
  const int64_t pow_res = Pown(res - 1);
  if (pow_res >= value) {
    return res - 1;
  } else {
    return res;
  }
}

IntEvenPower::IntEvenPower(Solver* s, IntExpr* e, int64_t n)
    : BasePower(s, e, n) {}

int64_t IntEvenPower::Min() const {
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  if (emin >= 0) {
    return Pown(emin);
  }
  if (emax < 0) {
    return Pown(emax);
  }
  return 0LL;
}

void IntEvenPower::SetMin(int64_t m) {
  if (m <= 0) {
    return;
  }
  int64_t emin = 0;
  int64_t emax = 0;
  expr_->Range(&emin, &emax);
  const int64_t root = SqrnUp(m);
  if (emin > -root) {
    expr_->SetMin(root);
  } else if (emax < root) {
    expr_->SetMax(-root);
  } else if (expr_->IsVar()) {
    reinterpret_cast<IntVar*>(expr_)->RemoveInterval(-root + 1, root - 1);
  }
}

int64_t IntEvenPower::Max() const {
  return std::max(Pown(expr_->Min()), Pown(expr_->Max()));
}

void IntEvenPower::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m == std::numeric_limits<int64_t>::max()) {
    return;
  }
  const int64_t root = SqrnDown(m);
  expr_->SetRange(-root, root);
}

PosIntEvenPower::PosIntEvenPower(Solver* s, IntExpr* e, int64_t n)
    : BasePower(s, e, n) {}

int64_t PosIntEvenPower::Min() const { return Pown(expr_->Min()); }
void PosIntEvenPower::SetMin(int64_t m) {
  if (m <= 0) {
    return;
  }
  expr_->SetMin(SqrnUp(m));
}
int64_t PosIntEvenPower::Max() const { return Pown(expr_->Max()); }
void PosIntEvenPower::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m == std::numeric_limits<int64_t>::max()) {
    return;
  }
  expr_->SetMax(SqrnDown(m));
}

IntOddPower::IntOddPower(Solver* s, IntExpr* e, int64_t n)
    : BasePower(s, e, n) {}

int64_t IntOddPower::Min() const { return Pown(expr_->Min()); }

void IntOddPower::SetMin(int64_t m) { expr_->SetMin(SqrnUp(m)); }
int64_t IntOddPower::Max() const { return Pown(expr_->Max()); }
void IntOddPower::SetMax(int64_t m) { expr_->SetMax(SqrnDown(m)); }

// ----- Min(expr, expr) -----

MinIntExpr::MinIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t MinIntExpr::Min() const {
  const int64_t lmin = left_->Min();
  const int64_t rmin = right_->Min();
  return std::min(lmin, rmin);
}
void MinIntExpr::SetMin(int64_t m) {
  left_->SetMin(m);
  right_->SetMin(m);
}
int64_t MinIntExpr::Max() const {
  const int64_t lmax = left_->Max();
  const int64_t rmax = right_->Max();
  return std::min(lmax, rmax);
}
void MinIntExpr::SetMax(int64_t m) {
  if (left_->Min() > m) {
    right_->SetMax(m);
  }
  if (right_->Min() > m) {
    left_->SetMax(m);
  }
}
std::string MinIntExpr::name() const {
  return absl::StrFormat("MinIntExpr(%s, %s)", left_->name(), right_->name());
}
std::string MinIntExpr::DebugString() const {
  return absl::StrFormat("MinIntExpr(%s, %s)", left_->DebugString(),
                         right_->DebugString());
}
void MinIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void MinIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
}

MinCstIntExpr::MinCstIntExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {}

int64_t MinCstIntExpr::Min() const { return std::min(expr_->Min(), value_); }

void MinCstIntExpr::SetMin(int64_t m) {
  if (m > value_) {
    solver()->Fail();
  }
  expr_->SetMin(m);
}

int64_t MinCstIntExpr::Max() const { return std::min(expr_->Max(), value_); }

void MinCstIntExpr::SetMax(int64_t m) {
  if (value_ > m) {
    expr_->SetMax(m);
  }
}

bool MinCstIntExpr::Bound() const {
  return (expr_->Bound() || expr_->Min() >= value_);
}

std::string MinCstIntExpr::name() const {
  return absl::StrFormat("MinCstIntExpr(%s, %d)", expr_->name(), value_);
}

std::string MinCstIntExpr::DebugString() const {
  return absl::StrFormat("MinCstIntExpr(%s, %d)", expr_->DebugString(), value_);
}

void MinCstIntExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void MinCstIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kMin, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kMin, this);
}

MaxIntExpr::MaxIntExpr(Solver* s, IntExpr* l, IntExpr* r)
    : BaseIntExpr(s), left_(l), right_(r) {}

int64_t MaxIntExpr::Min() const {
  return std::max(left_->Min(), right_->Min());
}

void MaxIntExpr::SetMin(int64_t m) {
  if (left_->Max() < m) {
    right_->SetMin(m);
  } else {
    if (right_->Max() < m) {
      left_->SetMin(m);
    }
  }
}

int64_t MaxIntExpr::Max() const {
  return std::max(left_->Max(), right_->Max());
}

void MaxIntExpr::SetMax(int64_t m) {
  left_->SetMax(m);
  right_->SetMax(m);
}

std::string MaxIntExpr::name() const {
  return absl::StrFormat("MaxIntExpr(%s, %s)", left_->name(), right_->name());
}

std::string MaxIntExpr::DebugString() const {
  return absl::StrFormat("MaxIntExpr(%s, %s)", left_->DebugString(),
                         right_->DebugString());
}

void MaxIntExpr::WhenRange(Demon* d) {
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void MaxIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
}

MaxCstIntExpr::MaxCstIntExpr(Solver* s, IntExpr* e, int64_t v)
    : BaseIntExpr(s), expr_(e), value_(v) {}

int64_t MaxCstIntExpr::Min() const { return std::max(expr_->Min(), value_); }

void MaxCstIntExpr::SetMin(int64_t m) {
  if (value_ < m) {
    expr_->SetMin(m);
  }
}

int64_t MaxCstIntExpr::Max() const { return std::max(expr_->Max(), value_); }

void MaxCstIntExpr::SetMax(int64_t m) {
  if (m < value_) {
    solver()->Fail();
  }
  expr_->SetMax(m);
}

bool MaxCstIntExpr::Bound() const {
  return (expr_->Bound() || expr_->Max() <= value_);
}

std::string MaxCstIntExpr::name() const {
  return absl::StrFormat("MaxCstIntExpr(%s, %d)", expr_->name(), value_);
}

std::string MaxCstIntExpr::DebugString() const {
  return absl::StrFormat("MaxCstIntExpr(%s, %d)", expr_->DebugString(), value_);
}

void MaxCstIntExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void MaxCstIntExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kMax, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kMax, this);
}

SimpleConvexPiecewiseExpr::SimpleConvexPiecewiseExpr(Solver* s, IntExpr* e,
                                                     int64_t ec, int64_t ed,
                                                     int64_t ld, int64_t lc)
    : BaseIntExpr(s),
      expr_(e),
      early_cost_(ec),
      early_date_(ec == 0 ? std::numeric_limits<int64_t>::min() : ed),
      late_date_(lc == 0 ? std::numeric_limits<int64_t>::max() : ld),
      late_cost_(lc) {
  DCHECK_GE(ec, int64_t{0});
  DCHECK_GE(lc, int64_t{0});
  DCHECK_GE(ld, ed);

  // If the penalty is 0, we can push the "comfort zone or zone of no cost
  // towards infinity.
}

int64_t SimpleConvexPiecewiseExpr::Min() const {
  const int64_t vmin = expr_->Min();
  const int64_t vmax = expr_->Max();
  if (vmin >= late_date_) {
    return (vmin - late_date_) * late_cost_;
  } else if (vmax <= early_date_) {
    return (early_date_ - vmax) * early_cost_;
  } else {
    return 0LL;
  }
}

void SimpleConvexPiecewiseExpr::SetMin(int64_t m) {
  if (m <= 0) {
    return;
  }
  int64_t vmin = 0;
  int64_t vmax = 0;
  expr_->Range(&vmin, &vmax);

  const int64_t rb =
      (late_cost_ == 0 ? vmax : late_date_ + PosIntDivUp(m, late_cost_) - 1);
  const int64_t lb =
      (early_cost_ == 0 ? vmin : early_date_ - PosIntDivUp(m, early_cost_) + 1);

  if (expr_->IsVar()) {
    expr_->Var()->RemoveInterval(lb, rb);
  }
}

int64_t SimpleConvexPiecewiseExpr::Max() const {
  const int64_t vmin = expr_->Min();
  const int64_t vmax = expr_->Max();
  const int64_t mr = vmax > late_date_ ? (vmax - late_date_) * late_cost_ : 0;
  const int64_t ml =
      vmin < early_date_ ? (early_date_ - vmin) * early_cost_ : 0;
  return std::max(mr, ml);
}

void SimpleConvexPiecewiseExpr::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (late_cost_ != 0LL) {
    const int64_t rb = late_date_ + PosIntDivDown(m, late_cost_);
    if (early_cost_ != 0LL) {
      const int64_t lb = early_date_ - PosIntDivDown(m, early_cost_);
      expr_->SetRange(lb, rb);
    } else {
      expr_->SetMax(rb);
    }
  } else {
    if (early_cost_ != 0LL) {
      const int64_t lb = early_date_ - PosIntDivDown(m, early_cost_);
      expr_->SetMin(lb);
    }
  }
}

std::string SimpleConvexPiecewiseExpr::name() const {
  return absl::StrFormat(
      "ConvexPiecewiseExpr(%s, ec = %d, ed = %d, ld = %d, lc = %d)",
      expr_->name(), early_cost_, early_date_, late_date_, late_cost_);
}

std::string SimpleConvexPiecewiseExpr::DebugString() const {
  return absl::StrFormat(
      "ConvexPiecewiseExpr(%s, ec = %d, ed = %d, ld = %d, lc = %d)",
      expr_->DebugString(), early_cost_, early_date_, late_date_, late_cost_);
}

void SimpleConvexPiecewiseExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void SimpleConvexPiecewiseExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kEarlyCostArgument, early_cost_);
  visitor->VisitIntegerArgument(ModelVisitor::kEarlyDateArgument, early_date_);
  visitor->VisitIntegerArgument(ModelVisitor::kLateCostArgument, late_cost_);
  visitor->VisitIntegerArgument(ModelVisitor::kLateDateArgument, late_date_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kConvexPiecewise, this);
}

SemiContinuousExpr::SemiContinuousExpr(Solver* s, IntExpr* e,
                                       int64_t fixed_charge, int64_t step)
    : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge), step_(step) {
  DCHECK_GE(fixed_charge, int64_t{0});
  DCHECK_GT(step, int64_t{0});
}

int64_t SemiContinuousExpr::Value(int64_t x) const {
  if (x <= 0) {
    return 0;
  } else {
    return CapAdd(fixed_charge_, CapProd(x, step_));
  }
}

int64_t SemiContinuousExpr::Min() const { return Value(expr_->Min()); }

void SemiContinuousExpr::SetMin(int64_t m) {
  if (m >= CapAdd(fixed_charge_, step_)) {
    const int64_t y = PosIntDivUp(CapSub(m, fixed_charge_), step_);
    expr_->SetMin(y);
  } else if (m > 0) {
    expr_->SetMin(1);
  }
}

int64_t SemiContinuousExpr::Max() const { return Value(expr_->Max()); }

void SemiContinuousExpr::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m == std::numeric_limits<int64_t>::max()) {
    return;
  }
  if (m < CapAdd(fixed_charge_, step_)) {
    expr_->SetMax(0);
  } else {
    const int64_t y = PosIntDivDown(CapSub(m, fixed_charge_), step_);
    expr_->SetMax(y);
  }
}

std::string SemiContinuousExpr::name() const {
  return absl::StrFormat("SemiContinuous(%s, fixed_charge = %d, step = %d)",
                         expr_->name(), fixed_charge_, step_);
}

std::string SemiContinuousExpr::DebugString() const {
  return absl::StrFormat("SemiContinuous(%s, fixed_charge = %d, step = %d)",
                         expr_->DebugString(), fixed_charge_, step_);
}

void SemiContinuousExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void SemiContinuousExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                fixed_charge_);
  visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, step_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
}

SemiContinuousStepOneExpr::SemiContinuousStepOneExpr(Solver* s, IntExpr* e,
                                                     int64_t fixed_charge)
    : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
  DCHECK_GE(fixed_charge, int64_t{0});
}

int64_t SemiContinuousStepOneExpr::Value(int64_t x) const {
  if (x <= 0) {
    return 0;
  } else {
    return fixed_charge_ + x;
  }
}

int64_t SemiContinuousStepOneExpr::Min() const { return Value(expr_->Min()); }

void SemiContinuousStepOneExpr::SetMin(int64_t m) {
  if (m >= fixed_charge_ + 1) {
    expr_->SetMin(m - fixed_charge_);
  } else if (m > 0) {
    expr_->SetMin(1);
  }
}

int64_t SemiContinuousStepOneExpr::Max() const { return Value(expr_->Max()); }

void SemiContinuousStepOneExpr::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m < fixed_charge_ + 1) {
    expr_->SetMax(0);
  } else {
    expr_->SetMax(m - fixed_charge_);
  }
}

std::string SemiContinuousStepOneExpr::name() const {
  return absl::StrFormat("SemiContinuousStepOne(%s, fixed_charge = %d)",
                         expr_->name(), fixed_charge_);
}

std::string SemiContinuousStepOneExpr::DebugString() const {
  return absl::StrFormat("SemiContinuousStepOne(%s, fixed_charge = %d)",
                         expr_->DebugString(), fixed_charge_);
}

void SemiContinuousStepOneExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void SemiContinuousStepOneExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                fixed_charge_);
  visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 1);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
}

SemiContinuousStepZeroExpr::SemiContinuousStepZeroExpr(Solver* s, IntExpr* e,
                                                       int64_t fixed_charge)
    : BaseIntExpr(s), expr_(e), fixed_charge_(fixed_charge) {
  DCHECK_GT(fixed_charge, int64_t{0});
}

int64_t SemiContinuousStepZeroExpr::Value(int64_t x) const {
  if (x <= 0) {
    return 0;
  } else {
    return fixed_charge_;
  }
}

int64_t SemiContinuousStepZeroExpr::Min() const { return Value(expr_->Min()); }

void SemiContinuousStepZeroExpr::SetMin(int64_t m) {
  if (m >= fixed_charge_) {
    solver()->Fail();
  } else if (m > 0) {
    expr_->SetMin(1);
  }
}

int64_t SemiContinuousStepZeroExpr::Max() const { return Value(expr_->Max()); }

void SemiContinuousStepZeroExpr::SetMax(int64_t m) {
  if (m < 0) {
    solver()->Fail();
  }
  if (m < fixed_charge_) {
    expr_->SetMax(0);
  }
}

std::string SemiContinuousStepZeroExpr::name() const {
  return absl::StrFormat("SemiContinuousStepZero(%s, fixed_charge = %d)",
                         expr_->name(), fixed_charge_);
}

std::string SemiContinuousStepZeroExpr::DebugString() const {
  return absl::StrFormat("SemiContinuousStepZero(%s, fixed_charge = %d)",
                         expr_->DebugString(), fixed_charge_);
}

void SemiContinuousStepZeroExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void SemiContinuousStepZeroExpr::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expr_);
  visitor->VisitIntegerArgument(ModelVisitor::kFixedChargeArgument,
                                fixed_charge_);
  visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, 0);
  visitor->EndVisitIntegerExpression(ModelVisitor::kSemiContinuous, this);
}

// ----- Conditional Expression -----

ExprWithEscapeValue::ExprWithEscapeValue(Solver* s, IntVar* c, IntExpr* e,
                                         int64_t unperformed_value)
    : BaseIntExpr(s),
      condition_(c),
      expression_(e),
      unperformed_value_(unperformed_value) {}

int64_t ExprWithEscapeValue::Min() const {
  if (condition_->Min() == 1) {
    return expression_->Min();
  } else if (condition_->Max() == 1) {
    return std::min(unperformed_value_, expression_->Min());
  } else {
    return unperformed_value_;
  }
}

void ExprWithEscapeValue::SetMin(int64_t m) {
  if (m > unperformed_value_) {
    condition_->SetValue(1);
    expression_->SetMin(m);
  } else if (condition_->Min() == 1) {
    expression_->SetMin(m);
  } else if (m > expression_->Max()) {
    condition_->SetValue(0);
  }
}

int64_t ExprWithEscapeValue::Max() const {
  if (condition_->Min() == 1) {
    return expression_->Max();
  } else if (condition_->Max() == 1) {
    return std::max(unperformed_value_, expression_->Max());
  } else {
    return unperformed_value_;
  }
}

void ExprWithEscapeValue::SetMax(int64_t m) {
  if (m < unperformed_value_) {
    condition_->SetValue(1);
    expression_->SetMax(m);
  } else if (condition_->Min() == 1) {
    expression_->SetMax(m);
  } else if (m < expression_->Min()) {
    condition_->SetValue(0);
  }
}

void ExprWithEscapeValue::SetRange(int64_t mi, int64_t ma) {
  if (ma < unperformed_value_ || mi > unperformed_value_) {
    condition_->SetValue(1);
    expression_->SetRange(mi, ma);
  } else if (condition_->Min() == 1) {
    expression_->SetRange(mi, ma);
  } else if (ma < expression_->Min() || mi > expression_->Max()) {
    condition_->SetValue(0);
  }
}

void ExprWithEscapeValue::SetValue(int64_t v) {
  if (v != unperformed_value_) {
    condition_->SetValue(1);
    expression_->SetValue(v);
  } else if (condition_->Min() == 1) {
    expression_->SetValue(v);
  } else if (v < expression_->Min() || v > expression_->Max()) {
    condition_->SetValue(0);
  }
}

bool ExprWithEscapeValue::Bound() const {
  return condition_->Max() == 0 || expression_->Bound();
}

void ExprWithEscapeValue::WhenRange(Demon* d) {
  expression_->WhenRange(d);
  condition_->WhenBound(d);
}

std::string ExprWithEscapeValue::DebugString() const {
  return absl::StrFormat("ConditionExpr(%s, %s, %d)", condition_->DebugString(),
                         expression_->DebugString(), unperformed_value_);
}

void ExprWithEscapeValue::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitIntegerExpression(ModelVisitor::kConditionalExpr, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kVariableArgument,
                                          condition_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          expression_);
  visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                unperformed_value_);
  visitor->EndVisitIntegerExpression(ModelVisitor::kConditionalExpr, this);
}

// ----- Piecewise Linear -----

PiecewiseLinearExpr::PiecewiseLinearExpr(Solver* solver, IntExpr* expr,
                                         const PiecewiseLinearFunction& f)
    : BaseIntExpr(solver), expr_(expr), f_(f) {}
int64_t PiecewiseLinearExpr::Min() const {
  return f_.GetMinimum(expr_->Min(), expr_->Max());
}
void PiecewiseLinearExpr::SetMin(int64_t m) {
  const auto& range =
      f_.GetSmallestRangeGreaterThanValue(expr_->Min(), expr_->Max(), m);
  expr_->SetRange(range.first, range.second);
}

int64_t PiecewiseLinearExpr::Max() const {
  return f_.GetMaximum(expr_->Min(), expr_->Max());
}

void PiecewiseLinearExpr::SetMax(int64_t m) {
  const auto& range =
      f_.GetSmallestRangeLessThanValue(expr_->Min(), expr_->Max(), m);
  expr_->SetRange(range.first, range.second);
}

void PiecewiseLinearExpr::SetRange(int64_t l, int64_t u) {
  const auto& range =
      f_.GetSmallestRangeInValueRange(expr_->Min(), expr_->Max(), l, u);
  expr_->SetRange(range.first, range.second);
}
std::string PiecewiseLinearExpr::name() const {
  return absl::StrFormat("PiecewiseLinear(%s, f = %s)", expr_->name(),
                         f_.DebugString());
}

std::string PiecewiseLinearExpr::DebugString() const {
  return absl::StrFormat("PiecewiseLinear(%s, f = %s)", expr_->DebugString(),
                         f_.DebugString());
}

void PiecewiseLinearExpr::WhenRange(Demon* d) { expr_->WhenRange(d); }

void PiecewiseLinearExpr::Accept(ModelVisitor*) const {
  // TODO(user): Implement visitor.
}

// Discovery methods and helpers.

bool IsExprPower(IntExpr* expr, IntExpr** sub_expr, int64_t* exponant) {
  BasePower* const base_power = dynamic_cast<BasePower*>(expr);
  if (base_power != nullptr) {
    *sub_expr = base_power->expr();
    *exponant = base_power->exponant();
    return true;
  }
  IntSquare* const int_square = dynamic_cast<IntSquare*>(expr);
  if (int_square != nullptr) {
    *sub_expr = int_square->expr();
    *exponant = 2;
    return true;
  }
  if (expr->IsVar()) {
    IntVar* const var = expr->Var();
    IntExpr* const sub = var->solver()->CastExpression(var);
    BasePower* const sub_power = dynamic_cast<BasePower*>(sub);
    if (sub != nullptr && sub_power != nullptr) {
      *sub_expr = sub_power->expr();
      *exponant = sub_power->exponant();
      return true;
    }
    IntSquare* const sub_square = dynamic_cast<IntSquare*>(sub);
    if (sub != nullptr && sub_square != nullptr) {
      *sub_expr = sub_square->expr();
      *exponant = 2;
      return true;
    }
  }
  return false;
}

bool IsExprProduct(IntExpr* expr, IntExpr** inner_expr, int64_t* coefficient) {
  IntVar* inner_var = nullptr;
  if (IsVarProduct(expr, &inner_var, coefficient)) {
    *inner_expr = inner_var;
    return true;
  }
  TimesIntCstExpr* const prod = dynamic_cast<TimesIntCstExpr*>(expr);
  if (prod != nullptr) {
    *coefficient = prod->Constant();
    *inner_expr = prod->Expr();
    return true;
  }
  *inner_expr = expr;
  *coefficient = 1;
  return false;
}

bool IsADifference(IntExpr* expr, IntExpr** left, IntExpr** right) {
  if (expr->IsVar()) {
    IntVar* const expr_var = expr->Var();
    expr = expr_var->solver()->CastExpression(expr_var);
  }
  // This is a dynamic cast to check the type of expr.
  // It returns nullptr is expr is not a subclass of SubIntExpr.
  SubIntExpr* const sub_expr = dynamic_cast<SubIntExpr*>(expr);
  if (sub_expr != nullptr) {
    *left = sub_expr->left();
    *right = sub_expr->right();
    return true;
  }
  return false;
}

int64_t IntPowerValue(int64_t value, int64_t power) {
  int64_t result = value;
  // TODO(user): Speed that up.
  for (int i = 1; i < power; ++i) {
    result *= value;
  }
  return result;
}

int64_t IntPowerOverflowLimit(int64_t power) {
  return static_cast<int64_t>(floor(exp(
      log(static_cast<double>(std::numeric_limits<int64_t>::max())) / power)));
}

}  // namespace operations_research
