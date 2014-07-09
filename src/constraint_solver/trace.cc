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
#include "base/hash.h"
#include <stack>
#include <string>
#include <utility>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "base/map_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

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

  virtual ~TraceIntVar() {}

  virtual int64 Min() const { return inner_->Min(); }

  virtual void SetMin(int64 m) {
    if (m > inner_->Min()) {
      solver()->GetPropagationMonitor()->SetMin(inner_, m);
      inner_->SetMin(m);
    }
  }

  virtual int64 Max() const { return inner_->Max(); }

  virtual void SetMax(int64 m) {
    if (m < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetMax(inner_, m);
      inner_->SetMax(m);
    }
  }

  virtual void Range(int64* l, int64* u) { inner_->Range(l, u); }

  virtual void SetRange(int64 l, int64 u) {
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

  virtual bool Bound() const { return inner_->Bound(); }

  virtual bool IsVar() const { return true; }

  virtual IntVar* Var() { return this; }

  virtual int64 Value() const { return inner_->Value(); }

  virtual void RemoveValue(int64 v) {
    if (inner_->Contains(v)) {
      solver()->GetPropagationMonitor()->RemoveValue(inner_, v);
      inner_->RemoveValue(v);
    }
  }

  virtual void SetValue(int64 v) {
    solver()->GetPropagationMonitor()->SetValue(inner_, v);
    inner_->SetValue(v);
  }

  virtual void RemoveInterval(int64 l, int64 u) {
    solver()->GetPropagationMonitor()->RemoveInterval(inner_, l, u);
    inner_->RemoveInterval(l, u);
  }

  virtual void RemoveValues(const std::vector<int64>& values) {
    solver()->GetPropagationMonitor()->RemoveValues(inner_, values);
    inner_->RemoveValues(values);
  }

  virtual void SetValues(const std::vector<int64>& values) {
    solver()->GetPropagationMonitor()->SetValues(inner_, values);
    inner_->SetValues(values);
  }

  virtual void WhenRange(Demon* d) { inner_->WhenRange(d); }

  virtual void WhenBound(Demon* d) { inner_->WhenBound(d); }

  virtual void WhenDomain(Demon* d) { inner_->WhenDomain(d); }

  virtual uint64 Size() const { return inner_->Size(); }

  virtual bool Contains(int64 v) const { return inner_->Contains(v); }

  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return inner_->MakeHoleIterator(reversible);
  }

  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return inner_->MakeDomainIterator(reversible);
  }

  virtual int64 OldMin() const { return inner_->OldMin(); }

  virtual int64 OldMax() const { return inner_->OldMax(); }

  virtual int VarType() const { return TRACE_VAR; }

  virtual void Accept(ModelVisitor* const visitor) const {
    IntExpr* const cast_expr =
        solver()->CastExpression(const_cast<TraceIntVar*>(this));
    if (cast_expr != nullptr) {
      visitor->VisitIntegerVariable(this, cast_expr);
    } else {
      visitor->VisitIntegerVariable(this, ModelVisitor::kTraceOperation, 0,
                                    inner_);
    }
  }

  virtual std::string DebugString() const { return inner_->DebugString(); }

  virtual IntVar* IsEqual(int64 constant) { return inner_->IsEqual(constant); }

  virtual IntVar* IsDifferent(int64 constant) {
    return inner_->IsDifferent(constant);
  }

  virtual IntVar* IsGreaterOrEqual(int64 constant) {
    return inner_->IsGreaterOrEqual(constant);
  }

  virtual IntVar* IsLessOrEqual(int64 constant) {
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

  virtual ~TraceIntExpr() {}

  virtual int64 Min() const { return inner_->Min(); }

  virtual void SetMin(int64 m) {
    solver()->GetPropagationMonitor()->SetMin(inner_, m);
    inner_->SetMin(m);
  }

  virtual int64 Max() const { return inner_->Max(); }

  virtual void SetMax(int64 m) {
    solver()->GetPropagationMonitor()->SetMax(inner_, m);
    inner_->SetMax(m);
  }

  virtual void Range(int64* l, int64* u) { inner_->Range(l, u); }

  virtual void SetRange(int64 l, int64 u) {
    if (l > inner_->Min() || u < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetRange(inner_, l, u);
      inner_->SetRange(l, u);
    }
  }

  virtual bool Bound() const { return inner_->Bound(); }

  virtual bool IsVar() const {
    DCHECK(!inner_->IsVar());
    return false;
  }

  virtual IntVar* Var() { return solver()->RegisterIntVar(inner_->Var()); }

  virtual void WhenRange(Demon* d) { inner_->WhenRange(d); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitIntegerExpression(ModelVisitor::kTrace, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            inner_);
    visitor->EndVisitIntegerExpression(ModelVisitor::kTrace, this);
  }

  virtual std::string DebugString() const { return inner_->DebugString(); }

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
  virtual ~TraceIntervalVar() {}

  virtual int64 StartMin() const { return inner_->StartMin(); }

  virtual int64 StartMax() const { return inner_->StartMax(); }

  virtual void SetStartMin(int64 m) {
    if (inner_->MayBePerformed() && (m > inner_->StartMin())) {
      solver()->GetPropagationMonitor()->SetStartMin(inner_, m);
      inner_->SetStartMin(m);
    }
  }

  virtual void SetStartMax(int64 m) {
    if (inner_->MayBePerformed() && (m < inner_->StartMax())) {
      solver()->GetPropagationMonitor()->SetStartMax(inner_, m);
      inner_->SetStartMax(m);
    }
  }

  virtual void SetStartRange(int64 mi, int64 ma) {
    if (inner_->MayBePerformed() &&
        (mi > inner_->StartMin() || ma < inner_->StartMax())) {
      solver()->GetPropagationMonitor()->SetStartRange(inner_, mi, ma);
      inner_->SetStartRange(mi, ma);
    }
  }

  virtual int64 OldStartMin() const { return inner_->OldStartMin(); }

  virtual int64 OldStartMax() const { return inner_->OldStartMax(); }

  virtual void WhenStartRange(Demon* const d) { inner_->WhenStartRange(d); }

  virtual void WhenStartBound(Demon* const d) { inner_->WhenStartBound(d); }

  virtual int64 EndMin() const { return inner_->EndMin(); }

  virtual int64 EndMax() const { return inner_->EndMax(); }

  virtual void SetEndMin(int64 m) {
    if (inner_->MayBePerformed() && (m > inner_->EndMin())) {
      solver()->GetPropagationMonitor()->SetEndMin(inner_, m);
      inner_->SetEndMin(m);
    }
  }

  virtual void SetEndMax(int64 m) {
    if (inner_->MayBePerformed() && (m < inner_->EndMax())) {
      solver()->GetPropagationMonitor()->SetEndMax(inner_, m);
      inner_->SetEndMax(m);
    }
  }

  virtual void SetEndRange(int64 mi, int64 ma) {
    if (inner_->MayBePerformed() &&
        (mi > inner_->EndMin() || ma < inner_->EndMax())) {
      solver()->GetPropagationMonitor()->SetEndRange(inner_, mi, ma);
      inner_->SetEndRange(mi, ma);
    }
  }

  virtual int64 OldEndMin() const { return inner_->OldEndMin(); }

  virtual int64 OldEndMax() const { return inner_->OldEndMax(); }

  virtual void WhenEndRange(Demon* const d) { inner_->WhenEndRange(d); }

  virtual void WhenEndBound(Demon* const d) { inner_->WhenStartBound(d); }

  virtual int64 DurationMin() const { return inner_->DurationMin(); }

  virtual int64 DurationMax() const { return inner_->DurationMax(); }

  virtual void SetDurationMin(int64 m) {
    if (inner_->MayBePerformed() && (m > inner_->DurationMin())) {
      solver()->GetPropagationMonitor()->SetDurationMin(inner_, m);
      inner_->SetDurationMin(m);
    }
  }

  virtual void SetDurationMax(int64 m) {
    if (inner_->MayBePerformed() && (m < inner_->DurationMax())) {
      solver()->GetPropagationMonitor()->SetDurationMax(inner_, m);
      inner_->SetDurationMax(m);
    }
  }

  virtual void SetDurationRange(int64 mi, int64 ma) {
    if (inner_->MayBePerformed() &&
        (mi > inner_->DurationMin() || ma < inner_->DurationMax())) {
      solver()->GetPropagationMonitor()->SetDurationRange(inner_, mi, ma);
      inner_->SetDurationRange(mi, ma);
    }
  }

  virtual int64 OldDurationMin() const { return inner_->OldDurationMin(); }

  virtual int64 OldDurationMax() const { return inner_->OldDurationMax(); }

  virtual void WhenDurationRange(Demon* const d) {
    inner_->WhenDurationRange(d);
  }

  virtual void WhenDurationBound(Demon* const d) {
    inner_->WhenDurationBound(d);
  }

  virtual bool MustBePerformed() const { return inner_->MustBePerformed(); }

  virtual bool MayBePerformed() const { return inner_->MayBePerformed(); }

  virtual void SetPerformed(bool value) {
    if ((value && !inner_->MustBePerformed()) ||
        (!value && inner_->MayBePerformed())) {
      solver()->GetPropagationMonitor()->SetPerformed(inner_, value);
      inner_->SetPerformed(value);
    }
  }

  virtual bool WasPerformedBound() const { return inner_->WasPerformedBound(); }

  virtual void WhenPerformedBound(Demon* const d) {
    inner_->WhenPerformedBound(d);
  }

  virtual IntExpr* StartExpr() { return inner_->StartExpr(); }
  virtual IntExpr* DurationExpr() { return inner_->DurationExpr(); }
  virtual IntExpr* EndExpr() { return inner_->EndExpr(); }
  virtual IntExpr* PerformedExpr() { return inner_->PerformedExpr(); }
  virtual IntExpr* SafeStartExpr(int64 unperformed_value) {
    return inner_->SafeStartExpr(unperformed_value);
  }
  virtual IntExpr* SafeDurationExpr(int64 unperformed_value) {
    return inner_->SafeDurationExpr(unperformed_value);
  }
  virtual IntExpr* SafeEndExpr(int64 unperformed_value) {
    return inner_->SafeEndExpr(unperformed_value);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    inner_->Accept(visitor);
  }

  virtual std::string DebugString() const { return inner_->DebugString(); }

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

  virtual ~PrintTrace() {}

  // ----- Search events -----

  virtual void BeginInitialPropagation() {
    CheckNoDelayed();
    DisplaySearch("Root Node Propagation");
    IncreaseIndent();
  }
  virtual void EndInitialPropagation() {
    DecreaseIndent();
    DisplaySearch("Starting Tree Search");
  }

  virtual void BeginNextDecision(DecisionBuilder* const b) {
    DisplaySearch(
        StringPrintf("DecisionBuilder(%s)", b->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision_builder = true;
  }

  // After calling DecisionBuilder::Next, along with the returned decision.
  virtual void EndNextDecision(DecisionBuilder* const b, Decision* const d) {
    contexes_.top().in_decision_builder = false;
    DecreaseIndent();
  }

  virtual void BeginFail() {
    contexes_.top().Clear();
    while (!contexes_.top().TopLevel()) {
      DecreaseIndent();
      LOG(INFO) << Indent() << "}";
    }
    DisplaySearch(StringPrintf("Failure at depth %d", solver()->SearchDepth()));
  }

  virtual bool AtSolution() {
    DisplaySearch(
        StringPrintf("Solution found at depth %d", solver()->SearchDepth()));
    return false;
  }

  virtual void ApplyDecision(Decision* const decision) {
    DisplaySearch(
        StringPrintf("ApplyDecision(%s)", decision->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision = true;
  }

  virtual void RefuteDecision(Decision* const decision) {
    if (contexes_.top().in_objective) {
      DecreaseIndent();
      contexes_.top().in_objective = false;
    }
    DisplaySearch(
        StringPrintf("RefuteDecision(%s)", decision->DebugString().c_str()));
    IncreaseIndent();
    contexes_.top().in_decision = true;
  }

  virtual void AfterDecision(Decision* const decision, bool direction) {
    DecreaseIndent();
    contexes_.top().in_decision = false;
  }

  virtual void EnterSearch() {
    if (solver()->SolveDepth() == 0) {
      CHECK_EQ(1, contexes_.size());
      contexes_.top().Clear();
    } else {
      PrintDelayedString();
      PushNestedContext();
    }
    DisplaySearch("Enter Search");
  }

  virtual void ExitSearch() {
    DisplaySearch("Exit Search");
    CHECK(contexes_.top().TopLevel());
    if (solver()->SolveDepth() > 1) {
      contexes_.pop();
    }
  }

  virtual void RestartSearch() { CHECK(contexes_.top().TopLevel()); }

  // ----- Propagation events -----

  virtual void BeginConstraintInitialPropagation(Constraint* const constraint) {
    PushDelayedInfo(
        StringPrintf("Constraint(%s)", constraint->DebugString().c_str()));
    contexes_.top().in_constraint = true;
  }

  virtual void EndConstraintInitialPropagation(Constraint* const constraint) {
    PopDelayedInfo();
    contexes_.top().in_constraint = false;
  }

  virtual void BeginNestedConstraintInitialPropagation(
      Constraint* const parent, Constraint* const nested) {
    PushDelayedInfo(
        StringPrintf("Constraint(%s)", nested->DebugString().c_str()));
    contexes_.top().in_constraint = true;
  }
  virtual void EndNestedConstraintInitialPropagation(Constraint* const,
                                                     Constraint* const) {
    PopDelayedInfo();
    contexes_.top().in_constraint = false;
  }

  virtual void RegisterDemon(Demon* const demon) {}

  virtual void BeginDemonRun(Demon* const demon) {
    if (demon->priority() != Solver::VAR_PRIORITY) {
      contexes_.top().in_demon = true;
      PushDelayedInfo(StringPrintf("Demon(%s)", demon->DebugString().c_str()));
    }
  }

  virtual void EndDemonRun(Demon* const demon) {
    if (demon->priority() != Solver::VAR_PRIORITY) {
      contexes_.top().in_demon = false;
      PopDelayedInfo();
    }
  }

  virtual void StartProcessingIntegerVariable(IntVar* const var) {
    PushDelayedInfo(
        StringPrintf("StartProcessing(%s)", var->DebugString().c_str()));
  }

  virtual void EndProcessingIntegerVariable(IntVar* const var) {
    PopDelayedInfo();
  }

  virtual void PushContext(const std::string& context) { PushDelayedInfo(context); }

  virtual void PopContext() { PopDelayedInfo(); }

  // ----- IntExpr modifiers -----

  virtual void SetMin(IntExpr* const expr, int64 new_min) {
    DisplayModification(
        StringPrintf("SetMin(%s, %lld)", expr->DebugString().c_str(), new_min));
  }

  virtual void SetMax(IntExpr* const expr, int64 new_max) {
    DisplayModification(
        StringPrintf("SetMax(%s, %lld)", expr->DebugString().c_str(), new_max));
  }

  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     expr->DebugString().c_str(), new_min,
                                     new_max));
  }

  // ----- IntVar modifiers -----

  virtual void SetMin(IntVar* const var, int64 new_min) {
    DisplayModification(
        StringPrintf("SetMin(%s, %lld)", var->DebugString().c_str(), new_min));
  }

  virtual void SetMax(IntVar* const var, int64 new_max) {
    DisplayModification(
        StringPrintf("SetMax(%s, %lld)", var->DebugString().c_str(), new_max));
  }

  virtual void SetRange(IntVar* const var, int64 new_min, int64 new_max) {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  virtual void RemoveValue(IntVar* const var, int64 value) {
    DisplayModification(StringPrintf("RemoveValue(%s, %lld)",
                                     var->DebugString().c_str(), value));
  }

  virtual void SetValue(IntVar* const var, int64 value) {
    DisplayModification(
        StringPrintf("SetValue(%s, %lld)", var->DebugString().c_str(), value));
  }

  virtual void RemoveInterval(IntVar* const var, int64 imin, int64 imax) {
    DisplayModification(StringPrintf("RemoveInterval(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), imin, imax));
  }

  virtual void SetValues(IntVar* const var, const std::vector<int64>& values) {
    DisplayModification(StringPrintf("SetValues(%s, %s)",
                                     var->DebugString().c_str(),
                                     strings::Join(values, ", ").c_str()));
  }

  virtual void RemoveValues(IntVar* const var, const std::vector<int64>& values) {
    DisplayModification(StringPrintf("RemoveValues(%s, %s)",
                                     var->DebugString().c_str(),
                                     strings::Join(values, ", ").c_str()));
  }

  // ----- IntervalVar modifiers -----

  virtual void SetStartMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetStartMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  virtual void SetStartMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetStartMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  virtual void SetStartRange(IntervalVar* const var, int64 new_min,
                             int64 new_max) {
    DisplayModification(StringPrintf("SetStartRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  virtual void SetEndMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetEndMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  virtual void SetEndMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetEndMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  virtual void SetEndRange(IntervalVar* const var, int64 new_min,
                           int64 new_max) {
    DisplayModification(StringPrintf("SetEndRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  virtual void SetDurationMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetDurationMin(%s, %lld)",
                                     var->DebugString().c_str(), new_min));
  }

  virtual void SetDurationMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetDurationMax(%s, %lld)",
                                     var->DebugString().c_str(), new_max));
  }

  virtual void SetDurationRange(IntervalVar* const var, int64 new_min,
                                int64 new_max) {
    DisplayModification(StringPrintf("SetDurationRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(), new_min,
                                     new_max));
  }

  virtual void SetPerformed(IntervalVar* const var, bool value) {
    DisplayModification(StringPrintf("SetPerformed(%s, %d)",
                                     var->DebugString().c_str(), value));
  }

  virtual void RankFirst(SequenceVar* const var, int index) {
    DisplayModification(
        StringPrintf("RankFirst(%s, %d)", var->DebugString().c_str(), index));
  }

  virtual void RankNotFirst(SequenceVar* const var, int index) {
    DisplayModification(StringPrintf("RankNotFirst(%s, %d)",
                                     var->DebugString().c_str(), index));
  }

  virtual void RankLast(SequenceVar* const var, int index) {
    DisplayModification(
        StringPrintf("RankLast(%s, %d)", var->DebugString().c_str(), index));
  }

  virtual void RankNotLast(SequenceVar* const var, int index) {
    DisplayModification(
        StringPrintf("RankNotLast(%s, %d)", var->DebugString().c_str(), index));
  }

  virtual void RankSequence(SequenceVar* const var,
                            const std::vector<int>& rank_first,
                            const std::vector<int>& rank_last,
                            const std::vector<int>& unperformed) {
    DisplayModification(StringPrintf(
        "RankSequence(%s, forward [%s], backward[%s], unperformed[%s])",
        var->DebugString().c_str(), strings::Join(rank_first, ", ").c_str(),
        strings::Join(rank_last, ", ").c_str(),
        strings::Join(unperformed, ", ").c_str()));
  }

  virtual void Install() {
    SearchMonitor::Install();
    if (solver()->SolveDepth() <= 1) {
      solver()->AddPropagationMonitor(this);
    }
  }

  virtual std::string DebugString() const { return "PrintTrace"; }

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
