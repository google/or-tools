// Copyright 2010-2013 Google
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
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/saturated_arithmetic.h"
#include "util/string_array.h"

namespace operations_research {

Demon* Solver::MakeConstraintInitialPropagateCallback(Constraint* const ct) {
  return MakeConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                              "InitialPropagate");
}

Demon* Solver::MakeDelayedConstraintInitialPropagateCallback(
    Constraint* const ct) {
  return MakeDelayedConstraintDemon0(this, ct, &Constraint::InitialPropagate,
                                     "InitialPropagate");
}

namespace {
class Callback1Demon : public Demon {
 public:
  // Ownership of the callback is transfered to the demon.
  explicit Callback1Demon(Callback1<Solver*>* const callback)
      : callback_(callback) {
    CHECK(callback != nullptr);
    callback_->CheckIsRepeatable();
  }
  virtual ~Callback1Demon() {}

  virtual void Run(Solver* const solver) { callback_->Run(solver); }

 private:
  std::unique_ptr<Callback1<Solver*> > callback_;
};

class ClosureDemon : public Demon {
 public:
  // Ownership of the callback is transfered to the demon.
  explicit ClosureDemon(Closure* const callback) : callback_(callback) {
    CHECK(callback != nullptr);
    callback_->CheckIsRepeatable();
  }
  virtual ~ClosureDemon() {}

  virtual void Run(Solver* const solver) { callback_->Run(); }

 private:
  std::unique_ptr<Closure> callback_;
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
  DCHECK(true_constraint_ != nullptr);
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
  DCHECK(false_constraint_ != nullptr);
  return false_constraint_;
}
Constraint* Solver::MakeFalseConstraint(const string& explanation) {
  return RevAlloc(new FalseConstraint(this, explanation));
}

void Solver::InitCachedConstraint() {
  DCHECK(true_constraint_ == nullptr);
  true_constraint_ = RevAlloc(new TrueConstraint(this));
  DCHECK(false_constraint_ == nullptr);
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
  MapDomain(Solver* const s, IntVar* const var, const std::vector<IntVar*>& actives)
      : Constraint(s), var_(var), actives_(actives) {
    holes_ = var->MakeHoleIterator(true);
  }

  virtual ~MapDomain() {}

  virtual void Post() {
    Demon* vd = MakeConstraintDemon0(solver(), this, &MapDomain::VarDomain,
                                     "VarDomain");
    var_->WhenDomain(vd);
    Demon* vb =
        MakeConstraintDemon0(solver(), this, &MapDomain::VarBound, "VarBound");
    var_->WhenBound(vb);
    std::unique_ptr<IntVarIterator> it(var_->MakeDomainIterator(false));
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 index = it->Value();
      if (index >= 0 && index < actives_.size() && !actives_[index]->Bound()) {
        Demon* d = MakeConstraintDemon1(
            solver(), this, &MapDomain::UpdateActive, "UpdateActive", index);
        actives_[index]->WhenDomain(d);
      }
    }
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < actives_.size(); ++i) {
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
    const int64 size = actives_.size();
    for (int64 j = std::max(oldmin, 0LL); j < std::min(vmin, size); ++j) {
      actives_[j]->SetValue(0);
    }
    for (holes_->Init(); holes_->Ok(); holes_->Next()) {
      const int64 j = holes_->Value();
      if (j >= 0 && j < size) {
        actives_[j]->SetValue(0);
      }
    }
    for (int64 j = std::max(vmax + 1LL, 0LL); j <= std::min(oldmax, size - 1LL);
         ++j) {
      actives_[j]->SetValue(0LL);
    }
  }

