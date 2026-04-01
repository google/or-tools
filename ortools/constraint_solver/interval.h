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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_INTERVAL_H_
#define ORTOOLS_CONSTRAINT_SOLVER_INTERVAL_H_

#include <cstdint>
#include <string>
#include <utility>

#include "ortools/base/base_export.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/constraint_solver/variables.h"

namespace operations_research {

class BooleanVar;
class Demon;
class IntExpr;
class ModelVisitor;
class PropagationBaseObject;
class Solver;

/// Interval variables are often used in scheduling. The main characteristics
/// of an IntervalVar are the start position, duration, and end
/// date. All these characteristics can be queried and set, and demons can
/// be posted on their modifications.
///
/// An important aspect is optionality: an IntervalVar can be performed or not.
/// If unperformed, then it simply does not exist, and its characteristics
/// cannot be accessed any more. An interval var is automatically marked
/// as unperformed when it is not consistent anymore (start greater
/// than end, duration < 0...)
class OR_DLL IntervalVar : public PropagationBaseObject {
 public:
  /// The smallest acceptable value to be returned by StartMin()
  static const int64_t kMinValidValue;
  /// The largest acceptable value to be returned by EndMax()
  static const int64_t kMaxValidValue;
  IntervalVar(Solver* solver, const std::string& name);

#ifndef SWIG
  // This type is neither copyable nor movable.
  IntervalVar(const IntervalVar&) = delete;
  IntervalVar& operator=(const IntervalVar&) = delete;
#endif
  ~IntervalVar() override = default;

