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

#include "sat/disjunctive.h"

//#include "base/iterator_adaptors.h"
#include "sat/sat_solver.h"
#include "util/sort.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> Disjunctive(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    bool is_all_different = true;
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    for (const IntervalVariable var : vars) {
      if (intervals->IsOptional(var) || intervals->MinSize(var) != 1 ||
          intervals->MaxSize(var) != 1) {
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

    DisjunctiveConstraint* disjunctive = new DisjunctiveConstraint(
        vars, model->GetOrCreate<Trail>(), model->GetOrCreate<IntegerTrail>(),
        intervals, model->GetOrCreate<PrecedencesPropagator>());
    disjunctive->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    disjunctive->SetParameters(model->GetOrCreate<SatSolver>()->parameters());
    model->TakeOwnership(disjunctive);
  };
}

std::function<void(Model*)> DisjunctiveWithBooleanPrecedences(
    const std::vector<IntervalVariable>& vars) {
  return [=](Model* model) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntervalsRepository* intervals = model->GetOrCreate<IntervalsRepository>();
    PrecedencesPropagator* precedences =
        model->GetOrCreate<PrecedencesPropagator>();
    for (int i = 0; i < vars.size(); ++i) {
      for (int j = 0; j < i; ++j) {
        const BooleanVariable boolean_var = sat_solver->NewBooleanVariable();
        const Literal i_before_j = Literal(boolean_var, true);
        const Literal j_before_i = i_before_j.Negated();
        precedences->AddConditionalPrecedence(intervals->EndVar(vars[i]),
                                              intervals->StartVar(vars[j]),
                                              i_before_j);
        precedences->AddConditionalPrecedence(intervals->EndVar(vars[j]),
                                              intervals->StartVar(vars[i]),
                                              j_before_i);
      }
    }
    DisjunctiveConstraint* disjunctive = new DisjunctiveConstraint(
        vars, model->GetOrCreate<Trail>(), model->GetOrCreate<IntegerTrail>(),
        intervals, precedences);
    disjunctive->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    disjunctive->SetParameters(sat_solver->parameters());
    model->TakeOwnership(disjunctive);
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
  // to scan the task before optimized_restart_ in the next ComputeMinEnd().
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

IntegerValue TaskSet::ComputeMinEnd(int task_to_ignore,
                                    int* critical_index) const {
  // The order in which we process tasks with the same min-start doesn't matter.
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

DisjunctiveConstraint::DisjunctiveConstraint(
    const std::vector<IntervalVariable>& non_overlapping_intervals,
    Trail* trail, IntegerTrail* integer_trail, IntervalsRepository* intervals,
    PrecedencesPropagator* precedences)
    : non_overlapping_intervals_(non_overlapping_intervals),
      trail_(trail),
      integer_trail_(integer_trail),
      intervals_(intervals),
      precedences_(precedences),
      stats_("DisjunctiveConstraint") {}

void DisjunctiveConstraint::SwitchToMirrorProblem() {
  std::swap(start_vars_, minus_end_vars_);
  std::swap(end_vars_, minus_start_vars_);
  std::swap(task_by_increasing_min_start_, task_by_decreasing_max_end_);
  std::swap(task_by_increasing_min_end_, task_by_decreasing_max_start_);
  std::swap(before_, after_);
}

bool DisjunctiveConstraint::Propagate() {
  SCOPED_TIME_STAT(&stats_);

  start_vars_.clear();
  end_vars_.clear();
  minus_end_vars_.clear();
  minus_start_vars_.clear();
  duration_vars_.clear();
  fixed_durations_.clear();
  reason_for_presence_.clear();
  task_is_currently_present_.clear();
  for (const IntervalVariable i : non_overlapping_intervals_) {
    if (intervals_->IsOptional(i)) {
      const Literal l = intervals_->IsPresentLiteral(i);
      if (trail_->Assignment().LiteralIsFalse(l)) continue;
      task_is_currently_present_.push_back(
          trail_->Assignment().LiteralIsTrue(l));
      reason_for_presence_.push_back(l.Index());
    } else {
      task_is_currently_present_.push_back(true);
      reason_for_presence_.push_back(kNoLiteralIndex);
    }
    if (intervals_->SizeVar(i) == kNoIntegerVariable) {
      duration_vars_.push_back(kNoIntegerVariable);
      fixed_durations_.push_back(intervals_->MinSize(i));
    } else {
      duration_vars_.push_back(intervals_->SizeVar(i));
      fixed_durations_.push_back(IntegerValue(0));
    }
    start_vars_.push_back(intervals_->StartVar(i));
    end_vars_.push_back(intervals_->EndVar(i));
    minus_start_vars_.push_back(NegationOf(intervals_->StartVar(i)));
    minus_end_vars_.push_back(NegationOf(intervals_->EndVar(i)));
  }

  // Because this class doesn't add any new precedences, we can compute the set
  // of task before (and after) some IntegerVariable only once before we enter
  // the propagation loop below.
  if (parameters_.use_precedences_in_disjunctive_constraint()) {
    precedences_->ComputePrecedences(end_vars_, task_is_currently_present_,
                                     &before_);
    precedences_->ComputePrecedences(minus_start_vars_,
                                     task_is_currently_present_, &after_);
  }

  // This is variable as it depends on the optional tasks that are present.
  const int num_tasks = start_vars_.size();
  DCHECK_EQ(num_tasks, minus_end_vars_.size());
  DCHECK_EQ(num_tasks, duration_vars_.size());
  DCHECK_EQ(num_tasks, fixed_durations_.size());
  DCHECK_EQ(num_tasks, reason_for_presence_.size());
  DCHECK_EQ(num_tasks, task_is_currently_present_.size());

  // Initialize our "order" vectors with all the task indices.
  if (task_by_increasing_min_start_.size() != num_tasks) {
    task_by_increasing_min_start_.resize(num_tasks);
    task_by_increasing_min_end_.resize(num_tasks);
    task_by_decreasing_max_start_.resize(num_tasks);
    task_by_decreasing_max_end_.resize(num_tasks);
    for (int t = 0; t < num_tasks; ++t) {
      task_by_increasing_min_start_[t] = t;
      task_by_increasing_min_end_[t] = t;
      task_by_decreasing_max_start_[t] = t;
      task_by_decreasing_max_end_[t] = t;
    }
  }

  // Loop until we reach the fixed-point. It should be unique (see Petr Villim
  // PhD).
  //
  // TODO(user): Some of these passes are idempotent, so there is no need to
  // call them if their input didn't change! Improve that.
  while (true) {
    const int64 old_timestamp = integer_trail_->num_enqueues();

    // This one is symmetric so there is no need to do it backward in time too.
    // It only detects conflict, it never propagates anything.
    if (!OverloadCheckingPass()) return false;

    if (!DetectablePrecedencePass()) return false;
    SwitchToMirrorProblem();
    if (!DetectablePrecedencePass()) return false;
    SwitchToMirrorProblem();
    if (old_timestamp != integer_trail_->num_enqueues()) continue;

    if (!NotLastPass()) return false;
    SwitchToMirrorProblem();
    if (!NotLastPass()) return false;
    SwitchToMirrorProblem();
    if (old_timestamp != integer_trail_->num_enqueues()) continue;

    if (!EdgeFindingPass()) return false;
    SwitchToMirrorProblem();
    if (!EdgeFindingPass()) return false;
    SwitchToMirrorProblem();
    if (old_timestamp != integer_trail_->num_enqueues()) continue;

    if (parameters_.use_precedences_in_disjunctive_constraint()) {
      if (!PrecedencePass()) return false;
      SwitchToMirrorProblem();
      if (!PrecedencePass()) return false;
      SwitchToMirrorProblem();
      if (old_timestamp != integer_trail_->num_enqueues()) continue;
    }

    break;
  }
  return true;
}

void DisjunctiveConstraint::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const IntervalVariable i : non_overlapping_intervals_) {
    watcher->WatchLowerBound(intervals_->StartVar(i), id);
    watcher->WatchUpperBound(intervals_->EndVar(i), id);
    if (intervals_->SizeVar(i) != kNoIntegerVariable) {
      watcher->WatchLowerBound(intervals_->SizeVar(i), id);
    }
    if (intervals_->IsOptional(i)) {
      watcher->WatchLiteral(intervals_->IsPresentLiteral(i), id);
    }
  }
}

