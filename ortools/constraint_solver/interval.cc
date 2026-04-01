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

#include "ortools/constraint_solver/interval.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraints.h"
#include "ortools/constraint_solver/sched_expr.h"
#include "ortools/constraint_solver/variables.h"
#include "ortools/util/saturated_arithmetic.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355 4804 4805)
#endif

namespace operations_research {

IntervalVar::IntervalVar(Solver* solver, const std::string& name)
    : PropagationBaseObject(solver) {
  set_name(name);
}

IntervalVar* NullInterval() { return nullptr; }

// ----- MirrorIntervalVar -----

MirrorIntervalVar::MirrorIntervalVar(Solver* s, IntervalVar* t)
    : IntervalVar(s, "Mirror<" + t->name() + ">"), t_(t) {}

MirrorIntervalVar::~MirrorIntervalVar() {}

// These methods query, set and watch the start position of the
// interval var.
int64_t MirrorIntervalVar::StartMin() const { return -t_->EndMax(); }
int64_t MirrorIntervalVar::StartMax() const { return -t_->EndMin(); }
void MirrorIntervalVar::SetStartMin(int64_t m) { t_->SetEndMax(-m); }
void MirrorIntervalVar::SetStartMax(int64_t m) { t_->SetEndMin(-m); }
void MirrorIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  t_->SetEndRange(-ma, -mi);
}
int64_t MirrorIntervalVar::OldStartMin() const { return -t_->OldEndMax(); }
int64_t MirrorIntervalVar::OldStartMax() const { return -t_->OldEndMin(); }
void MirrorIntervalVar::WhenStartRange(Demon* d) { t_->WhenEndRange(d); }
void MirrorIntervalVar::WhenStartBound(Demon* d) { t_->WhenEndBound(d); }

// These methods query, set and watch the duration of the interval var.
int64_t MirrorIntervalVar::DurationMin() const { return t_->DurationMin(); }
int64_t MirrorIntervalVar::DurationMax() const { return t_->DurationMax(); }
void MirrorIntervalVar::SetDurationMin(int64_t m) { t_->SetDurationMin(m); }
void MirrorIntervalVar::SetDurationMax(int64_t m) { t_->SetDurationMax(m); }
void MirrorIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  t_->SetDurationRange(mi, ma);
}
int64_t MirrorIntervalVar::OldDurationMin() const {
  return t_->OldDurationMin();
}
int64_t MirrorIntervalVar::OldDurationMax() const {
  return t_->OldDurationMax();
}
void MirrorIntervalVar::WhenDurationRange(Demon* d) {
  t_->WhenDurationRange(d);
}
void MirrorIntervalVar::WhenDurationBound(Demon* d) {
  t_->WhenDurationBound(d);
}

// These methods query, set and watch the end position of the interval var.
int64_t MirrorIntervalVar::EndMin() const { return -t_->StartMax(); }
int64_t MirrorIntervalVar::EndMax() const { return -t_->StartMin(); }
void MirrorIntervalVar::SetEndMin(int64_t m) { t_->SetStartMax(-m); }
void MirrorIntervalVar::SetEndMax(int64_t m) { t_->SetStartMin(-m); }
void MirrorIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  t_->SetStartRange(-ma, -mi);
}
int64_t MirrorIntervalVar::OldEndMin() const { return -t_->OldStartMax(); }
int64_t MirrorIntervalVar::OldEndMax() const { return -t_->OldStartMin(); }
void MirrorIntervalVar::WhenEndRange(Demon* d) { t_->WhenStartRange(d); }
void MirrorIntervalVar::WhenEndBound(Demon* d) { t_->WhenStartBound(d); }

// These methods query, set and watches the performed status of the
// interval var.
bool MirrorIntervalVar::MustBePerformed() const {
  return t_->MustBePerformed();
}
bool MirrorIntervalVar::MayBePerformed() const { return t_->MayBePerformed(); }
void MirrorIntervalVar::SetPerformed(bool val) { t_->SetPerformed(val); }
bool MirrorIntervalVar::WasPerformedBound() const {
  return t_->WasPerformedBound();
}
void MirrorIntervalVar::WhenPerformedBound(Demon* d) {
  t_->WhenPerformedBound(d);
}

void MirrorIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, ModelVisitor::kMirrorOperation, 0, t_);
}

std::string MirrorIntervalVar::DebugString() const {
  return absl::StrFormat("MirrorInterval(%s)", t_->DebugString());
}

IntExpr* MirrorIntervalVar::StartExpr() {
  return solver()->MakeOpposite(t_->EndExpr());
}
IntExpr* MirrorIntervalVar::DurationExpr() { return t_->DurationExpr(); }
IntExpr* MirrorIntervalVar::EndExpr() {
  return solver()->MakeOpposite(t_->StartExpr());
}
IntExpr* MirrorIntervalVar::PerformedExpr() { return t_->PerformedExpr(); }

// These methods create expressions encapsulating the start, end
// and duration of the interval var. If the interval var is
// unperformed, they will return the unperformed_value.
IntExpr* MirrorIntervalVar::SafeStartExpr(int64_t unperformed_value) {
  return solver()->MakeOpposite(t_->SafeEndExpr(-unperformed_value));
}
IntExpr* MirrorIntervalVar::SafeDurationExpr(int64_t unperformed_value) {
  return t_->SafeDurationExpr(unperformed_value);
}
IntExpr* MirrorIntervalVar::SafeEndExpr(int64_t unperformed_value) {
  return solver()->MakeOpposite(t_->SafeStartExpr(-unperformed_value));
}

// ----- AlwaysPerformedIntervalVarWrapper -----

AlwaysPerformedIntervalVarWrapper::AlwaysPerformedIntervalVarWrapper(
    IntervalVar* t)
    : IntervalVar(t->solver(),
                  absl::StrFormat("AlwaysPerformed<%s>", t->name())),
      t_(t),
      start_expr_(nullptr),
      duration_expr_(nullptr),
      end_expr_(nullptr) {}

AlwaysPerformedIntervalVarWrapper::~AlwaysPerformedIntervalVarWrapper() {}

