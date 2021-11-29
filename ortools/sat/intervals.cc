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

#include "ortools/sat/intervals.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ortools/sat/integer.h"
#include "ortools/util/sort.h"

namespace operations_research {
namespace sat {

IntervalVariable IntervalsRepository::CreateInterval(IntegerVariable start,
                                                     IntegerVariable end,
                                                     IntegerVariable size,
                                                     IntegerValue fixed_size,
                                                     LiteralIndex is_present) {
  return CreateInterval(AffineExpression(start), AffineExpression(end),
                        size == kNoIntegerVariable
                            ? AffineExpression(fixed_size)
                            : AffineExpression(size),
                        is_present, /*add_linear_relation=*/true);
}

IntervalVariable IntervalsRepository::CreateInterval(AffineExpression start,
                                                     AffineExpression end,
                                                     AffineExpression size,
                                                     LiteralIndex is_present,
                                                     bool add_linear_relation) {
  // Create the interval.
  const IntervalVariable i(starts_.size());
  starts_.push_back(start);
  ends_.push_back(end);
  sizes_.push_back(size);
  is_present_.push_back(is_present);

  std::vector<Literal> enforcement_literals;
  if (is_present != kNoLiteralIndex) {
    enforcement_literals.push_back(Literal(is_present));
  }

  if (add_linear_relation) {
    LinearConstraintBuilder builder(model_, IntegerValue(0), IntegerValue(0));
    builder.AddTerm(Start(i), IntegerValue(1));
    builder.AddTerm(Size(i), IntegerValue(1));
    builder.AddTerm(End(i), IntegerValue(-1));
    LoadConditionalLinearConstraint(enforcement_literals, builder.Build(),
                                    model_);
  }

  return i;
}

SchedulingConstraintHelper::SchedulingConstraintHelper(
    const std::vector<IntervalVariable>& tasks, Model* model)
    : trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      precedences_(model->GetOrCreate<PrecedencesPropagator>()) {
  starts_.clear();
  ends_.clear();
  minus_ends_.clear();
  minus_starts_.clear();
  sizes_.clear();
  reason_for_presence_.clear();

  auto* repository = model->GetOrCreate<IntervalsRepository>();
  for (const IntervalVariable i : tasks) {
    if (repository->IsOptional(i)) {
      reason_for_presence_.push_back(repository->PresenceLiteral(i).Index());
    } else {
      reason_for_presence_.push_back(kNoLiteralIndex);
    }
    sizes_.push_back(repository->Size(i));
    starts_.push_back(repository->Start(i));
    ends_.push_back(repository->End(i));
    minus_starts_.push_back(repository->Start(i).Negated());
    minus_ends_.push_back(repository->End(i).Negated());
  }

  RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  InitSortedVectors();
  if (!SynchronizeAndSetTimeDirection(true)) {
    model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
  }
}

SchedulingConstraintHelper::SchedulingConstraintHelper(int num_tasks,
                                                       Model* model)
    : trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      precedences_(model->GetOrCreate<PrecedencesPropagator>()) {
  starts_.resize(num_tasks);
  CHECK_EQ(NumTasks(), num_tasks);
}

bool SchedulingConstraintHelper::Propagate() {
  recompute_all_cache_ = true;
  return true;
}

bool SchedulingConstraintHelper::IncrementalPropagate(
    const std::vector<int>& watch_indices) {
  for (const int t : watch_indices) recompute_cache_[t] = true;
  return true;
}

void SchedulingConstraintHelper::SetLevel(int level) {
  // If there was an Untrail before, we need to refresh the cache so that
  // we never have value from lower in the search tree.
  //
  // TODO(user): We could be smarter here, but then this is not visible in our
  // cpu_profile since we call many times IncrementalPropagate() for each new
  // decision, but just call Propagate() once after each Untrail().
  if (level < previous_level_) {
    recompute_all_cache_ = true;
  }
  previous_level_ = level;
}

void SchedulingConstraintHelper::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  const int num_tasks = starts_.size();
  for (int t = 0; t < num_tasks; ++t) {
    watcher->WatchIntegerVariable(sizes_[t].var, id, t);
    watcher->WatchIntegerVariable(starts_[t].var, id, t);
    watcher->WatchIntegerVariable(ends_[t].var, id, t);
  }
  watcher->SetPropagatorPriority(id, 0);

