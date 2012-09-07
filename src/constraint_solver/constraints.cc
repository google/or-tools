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
#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

Demon* Solver::MakeConstraintInitialPropagateCallback(Constraint* const ct) {
  return MakeConstraintDemon0(this,
                              ct,
                              &Constraint::InitialPropagate,
                              "InitialPropagate");
}

Demon* Solver::MakeDelayedConstraintInitialPropagateCallback(
    Constraint* const ct) {
  return MakeDelayedConstraintDemon0(this,
                                     ct,
                                     &Constraint::InitialPropagate,
                                     "InitialPropagate");
}

namespace {
class Callback1Demon : public Demon {
 public:
  // Ownership of the callback is transfered to the demon.
  explicit Callback1Demon(Callback1<Solver*>* const callback)
      : callback_(callback) {
    CHECK_NOTNULL(callback);
    callback_->CheckIsRepeatable();
  }
  virtual ~Callback1Demon() {}

  virtual void Run(Solver* const solver) {
    callback_->Run(solver);
  }
 private:
  scoped_ptr<Callback1<Solver*> > callback_;
};

class ClosureDemon : public Demon {
 public:
  // Ownership of the callback is transfered to the demon.
  explicit ClosureDemon(Closure* const callback) : callback_(callback) {
    CHECK_NOTNULL(callback);
    callback_->CheckIsRepeatable();
  }
  virtual ~ClosureDemon() {}

  virtual void Run(Solver* const solver) {
    callback_->Run();
  }
 private:
  scoped_ptr<Closure> callback_;
};
}  // namespace

Demon* Solver::MakeCallbackDemon(Callback1<Solver*>* const callback) {
  return RevAlloc(new Callback1Demon(callback));
}

Demon* Solver::MakeCallbackDemon(Closure* const callback) {
  return RevAlloc(new ClosureDemon(callback));
}

// ----- True and False Constraint -----

namespace {
class TrueConstraint : public Constraint {
 public:
  explicit TrueConstraint(Solver* const s) : Constraint(s) {}
  virtual ~TrueConstraint() {}

  virtual void Post() {}
  virtual void InitialPropagate() {}
  virtual string DebugString() const { return "TrueConstraint()"; }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kTrueConstraint, this);
    visitor->EndVisitConstraint(ModelVisitor::kTrueConstraint, this);
  }
};
}  // namespace

Constraint* Solver::MakeTrueConstraint() {
  DCHECK(true_constraint_ != NULL);
  return true_constraint_;
}

namespace {
class FalseConstraint : public Constraint {
 public:
  explicit FalseConstraint(Solver* const s) : Constraint(s) {}
  FalseConstraint(Solver* const s, const string& explanation)
      : Constraint(s), explanation_(explanation) {}
  virtual ~FalseConstraint() {}

  virtual void Post() {}
  virtual void InitialPropagate() { solver()->Fail(); }
  virtual string DebugString() const {
    return StrCat("FalseConstraint(", explanation_, ")");
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kFalseConstraint, this);
    visitor->EndVisitConstraint(ModelVisitor::kFalseConstraint, this);
  }

 private:
  const string explanation_;
};
}  // namespace

Constraint* Solver::MakeFalseConstraint() {
  DCHECK(false_constraint_ != NULL);
  return false_constraint_;
}
Constraint* Solver::MakeFalseConstraint(const string& explanation) {
  return RevAlloc(new FalseConstraint(this, explanation));
}

void Solver::InitCachedConstraint() {
  DCHECK(true_constraint_ == NULL);
  true_constraint_ = RevAlloc(new TrueConstraint(this));
  DCHECK(false_constraint_ == NULL);
  false_constraint_ = RevAlloc(new FalseConstraint(this));
}
// ----- Map Variable Domain to Boolean Var Array -----
// TODO(user) : optimize constraint to avoid ping pong.
// After a boolvar is set to 0, we remove the value from the var.
// There is no need to rescan the var to find the hole if the size at the end of
// UpdateActive() is the same as the size at the beginning of VarDomain().

