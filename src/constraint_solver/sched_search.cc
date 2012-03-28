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
#include "util/string_array.h"

namespace operations_research {

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
    std::vector<int> best_possible_firsts;
    while (true) {
      if (FindSequenceVar(s, &best_sequence, &best_possible_firsts)) {
        // No not create a choice point if it is not needed.
        DCHECK(best_sequence != NULL);
        if (best_possible_firsts.size() == 1 &&
            best_sequence->Interval(
                best_possible_firsts.back())->MustBePerformed()) {
          best_sequence->RankFirst(best_possible_firsts.back());
          continue;
        }
        int best_interval = -1;
        if (!FindIntervalVar(s,
                             best_sequence,
                             best_possible_firsts,
                             &best_interval)) {
          s->Fail();
        }
        CHECK_NE(-1, best_interval);
        return s->RevAlloc(new RankFirst(best_sequence, best_interval));
    } else {
      for (int i = 0; i < size_; ++i) {
        CHECK_EQ(0, sequences_[i]->NotRanked()) << sequences_[i]->DebugString();
      }
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
                                 const std::vector<int>& best_possible_firsts,
                                 int* const best_interval_index) {
    int best_interval = -1;
    int64 best_start_min = kint64max;
    for (int index = 0; index < best_possible_firsts.size(); ++index) {
      const int candidate = best_possible_firsts[index];
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
                               const std::vector<int>& best_possible_firsts,
                               int* const best_interval_index) {
    DCHECK(!best_possible_firsts.empty());
    const int index = s->Rand32(best_possible_firsts.size());
    *best_interval_index = best_possible_firsts[index];
    return true;
  }

  bool FindIntervalVar(Solver* const s,
                       SequenceVar* const best_sequence,
                       const std::vector<int>& best_possible_firsts,
                       int* const best_interval_index) {
    switch (strategy_) {
      case Solver::SEQUENCE_DEFAULT:
      case Solver::SEQUENCE_SIMPLE:
      case Solver::CHOOSE_MIN_SLACK_RANK_FORWARD:
        return FindIntervalVarOnStartMin(s,
                                         best_sequence,
                                         best_possible_firsts,
                                         best_interval_index);
      case Solver::CHOOSE_RANDOM_RANK_FORWARD:
        return FindIntervalVarRandomly(s,
                                       best_sequence,
                                       best_possible_firsts,
                                       best_interval_index);
      default:
        LOG(FATAL) << "Unknown strategy " << strategy_;
    }
  }

  // Selects the sequence var to start ranking.
  bool FindSequenceVarOnSlack(Solver* const s,
                              SequenceVar** const best_sequence,
                              std::vector<int>* const best_possible_firsts) const {
    int64 best_slack = kint64max;
    int64 best_ahmin = kint64max;
    *best_sequence = NULL;
    best_possible_firsts->clear();
    for (int i = 0; i < size_; ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      if (candidate_sequence->NotRanked() > 0) {
        std::vector<int> candidate_possible_firsts;
        std::vector<int> candidate_possible_Lasts;
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts,
            &candidate_possible_Lasts);
        // No possible first, failing.
        if (candidate_possible_firsts.size() == 0) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts.size() == 1 &&
            candidate_sequence->Interval(
                candidate_possible_firsts.back())->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          *best_possible_firsts = candidate_possible_firsts;
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
          *best_possible_firsts = candidate_possible_firsts;
          best_ahmin = ahmin;
        }
      }
    }
    return *best_sequence != NULL;
  }

  bool FindSequenceVarRandomly(Solver* const s,
                               SequenceVar** const best_sequence,
                               std::vector<int>* const best_possible_firsts) const {
    std::vector<int> all_candidates;
    std::vector<std::vector<int> > all_possible_firsts;
    for (int i = 0; i < size_; ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      if (candidate_sequence->NotRanked() > 0) {
        std::vector<int> candidate_possible_firsts;
        std::vector<int> candidate_possible_Lasts;
        candidate_sequence->ComputePossibleFirstsAndLasts(
            &candidate_possible_firsts,
            &candidate_possible_Lasts);
        // No possible first, failing.
        if (candidate_possible_firsts.size() == 0) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts.size() == 1 &&
            candidate_sequence->Interval(
                candidate_possible_firsts.back())->MustBePerformed()) {
          *best_sequence = candidate_sequence;
          *best_possible_firsts = candidate_possible_firsts;
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
    *best_possible_firsts = all_possible_firsts[chosen];
    return true;
  }

  bool FindSequenceVar(Solver* const s,
                       SequenceVar** const best_sequence,
                       std::vector<int>* const best_possible_firsts) const {
    switch (strategy_) {
      case Solver::SEQUENCE_DEFAULT:
      case Solver::SEQUENCE_SIMPLE:
      case Solver::CHOOSE_MIN_SLACK_RANK_FORWARD:
        return FindSequenceVarOnSlack(s,
                                      best_sequence,
                                      best_possible_firsts);
      case Solver::CHOOSE_RANDOM_RANK_FORWARD:
        return FindSequenceVarRandomly(s,
                                       best_sequence,
                                       best_possible_firsts);
      default:
        LOG(FATAL) << "Unknown strategy " << strategy_;
    }
  }

  scoped_array<SequenceVar*> sequences_;
  const int size_;
  const Solver::SequenceStrategy strategy_;
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
