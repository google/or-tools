// Copyright 2010-2024 Google LLC
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

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

DEFINE_STRONG_INDEX_TYPE(IntervalVariable);
const IntervalVariable kNoIntervalVariable(-1);

class SchedulingConstraintHelper;
class SchedulingDemandHelper;

// This class maintains a set of intervals which correspond to three integer
// variables (start, end and size). It automatically registers with the
// PrecedencesPropagator the relation between the bounds of each interval and
// provides many helper functions to add precedences relation between intervals.
class IntervalsRepository {
 public:
  explicit IntervalsRepository(Model* model)
      : model_(model),
        assignment_(model->GetOrCreate<Trail>()->Assignment()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        implications_(model->GetOrCreate<BinaryImplicationGraph>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

  // This type is neither copyable nor movable.
  IntervalsRepository(const IntervalsRepository&) = delete;
  IntervalsRepository& operator=(const IntervalsRepository&) = delete;

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
                                  LiteralIndex is_present = kNoLiteralIndex,
                                  bool add_linear_relation = false);

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
  // assuming the interval is present. Because of that, these variables can
  // cross each other or have an empty domain. If any of this happen, then the
  // PresenceLiteral() of this interval will be propagated to false.
  AffineExpression Size(IntervalVariable i) const { return sizes_[i]; }
  AffineExpression Start(IntervalVariable i) const { return starts_[i]; }
  AffineExpression End(IntervalVariable i) const { return ends_[i]; }

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

  // Returns a SchedulingConstraintHelper corresponding to the given variables.
  // Note that the order of interval in the helper will be the same.
  //
  // It is possible to indicate that this correspond to a disjunctive constraint
  // by setting the Boolean to true. This is used by our scheduling heuristic
  // based on precedences.
  SchedulingConstraintHelper* GetOrCreateHelper(
      const std::vector<IntervalVariable>& variables,
      bool register_as_disjunctive_helper = false);

  // Returns a SchedulingDemandHelper corresponding to the given helper and
  // demands. Note that the order of interval in the helper and the order of
  // demands must be the compatible.
  SchedulingDemandHelper* GetOrCreateDemandHelper(
      SchedulingConstraintHelper* helper,
      absl::Span<const AffineExpression> demands);

  // Calls InitDecomposedEnergies on all SchedulingDemandHelper created.
  void InitAllDecomposedEnergies();

  // Assuming a and b cannot overlap if they are present, this create a new
  // literal such that:
  // - literal & presences => a is before b.
  // - not(literal) & presences => b is before a.
  // - not present => literal @ true for disallowing multiple solutions.
  //
  // If such literal already exists this returns it.
  void CreateDisjunctivePrecedenceLiteral(IntervalVariable a,
                                          IntervalVariable b);

  // Creates a literal l <=> start_b >= end_a.
  // Returns true if such literal is "non-trivial" and was created.
  // Note that this ignore the optionality of a or b, it just creates a literal
  // comparing the two affine expression.
  bool CreatePrecedenceLiteral(IntervalVariable a, IntervalVariable b);

  // Returns a literal l <=> start_b >= end_a if it exist or kNoLiteralIndex
  // otherwise. This could be the one created by
  // CreateDisjunctivePrecedenceLiteral() or CreatePrecedenceLiteral().
  LiteralIndex GetPrecedenceLiteral(IntervalVariable a,
                                    IntervalVariable b) const;

  const std::vector<SchedulingConstraintHelper*>& AllDisjunctiveHelpers()
      const {
    return disjunctive_helpers_;
  }

  // We register cumulative at load time so that our search heuristic can loop
  // over all cumulative constraints easily.
  struct CumulativeHelper {
    AffineExpression capacity;
    SchedulingConstraintHelper* task_helper;
    SchedulingDemandHelper* demand_helper;
  };
  void RegisterCumulative(CumulativeHelper helper) {
    cumulative_helpers_.push_back(helper);
  }
  const std::vector<CumulativeHelper>& AllCumulativeHelpers() const {
    return cumulative_helpers_;
  }

 private:
  // External classes needed.
  Model* model_;
  const VariablesAssignment& assignment_;
  SatSolver* sat_solver_;
  BinaryImplicationGraph* implications_;
  IntegerTrail* integer_trail_;

  // Literal indicating if the tasks is executed. Tasks that are always executed
  // will have a kNoLiteralIndex entry in this vector.
  absl::StrongVector<IntervalVariable, LiteralIndex> is_present_;

  // The integer variables for each tasks.
  absl::StrongVector<IntervalVariable, AffineExpression> starts_;
  absl::StrongVector<IntervalVariable, AffineExpression> ends_;
  absl::StrongVector<IntervalVariable, AffineExpression> sizes_;

  // We can share the helper for all the propagators that work on the same set
  // of intervals.
  absl::flat_hash_map<std::vector<IntervalVariable>,
                      SchedulingConstraintHelper*>
      helper_repository_;
  absl::flat_hash_map<
      std::pair<SchedulingConstraintHelper*, std::vector<AffineExpression>>,
      SchedulingDemandHelper*>
      demand_helper_repository_;

  // Disjunctive and normal precedences.
  //
  // Note that for normal precedences, we use directly the affine expression so
  // that if many intervals share the same start, we don't re-create Booleans
  // for no reason.
  absl::flat_hash_map<std::pair<IntervalVariable, IntervalVariable>, Literal>
      disjunctive_precedences_;
  absl::flat_hash_map<std::pair<AffineExpression, AffineExpression>, Literal>
      precedences_;

  // Disjunctive/Cumulative helpers_.
  std::vector<SchedulingConstraintHelper*> disjunctive_helpers_;
  std::vector<CumulativeHelper> cumulative_helpers_;
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
  ABSL_MUST_USE_RESULT bool ResetFromSubset(
      const SchedulingConstraintHelper& other, absl::Span<const int> tasks);

  // Returns the number of task.
  int NumTasks() const { return starts_.size(); }

  // Make sure the cached values are up to date. Also sets the time direction to
  // either forward/backward. This will impact all the functions below. This
  // MUST be called at the beginning of all Propagate() call that uses this
  // helper.
  void SetTimeDirection(bool is_forward);
  ABSL_MUST_USE_RESULT bool SynchronizeAndSetTimeDirection(bool is_forward);

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
  IntegerValue SizeMin(int t) const { return cached_size_min_[t]; }
  IntegerValue SizeMax(int t) const {
    // This one is "rare" so we don't cache it.
    return integer_trail_->UpperBound(sizes_[t]);
  }
  IntegerValue StartMin(int t) const { return cached_start_min_[t]; }
  IntegerValue EndMin(int t) const { return cached_end_min_[t]; }
  IntegerValue StartMax(int t) const { return -cached_negated_start_max_[t]; }
  IntegerValue EndMax(int t) const { return -cached_negated_end_max_[t]; }

  IntegerValue LevelZeroStartMin(int t) const {
    return integer_trail_->LevelZeroLowerBound(starts_[t]);
  }
  IntegerValue LevelZeroStartMax(int t) const {
    return integer_trail_->LevelZeroUpperBound(starts_[t]);
  }

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

  // As with ShiftedStartMin(), we can compute the shifted end max (that is
  // start_max + size_min.
  IntegerValue ShiftedEndMax(int t) const {
    return -cached_negated_shifted_end_max_[t];
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

  // Return a value so that End(a) + dist <= Start(b).
  // Returns kMinInterValue if we don't have any such relation.
  IntegerValue GetCurrentMinDistanceBetweenTasks(
      int a, int b, bool add_reason_if_after = false);

  // We detected a precedence between two tasks.
  // If we are at level zero, we might want to add the constraint.
  // If we are at positive level, we might want to propagate the associated
  // precedence literal if it exists.
  bool PropagatePrecedence(int a, int b);

  // Return the minimum overlap of interval i with the time window [start..end].
  //
  // Note: this is different from the mandatory part of an interval.
  IntegerValue GetMinOverlap(int t, IntegerValue start, IntegerValue end) const;

  // Returns a string with the current task bounds.
  std::string TaskDebugString(int t) const;

  // Sorts and returns the tasks in corresponding order at the time of the call.
  // Note that we do not mean strictly-increasing/strictly-decreasing, there
  // will be duplicate time values in these vectors.
  //
  // TODO(user): we could merge the first loop of IncrementalSort() with the
  // loop that fill TaskTime.time at each call.
  absl::Span<const TaskTime> TaskByIncreasingStartMin();
  absl::Span<const TaskTime> TaskByIncreasingEndMin();
  absl::Span<const TaskTime> TaskByDecreasingStartMax();
  absl::Span<const TaskTime> TaskByDecreasingEndMax();
  absl::Span<const TaskTime> TaskByIncreasingShiftedStartMin();

  // Returns a sorted vector where each task appear twice, the first occurrence
  // is at size (end_min - size_min) and the second one at (end_min).
  //
  // This is quite usage specific.
  struct ProfileEvent {
    IntegerValue time;
    int task;
    bool is_first;

    bool operator<(const ProfileEvent& other) const {
      if (time == other.time) {
        if (task == other.task) return is_first > other.is_first;
        return task < other.task;
      }
      return time < other.time;
    }
  };
  const std::vector<ProfileEvent>& GetEnergyProfile();

  // Functions to clear and then set the current reason.
  void ClearReason();
  void AddPresenceReason(int t);
  void AddAbsenceReason(int t);
  void AddSizeMinReason(int t);
  void AddSizeMinReason(int t, IntegerValue lower_bound);
  void AddSizeMaxReason(int t, IntegerValue upper_bound);
  void AddStartMinReason(int t, IntegerValue lower_bound);
  void AddStartMaxReason(int t, IntegerValue upper_bound);
  void AddEndMinReason(int t, IntegerValue lower_bound);
  void AddEndMaxReason(int t, IntegerValue upper_bound);

  void AddEnergyAfterReason(int t, IntegerValue energy_min, IntegerValue time);
  void AddEnergyMinInIntervalReason(int t, IntegerValue min, IntegerValue max);

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
  // conditioned on its presence. The functions will do the correct thing
  // depending on whether or not the start_min/end_max are optional variables
  // whose presence implies the interval presence.
  ABSL_MUST_USE_RESULT bool IncreaseStartMin(int t, IntegerValue value);
  ABSL_MUST_USE_RESULT bool IncreaseEndMin(int t, IntegerValue value);
  ABSL_MUST_USE_RESULT bool DecreaseEndMax(int t, IntegerValue value);
  ABSL_MUST_USE_RESULT bool PushLiteral(Literal l);
  ABSL_MUST_USE_RESULT bool PushTaskAbsence(int t);
  ABSL_MUST_USE_RESULT bool PushTaskPresence(int t);
  ABSL_MUST_USE_RESULT bool PushIntegerLiteral(IntegerLiteral lit);
  ABSL_MUST_USE_RESULT bool ReportConflict();
  ABSL_MUST_USE_RESULT bool PushIntegerLiteralIfTaskPresent(int t,
                                                            IntegerLiteral lit);

  // Returns the underlying affine expressions.
  absl::Span<const IntervalVariable> IntervalVariables() const {
    return interval_variables_;
  }
  absl::Span<const AffineExpression> Starts() const { return starts_; }
  absl::Span<const AffineExpression> Ends() const { return ends_; }
  absl::Span<const AffineExpression> Sizes() const { return sizes_; }

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
                      absl::Span<const int> map_to_other_helper,
                      IntegerValue event) {
    CHECK(other_helper != nullptr);
    other_helper_ = other_helper;
    map_to_other_helper_ = map_to_other_helper;
    event_for_other_helper_ = event;
  }

  bool HasOtherHelper() const { return other_helper_ != nullptr; }

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

  int CurrentDecisionLevel() const { return trail_->CurrentDecisionLevel(); }

 private:
  // Generic reason for a <= upper_bound, given that a = b + c in case the
  // current upper bound of a is not good enough.
  void AddGenericReason(const AffineExpression& a, IntegerValue upper_bound,
                        const AffineExpression& b, const AffineExpression& c);

  void InitSortedVectors();
  ABSL_MUST_USE_RESULT bool UpdateCachedValues(int t);

  // Internal function for IncreaseStartMin()/DecreaseEndMax().
  bool PushIntervalBound(int t, IntegerLiteral lit);

  // This will be called on any interval that is part of a reason or
  // a bound push. Since the last call to ClearReason(), for each unique
  // t, we will add once to other_helper_ the reason for t containing
  // the point event_for_other_helper_.
  void AddOtherReason(int t);

  // Import the reasons on the other helper into this helper.
  void ImportOtherReasons();

  Model* model_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  PrecedenceRelations* precedence_relations_;

  // The current direction of time, true for forward, false for backward.
  bool current_time_direction_ = true;

  // All the underlying variables of the tasks.
  // The vectors are indexed by the task index t.
  std::vector<IntervalVariable> interval_variables_;
  std::vector<AffineExpression> starts_;
  std::vector<AffineExpression> ends_;
  std::vector<AffineExpression> sizes_;
  std::vector<LiteralIndex> reason_for_presence_;

  // The negation of the start/end variable so that SetTimeDirection()
  // can do its job in O(1) instead of calling NegationOf() on each entry.
  std::vector<AffineExpression> minus_starts_;
  std::vector<AffineExpression> minus_ends_;

  // This is used by SetLevel() to detect untrail.
  int previous_level_ = 0;

  // The caches of all relevant interval values.
  // These are initially of size capacity and never resized.
  //
  // TODO(user): Because of std::swap() in SetTimeDirection, we cannot mark
  // most of them as "const" and as a result we loose some performance since
  // the address need to be re-fetched on most access.
  const int capacity_;
  const std::unique_ptr<IntegerValue[]> cached_size_min_;
  std::unique_ptr<IntegerValue[]> cached_start_min_;
  std::unique_ptr<IntegerValue[]> cached_end_min_;
  std::unique_ptr<IntegerValue[]> cached_negated_start_max_;
  std::unique_ptr<IntegerValue[]> cached_negated_end_max_;
  std::unique_ptr<IntegerValue[]> cached_shifted_start_min_;
  std::unique_ptr<IntegerValue[]> cached_negated_shifted_end_max_;

  // Sorted vectors returned by the TasksBy*() functions.
  std::vector<TaskTime> task_by_increasing_start_min_;
  std::vector<TaskTime> task_by_increasing_end_min_;
  std::vector<TaskTime> task_by_decreasing_start_max_;
  std::vector<TaskTime> task_by_decreasing_end_max_;

  // Sorted vector returned by GetEnergyProfile().
  bool recompute_energy_profile_ = true;
  std::vector<ProfileEvent> energy_profile_;

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

  // Optional 'proxy' helper used in the diffn constraint.
  SchedulingConstraintHelper* other_helper_ = nullptr;
  absl::Span<const int> map_to_other_helper_;
  IntegerValue event_for_other_helper_;
  std::vector<bool> already_added_to_other_reasons_;
};

// Helper class for cumulative constraint to wrap demands and expose concept
// like energy.
//
// In a cumulative constraint, an interval always has a size and a demand, but
// it can also have a set of "selector" literals each associated with a fixed
// size / fixed demands. This allows more precise energy estimation.
//
// TODO(user): Cache energy min and reason for the non O(1) cases.
class SchedulingDemandHelper {
 public:
  // Hack: this can be called with and empty demand vector as long as
  // OverrideEnergies() is called to define the energies.
  SchedulingDemandHelper(absl::Span<const AffineExpression> demands,
                         SchedulingConstraintHelper* helper, Model* model);

