// Copyright 2010-2011 Google
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

#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

//-----------------------------------------------------------------------------
// RangeEquality

namespace {
class RangeEquality : public Constraint {
 public:
  RangeEquality(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~RangeEquality() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kEquality, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kEquality, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

RangeEquality::RangeEquality(Solver* const s, IntVar* const l, IntVar* const r)
  : Constraint(s), left_(l), right_(r) {}

void RangeEquality::Post() {
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeEquality::InitialPropagate() {
  left_->SetRange(right_->Min(), right_->Max());
  right_->SetRange(left_->Min(), left_->Max());
}

string RangeEquality::DebugString() const {
  return left_->DebugString() + " == " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeEquality(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new RangeEquality(this, l, r));
}

//-----------------------------------------------------------------------------
// RangeLessOrEqual

namespace {
class RangeLessOrEqual : public Constraint {
 public:
  RangeLessOrEqual(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~RangeLessOrEqual() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

RangeLessOrEqual::RangeLessOrEqual(Solver* const s, IntVar* const l,
                                   IntVar* const r)
  : Constraint(s), left_(l), right_(r) {}

void RangeLessOrEqual::Post() {
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeLessOrEqual::InitialPropagate() {
  left_->SetMax(right_->Max());
  right_->SetMin(left_->Min());
}

string RangeLessOrEqual::DebugString() const {
  return left_->DebugString() + " <= " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeLessOrEqual(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new RangeLessOrEqual(this, l, r));
}

//-----------------------------------------------------------------------------
// RangeGreaterOrEqual

namespace {
class RangeGreaterOrEqual : public Constraint {
 public:
  RangeGreaterOrEqual(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~RangeGreaterOrEqual() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kGreaterOrEqual, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

RangeGreaterOrEqual::RangeGreaterOrEqual(Solver* const s, IntVar* const l,
                                         IntVar* const r)
    : Constraint(s), left_(l), right_(r) {}

void RangeGreaterOrEqual::Post() {
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeGreaterOrEqual::InitialPropagate() {
  left_->SetMin(right_->Min());
  right_->SetMax(left_->Max());
}

string RangeGreaterOrEqual::DebugString() const {
  return left_->DebugString() + " >= " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeGreaterOrEqual(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new RangeGreaterOrEqual(this, l, r));
}

//-----------------------------------------------------------------------------
// RangeLess

namespace {
class RangeLess : public Constraint {
 public:
  RangeLess(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~RangeLess() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLess, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLess, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

RangeLess::RangeLess(Solver* const s, IntVar* const l, IntVar* const r)
  : Constraint(s), left_(l), right_(r) {}

void RangeLess::Post() {
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeLess::InitialPropagate() {
  left_->SetMax(right_->Max() - 1);
  right_->SetMin(left_->Min() + 1);
}

string RangeLess::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeLess(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new RangeLess(this, l, r));
}

//-----------------------------------------------------------------------------
// RangeGreater

namespace {
class RangeGreater : public Constraint {
 public:
  RangeGreater(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~RangeGreater() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kGreater, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kGreater, this);
  }

 private:
  IntVar* const left_;
  IntVar* const right_;
};

RangeGreater::RangeGreater(Solver* const s, IntVar* const l, IntVar* const r)
  : Constraint(s), left_(l), right_(r) {}

void RangeGreater::Post() {
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenRange(d);
  right_->WhenRange(d);
}

void RangeGreater::InitialPropagate() {
  left_->SetMin(right_->Min() + 1);
  right_->SetMax(left_->Max() - 1);
}

string RangeGreater::DebugString() const {
  return left_->DebugString() + " > " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeGreater(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new RangeGreater(this, l, r));
}

//-----------------------------------------------------------------------------
// DiffVar

namespace {
class DiffVar : public Constraint {
 public:
  DiffVar(Solver* const s, IntVar* const l, IntVar* const r);
  virtual ~DiffVar() {}
  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;
  virtual void Accept(ModelVisitor* const visitor) const {
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
  Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
  left_->WhenBound(d);
  right_->WhenBound(d);
  // TODO(user) : improve me, separated demons, actually to test
}

void DiffVar::InitialPropagate() {
  if (left_->Bound()) {
    right_->RemoveValue(left_->Min());  // we use min instead of value
    // because we do not check again if the variable is bound
    // when the variable is bound, Min() == Max() == Value()
  }
  if (right_->Bound()) {
    left_->RemoveValue(right_->Min());  // see above
  }
}

string DiffVar::DebugString() const {
  return left_->DebugString() + " != " + right_->DebugString();
}
}  // namespace

Constraint* Solver::MakeNonEquality(IntVar* const l, IntVar* const r) {
  CHECK(l != NULL) << "left expression NULL, maybe a bad cast";
  CHECK(r != NULL) << "left expression NULL, maybe a bad cast";
  CHECK_EQ(this, l->solver());
  CHECK_EQ(this, r->solver());
  return RevAlloc(new DiffVar(this, l, r));
}

}  // namespace operations_research
