// Copyright 2010-2021 Google LLC
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

#include <memory>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/timetable.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Disjunctive(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    bool is_all_different = true;
    IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
    for (const IntervalVariable var : vars) {
      if (repository->IsOptional(var) || repository->MinSize(var) != 1 ||
          repository->MaxSize(var) != 1 ||
          repository->Start(var).constant != 0 ||
          repository->Start(var).coeff != 1) {
        is_all_different = false;
        break;
      }
    }
    if (is_all_different) {
      std::vector<IntegerVariable> starts;
      starts.reserve(vars.size());
      for (const IntervalVariable var : vars) {
        starts.push_back(model->Get(StartVar(var)));
      }
      model->Add(AllDifferentOnBounds(starts));
      return;
    }

    auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
    const auto& sat_parameters = *model->GetOrCreate<SatParameters>();
    if (vars.size() > 2 && sat_parameters.use_combined_no_overlap()) {
      model->GetOrCreate<CombinedDisjunctive<true>>()->AddNoOverlap(vars);
      model->GetOrCreate<CombinedDisjunctive<false>>()->AddNoOverlap(vars);
      return;
    }

    SchedulingConstraintHelper* helper =
        new SchedulingConstraintHelper(vars, model);
    model->TakeOwnership(helper);

    // Experiments to use the timetable only to propagate the disjunctive.
    if (/*DISABLES_CODE*/ (false)) {
      const AffineExpression one(IntegerValue(1));
      std::vector<AffineExpression> demands(vars.size(), one);
      TimeTablingPerTask* timetable = new TimeTablingPerTask(
          demands, one, model->GetOrCreate<IntegerTrail>(), helper);
      timetable->RegisterWith(watcher);
      model->TakeOwnership(timetable);
      return;
    }

    if (vars.size() == 2) {
      DisjunctiveWithTwoItems* propagator = new DisjunctiveWithTwoItems(helper);
      propagator->RegisterWith(watcher);
      model->TakeOwnership(propagator);
    } else {
      // We decided to create the propagators in this particular order, but it
      // shouldn't matter much because of the different priorities used.
      {
        // Only one direction is needed by this one.
        DisjunctiveOverloadChecker* overload_checker =
            new DisjunctiveOverloadChecker(helper);
        const int id = overload_checker->RegisterWith(watcher);
        watcher->SetPropagatorPriority(id, 1);
        model->TakeOwnership(overload_checker);
      }
      for (const bool time_direction : {true, false}) {
        DisjunctiveDetectablePrecedences* detectable_precedences =
            new DisjunctiveDetectablePrecedences(time_direction, helper);
        const int id = detectable_precedences->RegisterWith(watcher);
        watcher->SetPropagatorPriority(id, 2);
        model->TakeOwnership(detectable_precedences);
      }
      for (const bool time_direction : {true, false}) {
        DisjunctiveNotLast* not_last =
            new DisjunctiveNotLast(time_direction, helper);
        const int id = not_last->RegisterWith(watcher);
        watcher->SetPropagatorPriority(id, 3);
        model->TakeOwnership(not_last);
      }
      for (const bool time_direction : {true, false}) {
        DisjunctiveEdgeFinding* edge_finding =
            new DisjunctiveEdgeFinding(time_direction, helper);
        const int id = edge_finding->RegisterWith(watcher);
        watcher->SetPropagatorPriority(id, 4);
        model->TakeOwnership(edge_finding);
      }
    }

    // Note that we keep this one even when there is just two intervals. This is
    // because it might push a variable that is after both of the intervals
    // using the fact that they are in disjunction.
    if (sat_parameters.use_precedences_in_disjunctive_constraint() &&
        !sat_parameters.use_combined_no_overlap()) {
      for (const bool time_direction : {true, false}) {
        DisjunctivePrecedences* precedences = new DisjunctivePrecedences(
            time_direction, helper, model->GetOrCreate<IntegerTrail>(),
            model->GetOrCreate<PrecedencesPropagator>());
        const int id = precedences->RegisterWith(watcher);
        watcher->SetPropagatorPriority(id, 5);
        model->TakeOwnership(precedences);
      }
    }
  };
}

std::function<void(Model*)> DisjunctiveWithBooleanPrecedencesOnly(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    for (int i = 0; i < vars.size(); ++i) {
      for (int j = 0; j < i; ++j) {
        const BooleanVariable boolean_var = sat_solver->NewBooleanVariable();
        const Literal i_before_j = Literal(boolean_var, true);
        const Literal j_before_i = i_before_j.Negated();
        precedences->AddConditionalPrecedence(repository->EndVar(vars[i]),
                                              repository->StartVar(vars[j]),
                                              i_before_j);
        precedences->AddConditionalPrecedence(repository->EndVar(vars[j]),
                                              repository->StartVar(vars[i]),
                                              j_before_i);
      }
    }
  };
}

