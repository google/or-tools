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

// ----- Interval Var -----

// It's good to have the two extreme values being symmetrical around zero: it
// makes mirroring easier.
const int64 IntervalVar::kMaxValidValue = kint64max >> 2;
const int64 IntervalVar::kMinValidValue = -kMaxValidValue;

namespace {
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, ModelVisitor::kMirrorOperation, t_);
  }

  virtual string DebugString() const {
    return StringPrintf("MirrorInterval(%s)", t_->DebugString().c_str());
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this,
                                   ModelVisitor::kRelaxedMaxOperation,
                                   underlying());
  }

  virtual string DebugString() const {
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this,
                                   ModelVisitor::kRelaxedMinOperation,
                                   underlying());
  }

  virtual string DebugString() const {
    return StringPrintf("IntervalVarRelaxedMin(%s)",
                        underlying()->DebugString().c_str());
  }
};

// ----- FixedDurationIntervalVar -----

class FixedDurationIntervalVar : public IntervalVar {
 public:
  enum PerformedStatus { UNPERFORMED, PERFORMED, UNDECIDED };

  class Handler : public Demon {
   public:
    explicit Handler(FixedDurationIntervalVar* const var) : var_(var) {}
    virtual ~Handler() {}
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

  class Cleaner : public Action {
   public:
    explicit Cleaner(FixedDurationIntervalVar* const var) : var_(var) {}
    virtual ~Cleaner() {}
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
    if (performed_ != UNPERFORMED && start_min_ != start_max_) {
      start_range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenStartBound(Demon* const d) {
    if (performed_ != UNPERFORMED && start_min_ != start_max_) {
      start_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
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
    if (performed_ != UNPERFORMED && start_min_ != start_max_) {
      start_range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenEndBound(Demon* const d) {
    if (performed_ != UNPERFORMED && start_min_ != start_max_) {
      start_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual void WhenPerformedBound(Demon* const d) {
    if (performed_ == UNDECIDED) {
      performed_bound_demons_.PushIfNotTop(solver(),
                                           solver()->RegisterDemon(d));
    }
  }
  void Process();
  void ClearInProcess();
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", NULL);
  }

  virtual string BaseName() const { return "IntervalVar"; }

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
    if (performed_ == UNDECIDED) {
      old_performed_ = UNDECIDED;
    }
  }
  void CheckNotUnperformed() const {
    CHECK_NE(UNPERFORMED, performed_);
  }
  void Push();

  int64 start_min_;
  int64 start_max_;
  int64 new_start_min_;
  int64 new_start_max_;
  int64 old_start_min_;
  int64 old_start_max_;
  int64 duration_;
  PerformedStatus performed_;
  PerformedStatus new_performed_;
  PerformedStatus old_performed_;
  SimpleRevFIFO<Demon*> start_bound_demons_;
  SimpleRevFIFO<Demon*> start_range_demons_;
  SimpleRevFIFO<Demon*> performed_bound_demons_;
  Handler handler_;
  Cleaner cleaner_;
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
      performed_(optional ? UNDECIDED : PERFORMED),
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
      performed_(UNPERFORMED),
      new_performed_(UNPERFORMED),
      old_performed_(UNPERFORMED),
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
  if (performed_ != UNPERFORMED) {
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
    CHECK_NE(UNDECIDED, new_performed_);
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
  if (performed_ != UNPERFORMED) {
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
  if (performed_ != UNPERFORMED) {
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
  return (performed_ == PERFORMED);
}

bool FixedDurationIntervalVar::MayBePerformed() const {
  return (performed_ != UNPERFORMED);
}

void FixedDurationIntervalVar::SetPerformed(bool val) {
  CHECK_GE(val, 0);
  CHECK_LE(val, 1);
  if (performed_ == UNDECIDED) {
    if (in_process_) {
      if (new_performed_ == UNDECIDED) {
        new_performed_ = static_cast<PerformedStatus>(val);
      } else if (new_performed_ != val) {
        solver()->Fail();
      }
    } else {
      CheckOldPerformed();
      solver()->SaveAndSetValue(reinterpret_cast<int*>(&performed_),
                                static_cast<int>(val));
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
    case UNPERFORMED:
      out += "unperformed)";
      break;
    case PERFORMED:
      out += "performed)";
      break;
    case UNDECIDED:
      out += "optional)";
      break;
  }
  return out;
}

// ----- FixedDurationPerformedIntervalVar -----

class FixedDurationPerformedIntervalVar : public IntervalVar {
 public:
  class Handler : public Demon {
   public:
    explicit Handler(FixedDurationPerformedIntervalVar* const var)
        : var_(var) {}
    virtual ~Handler() {}
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
    FixedDurationPerformedIntervalVar* const var_;
  };

  class Cleaner : public Action {
   public:
    explicit Cleaner(FixedDurationPerformedIntervalVar* const var)
        : var_(var) {}
    virtual ~Cleaner() {}
    virtual void Run(Solver* const s) {
      var_->ClearInProcess();
    }
   private:
    FixedDurationPerformedIntervalVar* const var_;
  };

  FixedDurationPerformedIntervalVar(Solver* const s,
                                    int64 start_min,
                                    int64 start_max,
                                    int64 duration,
                                    const string& name);
  // Unperformed interval.
  FixedDurationPerformedIntervalVar(Solver* const s, const string& name);
  virtual ~FixedDurationPerformedIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual void WhenStartRange(Demon* const d) {
    if (start_min_ != start_max_) {
      start_range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenStartBound(Demon* const d) {
    if (start_min_ != start_max_) {
      start_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
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
    if (start_min_ != start_max_) {
      start_range_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }
  virtual void WhenEndBound(Demon* const d) {
    if (start_min_ != start_max_) {
      start_bound_demons_.PushIfNotTop(solver(), solver()->RegisterDemon(d));
    }
  }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual void WhenPerformedBound(Demon* const d) {
  }
  void Process();
  void ClearInProcess();
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", NULL);
  }

 private:
  void CheckOldStartBounds() {
    if (old_start_min_ > start_min_) {
      old_start_min_ = start_min_;
    }
    if (old_start_max_ < start_max_) {
      old_start_max_ = start_max_;
    }
  }
  void CheckOldPerformed() {}
  void Push();

  int64 start_min_;
  int64 start_max_;
  int64 new_start_min_;
  int64 new_start_max_;
  int64 old_start_min_;
  int64 old_start_max_;
  int64 duration_;
  SimpleRevFIFO<Demon*> start_bound_demons_;
  SimpleRevFIFO<Demon*> start_range_demons_;
  Handler handler_;
  Cleaner cleaner_;
  bool in_process_;
};

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* const s,
    int64 start_min,
    int64 start_max,
    int64 duration,
    const string& name)
    : IntervalVar(s, name),
      start_min_(start_min),
      start_max_(start_max),
      new_start_min_(start_min),
      new_start_max_(start_max),
      old_start_min_(start_min),
      old_start_max_(start_max),
      duration_(duration),
      handler_(this),
      cleaner_(this),
      in_process_(false) {
}

FixedDurationPerformedIntervalVar::FixedDurationPerformedIntervalVar(
    Solver* const s,
    const string& name)
    : IntervalVar(s, name),
      start_min_(0),
      start_max_(0),
      new_start_min_(0),
      new_start_max_(0),
      old_start_min_(0),
      old_start_max_(0),
      duration_(0),
      handler_(this),
      cleaner_(this),
      in_process_(false) {
}

void FixedDurationPerformedIntervalVar::Process() {
  CHECK(!in_process_);
  in_process_ = true;
  new_start_min_ = start_min_;
  new_start_max_ = start_max_;
  set_queue_action_on_fail(&cleaner_);
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
  ProcessDemonsOnQueue();
  clear_queue_action_on_fail();
  ClearInProcess();
  old_start_min_ = start_min_;
  old_start_max_ = start_max_;
  if (start_min_ < new_start_min_) {
    SetStartMin(new_start_min_);
  }
  if (start_max_ > new_start_max_) {
    SetStartMax(new_start_max_);
  }
}

void FixedDurationPerformedIntervalVar::ClearInProcess() {
  in_process_ = false;
}

int64 FixedDurationPerformedIntervalVar::StartMin() const {
  return start_min_;
}

int64 FixedDurationPerformedIntervalVar::StartMax() const {
  return start_max_;
}

void FixedDurationPerformedIntervalVar::SetStartMin(int64 m) {
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

void FixedDurationPerformedIntervalVar::SetStartMax(int64 m) {
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

void FixedDurationPerformedIntervalVar::SetStartRange(int64 mi, int64 ma) {
  SetStartMin(mi);
  SetStartMax(ma);
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
  return start_min_ + duration_;
}

int64 FixedDurationPerformedIntervalVar::EndMax() const {
  return start_max_ + duration_;
}

void FixedDurationPerformedIntervalVar::SetEndMin(int64 m) {
  if (m > start_min_ + duration_) {
    SetStartMin(m - duration_);
  }
}

void FixedDurationPerformedIntervalVar::SetEndMax(int64 m) {
  if (m < start_max_ + duration_) {
    SetStartMax(m - duration_);
  }
}

void FixedDurationPerformedIntervalVar::SetEndRange(int64 mi, int64 ma) {
  if (mi < start_min_ + duration_) {
    mi = start_min_ + duration_;
  }
  if (ma > start_max_ + duration_) {
    ma = start_max_ + duration_;
  }
  SetStartRange(mi - duration_, ma - duration_);
}


void FixedDurationPerformedIntervalVar::SetDurationRange(int64 mi, int64 ma) {
  if (mi > duration_ || ma < duration_ || mi > ma) {
    SetPerformed(false);
  }
}

bool FixedDurationPerformedIntervalVar::MustBePerformed() const {
  return true;
}

bool FixedDurationPerformedIntervalVar::MayBePerformed() const {
  return true;
}

void FixedDurationPerformedIntervalVar::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

void FixedDurationPerformedIntervalVar::Push() {
  const bool in_process = in_process_;
  Enqueue(&handler_);
  CHECK_EQ(in_process, in_process_);
}

string FixedDurationPerformedIntervalVar::DebugString() const {
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
  out += "performed)";
  return out;
}

// ----- StartVarPerformedIntervalVar -----

class StartVarPerformedIntervalVar : public IntervalVar {
 public:
  StartVarPerformedIntervalVar(Solver* const s,
                               IntVar* const start_var,
                               int64 duration,
                               const string& name);
  virtual ~StartVarPerformedIntervalVar() {}

  virtual int64 StartMin() const;
  virtual int64 StartMax() const;
  virtual void SetStartMin(int64 m);
  virtual void SetStartMax(int64 m);
  virtual void SetStartRange(int64 mi, int64 ma);
  virtual void WhenStartRange(Demon* const d) {
    start_var_->WhenRange(d);
  }
  virtual void WhenStartBound(Demon* const d) {
    start_var_->WhenBound(d);
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
    start_var_->WhenRange(d);
  }
  virtual void WhenEndBound(Demon* const d) {
    start_var_->WhenBound(d);
  }

  virtual bool MustBePerformed() const;
  virtual bool MayBePerformed() const;
  virtual void SetPerformed(bool val);
  virtual void WhenPerformedBound(Demon* const d) {}
  virtual string DebugString() const;

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", NULL);
  }

 private:
  IntVar* const start_var_;
  int64 duration_;
};

StartVarPerformedIntervalVar::StartVarPerformedIntervalVar(
    Solver* const s,
    IntVar* const var,
    int64 duration,
    const string& name)
    : IntervalVar(s, name),
      start_var_(var),
      duration_(duration) {
}

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

int64 StartVarPerformedIntervalVar::DurationMin() const {
  return duration_;
}

int64 StartVarPerformedIntervalVar::DurationMax() const {
  return duration_;
}

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

bool StartVarPerformedIntervalVar::MustBePerformed() const {
  return true;
}

bool StartVarPerformedIntervalVar::MayBePerformed() const {
  return true;
}

void StartVarPerformedIntervalVar::SetPerformed(bool val) {
  if (!val) {
    solver()->Fail();
  }
}

string StartVarPerformedIntervalVar::DebugString() const {
  string out;
  const string& var_name = name();
  if (!var_name.empty()) {
    out = var_name + "(start = ";
  } else {
    out = "IntervalVar(start = ";
  }
  StringAppendF(&out, "%" GG_LL_FORMAT "d", start_var_->Min());
  if (!start_var_->Bound()) {
    StringAppendF(&out, " .. %" GG_LL_FORMAT "d", start_var_->Max());
  }

  StringAppendF(&out, ", duration = %" GG_LL_FORMAT "d, status = ", duration_);
  out += "performed)";
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

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->VisitIntervalVariable(this, "", NULL);
  }

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
  } else if (!optional) {
    return RegisterIntervalVar(
        RevAlloc(new FixedDurationPerformedIntervalVar(this,
                                                       start_min,
                                                       start_max,
                                                       duration,
                                                       name)));
  }
  return RegisterIntervalVar(
      RevAlloc(new FixedDurationIntervalVar(this, start_min, start_max,
                                            duration, optional, name)));
}

void Solver::MakeFixedDurationIntervalVarArray(int count,
                                               int64 start_min,
                                               int64 start_max,
                                               int64 duration,
                                               bool optional,
                                               const string& name,
                                               std::vector<IntervalVar*>* array) {
  CHECK_GT(count, 0);
  CHECK_NOTNULL(array);
  array->clear();
  for (int i = 0; i < count; ++i) {
    const string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(MakeFixedDurationIntervalVar(start_min,
                                                  start_max,
                                                  duration,
                                                  optional,
                                                  var_name));
  }
}

IntervalVar* Solver::MakeFixedDurationIntervalVar(IntVar* const start_variable,
                                                  int64 duration,
                                                  const string& name) {
  CHECK_NOTNULL(start_variable);
  CHECK_GE(duration, 0);
  return RegisterIntervalVar(
      new StartVarPerformedIntervalVar(this, start_variable, duration, name));
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    int64 duration,
    const string& name,
    std::vector<IntervalVar*>* array) {
  CHECK_NOTNULL(array);
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  duration,
                                                  var_name));
  }
}

  // This method fills the vector with interval variables built with
  // the corresponding start variables.
void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int64>& durations,
    const string& name,
    std::vector<IntervalVar*>* array) {
  CHECK_NOTNULL(array);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i],
                                                  var_name));
  }
}

void Solver::MakeFixedDurationIntervalVarArray(
    const std::vector<IntVar*>& start_variables,
    const std::vector<int>& durations,
    const string& name,
    std::vector<IntervalVar*>* array) {
  CHECK_NOTNULL(array);
  CHECK_EQ(start_variables.size(), durations.size());
  array->clear();
  for (int i = 0; i < start_variables.size(); ++i) {
    const string var_name = StringPrintf("%s%i", name.c_str(), i);
    array->push_back(MakeFixedDurationIntervalVar(start_variables[i],
                                                  durations[i],
                                                  var_name));
  }
}
}  // namespace operations_research
