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

  virtual std::string DebugString() const {
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

  virtual std::string DebugString() const {
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

  virtual std::string DebugString() const {
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
}  // namespace

// ----- API -----

IntExpr* BuildStartExpr(IntervalVar* var) {
  Solver* const s = var->solver();
  IntExpr* const expr =
      s->RegisterIntExpr(s->RevAlloc(new IntervalVarStartExpr(var)));
  if (var->HasName()) {
    expr->set_name(StringPrintf("start<%s>", var->name().c_str()));
  }
  return expr;
}

IntExpr* BuildDurationExpr(IntervalVar* var) {
  Solver* const s = var->solver();
  IntExpr* const expr =
      s->RegisterIntExpr(s->RevAlloc(new IntervalVarDurationExpr(var)));
  if (var->HasName()) {
    expr->set_name(StringPrintf("duration<%s>", var->name().c_str()));
  }
  return expr;
}

IntExpr* BuildEndExpr(IntervalVar* var) {
  Solver* const s = var->solver();
  IntExpr* const expr =
      s->RegisterIntExpr(s->RevAlloc(new IntervalVarEndExpr(var)));
  if (var->HasName()) {
    expr->set_name(StringPrintf("end<%s>", var->name().c_str()));
  }
  return expr;
}

IntExpr* BuildSafeStartExpr(IntervalVar* var, int64 unperformed_value) {
  return var->solver()->MakeConditionalExpression(
      var->PerformedExpr()->Var(), var->StartExpr(), unperformed_value);
}

IntExpr* BuildSafeDurationExpr(IntervalVar* var, int64 unperformed_value) {
  return var->solver()->MakeConditionalExpression(
      var->PerformedExpr()->Var(), var->DurationExpr(), unperformed_value);
}

IntExpr* BuildSafeEndExpr(IntervalVar* var, int64 unperformed_value) {
  return var->solver()->MakeConditionalExpression(
      var->PerformedExpr()->Var(), var->EndExpr(), unperformed_value);
}
}  // namespace operations_research