  void VarBound() {
    const int64 val = var_->Min();
    if (val >= 0 && val < actives_.size()) {
      actives_[val]->SetValue(1);
    }
  }
  virtual string DebugString() const {
    return StringPrintf("MapDomain(%s, [%s])", var_->DebugString().c_str(),
                        DebugStringVector(actives_, ", ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kMapDomain, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               actives_);
    visitor->EndVisitConstraint(ModelVisitor::kMapDomain, this);
  }

 private:
  IntVar* const var_;
  std::vector<IntVar*> actives_;
  IntVarIterator* holes_;
};
}  // namespace

Constraint* Solver::MakeMapDomain(IntVar* const var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives));
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
  NoCycle(Solver* const s, const std::vector<IntVar*>& nexts,
          const std::vector<IntVar*>& active,
          ResultCallback1<bool, int64>* sink_handler, bool owner,
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
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerArgument("assume_paths", assume_paths_);
    visitor->VisitInt64ToBoolExtension(sink_handler_, -size(), size());
    visitor->EndVisitConstraint(ModelVisitor::kNoCycle, this);
  }

 private:
  int64 size() const { return nexts_.size(); }

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  std::vector<int64> starts_;
  std::vector<int64> ends_;
  std::vector<int64> outbound_supports_;
  ResultCallback1<bool, int64>* sink_handler_;
  std::vector<int64> sinks_;
  bool owner_;
  bool assume_paths_;
};

NoCycle::NoCycle(Solver* const s, const std::vector<IntVar*>& nexts,
                 const std::vector<IntVar*>& active,
                 ResultCallback1<bool, int64>* sink_handler, bool owner,
                 bool assume_paths)
    : Constraint(s),
      nexts_(nexts),
      active_(active),
      starts_(nexts.size()),
      ends_(nexts.size()),
      outbound_supports_(nexts.size(), -1),
      sink_handler_(sink_handler),
      owner_(owner),
      assume_paths_(assume_paths) {
  for (int i = 0; i < size(); ++i) {
    starts_[i] = i;
    ends_[i] = i;
  }
  sink_handler_->CheckIsRepeatable();
}

void NoCycle::InitialPropagate() {
  // Reduce next domains to sinks + range of nexts
  for (int i = 0; i < size(); ++i) {
    IntVar* next = nexts_[i];
    for (int j = next->Min(); j < 0; ++j) {
      if (!sink_handler_->Run(j)) {
        next->RemoveValue(j);
      }
    }
    for (int j = next->Max(); j >= size(); --j) {
      if (!sink_handler_->Run(j)) {
        next->RemoveValue(j);
      }
    }
  }
  for (int i = 0; i < size(); ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    }
  }
  ComputeSupports();
}

