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

#include "ortools/sat/disjunctive.h"

#include "ortools/base/iterator_adaptors.h"
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
      for (const IntervalVariable var : vars) {
        starts.push_back(model->Get(StartVar(var)));
      }
      model->Add(AllDifferentOnBounds(starts));
      return;
    }

    SchedulingConstraintHelper* helper = new SchedulingConstraintHelper(
        vars, model->GetOrCreate<Trail>(), model->GetOrCreate<IntegerTrail>(),
        repository);
    model->TakeOwnership(helper);
    GenericLiteralWatcher* watcher =
        model->GetOrCreate<GenericLiteralWatcher>();

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
    if (model->GetOrCreate<SatSolver>()
            ->parameters()
            .use_precedences_in_disjunctive_constraint()) {
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
  sorted_tasks_.push_back(e);
  int j = static_cast<int>(sorted_tasks_.size()) - 1;
  while (j > 0 && sorted_tasks_[j - 1].min_start > e.min_start) {
    sorted_tasks_[j] = sorted_tasks_[j - 1];
    --j;
  }
  sorted_tasks_[j] = e;
  DCHECK(std::is_sorted(sorted_tasks_.begin(), sorted_tasks_.end()));

  // If the task is added after optimized_restart_, we know that we don't need
  // to scan the task before optimized_restart_ in the next ComputeEndMin().
  if (j <= optimized_restart_) optimized_restart_ = 0;
}

