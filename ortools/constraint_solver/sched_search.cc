// Copyright 2010-2017 Google
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


#include <cstring>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/string_array.h"

namespace operations_research {
namespace {
int64 ValueToIndex(int64 value) { return value - 1; }

int64 IndexToValue(int64 index) { return index + 1; }
}  // namespace

// ----- SequenceVar -----

// TODO(user): Add better class invariants, in particular checks
// that ranked_first, ranked_last, and unperformed are truly disjoint.

SequenceVar::SequenceVar(Solver* const s,
                         const std::vector<IntervalVar*>& intervals,
                         const std::vector<IntVar*>& nexts, const std::string& name)
    : PropagationBaseObject(s),
      intervals_(intervals),
      nexts_(nexts),
      previous_(nexts.size() + 1, -1) {
  set_name(name);
}

SequenceVar::~SequenceVar() {}

IntervalVar* SequenceVar::Interval(int index) const {
  return intervals_[index];
}

IntVar* SequenceVar::Next(int index) const { return nexts_[index]; }

std::string SequenceVar::DebugString() const {
  int64 hmin, hmax, dmin, dmax;
  HorizonRange(&hmin, &hmax);
  DurationRange(&dmin, &dmax);
  int unperformed = 0;
  int ranked = 0;
  int not_ranked = 0;
  ComputeStatistics(&ranked, &not_ranked, &unperformed);
  return StringPrintf("%s(horizon = %" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                      "d, duration = %" GG_LL_FORMAT "d..%" GG_LL_FORMAT
                      "d, not ranked = %d, ranked = %d, nexts = [%s])",
                      name().c_str(), hmin, hmax, dmin, dmax, not_ranked,
                      ranked, JoinDebugStringPtr(nexts_, ", ").c_str());
}

void SequenceVar::Accept(ModelVisitor* const visitor) const {
  visitor->VisitSequenceVariable(this);
}

void SequenceVar::DurationRange(int64* const dmin, int64* const dmax) const {
  int64 dur_min = 0;
  int64 dur_max = 0;
  for (int i = 0; i < intervals_.size(); ++i) {
    IntervalVar* const t = intervals_[i];
    if (t->MayBePerformed()) {
      if (t->MustBePerformed()) {
        dur_min += t->DurationMin();
      }
      dur_max += t->DurationMax();
    }
  }
  *dmin = dur_min;
  *dmax = dur_max;
}

void SequenceVar::HorizonRange(int64* const hmin, int64* const hmax) const {
  int64 hor_min = kint64max;
  int64 hor_max = kint64min;
  for (int i = 0; i < intervals_.size(); ++i) {
    IntervalVar* const t = intervals_[i];
    if (t->MayBePerformed()) {
      IntervalVar* const t = intervals_[i];
      hor_min = std::min(hor_min, t->StartMin());
      hor_max = std::max(hor_max, t->EndMax());
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void SequenceVar::ActiveHorizonRange(int64* const hmin,
                                     int64* const hmax) const {
  std::unordered_set<int> decided;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      decided.insert(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < nexts_.size()) {
      decided.insert(ValueToIndex(first));
    } else {
      break;
    }
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      decided.insert(ValueToIndex(last));
    }
  }
  int64 hor_min = kint64max;
  int64 hor_max = kint64min;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (!ContainsKey(decided, i)) {
      IntervalVar* const t = intervals_[i];
      hor_min = std::min(hor_min, t->StartMin());
      hor_max = std::max(hor_max, t->EndMax());
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void SequenceVar::ComputeStatistics(int* const ranked, int* const not_ranked,
                                    int* const unperformed) const {
  *unperformed = 0;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      (*unperformed)++;
    }
  }
  *ranked = 0;
  int first = 0;
  while (first < nexts_.size() && nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    (*ranked)++;
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      (*ranked)++;
    }
  } else {  // We counted the sentinel.
    (*ranked)--;
  }
  *not_ranked = intervals_.size() - *ranked - *unperformed;
}

int SequenceVar::ComputeForwardFrontier() {
  int first = 0;
  while (first != nexts_.size() && nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
  }
  return first;
}

int SequenceVar::ComputeBackwardFrontier() {
  UpdatePrevious();
  int last = nexts_.size();
  while (previous_[last] != -1) {
    last = previous_[last];
  }
  return last;
}

void SequenceVar::ComputePossibleFirstsAndLasts(
    std::vector<int>* const possible_firsts,
    std::vector<int>* const possible_lasts) {
  possible_firsts->clear();
  possible_lasts->clear();
  std::unordered_set<int> to_check;
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->MayBePerformed()) {
      to_check.insert(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first == nexts_.size()) {
      return;
    }
    to_check.erase(ValueToIndex(first));
  }

