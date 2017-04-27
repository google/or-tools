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


#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <stack>
#include <string>
#include <utility>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

DEFINE_bool(
    cp_full_trace, false,
    "Display all trace information, even if the modifiers has no effect");

namespace operations_research {
namespace {
// ---------- Code Instrumentation ----------
class TraceIntVar : public IntVar {
 public:
  TraceIntVar(Solver* const solver, IntVar* const inner)
      : IntVar(solver), inner_(inner) {
    if (inner->HasName()) {
      set_name(inner->name());
    }
    CHECK_NE(inner->VarType(), TRACE_VAR);
  }

  ~TraceIntVar() override {}

  int64 Min() const override { return inner_->Min(); }

  void SetMin(int64 m) override {
    if (m > inner_->Min()) {
      solver()->GetPropagationMonitor()->SetMin(inner_, m);
      inner_->SetMin(m);
    }
  }

  int64 Max() const override { return inner_->Max(); }

  void SetMax(int64 m) override {
    if (m < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetMax(inner_, m);
      inner_->SetMax(m);
    }
  }

  void Range(int64* l, int64* u) override { inner_->Range(l, u); }

  void SetRange(int64 l, int64 u) override {
    if (l > inner_->Min() || u < inner_->Max()) {
      if (l == u) {
        solver()->GetPropagationMonitor()->SetValue(inner_, l);
        inner_->SetValue(l);
      } else {
        solver()->GetPropagationMonitor()->SetRange(inner_, l, u);
        inner_->SetRange(l, u);
      }
    }
  }

  bool Bound() const override { return inner_->Bound(); }

  bool IsVar() const override { return true; }

  IntVar* Var() override { return this; }

  int64 Value() const override { return inner_->Value(); }

  void RemoveValue(int64 v) override {
    if (inner_->Contains(v)) {
      solver()->GetPropagationMonitor()->RemoveValue(inner_, v);
      inner_->RemoveValue(v);
    }
  }

  void SetValue(int64 v) override {
    solver()->GetPropagationMonitor()->SetValue(inner_, v);
    inner_->SetValue(v);
  }

  void RemoveInterval(int64 l, int64 u) override {
    solver()->GetPropagationMonitor()->RemoveInterval(inner_, l, u);
    inner_->RemoveInterval(l, u);
  }

  void RemoveValues(const std::vector<int64>& values) override {
    solver()->GetPropagationMonitor()->RemoveValues(inner_, values);
    inner_->RemoveValues(values);
  }

  void SetValues(const std::vector<int64>& values) override {
    solver()->GetPropagationMonitor()->SetValues(inner_, values);
    inner_->SetValues(values);
  }

  void WhenRange(Demon* d) override { inner_->WhenRange(d); }

  void WhenBound(Demon* d) override { inner_->WhenBound(d); }

  void WhenDomain(Demon* d) override { inner_->WhenDomain(d); }

  uint64 Size() const override { return inner_->Size(); }

  bool Contains(int64 v) const override { return inner_->Contains(v); }

  IntVarIterator* MakeHoleIterator(bool reversible) const override {
    return inner_->MakeHoleIterator(reversible);
  }

  IntVarIterator* MakeDomainIterator(bool reversible) const override {
    return inner_->MakeDomainIterator(reversible);
  }

  int64 OldMin() const override { return inner_->OldMin(); }

  int64 OldMax() const override { return inner_->OldMax(); }

  int VarType() const override { return TRACE_VAR; }

  void Accept(ModelVisitor* const visitor) const override {
    IntExpr* const cast_expr =
        solver()->CastExpression(const_cast<TraceIntVar*>(this));
    if (cast_expr != nullptr) {
      visitor->VisitIntegerVariable(this, cast_expr);
    } else {
      visitor->VisitIntegerVariable(this, ModelVisitor::kTraceOperation, 0,
                                    inner_);
    }
  }

  std::string DebugString() const override { return inner_->DebugString(); }

  IntVar* IsEqual(int64 constant) override { return inner_->IsEqual(constant); }

  IntVar* IsDifferent(int64 constant) override {
    return inner_->IsDifferent(constant);
  }

  IntVar* IsGreaterOrEqual(int64 constant) override {
    return inner_->IsGreaterOrEqual(constant);
  }

  IntVar* IsLessOrEqual(int64 constant) override {
    return inner_->IsLessOrEqual(constant);
  }

