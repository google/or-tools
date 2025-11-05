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

#include "ortools/sat/disjunctive.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/timetable.h"
#include "ortools/sat/util.h"
#include "ortools/util/scheduling.h"
#include "ortools/util/sort.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void AddDisjunctive(const std::vector<Literal>& enforcement_literals,
                    const std::vector<IntervalVariable>& intervals,
                    Model* model) {
  // Depending on the parameters, create all pair of conditional precedences.
  // TODO(user): create them dynamically instead?
  const auto& params = *model->GetOrCreate<SatParameters>();
  if (intervals.size() <=
          params.max_size_to_create_precedence_literals_in_disjunctive() &&
      params.use_strong_propagation_in_disjunctive() &&
      enforcement_literals.empty()) {
    AddDisjunctiveWithBooleanPrecedencesOnly(intervals, model);
  }

  bool is_all_different = true;
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  for (const IntervalVariable var : intervals) {
    if (repository->IsOptional(var) || repository->MinSize(var) != 1 ||
        repository->MaxSize(var) != 1) {
      is_all_different = false;
      break;
    }
  }
  if (is_all_different) {
    std::vector<AffineExpression> starts;
    starts.reserve(intervals.size());
    for (const IntervalVariable interval : intervals) {
      starts.push_back(repository->Start(interval));
    }
    model->Add(AllDifferentOnBounds(enforcement_literals, starts));
    return;
  }

  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  if (intervals.size() > 2 && params.use_combined_no_overlap() &&
      enforcement_literals.empty()) {
    model->GetOrCreate<CombinedDisjunctive<true>>()->AddNoOverlap(intervals);
    model->GetOrCreate<CombinedDisjunctive<false>>()->AddNoOverlap(intervals);
    return;
  }

  SchedulingConstraintHelper* helper = repository->GetOrCreateHelper(
      enforcement_literals, intervals, /*register_as_disjunctive_helper=*/true);

  // Experiments to use the timetable only to propagate the disjunctive.
  if (/*DISABLES_CODE*/ (false && enforcement_literals.empty())) {
    const AffineExpression one(IntegerValue(1));
    std::vector<AffineExpression> demands(intervals.size(), one);
    SchedulingDemandHelper* demands_helper =
        repository->GetOrCreateDemandHelper(helper, demands);

    TimeTablingPerTask* timetable =
        new TimeTablingPerTask(one, helper, demands_helper, model);
    timetable->RegisterWith(watcher);
    model->TakeOwnership(timetable);
    return;
  }

  if (intervals.size() == 2) {
    DisjunctiveWithTwoItems* propagator = new DisjunctiveWithTwoItems(helper);
    propagator->RegisterWith(watcher);
    model->TakeOwnership(propagator);
  } else {
    // We decided to create the propagators in this particular order, but it
    // shouldn't matter much because of the different priorities used.
    if (!params.use_strong_propagation_in_disjunctive()) {
      // This one will not propagate anything if we added all precedence
      // literals since the linear propagator will already do that in that case.
      DisjunctiveSimplePrecedences* simple_precedences =
          new DisjunctiveSimplePrecedences(helper, model);
      const int id = simple_precedences->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 1);
      model->TakeOwnership(simple_precedences);
    }
    {
      // Only one direction is needed by this one.
      DisjunctiveOverloadChecker* overload_checker =
          new DisjunctiveOverloadChecker(helper, model);
      const int id = overload_checker->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 1);
      model->TakeOwnership(overload_checker);
    }
    for (const bool time_direction : {true, false}) {
      DisjunctiveDetectablePrecedences* detectable_precedences =
          new DisjunctiveDetectablePrecedences(time_direction, helper, model);
      const int id = detectable_precedences->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 2);
      model->TakeOwnership(detectable_precedences);
    }
    for (const bool time_direction : {true, false}) {
      DisjunctiveNotLast* not_last =
          new DisjunctiveNotLast(time_direction, helper, model);
      const int id = not_last->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 3);
      model->TakeOwnership(not_last);
    }
    for (const bool time_direction : {true, false}) {
      DisjunctiveEdgeFinding* edge_finding =
          new DisjunctiveEdgeFinding(time_direction, helper, model);
      const int id = edge_finding->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 4);
      model->TakeOwnership(edge_finding);
    }
  }

  // Note that we keep this one even when there is just two intervals. This is
  // because it might push a variable that is after both of the intervals
  // using the fact that they are in disjunction.
  if (params.use_precedences_in_disjunctive_constraint() &&
      !params.use_combined_no_overlap()) {
    // Lets try to exploit linear3 too.
    model->GetOrCreate<LinearPropagator>()->SetPushAffineUbForBinaryRelation();

    for (const bool time_direction : {true, false}) {
      DisjunctivePrecedences* precedences =
          new DisjunctivePrecedences(time_direction, helper, model);
      const int id = precedences->RegisterWith(watcher);
      watcher->SetPropagatorPriority(id, 5);
      model->TakeOwnership(precedences);
    }
  }
}

void AddDisjunctiveWithBooleanPrecedencesOnly(
    absl::Span<const IntervalVariable> intervals, Model* model) {
  auto* repo = model->GetOrCreate<IntervalsRepository>();
  for (int i = 0; i < intervals.size(); ++i) {
    for (int j = i + 1; j < intervals.size(); ++j) {
      repo->CreateDisjunctivePrecedenceLiteral(intervals[i], intervals[j]);
    }
  }
}

void TaskSet::AddEntry(const Entry& e) {
  int j = sorted_tasks_.size();
  sorted_tasks_.push_back(e);
  while (j > 0 && sorted_tasks_[j - 1].start_min > e.start_min) {
    sorted_tasks_[j] = sorted_tasks_[j - 1];
    --j;
  }
  sorted_tasks_[j] = e;
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));

  // If the task is added after optimized_restart_, we know that we don't need
  // to scan the task before optimized_restart_ in the next ComputeEndMin().
  if (j <= optimized_restart_) optimized_restart_ = 0;
}

void TaskSet::AddShiftedStartMinEntry(const SchedulingConstraintHelper& helper,
                                      int t) {
  const IntegerValue dmin = helper.SizeMin(t);
  AddEntry({t, std::max(helper.StartMin(t), helper.EndMin(t) - dmin), dmin});
}

void TaskSet::NotifyEntryIsNowLastIfPresent(const Entry& e) {
  const int size = sorted_tasks_.size();
  for (int i = 0;; ++i) {
    if (i == size) return;
    if (sorted_tasks_[i].task == e.task) {
      for (int j = i; j + 1 < size; ++j) {
        sorted_tasks_[j] = sorted_tasks_[j + 1];
      }
      sorted_tasks_.pop_back();
      break;
    }
  }

  optimized_restart_ = sorted_tasks_.size();
  sorted_tasks_.push_back(e);
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
}

IntegerValue TaskSet::ComputeEndMin() const {
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
  const int size = sorted_tasks_.size();
  IntegerValue end_min = kMinIntegerValue;
  for (int i = optimized_restart_; i < size; ++i) {
    const Entry& e = sorted_tasks_[i];
    if (e.start_min >= end_min) {
      optimized_restart_ = i;
      end_min = e.start_min + e.size_min;
    } else {
      end_min += e.size_min;
    }
  }
  return end_min;
}

IntegerValue TaskSet::ComputeEndMin(int task_to_ignore,
                                    int* critical_index) const {
  // The order in which we process tasks with the same start-min doesn't matter.
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
  bool ignored = false;
  const int size = sorted_tasks_.size();
  IntegerValue end_min = kMinIntegerValue;

  // If the ignored task is last and was the start of the critical block, then
  // we need to reset optimized_restart_.
  if (optimized_restart_ + 1 == size &&
      sorted_tasks_[optimized_restart_].task == task_to_ignore) {
    optimized_restart_ = 0;
  }

  for (int i = optimized_restart_; i < size; ++i) {
    const Entry& e = sorted_tasks_[i];
    if (e.task == task_to_ignore) {
      ignored = true;
      continue;
    }
    if (e.start_min >= end_min) {
      *critical_index = i;
      if (!ignored) optimized_restart_ = i;
      end_min = e.start_min + e.size_min;
    } else {
      end_min += e.size_min;
    }
  }
  return end_min;
}