namespace {
class MapDomain : public Constraint {
 public:
  MapDomain(Solver* const s,
            IntVar* const var,
            IntVar* const * actives,
            int size)
      : Constraint(s), var_(var), actives_(new IntVar*[size]), size_(size) {
    memcpy(actives_.get(), actives, size_ * sizeof(*actives));
    holes_ = var->MakeHoleIterator(true);
  }

  virtual ~MapDomain() {}

  virtual void Post() {
    Demon* vd = MakeConstraintDemon0(solver(),
                                     this,
                                     &MapDomain::VarDomain,
                                     "VarDomain");
    var_->WhenDomain(vd);
    Demon* vb = MakeConstraintDemon0(solver(),
                                     this,
                                     &MapDomain::VarBound,
                                     "VarBound");
    var_->WhenBound(vb);
    scoped_ptr<IntVarIterator> it(var_->MakeDomainIterator(false));
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 index = it->Value();
      if (index >= 0 && index < size_ && !actives_[index]->Bound()) {
        Demon* d = MakeConstraintDemon1(solver(),
                                        this,
                                        &MapDomain::UpdateActive,
                                        "UpdateActive",
                                        index);
        actives_[index]->WhenDomain(d);
      }
    }
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < size_; ++i) {
      actives_[i]->SetRange(0LL, 1LL);
      if (!var_->Contains(i)) {
        actives_[i]->SetValue(0);
      } else if (actives_[i]->Max() == 0LL) {
        var_->RemoveValue(i);
      }
      if (actives_[i]->Min() == 1LL) {
        var_->SetValue(i);
      }
    }
    if (var_->Bound()) {
      VarBound();
    }
  }

  void UpdateActive(int64 index) {
    IntVar* const act = actives_[index];
    if (act->Max() == 0) {
      var_->RemoveValue(index);
    } else if (act->Min() == 1) {
      var_->SetValue(index);
    }
  }

  void VarDomain() {
    const int64 oldmin = var_->OldMin();
    const int64 oldmax = var_->OldMax();
    const int64 vmin = var_->Min();
    const int64 vmax = var_->Max();
    const int64 size = size_;
    for (int64 j = std::max(oldmin, 0LL); j < std::min(vmin, size); ++j) {
      actives_[j]->SetValue(0);
    }
    for (holes_->Init(); holes_->Ok(); holes_->Next()) {
      const int64 j = holes_->Value();
      if (j >= 0 && j < size_) {
        actives_[j]->SetValue(0);
      }
    }
    for (int64 j = std::max(vmax + 1LL, 0LL);
         j <= std::min(oldmax, size_ - 1LL); ++j) {
      actives_[j]->SetValue(0LL);
    }
  }

  void VarBound() {
    const int64 val = var_->Min();
    if (val >= 0 && val < size_) {
      actives_[val]->SetValue(1);
    }
  }
  virtual string DebugString() const {
    string out = "MapDomain(" + var_->DebugString() + ", [";
    for (int i = 0; i < size_; ++i) {
      out += actives_[i]->DebugString() + " ";
    }
    out += "])";
    return out;
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kMapDomain, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               actives_.get(),
                                               size_);
    visitor->EndVisitConstraint(ModelVisitor::kMapDomain, this);
  }

 private:
  IntVar* const var_;
  scoped_array<IntVar*> actives_;
  int size_;
  IntVarIterator* holes_;
};
}  // namespace

Constraint* Solver::MakeMapDomain(IntVar* const var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives.data(), actives.size()));
}

// ---------- No cycle ----------

// This constraint ensures there are no cycles in the variable/value graph.
// "Sink" values are values outside the range of the array of variables; they
// are used to end paths.
// The constraint does essentially two things:
// - forbid partial paths from looping back to themselves
// - ensure each variable/node can be connected to a "sink".
// If assume_paths is true, the constraint assumes the 'next' variables
// represent paths (and performs a faster propagation); otherwise the
// constraint assumes the 'next' variables represent a forest.
// TODO(user): improve code when assume_paths is false (currently does an
// expensive n^2 loop).