void NoCycle::Post() {
  if (size() == 0) return;
  for (int i = 0; i < size(); ++i) {
    IntVar* next = nexts_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &NoCycle::NextBound,
                                    "NextBound", i);
    next->WhenBound(d);
    Demon* support_demon = MakeConstraintDemon1(
        solver(), this, &NoCycle::CheckSupport, "CheckSupport", i);
    next->WhenDomain(support_demon);
    Demon* active_demon = MakeConstraintDemon1(
        solver(), this, &NoCycle::ActiveBound, "ActiveBound", i);
    active_[i]->WhenBound(active_demon);
  }
  // Setting up sinks
  int64 min_min = nexts_[0]->Min();
  int64 max_max = nexts_[0]->Max();
  for (int i = 1; i < size(); ++i) {
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
      for (int i = 0; i < size(); ++i) {
        int64 current = i;
        bool found = (current == chain_end);
        // Counter to detect implicit cycles.
        int count = 0;
        while (!found && count < size() && !sink_handler_->Run(current) &&
               nexts_[current]->Bound()) {
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
  std::unique_ptr<int64[]> supported(new int64[size()]);
  int64 support_count = 0;
  for (int i = 0; i < size(); ++i) {
    if (nexts_[i]->Bound()) {
      supported[support_count] = i;
      outbound_supports_[i] = nexts_[i]->Value();
      ++support_count;
    } else {
      outbound_supports_[i] = -1;
    }
  }
  if (size() == support_count) {
    return;
  }
  const int sink_size = sinks_.size();
  for (int i = 0; i < size(); ++i) {
    const IntVar* next = nexts_[i];
    if (!nexts_[i]->Bound()) {
      for (int j = 0; j < sink_size; ++j) {
        if (next->Contains(sinks_[j])) {
          supported[support_count] = i;
          outbound_supports_[i] = sinks_[j];
          ++support_count;
          break;
        }
      }
    }
  }
  for (int i = 0; i < support_count && support_count < size(); ++i) {
    const int64 supported_i = supported[i];
    for (int j = 0; j < size(); ++j) {
      if (outbound_supports_[j] < 0 && nexts_[j]->Contains(supported_i)) {
        supported[support_count] = j;
        outbound_supports_[j] = supported_i;
        ++support_count;
      }
    }
  }
  if (support_count != size()) {
    supported.reset();
    for (int i = 0; i < size(); ++i) {
      if (outbound_supports_[i] < 0) {
        active_[i]->SetMax(0);
      }
    }
  }
}

string NoCycle::DebugString() const {
  return StringPrintf("NoCycle(%s)", DebugStringVector(nexts_, ", ").c_str());
}

// ----- Circuit constraint -----

class Circuit : public Constraint {
 public:
  static const int kRoot;
  Circuit(Solver* const s, const std::vector<IntVar*>& nexts)
      : Constraint(s), nexts_(nexts), size_(nexts_.size()), processed_(0),
        starts_(size_, -1), ends_(size_, -1), domains_(size_),
        outbound_support_(size_, -1), inbound_support_(size_, -1),
        temp_support_(size_, -1), inbound_demon_(nullptr),
        outbound_demon_(nullptr) {
    for (int i = 0; i < size_; ++i) {
      domains_[i] = nexts_[i]->MakeDomainIterator(true);
    }
  }

  virtual ~Circuit() {}

  virtual void Post() {
    inbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &Circuit::CheckReachabilityToRoot,
        "CheckReachabilityToRoot");
    outbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &Circuit::CheckReachabilityFromRoot,
        "CheckReachabilityFromRoot");
    for (int i = 0; i < size_; ++i) {
      Demon* const domain_demon = MakeConstraintDemon1(
          solver(), this, &Circuit::NextDomain, "NextDomain", i);
      nexts_[i]->WhenDomain(domain_demon);
      Demon* const bound_demon = MakeConstraintDemon1(
          solver(), this, &Circuit::NextBound, "NextBound", i);
      nexts_[i]->WhenBound(bound_demon);
    }
    solver()->AddConstraint(solver()->MakeAllDifferent(nexts_));
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < size_; ++i) {
      nexts_[i]->SetRange(0, size_ - 1);
      nexts_[i]->RemoveValue(i);
    }
    for (int i = 0; i < size_; ++i) {
      starts_.SetValue(solver(), i, i);
      ends_.SetValue(solver(), i, i);
    }
    for (int i = 0; i < size_; ++i) {
      if (nexts_[i]->Bound()) {
        NextBound(i);
      }
    }
    CheckReachabilityFromRoot();
    CheckReachabilityToRoot();
  }

  virtual string DebugString() const {
    return StringPrintf("Circuit(%s)", DebugStringVector(nexts_, " ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kCircuit, this);
    visitor->VisitIntegerVariableArrayArgument(
        ModelVisitor::kNextsArgument, nexts_);
    visitor->EndVisitConstraint(ModelVisitor::kCircuit, this);
  }

 private:
  void NextBound(int index) {
    Solver* const s = solver();
    const int destination = nexts_[index]->Value();
    const int new_end = ends_.Value(destination);
    const int new_start = starts_.Value(index);
    int chain_length = 0;
    for (int current = new_start; current != new_end;
         current = nexts_[current]->Value()) {
      if (current != kRoot) {
        starts_.SetValue(s, current, new_start);
        ends_.SetValue(s, current, new_end);
        chain_length++;
      }
    }
    if (chain_length < size_ - 1) {
      nexts_[new_end]->RemoveValue(new_start);
    }
  }

  void NextDomain(int index) {
    if (!nexts_[index]->Contains(outbound_support_[index])) {
      EnqueueDelayedDemon(outbound_demon_);
    }
    if (!nexts_[index]->Contains(inbound_support_[index])) {
      EnqueueDelayedDemon(inbound_demon_);
    }
  }

  void CheckReachabilityFromRoot() {
    reached_.clear();
    reached_.insert(kRoot);
    processed_ = 0;
    insertion_queue_.clear();
    insertion_queue_.push_back(kRoot);
    while (processed_ < insertion_queue_.size() &&
           reached_.size() < size_) {
      const int candidate = insertion_queue_[processed_++];
      IntVarIterator* const domain = domains_[candidate];
      for (domain->Init(); domain->Ok(); domain->Next()) {
        const int64 after = domain->Value();
        if (!ContainsKey(reached_, after)) {
          reached_.insert(after);
          insertion_queue_.push_back(after);
          temp_support_[candidate] = after;
        }
      }
    }
    if (insertion_queue_.size() < size_) {
      solver()->Fail();
    } else {
      outbound_support_.swap(temp_support_);
    }
  }

  void CheckReachabilityToRoot() {
    insertion_queue_.clear();
    insertion_queue_.push_back(kRoot);
    temp_support_[kRoot] = nexts_[kRoot]->Min();
    processed_ = 0;
    to_visit_.clear();
    for (int i = 1; i < size_; ++i) {
      to_visit_.push_back(i);
    }
    while (processed_ < insertion_queue_.size() &&
           insertion_queue_.size() < size_) {
      const int inserted = insertion_queue_[processed_++];
      std::vector<int> rejected;
      for (int index = 0; index < to_visit_.size(); ++index) {
        const int candidate = to_visit_[index];
        if (nexts_[candidate]->Contains(inserted)) {
          insertion_queue_.push_back(candidate);
          temp_support_[candidate] = inserted;
        } else {
          rejected.push_back(candidate);
        }
      }
      to_visit_.clear();
      to_visit_.swap(rejected);
    }
    if (insertion_queue_.size() < size_) {
      solver()->Fail();
    } else {
      temp_support_.swap(inbound_support_);
    }
  }

  const std::vector<IntVar*> nexts_;
  const int size_;
  std::vector<int> insertion_queue_;
  std::vector<int> to_visit_;
  hash_set<int> reached_;
  int processed_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  std::vector<IntVarIterator*> domains_;
  std::vector<int> outbound_support_;
  std::vector<int> inbound_support_;
  std::vector<int> temp_support_;
  Demon* inbound_demon_;
  Demon* outbound_demon_;
};

const int Circuit::kRoot = 0;

// ----- Circuit constraint -----

class SubCircuit : public Constraint {
 public:
  SubCircuit(Solver* const s, const std::vector<IntVar*>& nexts)
      : Constraint(s), nexts_(nexts), size_(nexts_.size()), processed_(0),
        starts_(size_, -1), ends_(size_, -1), domains_(size_),
        outbound_support_(size_, -1), inbound_support_(size_, -1),
        temp_support_(size_, -1), inbound_demon_(nullptr),
        outbound_demon_(nullptr), root_(-1) {
    for (int i = 0; i < size_; ++i) {
      domains_[i] = nexts_[i]->MakeDomainIterator(true);
    }
  }

  virtual ~SubCircuit() {}

  virtual void Post() {
    inbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &SubCircuit::CheckReachabilityToRoot,
        "CheckReachabilityToRoot");
    outbound_demon_ = MakeDelayedConstraintDemon0(
        solver(), this, &SubCircuit::CheckReachabilityFromRoot,
        "CheckReachabilityFromRoot");
    for (int i = 0; i < size_; ++i) {
      Demon* const bound_demon = MakeConstraintDemon1(
          solver(), this, &SubCircuit::NextBound, "NextBound", i);
      nexts_[i]->WhenBound(bound_demon);
      Demon* const domain_demon = MakeConstraintDemon1(
          solver(), this, &SubCircuit::NextDomain, "NextDomain", i);
      nexts_[i]->WhenDomain(domain_demon);
    }
    solver()->AddConstraint(solver()->MakeAllDifferent(nexts_));
  }

  virtual void InitialPropagate() {
    for (int i = 0; i < size_; ++i) {
      nexts_[i]->SetRange(0, size_ - 1);
    }
    for (int i = 0; i < size_; ++i) {
      starts_.SetValue(solver(), i, i);
      ends_.SetValue(solver(), i, i);
    }
    for (int i = 0; i < size_; ++i) {
      if (nexts_[i]->Bound()) {
        NextBound(i);
      }
    }
    CheckReachabilityFromRoot();
    CheckReachabilityToRoot();
  }

  virtual string DebugString() const {
    return StringPrintf(
        "SubCircuit(%s)", DebugStringVector(nexts_, " ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kCircuit, this);
    visitor->VisitIntegerVariableArrayArgument(
        ModelVisitor::kNextsArgument, nexts_);
    visitor->EndVisitConstraint(ModelVisitor::kCircuit, this);
  }

 private:
  bool Inactive(int index) const {
    return nexts_[index]->Bound() && nexts_[index]->Min() == index;
  }

  int NumberOfInactiveNodes() const {
    int inactive = 0;
    for (int i = 0; i < size_; ++i) {
      inactive += Inactive(i);
    }
    return inactive;
  }

  void NextBound(int index) {
    Solver* const s = solver();
    const int destination = nexts_[index]->Value();
    if (destination != index) {
      if (root_.Value() == -1) {
        root_.SetValue(s, index);
      }
      const int new_end = ends_.Value(destination);
      const int new_start = starts_.Value(index);
      int chain_length = 0;
      for (int current = new_start; current != new_end;
           current = nexts_[current]->Value()) {
        if (current != root_.Value()) {
          starts_.SetValue(s, current, new_start);
          ends_.SetValue(s, current, new_end);
          chain_length++;
        }
      }
      const int inactive = NumberOfInactiveNodes();
      if (chain_length < size_ - 1 - inactive) {
        nexts_[new_end]->RemoveValue(new_start);
      }
    }
  }

  void NextDomain(int index) {
    if (root_.Value() == -1) {
      return;
    }
    if (!nexts_[index]->Contains(outbound_support_[index])) {
      EnqueueDelayedDemon(outbound_demon_);
    }
    if (!nexts_[index]->Contains(inbound_support_[index])) {
      EnqueueDelayedDemon(inbound_demon_);
    }
  }

  void AddReached(int index) {
    reached_.insert(index);
    insertion_queue_.push_back(index);
  }

  void CheckReachabilityFromRoot() {
    if (root_.Value() == -1) {  // Root is not yet defined. Nothing to deduce.
      return;
    }

    const int inactive = NumberOfInactiveNodes();

    // Assign temp_support_ to a dummy value.
    for (int i = 0; i < size_; ++i) {
      temp_support_[i] = -1;
    }
    // Clear the spanning tree.
    processed_ = 0;
    reached_.clear();
    insertion_queue_.clear();
    // Add the root node.
    AddReached(root_.Value());
    // Compute reachable nodes.
    while (processed_ < insertion_queue_.size() &&
           reached_.size() + inactive < size_) {
      const int candidate = insertion_queue_[processed_++];
      IntVarIterator* const domain = domains_[candidate];
      for (domain->Init(); domain->Ok(); domain->Next()) {
        const int64 after = domain->Value();
        if (!ContainsKey(reached_, after)) {
          AddReached(after);
          temp_support_[candidate] = after;
        }
      }
    }
    // All non reachable nodes should point to themselves.
    for (int i = 0; i < size_; ++i) {
      if (!ContainsKey(reached_, i)) {
        nexts_[i]->SetValue(i);
      }
    }
    // Update the outbound_support_ vector.
    outbound_support_.swap(temp_support_);
  }

  void CheckReachabilityToRoot() {
    if (root_.Value() == -1) {
      return;
    }
    const int inactive = NumberOfInactiveNodes();

    insertion_queue_.clear();
    insertion_queue_.push_back(root_.Value());
    temp_support_[root_.Value()] = nexts_[root_.Value()]->Min();
    processed_ = 0;
    to_visit_.clear();
    for (int i = 0; i < size_; ++i) {
      if (!Inactive(i) && i != root_.Value()) {
        to_visit_.push_back(i);
      }
    }
    while (processed_ < insertion_queue_.size() &&
           insertion_queue_.size() + inactive < size_) {
      const int inserted = insertion_queue_[processed_++];
      std::vector<int> rejected;
      for (int index = 0; index < to_visit_.size(); ++index) {
        const int candidate = to_visit_[index];
        if (nexts_[candidate]->Contains(inserted)) {
          insertion_queue_.push_back(candidate);
          temp_support_[candidate] = inserted;
        } else {
          rejected.push_back(candidate);
        }
      }
      to_visit_.clear();
      to_visit_.swap(rejected);
    }
    for (int i = 0; i < to_visit_.size(); ++i) {
      const int node = to_visit_[i];
      nexts_[node]->SetValue(node);
    }
    temp_support_.swap(inbound_support_);
  }

  const std::vector<IntVar*> nexts_;
  const int size_;
  std::vector<int> insertion_queue_;
  std::vector<int> to_visit_;
  hash_set<int> reached_;
  int processed_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  std::vector<IntVarIterator*> domains_;
  std::vector<int> outbound_support_;
  std::vector<int> inbound_support_;
  std::vector<int> temp_support_;
  Demon* inbound_demon_;
  Demon* outbound_demon_;
  Rev<int> root_;
};

// ----- Misc -----

bool GreaterThan(int64 x, int64 y) {
  return y >= x;
}
}  // namespace

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                ResultCallback1<bool, int64>* sink_handler,
                                bool assume_paths) {
  CHECK_EQ(nexts.size(), active.size());
  if (sink_handler == nullptr) {
    sink_handler =
        NewPermanentCallback(&GreaterThan, static_cast<int64>(nexts.size()));
  }
  return RevAlloc(
      new NoCycle(this, nexts, active, sink_handler, true, assume_paths));
}

