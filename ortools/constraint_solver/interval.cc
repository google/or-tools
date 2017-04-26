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
#include "ortools/base/join.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/saturated_arithmetic.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355 4804 4805)
#endif

namespace operations_research {
// Generic code for start/end/duration expressions.
// This is not done in a superclass as this is not compatible with the current
// class hierarchy.

// ----- Expression builders ------

IntExpr* BuildStartExpr(IntervalVar* var);
IntExpr* BuildDurationExpr(IntervalVar* var);
IntExpr* BuildEndExpr(IntervalVar* var);
IntExpr* BuildSafeStartExpr(IntervalVar* var, int64 unperformed_value);
IntExpr* BuildSafeDurationExpr(IntervalVar* var, int64 unperformed_value);
IntExpr* BuildSafeEndExpr(IntervalVar* var, int64 unperformed_value);
void LinkVarExpr(Solver* const s, IntExpr* const expr, IntVar* const var);

// It's good to have the two extreme values being symmetrical around zero: it
// makes mirroring easier.
const int64 IntervalVar::kMaxValidValue = kint64max >> 2;
const int64 IntervalVar::kMinValidValue = -kMaxValidValue;

namespace {
enum IntervalField { START, DURATION, END };

IntervalVar* NullInterval() { return nullptr; }
// ----- MirrorIntervalVar -----

class MirrorIntervalVar : public IntervalVar {
 public:
  MirrorIntervalVar(Solver* const s, IntervalVar* const t)
      : IntervalVar(s, "Mirror<" + t->name() + ">"), t_(t) {}
  ~MirrorIntervalVar() override {}

  // These methods query, set and watch the start position of the
  // interval var.
  int64 StartMin() const override { return -t_->EndMax(); }
  int64 StartMax() const override { return -t_->EndMin(); }
  void SetStartMin(int64 m) override { t_->SetEndMax(-m); }
  void SetStartMax(int64 m) override { t_->SetEndMin(-m); }
  void SetStartRange(int64 mi, int64 ma) override { t_->SetEndRange(-ma, -mi); }
  int64 OldStartMin() const override { return -t_->OldEndMax(); }
  int64 OldStartMax() const override { return -t_->OldEndMin(); }
  void WhenStartRange(Demon* const d) override { t_->WhenEndRange(d); }
  void WhenStartBound(Demon* const d) override { t_->WhenEndBound(d); }

  // These methods query, set and watch the duration of the interval var.
  int64 DurationMin() const override { return t_->DurationMin(); }
  int64 DurationMax() const override { return t_->DurationMax(); }
  void SetDurationMin(int64 m) override { t_->SetDurationMin(m); }
  void SetDurationMax(int64 m) override { t_->SetDurationMax(m); }
  void SetDurationRange(int64 mi, int64 ma) override {
    t_->SetDurationRange(mi, ma);
  }
  int64 OldDurationMin() const override { return t_->OldDurationMin(); }
  int64 OldDurationMax() const override { return t_->OldDurationMax(); }
  void WhenDurationRange(Demon* const d) override { t_->WhenDurationRange(d); }
  void WhenDurationBound(Demon* const d) override { t_->WhenDurationBound(d); }

  // These methods query, set and watch the end position of the interval var.
  int64 EndMin() const override { return -t_->StartMax(); }
  int64 EndMax() const override { return -t_->StartMin(); }
  void SetEndMin(int64 m) override { t_->SetStartMax(-m); }
  void SetEndMax(int64 m) override { t_->SetStartMin(-m); }
  void SetEndRange(int64 mi, int64 ma) override { t_->SetStartRange(-ma, -mi); }
  int64 OldEndMin() const override { return -t_->OldStartMax(); }
  int64 OldEndMax() const override { return -t_->OldStartMin(); }
  void WhenEndRange(Demon* const d) override { t_->WhenStartRange(d); }
  void WhenEndBound(Demon* const d) override { t_->WhenStartBound(d); }

  // These methods query, set and watches the performed status of the
  // interval var.
  bool MustBePerformed() const override { return t_->MustBePerformed(); }
  bool MayBePerformed() const override { return t_->MayBePerformed(); }
  void SetPerformed(bool val) override { t_->SetPerformed(val); }
  bool WasPerformedBound() const override { return t_->WasPerformedBound(); }
  void WhenPerformedBound(Demon* const d) override {
    t_->WhenPerformedBound(d);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, ModelVisitor::kMirrorOperation, 0, t_);
  }

  std::string DebugString() const override {
    return StringPrintf("MirrorInterval(%s)", t_->DebugString().c_str());
  }

  IntExpr* StartExpr() override {
    return solver()->MakeOpposite(t_->EndExpr());
  }
  IntExpr* DurationExpr() override { return t_->DurationExpr(); }
  IntExpr* EndExpr() override {
    return solver()->MakeOpposite(t_->StartExpr());
  }
  IntExpr* PerformedExpr() override { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return solver()->MakeOpposite(t_->SafeEndExpr(-unperformed_value));
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return t_->SafeDurationExpr(unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return solver()->MakeOpposite(t_->SafeStartExpr(-unperformed_value));
  }

 private:
  IntervalVar* const t_;
  DISALLOW_COPY_AND_ASSIGN(MirrorIntervalVar);
};

// An IntervalVar that passes all function calls to an underlying interval
// variable as long as it is not prohibited, and that interprets prohibited
// intervals as intervals of duration 0 that must be executed between
// [kMinValidValue and kMaxValidValue].
//
// Such interval variables have a very similar behavior to others.
// Invariants such as StartMin() + DurationMin() <= EndMin() that are maintained
// for traditional interval variables are maintained for instances of
// AlwaysPerformedIntervalVarWrapper. However, there is no monotonicity of the
// values returned by the start/end getters. For example, during a given
// propagation, three successive calls to StartMin could return,
// in this order, 1, 2, and kMinValidValue.
//

// This class exists so that we can easily implement the
// IntervalVarRelaxedMax and IntervalVarRelaxedMin classes below.
class AlwaysPerformedIntervalVarWrapper : public IntervalVar {
 public:
  explicit AlwaysPerformedIntervalVarWrapper(IntervalVar* const t)
      : IntervalVar(t->solver(),
                    StringPrintf("AlwaysPerformed<%s>", t->name().c_str())),
        t_(t),
        start_expr_(nullptr),
        duration_expr_(nullptr),
        end_expr_(nullptr) {}

