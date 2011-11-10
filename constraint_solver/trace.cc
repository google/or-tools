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
      LOG(INFO) << "SetMin(" << inner_->DebugString() << ", " << m << ")";
      inner_->SetMin(m);
    }
  }

  virtual int64 Max() const { return inner_->Max(); }

  virtual void SetMax(int64 m) {
    if (m < inner_->Max()) {
      LOG(INFO) << "SetMax(" << inner_->DebugString() << ", " << m << ")";
      inner_->SetMax(m);
    }
  }

  virtual void Range(int64* l, int64* u) {
    inner_->Range(l, u);
  }

  virtual void SetRange(int64 l, int64 u) {
    if (l > inner_->Min() || u < inner_->Max()) {
      LOG(INFO) << "SetRange(" << inner_->DebugString() << ", ["
                << l << ".." << u << "])";
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
      LOG(INFO) << "RemoveValue(" << inner_->DebugString() << ", " << v << ")";
      inner_->RemoveValue(v);
    }
  }

  virtual void SetValue(int64 v) {
    LOG(INFO) << "SetValue(" << inner_->DebugString() << ", " << v << ")";
    inner_->SetValue(v);
  }

  virtual void RemoveInterval(int64 l, int64 u) {
    LOG(INFO) << "RemoveInterval(" << inner_->DebugString() << ", ["
              << l << ".." << u << "])";
    inner_->RemoveInterval(l, u);
  }

  virtual void RemoveValues(const int64* const values, int size) {
    LOG(INFO) << "RemoveValues(" << inner_->DebugString() << ", ["
              << Int64ArrayToString(values, size, ", ") << "])";
    inner_->RemoveValues(values, size);
  }

  virtual void SetValues(const int64* const values, int size) {
    LOG(INFO) << "SetValues(" << inner_->DebugString() << ", ["
              << Int64ArrayToString(values, size, ", ") << "])";
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
    LOG(INFO) << "SetMin(" << inner_->DebugString() << ", " << m << ")";
    inner_->SetMin(m);
  }

  virtual int64 Max() const { return inner_->Max(); }

  virtual void SetMax(int64 m) {
    LOG(INFO) << "SetMax(" << inner_->DebugString() << ", " << m << ")";
    inner_->SetMax(m);
  }

  virtual void Range(int64* l, int64* u) {
    inner_->Range(l, u);
  }

  virtual void SetRange(int64 l, int64 u) {
    if (l > inner_->Min() || u < inner_->Max()) {
      LOG(INFO) << "SetRange(" << inner_->DebugString() << ", ["
                << l << ".." << u << "])";
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
  TraceIntervalVar(Solver* const solver, IntervalVar* const interval)
      : IntervalVar(solver, ""), interval_(interval) {
    if (interval->HasName()) {
      set_name(interval->name());
    }
  }
  virtual ~TraceIntervalVar() {}

  virtual int64 StartMin() const {
    return interval_->StartMin();
  }

  virtual int64 StartMax() const  {
    return interval_->StartMax();
  }

  virtual void SetStartMin(int64 m) {
    if (m > interval_->StartMin()) {
      LOG(INFO) << "SetStartMin(" << interval_->DebugString() << ", "
                << m << ")";
      interval_->SetStartMin(m);
    }
  }

  virtual void SetStartMax(int64 m) {
    if (m < interval_->StartMax()) {
      LOG(INFO) << "SetStartMax(" << interval_->DebugString() << ", "
                << m << ")";
      interval_->SetStartMax(m);
    }
  }

  virtual void SetStartRange(int64 mi, int64 ma) {
    if (mi > interval_->StartMin() || ma < interval_->StartMax()) {
      LOG(INFO) << "SetStartRange(" << interval_->DebugString() << ", ["
                << mi << ".." << ma << "])";
      interval_->SetStartRange(mi, ma);
    }
  }

  virtual void WhenStartRange(Demon* const d) {
    interval_->WhenStartRange(d);
  }

  virtual void WhenStartBound(Demon* const d) {
    interval_->WhenStartBound(d);
  }

  virtual int64 EndMin() const {
    return interval_->EndMin();
  }

  virtual int64 EndMax() const  {
    return interval_->EndMax();
  }

  virtual void SetEndMin(int64 m) {
    if (m > interval_->EndMin()) {
      LOG(INFO) << "SetEndMin(" << interval_->DebugString() << ", " << m << ")";
      interval_->SetEndMin(m);
    }
  }

  virtual void SetEndMax(int64 m) {
    if (m < interval_->EndMax()) {
      LOG(INFO) << "SetEndMax(" << interval_->DebugString() << ", " << m << ")";
      interval_->SetEndMax(m);
    }
  }

  virtual void SetEndRange(int64 mi, int64 ma) {
    if (mi > interval_->EndMin() || ma < interval_->EndMax()) {
      LOG(INFO) << "SetEndRange(" << interval_->DebugString() << ", ["
                << mi << ".." << ma << "])";
      interval_->SetEndRange(mi, ma);
    }
  }

  virtual void WhenEndRange(Demon* const d) {
    interval_->WhenEndRange(d);
  }

  virtual void WhenEndBound(Demon* const d) {
    interval_->WhenStartBound(d);
  }

  virtual int64 DurationMin() const {
    return interval_->DurationMin();
  }

  virtual int64 DurationMax() const  {
    return interval_->DurationMax();
  }

  virtual void SetDurationMin(int64 m) {
    if (m > interval_->DurationMin()) {
      LOG(INFO) << "SetDurationMin(" << interval_->DebugString() << ", "
                << m << ")";
      interval_->SetDurationMin(m);
    }
  }

  virtual void SetDurationMax(int64 m) {
    if (m < interval_->DurationMax()) {
      LOG(INFO) << "SetDurationMax(" << interval_->DebugString() << ", "
                << m << ")";
      interval_->SetDurationMax(m);
    }
  }

  virtual void SetDurationRange(int64 mi, int64 ma) {
    if (mi > interval_->DurationMin() || ma < interval_->DurationMax()) {
      LOG(INFO) << "SetDurationRange(" << interval_->DebugString() << ", ["
                << mi << ".." << ma << "])";
      interval_->SetDurationRange(mi, ma);
    }
  }

  virtual void WhenDurationRange(Demon* const d) {
    interval_->WhenDurationRange(d);
  }

  virtual void WhenDurationBound(Demon* const d) {
    interval_->WhenDurationBound(d);
  }

  virtual bool MustBePerformed() const {
    return interval_->MustBePerformed();
  }

  virtual bool MayBePerformed() const {
    return interval_->MayBePerformed();
  }

  virtual void SetPerformed(bool val) {
    if ((val && !interval_->MustBePerformed()) ||
        (!val && interval_->MayBePerformed())) {
      LOG(INFO) << "SetPerformed(" << interval_->DebugString() << ", "
                << val << ")";
      interval_->SetPerformed(val);
    }
  }

  virtual void WhenPerformedBound(Demon* const d) {
    interval_->WhenPerformedBound(d);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    interval_->Accept(visitor);
  }

 private:
  IntervalVar* const interval_;
};

// ---------- Trace ----------

class Trace : public PropagationMonitor {
 public:
  virtual void StartInitialPropagation() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->StartInitialPropagation();
    }
  }

  virtual void EndInitialPropagation() {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndInitialPropagation();
    }
  }

  virtual void StartConstraintInitialPropagation(
      const Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->StartConstraintInitialPropagation(constraint);
    }
  }

  virtual void EndConstraintInitialPropagation(
      const Constraint* const constraint) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->EndConstraintInitialPropagation(constraint);
    }
  }

  virtual void StartNestedConstraintInitialPropagation(
      const Constraint* const parent,
      const Constraint* const nested) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->StartNestedConstraintInitialPropagation(parent, nested);
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

  virtual void StartDemonRun(const Demon* const demon) {
    for (int i = 0; i < monitors_.size(); ++i) {
      monitors_[i]->StartDemonRun(demon);
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

  void Add(PropagationMonitor* const monitor) {
    if (monitor != NULL) {
      monitors_.push_back(monitor);
    }
  }

 private:
  std::vector<PropagationMonitor*> monitors_;
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

PropagationMonitor* Solver::Trace() const {
  return trace_.get();
}

void Solver::AddPropagationMonitor(PropagationMonitor* const monitor) {
  // TODO(user): Check solver state?
  reinterpret_cast<class Trace*>(trace_.get())->Add(monitor);
}
}  // namespace operations_research