Constraint* Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                ResultCallback1<bool, int64>* sink_handler) {
  return MakeNoCycle(nexts, active, sink_handler, true);
}

Constraint* Solver::MakeCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new Circuit(this, nexts));
}

Constraint* Solver::MakeSubCircuit(const std::vector<IntVar*>& nexts) {
  return RevAlloc(new SubCircuit(this, nexts));
}

// ----- Path cumul constraints -----

namespace {
class BasePathCumul : public Constraint {
 public:
  BasePathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls);
  virtual ~BasePathCumul() {}
  virtual void Post();
  virtual void InitialPropagate();
  void ActiveBound(int index);
  virtual void NextBound(int index) = 0;
  virtual bool AcceptLink(int i, int j) const = 0;
  void UpdateSupport(int index);
  void CumulRange(int index);
  virtual string DebugString() const;

 protected:
  int64 size() const { return nexts_.size(); }
  int cumul_size() const { return cumuls_.size(); }

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  const std::vector<IntVar*> cumuls_;
  RevArray<int> prevs_;
  std::vector<int> supports_;
};

BasePathCumul::BasePathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                             const std::vector<IntVar*>& active,
                             const std::vector<IntVar*>& cumuls)
    : Constraint(s),
      nexts_(nexts),
      active_(active),
      cumuls_(cumuls),
      prevs_(cumuls.size(), -1),
      supports_(nexts.size()) {
  CHECK_GE(cumul_size(), size());
  for (int i = 0; i < size(); ++i) {
    supports_[i] = -1;
  }
}

