// Copyright 2010 Google
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

#include <sstream>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {

// ----- Decisions and DecisionBuilders on interval vars -----

// TODO(user) : treat optional intervals
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
 private:
  scoped_array<IntervalVar*> vars_;
  const int size_;
  scoped_array<int64> markers_;
};

DecisionBuilder* Solver::MakePhase(const vector<IntervalVar*>& intervals,
                                   IntervalStrategy str) {
  return RevAlloc(new SetTimesForward(intervals.data(), intervals.size()));
}

// ----- Decisions and DecisionBuilders on sequences -----

class TryRankFirst : public Decision {
 public:
  TryRankFirst(Sequence* const seq, int index)
      : sequence_(seq), index_(index) {}
  virtual ~TryRankFirst() {}

  virtual void Apply(Solver* const s) {
    sequence_->RankFirst(index_);
  }

  virtual void Refute(Solver* const s) {
    sequence_->RankNotFirst(index_);
  }

  virtual string DebugString() const {
    return StringPrintf("TryRankFirst(%s, %d)",
                        sequence_->DebugString().c_str(), index_);
  }
 private:
  Sequence* const sequence_;
  const int index_;
};

class RankFirstSequences : public DecisionBuilder {
 public:
  RankFirstSequences(const Sequence* const * sequences, int size)
      : sequences_(new Sequence*[size]), size_(size) {
  memcpy(sequences_.get(), sequences, size_ * sizeof(*sequences));
      }
  virtual ~RankFirstSequences() {}

  virtual Decision* Next(Solver* const s) {
    Sequence* seq = NULL;
    int64 slack = kint64max;
    int64 best_hmin = kint64max;
    for (int i = 0; i < size_; ++i) {
      Sequence* se = sequences_[i];
      if (se->NotRanked() > 0) {
        int64 hmin, hmax, dmin, dmax;
        se->HorizonRange(&hmin, &hmax);
        se->DurationRange(&dmin, &dmax);
        const int64 current_slack = (hmax - hmin - dmax);
        if (current_slack < slack) {
          slack = current_slack;
          seq = se;
          int64 hmin, hmax;
          se->ActiveHorizonRange(&hmin, &hmax);
          best_hmin = hmin;
        } else if (current_slack == slack) {
          int64 hmin, hmax;
          se->ActiveHorizonRange(&hmin, &hmax);
          if (hmin < best_hmin) {
            seq = se;
            best_hmin = hmin;
          }
        }
      }
    }
    if (seq != NULL) {
      seq->ComputePossibleRanks();
      int index = -1;
      int64 smin = kint64max;
      for (int i = 0; i < seq->size(); ++i) {
        if (seq->PossibleFirst(i)) {
          IntervalVar* t = seq->Interval(i);
          if (t->MayBePerformed() && t->StartMin() < smin) {
            index = i;
            smin = t->StartMin();
          }
        }
      }
      if (index == -1) {
        s->Fail();
      }
      CHECK_NE(-1, index);
      return s->RevAlloc(new TryRankFirst(seq, index));
    }
    return NULL;
  }
 private:
  scoped_array<Sequence*> sequences_;
  const int size_;
};

DecisionBuilder* Solver::MakePhase(const vector<Sequence*>& sequences,
                                   SequenceStrategy str) {
  return RevAlloc(new RankFirstSequences(sequences.data(), sequences.size()));
}

}  // namespace operations_research