std::function<void(Model*)> DisjunctiveWithBooleanPrecedences(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    model->Add(DisjunctiveWithBooleanPrecedencesOnly(vars));
    model->Add(Disjunctive(vars));
  };
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
      sorted_tasks_.erase(sorted_tasks_.begin() + i);
      break;
    }
  }

  optimized_restart_ = sorted_tasks_.size();
  sorted_tasks_.push_back(e);
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
}

void TaskSet::RemoveEntryWithIndex(int index) {
  sorted_tasks_.erase(sorted_tasks_.begin() + index);
  optimized_restart_ = 0;
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
  if (!helper_->SynchronizeAndSetTimeDirection(true)) return false;

  // We can't propagate anything if one of the interval is absent for sure.
  if (helper_->IsAbsent(0) || helper_->IsAbsent(1)) return true;

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
  if (helper_->StartMax(0) < helper_->EndMin(1)) {
    // Task 0 must be before task 1.
  } else if (helper_->StartMax(1) < helper_->EndMin(0)) {
    // Task 1 must be before task 0.
    std::swap(task_before, task_after);
  } else {
    return true;
  }

  if (helper_->IsPresent(task_before)) {
    const IntegerValue end_min_before = helper_->EndMin(task_before);
    if (helper_->StartMin(task_after) < end_min_before) {
      // Reason for precedences if both present.
      helper_->ClearReason();
      helper_->AddReasonForBeingBefore(task_before, task_after);

      // Reason for the bound push.
      helper_->AddPresenceReason(task_before);
      helper_->AddEndMinReason(task_before, end_min_before);
      if (!helper_->IncreaseStartMin(task_after, end_min_before)) {
        return false;
      }
    }
  }

  if (helper_->IsPresent(task_after)) {
    const IntegerValue start_max_after = helper_->StartMax(task_after);
    if (helper_->EndMax(task_before) > start_max_after) {
      // Reason for precedences if both present.
      helper_->ClearReason();
      helper_->AddReasonForBeingBefore(task_before, task_after);

      // Reason for the bound push.
      helper_->AddPresenceReason(task_after);
      helper_->AddStartMaxReason(task_after, start_max_after);
      if (!helper_->DecreaseEndMax(task_before, start_max_after)) {
        return false;
      }
    }
  }

  return true;
}

int DisjunctiveWithTwoItems::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator reach the fix point in one pass.
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  return id;
}

template <bool time_direction>
CombinedDisjunctive<time_direction>::CombinedDisjunctive(Model* model)
    : helper_(model->GetOrCreate<AllIntervalsHelper>()) {
  task_to_disjunctives_.resize(helper_->NumTasks());

  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher, /*watch_start_max=*/true,
                         /*watch_end_max=*/false);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

template <bool time_direction>
void CombinedDisjunctive<time_direction>::AddNoOverlap(
    const std::vector<IntervalVariable>& vars) {
  const int index = task_sets_.size();
  task_sets_.emplace_back(vars.size());
  end_mins_.push_back(kMinIntegerValue);
  for (const IntervalVariable var : vars) {
    task_to_disjunctives_[var.value()].push_back(index);
  }
}