void DisjunctiveConstraint::AddPresenceAndDurationReason(int t) {
  if (reason_for_presence_[t] != kNoLiteralIndex) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
  }
  if (duration_vars_[t] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
  }
}

void DisjunctiveConstraint::AddMinDurationReason(int t) {
  if (duration_vars_[t] != kNoIntegerVariable) {
    integer_reason_.push_back(
        integer_trail_->LowerBoundAsLiteral(duration_vars_[t]));
  }
}

void DisjunctiveConstraint::AddMinStartReason(int t, IntegerValue lower_bound) {
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(start_vars_[t], lower_bound));
}

void DisjunctiveConstraint::AddMinEndReason(int t, IntegerValue lower_bound) {
  integer_reason_.push_back(
      IntegerLiteral::GreaterOrEqual(end_vars_[t], lower_bound));
}

void DisjunctiveConstraint::AddMaxEndReason(int t, IntegerValue upper_bound) {
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(end_vars_[t], upper_bound));
}

void DisjunctiveConstraint::AddMaxStartReason(int t, IntegerValue upper_bound) {
  integer_reason_.push_back(
      IntegerLiteral::LowerOrEqual(start_vars_[t], upper_bound));
}

bool DisjunctiveConstraint::IncreaseMinStart(int t,
                                             IntegerValue new_min_start) {
  if (!integer_trail_->Enqueue(
          IntegerLiteral::GreaterOrEqual(start_vars_[t], new_min_start),
          literal_reason_, integer_reason_)) {
    return false;
  }

  // We propagate right away the new min-end lower-bound we have.
  const IntegerValue min_end_lb = new_min_start + MinDuration(t);
  if (MinEnd(t) < min_end_lb) {
    integer_reason_.clear();
    literal_reason_.clear();
    AddMinStartReason(t, new_min_start);
    AddMinDurationReason(t);
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(end_vars_[t], min_end_lb),
            literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

bool DisjunctiveConstraint::DecreaseMaxEnd(int t, IntegerValue new_max_end) {
  if (!integer_trail_->Enqueue(
          IntegerLiteral::LowerOrEqual(end_vars_[t], new_max_end),
          literal_reason_, integer_reason_)) {
    return false;
  }

  // We propagate right away the new max-start upper-bound we have.
  const IntegerValue max_start_ub = new_max_end - MinDuration(t);
  if (MaxStart(t) > max_start_ub) {
    integer_reason_.clear();
    literal_reason_.clear();
    AddMaxEndReason(t, new_max_end);
    AddMinDurationReason(t);
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(start_vars_[t], max_start_ub),
            literal_reason_, integer_reason_)) {
      return false;
    }
  }

  return true;
}