void BasePathCumul::InitialPropagate() {
  for (int i = 0; i < size(); ++i) {
    if (nexts_[i]->Bound()) {
      NextBound(i);
    } else {
      UpdateSupport(i);
    }
  }
}

void BasePathCumul::Post() {
  for (int i = 0; i < size(); ++i) {
    IntVar* var = nexts_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &BasePathCumul::NextBound,
                                    "NextBound", i);
    var->WhenBound(d);
    Demon* ds = MakeConstraintDemon1(
        solver(), this, &BasePathCumul::UpdateSupport, "UpdateSupport", i);
    var->WhenDomain(ds);
    Demon* active_demon = MakeConstraintDemon1(
        solver(), this, &BasePathCumul::ActiveBound, "ActiveBound", i);
    active_[i]->WhenBound(active_demon);
  }
  for (int i = 0; i < cumul_size(); ++i) {
    IntVar* cumul = cumuls_[i];
    Demon* d = MakeConstraintDemon1(solver(), this, &BasePathCumul::CumulRange,
                                    "CumulRange", i);
    cumul->WhenRange(d);
  }
}

void BasePathCumul::ActiveBound(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  }
}

void BasePathCumul::CumulRange(int index) {
  if (index < size()) {
    if (nexts_[index]->Bound()) {
      NextBound(index);
    } else {
      UpdateSupport(index);
    }
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

void BasePathCumul::UpdateSupport(int index) {
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

string BasePathCumul::DebugString() const {
  string out = "PathCumul(";
  for (int i = 0; i < size(); ++i) {
    out += nexts_[i]->DebugString() + " " + cumuls_[i]->DebugString();
  }
  out += ")";
  return out;
}

// cumuls[next[i]] = cumuls[i] + transits[i]

class PathCumul : public BasePathCumul {
 public:
  PathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
            const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
            const std::vector<IntVar*>& transits)
      : BasePathCumul(s, nexts, active, cumuls), transits_(transits) {}
  virtual ~PathCumul() {}
  virtual void Post();
  virtual void NextBound(int index);
  virtual bool AcceptLink(int i, int j) const;
  void TransitRange(int index);

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kTransitsArgument,
                                               transits_);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  const std::vector<IntVar*> transits_;
};

void PathCumul::Post() {
  BasePathCumul::Post();
  for (int i = 0; i < size(); ++i) {
    Demon* transit_demon = MakeConstraintDemon1(
        solver(), this, &PathCumul::TransitRange, "TransitRange", i);
    transits_[i]->WhenRange(transit_demon);
  }
}

void PathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* cumul = cumuls_[index];
  IntVar* cumul_next = cumuls_[next];
  IntVar* transit = transits_[index];
  cumul_next->SetMin(cumul->Min() + transit->Min());
  cumul_next->SetMax(CapAdd(cumul->Max(), transit->Max()));
  cumul->SetMin(CapSub(cumul_next->Min(), transit->Max()));
  cumul->SetMax(CapSub(cumul_next->Max(), transit->Min()));
  transit->SetMin(CapSub(cumul_next->Min(), cumul->Max()));
  transit->SetMax(CapSub(cumul_next->Max(), cumul->Min()));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

void PathCumul::TransitRange(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  } else {
    UpdateSupport(index);
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

bool PathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const IntVar* const transit_i = transits_[i];
  return transit_i->Min() <= CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <= transit_i->Max();
}

// cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i])

class ResultCallback2PathCumul : public BasePathCumul {
 public:
  ResultCallback2PathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                           const std::vector<IntVar*>& active,
                           const std::vector<IntVar*>& cumuls,
                           Solver::IndexEvaluator2* transit_evaluator);
  virtual ~ResultCallback2PathCumul() {}
  virtual void NextBound(int index);
  virtual bool AcceptLink(int i, int j) const;

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    // TODO(user): Visit transit correctly.
    // visitor->VisitIntegerVariableArrayArgument(
    //     ModelVisitor::kTransitsArgument,
    //     transit_evaluator);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  std::unique_ptr<Solver::IndexEvaluator2> transits_evaluator_;
};

