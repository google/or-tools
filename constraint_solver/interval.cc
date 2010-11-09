// Copyright 2010 Google
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

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4351 4355 4804 4805)
#endif

namespace operations_research {

// ----- Interval Var -----

const int64 IntervalVar::kMinValidValue = kint64min >> 2;
const int64 IntervalVar::kMaxValidValue = kint64max >> 2;

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
  virtual void WhenDurationRange(Demon* const d) { t_->WhenDurationRange(d); }
  virtual void WhenDurationBound(Demon* const d) { t_->WhenDurationBound(d); }

  // These methods query, set and watch the end position of the interval var.
  virtual int64 EndMin() const { return -t_->StartMax(); }
  virtual int64 EndMax() const { return -t_->StartMin(); }
  virtual void SetEndMin(int64 m) { t_->SetStartMax(-m); }
  virtual void SetEndMax(int64 m) { t_->SetStartMin(-m); }
  virtual void SetEndRange(int64 mi, int64 ma) { t_->SetStartRange(-ma, -mi); }
  virtual void WhenEndRange(Demon* const d) { t_->WhenStartRange(d); }
  virtual void WhenEndBound(Demon* const d) { t_->WhenStartBound(d); }

  // These methods query, set and watches the performed status of the
  // interval var.
  virtual bool MustBePerformed() const { return t_->MustBePerformed(); }
  virtual bool MayBePerformed() const { return t_->MayBePerformed(); }
  virtual void SetPerformed(bool val) { t_->SetPerformed(val); }
  virtual void WhenPerformedBound(Demon* const d) { t_->WhenPerformedBound(d); }

 private:
  IntervalVar* const t_;
  DISALLOW_COPY_AND_ASSIGN(MirrorIntervalVar);
};

IntervalVar* Solver::MakeMirrorInterval(IntervalVar* const t) {
  return RevAlloc(new MirrorIntervalVar(this, t));
}

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
        t_(t) {}
  virtual ~AlwaysPerformedIntervalVarWrapper() {}
  virtual int64 StartMin() const {
    return MayUnderlyingBePerformed()? t_->StartMin() : kMinValidValue;
  }
  virtual int64 StartMax() const {
    return MayUnderlyingBePerformed() ? t_->StartMax() : kMaxValidValue;
  }
  virtual void SetStartMin(int64 m) { t_->SetStartMin(m); }
  virtual void SetStartMax(int64 m) { t_->SetStartMax(m); }
  virtual void SetStartRange(int64 mi, int64 ma) { t_->SetStartRange(mi, ma); }
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
  virtual void WhenPerformedBound(Demon* const d) { t_->WhenPerformedBound(d); }
 protected:
  const IntervalVar* const underlying() const { return t_; }
  bool MayUnderlyingBePerformed() const {
    return underlying()->MayBePerformed();
  }
 private:
  IntervalVar* const t_;
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
    return underlying()->MustBePerformed() ?
        underlying()->StartMax() : (kMaxValidValue - DurationMin());
  }
  virtual void SetStartMax(int64 m) {
    LOG(FATAL)
        << "Calling SetStartMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  virtual int64 EndMax() const {
    return underlying()->MustBePerformed() ?
        underlying()->EndMax() : kMaxValidValue;
  }
  virtual void SetEndMax(int64 m) {
    LOG(FATAL)
        << "Calling SetEndMax on a IntervalVarRelaxedMax is not supported, "
        << "as it seems there is no legitimate use case.";
  }
};

