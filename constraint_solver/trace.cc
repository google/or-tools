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

#include <math.h>
#include <string.h>
#include <algorithm>
#include "base/hash.h"
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

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

  virtual void Range(int64* l, int64* u) {
    inner_->Range(l, u);
  }

  virtual void SetRange(int64 l, int64 u) {
    if (l > inner_->Min() || u < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetRange(inner_, l, u);
      inner_->SetRange(l, u);
    }
  }

  virtual bool Bound() const {
    return inner_->Bound();
  }

  virtual bool IsVar() const { return true; }

  virtual IntVar* Var() { return this; }

  virtual int64 Value() const {
    return inner_->Value();
  }

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

  virtual void RemoveValues(const int64* const values, int size) {
    solver()->GetPropagationMonitor()->RemoveValues(inner_, values, size);
    inner_->RemoveValues(values, size);
  }

  virtual void SetValues(const int64* const values, int size) {
    solver()->GetPropagationMonitor()->SetValues(inner_, values, size);
    inner_->SetValues(values, size);
  }

  virtual void WhenRange(Demon* d) {
    inner_->WhenRange(d);
  }

  virtual void WhenBound(Demon* d) {
    inner_->WhenBound(d);
  }

  virtual void WhenDomain(Demon* d) {
    inner_->WhenDomain(d);
  }

  virtual uint64 Size() const {
    return inner_->Size();
  }

  virtual bool Contains(int64 v) const {
    return inner_->Contains(v);
  }

  virtual IntVarIterator* MakeHoleIterator(bool reversible) const {
    return inner_->MakeHoleIterator(reversible);
  }

  virtual IntVarIterator* MakeDomainIterator(bool reversible) const {
    return inner_->MakeDomainIterator(reversible);
  }

  virtual int64 OldMin() const {
    return inner_->OldMin();
  }

  virtual int64 OldMax() const {
    return inner_->OldMax();
  }

  virtual int VarType() const {
    return TRACE_VAR;
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    inner_->Accept(visitor);
  }

  virtual string DebugString() const {
    return inner_->DebugString();
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

  virtual void Range(int64* l, int64* u) {
    inner_->Range(l, u);
  }

  virtual void SetRange(int64 l, int64 u) {
    if (l > inner_->Min() || u < inner_->Max()) {
      solver()->GetPropagationMonitor()->SetRange(inner_, l, u);
      inner_->SetRange(l, u);
    }
  }

  virtual bool Bound() const {
    return inner_->Bound();
  }

  virtual bool IsVar() const {
    DCHECK(!inner_->IsVar());
    return false;
  }

  virtual IntVar* Var() {
    return solver()->RegisterIntVar(inner_->Var());
  }

  virtual void WhenRange(Demon* d) {
    inner_->WhenRange(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    inner_->Accept(visitor);
  }

  virtual string DebugString() const {
    return inner_->DebugString();
  }

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

  virtual int64 StartMin() const {
    return inner_->StartMin();
  }

  virtual int64 StartMax() const  {
    return inner_->StartMax();
  }

  virtual void SetStartMin(int64 m) {
    if (m > inner_->StartMin()) {
      solver()->GetPropagationMonitor()->SetStartMin(inner_, m);
      inner_->SetStartMin(m);
    }
  }

  virtual void SetStartMax(int64 m) {
    if (m < inner_->StartMax()) {
      solver()->GetPropagationMonitor()->SetStartMax(inner_, m);
      inner_->SetStartMax(m);
    }
  }

  virtual void SetStartRange(int64 mi, int64 ma) {
    if (mi > inner_->StartMin() || ma < inner_->StartMax()) {
      solver()->GetPropagationMonitor()->SetStartRange(inner_, mi, ma);
      inner_->SetStartRange(mi, ma);
    }
  }

  virtual void WhenStartRange(Demon* const d) {
    inner_->WhenStartRange(d);
  }

  virtual void WhenStartBound(Demon* const d) {
    inner_->WhenStartBound(d);
  }

  virtual int64 EndMin() const {
    return inner_->EndMin();
  }

  virtual int64 EndMax() const  {
    return inner_->EndMax();
  }

  virtual void SetEndMin(int64 m) {
    if (m > inner_->EndMin()) {
      solver()->GetPropagationMonitor()->SetEndMin(inner_, m);
      inner_->SetEndMin(m);
    }
  }

  virtual void SetEndMax(int64 m) {
    if (m < inner_->EndMax()) {
      solver()->GetPropagationMonitor()->SetEndMax(inner_, m);
      inner_->SetEndMax(m);
    }
  }

  virtual void SetEndRange(int64 mi, int64 ma) {
    if (mi > inner_->EndMin() || ma < inner_->EndMax()) {
      solver()->GetPropagationMonitor()->SetEndRange(inner_, mi, ma);
      inner_->SetEndRange(mi, ma);
    }
  }

  virtual void WhenEndRange(Demon* const d) {
    inner_->WhenEndRange(d);
  }

  virtual void WhenEndBound(Demon* const d) {
    inner_->WhenStartBound(d);
  }

  virtual int64 DurationMin() const {
    return inner_->DurationMin();
  }

  virtual int64 DurationMax() const  {
    return inner_->DurationMax();
  }

  virtual void SetDurationMin(int64 m) {
    if (m > inner_->DurationMin()) {
      solver()->GetPropagationMonitor()->SetDurationMin(inner_, m);
      inner_->SetDurationMin(m);
    }
  }

  virtual void SetDurationMax(int64 m) {
    if (m < inner_->DurationMax()) {
      solver()->GetPropagationMonitor()->SetDurationMax(inner_, m);
      inner_->SetDurationMax(m);
    }
  }

  virtual void SetDurationRange(int64 mi, int64 ma) {
    if (mi > inner_->DurationMin() || ma < inner_->DurationMax()) {
      solver()->GetPropagationMonitor()->SetDurationRange(inner_, mi, ma);
      inner_->SetDurationRange(mi, ma);
    }
  }

  virtual void WhenDurationRange(Demon* const d) {
    inner_->WhenDurationRange(d);
  }

  virtual void WhenDurationBound(Demon* const d) {
    inner_->WhenDurationBound(d);
  }

  virtual bool MustBePerformed() const {
    return inner_->MustBePerformed();
  }

  virtual bool MayBePerformed() const {
    return inner_->MayBePerformed();
  }

  virtual void SetPerformed(bool value) {
    if ((value && !inner_->MustBePerformed()) ||
        (!value && inner_->MayBePerformed())) {
      solver()->GetPropagationMonitor()->SetPerformed(inner_, value);
      inner_->SetPerformed(value);
    }
  }

  virtual void WhenPerformedBound(Demon* const d) {
    inner_->WhenPerformedBound(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    inner_->Accept(visitor);
  }

 private:
  IntervalVar* const inner_;
};

// ---------- Trace ----------

class Trace : public PropagationMonitor {
 public:
  virtual void BeginInitialPropagation() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginInitialPropagation();
    }
  }

  virtual void EndInitialPropagation() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndInitialPropagation();
    }
  }

  virtual void BeginConstraintInitialPropagation(
      const Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginConstraintInitialPropagation(constraint);
    }
  }

  virtual void EndConstraintInitialPropagation(
      const Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndConstraintInitialPropagation(constraint);
    }
  }

  virtual void BeginNestedConstraintInitialPropagation(
      const Constraint* const parent,
      const Constraint* const nested) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginNestedConstraintInitialPropagation(parent, nested);
    }
  }

  virtual void EndNestedConstraintInitialPropagation(
      const Constraint* const parent,
      const Constraint* const nested) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndNestedConstraintInitialPropagation(parent, nested);
    }
  }

  virtual void RegisterDemon(const Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RegisterDemon(demon);
    }
  }

  virtual void BeginDemonRun(const Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->BeginDemonRun(demon);
    }
  }

  virtual void EndDemonRun(const Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndDemonRun(demon);
    }
  }

  virtual void RaiseFailure() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RaiseFailure();
    }
  }

  virtual void FindSolution() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->FindSolution();
    }
  }

  virtual void ApplyDecision(Decision* const decision) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->ApplyDecision(decision);
    }
  }

  virtual void RefuteDecision(Decision* const decision) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RefuteDecision(decision);
    }
  }

  virtual void AfterDecision(Decision* const decision) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->AfterDecision(decision);
    }
  }

  virtual void EnterSearch() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EnterSearch();
    }
  }

  virtual void ExitSearch() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->ExitSearch();
    }
  }

  virtual void RestartSearch() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RestartSearch();
    }
  }

  // IntExpr modifiers.
  virtual void SetMin(IntExpr* const expr, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMin(expr, new_min);
    }
  }

  virtual void SetMax(IntExpr* const expr, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMax(expr, new_max);
    }
  }

  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetRange(expr, new_min, new_max);
    }
  }

  // IntVar modifiers.
  virtual void SetMin(IntVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMin(var, new_min);
    }
  }

  virtual void SetMax(IntVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetMax(var, new_max);
    }
  }

  virtual void SetRange(IntVar* const var, int64 new_min, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetRange(var, new_min, new_max);
    }
  }

  virtual void RemoveValue(IntVar* const var, int64 value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveValue(var, value);
    }
  }

  virtual void SetValue(IntVar* const var, int64 value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetValue(var, value);
    }
  }

  virtual void RemoveInterval(IntVar* const var, int64 imin, int64 imax) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveInterval(var, imin, imax);
    }
  }

  virtual void SetValues(IntVar* const var,
                         const int64* const values,
                         int size) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetValues(var, values, size);
    }
  }

  virtual void RemoveValues(IntVar* const var,
                            const int64* const values,
                            int size) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->RemoveValues(var, values, size);
    }
  }

  // IntervalVar modifiers.
  virtual void SetStartMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartMin(var, new_min);
    }
  }

  virtual void SetStartMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartMax(var, new_max);
    }
  }

  virtual void SetStartRange(IntervalVar* const var,
                             int64 new_min,
                             int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetStartRange(var, new_min, new_max);
    }
  }

  virtual void SetEndMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndMin(var, new_min);
    }
  }

  virtual void SetEndMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndMax(var, new_max);
    }
  }

  virtual void SetEndRange(IntervalVar* const var,
                           int64 new_min,
                           int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetEndRange(var, new_min, new_max);
    }
  }

  virtual void SetDurationMin(IntervalVar* const var, int64 new_min) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationMin(var, new_min);
    }
  }

  virtual void SetDurationMax(IntervalVar* const var, int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationMax(var, new_max);
    }
  }

  virtual void SetDurationRange(IntervalVar* const var,
                                int64 new_min,
                                int64 new_max) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetDurationRange(var, new_min, new_max);
    }
  }

  virtual void SetPerformed(IntervalVar* const var, bool value) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->SetPerformed(var, value);
    }
  }

  void Add(PropagationMonitor* const monitor) {
    if (monitor != NULL) {
      monitors_.push_back(monitor);
    }
  }

 private:
  std::vector<PropagationMonitor*> monitors_;
};

