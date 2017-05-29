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

#ifndef OR_TOOLS_SAT_INTERVALS_H_
#define OR_TOOLS_SAT_INTERVALS_H_

#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

DEFINE_INT_TYPE(IntervalVariable, int32);
const IntervalVariable kNoIntervalVariable(-1);

// This class maintains a set of intervals which correspond to three integer
// variables (start, end and size). It automatically registers with the
// PrecedencesPropagator the relation between the bounds of each interval and
// provides many helper functions to add precedences relation between intervals.
class IntervalsRepository {
 public:
  IntervalsRepository(IntegerTrail* integer_trail,
                      PrecedencesPropagator* precedences)
      : integer_trail_(integer_trail), precedences_(precedences) {}

  static IntervalsRepository* CreateInModel(Model* model) {
    IntervalsRepository* intervals =
        new IntervalsRepository(model->GetOrCreate<IntegerTrail>(),
                                model->GetOrCreate<PrecedencesPropagator>());
    model->TakeOwnership(intervals);
    return intervals;
  }

  // Returns the current number of intervals in the repository.
  // The interval will always be identified by an integer in [0, num_intervals).
  int NumIntervals() const { return start_vars_.size(); }

  // Functions to add a new interval to the repository.
  // - If size == kNoIntegerVariable, then the size is assumed to be fixed
  //   to fixed_size.
  // - If is_present != kNoLiteralIndex, then this is an optional interval.
  IntervalVariable CreateInterval(IntegerVariable start, IntegerVariable end,
                                  IntegerVariable size, IntegerValue fixed_size,
                                  LiteralIndex is_present);

  // Returns whether or not a interval is optional and the associated literal.
  bool IsOptional(IntervalVariable i) const {
    return is_present_[i] != kNoLiteralIndex;
  }
  Literal IsPresentLiteral(IntervalVariable i) const {
    return Literal(is_present_[i]);
  }

  // The 3 integer variables associated to a interval.
  // Fixed size intervals will have a kNoIntegerVariable as size.
  //
  // Note: For an optional interval, the start/end variables are propagated
  // asssuming the interval is present. Because of that, these variables can
  // cross each other or have an empty domain. If any of this happen, then the
  // IsPresentLiteral() of this interval will be propagated to false.
  IntegerVariable SizeVar(IntervalVariable i) const { return size_vars_[i]; }
  IntegerVariable StartVar(IntervalVariable i) const { return start_vars_[i]; }
  IntegerVariable EndVar(IntervalVariable i) const { return end_vars_[i]; }

  // Return the minimum size of the given IntervalVariable.
  IntegerValue MinSize(IntervalVariable i) const {
    const IntegerVariable size_var = size_vars_[i];
    if (size_var == kNoIntegerVariable) return fixed_sizes_[i];
    return integer_trail_->LowerBound(size_var);
  }

  // Return the maximum size of the given IntervalVariable.
  IntegerValue MaxSize(IntervalVariable i) const {
    const IntegerVariable size_var = size_vars_[i];
    if (size_var == kNoIntegerVariable) return fixed_sizes_[i];
    return integer_trail_->UpperBound(size_var);
  }

 private:
  // External classes needed.
  IntegerTrail* integer_trail_;
  PrecedencesPropagator* precedences_;

  // Literal indicating if the tasks is executed. Tasks that are always executed
  // will have a kNoLiteralIndex entry in this vector.
  ITIVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  ITIVector<IntervalVariable, IntegerVariable> start_vars_;
  ITIVector<IntervalVariable, IntegerVariable> end_vars_;
  ITIVector<IntervalVariable, IntegerVariable> size_vars_;
  ITIVector<IntervalVariable, IntegerValue> fixed_sizes_;

  DISALLOW_COPY_AND_ASSIGN(IntervalsRepository);
};