  ~AlwaysPerformedIntervalVarWrapper() override {}
  int64 StartMin() const override {
    return MayUnderlyingBePerformed() ? t_->StartMin() : kMinValidValue;
  }
  int64 StartMax() const override {
    return MayUnderlyingBePerformed() ? t_->StartMax() : kMaxValidValue;
  }
  void SetStartMin(int64 m) override { t_->SetStartMin(m); }
  void SetStartMax(int64 m) override { t_->SetStartMax(m); }
  void SetStartRange(int64 mi, int64 ma) override { t_->SetStartRange(mi, ma); }
  int64 OldStartMin() const override {
    return MayUnderlyingBePerformed() ? t_->OldStartMin() : kMinValidValue;
  }
  int64 OldStartMax() const override {
    return MayUnderlyingBePerformed() ? t_->OldStartMax() : kMaxValidValue;
  }
  void WhenStartRange(Demon* const d) override { t_->WhenStartRange(d); }
  void WhenStartBound(Demon* const d) override { t_->WhenStartBound(d); }
  int64 DurationMin() const override {
    return MayUnderlyingBePerformed() ? t_->DurationMin() : 0LL;
  }
  int64 DurationMax() const override {
    return MayUnderlyingBePerformed() ? t_->DurationMax() : 0LL;
  }
  void SetDurationMin(int64 m) override { t_->SetDurationMin(m); }
  void SetDurationMax(int64 m) override { t_->SetDurationMax(m); }
  void SetDurationRange(int64 mi, int64 ma) override {
    t_->SetDurationRange(mi, ma);
  }
  int64 OldDurationMin() const override {
    return MayUnderlyingBePerformed() ? t_->OldDurationMin() : 0LL;
  }
  int64 OldDurationMax() const override {
    return MayUnderlyingBePerformed() ? t_->OldDurationMax() : 0LL;
  }
  void WhenDurationRange(Demon* const d) override { t_->WhenDurationRange(d); }
  void WhenDurationBound(Demon* const d) override { t_->WhenDurationBound(d); }
  int64 EndMin() const override {
    return MayUnderlyingBePerformed() ? t_->EndMin() : kMinValidValue;
  }
  int64 EndMax() const override {
    return MayUnderlyingBePerformed() ? t_->EndMax() : kMaxValidValue;
  }
  void SetEndMin(int64 m) override { t_->SetEndMin(m); }
  void SetEndMax(int64 m) override { t_->SetEndMax(m); }
  void SetEndRange(int64 mi, int64 ma) override { t_->SetEndRange(mi, ma); }
  int64 OldEndMin() const override {
    return MayUnderlyingBePerformed() ? t_->OldEndMin() : kMinValidValue;
  }
  int64 OldEndMax() const override {
    return MayUnderlyingBePerformed() ? t_->OldEndMax() : kMaxValidValue;
  }
  void WhenEndRange(Demon* const d) override { t_->WhenEndRange(d); }
  void WhenEndBound(Demon* const d) override { t_->WhenEndBound(d); }
  bool MustBePerformed() const override { return true; }
  bool MayBePerformed() const override { return true; }
  void SetPerformed(bool val) override {
    // An AlwaysPerformedIntervalVarWrapper interval variable is always
    // performed. So setting it to be performed does not change anything,
    // and setting it not to be performed is inconsistent and should cause
    // a failure.
    if (!val) {
      solver()->Fail();
    }
  }
  bool WasPerformedBound() const override { return true; }
  void WhenPerformedBound(Demon* const d) override {
    t_->WhenPerformedBound(d);
  }
  IntExpr* StartExpr() override {
    if (start_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&start_expr_));
      start_expr_ = BuildStartExpr(this);
    }
    return start_expr_;
  }
  IntExpr* DurationExpr() override {
    if (duration_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&duration_expr_));
      duration_expr_ = BuildDurationExpr(this);
    }
    return duration_expr_;
  }
  IntExpr* EndExpr() override {
    if (end_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&end_expr_));
      end_expr_ = BuildEndExpr(this);
    }
    return end_expr_;
  }
  IntExpr* PerformedExpr() override { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return StartExpr();
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return DurationExpr();
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override { return EndExpr(); }

 protected:
  IntervalVar* const underlying() const { return t_; }
  bool MayUnderlyingBePerformed() const {
    return underlying()->MayBePerformed();
  }

 private:
  IntervalVar* const t_;
  IntExpr* start_expr_;
  IntExpr* duration_expr_;
  IntExpr* end_expr_;
  DISALLOW_COPY_AND_ASSIGN(AlwaysPerformedIntervalVarWrapper);
};

// An interval variable that wraps around an underlying one, relaxing the max
// start and end. Relaxing means making unbounded when optional.
//
// More precisely, such an interval variable behaves as follows:
// * When the underlying must be performed, this interval variable behaves
//     exactly as the underlying;
// * When the underlying may or may not be performed, this interval variable
//     behaves like the underlying, except that it is unbounded on the max side;
// * When the underlying cannot be performed, this interval variable is of
//      duration 0 and must be performed in an interval unbounded on both sides.
//
// This class is very useful to implement propagators that may only modify
// the start min or end min.
class IntervalVarRelaxedMax : public AlwaysPerformedIntervalVarWrapper {
 public:
  explicit IntervalVarRelaxedMax(IntervalVar* const t)
      : AlwaysPerformedIntervalVarWrapper(t) {}
  ~IntervalVarRelaxedMax() override {}
  int64 StartMax() const override {
    // It matters to use DurationMin() and not underlying()->DurationMin() here.
    return underlying()->MustBePerformed() ? underlying()->StartMax()
                                           : (kMaxValidValue - DurationMin());
  }
  void SetStartMax(int64 m) override {
    LOG(FATAL)
        << "Calling SetStartMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  int64 EndMax() const override {
    return underlying()->MustBePerformed() ? underlying()->EndMax()
                                           : kMaxValidValue;
  }
  void SetEndMax(int64 m) override {
    LOG(FATAL)
        << "Calling SetEndMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMaxOperation, 0,
                                   underlying());
  }

  std::string DebugString() const override {
    return StringPrintf("IntervalVarRelaxedMax(%s)",
                        underlying()->DebugString().c_str());
  }
};

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
class IntervalVarRelaxedMin : public AlwaysPerformedIntervalVarWrapper {
 public:
  explicit IntervalVarRelaxedMin(IntervalVar* const t)
      : AlwaysPerformedIntervalVarWrapper(t) {}
  ~IntervalVarRelaxedMin() override {}
  int64 StartMin() const override {
    return underlying()->MustBePerformed() ? underlying()->StartMin()
                                           : kMinValidValue;
  }
  void SetStartMin(int64 m) override {
    LOG(FATAL)
        << "Calling SetStartMin on a IntervalVarRelaxedMin is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  int64 EndMin() const override {
    // It matters to use DurationMin() and not underlying()->DurationMin() here.
    return underlying()->MustBePerformed() ? underlying()->EndMin()
                                           : (kMinValidValue + DurationMin());
  }
  void SetEndMin(int64 m) override {
    LOG(FATAL)
        << "Calling SetEndMin on a IntervalVarRelaxedMin is not supported, "
        << "as it seems there is no legitimate use case.";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMinOperation, 0,
                                   underlying());
  }

  std::string DebugString() const override {
    return StringPrintf("IntervalVarRelaxedMin(%s)",
                        underlying()->DebugString().c_str());
  }
};

// ----- BaseIntervalVar -----

class BaseIntervalVar : public IntervalVar {
 public:
  class Handler : public Demon {
   public:
    explicit Handler(BaseIntervalVar* const var) : var_(var) {}
    ~Handler() override {}
    void Run(Solver* const s) override { var_->Process(); }
    Solver::DemonPriority priority() const override {
      return Solver::VAR_PRIORITY;
    }
    std::string DebugString() const override {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }

   private:
    BaseIntervalVar* const var_;
  };

  BaseIntervalVar(Solver* const s, const std::string& name)
      : IntervalVar(s, name),
        in_process_(false),
        handler_(this),
        cleaner_([this](Solver* s) { CleanInProcess(); }) {}

  ~BaseIntervalVar() override {}

  virtual void Process() = 0;

  virtual void Push() = 0;

  void CleanInProcess() { in_process_ = false; }