template <bool time_direction>
bool CombinedDisjunctive<time_direction>::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction)) return false;
  const auto& task_by_increasing_end_min = helper_->TaskByIncreasingEndMin();
  const auto& task_by_decreasing_start_max =
      helper_->TaskByDecreasingStartMax();

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
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
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
    helper_->ClearReason();
    const std::vector<TaskSet::Entry>& sorted_tasks =
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
  if (!helper_->SynchronizeAndSetTimeDirection(/*is_forward=*/true))
    return false;

  // Split problem into independent part.
  //
  // Many propagators in this file use the same approach, we start by processing
  // the task by increasing start-min, packing everything to the left. We then
  // process each "independent" set of task separately. A task is independent
  // from the one before it, if its start-min wasn't pushed.
  //
  // This way, we get one or more window [window_start, window_end] so that for
  // all task in the window, [start_min, end_min] is inside the window, and the
  // end min of any set of task to the left is <= window_start, and the
  // start_min of any task to the right is >= end_min.
  window_.clear();
  IntegerValue window_end = kMinIntegerValue;
  IntegerValue relevant_end;
  int relevant_size = 0;
  for (const TaskTime task_time : helper_->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task)) continue;

    const IntegerValue start_min = task_time.time;
    if (start_min < window_end) {
      window_.push_back(task_time);
      window_end += helper_->SizeMin(task);
      if (window_end > helper_->EndMax(task)) {
        relevant_size = window_.size();
        relevant_end = window_end;
      }
      continue;
    }

    // Process current window.
    // We don't need to process the end of the window (after relevant_size)
    // because these interval can be greedily assembled in a feasible solution.
    window_.resize(relevant_size);
    if (relevant_size > 0 && !PropagateSubwindow(relevant_end)) {
      return false;
    }

    // Start of the next window.
    window_.clear();
    window_.push_back(task_time);
    window_end = start_min + helper_->SizeMin(task);
    relevant_size = 0;
  }

  // Process last window.
  window_.resize(relevant_size);
  if (relevant_size > 0 && !PropagateSubwindow(relevant_end)) {
    return false;
  }

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
    IntegerValue global_window_end) {
  // Set up theta tree and task_by_increasing_end_max_.
  const int window_size = window_.size();
  theta_tree_.Reset(window_size);
  task_by_increasing_end_max_.clear();
  for (int i = 0; i < window_size; ++i) {
    // No point adding a task if its end_max is too large.
    const int task = window_[i].task_index;
    const IntegerValue end_max = helper_->EndMax(task);
    if (end_max < global_window_end) {
      task_to_event_[task] = i;
      task_by_increasing_end_max_.push_back({task, end_max});
    }
  }

  // Introduce events by increasing end_max, check for overloads.
  std::sort(task_by_increasing_end_max_.begin(),
            task_by_increasing_end_max_.end());
  for (const auto task_time : task_by_increasing_end_max_) {
    const int current_task = task_time.task_index;

    // We filtered absent task while constructing the subwindow, but it is
    // possible that as we propagate task absence below, other task also become
    // absent (if they share the same presence Boolean).
    if (helper_->IsAbsent(current_task)) continue;

    DCHECK_NE(task_to_event_[current_task], -1);
    {
      const int current_event = task_to_event_[current_task];
      const IntegerValue energy_min = helper_->SizeMin(current_task);
      if (helper_->IsPresent(current_task)) {
        // TODO(user): Add max energy deduction for variable
        // sizes by putting the energy_max here and modifying the code
        // dealing with the optional envelope greater than current_end below.
        theta_tree_.AddOrUpdateEvent(current_event, window_[current_event].time,
                                     energy_min, energy_min);
      } else {
        theta_tree_.AddOrUpdateOptionalEvent(
            current_event, window_[current_event].time, energy_min);
      }
    }

    const IntegerValue current_end = task_time.time;
    if (theta_tree_.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      helper_->ClearReason();
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = window_[critical_event].time;
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < window_size; event++) {
        const IntegerValue energy_min = theta_tree_.EnergyMin(event);
        if (energy_min > 0) {
          const int task = window_[event].task_index;
          helper_->AddPresenceReason(task);
          helper_->AddEnergyAfterReason(task, energy_min, window_start);
          helper_->AddEndMaxReason(task, window_end);
        }
      }
      return helper_->ReportConflict();
    }

    // Exclude all optional tasks that would overload an interval ending here.
    while (theta_tree_.GetOptionalEnvelope() > current_end) {
      // Explain exclusion with tasks present in the critical interval.
      // TODO(user): This could be done lazily, like most of the loop to
      // compute the reasons in this file.
      helper_->ClearReason();
      int critical_event;
      int optional_event;
      IntegerValue available_energy;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          current_end, &critical_event, &optional_event, &available_energy);

      const int optional_task = window_[optional_event].task_index;

      // If tasks shares the same presence literal, it is possible that we
      // already pushed this task absence.
      if (!helper_->IsAbsent(optional_task)) {
        const IntegerValue optional_size_min = helper_->SizeMin(optional_task);
        const IntegerValue window_start = window_[critical_event].time;
        const IntegerValue window_end =
            current_end + optional_size_min - available_energy - 1;
        for (int event = critical_event; event < window_size; event++) {
          const IntegerValue energy_min = theta_tree_.EnergyMin(event);
          if (energy_min > 0) {
            const int task = window_[event].task_index;
            helper_->AddPresenceReason(task);
            helper_->AddEnergyAfterReason(task, energy_min, window_start);
            helper_->AddEndMaxReason(task, window_end);
          }
        }

        helper_->AddEnergyAfterReason(optional_task, optional_size_min,
                                      window_start);
        helper_->AddEndMaxReason(optional_task, window_end);

        if (!helper_->PushTaskAbsence(optional_task)) return false;
      }

      theta_tree_.RemoveEvent(optional_event);
    }
  }

  return true;
}