  // When defined, the interval will consume this much demand during its whole
  // duration. Some propagator only relies on the "energy" and thus never uses
  // this.
  IntegerValue DemandMin(int t) const;
  IntegerValue DemandMax(int t) const;
  bool DemandIsFixed(int t) const;
  void AddDemandMinReason(int t);
  const std::vector<AffineExpression>& Demands() const { return demands_; }

  // Adds the linearized demand (either the affine demand expression, or the
  // demand part of the decomposed energy if present) to the builder.
  // It returns false and do not add any term to the builder.if any literal
  // involved has no integer view.
  ABSL_MUST_USE_RESULT bool AddLinearizedDemand(
      int t, LinearConstraintBuilder* builder) const;

  // The "energy" is usually size * demand, but in some non-conventional usage
  // it might have a more complex formula. In all case, the energy is assumed
  // to be only consumed during the interval duration.
  //
  // IMPORTANT: One must call CacheAllEnergyValues() for the values to be
  // updated. TODO(user): this is error prone, maybe we should revisit. But if
  // there is many alternatives, we don't want to rescan the list more than a
  // linear number of time per propagation.
  //
  // TODO(user): Add more complex EnergyMinBefore(time) once we also support
  // expressing the interval as a set of alternatives.
  //
  // At level 0, it will filter false literals from decomposed energies.
  void CacheAllEnergyValues();
  IntegerValue EnergyMin(int t) const { return cached_energies_min_[t]; }
  IntegerValue EnergyMax(int t) const { return cached_energies_max_[t]; }
  bool EnergyIsQuadratic(int t) const { return energy_is_quadratic_[t]; }
  void AddEnergyMinReason(int t);