int64_t AlwaysPerformedIntervalVarWrapper::StartMin() const {
  return MayUnderlyingBePerformed() ? t_->StartMin() : kMinValidValue;
}
int64_t AlwaysPerformedIntervalVarWrapper::StartMax() const {
  return MayUnderlyingBePerformed() ? t_->StartMax() : kMaxValidValue;
}
void AlwaysPerformedIntervalVarWrapper::SetStartMin(int64_t m) {
  t_->SetStartMin(m);
}
void AlwaysPerformedIntervalVarWrapper::SetStartMax(int64_t m) {
  t_->SetStartMax(m);
}
void AlwaysPerformedIntervalVarWrapper::SetStartRange(int64_t mi, int64_t ma) {
  t_->SetStartRange(mi, ma);
}
int64_t AlwaysPerformedIntervalVarWrapper::OldStartMin() const {
  return MayUnderlyingBePerformed() ? t_->OldStartMin() : kMinValidValue;
}
int64_t AlwaysPerformedIntervalVarWrapper::OldStartMax() const {
  return MayUnderlyingBePerformed() ? t_->OldStartMax() : kMaxValidValue;
}
void AlwaysPerformedIntervalVarWrapper::WhenStartRange(Demon* d) {
  t_->WhenStartRange(d);
}
void AlwaysPerformedIntervalVarWrapper::WhenStartBound(Demon* d) {
  t_->WhenStartBound(d);
}
int64_t AlwaysPerformedIntervalVarWrapper::DurationMin() const {
  return MayUnderlyingBePerformed() ? t_->DurationMin() : 0LL;
}
int64_t AlwaysPerformedIntervalVarWrapper::DurationMax() const {
  return MayUnderlyingBePerformed() ? t_->DurationMax() : 0LL;
}
void AlwaysPerformedIntervalVarWrapper::SetDurationMin(int64_t m) {
  t_->SetDurationMin(m);
}
void AlwaysPerformedIntervalVarWrapper::SetDurationMax(int64_t m) {
  t_->SetDurationMax(m);
}
void AlwaysPerformedIntervalVarWrapper::SetDurationRange(int64_t mi,
                                                         int64_t ma) {
  t_->SetDurationRange(mi, ma);
}
int64_t AlwaysPerformedIntervalVarWrapper::OldDurationMin() const {
  return MayUnderlyingBePerformed() ? t_->OldDurationMin() : 0LL;
}
int64_t AlwaysPerformedIntervalVarWrapper::OldDurationMax() const {
  return MayUnderlyingBePerformed() ? t_->OldDurationMax() : 0LL;
}
void AlwaysPerformedIntervalVarWrapper::WhenDurationRange(Demon* d) {
  t_->WhenDurationRange(d);
}
void AlwaysPerformedIntervalVarWrapper::WhenDurationBound(Demon* d) {
  t_->WhenDurationBound(d);
}
int64_t AlwaysPerformedIntervalVarWrapper::EndMin() const {
  return MayUnderlyingBePerformed() ? t_->EndMin() : kMinValidValue;
}
int64_t AlwaysPerformedIntervalVarWrapper::EndMax() const {
  return MayUnderlyingBePerformed() ? t_->EndMax() : kMaxValidValue;
}
void AlwaysPerformedIntervalVarWrapper::SetEndMin(int64_t m) {
  t_->SetEndMin(m);
}
void AlwaysPerformedIntervalVarWrapper::SetEndMax(int64_t m) {
  t_->SetEndMax(m);
}
void AlwaysPerformedIntervalVarWrapper::SetEndRange(int64_t mi, int64_t ma) {
  t_->SetEndRange(mi, ma);
}
int64_t AlwaysPerformedIntervalVarWrapper::OldEndMin() const {
  return MayUnderlyingBePerformed() ? t_->OldEndMin() : kMinValidValue;
}
int64_t AlwaysPerformedIntervalVarWrapper::OldEndMax() const {
  return MayUnderlyingBePerformed() ? t_->OldEndMax() : kMaxValidValue;
}
void AlwaysPerformedIntervalVarWrapper::WhenEndRange(Demon* d) {
  t_->WhenEndRange(d);
}
void AlwaysPerformedIntervalVarWrapper::WhenEndBound(Demon* d) {
  t_->WhenEndBound(d);
}
bool AlwaysPerformedIntervalVarWrapper::MustBePerformed() const { return true; }
bool AlwaysPerformedIntervalVarWrapper::MayBePerformed() const { return true; }
void AlwaysPerformedIntervalVarWrapper::SetPerformed(bool val) {
  // An AlwaysPerformedIntervalVarWrapper interval variable is always
  // performed. So setting it to be performed does not change anything,
  // and setting it not to be performed is inconsistent and should cause
  // a failure.
  if (!val) {
    solver()->Fail();
  }
}
bool AlwaysPerformedIntervalVarWrapper::WasPerformedBound() const {
  return true;
}
void AlwaysPerformedIntervalVarWrapper::WhenPerformedBound(Demon* d) {
  t_->WhenPerformedBound(d);
}
IntExpr* AlwaysPerformedIntervalVarWrapper::StartExpr() {
  if (start_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&start_expr_));
    start_expr_ = BuildStartExpr(this);
  }
  return start_expr_;
}
IntExpr* AlwaysPerformedIntervalVarWrapper::DurationExpr() {
  if (duration_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&duration_expr_));
    duration_expr_ = BuildDurationExpr(this);
  }
  return duration_expr_;
}
IntExpr* AlwaysPerformedIntervalVarWrapper::EndExpr() {
  if (end_expr_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&end_expr_));
    end_expr_ = BuildEndExpr(this);
  }
  return end_expr_;
}
IntExpr* AlwaysPerformedIntervalVarWrapper::PerformedExpr() {
  return solver()->MakeIntConst(1);
}
IntExpr* AlwaysPerformedIntervalVarWrapper::SafeStartExpr(int64_t) {
  return StartExpr();
}
IntExpr* AlwaysPerformedIntervalVarWrapper::SafeDurationExpr(int64_t) {
  return DurationExpr();
}
IntExpr* AlwaysPerformedIntervalVarWrapper::SafeEndExpr(int64_t) {
  return EndExpr();
}

IntervalVar* AlwaysPerformedIntervalVarWrapper::underlying() const {
  return t_;
}
bool AlwaysPerformedIntervalVarWrapper::MayUnderlyingBePerformed() const {
  return underlying()->MayBePerformed();
}

IntervalVarRelaxedMax::IntervalVarRelaxedMax(IntervalVar* t)
    : AlwaysPerformedIntervalVarWrapper(t) {}
IntervalVarRelaxedMax::~IntervalVarRelaxedMax() {}
int64_t IntervalVarRelaxedMax::StartMax() const {
  // It matters to use DurationMin() and not underlying()->DurationMin() here.
  return underlying()->MustBePerformed() ? underlying()->StartMax()
                                         : (kMaxValidValue - DurationMin());
}
void IntervalVarRelaxedMax::SetStartMax(int64_t) {
  LOG(FATAL)
      << "Calling SetStartMax on a IntervalVarRelaxedMax is not supported, "
      << "as it seems there is no legitimate use case.";
}
int64_t IntervalVarRelaxedMax::EndMax() const {
  return underlying()->MustBePerformed() ? underlying()->EndMax()
                                         : kMaxValidValue;
}
void IntervalVarRelaxedMax::SetEndMax(int64_t) {
  LOG(FATAL)
      << "Calling SetEndMax on a IntervalVarRelaxedMax is not supported, "
      << "as it seems there is no legitimate use case.";
}

void IntervalVarRelaxedMax::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMaxOperation, 0,
                                 underlying());
}

std::string IntervalVarRelaxedMax::DebugString() const {
  return absl::StrFormat("IntervalVarRelaxedMax(%s)",
                         underlying()->DebugString());
}

// An interval variable that wraps around an underlying one, relaxing the min
// start and end. Relaxing means making unbounded when optional.
//
// More precisely, such an interval variable behaves as follows:
// * When the underlying must be performed, this interval variable behaves
//     exactly as the underlying;
// * When the underlying may or may not be performed, this interval variable
//     behaves like the underlying, except that it is unbounded on the min side;
// * When the underlying cannot be performed, this interval variable is of
//      duration 0 and must be performed in an interval unbounded on both sides.
//

// This class is very useful to implement propagators that may only modify
// the start max or end max.
IntervalVarRelaxedMin::IntervalVarRelaxedMin(IntervalVar* t)
    : AlwaysPerformedIntervalVarWrapper(t) {}
IntervalVarRelaxedMin::~IntervalVarRelaxedMin() {}
int64_t IntervalVarRelaxedMin::StartMin() const {
  return underlying()->MustBePerformed() ? underlying()->StartMin()
                                         : kMinValidValue;
}
void IntervalVarRelaxedMin::SetStartMin(int64_t) {
  LOG(FATAL)
      << "Calling SetStartMin on a IntervalVarRelaxedMin is not supported, "
      << "as it seems there is no legitimate use case.";
}
int64_t IntervalVarRelaxedMin::EndMin() const {
  // It matters to use DurationMin() and not underlying()->DurationMin() here.
  return underlying()->MustBePerformed() ? underlying()->EndMin()
                                         : (kMinValidValue + DurationMin());
}
void IntervalVarRelaxedMin::SetEndMin(int64_t) {
  LOG(FATAL)
      << "Calling SetEndMin on a IntervalVarRelaxedMin is not supported, "
      << "as it seems there is no legitimate use case.";
}

void IntervalVarRelaxedMin::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMinOperation, 0,
                                 underlying());
}

std::string IntervalVarRelaxedMin::DebugString() const {
  return absl::StrFormat("IntervalVarRelaxedMin(%s)",
                         underlying()->DebugString());
}

// ----- BaseIntervalVar -----

BaseIntervalVar::Handler::Handler(BaseIntervalVar* var) : var_(var) {}
BaseIntervalVar::Handler::~Handler() {}
void BaseIntervalVar::Handler::Run(Solver*) { var_->Process(); }
Solver::DemonPriority BaseIntervalVar::Handler::priority() const {
  return Solver::VAR_PRIORITY;
}
std::string BaseIntervalVar::Handler::DebugString() const {
  return absl::StrFormat("Handler(%s)", var_->DebugString());
}

BaseIntervalVar::BaseIntervalVar(Solver* s, const std::string& name)
    : IntervalVar(s, name),
      in_process_(false),
      handler_(this),
      cleaner_([this](Solver*) { CleanInProcess(); }) {}

BaseIntervalVar::~BaseIntervalVar() {}

void BaseIntervalVar::CleanInProcess() { in_process_ = false; }

std::string BaseIntervalVar::BaseName() const { return "IntervalVar"; }

bool BaseIntervalVar::InProcess() const { return in_process_; }

// ----- RangeVar -----

RangeVar::RangeVar(Solver* s, BaseIntervalVar* var, int64_t mi, int64_t ma)
    : IntExpr(s),
      min_(mi),
      max_(ma),
      var_(var),
      postponed_min_(mi),
      postponed_max_(ma),
      previous_min_(mi),
      previous_max_(ma),
      cast_var_(nullptr) {}

RangeVar::~RangeVar() {}