int DisjunctiveOverloadChecker::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator reach the fix point in one pass.
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(/*is_forward=*/true);
  helper_->WatchAllTasks(id, watcher, /*watch_start_max=*/false,
                         /*watch_end_max=*/true);
  return id;
}

bool DisjunctiveDetectablePrecedences::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) return false;

  to_propagate_.clear();
  processed_.assign(helper_->NumTasks(), false);

  // Split problem into independent part.
  //
  // The "independent" window can be processed separately because for each of
  // them, a task [start-min, end-min] is in the window [window_start,
  // window_end]. So any task to the left of the window cannot push such
  // task start_min, and any task to the right of the window will have a
  // start_max >= end_min, so wouldn't be in detectable precedence.
  task_by_increasing_end_min_.clear();
  IntegerValue window_end = kMinIntegerValue;
  for (const TaskTime task_time : helper_->TaskByIncreasingStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task)) continue;

    // Note that the helper returns value assuming the task is present.
    const IntegerValue start_min = helper_->StartMin(task);
    const IntegerValue size_min = helper_->SizeMin(task);
    const IntegerValue end_min = helper_->EndMin(task);
    DCHECK_GE(end_min, start_min + size_min);

    if (start_min < window_end) {
      task_by_increasing_end_min_.push_back({task, end_min});
      window_end = std::max(window_end, start_min) + size_min;
      continue;
    }

    // Process current window.
    if (task_by_increasing_end_min_.size() > 1 && !PropagateSubwindow()) {
      return false;
    }

    // Start of the next window.
    task_by_increasing_end_min_.clear();
    task_by_increasing_end_min_.push_back({task, end_min});
    window_end = end_min;
  }

  if (task_by_increasing_end_min_.size() > 1 && !PropagateSubwindow()) {
    return false;
  }

  return true;
}