 private:
  IntVar* const inner_;
};

class TraceIntExpr : public IntExpr {
 public:
  TraceIntExpr(Solver* const solver, IntExpr* const inner)
      : IntExpr(solver), inner_(inner) {
    CHECK(!inner->IsVar());
    if (inner->HasName()) {
      set_name(inner->name());
    }
  }

  ~TraceIntExpr() override {}

  int64 Min() const override { return inner_->Min(); }

  void SetMin(int64 m) override {
    solver()->GetPropagationMonitor()->SetMin(inner_, m);
    inner_->SetMin(m);
  }

  int64 Max() const override { return inner_->Max(); }

  void SetMax(int64 m) override {
    solver()->GetPropagationMonitor()->SetMax(inner_, m);
    inner_->SetMax(m);
  }

  void Range(int64* l, int64* u) override { inner_->Range(l, u); }

  void SetRange(int64 l, int64 u) override {
    if (l > inner_->Min() || u < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetRange(inner_, l, u);
      inner_->SetRange(l, u);
    }
  }

  bool Bound() const override { return inner_->Bound(); }

  bool IsVar() const override {
    DCHECK(!inner_->IsVar());
    return false;
  }

  IntVar* Var() override { return solver()->RegisterIntVar(inner_->Var()); }

  void WhenRange(Demon* d) override { inner_->WhenRange(d); }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kTrace, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            inner_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kTrace, this);
  }

  std::string DebugString() const override { return inner_->DebugString(); }

 private:
  IntExpr* const inner_;
};

class TraceIntervalVar : public IntervalVar {
 public:
  TraceIntervalVar(Solver* const solver, IntervalVar* const inner)
      : IntervalVar(solver, ""), inner_(inner) {
    if (inner->HasName()) {
      set_name(inner->name());
    }
  }
  ~TraceIntervalVar() override {}

  int64 StartMin() const override { return inner_->StartMin(); }

  int64 StartMax() const override { return inner_->StartMax(); }

  void SetStartMin(int64 m) override {
    if (inner_->MayBePerformed() && (m > inner_->StartMin())) {
      solver()->GetPropagationMonitor()->SetStartMin(inner_, m);
      inner_->SetStartMin(m);
    }
  }

  void SetStartMax(int64 m) override {
    if (inner_->MayBePerformed() && (m < inner_->StartMax())) {
      solver()->GetPropagationMonitor()->SetStartMax(inner_, m);
      inner_->SetStartMax(m);
    }
  }

  void SetStartRange(int64 mi, int64 ma) override {
    if (inner_->MayBePerformed() &&
        (mi > inner_->StartMin() || ma < inner_->StartMax())) {
      solver()->GetPropagationMonitor()->SetStartRange(inner_, mi, ma);
      inner_->SetStartRange(mi, ma);
    }
  }

  int64 OldStartMin() const override { return inner_->OldStartMin(); }

  int64 OldStartMax() const override { return inner_->OldStartMax(); }

  void WhenStartRange(Demon* const d) override { inner_->WhenStartRange(d); }

  void WhenStartBound(Demon* const d) override { inner_->WhenStartBound(d); }

  int64 EndMin() const override { return inner_->EndMin(); }

  int64 EndMax() const override { return inner_->EndMax(); }

  void SetEndMin(int64 m) override {
    if (inner_->MayBePerformed() && (m > inner_->EndMin())) {
      solver()->GetPropagationMonitor()->SetEndMin(inner_, m);
      inner_->SetEndMin(m);
    }
  }

  void SetEndMax(int64 m) override {
    if (inner_->MayBePerformed() && (m < inner_->EndMax())) {
      solver()->GetPropagationMonitor()->SetEndMax(inner_, m);
      inner_->SetEndMax(m);
    }
  }

  void SetEndRange(int64 mi, int64 ma) override {
    if (inner_->MayBePerformed() &&
        (mi > inner_->EndMin() || ma < inner_->EndMax())) {
      solver()->GetPropagationMonitor()->SetEndRange(inner_, mi, ma);
      inner_->SetEndRange(mi, ma);
    }
  }

  int64 OldEndMin() const override { return inner_->OldEndMin(); }

  int64 OldEndMax() const override { return inner_->OldEndMax(); }

  void WhenEndRange(Demon* const d) override { inner_->WhenEndRange(d); }

  void WhenEndBound(Demon* const d) override { inner_->WhenStartBound(d); }

  int64 DurationMin() const override { return inner_->DurationMin(); }

  int64 DurationMax() const override { return inner_->DurationMax(); }

