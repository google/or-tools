// Copyright 2010-2025 Google LLC
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

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/sequence_var.h"

namespace operations_research {

// TODO(user) : treat optional intervals
// TODO(user) : Call DecisionVisitor and pass name of variable
namespace {
//
// Forward scheduling.
//
class ScheduleOrPostpone : public Decision {
 public:
  ScheduleOrPostpone(IntervalVar* var, int64_t est, int64_t* marker)
      : var_(var), est_(est), marker_(marker) {}
  ~ScheduleOrPostpone() override {}

  void Apply(Solver* s) override {
    var_->SetPerformed(true);
    if (est_.Value() < var_->StartMin()) {
      est_.SetValue(s, var_->StartMin());
    }
    var_->SetStartRange(est_.Value(), est_.Value());
  }

  void Refute(Solver* s) override { s->SaveAndSetValue(marker_, est_.Value()); }

  void Accept(DecisionVisitor* visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitScheduleOrPostpone(var_, est_.Value());
  }

  std::string DebugString() const override {
    return absl::StrFormat("ScheduleOrPostpone(%s at %d)", var_->DebugString(),
                           est_.Value());
  }

 private:
  IntervalVar* const var_;
  NumericalRev<int64_t> est_;
  int64_t* const marker_;
};

class SetTimesForward : public DecisionBuilder {
 public:
  explicit SetTimesForward(const std::vector<IntervalVar*>& vars)
      : vars_(vars),
        markers_(vars.size(), std::numeric_limits<int64_t>::min()) {}

  ~SetTimesForward() override {}

  Decision* Next(Solver* s) override {
    int64_t best_est = std::numeric_limits<int64_t>::max();
    int64_t best_lct = std::numeric_limits<int64_t>::max();
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
      UnperformPostponedTaskBefore(std::numeric_limits<int64_t>::max());
      return nullptr;
    }
    UnperformPostponedTaskBefore(best_est);
    return s->RevAlloc(
        new ScheduleOrPostpone(vars_[support], best_est, &markers_[support]));
  }

  std::string DebugString() const override { return "SetTimesForward()"; }