namespace {
class NoCycle : public Constraint {
 public:
  NoCycle(Solver* const s, const IntVar* const* nexts, int size,
          const IntVar* const* active,
          ResultCallback1<bool, int64>* sink_handler,
          bool owner,
          bool assume_paths);
  virtual ~NoCycle() {
    if (owner_) {
      delete sink_handler_;
    }
  }
  virtual void Post();
  virtual void InitialPropagate();
  void CheckSupport(int index);
  void ActiveBound(int index);
  void NextBound(int index);
  void ComputeSupports();
  virtual string DebugString() const;

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kNoCycle, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_.get(),
                                               size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_.get(),
                                               size_);
    visitor->VisitIntegerArgument("assume_paths", assume_paths_);
    visitor->VisitInt64ToBoolExtension(sink_handler_, -size_, size_);
    visitor->EndVisitConstraint(ModelVisitor::kNoCycle, this);
  }

 private:
  const scoped_array<IntVar*> nexts_;
  const int size_;
  const scoped_array<IntVar*> active_;
  scoped_array<int64> starts_;
  scoped_array<int64> ends_;
  scoped_array<int64> outbound_supports_;
  ResultCallback1<bool, int64>* sink_handler_;
  std::vector<int64> sinks_;
  bool owner_;
  bool assume_paths_;
};

NoCycle::NoCycle(Solver* const s, const IntVar* const* nexts, int size,
                 const IntVar* const* active,
                 ResultCallback1<bool, int64>* sink_handler, bool owner,
                 bool assume_paths)
    : Constraint(s),
      nexts_(new IntVar*[size]),
      size_(size),
      active_(new IntVar*[size]),
      starts_(new int64[size]),
      ends_(new int64[size]),
      outbound_supports_(new int64[size]),
      sink_handler_(sink_handler),
      owner_(owner),
      assume_paths_(assume_paths) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    memcpy(nexts_.get(), nexts, size_ * sizeof(*nexts));
    memcpy(active_.get(), active, size_ * sizeof(*active));
  }
  for (int i = 0; i < size; ++i) {
    starts_[i] = i;
    ends_[i] = i;
    outbound_supports_[i] = -1;
  }
  sink_handler_->CheckIsRepeatable();
}

void NoCycle::InitialPropagate() {
  // Reduce next domains to sinks + range of nexts
  for (int i = 0; i < size_; ++i) {
    IntVar* next = nexts_[i];
    for (int j = next->Min(); j < 0; ++j) {
      if (!sink_handler_->Run(j)) {
        next->RemoveValue(j);
      }
    }
    for (int j = next->Max(); j >= size_; --j) {
      if (!sink_handler_->Run(j)) {
        next->RemoveValue(j);
      }
    }
  }
  for (int i = 0; i < size_; ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    }
  }
  ComputeSupports();
}

void NoCycle::Post() {
  if (size_ == 0) return;
  for (int i = 0; i < size_; ++i) {
    IntVar* next = nexts_[i];
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &NoCycle::NextBound,
                                    "NextBound",
                                    i);
    next->WhenBound(d);
    Demon* support_demon = MakeConstraintDemon1(solver(),
                                                this,
                                                &NoCycle::CheckSupport,
                                                "CheckSupport",
                                                i);
    next->WhenDomain(support_demon);
    Demon* active_demon = MakeConstraintDemon1(solver(),
                                               this,
                                               &NoCycle::ActiveBound,
                                               "ActiveBound",
                                               i);
    active_[i]->WhenBound(active_demon);
  }
  // Setting up sinks
  int64 min_min = nexts_[0]->Min();
  int64 max_max = nexts_[0]->Max();
  for (int i = 1; i < size_; ++i) {
    const IntVar* next = nexts_[i];
    min_min = std::min(min_min, next->Min());
    max_max = std::max(max_max, next->Max());
  }
  sinks_.clear();
  for (int i = min_min; i <= max_max; ++i) {
    if (sink_handler_->Run(i)) {
      sinks_.push_back(i);
    }
  }
}