// ---------- PrintTrace ----------

class PrintTrace : public PropagationMonitor {
 public:
  PrintTrace() : indent_(0) {}
  virtual ~PrintTrace() {}

  // Propagation events.
  virtual void BeginInitialPropagation() {
    CheckNoDelayed();
    Display("Initial Propagation {");
    IncreaseIndent();
  }
  virtual void EndInitialPropagation() {
    DecreaseIndent();
    Display("}  Starting Search");
  }

  virtual void BeginConstraintInitialPropagation(
      const Constraint* const constraint) {
    DelayPrintAndIndent(StringPrintf("InitialPropagate(%s)",
                                     constraint->DebugString().c_str()));
  }

  virtual void EndConstraintInitialPropagation(
      const Constraint* const constraint) {
    DelayCloseAndUnindent();
  }

  virtual void BeginNestedConstraintInitialPropagation(
      const Constraint* const parent,
      const Constraint* const nested) {
    DelayPrintAndIndent(StringPrintf("InitialPropagate(%s)",
                                     nested->DebugString().c_str()));
  }
  virtual void EndNestedConstraintInitialPropagation(
      const Constraint* const parent,
      const Constraint* const nested) {
    DelayCloseAndUnindent();
  }

  virtual void RegisterDemon(const Demon* const demon) {}