  void Accept(ModelVisitor* visitor) const override {
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

  void UnperformPostponedTaskBefore(int64_t date) {
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
  std::vector<int64_t> markers_;
};

//
// Backward scheduling.
//
class ScheduleOrExpedite : public Decision {
 public:
  ScheduleOrExpedite(IntervalVar* var, int64_t est, int64_t* marker)
      : var_(var), est_(est), marker_(marker) {}
  ~ScheduleOrExpedite() override {}

  void Apply(Solver* s) override {
    var_->SetPerformed(true);
    if (est_.Value() > var_->EndMax()) {
      est_.SetValue(s, var_->EndMax());
    }
    var_->SetEndRange(est_.Value(), est_.Value());
  }

  void Refute(Solver* s) override {
    s->SaveAndSetValue(marker_, est_.Value() - 1);
  }

  void Accept(DecisionVisitor* visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitScheduleOrExpedite(var_, est_.Value());
  }

  std::string DebugString() const override {
    return absl::StrFormat("ScheduleOrExpedite(%s at %d)", var_->DebugString(),
                           est_.Value());
  }

 private:
  IntervalVar* const var_;
  NumericalRev<int64_t> est_;
  int64_t* const marker_;
};

class SetTimesBackward : public DecisionBuilder {
 public:
  explicit SetTimesBackward(const std::vector<IntervalVar*>& vars)
      : vars_(vars),
        markers_(vars.size(), std::numeric_limits<int64_t>::max()) {}

  ~SetTimesBackward() override {}

  Decision* Next(Solver* s) override {
    int64_t best_end = std::numeric_limits<int64_t>::min();
    int64_t best_start = std::numeric_limits<int64_t>::min();
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  const std::vector<IntervalVar*> vars_;
  std::vector<int64_t> markers_;
};

// ----- Decisions and DecisionBuilders on sequences -----

class RankFirst : public Decision {
 public:
  RankFirst(SequenceVar* seq, int index) : sequence_(seq), index_(index) {}
  ~RankFirst() override {}

  void Apply(Solver*) override { sequence_->RankFirst(index_); }

  void Refute(Solver*) override { sequence_->RankNotFirst(index_); }

  void Accept(DecisionVisitor* visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitRankFirstInterval(sequence_, index_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("RankFirst(%s, %d)", sequence_->DebugString(),
                           index_);
  }

 private:
  SequenceVar* const sequence_;
  const int index_;
};

class RankLast : public Decision {
 public:
  RankLast(SequenceVar* seq, int index) : sequence_(seq), index_(index) {}
  ~RankLast() override {}

  void Apply(Solver*) override { sequence_->RankLast(index_); }

  void Refute(Solver*) override { sequence_->RankNotLast(index_); }

  void Accept(DecisionVisitor* visitor) const override {
    CHECK(visitor != nullptr);
    visitor->VisitRankLastInterval(sequence_, index_);
  }

  std::string DebugString() const override {
    return absl::StrFormat("RankLast(%s, %d)", sequence_->DebugString(),
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

  Decision* Next(Solver* s) override {
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

  void Accept(ModelVisitor* visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitSequenceArrayArgument(ModelVisitor::kSequencesArgument,
                                        sequences_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  // Selects the interval var to rank.
  bool FindIntervalVarOnStartMin(Solver*, SequenceVar* best_sequence,
                                 int* best_interval_index) {
    int best_interval = -1;
    int64_t best_start_min = std::numeric_limits<int64_t>::max();
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

  bool FindIntervalVarRandomly(Solver* s, SequenceVar*,
                               int* best_interval_index) {
    DCHECK(!best_possible_firsts_.empty());
    const int index = s->Rand32(best_possible_firsts_.size());
    *best_interval_index = best_possible_firsts_[index];
    return true;
  }

  bool FindIntervalVar(Solver* s, SequenceVar* best_sequence,
                       int* best_interval_index) {
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
  bool FindSequenceVarOnSlack(Solver* s, SequenceVar** best_sequence) {
    int64_t best_slack = std::numeric_limits<int64_t>::max();
    int64_t best_ahmin = std::numeric_limits<int64_t>::max();
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
        int64_t hmin, hmax, dmin, dmax;
        candidate_sequence->HorizonRange(&hmin, &hmax);
        candidate_sequence->DurationRange(&dmin, &dmax);
        int64_t ahmin, ahmax;
        candidate_sequence->ActiveHorizonRange(&ahmin, &ahmax);
        const int64_t current_slack = (hmax - hmin - dmax);
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

  bool FindSequenceVarRandomly(Solver* s, SequenceVar** best_sequence) {
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
    best_possible_firsts_ = std::move(all_possible_firsts[chosen]);
    return true;
  }

  bool FindSequenceVar(Solver* s, SequenceVar** best_sequence) {
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

Decision* Solver::MakeScheduleOrPostpone(IntervalVar* var, int64_t est,
                                         int64_t* marker) {
  CHECK(var != nullptr);
  CHECK(marker != nullptr);
  return RevAlloc(new ScheduleOrPostpone(var, est, marker));
}

Decision* Solver::MakeScheduleOrExpedite(IntervalVar* var, int64_t est,
                                         int64_t* marker) {
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

Decision* Solver::MakeRankFirstInterval(SequenceVar* sequence, int index) {
  CHECK(sequence != nullptr);
  return RevAlloc(new RankFirst(sequence, index));
}

Decision* Solver::MakeRankLastInterval(SequenceVar* sequence, int index) {
  CHECK(sequence != nullptr);
  return RevAlloc(new RankLast(sequence, index));
}

DecisionBuilder* Solver::MakePhase(const std::vector<SequenceVar*>& sequences,
                                   SequenceStrategy str) {
  return RevAlloc(new RankFirstIntervalVars(sequences, str));
}

}  // namespace operations_research