  std::string BaseName() const override { return "IntervalVar"; }

  bool InProcess() const { return in_process_; }

 protected:
  bool in_process_;
  Handler handler_;
  Solver::Action cleaner_;
};

class RangeVar : public IntExpr {
 public:
  RangeVar(Solver* const s, BaseIntervalVar* var, int64 mi, int64 ma)
      : IntExpr(s),
        min_(mi),
        max_(ma),
        var_(var),
        postponed_min_(mi),
        postponed_max_(ma),
        previous_min_(mi),
        previous_max_(ma),
        cast_var_(nullptr) {}

  ~RangeVar() override {}

  bool Bound() const override { return min_.Value() == max_.Value(); }

  int64 Min() const override { return min_.Value(); }

  int64 Max() const override { return max_.Value(); }

  void SetMin(int64 m) override {
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

  int64 OldMin() const {
    DCHECK(var_->InProcess());
    return previous_min_;
  }

  void SetMax(int64 m) override {
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

  int64 OldMax() const { return previous_min_; }

  void SetRange(int64 mi, int64 ma) override {
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

  void WhenRange(Demon* const demon) override {
    if (!Bound()) {
      if (demon->priority() == Solver::DELAYED_PRIORITY) {
        delayed_range_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(demon));
      } else {
        range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(demon));
      }
    }
  }

  virtual void WhenBound(Demon* const demon) {
    if (!Bound()) {
      if (demon->priority() == Solver::DELAYED_PRIORITY) {
        delayed_bound_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(demon));
      } else {
        bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(demon));
      }
    }
  }

  void UpdatePostponedBounds() {
    postponed_min_ = min_.Value();
    postponed_max_ = max_.Value();
  }

  void ProcessDemons() {
    if (Bound()) {
      ExecuteAll(bound_demons_);
      EnqueueAll(delayed_bound_demons_);
    }
    if (min_.Value() != previous_min_ || max_.Value() != previous_max_) {
      ExecuteAll(range_demons_);
      EnqueueAll(delayed_range_demons_);
    }
  }

  void UpdatePreviousBounds() {
    previous_min_ = min_.Value();
    previous_max_ = max_.Value();
  }

  // TODO(user): Remove this interval field enum.
  void ApplyPostponedBounds(IntervalField which) {
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

  IntVar* Var() override {
    if (cast_var_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&cast_var_));
      cast_var_ = solver()->MakeIntVar(min_.Value(), max_.Value());
      LinkVarExpr(solver(), this, cast_var_);
    }
    return cast_var_;
  }

  std::string DebugString() const override {
    std::string out = StrCat(min_.Value());
    if (!Bound()) {
      StringAppendF(&out, " .. %" GG_LL_FORMAT "d", max_.Value());
    }
    return out;
  }

 private:
  // The previous bounds are maintained lazily and non reversibly.
  // When going down in the search tree, the modifications are
  // monotonic, thus SyncPreviousBounds is a no-op because they are
  // correctly updated at the end of the ProcessDemons() call. After
  // a fail, if they are inconsistent, then they will be outside the
  // current interval, thus this check.
  void SyncPreviousBounds() {
    if (previous_min_ > min_.Value()) {
      previous_min_ = min_.Value();
    }
    if (previous_max_ < max_.Value()) {
      previous_max_ = max_.Value();
    }
  }

  // The current reversible bounds of the interval.
  NumericalRev<int64> min_;
  NumericalRev<int64> max_;
  BaseIntervalVar* const var_;
  // When in process, the modifications are postponed and stored in
  // these 2 fields.
  int64 postponed_min_;
  int64 postponed_max_;
  // The previous bounds stores the bounds since the last time
  // ProcessDemons() was run. These are maintained lazily.
  int64 previous_min_;
  int64 previous_max_;
  // Demons attached to the 'bound' event (min == max).
  SimpleRevFIFO<Demon*> bound_demons_;
  SimpleRevFIFO<Demon*> delayed_bound_demons_;
  // Demons attached to a modification of bounds.
  SimpleRevFIFO<Demon*> range_demons_;
  SimpleRevFIFO<Demon*> delayed_range_demons_;
  IntVar* cast_var_;
};  // class RangeVar

// ----- PerformedVar -----

class PerformedVar : public BooleanVar {
 public:
  // Optional = true -> var = [0..1], Optional = false -> var = [1].
  PerformedVar(Solver* const s, BaseIntervalVar* const var, bool optional)
      : BooleanVar(s, ""),
        var_(var),
        previous_value_(optional ? kUnboundBooleanVarValue : 1),
        postponed_value_(optional ? kUnboundBooleanVarValue : 1) {
    if (!optional) {
      value_ = 1;
    }
  }
  // var = [0] (always unperformed).
  PerformedVar(Solver* const s, BaseIntervalVar* var)
      : BooleanVar(s, ""), var_(var), previous_value_(0), postponed_value_(0) {
    value_ = 1;
  }

  ~PerformedVar() override {}

  void SetValue(int64 v) override {
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
      InternalSaveBooleanVarValue(solver(), this);
      value_ = static_cast<int>(v);
      var_->Push();
    }
  }

  int64 OldMin() const override { return previous_value_ == 1; }

  int64 OldMax() const override { return previous_value_ != 0; }

  void RestoreValue() override {
    previous_value_ = kUnboundBooleanVarValue;
    value_ = kUnboundBooleanVarValue;
    postponed_value_ = kUnboundBooleanVarValue;
  }

  void Process() {
    if (previous_value_ != value_) {
      ExecuteAll(bound_demons_);
      EnqueueAll(delayed_bound_demons_);
    }
  }

  void UpdatePostponedValue() { postponed_value_ = value_; }

  void UpdatePreviousValueAndApplyPostponedValue() {
    previous_value_ = value_;
    if (value_ != postponed_value_) {
      DCHECK_NE(kUnboundBooleanVarValue, postponed_value_);
      SetValue(postponed_value_);
    }
  }

  std::string DebugString() const override {
    switch (value_) {
      case 0:
        return "false";
      case 1:
        return "true";
      default:
        return "undecided";
    }
  }

 private:
  BaseIntervalVar* const var_;
  int previous_value_;
  int postponed_value_;
};

// ----- FixedDurationIntervalVar -----

class FixedDurationIntervalVar : public BaseIntervalVar {
 public:
  FixedDurationIntervalVar(Solver* const s, int64 start_min, int64 start_max,
                           int64 duration, bool optional, const std::string& name);
  // Unperformed interval.
  FixedDurationIntervalVar(Solver* const s, const std::string& name);
  ~FixedDurationIntervalVar() override {}

  int64 StartMin() const override;
  int64 StartMax() const override;
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_.OldMin(); }
  int64 OldStartMax() const override { return start_.OldMax(); }
  void WhenStartRange(Demon* const d) override {
    if (performed_.Max() == 1) {
      start_.WhenRange(d);
    }
  }
  void WhenStartBound(Demon* const d) override {
    if (performed_.Max() == 1) {
      start_.WhenBound(d);
    }
  }

  int64 DurationMin() const override;
  int64 DurationMax() const override;
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}

  int64 EndMin() const override;
  int64 EndMax() const override;
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return CapAdd(OldStartMin(), duration_); }
  int64 OldEndMax() const override { return CapAdd(OldStartMax(), duration_); }
  void WhenEndRange(Demon* const d) override { WhenStartRange(d); }
  void WhenEndBound(Demon* const d) override { WhenStartBound(d); }

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override {
    return performed_.OldMin() == performed_.OldMax();
  }
  void WhenPerformedBound(Demon* const d) override { performed_.WhenBound(d); }
  void Process() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  IntExpr* StartExpr() override { return &start_; }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(StartExpr(), duration_);
  }
  IntExpr* PerformedExpr() override { return &performed_; }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return BuildSafeEndExpr(this, unperformed_value);
  }

  void Push() override;

 private:
  RangeVar start_;
  int64 duration_;
  PerformedVar performed_;
};

