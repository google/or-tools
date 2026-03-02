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

//  Range constraints

#include "ortools/constraint_solver/range_cst.h"

#include <stddef.h>

#include <string>

#include "absl/strings/str_format.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"

namespace operations_research {

//-----------------------------------------------------------------------------
// RangeEquality

RangeEquality::RangeEquality(Solver* s, IntExpr* l, IntExpr* r)
    : Constraint(s), left_(l), right_(r) {}

void RangeEquality::Post() {
  Demon* const d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeEquality::InitialPropagate() {
  left_->SetRange(right_->Min(), right_->Max());
  right_->SetRange(left_->Min(), left_->Max());
}

std::string RangeEquality::DebugString() const {
  return left_->DebugString() + " == " + right_->DebugString();
}

IntVar* RangeEquality::Var() { return solver()->MakeIsEqualVar(left_, right_); }

void RangeEquality::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
}

//-----------------------------------------------------------------------------
// RangeLessOrEqual

RangeLessOrEqual::RangeLessOrEqual(Solver* s, IntExpr* l, IntExpr* r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void RangeLessOrEqual::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void RangeLessOrEqual::InitialPropagate() {
  left_->SetMax(right_->Max());
  right_->SetMin(left_->Min());
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string RangeLessOrEqual::DebugString() const {
  return left_->DebugString() + " <= " + right_->DebugString();
}

IntVar* RangeLessOrEqual::Var() {
  return solver()->MakeIsLessOrEqualVar(left_, right_);
}

void RangeLessOrEqual::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
}

//-----------------------------------------------------------------------------
// RangeLess

RangeLess::RangeLess(Solver* s, IntExpr* l, IntExpr* r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void RangeLess::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void RangeLess::InitialPropagate() {
  if (right_->Max() == kint64min) solver()->Fail();
  left_->SetMax(right_->Max() - 1);
  if (left_->Min() == kint64max) solver()->Fail();
  right_->SetMin(left_->Min() + 1);
  if (left_->Max() < right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string RangeLess::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}

IntVar* RangeLess::Var() { return solver()->MakeIsLessVar(left_, right_); }

void RangeLess::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kLess, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitConstraint(ModelVisitor::kLess, this);
}

//-----------------------------------------------------------------------------
// DiffVar

DiffVar::DiffVar(Solver* s, IntVar* l, IntVar* r)
    : Constraint(s), left_(l), right_(r) {}

void DiffVar::Post() {
  Demon* const left_demon =
      MakeConstraintDemon0(solver(), this, &DiffVar::LeftBound, "LeftBound");
  Demon* const right_demon =
      MakeConstraintDemon0(solver(), this, &DiffVar::RightBound, "RightBound");
  left_->WhenBound(left_demon);
  right_->WhenBound(right_demon);
  // TODO(user) : improve me, separated demons, actually to test
}

void DiffVar::LeftBound() {
  if (right_->Size() < 0xFFFFFF) {
    right_->RemoveValue(left_->Min());  // we use min instead of value
  } else {
    solver()->AddConstraint(solver()->MakeNonEquality(right_, left_->Min()));
  }
}

void DiffVar::RightBound() {
  if (left_->Size() < 0xFFFFFF) {
    left_->RemoveValue(right_->Min());  // see above
  } else {
    solver()->AddConstraint(solver()->MakeNonEquality(left_, right_->Min()));
  }
}

void DiffVar::InitialPropagate() {
  if (left_->Bound()) {
    LeftBound();
  }
  if (right_->Bound()) {
    RightBound();
  }
}

std::string DiffVar::DebugString() const {
  return left_->DebugString() + " != " + right_->DebugString();
}

IntVar* DiffVar::Var() { return solver()->MakeIsDifferentVar(left_, right_); }

void DiffVar::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kNonEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->EndVisitConstraint(ModelVisitor::kNonEqual, this);
}

// --------------------- Reified API -------------------

// IsEqualCt

IsEqualCt::IsEqualCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b)
    : CastConstraint(s, b), left_(l), right_(r), range_demon_(nullptr) {}

void IsEqualCt::Post() {
  range_demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(range_demon_);
  right_->WhenRange(range_demon_);
  Demon* const target_demon = MakeConstraintDemon0(
      solver(), this, &IsEqualCt::PropagateTarget, "PropagateTarget");
  target_var_->WhenBound(target_demon);
}

void IsEqualCt::InitialPropagate() {
  if (target_var_->Bound()) {
    PropagateTarget();
    return;
  }
  if (left_->Min() > right_->Max() || left_->Max() < right_->Min()) {
    target_var_->SetValue(0);
    range_demon_->inhibit(solver());
  } else if (left_->Bound()) {
    if (right_->Bound()) {
      target_var_->SetValue(left_->Min() == right_->Min());
    } else if (right_->IsVar() && !right_->Var()->Contains(left_->Min())) {
      range_demon_->inhibit(solver());
      target_var_->SetValue(0);
    }
  } else if (right_->Bound() && left_->IsVar() &&
             !left_->Var()->Contains(right_->Min())) {
    range_demon_->inhibit(solver());
    target_var_->SetValue(0);
  }
}

void IsEqualCt::PropagateTarget() {
  if (target_var_->Min() == 0) {
    if (left_->Bound()) {
      range_demon_->inhibit(solver());
      if (right_->IsVar()) {
        right_->Var()->RemoveValue(left_->Min());
      } else {
        solver()->AddConstraint(
            solver()->MakeNonEquality(right_, left_->Min()));
      }
    } else if (right_->Bound()) {
      range_demon_->inhibit(solver());
      if (left_->IsVar()) {
        left_->Var()->RemoveValue(right_->Min());
      } else {
        solver()->AddConstraint(
            solver()->MakeNonEquality(left_, right_->Min()));
      }
    }
  } else {  // Var is true.
    left_->SetRange(right_->Min(), right_->Max());
    right_->SetRange(left_->Min(), left_->Max());
  }
}

std::string IsEqualCt::DebugString() const {
  return absl::StrFormat("IsEqualCt(%s, %s, %s)", left_->DebugString(),
                         right_->DebugString(), target_var_->DebugString());
}

void IsEqualCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsEqual, this);
}