void DisjunctiveConstraint::UpdateTaskByIncreasingMinStart() {
  IncrementalSort(task_by_increasing_min_start_.begin(),
                  task_by_increasing_min_start_.end(), [this](int t1, int t2) {
                    return MinStart(t1) < MinStart(t2);
                  });
}

void DisjunctiveConstraint::UpdateTaskByIncreasingMinEnd() {
  IncrementalSort(task_by_increasing_min_end_.begin(),
                  task_by_increasing_min_end_.end(),
                  [this](int t1, int t2) { return MinEnd(t1) < MinEnd(t2); });
}

void DisjunctiveConstraint::UpdateTaskByDecreasingMaxStart() {
  IncrementalSort(task_by_decreasing_max_start_.begin(),
                  task_by_decreasing_max_start_.end(), [this](int t1, int t2) {
                    return MaxStart(t1) > MaxStart(t2);
                  });
}

void DisjunctiveConstraint::UpdateTaskByDecreasingMaxEnd() {
  IncrementalSort(task_by_decreasing_max_end_.begin(),
                  task_by_decreasing_max_end_.end(),
                  [this](int t1, int t2) { return MaxEnd(t1) > MaxEnd(t2); });
}

bool DisjunctiveConstraint::OverloadCheckingPass() {
  SCOPED_TIME_STAT(&stats_);
  UpdateTaskByDecreasingMaxEnd();
  task_set_.Clear();
  for (auto it = task_by_decreasing_max_end_.rbegin();
       it != task_by_decreasing_max_end_.rend(); ++it) {
    const int t = *it;
    if (task_is_currently_present_[t]) {
      task_set_.AddEntry({t, MinStart(t), MinDuration(t)});
    }
    int critical_index = 0;
    const IntegerValue min_end_of_critical_tasks =
        task_set_.ComputeMinEnd(/*task_to_ignore=*/-1, &critical_index);
    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();

    // Check if we can detect that this optional task can't be executed.
    // TODO(user): This test doesn't detect all the cases. A better way to do
    // it would be like in the EdgeFindingPass() with the concept of gray tasks.
    if (!task_is_currently_present_[t] && !sorted_tasks.empty()) {
      // Skip if it was already shown not to be there.
      if (trail_->Assignment().LiteralIsFalse(
              Literal(reason_for_presence_[t]))) {
        continue;
      }

      const IntegerValue critical_start =
          sorted_tasks[critical_index].min_start;
      if (MinEnd(t) <= critical_start) continue;
      const IntegerValue new_min_end =
          CapAdd(min_end_of_critical_tasks,
                 MinStart(t) >= critical_start
                     ? MinDuration(t)
                     : MinEnd(t) - critical_start /* cannot overflow */);
      if (new_min_end > MaxEnd(t)) {
        // TODO(user): This could be done lazily, like most of the loop to
        // compute the reasons in this class.
        integer_reason_.clear();
        literal_reason_.clear();
        const IntegerValue window_start =
            std::min(MinStart(t), sorted_tasks[critical_index].min_start);
        const IntegerValue window_end = new_min_end - 1;
        for (int i = critical_index; i < sorted_tasks.size(); ++i) {
          const int ct = sorted_tasks[i].task;
          AddPresenceAndDurationReason(ct);
          AddMinStartReason(ct, window_start);
          AddMaxEndReason(ct, window_end);
        }
        AddMinDurationReason(t);
        AddMinStartReason(t, window_start);
        AddMaxEndReason(t, window_end);

        DCHECK_NE(reason_for_presence_[t], kNoLiteralIndex);
        integer_trail_->EnqueueLiteral(
            Literal(reason_for_presence_[t]).Negated(), literal_reason_,
            integer_reason_);
      }
    }

    // Overload checking.
    if (min_end_of_critical_tasks > MaxEnd(t)) {
      DCHECK(task_is_currently_present_[t]);
      literal_reason_.clear();
      integer_reason_.clear();

      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      const IntegerValue window_end = min_end_of_critical_tasks - 1;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        AddPresenceAndDurationReason(ct);
        AddMinStartReason(ct, window_start);
        AddMaxEndReason(ct, window_end);
      }

      return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
    }
  }
  return true;
}