FixedDurationIntervalVar::FixedDurationIntervalVar(
    Solver* const s, int64 start_min, int64 start_max, int64 duration,
    bool optional, const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, start_min, start_max),
      duration_(duration),
      performed_(s, this, optional) {}

FixedDurationIntervalVar::FixedDurationIntervalVar(Solver* const s,
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

int64 FixedDurationIntervalVar::StartMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Min();
}

int64 FixedDurationIntervalVar::StartMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Max();
}

void FixedDurationIntervalVar::SetStartMin(int64 m) {
  if (performed_.Max() == 1) {
    start_.SetMin(m);
  }
}

void FixedDurationIntervalVar::SetStartMax(int64 m) {
  if (performed_.Max() == 1) {
    start_.SetMax(m);
  }
}

void FixedDurationIntervalVar::SetStartRange(int64 mi, int64 ma) {
  if (performed_.Max() == 1) {
    start_.SetRange(mi, ma);
  }
}

int64 FixedDurationIntervalVar::DurationMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_;
}

int64 FixedDurationIntervalVar::DurationMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return duration_;
}

void FixedDurationIntervalVar::SetDurationMin(int64 m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void FixedDurationIntervalVar::SetDurationMax(int64 m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}

void FixedDurationIntervalVar::SetDurationRange(int64 mi, int64 ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

int64 FixedDurationIntervalVar::EndMin() const {
  CHECK_EQ(performed_.Max(), 1);
  return start_.Min() + duration_;
}

int64 FixedDurationIntervalVar::EndMax() const {
  CHECK_EQ(performed_.Max(), 1);
  return CapAdd(start_.Max(), duration_);
}

void FixedDurationIntervalVar::SetEndMin(int64 m) {
  SetStartMin(CapSub(m, duration_));
}

void FixedDurationIntervalVar::SetEndMax(int64 m) {
  SetStartMax(CapSub(m, duration_));
}

void FixedDurationIntervalVar::SetEndRange(int64 mi, int64 ma) {
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
      return StringPrintf("%s(performed = false)", var_name.c_str());
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
    StringAppendF(&out, "%s, duration = %" GG_LL_FORMAT "d, performed = %s)",
                  start_.DebugString().c_str(), duration_,
                  performed_.DebugString().c_str());
    return out;
  }
}

// ----- FixedDurationPerformedIntervalVar -----

class FixedDurationPerformedIntervalVar : public BaseIntervalVar {
 public:
  FixedDurationPerformedIntervalVar(Solver* const s, int64 start_min,
                                    int64 start_max, int64 duration,
                                    const std::string& name);
  // Unperformed interval.
  FixedDurationPerformedIntervalVar(Solver* const s, const std::string& name);
  ~FixedDurationPerformedIntervalVar() override {}

  int64 StartMin() const override;
  int64 StartMax() const override;
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_.OldMin(); }
  int64 OldStartMax() const override { return start_.OldMax(); }
  void WhenStartRange(Demon* const d) override { start_.WhenRange(d); }
  void WhenStartBound(Demon* const d) override { start_.WhenBound(d); }

  int64 DurationMin() const override;
  int64 DurationMax() const override;
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}

  int64 EndMin() const override;
  int64 EndMax() const override;
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return CapAdd(OldStartMin(), duration_); }
  int64 OldEndMax() const override { return CapAdd(OldStartMax(), duration_); }
  void WhenEndRange(Demon* const d) override { WhenStartRange(d); }
  void WhenEndBound(Demon* const d) override { WhenEndRange(d); }

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override { return true; }
  void WhenPerformedBound(Demon* const d) override {}
  void Process() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  IntExpr* StartExpr() override { return &start_; }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(StartExpr(), duration_);
  }
  IntExpr* PerformedExpr() override { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return StartExpr();
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return DurationExpr();
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override { return EndExpr(); }

 private:
  void CheckOldPerformed() {}
  void Push() override;

  RangeVar start_;
  int64 duration_;
};

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* const s, int64 start_min, int64 start_max, int64 duration,
    const std::string& name)
    : BaseIntervalVar(s, name),
      start_(s, this, start_min, start_max),
      duration_(duration) {}

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* const s, const std::string& name)
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

int64 FixedDurationPerformedIntervalVar::StartMin() const {
  return start_.Min();
}

int64 FixedDurationPerformedIntervalVar::StartMax() const {
  return start_.Max();
}

void FixedDurationPerformedIntervalVar::SetStartMin(int64 m) {
  start_.SetMin(m);
}

void FixedDurationPerformedIntervalVar::SetStartMax(int64 m) {
  start_.SetMax(m);
}

void FixedDurationPerformedIntervalVar::SetStartRange(int64 mi, int64 ma) {
  start_.SetRange(mi, ma);
}

int64 FixedDurationPerformedIntervalVar::DurationMin() const {
  return duration_;
}

int64 FixedDurationPerformedIntervalVar::DurationMax() const {
  return duration_;
}

void FixedDurationPerformedIntervalVar::SetDurationMin(int64 m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void FixedDurationPerformedIntervalVar::SetDurationMax(int64 m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}
int64 FixedDurationPerformedIntervalVar::EndMin() const {
  return CapAdd(start_.Min(), duration_);
}

int64 FixedDurationPerformedIntervalVar::EndMax() const {
  return CapAdd(start_.Max(), duration_);
}

void FixedDurationPerformedIntervalVar::SetEndMin(int64 m) {
  SetStartMin(CapSub(m, duration_));
}

void FixedDurationPerformedIntervalVar::SetEndMax(int64 m) {
  SetStartMax(CapSub(m, duration_));
}

void FixedDurationPerformedIntervalVar::SetEndRange(int64 mi, int64 ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

void FixedDurationPerformedIntervalVar::SetDurationRange(int64 mi, int64 ma) {
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
  StringAppendF(&out, "%s, duration = %" GG_LL_FORMAT "d, performed = true)",
                start_.DebugString().c_str(), duration_);
  return out;
}

// ----- StartVarPerformedIntervalVar -----

class StartVarPerformedIntervalVar : public IntervalVar {
 public:
  StartVarPerformedIntervalVar(Solver* const s, IntVar* const start_var,
                               int64 duration, const std::string& name);
  ~StartVarPerformedIntervalVar() override {}

  int64 StartMin() const override;
  int64 StartMax() const override;
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_var_->OldMin(); }
  int64 OldStartMax() const override { return start_var_->OldMax(); }
  void WhenStartRange(Demon* const d) override { start_var_->WhenRange(d); }
  void WhenStartBound(Demon* const d) override { start_var_->WhenBound(d); }

  int64 DurationMin() const override;
  int64 DurationMax() const override;
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}

  int64 EndMin() const override;
  int64 EndMax() const override;
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return CapAdd(OldStartMin(), duration_); }
  int64 OldEndMax() const override { return CapAdd(OldStartMax(), duration_); }
  void WhenEndRange(Demon* const d) override { start_var_->WhenRange(d); }
  void WhenEndBound(Demon* const d) override { start_var_->WhenBound(d); }

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override { return true; }
  void WhenPerformedBound(Demon* const d) override {}
  std::string DebugString() const override;

  IntExpr* StartExpr() override { return start_var_; }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(start_var_, duration_);
  }
  IntExpr* PerformedExpr() override { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return StartExpr();
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return DurationExpr();
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override { return EndExpr(); }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

 private:
  IntVar* const start_var_;
  int64 duration_;
};