  // Returns the energy min in [start, end].
  //
  // Note(user): These functions are not in O(1) if the decomposition is used,
  // so we have to be careful in not calling them too often.
  IntegerValue EnergyMinInWindow(int t, IntegerValue window_start,
                                 IntegerValue window_end);
  void AddEnergyMinInWindowReason(int t, IntegerValue window_start,
                                  IntegerValue window_end);

  // Important: This might not do anything depending on the representation of
  // the energy we have.
  ABSL_MUST_USE_RESULT bool DecreaseEnergyMax(int t, IntegerValue value);

  // Different optional representation of the energy of an interval.
  //
  // Important: first value is size, second value is demand.
  const std::vector<std::vector<LiteralValueValue>>& DecomposedEnergies()
      const {
    return decomposed_energies_;
  }

  // Visible for testing.
  void OverrideLinearizedEnergies(absl::Span<const LinearExpression> energies);
  void OverrideDecomposedEnergies(
      const std::vector<std::vector<LiteralValueValue>>& energies);
  // Returns the decomposed energy terms compatible with the current literal
  // assignment. It must not be used to create reasons if not at level 0.
  // It returns en empty vector if the decomposed energy is not available.
  //
  // Important: first value is size, second value is demand.
  std::vector<LiteralValueValue> FilteredDecomposedEnergy(int index);

