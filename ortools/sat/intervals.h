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
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/strong_vector.h"
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
      : model_(model),
        assignment_(model->GetOrCreate<Trail>()->Assignment()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

  // Returns the current number of intervals in the repository.
  // The interval will always be identified by an integer in [0, num_intervals).
  int NumIntervals() const { return starts_.size(); }

  // Functions to add a new interval to the repository.
  // If add_linear_relation is true, then we also link start, size and end.
  //
  // - If size == kNoIntegerVariable, then the size is fixed to fixed_size.
  // - If is_present != kNoLiteralIndex, then this is an optional interval.
  IntervalVariable CreateInterval(IntegerVariable start, IntegerVariable end,
                                  IntegerVariable size, IntegerValue fixed_size,
                                  LiteralIndex is_present);
  IntervalVariable CreateInterval(AffineExpression start, AffineExpression end,
                                  AffineExpression size,
                                  LiteralIndex is_present,
                                  bool add_linear_relation);

  // Returns whether or not a interval is optional and the associated literal.
  bool IsOptional(IntervalVariable i) const {
    return is_present_[i] != kNoLiteralIndex;
  }
  Literal PresenceLiteral(IntervalVariable i) const {
    return Literal(is_present_[i]);
  }
  bool IsPresent(IntervalVariable i) const {
    if (!IsOptional(i)) return true;
    return assignment_.LiteralIsTrue(PresenceLiteral(i));
  }
  bool IsAbsent(IntervalVariable i) const {
    if (!IsOptional(i)) return false;
    return assignment_.LiteralIsFalse(PresenceLiteral(i));
  }

  // The 3 integer variables associated to a interval.
  // Fixed size intervals will have a kNoIntegerVariable as size.
  //
  // Note: For an optional interval, the start/end variables are propagated
  // asssuming the interval is present. Because of that, these variables can
  // cross each other or have an empty domain. If any of this happen, then the
  // PresenceLiteral() of this interval will be propagated to false.
  AffineExpression Size(IntervalVariable i) const { return sizes_[i]; }
  AffineExpression Start(IntervalVariable i) const { return starts_[i]; }
  AffineExpression End(IntervalVariable i) const { return ends_[i]; }

  // Deprecated.
  IntegerVariable SizeVar(IntervalVariable i) const {
    if (sizes_[i].var != kNoIntegerVariable) {
      CHECK_EQ(sizes_[i].coeff, 1);
      CHECK_EQ(sizes_[i].constant, 0);
    }
    return sizes_[i].var;
  }
  IntegerVariable StartVar(IntervalVariable i) const {
    if (starts_[i].var != kNoIntegerVariable) {
      CHECK_EQ(starts_[i].coeff, 1);
      CHECK_EQ(starts_[i].constant, 0);
    }
    return starts_[i].var;
  }
  IntegerVariable EndVar(IntervalVariable i) const {
    if (ends_[i].var != kNoIntegerVariable) {
      CHECK_EQ(ends_[i].coeff, 1);
      CHECK_EQ(ends_[i].constant, 0);
    }
    return ends_[i].var;
  }

  // Return the minimum size of the given IntervalVariable.
  IntegerValue MinSize(IntervalVariable i) const {
    return integer_trail_->LowerBound(sizes_[i]);
  }

  // Return the maximum size of the given IntervalVariable.
  IntegerValue MaxSize(IntervalVariable i) const {
    return integer_trail_->UpperBound(sizes_[i]);
  }

  // Utility function that returns a vector will all intervals.
  std::vector<IntervalVariable> AllIntervals() const {
    std::vector<IntervalVariable> result;
    for (IntervalVariable i(0); i < NumIntervals(); ++i) {
      result.push_back(i);
    }
    return result;
  }

 private:
  // External classes needed.
  Model* model_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;

  // Literal indicating if the tasks is executed. Tasks that are always executed
  // will have a kNoLiteralIndex entry in this vector.
  absl::StrongVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  absl::StrongVector<IntervalVariable, AffineExpression> starts_;
  absl::StrongVector<IntervalVariable, AffineExpression> ends_;
  absl::StrongVector<IntervalVariable, AffineExpression> sizes_;

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
class SchedulingConstraintHelper : public PropagatorInterface,
                                   ReversibleInterface {
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

  // This is a propagator so we can "cache" all the intervals relevant
  // information. This gives good speedup. Note however that the info is stale
  // except if a bound was pushed by this helper or if this was called. We run
  // it at the highest priority, so that will mostly be the case at the
  // beginning of each Propagate() call of the classes using this.
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher);
  void SetLevel(int level) final;

  // Resets the class to the same state as if it was constructed with
  // the given subset of tasks from other.
  void ResetFromSubset(const SchedulingConstraintHelper& other,
                       absl::Span<const int> tasks);

  // Returns the number of task.
  int NumTasks() const { return starts_.size(); }

  // Make sure the cached values are up to date. Also sets the time direction to
  // either forward/backward. This will impact all the functions below. This
  // MUST be called at the beginning of all Propagate() call that uses this
  // helper.
  void SynchronizeAndSetTimeDirection(bool is_forward);

  // Helpers for the current bounds on the current task time window.
  //      [  (size-min)         ...        (size-min)  ]
  //      ^             ^                ^             ^
  //   start-min     end-min          start-max     end-max
  //
  // Note that for tasks with variable durations, we don't necessarily have
  // duration-min between the XXX-min and XXX-max value.
  //
  // Remark: We use cached values for most of these function as this is faster.
  // In practice, the cache will almost always be up to date, but not in corner
  // cases where pushing the start of one task will change values for many
  // others. This is fine as the new values will be picked up as we reach the
  // propagation fixed point.
  IntegerValue SizeMin(int t) const { return cached_duration_min_[t]; }
  IntegerValue SizeMax(int t) const {
    // This one is "rare" so we don't cache it.
    return integer_trail_->UpperBound(sizes_[t]);
  }
  IntegerValue StartMin(int t) const { return cached_start_min_[t]; }
  IntegerValue EndMin(int t) const { return cached_end_min_[t]; }
  IntegerValue StartMax(int t) const { return -cached_negated_start_max_[t]; }
  IntegerValue EndMax(int t) const { return -cached_negated_end_max_[t]; }

  // In the presence of tasks with a variable size, we do not necessarily
  // have start_min + size_min = end_min, we can instead have a situation
  // like:
  //         |          |<--- size-min --->|
  //         ^          ^                  ^
  //        start-min   |                end-min
  //                    |
  // We define the "shifted start min" to be the right most time such that
  // we known that we must have min-size "energy" to the right of it if the
  // task is present. Using it in our scheduling propagators allows to propagate
  // more in the presence of tasks with variable size (or optional task
  // where we also do not necessarily have start_min + size_min = end_min.
  //
  // To explain this shifted start min, one must use the AddEnergyAfterReason().
  IntegerValue ShiftedStartMin(int t) const {
    return cached_shifted_start_min_[t];
  }

  bool StartIsFixed(int t) const;
  bool EndIsFixed(int t) const;
  bool SizeIsFixed(int t) const;

  // Returns true if the corresponding fact is known for sure. A normal task is
  // always present. For optional task for which the presence is still unknown,
  // both of these function will return false.
  bool IsOptional(int t) const;
  bool IsPresent(int t) const;
  bool IsAbsent(int t) const;

  // Returns a string with the current task bounds.
  std::string TaskDebugString(int t) const;

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
  void AddAbsenceReason(int t);
  void AddSizeMinReason(int t);
  void AddSizeMinReason(int t, IntegerValue lower_bound);
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
  ABSL_MUST_USE_RESULT bool IncreaseStartMin(int t, IntegerValue new_start_min);
  ABSL_MUST_USE_RESULT bool DecreaseEndMax(int t, IntegerValue new_start_max);
  ABSL_MUST_USE_RESULT bool PushTaskAbsence(int t);
  ABSL_MUST_USE_RESULT bool PushTaskPresence(int t);
  ABSL_MUST_USE_RESULT bool PushIntegerLiteral(IntegerLiteral lit);
  ABSL_MUST_USE_RESULT bool ReportConflict();
  ABSL_MUST_USE_RESULT bool PushIntegerLiteralIfTaskPresent(int t,
                                                            IntegerLiteral lit);

  // Returns the underlying affine expressions.
  const std::vector<AffineExpression>& Starts() const { return starts_; }
  const std::vector<AffineExpression>& Ends() const { return ends_; }
  const std::vector<AffineExpression>& Sizes() const { return sizes_; }
  Literal PresenceLiteral(int index) const {
    DCHECK(IsOptional(index));
    return Literal(reason_for_presence_[index]);
  }

  // Registers the given propagator id to be called if any of the tasks
  // in this class change. Note that we do not watch size max though.
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

  // TODO(user): Change the propagation loop code so that we don't stop
  // pushing in the middle of the propagation as more advanced propagator do
  // not handle this correctly.
  bool InPropagationLoop() const { return integer_trail_->InPropagationLoop(); }

 private:
  void InitSortedVectors();
  void UpdateCachedValues(int t);

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
  std::vector<AffineExpression> starts_;
  std::vector<AffineExpression> ends_;
  std::vector<AffineExpression> sizes_;
  std::vector<LiteralIndex> reason_for_presence_;

  // The negation of the start/end variable so that SetTimeDirection()
  // can do its job in O(1) instead of calling NegationOf() on each entry.
  std::vector<AffineExpression> minus_starts_;
  std::vector<AffineExpression> minus_ends_;

  // This is used by SetLevel() to dected untrail.
  int previous_level_ = 0;

  // The caches of all relevant interval values.
  std::vector<IntegerValue> cached_duration_min_;
  std::vector<IntegerValue> cached_start_min_;
  std::vector<IntegerValue> cached_end_min_;
  std::vector<IntegerValue> cached_negated_start_max_;
  std::vector<IntegerValue> cached_negated_end_max_;
  std::vector<IntegerValue> cached_shifted_start_min_;
  std::vector<IntegerValue> cached_negated_shifted_end_max_;

  // Sorted vectors returned by the TasksBy*() functions.
  std::vector<TaskTime> task_by_increasing_start_min_;
  std::vector<TaskTime> task_by_increasing_end_min_;
  std::vector<TaskTime> task_by_decreasing_start_max_;
  std::vector<TaskTime> task_by_decreasing_end_max_;

  // This one is the most commonly used, so we optimized a bit more its
  // computation by detecting when there is nothing to do.
  std::vector<TaskTime> task_by_increasing_shifted_start_min_;
  std::vector<TaskTime> task_by_negated_shifted_end_max_;
  bool recompute_shifted_start_min_ = true;
  bool recompute_negated_shifted_end_max_ = true;

  // If recompute_cache_[t] is true, then we need to update all the cached
  // value for the task t in SynchronizeAndSetTimeDirection().
  bool recompute_all_cache_ = true;
  std::vector<bool> recompute_cache_;

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

inline bool SchedulingConstraintHelper::StartIsFixed(int t) const {
  return integer_trail_->IsFixed(starts_[t]);
}

inline bool SchedulingConstraintHelper::EndIsFixed(int t) const {
  return integer_trail_->IsFixed(ends_[t]);
}

inline bool SchedulingConstraintHelper::SizeIsFixed(int t) const {
  return integer_trail_->IsFixed(sizes_[t]);
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
  DCHECK(IsPresent(t));
  AddOtherReason(t);
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
  }
}