  IntVar* const forward_var = nexts_[first];
  std::vector<int> candidates;
  int64 smallest_start_max = kint64max;
  int ssm_support = -1;
  for (int64 i = forward_var->Min(); i <= forward_var->Max(); ++i) {
    // TODO(user): use domain iterator.
    if (i != 0 && i < IndexToValue(intervals_.size()) &&
        intervals_[ValueToIndex(i)]->MayBePerformed() &&
        forward_var->Contains(i)) {
      const int candidate = ValueToIndex(i);
      candidates.push_back(candidate);
      if (intervals_[candidate]->MustBePerformed()) {
        if (smallest_start_max > intervals_[candidate]->StartMax()) {
          smallest_start_max = intervals_[candidate]->StartMax();
          ssm_support = candidate;
        }
      }
    }
  }
  for (int i = 0; i < candidates.size(); ++i) {
    const int candidate = candidates[i];
    if (candidate == ssm_support ||
        intervals_[candidate]->EndMin() <= smallest_start_max) {
      possible_firsts->push_back(candidate);
    }
  }

  UpdatePrevious();
  int last = nexts_.size();
  while (previous_[last] != -1) {
    last = previous_[last];
    to_check.erase(ValueToIndex(last));
  }

  candidates.clear();
  int64 biggest_end_min = kint64min;
  int bem_support = -1;
  for (const int candidate : to_check) {
    if (nexts_[IndexToValue(candidate)]->Contains(last)) {
      candidates.push_back(candidate);
      if (intervals_[candidate]->MustBePerformed()) {
        if (biggest_end_min < intervals_[candidate]->EndMin()) {
          biggest_end_min = intervals_[candidate]->EndMin();
          bem_support = candidate;
        }
      }
    }
  }

  for (int i = 0; i < candidates.size(); ++i) {
    const int candidate = candidates[i];
    if (candidate == bem_support ||
        intervals_[candidate]->StartMax() >= biggest_end_min) {
      possible_lasts->push_back(candidate);
    }
  }
}

void SequenceVar::RankSequence(const std::vector<int>& rank_first,
                               const std::vector<int>& rank_last,
                               const std::vector<int>& unperformed) {
  solver()->GetPropagationMonitor()->RankSequence(this, rank_first, rank_last,
                                                  unperformed);
  // Mark unperformed.
  for (const int value : unperformed) {
    intervals_[value]->SetPerformed(false);
  }
  // Forward.
  int forward = 0;
  for (int i = 0; i < rank_first.size(); ++i) {
    const int next = 1 + rank_first[i];
    nexts_[forward]->SetValue(next);
    forward = next;
  }
  // Backward.
  int backward = IndexToValue(intervals_.size());
  for (int i = 0; i < rank_last.size(); ++i) {
    const int next = 1 + rank_last[i];
    nexts_[next]->SetValue(backward);
    backward = next;
  }
}

void SequenceVar::RankFirst(int index) {
  solver()->GetPropagationMonitor()->RankFirst(this, index);
  intervals_[index]->SetPerformed(true);
  int forward_frontier = 0;
  while (forward_frontier != nexts_.size() &&
         nexts_[forward_frontier]->Bound()) {
    forward_frontier = nexts_[forward_frontier]->Min();
    if (forward_frontier == IndexToValue(index)) {
      return;
    }
  }
  DCHECK_LT(forward_frontier, nexts_.size());
  nexts_[forward_frontier]->SetValue(IndexToValue(index));
}

void SequenceVar::RankNotFirst(int index) {
  solver()->GetPropagationMonitor()->RankNotFirst(this, index);
  const int forward_frontier = ComputeForwardFrontier();
  if (forward_frontier < nexts_.size()) {
    nexts_[forward_frontier]->RemoveValue(IndexToValue(index));
  }
}