bool DisjunctiveWithTwoItems::Propagate() {
  DCHECK_EQ(helper_->NumTasks(), 2);
  if (!helper_->IsEnforced()) return true;
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

  // We can't propagate anything if one of the interval is absent for sure.
  if (helper_->IsAbsent(0) || helper_->IsAbsent(1)) return true;

  // We can't propagate anything for two intervals if neither are present.
  if (!helper_->IsPresent(0) && !helper_->IsPresent(0)) return true;

  // Note that this propagation also take care of the "overload checker" part.
  // It also propagates as much as possible, even in the presence of task with
  // variable sizes.
  //
  // TODO(user): For optional interval whose presence in unknown and without
  // optional variable, the end-min may not be propagated to at least (start_min
  // + size_min). Consider that into the computation so we may decide the
  // interval forced absence? Same for the start-max.
  int task_before = 0;
  int task_after = 1;

  const bool task_0_before_task_1 = helper_->TaskIsBeforeOrIsOverlapping(0, 1);
  const bool task_1_before_task_0 = helper_->TaskIsBeforeOrIsOverlapping(1, 0);

  if (task_0_before_task_1 && task_1_before_task_0 &&
      helper_->IsPresent(task_before) && helper_->IsPresent(task_after)) {
    helper_->ResetReason();
    helper_->AddPresenceReason(task_before);
    helper_->AddPresenceReason(task_after);
    helper_->AddReasonForBeingBeforeAssumingNoOverlap(task_before, task_after);
    helper_->AddReasonForBeingBeforeAssumingNoOverlap(task_after, task_before);
    return helper_->ReportConflict();
  }

  if (task_0_before_task_1) {
    // Task 0 must be before task 1.
  } else if (task_1_before_task_0) {
    // Task 1 must be before task 0.
    std::swap(task_before, task_after);
  } else {
    return true;
  }

  helper_->ResetReason();
  helper_->AddReasonForBeingBeforeAssumingNoOverlap(task_before, task_after);
  if (!helper_->PushTaskOrderWhenPresent(task_before, task_after)) {
    return false;
  }
  return true;
}

int DisjunctiveWithTwoItems::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

template <bool time_direction>
CombinedDisjunctive<time_direction>::CombinedDisjunctive(Model* model)
    : helper_(model->GetOrCreate<IntervalsRepository>()->GetOrCreateHelper(
          /*enforcement_literals=*/{},
          model->GetOrCreate<IntervalsRepository>()->AllIntervals())) {
  task_to_disjunctives_.resize(helper_->NumTasks());

  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

template <bool time_direction>
void CombinedDisjunctive<time_direction>::AddNoOverlap(
    absl::Span<const IntervalVariable> vars) {
  const int index = task_sets_.size();
  task_sets_.emplace_back(task_set_storage_.emplace_back());
  end_mins_.push_back(kMinIntegerValue);
  for (const IntervalVariable var : vars) {
    task_to_disjunctives_[var.value()].push_back(index);
  }
}

template <bool time_direction>
bool CombinedDisjunctive<time_direction>::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction)) return false;
  const auto& task_by_increasing_end_min = helper_->TaskByIncreasingEndMin();
  const auto& task_by_negated_start_max =
      helper_->TaskByIncreasingNegatedStartMax();

  for (auto& task_set : task_sets_) task_set.Clear();
  end_mins_.assign(end_mins_.size(), kMinIntegerValue);
  IntegerValue max_of_end_min = kMinIntegerValue;

  const int num_tasks = helper_->NumTasks();
  task_is_added_.assign(num_tasks, false);
  int queue_index = num_tasks - 1;
  for (const auto task_time : task_by_increasing_end_min) {
    const int t = task_time.task_index;
    const IntegerValue end_min = task_time.time;
    if (helper_->IsAbsent(t)) continue;

    // Update all task sets.
    while (queue_index >= 0) {
      const auto to_insert = task_by_negated_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = -to_insert.time;
      if (end_min <= start_max) break;
      if (helper_->IsPresent(task_index)) {
        task_is_added_[task_index] = true;
        const IntegerValue shifted_smin = helper_->ShiftedStartMin(task_index);
        const IntegerValue size_min = helper_->SizeMin(task_index);
        for (const int d_index : task_to_disjunctives_[task_index]) {
          // TODO(user): AddEntry() and ComputeEndMin() could be combined.
          task_sets_[d_index].AddEntry({task_index, shifted_smin, size_min});
          end_mins_[d_index] = task_sets_[d_index].ComputeEndMin();
          max_of_end_min = std::max(max_of_end_min, end_mins_[d_index]);
        }
      }
      --queue_index;
    }

    // Find out amongst the disjunctives in which t appear, the one with the
    // largest end_min, ignoring t itself. This will be the new start min for t.
    IntegerValue new_start_min = helper_->StartMin(t);
    if (new_start_min >= max_of_end_min) continue;
    int best_critical_index = 0;
    int best_d_index = -1;
    if (task_is_added_[t]) {
      for (const int d_index : task_to_disjunctives_[t]) {
        if (new_start_min >= end_mins_[d_index]) continue;
        int critical_index = 0;
        const IntegerValue end_min_of_critical_tasks =
            task_sets_[d_index].ComputeEndMin(/*task_to_ignore=*/t,
                                              &critical_index);
        DCHECK_LE(end_min_of_critical_tasks, max_of_end_min);
        if (end_min_of_critical_tasks > new_start_min) {
          new_start_min = end_min_of_critical_tasks;
          best_d_index = d_index;
          best_critical_index = critical_index;
        }
      }
    } else {
      // If the task t was not added, then there is no task to ignore and
      // end_mins_[d_index] is up to date.
      for (const int d_index : task_to_disjunctives_[t]) {
        if (end_mins_[d_index] > new_start_min) {
          new_start_min = end_mins_[d_index];
          best_d_index = d_index;
        }
      }
      if (best_d_index != -1) {
        const IntegerValue end_min_of_critical_tasks =
            task_sets_[best_d_index].ComputeEndMin(/*task_to_ignore=*/t,
                                                   &best_critical_index);
        CHECK_EQ(end_min_of_critical_tasks, new_start_min);
      }
    }

    // Do we push something?
    if (best_d_index == -1) continue;

    // Same reason as DisjunctiveDetectablePrecedences.
    // TODO(user): Maybe factor out the code? It does require a function with a
    // lot of arguments though.
    helper_->ResetReason();
    const absl::Span<const TaskSet::Entry> sorted_tasks =
        task_sets_[best_d_index].SortedTasks();
    const IntegerValue window_start =
        sorted_tasks[best_critical_index].start_min;
    for (int i = best_critical_index; i < sorted_tasks.size(); ++i) {
      const int ct = sorted_tasks[i].task;
      if (ct == t) continue;
      helper_->AddPresenceReason(ct);
      helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min, window_start);
      helper_->AddStartMaxReason(ct, end_min - 1);
    }
    helper_->AddEndMinReason(t, end_min);
    if (!helper_->IncreaseStartMin(t, new_start_min)) {
      return false;
    }

    // We need to reorder t inside task_set_. Note that if t is in the set,
    // it means that the task is present and that IncreaseStartMin() did push
    // its start (by opposition to an optional interval where the push might
    // not happen if its start is not optional).
    if (task_is_added_[t]) {
      const IntegerValue shifted_smin = helper_->ShiftedStartMin(t);
      const IntegerValue size_min = helper_->SizeMin(t);
      for (const int d_index : task_to_disjunctives_[t]) {
        // TODO(user): Refactor the code to use the same algo as in
        // DisjunctiveDetectablePrecedences, it is superior and do not need
        // this function.
        task_sets_[d_index].NotifyEntryIsNowLastIfPresent(
            {t, shifted_smin, size_min});
        end_mins_[d_index] = task_sets_[d_index].ComputeEndMin();
        max_of_end_min = std::max(max_of_end_min, end_mins_[d_index]);
      }
    }
  }
  return true;
}