// TODO(user): Take care of overflows.
StartVarPerformedIntervalVar::StartVarPerformedIntervalVar(Solver* const s,
                                                           IntVar* const var,
                                                           int64 duration,
                                                           const std::string& name)
    : IntervalVar(s, name), start_var_(var), duration_(duration) {}

int64 StartVarPerformedIntervalVar::StartMin() const {
  return start_var_->Min();
}

int64 StartVarPerformedIntervalVar::StartMax() const {
  return start_var_->Max();
}

void StartVarPerformedIntervalVar::SetStartMin(int64 m) {
  start_var_->SetMin(m);
}

void StartVarPerformedIntervalVar::SetStartMax(int64 m) {
  start_var_->SetMax(m);
}

void StartVarPerformedIntervalVar::SetStartRange(int64 mi, int64 ma) {
  start_var_->SetRange(mi, ma);
}

int64 StartVarPerformedIntervalVar::DurationMin() const { return duration_; }

int64 StartVarPerformedIntervalVar::DurationMax() const { return duration_; }

void StartVarPerformedIntervalVar::SetDurationMin(int64 m) {
  if (m > duration_) {
    solver()->Fail();
  }
}

void StartVarPerformedIntervalVar::SetDurationMax(int64 m) {
  if (m < duration_) {
    solver()->Fail();
  }
}
int64 StartVarPerformedIntervalVar::EndMin() const {
  return start_var_->Min() + duration_;
}

int64 StartVarPerformedIntervalVar::EndMax() const {
  return start_var_->Max() + duration_;
}

void StartVarPerformedIntervalVar::SetEndMin(int64 m) {
  SetStartMin(CapSub(m, duration_));
}

void StartVarPerformedIntervalVar::SetEndMax(int64 m) {
  SetStartMax(CapSub(m, duration_));
}

void StartVarPerformedIntervalVar::SetEndRange(int64 mi, int64 ma) {
  SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
}

void StartVarPerformedIntervalVar::SetDurationRange(int64 mi, int64 ma) {
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
  StringAppendF(&out, "%" GG_LL_FORMAT "d", start_var_->Min());
  if (!start_var_->Bound()) {
    StringAppendF(&out, " .. %" GG_LL_FORMAT "d", start_var_->Max());
  }

  StringAppendF(&out, ", duration = %" GG_LL_FORMAT "d, performed = true)",
                duration_);
  return out;
}

// ----- StartVarIntervalVar -----

class StartVarIntervalVar : public BaseIntervalVar {
 public:
  StartVarIntervalVar(Solver* const s, IntVar* const start, int64 duration,
                      IntVar* const performed, const std::string& name);
  ~StartVarIntervalVar() override {}

  int64 StartMin() const override;
  int64 StartMax() const override;
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_->OldMin(); }
  int64 OldStartMax() const override { return start_->OldMax(); }
  void WhenStartRange(Demon* const d) override {
    if (performed_->Max() == 1) {
      start_->WhenRange(d);
    }
  }
  void WhenStartBound(Demon* const d) override {
    if (performed_->Max() == 1) {
      start_->WhenBound(d);
    }
  }

  int64 DurationMin() const override;
  int64 DurationMax() const override;
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}

  int64 EndMin() const override;
  int64 EndMax() const override;
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return CapAdd(OldStartMin(), duration_); }
  int64 OldEndMax() const override { return CapAdd(OldStartMax(), duration_); }
  void WhenEndRange(Demon* const d) override { WhenStartRange(d); }
  void WhenEndBound(Demon* const d) override { WhenStartBound(d); }

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override {
    return performed_->OldMin() == performed_->OldMax();
  }
  void WhenPerformedBound(Demon* const d) override { performed_->WhenBound(d); }
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  IntExpr* StartExpr() override { return start_; }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(StartExpr(), duration_);
  }
  IntExpr* PerformedExpr() override { return performed_; }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return BuildSafeEndExpr(this, unperformed_value);
  }

  void Process() override { LOG(FATAL) << "Should not be here"; }

  void Push() override { LOG(FATAL) << "Should not be here"; }

  int64 StoredMin() const { return start_min_.Value(); }
  int64 StoredMax() const { return start_max_.Value(); }

 private:
  IntVar* const start_;
  int64 duration_;
  IntVar* const performed_;
  Rev<int64> start_min_;
  Rev<int64> start_max_;
};

StartVarIntervalVar::StartVarIntervalVar(Solver* const s, IntVar* const start,
                                         int64 duration,
                                         IntVar* const performed,
                                         const std::string& name)
    : BaseIntervalVar(s, name),
      start_(start),
      duration_(duration),
      performed_(performed),
      start_min_(start->Min()),
      start_max_(start->Max()) {}

int64 StartVarIntervalVar::StartMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return std::max(start_->Min(), start_min_.Value());
}

int64 StartVarIntervalVar::StartMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return std::min(start_->Max(), start_max_.Value());
}

void StartVarIntervalVar::SetStartMin(int64 m) {
  if (performed_->Min() == 1) {
    start_->SetMin(m);
  } else {
    start_min_.SetValue(solver(), std::max(m, start_min_.Value()));
    if (start_min_.Value() > std::min(start_max_.Value(), start_->Max())) {
      performed_->SetValue(0);
    }
  }
}

void StartVarIntervalVar::SetStartMax(int64 m) {
  if (performed_->Min() == 1) {
    start_->SetMax(m);
  } else {
    start_max_.SetValue(solver(), std::min(m, start_max_.Value()));
    if (start_max_.Value() < std::max(start_min_.Value(), start_->Min())) {
      performed_->SetValue(0);
    }
  }
}

void StartVarIntervalVar::SetStartRange(int64 mi, int64 ma) {
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

int64 StartVarIntervalVar::DurationMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return duration_;
}

int64 StartVarIntervalVar::DurationMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return duration_;
}

void StartVarIntervalVar::SetDurationMin(int64 m) {
  if (m > duration_) {
    SetPerformed(false);
  }
}

void StartVarIntervalVar::SetDurationMax(int64 m) {
  if (m < duration_) {
    SetPerformed(false);
  }
}

void StartVarIntervalVar::SetDurationRange(int64 mi, int64 ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

int64 StartVarIntervalVar::EndMin() const {
  DCHECK_EQ(performed_->Max(), 1);
  return CapAdd(StartMin(), duration_);
}

int64 StartVarIntervalVar::EndMax() const {
  DCHECK_EQ(performed_->Max(), 1);
  return CapAdd(StartMax(), duration_);
}

void StartVarIntervalVar::SetEndMin(int64 m) {
  SetStartMin(CapSub(m, duration_));
}

void StartVarIntervalVar::SetEndMax(int64 m) {
  SetStartMax(CapSub(m, duration_));
}

void StartVarIntervalVar::SetEndRange(int64 mi, int64 ma) {
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
      return StringPrintf("%s(performed = false)", var_name.c_str());
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
    StringAppendF(&out, "%s, duration = %" GG_LL_FORMAT "d, performed = %s)",
                  start_->DebugString().c_str(), duration_,
                  performed_->DebugString().c_str());
    return out;
  }
}