  // Init all decomposed energies. It needs probing to be finished. This happens
  // after the creation of the helper.
  void InitDecomposedEnergies();

 private:
  IntegerValue SimpleEnergyMin(int t) const;
  IntegerValue LinearEnergyMin(int t) const;
  IntegerValue SimpleEnergyMax(int t) const;
  IntegerValue LinearEnergyMax(int t) const;
  IntegerValue DecomposedEnergyMin(int t) const;
  IntegerValue DecomposedEnergyMax(int t) const;

  IntegerTrail* integer_trail_;
  ProductDecomposer* product_decomposer_;
  SatSolver* sat_solver_;  // To get the current propagation level.
  const VariablesAssignment& assignment_;
  std::vector<AffineExpression> demands_;
  SchedulingConstraintHelper* helper_;

  // Cached value of the energies, as it can be a bit costly to compute.
  std::vector<IntegerValue> cached_energies_min_;
  std::vector<IntegerValue> cached_energies_max_;
  std::vector<bool> energy_is_quadratic_;

  // A representation of the energies as a set of alternative.
  // If subvector is empty, we don't have this representation.
  std::vector<std::vector<LiteralValueValue>> decomposed_energies_;

  // A representation of the energies as a set of linear expression.
  // If the optional is not set, we don't have this representation.
  std::vector<std::optional<LinearExpression>> linearized_energies_;
};

// =============================================================================
// Utilities
// =============================================================================

IntegerValue ComputeEnergyMinInWindow(
    IntegerValue start_min, IntegerValue start_max, IntegerValue end_min,
    IntegerValue end_max, IntegerValue size_min, IntegerValue demand_min,
    absl::Span<const LiteralValueValue> filtered_energy,
    IntegerValue window_start, IntegerValue window_end);

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
  AddSizeMinReason(t, SizeMin(t));
}

