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

#ifndef OR_TOOLS_SAT_SCHEDULING_HELPERS_H_
#define OR_TOOLS_SAT_SCHEDULING_HELPERS_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/enforcement_helper.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// An helper struct to sort task by time. This is used by the
// SchedulingConstraintHelper but also by many scheduling propagators to sort
// tasks.
struct TaskTime {
  int task_index;
  IntegerValue time;
  bool operator<(TaskTime other) const { return time < other.time; }
  bool operator>(TaskTime other) const { return time > other.time; }
};

// We have some free space in TaskTime.
// We stick the presence_lit to save an indirection in some algo.
//
// TODO(user): Experiment caching more value. In particular
// TaskByIncreasingShiftedStartMin() could tie break task for better heuristics?
struct CachedTaskBounds {
  int task_index;
  LiteralIndex presence_lit;
  IntegerValue time;
  bool operator<(CachedTaskBounds other) const { return time < other.time; }
  bool operator>(CachedTaskBounds other) const { return time > other.time; }
};

struct IntervalDefinition {
  AffineExpression start;
  AffineExpression end;
  AffineExpression size;
  std::optional<Literal> is_present;

  template <typename H>
  friend H AbslHashValue(H h, const IntervalDefinition& i) {
    return H::combine(std::move(h), i.start, i.end, i.size, i.is_present);
  }

  bool operator==(const IntervalDefinition& other) const {
    return start == other.start && end == other.end && size == other.size &&
           is_present == other.is_present;
  }
};

// Helper class shared by the propagators that manage a given list of tasks.
//
// One of the main advantage of this class is that it allows to share the
// vectors of tasks sorted by various criteria between propagator for a faster
// code. It is also helpful to allow in-processing: the intervals that are
// handled by this class are not necessarily the same as the ones in the model.
class SchedulingConstraintHelper : public PropagatorInterface {
 public:
  // All the functions below refer to a task by its index t in the tasks
  // vector given at construction.
  SchedulingConstraintHelper(std::vector<AffineExpression> starts,
                             std::vector<AffineExpression> ends,
                             std::vector<AffineExpression> sizes,
                             std::vector<LiteralIndex> reason_for_presence,
                             Model* model);

  // Temporary constructor.
  // The class will not be usable until ResetFromSubset() is called.
  //
  // TODO(user): Remove this. It is a hack because the disjunctive class needs
  // to fetch the maximum possible number of task at construction.
  SchedulingConstraintHelper(int num_tasks, Model* model);

  // Returns true if and only if all the enforcement literals are true.
  bool IsEnforced() const;

  // This is a propagator so we can "cache" all the intervals relevant
  // information. This gives good speedup. Note however that the info is stale
  // except if a bound was pushed by this helper or if this was called. We run
  // it at the highest priority, so that will mostly be the case at the
  // beginning of each Propagate() call of the classes using this.
  bool Propagate() final;
  bool IncrementalPropagate(const std::vector<int>& watch_indices) final;
  void RegisterWith(GenericLiteralWatcher* watcher,
                    absl::Span<const Literal> enforcement_literals);

  // Sets the enforcement ID without registering this propagator with a watcher.
  // This is used by NoOverlap2DConstraintHelper, which registers itself but
  // does not register its x and y SchedulingConstraintHelpers.
  void SetEnforcementId(EnforcementId id) { enforcement_id_ = id; }

  // Resets the class to the same state as if it was constructed with
  // the given subset of tasks from other (and the same enforcement literals).
  ABSL_MUST_USE_RESULT bool ResetFromSubset(
      const SchedulingConstraintHelper& other, absl::Span<const int> tasks);

  // Returns the number of task.
  int NumTasks() const { return starts_.size(); }

  // Make sure the cached values are up to date. Also sets the time direction to
  // either forward/backward. This will impact all the functions below. This
  // MUST be called at the beginning of all Propagate() call that uses this
  // helper.
  void SetTimeDirection(bool is_forward);
  bool CurrentTimeIsForward() const { return current_time_direction_; }
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