bool DisjunctiveConstraint::DetectablePrecedencePass() {
  SCOPED_TIME_STAT(&stats_);

  UpdateTaskByIncreasingMinEnd();
  UpdateTaskByDecreasingMaxStart();

  const int num_tasks = start_vars_.size();
  int queue_index = num_tasks - 1;
  task_set_.Clear();
  for (const int t : task_by_increasing_min_end_) {
    const IntegerValue min_end = MinEnd(t);
    while (queue_index >= 0) {
      const int to_insert = task_by_decreasing_max_start_[queue_index];
      if (min_end <= MaxStart(to_insert)) break;
      if (task_is_currently_present_[to_insert]) {
        task_set_.AddEntry(
            {to_insert, MinStart(to_insert), MinDuration(to_insert)});
      }
      --queue_index;
    }

    // task_set_ contains all the tasks that must be executed before t.
    // They are in "dectable precedence" because their max_start is smaller than
    // the min-end of t like so:
    //          [(the task t)
    //                      (a task in task_set_)]
    // From there, we deduce that the min-start of t is greater or equal to the
    // min-end of the critical tasks.
    //
    // Note that this works as well when task_is_currently_present_[t] is false.
    int critical_index = 0;
    const IntegerValue min_end_of_critical_tasks =
        task_set_.ComputeMinEnd(/*task_to_ignore=*/t, &critical_index);
    if (min_end_of_critical_tasks > MinStart(t)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      literal_reason_.clear();
      integer_reason_.clear();

      // We need:
      // - MaxStart(ct) < MinEnd(t) for the detectable precedence.
      // - MinStart(ct) > window_start for the min_end_of_critical_tasks reason.
      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        DCHECK(task_is_currently_present_[ct]);
        DCHECK_GE(MinStart(ct), window_start);
        DCHECK_LT(MaxStart(ct), min_end);
        AddPresenceAndDurationReason(ct);
        AddMinStartReason(ct, window_start);
        AddMaxStartReason(ct, min_end - 1);
      }

      // Add the reason for t (we only need the min-end).
      AddMinEndReason(t, MinEnd(t));

      // This augment the min-start of t and subsequently it can augment the
      // next min_end_of_critical_tasks, but our deduction is still valid.
      if (!IncreaseMinStart(t, min_end_of_critical_tasks)) return false;

      // We need to reorder t inside task_set_.
      task_set_.NotifyEntryIsNowLastIfPresent({t, MinStart(t), MinDuration(t)});
    }
  }
  return true;
}