inline void SchedulingConstraintHelper::AddGenericReason(
    const AffineExpression& a, IntegerValue upper_bound,
    const AffineExpression& b, const AffineExpression& c) {
  if (integer_trail_->UpperBound(a) <= upper_bound) {
    if (a.var != kNoIntegerVariable) {
      integer_reason_.push_back(a.LowerOrEqual(upper_bound));
    }
    return;
  }
  CHECK_NE(a.var, kNoIntegerVariable);

  // Here we assume that the upper_bound on a comes from the bound on b + c.
  const IntegerValue slack = upper_bound - integer_trail_->UpperBound(b) -
                             integer_trail_->UpperBound(c);
  CHECK_GE(slack, 0);
  if (b.var == kNoIntegerVariable && c.var == kNoIntegerVariable) return;
  if (b.var == kNoIntegerVariable) {
    integer_reason_.push_back(c.LowerOrEqual(upper_bound - b.constant));
  } else if (c.var == kNoIntegerVariable) {
    integer_reason_.push_back(b.LowerOrEqual(upper_bound - c.constant));
  } else {
    integer_trail_->AppendRelaxedLinearReason(
        slack, {b.coeff, c.coeff}, {NegationOf(b.var), NegationOf(c.var)},
        &integer_reason_);
  }
}

inline void SchedulingConstraintHelper::AddSizeMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  if (lower_bound <= 0) return;
  AddGenericReason(sizes_[t].Negated(), -lower_bound, minus_ends_[t],
                   starts_[t]);
}

inline void SchedulingConstraintHelper::AddSizeMaxReason(
    int t, IntegerValue upper_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(sizes_[t], upper_bound, ends_[t], minus_starts_[t]);
}