ResultCallback2PathCumul::ResultCallback2PathCumul(
    Solver* const s, const std::vector<IntVar*>& nexts,
    const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
    Solver::IndexEvaluator2* transit_evaluator)
    : BasePathCumul(s, nexts, active, cumuls),
      transits_evaluator_(transit_evaluator) {
  transits_evaluator_->CheckIsRepeatable();
}

void ResultCallback2PathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* cumul = cumuls_[index];
  IntVar* cumul_next = cumuls_[next];
  const int64 transit = transits_evaluator_->Run(index, next);
  cumul_next->SetMin(cumul->Min() + transit);
  cumul_next->SetMax(CapAdd(cumul->Max(), transit));
  cumul->SetMin(CapSub(cumul_next->Min(), transit));
  cumul->SetMax(CapSub(cumul_next->Max(), transit));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

bool ResultCallback2PathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const int64 transit = transits_evaluator_->Run(i, j);
  return transit <= CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <= transit;
}

// ----- ResulatCallback2SlackPathCumul -----

class ResultCallback2SlackPathCumul : public BasePathCumul {
 public:
  ResultCallback2SlackPathCumul(Solver* const s, const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                const std::vector<IntVar*>& cumuls,
                                const std::vector<IntVar*>& slacks,
                                Solver::IndexEvaluator2* transit_evaluator);
  virtual ~ResultCallback2SlackPathCumul() {}
  virtual void Post();
  virtual void NextBound(int index);
  virtual bool AcceptLink(int i, int j) const;
  void SlackRange(int index);

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kPathCumul, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kNextsArgument,
                                               nexts_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kActiveArgument,
                                               active_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kCumulsArgument,
                                               cumuls_);
    // TODO(user): Visit transit correctly.
    // visitor->VisitIntegerVariableArrayArgument(
    //     ModelVisitor::kTransitsArgument,
    //     transit_evaluator);
    visitor->EndVisitConstraint(ModelVisitor::kPathCumul, this);
  }

 private:
  const std::vector<IntVar*> slacks_;
  std::unique_ptr<Solver::IndexEvaluator2> transits_evaluator_;
};