bool DisjunctiveOverloadChecker::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();
  if (!helper_->SynchronizeAndSetTimeDirection(/*is_forward=*/true)) {
    ++stats_.num_conflicts;
    return false;
  }

  absl::Span<TaskTime> window_span =
      absl::MakeSpan(helper_->GetScratchTaskTimeVector1());

  // We use this to detect precedence between task that must cause a push.
  TaskTime task_with_max_end_min = {0, kMinIntegerValue};

  // We will process the task by increasing shifted_end_max, thus consuming task
  // from the end of that vector.
  absl::Span<const CachedTaskBounds>
      task_by_increasing_negated_shifted_end_max =
          helper_->TaskByIncreasingNegatedShiftedEndMax();

  // Split problem into independent part.
  //
  // Many propagators in this file use the same approach, we start by processing
  // the task by increasing shifted start-min, packing everything to the left.
  // We then process each "independent" set of task separately. A task is
  // independent from the one before it, if its start-min wasn't pushed.
  //
  // This way, we get one or more window [window_start, window_end] so that for
  // all task in the window, [start_min, end_min] is inside the window, and the
  // end min of any set of task to the left is <= window_start, and the
  // start_min of any task to the right is >= window_end.

  // We keep the best end_min for present task only and present task plus at
  // most a single optional task.
  IntegerValue end_min_of_present = kMinIntegerValue;
  IntegerValue end_min_with_one_optional = kMinIntegerValue;

  IntegerValue relevant_end;
  int window_size = 0;
  int relevant_size = 0;
  TaskTime* const window = window_span.data();
  for (const auto [task, presence_lit, start_min] :
       helper_->TaskByIncreasingShiftedStartMin()) {
    if (helper_->IsAbsent(presence_lit)) continue;

    // Nothing to do with overload checking, but almost free to do that here.
    const IntegerValue size_min = helper_->SizeMin(task);
    const IntegerValue end_min = start_min + size_min;
    const IntegerValue start_max = helper_->StartMax(task);
    if (start_max < task_with_max_end_min.time &&
        helper_->IsPresent(presence_lit) && size_min > 0) {
      // We have a detectable precedence that must cause a push.
      //
      // Remarks: If we added all precedence literals + linear relation, this
      // propagation would have been done by the linear propagator, but if we
      // didn't add such relations yet, it is beneficial to detect that here!
      //
      // TODO(user): Actually, we just inferred a "not last" so we could check
      // for relevant_size > 2 potential propagation?
      //
      // Note that the DisjunctiveSimplePrecedences propagators will also
      // detects such precedences between two tasks.
      const int to_push = task_with_max_end_min.task_index;
      helper_->ResetReason();
      helper_->AddReasonForBeingBeforeAssumingNoOverlap(task, to_push);
      if (!helper_->PushTaskOrderWhenPresent(task, to_push)) {
        ++stats_.num_conflicts;
        return false;
      }

      // TODO(user): Shall we keep propagating? we know the prefix didn't
      // change, so we could be faster here. On another hand, it might be
      // better to propagate all the linear constraints before returning
      // here.
      ++stats_.num_propagations;

      stats_.EndWithoutConflicts();
      return true;
    }

    // Note that we need to do that AFTER the block above.
    if (end_min > task_with_max_end_min.time) {
      task_with_max_end_min = {task, end_min};
    }

    // Extend window.
    const IntegerValue old_value = end_min_with_one_optional;
    if (start_min < end_min_with_one_optional) {
      window[window_size++] = {task, start_min};

      if (helper_->IsPresent(presence_lit)) {
        end_min_of_present =
            std::max(start_min + size_min, end_min_of_present + size_min);
        end_min_with_one_optional += size_min;
      } else {
        if (end_min_of_present == kMinIntegerValue) {
          // only optional:
          end_min_with_one_optional =
              std::max(end_min_with_one_optional, start_min + size_min);
        } else {
          end_min_with_one_optional =
              std::max(end_min_with_one_optional,
                       std::max(end_min_of_present, start_min) + size_min);
        }
      }

      if (old_value > start_max) {
        relevant_size = window_size;
        relevant_end = end_min_with_one_optional;
      }
      continue;
    }

    // Process current window.
    // We don't need to process the end of the window (after relevant_size)
    // because these interval can be greedily assembled in a feasible solution.
    if (relevant_size > 0 &&
        !PropagateSubwindow(window_span.subspan(0, relevant_size), relevant_end,
                            &task_by_increasing_negated_shifted_end_max)) {
      ++stats_.num_conflicts;
      return false;
    }

    // Subwindow propagation might have propagated that the
    // task_with_max_end_min must be absent.
    if (helper_->IsAbsent(task_with_max_end_min.task_index)) {
      task_with_max_end_min = {0, kMinIntegerValue};
    }

    // Start of the next window.
    window_size = 0;
    window[window_size++] = {task, start_min};

    if (helper_->IsPresent(presence_lit)) {
      end_min_of_present = start_min + size_min;
      end_min_with_one_optional = end_min_of_present;
    } else {
      end_min_of_present = kMinIntegerValue;
      end_min_with_one_optional = start_min + size_min;
    }

    relevant_size = 0;
  }

  // Process last window.
  if (relevant_size > 0 &&
      !PropagateSubwindow(window_span.subspan(0, relevant_size), relevant_end,
                          &task_by_increasing_negated_shifted_end_max)) {
    ++stats_.num_conflicts;
    return false;
  }

  if (DEBUG_MODE) {
    // This test that our "subwindow" logic is sound and we don't miss any
    // conflict or propagation by splitting the intervals like this.
    //
    // TODO(user): I think we can come up with corner case where pushing a task
    // absence forces another one to be present, which might create more
    // propagation. This is the same reason why we might not reach a fixed point
    // in one go. However this should be rare and that piece of code allowed me
    // to fix many bug. So I left it as is, we can revisit if we starts to see
    // crashes on our tests.
    window_size = 0;
    task_by_increasing_negated_shifted_end_max =
        helper_->TaskByIncreasingNegatedShiftedEndMax();
    for (const auto [task, presence_lit, start_min] :
         helper_->TaskByIncreasingShiftedStartMin()) {
      if (helper_->IsAbsent(presence_lit)) continue;
      window[window_size++] = {task, start_min};
    }
    const int64_t saved = helper_->PropagationTimestamp();
    CHECK(PropagateSubwindow(window_span.subspan(0, window_size),
                             kMaxIntegerValue,
                             &task_by_increasing_negated_shifted_end_max));
    CHECK_EQ(saved, helper_->PropagationTimestamp());
  }

  stats_.EndWithoutConflicts();
  return true;
}

// TODO(user): Improve the Overload Checker using delayed insertion.
// We insert events at the cost of O(log n) per insertion, and this is where
// the algorithm spends most of its time, thus it is worth improving.
// We can insert an arbitrary set of tasks at the cost of O(n) for the whole
// set. This is useless for the overload checker as is since we need to check
// overload after every insertion, but we could use an upper bound of the
// theta envelope to save us from checking the actual value.
bool DisjunctiveOverloadChecker::PropagateSubwindow(
    absl::Span<TaskTime> sub_window, IntegerValue global_window_end,
    absl::Span<const CachedTaskBounds>*
        task_by_increasing_negated_shifted_end_max) {
  // Set up theta tree.
  TaskTime* const window = sub_window.data();
  int num_events = 0;
  for (int i = 0; i < sub_window.size(); ++i) {
    // No point adding a task if its end_max is too large.
    const TaskTime& task_time = window[i];
    const int task = task_time.task_index;

    // We use the shifted end-max which might be < EndMax().
    const IntegerValue end_max = helper_->ShiftedEndMax(task);
    if (end_max < global_window_end) {
      window[num_events] = task_time;
      task_to_event_[task] = num_events;
      ++num_events;
    }
  }
  if (num_events <= 1) return true;
  theta_tree_.Reset(num_events);

  // Introduce events by increasing end_max, check for overloads.
  while (!task_by_increasing_negated_shifted_end_max->empty()) {
    const auto [current_task, presence_lit, minus_shifted_end_max] =
        task_by_increasing_negated_shifted_end_max->back();
    const IntegerValue current_end = -minus_shifted_end_max;
    DCHECK_EQ(current_end, helper_->ShiftedEndMax(current_task));
    if (current_end >= global_window_end) return true;  // go to next window.
    task_by_increasing_negated_shifted_end_max->remove_suffix(1);  // consume.

    // Ignore task not part of that window.
    // Note that we never clear task_to_event_, so it might contains garbage
    // for the values that we didn't set above. However, we can easily check
    // if task_to_event_[current_task] point to an element that we set above.
    // This is a bit faster than clearing task_to_event_ on exit.
    const int current_event = task_to_event_[current_task];
    if (current_event < 0 || current_event >= num_events) continue;
    if (window[current_event].task_index != current_task) continue;

    // We filtered absent task while constructing the subwindow, but it is
    // possible that as we propagate task absence below, other task also become
    // absent (if they share the same presence Boolean).
    if (helper_->IsAbsent(presence_lit)) continue;

    DCHECK_NE(task_to_event_[current_task], -1);
    {
      const IntegerValue energy_min = helper_->SizeMin(current_task);
      if (helper_->IsPresent(presence_lit)) {
        // TODO(user): Add max energy deduction for variable
        // sizes by putting the energy_max here and modifying the code
        // dealing with the optional envelope greater than current_end below.
        theta_tree_.AddOrUpdateEvent(current_event, window[current_event].time,
                                     energy_min, energy_min);
      } else {
        theta_tree_.AddOrUpdateOptionalEvent(
            current_event, window[current_event].time, energy_min);
      }
    }

    if (theta_tree_.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      helper_->ResetReason();
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = window[critical_event].time;
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < num_events; event++) {
        const IntegerValue energy_min = theta_tree_.EnergyMin(event);
        if (energy_min > 0) {
          const int task = window[event].task_index;
          helper_->AddPresenceReason(task);
          helper_->AddEnergyAfterReason(task, energy_min, window_start);
          helper_->AddShiftedEndMaxReason(task, window_end);
        }
      }
      return helper_->ReportConflict();
    }

    // Exclude all optional tasks that would overload an interval ending here.
    while (theta_tree_.GetOptionalEnvelope() > current_end) {
      // Explain exclusion with tasks present in the critical interval.
      // TODO(user): This could be done lazily, like most of the loop to
      // compute the reasons in this file.
      helper_->ResetReason();
      int critical_event;
      int optional_event;
      IntegerValue available_energy;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          current_end, &critical_event, &optional_event, &available_energy);

      const int optional_task = window[optional_event].task_index;

      // If tasks shares the same presence literal, it is possible that we
      // already pushed this task absence.
      if (!helper_->IsAbsent(optional_task)) {
        const IntegerValue optional_size_min = helper_->SizeMin(optional_task);
        const IntegerValue window_start = window[critical_event].time;
        const IntegerValue window_end =
            current_end + optional_size_min - available_energy - 1;
        for (int event = critical_event; event < num_events; event++) {
          const IntegerValue energy_min = theta_tree_.EnergyMin(event);
          if (energy_min > 0) {
            const int task = window[event].task_index;
            helper_->AddPresenceReason(task);
            helper_->AddEnergyAfterReason(task, energy_min, window_start);
            helper_->AddShiftedEndMaxReason(task, window_end);
          }
        }

        helper_->AddEnergyAfterReason(optional_task, optional_size_min,
                                      window_start);
        helper_->AddShiftedEndMaxReason(optional_task, window_end);

        ++stats_.num_propagations;
        if (!helper_->PushTaskAbsence(optional_task)) return false;
      }

      theta_tree_.RemoveEvent(optional_event);
    }
  }

  return true;
}