  IntegerValue LevelZeroSizeMin(int t) const {
    // Note the variable that encodes the size of an absent task can be
    // negative.
    return std::max(IntegerValue(0),
                    integer_trail_->LevelZeroLowerBound(sizes_[t]));
  }
  IntegerValue LevelZeroStartMin(int t) const {
    return integer_trail_->LevelZeroLowerBound(starts_[t]);
  }
  IntegerValue LevelZeroStartMax(int t) const {
    return integer_trail_->LevelZeroUpperBound(starts_[t]);
  }
  IntegerValue LevelZeroEndMax(int t) const {
    return integer_trail_->LevelZeroUpperBound(ends_[t]);
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

  // Same if one already have the presence LiteralIndex of a task.
  bool IsOptional(LiteralIndex lit) const;
  bool IsPresent(LiteralIndex lit) const;
  bool IsAbsent(LiteralIndex lit) const;

  // Returns a value so that End(a) + dist <= Start(b).
  //
  // TODO(user): we use this to optimize some reason, but ideally we only want
  // to use linear2 bounds here, not bounds coming from trivial bounds. Make
  // sure we have the best possible reason.
  IntegerValue GetCurrentMinDistanceBetweenTasks(int a, int b);

  // We detected a precedence between two tasks at level zero.
  // This register a new constraint and notify the linear2 root level bounds
  // repository. Returns false on conflict.
  //
  // TODO(user): We could also call this at positive decision level, but it is a
  // bit harder to exploit as we will also need to store the reasons.
  bool NotifyLevelZeroPrecedence(int a, int b);

  // Return the minimum overlap of task t with the time window [start..end].
  //
  // Note: this is different from the mandatory part of an interval.
  IntegerValue GetMinOverlap(int t, IntegerValue start, IntegerValue end) const;

  bool TaskIsBeforeOrIsOverlapping(int before, int after);

  // Returns a string with the current task bounds.
  std::string TaskDebugString(int t) const;

  // Sorts and returns the tasks in corresponding order at the time of the call.
  // Note that we do not mean strictly-increasing/strictly-decreasing, there
  // will be duplicate time values in these vectors.
  //
  // TODO(user): we could merge the first loop of IncrementalSort() with the
  // loop that fill TaskTime.time at each call.
  absl::Span<const TaskTime> TaskByIncreasingStartMin();
  absl::Span<const TaskTime> TaskByDecreasingEndMax();

  absl::Span<const TaskTime> TaskByIncreasingNegatedStartMax();
  absl::Span<const TaskTime> TaskByIncreasingEndMin();

  absl::Span<const CachedTaskBounds> TaskByIncreasingShiftedStartMin();
  absl::Span<const CachedTaskBounds> TaskByIncreasingNegatedShiftedEndMax();

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

  // Functions to reset and then set the current reason.
  void ResetReason();
  void ImportReasonsFromOther(const SchedulingConstraintHelper& other_helper);
  void AddPresenceReason(int t);
  void AddAbsenceReason(int t);
  void AddSizeMinReason(int t);
  void AddSizeMinReason(int t, IntegerValue lower_bound);
  void AddSizeMaxReason(int t, IntegerValue upper_bound);
  void AddStartMinReason(int t, IntegerValue lower_bound);
  void AddStartMaxReason(int t, IntegerValue upper_bound);
  void AddEndMinReason(int t, IntegerValue lower_bound);
  void AddEndMaxReason(int t, IntegerValue upper_bound);
  void AddShiftedEndMaxReason(int t, IntegerValue upper_bound);

  void AddEnergyAfterReason(int t, IntegerValue energy_min, IntegerValue time);
  void AddEnergyMinInIntervalReason(int t, IntegerValue min, IntegerValue max);

  void AddLiteralReason(Literal l);
  void AddIntegerReason(IntegerLiteral l);

  // Adds the reason why the task "before" must be before task "after", in
  // the sense that "after" can only start at the same time or later than the
  // task "before" ends.
  //
  // Important: this assumes that the two task cannot overlap. So we can have
  // a more relaxed reason than Start(after) >= Ends(before).
  //
  // There are actually many possibilities to explain such relation:
  // - StartMax(before) < EndMin(after).
  // - We have a linear2: Start(after) >= End(before) - SizeMin(before);
  // - etc...
  // We try to pick the best one.
  //
  // TODO(user): Refine the heuritic. Also consider other reason for the
  // complex cases where Start() and End() do not use the same integer variable.
  void AddReasonForBeingBeforeAssumingNoOverlap(int before, int after);

  void AddReasonForUpperBoundLowerThan(LinearExpression2 expr, IntegerValue ub);

  void AppendAndResetReason(std::vector<IntegerLiteral>* integer_reason,
                            std::vector<Literal>* literal_reason);

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

  // Push that t_before must end at the same time or before t_after starts.
  // This function does the correct thing if t_before or t_after are optional
  // and their presence is unknown. Returns false on conflict.
  ABSL_MUST_USE_RESULT bool PushTaskOrderWhenPresent(int t_before, int t_after);

  absl::Span<const AffineExpression> Starts() const { return starts_; }
  absl::Span<const AffineExpression> Ends() const { return ends_; }
  absl::Span<const AffineExpression> Sizes() const { return sizes_; }

  IntervalDefinition GetIntervalDefinition(int index) const {
    return IntervalDefinition{
        .start = starts_[index],
        .end = ends_[index],
        .size = sizes_[index],
        .is_present = (reason_for_presence_[index] == kNoLiteralIndex
                           ? std::optional<Literal>()
                           : Literal(reason_for_presence_[index]))};
  }

  Literal PresenceLiteral(int index) const {
    DCHECK(IsOptional(index));
    return Literal(reason_for_presence_[index]);
  }

  // Registers the given propagator id to be called if any of the tasks
  // in this class change. Note that we do not watch size max though.
  void WatchAllTasks(int id);

  // Sometimes, typically for no_overlap_2d, we can use the variables that are
  // fixed at current decision level to define a scheduling sub-problem. For
  // example, if all the x coordinates are fixed and the intervals overlap on
  // the x axis, we can just run the disjunction propagators on the y
  // coordinates.
  //
  // To be able to run scheduling propagators on a sub-problem, we can register
  // a callback that before any explanation or when one of the bounds of an item
  // is pushed. The callback will get the list of all items that participated in
  // the reason and/or the bound push and allows the caller to introduce any new
  // literals needed to make sure that the conditions making it a sub-problem
  // hold.
  void SetExtraExplanationForItemCallback(
      std::function<void(absl::Span<const int> items,
                         std::vector<Literal>* reason,
                         std::vector<IntegerLiteral>* integer_reason)>
          extra_explanation_callback) {
    used_items_for_reason_.ClearAndResize(
        extra_explanation_callback == nullptr ? 0 : capacity_);
    extra_explanation_callback_ = std::move(extra_explanation_callback);
  }

  // TODO(user): Change the propagation loop code so that we don't stop
  // pushing in the middle of the propagation as more advanced propagator do
  // not handle this correctly.
  bool InPropagationLoop() const { return integer_trail_->InPropagationLoop(); }

  int CurrentDecisionLevel() const {
    return sat_solver_->CurrentDecisionLevel();
  }

  // This increase as soon as we propagate something.
  int64_t PropagationTimestamp() const {
    return integer_trail_->timestamp() + trail_->NumberOfEnqueues();
  }

 private:
  // Tricky: when a task is optional, it is possible it size min is negative,
  // but we know that if a task is present, its size should be >= 0. So in the
  // reason, when we need the size_min and it is currently negative, we can just
  // ignore it and use zero instead.
  AffineExpression NegatedSizeOrZero(int t) {
    if (integer_trail_->LowerBound(sizes_[t]) <= 0) {
      return AffineExpression(0);
    }
    return sizes_[t].Negated();
  }

  // Generic reason for a <= upper_bound, given that a = b + c in case the
  // current upper bound of a is not good enough.
  void AddGenericReason(const AffineExpression& a, IntegerValue upper_bound,
                        const AffineExpression& b, const AffineExpression& c);

  void InitSortedVectors();
  ABSL_MUST_USE_RESULT bool UpdateCachedValues(int t);

  // Internal function for IncreaseStartMin()/DecreaseEndMax().
  bool PushIntervalBound(int t, IntegerLiteral lit);

  // This should be called every time a task that is part of a reason or a
  // bound push to update the items_in_reason_ vector.
  void FlagItemAsUsedInReason(int t);

  void RunCallbackIfSet();

  Model* model_;
  SatSolver* sat_solver_;
  const VariablesAssignment& assignment_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  GenericLiteralWatcher* watcher_;
  Linear2Bounds* linear2_bounds_;
  RootLevelLinear2Bounds* root_level_lin2_bounds_;
  EnforcementHelper& enforcement_helper_;
  EnforcementId enforcement_id_;

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

  // This is used to detect when we need to invalidate the cache.
  int64_t saved_num_backtracks_ = 0;

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
  std::vector<TaskTime> task_by_decreasing_end_max_;

  bool recompute_by_start_max_ = true;
  bool recompute_by_end_min_ = true;
  std::vector<TaskTime> task_by_increasing_negated_start_max_;
  std::vector<TaskTime> task_by_increasing_end_min_;

  // Sorted vector returned by GetEnergyProfile().
  bool recompute_energy_profile_ = true;
  std::vector<ProfileEvent> energy_profile_;

  // This one is the most commonly used, so we optimized a bit more its
  // computation by detecting when there is nothing to do.
  std::vector<CachedTaskBounds> task_by_increasing_shifted_start_min_;
  std::vector<CachedTaskBounds> task_by_negated_shifted_end_max_;
  bool recompute_shifted_start_min_ = true;
  bool recompute_negated_shifted_end_max_ = true;

  // If recompute_cache_[t] is true, then we need to update all the cached
  // value for the task t in SynchronizeAndSetTimeDirection().
  bool recompute_all_cache_ = true;
  Bitset64<int> recompute_cache_;

  // For large problems, LNS will have a lot of fixed intervals.
  // And fixed intervals will never changes, so there is no point recomputing
  // the cache for them.
  std::vector<int> non_fixed_intervals_;

  // Reason vectors.
  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  std::function<void(absl::Span<const int> items, std::vector<Literal>*,
                     std::vector<IntegerLiteral>*)>
      extra_explanation_callback_ = nullptr;

  SparseBitset<int> used_items_for_reason_;

  // List of watcher to "wake-up" each time one of the task bounds changes.
  std::vector<int> propagator_ids_;
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
  IntegerValue LevelZeroDemandMin(int t) const {
    return integer_trail_->LevelZeroLowerBound(demands_[t]);
  }
  bool DemandIsFixed(int t) const;
  void AddDemandMinReason(int t);
  void AddDemandMinReason(int t, IntegerValue min_demand);
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
  // Returns false if the energy can overflow and was not computed.
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
  bool CacheAllEnergyValues();
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
  IntegerValue SimpleEnergyMax(int t) const;
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
  DCHECK_GE(t, 0);
  DCHECK_LT(t, reason_for_presence_.size());
  return reason_for_presence_.data()[t] != kNoLiteralIndex;
}

inline bool SchedulingConstraintHelper::IsPresent(int t) const {
  DCHECK_GE(t, 0);
  DCHECK_LT(t, reason_for_presence_.size());
  const LiteralIndex lit = reason_for_presence_.data()[t];
  if (lit == kNoLiteralIndex) return true;
  return assignment_.LiteralIsTrue(Literal(lit));
}

inline bool SchedulingConstraintHelper::IsAbsent(int t) const {
  DCHECK_GE(t, 0);
  DCHECK_LT(t, reason_for_presence_.size());
  const LiteralIndex lit = reason_for_presence_.data()[t];
  if (lit == kNoLiteralIndex) return false;
  return assignment_.LiteralIsFalse(Literal(lit));
}

inline bool SchedulingConstraintHelper::IsOptional(LiteralIndex lit) const {
  return lit != kNoLiteralIndex;
}

inline bool SchedulingConstraintHelper::IsPresent(LiteralIndex lit) const {
  if (lit == kNoLiteralIndex) return true;
  return assignment_.LiteralIsTrue(Literal(lit));
}

inline bool SchedulingConstraintHelper::IsAbsent(LiteralIndex lit) const {
  if (lit == kNoLiteralIndex) return false;
  return assignment_.LiteralIsFalse(Literal(lit));
}

inline void SchedulingConstraintHelper::ResetReason() {
  integer_reason_.clear();
  literal_reason_.clear();
  enforcement_helper_.AddEnforcementReason(enforcement_id_, &literal_reason_);
  used_items_for_reason_.ClearAndResize(capacity_);
}

inline void SchedulingConstraintHelper::AddPresenceReason(int t) {
  DCHECK(IsPresent(t));
  FlagItemAsUsedInReason(t);
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
  }
}