class LinkStartVarIntervalVar : public Constraint {
 public:
  LinkStartVarIntervalVar(Solver* const solver,
                          StartVarIntervalVar* const interval,
                          IntVar* const start, IntVar* const performed)
      : Constraint(solver),
        interval_(interval),
        start_(start),
        performed_(performed) {}

  ~LinkStartVarIntervalVar() override {}

  void Post() override {
    Demon* const demon = MakeConstraintDemon0(
        solver(), this, &LinkStartVarIntervalVar::PerformedBound,
        "PerformedBound");
    performed_->WhenBound(demon);
  }

  void InitialPropagate() override {
    if (performed_->Bound()) {
      PerformedBound();
    }
  }

  void PerformedBound() {
    if (performed_->Min() == 1) {
      start_->SetRange(interval_->StoredMin(), interval_->StoredMax());
    }
  }

 private:
  StartVarIntervalVar* const interval_;
  IntVar* const start_;
  IntVar* const performed_;
};

// ----- FixedInterval -----

class FixedInterval : public IntervalVar {
 public:
  FixedInterval(Solver* const s, int64 start, int64 duration,
                const std::string& name);
  ~FixedInterval() override {}

  int64 StartMin() const override { return start_; }
  int64 StartMax() const override { return start_; }
  void SetStartMin(int64 m) override;
  void SetStartMax(int64 m) override;
  void SetStartRange(int64 mi, int64 ma) override;
  int64 OldStartMin() const override { return start_; }
  int64 OldStartMax() const override { return start_; }
  void WhenStartRange(Demon* const d) override {}
  void WhenStartBound(Demon* const d) override {}

  int64 DurationMin() const override { return duration_; }
  int64 DurationMax() const override { return duration_; }
  void SetDurationMin(int64 m) override;
  void SetDurationMax(int64 m) override;
  void SetDurationRange(int64 mi, int64 ma) override;
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}

  int64 EndMin() const override { return start_ + duration_; }
  int64 EndMax() const override { return start_ + duration_; }
  void SetEndMin(int64 m) override;
  void SetEndMax(int64 m) override;
  void SetEndRange(int64 mi, int64 ma) override;
  int64 OldEndMin() const override { return start_ + duration_; }
  int64 OldEndMax() const override { return start_ + duration_; }
  void WhenEndRange(Demon* const d) override {}
  void WhenEndBound(Demon* const d) override {}

  bool MustBePerformed() const override { return true; }
  bool MayBePerformed() const override { return true; }
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override { return true; }
  void WhenPerformedBound(Demon* const d) override {}
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  IntExpr* StartExpr() override { return solver()->MakeIntConst(start_); }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeIntConst(start_ + duration_);
  }
  IntExpr* PerformedExpr() override { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return StartExpr();
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return DurationExpr();
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override { return EndExpr(); }

 private:
  const int64 start_;
  const int64 duration_;
};

FixedInterval::FixedInterval(Solver* const s, int64 start, int64 duration,
                             const std::string& name)
    : IntervalVar(s, name), start_(start), duration_(duration) {}