// Helper class shared by the propagators that manage a given list of tasks.
//
// One of the main advantage of this class is that it allows to share the
// vectors of tasks sorted by various criteria between propagator for a faster
// code.
class SchedulingConstraintHelper {
 public:
  // All the functions below refer to a task by its index t in the tasks
  // vector given at construction.
  SchedulingConstraintHelper(const std::vector<IntervalVariable>& tasks,
                             Trail* trail, IntegerTrail* integer_trail,
                             IntervalsRepository* intervals_repository);

  // Returns the number of task.
  int NumTasks() const { return start_vars_.size(); }

  // Sets the time direction to either forward/backward. This will impact all
  // the functions below.
  void SetTimeDirection(bool is_forward);

  // Helpers for the current bounds on the current task time window.
  //      [(duration-min)       ...      (duration-min)]
  //      ^             ^                ^             ^
  //   start-min     end-min          start-max     end-max
  //
  // Note that for tasks with variable durations, we don't necessarily have
  // duration-min between the the XXX-min and XXX-max value.
  IntegerValue DurationMin(int t) const;
  IntegerValue DurationMax(int t) const;
  IntegerValue StartMin(int t) const;
  IntegerValue StartMax(int t) const;
  IntegerValue EndMin(int t) const;
  IntegerValue EndMax(int t) const;

  // Returns true if the corresponding fact is known for sure. A normal task is
  // always present. For optional task for which the presence is still unknown,
  // both of these function will return false.
  bool IsPresent(int t) const;
  bool IsAbsent(int t) const;

  // Sorts and returns the tasks in corresponding order at the time of the call.
  // Note that we do not mean strictly-increasing/strictly-decreasing, there
  // will be duplicate time values in these vectors.
  //
  // TODO(user): we could merge the first loop of IncrementalSort() with the
  // loop that fill TaskTime.time at each call.
  struct TaskTime {
    int task_index;
    IntegerValue time;
    bool operator<(TaskTime other) const { return time < other.time; }
    bool operator>(TaskTime other) const { return time > other.time; }
  };
  const std::vector<TaskTime>& TaskByIncreasingStartMin();
  const std::vector<TaskTime>& TaskByIncreasingEndMin();
  const std::vector<TaskTime>& TaskByDecreasingStartMax();
  const std::vector<TaskTime>& TaskByDecreasingEndMax();

  // Functions to clear and then set the current reason.
  void ClearReason();
  void AddPresenceReason(int t);
  void AddDurationMinReason(int t);
  void AddDurationMinReason(int t, IntegerValue lower_bound);
  void AddStartMinReason(int t, IntegerValue lower_bound);
  void AddStartMaxReason(int t, IntegerValue upper_bound);
  void AddEndMinReason(int t, IntegerValue lower_bound);
  void AddEndMaxReason(int t, IntegerValue upper_bound);

  // It is also possible to directly manipulates the underlying reason vectors
  // that will be used when pushing something.
  std::vector<Literal>* MutableLiteralReason() { return &literal_reason_; }
  std::vector<IntegerLiteral>* MutableIntegerReason() {
    return &integer_reason_;
  }

  // Push something using the current reason. Note that IncreaseStartMin() will
  // also increase the end-min, and DecreaseEndMax() will also decrease the
  // start-max.
  bool IncreaseStartMin(int t, IntegerValue new_min_start);
  bool DecreaseEndMax(int t, IntegerValue new_max_end);
  void PushTaskAbsence(int t);
  bool PushIntegerLiteral(IntegerLiteral bound);
  bool ReportConflict();

  // Returns the underlying integer variables.
  const std::vector<IntegerVariable>& StartVars() const { return start_vars_; }
  const std::vector<IntegerVariable>& EndVars() const { return end_vars_; }

  // Registers the given propagator id to be called if any of the tasks
  // in this class change.
  void WatchAllTasks(int id, GenericLiteralWatcher* watcher) const;

 private:
  Trail* trail_;
  IntegerTrail* integer_trail_;

  // All the underlying variables of the tasks.
  // The vectors are indexed by the task index t.
  std::vector<IntegerVariable> start_vars_;
  std::vector<IntegerVariable> end_vars_;
  std::vector<IntegerVariable> duration_vars_;
  std::vector<IntegerValue> fixed_durations_;
  std::vector<LiteralIndex> reason_for_presence_;

