// Copyright 2010-2017 Google
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


#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {

// ----- interval <unary relation> date -----

namespace {
const char* kUnaryNames[] = {
    "ENDS_AFTER", "ENDS_AT",       "ENDS_BEFORE", "STARTS_AFTER",
    "STARTS_AT",  "STARTS_BEFORE", "CROSS_DATE",  "AVOID_DATE",
};

const char* kBinaryNames[] = {
    "ENDS_AFTER_END", "ENDS_AFTER_START", "ENDS_AT_END",
    "ENDS_AT_START",  "STARTS_AFTER_END", "STARTS_AFTER_START",
    "STARTS_AT_END",  "STARTS_AT_START",  "STAYS_IN_SYNC"};

class IntervalUnaryRelation : public Constraint {
 public:
  IntervalUnaryRelation(Solver* const s, IntervalVar* const t, int64 d,
                        Solver::UnaryIntervalRelation rel)
      : Constraint(s), t_(t), d_(d), rel_(rel) {}
  ~IntervalUnaryRelation() override {}

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override {
    return StringPrintf("(%s %s %" GG_LL_FORMAT "d)", t_->DebugString().c_str(),
                        kUnaryNames[rel_], d_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIntervalUnaryRelation, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, t_);
    visitor->VisitIntegerArgument(ModelVisitor::kRelationArgument, rel_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, d_);
    visitor->EndVisitConstraint(ModelVisitor::kIntervalUnaryRelation, this);
  }

 private:
  IntervalVar* const t_;
  const int64 d_;
  const Solver::UnaryIntervalRelation rel_;
};

void IntervalUnaryRelation::Post() {
  if (t_->MayBePerformed()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    t_->WhenAnything(d);
  }
}

void IntervalUnaryRelation::InitialPropagate() {
  if (t_->MayBePerformed()) {
    switch (rel_) {
      case Solver::ENDS_AFTER:
        t_->SetEndMin(d_);
        break;
      case Solver::ENDS_AT:
        t_->SetEndRange(d_, d_);
        break;
      case Solver::ENDS_BEFORE:
        t_->SetEndMax(d_);
        break;
      case Solver::STARTS_AFTER:
        t_->SetStartMin(d_);
        break;
      case Solver::STARTS_AT:
        t_->SetStartRange(d_, d_);
        break;

      case Solver::STARTS_BEFORE:
        t_->SetStartMax(d_);
        break;
      case Solver::CROSS_DATE:
        t_->SetStartMax(d_);
        t_->SetEndMin(d_);
        break;
      case Solver::AVOID_DATE:
        if (t_->EndMin() > d_) {
          t_->SetStartMin(d_);
        } else if (t_->StartMax() < d_) {
          t_->SetEndMax(d_);
        }
        break;
    }
  }
}
}  // namespace

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t,
                                            Solver::UnaryIntervalRelation r,
                                            int64 d) {
  return RevAlloc(new IntervalUnaryRelation(this, t, d, r));
}

// ----- interval <binary relation> interval -----

namespace {
class IntervalBinaryRelation : public Constraint {
 public:
  IntervalBinaryRelation(Solver* const s, IntervalVar* const t1,
                         IntervalVar* const t2,
                         Solver::BinaryIntervalRelation rel, int64 delay)
      : Constraint(s), t1_(t1), t2_(t2), rel_(rel), delay_(delay) {}
  ~IntervalBinaryRelation() override {}

  void Post() override;

  void InitialPropagate() override;

