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
#include "util/saturated_arithmetic.h"

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
enum IntervalField {
  START,
  DURATION,
  END
};

IntervalVar* NullInterval() { return nullptr; }
// ----- MirrorIntervalVar -----

class MirrorIntervalVar : public IntervalVar {
 public:
  MirrorIntervalVar(Solver* const s, IntervalVar* const t)
      : IntervalVar(s, "Mirror<" + t->name() + ">"), t_(t) {}
  virtual ~MirrorIntervalVar() {}

  // These methods query, set and watch the start position of the
  // interval var.
  virtual int64 StartMin() const { return -t_->EndMax(); }
  virtual int64 StartMax() const { return -t_->EndMin(); }
  virtual void SetStartMin(int64 m) { t_->SetEndMax(-m); }
  virtual void SetStartMax(int64 m) { t_->SetEndMin(-m); }
  virtual void SetStartRange(int64 mi, int64 ma) { t_->SetEndRange(-ma, -mi); }
  virtual int64 OldStartMin() const { return -t_->OldEndMax(); }
  virtual int64 OldStartMax() const { return -t_->OldEndMin(); }
  virtual void WhenStartRange(Demon* const d) { t_->WhenEndRange(d); }
  virtual void WhenStartBound(Demon* const d) { t_->WhenEndBound(d); }

  // These methods query, set and watch the duration of the interval var.
  virtual int64 DurationMin() const { return t_->DurationMin(); }
  virtual int64 DurationMax() const { return t_->DurationMax(); }
  virtual void SetDurationMin(int64 m) { t_->SetDurationMin(m); }
  virtual void SetDurationMax(int64 m) { t_->SetDurationMax(m); }
  virtual void SetDurationRange(int64 mi, int64 ma) {
    t_->SetDurationRange(mi, ma);
  }
  virtual int64 OldDurationMin() const { return t_->OldDurationMin(); }
  virtual int64 OldDurationMax() const { return t_->OldDurationMax(); }
  virtual void WhenDurationRange(Demon* const d) { t_->WhenDurationRange(d); }
  virtual void WhenDurationBound(Demon* const d) { t_->WhenDurationBound(d); }

