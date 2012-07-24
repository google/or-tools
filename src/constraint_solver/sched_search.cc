// Copyright 2010-2012 Google
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

#include <string.h>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

namespace operations_research {

// ----- SequenceVar -----

// TODO(user): Add better class invariants, in particular checks
// that ranked_first, ranked_last, and unperformed are truly disjoint.

SequenceVar::SequenceVar(Solver* const s,
                         const IntervalVar* const * intervals,
                         const IntVar* const * nexts,
                         int size,
                         const string& name)
  : PropagationBaseObject(s),
    intervals_(new IntervalVar*[size]),
    nexts_(new IntVar*[size + 1]),
    size_(size),
    next_size_(size + 1),
    previous_(size + 2, -1) {
  memcpy(intervals_.get(), intervals, size_ * sizeof(*intervals));
  memcpy(nexts_.get(), nexts, next_size_ * sizeof(*nexts));
  set_name(name);
}

SequenceVar::~SequenceVar() {}

IntervalVar* SequenceVar::Interval(int index) const {
  return intervals_[index];
}

IntVar* SequenceVar::Next(int index) const {
  return nexts_[index];
}

string SequenceVar::DebugString() const {
  int64 hmin, hmax, dmin, dmax;
  HorizonRange(&hmin, &hmax);
  DurationRange(&dmin, &dmax);
  int unperformed = 0;
  int ranked = 0;
  int not_ranked = 0;
  ComputeStatistics(&ranked, &not_ranked, &unperformed);
  return StringPrintf("%s(horizon = %" GG_LL_FORMAT
                      "d..%" GG_LL_FORMAT
                      "d, duration = %" GG_LL_FORMAT
                      "d..%" GG_LL_FORMAT
                      "d, not ranked = %d, ranked = %d)",
                      name().c_str(),
                      hmin,
                      hmax,
                      dmin,
                      dmax,
                      not_ranked,
                      ranked);
}

void SequenceVar::Accept(ModelVisitor* const visitor) const {
  visitor->VisitSequenceVariable(this);
}

void SequenceVar::DurationRange(int64* const dmin, int64* const dmax) const {
  int64 dur_min = 0;
  int64 dur_max = 0;
  for (int i = 0; i < size_; ++i) {
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
  for (int i = 0; i < size_; ++i) {
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
  hash_set<int> decided;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      decided.insert(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < next_size_) {
      decided.insert(first - 1);
    } else {
      break;
    }
  }
  if (first != next_size_) {
    UpdatePrevious();
    int last = next_size_;
    while (previous_[last] != -1) {
      last = previous_[last];
      decided.insert(last - 1);
    }
  }
  int64 hor_min = kint64max;
  int64 hor_max = kint64min;
  for (int i = 0; i < size_; ++i) {
    if (!ContainsKey(decided, i)) {
      IntervalVar* const t = intervals_[i];
      hor_min = std::min(hor_min, t->StartMin());
      hor_max = std::max(hor_max, t->EndMax());
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void SequenceVar::ComputeStatistics(int* const ranked,
                                    int* const not_ranked,
                                    int* const unperformed) const {
  *unperformed = 0;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      (*unperformed)++;
    }
  }
  *ranked = 0;
  int first = 0;
  while (first < next_size_ && nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    (*ranked)++;
  }
  if (first != next_size_) {
    UpdatePrevious();
    int last = next_size_;
    while (previous_[last] != -1) {
      last = previous_[last];
      (*ranked)++;
    }
  }
  *not_ranked = size_ - *ranked - *unperformed;
}

int SequenceVar::ComputeForwardFrontier() {
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first == next_size_) {
      return first;
    }
  }
  return first;
}

int SequenceVar::ComputeBackwardFrontier() {
  UpdatePrevious();
  int last = next_size_;
  while (previous_[last] != -1) {
    last = previous_[last];
  }
  return last;
}

void SequenceVar::ComputePossibleFirstsAndLasts(
    std::vector<int>* const possible_firsts,
    std::vector<int>* const possible_lasts) {
  IntVar* const forward_var = nexts_[ComputeForwardFrontier()];
  std::vector<int> candidates;
  int64 smallest_start_max = kint64max;
  int ssm_support = -1;
  for (int64 i = forward_var->Min(); i <= forward_var->Max(); ++i) {
    // TODO(lperron): use domain iterator.
    if (i != 0 && i < size_ + 1 && forward_var->Contains(i)) {
      const int candidate = i - 1;
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

  // TODO(lperron) : backward
}

void SequenceVar::RankSequence(const std::vector<int>& rank_first,
                               const std::vector<int>& rank_last,
                               const std::vector<int>& unperformed) {
  solver()->GetPropagationMonitor()->RankSequence(this,
                                                  rank_first,
                                                  rank_last,
                                                  unperformed);
  // Mark unperformed.
  for (ConstIter<std::vector<int> > it(unperformed); !it.at_end(); ++it) {
    intervals_[*it]->SetPerformed(false);
  }
  // Forward.
  int forward = 0;
  for (int i = 0; i < rank_first.size(); ++i) {
    const int next = 1 + rank_first[i];
    nexts_[forward]->SetValue(next);
    forward = next;
  }
  // Backward.
  int backward = size_ + 1;
  for (int i = 0; i < rank_last.size(); ++i) {
    const int next = 1 + rank_last[i];
    nexts_[next]->SetValue(backward);
    backward = next;
  }
}

void SequenceVar::RankFirst(int index) {
  solver()->GetPropagationMonitor()->RankFirst(this, index);
  intervals_[index]->SetPerformed(true);
  const int forward_frontier = ComputeForwardFrontier();
  nexts_[forward_frontier]->SetValue(index + 1);
}

void SequenceVar::RankNotFirst(int index) {
  solver()->GetPropagationMonitor()->RankNotFirst(this, index);
  const int forward_frontier = ComputeForwardFrontier();
  if (forward_frontier < next_size_) {
    nexts_[forward_frontier]->RemoveValue(index + 1);
  } else {
    solver()->Fail();
  }
}

void SequenceVar::RankLast(int index) {
  solver()->GetPropagationMonitor()->RankLast(this, index);
  intervals_[index]->SetPerformed(true);
  const int backward_frontier = ComputeBackwardFrontier();
  nexts_[index + 1]->SetValue(backward_frontier);
}

void SequenceVar::RankNotLast(int index) {
  solver()->GetPropagationMonitor()->RankNotLast(this, index);
  const int backward_frontier = ComputeBackwardFrontier();
  nexts_[index + 1]->RemoveValue(backward_frontier);
}

void SequenceVar::UpdatePrevious() const {
  for (int i = 0; i < size_ + 2; ++i) {
    previous_[i] = -1;
  }
  for (int i = 0; i < next_size_; ++i) {
    if (nexts_[i]->Bound()) {
      previous_[nexts_[i]->Min()] = i;
    }
  }
}

void SequenceVar::FillSequence(std::vector<int>* const rank_first,
                               std::vector<int>* const rank_last,
                               std::vector<int>* const unperformed) const {
  CHECK_NOTNULL(rank_first);
  CHECK_NOTNULL(rank_last);
  CHECK_NOTNULL(unperformed);
  rank_first->clear();
  rank_last->clear();
  unperformed->clear();
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->CannotBePerformed()) {
      unperformed->push_back(i);
    }
  }
  int first = 0;
  while (nexts_[first]->Bound()) {
    first = nexts_[first]->Min();
    if (first < next_size_) {
      rank_first->push_back(first - 1);
    } else {
      break;
    }
  }
  if (first != next_size_) {
    UpdatePrevious();
    int last = next_size_;
    while (previous_[last] != -1) {
      last = previous_[last];
      rank_last->push_back(last - 1);
    }
  }
}

// ----- Decisions and DecisionBuilders on interval vars -----

// TODO(user) : treat optional intervals
// TODO(user) : Call DecisionVisitor and pass name of variable
namespace {
class ScheduleOrPostpone : public Decision {
 public:
  ScheduleOrPostpone(IntervalVar* const var, int64 est, int64* const marker)
      : var_(var), est_(est), marker_(marker) {}
  virtual ~ScheduleOrPostpone() {}