inline void SchedulingConstraintHelper::AddAbsenceReason(int t) {
  DCHECK(IsAbsent(t));
  AddOtherReason(t);
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]));
  }
}

inline void SchedulingConstraintHelper::AddSizeMinReason(int t) {
  AddOtherReason(t);
  if (sizes_[t].var != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(sizes_[t].var));
  }
}

inline void SchedulingConstraintHelper::AddSizeMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);
  if (sizes_[t].var != kNoIntegerVariable) {
    integer_reason_.push_back(sizes_[t].GreaterOrEqual(lower_bound));
  }
}

inline void SchedulingConstraintHelper::AddStartMinReason(
    int t, IntegerValue lower_bound) {
  DCHECK_GE(StartMin(t), lower_bound);
  AddOtherReason(t);
  if (starts_[t].var != kNoIntegerVariable) {
    integer_reason_.push_back(starts_[t].GreaterOrEqual(lower_bound));
  }
}

inline void SchedulingConstraintHelper::AddStartMaxReason(
    int t, IntegerValue upper_bound) {
  AddOtherReason(t);

  // Note that we cannot use the cache here!
  if (integer_trail_->UpperBound(starts_[t]) <= upper_bound) {
    if (starts_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(starts_[t].LowerOrEqual(upper_bound));
    }
  } else {
    // This might happen if we used StartMax() <= EndMax() - SizeMin().
    if (sizes_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(sizes_[t].var));
    }
    if (ends_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(
          ends_[t].LowerOrEqual(upper_bound + SizeMin(t)));
    }
  }
}