  // These methods query, set and watch the end position of the interval var.
  virtual int64 EndMin() const { return -t_->StartMax(); }
  virtual int64 EndMax() const { return -t_->StartMin(); }
  virtual void SetEndMin(int64 m) { t_->SetStartMax(-m); }
  virtual void SetEndMax(int64 m) { t_->SetStartMin(-m); }
  virtual void SetEndRange(int64 mi, int64 ma) { t_->SetStartRange(-ma, -mi); }
  virtual int64 OldEndMin() const { return -t_->OldStartMax(); }
  virtual int64 OldEndMax() const { return -t_->OldStartMin(); }
  virtual void WhenEndRange(Demon* const d) { t_->WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { t_->WhenStartBound(d); }

  // These methods query, set and watches the performed status of the
  // interval var.
  virtual bool MustBePerformed() const { return t_->MustBePerformed(); }
  virtual bool MayBePerformed() const { return t_->MayBePerformed(); }
  virtual void SetPerformed(bool val) { t_->SetPerformed(val); }
  virtual bool WasPerformedBound() const { return t_->WasPerformedBound(); }
  virtual void WhenPerformedBound(Demon* const d) { t_->WhenPerformedBound(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, ModelVisitor::kMirrorOperation, 0, t_);
  }

  virtual std::string DebugString() const {
    return StringPrintf("MirrorInterval(%s)", t_->DebugString().c_str());
  }

  virtual IntExpr* StartExpr() { return solver()->MakeOpposite(t_->EndExpr()); }
  virtual IntExpr* DurationExpr() { return t_->DurationExpr(); }
  virtual IntExpr* EndExpr() { return solver()->MakeOpposite(t_->StartExpr()); }
  virtual IntExpr* PerformedExpr() { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return solver()->MakeOpposite(t_->SafeEndExpr(-unperformed_value));
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return t_->SafeDurationExpr(unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
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

  virtual ~AlwaysPerformedIntervalVarWrapper() {}
  virtual int64 StartMin() const {
    return MayUnderlyingBePerformed() ? t_->StartMin() : kMinValidValue;
  }
  virtual int64 StartMax() const {
    return MayUnderlyingBePerformed() ? t_->StartMax() : kMaxValidValue;
  }
  virtual void SetStartMin(int64 m) { t_->SetStartMin(m); }
  virtual void SetStartMax(int64 m) { t_->SetStartMax(m); }
  virtual void SetStartRange(int64 mi, int64 ma) { t_->SetStartRange(mi, ma); }
  virtual int64 OldStartMin() const {
    return MayUnderlyingBePerformed() ? t_->OldStartMin() : kMinValidValue;
  }
  virtual int64 OldStartMax() const {
    return MayUnderlyingBePerformed() ? t_->OldStartMax() : kMaxValidValue;
  }
  virtual void WhenStartRange(Demon* const d) { t_->WhenStartRange(d); }
  virtual void WhenStartBound(Demon* const d) { t_->WhenStartBound(d); }
  virtual int64 DurationMin() const {
    return MayUnderlyingBePerformed() ? t_->DurationMin() : 0LL;
  }
  virtual int64 DurationMax() const {
    return MayUnderlyingBePerformed() ? t_->DurationMax() : 0LL;
  }
  virtual void SetDurationMin(int64 m) { t_->SetDurationMin(m); }
  virtual void SetDurationMax(int64 m) { t_->SetDurationMax(m); }
  virtual void SetDurationRange(int64 mi, int64 ma) {
    t_->SetDurationRange(mi, ma);
  }
  virtual int64 OldDurationMin() const {
    return MayUnderlyingBePerformed() ? t_->OldDurationMin() : 0LL;
  }
  virtual int64 OldDurationMax() const {
    return MayUnderlyingBePerformed() ? t_->OldDurationMax() : 0LL;
  }
  virtual void WhenDurationRange(Demon* const d) { t_->WhenDurationRange(d); }
  virtual void WhenDurationBound(Demon* const d) { t_->WhenDurationBound(d); }
  virtual int64 EndMin() const {
    return MayUnderlyingBePerformed() ? t_->EndMin() : kMinValidValue;
  }
  virtual int64 EndMax() const {
    return MayUnderlyingBePerformed() ? t_->EndMax() : kMaxValidValue;
  }
  virtual void SetEndMin(int64 m) { t_->SetEndMin(m); }
  virtual void SetEndMax(int64 m) { t_->SetEndMax(m); }
  virtual void SetEndRange(int64 mi, int64 ma) { t_->SetEndRange(mi, ma); }
  virtual int64 OldEndMin() const {
    return MayUnderlyingBePerformed() ? t_->OldEndMin() : kMinValidValue;
  }
  virtual int64 OldEndMax() const {
    return MayUnderlyingBePerformed() ? t_->OldEndMax() : kMaxValidValue;
  }
  virtual void WhenEndRange(Demon* const d) { t_->WhenEndRange(d); }
  virtual void WhenEndBound(Demon* const d) { t_->WhenEndBound(d); }
  virtual bool MustBePerformed() const { return true; }
  virtual bool MayBePerformed() const { return true; }
  virtual void SetPerformed(bool val) {
    // An AlwaysPerformedIntervalVarWrapper interval variable is always
    // performed. So setting it to be performed does not change anything,
    // and setting it not to be performed is inconsistent and should cause
    // a failure.
    if (!val) {
      solver()->Fail();
    }
  }
  virtual bool WasPerformedBound() const { return true; }
  virtual void WhenPerformedBound(Demon* const d) { t_->WhenPerformedBound(d); }
  virtual IntExpr* StartExpr() {
    if (start_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&start_expr_));
      start_expr_ = BuildStartExpr(this);
    }
    return start_expr_;
  }
  IntExpr* DurationExpr() {
    if (duration_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&duration_expr_));
      duration_expr_ = BuildDurationExpr(this);
    }
    return duration_expr_;
  }
  IntExpr* EndExpr() {
    if (end_expr_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&end_expr_));
      end_expr_ = BuildEndExpr(this);
    }
    return end_expr_;
  }
  IntExpr* PerformedExpr() { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) { return StartExpr(); }
  IntExpr* SafeDurationExpr(int64 unperformed_value) { return DurationExpr(); }
  IntExpr* SafeEndExpr(int64 unperformed_value) { return EndExpr(); }

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
  virtual ~IntervalVarRelaxedMax() {}
  virtual int64 StartMax() const {
    // It matters to use DurationMin() and not underlying()->DurationMin() here.
    return underlying()->MustBePerformed() ? underlying()->StartMax()
                                           : (kMaxValidValue - DurationMin());
  }
  virtual void SetStartMax(int64 m) {
    LOG(FATAL)
        << "Calling SetStartMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  virtual int64 EndMax() const {
    return underlying()->MustBePerformed() ? underlying()->EndMax()
                                           : kMaxValidValue;
  }
  virtual void SetEndMax(int64 m) {
    LOG(FATAL)
        << "Calling SetEndMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMaxOperation, 0,
                                   underlying());
  }

  virtual std::string DebugString() const {
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
  virtual ~IntervalVarRelaxedMin() {}
  virtual int64 StartMin() const {
    return underlying()->MustBePerformed() ? underlying()->StartMin()
                                           : kMinValidValue;
  }
  virtual void SetStartMin(int64 m) {
    LOG(FATAL)
        << "Calling SetStartMin on a IntervalVarRelaxedMin is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  virtual int64 EndMin() const {
    // It matters to use DurationMin() and not underlying()->DurationMin() here.
    return underlying()->MustBePerformed() ? underlying()->EndMin()
                                           : (kMinValidValue + DurationMin());
  }
  virtual void SetEndMin(int64 m) {
    LOG(FATAL)
        << "Calling SetEndMin on a IntervalVarRelaxedMin is not supported, "
        << "as it seems there is no legitimate use case.";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, ModelVisitor::kRelaxedMinOperation, 0,
                                   underlying());
  }

  virtual std::string DebugString() const {
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
    virtual ~Handler() {}
    virtual void Run(Solver* const s) { var_->Process(); }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual std::string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }

   private:
    BaseIntervalVar* const var_;
  };

  class Cleaner : public Action {
   public:
    explicit Cleaner(BaseIntervalVar* const var) : var_(var) {}
    virtual ~Cleaner() {}
    virtual void Run(Solver* const s) { var_->ClearInProcess(); }

   private:
    BaseIntervalVar* const var_;
  };

  BaseIntervalVar(Solver* const s, const std::string& name)
      : IntervalVar(s, name),
        in_process_(false),
        handler_(this),
        cleaner_(this) {}

  virtual ~BaseIntervalVar() {}

  virtual void Process() = 0;

  virtual void Push() = 0;

  void ClearInProcess() { in_process_ = false; }

  virtual std::string BaseName() const { return "IntervalVar"; }

  bool InProcess() const { return in_process_; }

 protected:
  bool in_process_;
  Handler handler_;
  Cleaner cleaner_;
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

  virtual ~RangeVar() {}

  virtual bool Bound() const { return min_.Value() == max_.Value(); }

  virtual int64 Min() const { return min_.Value(); }

  virtual int64 Max() const { return max_.Value(); }

  virtual void SetMin(int64 m) {
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

  virtual void SetMax(int64 m) {
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

  virtual void SetRange(int64 mi, int64 ma) {
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

  virtual void WhenRange(Demon* const demon) {
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

  IntVar* Var() {
    if (cast_var_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&cast_var_));
      cast_var_ = solver()->MakeIntVar(min_.Value(), max_.Value());
      LinkVarExpr(solver(), this, cast_var_);
    }
    return cast_var_;
  }

  std::string DebugString() const {
    std::string out = StringPrintf("%" GG_LL_FORMAT "d", min_.Value());
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

  virtual ~PerformedVar() {}

  virtual void SetValue(int64 v) {
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

  virtual int64 OldMin() const { return previous_value_ == 1; }

  virtual int64 OldMax() const { return previous_value_ != 0; }

  virtual void RestoreValue() {
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

  virtual std::string DebugString() const {
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
                           int64 duration, bool optional,
                           const std::string& name);
  // Unperformed interval.
  FixedDurationIntervalVar(Solver* const s, const std::string& name);
  virtual ~FixedDurationIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual int64 OldStartMin() const { return start_.OldMin(); }
  virtual int64 OldStartMax() const { return start_.OldMax(); }
  virtual void WhenStartRange(Demon* const d) {
    if (performed_.Max() == 1) {
      start_.WhenRange(d);
    }
  }
  virtual void WhenStartBound(Demon* const d) {
    if (performed_.Max() == 1) {
      start_.WhenBound(d);
    }
  }

  virtual int64 DurationMin() const;
  virtual int64 DurationMax() const;
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}

  virtual int64 EndMin() const;
  virtual int64 EndMax() const;
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual int64 OldEndMin() const { return CapAdd(OldStartMin(), duration_); }
  virtual int64 OldEndMax() const { return CapAdd(OldStartMax(), duration_); }
  virtual void WhenEndRange(Demon* const d) { WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { WhenStartBound(d); }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual bool WasPerformedBound() const {
    return performed_.OldMin() == performed_.OldMax();
  }
  virtual void WhenPerformedBound(Demon* const d) { performed_.WhenBound(d); }
  virtual void Process();
  virtual std::string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  virtual IntExpr* StartExpr() { return &start_; }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeSum(StartExpr(), duration_);
  }
  virtual IntExpr* PerformedExpr() { return &performed_; }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return BuildSafeEndExpr(this, unperformed_value);
  }

  virtual void Push();

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
  set_queue_action_on_fail(&cleaner_);
  if (performed_.Max() == 1) {
    start_.ProcessDemons();
  }
  performed_.Process();
  clear_queue_action_on_fail();
  ClearInProcess();
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
  virtual ~FixedDurationPerformedIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual int64 OldStartMin() const { return start_.OldMin(); }
  virtual int64 OldStartMax() const { return start_.OldMax(); }
  virtual void WhenStartRange(Demon* const d) { start_.WhenRange(d); }
  virtual void WhenStartBound(Demon* const d) { start_.WhenBound(d); }

  virtual int64 DurationMin() const;
  virtual int64 DurationMax() const;
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}

  virtual int64 EndMin() const;
  virtual int64 EndMax() const;
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual int64 OldEndMin() const { return CapAdd(OldStartMin(), duration_); }
  virtual int64 OldEndMax() const { return CapAdd(OldStartMax(), duration_); }
  virtual void WhenEndRange(Demon* const d) { WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { WhenEndRange(d); }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual bool WasPerformedBound() const { return true; }
  virtual void WhenPerformedBound(Demon* const d) {}
  virtual void Process();
  virtual std::string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  virtual IntExpr* StartExpr() { return &start_; }
  IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  IntExpr* EndExpr() { return solver()->MakeSum(StartExpr(), duration_); }
  IntExpr* PerformedExpr() { return solver()->MakeIntConst(1); }
  IntExpr* SafeStartExpr(int64 unperformed_value) { return StartExpr(); }
  IntExpr* SafeDurationExpr(int64 unperformed_value) { return DurationExpr(); }
  IntExpr* SafeEndExpr(int64 unperformed_value) { return EndExpr(); }

 private:
  void CheckOldPerformed() {}
  virtual void Push();

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
  set_queue_action_on_fail(&cleaner_);
  start_.ProcessDemons();
  clear_queue_action_on_fail();
  ClearInProcess();
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
  virtual ~StartVarPerformedIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual int64 OldStartMin() const { return start_var_->OldMin(); }
  virtual int64 OldStartMax() const { return start_var_->OldMax(); }
  virtual void WhenStartRange(Demon* const d) { start_var_->WhenRange(d); }
  virtual void WhenStartBound(Demon* const d) { start_var_->WhenBound(d); }

  virtual int64 DurationMin() const;
  virtual int64 DurationMax() const;
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}

  virtual int64 EndMin() const;
  virtual int64 EndMax() const;
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual int64 OldEndMin() const { return CapAdd(OldStartMin(), duration_); }
  virtual int64 OldEndMax() const { return CapAdd(OldStartMax(), duration_); }
  virtual void WhenEndRange(Demon* const d) { start_var_->WhenRange(d); }
  virtual void WhenEndBound(Demon* const d) { start_var_->WhenBound(d); }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual bool WasPerformedBound() const { return true; }
  virtual void WhenPerformedBound(Demon* const d) {}
  virtual std::string DebugString() const;

  virtual IntExpr* StartExpr() { return start_var_; }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeSum(start_var_, duration_);
  }
  virtual IntExpr* PerformedExpr() { return solver()->MakeIntConst(1); }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return StartExpr();
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return DurationExpr();
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) { return EndExpr(); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

 private:
  IntVar* const start_var_;
  int64 duration_;
};

