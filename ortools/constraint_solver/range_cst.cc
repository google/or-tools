// Copyright 2010-2014 Google
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
//  Range constraints

#include <stddef.h>
#include <string>

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {

//-----------------------------------------------------------------------------
// RangeEquality

namespace {
class RangeEquality : public Constraint {
 public:
  RangeEquality(Solver* const s, IntExpr* const l, IntExpr* const r)
      : Constraint(s), left_(l), right_(r) {}

  ~RangeEquality() override {}

  void Post() override {
    Demon* const d = solver()->MakeConstraintInitialPropagateCallback(this);
    left_->WhenRange(d);
    right_->WhenRange(d);
  }

  void InitialPropagate() override {
    left_->SetRange(right_->Min(), right_->Max());
    right_->SetRange(left_->Min(), left_->Max());
  }

  std::string DebugString() const override {
    return left_->DebugString() + " == " + right_->DebugString();
  }

  IntVar* Var() override { return solver()->MakeIsEqualVar(left_, right_); }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
};

//-----------------------------------------------------------------------------
// RangeLessOrEqual

class RangeLessOrEqual : public Constraint {
 public:
  RangeLessOrEqual(Solver* const s, IntExpr* const l, IntExpr* const r);
  ~RangeLessOrEqual() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsLessOrEqualVar(left_, right_);
  }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

RangeLessOrEqual::RangeLessOrEqual(Solver* const s, IntExpr* const l,
                                   IntExpr* const r)
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

//-----------------------------------------------------------------------------
// RangeLess

class RangeLess : public Constraint {
 public:
  RangeLess(Solver* const s, IntExpr* const l, IntExpr* const r);
  ~RangeLess() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override { return solver()->MakeIsLessVar(left_, right_); }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLess, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLess, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

RangeLess::RangeLess(Solver* const s, IntExpr* const l, IntExpr* const r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void RangeLess::Post() {
  demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void RangeLess::InitialPropagate() {
  left_->SetMax(right_->Max() - 1);
  right_->SetMin(left_->Min() + 1);
  if (left_->Max() < right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string RangeLess::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}

//-----------------------------------------------------------------------------
// DiffVar

class DiffVar : public Constraint {
 public:
  DiffVar(Solver* const s, IntVar* const l, IntVar* const r);
  ~DiffVar() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override { return solver()->MakeIsDifferentVar(left_, right_); }
  void LeftBound();
  void RightBound();

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kNonEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kNonEqual, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

DiffVar::DiffVar(Solver* const s, IntVar* const l, IntVar* const r)
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

// --------------------- Reified API -------------------
// A reified API transforms an constraint into a status variables.
// For example x == y is transformed into IsEqual(x, y, b) where
// b is a boolean variable which is true if and only if x is equal to b.

// IsEqualCt

class IsEqualCt : public CastConstraint {
 public:
  IsEqualCt(Solver* const s, IntExpr* const l, IntExpr* const r,
            IntVar* const b)
      : CastConstraint(s, b), left_(l), right_(r), range_demon_(nullptr) {}

  ~IsEqualCt() override {}

  void Post() override {
    range_demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    left_->WhenRange(range_demon_);
    right_->WhenRange(range_demon_);
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &IsEqualCt::PropagateTarget, "PropagateTarget");
    target_var_->WhenBound(target_demon);
  }

  void InitialPropagate() override {
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

  void PropagateTarget() {
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

  std::string DebugString() const override {
    return StringPrintf("IsEqualCt(%s, %s, %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIsEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsEqual, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* range_demon_;
};

// IsDifferentCt

class IsDifferentCt : public CastConstraint {
 public:
  IsDifferentCt(Solver* const s, IntExpr* const l, IntExpr* const r,
                IntVar* const b)
      : CastConstraint(s, b), left_(l), right_(r), range_demon_(nullptr) {}

  ~IsDifferentCt() override {}

  void Post() override {
    range_demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    left_->WhenRange(range_demon_);
    right_->WhenRange(range_demon_);
    Demon* const target_demon = MakeConstraintDemon0(
        solver(), this, &IsDifferentCt::PropagateTarget, "PropagateTarget");
    target_var_->WhenBound(target_demon);
  }

  void InitialPropagate() override {
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

  void PropagateTarget() {
    if (target_var_->Min() == 0) {
      left_->SetRange(right_->Min(), right_->Max());
      right_->SetRange(left_->Min(), left_->Max());
    } else {  // Var is true.
      if (left_->Bound()) {
        range_demon_->inhibit(solver());
        solver()->AddConstraint(
            solver()->MakeNonEquality(right_, left_->Min()));
      } else if (right_->Bound()) {
        range_demon_->inhibit(solver());
        solver()->AddConstraint(
            solver()->MakeNonEquality(left_, right_->Min()));
      }
    }
  }

  std::string DebugString() const override {
    return StringPrintf(
        "IsDifferentCt(%s, %s, %s)", left_->DebugString().c_str(),
        right_->DebugString().c_str(), target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIsDifferent, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsDifferent, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* range_demon_;
};

class IsLessOrEqualCt : public CastConstraint {
 public:
  IsLessOrEqualCt(Solver* const s, IntExpr* const l, IntExpr* const r,
                  IntVar* const b)
      : CastConstraint(s, b), left_(l), right_(r), demon_(nullptr) {}

  ~IsLessOrEqualCt() override {}

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    left_->WhenRange(demon_);
    right_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf(
        "IsLessOrEqualCt(%s, %s, %s)", left_->DebugString().c_str(),
        right_->DebugString().c_str(), target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsLessOrEqual, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

class IsLessCt : public CastConstraint {
 public:
  IsLessCt(Solver* const s, IntExpr* const l, IntExpr* const r, IntVar* const b)
      : CastConstraint(s, b), left_(l), right_(r), demon_(nullptr) {}

  ~IsLessCt() override {}

  void Post() override {
    demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
    left_->WhenRange(demon_);
    right_->WhenRange(demon_);
    target_var_->WhenBound(demon_);
  }

  void InitialPropagate() override {
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

  std::string DebugString() const override {
    return StringPrintf("IsLessCt(%s, %s, %s)", left_->DebugString().c_str(),
                        right_->DebugString().c_str(),
                        target_var_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIsLess, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            target_var_);
    visitor->EndVisitConstraint(ModelVisitor::kIsLess, this);
  }

 private:
  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};
}  // namespace

Constraint* Solver::MakeEquality(IntExpr* const l, IntExpr* const r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeEquality(r, l->Min());
  } else if (r->Bound()) {
    return MakeEquality(l, r->Min());
  } else {
    return RevAlloc(new RangeEquality(this, l, r));
  }
}

Constraint* Solver::MakeLessOrEqual(IntExpr* const l, IntExpr* const r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l == r) {
    return MakeTrueConstraint();
  } else if (l->Bound()) {
    return MakeGreaterOrEqual(r, l->Min());
  } else if (r->Bound()) {
    return MakeLessOrEqual(l, r->Min());
  } else {
    return RevAlloc(new RangeLessOrEqual(this, l, r));
  }
}

Constraint* Solver::MakeGreaterOrEqual(IntExpr* const l, IntExpr* const r) {
  return MakeLessOrEqual(r, l);
}

Constraint* Solver::MakeLess(IntExpr* const l, IntExpr* const r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeGreater(r, l->Min());
  } else if (r->Bound()) {
    return MakeLess(l, r->Min());
  } else {
    return RevAlloc(new RangeLess(this, l, r));
  }
}

Constraint* Solver::MakeGreater(IntExpr* const l, IntExpr* const r) {
  return MakeLess(r, l);
}

Constraint* Solver::MakeNonEquality(IntExpr* const l, IntExpr* const r) {
  CHECK(l != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK(r != nullptr) << "left expression nullptr, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  if (l->Bound()) {
    return MakeNonEquality(r, l->Min());
  } else if (r->Bound()) {
    return MakeNonEquality(l, r->Min());
  }
  return RevAlloc(new DiffVar(this, l->Var(), r->Var()));
}

IntVar* Solver::MakeIsEqualVar(IntExpr* const v1, IntExpr* const v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstVar(v2, v1->Min());
  } else if (v2->Bound()) {
    return MakeIsEqualCstVar(v1, v2->Min());
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      v1, v2, ModelCache::EXPR_EXPR_IS_EQUAL);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(
        v2, v1, ModelCache::EXPR_EXPR_IS_EQUAL);
  }
  if (cache != nullptr) {
    return cache->Var();
  } else {
    IntVar* boolvar = nullptr;
    IntExpr* reverse_cache = model_cache_->FindExprExprExpression(
        v1, v2, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    if (reverse_cache == nullptr) {
      reverse_cache = model_cache_->FindExprExprExpression(
          v2, v1, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    }
    if (reverse_cache != nullptr) {
      boolvar = MakeDifference(1, reverse_cache)->Var();
    } else {
      std::string name1 = v1->name();
      if (name1.empty()) {
        name1 = v1->DebugString();
      }
      std::string name2 = v2->name();
      if (name2.empty()) {
        name2 = v2->DebugString();
      }
      boolvar = MakeBoolVar(
          StringPrintf("IsEqualVar(%s, %s)", name1.c_str(), name2.c_str()));
      AddConstraint(MakeIsEqualCt(v1, v2, boolvar));
      model_cache_->InsertExprExprExpression(boolvar, v1, v2,
                                             ModelCache::EXPR_EXPR_IS_EQUAL);
    }
    return boolvar;
  }
}

Constraint* Solver::MakeIsEqualCt(IntExpr* const v1, IntExpr* const v2,
                                  IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsEqualCstCt(v2, v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsEqualCstCt(v1, v2->Min(), b);
  }
  if (b->Bound()) {
    if (b->Min() == 0) {
      return MakeNonEquality(v1, v2);
    } else {
      return MakeEquality(v1, v2);
    }
  }
  return RevAlloc(new IsEqualCt(this, v1, v2, b));
}

IntVar* Solver::MakeIsDifferentVar(IntExpr* const v1, IntExpr* const v2) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstVar(v2, v1->Min());
  } else if (v2->Bound()) {
    return MakeIsDifferentCstVar(v1, v2->Min());
  }
  IntExpr* cache = model_cache_->FindExprExprExpression(
      v1, v2, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
  if (cache == nullptr) {
    cache = model_cache_->FindExprExprExpression(
        v2, v1, ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
  }
  if (cache != nullptr) {
    return cache->Var();
  } else {
    IntVar* boolvar = nullptr;
    IntExpr* reverse_cache = model_cache_->FindExprExprExpression(
        v1, v2, ModelCache::EXPR_EXPR_IS_EQUAL);
    if (reverse_cache == nullptr) {
      reverse_cache = model_cache_->FindExprExprExpression(
          v2, v1, ModelCache::EXPR_EXPR_IS_EQUAL);
    }
    if (reverse_cache != nullptr) {
      boolvar = MakeDifference(1, reverse_cache)->Var();
    } else {
      std::string name1 = v1->name();
      if (name1.empty()) {
        name1 = v1->DebugString();
      }
      std::string name2 = v2->name();
      if (name2.empty()) {
        name2 = v2->DebugString();
      }
      boolvar = MakeBoolVar(
          StringPrintf("IsDifferentVar(%s, %s)", name1.c_str(), name2.c_str()));
      AddConstraint(MakeIsDifferentCt(v1, v2, boolvar));
    }
    model_cache_->InsertExprExprExpression(boolvar, v1, v2,
                                           ModelCache::EXPR_EXPR_IS_NOT_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsDifferentCt(IntExpr* const v1, IntExpr* const v2,
                                      IntVar* b) {
  CHECK_EQ(this, v1->solver());
  CHECK_EQ(this, v2->solver());
  if (v1->Bound()) {
    return MakeIsDifferentCstCt(v2, v1->Min(), b);
  } else if (v2->Bound()) {
    return MakeIsDifferentCstCt(v1, v2->Min(), b);
  }
  return RevAlloc(new IsDifferentCt(this, v1, v2, b));
}

IntVar* Solver::MakeIsLessOrEqualVar(IntExpr* const left,
                                     IntExpr* const right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstVar(right, left->Min());
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstVar(left, right->Min());
  }
  IntExpr* const cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_IS_LESS_OR_EQUAL);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    std::string name1 = left->name();
    if (name1.empty()) {
      name1 = left->DebugString();
    }
    std::string name2 = right->name();
    if (name2.empty()) {
      name2 = right->DebugString();
    }
    IntVar* const boolvar = MakeBoolVar(
        StringPrintf("IsLessOrEqual(%s, %s)", name1.c_str(), name2.c_str()));

    AddConstraint(RevAlloc(new IsLessOrEqualCt(this, left, right, boolvar)));
    model_cache_->InsertExprExprExpression(
        boolvar, left, right, ModelCache::EXPR_EXPR_IS_LESS_OR_EQUAL);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessOrEqualCt(IntExpr* const left,
                                        IntExpr* const right, IntVar* const b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterOrEqualCstCt(right, left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessOrEqualCstCt(left, right->Min(), b);
  }
  return RevAlloc(new IsLessOrEqualCt(this, left, right, b));
}

IntVar* Solver::MakeIsLessVar(IntExpr* const left, IntExpr* const right) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstVar(right, left->Min());
  } else if (right->Bound()) {
    return MakeIsLessCstVar(left, right->Min());
  }
  IntExpr* const cache = model_cache_->FindExprExprExpression(
      left, right, ModelCache::EXPR_EXPR_IS_LESS);
  if (cache != nullptr) {
    return cache->Var();
  } else {
    std::string name1 = left->name();
    if (name1.empty()) {
      name1 = left->DebugString();
    }
    std::string name2 = right->name();
    if (name2.empty()) {
      name2 = right->DebugString();
    }
    IntVar* const boolvar = MakeBoolVar(
        StringPrintf("IsLessOrEqual(%s, %s)", name1.c_str(), name2.c_str()));

    AddConstraint(RevAlloc(new IsLessCt(this, left, right, boolvar)));
    model_cache_->InsertExprExprExpression(boolvar, left, right,
                                           ModelCache::EXPR_EXPR_IS_LESS);
    return boolvar;
  }
}

Constraint* Solver::MakeIsLessCt(IntExpr* const left, IntExpr* const right,
                                 IntVar* const b) {
  CHECK_EQ(this, left->solver());
  CHECK_EQ(this, right->solver());
  if (left->Bound()) {
    return MakeIsGreaterCstCt(right, left->Min(), b);
  } else if (right->Bound()) {
    return MakeIsLessCstCt(left, right->Min(), b);
  }
  return RevAlloc(new IsLessCt(this, left, right, b));
}

IntVar* Solver::MakeIsGreaterOrEqualVar(IntExpr* const left,
                                        IntExpr* const right) {
  return MakeIsLessOrEqualVar(right, left);
}

Constraint* Solver::MakeIsGreaterOrEqualCt(IntExpr* const left,
                                           IntExpr* const right,
                                           IntVar* const b) {
  return MakeIsLessOrEqualCt(right, left, b);
}

IntVar* Solver::MakeIsGreaterVar(IntExpr* const left, IntExpr* const right) {
  return MakeIsLessVar(right, left);
}

Constraint* Solver::MakeIsGreaterCt(IntExpr* const left, IntExpr* const right,
                                    IntVar* const b) {
  return MakeIsLessCt(right, left, b);
}

}  // namespace operations_research