  void SetDurationMin(int64 m) override {
    if (inner_->MayBePerformed() && (m > inner_->DurationMin())) {
      solver()->GetPropagationMonitor()->SetDurationMin(inner_, m);
      inner_->SetDurationMin(m);
    }
  }

  void SetDurationMax(int64 m) override {
    if (inner_->MayBePerformed() && (m < inner_->DurationMax())) {
      solver()->GetPropagationMonitor()->SetDurationMax(inner_, m);
      inner_->SetDurationMax(m);
    }
  }

  void SetDurationRange(int64 mi, int64 ma) override {
    if (inner_->MayBePerformed() &&
        (mi > inner_->DurationMin() || ma < inner_->DurationMax())) {
      solver()->GetPropagationMonitor()->SetDurationRange(inner_, mi, ma);
      inner_->SetDurationRange(mi, ma);
    }
  }

  int64 OldDurationMin() const override { return inner_->OldDurationMin(); }

  int64 OldDurationMax() const override { return inner_->OldDurationMax(); }

  void WhenDurationRange(Demon* const d) override {
    inner_->WhenDurationRange(d);
  }

  void WhenDurationBound(Demon* const d) override {
    inner_->WhenDurationBound(d);
  }

  bool MustBePerformed() const override { return inner_->MustBePerformed(); }

  bool MayBePerformed() const override { return inner_->MayBePerformed(); }

  void SetPerformed(bool value) override {
    if ((value && !inner_->MustBePerformed()) ||
        (!value && inner_->MayBePerformed())) {
      solver()->GetPropagationMonitor()->SetPerformed(inner_, value);
      inner_->SetPerformed(value);
    }
  }

  bool WasPerformedBound() const override {
    return inner_->WasPerformedBound();
  }

  void WhenPerformedBound(Demon* const d) override {
    inner_->WhenPerformedBound(d);
  }

  IntExpr* StartExpr() override { return inner_->StartExpr(); }
  IntExpr* DurationExpr() override { return inner_->DurationExpr(); }
  IntExpr* EndExpr() override { return inner_->EndExpr(); }
  IntExpr* PerformedExpr() override { return inner_->PerformedExpr(); }
  IntExpr* SafeStartExpr(int64 unperformed_value) override {
    return inner_->SafeStartExpr(unperformed_value);
  }
  IntExpr* SafeDurationExpr(int64 unperformed_value) override {
    return inner_->SafeDurationExpr(unperformed_value);
  }
  IntExpr* SafeEndExpr(int64 unperformed_value) override {
    return inner_->SafeEndExpr(unperformed_value);
  }

  void Accept(ModelVisitor* const visitor) const override {
    inner_->Accept(visitor);
  }

  std::string DebugString() const override { return inner_->DebugString(); }

 private:
  IntervalVar* const inner_;
};

// ---------- PrintTrace ----------

class PrintTrace : public PropagationMonitor {
 public:
  struct Info {
    explicit Info(const std::string& m) : message(m), displayed(false) {}
    std::string message;
    bool displayed;
  };

  struct Context {
    Context()
        : initial_indent(0),
          indent(0),
          in_demon(false),
          in_constraint(false),
          in_decision_builder(false),
          in_decision(false),
          in_objective(false) {}

    explicit Context(int start_indent)
        : initial_indent(start_indent),
          indent(start_indent),
          in_demon(false),
          in_constraint(false),
          in_decision_builder(false),
          in_decision(false),
          in_objective(false) {}

    bool TopLevel() const { return initial_indent == indent; }

    void Clear() {
      indent = initial_indent;
      in_demon = false;
      in_constraint = false;
      in_decision_builder = false;
      in_decision = false;
      in_objective = false;
      delayed_info.clear();
    }

    int initial_indent;
    int indent;
    bool in_demon;
    bool in_constraint;
    bool in_decision_builder;
    bool in_decision;
    bool in_objective;
    std::vector<Info> delayed_info;
  };

  explicit PrintTrace(Solver* const s) : PropagationMonitor(s) {
    contexes_.push(Context());
  }

  ~PrintTrace() override {}

  // ----- Search events -----

  void BeginInitialPropagation() override {
    CheckNoDelayed();
    DisplaySearch("Root Node Propagation");
    IncreaseIndent();
  }
  void EndInitialPropagation() override {
    DecreaseIndent();
    DisplaySearch("Starting Tree Search");
  }