bool DisjunctiveDetectablePrecedences::PropagateSubwindow() {
  DCHECK(!task_by_increasing_end_min_.empty());

  // The vector is already sorted by shifted_start_min, so there is likely a
  // good correlation, hence the incremental sort.
  IncrementalSort(task_by_increasing_end_min_.begin(),
                  task_by_increasing_end_min_.end());
  const IntegerValue max_end_min = task_by_increasing_end_min_.back().time;

  // Fill and sort task_by_increasing_start_max_.
  //
  // TODO(user): we should use start max if present, but more generally, all
  // helper function should probably return values "if present".
  task_by_increasing_start_max_.clear();
  for (const TaskTime entry : task_by_increasing_end_min_) {
    const int task = entry.task_index;
    const IntegerValue start_max = helper_->StartMax(task);
    if (start_max < max_end_min && helper_->IsPresent(task)) {
      task_by_increasing_start_max_.push_back({task, start_max});
    }
  }
  if (task_by_increasing_start_max_.empty()) return true;
  std::sort(task_by_increasing_start_max_.begin(),
            task_by_increasing_start_max_.end());

  // Invariant: need_update is false implies that task_set_end_min is equal to
  // task_set_.ComputeEndMin().
  //
  // TODO(user): Maybe it is just faster to merge ComputeEndMin() with
  // AddEntry().
  task_set_.Clear();
  to_propagate_.clear();
  bool need_update = false;
  IntegerValue task_set_end_min = kMinIntegerValue;

  int queue_index = 0;
  int blocking_task = -1;
  const int queue_size = task_by_increasing_start_max_.size();
  for (const auto task_time : task_by_increasing_end_min_) {
    // Note that we didn't put absent task in task_by_increasing_end_min_, but
    // the absence might have been pushed while looping here. This is fine since
    // any push we do on this task should handle this case correctly.
    const int current_task = task_time.task_index;
    const IntegerValue current_end_min = task_time.time;
    if (helper_->IsAbsent(current_task)) continue;

    for (; queue_index < queue_size; ++queue_index) {
      const auto to_insert = task_by_increasing_start_max_[queue_index];
      const IntegerValue start_max = to_insert.time;
      if (current_end_min <= start_max) break;

      const int t = to_insert.task_index;
      DCHECK(helper_->IsPresent(t));

      // If t has not been processed yet, it has a mandatory part, and rather
      // than adding it right away to task_set, we will delay all propagation
      // until current_task is equal to this "blocking task".
      //
      // This idea is introduced in "Linear-Time Filtering Algorithms for the
      // Disjunctive Constraints" Hamed Fahimi, Claude-Guy Quimper.
      //
      // Experiments seems to indicate that it is slighlty faster rather than
      // having to ignore one of the task already inserted into task_set_ when
      // we have tasks with mandatory parts. It also open-up more option for the
      // data structure used in task_set_.
      if (!processed_[t]) {
        if (blocking_task != -1) {
          // We have two blocking tasks, which means they are in conflict.
          helper_->ClearReason();
          helper_->AddPresenceReason(blocking_task);
          helper_->AddPresenceReason(t);
          helper_->AddReasonForBeingBefore(blocking_task, t);
          helper_->AddReasonForBeingBefore(t, blocking_task);
          return helper_->ReportConflict();
        }
        DCHECK_LT(start_max, helper_->ShiftedStartMin(t) + helper_->SizeMin(t))
            << " task should have mandatory part: "
            << helper_->TaskDebugString(t);
        DCHECK(to_propagate_.empty());
        blocking_task = t;
        to_propagate_.push_back(t);
      } else {
        need_update = true;
        task_set_.AddShiftedStartMinEntry(*helper_, t);
      }
    }

    // If we have a blocking task, we delay the propagation until current_task
    // is the blocking task.
    if (blocking_task != current_task) {
      to_propagate_.push_back(current_task);
      if (blocking_task != -1) continue;
    }
    for (const int t : to_propagate_) {
      DCHECK(!processed_[t]);
      processed_[t] = true;
      if (need_update) {
        need_update = false;
        task_set_end_min = task_set_.ComputeEndMin();
      }

      // Corner case if a previous push from to_propagate_ caused a subsequent
      // task to be absent.
      if (helper_->IsAbsent(t)) continue;

      // task_set_ contains all the tasks that must be executed before t. They
      // are in "detectable precedence" because their start_max is smaller than
      // the end-min of t like so:
      //          [(the task t)
      //                     (a task in task_set_)]
      // From there, we deduce that the start-min of t is greater or equal to
      // the end-min of the critical tasks.
      //
      // Note that this works as well when IsPresent(t) is false.
      if (task_set_end_min > helper_->StartMin(t)) {
        const int critical_index = task_set_.GetCriticalIndex();
        const std::vector<TaskSet::Entry>& sorted_tasks =
            task_set_.SortedTasks();
        helper_->ClearReason();

        // We need:
        // - StartMax(ct) < EndMin(t) for the detectable precedence.
        // - StartMin(ct) >= window_start for the value of task_set_end_min.
        const IntegerValue end_min_if_present =
            helper_->ShiftedStartMin(t) + helper_->SizeMin(t);
        const IntegerValue window_start =
            sorted_tasks[critical_index].start_min;
        for (int i = critical_index; i < sorted_tasks.size(); ++i) {
          const int ct = sorted_tasks[i].task;
          DCHECK_NE(ct, t);
          helper_->AddPresenceReason(ct);
          helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min,
                                        window_start);
          helper_->AddStartMaxReason(ct, end_min_if_present - 1);
        }

        // Add the reason for t (we only need the end-min).
        helper_->AddEndMinReason(t, end_min_if_present);

        // This augment the start-min of t. Note that t is not in task set
        // yet, so we will use this updated start if we ever add it there.
        if (!helper_->IncreaseStartMin(t, task_set_end_min)) {
          return false;
        }

        // This propagators assumes that every push is reflected for its
        // correctness.
        if (helper_->InPropagationLoop()) return true;
      }

      if (t == blocking_task) {
        // Insert the blocking_task. Note that because we just pushed it,
        // it will be last in task_set_ and also the only reason used to push
        // any of the subsequent tasks. In particular, the reason will be valid
        // even though task_set might contains tasks with a start_max greater or
        // equal to the end_min of the task we push.
        need_update = true;
        blocking_task = -1;
        task_set_.AddShiftedStartMinEntry(*helper_, t);
      }
    }
    to_propagate_.clear();
  }
  return true;
}

int DisjunctiveDetectablePrecedences::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id, watcher, /*watch_start_max=*/true,
                         /*watch_end_max=*/false);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctivePrecedences::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) return false;
  window_.clear();
  IntegerValue window_end = kMinIntegerValue;
  for (const TaskTime task_time : helper_->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;
    if (!helper_->IsPresent(task)) continue;

    const IntegerValue start_min = task_time.time;
    if (start_min < window_end) {
      window_.push_back(task_time);
      window_end += helper_->SizeMin(task);
      continue;
    }

    if (window_.size() > 1 && !PropagateSubwindow()) {
      return false;
    }

    // Start of the next window.
    window_.clear();
    window_.push_back(task_time);
    window_end = start_min + helper_->SizeMin(task);
  }
  if (window_.size() > 1 && !PropagateSubwindow()) {
    return false;
  }
  return true;
}