  std::string DebugString() const override {
    return StringPrintf("(%s %s %s)", t1_->DebugString().c_str(),
                        kBinaryNames[rel_], t2_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIntervalBinaryRelation, this);
    visitor->VisitIntervalArgument(ModelVisitor::kLeftArgument, t1_);
    visitor->VisitIntegerArgument(ModelVisitor::kRelationArgument, rel_);
    visitor->VisitIntervalArgument(ModelVisitor::kRightArgument, t2_);
    visitor->EndVisitConstraint(ModelVisitor::kIntervalBinaryRelation, this);
  }

 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  const Solver::BinaryIntervalRelation rel_;
  const int64 delay_;
};

void IntervalBinaryRelation::Post() {
  if (t1_->MayBePerformed() && t2_->MayBePerformed()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    t1_->WhenAnything(d);
    t2_->WhenAnything(d);
  }
}

// TODO(user) : make code more compact, use function pointers?
void IntervalBinaryRelation::InitialPropagate() {
  if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
    switch (rel_) {
      case Solver::ENDS_AFTER_END:
        t1_->SetEndMin(t2_->EndMin() + delay_);
        break;
      case Solver::ENDS_AFTER_START:
        t1_->SetEndMin(t2_->StartMin() + delay_);
        break;
      case Solver::ENDS_AT_END:
        t1_->SetEndRange(t2_->EndMin() + delay_, t2_->EndMax() + delay_);
        break;
      case Solver::ENDS_AT_START:
        t1_->SetEndRange(t2_->StartMin() + delay_, t2_->StartMax() + delay_);
        break;
      case Solver::STARTS_AFTER_END:
        t1_->SetStartMin(t2_->EndMin() + delay_);
        break;
      case Solver::STARTS_AFTER_START:
        t1_->SetStartMin(t2_->StartMin() + delay_);
        break;
      case Solver::STARTS_AT_END:
        t1_->SetStartRange(t2_->EndMin() + delay_, t2_->EndMax() + delay_);
        break;
      case Solver::STARTS_AT_START:
        t1_->SetStartRange(t2_->StartMin() + delay_, t2_->StartMax() + delay_);
        break;
      case Solver::STAYS_IN_SYNC:
        t1_->SetStartRange(t2_->StartMin() + delay_, t2_->StartMax() + delay_);
        t1_->SetEndRange(t2_->EndMin() + delay_, t2_->EndMax() + delay_);
        break;
    }
  }

  if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
    switch (rel_) {
      case Solver::ENDS_AFTER_END:
        t2_->SetEndMax(t1_->EndMax() - delay_);
        break;
      case Solver::ENDS_AFTER_START:
        t2_->SetStartMax(t1_->EndMax() - delay_);
        break;
      case Solver::ENDS_AT_END:
        t2_->SetEndRange(t1_->EndMin() - delay_, t1_->EndMax() - delay_);
        break;
      case Solver::ENDS_AT_START:
        t2_->SetStartRange(t1_->EndMin() - delay_, t1_->EndMax() - delay_);
        break;
      case Solver::STARTS_AFTER_END:
        t2_->SetEndMax(t1_->StartMax() - delay_);
        break;
      case Solver::STARTS_AFTER_START:
        t2_->SetStartMax(t1_->StartMax() - delay_);
        break;
      case Solver::STARTS_AT_END:
        t2_->SetEndRange(t1_->StartMin() - delay_, t1_->StartMax() - delay_);
        break;
      case Solver::STARTS_AT_START:
        t2_->SetStartRange(t1_->StartMin() - delay_, t1_->StartMax() - delay_);
        break;
      case Solver::STAYS_IN_SYNC:
        t2_->SetStartRange(t1_->StartMin() - delay_, t1_->StartMax() - delay_);
        t2_->SetEndRange(t1_->EndMin() - delay_, t1_->EndMax() - delay_);
        break;
    }
  }
}
}  // namespace

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t1,
                                            Solver::BinaryIntervalRelation r,
                                            IntervalVar* const t2) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, 0));
}

Constraint* Solver::MakeIntervalVarRelationWithDelay(
    IntervalVar* const t1, Solver::BinaryIntervalRelation r,
    IntervalVar* const t2, int64 delay) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r, delay));
}

// ----- activity a before activity b or activity b before activity a -----

namespace {
class TemporalDisjunction : public Constraint {
 public:
  enum State { ONE_BEFORE_TWO, TWO_BEFORE_ONE, UNDECIDED };

  TemporalDisjunction(Solver* const s, IntervalVar* const t1,
                      IntervalVar* const t2, IntVar* const alt)
      : Constraint(s), t1_(t1), t2_(t2), alt_(alt), state_(UNDECIDED) {}
  ~TemporalDisjunction() override {}

  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