// TODO(user): Take care of overflows.
StartVarPerformedIntervalVar::StartVarPerformedIntervalVar(
    Solver* const s, IntVar* const var, int64 duration, const std::string& name)
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
  SetStartMin(m - duration_);
}

void StartVarPerformedIntervalVar::SetEndMax(int64 m) {
  SetStartMax(m - duration_);
}

void StartVarPerformedIntervalVar::SetEndRange(int64 mi, int64 ma) {
  SetStartRange(mi - duration_, ma - duration_);
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
  virtual ~StartVarIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual int64 OldStartMin() const { return start_->OldMin(); }
  virtual int64 OldStartMax() const { return start_->OldMax(); }
  virtual void WhenStartRange(Demon* const d) {
    if (performed_->Max() == 1) {
      start_->WhenRange(d);
    }
  }
  virtual void WhenStartBound(Demon* const d) {
    if (performed_->Max() == 1) {
      start_->WhenBound(d);
    }
  }

  virtual int64 DurationMin() const;
  virtual int64 DurationMax() const;
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}

  virtual int64 EndMin() const;
  virtual int64 EndMax() const;
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual int64 OldEndMin() const { return CapAdd(OldStartMin(), duration_); }
  virtual int64 OldEndMax() const { return CapAdd(OldStartMax(), duration_); }
  virtual void WhenEndRange(Demon* const d) { WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { WhenStartBound(d); }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual bool WasPerformedBound() const {
    return performed_->OldMin() == performed_->OldMax();
  }
  virtual void WhenPerformedBound(Demon* const d) { performed_->WhenBound(d); }
  virtual std::string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  virtual IntExpr* StartExpr() { return start_; }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeSum(StartExpr(), duration_);
  }
  virtual IntExpr* PerformedExpr() { return performed_; }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return BuildSafeEndExpr(this, unperformed_value);
  }

  virtual void Process() { LOG(FATAL) << "Should not be here"; }

  virtual void Push() { LOG(FATAL) << "Should not be here"; }

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
  CHECK_EQ(performed_->Max(), 1);
  return std::max(start_->Min(), start_min_.Value());
}