  void BeginNextDecision(DecisionBuilder* const b) override {
    DisplaySearch(
        StringPrintf("DecisionBuilder(%s)", b->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision_builder = true;
  }

  // After calling DecisionBuilder::Next, along with the returned decision.
  void EndNextDecision(DecisionBuilder* const b, Decision* const d) override {
    contexes_.top().in_decision_builder = false;
    DecreaseIndent();
  }

  void BeginFail() override {
    contexes_.top().Clear();
    while (!contexes_.top().TopLevel()) {
      DecreaseIndent();
      LOG(INFO) << Indent() << "}";
    }
    DisplaySearch(StringPrintf("Failure at depth %d", solver()->SearchDepth()));
  }

  bool AtSolution() override {
    DisplaySearch(
        StringPrintf("Solution found at depth %d", solver()->SearchDepth()));
    return false;
  }

  void ApplyDecision(Decision* const decision) override {
    DisplaySearch(
        StringPrintf("ApplyDecision(%s)", decision->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision = true;
  }

  void RefuteDecision(Decision* const decision) override {
    if (contexes_.top().in_objective) {
      DecreaseIndent();
      contexes_.top().in_objective = false;
    }
    DisplaySearch(
        StringPrintf("RefuteDecision(%s)", decision->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision = true;
  }

  void AfterDecision(Decision* const decision, bool direction) override {
    DecreaseIndent();
    contexes_.top().in_decision = false;
  }

  void EnterSearch() override {
    if (solver()->SolveDepth() == 0) {
      CHECK_EQ(1, contexes_.size());
      contexes_.top().Clear();
    } else {
      PrintDelayedString();
      PushNestedContext();
    }
    DisplaySearch("Enter Search");
  }

  void ExitSearch() override {
    DisplaySearch("Exit Search");
    CHECK(contexes_.top().TopLevel());
    if (solver()->SolveDepth() > 1) {
      contexes_.pop();
    }
  }

  void RestartSearch() override { CHECK(contexes_.top().TopLevel()); }

  // ----- Propagation events -----

  void BeginConstraintInitialPropagation(
      Constraint* const constraint) override {
    PushDelayedInfo(
        StringPrintf("Constraint(%s)", constraint->DebugString().c_str()));
    contexes_.top().in_constraint = true;
  }

  void EndConstraintInitialPropagation(Constraint* const constraint) override {
    PopDelayedInfo();
    contexes_.top().in_constraint = false;
  }

  void BeginNestedConstraintInitialPropagation(
      Constraint* const parent, Constraint* const nested) override {
    PushDelayedInfo(
        StringPrintf("Constraint(%s)", nested->DebugString().c_str()));
    contexes_.top().in_constraint = true;
  }
  void EndNestedConstraintInitialPropagation(Constraint* const,
                                             Constraint* const) override {
    PopDelayedInfo();
    contexes_.top().in_constraint = false;
  }

  void RegisterDemon(Demon* const demon) override {}

  void BeginDemonRun(Demon* const demon) override {
    if (demon->priority() != Solver::VAR_PRIORITY) {
      contexes_.top().in_demon = true;
      PushDelayedInfo(StringPrintf("Demon(%s)", demon->DebugString().c_str()));
    }
  }

  void EndDemonRun(Demon* const demon) override {
    if (demon->priority() != Solver::VAR_PRIORITY) {
      contexes_.top().in_demon = false;
      PopDelayedInfo();
    }
  }

  void StartProcessingIntegerVariable(IntVar* const var) override {
    PushDelayedInfo(
        StringPrintf("StartProcessing(%s)", var->DebugString().c_str()));
  }

  void EndProcessingIntegerVariable(IntVar* const var) override {
    PopDelayedInfo();
  }

  void PushContext(const std::string& context) override { PushDelayedInfo(context); }

  void PopContext() override { PopDelayedInfo(); }

  // ----- IntExpr modifiers -----

  void SetMin(IntExpr* const expr, int64 new_min) override {
    DisplayModification(
        StringPrintf("SetMin(%s, %lld)", expr->DebugString().c_str(), new_min));
  }

  void SetMax(IntExpr* const expr, int64 new_max) override {
    DisplayModification(
        StringPrintf("SetMax(%s, %lld)", expr->DebugString().c_str(), new_max));
  }

  void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) override {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     expr->DebugString().c_str(), new_min,
                                     new_max));
  }

  // ----- IntVar modifiers -----

  void SetMin(IntVar* const var, int64 new_min) override {
    DisplayModification(
        StringPrintf("SetMin(%s, %lld)", var->DebugString().c_str(), new_min));
  }

  void SetMax(IntVar* const var, int64 new_max) override {
    DisplayModification(
        StringPrintf("SetMax(%s, %lld)", var->DebugString().c_str(), new_max));
  }

  void SetRange(IntVar* const var, int64 new_min, int64 new_max) override {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  void RemoveValue(IntVar* const var, int64 value) override {
    DisplayModification(StringPrintf("RemoveValue(%s, %lld)",
                                     var->DebugString().c_str(), value));
  }

  void SetValue(IntVar* const var, int64 value) override {
    DisplayModification(
        StringPrintf("SetValue(%s, %lld)", var->DebugString().c_str(), value));
  }

  void RemoveInterval(IntVar* const var, int64 imin, int64 imax) override {
    DisplayModification(StringPrintf("RemoveInterval(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), imin, imax));
  }

  void SetValues(IntVar* const var, const std::vector<int64>& values) override {
    DisplayModification(StringPrintf("SetValues(%s, %s)",
                                     var->DebugString().c_str(),
                                     strings::Join(values, ", ").c_str()));
  }

  void RemoveValues(IntVar* const var,
                    const std::vector<int64>& values) override {
    DisplayModification(StringPrintf("RemoveValues(%s, %s)",
                                     var->DebugString().c_str(),
                                     strings::Join(values, ", ").c_str()));
  }

  // ----- IntervalVar modifiers -----

  void SetStartMin(IntervalVar* const var, int64 new_min) override {
    DisplayModification(StringPrintf("SetStartMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  void SetStartMax(IntervalVar* const var, int64 new_max) override {
    DisplayModification(StringPrintf("SetStartMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  void SetStartRange(IntervalVar* const var, int64 new_min,
                     int64 new_max) override {
    DisplayModification(StringPrintf("SetStartRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  void SetEndMin(IntervalVar* const var, int64 new_min) override {
    DisplayModification(StringPrintf("SetEndMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  void SetEndMax(IntervalVar* const var, int64 new_max) override {
    DisplayModification(StringPrintf("SetEndMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  void SetEndRange(IntervalVar* const var, int64 new_min,
                   int64 new_max) override {
    DisplayModification(StringPrintf("SetEndRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  void SetDurationMin(IntervalVar* const var, int64 new_min) override {
    DisplayModification(StringPrintf("SetDurationMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  void SetDurationMax(IntervalVar* const var, int64 new_max) override {
    DisplayModification(StringPrintf("SetDurationMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  void SetDurationRange(IntervalVar* const var, int64 new_min,
                        int64 new_max) override {
    DisplayModification(StringPrintf("SetDurationRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  void SetPerformed(IntervalVar* const var, bool value) override {
    DisplayModification(StringPrintf("SetPerformed(%s, %d)",
                                     var->DebugString().c_str(), value));
  }

  void RankFirst(SequenceVar* const var, int index) override {
    DisplayModification(
        StringPrintf("RankFirst(%s, %d)", var->DebugString().c_str(), index));
  }

  void RankNotFirst(SequenceVar* const var, int index) override {
    DisplayModification(StringPrintf("RankNotFirst(%s, %d)",
                                     var->DebugString().c_str(), index));
  }

  void RankLast(SequenceVar* const var, int index) override {
    DisplayModification(
        StringPrintf("RankLast(%s, %d)", var->DebugString().c_str(), index));
  }

  void RankNotLast(SequenceVar* const var, int index) override {
    DisplayModification(
        StringPrintf("RankNotLast(%s, %d)", var->DebugString().c_str(), index));
  }

  void RankSequence(SequenceVar* const var, const std::vector<int>& rank_first,
                    const std::vector<int>& rank_last,
                    const std::vector<int>& unperformed) override {
    DisplayModification(StringPrintf(
        "RankSequence(%s, forward [%s], backward[%s], unperformed[%s])",
        var->DebugString().c_str(), strings::Join(rank_first, ", ").c_str(),
        strings::Join(rank_last, ", ").c_str(),
        strings::Join(unperformed, ", ").c_str()));
  }

  void Install() override {
    SearchMonitor::Install();
    if (solver()->SolveDepth() <= 1) {
      solver()->AddPropagationMonitor(this);
    }
  }

  std::string DebugString() const override { return "PrintTrace"; }

 private:
  void PushDelayedInfo(const std::string& delayed) {
    if (FLAGS_cp_full_trace) {
      LOG(INFO) << Indent() << delayed << " {";
      IncreaseIndent();
    } else {
      contexes_.top().delayed_info.push_back(Info(delayed));
    }
  }

  void PopDelayedInfo() {
    if (FLAGS_cp_full_trace) {
      DecreaseIndent();
      LOG(INFO) << Indent() << "}";
    } else {
      CHECK(!contexes_.top().delayed_info.empty());
      if (contexes_.top().delayed_info.back().displayed &&
          !contexes_.top().TopLevel()) {
        DecreaseIndent();
        LOG(INFO) << Indent() << "}";
      } else {
        contexes_.top().delayed_info.pop_back();
      }
    }
  }

  void CheckNoDelayed() { CHECK(contexes_.top().delayed_info.empty()); }

  void PrintDelayedString() {
    const std::vector<Info>& infos = contexes_.top().delayed_info;
    for (int i = 0; i < infos.size(); ++i) {
      const Info& info = infos[i];
      if (!info.displayed) {
        LOG(INFO) << Indent() << info.message << " {";
        IncreaseIndent();
        // Marks it as displayed.
        contexes_.top().delayed_info[i].displayed = true;
      }
    }
  }

  void DisplayModification(const std::string& to_print) {
    if (FLAGS_cp_full_trace) {
      LOG(INFO) << Indent() << to_print;
    } else {
      PrintDelayedString();
      if (contexes_.top().in_demon || contexes_.top().in_constraint ||
          contexes_.top().in_decision_builder || contexes_.top().in_decision ||
          contexes_.top().in_objective) {
        // Inside a demon, constraint, decision builder -> normal print.
        LOG(INFO) << Indent() << to_print;
      } else {
        // Top level, modification pushed by the objective.  This is a
        // hack. The SetMax or SetMin done by the objective happens in
        // the RefuteDecision callback of search monitors.  We cannot
        // easily differentiate that from the actual modifications done
        // by the Refute() call itself.  To distinguish that, we force
        // the print trace to be last in the list of monitors. Thus
        // modifications that happens at the top level before the
        // RefuteDecision() callbacks must be from the objective.
        // In that case, we push the in_objective context.
        CHECK(contexes_.top().TopLevel());
        DisplaySearch(StringPrintf("Objective -> %s", to_print.c_str()));
        IncreaseIndent();
        contexes_.top().in_objective = true;
      }
    }
  }

  void DisplaySearch(const std::string& to_print) {
    const int solve_depth = solver()->SolveDepth();
    if (solve_depth <= 1) {
      LOG(INFO) << Indent() << "######## Top Level Search: " << to_print;
    } else {
      LOG(INFO) << Indent() << "######## Nested Search(" << solve_depth - 1
                << "): " << to_print;
    }
  }

  std::string Indent() {
    CHECK_GE(contexes_.top().indent, 0);
    std::string output = " @ ";
    for (int i = 0; i < contexes_.top().indent; ++i) {
      output.append("    ");
    }
    return output;
  }

  void IncreaseIndent() { contexes_.top().indent++; }

  void DecreaseIndent() {
    if (contexes_.top().indent > 0) {
      contexes_.top().indent--;
    }
  }

  void PushNestedContext() {
    const int initial_indent = contexes_.top().indent;
    contexes_.push(Context(initial_indent));
  }

  std::stack<Context> contexes_;
};
}  // namespace

IntExpr* Solver::RegisterIntExpr(IntExpr* const expr) {
  if (InstrumentsVariables()) {
    if (expr->IsVar()) {
      return RegisterIntVar(expr->Var());
    } else {
      return RevAlloc(new TraceIntExpr(this, expr));
    }
  } else {
    return expr;
  }
}

IntVar* Solver::RegisterIntVar(IntVar* const var) {
  if (InstrumentsVariables() && var->VarType() != TRACE_VAR) {  // Not already a
                                                                // trace var.
    return RevAlloc(new TraceIntVar(this, var));
  } else {
    return var;
  }
}

IntervalVar* Solver::RegisterIntervalVar(IntervalVar* const var) {
  if (InstrumentsVariables()) {
    return RevAlloc(new TraceIntervalVar(this, var));
  } else {
    return var;
  }
}

PropagationMonitor* BuildPrintTrace(Solver* const s) {
  return s->RevAlloc(new PrintTrace(s));
}
}  // namespace operations_research
