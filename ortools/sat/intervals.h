// Copyright 2010-2018 Google LLC
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

#include <functional>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
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
  explicit IntervalsRepository(Model* model)
      : integer_trail_(model->GetOrCreate<IntegerTrail>()),
        precedences_(model->GetOrCreate<PrecedencesPropagator>()) {}

  // Returns the current number of intervals in the repository.
  // The interval will always be identified by an integer in [0, num_intervals).
  int NumIntervals() const { return start_vars_.size(); }

  // Functions to add a new interval to the repository.
  // - If size == kNoIntegerVariable, then the size is fixed to fixed_size.
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
  gtl::ITIVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  gtl::ITIVector<IntervalVariable, IntegerVariable> start_vars_;
  gtl::ITIVector<IntervalVariable, IntegerVariable> end_vars_;
  gtl::ITIVector<IntervalVariable, IntegerVariable> size_vars_;
  gtl::ITIVector<IntervalVariable, IntegerValue> fixed_sizes_;

  DISALLOW_COPY_AND_ASSIGN(IntervalsRepository);
};

// An helper struct to sort task by time. This is used by the
// SchedulingConstraintHelper but also by many scheduling propagators to sort
// tasks.
struct TaskTime {
  int task_index;
  IntegerValue time;
  bool operator<(TaskTime other) const { return time < other.time; }
  bool operator>(TaskTime other) const { return time > other.time; }
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
                             Model* model);

  // Temporary constructor.
  // The class will not be usable until ResetFromSubset() is called.
  //
  // TODO(user): Remove this. It is a hack because the disjunctive class needs
  // to fetch the maximum possible number of task at construction.
  SchedulingConstraintHelper(int num_tasks, Model* model);

  // Resets the class to the same state as if it was constructed with
  // the given subset of tasks from other.
  void ResetFromSubset(const SchedulingConstraintHelper& other,
                       absl::Span<const int> tasks);

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
  // duration-min between the XXX-min and XXX-max value.
  IntegerValue DurationMin(int t) const;
  IntegerValue DurationMax(int t) const;
  IntegerValue StartMin(int t) const;
  IntegerValue StartMax(int t) const;
  IntegerValue EndMin(int t) const;
  IntegerValue EndMax(int t) const;

  // In the presense of tasks with a variable duration, we do not necessarily
  // have start_min + duration_min = end_min, we can instead have a situation
  // like:
  //         |          |<- duration-min ->|
  //         ^          ^                  ^
  //        start-min   |                end-min
  //                    |
  // We define the "shifted start min" to be the right most time such that
  // we known that we must have min-duration "energy" to the right of it if the
  // task is present. Using it in our scheduling propagators allows to propagate
  // more in the presence of tasks with variable duration (or optional task
  // where we also do not necessarily have start_min + duration_min = end_min.
  //
  // To explain this shifted start min, one must use the AddEnergyAfterReason().
  IntegerValue ShiftedStartMin(int t) const;

  bool StartIsFixed(int t) const;
  bool EndIsFixed(int t) const;

  // Returns true if the corresponding fact is known for sure. A normal task is
  // always present. For optional task for which the presence is still unknown,
  // both of these function will return false.
  bool IsOptional(int t) const;
  bool IsPresent(int t) const;
  bool IsAbsent(int t) const;

  // Sorts and returns the tasks in corresponding order at the time of the call.
  // Note that we do not mean strictly-increasing/strictly-decreasing, there
  // will be duplicate time values in these vectors.
  //
  // TODO(user): we could merge the first loop of IncrementalSort() with the
  // loop that fill TaskTime.time at each call.
  const std::vector<TaskTime>& TaskByIncreasingStartMin();
  const std::vector<TaskTime>& TaskByIncreasingEndMin();
  const std::vector<TaskTime>& TaskByDecreasingStartMax();
  const std::vector<TaskTime>& TaskByDecreasingEndMax();
  const std::vector<TaskTime>& TaskByIncreasingShiftedStartMin();

  // Functions to clear and then set the current reason.
  void ClearReason();
  void AddPresenceReason(int t);
  void AddDurationMinReason(int t);
  void AddDurationMinReason(int t, IntegerValue lower_bound);
  void AddStartMinReason(int t, IntegerValue lower_bound);
  void AddStartMaxReason(int t, IntegerValue upper_bound);
  void AddEndMinReason(int t, IntegerValue lower_bound);
  void AddEndMaxReason(int t, IntegerValue upper_bound);
  void AddEnergyAfterReason(int t, IntegerValue energy_min, IntegerValue time);

  // Adds the reason why task "before" must be before task "after".
  // That is StartMax(before) < EndMin(after).
  void AddReasonForBeingBefore(int before, int after);

  // It is also possible to directly manipulates the underlying reason vectors
  // that will be used when pushing something.
  std::vector<Literal>* MutableLiteralReason() { return &literal_reason_; }
  std::vector<IntegerLiteral>* MutableIntegerReason() {
    return &integer_reason_;
  }

  // Push something using the current reason. Note that IncreaseStartMin() will
  // also increase the end-min, and DecreaseEndMax() will also decrease the
  // start-max.
  //
  // Important: IncreaseStartMin() and DecreaseEndMax() can be called on an
  // optional interval whose presence is still unknown and push a bound
  // conditionned on its presence. The functions will do the correct thing
  // depending on whether or not the start_min/end_max are optional variables
  // whose presence implies the interval presence.
  ABSL_MUST_USE_RESULT bool IncreaseStartMin(int t, IntegerValue new_min_start);
  ABSL_MUST_USE_RESULT bool DecreaseEndMax(int t, IntegerValue new_max_end);
  ABSL_MUST_USE_RESULT bool PushTaskAbsence(int t);
  ABSL_MUST_USE_RESULT bool PushIntegerLiteral(IntegerLiteral bound);
  ABSL_MUST_USE_RESULT bool ReportConflict();

  // Returns the underlying integer variables.
  const std::vector<IntegerVariable>& StartVars() const { return start_vars_; }
  const std::vector<IntegerVariable>& EndVars() const { return end_vars_; }
  const std::vector<IntegerVariable>& DurationVars() const {
    return duration_vars_;
  }

  // Registers the given propagator id to be called if any of the tasks
  // in this class change.
  void WatchAllTasks(int id, GenericLiteralWatcher* watcher,
                     bool watch_start_max = true,
                     bool watch_end_max = true) const;

  // Manages the other helper (used by the diffn constraint).
  //
  // For each interval appearing in a reason on this helper, another reason
  // will be added. This other reason specifies that on the other helper, the
  // corresponding interval overlaps 'event'.
  void SetOtherHelper(SchedulingConstraintHelper* other_helper,
                      IntegerValue event) {
    CHECK(other_helper != nullptr);
    other_helper_ = other_helper;
    event_for_other_helper_ = event;
  }

  void ClearOtherHelper() { other_helper_ = nullptr; }

  // Adds to this helper reason all the explanation of the other helper.
  // This checks that other_helper_ is null.
  //
  // This is used in the 2D energetic reasoning in the diffn constraint.
  void ImportOtherReasons(const SchedulingConstraintHelper& other_helper);

 private:
  void InitSortedVectors();

  // Internal function for IncreaseStartMin()/DecreaseEndMax().
  bool PushIntervalBound(int t, IntegerLiteral lit);

  // This will be called on any interval that is part of a reason or
  // a bound push. Since the last call to ClearReason(), for each unique
  // t, we will add once to other_helper_ the reason for t containing
  // the point event_for_other_helper_.
  void AddOtherReason(int t);

  // Import the reasons on the other helper into this helper.
  void ImportOtherReasons();

  Trail* trail_;
  IntegerTrail* integer_trail_;
  PrecedencesPropagator* precedences_;

  // The current direction of time, true for forward, false for backward.
  bool current_time_direction_ = true;

  // All the underlying variables of the tasks.
  // The vectors are indexed by the task index t.
  std::vector<IntegerVariable> start_vars_;
  std::vector<IntegerVariable> end_vars_;
  std::vector<IntegerVariable> duration_vars_;
  std::vector<IntegerValue> fixed_durations_;
  std::vector<LiteralIndex> reason_for_presence_;

  // The negation of the start/end variable so that SetTimeDirection()
  // can do its job in O(1) instead of calling NegationOf() on each entry.
  std::vector<IntegerVariable> minus_start_vars_;
  std::vector<IntegerVariable> minus_end_vars_;

  // Sorted vectors returned by the TasksBy*() functions.
  std::vector<TaskTime> task_by_increasing_min_start_;
  std::vector<TaskTime> task_by_increasing_min_end_;
  std::vector<TaskTime> task_by_decreasing_max_start_;
  std::vector<TaskTime> task_by_decreasing_max_end_;

  std::vector<TaskTime> task_by_increasing_shifted_start_min_;
  std::vector<TaskTime> task_by_decreasing_shifted_end_max_;

  // Reason vectors.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  // Optional 'slave' helper used in the diffn constraint.
  SchedulingConstraintHelper* other_helper_ = nullptr;
  IntegerValue event_for_other_helper_;
  std::vector<bool> already_added_to_other_reasons_;
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