bool DisjunctiveConstraint::PrecedencePass() {
  SCOPED_TIME_STAT(&stats_);
  const int num_tasks = start_vars_.size();

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
      task_set_.AddUnsortedEntry({task, MinStart(task), MinDuration(task)});
    }
    if (task_set_.SortedTasks().size() < 2) continue;
    task_set_.Sort();

    const IntegerValue min_end =
        task_set_.ComputeMinEnd(/*task_to_ignore=*/-1, &critical_index);
    if (min_end > integer_trail_->LowerBound(var)) {
      const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
      literal_reason_.clear();
      integer_reason_.clear();

      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        DCHECK(task_is_currently_present_[ct]);
        DCHECK_GE(MinStart(ct), window_start);
        AddPresenceAndDurationReason(ct);
        AddMinStartReason(ct, window_start);
        if (reason_for_beeing_before_[ct] != kNoLiteralIndex) {
          literal_reason_.push_back(
              Literal(reason_for_beeing_before_[ct]).Negated());
        }
      }

      // TODO(user): If var is actually a min-start of an interval, we
      // could push the min-end and check the interval consistency right away.
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, min_end),
                                   literal_reason_, integer_reason_)) {
        return false;
      }
    }
  }
  return true;
}

bool DisjunctiveConstraint::NotLastPass() {
  SCOPED_TIME_STAT(&stats_);
  UpdateTaskByDecreasingMaxStart();
  UpdateTaskByDecreasingMaxEnd();

  const int num_tasks = start_vars_.size();
  int queue_index = num_tasks - 1;

  task_set_.Clear();
  for (auto it = task_by_decreasing_max_end_.rbegin();
       it != task_by_decreasing_max_end_.rend(); ++it) {
    const int t = *it;
    // task_set_ contains all the tasks that must start before the max-end of t.
    // These are the only candidates that have a chance to decrease the max-end
    // of t.
    const IntegerValue max_end = MaxEnd(t);
    while (queue_index >= 0) {
      const int to_insert = task_by_decreasing_max_start_[queue_index];
      if (max_end <= MaxStart(to_insert)) break;
      if (task_is_currently_present_[to_insert]) {
        task_set_.AddEntry(
            {to_insert, MinStart(to_insert), MinDuration(to_insert)});
      }
      --queue_index;
    }

    // In the following case, task t cannot be after all the critical tasks
    // (i.e. it cannot be last):
    //
    // [(critical tasks)
    //              | <- t max-start
    //
    // So we can deduce that the max-end of t is smaller than or equal to the
    // largest max-start of the critical tasks.
    //
    // Note that this works as well when task_is_currently_present_[t] is false.
    int critical_index = 0;
    const IntegerValue min_end_of_critical_tasks =
        task_set_.ComputeMinEnd(/*task_to_ignore=*/t, &critical_index);
    if (min_end_of_critical_tasks <= MaxStart(t)) continue;

    // Find the largest max-start of the critical tasks (excluding t). The
    // max-end for t need to be smaller than or equal to this.
    IntegerValue largest_ct_max_start = kMinIntegerValue;
    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
    for (int i = critical_index; i < sorted_tasks.size(); ++i) {
      const int ct = sorted_tasks[i].task;
      if (t == ct) continue;
      const IntegerValue max_start = MaxStart(ct);
      if (max_start > largest_ct_max_start) {
        largest_ct_max_start = max_start;
      }
    }

    // If we have any critical task, the test will always be true because
    // of the tasks we put in task_set_.
    DCHECK(largest_ct_max_start == kMinIntegerValue ||
           max_end > largest_ct_max_start);
    if (max_end > largest_ct_max_start) {
      literal_reason_.clear();
      integer_reason_.clear();

      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (ct == t) continue;
        AddPresenceAndDurationReason(ct);
        AddMinStartReason(ct, window_start);
        AddMaxStartReason(ct, largest_ct_max_start);
      }

      // Add the reason for t, we only need the max-start.
      AddMaxStartReason(t, min_end_of_critical_tasks - 1);

      // Enqueue the new max-end for t.
      // Note that changing it will not influence the rest of the loop.
      if (!DecreaseMaxEnd(t, largest_ct_max_start)) return false;
    }
  }
  return true;
}