bool RangeVar::Bound() const { return min_.Value() == max_.Value(); }

int64_t RangeVar::Min() const { return min_.Value(); }

int64_t RangeVar::Max() const { return max_.Value(); }

void RangeVar::SetMin(int64_t m) {
  // No Op.
  if (m <= min_.Value()) {
    return;
  }
  // Inconsistent value.
  if (m > max_.Value()) {
    var_->SetPerformed(false);
    return;
  }
  if (var_->InProcess()) {
    // In process, postpone modifications.
    if (m > postponed_max_) {
      var_->SetPerformed(false);
    }
    if (m > postponed_min_) {
      postponed_min_ = m;
    }
  } else {
    // Not in process.
    SyncPreviousBounds();
    min_.SetValue(solver(), m);
    var_->Push();
  }
}

int64_t RangeVar::OldMin() const {
  DCHECK(var_->InProcess());
  return previous_min_;
}

void RangeVar::SetMax(int64_t m) {
  if (m >= max_.Value()) {
    return;
  }
  if (m < min_.Value()) {
    var_->SetPerformed(false);
    return;
  }
  if (var_->InProcess()) {
    // In process, postpone modifications.
    if (m < postponed_min_) {
      var_->SetPerformed(false);
    }
    if (m < postponed_max_) {
      postponed_max_ = m;
    }
  } else {
    // Not in process.
    SyncPreviousBounds();
    max_.SetValue(solver(), m);
    var_->Push();
  }
}

int64_t RangeVar::OldMax() const { return previous_min_; }

void RangeVar::SetRange(int64_t mi, int64_t ma) {
  if (mi <= min_.Value() && ma >= max_.Value()) {
    // No Op.
    return;
  }
  if (mi > max_.Value() || ma < min_.Value() || mi > ma) {
    var_->SetPerformed(false);
  }
  if (var_->InProcess()) {
    if (mi > postponed_max_ || ma < postponed_min_) {
      var_->SetPerformed(false);
    }
    if (mi > postponed_min_) {
      postponed_min_ = mi;
    }
    if (ma < postponed_max_) {
      postponed_max_ = ma;
    }
  } else {
    // Not in process.
    SyncPreviousBounds();
    if (mi > min_.Value()) {
      min_.SetValue(solver(), mi);
    }
    if (ma < max_.Value()) {
      max_.SetValue(solver(), ma);
    }
    var_->Push();
  }
}

void RangeVar::WhenRange(Demon* demon) {
  if (!Bound()) {
    if (demon->priority() == Solver::DELAYED_PRIORITY) {
      delayed_range_demons_.PushIfNotTop(solver(),
                                         solver()->RegisterDemon(demon));
    } else {
      range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(demon));
    }
  }
}

void RangeVar::WhenBound(Demon* demon) {
  if (!Bound()) {
    if (demon->priority() == Solver::DELAYED_PRIORITY) {
      delayed_bound_demons_.PushIfNotTop(solver(),
                                         solver()->RegisterDemon(demon));
    } else {
      bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(demon));
    }
  }
}

void RangeVar::UpdatePostponedBounds() {
  postponed_min_ = min_.Value();
  postponed_max_ = max_.Value();
}

void RangeVar::ProcessDemons() {
  if (Bound()) {
    ExecuteAll(bound_demons_);
    EnqueueAll(delayed_bound_demons_);
  }
  if (min_.Value() != previous_min_ || max_.Value() != previous_max_) {
    ExecuteAll(range_demons_);
    EnqueueAll(delayed_range_demons_);
  }
}

void RangeVar::UpdatePreviousBounds() {
  previous_min_ = min_.Value();
  previous_max_ = max_.Value();
}

// TODO(user): Remove this interval field enum.
void RangeVar::ApplyPostponedBounds(IntervalField which) {
  if (min_.Value() < postponed_min_ || max_.Value() > postponed_max_) {
    switch (which) {
      case START:
        var_->SetStartRange(std::max(postponed_min_, min_.Value()),
                            std::min(postponed_max_, max_.Value()));
        break;
      case DURATION:
        var_->SetDurationRange(std::max(postponed_min_, min_.Value()),
                               std::min(postponed_max_, max_.Value()));
        break;
      case END:
        var_->SetEndRange(std::max(postponed_min_, min_.Value()),
                          std::min(postponed_max_, max_.Value()));
        break;
    }
  }
}

IntVar* RangeVar::Var() {
  if (cast_var_ == nullptr) {
    solver()->SaveValue(reinterpret_cast<void**>(&cast_var_));
    cast_var_ = solver()->MakeIntVar(min_.Value(), max_.Value());
    LinkVarExpr(solver(), this, cast_var_);
  }
  return cast_var_;
}

std::string RangeVar::DebugString() const {
  std::string out = absl::StrCat(min_.Value());
  if (!Bound()) {
    absl::StrAppendFormat(&out, " .. %d", max_.Value());
  }
  return out;
}

void RangeVar::SyncPreviousBounds() {
  if (previous_min_ > min_.Value()) {
    previous_min_ = min_.Value();
  }
  if (previous_max_ < max_.Value()) {
    previous_max_ = max_.Value();
  }
}

// ----- PerformedVar -----

PerformedVar::PerformedVar(Solver* s, BaseIntervalVar* var, bool optional)
    : BooleanVar(s, ""),
      var_(var),
      previous_value_(optional ? kUnboundBooleanVarValue : 1),
      postponed_value_(optional ? kUnboundBooleanVarValue : 1) {
  if (!optional) {
    value_ = 1;
  }
}

PerformedVar::PerformedVar(Solver* s, BaseIntervalVar* var)
    : BooleanVar(s, ""), var_(var), previous_value_(0), postponed_value_(0) {
  value_ = 1;
}

PerformedVar::~PerformedVar() {}

void PerformedVar::SetValue(int64_t v) {
  if ((v & 0xfffffffffffffffe) != 0 ||  // Not 0 or 1.
      (value_ != kUnboundBooleanVarValue && v != value_)) {
    solver()->Fail();
  }
  if (var_->InProcess()) {
    if (postponed_value_ != kUnboundBooleanVarValue &&
        v != postponed_value_) {  // Fail early.
      solver()->Fail();
    } else {
      postponed_value_ = v;
    }
  } else if (value_ == kUnboundBooleanVarValue) {
    previous_value_ = kUnboundBooleanVarValue;
    solver()->InternalSaveBooleanVarValue(this);
    value_ = static_cast<int>(v);
    var_->Push();
  }
}

int64_t PerformedVar::OldMin() const { return previous_value_ == 1; }

int64_t PerformedVar::OldMax() const { return previous_value_ != 0; }

void PerformedVar::RestoreBooleanValue() {
  previous_value_ = kUnboundBooleanVarValue;
  value_ = kUnboundBooleanVarValue;
  postponed_value_ = kUnboundBooleanVarValue;
}

void PerformedVar::Process() {
  if (previous_value_ != value_) {
    ExecuteAll(bound_demons_);
    EnqueueAll(delayed_bound_demons_);
  }
}

void PerformedVar::UpdatePostponedValue() { postponed_value_ = value_; }

void PerformedVar::UpdatePreviousValueAndApplyPostponedValue() {
  previous_value_ = value_;
  if (value_ != postponed_value_) {
    DCHECK_NE(kUnboundBooleanVarValue, postponed_value_);
    SetValue(postponed_value_);
  }
}

std::string PerformedVar::DebugString() const {
  switch (value_) {
    case 0:
      return "false";
    case 1:
      return "true";
    default:
      return "undecided";
  }
}

// ----- FixedDurationIntervalVar -----

FixedDurationIntervalVar::~FixedDurationIntervalVar() {}

int64_t FixedDurationIntervalVar::OldStartMin() const {
  return start_.OldMin();
}
int64_t FixedDurationIntervalVar::OldStartMax() const {
  return start_.OldMax();
}
void FixedDurationIntervalVar::WhenStartRange(Demon* d) {
  if (performed_.Max() == 1) {
    start_.WhenRange(d);
  }
}
void FixedDurationIntervalVar::WhenStartBound(Demon* d) {
  if (performed_.Max() == 1) {
    start_.WhenBound(d);
  }
}

int64_t FixedDurationIntervalVar::OldDurationMin() const { return duration_; }
int64_t FixedDurationIntervalVar::OldDurationMax() const { return duration_; }
void FixedDurationIntervalVar::WhenDurationRange(Demon*) {}
void FixedDurationIntervalVar::WhenDurationBound(Demon*) {}