// IsDifferentCt

IsDifferentCt::IsDifferentCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b)
    : CastConstraint(s, b), left_(l), right_(r), range_demon_(nullptr) {}

void IsDifferentCt::Post() {
  range_demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(range_demon_);
  right_->WhenRange(range_demon_);
  Demon* const target_demon = MakeConstraintDemon0(
      solver(), this, &IsDifferentCt::PropagateTarget, "PropagateTarget");
  target_var_->WhenBound(target_demon);
}

void IsDifferentCt::InitialPropagate() {
  if (target_var_->Bound()) {
    PropagateTarget();
    return;
  }
  if (left_->Min() > right_->Max() || left_->Max() < right_->Min()) {
    target_var_->SetValue(1);
    range_demon_->inhibit(solver());
  } else if (left_->Bound()) {
    if (right_->Bound()) {
      target_var_->SetValue(left_->Min() != right_->Min());
    } else if (right_->IsVar() && !right_->Var()->Contains(left_->Min())) {
      range_demon_->inhibit(solver());
      target_var_->SetValue(1);
    }
  } else if (right_->Bound() && left_->IsVar() &&
             !left_->Var()->Contains(right_->Min())) {
    range_demon_->inhibit(solver());
    target_var_->SetValue(1);
  }
}

void IsDifferentCt::PropagateTarget() {
  if (target_var_->Min() == 0) {
    left_->SetRange(right_->Min(), right_->Max());
    right_->SetRange(left_->Min(), left_->Max());
  } else {  // Var is true.
    if (left_->Bound()) {
      range_demon_->inhibit(solver());
      solver()->AddConstraint(solver()->MakeNonEquality(right_, left_->Min()));
    } else if (right_->Bound()) {
      range_demon_->inhibit(solver());
      solver()->AddConstraint(solver()->MakeNonEquality(left_, right_->Min()));
    }
  }
}

std::string IsDifferentCt::DebugString() const {
  return absl::StrFormat("IsDifferentCt(%s, %s, %s)", left_->DebugString(),
                         right_->DebugString(), target_var_->DebugString());
}

void IsDifferentCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsDifferent, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsDifferent, this);
}

// IsLessOrEqualCt

IsLessOrEqualCt::IsLessOrEqualCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b)
    : CastConstraint(s, b), left_(l), right_(r), demon_(nullptr) {}

void IsLessOrEqualCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
  target_var_->WhenBound(demon_);
}

void IsLessOrEqualCt::InitialPropagate() {
  if (target_var_->Bound()) {
    if (target_var_->Min() == 0) {
      right_->SetMax(left_->Max() - 1);
      left_->SetMin(right_->Min() + 1);
    } else {  // Var is true.
      right_->SetMin(left_->Min());
      left_->SetMax(right_->Max());
    }
  } else if (right_->Min() >= left_->Max()) {
    demon_->inhibit(solver());
    target_var_->SetValue(1);
  } else if (right_->Max() < left_->Min()) {
    demon_->inhibit(solver());
    target_var_->SetValue(0);
  }
}

std::string IsLessOrEqualCt::DebugString() const {
  return absl::StrFormat("IsLessOrEqualCt(%s, %s, %s)", left_->DebugString(),
                         right_->DebugString(), target_var_->DebugString());
}

void IsLessOrEqualCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
}

// IsLessCt

IsLessCt::IsLessCt(Solver* s, IntExpr* l, IntExpr* r, IntVar* b)
    : CastConstraint(s, b), left_(l), right_(r), demon_(nullptr) {}

void IsLessCt::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
  target_var_->WhenBound(demon_);
}

void IsLessCt::InitialPropagate() {
  if (target_var_->Bound()) {
    if (target_var_->Min() == 0) {
      right_->SetMax(left_->Max());
      left_->SetMin(right_->Min());
    } else {  // Var is true.
      right_->SetMin(left_->Min() + 1);
      left_->SetMax(right_->Max() - 1);
    }
  } else if (right_->Min() > left_->Max()) {
    demon_->inhibit(solver());
    target_var_->SetValue(1);
  } else if (right_->Max() <= left_->Min()) {
    demon_->inhibit(solver());
    target_var_->SetValue(0);
  }
}

std::string IsLessCt::DebugString() const {
  return absl::StrFormat("IsLessCt(%s, %s, %s)", left_->DebugString(),
                         right_->DebugString(), target_var_->DebugString());
}

void IsLessCt::Accept(ModelVisitor* visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kIsLess, this);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, right_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                          target_var_);
  visitor->EndVisitConstraint(ModelVisitor::kIsLess, this);
}

}  // namespace operations_research