  // Note that it is important to register with the integer_trail_ so we are
  // ALWAYS called before any propagator that depends on this helper.
  integer_trail_->RegisterReversibleClass(this);
}

bool SchedulingConstraintHelper::UpdateCachedValues(int t) {
  recompute_cache_[t] = false;
  if (IsAbsent(t)) return true;

  IntegerValue smin = integer_trail_->LowerBound(starts_[t]);
  IntegerValue smax = integer_trail_->UpperBound(starts_[t]);
  IntegerValue emin = integer_trail_->LowerBound(ends_[t]);
  IntegerValue emax = integer_trail_->UpperBound(ends_[t]);

  // We take the max for the corner case where the size of an optional interval
  // is used elsewhere and has a domain with negative value.
  //
  // TODO(user): maybe we should just disallow size with a negative domain, but
  // is is harder to enforce if we have a linear expression for size.
  IntegerValue dmin =
      std::max(IntegerValue(0), integer_trail_->LowerBound(sizes_[t]));
  IntegerValue dmax = integer_trail_->UpperBound(sizes_[t]);

  // Detect first if we have a conflict using the relation start + size = end.
  if (dmax < 0) {
    AddSizeMaxReason(t, dmax);
    return PushTaskAbsence(t);
  }
  if (smin + dmin - emax > 0) {
    ClearReason();
    AddStartMinReason(t, smin);
    AddSizeMinReason(t, dmin);
    AddEndMaxReason(t, emax);
    return PushTaskAbsence(t);
  }
  if (smax + dmax - emin < 0) {
    ClearReason();
    AddStartMaxReason(t, smax);
    AddSizeMaxReason(t, dmax);
    AddEndMinReason(t, emin);
    return PushTaskAbsence(t);
  }

  // Sometimes, for optional interval with non-optional bounds, this propagation
  // give tighter bounds. We always consider the value assuming
  // the interval is present.
  //
  // Note that this is also useful in case not everything was propagated. Note
  // also that since there is no conflict, we reach the fix point in one pass.
  smin = std::max(smin, emin - dmax);
  smax = std::min(smax, emax - dmin);
  dmin = std::max(dmin, emin - smax);
  emin = std::max(emin, smin + dmin);
  emax = std::min(emax, smax + dmax);

  cached_start_min_[t] = smin;
  cached_end_min_[t] = emin;
  cached_negated_start_max_[t] = -smax;
  cached_negated_end_max_[t] = -emax;
  cached_size_min_[t] = dmin;

  // Note that we use the cached value here for EndMin()/StartMax().
  const IntegerValue new_shifted_start_min = EndMin(t) - dmin;
  if (new_shifted_start_min != cached_shifted_start_min_[t]) {
    recompute_shifted_start_min_ = true;
    cached_shifted_start_min_[t] = new_shifted_start_min;
  }
  const IntegerValue new_negated_shifted_end_max = -(StartMax(t) + dmin);
  if (new_negated_shifted_end_max != cached_negated_shifted_end_max_[t]) {
    recompute_negated_shifted_end_max_ = true;
    cached_negated_shifted_end_max_[t] = new_negated_shifted_end_max;
  }
  return true;
}

bool SchedulingConstraintHelper::ResetFromSubset(
    const SchedulingConstraintHelper& other, absl::Span<const int> tasks) {
  current_time_direction_ = other.current_time_direction_;

  const int num_tasks = tasks.size();
  starts_.resize(num_tasks);
  ends_.resize(num_tasks);
  minus_ends_.resize(num_tasks);
  minus_starts_.resize(num_tasks);
  sizes_.resize(num_tasks);
  reason_for_presence_.resize(num_tasks);
  for (int i = 0; i < num_tasks; ++i) {
    const int t = tasks[i];
    starts_[i] = other.starts_[t];
    ends_[i] = other.ends_[t];
    minus_ends_[i] = other.minus_ends_[t];
    minus_starts_[i] = other.minus_starts_[t];
    sizes_[i] = other.sizes_[t];
    reason_for_presence_[i] = other.reason_for_presence_[t];
  }

  InitSortedVectors();
  return SynchronizeAndSetTimeDirection(true);
}

void SchedulingConstraintHelper::InitSortedVectors() {
  const int num_tasks = starts_.size();

  recompute_all_cache_ = true;
  recompute_cache_.resize(num_tasks, true);

  cached_shifted_start_min_.resize(num_tasks);
  cached_negated_shifted_end_max_.resize(num_tasks);
  cached_size_min_.resize(num_tasks);
  cached_start_min_.resize(num_tasks);
  cached_end_min_.resize(num_tasks);
  cached_negated_start_max_.resize(num_tasks);
  cached_negated_end_max_.resize(num_tasks);

  task_by_increasing_start_min_.resize(num_tasks);
  task_by_increasing_end_min_.resize(num_tasks);
  task_by_decreasing_start_max_.resize(num_tasks);
  task_by_decreasing_end_max_.resize(num_tasks);
  task_by_increasing_shifted_start_min_.resize(num_tasks);
  task_by_negated_shifted_end_max_.resize(num_tasks);
  for (int t = 0; t < num_tasks; ++t) {
    task_by_increasing_start_min_[t].task_index = t;
    task_by_increasing_end_min_[t].task_index = t;
    task_by_decreasing_start_max_[t].task_index = t;
    task_by_decreasing_end_max_[t].task_index = t;
    task_by_increasing_shifted_start_min_[t].task_index = t;
    task_by_negated_shifted_end_max_[t].task_index = t;
  }

  recompute_shifted_start_min_ = true;
  recompute_negated_shifted_end_max_ = true;
}

void SchedulingConstraintHelper::SetTimeDirection(bool is_forward) {
  if (current_time_direction_ != is_forward) {
    current_time_direction_ = is_forward;

    std::swap(starts_, minus_ends_);
    std::swap(ends_, minus_starts_);

    std::swap(task_by_increasing_start_min_, task_by_decreasing_end_max_);
    std::swap(task_by_increasing_end_min_, task_by_decreasing_start_max_);
    std::swap(task_by_increasing_shifted_start_min_,
              task_by_negated_shifted_end_max_);

    std::swap(cached_start_min_, cached_negated_end_max_);
    std::swap(cached_end_min_, cached_negated_start_max_);
    std::swap(cached_shifted_start_min_, cached_negated_shifted_end_max_);
    std::swap(recompute_shifted_start_min_, recompute_negated_shifted_end_max_);
  }
}

bool SchedulingConstraintHelper::SynchronizeAndSetTimeDirection(
    bool is_forward) {
  SetTimeDirection(is_forward);
  if (recompute_all_cache_) {
    for (int t = 0; t < recompute_cache_.size(); ++t) {
      if (!UpdateCachedValues(t)) return false;
    }
  } else {
    for (int t = 0; t < recompute_cache_.size(); ++t) {
      if (recompute_cache_[t])
        if (!UpdateCachedValues(t)) return false;
    }
  }
  recompute_all_cache_ = false;
  return true;
}

const std::vector<TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingStartMin() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_increasing_start_min_[i];
    ref.time = StartMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_start_min_.begin(),
                  task_by_increasing_start_min_.end());
  return task_by_increasing_start_min_;
}

