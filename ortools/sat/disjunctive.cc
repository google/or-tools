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

#include "ortools/sat/disjunctive.h"

#include <memory>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/logging.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Disjunctive(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    bool is_all_different = true;
    IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
    for (const IntervalVariable var : vars) {
      if (repository->IsOptional(var) || repository->MinSize(var) != 1 ||
          repository->MaxSize(var) != 1) {
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

    SchedulingConstraintHelper* helper =
        new SchedulingConstraintHelper(vars, model);
    model->TakeOwnership(helper);
    GenericLiteralWatcher* watcher =
        model->GetOrCreate<GenericLiteralWatcher>();

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
            new DisjunctiveOverloadChecker(true, helper);
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
    if (model->GetOrCreate<SatParameters>()
            ->use_precedences_in_disjunctive_constraint()) {
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

// Note that we can keep the optimized_restart_ at its current value here.
void TaskSet::NotifyEntryIsNowLastIfPresent(const Entry& e) {
  const int size = sorted_tasks_.size();
  for (int i = 0;; ++i) {
    if (i == size) return;
    if (sorted_tasks_[i].task == e.task) {
      sorted_tasks_.erase(sorted_tasks_.begin() + i);
      break;
    }
  }
  sorted_tasks_.push_back(e);
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
}

void TaskSet::RemoveEntryWithIndex(int index) {
  sorted_tasks_.erase(sorted_tasks_.begin() + index);
  optimized_restart_ = 0;
}

IntegerValue TaskSet::ComputeEndMin(int task_to_ignore,
                                    int* critical_index) const {
  // The order in which we process tasks with the same start-min doesn't matter.
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));
  bool ignored = false;
  const int size = sorted_tasks_.size();
  IntegerValue end_min = kMinIntegerValue;
  for (int i = optimized_restart_; i < size; ++i) {
    const Entry& e = sorted_tasks_[i];
    if (e.task == task_to_ignore) {
      ignored = true;
      continue;
    }
    if (e.start_min >= end_min) {
      *critical_index = i;
      if (!ignored) optimized_restart_ = i;
      end_min = e.start_min + e.duration_min;
    } else {
      end_min += e.duration_min;
    }
  }
  return end_min;
}

bool DisjunctiveWithTwoItems::Propagate() {
  DCHECK_EQ(helper_->NumTasks(), 2);

  // We can't propagate anything if one of the interval is absent for sure.
  if (helper_->IsAbsent(0) || helper_->IsAbsent(1)) return true;

  // Note that this propagation also take care of the "overload checker" part.
  // It also propagates as much as possible, even in the presence of task with
  // variable durations.
  //
  // TODO(user): For optional interval whose presense in unknown and without
  // optional variable, the end-min may not be propagated to at least (start_min
  // + duration_min). Consider that into the computation so we may decide the
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

  if (helper_->IsPresent(task_before) &&
      helper_->StartMin(task_after) < helper_->EndMin(task_before)) {
    // Reason for precedences if both present.
    helper_->ClearReason();
    helper_->AddReasonForBeingBefore(task_before, task_after);

    // Reason for the bound push.
    helper_->AddPresenceReason(task_before);
    helper_->AddEndMinReason(task_before, helper_->EndMin(task_before));
    if (!helper_->IncreaseStartMin(task_after, helper_->EndMin(task_before))) {
      return false;
    }
  }

  if (helper_->IsPresent(task_after) &&
      helper_->EndMax(task_before) > helper_->StartMax(task_after)) {
    // Reason for precedences if both present.
    helper_->ClearReason();
    helper_->AddReasonForBeingBefore(task_before, task_after);

    // Reason for the bound push.
    helper_->AddPresenceReason(task_after);
    helper_->AddStartMaxReason(task_after, helper_->StartMax(task_after));
    if (!helper_->DecreaseEndMax(task_before, helper_->StartMax(task_after))) {
      return false;
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

// TODO(user): Improve the Overload Checker using delayed insertion.
// We insert events at the cost of O(log n) per insertion, and this is where
// the algorithm spends most of its time, thus it is worth improving.
// We can insert an arbitrary set of tasks at the cost of O(n) for the whole
// set. This is useless for the overload checker as is since we need to check
// overload after every insertion, but we could use an upper bound of the
// theta envelope to save us from checking the actual value.
bool DisjunctiveOverloadChecker::Propagate() {
  helper_->SetTimeDirection(time_direction_);

  // Set up theta tree.
  event_to_task_.clear();
  event_time_.clear();
  int num_events = 0;
  for (const auto task_time : helper_->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;

    // TODO(user): We need to take into account task with zero duration because
    // in this constraint, such a task cannot be overlapped by other. However,
    // we currently use the fact that the energy min is zero to detect that a
    // task is present and non-optional in the theta_tree_. Fix.
    if (helper_->IsAbsent(task) || helper_->DurationMin(task) == 0) {
      task_to_event_[task] = -1;
      continue;
    }
    event_to_task_.push_back(task);
    event_time_.push_back(task_time.time);
    task_to_event_[task] = num_events;
    num_events++;
  }
  theta_tree_.Reset(num_events);

  // Introduce events by nondecreasing end_max, check for overloads.
  for (const auto task_time :
       ::gtl::reversed_view(helper_->TaskByDecreasingEndMax())) {
    const int current_task = task_time.task_index;
    if (task_to_event_[current_task] == -1) continue;

    {
      const int current_event = task_to_event_[current_task];
      const IntegerValue energy_min = helper_->DurationMin(current_task);
      if (helper_->IsPresent(current_task)) {
        // TODO(user,user): Add max energy deduction for variable
        // durations by putting the energy_max here and modifying the code
        // dealing with the optional envelope greater than current_end below.
        theta_tree_.AddOrUpdateEvent(current_event, event_time_[current_event],
                                     energy_min, energy_min);
      } else {
        theta_tree_.AddOrUpdateOptionalEvent(
            current_event, event_time_[current_event], energy_min);
      }
    }

    const IntegerValue current_end = task_time.time;
    if (theta_tree_.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      helper_->ClearReason();
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = event_time_[critical_event];
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < num_events; event++) {
        const IntegerValue energy_min = theta_tree_.EnergyMin(event);
        if (energy_min > 0) {
          const int task = event_to_task_[event];
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

      const int optional_task = event_to_task_[optional_event];
      const IntegerValue optional_duration_min =
          helper_->DurationMin(optional_task);
      const IntegerValue window_start = event_time_[critical_event];
      const IntegerValue window_end =
          current_end + optional_duration_min - available_energy - 1;
      for (int event = critical_event; event < num_events; event++) {
        const IntegerValue energy_min = theta_tree_.EnergyMin(event);
        if (energy_min > 0) {
          const int task = event_to_task_[event];
          helper_->AddPresenceReason(task);
          helper_->AddEnergyAfterReason(task, energy_min, window_start);
          helper_->AddEndMaxReason(task, window_end);
        }
      }

      helper_->AddEnergyAfterReason(optional_task, optional_duration_min,
                                    window_start);
      helper_->AddEndMaxReason(optional_task, window_end);

      // If tasks shares the same presence literal, it is possible that we
      // already pushed this task absence.
      if (!helper_->IsAbsent(optional_task)) {
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
  helper_->WatchAllTasks(id, watcher);
  return id;
}

bool DisjunctiveDetectablePrecedences::Propagate() {
  helper_->SetTimeDirection(time_direction_);
  const auto& task_by_increasing_end_min = helper_->TaskByIncreasingEndMin();
  const auto& task_by_decreasing_start_max =
      helper_->TaskByDecreasingStartMax();

  const int num_tasks = helper_->NumTasks();
  int queue_index = num_tasks - 1;
  task_set_.Clear();
  for (const auto task_time : task_by_increasing_end_min) {
    const int t = task_time.task_index;
    const IntegerValue end_min = task_time.time;

    if (helper_->IsAbsent(t)) continue;

    while (queue_index >= 0) {
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
      if (end_min <= start_max) break;
      if (helper_->IsPresent(task_index)) {
        task_set_.AddEntry({task_index, helper_->ShiftedStartMin(task_index),
                            helper_->DurationMin(task_index)});
      }
      --queue_index;
    }

    // task_set_ contains all the tasks that must be executed before t.
    // They are in "dectable precedence" because their start_max is smaller than
    // the end-min of t like so:
    //          [(the task t)
    //                     (a task in task_set_)]
    // From there, we deduce that the start-min of t is greater or equal to the
    // end-min of the critical tasks.
    //
    // Note that this works as well when IsPresent(t) is false.
    int critical_index = 0;
    const IntegerValue end_min_of_critical_tasks =
        task_set_.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (end_min_of_critical_tasks > helper_->StartMin(t)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      helper_->ClearReason();

      // We need:
      // - StartMax(ct) < EndMin(t) for the detectable precedence.
      // - StartMin(ct) > window_start for the end_min_of_critical_tasks reason.
      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        DCHECK(helper_->IsPresent(ct));
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
                                      window_start);
        helper_->AddStartMaxReason(ct, end_min - 1);
      }

      // Add the reason for t (we only need the end-min).
      helper_->AddEndMinReason(t, end_min);

      // This augment the start-min of t and subsequently it can augment the
      // next end_min_of_critical_tasks, but our deduction is still valid.
      if (!helper_->IncreaseStartMin(t, end_min_of_critical_tasks)) {
        return false;
      }

      // We need to reorder t inside task_set_. Note that if t is in the set,
      // it means that the task is present and that IncreaseStartMin() did push
      // its start (by opposition to an optional interval where the push might
      // not happen if its start is not optional).
      task_set_.NotifyEntryIsNowLastIfPresent(
          {t, helper_->ShiftedStartMin(t), helper_->DurationMin(t)});
    }
  }
  return true;
}

int DisjunctiveDetectablePrecedences::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

bool DisjunctivePrecedences::Propagate() {
  helper_->SetTimeDirection(time_direction_);

  const int num_tasks = helper_->NumTasks();
  for (int t = 0; t < num_tasks; ++t) {
    task_is_currently_present_[t] = helper_->IsPresent(t);
  }
  precedences_->ComputePrecedences(helper_->EndVars(),
                                   task_is_currently_present_, &before_);

  // We don't care about the initial content of this vector.
  task_to_arc_index_.resize(num_tasks);
  int critical_index;
  const int size = before_.size();
  for (int i = 0; i < size;) {
    const IntegerVariable var = before_[i].var;
    DCHECK_NE(var, kNoIntegerVariable);
    task_set_.Clear();
    const int initial_i = i;
    IntegerValue min_offset = before_[i].offset;
    for (; i < size && before_[i].var == var; ++i) {
      const int task = before_[i].index;
      min_offset = std::min(min_offset, before_[i].offset);
      task_set_.AddUnsortedEntry(
          {task, helper_->ShiftedStartMin(task), helper_->DurationMin(task)});
    }
    DCHECK_GE(task_set_.SortedTasks().size(), 2);
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    task_set_.Sort();

    // TODO(user): Only use the min_offset of the critical task? Or maybe do a
    // more general computation to find by how much we can push var?
    const IntegerValue new_lb =
        task_set_.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index) +
        min_offset;
    if (new_lb > integer_trail_->LowerBound(var)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      helper_->ClearReason();

      // Fill task_to_arc_index_ since we need it for the reason.
      for (int j = initial_i; j < i; ++j) {
        task_to_arc_index_[before_[j].index] = before_[j].arc_index;
      }

      const IntegerValue window_start = sorted_tasks[critical_index].start_min;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        DCHECK(helper_->IsPresent(ct));
        helper_->AddPresenceReason(ct);
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
                                      window_start);
        precedences_->AddPrecedenceReason(task_to_arc_index_[ct], min_offset,
                                          helper_->MutableLiteralReason(),
                                          helper_->MutableIntegerReason());
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
  helper_->WatchAllTasks(id, watcher);
  return id;
}

bool DisjunctiveNotLast::Propagate() {
  helper_->SetTimeDirection(time_direction_);
  const auto& task_by_decreasing_start_max =
      helper_->TaskByDecreasingStartMax();

  const int num_tasks = helper_->NumTasks();
  int queue_index = num_tasks - 1;

  task_set_.Clear();
  const auto& task_by_increasing_end_max =
      ::gtl::reversed_view(helper_->TaskByDecreasingEndMax());
  for (const auto task_time : task_by_increasing_end_max) {
    const int t = task_time.task_index;
    const IntegerValue end_max = task_time.time;

    if (helper_->IsAbsent(t)) continue;

    // task_set_ contains all the tasks that must start before the end-max of t.
    // These are the only candidates that have a chance to decrease the end-max
    // of t.
    while (queue_index >= 0) {
      const auto to_insert = task_by_decreasing_start_max[queue_index];
      const int task_index = to_insert.task_index;
      const IntegerValue start_max = to_insert.time;
      if (end_max <= start_max) break;
      if (helper_->IsPresent(task_index)) {
        task_set_.AddEntry({task_index, helper_->ShiftedStartMin(task_index),
                            helper_->DurationMin(task_index)});
      }
      --queue_index;
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
    // Note that this works as well when task_is_currently_present_[t] is false.
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
        helper_->AddEnergyAfterReason(ct, sorted_tasks[i].duration_min,
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
  helper_->SetTimeDirection(time_direction_);
  const int num_tasks = helper_->NumTasks();

  // Some task in the theta tree will be considered "gray".
  // When computing the end-min of the sorted task, we will compute it for:
  // - All the non-gray task
  // - All the non-gray task + at most one gray task.
  is_gray_.resize(num_tasks, false);

  // Set up theta tree.
  //
  // TODO(user): it should be faster to initialize it all at once rather
  // than calling AddOrUpdate() n times.
  theta_tree_.Reset(num_tasks);
  non_gray_task_to_event_.resize(num_tasks);
  event_to_task_.clear();
  event_time_.clear();
  event_size_.clear();
  int num_events = 0;
  int num_not_gray = 0;
  for (const auto task_time : helper_->TaskByIncreasingShiftedStartMin()) {
    const int task = task_time.task_index;
    if (helper_->IsAbsent(task)) {
      // Even though we completely ignore absent tasks, we still mark them as
      // gray to simplify the rest of the code in this function.
      is_gray_[task] = true;
      continue;
    }
    event_to_task_.push_back(task);
    event_time_.push_back(task_time.time);

    // We already mark all the non-present task as gray.
    const IntegerValue energy_min = helper_->DurationMin(task);
    event_size_.push_back(energy_min);
    if (helper_->IsPresent(task)) {
      ++num_not_gray;
      is_gray_[task] = false;
      non_gray_task_to_event_[task] = num_events;
      theta_tree_.AddOrUpdateEvent(num_events, task_time.time, energy_min,
                                   energy_min);
    } else {
      is_gray_[task] = true;
      theta_tree_.AddOrUpdateOptionalEvent(num_events, task_time.time,
                                           energy_min);
    }

    num_events++;
  }

  int end_max_index = 0;
  const auto& task_by_decreasing_end_max = helper_->TaskByDecreasingEndMax();

  // At each iteration we either transform a non-gray task into a gray one or
  // remove a gray task, so this loop is linear in complexity.
  while (num_not_gray > 0) {
    // Set end_max_index to the next non-gray task.
    while (is_gray_[task_by_decreasing_end_max[end_max_index].task_index]) {
      ++end_max_index;
      CHECK_LT(end_max_index, num_tasks);
    }
    const IntegerValue non_gray_end_max =
        task_by_decreasing_end_max[end_max_index].time;

    // Overload checking.
    const IntegerValue non_gray_end_min = theta_tree_.GetEnvelope();
    if (non_gray_end_min > non_gray_end_max) {
      helper_->ClearReason();

      // We need the reasons for the critical tasks to fall in:
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(non_gray_end_max);
      const IntegerValue window_start = event_time_[critical_event];
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;
      for (int event = critical_event; event < num_events; event++) {
        const int task = event_to_task_[event];
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
      const int gray_task = event_to_task_[gray_event];

      // Since the gray task is after all the other, we have a new lower bound.
      CHECK(is_gray_[gray_task]);
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
        const IntegerValue first_start = event_time_[first_event];
        const IntegerValue second_start = event_time_[second_event];

        // window_end is chosen to be has big as possible and still have an
        // overload if the gray task is not last.
        const IntegerValue window_end =
            non_gray_end_max + event_size_[gray_event] - available_energy - 1;
        CHECK_GE(window_end, non_gray_end_max);

        // The non-gray part of the explanation as detailed above.
        helper_->ClearReason();
        for (int event = first_event; event < num_events; event++) {
          const int task = event_to_task_[event];
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
                                      event_time_[critical_event_with_gray]);

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

    // Make the non-gray task with larger end-max gray.
    if (num_not_gray == 1) break;
    {
      const int new_gray_task =
          task_by_decreasing_end_max[end_max_index].task_index;
      const int new_gray_event = non_gray_task_to_event_[new_gray_task];
      DCHECK(!is_gray_[new_gray_task]);
      is_gray_[new_gray_task] = true;
      theta_tree_.AddOrUpdateOptionalEvent(new_gray_event,
                                           event_time_[new_gray_event],
                                           event_size_[new_gray_event]);
    }
    ++end_max_index;
    --num_not_gray;
  }

  return true;
}

int DisjunctiveEdgeFinding::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  helper_->WatchAllTasks(id, watcher);
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
  return id;
}

}  // namespace sat
}  // namespace operations_research