inline void SchedulingConstraintHelper::AddEndMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);

  // Note that we cannot use the cache here!
  if (integer_trail_->LowerBound(ends_[t]) >= lower_bound) {
    if (ends_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(ends_[t].GreaterOrEqual(lower_bound));
    }
  } else {
    // This might happen if we used EndMin() >= StartMin() + SizeMin().
    if (sizes_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(
          integer_trail_->LowerBoundAsLiteral(sizes_[t].var));
    }
    if (starts_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(
          starts_[t].GreaterOrEqual(lower_bound - SizeMin(t)));
    }
  }
}

inline void SchedulingConstraintHelper::AddEndMaxReason(
    int t, IntegerValue upper_bound) {
  DCHECK_LE(EndMax(t), upper_bound);
  AddOtherReason(t);
  if (ends_[t].var != kNoIntegerVariable) {
    integer_reason_.push_back(ends_[t].LowerOrEqual(upper_bound));
  }
}

inline void SchedulingConstraintHelper::AddEnergyAfterReason(
    int t, IntegerValue energy_min, IntegerValue time) {
  AddOtherReason(t);
  if (StartMin(t) >= time) {
    if (starts_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(starts_[t].GreaterOrEqual(time));
    }
  } else {
    if (ends_[t].var != kNoIntegerVariable) {
      integer_reason_.push_back(ends_[t].GreaterOrEqual(time + energy_min));
    }
  }
  if (sizes_[t].var != kNoIntegerVariable) {
    integer_reason_.push_back(sizes_[t].GreaterOrEqual(energy_min));
  }
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
    return model.Get<IntervalsRepository>()->PresenceLiteral(v);
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
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* intervals = model->GetOrCreate<IntervalsRepository>();

    std::vector<Literal> presences;
    std::vector<IntegerValue> sizes;

    // Create an "exactly one executed" constraint on the alternatives.
    std::vector<LiteralWithCoeff> sat_ct;
    for (const IntervalVariable member : members) {
      CHECK(intervals->IsOptional(member));
      const Literal is_present = intervals->PresenceLiteral(member);
      sat_ct.push_back({is_present, Coefficient(1)});
      model->Add(
          Equality(model->Get(StartVar(master)), model->Get(StartVar(member))));
      model->Add(
          Equality(model->Get(EndVar(master)), model->Get(EndVar(member))));

      // TODO(user): IsOneOf() only work for members with fixed size.
      // Generalize to an "int_var_element" constraint.
      CHECK(integer_trail->IsFixed(intervals->Size(member)));
      presences.push_back(is_present);
      sizes.push_back(intervals->MinSize(member));
    }
    if (intervals->SizeVar(master) != kNoIntegerVariable) {
      model->Add(IsOneOf(intervals->SizeVar(master), presences, sizes));
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