int DisjunctiveOverloadChecker::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator should mostly reach the fix point in one pass, except if
  // pushing a task absence make another task present... So we still prefer to
  // re-run it as it is relatively fast.
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(/*is_forward=*/true);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  helper_->WatchAllTasks(id);
  return id;
}

int DisjunctiveSimplePrecedences::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctiveSimplePrecedences::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();

  const bool current_direction = helper_->CurrentTimeIsForward();
  for (const bool direction : {current_direction, !current_direction}) {
    if (!helper_->SynchronizeAndSetTimeDirection(direction)) {
      ++stats_.num_conflicts;
      return false;
    }
    if (!PropagateOneDirection()) {
      ++stats_.num_conflicts;
      return false;
    }
  }

  stats_.EndWithoutConflicts();
  return true;
}

bool DisjunctiveSimplePrecedences::Push(TaskTime before, int t) {
  const int t_before = before.task_index;

  DCHECK_NE(t_before, t);
  helper_->ResetReason();
  helper_->AddReasonForBeingBeforeAssumingNoOverlap(t_before, t);
  if (!helper_->PushTaskOrderWhenPresent(t_before, t)) return false;
  ++stats_.num_propagations;
  return true;
}

bool DisjunctiveSimplePrecedences::PropagateOneDirection() {
  // We will loop in a decreasing way here.
  // And add tasks that are present to the task_set_.
  absl::Span<const TaskTime> task_by_negated_start_max =
      helper_->TaskByIncreasingNegatedStartMax();

  // We just keep amongst all the task before current_end_min, the one with the
  // highesh end-min.
  TaskTime best_task_before = {-1, kMinIntegerValue};

  // We will loop in an increasing way here and consume task from beginning.
  absl::Span<const TaskTime> task_by_increasing_end_min =
      helper_->TaskByIncreasingEndMin();

  for (; !task_by_increasing_end_min.empty();) {
    // Skip absent task.
    if (helper_->IsAbsent(task_by_increasing_end_min.front().task_index)) {
      task_by_increasing_end_min.remove_prefix(1);
      continue;
    }

    // Consider all task with a start_max < current_end_min.
    int blocking_task = -1;
    IntegerValue blocking_start_max;
    IntegerValue current_end_min = task_by_increasing_end_min.front().time;
    for (; true; task_by_negated_start_max.remove_suffix(1)) {
      if (task_by_negated_start_max.empty()) {
        // Small optim: this allows to process all remaining task rather than
        // looping around are retesting this for all remaining tasks.
        current_end_min = kMaxIntegerValue;
        break;
      }

      const auto [t, negated_start_max] = task_by_negated_start_max.back();
      const IntegerValue start_max = -negated_start_max;
      if (current_end_min <= start_max) break;
      if (!helper_->IsPresent(t)) continue;

      // If t has a mandatory part, and extend further than current_end_min
      // then we can push it first. All tasks for which their push is delayed
      // are necessarily after this "blocking task".
      //
      // This idea is introduced in "Linear-Time Filtering Algorithms for the
      // Disjunctive Constraints" Hamed Fahimi, Claude-Guy Quimper.
      const IntegerValue end_min = helper_->EndMin(t);
      if (blocking_task == -1 && end_min >= current_end_min) {
        DCHECK_LT(start_max, end_min) << " task should have mandatory part: "
                                      << helper_->TaskDebugString(t);
        blocking_task = t;
        blocking_start_max = start_max;
        current_end_min = end_min;
      } else if (blocking_task != -1 && blocking_start_max < end_min) {
        // Conflict! the task is after the blocking_task but also before.
        helper_->ResetReason();
        helper_->AddPresenceReason(blocking_task);
        helper_->AddPresenceReason(t);
        helper_->AddReasonForBeingBeforeAssumingNoOverlap(blocking_task, t);
        helper_->AddReasonForBeingBeforeAssumingNoOverlap(t, blocking_task);
        return helper_->ReportConflict();
      } else if (end_min > best_task_before.time) {
        best_task_before = {t, end_min};
      }
    }

    // If we have a blocking task. We need to propagate it first.
    if (blocking_task != -1) {
      DCHECK(!helper_->IsAbsent(blocking_task));
      if (best_task_before.time > helper_->StartMin(blocking_task)) {
        if (!Push(best_task_before, blocking_task)) return false;
      }

      // Update best_task_before.
      //
      // Note that we want the blocking task here as it is the only one
      // guaranteed to be before all the task we push below. If we are not in a
      // propagation loop, it should also be the best.
      const IntegerValue end_min = helper_->EndMin(blocking_task);
      best_task_before = {blocking_task, end_min};
    }

    // Lets propagate all task after best_task_before.
    for (; !task_by_increasing_end_min.empty();
         task_by_increasing_end_min.remove_prefix(1)) {
      const auto [t, end_min] = task_by_increasing_end_min.front();
      if (end_min > current_end_min) break;
      if (t == blocking_task) continue;  // Already done.

      // Lets propagate current_task.
      if (best_task_before.time > helper_->StartMin(t)) {
        // Corner case if a previous push caused a subsequent task to be absent.
        if (helper_->IsAbsent(t)) continue;
        if (!Push(best_task_before, t)) return false;
      }
    }
  }
  return true;
}

bool DisjunctiveDetectablePrecedences::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) {
    ++stats_.num_conflicts;
    return false;
  }

  // Compute the "rank" of each task.
  const auto by_shifted_smin = helper_->TaskByIncreasingShiftedStartMin();
  int rank = -1;
  IntegerValue window_end = kMinIntegerValue;
  int* const ranks = ranks_.data();
  for (const auto [task, presence_lit, start_min] : by_shifted_smin) {
    if (!helper_->IsPresent(presence_lit)) {
      ranks[task] = -1;
      continue;
    }

    const IntegerValue size_min = helper_->SizeMin(task);
    if (start_min < window_end) {
      ranks[task] = rank;
      window_end += size_min;
    } else {
      ranks[task] = ++rank;
      window_end = start_min + size_min;
    }
  }

  if (!PropagateWithRanks()) {
    ++stats_.num_conflicts;
    return false;
  }
  stats_.EndWithoutConflicts();
  return true;
}

