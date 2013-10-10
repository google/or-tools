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
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355 4804 4805)
#endif

namespace operations_research {
namespace {
class IntervalVarStartExpr : public BaseIntExpr {
 public:
  explicit IntervalVarStartExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  virtual ~IntervalVarStartExpr() {}

  virtual int64 Min() const { return interval_->StartMin(); }

  virtual void SetMin(int64 m) { interval_->SetStartMin(m); }

  virtual int64 Max() const { return interval_->StartMax(); }

  virtual void SetMax(int64 m) { interval_->SetStartMax(m); }

  virtual void SetRange(int64 l, int64 u) { interval_->SetStartRange(l, u); }

  virtual void SetValue(int64 v) { interval_->SetStartRange(v, v); }

  virtual bool Bound() const {
    return interval_->StartMin() == interval_->StartMax();
  }

  virtual void WhenRange(Demon* d) { interval_->WhenStartRange(d); }

  virtual string DebugString() const {
    return StringPrintf("start(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kStartExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kStartExpr, this);
  }

 private:
  IntervalVar* interval_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarStartExpr);
};

class IntervalVarEndExpr : public BaseIntExpr {
 public:
  explicit IntervalVarEndExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  virtual ~IntervalVarEndExpr() {}

  virtual int64 Min() const { return interval_->EndMin(); }

  virtual void SetMin(int64 m) { interval_->SetEndMin(m); }

  virtual int64 Max() const { return interval_->EndMax(); }

  virtual void SetMax(int64 m) { interval_->SetEndMax(m); }

  virtual void SetRange(int64 l, int64 u) { interval_->SetEndRange(l, u); }

  virtual void SetValue(int64 v) { interval_->SetEndRange(v, v); }

  virtual bool Bound() const {
    return interval_->EndMin() == interval_->EndMax();
  }

  virtual void WhenRange(Demon* d) { interval_->WhenEndRange(d); }

  virtual string DebugString() const {
    return StringPrintf("end(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kEndExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kEndExpr, this);
  }

 private:
  IntervalVar* interval_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarEndExpr);
};

class IntervalVarDurationExpr : public BaseIntExpr {
 public:
  explicit IntervalVarDurationExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  virtual ~IntervalVarDurationExpr() {}

  virtual int64 Min() const { return interval_->DurationMin(); }

  virtual void SetMin(int64 m) { interval_->SetDurationMin(m); }

  virtual int64 Max() const { return interval_->DurationMax(); }

  virtual void SetMax(int64 m) { interval_->SetDurationMax(m); }

  virtual void SetRange(int64 l, int64 u) { interval_->SetDurationRange(l, u); }

  virtual void SetValue(int64 v) { interval_->SetDurationRange(v, v); }

  virtual bool Bound() const {
    return interval_->DurationMin() == interval_->DurationMax();
  }

  virtual void WhenRange(Demon* d) { interval_->WhenDurationRange(d); }

  virtual string DebugString() const {
    return StringPrintf("duration(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDurationExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDurationExpr, this);
  }

 private:
  IntervalVar* interval_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarDurationExpr);
};

class IntervalVarPerformedExpr : public BaseIntExpr {
 public:
  explicit IntervalVarPerformedExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  virtual ~IntervalVarPerformedExpr() {}

  virtual int64 Min() const {
    // Returns 0ll or 1ll
    return static_cast<int64>(interval_->MustBePerformed());
  }

  virtual void SetMin(int64 m) {
    if (m == 1) {
      interval_->SetPerformed(true);
    } else if (m > 1) {
      solver()->Fail();
    }
  }

  virtual int64 Max() const {
    // Returns 0ll or 1ll
    return static_cast<int64>(interval_->MayBePerformed());
  }

  virtual void SetMax(int64 m) {
    if (m == 0) {
      interval_->SetPerformed(false);
    } else if (m < 0) {
      solver()->Fail();
    }
  }

  virtual void SetRange(int64 l, int64 u) {
    SetMin(l);
    SetMax(u);
  }

  virtual void SetValue(int64 v) { SetRange(v, v); }

  virtual bool Bound() const { return interval_->IsPerformedBound(); }

  virtual void WhenRange(Demon* d) { interval_->WhenPerformedBound(d); }

  virtual string DebugString() const {
    return StringPrintf("performed(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kPerformedExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kPerformedExpr, this);
  }

 private:
  IntervalVar* interval_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarPerformedExpr);
};

class IntervalVarSafeStartExpr : public BaseIntExpr {
 public:
  IntervalVarSafeStartExpr(IntervalVar* const i, int64 unperformed_value)
      : BaseIntExpr(i->solver()),
        interval_(i),
        unperformed_value_(unperformed_value) {}

  virtual ~IntervalVarSafeStartExpr() {}