inline void SchedulingConstraintHelper::AddAbsenceReason(int t) {
  DCHECK(IsAbsent(t));
  FlagItemAsUsedInReason(t);
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
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  if (lower_bound <= 0) return;
  AddGenericReason(sizes_[t].Negated(), -lower_bound, minus_ends_[t],
                   starts_[t]);
}

inline void SchedulingConstraintHelper::AddSizeMaxReason(
    int t, IntegerValue upper_bound) {
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(sizes_[t], upper_bound, ends_[t], minus_starts_[t]);
}

inline void SchedulingConstraintHelper::AddStartMinReason(
    int t, IntegerValue lower_bound) {
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(minus_starts_[t], -lower_bound, minus_ends_[t], sizes_[t]);
}

inline void SchedulingConstraintHelper::AddStartMaxReason(
    int t, IntegerValue upper_bound) {
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(starts_[t], upper_bound, ends_[t], NegatedSizeOrZero(t));
}

inline void SchedulingConstraintHelper::AddEndMinReason(
    int t, IntegerValue lower_bound) {
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(minus_ends_[t], -lower_bound, minus_starts_[t],
                   NegatedSizeOrZero(t));
}

inline void SchedulingConstraintHelper::AddEndMaxReason(
    int t, IntegerValue upper_bound) {
  FlagItemAsUsedInReason(t);
  DCHECK(!IsAbsent(t));
  AddGenericReason(ends_[t], upper_bound, starts_[t], sizes_[t]);
}

inline void SchedulingConstraintHelper::AddShiftedEndMaxReason(
    int t, IntegerValue upper_bound) {
  AddStartMaxReason(t, upper_bound - SizeMin(t));
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

inline void SchedulingConstraintHelper::AddLiteralReason(Literal l) {
  literal_reason_.push_back(l);
}

inline void SchedulingConstraintHelper::AddIntegerReason(IntegerLiteral l) {
  integer_reason_.push_back(l);
}

// Cuts helpers.
enum IntegerVariablesToAddMask {
  kStart = 1 << 0,
  kEnd = 1 << 1,
  kSize = 1 << 2,
  kPresence = 1 << 3,
};
void AddIntegerVariableFromIntervals(const SchedulingConstraintHelper* helper,
                                     Model* model,
                                     std::vector<IntegerVariable>* vars,
                                     int mask);

void AppendVariablesFromCapacityAndDemands(
    const AffineExpression& capacity, SchedulingDemandHelper* demands_helper,
    Model* model, std::vector<IntegerVariable>* vars);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_HELPERS_H_