bool DisjunctivePrecedences::PropagateSubwindow() {
  // TODO(user): We shouldn't consider ends for fixed intervals here. But
  // then we should do a better job of computing the min-end of a subset of
  // intervals from this disjunctive (like using fixed intervals even if there
  // is no "before that variable" relationship). Ex: If a variable is after two
  // intervals that cannot be both before a fixed one, we could propagate more.
  index_to_end_vars_.clear();
  int new_size = 0;
  for (const auto task_time : window_) {
    const int task = task_time.task_index;
    const AffineExpression& end_exp = helper_->Ends()[task];

    // TODO(user): Handle generic affine relation?
    if (end_exp.var == kNoIntegerVariable || end_exp.coeff != 1) continue;

    window_[new_size++] = task_time;
    index_to_end_vars_.push_back(end_exp.var);
  }
  window_.resize(new_size);
  precedences_->ComputePrecedences(index_to_end_vars_, &before_);

  const int size = before_.size();
  for (int i = 0; i < size;) {
    const IntegerVariable var = before_[i].var;
    DCHECK_NE(var, kNoIntegerVariable);
    task_set_.Clear();

    const int initial_i = i;
    IntegerValue min_offset = kMaxIntegerValue;
    for (; i < size && before_[i].var == var; ++i) {
      // Because we resized the window, the index is valid.
      const TaskTime task_time = window_[before_[i].index];

      // We have var >= end_exp.var + offset, so
      // var >= (end_exp.var + end_exp.constant) + (offset - end_exp.constant)
      // var >= task end + new_offset.
      const AffineExpression& end_exp = helper_->Ends()[task_time.task_index];
      min_offset = std::min(min_offset, before_[i].offset - end_exp.constant);

      // The task are actually in sorted order, so we do not need to call
      // task_set_.Sort(). This property is DCHECKed.
      task_set_.AddUnsortedEntry({task_time.task_index, task_time.time,
                                  helper_->SizeMin(task_time.task_index)});
    }
    DCHECK_GE(task_set_.SortedTasks().size(), 2);
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;

    // TODO(user): Only use the min_offset of the critical task? Or maybe do a
    // more general computation to find by how much we can push var?
    const IntegerValue new_lb = task_set_.ComputeEndMin() + min_offset;
    if (new_lb > integer_trail_->LowerBound(var)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      helper_->ClearReason();

      // Fill task_to_arc_index_ since we need it for the reason.
      // Note that we do not care about the initial content of this vector.
      for (int j = initial_i; j < i; ++j) {
        const int task = window_[before_[j].index].task_index;
        task_to_arc_index_[task] = before_[j].arc_index;
      }

      const int critical_index = task_set_.GetCriticalIndex();
      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min,
                                      window_start);

        const AffineExpression& end_exp = helper_->Ends()[ct];
        precedences_->AddPrecedenceReason(
            task_to_arc_index_[ct], min_offset + end_exp.constant,
            helper_->MutableLiteralReason(), helper_->MutableIntegerReason());
      }

      // TODO(user): If var is actually a start-min of an interval, we
      // could push the end-min and check the interval consistency right away.
      if (!helper_->PushIntegerLiteral(
              IntegerLiteral::GreaterOrEqual(var, new_lb))) {
        return false;
      }
    }
  }
  return true;
}

int DisjunctivePrecedences::RegisterWith(GenericLiteralWatcher* watcher) {
  // This propagator reach the fixed point in one go.
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id, watcher, /*watch_start_max=*/false,
                         /*watch_end_max=*/false);
  return id;
}