  virtual void Apply(Solver* const s) {
    var_->SetPerformed(true);
    var_->SetStartRange(est_, est_);
  }

  virtual void Refute(Solver* const s) {
    s->SaveAndSetValue(marker_, est_ + 1);
  }

  virtual void Accept(DecisionVisitor* const visitor) const {
    CHECK_NOTNULL(visitor);
    visitor->VisitScheduleOrPostpone(var_, est_);
  }

  virtual string DebugString() const {
    return StringPrintf("ScheduleOrPostpone(%s at %" GG_LL_FORMAT "d)",
                        var_->DebugString().c_str(), est_);
  }

 private:
  IntervalVar* const var_;
  const int64 est_;
  int64* const marker_;
};

class SetTimesForward : public DecisionBuilder {
 public:
  SetTimesForward(const IntervalVar* const * vars, int size)
    : vars_(new IntervalVar*[size]),
      size_(size),
      markers_(new int64[size]) {
    memcpy(vars_.get(), vars, sizeof(*vars) * size);
    for (int i = 0; i < size_; ++i) {
      markers_[i] = kint64min;
    }
  }

  virtual ~SetTimesForward() {}

  virtual Decision* Next(Solver* const s) {
    int64 best_est = kint64max;
    int64 best_lct = kint64max;
    int support = -1;
    int refuted = 0;
    for (int i = 0; i < size_; ++i) {
      IntervalVar* const v = vars_[i];
      if (v->MayBePerformed() && v->StartMax() > v->StartMin()) {
        if (v->StartMin() >= markers_[i] &&
                  (v->StartMin() < best_est ||
                   (v->StartMin() == best_est && v->EndMax() < best_lct))) {
          best_est = v->StartMin();
          best_lct = v->EndMax();
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
        return NULL;
      } else {
        s->Fail();
      }
    }
    return s->RevAlloc(new ScheduleOrPostpone(vars_[support],
                                              vars_[support]->StartMin(),
                                              &markers_[support]));
  }

  virtual string DebugString() const {
    return "SetTimesForward()";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        vars_.get(),
                                        size_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  scoped_array<IntervalVar*> vars_;
  const int size_;
  scoped_array<int64> markers_;
};

// ----- Decisions and DecisionBuilders on sequences -----

class RankFirst : public Decision {
 public:
  RankFirst(SequenceVar* const seq, int index)
      : sequence_(seq), index_(index) {}
  virtual ~RankFirst() {}

  virtual void Apply(Solver* const s) {
    sequence_->RankFirst(index_);
  }

  virtual void Refute(Solver* const s) {
    sequence_->RankNotFirst(index_);
  }

  void Accept(DecisionVisitor* const visitor) const {
    CHECK_NOTNULL(visitor);
    visitor->VisitRankFirstInterval(sequence_, index_);
  }

  virtual string DebugString() const {
    return StringPrintf("RankFirst(%s, %d)",
                        sequence_->DebugString().c_str(), index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};

class RankLast : public Decision {
 public:
  RankLast(SequenceVar* const seq, int index)
      : sequence_(seq), index_(index) {}
  virtual ~RankLast() {}

  virtual void Apply(Solver* const s) {
    sequence_->RankLast(index_);
  }

  virtual void Refute(Solver* const s) {
    sequence_->RankNotLast(index_);
  }

  void Accept(DecisionVisitor* const visitor) const {
    CHECK_NOTNULL(visitor);
    visitor->VisitRankLastInterval(sequence_, index_);
  }

  virtual string DebugString() const {
    return StringPrintf("RankLast(%s, %d)",
                        sequence_->DebugString().c_str(), index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};

class RankFirstIntervalVars : public DecisionBuilder {
 public:
  RankFirstIntervalVars(const SequenceVar* const * sequences,
                        int size,
                        Solver::SequenceStrategy str)
      : sequences_(new SequenceVar*[size]), size_(size), strategy_(str) {
    memcpy(sequences_.get(), sequences, size_ * sizeof(*sequences));
  }

  virtual ~RankFirstIntervalVars() {}

  virtual Decision* Next(Solver* const s) {
    SequenceVar* best_sequence = NULL;
    best_possible_firsts_.clear();
    while (true) {
      if (FindSequenceVar(s, &best_sequence)) {
        // No not create a choice point if it is not needed.
        DCHECK(best_sequence != NULL);
        if (best_possible_firsts_.size() == 1 &&
            best_sequence->Interval(
                best_possible_firsts_.back())->MustBePerformed()) {
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
        return NULL;
      }
    }
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitSequenceArrayArgument(ModelVisitor::kSequencesArgument,
                                        sequences_.get(),
                                        size_);
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
      //      LOG(INFO) << "  - " << candidate << " -> " << interval->DebugString();
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

  bool FindIntervalVar(Solver* const s,
                       SequenceVar* const best_sequence,
                       int* const best_interval_index) {
    switch (strategy_) {
      case Solver::SEQUENCE_DEFAULT:
      case Solver::SEQUENCE_SIMPLE:
      case Solver::CHOOSE_MIN_SLACK_RANK_FORWARD:
        return FindIntervalVarOnStartMin(s,
                                         best_sequence,
                                         best_interval_index);
      case Solver::CHOOSE_RANDOM_RANK_FORWARD:
        return FindIntervalVarRandomly(s,
                                       best_sequence,
                                       best_interval_index);
      default:
        LOG(FATAL) << "Unknown strategy " << strategy_;
    }
  }

  // Selects the sequence var to start ranking.
  bool FindSequenceVarOnSlack(Solver* const s,
                              SequenceVar** const best_sequence) const {
    int64 best_slack = kint64max;
    int64 best_ahmin = kint64max;
    *best_sequence = NULL;
    best_possible_firsts_.clear();
    for (int i = 0; i < size_; ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      int ranked = 0;
      int not_ranked = 0;
      int unperformed = 0;
      candidate_sequence->ComputeStatistics(&ranked, &not_ranked, &unperformed);
      if (not_ranked > 0) {
        std::vector<int> candidate_possible_firsts;
        std::vector<int> candidate_possible_lasts;
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts,
            &candidate_possible_lasts);
        // No possible first, failing.
        if (candidate_possible_firsts.size() == 0) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts.size() == 1 &&
            candidate_sequence->Interval(
                candidate_possible_firsts.back())->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          best_possible_firsts_ = candidate_possible_firsts;
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
          best_possible_firsts_ = candidate_possible_firsts;
          best_ahmin = ahmin;
        }
      }
    }
    return *best_sequence != NULL;
  }

  bool FindSequenceVarRandomly(Solver* const s,
                               SequenceVar** const best_sequence) const {
    std::vector<int> all_candidates;
    std::vector<std::vector<int> > all_possible_firsts;
    for (int i = 0; i < size_; ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      int ranked = 0;
      int not_ranked = 0;
      int unperformed = 0;
      candidate_sequence->ComputeStatistics(&ranked, &not_ranked, &unperformed);
      if (not_ranked > 0) {
        std::vector<int> candidate_possible_firsts;
        std::vector<int> candidate_possible_lasts;
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts,
            &candidate_possible_lasts);
        // No possible first, failing.
        if (candidate_possible_firsts.size() == 0) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts.size() == 1 &&
            candidate_sequence->Interval(
                candidate_possible_firsts.back())->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          best_possible_firsts_ = candidate_possible_firsts;
          return true;
        }

        all_candidates.push_back(i);
        all_possible_firsts.push_back(candidate_possible_firsts);
      }
    }
    if (all_candidates.empty()) {
      return false;
    }
    const int chosen = s->Rand32(all_candidates.size());
    *best_sequence = sequences_[all_candidates[chosen]];
    best_possible_firsts_ = all_possible_firsts[chosen];
    return true;
  }

  bool FindSequenceVar(Solver* const s,
                       SequenceVar** const best_sequence) const {
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

  scoped_array<SequenceVar*> sequences_;
  const int size_;
  const Solver::SequenceStrategy strategy_;
  mutable std::vector<int> best_possible_firsts_;
};
}  // namespace

Decision* Solver::MakeScheduleOrPostpone(IntervalVar* const var,
                                         int64 est,
                                         int64* const marker) {
  CHECK_NOTNULL(var);
  CHECK_NOTNULL(marker);
  return RevAlloc(new ScheduleOrPostpone(var, est, marker));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntervalVar*>& intervals,
                                   IntervalStrategy str) {
  return RevAlloc(new SetTimesForward(intervals.data(), intervals.size()));
}

Decision* Solver::MakeRankFirstInterval(SequenceVar* const sequence,
                                        int index) {
  CHECK_NOTNULL(sequence);
  return RevAlloc(new RankFirst(sequence, index));
}

Decision* Solver::MakeRankLastInterval(SequenceVar* const sequence, int index) {
  CHECK_NOTNULL(sequence);
  return RevAlloc(new RankLast(sequence, index));
}

DecisionBuilder* Solver::MakePhase(const std::vector<SequenceVar*>& sequences,
                                   SequenceStrategy str) {
  return RevAlloc(new RankFirstIntervalVars(sequences.data(),
                                            sequences.size(),
                                            str));
}

}  // namespace operations_research