const std::vector<TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingEndMin() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_increasing_end_min_[i];
    ref.time = EndMin(ref.task_index);
  }
  IncrementalSort(task_by_increasing_end_min_.begin(),
                  task_by_increasing_end_min_.end());
  return task_by_increasing_end_min_;
}

const std::vector<TaskTime>&
SchedulingConstraintHelper::TaskByDecreasingStartMax() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_decreasing_start_max_[i];
    ref.time = StartMax(ref.task_index);
  }
  IncrementalSort(task_by_decreasing_start_max_.begin(),
                  task_by_decreasing_start_max_.end(),
                  std::greater<TaskTime>());
  return task_by_decreasing_start_max_;
}

const std::vector<TaskTime>&
SchedulingConstraintHelper::TaskByDecreasingEndMax() {
  const int num_tasks = NumTasks();
  for (int i = 0; i < num_tasks; ++i) {
    TaskTime& ref = task_by_decreasing_end_max_[i];
    ref.time = EndMax(ref.task_index);
  }
  IncrementalSort(task_by_decreasing_end_max_.begin(),
                  task_by_decreasing_end_max_.end(), std::greater<TaskTime>());
  return task_by_decreasing_end_max_;
}

const std::vector<TaskTime>&
SchedulingConstraintHelper::TaskByIncreasingShiftedStartMin() {
  if (recompute_shifted_start_min_) {
    recompute_shifted_start_min_ = false;
    const int num_tasks = NumTasks();
    bool is_sorted = true;
    IntegerValue previous = kMinIntegerValue;
    for (int i = 0; i < num_tasks; ++i) {
      TaskTime& ref = task_by_increasing_shifted_start_min_[i];
      ref.time = ShiftedStartMin(ref.task_index);
      is_sorted = is_sorted && ref.time >= previous;
      previous = ref.time;
    }
    if (is_sorted) return task_by_increasing_shifted_start_min_;
    IncrementalSort(task_by_increasing_shifted_start_min_.begin(),
                    task_by_increasing_shifted_start_min_.end());
  }
  return task_by_increasing_shifted_start_min_;
}