ResultCallback2SlackPathCumul::ResultCallback2SlackPathCumul(
    Solver* const s, const std::vector<IntVar*>& nexts,
    const std::vector<IntVar*>& active, const std::vector<IntVar*>& cumuls,
    const std::vector<IntVar*>& slacks, Solver::IndexEvaluator2* transit_evaluator)
    : BasePathCumul(s, nexts, active, cumuls),
      slacks_(slacks),
      transits_evaluator_(transit_evaluator) {
  transits_evaluator_->CheckIsRepeatable();
}

void ResultCallback2SlackPathCumul::Post() {
  BasePathCumul::Post();
  for (int i = 0; i < size(); ++i) {
    Demon* slack_demon = MakeConstraintDemon1(
        solver(), this, &ResultCallback2SlackPathCumul::SlackRange,
        "SlackRange", i);
    slacks_[i]->WhenRange(slack_demon);
  }
}

void ResultCallback2SlackPathCumul::SlackRange(int index) {
  if (nexts_[index]->Bound()) {
    NextBound(index);
  } else {
    UpdateSupport(index);
  }
  if (prevs_[index] >= 0) {
    NextBound(prevs_[index]);
  } else {
    for (int i = 0; i < size(); ++i) {
      if (index == supports_[i]) {
        UpdateSupport(i);
      }
    }
  }
}