void TaskSet::AddOrderedLastEntry(const Entry& e) {
  if (DEBUG_MODE && !sorted_tasks_.empty()) {
    CHECK_GE(e.min_start, sorted_tasks_.back().min_start);
  }
  sorted_tasks_.push_back(e);
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
  IntegerValue min_end = kMinIntegerValue;
  for (int i = optimized_restart_; i < size; ++i) {
    const Entry& e = sorted_tasks_[i];
    if (e.task == task_to_ignore) {
      ignored = true;
      continue;
    }
    if (e.min_start >= min_end) {
      *critical_index = i;
      if (!ignored) optimized_restart_ = i;
      min_end = CapAdd(e.min_start, e.min_duration);
    } else {
      min_end = CapAdd(min_end, e.min_duration);
    }
  }
  return min_end;
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
  start_event_to_task_.clear();
  start_event_time_.clear();
  int num_events_ = 0;
  for (const auto task_time : helper_->TaskByIncreasingStartMin()) {
    const int task = task_time.task_index;
    // TODO(user): Add max energy deduction for variable durations.
    // Those would take into account tasks with DurationMin(task) == 0.
    if (helper_->IsAbsent(task) || helper_->DurationMin(task) == 0) {
      task_to_start_event_[task] = -1;
      continue;
    }
    start_event_to_task_.push_back(task);
    start_event_time_.push_back(task_time.time);
    task_to_start_event_[task] = num_events_;
    num_events_++;
  }
  const int num_events = num_events_;
  theta_tree_.Reset(num_events);

  // Introduce events by nondecreasing end_max, check for overloads.
  for (const auto task_time :
       ::gtl::reversed_view(helper_->TaskByDecreasingEndMax())) {
    const int current_task = task_time.task_index;
    if (task_to_start_event_[current_task] == -1) continue;

    {
      const int current_event = task_to_start_event_[current_task];
      const bool is_present = helper_->IsPresent(current_task);
      // TODO(user): consider reducing max available duration.
      const IntegerValue energy_max = helper_->DurationMin(current_task);
      const IntegerValue energy_min = is_present ? energy_max : IntegerValue(0);

      theta_tree_.AddOrUpdateEvent(current_event,
                                   start_event_time_[current_event], energy_min,
                                   energy_max);
    }

    const IntegerValue current_end = task_time.time;
    if (theta_tree_.GetEnvelope() > current_end) {
      // Explain failure with tasks in critical interval.
      helper_->ClearReason();
      const int critical_event =
          theta_tree_.GetMaxEventWithEnvelopeGreaterThan(current_end);
      const IntegerValue window_start = start_event_time_[critical_event];
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) - 1;

      for (int event = critical_event; event < num_events; event++) {
        if (theta_tree_.EnergyMin(event) > 0) {
          const int task = start_event_to_task_[event];
          helper_->AddPresenceReason(task);
          helper_->AddDurationMinReason(task);
          helper_->AddStartMinReason(task, window_start);
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
      IntegerValue unused;
      theta_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          current_end, &critical_event, &optional_event, &unused);

      const IntegerValue window_start = start_event_time_[critical_event];
      const int optional_task = start_event_to_task_[optional_event];
      const IntegerValue window_end =
          theta_tree_.GetEnvelopeOf(critical_event) +
          helper_->DurationMin(optional_task) - 1;

      for (int event = critical_event; event < num_events; event++) {
        if (theta_tree_.EnergyMin(event) > 0) {
          const int task = start_event_to_task_[event];
          helper_->AddPresenceReason(task);
          helper_->AddDurationMinReason(task);
          helper_->AddStartMinReason(task, window_start);
          helper_->AddEndMaxReason(task, window_end);
        }
      }

      helper_->AddDurationMinReason(optional_task);
      helper_->AddStartMinReason(optional_task, window_start);
      helper_->AddEndMaxReason(optional_task, window_end);

      helper_->PushTaskAbsence(optional_task);  // This never fails.
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
  const auto& task_by_increasing_min_end = helper_->TaskByIncreasingEndMin();
  const auto& task_by_decreasing_max_start =
      helper_->TaskByDecreasingStartMax();

  const int num_tasks = helper_->NumTasks();
  int queue_index = num_tasks - 1;
  task_set_.Clear();
  for (const auto task_time : task_by_increasing_min_end) {
    const int t = task_time.task_index;
    const IntegerValue min_end = task_time.time;

    if (helper_->IsAbsent(t)) continue;

    while (queue_index >= 0) {
      const int to_insert =
          task_by_decreasing_max_start[queue_index].task_index;
      if (min_end <= helper_->StartMax(to_insert)) break;
      if (helper_->IsPresent(to_insert)) {
        task_set_.AddEntry({to_insert, helper_->StartMin(to_insert),
                            helper_->DurationMin(to_insert)});
      }
      --queue_index;
    }

    // task_set_ contains all the tasks that must be executed before t.
    // They are in "dectable precedence" because their max_start is smaller than
    // the end-min of t like so:
    //          [(the task t)
    //                     (a task in task_set_)]
    // From there, we deduce that the start-min of t is greater or equal to the
    // end-min of the critical tasks.
    //
    // Note that this works as well when task_is_currently_present_[t] is false.
    int critical_index = 0;
    const IntegerValue min_end_of_critical_tasks =
        task_set_.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (min_end_of_critical_tasks > helper_->StartMin(t)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      helper_->ClearReason();

      // We need:
      // - StartMax(ct) < EndMin(t) for the detectable precedence.
      // - StartMin(ct) > window_start for the min_end_of_critical_tasks reason.
      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        DCHECK(helper_->IsPresent(ct));
        DCHECK_GE(helper_->StartMin(ct), window_start);
        DCHECK_LT(helper_->StartMax(ct), min_end);
        helper_->AddPresenceReason(ct);
        helper_->AddDurationMinReason(ct);
        helper_->AddStartMinReason(ct, window_start);
        helper_->AddStartMaxReason(ct, min_end - 1);
      }

      // Add the reason for t (we only need the end-min).
      helper_->AddEndMinReason(t, min_end);

      // This augment the start-min of t and subsequently it can augment the
      // next min_end_of_critical_tasks, but our deduction is still valid.
      if (!helper_->IncreaseStartMin(t, min_end_of_critical_tasks)) {
        return false;
      }

      // We need to reorder t inside task_set_.
      task_set_.NotifyEntryIsNowLastIfPresent(
          {t, helper_->StartMin(t), helper_->DurationMin(t)});
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

  std::vector<bool> task_is_currently_present(helper_->EndVars().size());
  for (int i = 0; i < helper_->NumTasks(); ++i) {
    task_is_currently_present[i] = helper_->IsPresent(i);
  }
  precedences_->ComputePrecedences(helper_->EndVars(),
                                   task_is_currently_present, &before_);

  const int num_tasks = helper_->NumTasks();

  // We don't care about the initial content of this vector.
  reason_for_beeing_before_.resize(num_tasks, kNoLiteralIndex);
  int critical_index;
  const int size = before_.size();
  for (int i = 0; i < size;) {
    const IntegerVariable var = before_[i].var;
    DCHECK_NE(var, kNoIntegerVariable);
    task_set_.Clear();
    for (; i < size && before_[i].var == var; ++i) {
      const int task = before_[i].index;
      reason_for_beeing_before_[task] = before_[i].reason;
      task_set_.AddUnsortedEntry(
          {task, helper_->StartMin(task), helper_->DurationMin(task)});
    }
    if (task_set_.SortedTasks().size() < 2) continue;
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    task_set_.Sort();

    const IntegerValue min_end =
        task_set_.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index);
    if (min_end > integer_trail_->LowerBound(var)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      helper_->ClearReason();

      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        DCHECK(helper_->IsPresent(ct));
        DCHECK_GE(helper_->StartMin(ct), window_start);
        helper_->AddPresenceReason(ct);
        helper_->AddDurationMinReason(ct);
        helper_->AddStartMinReason(ct, window_start);
        if (reason_for_beeing_before_[ct] != kNoLiteralIndex) {
          helper_->MutableLiteralReason()->push_back(
              Literal(reason_for_beeing_before_[ct]).Negated());
        }
      }

      // TODO(user): If var is actually a start-min of an interval, we
      // could push the end-min and check the interval consistency right away.
      if (!helper_->PushIntegerLiteral(
              IntegerLiteral::GreaterOrEqual(var, min_end))) {
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
  const auto& task_by_decreasing_max_start =
      helper_->TaskByDecreasingStartMax();

  const int num_tasks = helper_->NumTasks();
  int queue_index = num_tasks - 1;

  task_set_.Clear();
  const auto& task_by_increasing_max_end =
      ::gtl::reversed_view(helper_->TaskByDecreasingEndMax());
  for (const auto task_time : task_by_increasing_max_end) {
    const int t = task_time.task_index;
    const IntegerValue max_end = task_time.time;

    if (helper_->IsAbsent(t)) continue;

    // task_set_ contains all the tasks that must start before the end-max of t.
    // These are the only candidates that have a chance to decrease the end-max
    // of t.
    while (queue_index >= 0) {
      const int to_insert =
          task_by_decreasing_max_start[queue_index].task_index;
      if (max_end <= helper_->StartMax(to_insert)) break;
      if (helper_->IsPresent(to_insert)) {
        task_set_.AddEntry({to_insert, helper_->StartMin(to_insert),
                            helper_->DurationMin(to_insert)});
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
    const IntegerValue min_end_of_critical_tasks =
        task_set_.ComputeEndMin(/*task_to_ignore=*/t, &critical_index);
    if (min_end_of_critical_tasks <= helper_->StartMax(t)) continue;

    // Find the largest start-max of the critical tasks (excluding t). The
    // end-max for t need to be smaller than or equal to this.
    IntegerValue largest_ct_max_start = kMinIntegerValue;
    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
    for (int i = critical_index; i < sorted_tasks.size(); ++i) {
      const int ct = sorted_tasks[i].task;
      if (t == ct) continue;
      const IntegerValue max_start = helper_->StartMax(ct);
      if (max_start > largest_ct_max_start) {
        largest_ct_max_start = max_start;
      }
    }

    // If we have any critical task, the test will always be true because
    // of the tasks we put in task_set_.
    DCHECK(largest_ct_max_start == kMinIntegerValue ||
           max_end > largest_ct_max_start);
    if (max_end > largest_ct_max_start) {
      helper_->ClearReason();

      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        helper_->AddPresenceReason(ct);
        helper_->AddDurationMinReason(ct);
        helper_->AddStartMinReason(ct, window_start);
        helper_->AddStartMaxReason(ct, largest_ct_max_start);
      }

      // Add the reason for t, we only need the start-max.
      helper_->AddStartMaxReason(t, min_end_of_critical_tasks - 1);

      // Enqueue the new end-max for t.
      // Note that changing it will not influence the rest of the loop.
      if (!helper_->DecreaseEndMax(t, largest_ct_max_start)) return false;
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

  // Some task in sorted_tasks_ will be considered "gray".
  // When computing the end-min of the sorted task, we will compute it for:
  // - All the non-gray task
  // - All the non-gray task + at most one gray task.
  is_gray_.resize(num_tasks, false);

  // The algorithm is slightly different than the others. We start with a full
  // sorted_tasks, and remove tasks one by one starting by the one with highest
  // end-max.
  task_set_.Clear();
  const auto& task_by_increasing_min_start =
      helper_->TaskByIncreasingStartMin();
  int num_not_gray = 0;
  for (const auto task_time : task_by_increasing_min_start) {
    const int t = task_time.task_index;
    const IntegerValue min_start = task_time.time;

    // Even though we completely ignore absent tasks, we still mark them as
    // gray to simplify the rest of the code in this function.
    if (helper_->IsAbsent(t)) {
      is_gray_[t] = true;
      continue;
    }

    task_set_.AddOrderedLastEntry({t, min_start, helper_->DurationMin(t)});

    // We already mark all the non-present task as gray.
    if (helper_->IsPresent(t)) {
      ++num_not_gray;
      is_gray_[t] = false;
    } else {
      is_gray_[t] = true;
    }
  }

  const auto& task_by_decreasing_max_end = helper_->TaskByDecreasingEndMax();
  int decreasing_max_end_index = 0;

  // At each iteration we either transform a non-gray task into a gray one or
  // remove a gray task, so this loop is linear in complexity.
  while (num_not_gray > 0) {
    // This is really similar to task_set_.ComputeEndMin() except that we
    // deal properly with the "gray" tasks.
    int critical_index = -1;
    int gray_critical_index = -1;

    // Respectively without gray task and with at most one one gray task.
    IntegerValue min_end_of_critical_tasks = kMinIntegerValue;
    IntegerValue min_end_of_critical_tasks_with_gray = kMinIntegerValue;

    // The index of the gray task in the critical tasks with one gray, if any.
    int gray_task_index = -1;

    // Set decreasing_max_end_index to the next non-gray task.
    while (is_gray_[task_by_decreasing_max_end[decreasing_max_end_index]
                        .task_index]) {
      ++decreasing_max_end_index;
      CHECK_LT(decreasing_max_end_index, num_tasks);
    }
    const IntegerValue non_gray_max_end = helper_->EndMax(
        task_by_decreasing_max_end[decreasing_max_end_index].task_index);

    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
    for (int i = 0; i < sorted_tasks.size(); ++i) {
      const TaskSet::Entry& e = sorted_tasks[i];
      if (is_gray_[e.task]) {
        if (e.min_start >= min_end_of_critical_tasks) {
          // Is this gray task increasing the end-min by itself?
          const IntegerValue candidate = CapAdd(e.min_start, e.min_duration);
          if (candidate >= min_end_of_critical_tasks_with_gray) {
            gray_critical_index = gray_task_index = i;
            min_end_of_critical_tasks_with_gray = candidate;
          }
        } else {
          // Is the task at the end of the non-gray critical block better?
          const IntegerValue candidate =
              CapAdd(min_end_of_critical_tasks, e.min_duration);
          if (candidate >= min_end_of_critical_tasks_with_gray) {
            gray_critical_index = critical_index;
            gray_task_index = i;
            min_end_of_critical_tasks_with_gray = candidate;
          }
        }
      } else {
        DCHECK_LE(helper_->EndMax(e.task), non_gray_max_end);

        // Augment the gray block. We could augment it more if e.min_start >=
        // min_end_of_critical_tasks_with_gray, but we don't care much about
        // this case because we will only trigger something if
        // min_end_of_critical_tasks_with_gray > min_end_of_critical_tasks.
        min_end_of_critical_tasks_with_gray =
            CapAdd(min_end_of_critical_tasks_with_gray, e.min_duration);

        // Augment the non-gray block.
        if (e.min_start >= min_end_of_critical_tasks) {
          critical_index = i;
          min_end_of_critical_tasks = CapAdd(e.min_start, e.min_duration);
        } else {
          min_end_of_critical_tasks =
              CapAdd(min_end_of_critical_tasks, e.min_duration);
        }
      }
    }

    // Overload checking.
    if (min_end_of_critical_tasks > non_gray_max_end) {
      helper_->ClearReason();

      // We need the reasons for the critical tasks to fall in:
      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      const IntegerValue window_end = min_end_of_critical_tasks - 1;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (is_gray_[ct]) continue;
        helper_->AddPresenceReason(ct);
        helper_->AddDurationMinReason(ct);
        helper_->AddStartMinReason(ct, window_start);
        helper_->AddEndMaxReason(ct, window_end);
      }
      return helper_->ReportConflict();
    }

    // Optimization, we can remove all the gray tasks that starts at or after
    // min_end_of_critical_tasks. Note that we deal with the case where
    // gray_task_index is removed by this code below.
    while (is_gray_[sorted_tasks.back().task] &&
           sorted_tasks.back().min_start >= min_end_of_critical_tasks) {
      task_set_.RemoveEntryWithIndex(sorted_tasks.size() - 1);
      CHECK(!sorted_tasks.empty());
    }

    // Edge-finding.
    // If we have a situation like:
    //     [(critical_task_with_gray_task)
    //                           ]
    //                           ^ end-max without the gray task.
    //
    // Then the gray task must be after all the critical tasks (all the non-gray
    // tasks in sorted_tasks actually), otherwise there will be no way to
    // schedule the critical_tasks inside their time window.
    if (min_end_of_critical_tasks_with_gray > non_gray_max_end) {
      DCHECK_NE(gray_task_index, -1);

      // We may have removed it with the optimization above, so simply continue
      // in this case.
      if (gray_task_index >= sorted_tasks.size()) continue;
      const int gray_task = sorted_tasks[gray_task_index].task;
      DCHECK(is_gray_[gray_task]);

      // Because of the optimization above, we necessarily have this order:
      CHECK_LE(gray_critical_index, critical_index);

      // Since the gray task is after all the other, we have a new lower bound.
      if (min_end_of_critical_tasks > helper_->StartMin(gray_task)) {
        helper_->ClearReason();
        const IntegerValue low_start =
            sorted_tasks[gray_critical_index].min_start;
        const IntegerValue high_start = sorted_tasks[critical_index].min_start;
        const IntegerValue window_end = min_end_of_critical_tasks_with_gray - 1;
        for (int i = gray_critical_index; i < sorted_tasks.size(); ++i) {
          const int ct = sorted_tasks[i].task;
          if (is_gray_[ct]) continue;
          helper_->AddPresenceReason(ct);
          helper_->AddDurationMinReason(ct);
          helper_->AddStartMinReason(
              ct, i >= critical_index ? high_start : low_start);
          helper_->AddEndMaxReason(ct, window_end);
        }

        // Add the reason for the gray_task (we don't need the end-max or
        // presence reason).
        helper_->AddDurationMinReason(gray_task);
        helper_->AddStartMinReason(gray_task, low_start);

        // Enqueue the new start-min for gray_task.
        //
        // TODO(user): propagate the precedence Boolean here too? I think it
        // will be more powerful. Even if eventually all these precedence will
        // become detectable (see Petr Villim PhD).
        if (!helper_->IncreaseStartMin(gray_task, min_end_of_critical_tasks)) {
          return false;
        }
      }

      // Remove the gray_task from sorted_tasks_.
      task_set_.RemoveEntryWithIndex(gray_task_index);
    } else {
      // Change more task into gray ones.
      if (num_not_gray == 1) break;

      // Optimization: the non_gray_max_end must move below this threshold
      // before we need to recompute anything.
      const IntegerValue threshold = std::max(
          min_end_of_critical_tasks, min_end_of_critical_tasks_with_gray);
      do {
        DCHECK(!is_gray_[task_by_decreasing_max_end[decreasing_max_end_index]
                             .task_index]);
        is_gray_[task_by_decreasing_max_end[decreasing_max_end_index]
                     .task_index] = true;
        --num_not_gray;
        if (num_not_gray == 0) break;

        // Set decreasing_max_end_index to the next non-gray task.
        while (is_gray_[task_by_decreasing_max_end[decreasing_max_end_index]
                            .task_index]) {
          ++decreasing_max_end_index;
          CHECK_LT(decreasing_max_end_index, num_tasks);
        }
      } while (
          helper_->EndMax(task_by_decreasing_max_end[decreasing_max_end_index]
                              .task_index) >= threshold);
    }
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