void SequenceVar::RankLast(int index) {
  solver()->GetPropagationMonitor()->RankLast(this, index);
  intervals_[index]->SetPerformed(true);
  UpdatePrevious();
  int backward_frontier = nexts_.size();
  while (previous_[backward_frontier] != -1) {
    backward_frontier = previous_[backward_frontier];
    if (backward_frontier == IndexToValue(index)) {
      return;
    }
  }
  DCHECK_NE(backward_frontier, 0);
  nexts_[IndexToValue(index)]->SetValue(backward_frontier);
}

void SequenceVar::RankNotLast(int index) {
  solver()->GetPropagationMonitor()->RankNotLast(this, index);
  const int backward_frontier = ComputeBackwardFrontier();
  nexts_[IndexToValue(index)]->RemoveValue(backward_frontier);
}

void SequenceVar::UpdatePrevious() const {
  for (int i = 0; i < intervals_.size() + 2; ++i) {
    previous_[i] = -1;
  }
  for (int i = 0; i < nexts_.size(); ++i) {
    if (nexts_[i]->Bound()) {
      previous_[nexts_[i]->Min()] = i;
    }
  }
}

void SequenceVar::FillSequence(std::vector<int>* const rank_first,
                               std::vector<int>* const rank_last,
                               std::vector<int>* const unperformed) const {
  CHECK(rank_first != nullptr);
  CHECK(rank_last != nullptr);
  CHECK(unperformed != nullptr);
  rank_first->clear();
  rank_last->clear();
  unperformed->clear();
  for (int i = 0; i < intervals_.size(); ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      unperformed->push_back(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < nexts_.size()) {
      rank_first->push_back(ValueToIndex(first));
    } else {
      break;
    }
  }
  if (first != nexts_.size()) {
    UpdatePrevious();
    int last = nexts_.size();
    while (previous_[last] != -1) {
      last = previous_[last];
      rank_last->push_back(ValueToIndex(last));
    }
  }
}

// ----- Decisions and DecisionBuilders on interval vars -----

// TODO(user) : treat optional intervals
// TODO(user) : Call DecisionVisitor and pass name of variable
namespace {
//
// Forward scheduling.
//
class ScheduleOrPostpone : public Decision {
 public:
  ScheduleOrPostpone(IntervalVar* const var, int64 est, int64* const marker)
      : var_(var), est_(est), marker_(marker) {}
  ~ScheduleOrPostpone() override {}

  void Apply(Solver* const s) override {
    var_->SetPerformed(true);
    if (est_.Value() < var_->StartMin()) {
      est_.SetValue(s, var_->StartMin());
    }
    var_->SetStartRange(est_.Value(), est_.Value());
  }

  void Refute(Solver* const s) override {
    s->SaveAndSetValue(marker_, est_.Value());
  }

  void Accept(DecisionVisitor* const visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitScheduleOrPostpone(var_, est_.Value());
  }

  std::string DebugString() const override {
    return StringPrintf("ScheduleOrPostpone(%s at %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), est_.Value());
  }

 private:
  IntervalVar* const var_;
  NumericalRev<int64> est_;
  int64* const marker_;
};

class SetTimesForward : public DecisionBuilder {
 public:
  explicit SetTimesForward(const std::vector<IntervalVar*>& vars)
      : vars_(vars), markers_(vars.size(), kint64min) {}

  ~SetTimesForward() override {}

  Decision* Next(Solver* const s) override {
    int64 best_est = kint64max;
    int64 best_lct = kint64max;
    int support = -1;
    // We are looking for the interval that has the smallest start min
    // (tie break with smallest end max) and is not postponed. And
    // you're going to schedule that interval at its start min.
    for (int i = 0; i < vars_.size(); ++i) {
      IntervalVar* const v = vars_[i];
      if (v->MayBePerformed() && v->StartMax() != v->StartMin() &&
          !IsPostponed(i) &&
          (v->StartMin() < best_est ||
           (v->StartMin() == best_est && v->EndMax() < best_lct))) {
        best_est = v->StartMin();
        best_lct = v->EndMax();
        support = i;
      }
    }
    // TODO(user) : remove this crude quadratic loop with
    // reversibles range reduction.
    if (support == -1) {  // All intervals are either fixed or postponed.
      UnperformPostponedTaskBefore(kint64max);
      return nullptr;
    }
    UnperformPostponedTaskBefore(best_est);
    return s->RevAlloc(
        new ScheduleOrPostpone(vars_[support], best_est, &markers_[support]));
  }

