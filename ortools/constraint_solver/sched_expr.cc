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

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355 4804 4805)
#endif

namespace operations_research {
namespace {
class IntervalVarStartExpr : public BaseIntExpr {
 public:
  explicit IntervalVarStartExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  ~IntervalVarStartExpr() override {}

  int64 Min() const override { return interval_->StartMin(); }

  void SetMin(int64 m) override { interval_->SetStartMin(m); }

  int64 Max() const override { return interval_->StartMax(); }

  void SetMax(int64 m) override { interval_->SetStartMax(m); }

  void SetRange(int64 l, int64 u) override { interval_->SetStartRange(l, u); }

  void SetValue(int64 v) override { interval_->SetStartRange(v, v); }

  bool Bound() const override {
    return interval_->StartMin() == interval_->StartMax();
  }

  void WhenRange(Demon* d) override { interval_->WhenStartRange(d); }

  std::string DebugString() const override {
    return StringPrintf("start(%s)", interval_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~IntervalVarEndExpr() override {}

  int64 Min() const override { return interval_->EndMin(); }

  void SetMin(int64 m) override { interval_->SetEndMin(m); }

  int64 Max() const override { return interval_->EndMax(); }

  void SetMax(int64 m) override { interval_->SetEndMax(m); }

  void SetRange(int64 l, int64 u) override { interval_->SetEndRange(l, u); }

  void SetValue(int64 v) override { interval_->SetEndRange(v, v); }

  bool Bound() const override {
    return interval_->EndMin() == interval_->EndMax();
  }

  void WhenRange(Demon* d) override { interval_->WhenEndRange(d); }

  std::string DebugString() const override {
    return StringPrintf("end(%s)", interval_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
  ~IntervalVarDurationExpr() override {}

  int64 Min() const override { return interval_->DurationMin(); }

  void SetMin(int64 m) override { interval_->SetDurationMin(m); }

  int64 Max() const override { return interval_->DurationMax(); }

  void SetMax(int64 m) override { interval_->SetDurationMax(m); }

  void SetRange(int64 l, int64 u) override {
    interval_->SetDurationRange(l, u);
  }

  void SetValue(int64 v) override { interval_->SetDurationRange(v, v); }

  bool Bound() const override {
    return interval_->DurationMin() == interval_->DurationMax();
  }

  void WhenRange(Demon* d) override { interval_->WhenDurationRange(d); }

  std::string DebugString() const override {
    return StringPrintf("duration(%s)", interval_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
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