void FixedInterval::SetStartMin(int64 m) {
  if (m > start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetStartMax(int64 m) {
  if (m < start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetStartRange(int64 mi, int64 ma) {
  if (mi > start_ || ma < start_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationMin(int64 m) {
  if (m > duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationMax(int64 m) {
  if (m < duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndMin(int64 m) {
  if (m > start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndMax(int64 m) {
  if (m < start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetEndRange(int64 mi, int64 ma) {
  if (mi > start_ + duration_ || ma < start_ + duration_) {
    solver()->Fail();
  }
}

void FixedInterval::SetDurationRange(int64 mi, int64 ma) {
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
  StringAppendF(&out, "%" GG_LL_FORMAT "d, duration = %" GG_LL_FORMAT
                      "d, performed = true)",
                start_, duration_);
  return out;
}

// ----- VariableDurationIntervalVar -----

class VariableDurationIntervalVar : public BaseIntervalVar {
 public:
  VariableDurationIntervalVar(Solver* const s, int64 start_min, int64 start_max,
                              int64 duration_min, int64 duration_max,
                              int64 end_min, int64 end_max, bool optional,
                              const std::string& name)
      : BaseIntervalVar(s, name),
        start_(s, this, std::max(start_min, CapSub(end_min, duration_max)),
               std::min(start_max, CapSub(end_max, duration_min))),
        duration_(s, this, std::max(duration_min, CapSub(end_min, start_max)),
                  std::min(duration_max, CapSub(end_max, start_min))),
        end_(s, this, std::max(end_min, CapAdd(start_min, duration_min)),
             std::min(end_max, CapAdd(start_max, duration_max))),
        performed_(s, this, optional) {}

  ~VariableDurationIntervalVar() override {}

  int64 StartMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    return start_.Min();
  }

  int64 StartMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    return start_.Max();
  }

  void SetStartMin(int64 m) override {
    if (performed_.Max() == 1) {
      start_.SetMin(m);
    }
  }

  void SetStartMax(int64 m) override {
    if (performed_.Max() == 1) {
      start_.SetMax(m);
    }
  }

  void SetStartRange(int64 mi, int64 ma) override {
    if (performed_.Max() == 1) {
      start_.SetRange(mi, ma);
    }
  }

  int64 OldStartMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return start_.OldMin();
  }

  int64 OldStartMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return start_.OldMax();
  }

  void WhenStartRange(Demon* const d) override {
    if (performed_.Max() == 1) {
      start_.WhenRange(d);
    }
  }

  void WhenStartBound(Demon* const d) override {
    if (performed_.Max() == 1) {
      start_.WhenBound(d);
    }
  }

  int64 DurationMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    return duration_.Min();
  }

  int64 DurationMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    return duration_.Max();
  }

  void SetDurationMin(int64 m) override {
    if (performed_.Max() == 1) {
      duration_.SetMin(m);
    }
  }

  void SetDurationMax(int64 m) override {
    if (performed_.Max() == 1) {
      duration_.SetMax(m);
    }
  }

  void SetDurationRange(int64 mi, int64 ma) override {
    if (performed_.Max() == 1) {
      duration_.SetRange(mi, ma);
    }
  }

  int64 OldDurationMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return duration_.OldMin();
  }

  int64 OldDurationMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return duration_.OldMax();
  }

  void WhenDurationRange(Demon* const d) override {
    if (performed_.Max() == 1) {
      duration_.WhenRange(d);
    }
  }

  void WhenDurationBound(Demon* const d) override {
    if (performed_.Max() == 1) {
      duration_.WhenBound(d);
    }
  }

  int64 EndMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    return end_.Min();
  }

  int64 EndMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    return end_.Max();
  }

  void SetEndMin(int64 m) override {
    if (performed_.Max() == 1) {
      end_.SetMin(m);
    }
  }

  void SetEndMax(int64 m) override {
    if (performed_.Max() == 1) {
      end_.SetMax(m);
    }
  }

  void SetEndRange(int64 mi, int64 ma) override {
    if (performed_.Max() == 1) {
      end_.SetRange(mi, ma);
    }
  }

  int64 OldEndMin() const override {
    CHECK_EQ(performed_.Max(), 1);
    DCHECK(in_process_);
    return end_.OldMin();
  }

  int64 OldEndMax() const override {
    CHECK_EQ(performed_.Max(), 1);
    DCHECK(in_process_);
    return end_.OldMax();
  }

  void WhenEndRange(Demon* const d) override {
    if (performed_.Max() == 1) {
      end_.WhenRange(d);
    }
  }

  void WhenEndBound(Demon* const d) override {
    if (performed_.Max() == 1) {
      end_.WhenBound(d);
    }
  }

  bool MustBePerformed() const override { return (performed_.Min() == 1); }

  bool MayBePerformed() const override { return (performed_.Max() == 1); }

  void SetPerformed(bool val) override { performed_.SetValue(val); }

  bool WasPerformedBound() const override {
    CHECK(in_process_);
    return performed_.OldMin() == performed_.OldMax();
  }

  void WhenPerformedBound(Demon* const d) override { performed_.WhenBound(d); }

  void Process() override {
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

  std::string DebugString() const override {
    const std::string& var_name = name();
    if (performed_.Max() != 1) {
      if (!var_name.empty()) {
        return StringPrintf("%s(performed = false)", var_name.c_str());
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

      StringAppendF(&out, "%s, duration = %s, end = %s, performed = %s)",
                    start_.DebugString().c_str(),
                    duration_.DebugString().c_str(), end_.DebugString().c_str(),
                    performed_.DebugString().c_str());
      return out;
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  IntExpr* StartExpr() override { return &start_; }
  IntExpr* DurationExpr() override { return &duration_; }
  IntExpr* EndExpr() override { return &end_; }
  IntExpr* PerformedExpr() override { return &performed_; }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return BuildSafeEndExpr(this, unperformed_value);
  }

 private:
  void Push() override {
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

  RangeVar start_;
  RangeVar duration_;
  RangeVar end_;
  PerformedVar performed_;
};

// ----- Base synced interval var -----

class FixedDurationSyncedIntervalVar : public IntervalVar {
 public:
  FixedDurationSyncedIntervalVar(IntervalVar* const t, int64 duration,
                                 int64 offset, const std::string& name)
      : IntervalVar(t->solver(), name),
        t_(t),
        duration_(duration),
        offset_(offset) {}
  ~FixedDurationSyncedIntervalVar() override {}
  int64 DurationMin() const override { return duration_; }
  int64 DurationMax() const override { return duration_; }
  void SetDurationMin(int64 m) override {
    if (m > duration_) {
      solver()->Fail();
    }
  }
  void SetDurationMax(int64 m) override {
    if (m < duration_) {
      solver()->Fail();
    }
  }
  void SetDurationRange(int64 mi, int64 ma) override {
    if (mi > duration_ || ma < duration_ || mi > ma) {
      solver()->Fail();
    }
  }
  int64 OldDurationMin() const override { return duration_; }
  int64 OldDurationMax() const override { return duration_; }
  void WhenDurationRange(Demon* const d) override {}
  void WhenDurationBound(Demon* const d) override {}
  int64 EndMin() const override { return CapAdd(StartMin(), duration_); }
  int64 EndMax() const override { return CapAdd(StartMax(), duration_); }
  void SetEndMin(int64 m) override { SetStartMin(CapSub(m, duration_)); }
  void SetEndMax(int64 m) override { SetStartMax(CapSub(m, duration_)); }
  void SetEndRange(int64 mi, int64 ma) override {
    SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
  }
  int64 OldEndMin() const override { return CapAdd(OldStartMin(), duration_); }
  int64 OldEndMax() const override { return CapAdd(OldStartMax(), duration_); }
  void WhenEndRange(Demon* const d) override { WhenStartRange(d); }
  void WhenEndBound(Demon* const d) override { WhenStartBound(d); }
  bool MustBePerformed() const override { return t_->MustBePerformed(); }
  bool MayBePerformed() const override { return t_->MayBePerformed(); }
  void SetPerformed(bool val) override { t_->SetPerformed(val); }
  bool WasPerformedBound() const override { return t_->WasPerformedBound(); }
  void WhenPerformedBound(Demon* const d) override {
    t_->WhenPerformedBound(d);
  }

 protected:
  IntervalVar* const t_;
  const int64 duration_;
  const int64 offset_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FixedDurationSyncedIntervalVar);
};

// ----- Fixed duration interval var synced on start -----

class FixedDurationIntervalVarStartSyncedOnStart
    : public FixedDurationSyncedIntervalVar {
 public:
  FixedDurationIntervalVarStartSyncedOnStart(IntervalVar* const t,
                                             int64 duration, int64 offset)
      : FixedDurationSyncedIntervalVar(
            t, duration, offset,
            StringPrintf(
                "IntervalStartSyncedOnStart(%s, duration = %" GG_LL_FORMAT
                "d, offset = %" GG_LL_FORMAT "d)",
                t->name().c_str(), duration, offset)) {}
  ~FixedDurationIntervalVarStartSyncedOnStart() override {}
  int64 StartMin() const override { return CapAdd(t_->StartMin(), offset_); }
  int64 StartMax() const override { return CapAdd(t_->StartMax(), offset_); }
  void SetStartMin(int64 m) override { t_->SetStartMin(CapSub(m, offset_)); }
  void SetStartMax(int64 m) override { t_->SetStartMax(CapSub(m, offset_)); }
  void SetStartRange(int64 mi, int64 ma) override {
    t_->SetStartRange(CapSub(mi, offset_), CapSub(ma, offset_));
  }
  int64 OldStartMin() const override {
    return CapAdd(t_->OldStartMin(), offset_);
  }
  int64 OldStartMax() const override {
    return CapAdd(t_->OldStartMax(), offset_);
  }
  void WhenStartRange(Demon* const d) override { t_->WhenStartRange(d); }
  void WhenStartBound(Demon* const d) override { t_->WhenStartBound(d); }
  IntExpr* StartExpr() override {
    return solver()->MakeSum(t_->StartExpr(), offset_);
  }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(t_->StartExpr(), offset_ + duration_);
  }
  IntExpr* PerformedExpr() override { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return BuildSafeStartExpr(t_, unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return BuildSafeDurationExpr(t_, unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return BuildSafeEndExpr(t_, unperformed_value);
  }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(
        this, ModelVisitor::kStartSyncOnStartOperation, offset_, t_);
  }
  std::string DebugString() const override {
    return StringPrintf(
        "IntervalStartSyncedOnStart(%s, duration = %" GG_LL_FORMAT
        "d, offset = %" GG_LL_FORMAT "d)",
        t_->DebugString().c_str(), duration_, offset_);
  }
};

// ----- Fixed duration interval start synced on end -----

class FixedDurationIntervalVarStartSyncedOnEnd
    : public FixedDurationSyncedIntervalVar {
 public:
  FixedDurationIntervalVarStartSyncedOnEnd(IntervalVar* const t, int64 duration,
                                           int64 offset)
      : FixedDurationSyncedIntervalVar(
            t, duration, offset,
            StringPrintf(
                "IntervalStartSyncedOnEnd(%s, duration = %" GG_LL_FORMAT
                "d, offset = %" GG_LL_FORMAT "d)",
                t->name().c_str(), duration, offset)) {}
  ~FixedDurationIntervalVarStartSyncedOnEnd() override {}
  int64 StartMin() const override { return CapAdd(t_->EndMin(), offset_); }
  int64 StartMax() const override { return CapAdd(t_->EndMax(), offset_); }
  void SetStartMin(int64 m) override { t_->SetEndMin(CapSub(m, offset_)); }
  void SetStartMax(int64 m) override { t_->SetEndMax(CapSub(m, offset_)); }
  void SetStartRange(int64 mi, int64 ma) override {
    t_->SetEndRange(CapSub(mi, offset_), CapSub(ma, offset_));
  }
  int64 OldStartMin() const override {
    return CapAdd(t_->OldEndMin(), offset_);
  }
  int64 OldStartMax() const override {
    return CapAdd(t_->OldEndMax(), offset_);
  }
  void WhenStartRange(Demon* const d) override { t_->WhenEndRange(d); }
  void WhenStartBound(Demon* const d) override { t_->WhenEndBound(d); }
  IntExpr* StartExpr() override {
    return solver()->MakeSum(t_->EndExpr(), offset_);
  }
  IntExpr* DurationExpr() override { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() override {
    return solver()->MakeSum(t_->EndExpr(), offset_ + duration_);
  }
  IntExpr* PerformedExpr() override { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return BuildSafeStartExpr(t_, unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return BuildSafeDurationExpr(t_, unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return BuildSafeEndExpr(t_, unperformed_value);
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->VisitIntervalVariable(this, ModelVisitor::kStartSyncOnEndOperation,
                                   offset_, t_);
  }
  std::string DebugString() const override {
    return StringPrintf("IntervalStartSyncedOnEnd(%s, duration = %" GG_LL_FORMAT
                        "d, offset = %" GG_LL_FORMAT "d)",
                        t_->DebugString().c_str(), duration_, offset_);
  }
};
}  // namespace

// ----- API -----

IntervalVar* Solver::MakeMirrorInterval(IntervalVar* const t) {
  return RegisterIntervalVar(RevAlloc(new MirrorIntervalVar(this, t)));
}

IntervalVar* Solver::MakeIntervalRelaxedMax(IntervalVar* const interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMax(interval_var)));
  }
}

IntervalVar* Solver::MakeIntervalRelaxedMin(IntervalVar* const interval_var) {
  if (interval_var->MustBePerformed()) {
    return interval_var;
  } else {
    return RegisterIntervalVar(
        RevAlloc(new IntervalVarRelaxedMin(interval_var)));
  }
}

void IntervalVar::WhenAnything(Demon* const d) {
  WhenStartRange(d);
  WhenDurationRange(d);
  WhenEndRange(d);
  WhenPerformedBound(d);
}

IntervalVar* Solver::MakeFixedInterval(int64 start, int64 duration,
                                       const std::string& name) {
  return RevAlloc(new FixedInterval(this, start, duration, name));
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(int64 start_min,
                                                  int64 start_max,
                                                  int64 duration, bool optional,
                                                  const std::string& name) {
  if (start_min == start_max && !optional) {
    return MakeFixedInterval(start_min, duration, name);
  } else if (!optional) {
    return RegisterIntervalVar(RevAlloc(new FixedDurationPerformedIntervalVar(
        this, start_min, start_max, duration, name)));
  }
  return RegisterIntervalVar(RevAlloc(new FixedDurationIntervalVar(
      this, start_min, start_max, duration, optional, name)));
}

void Solver::MakeFixedDurationIntervalVarArray(
    int count, int64 start_min, int64 start_max, int64 duration, bool optional,
    const std::string& name, std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_min, start_max, duration, optional, var_name));
  }
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* const start_variable,
                                                  int64 duration,
                                                  const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK_GE(duration, 0);
  return RegisterIntervalVar(RevAlloc(
      new StartVarPerformedIntervalVar(this, start_variable, duration, name)));
}

// Creates an interval var with a fixed duration, and performed var.
// The duration must be greater than 0.
IntervalVar* Solver::MakeFixedDurationIntervalVar(
    IntVar* const start_variable, int64 duration,
    IntVar* const performed_variable, const std::string& name) {
  CHECK(start_variable != nullptr);
  CHECK(performed_variable != nullptr);
  CHECK_GE(duration, 0);
  if (!performed_variable->Bound()) {
    StartVarIntervalVar* const interval =
        reinterpret_cast<StartVarIntervalVar*>(
            RegisterIntervalVar(RevAlloc(new StartVarIntervalVar(
                this, start_variable, duration, performed_variable, name))));
    AddConstraint(RevAlloc(new LinkStartVarIntervalVar(
        this, interval, start_variable, performed_variable)));
    return interval;
  } else if (performed_variable->Min() == 1) {
    return RegisterIntervalVar(RevAlloc(new StartVarPerformedIntervalVar(
        this, start_variable, duration, name)));
  }
  return nullptr;
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables, int64 duration,
    const std::string& name, std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(
        MakeFixedDurationIntervalVar(start_variables[i], duration, var_name));
  }
}

// This method fills the vector with interval variables built with
// the corresponding start variables.
void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int64>& durations, const std::string& name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int>& durations, const std::string& name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int>& durations,
    const std::vector<IntVar*>& performed_variables, const std::string& name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int64>& durations,
    const std::vector<IntVar*>& performed_variables, const std::string& name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeFixedDurationIntervalVar(
        start_variables[i], durations[i], performed_variables[i], var_name));
  }
}