void NoCycle::CheckSupport(int index) {
  // TODO(user): make this incremental
  if (!nexts_[index]->Contains(outbound_supports_[index])) {
    ComputeSupports();
  }
}

void NoCycle::ActiveBound(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  }
}

void NoCycle::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  const int64 chain_start = starts_[index];
  const int64 chain_end = !sink_handler_->Run(next) ? ends_[next] : next;
  Solver* const s = solver();
  s->SaveAndSetValue(&ends_[chain_start], chain_end);
  if (!sink_handler_->Run(chain_end)) {
    s->SaveAndSetValue(&starts_[chain_end], chain_start);
    nexts_[chain_end]->RemoveValue(chain_start);
    if (!assume_paths_) {
      for (int i = 0; i < size_; ++i) {
        int64 current = i;
        bool found = (current == chain_end);
        // Counter to detect implicit cycles.
        int count = 0;
        while (!found
               && count < size_
               && !sink_handler_->Run(current)
               && nexts_[current]->Bound()) {
          current = nexts_[current]->Value();
          found = (current == chain_end);
          ++count;
        }
        if (found) {
          nexts_[chain_end]->RemoveValue(i);
        }
      }
    }
  }
}

// For each variable, find a path connecting to a sink. Starts partial paths
// from the sinks down to all unconnected variables. If some variables remain
// unconnected, fail. Resulting paths are used as supports.
void NoCycle::ComputeSupports() {
  scoped_array<int64> supported(new int64[size_]);
  int64 support_count = 0;
  for (int i = 0; i < size_; ++i) {
    if (nexts_[i]->Bound()) {
      supported[support_count] = i;
      outbound_supports_[i] = nexts_[i]->Value();
      ++support_count;
    } else {
      outbound_supports_[i] = -1;
    }
  }
  if (size_ == support_count) {
    return;
  }
  const int size = sinks_.size();
  for (int i = 0; i < size_; ++i) {
    const IntVar* next = nexts_[i];
    if (!nexts_[i]->Bound()) {
      for (int j = 0; j < size; ++j) {
        if (next->Contains(sinks_[j])) {
          supported[support_count] = i;
          outbound_supports_[i] = sinks_[j];
          ++support_count;
          break;
        }
      }
    }
  }
  for (int i = 0; i < support_count && support_count < size_; ++i) {
    const int64 supported_i = supported[i];
    for (int j = 0; j < size_; ++j) {
      if (outbound_supports_[j] < 0 && nexts_[j]->Contains(supported_i)) {
        supported[support_count] = j;
        outbound_supports_[j] = supported_i;
        ++support_count;
      }
    }
  }
  if (size_ != support_count) {
    supported.reset(NULL);
    for (int i = 0; i < size_; ++i) {
      if (outbound_supports_[i] < 0) {
        active_[i]->SetMax(0);
      }
    }
  }
}

string NoCycle::DebugString() const {
  string out = "NoCycle(";
  for (int i = 0; i < size_; ++i) {
    out += nexts_[i]->DebugString() + " ";
  }
  out += ")";
  return out;
}

bool GreaterThan(int64 x, int64 y) {
  return y >= x;
}
}  // namespace

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                ResultCallback1<bool, int64>* sink_handler,
                                bool assume_paths) {
  CHECK_EQ(nexts.size(), active.size());
  if (sink_handler == NULL) {
    sink_handler = NewPermanentCallback(&GreaterThan,
                                        static_cast<int64>(nexts.size()));
  }
  return RevAlloc(new NoCycle(this,
                              nexts.data(),
                              nexts.size(),
                              active.data(),
                              sink_handler,
                              true,
                              assume_paths));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                ResultCallback1<bool, int64>* sink_handler) {
  return MakeNoCycle(nexts, active, sink_handler, true);
}