// task_set_ contains all the tasks that must be executed before t. They
// are in "detectable precedence" because their start_max is smaller than
// the end-min of t like so:
//          [(the task t)
//                     (a task in task_set_)]
// From there, we deduce that the start-min of t is greater or equal to
// the end-min of the critical tasks.
//
// Note that this works as well when IsPresent(t) is false.
bool DisjunctiveDetectablePrecedences::Push(IntegerValue task_set_end_min,
                                            int t, TaskSet& task_set) {
  const int critical_index = task_set.GetCriticalIndex();
  const absl::Span<const TaskSet::Entry> sorted_tasks = task_set.SortedTasks();
  helper_->ResetReason();

  // We need:
  // - StartMax(ct) < EndMin(t) for the detectable precedence.
  // - StartMin(ct) >= window_start for the value of task_set_end_min.
  const IntegerValue end_min_if_present =
      helper_->ShiftedStartMin(t) + helper_->SizeMin(t);
  const IntegerValue window_start = sorted_tasks[critical_index].start_min;
  IntegerValue min_slack = kMaxIntegerValue;
  bool all_already_before = true;
  IntegerValue energy_of_task_before = 0;
  for (int i = critical_index; i < sorted_tasks.size(); ++i) {
    const int ct = sorted_tasks[i].task;
    DCHECK_NE(ct, t);
    helper_->AddPresenceReason(ct);

    // Heuristic, if some tasks are known to be after the first one,
    // we just add the min-size as a reason.
    //
    // TODO(user): ideally we don't want to do that if we don't have a level
    // zero precedence...
    if (i > critical_index && helper_->GetCurrentMinDistanceBetweenTasks(
                                  sorted_tasks[critical_index].task, ct) >= 0) {
      helper_->AddReasonForBeingBeforeAssumingNoOverlap(
          sorted_tasks[critical_index].task, ct);
      helper_->AddSizeMinReason(ct);
    } else {
      helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min, window_start);
    }

    // We only need the reason for being before if we don't already have
    // a static precedence between the tasks.
    const IntegerValue dist = helper_->GetCurrentMinDistanceBetweenTasks(ct, t);
    if (dist >= 0) {
      helper_->AddReasonForBeingBeforeAssumingNoOverlap(ct, t);
      energy_of_task_before += sorted_tasks[i].size_min;
      min_slack = std::min(min_slack, dist);

      // TODO(user): The reason in AddReasonForBeingBeforeAssumingNoOverlap()
      // only account for dist == 0 !! If we want to use a stronger strictly
      // positive distance, we actually need to explain it. Fix this.
      min_slack = 0;
    } else {
      all_already_before = false;
      helper_->AddStartMaxReason(ct, end_min_if_present - 1);
    }
  }

  // We only need the end-min of t if not all the task are already known
  // to be before.
  IntegerValue new_start_min = task_set_end_min;
  if (all_already_before) {
    // We can actually push further!
    // And we don't need other reason except the precedences.
    new_start_min += min_slack;
  } else {
    helper_->AddEndMinReason(t, end_min_if_present);
  }

  // In some situation, we can push the task further.
  // TODO(user): We can also reduce the reason in this case.
  if (min_slack != kMaxIntegerValue &&
      window_start + energy_of_task_before + min_slack > new_start_min) {
    new_start_min = window_start + energy_of_task_before + min_slack;
  }

  // Process detected precedence.
  if (helper_->CurrentDecisionLevel() == 0 && helper_->IsPresent(t)) {
    for (int i = critical_index; i < sorted_tasks.size(); ++i) {
      if (!helper_->NotifyLevelZeroPrecedence(sorted_tasks[i].task, t)) {
        return false;
      }
    }
  }

  // This augment the start-min of t. Note that t is not in task set
  // yet, so we will use this updated start if we ever add it there.
  ++stats_.num_propagations;
  if (!helper_->IncreaseStartMin(t, new_start_min)) {
    return false;
  }

  return true;
}

bool DisjunctiveDetectablePrecedences::PropagateWithRanks() {
  // We will "consume" tasks from here.
  absl::Span<const TaskTime> task_by_increasing_end_min =
      helper_->TaskByIncreasingEndMin();
  absl::Span<const TaskTime> task_by_negated_start_max =
      helper_->TaskByIncreasingNegatedStartMax();

  // We will stop using ranks as soon as we propagated something. This allow to
  // be sure this propagate as much as possible in a single pass and seems to
  // help slightly.
  int highest_rank = 0;
  bool some_propagation = false;

  // Invariant: need_update is false implies that task_set_end_min is equal to
  // task_set.ComputeEndMin().
  //
  // TODO(user): Maybe it is just faster to merge ComputeEndMin() with
  // AddEntry().
  TaskSet task_set(helper_->GetScratchTaskSet());
  to_add_.clear();
  IntegerValue task_set_end_min = kMinIntegerValue;
  for (; !task_by_increasing_end_min.empty();) {
    // Consider all task with a start_max < current_end_min.
    int blocking_task = -1;
    IntegerValue blocking_start_max;
    IntegerValue current_end_min = task_by_increasing_end_min.front().time;

    for (; true; task_by_negated_start_max.remove_suffix(1)) {
      if (task_by_negated_start_max.empty()) {
        // Small optim: this allows to process all remaining task rather than
        // looping around are retesting this for all remaining tasks.
        current_end_min = kMaxIntegerValue;
        break;
      }

      const auto [t, negated_start_max] = task_by_negated_start_max.back();
      const IntegerValue start_max = -negated_start_max;
      if (current_end_min <= start_max) break;

      // If the task is not present, its rank will be -1.
      const int rank = ranks_[t];
      if (rank < highest_rank) continue;
      DCHECK(helper_->IsPresent(t));

      // If t has a mandatory part, and extend further than current_end_min
      // then we can push it first. All tasks for which their push is delayed
      // are necessarily after this "blocking task".
      //
      // This idea is introduced in "Linear-Time Filtering Algorithms for the
      // Disjunctive Constraints" Hamed Fahimi, Claude-Guy Quimper.
      const IntegerValue end_min = helper_->EndMin(t);
      if (blocking_task == -1 && end_min >= current_end_min) {
        DCHECK_LT(start_max, end_min) << " task should have mandatory part: "
                                      << helper_->TaskDebugString(t);
        blocking_task = t;
        blocking_start_max = start_max;
        current_end_min = end_min;
      } else if (blocking_task != -1 && blocking_start_max < end_min) {
        // Conflict! the task is after the blocking_task but also before.
        helper_->ResetReason();
        helper_->AddPresenceReason(blocking_task);
        helper_->AddPresenceReason(t);
        helper_->AddReasonForBeingBeforeAssumingNoOverlap(blocking_task, t);
        helper_->AddReasonForBeingBeforeAssumingNoOverlap(t, blocking_task);
        return helper_->ReportConflict();
      } else {
        if (!some_propagation && rank > highest_rank) {
          to_add_.clear();
          task_set.Clear();
          highest_rank = rank;
        }
        to_add_.push_back(t);
      }
    }

    // If we have a blocking task. We need to propagate it first.
    if (blocking_task != -1) {
      DCHECK(!helper_->IsAbsent(blocking_task));

      if (!to_add_.empty()) {
        for (const int t : to_add_) {
          task_set.AddShiftedStartMinEntry(*helper_, t);
        }
        to_add_.clear();
        task_set_end_min = task_set.ComputeEndMin();
      }

      if (task_set_end_min > helper_->StartMin(blocking_task)) {
        some_propagation = true;
        if (!Push(task_set_end_min, blocking_task, task_set)) return false;
      }

      // This propagators assumes that every push is reflected for its
      // correctness.
      if (helper_->InPropagationLoop()) return true;

      // Insert the blocking_task. Note that because we just pushed it,
      // it will be last in task_set_ and also the only reason used to push
      // any of the subsequent tasks below. In particular, the reason will be
      // valid even though task_set might contains tasks with a start_max
      // greater or equal to the end_min of the task we push.
      if (!some_propagation && ranks_[blocking_task] > highest_rank) {
        to_add_.clear();
        task_set.Clear();
        highest_rank = ranks_[blocking_task];
      }
      to_add_.push_back(blocking_task);
    }

    // Lets propagate all task with end_min <= current_end_min that are also
    // after all the task in task_set_.
    for (; !task_by_increasing_end_min.empty();
         task_by_increasing_end_min.remove_prefix(1)) {
      const auto [t, end_min] = task_by_increasing_end_min.front();
      if (end_min > current_end_min) break;
      if (t == blocking_task) continue;  // Already done.

      if (!to_add_.empty()) {
        for (const int t : to_add_) {
          task_set.AddShiftedStartMinEntry(*helper_, t);
        }
        to_add_.clear();
        task_set_end_min = task_set.ComputeEndMin();
      }

      // Lets propagate current_task.
      if (task_set_end_min > helper_->StartMin(t)) {
        // Corner case if a previous push caused a subsequent task to be absent.
        if (helper_->IsAbsent(t)) continue;

        some_propagation = true;
        if (!Push(task_set_end_min, t, task_set)) return false;
      }
    }
  }

  return true;
}