int64_t FixedDurationIntervalVar::OldEndMin() const {
  return CapAdd(OldStartMin(), duration_);
}
int64_t FixedDurationIntervalVar::OldEndMax() const {
  return CapAdd(OldStartMax(), duration_);
}
void FixedDurationIntervalVar::WhenEndRange(Demon* d) { WhenStartRange(d); }
void FixedDurationIntervalVar::WhenEndBound(Demon* d) { WhenStartBound(d); }

bool FixedDurationIntervalVar::WasPerformedBound() const {
  return performed_.OldMin() == performed_.OldMax();
}
void FixedDurationIntervalVar::WhenPerformedBound(Demon* d) {
  performed_.WhenBound(d);
}

void FixedDurationIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

IntExpr* FixedDurationIntervalVar::StartExpr() { return &start_; }
IntExpr* FixedDurationIntervalVar::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* FixedDurationIntervalVar::EndExpr() {
  return solver()->MakeSum(StartExpr(), duration_);
}
IntExpr* FixedDurationIntervalVar::PerformedExpr() { return &performed_; }
IntExpr* FixedDurationIntervalVar::SafeStartExpr(int64_t unperformed_value) {
  return BuildSafeStartExpr(this, unperformed_value);
}
IntExpr* FixedDurationIntervalVar::SafeDurationExpr(int64_t unperformed_value) {
  return BuildSafeDurationExpr(this, unperformed_value);
}
IntExpr* FixedDurationIntervalVar::SafeEndExpr(int64_t unperformed_value) {
  return BuildSafeEndExpr(this, unperformed_value);
}

FixedDurationIntervalVar::FixedDurationIntervalVar(Solver* s, int64_t start_min,
                                                   int64_t start_max,
                                                   int64_t duration,
                                                   bool optional,
                                                   const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, start_min, start_max),
      duration_(duration),
      performed_(s, this, optional) {}

FixedDurationIntervalVar::FixedDurationIntervalVar(Solver* s,
                                                   const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, 0, 0),
      duration_(0),
      performed_(s, this) {}

void FixedDurationIntervalVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  start_.UpdatePostponedBounds();
  performed_.UpdatePostponedValue();
  set_action_on_fail(cleaner_);
  if (performed_.Max() == 1) {
    start_.ProcessDemons();
  }
  performed_.Process();
  reset_action_on_fail();
  CleanInProcess();
  start_.UpdatePreviousBounds();
  start_.ApplyPostponedBounds(START);
  performed_.UpdatePreviousValueAndApplyPostponedValue();
}

int64_t FixedDurationIntervalVar::StartMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Min();
}

int64_t FixedDurationIntervalVar::StartMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Max();
}

void FixedDurationIntervalVar::SetStartMin(int64_t m) {
  if (performed_.Max() == 1) {
    start_.SetMin(m);
  }
}

void FixedDurationIntervalVar::SetStartMax(int64_t m) {
  if (performed_.Max() == 1) {
    start_.SetMax(m);
  }
}

void FixedDurationIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  if (performed_.Max() == 1) {
    start_.SetRange(mi, ma);
  }
}

int64_t FixedDurationIntervalVar::DurationMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_;
}

int64_t FixedDurationIntervalVar::DurationMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_;
}

void FixedDurationIntervalVar::SetDurationMin(int64_t m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void FixedDurationIntervalVar::SetDurationMax(int64_t m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}

void FixedDurationIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

int64_t FixedDurationIntervalVar::EndMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Min() + duration_;
}

int64_t FixedDurationIntervalVar::EndMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return CapAdd(start_.Max(), duration_);
}

void FixedDurationIntervalVar::SetEndMin(int64_t m) {
  SetStartMin(CapSub(m, duration_));
}

void FixedDurationIntervalVar::SetEndMax(int64_t m) {
  SetStartMax(CapSub(m, duration_));
}

void FixedDurationIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

bool FixedDurationIntervalVar::MustBePerformed() const {
  return (performed_.Min() == 1);
}

bool FixedDurationIntervalVar::MayBePerformed() const {
  return (performed_.Max() == 1);
}

void FixedDurationIntervalVar::SetPerformed(bool val) {
  performed_.SetValue(val);
}

void FixedDurationIntervalVar::Push() {
  DCHECK(!in_process_);
  EnqueueVar(&handler_);
  DCHECK(!in_process_);
}

std::string FixedDurationIntervalVar::DebugString() const {
  const std::string& var_name = name();
  if (performed_.Max() == 0) {
    if (!var_name.empty()) {
      return absl::StrFormat("%s(performed = false)", var_name);
    } else {
      return "IntervalVar(performed = false)";
    }
  } else {
    std::string out;
    if (!var_name.empty()) {
      out = var_name + "(start = ";
    } else {
      out = "IntervalVar(start = ";
    }
    absl::StrAppendFormat(&out, "%s, duration = %d, performed = %s)",
                          start_.DebugString(), duration_,
                          performed_.DebugString());
    return out;
  }
}

// ----- FixedDurationPerformedIntervalVar -----

FixedDurationPerformedIntervalVar::~FixedDurationPerformedIntervalVar() {}

int64_t FixedDurationPerformedIntervalVar::OldStartMin() const {
  return start_.OldMin();
}
int64_t FixedDurationPerformedIntervalVar::OldStartMax() const {
  return start_.OldMax();
}
void FixedDurationPerformedIntervalVar::WhenStartRange(Demon* d) {
  start_.WhenRange(d);
}
void FixedDurationPerformedIntervalVar::WhenStartBound(Demon* d) {
  start_.WhenBound(d);
}

int64_t FixedDurationPerformedIntervalVar::OldDurationMin() const {
  return duration_;
}
int64_t FixedDurationPerformedIntervalVar::OldDurationMax() const {
  return duration_;
}
void FixedDurationPerformedIntervalVar::WhenDurationRange(Demon*) {}
void FixedDurationPerformedIntervalVar::WhenDurationBound(Demon*) {}

int64_t FixedDurationPerformedIntervalVar::OldEndMin() const {
  return CapAdd(OldStartMin(), duration_);
}
int64_t FixedDurationPerformedIntervalVar::OldEndMax() const {
  return CapAdd(OldStartMax(), duration_);
}
void FixedDurationPerformedIntervalVar::WhenEndRange(Demon* d) {
  WhenStartRange(d);
}
void FixedDurationPerformedIntervalVar::WhenEndBound(Demon* d) {
  WhenEndRange(d);
}

bool FixedDurationPerformedIntervalVar::WasPerformedBound() const {
  return true;
}
void FixedDurationPerformedIntervalVar::WhenPerformedBound(Demon*) {}

void FixedDurationPerformedIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

IntExpr* FixedDurationPerformedIntervalVar::StartExpr() { return &start_; }
IntExpr* FixedDurationPerformedIntervalVar::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* FixedDurationPerformedIntervalVar::EndExpr() {
  return solver()->MakeSum(StartExpr(), duration_);
}
IntExpr* FixedDurationPerformedIntervalVar::PerformedExpr() {
  return solver()->MakeIntConst(1);
}
IntExpr* FixedDurationPerformedIntervalVar::SafeStartExpr(int64_t) {
  return StartExpr();
}
IntExpr* FixedDurationPerformedIntervalVar::SafeDurationExpr(int64_t) {
  return DurationExpr();
}
IntExpr* FixedDurationPerformedIntervalVar::SafeEndExpr(int64_t) {
  return EndExpr();
}

void FixedDurationPerformedIntervalVar::CheckOldPerformed() {}

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* s, int64_t start_min, int64_t start_max, int64_t duration,
    const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, start_min, start_max),
      duration_(duration) {}

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* s, const std::string& name)
    : BaseIntervalVar(s, name), start_(s, this, 0, 0), duration_(0) {}

void FixedDurationPerformedIntervalVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  start_.UpdatePostponedBounds();
  set_action_on_fail(cleaner_);
  start_.ProcessDemons();
  reset_action_on_fail();
  CleanInProcess();
  start_.UpdatePreviousBounds();
  start_.ApplyPostponedBounds(START);
}

int64_t FixedDurationPerformedIntervalVar::StartMin() const {
  return start_.Min();
}

int64_t FixedDurationPerformedIntervalVar::StartMax() const {
  return start_.Max();
}

void FixedDurationPerformedIntervalVar::SetStartMin(int64_t m) {
  start_.SetMin(m);
}

void FixedDurationPerformedIntervalVar::SetStartMax(int64_t m) {
  start_.SetMax(m);
}

void FixedDurationPerformedIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  start_.SetRange(mi, ma);
}

int64_t FixedDurationPerformedIntervalVar::DurationMin() const {
  return duration_;
}

int64_t FixedDurationPerformedIntervalVar::DurationMax() const {
  return duration_;
}