inline void SchedulingConstraintHelper::AddStartMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(minus_starts_[t], -lower_bound, minus_ends_[t], sizes_[t]);
}

inline void SchedulingConstraintHelper::AddStartMaxReason(
    int t, IntegerValue upper_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(starts_[t], upper_bound, ends_[t], sizes_[t].Negated());
}

inline void SchedulingConstraintHelper::AddEndMinReason(
    int t, IntegerValue lower_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(minus_ends_[t], -lower_bound, minus_starts_[t],
                   sizes_[t].Negated());
}

inline void SchedulingConstraintHelper::AddEndMaxReason(
    int t, IntegerValue upper_bound) {
  AddOtherReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(ends_[t], upper_bound, starts_[t], sizes_[t]);
}

inline void SchedulingConstraintHelper::AddEnergyAfterReason(
    int t, IntegerValue energy_min, IntegerValue time) {
  if (StartMin(t) >= time) {
    AddStartMinReason(t, time);
  } else {
    AddEndMinReason(t, time + energy_min);
  }
  AddSizeMinReason(t, energy_min);
}

inline void SchedulingConstraintHelper::AddEnergyMinInIntervalReason(
    int t, IntegerValue time_min, IntegerValue time_max) {
  const IntegerValue energy_min = SizeMin(t);
  CHECK_LE(time_min + energy_min, time_max);
  if (StartMin(t) >= time_min) {
    AddStartMinReason(t, time_min);
  } else {
    AddEndMinReason(t, time_min + energy_min);
  }
  if (EndMax(t) <= time_max) {
    AddEndMaxReason(t, time_max);
  } else {
    AddStartMaxReason(t, time_max - energy_min);
  }
  AddSizeMinReason(t, energy_min);
}

// =============================================================================
// Model based functions.
// =============================================================================

inline std::function<int64_t(const Model&)> MinSize(IntervalVariable v) {
  return [=](const Model& model) {
    return model.Get<IntervalsRepository>()->MinSize(v).value();
  };
}

inline std::function<int64_t(const Model&)> MaxSize(IntervalVariable v) {
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

inline std::function<IntervalVariable(Model*)> NewInterval(int64_t min_start,
                                                           int64_t max_end,
                                                           int64_t size) {
  return [=](Model* model) {
    CHECK_LE(min_start + size, max_end);
    const IntegerVariable start =
        model->Add(NewIntegerVariable(min_start, max_end - size));
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        AffineExpression(start),
        AffineExpression(start, IntegerValue(1), IntegerValue(size)),
        AffineExpression(IntegerValue(size)), kNoLiteralIndex,
        /*add_linear_relation=*/false);
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
    int64_t min_start, int64_t max_end, int64_t min_size, int64_t max_size) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        kNoLiteralIndex);
  };
}

// Note that this should only be used in tests.
inline std::function<IntervalVariable(Model*)> NewOptionalInterval(
    int64_t min_start, int64_t max_end, int64_t size, Literal is_present) {
  return [=](Model* model) {
    CHECK_LE(min_start + size, max_end);
    const IntegerVariable start =
        model->Add(NewIntegerVariable(min_start, max_end - size));
    const IntervalVariable interval =
        model->GetOrCreate<IntervalsRepository>()->CreateInterval(
            AffineExpression(start),
            AffineExpression(start, IntegerValue(1), IntegerValue(size)),
            AffineExpression(IntegerValue(size)), is_present.Index(),
            /*add_linear_relation=*/false);

    // To not have too many solutions during enumeration, we force the
    // start at its min value for absent interval.
    model->Add(Implication({is_present.Negated()},
                           IntegerLiteral::LowerOrEqual(start, min_start)));
    return interval;
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
NewOptionalIntervalWithVariableSize(int64_t min_start, int64_t max_end,
                                    int64_t min_size, int64_t max_size,
                                    Literal is_present) {
  return [=](Model* model) {
    return model->GetOrCreate<IntervalsRepository>()->CreateInterval(
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_start, max_end)),
        model->Add(NewIntegerVariable(min_size, max_size)), IntegerValue(0),
        is_present.Index());
  };
}

// Cuts helpers.
void AddIntegerVariableFromIntervals(SchedulingConstraintHelper* helper,
                                     Model* model,
                                     std::vector<IntegerVariable>* vars);

void AppendVariablesFromCapacityAndDemands(
    const AffineExpression& capacity, SchedulingDemandHelper* demands_helper,
    Model* model, std::vector<IntegerVariable>* vars);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTERVALS_H_