// Produces a relaxed reason for StartMax(before) < EndMin(after).
void SchedulingConstraintHelper::AddReasonForBeingBefore(int before,
                                                         int after) {
  AddOtherReason(before);
  AddOtherReason(after);

  std::vector<IntegerVariable> vars;

  // Reason for StartMax(before).
  const IntegerValue smax_before = StartMax(before);
  if (smax_before >= integer_trail_->UpperBound(starts_[before])) {
    if (starts_[before].var != kNoIntegerVariable) {
      vars.push_back(NegationOf(starts_[before].var));
    }
  } else {
    if (ends_[before].var != kNoIntegerVariable) {
      vars.push_back(NegationOf(ends_[before].var));
    }
    if (sizes_[before].var != kNoIntegerVariable) {
      vars.push_back(sizes_[before].var);
    }
  }

  // Reason for EndMin(after);
  const IntegerValue emin_after = EndMin(after);
  if (emin_after <= integer_trail_->LowerBound(ends_[after])) {
    if (ends_[after].var != kNoIntegerVariable) {
      vars.push_back(ends_[after].var);
    }
  } else {
    if (starts_[after].var != kNoIntegerVariable) {
      vars.push_back(starts_[after].var);
    }
    if (sizes_[after].var != kNoIntegerVariable) {
      vars.push_back(sizes_[after].var);
    }
  }

  DCHECK_LT(smax_before, emin_after);
  const IntegerValue slack = emin_after - smax_before - 1;
  integer_trail_->AppendRelaxedLinearReason(
      slack, std::vector<IntegerValue>(vars.size(), IntegerValue(1)), vars,
      &integer_reason_);
}

bool SchedulingConstraintHelper::PushIntegerLiteral(IntegerLiteral lit) {
  CHECK(other_helper_ == nullptr);
  return integer_trail_->Enqueue(lit, literal_reason_, integer_reason_);
}

bool SchedulingConstraintHelper::PushIntegerLiteralIfTaskPresent(
    int t, IntegerLiteral lit) {
  if (IsAbsent(t)) return true;
  AddOtherReason(t);
  ImportOtherReasons();
  if (IsOptional(t)) {
    return integer_trail_->ConditionalEnqueue(
        PresenceLiteral(t), lit, &literal_reason_, &integer_reason_);
  }
  return integer_trail_->Enqueue(lit, literal_reason_, integer_reason_);
}

// We also run directly the precedence propagator for this variable so that when
// we push an interval start for example, we have a chance to push its end.
bool SchedulingConstraintHelper::PushIntervalBound(int t, IntegerLiteral lit) {
  if (!PushIntegerLiteralIfTaskPresent(t, lit)) return false;
  if (IsAbsent(t)) return true;
  if (!precedences_->PropagateOutgoingArcs(lit.var)) return false;
  if (!UpdateCachedValues(t)) return false;
  return true;
}

bool SchedulingConstraintHelper::IncreaseStartMin(int t,
                                                  IntegerValue new_start_min) {
  if (starts_[t].var == kNoIntegerVariable) {
    if (new_start_min > starts_[t].constant) return PushTaskAbsence(t);
    return true;
  }
  return PushIntervalBound(t, starts_[t].GreaterOrEqual(new_start_min));
}