  virtual void BeginDemonRun(const Demon* const demon) {
    in_demon_ = true;
    DelayPrintAndIndent(StringPrintf("Run(%s)", demon->DebugString().c_str()));
  }

  virtual void EndDemonRun(const Demon* const demon) {
    in_demon_ = false;
    DelayCloseAndUnindent();
  }

  virtual void RaiseFailure() {
    in_demon_ = false;
    const bool top_level = indent_ == 0;
    DelayCloseAndUnindent();
    ClearIndent();
    if (top_level) {
      Display("  -------------------- Failure --------------------");
    } else {
      Display("} -------------------- Failure --------------------");
    }
  }

  virtual void FindSolution() {
    Display("++++++++++++++++++++ Solution ++++++++++++++++++++");
  }

  virtual void ApplyDecision(Decision* const decision) {
    Display(StringPrintf("----- Apply(%s) {", decision->DebugString().c_str()));
    IncreaseIndent();
  }

  virtual void RefuteDecision(Decision* const decision) {
    Display(StringPrintf("----- Refute(%s) {",
                         decision->DebugString().c_str()));
    IncreaseIndent();
  }

  virtual void AfterDecision(Decision* const decision) {
    DecreaseIndent();
    Display("}");
  }

  virtual void EnterSearch() {
    ClearIndent();
  }