bool DisjunctiveConstraint::EdgeFindingPass() {
  SCOPED_TIME_STAT(&stats_);
  const int num_tasks = start_vars_.size();

  // Some task in sorted_tasks_ will be considered "gray".
  // When computing the min-end of the sorted task, we will compute it for:
  // - All the non-gray task
  // - All the non-gray task + at most one gray task.
  is_gray_.resize(num_tasks, false);

  // The algorithm is slightly different than the others. We start with a full
  // sorted_tasks, and remove tasks one by one starting by the one with highest
  // max-end.
  task_set_.Clear();
  UpdateTaskByIncreasingMinStart();
  int num_not_gray = 0;
  for (const int t : task_by_increasing_min_start_) {
    task_set_.AddOrderedLastEntry({t, MinStart(t), MinDuration(t)});

    // We already mark all the non-present task as gray.
    if (task_is_currently_present_[t]) {
      ++num_not_gray;
      is_gray_[t] = false;
    } else {
      is_gray_[t] = true;
    }
  }

  UpdateTaskByDecreasingMaxEnd();
  int decreasing_max_end_index = 0;

  // At each iteration we either transform a non-gray task into a gray one or
  // remove a gray task, so this loop is linear in complexity.
  while (num_not_gray > 0) {
    // This is really similar to task_set_.ComputeMinEnd() except that we
    // deal properly with the "gray" tasks.
    int critical_index = -1;
    int gray_critical_index = -1;

    // Respectively without gray task and with at most one one gray task.
    IntegerValue min_end_of_critical_tasks = kMinIntegerValue;
    IntegerValue min_end_of_critical_tasks_with_gray = kMinIntegerValue;

    // The index of the gray task in the critical tasks with one gray, if any.
    int gray_task_index = -1;

    // Set decreasing_max_end_index to the next non-gray task.
    while (is_gray_[task_by_decreasing_max_end_[decreasing_max_end_index]]) {
      ++decreasing_max_end_index;
      CHECK_LT(decreasing_max_end_index, num_tasks);
    }
    const IntegerValue non_gray_max_end =
        MaxEnd(task_by_decreasing_max_end_[decreasing_max_end_index]);

    const std::vector<TaskSet::Entry>& sorted_tasks = task_set_.SortedTasks();
    for (int i = 0; i < sorted_tasks.size(); ++i) {
      const TaskSet::Entry& e = sorted_tasks[i];
      if (is_gray_[e.task]) {
        if (e.min_start >= min_end_of_critical_tasks) {
          // Is this gray task increasing the min-end by itself?
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
        DCHECK_LE(MaxEnd(e.task), non_gray_max_end);

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
      literal_reason_.clear();
      integer_reason_.clear();

      // We need the reasons for the critical tasks to fall in:
      const IntegerValue window_start = sorted_tasks[critical_index].min_start;
      const IntegerValue window_end = min_end_of_critical_tasks - 1;
      for (int i = critical_index; i < sorted_tasks.size(); ++i) {
        const int ct = sorted_tasks[i].task;
        if (is_gray_[ct]) continue;
        AddPresenceAndDurationReason(ct);
        AddMinStartReason(ct, window_start);
        AddMaxEndReason(ct, window_end);
      }
      return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
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
    //                           ^ max-end without the gray task.
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
      if (min_end_of_critical_tasks > MinStart(gray_task)) {
        literal_reason_.clear();
        integer_reason_.clear();
        const IntegerValue low_start =
            sorted_tasks[gray_critical_index].min_start;
        const IntegerValue high_start = sorted_tasks[critical_index].min_start;
        const IntegerValue window_end = min_end_of_critical_tasks_with_gray - 1;
        for (int i = gray_critical_index; i < sorted_tasks.size(); ++i) {
          const int ct = sorted_tasks[i].task;
          if (is_gray_[ct]) continue;
          AddPresenceAndDurationReason(ct);
          AddMinStartReason(ct, i >= critical_index ? high_start : low_start);
          AddMaxEndReason(ct, window_end);
        }

        // Add the reason for the gray_task (we don't need the max-end or
        // presence reason).
        AddMinDurationReason(gray_task);
        AddMinStartReason(gray_task, low_start);

        // Enqueue the new min-start for gray_task.
        //
        // TODO(user): propagate the precedence Boolean here too? I think it
        // will be more powerful. Even if eventually all these precedence will
        // become detectable (see Petr Villim PhD).
        if (!IncreaseMinStart(gray_task, min_end_of_critical_tasks)) {
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
        DCHECK(
            !is_gray_[task_by_decreasing_max_end_[decreasing_max_end_index]]);
        is_gray_[task_by_decreasing_max_end_[decreasing_max_end_index]] = true;
        --num_not_gray;
        if (num_not_gray == 0) break;

        // Set decreasing_max_end_index to the next non-gray task.
        while (
            is_gray_[task_by_decreasing_max_end_[decreasing_max_end_index]]) {
          ++decreasing_max_end_index;
          CHECK_LT(decreasing_max_end_index, num_tasks);
        }
      } while (MaxEnd(task_by_decreasing_max_end_[decreasing_max_end_index]) >=
               threshold);
    }
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