void FixedDurationPerformedIntervalVar::SetDurationMin(int64_t m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void FixedDurationPerformedIntervalVar::SetDurationMax(int64_t m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}
int64_t FixedDurationPerformedIntervalVar::EndMin() const {
  return CapAdd(start_.Min(), duration_);
}

int64_t FixedDurationPerformedIntervalVar::EndMax() const {
  return CapAdd(start_.Max(), duration_);
}

void FixedDurationPerformedIntervalVar::SetEndMin(int64_t m) {
  SetStartMin(CapSub(m, duration_));
}

void FixedDurationPerformedIntervalVar::SetEndMax(int64_t m) {
  SetStartMax(CapSub(m, duration_));
}

void FixedDurationPerformedIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

void FixedDurationPerformedIntervalVar::SetDurationRange(int64_t mi,
                                                         int64_t ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

bool FixedDurationPerformedIntervalVar::MustBePerformed() const { return true; }

bool FixedDurationPerformedIntervalVar::MayBePerformed() const { return true; }

void FixedDurationPerformedIntervalVar::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

void FixedDurationPerformedIntervalVar::Push() {
  DCHECK(!in_process_);
  EnqueueVar(&handler_);
  DCHECK(!in_process_);
}

std::string FixedDurationPerformedIntervalVar::DebugString() const {
  std::string out;
  const std::string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  absl::StrAppendFormat(&out, "%s, duration = %d, performed = true)",
                        start_.DebugString(), duration_);
  return out;
}

// ----- StartVarPerformedIntervalVar -----

StartVarPerformedIntervalVar::~StartVarPerformedIntervalVar() {}

int64_t StartVarPerformedIntervalVar::OldStartMin() const {
  return start_var_->OldMin();
}
int64_t StartVarPerformedIntervalVar::OldStartMax() const {
  return start_var_->OldMax();
}
void StartVarPerformedIntervalVar::WhenStartRange(Demon* d) {
  start_var_->WhenRange(d);
}
void StartVarPerformedIntervalVar::WhenStartBound(Demon* d) {
  start_var_->WhenBound(d);
}

int64_t StartVarPerformedIntervalVar::OldDurationMin() const {
  return duration_;
}
int64_t StartVarPerformedIntervalVar::OldDurationMax() const {
  return duration_;
}
void StartVarPerformedIntervalVar::WhenDurationRange(Demon*) {}
void StartVarPerformedIntervalVar::WhenDurationBound(Demon*) {}

int64_t StartVarPerformedIntervalVar::OldEndMin() const {
  return CapAdd(OldStartMin(), duration_);
}
int64_t StartVarPerformedIntervalVar::OldEndMax() const {
  return CapAdd(OldStartMax(), duration_);
}
void StartVarPerformedIntervalVar::WhenEndRange(Demon* d) {
  start_var_->WhenRange(d);
}
void StartVarPerformedIntervalVar::WhenEndBound(Demon* d) {
  start_var_->WhenBound(d);
}

bool StartVarPerformedIntervalVar::WasPerformedBound() const { return true; }
void StartVarPerformedIntervalVar::WhenPerformedBound(Demon*) {}

IntExpr* StartVarPerformedIntervalVar::StartExpr() { return start_var_; }
IntExpr* StartVarPerformedIntervalVar::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* StartVarPerformedIntervalVar::EndExpr() {
  return solver()->MakeSum(start_var_, duration_);
}
IntExpr* StartVarPerformedIntervalVar::PerformedExpr() {
  return solver()->MakeIntConst(1);
}
IntExpr* StartVarPerformedIntervalVar::SafeStartExpr(int64_t) {
  return StartExpr();
}
IntExpr* StartVarPerformedIntervalVar::SafeDurationExpr(int64_t) {
  return DurationExpr();
}
IntExpr* StartVarPerformedIntervalVar::SafeEndExpr(int64_t) {
  return EndExpr();
}

void StartVarPerformedIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

// TODO(user): Take care of overflows.
StartVarPerformedIntervalVar::StartVarPerformedIntervalVar(
    Solver* s, IntVar* var, int64_t duration, const std::string& name)
    : IntervalVar(s, name), start_var_(var), duration_(duration) {}

int64_t StartVarPerformedIntervalVar::StartMin() const {
  return start_var_->Min();
}

int64_t StartVarPerformedIntervalVar::StartMax() const {
  return start_var_->Max();
}

void StartVarPerformedIntervalVar::SetStartMin(int64_t m) {
  start_var_->SetMin(m);
}

void StartVarPerformedIntervalVar::SetStartMax(int64_t m) {
  start_var_->SetMax(m);
}

void StartVarPerformedIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  start_var_->SetRange(mi, ma);
}

int64_t StartVarPerformedIntervalVar::DurationMin() const { return duration_; }

int64_t StartVarPerformedIntervalVar::DurationMax() const { return duration_; }

void StartVarPerformedIntervalVar::SetDurationMin(int64_t m) {
  if (m > duration_) {
    solver()->Fail();
  }
}

void StartVarPerformedIntervalVar::SetDurationMax(int64_t m) {
  if (m < duration_) {
    solver()->Fail();
  }
}
int64_t StartVarPerformedIntervalVar::EndMin() const {
  return start_var_->Min() + duration_;
}

int64_t StartVarPerformedIntervalVar::EndMax() const {
  return start_var_->Max() + duration_;
}

void StartVarPerformedIntervalVar::SetEndMin(int64_t m) {
  SetStartMin(CapSub(m, duration_));
}

void StartVarPerformedIntervalVar::SetEndMax(int64_t m) {
  SetStartMax(CapSub(m, duration_));
}

void StartVarPerformedIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

void StartVarPerformedIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    solver()->Fail();
  }
}

bool StartVarPerformedIntervalVar::MustBePerformed() const { return true; }

bool StartVarPerformedIntervalVar::MayBePerformed() const { return true; }

void StartVarPerformedIntervalVar::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

std::string StartVarPerformedIntervalVar::DebugString() const {
  std::string out;
  const std::string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  absl::StrAppendFormat(&out, "%d", start_var_->Min());
  if (!start_var_->Bound()) {
    absl::StrAppendFormat(&out, " .. %d", start_var_->Max());
  }

  absl::StrAppendFormat(&out, ", duration = %d, performed = true)", duration_);
  return out;
}

// ----- StartVarIntervalVar -----

int64_t StartVarIntervalVar::OldStartMin() const { return start_->OldMin(); }
int64_t StartVarIntervalVar::OldStartMax() const { return start_->OldMax(); }
void StartVarIntervalVar::WhenStartRange(Demon* d) {
  if (performed_->Max() == 1) {
    start_->WhenRange(d);
  }
}
void StartVarIntervalVar::WhenStartBound(Demon* d) {
  if (performed_->Max() == 1) {
    start_->WhenBound(d);
  }
}

int64_t StartVarIntervalVar::OldDurationMin() const { return duration_; }
int64_t StartVarIntervalVar::OldDurationMax() const { return duration_; }
void StartVarIntervalVar::WhenDurationRange(Demon*) {}
void StartVarIntervalVar::WhenDurationBound(Demon*) {}

int64_t StartVarIntervalVar::OldEndMin() const {
  return CapAdd(OldStartMin(), duration_);
}
int64_t StartVarIntervalVar::OldEndMax() const {
  return CapAdd(OldStartMax(), duration_);
}
void StartVarIntervalVar::WhenEndRange(Demon* d) { WhenStartRange(d); }
void StartVarIntervalVar::WhenEndBound(Demon* d) { WhenStartBound(d); }

bool StartVarIntervalVar::WasPerformedBound() const {
  return performed_->OldMin() == performed_->OldMax();
}
void StartVarIntervalVar::WhenPerformedBound(Demon* d) {
  performed_->WhenBound(d);
}

void StartVarIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

IntExpr* StartVarIntervalVar::StartExpr() { return start_; }
IntExpr* StartVarIntervalVar::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* StartVarIntervalVar::EndExpr() {
  return solver()->MakeSum(StartExpr(), duration_);
}
IntExpr* StartVarIntervalVar::PerformedExpr() { return performed_; }
IntExpr* StartVarIntervalVar::SafeStartExpr(int64_t unperformed_value) {
  return BuildSafeStartExpr(this, unperformed_value);
}
IntExpr* StartVarIntervalVar::SafeDurationExpr(int64_t unperformed_value) {
  return BuildSafeDurationExpr(this, unperformed_value);
}
IntExpr* StartVarIntervalVar::SafeEndExpr(int64_t unperformed_value) {
  return BuildSafeEndExpr(this, unperformed_value);
}

void StartVarIntervalVar::Process() { LOG(FATAL) << "Should not be here"; }

void StartVarIntervalVar::Push() { LOG(FATAL) << "Should not be here"; }