IntervalVar* Solver::MakeIntervalRelaxedMax(IntervalVar* const interval_var) {
  return RevAlloc(new IntervalVarRelaxedMax(interval_var));
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
class IntervalVarRelaxedMin : public AlwaysPerformedIntervalVarWrapper {
 public:
  explicit IntervalVarRelaxedMin(IntervalVar* const t)
      : AlwaysPerformedIntervalVarWrapper(t) {}
  virtual ~IntervalVarRelaxedMin() {}
  virtual int64 StartMin() const {
    return underlying()->MustBePerformed() ?
        underlying()->StartMin() : kMinValidValue;
  }
  virtual void SetStartMin(int64 m) {
    LOG(FATAL)
        << "Calling SetStartMin on a IntervalVarRelaxedMin is not supported, "
        << "as it seems there is no legitimate use case.";
  }
  virtual int64 EndMin() const {
    // It matters to use DurationMin() and not underlying()->DurationMin() here.
    return underlying()->MustBePerformed() ?
        underlying()->EndMin() : (kMinValidValue + DurationMin());
  }
  virtual void SetEndMin(int64 m) {
    LOG(FATAL)
      << "Calling SetEndMin on a IntervalVarRelaxedMin is not supported, "
      << "as it seems there is no legitimate use case.";
  }
};

IntervalVar* Solver::MakeIntervalRelaxedMin(IntervalVar* const interval_var) {
  return RevAlloc(new IntervalVarRelaxedMin(interval_var));
}

class IntervalVarStartExpr : public BaseIntExpr {
 public:
  explicit IntervalVarStartExpr(IntervalVar* const i)
      : BaseIntExpr(i->solver()), interval_(i) {}
  virtual ~IntervalVarStartExpr() {}

  virtual int64 Min() const {
    return interval_->StartMin();
  }

  virtual void SetMin(int64 m) {
    interval_->SetStartMin(m);
  }

  virtual int64 Max() const {
    return interval_->StartMax();
  }

  virtual void SetMax(int64 m) {
    interval_->SetStartMax(m);
  }

  virtual void SetRange(int64 l, int64 u) {
    interval_->SetStartRange(l, u);
  }

  virtual void SetValue(int64 v) {
    interval_->SetStartRange(v, v);
  }

  virtual bool Bound() const {
    return interval_->StartMin() == interval_->StartMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenStartRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("start(%s)", interval_->DebugString().c_str());
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

  virtual int64 Min() const {
    return interval_->EndMin();
  }

  virtual void SetMin(int64 m) {
    interval_->SetEndMin(m);
  }

  virtual int64 Max() const {
    return interval_->EndMax();
  }

  virtual void SetMax(int64 m) {
    interval_->SetEndMax(m);
  }

  virtual void SetRange(int64 l, int64 u) {
    interval_->SetEndRange(l, u);
  }

  virtual void SetValue(int64 v) {
    interval_->SetEndRange(v, v);
  }

  virtual bool Bound() const {
    return interval_->EndMin() == interval_->EndMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenEndRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("end(%s)", interval_->DebugString().c_str());
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

  virtual int64 Min() const {
    return interval_->DurationMin();
  }

  virtual void SetMin(int64 m) {
    interval_->SetDurationMin(m);
  }

  virtual int64 Max() const {
    return interval_->DurationMax();
  }

  virtual void SetMax(int64 m) {
    interval_->SetDurationMax(m);
  }

  virtual void SetRange(int64 l, int64 u) {
    interval_->SetDurationRange(l, u);
  }

  virtual void SetValue(int64 v) {
    interval_->SetDurationRange(v, v);
  }

  virtual bool Bound() const {
    return interval_->DurationMin() == interval_->DurationMax();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenDurationRange(d);
  }

  virtual string DebugString() const {
    return StringPrintf("duration(%s)", interval_->DebugString().c_str());
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

  virtual void SetValue(int64 v) {
    SetRange(v, v);
  }

  virtual bool Bound() const {
    return interval_->IsPerformedBound();
  }

  virtual void WhenRange(Demon* d) {
    interval_->WhenPerformedBound(d);
  }

  virtual string DebugString() const {
    return StringPrintf("performed(%s)", interval_->DebugString().c_str());
  }
 private:
  IntervalVar* interval_;
  DISALLOW_COPY_AND_ASSIGN(IntervalVarPerformedExpr);
};

IntExpr* IntervalVar::StartExpr() {
  if (start_expr_ == NULL) {
    solver()->SaveValue(reinterpret_cast<void**>(&start_expr_));
    start_expr_ = solver()->RevAlloc(new IntervalVarStartExpr(this));
  }
  return start_expr_;
}

IntExpr* IntervalVar::DurationExpr() {
  if (duration_expr_ == NULL) {
    solver()->SaveValue(reinterpret_cast<void**>(&duration_expr_));
    duration_expr_ = solver()->RevAlloc(new IntervalVarDurationExpr(this));
  }
  return duration_expr_;
}

IntExpr* IntervalVar::EndExpr() {
  if (end_expr_ == NULL) {
    solver()->SaveValue(reinterpret_cast<void**>(&end_expr_));
    end_expr_ = solver()->RevAlloc(new IntervalVarEndExpr(this));
  }
  return end_expr_;
}

IntExpr* IntervalVar::PerformedExpr() {
  if (performed_expr_ == NULL) {
    solver()->SaveValue(reinterpret_cast<void**>(&performed_expr_));
    performed_expr_ = solver()->RevAlloc(new IntervalVarPerformedExpr(this));
  }
  return performed_expr_;
}

// ----- FixedDurationIntervalVar -----

class FixedDurationIntervalVar : public IntervalVar {
 public:

  class FixedDurationIntervalVarHandler : public Demon {
   public:
    explicit FixedDurationIntervalVarHandler(
        FixedDurationIntervalVar* const var) : Demon(), var_(var) {}
    virtual ~FixedDurationIntervalVarHandler() {}
    virtual void Run(Solver* const s) {
      var_->Process();
    }
    virtual Solver::DemonPriority priority() const {
      return Solver::VAR_PRIORITY;
    }
    virtual string DebugString() const {
      return StringPrintf("Handler(%s)", var_->DebugString().c_str());
    }
   private:
    FixedDurationIntervalVar* const var_;
  };

  class FixedDurationIntervalVarCleaner : public Action {
   public:
    explicit FixedDurationIntervalVarCleaner(
        FixedDurationIntervalVar* const var) : Action(), var_(var) {}
    virtual ~FixedDurationIntervalVarCleaner() {}
    virtual void Run(Solver* const s) {
      var_->ClearInProcess();
    }
   private:
    FixedDurationIntervalVar* const var_;
  };

  FixedDurationIntervalVar(Solver* const s,
                           int64 start_min,
                           int64 start_max,
                           int64 duration,
                           bool optional,
                           const string& name);
  // Unperformed interval.
  FixedDurationIntervalVar(Solver* const s, const string& name);
  virtual ~FixedDurationIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual void WhenStartRange(Demon* const d) {
    start_range_demons_.PushIfNotTop(solver(), d);
  }
  virtual void WhenStartBound(Demon* const d) {
    start_bound_demons_.PushIfNotTop(solver(), d);
  }

  virtual int64 DurationMin() const;
  virtual int64 DurationMax() const;
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual void WhenDurationRange(Demon* const d) { }
  virtual void WhenDurationBound(Demon* const d) { }

  virtual int64 EndMin() const;
  virtual int64 EndMax() const;
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual void WhenEndRange(Demon* const d)  {
    start_range_demons_.PushIfNotTop(solver(), d);
  }
  virtual void WhenEndBound(Demon* const d) {
    start_bound_demons_.PushIfNotTop(solver(), d);
  }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual void WhenPerformedBound(Demon* const d) {
    performed_bound_demons_.PushIfNotTop(solver(), d);
  }
  void Process();
  void ClearInProcess();
  virtual string DebugString() const;
 private:
  void CheckOldStartBounds() {
    if (old_start_min_ > start_min_) {
      old_start_min_ = start_min_;
    }
    if (old_start_max_ < start_max_) {
      old_start_max_ = start_max_;
    }
  }
  void CheckOldPerformed() {
    if (performed_ == 2) {
      old_performed_ = 2;
    }
  }
  void CheckNotUnperformed() const {
    CHECK_NE(0, performed_);
  }
  void Push();

  int64 start_min_;
  int64 start_max_;
  int64 new_start_min_;
  int64 new_start_max_;
  int64 old_start_min_;
  int64 old_start_max_;
  int64 duration_;
  int performed_;
  int new_performed_;
  int old_performed_;
  SimpleRevFIFO<Demon*> start_bound_demons_;
  SimpleRevFIFO<Demon*> start_range_demons_;
  SimpleRevFIFO<Demon*> performed_bound_demons_;
  FixedDurationIntervalVarHandler handler_;
  FixedDurationIntervalVarCleaner cleaner_;
  bool in_process_;
};

FixedDurationIntervalVar::FixedDurationIntervalVar(Solver* const s,
                                                   int64 start_min,
                                                   int64 start_max,
                                                   int64 duration,
                                                   bool optional,
                                                   const string& name)
    : IntervalVar(s, name),
      start_min_(start_min),
      start_max_(start_max),
      new_start_min_(start_min),
      new_start_max_(start_max),
      old_start_min_(start_min),
      old_start_max_(start_max),
      duration_(duration),
      performed_(optional ? 2 : 1),
      new_performed_(performed_),
      old_performed_(performed_),
      handler_(this),
      cleaner_(this),
      in_process_(false) {
}

FixedDurationIntervalVar::FixedDurationIntervalVar(Solver* const s,
                                                   const string& name)
    : IntervalVar(s, name),
      start_min_(0),
      start_max_(0),
      new_start_min_(0),
      new_start_max_(0),
      old_start_min_(0),
      old_start_max_(0),
      duration_(0),
      performed_(0),
      new_performed_(0),
      old_performed_(0),
      handler_(this),
      cleaner_(this),
      in_process_(false) {
}

void FixedDurationIntervalVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  new_start_min_ = start_min_;
  new_start_max_ = start_max_;
  new_performed_ = performed_;
  set_queue_action_on_fail(&cleaner_);
  if (performed_ != 0) {
    if (start_min_ == start_max_) {
      for (SimpleRevFIFO<Demon*>::Iterator it(&start_bound_demons_);
           it.ok();
           ++it) {
        Enqueue(*it);
      }
    }
    if (start_min_ != old_start_min_ || start_max_ != old_start_max_) {
      for (SimpleRevFIFO<Demon*>::Iterator it(&start_range_demons_);
           it.ok();
           ++it) {
        Enqueue(*it);
      }
    }
  }
  if (old_performed_ != performed_) {
    for (SimpleRevFIFO<Demon*>::Iterator it(&performed_bound_demons_);
         it.ok();
         ++it) {
      Enqueue(*it);
    }
  }
  ProcessDemonsOnQueue();
  clear_queue_action_on_fail();
  ClearInProcess();
  old_start_min_ = start_min_;
  old_start_max_ = start_max_;
  old_performed_ = performed_;
  if (start_min_ < new_start_min_) {
    SetStartMin(new_start_min_);
  }
  if (start_max_ > new_start_max_) {
    SetStartMax(new_start_max_);
  }
  if (new_performed_ != performed_) {
    CHECK_NE(2, new_performed_);
    SetPerformed(new_performed_);
  }
}

void FixedDurationIntervalVar::ClearInProcess() {
  in_process_ = false;
}

int64 FixedDurationIntervalVar::StartMin() const {
  CheckNotUnperformed();
  return start_min_;
}

int64 FixedDurationIntervalVar::StartMax() const {
  CheckNotUnperformed();
  return start_max_;
}

void FixedDurationIntervalVar::SetStartMin(int64 m) {
  if (performed_ != 0) {
    if (m > start_max_) {
      SetPerformed(false);
      return;
    }
    if (m > start_min_) {
      if (in_process_) {
        if (m > new_start_max_) {
          solver()->Fail();
        }
        if (m > new_start_min_) {
          new_start_min_ = m;
        }
      } else {
        CheckOldStartBounds();
        solver()->SaveAndSetValue(&start_min_, m);
        Push();
      }
    }
  }
}

void FixedDurationIntervalVar::SetStartMax(int64 m) {
  if (performed_ != 0) {
    if (m < start_min_) {
      SetPerformed(false);
      return;
    }
    if (m < start_max_) {
      if (in_process_) {
         if (m < new_start_min_) {
          solver()->Fail();
        }
        if (m < new_start_max_) {
          new_start_max_ = m;
        }
      } else {
        CheckOldStartBounds();
        solver()->SaveAndSetValue(&start_max_, m);
        Push();
      }
    }
  }
}

void FixedDurationIntervalVar::SetStartRange(int64 mi, int64 ma) {
  SetStartMin(mi);
  SetStartMax(ma);
}

int64 FixedDurationIntervalVar::DurationMin() const {
  CheckNotUnperformed();
  return duration_;
}

int64 FixedDurationIntervalVar::DurationMax() const {
  CheckNotUnperformed();
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
int64 FixedDurationIntervalVar::EndMin() const {
  CheckNotUnperformed();
  return start_min_ + duration_;
}

int64 FixedDurationIntervalVar::EndMax() const {
  CheckNotUnperformed();
  return start_max_ + duration_;
}

void FixedDurationIntervalVar::SetEndMin(int64 m) {
  if (m > start_min_ + duration_) {
    SetStartMin(m - duration_);
  }
}

void FixedDurationIntervalVar::SetEndMax(int64 m) {
  if (m < start_max_ + duration_) {
    SetStartMax(m - duration_);
  }
}

void FixedDurationIntervalVar::SetEndRange(int64 mi, int64 ma) {
  if (mi < start_min_ + duration_) {
    mi = start_min_ + duration_;
  }
  if (ma > start_max_ + duration_) {
    ma = start_max_ + duration_;
  }
  SetStartRange(mi - duration_, ma - duration_);
}


void FixedDurationIntervalVar::SetDurationRange(int64 mi, int64 ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

bool FixedDurationIntervalVar::MustBePerformed() const {
  return (performed_ == 1);
}

bool FixedDurationIntervalVar::MayBePerformed() const {
  return (performed_ != 0);
}

void FixedDurationIntervalVar::SetPerformed(bool val) {
  CHECK_GE(val, 0);
  CHECK_LE(val, 1);
  if (performed_ == 2) {
    if (in_process_) {
      if (new_performed_ == 2) {
        new_performed_ = val;
      } else if (new_performed_ != val) {
        solver()->Fail();
      }
    } else {
      CheckOldPerformed();
      solver()->SaveAndSetValue(&performed_, static_cast<int>(val));
      Push();
    }
  } else if (performed_ != val) {
    solver()->Fail();
  }
}

void FixedDurationIntervalVar::Push() {
  const bool in_process = in_process_;
  Enqueue(&handler_);
  CHECK_EQ(in_process, in_process_);
}

string FixedDurationIntervalVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  StringAppendF(&out, "%" GG_LL_FORMAT "d", start_min_);
  if (start_max_ != start_min_) {
    StringAppendF(&out, " .. %" GG_LL_FORMAT "d", start_max_);
  }
  StringAppendF(&out, ", duration = %" GG_LL_FORMAT "d, status = ", duration_);
  switch (performed_) {
    case 0:
      out += "unperformed)";
      break;
    case 1:
      out += "performed)";
      break;
    case 2:
      out += "optional)";
      break;
  }
  return out;
}

// ----- FixedInterval -----

class FixedInterval : public IntervalVar {
 public:
  FixedInterval(Solver* const s, int64 start, int64 duration,
                const string& name);
  virtual ~FixedInterval() {}

  virtual int64 StartMin() const { return start_; }
  virtual int64 StartMax() const { return start_; }
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual void WhenStartRange(Demon* const d) {}
  virtual void WhenStartBound(Demon* const d) {}

  virtual int64 DurationMin() const { return duration_; }
  virtual int64 DurationMax() const { return duration_; }
  virtual void SetDurationMin(int64 m);
  virtual void SetDurationMax(int64 m);
  virtual void SetDurationRange(int64 mi, int64 ma);
  virtual void WhenDurationRange(Demon* const d) { }
  virtual void WhenDurationBound(Demon* const d) { }

  virtual int64 EndMin() const { return start_ + duration_; }
  virtual int64 EndMax() const { return start_ + duration_; }
  virtual void SetEndMin(int64 m);
  virtual void SetEndMax(int64 m);
  virtual void SetEndRange(int64 mi, int64 ma);
  virtual void WhenEndRange(Demon* const d)  {}
  virtual void WhenEndBound(Demon* const d) {}

  virtual bool MustBePerformed() const { return true; }
  virtual bool MayBePerformed() const { return true; }
  virtual void SetPerformed(bool val);
  virtual void WhenPerformedBound(Demon* const d) {}
  virtual string DebugString() const;
 private:
  const int64 start_;
  const int64 duration_;
};

FixedInterval::FixedInterval(Solver* const s,
                             int64 start,
                             int64 duration,
                             const string& name)
    : IntervalVar(s, name),
      start_(start),
      duration_(duration) {
}

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

string FixedInterval::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  StringAppendF(&out, "%" GG_LL_FORMAT "d, duration = %" GG_LL_FORMAT
                "d, status = performed)",
                start_, duration_);
  return out;
}

IntervalVar* Solver::MakeFixedInterval(int64 start,
                                       int64 duration,
                                       const string& name) {
  return RevAlloc(new FixedInterval(this, start, duration, name));
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(int64 start_min,
                                                  int64 start_max,
                                                  int64 duration,
                                                  bool optional,
                                                  const string& name) {
  if (start_min == start_max && !optional) {
    return MakeFixedInterval(start_min, duration, name);
  }
  return RevAlloc(new FixedDurationIntervalVar(this, start_min, start_max,
                                               duration, optional, name));
}



}  // namespace operations_research