int64 StartVarIntervalVar::StartMax() const {
  CHECK_EQ(performed_->Max(), 1);
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
  CHECK_EQ(performed_->Max(), 1);
  return duration_;
}

int64 StartVarIntervalVar::DurationMax() const {
  CHECK_EQ(performed_->Max(), 1);
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
  return CapAdd(StartMin(), duration_);
}

int64 StartVarIntervalVar::EndMax() const {
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

  ~LinkStartVarIntervalVar() {}

  virtual void Post() {
    Demon* const demon = MakeConstraintDemon0(
        solver(), this, &LinkStartVarIntervalVar::PerformedBound,
        "PerformedBound");
    performed_->WhenBound(demon);
  }

  virtual void InitialPropagate() {
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
  virtual ~FixedInterval() {}

  virtual int64 StartMin() const { return start_; }
  virtual int64 StartMax() const { return start_; }
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual int64 OldStartMin() const { return start_; }
  virtual int64 OldStartMax() const { return start_; }
  virtual void WhenStartRange(Demon* const d) {}
  virtual void WhenStartBound(Demon* const d) {}

  virtual int64 DurationMin() const { return duration_; }
  virtual int64 DurationMax() const { return duration_; }
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}

  virtual int64 EndMin() const { return start_ + duration_; }
  virtual int64 EndMax() const { return start_ + duration_; }
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual int64 OldEndMin() const { return start_ + duration_; }
  virtual int64 OldEndMax() const { return start_ + duration_; }
  virtual void WhenEndRange(Demon* const d) {}
  virtual void WhenEndBound(Demon* const d) {}

  virtual bool MustBePerformed() const { return true; }
  virtual bool MayBePerformed() const { return true; }
  virtual void SetPerformed(bool val);
  virtual bool WasPerformedBound() const { return true; }
  virtual void WhenPerformedBound(Demon* const d) {}
  virtual std::string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  virtual IntExpr* StartExpr() { return solver()->MakeIntConst(start_); }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeIntConst(start_ + duration_);
  }
  virtual IntExpr* PerformedExpr() { return solver()->MakeIntConst(1); }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return StartExpr();
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return DurationExpr();
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) { return EndExpr(); }

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
        start_(s, this, std::max(start_min, end_min - duration_max),
               std::min(start_max, end_max - duration_min)),
        duration_(s, this, std::max(duration_min, end_min - start_max),
                  std::min(duration_max, end_max - start_min)),
        end_(s, this, std::max(end_min, start_min + duration_min),
             std::min(end_max, start_max + duration_max)),
        performed_(s, this, optional) {}

  virtual ~VariableDurationIntervalVar() {}

  virtual int64 StartMin() const {
    CHECK_EQ(performed_.Max(), 1);
    return start_.Min();
  }

  virtual int64 StartMax() const {
    CHECK_EQ(performed_.Max(), 1);
    return start_.Max();
  }

  virtual void SetStartMin(int64 m) {
    if (performed_.Max() == 1) {
      start_.SetMin(m);
    }
  }

  virtual void SetStartMax(int64 m) {
    if (performed_.Max() == 1) {
      start_.SetMax(m);
    }
  }

  virtual void SetStartRange(int64 mi, int64 ma) {
    if (performed_.Max() == 1) {
      start_.SetRange(mi, ma);
    }
  }

  virtual int64 OldStartMin() const {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return start_.OldMin();
  }

  virtual int64 OldStartMax() const {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return start_.OldMax();
  }

  virtual void WhenStartRange(Demon* const d) {
    if (performed_.Max() == 1) {
      start_.WhenRange(d);
    }
  }

  virtual void WhenStartBound(Demon* const d) {
    if (performed_.Max() == 1) {
      start_.WhenBound(d);
    }
  }

  virtual int64 DurationMin() const {
    CHECK_EQ(performed_.Max(), 1);
    return duration_.Min();
  }

  virtual int64 DurationMax() const {
    CHECK_EQ(performed_.Max(), 1);
    return duration_.Max();
  }

  virtual void SetDurationMin(int64 m) {
    if (performed_.Max() == 1) {
      duration_.SetMin(m);
    }
  }

  virtual void SetDurationMax(int64 m) {
    if (performed_.Max() == 1) {
      duration_.SetMax(m);
    }
  }

  virtual void SetDurationRange(int64 mi, int64 ma) {
    if (performed_.Max() == 1) {
      duration_.SetRange(mi, ma);
    }
  }

  virtual int64 OldDurationMin() const {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return duration_.OldMin();
  }

  virtual int64 OldDurationMax() const {
    CHECK_EQ(performed_.Max(), 1);
    CHECK(in_process_);
    return duration_.OldMax();
  }

  virtual void WhenDurationRange(Demon* const d) {
    if (performed_.Max() == 1) {
      duration_.WhenRange(d);
    }
  }

  virtual void WhenDurationBound(Demon* const d) {
    if (performed_.Max() == 1) {
      duration_.WhenBound(d);
    }
  }

  virtual int64 EndMin() const {
    CHECK_EQ(performed_.Max(), 1);
    return end_.Min();
  }

  virtual int64 EndMax() const {
    CHECK_EQ(performed_.Max(), 1);
    return end_.Max();
  }

  virtual void SetEndMin(int64 m) {
    if (performed_.Max() == 1) {
      end_.SetMin(m);
    }
  }

  virtual void SetEndMax(int64 m) {
    if (performed_.Max() == 1) {
      end_.SetMax(m);
    }
  }

  virtual void SetEndRange(int64 mi, int64 ma) {
    if (performed_.Max() == 1) {
      end_.SetRange(mi, ma);
    }
  }

  virtual int64 OldEndMin() const {
    CHECK_EQ(performed_.Max(), 1);
    DCHECK(in_process_);
    return end_.OldMin();
  }

  virtual int64 OldEndMax() const {
    CHECK_EQ(performed_.Max(), 1);
    DCHECK(in_process_);
    return end_.OldMax();
  }

  virtual void WhenEndRange(Demon* const d) {
    if (performed_.Max() == 1) {
      end_.WhenRange(d);
    }
  }

  virtual void WhenEndBound(Demon* const d) {
    if (performed_.Max() == 1) {
      end_.WhenBound(d);
    }
  }

  virtual bool MustBePerformed() const { return (performed_.Min() == 1); }

  virtual bool MayBePerformed() const { return (performed_.Max() == 1); }

  virtual void SetPerformed(bool val) { performed_.SetValue(val); }

  virtual bool WasPerformedBound() const {
    CHECK(in_process_);
    return performed_.OldMin() == performed_.OldMax();
  }

  virtual void WhenPerformedBound(Demon* const d) { performed_.WhenBound(d); }

  virtual void Process() {
    CHECK(!in_process_);
    in_process_ = true;
    start_.UpdatePostponedBounds();
    duration_.UpdatePostponedBounds();
    end_.UpdatePostponedBounds();
    performed_.UpdatePostponedValue();
    set_queue_action_on_fail(&cleaner_);
    if (performed_.Max() == 1) {
      start_.ProcessDemons();
      duration_.ProcessDemons();
      end_.ProcessDemons();
    }
    performed_.Process();
    clear_queue_action_on_fail();
    ClearInProcess();
    // TODO(user): Replace this enum by a callback.
    start_.UpdatePreviousBounds();
    start_.ApplyPostponedBounds(START);
    duration_.UpdatePreviousBounds();
    duration_.ApplyPostponedBounds(DURATION);
    end_.UpdatePreviousBounds();
    end_.ApplyPostponedBounds(END);
    performed_.UpdatePreviousValueAndApplyPostponedValue();
  }

  virtual std::string DebugString() const {
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", 0, NullInterval());
  }

  virtual IntExpr* StartExpr() { return &start_; }
  virtual IntExpr* DurationExpr() { return &duration_; }
  virtual IntExpr* EndExpr() { return &end_; }
  virtual IntExpr* PerformedExpr() { return &performed_; }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return BuildSafeStartExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return BuildSafeDurationExpr(this, unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return BuildSafeEndExpr(this, unperformed_value);
  }

 private:
  virtual void Push() {
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
  virtual ~FixedDurationSyncedIntervalVar() {}
  virtual int64 DurationMin() const { return duration_; }
  virtual int64 DurationMax() const { return duration_; }
  virtual void SetDurationMin(int64 m) {
    if (m > duration_) {
      solver()->Fail();
    }
  }
  virtual void SetDurationMax(int64 m) {
    if (m < duration_) {
      solver()->Fail();
    }
  }
  virtual void SetDurationRange(int64 mi, int64 ma) {
    if (mi > duration_ || ma < duration_ || mi > ma) {
      solver()->Fail();
    }
  }
  virtual int64 OldDurationMin() const { return duration_; }
  virtual int64 OldDurationMax() const { return duration_; }
  virtual void WhenDurationRange(Demon* const d) {}
  virtual void WhenDurationBound(Demon* const d) {}
  virtual int64 EndMin() const { return CapAdd(StartMin(), duration_); }
  virtual int64 EndMax() const { return CapAdd(StartMax(), duration_); }
  virtual void SetEndMin(int64 m) { SetStartMin(CapSub(m, duration_)); }
  virtual void SetEndMax(int64 m) { SetStartMax(CapSub(m, duration_)); }
  virtual void SetEndRange(int64 mi, int64 ma) {
    SetStartRange(CapSub(mi, duration_), CapSub(ma, duration_));
  }
  virtual int64 OldEndMin() const { return CapAdd(OldStartMin(), duration_); }
  virtual int64 OldEndMax() const { return CapAdd(OldStartMax(), duration_); }
  virtual void WhenEndRange(Demon* const d) { WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { WhenStartBound(d); }
  virtual bool MustBePerformed() const { return t_->MustBePerformed(); }
  virtual bool MayBePerformed() const { return t_->MayBePerformed(); }
  virtual void SetPerformed(bool val) { t_->SetPerformed(val); }
  virtual bool WasPerformedBound() const { return t_->WasPerformedBound(); }
  virtual void WhenPerformedBound(Demon* const d) { t_->WhenPerformedBound(d); }

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
  virtual ~FixedDurationIntervalVarStartSyncedOnStart() {}
  virtual int64 StartMin() const { return CapAdd(t_->StartMin(), offset_); }
  virtual int64 StartMax() const { return CapAdd(t_->StartMax(), offset_); }
  virtual void SetStartMin(int64 m) { t_->SetStartMin(CapSub(m, offset_)); }
  virtual void SetStartMax(int64 m) { t_->SetStartMax(CapSub(m, offset_)); }
  virtual void SetStartRange(int64 mi, int64 ma) {
    t_->SetStartRange(CapSub(mi, offset_), CapSub(ma, offset_));
  }
  virtual int64 OldStartMin() const {
    return CapAdd(t_->OldStartMin(), offset_);
  }
  virtual int64 OldStartMax() const {
    return CapAdd(t_->OldStartMax(), offset_);
  }
  virtual void WhenStartRange(Demon* const d) { t_->WhenStartRange(d); }
  virtual void WhenStartBound(Demon* const d) { t_->WhenStartBound(d); }
  virtual IntExpr* StartExpr() {
    return solver()->MakeSum(t_->StartExpr(), offset_);
  }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeSum(t_->StartExpr(), offset_ + duration_);
  }
  virtual IntExpr* PerformedExpr() { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return BuildSafeStartExpr(t_, unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return BuildSafeDurationExpr(t_, unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return BuildSafeEndExpr(t_, unperformed_value);
  }
  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(
        this, ModelVisitor::kStartSyncOnStartOperation, offset_, t_);
  }
  virtual std::string DebugString() const {
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
  virtual ~FixedDurationIntervalVarStartSyncedOnEnd() {}
  virtual int64 StartMin() const { return CapAdd(t_->EndMin(), offset_); }
  virtual int64 StartMax() const { return CapAdd(t_->EndMax(), offset_); }
  virtual void SetStartMin(int64 m) { t_->SetEndMin(CapSub(m, offset_)); }
  virtual void SetStartMax(int64 m) { t_->SetEndMax(CapSub(m, offset_)); }
  virtual void SetStartRange(int64 mi, int64 ma) {
    t_->SetEndRange(CapSub(mi, offset_), CapSub(ma, offset_));
  }
  virtual int64 OldStartMin() const { return CapAdd(t_->OldEndMin(), offset_); }
  virtual int64 OldStartMax() const { return CapAdd(t_->OldEndMax(), offset_); }
  virtual void WhenStartRange(Demon* const d) { t_->WhenEndRange(d); }
  virtual void WhenStartBound(Demon* const d) { t_->WhenEndBound(d); }
  virtual IntExpr* StartExpr() {
    return solver()->MakeSum(t_->EndExpr(), offset_);
  }
  virtual IntExpr* DurationExpr() { return solver()->MakeIntConst(duration_); }
  virtual IntExpr* EndExpr() {
    return solver()->MakeSum(t_->EndExpr(), offset_ + duration_);
  }
  virtual IntExpr* PerformedExpr() { return t_->PerformedExpr(); }
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return BuildSafeStartExpr(t_, unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return BuildSafeDurationExpr(t_, unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return BuildSafeEndExpr(t_, unperformed_value);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, ModelVisitor::kStartSyncOnEndOperation,
                                   offset_, t_);
  }
  virtual std::string DebugString() const {
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(
        MakeFixedDurationIntervalVar(start_variables[i], duration, var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int64>& durations, const std::string& name,
    std::vector<IntervalVar*>* array) {
  CHECK(array != nullptr);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
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
    const std::string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(
        MakeIntervalVar(start_min, start_max, duration_min, duration_max,
                        end_min, end_max, optional, var_name));
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
          interval_var, duration, offset - duration)));
}

IntervalVar* Solver::MakeFixedDurationEndSyncedOnEndIntervalVar(
    IntervalVar* const interval_var, int64 duration, int64 offset) {
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVarStartSyncedOnEnd(
          interval_var, duration, offset - duration)));
}
}  // namespace operations_research