// for optional interval, we don't necessarily have start + duration = end.
inline IntegerValue SchedulingConstraintHelper::ShiftedStartMin(int t) const {
  return std::max(StartMin(t), EndMin(t) - DurationMin(t));
}

inline bool SchedulingConstraintHelper::StartIsFixed(int t) const {
  return StartMin(t) == StartMax(t);
}

inline bool SchedulingConstraintHelper::EndIsFixed(int t) const {
  return EndMin(t) == EndMax(t);
}

inline bool SchedulingConstraintHelper::IsOptional(int t) const {
  return reason_for_presence_[t] != kNoLiteralIndex;
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
  if (other_helper_) {
    other_helper_->ClearReason();
    already_added_to_other_reasons_.assign(NumTasks(), false);
  }
}

inline void SchedulingConstraintHelper::AddPresenceReason(int t) {
  AddOtherReason(t);
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
  }
}

inline void SchedulingConstraintHelper::AddDurationMinReason(int t) {
  AddOtherReason(t);
  if (duration_vars_[t] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
  }
}

inline void SchedulingConstraintHelper::AddDurationMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);
  if (duration_vars_[t] != kNoIntegerVariable) {
    DCHECK_GE(DurationMin(t), lower_bound);
    integer_reason_.push_back(
        IntegerLiteral::GreaterOrEqual(duration_vars_[t], lower_bound));
  }
}