int64_t StartVarIntervalVar::StoredMin() const { return start_min_.Value(); }
int64_t StartVarIntervalVar::StoredMax() const { return start_max_.Value(); }

StartVarIntervalVar::StartVarIntervalVar(Solver* s, IntVar* start,
                                         int64_t duration, IntVar* performed,
                                         const std::string& name)
    : BaseIntervalVar(s, name),
      start_(start),
      duration_(duration),
      performed_(performed),
      start_min_(start->Min()),
      start_max_(start->Max()) {}

int64_t StartVarIntervalVar::StartMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return std::max(start_->Min(), start_min_.Value());
}

int64_t StartVarIntervalVar::StartMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return std::min(start_->Max(), start_max_.Value());
}

void StartVarIntervalVar::SetStartMin(int64_t m) {
  if (performed_->Min() == 1) {
    start_->SetMin(m);
  } else {
    start_min_.SetValue(solver(), std::max(m, start_min_.Value()));
    if (start_min_.Value() > std::min(start_max_.Value(), start_->Max())) {
      performed_->SetValue(0);
    }
  }
}

void StartVarIntervalVar::SetStartMax(int64_t m) {
  if (performed_->Min() == 1) {
    start_->SetMax(m);
  } else {
    start_max_.SetValue(solver(), std::min(m, start_max_.Value()));
    if (start_max_.Value() < std::max(start_min_.Value(), start_->Min())) {
      performed_->SetValue(0);
    }
  }
}

void StartVarIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  if (performed_->Min() == 1) {
    start_->SetRange(mi, ma);
  } else {
    start_min_.SetValue(solver(), std::max(mi, start_min_.Value()));
    start_max_.SetValue(solver(), std::min(ma, start_max_.Value()));
    if (std::max(start_min_.Value(), start_->Min()) >
        std::min(start_max_.Value(), start_->Max())) {
      performed_->SetValue(0);
    }
  }
}

int64_t StartVarIntervalVar::DurationMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return duration_;
}

int64_t StartVarIntervalVar::DurationMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return duration_;
}

void StartVarIntervalVar::SetDurationMin(int64_t m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void StartVarIntervalVar::SetDurationMax(int64_t m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}

void StartVarIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

int64_t StartVarIntervalVar::EndMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return CapAdd(StartMin(), duration_);
}

int64_t StartVarIntervalVar::EndMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return CapAdd(StartMax(), duration_);
}

void StartVarIntervalVar::SetEndMin(int64_t m) {
  SetStartMin(CapSub(m, duration_));
}

void StartVarIntervalVar::SetEndMax(int64_t m) {
  SetStartMax(CapSub(m, duration_));
}

void StartVarIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

bool StartVarIntervalVar::MustBePerformed() const {
  return (performed_->Min() == 1);
}

bool StartVarIntervalVar::MayBePerformed() const {
  return (performed_->Max() == 1);
}

void StartVarIntervalVar::SetPerformed(bool val) {
  const bool was_bound = performed_->Bound();
  performed_->SetValue(val);
  if (val && !was_bound) {
    start_->SetRange(start_min_.Value(), start_max_.Value());
  }
}

std::string StartVarIntervalVar::DebugString() const {
  const std::string& var_name = name();
  if (performed_->Max() == 0) {
    if (!var_name.empty()) {
      return absl::StrFormat("%s(performed = false)", var_name);
    } else {
      return "IntervalVar(performed = false)";
    }
  } else {
    std::string out;
    if (!var_name.empty()) {
      out = var_name + "(start = ";
    } else {
      out = "IntervalVar(start = ";
    }
    absl::StrAppendFormat(&out, "%s, duration = %d, performed = %s)",
                          start_->DebugString(), duration_,
                          performed_->DebugString());
    return out;
  }
}

// ----- LinkStartVarIntervalVar -----

LinkStartVarIntervalVar::LinkStartVarIntervalVar(Solver* solver,
                                                 StartVarIntervalVar* interval,
                                                 IntVar* start,
                                                 IntVar* performed)
    : Constraint(solver),
      interval_(interval),
      start_(start),
      performed_(performed) {}

LinkStartVarIntervalVar::~LinkStartVarIntervalVar() {}

void LinkStartVarIntervalVar::Post() {
  Demon* const demon = MakeConstraintDemon0(
      solver(), this, &LinkStartVarIntervalVar::PerformedBound,
      "PerformedBound");
  performed_->WhenBound(demon);
}

void LinkStartVarIntervalVar::InitialPropagate() {
  if (performed_->Bound()) {
    PerformedBound();
  }
}

void LinkStartVarIntervalVar::PerformedBound() {
  if (performed_->Min() == 1) {
    start_->SetRange(interval_->StoredMin(), interval_->StoredMax());
  }
}

// ----- FixedInterval -----

FixedInterval::~FixedInterval() {}

int64_t FixedInterval::StartMin() const { return start_; }
int64_t FixedInterval::StartMax() const { return start_; }
int64_t FixedInterval::OldStartMin() const { return start_; }
int64_t FixedInterval::OldStartMax() const { return start_; }
void FixedInterval::WhenStartRange(Demon*) {}
void FixedInterval::WhenStartBound(Demon*) {}

int64_t FixedInterval::DurationMin() const { return duration_; }
int64_t FixedInterval::DurationMax() const { return duration_; }
int64_t FixedInterval::OldDurationMin() const { return duration_; }
int64_t FixedInterval::OldDurationMax() const { return duration_; }
void FixedInterval::WhenDurationRange(Demon*) {}
void FixedInterval::WhenDurationBound(Demon*) {}

int64_t FixedInterval::EndMin() const { return start_ + duration_; }
int64_t FixedInterval::EndMax() const { return start_ + duration_; }
int64_t FixedInterval::OldEndMin() const { return start_ + duration_; }
int64_t FixedInterval::OldEndMax() const { return start_ + duration_; }
void FixedInterval::WhenEndRange(Demon*) {}
void FixedInterval::WhenEndBound(Demon*) {}

bool FixedInterval::MustBePerformed() const { return true; }
bool FixedInterval::MayBePerformed() const { return true; }
bool FixedInterval::WasPerformedBound() const { return true; }
void FixedInterval::WhenPerformedBound(Demon*) {}

void FixedInterval::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

IntExpr* FixedInterval::StartExpr() { return solver()->MakeIntConst(start_); }
IntExpr* FixedInterval::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* FixedInterval::EndExpr() {
  return solver()->MakeIntConst(start_ + duration_);
}
IntExpr* FixedInterval::PerformedExpr() { return solver()->MakeIntConst(1); }
IntExpr* FixedInterval::SafeStartExpr(int64_t) { return StartExpr(); }
IntExpr* FixedInterval::SafeDurationExpr(int64_t) { return DurationExpr(); }
IntExpr* FixedInterval::SafeEndExpr(int64_t) { return EndExpr(); }

FixedInterval::FixedInterval(Solver* s, int64_t start, int64_t duration,
                             const std::string& name)
    : IntervalVar(s, name), start_(start), duration_(duration) {}