  virtual void ExitSearch() {
    DCHECK_EQ(0, indent_);
  }

  virtual void RestartSearch() {
    DCHECK_EQ(0, indent_);
  }

  // IntExpr modifiers.
  virtual void SetMin(IntExpr* const expr, int64 new_min) {
    DisplayModification(StringPrintf("SetMin(%s, %lld)",
                                     expr->DebugString().c_str(),
                                     new_min));
  }

  virtual void SetMax(IntExpr* const expr, int64 new_max) {
    DisplayModification(StringPrintf("SetMax(%s, %lld)",
                                     expr->DebugString().c_str(),
                                     new_max));
  }

  virtual void SetRange(IntExpr* const expr, int64 new_min, int64 new_max) {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     expr->DebugString().c_str(),
                                     new_min,
                                     new_max));
  }

  // IntVar modifiers.
  virtual void SetMin(IntVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetMin(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_min));
  }

  virtual void SetMax(IntVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetMax(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_max));
  }

  virtual void SetRange(IntVar* const var, int64 new_min, int64 new_max) {
    DisplayModification(StringPrintf("SetRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(),
                                     new_min,
                                     new_max));
  }

  virtual void RemoveValue(IntVar* const var, int64 value) {
    DisplayModification(StringPrintf("RemoveValue(%s, %lld)",
                                     var->DebugString().c_str(),
                                     value));
  }

  virtual void SetValue(IntVar* const var, int64 value) {
    DisplayModification(StringPrintf("SetValue(%s, %lld)",
                                     var->DebugString().c_str(),
                                     value));
  }

  virtual void RemoveInterval(IntVar* const var, int64 imin, int64 imax) {
    DisplayModification(StringPrintf("RemoveInterval(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(),
                                     imin,
                                     imax));
  }

  virtual void SetValues(IntVar* const var,
                         const int64* const values,
                         int size) {}

  virtual void RemoveValues(IntVar* const var,
                            const int64* const values,
                            int size) {}
  // IntervalVar modifiers.
  virtual void SetStartMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetStartMin(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_min));
  }

  virtual void SetStartMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetStartMax(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_max));
  }

  virtual void SetStartRange(IntervalVar* const var,
                             int64 new_min,
                             int64 new_max) {
    DisplayModification(StringPrintf("SetStartRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(),
                                     new_min,
                                     new_max));
  }

  virtual void SetEndMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetEndMin(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_min));
  }

  virtual void SetEndMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetEndMax(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_max));
  }

  virtual void SetEndRange(IntervalVar* const var,
                           int64 new_min,
                           int64 new_max) {
    DisplayModification(StringPrintf("SetEndRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(),
                                     new_min,
                                     new_max));
  }

  virtual void SetDurationMin(IntervalVar* const var, int64 new_min) {
    DisplayModification(StringPrintf("SetDurationMin(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_min));
  }

  virtual void SetDurationMax(IntervalVar* const var, int64 new_max) {
    DisplayModification(StringPrintf("SetDurationMax(%s, %lld)",
                                     var->DebugString().c_str(),
                                     new_max));
  }

  virtual void SetDurationRange(IntervalVar* const var,
                                int64 new_min,
                                int64 new_max) {
    DisplayModification(StringPrintf("SetDurationRange(%s, [%lld .. %lld])",
                                     var->DebugString().c_str(),
                                     new_min,
                                     new_max));
  }

  virtual void SetPerformed(IntervalVar* const var, bool value) {
    DisplayModification(StringPrintf("SetPerformed(%s, %d)",
                                     var->DebugString().c_str(),
                                     value));
  }

 private:
  void DelayPrintAndIndent(const string& delayed) {
    CHECK(delayed_string_.empty());
    delayed_string_ = delayed;
  }

  void DelayCloseAndUnindent() {
    if (delayed_string_.empty() && indent_ > 0) {
      DecreaseIndent();
      Display("}");
    } else {
      delayed_string_ = "";
    }
  }

  void CheckNoDelayed() {
    CHECK(delayed_string_.empty());
  }

  void DisplayModification(const string& to_print) {
    if (!delayed_string_.empty()) {
      LOG(INFO) << Indent() << delayed_string_ << " {";
      IncreaseIndent();
      delayed_string_ = "";
    }
    if (in_demon_) {  // Inside a demon, normal print.
      LOG(INFO) << Indent() << to_print;
    } else if (indent_ == 0) {  // Top level, modification pushed by the
      // objective.
      LOG(INFO) << Indent() << "Objective: " << to_print;
    } else { // Not top level, but not in a demon -> Decision.
      LOG(INFO) << Indent() << "Decision: " << to_print;
    }
  }

  void Display(const string& to_print) {
    LOG(INFO) << Indent() << to_print;
  }

  string Indent() {
    string output = " @ ";
    for (int i = 0; i < indent_; ++i) {
      output.append("    ");
    }
    return output;
  }

  void IncreaseIndent() {
    indent_++;
  }

  void DecreaseIndent() {
    indent_--;
  }

  void ClearIndent() {
    indent_ = 0;
  }

  int indent_;
  string delayed_string_;
  bool in_demon_;
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
  if (InstrumentsVariables() &&
      var->VarType() != TRACE_VAR) {  // Not already a trace var.
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

PropagationMonitor* BuildTrace() {
  return new Trace();
}

void Solver::AddPropagationMonitor(PropagationMonitor* const monitor) {
  // TODO(user): Check solver state?
  reinterpret_cast<class Trace*>(propagation_monitor_.get())->Add(monitor);
}

PropagationMonitor* Solver::GetPropagationMonitor() const {
  return propagation_monitor_.get();
}

PropagationMonitor* BuildPrintTrace() {
  return new PrintTrace();
}
}  // namespace operations_research