  std::string DebugString() const override { return "SetTimesForward()"; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  bool IsPostponed(int index) {
    DCHECK(vars_[index]->MayBePerformed());
    return vars_[index]->StartMin() <= markers_[index];
  }

  void UnperformPostponedTaskBefore(int64 date) {
    for (int i = 0; i < vars_.size(); ++i) {
      IntervalVar* const v = vars_[i];
      if (v->MayBePerformed() && v->StartMin() != v->StartMax() &&
          IsPostponed(i) &&
          // There are two rules here:
          //  - v->StartMax() <= date: the interval should have been scheduled
          //    as it cannot be scheduled later (assignment is chronological).
          //  - v->EndMin() <= date: The interval can fit before the current
          //    start date. In that case, it 'should' always fit, and as it has
          //    not be scheduled, then we are missing it. So, as a dominance
          //    rule, it should be marked as unperformed.
          (v->EndMin() <= date || v->StartMax() <= date)) {
        v->SetPerformed(false);
      }
    }
  }

  const std::vector<IntervalVar*> vars_;
  std::vector<int64> markers_;
};

//
// Backward scheduling.
//
class ScheduleOrExpedite : public Decision {
 public:
  ScheduleOrExpedite(IntervalVar* const var, int64 est, int64* const marker)
      : var_(var), est_(est), marker_(marker) {}
  ~ScheduleOrExpedite() override {}

  void Apply(Solver* const s) override {
    var_->SetPerformed(true);
    if (est_.Value() > var_->EndMax()) {
      est_.SetValue(s, var_->EndMax());
    }
    var_->SetEndRange(est_.Value(), est_.Value());
  }

  void Refute(Solver* const s) override {
    s->SaveAndSetValue(marker_, est_.Value() - 1);
  }

  void Accept(DecisionVisitor* const visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitScheduleOrExpedite(var_, est_.Value());
  }

  std::string DebugString() const override {
    return StringPrintf("ScheduleOrExpedite(%s at %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), est_.Value());
  }

 private:
  IntervalVar* const var_;
  NumericalRev<int64> est_;
  int64* const marker_;
};

class SetTimesBackward : public DecisionBuilder {
 public:
  explicit SetTimesBackward(const std::vector<IntervalVar*>& vars)
      : vars_(vars), markers_(vars.size(), kint64max) {}

  ~SetTimesBackward() override {}

  Decision* Next(Solver* const s) override {
    int64 best_end = kint64min;
    int64 best_start = kint64min;
    int support = -1;
    int refuted = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      IntervalVar* const v = vars_[i];
      if (v->MayBePerformed() && v->EndMax() > v->EndMin()) {
        if (v->EndMax() <= markers_[i] &&
            (v->EndMax() > best_end ||
             (v->EndMax() == best_end && v->StartMin() > best_start))) {
          best_end = v->EndMax();
          best_start = v->StartMin();
          support = i;
        } else {
          refuted++;
        }
      }
    }
    // TODO(user) : remove this crude quadratic loop with
    // reversibles range reduction.
    if (support == -1) {
      if (refuted == 0) {
        return nullptr;
      } else {
        s->Fail();
      }
    }
    return s->RevAlloc(new ScheduleOrExpedite(
        vars_[support], vars_[support]->EndMax(), &markers_[support]));
  }

  std::string DebugString() const override { return "SetTimesBackward()"; }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  const std::vector<IntervalVar*> vars_;
  std::vector<int64> markers_;
};

// ----- Decisions and DecisionBuilders on sequences -----

class RankFirst : public Decision {
 public:
  RankFirst(SequenceVar* const seq, int index)
      : sequence_(seq), index_(index) {}
  ~RankFirst() override {}

  void Apply(Solver* const s) override { sequence_->RankFirst(index_); }

  void Refute(Solver* const s) override { sequence_->RankNotFirst(index_); }

  void Accept(DecisionVisitor* const visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitRankFirstInterval(sequence_, index_);
  }