int DisjunctiveDetectablePrecedences::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctivePrecedences::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) {
    ++stats_.num_conflicts;
    return false;
  }
  FixedCapacityVector<TaskTime>& window = helper_->GetScratchTaskTimeVector1();

  // We only need to consider "critical" set of tasks given how we compute the
  // min-offset in PropagateSubwindow().
  IntegerValue window_end = kMinIntegerValue;
  for (const auto [task, presence_lit, start_min] :
       helper_->TaskByIncreasingShiftedStartMin()) {
    if (!helper_->IsPresent(presence_lit)) continue;

    if (start_min < window_end) {
      window.push_back({task, start_min});
      window_end += helper_->SizeMin(task);
      continue;
    }

    if (window.size() > 1 && !PropagateSubwindow(absl::MakeSpan(window))) {
      ++stats_.num_conflicts;
      return false;
    }

    // Start of the next window.
    window.clear();
    window.push_back({task, start_min});
    window_end = start_min + helper_->SizeMin(task);
  }
  if (window.size() > 1 && !PropagateSubwindow(absl::MakeSpan(window))) {
    ++stats_.num_conflicts;
    return false;
  }

  stats_.EndWithoutConflicts();
  return true;
}

bool DisjunctivePrecedences::PropagateSubwindow(absl::Span<TaskTime> window) {
  // This function can be slow, so count it in the dtime.
  int64_t num_hash_lookup = 0;
  auto cleanup = ::absl::MakeCleanup([&num_hash_lookup, this]() {
    time_limit_->AdvanceDeterministicTime(static_cast<double>(num_hash_lookup) *
                                          5e-8);
  });

  // TODO(user): We shouldn't consider ends for fixed intervals here. But
  // then we should do a better job of computing the min-end of a subset of
  // intervals from this disjunctive (like using fixed intervals even if there
  // is no "before that variable" relationship). Ex: If a variable is after two
  // intervals that cannot be both before a fixed one, we could propagate more.
  index_to_end_vars_.clear();
  int new_size = 0;
  for (const auto task_time : window) {
    const int task = task_time.task_index;
    const AffineExpression& end_exp = helper_->Ends()[task];

    // TODO(user): Handle generic affine relation?
    if (end_exp.var == kNoIntegerVariable || end_exp.coeff != 1) continue;

    window[new_size++] = task_time;
    index_to_end_vars_.push_back(end_exp.var);
  }
  window = window.subspan(0, new_size);

  // Because we use the cached value in the window, we don't really care
  // on which order we process them.
  precedence_relations_->CollectPrecedences(index_to_end_vars_, &before_);

  const int size = before_.size();
  for (int global_i = 0; global_i < size;) {
    const int global_start_i = global_i;
    const IntegerVariable var = before_[global_i].var;
    DCHECK_NE(var, kNoIntegerVariable);

    // Decode the set of task before var.
    // Note that like in Propagate() we split this set of task into critical
    // subpart as there is no point considering them together.
    //
    // TODO(user): If more than one set of task push the same variable, we
    // probably only want to keep the best push? Maybe we want to process them
    // in reverse order of what we do here?
    indices_before_.clear();
    IntegerValue local_start;
    IntegerValue local_end;
    for (; global_i < size; ++global_i) {
      const EnforcedLinear2Bounds::PrecedenceData& data = before_[global_i];
      if (data.var != var) break;
      const int index = data.var_index;
      const auto [t, start_of_t] = window[index];
      if (global_i == global_start_i) {  // First loop.
        local_start = start_of_t;
        local_end = local_start + helper_->SizeMin(t);
      } else {
        if (start_of_t >= local_end) break;
        local_end += helper_->SizeMin(t);
      }
      indices_before_.push_back({index, data.lin2_index});
    }

    // No need to consider if we don't have at least two tasks before var.
    const int num_before = indices_before_.size();
    if (num_before < 2) continue;
    skip_.assign(num_before, false);

    // Heuristic.
    // We will use the current end-min of all the task in indices_before_
    // to skip task with an offset not large enough.
    IntegerValue end_min_when_all_present = local_end;

    // We will consider the end-min of all the subsets [i, num_items) to try to
    // push var using the min-offset between var and items of such subset. This
    // can be done in linear time by scanning from i = num_items - 1 to 0.
    //
    // Note that this needs the items in indices_before_ to be sorted by
    // their shifted start min (it should be the case).
    int best_index = -1;
    const IntegerValue current_var_lb = integer_trail_.LowerBound(var);
    IntegerValue best_new_lb = current_var_lb;
    IntegerValue min_offset_at_best = kMinIntegerValue;
    IntegerValue min_offset = kMaxIntegerValue;
    IntegerValue sum_of_duration = 0;
    for (int i = num_before; --i >= 0;) {
      const auto [task_index, lin2_index] = indices_before_[i];
      const TaskTime task_time = window[task_index];
      const AffineExpression& end_exp = helper_->Ends()[task_time.task_index];

      // TODO(user): The lookup here is a bit slow, so we avoid fetching
      // the offset as much as possible.
      ++num_hash_lookup;
      const IntegerValue inner_offset =
          -linear2_bounds_->NonTrivialUpperBound(lin2_index);
      DCHECK_NE(inner_offset, kMinIntegerValue);

      // We have var >= end_exp.var + inner_offset, so
      // var >= (end_exp.var + end_exp.constant)
      //        + (inner_offset - end_exp.constant)
      // var >= task end + offset.
      const IntegerValue offset = inner_offset - end_exp.constant;

      // Heuristic: do not consider this relations if its offset is clearly bad.
      const IntegerValue task_size = helper_->SizeMin(task_time.task_index);
      if (end_min_when_all_present + offset <= best_new_lb) {
        // This is true if we skipped all task so far in this block.
        if (min_offset == kMaxIntegerValue) {
          // If only one task is left, we can abort.
          // This avoid a linear2_bounds_ lookup.
          if (i == 1) break;

          // Lower the end_min_when_all_present for better filtering later.
          end_min_when_all_present -= task_size;
        }

        skip_[i] = true;
        continue;
      }

      // Add this task to the current subset and compute the new bound.
      min_offset = std::min(min_offset, offset);
      sum_of_duration += task_size;
      const IntegerValue start = task_time.time;
      const IntegerValue new_lb = start + sum_of_duration + min_offset;
      if (new_lb > best_new_lb) {
        min_offset_at_best = min_offset;
        best_new_lb = new_lb;
        best_index = i;
      }
    }

    // Push?
    if (best_new_lb > current_var_lb) {
      DCHECK_NE(best_index, -1);
      helper_->ResetReason();
      const IntegerValue window_start =
          window[indices_before_[best_index].first].time;
      for (int i = best_index; i < num_before; ++i) {
        if (skip_[i]) continue;
        const int ct = window[indices_before_[i].first].task_index;
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, helper_->SizeMin(ct), window_start);

        // Fetch the explanation of (var - end) >= min_offset
        // This is okay if a bit slow since we only do that when we push.
        const auto [expr, ub] = EncodeDifferenceLowerThan(
            helper_->Ends()[ct], var, -min_offset_at_best);
        helper_->AddReasonForUpperBoundLowerThan(expr, ub);
      }
      ++stats_.num_propagations;
      if (!helper_->PushIntegerLiteral(
              IntegerLiteral::GreaterOrEqual(var, best_new_lb))) {
        return false;
      }
    }
  }
  return true;
}

int DisjunctivePrecedences::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator reach the fixed point in one go.
  // Maybe not in corner cases, but since it is expansive, it is okay not to
  // run it again right away
  //
  // Note also that technically, we don't need to be waked up if only the upper
  // bound of the task changes, but this require to use more memory and the gain
  // is unclear as this runs with the highest priority.
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id);
  return id;
}

