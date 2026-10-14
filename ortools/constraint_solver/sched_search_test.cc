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

#include <stddef.h>

#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/sequence_var.h"

namespace operations_research {
namespace {
class MyDecisionVisitor : public DecisionVisitor {
 public:
  MyDecisionVisitor()
      : interval_var_(nullptr), est_(-1), sequence_(nullptr), index_(-1) {}
  ~MyDecisionVisitor() override {}

  IntervalVar* interval_var() const { return interval_var_; }
  int64_t est() const { return est_; }
  SequenceVar* sequence() const { return sequence_; }
  int index() const { return index_; }

  void VisitScheduleOrPostpone(IntervalVar* const var, int64_t est) override {
    interval_var_ = var;
    est_ = est;
  }

  void VisitScheduleOrExpedite(IntervalVar* const var, int64_t est) override {
    interval_var_ = var;
    est_ = est;
  }

  void VisitRankFirstInterval(SequenceVar* const sequence, int index) override {
    sequence_ = sequence;
    index_ = index;
  }

  void VisitRankLastInterval(SequenceVar* const sequence, int index) override {
    sequence_ = sequence;
    index_ = index;
  }

 private:
  IntervalVar* interval_var_;
  int64_t est_;
  SequenceVar* sequence_;
  int index_;
};

TEST(SchedSearchTest, VisitScheduleOrPostpone) {
  Solver solver("VisitScheduleOrPostpone");
  const int64_t kScheduleTime = 12;
  int64_t marker;
  MyDecisionVisitor visitor;
  IntervalVar* const var =
      solver.MakeFixedDurationIntervalVar(0, 100, 3, false, "var");
  Decision* const decision =
      solver.MakeScheduleOrPostpone(var, kScheduleTime, &marker);
  EXPECT_NE(var, visitor.interval_var());
  EXPECT_NE(kScheduleTime, visitor.est());
  decision->Accept(&visitor);
  EXPECT_EQ(var, visitor.interval_var());
  EXPECT_EQ(kScheduleTime, visitor.est());
}

TEST(SchedSearchTest, VisitScheduleOrExpedite) {
  Solver solver("VisitScheduleOrExpedite");
  const int64_t kScheduleEndTime = 12;
  int64_t marker;
  MyDecisionVisitor visitor;
  IntervalVar* const var =
      solver.MakeFixedDurationIntervalVar(0, 100, 3, false, "var");
  Decision* const decision =
      solver.MakeScheduleOrExpedite(var, kScheduleEndTime, &marker);
  EXPECT_NE(var, visitor.interval_var());
  EXPECT_NE(kScheduleEndTime, visitor.est());
  decision->Accept(&visitor);
  EXPECT_EQ(var, visitor.interval_var());
  EXPECT_EQ(kScheduleEndTime, visitor.est());
}

TEST(SchedSearchTest, VisitRankFirstInterval) {
  Solver solver("VisitRankFirstInterval");
  const int64_t kIntervalIndex = 5;
  MyDecisionVisitor visitor;
  std::vector<IntervalVar*> vars;
  solver.MakeFixedDurationIntervalVarArray(10, 0, 100, 3, false, "var_", &vars);
  SequenceVar* const sequence =
      solver.MakeDisjunctiveConstraint(vars, "sequence")->MakeSequenceVar();
  Decision* const decision =
      solver.MakeRankFirstInterval(sequence, kIntervalIndex);
  EXPECT_NE(sequence, visitor.sequence());
  EXPECT_NE(kIntervalIndex, visitor.index());
  decision->Accept(&visitor);
  EXPECT_EQ(sequence, visitor.sequence());
  EXPECT_EQ(kIntervalIndex, visitor.index());
}

TEST(SchedSearchTest, VisitRankLastInterval) {
  Solver solver("VisitRankLastInterval");
  const int64_t kIntervalIndex = 5;
  MyDecisionVisitor visitor;
  std::vector<IntervalVar*> vars;
  solver.MakeFixedDurationIntervalVarArray(10, 0, 100, 3, false, "var_", &vars);
  SequenceVar* const sequence =
      solver.MakeDisjunctiveConstraint(vars, "sequence")->MakeSequenceVar();
  Decision* const decision =
      solver.MakeRankLastInterval(sequence, kIntervalIndex);
  EXPECT_NE(sequence, visitor.sequence());
  EXPECT_NE(kIntervalIndex, visitor.index());
  decision->Accept(&visitor);
  EXPECT_EQ(sequence, visitor.sequence());
  EXPECT_EQ(kIntervalIndex, visitor.index());
}

TEST(SchedSearchTest, Rank3Intervals) {
  Solver solver("Rank3Intervals");
  std::vector<IntervalVar*> vars;
  solver.MakeFixedDurationIntervalVarArray(3, 0, 25, 5, false, "var_", &vars);
  Constraint* const disj = solver.MakeDisjunctiveConstraint(vars, "sequence");
  solver.AddConstraint(disj);
  DecisionBuilder* const interval_decision =
      solver.MakePhase(vars, Solver::INTERVAL_DEFAULT);
  solver.NewSearch(interval_decision);
  int count = 0;
  while (solver.NextSolution()) {
    LOG(INFO) << "----------------------------";
    LOG(INFO) << "Solution " << count++;
    for (IntervalVar* const var : vars) {
      LOG(INFO) << var->DebugString();
    }
  }
  solver.EndSearch();
  EXPECT_EQ(6, count);
}

TEST(SchedSearchTest, Rank3SequencedIntervals) {
  Solver solver("Rank3SequencedIntervals");
  std::vector<IntervalVar*> vars;
  solver.MakeFixedDurationIntervalVarArray(3, 0, 25, 5, false, "var_", &vars);
  DisjunctiveConstraint* const disj =
      solver.MakeDisjunctiveConstraint(vars, "sequence");
  solver.AddConstraint(disj);
  SequenceVar* const sequence = disj->MakeSequenceVar();
  sequence->RankFirst(1);
  DecisionBuilder* const interval_decision =
      solver.MakePhase(vars, Solver::INTERVAL_DEFAULT);
  DecisionBuilder* const db = interval_decision;
  solver.NewSearch(db);
  int count = 0;
  while (solver.NextSolution()) {
    LOG(INFO) << "----------------------------";
    LOG(INFO) << "Solution " << count++;
    for (IntervalVar* const var : vars) {
      LOG(INFO) << var->DebugString();
    }
  }
  solver.EndSearch();
  EXPECT_EQ(2, count);
}
}  // namespace
}  // namespace operations_research