bool SchedulingConstraintHelper::DecreaseEndMax(int t,
                                                IntegerValue new_end_max) {
  if (ends_[t].var == kNoIntegerVariable) {
    if (new_end_max < ends_[t].constant) return PushTaskAbsence(t);
    return true;
  }
  return PushIntervalBound(t, ends_[t].LowerOrEqual(new_end_max));
}

bool SchedulingConstraintHelper::PushTaskAbsence(int t) {
  if (IsAbsent(t)) return true;
  if (!IsOptional(t)) return ReportConflict();

  AddOtherReason(t);

  if (IsPresent(t)) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]).Negated());
    return ReportConflict();
  }
  ImportOtherReasons();
  integer_trail_->EnqueueLiteral(Literal(reason_for_presence_[t]).Negated(),
                                 literal_reason_, integer_reason_);
  return true;
}

bool SchedulingConstraintHelper::PushTaskPresence(int t) {
  DCHECK_NE(reason_for_presence_[t], kNoLiteralIndex);
  DCHECK(!IsPresent(t));

  AddOtherReason(t);

  if (IsAbsent(t)) {
    literal_reason_.push_back(Literal(reason_for_presence_[t]));
    return ReportConflict();
  }
  ImportOtherReasons();
  integer_trail_->EnqueueLiteral(Literal(reason_for_presence_[t]),
                                 literal_reason_, integer_reason_);
  return true;
}

bool SchedulingConstraintHelper::ReportConflict() {
  ImportOtherReasons();
  return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
}

void SchedulingConstraintHelper::WatchAllTasks(int id,
                                               GenericLiteralWatcher* watcher,
                                               bool watch_start_max,
                                               bool watch_end_max) const {
  const int num_tasks = starts_.size();
  for (int t = 0; t < num_tasks; ++t) {
    watcher->WatchLowerBound(starts_[t], id);
    watcher->WatchLowerBound(ends_[t], id);
    watcher->WatchLowerBound(sizes_[t], id);
    if (watch_start_max) {
      watcher->WatchUpperBound(starts_[t], id);
    }
    if (watch_end_max) {
      watcher->WatchUpperBound(ends_[t], id);
    }
    if (!IsPresent(t) && !IsAbsent(t)) {
      watcher->WatchLiteral(Literal(reason_for_presence_[t]), id);
    }
  }
}

void SchedulingConstraintHelper::AddOtherReason(int t) {
  if (other_helper_ == nullptr || already_added_to_other_reasons_[t]) return;
  already_added_to_other_reasons_[t] = true;
  const int mapped_t = map_to_other_helper_[t];
  other_helper_->AddStartMaxReason(mapped_t, event_for_other_helper_);
  other_helper_->AddEndMinReason(mapped_t, event_for_other_helper_ + 1);
}

void SchedulingConstraintHelper::ImportOtherReasons() {
  if (other_helper_ != nullptr) ImportOtherReasons(*other_helper_);
}

void SchedulingConstraintHelper::ImportOtherReasons(
    const SchedulingConstraintHelper& other_helper) {
  literal_reason_.insert(literal_reason_.end(),
                         other_helper.literal_reason_.begin(),
                         other_helper.literal_reason_.end());
  integer_reason_.insert(integer_reason_.end(),
                         other_helper.integer_reason_.begin(),
                         other_helper.integer_reason_.end());
}

std::string SchedulingConstraintHelper::TaskDebugString(int t) const {
  return absl::StrCat("t=", t, " is_present=", IsPresent(t), " size=[",
                      SizeMin(t).value(), ",", SizeMax(t).value(), "]",
                      " start=[", StartMin(t).value(), ",", StartMax(t).value(),
                      "]", " end=[", EndMin(t).value(), ",", EndMax(t).value(),
                      "]");
}

IntegerValue SchedulingConstraintHelper::GetMinOverlap(int t,
                                                       IntegerValue start,
                                                       IntegerValue end) const {
  return std::min(std::min(end - start, SizeMin(t)),
                  std::min(EndMin(t) - start, end - StartMax(t)));
}

}  // namespace sat
}  // namespace operations_research