bool DisjunctiveNotLast::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) {
    ++stats_.num_conflicts;
    return false;
  }

  const auto task_by_negated_start_max =
      helper_->TaskByIncreasingNegatedStartMax();
  const auto task_by_increasing_shifted_start_min =
      helper_->TaskByIncreasingShiftedStartMin();

  TaskSet task_set(helper_->GetScratchTaskSet());
  FixedCapacityVector<TaskTime>& start_min_window =
      helper_->GetScratchTaskTimeVector1();
  FixedCapacityVector<TaskTime>& start_max_window =
      helper_->GetScratchTaskTimeVector2();

  // Split problem into independent part.
  //
  // The situation is trickier here, and we use two windows:
  // - The classical "start_min_window_" as in the other propagator.
  // - A second window, that includes all the task with a start_max inside
  //   [window_start, window_end].
  //
  // Now, a task from the second window can be detected to be "not last" by only
  // looking at the task in the first window. Tasks to the left do not cause
  // issue for the task to be last, and tasks to the right will not lower the
  // end-min of the task under consideration.
  int queue_index = task_by_negated_start_max.size() - 1;
  const int num_tasks = task_by_increasing_shifted_start_min.size();
  for (int i = 0; i < num_tasks;) {
    start_min_window.clear();
    IntegerValue window_end = kMinIntegerValue;
    for (; i < num_tasks; ++i) {
      const auto [task, presence_lit, start_min] =
          task_by_increasing_shifted_start_min[i];
      if (!helper_->IsPresent(presence_lit)) continue;

      if (start_min_window.empty()) {
        start_min_window.push_back({task, start_min});
        window_end = start_min + helper_->SizeMin(task);
      } else if (start_min < window_end) {
        start_min_window.push_back({task, start_min});
        window_end += helper_->SizeMin(task);
      } else {
        break;
      }
    }

    // Add to start_max_window_ all the task whose start_max
    // fall into [window_start, window_end).
    start_max_window.clear();
    for (; queue_index >= 0; queue_index--) {
      const auto [t, negated_start_max] =
          task_by_negated_start_max[queue_index];
      const IntegerValue start_max = -negated_start_max;

      // Note that we add task whose presence is still unknown here.
      if (start_max >= window_end) break;
      if (helper_->IsAbsent(t)) continue;
      start_max_window.push_back({t, start_max});
    }

    // If this is the case, we cannot propagate more than the detectable
    // precedence propagator. Note that this continue must happen after we
    // computed start_max_window_ though.
    if (start_min_window.size() <= 1) continue;

    // Process current window.
    if (!start_max_window.empty() &&
        !PropagateSubwindow(task_set, absl::MakeSpan(start_min_window),
                            absl::MakeSpan(start_max_window))) {
      ++stats_.num_conflicts;
      return false;
    }
  }

  stats_.EndWithoutConflicts();
  return true;
}

bool DisjunctiveNotLast::PropagateSubwindow(
    TaskSet& task_set, absl::Span<TaskTime> task_by_increasing_start_max,
    absl::Span<TaskTime> task_by_increasing_end_max) {
  for (TaskTime& entry : task_by_increasing_end_max) {
    entry.time = helper_->EndMax(entry.task_index);
  }
  IncrementalSort(task_by_increasing_end_max.begin(),
                  task_by_increasing_end_max.end());

  const IntegerValue threshold = task_by_increasing_end_max.back().time;
  int queue_size = 0;
  for (const TaskTime entry : task_by_increasing_start_max) {
    const int task = entry.task_index;
    const IntegerValue start_max = helper_->StartMax(task);
    DCHECK(helper_->IsPresent(task));
    if (start_max < threshold) {
      task_by_increasing_start_max[queue_size++] = {task, start_max};
    }
  }

  // If the size is one, we cannot propagate more than the detectable precedence
  // propagator.
  if (queue_size <= 1) return true;

  task_by_increasing_start_max =
      task_by_increasing_start_max.subspan(0, queue_size);
  std::sort(task_by_increasing_start_max.begin(),
            task_by_increasing_start_max.end());

  task_set.Clear();
  int queue_index = 0;
  for (const auto task_time : task_by_increasing_end_max) {
    const int t = task_time.task_index;
    const IntegerValue end_max = task_time.time;

    // We filtered absent task before, but it is possible that as we push
    // bounds of optional tasks, more task become absent.
    if (helper_->IsAbsent(t)) continue;

    // task_set_ contains all the tasks that must start before the end-max of t.
    // These are the only candidates that have a chance to decrease the end-max
    // of t.
    while (queue_index < queue_size) {
      const auto to_insert = task_by_increasing_start_max[queue_index];
      const IntegerValue start_max = to_insert.time;
      if (end_max <= start_max) break;

      const int task_index = to_insert.task_index;
      DCHECK(helper_->IsPresent(task_index));
      task_set.AddEntry({task_index, helper_->ShiftedStartMin(task_index),
                         helper_->SizeMin(task_index)});
      ++queue_index;
    }

    // In the following case, task t cannot be after all the critical tasks
    // (i.e. it cannot be last):
    //
    // [(critical tasks)
    //              | <- t start-max
    //
    // So we can deduce that the end-max of t is smaller than or equal to the
    // largest start-max of the critical tasks.
    //
    // Note that this works as well when the presence of t is still unknown.
    int critical_index = 0;
    const IntegerValue end_min_of_critical_tasks =
        task_set.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (end_min_of_critical_tasks <= helper_->StartMax(t)) continue;

    // Find the largest start-max of the critical tasks (excluding t). The
    // end-max for t need to be smaller than or equal to this.
    IntegerValue largest_ct_start_max = kMinIntegerValue;
    const absl::Span<const TaskSet::Entry> sorted_tasks =
        task_set.SortedTasks();
    const int sorted_tasks_size = sorted_tasks.size();
    for (int i = critical_index; i < sorted_tasks_size; ++i) {
      const int ct = sorted_tasks[i].task;
      if (t == ct) continue;
      const IntegerValue start_max = helper_->StartMax(ct);

      // If we already known that t is after ct we can have a tighter start-max.
      if (start_max > largest_ct_start_max &&
          helper_->GetCurrentMinDistanceBetweenTasks(ct, t) < 0) {
        largest_ct_start_max = start_max;
      }
    }

    // If we have any critical task, the test will always be true because
    // of the tasks we put in task_set_.
    DCHECK(largest_ct_start_max == kMinIntegerValue ||
           end_max > largest_ct_start_max);
    if (end_max > largest_ct_start_max) {
      helper_->ResetReason();

      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      for (int i = critical_index; i < sorted_tasks_size; ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min,
                                      window_start);
        if (helper_->GetCurrentMinDistanceBetweenTasks(ct, t) >= 0) {
          helper_->AddReasonForBeingBeforeAssumingNoOverlap(ct, t);
        } else {
          helper_->AddStartMaxReason(ct, largest_ct_start_max);
        }
      }

      // Add the reason for t, we only need the start-max.
      helper_->AddStartMaxReason(t, end_min_of_critical_tasks - 1);

      // If largest_ct_start_max == kMinIntegerValue, we have a conflict. To
      // avoid integer overflow, we report it directly. This might happen
      // because the task is known to be after all the other, and thus it cannot
      // be "not last".
      if (largest_ct_start_max == kMinIntegerValue) {
        return helper_->ReportConflict();
      }

      // Enqueue the new end-max for t.
      // Note that changing it will not influence the rest of the loop.
      ++stats_.num_propagations;
      if (!helper_->DecreaseEndMax(t, largest_ct_start_max)) return false;
    }
  }
  return true;
}

int DisjunctiveNotLast::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctiveEdgeFinding::Propagate() {
  if (!helper_->IsEnforced()) return true;
  stats_.OnPropagate();
  const int num_tasks = helper_->NumTasks();
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) {
    ++stats_.num_conflicts;
    return false;
  }
  is_gray_.resize(num_tasks, false);
  non_gray_task_to_event_.resize(num_tasks);

  FixedCapacityVector<TaskTime>& window = helper_->GetScratchTaskTimeVector1();

  // This only contains non-gray tasks.
  FixedCapacityVector<TaskTime>& task_by_increasing_end_max =
      helper_->GetScratchTaskTimeVector2();

  IntegerValue window_end = kMinIntegerValue;
  for (const auto [task, presence_lit, shifted_smin] :
       helper_->TaskByIncreasingShiftedStartMin()) {
    if (helper_->IsAbsent(presence_lit)) continue;

    // Note that we use the real start min here not the shifted one. This is
    // because we might be able to push it if it is smaller than window end.
    if (helper_->StartMin(task) < window_end) {
      window.push_back({task, shifted_smin});
      window_end += helper_->SizeMin(task);
      continue;
    }

    // We need at least 3 tasks for the edge-finding to be different from
    // detectable precedences.
    if (window.size() > 2 &&
        !PropagateSubwindow(window_end, absl::MakeSpan(window),
                            task_by_increasing_end_max)) {
      ++stats_.num_conflicts;
      return false;
    }

    // Corner case: The propagation of the previous window might have made the
    // current task absent even if it wasn't at the loop beginning.
    if (helper_->IsAbsent(presence_lit)) {
      window.clear();
      window_end = kMinIntegerValue;
      continue;
    }

    // Start of the next window.
    window.clear();
    window.push_back({task, shifted_smin});
    window_end = shifted_smin + helper_->SizeMin(task);
  }
  if (window.size() > 2 &&
      !PropagateSubwindow(window_end, absl::MakeSpan(window),
                          task_by_increasing_end_max)) {
    ++stats_.num_conflicts;
    return false;
  }

  stats_.EndWithoutConflicts();
  return true;
}