// ----- Path cumul constraint -----

// cumuls[next[i]] = cumuls[i] + transits[i]

namespace {
class PathCumul : public Constraint {
 public:
  PathCumul(Solver* const s,
            const IntVar* const* nexts, int size,
            const IntVar* const* active,
            const IntVar* const* cumuls, int cumul_size,
            const IntVar* const* transits);
  virtual ~PathCumul() {}
  virtual void Post();
  virtual void InitialPropagate();
  void ActiveBound(int index);
  void NextBound(int index);
  bool AcceptLink(int i, int j) const;
  void UpdateSupport(int index);
  void CumulRange(int index);
  void TransitRange(int index);
  void FilterNext(int index);

  virtual string DebugString() const;

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_.get(),
                                               size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_.get(),
                                               size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_.get(),
                                               cumul_size_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTransitsArgument,
                                               transits_.get(),
                                               size_);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  scoped_array<IntVar*> nexts_;
  int size_;
  scoped_array<IntVar*> active_;
  scoped_array<IntVar*> cumuls_;
  int cumul_size_;
  scoped_array<IntVar*> transits_;
  RevArray<int> prevs_;
  scoped_array<int> supports_;
};

PathCumul::PathCumul(Solver* const s,
                     const IntVar* const* nexts, int size,
                     const IntVar* const* active,
                     const IntVar* const* cumuls, int cumul_size,
                     const IntVar* const* transits)
    : Constraint(s),
      nexts_(NULL),
      size_(size),
      active_(NULL),
      cumuls_(NULL),
      cumul_size_(cumul_size),
      prevs_(cumul_size, -1),
      supports_(new int[size]) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    nexts_.reset(new IntVar*[size_]);
    memcpy(nexts_.get(), nexts, size_ * sizeof(*nexts));
    active_.reset(new IntVar*[size_]);
    memcpy(active_.get(), active, size_ * sizeof(*active));
    transits_.reset(new IntVar*[size_]);
    memcpy(transits_.get(), transits, size_ * sizeof(*transits));
  }
  CHECK_GE(cumul_size_, 0);
  CHECK_GE(cumul_size_, size_);
  if (cumul_size_ > 0) {
    cumuls_.reset(new IntVar*[cumul_size_]);
    memcpy(cumuls_.get(), cumuls, cumul_size_ * sizeof(*cumuls));
  }
  for (int i = 0; i < size_; ++i) {
    supports_[i] = -1;
  }
}

void PathCumul::InitialPropagate() {
  for (int i = 0; i < size_; ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    } else {
      UpdateSupport(i);
    }
  }
}

void PathCumul::Post() {
  for (int i = 0; i < size_; ++i) {
    IntVar* var = nexts_[i];
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &PathCumul::NextBound,
                                    "NextBound",
                                    i);
    var->WhenBound(d);
    Demon* ds = MakeConstraintDemon1(solver(),
                                     this,
                                     &PathCumul::UpdateSupport,
                                     "UpdateSupport",
                                     i);
    var->WhenDomain(ds);
    Demon* active_demon = MakeConstraintDemon1(solver(),
                                               this,
                                               &PathCumul::ActiveBound,
                                               "ActiveBound",
                                               i);
    active_[i]->WhenBound(active_demon);
    Demon* transit_demon = MakeConstraintDemon1(solver(),
                                                this,
                                                &PathCumul::TransitRange,
                                                "TransitRange",
                                                i);
    transits_[i]->WhenRange(transit_demon);
  }
  for (int i = 0; i < cumul_size_; ++i) {
    IntVar* cumul = cumuls_[i];
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &PathCumul::CumulRange,
                                    "CumulRange",
                                    i);
    cumul->WhenRange(d);
  }
}

void PathCumul::ActiveBound(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  }
}

void PathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* cumul = cumuls_[index];
  IntVar* cumul_next = cumuls_[next];
  IntVar* transit = transits_[index];
  cumul_next->SetRange(cumul->Min() + transit->Min(),
                       CapAdd(cumul->Max(), transit->Max()));
  cumul->SetRange(CapSub(cumul_next->Min(), transit->Max()),
                  CapSub(cumul_next->Max(), transit->Min()));
  transit->SetRange(CapSub(cumul_next->Min(), cumul->Max()),
                    CapSub(cumul_next->Max(), cumul->Min()));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

void PathCumul::CumulRange(int index) {
  if (index < size_) {
    if (nexts_[index]->Bound()) {
      NextBound(index);
    } else {
#if 0
      FilterNext(index);
#else
      UpdateSupport(index);
#endif
    }
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
#if 0
    FilterNext(index);
#else
    for (int i = 0; i < size_; ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
#endif
  }
}

void PathCumul::TransitRange(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  } else {
#if 0
    FilterNext(index);
#else
    UpdateSupport(index);
#endif
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
#if 0
    FilterNext(index);
#else
    for (int i = 0; i < size_; ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
#endif
  }
}

bool PathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const IntVar* const transit_i = transits_[i];
  return transit_i->Min() <= CapSub(cumul_j->Max(), cumul_i->Min())
      && CapSub(cumul_j->Min(), cumul_i->Max()) <= transit_i->Max();
}

void PathCumul::FilterNext(int index) {
  IntVar* var = nexts_[index];
  bool found = false;
  scoped_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
  std::vector<int64> to_remove;
  for (it->Init(); it->Ok(); it->Next()) {
    const int value = it->Value();
    if (AcceptLink(index, value)) {
      if (!found) {
        found = true;
        supports_[index] = value;
      }
    } else {
      to_remove.push_back(value);
    }
  }
  if (!found) {
    active_[index]->SetMax(0);
  } else {
    var->RemoveValues(to_remove);
  }
}

void PathCumul::UpdateSupport(int index) {
  int support = supports_[index];
  if (support < 0 || !AcceptLink(index, support)) {
    IntVar* var = nexts_[index];
    for (int i = var->Min(); i <= var->Max(); ++i) {
      if (i != support && AcceptLink(index, i)) {
        supports_[index] = i;
        return;
      }
    }
    active_[index]->SetMax(0);
  }
}

string PathCumul::DebugString() const {
  string out = "PathCumul(";
  for (int i = 0; i < size_; ++i) {
    out += nexts_[i]->DebugString() + " " + cumuls_[i]->DebugString();
  }
  out += ")";
  return out;
}
}  // namespace

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new PathCumul(this,
                                nexts.data(), nexts.size(),
                                active.data(),
                                cumuls.data(), cumuls.size(),
                                transits.data()));
}

namespace {
class VariableModulo : public Constraint {
 public:
  VariableModulo(Solver* const solver,
         IntVar* const x,
         IntVar* const mod,
         IntVar* const y)
      : Constraint(solver),
        x_(x),
        mod_(mod),
        y_(y) {
    CHECK_NOTNULL(solver);
    CHECK_NOTNULL(x);
    CHECK_NOTNULL(mod);
    CHECK_NOTNULL(y);
  }

  virtual ~VariableModulo() {}

  virtual void Post() {
    Solver* const s = solver();
    IntVar* const d = s->MakeIntVar(std::min(x_->Min(), -x_->Max()),
                                    std::max(x_->Max(), -x_->Min()));
    s->AddConstraint(
        s->MakeEquality(x_, s->MakeSum(s->MakeProd(mod_, d), y_)->Var()));
    s->AddConstraint(
        s->MakeGreater(y_, s->MakeOpposite(s->MakeAbs(mod_))->Var()));
    s->AddConstraint(s->MakeLess(y_, s->MakeAbs(mod_)->Var()));
    s->AddConstraint(
        s->MakeGreaterOrEqual(d, s->MakeMin(x_, s->MakeOpposite(x_))->Var()));
    s->AddConstraint(
        s->MakeLessOrEqual(d, s->MakeMax(x_, s->MakeOpposite(x_))->Var()));
  }