  // The current direction of time, true for forward, false for backward.
  bool current_time_direction_;

  // The negation of the start/end variable so that SetTimeDirection()
  // can do its job in O(1) instead of calling NegationOf() on each entry.
  std::vector<IntegerVariable> minus_start_vars_;
  std::vector<IntegerVariable> minus_end_vars_;

  // Sorted vectors returned by the TasksBy*() functions.
  std::vector<TaskTime> task_by_increasing_min_start_;
  std::vector<TaskTime> task_by_increasing_min_end_;
  std::vector<TaskTime> task_by_decreasing_max_start_;
  std::vector<TaskTime> task_by_decreasing_max_end_;

  // Reason vectors.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;
};

// =============================================================================
// SchedulingConstraintHelper inlined functions.
// =============================================================================

inline IntegerValue SchedulingConstraintHelper::DurationMin(int t) const {
  return duration_vars_[t] == kNoIntegerVariable
             ? fixed_durations_[t]
             : integer_trail_->LowerBound(duration_vars_[t]);
}

inline IntegerValue SchedulingConstraintHelper::DurationMax(int t) const {
  return duration_vars_[t] == kNoIntegerVariable
             ? fixed_durations_[t]
             : integer_trail_->UpperBound(duration_vars_[t]);
}

inline IntegerValue SchedulingConstraintHelper::StartMin(int t) const {
  return integer_trail_->LowerBound(start_vars_[t]);
}

inline IntegerValue SchedulingConstraintHelper::StartMax(int t) const {
  return integer_trail_->UpperBound(start_vars_[t]);
}

inline IntegerValue SchedulingConstraintHelper::EndMin(int t) const {
  return integer_trail_->LowerBound(end_vars_[t]);
}

inline IntegerValue SchedulingConstraintHelper::EndMax(int t) const {
  return integer_trail_->UpperBound(end_vars_[t]);
}

inline bool SchedulingConstraintHelper::IsPresent(int t) const {
  if (reason_for_presence_[t] == kNoLiteralIndex) return true;
  return trail_->Assignment().LiteralIsTrue(Literal(reason_for_presence_[t]));
}

inline bool SchedulingConstraintHelper::IsAbsent(int t) const {
  if (reason_for_presence_[t] == kNoLiteralIndex) return false;
  return trail_->Assignment().LiteralIsFalse(Literal(reason_for_presence_[t]));
}

inline void SchedulingConstraintHelper::ClearReason() {
  integer_reason_.clear();
  literal_reason_.clear();
}

inline void SchedulingConstraintHelper::AddPresenceReason(int t) {
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
  }
}

inline void SchedulingConstraintHelper::AddDurationMinReason(int t) {
  if (duration_vars_[t] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
  }
}

inline void SchedulingConstraintHelper::AddDurationMinReason(
    int t, IntegerValue lower_bound) {
  if (duration_vars_[t] != kNoIntegerVariable) {
    integer_reason_.push_back(
        IntegerLiteral::GreaterOrEqual(duration_vars_[t], lower_bound));
  }
}

inline void SchedulingConstraintHelper::AddStartMinReason(
    int t, IntegerValue lower_bound) {
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(start_vars_[t], lower_bound));
}

inline void SchedulingConstraintHelper::AddStartMaxReason(
    int t, IntegerValue upper_bound) {
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(start_vars_[t], upper_bound));
}

inline void SchedulingConstraintHelper::AddEndMinReason(
    int t, IntegerValue lower_bound) {
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(end_vars_[t], lower_bound));
}

inline void SchedulingConstraintHelper::AddEndMaxReason(
    int t, IntegerValue upper_bound) {
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(end_vars_[t], upper_bound));
}

// =============================================================================
// Model based functions.
// =============================================================================

inline std::function<IntegerVariable(const Model&)> StartVar(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->StartVar(v);
  };
}

inline std::function<IntegerVariable(const Model&)> EndVar(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->EndVar(v);
  };
}

