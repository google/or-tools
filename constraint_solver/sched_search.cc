// Copyright 2010-2011 Google
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
}  // namespace

Decision* Solver::MakeScheduleOrPostpone(IntervalVar* const var,
                                         int64 est,
                                         int64* const marker) {
  CHECK_NOTNULL(var);
  CHECK_NOTNULL(marker);
  return RevAlloc(new ScheduleOrPostpone(var, est, marker));
}

namespace {
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
}  // namespace

DecisionBuilder* Solver::MakePhase(const std::vector<IntervalVar*>& intervals,
                                   IntervalStrategy str) {
  return RevAlloc(new SetTimesForward(intervals.data(), intervals.size()));
}

// ----- Decisions and DecisionBuilders on sequences -----

namespace {
class TryRankFirst : public Decision {
 public:
  TryRankFirst(SequenceVar* const seq, int index)
      : sequence_(seq), index_(index) {}
  virtual ~TryRankFirst() {}

  virtual void Apply(Solver* const s) {
    sequence_->RankFirst(index_);
  }

  virtual void Refute(Solver* const s) {
    sequence_->RankNotFirst(index_);
  }

  void Accept(DecisionVisitor* const visitor) const {
    CHECK_NOTNULL(visitor);
    visitor->VisitTryRankFirst(sequence_, index_);
  }

  virtual string DebugString() const {
    return StringPrintf("TryRankFirst(%s, %d)",
                        sequence_->DebugString().c_str(), index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};
}  // namespace

Decision* Solver::MakeTryRankFirst(SequenceVar* const sequence, int index) {
  CHECK_NOTNULL(sequence);
  return RevAlloc(new TryRankFirst(sequence, index));
}

namespace {
class RankFirstSequenceVars : public DecisionBuilder {
 public:
  RankFirstSequenceVars(const SequenceVar* const * sequences, int size)
      : sequences_(new SequenceVar*[size]), size_(size) {
    memcpy(sequences_.get(), sequences, size_ * sizeof(*sequences));
  }

  virtual ~RankFirstSequenceVars() {}

  virtual Decision* Next(Solver* const s) {
    SequenceVar* best_sequence = NULL;
    int64 best_slack = kint64max;
    int64 best_ahmin = kint64max;
    std::vector<int> best_possible_firsts;
    for (int i = 0; i < size_; ++i) {
      SequenceVar* const candidate_sequence = sequences_[i];
      if (candidate_sequence->NotRanked() > 0) {
        std::vector<int> candidate_possible_firsts;
        candidate_sequence->ComputePossibleFirsts(&candidate_possible_firsts);
        // No possible first, failing.
        if (candidate_possible_firsts.size() == 0) {
          s->Fail();
        }
        // Only 1 candidate, and non optional: ranking without branching.
        if (candidate_possible_firsts.size() == 1 &&
            candidate_sequence->Interval(
                candidate_possible_firsts.back())->MustBePerformed()) {
          candidate_sequence->RankFirst(candidate_possible_firsts[0]);
          continue;
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
          best_sequence = candidate_sequence;
          best_possible_firsts = candidate_possible_firsts;
          best_ahmin = ahmin;
        }
      }
    }
    if (best_sequence != NULL) {
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
        s->Fail();
      }
      CHECK_NE(-1, best_interval);
      return s->RevAlloc(new TryRankFirst(best_sequence, best_interval));
    }
    return NULL;
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitSequenceArrayArgument(ModelVisitor::kSequencesArgument,
                                        sequences_.get(),
                                        size_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  scoped_array<SequenceVar*> sequences_;
  const int size_;
};
}  // namespace

DecisionBuilder* Solver::MakePhase(const std::vector<SequenceVar*>& sequences,
                                   SequenceStrategy str) {
  return RevAlloc(new RankFirstSequenceVars(sequences.data(),
                                            sequences.size()));
}

}  // namespace operations_research