  std::string DebugString() const override {
    return StringPrintf("RankFirst(%s, %d)", sequence_->DebugString().c_str(),
                        index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};

class RankLast : public Decision {
 public:
  RankLast(SequenceVar* const seq, int index) : sequence_(seq), index_(index) {}
  ~RankLast() override {}

  void Apply(Solver* const s) override { sequence_->RankLast(index_); }

  void Refute(Solver* const s) override { sequence_->RankNotLast(index_); }

  void Accept(DecisionVisitor* const visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitRankLastInterval(sequence_, index_);
  }

  std::string DebugString() const override {
    return StringPrintf("RankLast(%s, %d)", sequence_->DebugString().c_str(),
                        index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};

class RankFirstIntervalVars : public DecisionBuilder {
 public:
  RankFirstIntervalVars(const std::vector<SequenceVar*>& sequences,
                        Solver::SequenceStrategy str)
      : sequences_(sequences), strategy_(str) {}

  ~RankFirstIntervalVars() override {}

  Decision* Next(Solver* const s) override {
    SequenceVar* best_sequence = nullptr;
    best_possible_firsts_.clear();
    while (true) {
      if (FindSequenceVar(s, &best_sequence)) {
        // No not create a choice point if it is not needed.
        DCHECK(best_sequence != nullptr);
        if (best_possible_firsts_.size() == 1 &&
            best_sequence->Interval(best_possible_firsts_.back())
                ->MustBePerformed()) {
          best_sequence->RankFirst(best_possible_firsts_.back());
          continue;
        }
        int best_interval = -1;
        if (!FindIntervalVar(s, best_sequence, &best_interval)) {
          s->Fail();
        }
        CHECK_NE(-1, best_interval);
        return s->RevAlloc(new RankFirst(best_sequence, best_interval));
      } else {
        return nullptr;
      }
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitSequenceArrayArgument(ModelVisitor::kSequencesArgument,
                                        sequences_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  // Selects the interval var to rank.
  bool FindIntervalVarOnStartMin(Solver* const s,
                                 SequenceVar* const best_sequence,
                                 int* const best_interval_index) {
    int best_interval = -1;
    int64 best_start_min = kint64max;
    for (int index = 0; index < best_possible_firsts_.size(); ++index) {
      const int candidate = best_possible_firsts_[index];
      IntervalVar* const interval = best_sequence->Interval(candidate);
      if (interval->StartMin() < best_start_min) {
        best_interval = candidate;
        best_start_min = interval->StartMin();
      }
    }
    if (best_interval == -1) {
      return false;
    } else {
      *best_interval_index = best_interval;
      return true;
    }
  }

  bool FindIntervalVarRandomly(Solver* const s,
                               SequenceVar* const best_sequence,
                               int* const best_interval_index) {
    DCHECK(!best_possible_firsts_.empty());
    const int index = s->Rand32(best_possible_firsts_.size());
    *best_interval_index = best_possible_firsts_[index];
    return true;
  }

  bool FindIntervalVar(Solver* const s, SequenceVar* const best_sequence,
                       int* const best_interval_index) {
    switch (strategy_) {
      case Solver::SEQUENCE_DEFAULT:
      case Solver::SEQUENCE_SIMPLE:
      case Solver::CHOOSE_MIN_SLACK_RANK_FORWARD:
        return FindIntervalVarOnStartMin(s, best_sequence, best_interval_index);
      case Solver::CHOOSE_RANDOM_RANK_FORWARD:
        return FindIntervalVarRandomly(s, best_sequence, best_interval_index);
      default:
        LOG(FATAL) << "Unknown strategy " << strategy_;
        return false;
    }
  }

  // Selects the sequence var to start ranking.
  bool FindSequenceVarOnSlack(Solver* const s,
                              SequenceVar** const best_sequence) {
    int64 best_slack = kint64max;
    int64 best_ahmin = kint64max;
    *best_sequence = nullptr;
    best_possible_firsts_.clear();
    for (int i = 0; i < sequences_.size(); ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      int ranked = 0;
      int not_ranked = 0;
      int unperformed = 0;
      candidate_sequence->ComputeStatistics(&ranked, &not_ranked, &unperformed);
      if (not_ranked > 0) {
        candidate_possible_firsts_.clear();
        candidate_possible_lasts_.clear();
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts_, &candidate_possible_lasts_);
        // No possible first, failing.
        if (candidate_possible_firsts_.empty()) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts_.size() == 1 &&
            candidate_sequence->Interval(candidate_possible_firsts_.back())
                ->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          best_possible_firsts_ = candidate_possible_firsts_;
          return true;
        }

        // Evaluating the sequence.
        int64 hmin, hmax, dmin, dmax;
        candidate_sequence->HorizonRange(&hmin, &hmax);
        candidate_sequence->DurationRange(&dmin, &dmax);
        int64 ahmin, ahmax;
        candidate_sequence->ActiveHorizonRange(&ahmin, &ahmax);
        const int64 current_slack = (hmax - hmin - dmax);
        if (current_slack < best_slack ||
            (current_slack == best_slack && ahmin < best_ahmin)) {
          best_slack = current_slack;
          *best_sequence = candidate_sequence;
          best_possible_firsts_ = candidate_possible_firsts_;
          best_ahmin = ahmin;
        }
      }
    }
    return *best_sequence != nullptr;
  }

  bool FindSequenceVarRandomly(Solver* const s,
                               SequenceVar** const best_sequence) {
    std::vector<SequenceVar*> all_candidates;
    std::vector<std::vector<int>> all_possible_firsts;
    for (int i = 0; i < sequences_.size(); ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      int ranked = 0;
      int not_ranked = 0;
      int unperformed = 0;
      candidate_sequence->ComputeStatistics(&ranked, &not_ranked, &unperformed);
      if (not_ranked > 0) {
        candidate_possible_firsts_.clear();
        candidate_possible_lasts_.clear();
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts_, &candidate_possible_lasts_);
        // No possible first, failing.
        if (candidate_possible_firsts_.empty()) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts_.size() == 1 &&
            candidate_sequence->Interval(candidate_possible_firsts_.back())
                ->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          best_possible_firsts_ = candidate_possible_firsts_;
          return true;
        }

        all_candidates.push_back(candidate_sequence);
        all_possible_firsts.push_back(candidate_possible_firsts_);
      }
    }
    if (all_candidates.empty()) {
      return false;
    }
    const int chosen = s->Rand32(all_candidates.size());
    *best_sequence = all_candidates[chosen];
    best_possible_firsts_ = all_possible_firsts[chosen];
    return true;
  }

  bool FindSequenceVar(Solver* const s, SequenceVar** const best_sequence) {
    switch (strategy_) {
      case Solver::SEQUENCE_DEFAULT:
      case Solver::SEQUENCE_SIMPLE:
      case Solver::CHOOSE_MIN_SLACK_RANK_FORWARD:
        return FindSequenceVarOnSlack(s, best_sequence);
      case Solver::CHOOSE_RANDOM_RANK_FORWARD:
        return FindSequenceVarRandomly(s, best_sequence);
      default:
        LOG(FATAL) << "Unknown strategy " << strategy_;
    }
  }

  const std::vector<SequenceVar*> sequences_;
  const Solver::SequenceStrategy strategy_;
  std::vector<int> best_possible_firsts_;
  std::vector<int> candidate_possible_firsts_;
  std::vector<int> candidate_possible_lasts_;
};
}  // namespace

Decision* Solver::MakeScheduleOrPostpone(IntervalVar* const var, int64 est,
                                         int64* const marker) {
  CHECK(var != nullptr);
  CHECK(marker != nullptr);
  return RevAlloc(new ScheduleOrPostpone(var, est, marker));
}

Decision* Solver::MakeScheduleOrExpedite(IntervalVar* const var, int64 est,
                                         int64* const marker) {
  CHECK(var != nullptr);
  CHECK(marker != nullptr);
  return RevAlloc(new ScheduleOrExpedite(var, est, marker));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntervalVar*>& intervals,
                                   IntervalStrategy str) {
  switch (str) {
    case Solver::INTERVAL_DEFAULT:
    case Solver::INTERVAL_SIMPLE:
    case Solver::INTERVAL_SET_TIMES_FORWARD:
      return RevAlloc(new SetTimesForward(intervals));
    case Solver::INTERVAL_SET_TIMES_BACKWARD:
      return RevAlloc(new SetTimesBackward(intervals));
    default:
      LOG(FATAL) << "Unknown strategy " << str;
  }
}

Decision* Solver::MakeRankFirstInterval(SequenceVar* const sequence,
                                        int index) {
  CHECK(sequence != nullptr);
  return RevAlloc(new RankFirst(sequence, index));
}

Decision* Solver::MakeRankLastInterval(SequenceVar* const sequence, int index) {
  CHECK(sequence != nullptr);
  return RevAlloc(new RankLast(sequence, index));
}

DecisionBuilder* Solver::MakePhase(const std::vector<SequenceVar*>& sequences,
                                   SequenceStrategy str) {
  return RevAlloc(new RankFirstIntervalVars(sequences, str));
}

}  // namespace operations_research