// Variable Duration Interval Var

IntervalVar* Solver::MakeIntervalVar(int64 start_min, int64 start_max,
                                     int64 duration_min, int64 duration_max,
                                     int64 end_min, int64 end_max,
                                     bool optional, const std::string& name) {
  return RegisterIntervalVar(RevAlloc(new VariableDurationIntervalVar(
      this, start_min, start_max, duration_min, duration_max, end_min, end_max,
      optional, name)));
}

void Solver::MakeIntervalVarArray(int count, int64 start_min, int64 start_max,
                                  int64 duration_min, int64 duration_max,
                                  int64 end_min, int64 end_max, bool optional,
                                  const std::string& name,
                                  std::vector<IntervalVar*>* const array) {
  CHECK_GT(count, 0);
  CHECK(array != nullptr);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const std::string var_name = StrCat(name, i);
    array->push_back(MakeIntervalVar(start_min, start_max, duration_min,
                                     duration_max, end_min, end_max, optional,
                                     var_name));
  }
}

// Synced Interval Vars
IntervalVar* Solver::MakeFixedDurationStartSyncedOnStartIntervalVar(
    IntervalVar* const interval_var, int64 duration, int64 offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationStartSyncedOnEndIntervalVar(
    IntervalVar* const interval_var, int64 duration, int64 offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(interval_var,
                                                            duration, offset)));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnStartIntervalVar(
    IntervalVar* const interval_var, int64 duration, int64 offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnStart(
          interval_var, duration, CapSub(offset, duration))));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnEndIntervalVar(
    IntervalVar* const interval_var, int64 duration, int64 offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(
          interval_var, duration, CapSub(offset, duration))));
}
}  // namespace operations_research
