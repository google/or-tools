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
class IntervalVarSafeStartExpr : public BaseIntExpr {
 public:
  IntervalVarSafeStartExpr(IntervalVar* const i,
                           int64 unperformed_value)
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
    } else if (interval_->MustBePerformed()) {
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
    } else if (interval_->MustBePerformed()) {
      interval_->SetStartMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetStartRange(v, v);
    } else if (interval_->MustBePerformed()) {
      interval_->SetStartRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
        interval_->StartMin() == interval_->StartMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenStartRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_end(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kStartExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument,
                                   interval_);
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
  IntervalVarSafeDurationExpr(IntervalVar* const i,
                              int64 unperformed_value)
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
    } else if (interval_->MustBePerformed()) {
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
    } else if (interval_->MustBePerformed()) {
      interval_->SetDurationMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetDurationRange(v, v);
    } else if (interval_->MustBePerformed()) {
      interval_->SetDurationRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
        interval_->DurationMin() == interval_->DurationMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenDurationRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_end(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kDurationExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument,
                                   interval_);
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
  IntervalVarSafeEndExpr(IntervalVar* const i,
                         int64 unperformed_value)
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
    } else if (interval_->MustBePerformed()) {
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
    } else if (interval_->MustBePerformed()) {
      interval_->SetEndMax(m);
    }
  }

  virtual void SetValue(int64 v) {
    if (v != unperformed_value_) {
      interval_->SetPerformed(true);
      interval_->SetEndRange(v, v);
    } else if (interval_->MustBePerformed()) {
      interval_->SetEndRange(v, v);
    }
  }

  virtual bool Bound() const {
    return !interval_->MustBePerformed() ||
        interval_->EndMin() == interval_->EndMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenEndRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("safe_end(%s)", interval_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kEndExpr, this);
    visitor->VisitIntervalArgument(ModelVisitor::kIntervalArgument,
                                   interval_);
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

IntExpr* IntervalVar::SafeStartExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(solver()->RevAlloc(
      new IntervalVarSafeStartExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_start(%s)", name().c_str()));
  }
  return end_expr;
}

IntExpr* IntervalVar::SafeDurationExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(solver()->RevAlloc(
      new IntervalVarSafeDurationExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_start(%s)", name().c_str()));
  }
  return end_expr;
}

IntExpr* IntervalVar::SafeEndExpr(int64 unperformed_value) {
  IntExpr* const end_expr = solver()->RegisterIntExpr(
      solver()->RevAlloc(new IntervalVarSafeEndExpr(this, unperformed_value)));
  if (HasName()) {
    end_expr->set_name(StringPrintf("safe_end(%s)", name().c_str()));
  }
  return end_expr;
}
}  // namespace operations_research