  /// These methods query, set, and watch the start position of the
  /// interval var.
  virtual int64_t StartMin() const = 0;
  virtual int64_t StartMax() const = 0;
  virtual void SetStartMin(int64_t m) = 0;
  virtual void SetStartMax(int64_t m) = 0;
  virtual void SetStartRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldStartMin() const = 0;
  virtual int64_t OldStartMax() const = 0;
  virtual void WhenStartRange(Demon* d) = 0;
  void WhenStartRange(Solver::Closure closure) {
    WhenStartRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenStartRange(Solver::Action action) {
    WhenStartRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenStartBound(Demon* d) = 0;
  void WhenStartBound(Solver::Closure closure) {
    WhenStartBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenStartBound(Solver::Action action) {
    WhenStartBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the duration of the interval var.
  virtual int64_t DurationMin() const = 0;
  virtual int64_t DurationMax() const = 0;
  virtual void SetDurationMin(int64_t m) = 0;
  virtual void SetDurationMax(int64_t m) = 0;
  virtual void SetDurationRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldDurationMin() const = 0;
  virtual int64_t OldDurationMax() const = 0;
  virtual void WhenDurationRange(Demon* d) = 0;
  void WhenDurationRange(Solver::Closure closure) {
    WhenDurationRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenDurationRange(Solver::Action action) {
    WhenDurationRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenDurationBound(Demon* d) = 0;
  void WhenDurationBound(Solver::Closure closure) {
    WhenDurationBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenDurationBound(Solver::Action action) {
    WhenDurationBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the end position of the interval var.
  virtual int64_t EndMin() const = 0;
  virtual int64_t EndMax() const = 0;
  virtual void SetEndMin(int64_t m) = 0;
  virtual void SetEndMax(int64_t m) = 0;
  virtual void SetEndRange(int64_t mi, int64_t ma) = 0;
  virtual int64_t OldEndMin() const = 0;
  virtual int64_t OldEndMax() const = 0;
  virtual void WhenEndRange(Demon* d) = 0;
  void WhenEndRange(Solver::Closure closure) {
    WhenEndRange(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenEndRange(Solver::Action action) {
    WhenEndRange(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG
  virtual void WhenEndBound(Demon* d) = 0;
  void WhenEndBound(Solver::Closure closure) {
    WhenEndBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenEndBound(Solver::Action action) {
    WhenEndBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods query, set, and watch the performed status of the
  /// interval var.
  virtual bool MustBePerformed() const = 0;
  virtual bool MayBePerformed() const = 0;
  bool CannotBePerformed() const { return !MayBePerformed(); }
  bool IsPerformedBound() const {
    return MustBePerformed() || !MayBePerformed();
  }
  virtual void SetPerformed(bool val) = 0;
  virtual bool WasPerformedBound() const = 0;
  virtual void WhenPerformedBound(Demon* d) = 0;
  void WhenPerformedBound(Solver::Closure closure) {
    WhenPerformedBound(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  void WhenPerformedBound(Solver::Action action) {
    WhenPerformedBound(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// Attaches a demon awakened when anything about this interval changes.
  void WhenAnything(Demon* d);
  /// Attaches a closure awakened when anything about this interval changes.
  void WhenAnything(Solver::Closure closure) {
    WhenAnything(solver()->MakeClosureDemon(std::move(closure)));
  }
#if !defined(SWIG)
  /// Attaches an action awakened when anything about this interval changes.
  void WhenAnything(Solver::Action action) {
    WhenAnything(solver()->MakeActionDemon(std::move(action)));
  }
#endif  // SWIG

  /// These methods create expressions encapsulating the start, end
  /// and duration of the interval var. Please note that these must not
  /// be used if the interval var is unperformed.
  virtual IntExpr* StartExpr() = 0;
  virtual IntExpr* DurationExpr() = 0;
  virtual IntExpr* EndExpr() = 0;
  virtual IntExpr* PerformedExpr() = 0;
  /// These methods create expressions encapsulating the start, end
  /// and duration of the interval var. If the interval var is
  /// unperformed, they will return the unperformed_value.
  virtual IntExpr* SafeStartExpr(int64_t unperformed_value) = 0;
  virtual IntExpr* SafeDurationExpr(int64_t unperformed_value) = 0;
  virtual IntExpr* SafeEndExpr(int64_t unperformed_value) = 0;

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* visitor) const = 0;
};

#if !defined(SWIG)

// ----- MirrorIntervalVar -----

class MirrorIntervalVar : public IntervalVar {
 public:
  MirrorIntervalVar(Solver* s, IntervalVar* t);

  // This type is neither copyable nor movable.
  MirrorIntervalVar(const MirrorIntervalVar&) = delete;
  MirrorIntervalVar& operator=(const MirrorIntervalVar&) = delete;
  ~MirrorIntervalVar() override;

  // These methods query, set and watch the start position of the
  // interval var.
  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  // These methods query, set and watch the duration of the interval var.
  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  // These methods query, set and watch the end position of the interval var.
  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  // These methods query, set and watches the performed status of the
  // interval var.
  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;

  void Accept(ModelVisitor* visitor) const override;

  std::string DebugString() const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

 private:
  IntervalVar* const t_;
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
  explicit AlwaysPerformedIntervalVarWrapper(IntervalVar* t);

  // This type is neither copyable nor movable.
  AlwaysPerformedIntervalVarWrapper(const AlwaysPerformedIntervalVarWrapper&) =
      delete;
  AlwaysPerformedIntervalVarWrapper& operator=(
      const AlwaysPerformedIntervalVarWrapper&) = delete;

  ~AlwaysPerformedIntervalVarWrapper() override;
  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;
  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;
  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;
  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

 protected:
  IntervalVar* underlying() const;
  bool MayUnderlyingBePerformed() const;

 private:
  IntervalVar* const t_;
  IntExpr* start_expr_;
  IntExpr* duration_expr_;
  IntExpr* end_expr_;
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
  explicit IntervalVarRelaxedMax(IntervalVar* t);
  ~IntervalVarRelaxedMax() override;
  int64_t StartMax() const override;
  void SetStartMax(int64_t m) override;
  int64_t EndMax() const override;
  void SetEndMax(int64_t m) override;

  void Accept(ModelVisitor* visitor) const override;

  std::string DebugString() const override;
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
  explicit IntervalVarRelaxedMin(IntervalVar* t);
  ~IntervalVarRelaxedMin() override;
  int64_t StartMin() const override;
  void SetStartMin(int64_t m) override;
  int64_t EndMin() const override;
  void SetEndMin(int64_t m) override;

  void Accept(ModelVisitor* visitor) const override;

  std::string DebugString() const override;
};

// ----- BaseIntervalVar -----

enum IntervalField { START, DURATION, END };

IntervalVar* NullInterval();

class BaseIntervalVar : public IntervalVar {
 public:
  class Handler : public Demon {
   public:
    explicit Handler(BaseIntervalVar* var);
    ~Handler() override;
    void Run(Solver* s) override;
    Solver::DemonPriority priority() const override;
    std::string DebugString() const override;

   private:
    BaseIntervalVar* const var_;
  };

  BaseIntervalVar(Solver* s, const std::string& name);

  ~BaseIntervalVar() override;

  virtual void Process() = 0;

  virtual void Push() = 0;

  void CleanInProcess();

  std::string BaseName() const override;

  bool InProcess() const;

 protected:
  bool in_process_;
  Handler handler_;
  Solver::Action cleaner_;
};

// ----- RangeVar -----

class RangeVar : public IntExpr {
 public:
  RangeVar(Solver* s, BaseIntervalVar* var, int64_t mi, int64_t ma);

  ~RangeVar() override;

  bool Bound() const override;

  int64_t Min() const override;

  int64_t Max() const override;

  void SetMin(int64_t m) override;

  int64_t OldMin() const;

  void SetMax(int64_t m) override;

  int64_t OldMax() const;

  void SetRange(int64_t mi, int64_t ma) override;

  void WhenRange(Demon* demon) override;

  void WhenBound(Demon* demon);

  void UpdatePostponedBounds();

  void ProcessDemons();

  void UpdatePreviousBounds();

  // TODO(user): Remove this interval field enum.
  void ApplyPostponedBounds(IntervalField which);

  IntVar* Var() override;

  std::string DebugString() const override;

 private:
  void SyncPreviousBounds();

  // The current reversible bounds of the interval.
  NumericalRev<int64_t> min_;
  NumericalRev<int64_t> max_;
  BaseIntervalVar* const var_;
  // When in process, the modifications are postponed and stored in
  // these 2 fields.
  int64_t postponed_min_;
  int64_t postponed_max_;
  // The previous bounds stores the bounds since the last time
  // ProcessDemons() was run. These are maintained lazily.
  int64_t previous_min_;
  int64_t previous_max_;
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
  PerformedVar(Solver* s, BaseIntervalVar* var, bool optional);
  // var = [0] (always unperformed).
  PerformedVar(Solver* s, BaseIntervalVar* var);

  ~PerformedVar() override;

  void SetValue(int64_t v) override;

  int64_t OldMin() const override;

  int64_t OldMax() const override;

  void RestoreBooleanValue() override;

  void Process();

  void UpdatePostponedValue();

  void UpdatePreviousValueAndApplyPostponedValue();

  std::string DebugString() const override;

 private:
  BaseIntervalVar* const var_;
  int previous_value_;
  int postponed_value_;
};

// ----- FixedDurationIntervalVar -----

class FixedDurationIntervalVar : public BaseIntervalVar {
 public:
  FixedDurationIntervalVar(Solver* s, int64_t start_min, int64_t start_max,
                           int64_t duration, bool optional,
                           const std::string& name);
  // Unperformed interval.
  FixedDurationIntervalVar(Solver* s, const std::string& name);
  ~FixedDurationIntervalVar() override;

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  void Process() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

  void Push() override;

 private:
  RangeVar start_;
  int64_t duration_;
  PerformedVar performed_;
};

// ----- FixedDurationPerformedIntervalVar -----

class FixedDurationPerformedIntervalVar : public BaseIntervalVar {
 public:
  FixedDurationPerformedIntervalVar(Solver* s, int64_t start_min,
                                    int64_t start_max, int64_t duration,
                                    const std::string& name);
  // Unperformed interval.
  FixedDurationPerformedIntervalVar(Solver* s, const std::string& name);
  ~FixedDurationPerformedIntervalVar() override;

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  void Process() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

 private:
  void CheckOldPerformed();
  void Push() override;

  RangeVar start_;
  int64_t duration_;
};

// ----- StartVarPerformedIntervalVar -----

class StartVarPerformedIntervalVar : public IntervalVar {
 public:
  StartVarPerformedIntervalVar(Solver* s, IntVar* var, int64_t duration,
                               const std::string& name);
  ~StartVarPerformedIntervalVar() override;

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  std::string DebugString() const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

  void Accept(ModelVisitor* visitor) const override;

 private:
  IntVar* const start_var_;
  int64_t duration_;
};

// ----- StartVarIntervalVar -----

class StartVarIntervalVar : public BaseIntervalVar {
 public:
  StartVarIntervalVar(Solver* s, IntVar* start, int64_t duration,
                      IntVar* performed, const std::string& name);
  ~StartVarIntervalVar() override {}

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

  void Process() override;

  void Push() override;

  int64_t StoredMin() const;
  int64_t StoredMax() const;

 private:
  IntVar* const start_;
  int64_t duration_;
  IntVar* const performed_;
  Rev<int64_t> start_min_;
  Rev<int64_t> start_max_;
};

// ----- LinkStartVarIntervalVar -----

class LinkStartVarIntervalVar : public Constraint {
 public:
  LinkStartVarIntervalVar(Solver* solver, StartVarIntervalVar* interval,
                          IntVar* start, IntVar* performed);

  ~LinkStartVarIntervalVar() override;

  void Post() override;

  void InitialPropagate() override;

  void PerformedBound();

 private:
  StartVarIntervalVar* const interval_;
  IntVar* const start_;
  IntVar* const performed_;
};

// ----- FixedInterval -----

class FixedInterval : public IntervalVar {
 public:
  FixedInterval(Solver* s, int64_t start, int64_t duration,
                const std::string& name);
  ~FixedInterval() override;

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

 private:
  const int64_t start_;
  const int64_t duration_;
};

// ----- VariableDurationIntervalVar -----

class VariableDurationIntervalVar : public BaseIntervalVar {
 public:
  VariableDurationIntervalVar(Solver* s, int64_t start_min, int64_t start_max,
                              int64_t duration_min, int64_t duration_max,
                              int64_t end_min, int64_t end_max, bool optional,
                              const std::string& name);

  ~VariableDurationIntervalVar() override;

  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;

  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;

  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;

  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;

  void Process() override;

  std::string DebugString() const override;

  void Accept(ModelVisitor* visitor) const override;

  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

 private:
  void Push() override;

  RangeVar start_;
  RangeVar duration_;
  RangeVar end_;
  PerformedVar performed_;
};

// ----- Base synced interval var -----

class FixedDurationSyncedIntervalVar : public IntervalVar {
 public:
  FixedDurationSyncedIntervalVar(IntervalVar* t, int64_t duration,
                                 int64_t offset, const std::string& name);

  // This type is neither copyable nor movable.
  FixedDurationSyncedIntervalVar(const FixedDurationSyncedIntervalVar&) =
      delete;
  FixedDurationSyncedIntervalVar& operator=(
      const FixedDurationSyncedIntervalVar&) = delete;
  ~FixedDurationSyncedIntervalVar() override;
  int64_t DurationMin() const override;
  int64_t DurationMax() const override;
  void SetDurationMin(int64_t m) override;
  void SetDurationMax(int64_t m) override;
  void SetDurationRange(int64_t mi, int64_t ma) override;
  int64_t OldDurationMin() const override;
  int64_t OldDurationMax() const override;
  void WhenDurationRange(Demon* d) override;
  void WhenDurationBound(Demon* d) override;
  int64_t EndMin() const override;
  int64_t EndMax() const override;
  void SetEndMin(int64_t m) override;
  void SetEndMax(int64_t m) override;
  void SetEndRange(int64_t mi, int64_t ma) override;
  int64_t OldEndMin() const override;
  int64_t OldEndMax() const override;
  void WhenEndRange(Demon* d) override;
  void WhenEndBound(Demon* d) override;
  bool MustBePerformed() const override;
  bool MayBePerformed() const override;
  void SetPerformed(bool val) override;
  bool WasPerformedBound() const override;
  void WhenPerformedBound(Demon* d) override;

 protected:
  IntervalVar* const t_;
  const int64_t duration_;
  const int64_t offset_;
};

// ----- Fixed duration interval var synced on start -----

class FixedDurationIntervalVarStartSyncedOnStart
    : public FixedDurationSyncedIntervalVar {
 public:
  FixedDurationIntervalVarStartSyncedOnStart(IntervalVar* t, int64_t duration,
                                             int64_t offset);
  ~FixedDurationIntervalVarStartSyncedOnStart() override;
  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;
  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;
  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;
};

// ----- Fixed duration interval start synced on end -----

class FixedDurationIntervalVarStartSyncedOnEnd
    : public FixedDurationSyncedIntervalVar {
 public:
  FixedDurationIntervalVarStartSyncedOnEnd(IntervalVar* t, int64_t duration,
                                           int64_t offset);
  ~FixedDurationIntervalVarStartSyncedOnEnd() override;
  int64_t StartMin() const override;
  int64_t StartMax() const override;
  void SetStartMin(int64_t m) override;
  void SetStartMax(int64_t m) override;
  void SetStartRange(int64_t mi, int64_t ma) override;
  int64_t OldStartMin() const override;
  int64_t OldStartMax() const override;
  void WhenStartRange(Demon* d) override;
  void WhenStartBound(Demon* d) override;
  IntExpr* StartExpr() override;
  IntExpr* DurationExpr() override;
  IntExpr* EndExpr() override;
  IntExpr* PerformedExpr() override;
  // These methods create expressions encapsulating the start, end
  // and duration of the interval var. If the interval var is
  // unperformed, they will return the unperformed_value.
  IntExpr* SafeStartExpr(int64_t unperformed_value) override;
  IntExpr* SafeDurationExpr(int64_t unperformed_value) override;
  IntExpr* SafeEndExpr(int64_t unperformed_value) override;

  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;
};

#endif  // !defined(SWIG)

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_INTERVAL_H_