bool DisjunctiveNotLast::Propagate() {
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) return false;

  const auto& task_by_decreasing_start_max =
      helper_->TaskByDecreasingStartMax();
  const auto& task_by_increasing_shifted_start_min =
      helper_->TaskByIncreasingShiftedStartMin();

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
  int queue_index = task_by_decreasing_start_max.size() - 1;
  const int num_tasks = task_by_increasing_shifted_start_min.size();
  for (int i = 0; i < num_tasks;) {
    start_min_window_.clear();
    IntegerValue window_end = kMinIntegerValue;
    for (; i < num_tasks; ++i) {
      const TaskTime task_time = task_by_increasing_shifted_start_min[i];
      const int task = task_time.task_index;
      if (!helper_->IsPresent(task)) continue;

      const IntegerValue start_min = task_time.time;
      if (start_min_window_.empty()) {
        start_min_window_.push_back(task_time);
        window_end = start_min + helper_->SizeMin(task);
      } else if (start_min < window_end) {
        start_min_window_.push_back(task_time);
        window_end += helper_->SizeMin(task);
      } else {
        break;
      }
    }

    // Add to start_max_window_ all the task whose start_max
    // fall into [window_start, window_end).
    start_max_window_.clear();
    for (; queue_index >= 0; queue_index--) {
      const auto task_time = task_by_decreasing_start_max[queue_index];

      // Note that we add task whose presence is still unknown here.
      if (task_time.time >= window_end) break;
      if (helper_->IsAbsent(task_time.task_index)) continue;
      start_max_window_.push_back(task_time);
    }

    // If this is the case, we cannot propagate more than the detectable
    // precedence propagator. Note that this continue must happen after we
    // computed start_max_window_ though.
    if (start_min_window_.size() <= 1) continue;

    // Process current window.
    if (!start_max_window_.empty() && !PropagateSubwindow()) {
      return false;
    }
  }
  return true;
}

bool DisjunctiveNotLast::PropagateSubwindow() {
  auto& task_by_increasing_end_max = start_max_window_;
  for (TaskTime& entry : task_by_increasing_end_max) {
    entry.time = helper_->EndMax(entry.task_index);
  }
  IncrementalSort(task_by_increasing_end_max.begin(),
                  task_by_increasing_end_max.end());

  const IntegerValue threshold = task_by_increasing_end_max.back().time;
  auto& task_by_increasing_start_max = start_min_window_;
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

  task_by_increasing_start_max.resize(queue_size);
  std::sort(task_by_increasing_start_max.begin(),
            task_by_increasing_start_max.end());

  task_set_.Clear();
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
      task_set_.AddEntry({task_index, helper_->ShiftedStartMin(task_index),
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
        task_set_.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (end_min_of_critical_tasks <= helper_->StartMax(t)) continue;

    // Find the largest start-max of the critical tasks (excluding t). The
    // end-max for t need to be smaller than or equal to this.
    IntegerValue largest_ct_start_max = kMinIntegerValue;
    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
    const int sorted_tasks_size = sorted_tasks.size();
    for (int i = critical_index; i < sorted_tasks_size; ++i) {
      const int ct = sorted_tasks[i].task;
      if (t == ct) continue;
      const IntegerValue start_max = helper_->StartMax(ct);
      if (start_max > largest_ct_start_max) {
        largest_ct_start_max = start_max;
      }
    }

    // If we have any critical task, the test will always be true because
    // of the tasks we put in task_set_.
    DCHECK(largest_ct_start_max == kMinIntegerValue ||
           end_max > largest_ct_start_max);
    if (end_max > largest_ct_start_max) {
      helper_->ClearReason();

      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      for (int i = critical_index; i < sorted_tasks_size; ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].size_min,
                                      window_start);
        helper_->AddStartMaxReason(ct, largest_ct_start_max);
      }

      // Add the reason for t, we only need the start-max.
      helper_->AddStartMaxReason(t, end_min_of_critical_tasks - 1);

      // Enqueue the new end-max for t.
      // Note that changing it will not influence the rest of the loop.
      if (!helper_->DecreaseEndMax(t, largest_ct_start_max)) return false;
    }
  }
  return true;
}

int DisjunctiveNotLast::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctiveEdgeFinding::Propagate() {
  const int num_tasks = helper_->NumTasks();
  if (!helper_->SynchronizeAndSetTimeDirection(time_direction_)) return false;
  is_gray_.resize(num_tasks, false);
  non_gray_task_to_event_.resize(num_tasks);

  window_.clear();
  IntegerValue window_end = kMinIntegerValue;
  for (const TaskTime task_time : helper_->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task)) continue;

    // Note that we use the real start min here not the shifted one. This is
    // because we might be able to push it if it is smaller than window end.
    if (helper_->StartMin(task) < window_end) {
      window_.push_back(task_time);
      window_end += helper_->SizeMin(task);
      continue;
    }

    // We need at least 3 tasks for the edge-finding to be different from
    // detectable precedences.
    if (window_.size() > 2 && !PropagateSubwindow(window_end)) {
      return false;
    }

    // Start of the next window.
    window_.clear();
    window_.push_back(task_time);
    window_end = task_time.time + helper_->SizeMin(task);
  }
  if (window_.size() > 2 && !PropagateSubwindow(window_end)) {
    return false;
  }
  return true;
}