inline std::function<IntegerVariable(const Model&)> SizeVar(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->SizeVar(v);
  };
}

inline std::function<int64(const Model&)> MinSize(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->MinSize(v).value();
  };
}

inline std::function<int64(const Model&)> MaxSize(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->MaxSize(v).value();
  };
}

inline std::function<bool(const Model&)> IsOptional(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->IsOptional(v);
  };
}

inline std::function<Literal(const Model&)> IsPresentLiteral(
    IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->IsPresentLiteral(v);
  };
}

inline std::function<IntervalVariable(Model*)> NewInterval(int64 min_start,
                                                           int64 max_end,
                                                           int64 size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)), kNoIntegerVariable,
        IntegerValue(size), kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewInterval(
    IntegerVariable start, IntegerVariable end, IntegerVariable size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalWithVariableSize(
    int64 min_start, int64 max_end, int64 min_size, int64 max_size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        kNoLiteralIndex);
  };
}

inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    int64 min_start, int64 max_end, int64 size, Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)), kNoIntegerVariable,
        IntegerValue(size), is_present.Index());
  };
}

inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    IntegerVariable start, IntegerVariable end, IntegerVariable size,
    Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), is_present.Index());
  };
}

inline std::function<IntervalVariable(Model*)>
NewOptionalIntervalWithVariableSize(int64 min_start, int64 max_end,
                                    int64 min_size, int64 max_size,
                                    Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        is_present.Index());
  };
}

inline std::function<IntervalVariable(Model*)> NewIntervalFromStartAndSizeVars(
    IntegerVariable start, IntegerVariable size) {
  return [=](Model* model) {
    // Create the "end" variable.
    // TODO(user): deal with overflow.
    IntegerTrail* t = model->GetOrCreate<IntegerTrail>();
    const IntegerValue end_lb = t->LowerBound(start) + t->LowerBound(size);
    const IntegerValue end_ub = t->UpperBound(start) + t->UpperBound(size);
    const IntegerVariable end = t->AddIntegerVariable(end_lb, end_ub);
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, size, IntegerValue(0), kNoLiteralIndex);
  };
}

// This requires that all the alternatives are optional tasks.
inline std::function<void(Model*)> IntervalWithAlternatives(
    IntervalVariable master, const std::vector<IntervalVariable>& members) {
  return [=](Model* model) {
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();

    std::vector<Literal> presences;
    std::vector<IntegerValue> durations;

    // Create an "exactly one executed" constraint on the alternatives.
    std::vector<LiteralWithCoeff> sat_ct;
    for (const IntervalVariable member : members) {
      CHECK(intervals->IsOptional(member));
      const Literal is_present = intervals->IsPresentLiteral(member);
      sat_ct.push_back({is_present, Coefficient(1)});
      model->Add(
          Equality(model->Get(StartVar(master)), model->Get(StartVar(member))));
      model->Add(
          Equality(model->Get(EndVar(master)), model->Get(EndVar(member))));

      // TODO(user): IsOneOf() only work for members with fixed size.
      // Generalize to an "int_var_element" constraint.
      CHECK_EQ(intervals->SizeVar(member), kNoIntegerVariable);
      presences.push_back(is_present);
      durations.push_back(intervals->MinSize(member));
    }
    if (intervals->SizeVar(master) != kNoIntegerVariable) {
      model->Add(IsOneOf(intervals->SizeVar(master), presences, durations));
    }
    model->Add(BooleanLinearConstraint(1, 1, &sat_ct));

    // Propagate from the candidate bounds to the master interval ones.
    {
      std::vector<IntegerVariable> starts;
      for (const IntervalVariable member : members) {
        starts.push_back(intervals->StartVar(member));
      }
      model->Add(
          PartialIsOneOfVar(intervals->StartVar(master), starts, presences));
    }
    {
      std::vector<IntegerVariable> ends;
      for (const IntervalVariable member : members) {
        ends.push_back(intervals->EndVar(member));
      }
      model->Add(PartialIsOneOfVar(intervals->EndVar(master), ends, presences));
    }
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTERVALS_H_