  virtual void InitialPropagate() {
    mod_->RemoveValue(0);
  }

  virtual string DebugString() const {
    return StringPrintf("VariableModulo(%s, %s, %s)",
                        x_->DebugString().c_str(),
                        mod_->DebugString().c_str(),
                        y_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kModuloConstraint, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, x_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kModuloArgument,
                                            mod_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, y_);
    visitor->EndVisitConstraint(ModelVisitor::kModuloConstraint, this);
  }

 private:
  IntVar* const x_;
  IntVar* const mod_;
  IntVar* const y_;
};

class BoundModulo : public Constraint {
 public:
  BoundModulo(Solver* const solver, IntVar* const x, IntVar* const mod)
      : Constraint(solver), x_(x), mod_(mod) {
    CHECK_NOTNULL(solver);
    CHECK_NOTNULL(x);
    CHECK_NOTNULL(mod);
  }

  virtual ~BoundModulo() {}

  virtual void Post() {
    Solver* const s = solver();
    if (x_->Min() >= 0 && mod_->Min() >= 0) {
      s->AddConstraint(s->MakeEquality(
          x_,
          s->MakeProd(s->MakeDiv(x_, mod_), mod_)));
    } else {
      IntVar* const d = s->MakeIntVar(std::min(x_->Min(), -x_->Max()),
                                      std::max(x_->Max(), -x_->Min()));
      s->AddConstraint(s->MakeEquality(x_, s->MakeProd(mod_, d)->Var()));
    }
  }

  virtual void InitialPropagate() {
    mod_->RemoveValue(0);
  }

  virtual string DebugString() const {
    return StringPrintf("BoundModulo(%s, %s)",
                        x_->DebugString().c_str(),
                        mod_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kModuloConstraint, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            x_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kModuloArgument,
                                            mod_);
    visitor->EndVisitConstraint(ModelVisitor::kModuloConstraint, this);
  }

 private:
  IntVar* const x_;
  IntVar* const mod_;
};

class PositiveBoundModulo : public Constraint {
 public:
  PositiveBoundModulo(Solver* const solver, IntVar* const x, IntVar* const mod)
      : Constraint(solver), x_(x), mod_(mod) {
    CHECK_NOTNULL(solver);
    CHECK_NOTNULL(x);
    CHECK_NOTNULL(mod);
  }

  virtual ~PositiveBoundModulo() {}

  virtual void Post() {
    Demon* const demon =
        MakeConstraintDemon0(solver(),
                             this,
                             &PositiveBoundModulo::PropagateRange,
                             "PropagateRange");
    x_->WhenRange(demon);
    mod_->WhenRange(demon);
  }

  virtual void InitialPropagate() {
    mod_->RemoveValue(0);
    PropagateRange();
  }

  void PropagateRange() {
    if (mod_->Bound()) {
      const int64 mod = mod_->Min();
      x_->SetRange(PosIntDivUp(x_->Min(), mod) * mod,
                   PosIntDivDown(x_->Max(), mod) * mod);
    } else {
      const int64 x_min = x_->Min();
      const int64 x_max = x_->Max();
      const int64 mod_min = mod_->Min();
      const int64 mod_max = mod_->Max();
      const int64 div_min = PosIntDivUp(x_min, mod_max);
      const int64 div_max = PosIntDivDown(x_max, mod_min);
      x_->SetRange(div_min * mod_min, div_max * mod_max);
      mod_->SetRange(div_max != 0 ? PosIntDivUp(x_min, div_max) : mod_min,
                     PosIntDivDown(x_max, div_min));
    }
  }

  virtual string DebugString() const {
    return StringPrintf("%s %% %s == 0",
                        x_->DebugString().c_str(),
                        mod_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kModuloConstraint, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            x_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kModuloArgument,
                                            mod_);
    visitor->EndVisitConstraint(ModelVisitor::kModuloConstraint, this);
  }