bool DisjunctiveEdgeFinding::PropagateSubwindow(IntegerValue window_end_min) {
  // Cache the task end-max and abort early if possible.
  task_by_increasing_end_max_.clear();
  for (const auto task_time : window_) {
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
      task_by_increasing_end_max_.push_back({task, end_max});
    } else {
      is_gray_[task] = true;
    }
  }

  // If we have just 1 non-gray task, then this propagator does not propagate
  // more than the detectable precedences, so we abort early.
  if (task_by_increasing_end_max_.size() < 2) return true;
  std::sort(task_by_increasing_end_max_.begin(),
            task_by_increasing_end_max_.end());

  // Set up theta tree.
  //
  // Some task in the theta tree will be considered "gray".
  // When computing the end-min of the sorted task, we will compute it for:
  // - All the non-gray task
  // - All the non-gray task + at most one gray task.
  //
  // TODO(user): it should be faster to initialize it all at once rather
  // than calling AddOrUpdate() n times.
  const int window_size = window_.size();
  event_size_.clear();
  theta_tree_.Reset(window_size);
  for (int event = 0; event < window_size; ++event) {
    const TaskTime task_time = window_[event];
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
    DCHECK(!is_gray_[task_by_increasing_end_max_.back().task_index]);
    const IntegerValue non_gray_end_max =
        task_by_increasing_end_max_.back().time;

    // Overload checking.
    const IntegerValue non_gray_end_min = theta_tree_.GetEnvelope();
    if (non_gray_end_min > non_gray_end_max) {
      helper_->ClearReason();

      // We need the reasons for the critical tasks to fall in:
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(non_gray_end_max);
      const IntegerValue window_start = window_[critical_event].time;
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < window_size; event++) {
        const int task = window_[event].task_index;
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
      int critical_event_with_gray;
      int gray_event;
      IntegerValue available_energy;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          non_gray_end_max, &critical_event_with_gray, &gray_event,
          &available_energy);
      const int gray_task = window_[gray_event].task_index;
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
        const int first_event =
            std::min(critical_event, critical_event_with_gray);
        const int second_event =
            std::max(critical_event, critical_event_with_gray);
        const IntegerValue first_start = window_[first_event].time;
        const IntegerValue second_start = window_[second_event].time;

        // window_end is chosen to be has big as possible and still have an
        // overload if the gray task is not last.
        const IntegerValue window_end =
            non_gray_end_max + event_size_[gray_event] - available_energy - 1;
        CHECK_GE(window_end, non_gray_end_max);

        // The non-gray part of the explanation as detailed above.
        helper_->ClearReason();
        for (int event = first_event; event < window_size; event++) {
          const int task = window_[event].task_index;
          if (is_gray_[task]) continue;
          helper_->AddPresenceReason(task);
          helper_->AddEnergyAfterReason(
              task, event_size_[event],
              event >= second_event ? second_start : first_start);
          helper_->AddEndMaxReason(task, window_end);
        }

        // Add the reason for the gray_task (we don't need the end-max or
        // presence reason).
        helper_->AddEnergyAfterReason(gray_task, event_size_[gray_event],
                                      window_[critical_event_with_gray].time);

        // Enqueue the new start-min for gray_task.
        //
        // TODO(user): propagate the precedence Boolean here too? I think it
        // will be more powerful. Even if eventually all these precedence will
        // become detectable (see Petr Villim PhD).
        if (!helper_->IncreaseStartMin(gray_task, non_gray_end_min)) {
          return false;
        }
      }

      // Remove the gray_task.
      theta_tree_.RemoveEvent(gray_event);
    }

    // Stop before we get just one non-gray task.
    if (task_by_increasing_end_max_.size() <= 2) break;

    // Stop if the min of end_max is too big.
    if (task_by_increasing_end_max_[0].time >=
        theta_tree_.GetOptionalEnvelope()) {
      break;
    }

    // Make the non-gray task with larger end-max gray.
    const int new_gray_task = task_by_increasing_end_max_.back().task_index;
    task_by_increasing_end_max_.pop_back();
    const int new_gray_event = non_gray_task_to_event_[new_gray_task];
    DCHECK(!is_gray_[new_gray_task]);
    is_gray_[new_gray_task] = true;
    theta_tree_.AddOrUpdateOptionalEvent(new_gray_event,
                                         window_[new_gray_event].time,
                                         event_size_[new_gray_event]);
  }

  return true;
}

int DisjunctiveEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->SetTimeDirection(time_direction_);
  helper_->WatchAllTasks(id, watcher, /*watch_start_max=*/false,
                         /*watch_end_max=*/true);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

}  // namespace sat
}  // namespace operations_research