void ResultCallback2SlackPathCumul::NextBound(int index) {
  if (active_[index]->Min() == 0) return;
  const int64 next = nexts_[index]->Value();
  IntVar* const cumul = cumuls_[index];
  IntVar* const cumul_next = cumuls_[next];
  IntVar* const slack = slacks_[index];
  const int64 transit = transits_evaluator_->Run(index, next);
  cumul_next->SetMin(cumul->Min() + transit + slack->Min());
  cumul_next->SetMax(CapAdd(CapAdd(cumul->Max(), transit), slack->Max()));
  cumul->SetMin(CapSub(cumul_next->Min(), CapAdd(transit, slack->Max())));
  cumul->SetMax(CapSub(cumul_next->Max(), CapAdd(transit, slack->Min())));
  slack->SetMin(CapSub(cumul_next->Min(), CapAdd(transit, cumul->Max())));
  slack->SetMax(CapSub(cumul_next->Max(), CapAdd(transit, cumul->Min())));
  if (prevs_[next] < 0) {
    prevs_.SetValue(solver(), next, index);
  }
}

bool ResultCallback2SlackPathCumul::AcceptLink(int i, int j) const {
  const IntVar* const cumul_i = cumuls_[i];
  const IntVar* const cumul_j = cumuls_[j];
  const IntVar* const slack = slacks_[i];
  const int64 transit = transits_evaluator_->Run(i, j);
  return transit + slack->Min() <= CapSub(cumul_j->Max(), cumul_i->Min()) &&
         CapSub(cumul_j->Min(), cumul_i->Max()) <= slack->Max() + transit;
}
}  // namespace

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& transits) {
  CHECK_EQ(nexts.size(), active.size());
  CHECK_EQ(transits.size(), nexts.size());
  return RevAlloc(new PathCumul(this, nexts, active, cumuls, transits));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  Solver::IndexEvaluator2* transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new ResultCallback2PathCumul(this, nexts, active, cumuls,
                                               transit_evaluator));
}

Constraint* Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                                  const std::vector<IntVar*>& active,
                                  const std::vector<IntVar*>& cumuls,
                                  const std::vector<IntVar*>& slacks,
                                  Solver::IndexEvaluator2* transit_evaluator) {
  CHECK_EQ(nexts.size(), active.size());
  return RevAlloc(new ResultCallback2SlackPathCumul(this, nexts, active, cumuls,
                                                    slacks, transit_evaluator));
}
}  // namespace operations_research