void FixedInterval::SetStartMin(int64_t m) {
  if (m > start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetStartMax(int64_t m) {
  if (m < start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetStartRange(int64_t mi, int64_t ma) {
  if (mi > start_ || ma < start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationMin(int64_t m) {
  if (m > duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationMax(int64_t m) {
  if (m < duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndMin(int64_t m) {
  if (m > start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndMax(int64_t m) {
  if (m < start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndRange(int64_t mi, int64_t ma) {
  if (mi > start_ + duration_ || ma < start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationRange(int64_t mi, int64_t ma) {
  if (mi > duration_ || ma < duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

std::string FixedInterval::DebugString() const {
  std::string out;
  const std::string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  absl::StrAppendFormat(&out, "%d, duration = %d, performed = true)", start_,
                        duration_);
  return out;
}

// ----- VariableDurationIntervalVar -----

VariableDurationIntervalVar::VariableDurationIntervalVar(
    Solver* s, int64_t start_min, int64_t start_max, int64_t duration_min,
    int64_t duration_max, int64_t end_min, int64_t end_max, bool optional,
    const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, std::max(start_min, CapSub(end_min, duration_max)),
             std::min(start_max, CapSub(end_max, duration_min))),
      duration_(s, this, std::max(duration_min, CapSub(end_min, start_max)),
                std::min(duration_max, CapSub(end_max, start_min))),
      end_(s, this, std::max(end_min, CapAdd(start_min, duration_min)),
           std::min(end_max, CapAdd(start_max, duration_max))),
      performed_(s, this, optional) {}

VariableDurationIntervalVar::~VariableDurationIntervalVar() {}

int64_t VariableDurationIntervalVar::StartMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Min();
}

int64_t VariableDurationIntervalVar::StartMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Max();
}

void VariableDurationIntervalVar::SetStartMin(int64_t m) {
  if (performed_.Max() == 1) {
    start_.SetMin(m);
  }
}

void VariableDurationIntervalVar::SetStartMax(int64_t m) {
  if (performed_.Max() == 1) {
    start_.SetMax(m);
  }
}

void VariableDurationIntervalVar::SetStartRange(int64_t mi, int64_t ma) {
  if (performed_.Max() == 1) {
    start_.SetRange(mi, ma);
  }
}

int64_t VariableDurationIntervalVar::OldStartMin() const {
  CHECK_EQ(performed_.Max(), 1);
  CHECK(in_process_);
  return start_.OldMin();
}

int64_t VariableDurationIntervalVar::OldStartMax() const {
  CHECK_EQ(performed_.Max(), 1);
  CHECK(in_process_);
  return start_.OldMax();
}

void VariableDurationIntervalVar::WhenStartRange(Demon* d) {
  if (performed_.Max() == 1) {
    start_.WhenRange(d);
  }
}

void VariableDurationIntervalVar::WhenStartBound(Demon* d) {
  if (performed_.Max() == 1) {
    start_.WhenBound(d);
  }
}

int64_t VariableDurationIntervalVar::DurationMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_.Min();
}

int64_t VariableDurationIntervalVar::DurationMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_.Max();
}

void VariableDurationIntervalVar::SetDurationMin(int64_t m) {
  if (performed_.Max() == 1) {
    duration_.SetMin(m);
  }
}

void VariableDurationIntervalVar::SetDurationMax(int64_t m) {
  if (performed_.Max() == 1) {
    duration_.SetMax(m);
  }
}

void VariableDurationIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  if (performed_.Max() == 1) {
    duration_.SetRange(mi, ma);
  }
}

int64_t VariableDurationIntervalVar::OldDurationMin() const {
  CHECK_EQ(performed_.Max(), 1);
  CHECK(in_process_);
  return duration_.OldMin();
}

int64_t VariableDurationIntervalVar::OldDurationMax() const {
  CHECK_EQ(performed_.Max(), 1);
  CHECK(in_process_);
  return duration_.OldMax();
}

void VariableDurationIntervalVar::WhenDurationRange(Demon* d) {
  if (performed_.Max() == 1) {
    duration_.WhenRange(d);
  }
}

void VariableDurationIntervalVar::WhenDurationBound(Demon* d) {
  if (performed_.Max() == 1) {
    duration_.WhenBound(d);
  }
}

int64_t VariableDurationIntervalVar::EndMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return end_.Min();
}

int64_t VariableDurationIntervalVar::EndMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return end_.Max();
}

void VariableDurationIntervalVar::SetEndMin(int64_t m) {
  if (performed_.Max() == 1) {
    end_.SetMin(m);
  }
}

void VariableDurationIntervalVar::SetEndMax(int64_t m) {
  if (performed_.Max() == 1) {
    end_.SetMax(m);
  }
}

void VariableDurationIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  if (performed_.Max() == 1) {
    end_.SetRange(mi, ma);
  }
}

int64_t VariableDurationIntervalVar::OldEndMin() const {
  CHECK_EQ(performed_.Max(), 1);
  DCHECK(in_process_);
  return end_.OldMin();
}

int64_t VariableDurationIntervalVar::OldEndMax() const {
  CHECK_EQ(performed_.Max(), 1);
  DCHECK(in_process_);
  return end_.OldMax();
}

void VariableDurationIntervalVar::WhenEndRange(Demon* d) {
  if (performed_.Max() == 1) {
    end_.WhenRange(d);
  }
}

void VariableDurationIntervalVar::WhenEndBound(Demon* d) {
  if (performed_.Max() == 1) {
    end_.WhenBound(d);
  }
}

bool VariableDurationIntervalVar::MustBePerformed() const {
  return (performed_.Min() == 1);
}

bool VariableDurationIntervalVar::MayBePerformed() const {
  return (performed_.Max() == 1);
}

void VariableDurationIntervalVar::SetPerformed(bool val) {
  performed_.SetValue(val);
}

bool VariableDurationIntervalVar::WasPerformedBound() const {
  CHECK(in_process_);
  return performed_.OldMin() == performed_.OldMax();
}

void VariableDurationIntervalVar::WhenPerformedBound(Demon* d) {
  performed_.WhenBound(d);
}

void VariableDurationIntervalVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  start_.UpdatePostponedBounds();
  duration_.UpdatePostponedBounds();
  end_.UpdatePostponedBounds();
  performed_.UpdatePostponedValue();
  set_action_on_fail(cleaner_);
  if (performed_.Max() == 1) {
    start_.ProcessDemons();
    duration_.ProcessDemons();
    end_.ProcessDemons();
  }
  performed_.Process();
  reset_action_on_fail();
  CleanInProcess();
  // TODO(user): Replace this enum by a callback.
  start_.UpdatePreviousBounds();
  start_.ApplyPostponedBounds(START);
  duration_.UpdatePreviousBounds();
  duration_.ApplyPostponedBounds(DURATION);
  end_.UpdatePreviousBounds();
  end_.ApplyPostponedBounds(END);
  performed_.UpdatePreviousValueAndApplyPostponedValue();
}

std::string VariableDurationIntervalVar::DebugString() const {
  const std::string& var_name = name();
  if (performed_.Max() != 1) {
    if (!var_name.empty()) {
      return absl::StrFormat("%s(performed = false)", var_name);
    } else {
      return "IntervalVar(performed = false)";
    }
  } else {
    std::string out;
    if (!var_name.empty()) {
      out = var_name + "(start = ";
    } else {
      out = "IntervalVar(start = ";
    }

    absl::StrAppendFormat(&out, "%s, duration = %s, end = %s, performed = %s)",
                          start_.DebugString(), duration_.DebugString(),
                          end_.DebugString(), performed_.DebugString());
    return out;
  }
}

void VariableDurationIntervalVar::Accept(ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, "", 0, NullInterval());
}

IntExpr* VariableDurationIntervalVar::StartExpr() { return &start_; }
IntExpr* VariableDurationIntervalVar::DurationExpr() { return &duration_; }
IntExpr* VariableDurationIntervalVar::EndExpr() { return &end_; }
IntExpr* VariableDurationIntervalVar::PerformedExpr() { return &performed_; }
IntExpr* VariableDurationIntervalVar::SafeStartExpr(int64_t unperformed_value) {
  return BuildSafeStartExpr(this, unperformed_value);
}
IntExpr* VariableDurationIntervalVar::SafeDurationExpr(
    int64_t unperformed_value) {
  return BuildSafeDurationExpr(this, unperformed_value);
}
IntExpr* VariableDurationIntervalVar::SafeEndExpr(int64_t unperformed_value) {
  return BuildSafeEndExpr(this, unperformed_value);
}

void VariableDurationIntervalVar::Push() {
  DCHECK(!in_process_);
  if (performed_.Max() == 1) {
    // Performs the intersection on all intervals before pushing the
    // variable onto the queue. This way, we make sure the interval variable
    // is always in a consistent minimal state.
    start_.SetRange(CapSub(end_.Min(), duration_.Max()),
                    CapSub(end_.Max(), duration_.Min()));
    duration_.SetRange(CapSub(end_.Min(), start_.Max()),
                       CapSub(end_.Max(), start_.Min()));
    end_.SetRange(CapAdd(start_.Min(), duration_.Min()),
                  CapAdd(start_.Max(), duration_.Max()));
  }
  EnqueueVar(&handler_);
  DCHECK(!in_process_);
}

// ----- Base synced interval var -----

// ----- Base synced interval var -----

FixedDurationSyncedIntervalVar::FixedDurationSyncedIntervalVar(
    IntervalVar* t, int64_t duration, int64_t offset, const std::string& name)
    : IntervalVar(t->solver(), name),
      t_(t),
      duration_(duration),
      offset_(offset) {}

FixedDurationSyncedIntervalVar::~FixedDurationSyncedIntervalVar() {}

int64_t FixedDurationSyncedIntervalVar::DurationMin() const {
  return duration_;
}
int64_t FixedDurationSyncedIntervalVar::DurationMax() const {
  return duration_;
}
void FixedDurationSyncedIntervalVar::SetDurationMin(int64_t m) {
  if (m > duration_) {
    solver()->Fail();
  }
}
void FixedDurationSyncedIntervalVar::SetDurationMax(int64_t m) {
  if (m < duration_) {
    solver()->Fail();
  }
}
void FixedDurationSyncedIntervalVar::SetDurationRange(int64_t mi, int64_t ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    solver()->Fail();
  }
}
int64_t FixedDurationSyncedIntervalVar::OldDurationMin() const {
  return duration_;
}
int64_t FixedDurationSyncedIntervalVar::OldDurationMax() const {
  return duration_;
}
void FixedDurationSyncedIntervalVar::WhenDurationRange(Demon*) {}
void FixedDurationSyncedIntervalVar::WhenDurationBound(Demon*) {}
int64_t FixedDurationSyncedIntervalVar::EndMin() const {
  return CapAdd(StartMin(), duration_);
}
int64_t FixedDurationSyncedIntervalVar::EndMax() const {
  return CapAdd(StartMax(), duration_);
}
void FixedDurationSyncedIntervalVar::SetEndMin(int64_t m) {
  SetStartMin(CapSub(m, duration_));
}
void FixedDurationSyncedIntervalVar::SetEndMax(int64_t m) {
  SetStartMax(CapSub(m, duration_));
}
void FixedDurationSyncedIntervalVar::SetEndRange(int64_t mi, int64_t ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}
int64_t FixedDurationSyncedIntervalVar::OldEndMin() const {
  return CapAdd(OldStartMin(), duration_);
}
int64_t FixedDurationSyncedIntervalVar::OldEndMax() const {
  return CapAdd(OldStartMax(), duration_);
}
void FixedDurationSyncedIntervalVar::WhenEndRange(Demon* d) {
  WhenStartRange(d);
}
void FixedDurationSyncedIntervalVar::WhenEndBound(Demon* d) {
  WhenStartBound(d);
}
bool FixedDurationSyncedIntervalVar::MustBePerformed() const {
  return t_->MustBePerformed();
}
bool FixedDurationSyncedIntervalVar::MayBePerformed() const {
  return t_->MayBePerformed();
}
void FixedDurationSyncedIntervalVar::SetPerformed(bool val) {
  t_->SetPerformed(val);
}
bool FixedDurationSyncedIntervalVar::WasPerformedBound() const {
  return t_->WasPerformedBound();
}
void FixedDurationSyncedIntervalVar::WhenPerformedBound(Demon* d) {
  t_->WhenPerformedBound(d);
}

// ----- Fixed duration interval var synced on start -----

FixedDurationIntervalVarStartSyncedOnStart::
    FixedDurationIntervalVarStartSyncedOnStart(IntervalVar* t, int64_t duration,
                                               int64_t offset)
    : FixedDurationSyncedIntervalVar(
          t, duration, offset,
          absl::StrFormat(
              "IntervalStartSyncedOnStart(%s, duration = %d, offset = %d)",
              t->name(), duration, offset)) {}

FixedDurationIntervalVarStartSyncedOnStart::
    ~FixedDurationIntervalVarStartSyncedOnStart() {}

int64_t FixedDurationIntervalVarStartSyncedOnStart::StartMin() const {
  return CapAdd(t_->StartMin(), offset_);
}
int64_t FixedDurationIntervalVarStartSyncedOnStart::StartMax() const {
  return CapAdd(t_->StartMax(), offset_);
}
void FixedDurationIntervalVarStartSyncedOnStart::SetStartMin(int64_t m) {
  t_->SetStartMin(CapSub(m, offset_));
}
void FixedDurationIntervalVarStartSyncedOnStart::SetStartMax(int64_t m) {
  t_->SetStartMax(CapSub(m, offset_));
}
void FixedDurationIntervalVarStartSyncedOnStart::SetStartRange(int64_t mi,
                                                               int64_t ma) {
  t_->SetStartRange(CapSub(mi, offset_), CapSub(ma, offset_));
}
int64_t FixedDurationIntervalVarStartSyncedOnStart::OldStartMin() const {
  return CapAdd(t_->OldStartMin(), offset_);
}
int64_t FixedDurationIntervalVarStartSyncedOnStart::OldStartMax() const {
  return CapAdd(t_->OldStartMax(), offset_);
}
void FixedDurationIntervalVarStartSyncedOnStart::WhenStartRange(Demon* d) {
  t_->WhenStartRange(d);
}
void FixedDurationIntervalVarStartSyncedOnStart::WhenStartBound(Demon* d) {
  t_->WhenStartBound(d);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::StartExpr() {
  return solver()->MakeSum(t_->StartExpr(), offset_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::EndExpr() {
  return solver()->MakeSum(t_->StartExpr(), offset_ + duration_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::PerformedExpr() {
  return t_->PerformedExpr();
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::SafeStartExpr(
    int64_t unperformed_value) {
  return BuildSafeStartExpr(t_, unperformed_value);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::SafeDurationExpr(
    int64_t unperformed_value) {
  return BuildSafeDurationExpr(t_, unperformed_value);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnStart::SafeEndExpr(
    int64_t unperformed_value) {
  return BuildSafeEndExpr(t_, unperformed_value);
}
void FixedDurationIntervalVarStartSyncedOnStart::Accept(
    ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, ModelVisitor::kStartSyncOnStartOperation,
                                 offset_, t_);
}
std::string FixedDurationIntervalVarStartSyncedOnStart::DebugString() const {
  return absl::StrFormat(
      "IntervalStartSyncedOnStart(%s, duration = %d, offset = %d)",
      t_->DebugString(), duration_, offset_);
}

// ----- Fixed duration interval start synced on end -----

FixedDurationIntervalVarStartSyncedOnEnd::
    FixedDurationIntervalVarStartSyncedOnEnd(IntervalVar* t, int64_t duration,
                                             int64_t offset)
    : FixedDurationSyncedIntervalVar(
          t, duration, offset,
          absl::StrFormat(
              "IntervalStartSyncedOnEnd(%s, duration = %d, offset = %d)",
              t->name(), duration, offset)) {}

FixedDurationIntervalVarStartSyncedOnEnd::
    ~FixedDurationIntervalVarStartSyncedOnEnd() {}

int64_t FixedDurationIntervalVarStartSyncedOnEnd::StartMin() const {
  return CapAdd(t_->EndMin(), offset_);
}
int64_t FixedDurationIntervalVarStartSyncedOnEnd::StartMax() const {
  return CapAdd(t_->EndMax(), offset_);
}
void FixedDurationIntervalVarStartSyncedOnEnd::SetStartMin(int64_t m) {
  t_->SetEndMin(CapSub(m, offset_));
}
void FixedDurationIntervalVarStartSyncedOnEnd::SetStartMax(int64_t m) {
  t_->SetEndMax(CapSub(m, offset_));
}
void FixedDurationIntervalVarStartSyncedOnEnd::SetStartRange(int64_t mi,
                                                             int64_t ma) {
  t_->SetEndRange(CapSub(mi, offset_), CapSub(ma, offset_));
}
int64_t FixedDurationIntervalVarStartSyncedOnEnd::OldStartMin() const {
  return CapAdd(t_->OldEndMin(), offset_);
}
int64_t FixedDurationIntervalVarStartSyncedOnEnd::OldStartMax() const {
  return CapAdd(t_->OldEndMax(), offset_);
}
void FixedDurationIntervalVarStartSyncedOnEnd::WhenStartRange(Demon* d) {
  t_->WhenEndRange(d);
}
void FixedDurationIntervalVarStartSyncedOnEnd::WhenStartBound(Demon* d) {
  t_->WhenEndBound(d);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::StartExpr() {
  return solver()->MakeSum(t_->EndExpr(), offset_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::DurationExpr() {
  return solver()->MakeIntConst(duration_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::EndExpr() {
  return solver()->MakeSum(t_->EndExpr(), offset_ + duration_);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::PerformedExpr() {
  return t_->PerformedExpr();
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::SafeStartExpr(
    int64_t unperformed_value) {
  return BuildSafeStartExpr(t_, unperformed_value);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::SafeDurationExpr(
    int64_t unperformed_value) {
  return BuildSafeDurationExpr(t_, unperformed_value);
}
IntExpr* FixedDurationIntervalVarStartSyncedOnEnd::SafeEndExpr(
    int64_t unperformed_value) {
  return BuildSafeEndExpr(t_, unperformed_value);
}

void FixedDurationIntervalVarStartSyncedOnEnd::Accept(
    ModelVisitor* visitor) const {
  visitor->VisitIntervalVariable(this, ModelVisitor::kStartSyncOnEndOperation,
                                 offset_, t_);
}
std::string FixedDurationIntervalVarStartSyncedOnEnd::DebugString() const {
  return absl::StrFormat(
      "IntervalStartSyncedOnEnd(%s, duration = %d, offset = %d)",
      t_->DebugString(), duration_, offset_);
}

}  // namespace operations_research