bool DisjunctiveEdgeFinding::PropagateSubwindow(
    IntegerValue window_end_min, absl::Span<const TaskTime> window,
    FixedCapacityVector<TaskTime>& task_by_increasing_end_max) {
  // Cache the task end-max and abort early if possible.
  task_by_increasing_end_max.clear();
  for (const auto task_time : window) {
    const int task = task_time.task_index;
    DCHECK(!helper_->IsAbsent(task));

    // We already mark all the non-present task as gray.
    //
    // Same for task with an end-max that is too large: Tasks that are not
    // present can never trigger propagation or an overload checking failure.
    // theta_tree_.GetOptionalEnvelope() is always <= window_end, so tasks whose
    // end_max is >= window_end can never trigger propagation or failure either.
    // Thus, those tasks can be marked as gray, which removes their contribution
    // to theta right away.
    const IntegerValue end_max = helper_->EndMax(task);
    if (helper_->IsPresent(task) && end_max < window_end_min) {
      is_gray_[task] = false;
      task_by_increasing_end_max.push_back({task, end_max});
    } else {
      is_gray_[task] = true;
    }
  }

  // If we have just 1 non-gray task, then this propagator does not propagate
  // more than the detectable precedences, so we abort early.
  if (task_by_increasing_end_max.size() < 2) return true;
  std::sort(task_by_increasing_end_max.begin(),
            task_by_increasing_end_max.end());

  // Set up theta tree.
  //
  // Some task in the theta tree will be considered "gray".
  // When computing the end-min of the sorted task, we will compute it for:
  // - All the non-gray task
  // - All the non-gray task + at most one gray task.
  //
  // TODO(user): it should be faster to initialize it all at once rather
  // than calling AddOrUpdate() n times.
  const int window_size = window.size();
  event_size_.clear();
  theta_tree_.Reset(window_size);
  for (int event = 0; event < window_size; ++event) {
    const TaskTime task_time = window[event];
    const int task = task_time.task_index;
    const IntegerValue energy_min = helper_->SizeMin(task);
    event_size_.push_back(energy_min);
    if (is_gray_[task]) {
      theta_tree_.AddOrUpdateOptionalEvent(event, task_time.time, energy_min);
    } else {
      non_gray_task_to_event_[task] = event;
      theta_tree_.AddOrUpdateEvent(event, task_time.time, energy_min,
                                   energy_min);
    }
  }

  // At each iteration we either transform a non-gray task into a gray one or
  // remove a gray task, so this loop is linear in complexity.
  while (true) {
    DCHECK(!is_gray_[task_by_increasing_end_max.back().task_index]);
    const IntegerValue non_gray_end_max =
        task_by_increasing_end_max.back().time;

    // Overload checking.
    const IntegerValue non_gray_end_min = theta_tree_.GetEnvelope();
    if (non_gray_end_min > non_gray_end_max) {
      helper_->ResetReason();

      // We need the reasons for the critical tasks to fall in:
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(non_gray_end_max);
      const IntegerValue window_start = window[critical_event].time;
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < window_size; event++) {
        const int task = window[event].task_index;
        if (is_gray_[task]) continue;
        helper_->AddPresenceReason(task);
        helper_->AddEnergyAfterReason(task, event_size_[event], window_start);
        helper_->AddEndMaxReason(task, window_end);
      }
      return helper_->ReportConflict();
    }

    // Edge-finding.
    // If we have a situation like:
    //     [(critical_task_with_gray_task)
    //                           ]
    //                           ^ end-max without the gray task.
    //
    // Then the gray task must be after all the critical tasks (all the non-gray
    // tasks in the tree actually), otherwise there will be no way to schedule
    // the critical_tasks inside their time window.
    while (theta_tree_.GetOptionalEnvelope() > non_gray_end_max) {
      const IntegerValue end_min_with_gray = theta_tree_.GetOptionalEnvelope();
      int critical_event_with_gray;
      int gray_event;
      IntegerValue available_energy;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          non_gray_end_max, &critical_event_with_gray, &gray_event,
          &available_energy);
      const int gray_task = window[gray_event].task_index;
      DCHECK(is_gray_[gray_task]);

      // This might happen in the corner case where more than one interval are
      // controlled by the same Boolean.
      if (helper_->IsAbsent(gray_task)) {
        theta_tree_.RemoveEvent(gray_event);
        continue;
      }

      // Since the gray task is after all the other, we have a new lower bound.
      if (helper_->StartMin(gray_task) < non_gray_end_min) {
        // The API is not ideal here. We just want the start of the critical
        // tasks that explain the non_gray_end_min computed above.
        const int critical_event =
            theta_tree_.GetMaxEventWithEnvelopeGreaterThan(non_gray_end_min -
                                                           1);

        // Even if we need less task to explain the overload, because we are
        // going to explain the full non_gray_end_min, we can relax the
        // explanation.
        if (critical_event_with_gray > critical_event) {
          critical_event_with_gray = critical_event;
        }

        const int first_event = critical_event_with_gray;
        const int second_event = critical_event;
        const IntegerValue first_start = window[first_event].time;
        const IntegerValue second_start = window[second_event].time;

        // window_end is chosen to be has big as possible and still have an
        // overload if the gray task is not last.
        //
        // If gray_task is with variable duration, it is possible that it must
        // be last just because is end-min is large.
        bool use_energy_reason = true;
        IntegerValue window_end;
        if (helper_->EndMin(gray_task) > non_gray_end_max) {
          // This is actually a form of detectable precedence.
          //
          // TODO(user): We could relax the reason more, but this happen really
          // rarely it seems.
          use_energy_reason = false;
          window_end = helper_->EndMin(gray_task) - 1;
        } else {
          window_end = end_min_with_gray - 1;
        }
        CHECK_GE(window_end, non_gray_end_max);

        // The non-gray part of the explanation as detailed above.
        helper_->ResetReason();
        bool all_before = true;
        for (int event = first_event; event < window_size; event++) {
          const int task = window[event].task_index;
          if (is_gray_[task]) continue;
          helper_->AddPresenceReason(task);
          helper_->AddEnergyAfterReason(
              task, event_size_[event],
              event >= second_event ? second_start : first_start);

          const IntegerValue dist =
              helper_->GetCurrentMinDistanceBetweenTasks(task, gray_task);
          if (dist >= 0) {
            helper_->AddReasonForBeingBeforeAssumingNoOverlap(task, gray_task);
          } else {
            all_before = false;
            helper_->AddEndMaxReason(task, window_end);
          }
        }

        // Add the reason for the gray_task (we don't need the end-max or
        // presence reason) needed for the precedences.
        if (!all_before) {
          if (use_energy_reason) {
            helper_->AddEnergyAfterReason(gray_task, event_size_[gray_event],
                                          first_start);
          } else {
            helper_->AddEndMinReason(gray_task, helper_->EndMin(gray_task));
          }
        }

        // Process detected precedence.
        if (helper_->CurrentDecisionLevel() == 0 &&
            helper_->IsPresent(gray_task)) {
          for (int i = first_event; i < window_size; ++i) {
            const int task = window[i].task_index;
            if (!is_gray_[task]) {
              if (!helper_->NotifyLevelZeroPrecedence(task, gray_task)) {
                return false;
              }
            }
          }
        }

        // Enqueue the new start-min for gray_task.
        //
        // TODO(user): propagate the precedence Boolean here too? I think it
        // will be more powerful. Even if eventually all these precedence will
        // become detectable (see Petr Villim PhD).
        ++stats_.num_propagations;
        if (!helper_->IncreaseStartMin(gray_task, non_gray_end_min)) {
          return false;
        }
      }

      // Remove the gray_task.
      theta_tree_.RemoveEvent(gray_event);
    }

    // Stop before we get just one non-gray task.
    if (task_by_increasing_end_max.size() <= 2) break;

    // Stop if the min of end_max is too big.
    if (task_by_increasing_end_max[0].time >=
        theta_tree_.GetOptionalEnvelope()) {
      break;
    }

    // Make the non-gray task with larger end-max gray.
    const int new_gray_task = task_by_increasing_end_max.back().task_index;
    task_by_increasing_end_max.pop_back();
    const int new_gray_event = non_gray_task_to_event_[new_gray_task];
    DCHECK(!is_gray_[new_gray_task]);
    is_gray_[new_gray_task] = true;
    theta_tree_.AddOrUpdateOptionalEvent(new_gray_event,
                                         window[new_gray_event].time,
                                         event_size_[new_gray_event]);
  }

  return true;
}

int DisjunctiveEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

}  // namespace sat
}  // namespace operations_research