  virtual int64 Min() const {
    if (interval_->MustBePerformed()) {
      return interval_->StartMin();
    } else if (interval_->MayBePerformed()) {
      return std::min(unperformed_value_, interval_->StartMin());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMin(int64 m) {
    if (m > unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetStartMin(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetStartMin(m);
    }
  }

  virtual int64 Max() const {
    if (interval_->MustBePerformed()) {
      return interval_->StartMax();
    } else if (interval_->MayBePerformed()) {
      return std::max(unperformed_value_, interval_->StartMax());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMax(int64 m) {
    if (m < unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetStartMax(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetStartMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetStartRange(v, v);
    } else if (interval_->MayBePerformed()) {
      interval_->SetStartRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
           interval_->StartMin() == interval_->StartMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenStartRange(d);
    interval_->WhenPerformedBound(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_start(%s, %" GG_LL_FORMAT "d)",
                        interval_->DebugString().c_str(), unperformed_value_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kStartExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  unperformed_value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kStartExpr, this);
  }

 private:
  IntervalVar* const interval_;
  const int64 unperformed_value_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarSafeStartExpr);
};

class IntervalVarSafeDurationExpr : public BaseIntExpr {
 public:
  IntervalVarSafeDurationExpr(IntervalVar* const i, int64 unperformed_value)
      : BaseIntExpr(i->solver()),
        interval_(i),
        unperformed_value_(unperformed_value) {}

  virtual ~IntervalVarSafeDurationExpr() {}

  virtual int64 Min() const {
    if (interval_->MustBePerformed()) {
      return interval_->DurationMin();
    } else if (interval_->MayBePerformed()) {
      return std::min(unperformed_value_, interval_->DurationMin());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMin(int64 m) {
    if (m > unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetDurationMin(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetDurationMin(m);
    }
  }

  virtual int64 Max() const {
    if (interval_->MustBePerformed()) {
      return interval_->DurationMax();
    } else if (interval_->MayBePerformed()) {
      return std::max(unperformed_value_, interval_->DurationMax());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMax(int64 m) {
    if (m < unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetDurationMax(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetDurationMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetDurationRange(v, v);
    } else if (interval_->MayBePerformed()) {
      interval_->SetDurationRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
           interval_->DurationMin() == interval_->DurationMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenDurationRange(d);
    interval_->WhenPerformedBound(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_duration(%s, %" GG_LL_FORMAT "d)",
                        interval_->DebugString().c_str(), unperformed_value_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDurationExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  unperformed_value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kDurationExpr, this);
  }

 private:
  IntervalVar* const interval_;
  const int64 unperformed_value_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarSafeDurationExpr);
};

class IntervalVarSafeEndExpr : public BaseIntExpr {
 public:
  IntervalVarSafeEndExpr(IntervalVar* const i, int64 unperformed_value)
      : BaseIntExpr(i->solver()),
        interval_(i),
        unperformed_value_(unperformed_value) {}

  virtual ~IntervalVarSafeEndExpr() {}

  virtual int64 Min() const {
    if (interval_->MustBePerformed()) {
      return interval_->EndMin();
    } else if (interval_->MayBePerformed()) {
      return std::min(unperformed_value_, interval_->EndMin());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMin(int64 m) {
    if (m > unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetEndMin(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetEndMin(m);
    }
  }

  virtual int64 Max() const {
    if (interval_->MustBePerformed()) {
      return interval_->EndMax();
    } else if (interval_->MayBePerformed()) {
      return std::max(unperformed_value_, interval_->EndMax());
    } else {
      return unperformed_value_;
    }
  }

  virtual void SetMax(int64 m) {
    if (m < unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetEndMax(m);
    } else if (interval_->MayBePerformed()) {
      interval_->SetEndMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetEndRange(v, v);
    } else if (interval_->MayBePerformed()) {
      interval_->SetEndRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
           interval_->EndMin() == interval_->EndMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenEndRange(d);
    interval_->WhenPerformedBound(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_end(%s, %" GG_LL_FORMAT "d)",
                        interval_->DebugString().c_str(), unperformed_value_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kEndExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument, interval_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument,
                                  unperformed_value_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kEndExpr, this);
  }

 private:
  IntervalVar* const interval_;
  const int64 unperformed_value_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarSafeEndExpr);
};
}  // namespace

// ----- API -----

IntExpr* IntervalVar::StartExpr() {
  if (start_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&start_expr_));
    start_expr_ = solver()->RegisterIntExpr(
        solver()->RevAlloc(new IntervalVarStartExpr(this)));
    if (HasName()) {
      start_expr_->set_name(StringPrintf("start(%s)", name().c_str()));
    }
  }
  return start_expr_;
}

IntExpr* IntervalVar::DurationExpr() {
  if (duration_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&duration_expr_));
    duration_expr_ = solver()->RegisterIntExpr(
        solver()->RevAlloc(new IntervalVarDurationExpr(this)));
    if (HasName()) {
      duration_expr_->set_name(StringPrintf("duration(%s)", name().c_str()));
    }
  }
  return duration_expr_;
}

IntExpr* IntervalVar::EndExpr() {
  if (end_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&end_expr_));
    end_expr_ = solver()->RegisterIntExpr(
        solver()->RevAlloc(new IntervalVarEndExpr(this)));
    if (HasName()) {
      end_expr_->set_name(StringPrintf("end(%s)", name().c_str()));
    }
  }
  return end_expr_;
}

IntExpr* IntervalVar::PerformedExpr() {
  if (performed_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&performed_expr_));
    performed_expr_ = solver()->RegisterIntExpr(
        solver()->RevAlloc(new IntervalVarPerformedExpr(this)));
    if (HasName()) {
      performed_expr_->set_name(StringPrintf("performed(%s)", name().c_str()));
    }
  }
  return performed_expr_;
}

IntExpr* IntervalVar::SafeStartExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(solver()->RevAlloc(
      new IntervalVarSafeStartExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_start(%s, %" GG_LL_FORMAT "d)",
                                    name().c_str(), unperformed_value));
  }
  return end_expr;
}

IntExpr* IntervalVar::SafeDurationExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(solver()->RevAlloc(
      new IntervalVarSafeDurationExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_duration(%s, %" GG_LL_FORMAT "d)",
                                    name().c_str(), unperformed_value));
  }
  return end_expr;
}

IntExpr* IntervalVar::SafeEndExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(
      solver()->RevAlloc(new IntervalVarSafeEndExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_end(%s, %" GG_LL_FORMAT "d)",
                                    name().c_str(), unperformed_value));
  }
  return end_expr;
}
}  // namespace operations_research
