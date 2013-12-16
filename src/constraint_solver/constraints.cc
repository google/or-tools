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

#include <algorithm>
#include "base/unique_ptr.h"
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
  virtual std::string DebugString() const { return "TrueConstraint()"; }

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
  FalseConstraint(Solver* const s, const std::string& explanation)
      : Constraint(s), explanation_(explanation) {}
  virtual ~FalseConstraint() {}

  virtual void Post() {}
  virtual void InitialPropagate() { solver()->Fail(); }
  virtual std::string DebugString() const {
    return StrCat("FalseConstraint(", explanation_, ")");
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kFalseConstraint, this);
    visitor->EndVisitConstraint(ModelVisitor::kFalseConstraint, this);
  }

 private:
  const std::string explanation_;
};
}  // namespace

Constraint* Solver::MakeFalseConstraint() {
  DCHECK(false_constraint_ != nullptr);
  return false_constraint_;
}
Constraint* Solver::MakeFalseConstraint(const std::string& explanation) {
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
  virtual std::string DebugString() const {
    return StringPrintf("MapDomain(%s, [%s])", var_->DebugString().c_str(),
                        JoinDebugStringPtr(actives_, ", ").c_str());
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
}  // namespace operations_research