  void RangeDemon1();
  void RangeDemon2();
  void RangeAlt();
  void Decide(State s);
  void TryToDecide();

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kIntervalDisjunction, this);
    visitor->VisitIntervalArgument(ModelVisitor::kLeftArgument, t1_);
    visitor->VisitIntervalArgument(ModelVisitor::kRightArgument, t2_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            alt_);
    visitor->EndVisitConstraint(ModelVisitor::kIntervalDisjunction, this);
  }

 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  IntVar* const alt_;
  State state_;
};

void TemporalDisjunction::Post() {
  Solver* const s = solver();
  Demon* d = MakeConstraintDemon0(s, this, &TemporalDisjunction::RangeDemon1,
                                  "RangeDemon1");
  t1_->WhenAnything(d);
  d = MakeConstraintDemon0(s, this, &TemporalDisjunction::RangeDemon2,
                           "RangeDemon2");
  t2_->WhenAnything(d);
  if (alt_ != nullptr) {
    d = MakeConstraintDemon0(s, this, &TemporalDisjunction::RangeAlt,
                             "RangeAlt");
    alt_->WhenRange(d);
  }
}

void TemporalDisjunction::InitialPropagate() {
  if (alt_ != nullptr) {
    alt_->SetRange(0, 1);
  }
  if (alt_ != nullptr && alt_->Bound()) {
    RangeAlt();
  } else {
    RangeDemon1();
    RangeDemon2();
  }
}

std::string TemporalDisjunction::DebugString() const {
  std::string out;
  SStringPrintf(&out, "TemporalDisjunction(%s, %s", t1_->DebugString().c_str(),
                t2_->DebugString().c_str());
  if (alt_ != nullptr) {
    StringAppendF(&out, " => %s", alt_->DebugString().c_str());
  }
  out += ") ";
  return out;
}

void TemporalDisjunction::TryToDecide() {
  DCHECK_EQ(UNDECIDED, state_);
  if (t1_->MayBePerformed() && t2_->MayBePerformed() &&
      (t1_->MustBePerformed() || t2_->MustBePerformed())) {
    if (t1_->EndMin() > t2_->StartMax()) {
      Decide(TWO_BEFORE_ONE);
    } else if (t2_->EndMin() > t1_->StartMax()) {
      Decide(ONE_BEFORE_TWO);
    }
  }
}

void TemporalDisjunction::RangeDemon1() {
  switch (state_) {
    case ONE_BEFORE_TWO: {
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetStartMin(t1_->EndMin());
      }
      break;
    }
    case TWO_BEFORE_ONE: {
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndMax(t1_->StartMax());
      }
      break;
    }
    case UNDECIDED: {
      TryToDecide();
    }
  }
}

void TemporalDisjunction::RangeDemon2() {
  if (t1_->MayBePerformed() || t2_->MayBePerformed()) {
    switch (state_) {
      case ONE_BEFORE_TWO: {
        if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
          t1_->SetEndMax(t2_->StartMax());
        }
        break;
      }
      case TWO_BEFORE_ONE: {
        if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
          t1_->SetStartMin(t2_->EndMin());
        }
        break;
      }
      case UNDECIDED: {
        TryToDecide();
      }
    }
  }
}

void TemporalDisjunction::RangeAlt() {
  DCHECK(alt_ != nullptr);
  if (alt_->Value() == 0) {
    Decide(ONE_BEFORE_TWO);
  } else {
    Decide(TWO_BEFORE_ONE);
  }
}

void TemporalDisjunction::Decide(State s) {
  // Should Decide on a fixed state?
  DCHECK_NE(s, UNDECIDED);
  if (state_ != UNDECIDED && state_ != s) {
    solver()->Fail();
  }
  solver()->SaveValue(reinterpret_cast<int*>(&state_));
  state_ = s;
  if (alt_ != nullptr) {
    if (s == ONE_BEFORE_TWO) {
      alt_->SetValue(0);
    } else {
      alt_->SetValue(1);
    }
  }
  RangeDemon1();
  RangeDemon2();
}
}  // namespace

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2,
                                            IntVar* const alt) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, alt));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, nullptr));
}

}  // namespace operations_research