 private:
  IntVar* const x_;
  IntVar* const mod_;
};

class PositiveBoundBoundModulo : public Constraint {
 public:
  PositiveBoundBoundModulo(Solver* const solver, IntVar* const x, int64 mod)
      : Constraint(solver), x_(x), mod_(mod) {
    CHECK_NOTNULL(solver);
    CHECK_NOTNULL(x);
  }

  virtual ~PositiveBoundBoundModulo() {}

  virtual void Post() {
    Demon* const demon =
        MakeConstraintDemon0(solver(),
                             this,
                             &PositiveBoundBoundModulo::PropagateRange,
                             "PropagateRange");
    x_->WhenRange(demon);
  }

  virtual void InitialPropagate() {
    PropagateRange();
  }

  void PropagateRange() {
    x_->SetRange(PosIntDivUp(x_->Min(), mod_) * mod_,
                 (x_->Max() / mod_) * mod_);
  }

  string DebugString() const {
    return StringPrintf("%s %% %" GG_LL_FORMAT "d == 0",
                        x_->DebugString().c_str(),
                        mod_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kModuloConstraint, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                            x_);
    visitor->VisitIntegerArgument(ModelVisitor::kModuloArgument, mod_);
    visitor->EndVisitConstraint(ModelVisitor::kModuloConstraint, this);
  }

 private:
  IntVar* const x_;
  const int64 mod_;
};

class PositiveModulo : public Constraint {
 public:
  PositiveModulo(Solver* const solver,
                 IntVar* const x,
                 IntVar* const mod,
                 IntVar* const y)
      : Constraint(solver),
        x_(x),
        mod_(mod),
        y_(y) {
    CHECK_NOTNULL(solver);
    CHECK_NOTNULL(x);
    CHECK_NOTNULL(mod);
    CHECK_NOTNULL(y);
  }

  virtual ~PositiveModulo() {}

  virtual void Post() {
    Solver* const s = solver();
    s->AddConstraint(s->MakeEquality(
        y_,
        s->MakeDifference(x_,
                          s->MakeProd(s->MakeDiv(x_, mod_), mod_))->Var()));
    s->AddConstraint(s->MakeLess(y_, mod_));
  }

  virtual void InitialPropagate() {
    mod_->RemoveValue(0);
    y_->SetMin(0);
  }

  virtual string DebugString() const {
    return StringPrintf("PositiveModulo(%s, %s, %s)",
                        x_->DebugString().c_str(),
                        mod_->DebugString().c_str(),
                        y_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kModuloConstraint, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, x_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kModuloArgument,
                                            mod_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument, y_);
    visitor->EndVisitConstraint(ModelVisitor::kModuloConstraint, this);
  }

 private:
  IntVar* const x_;
  IntVar* const mod_;
  IntVar* const y_;
};

}  // namespace
Constraint* Solver::MakeModuloConstraint(IntVar* const x,
                                         int64 mod,
                                         IntVar* const y) {
  return MakeModuloConstraint(x, MakeIntConst(mod), y);
}

Constraint* Solver::MakeModuloConstraint(IntVar* const x,
                                         IntVar* const mod,
                                         IntVar* const y) {
  if (y->Bound() && y->Min() == 0) {
    if (x->Min() >= 0 && mod->Min() >= 0) {
      if (mod->Bound()) {
        return RevAlloc(new PositiveBoundBoundModulo(this, x, mod->Min()));
      } else {
        return RevAlloc(new PositiveBoundModulo(this, x, mod));
      }
    } else {
      return RevAlloc(new BoundModulo(this, x, mod));
    }
  } else if (x->Min() >= 0 && y->Min() >= 0 && mod->Min() >= 0) {
    return RevAlloc(new PositiveModulo(this, x, mod, y));
  } else {
    return RevAlloc(new VariableModulo(this, x, mod, y));
  }
}
}  // namespace operations_research