inline void SchedulingConstraintHelper::AddStartMinReason(
    int t, IntegerValue lower_bound) {
  DCHECK_GE(StartMin(t), lower_bound);
  AddOtherReason(t);
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(start_vars_[t], lower_bound));
}

inline void SchedulingConstraintHelper::AddStartMaxReason(
    int t, IntegerValue upper_bound) {
  DCHECK_LE(StartMax(t), upper_bound);
  AddOtherReason(t);
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(start_vars_[t], upper_bound));
}

inline void SchedulingConstraintHelper::AddEndMinReason(
    int t, IntegerValue lower_bound) {
  DCHECK_GE(EndMin(t), lower_bound);
  AddOtherReason(t);
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(end_vars_[t], lower_bound));
}

inline void SchedulingConstraintHelper::AddEndMaxReason(
    int t, IntegerValue upper_bound) {
  DCHECK_LE(EndMax(t), upper_bound);
  AddOtherReason(t);
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(end_vars_[t], upper_bound));
}

inline void SchedulingConstraintHelper::AddEnergyAfterReason(
    int t, IntegerValue energy_min, IntegerValue time) {
  if (StartMin(t) >= time) {
    AddStartMinReason(t, time);
  } else {
    AddEndMinReason(t, time + energy_min);
  }
  AddDurationMinReason(t, energy_min);
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

inline std::function<IntervalVariable(Model*)>
NewOptionalIntervalWithOptionalVariables(int64 min_start, int64 max_end,
                                         int64 size, Literal is_present) {
  return [=](Model* model) {
    // Note that we need to mark the optionality first.
    const IntegerVariable start =
        model->Add(NewIntegerVariable(min_start, max_end));
    const IntegerVariable end =
        model->Add(NewIntegerVariable(min_start, max_end));
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    integer_trail->MarkIntegerVariableAsOptional(start, is_present);
    integer_trail->MarkIntegerVariableAsOptional(end, is_present);
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        start, end, kNoIntegerVariable, IntegerValue(size), is_present.Index());
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
      starts.reserve(members.size());
      for (const IntervalVariable member : members) {
        starts.push_back(intervals->StartVar(member));
      }
      model->Add(
          PartialIsOneOfVar(intervals->StartVar(master), starts, presences));
    }
    {
      std::vector<IntegerVariable> ends;
      ends.reserve(members.size());
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
