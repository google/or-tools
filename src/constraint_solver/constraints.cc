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

// ----- True and False Constraint -----

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

// ----- Map Variable Domain to Boolean Var Array -----
// TODO(user) : optimize constraint to avoid ping pong.
// After a boolvar is set to 0, we remove the value from the var.
// There is no need to rescan the var to find the hole if the size at the end of
// UpdateActive() is the same as the size at the beginning of VarDomain().

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
    for (int64 j = std::max(vmax + 1LL, 0LL); j <= std::min(oldmax, size - 1LL); ++j) {
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

// ----- Lex constraint -----

class LexicalLess : public Constraint {
 public:
  LexicalLess(Solver* const s, const std::vector<IntVar*>& left,
              const std::vector<IntVar*>& right, bool strict)
      : Constraint(s),
        left_(left),
        right_(right),
        active_var_(0),
        strict_(strict),
        demon_(nullptr) {
    CHECK_EQ(left.size(), right.size());
  }

  virtual ~LexicalLess() {}

  virtual void Post() {
    const int position = JumpEqualVariables(0);
    active_var_.SetValue(solver(), position);
    if (position < left_.size()) {
      demon_ = solver()->MakeConstraintInitialPropagateCallback(this);
      left_[position]->WhenRange(demon_);
      right_[position]->WhenRange(demon_);
    }
  }

  virtual void InitialPropagate() {
    const int position = JumpEqualVariables(active_var_.Value());
    if (position >= left_.size()) {
      if (strict_) {
        solver()->Fail();
      }
      return;
    }
    if (position != active_var_.Value()) {
      left_[position]->WhenRange(demon_);
      right_[position]->WhenRange(demon_);
      active_var_.SetValue(solver(), position);
    }
    const int next_non_equal = JumpEqualVariables(position + 1);
    if ((strict_ && next_non_equal == left_.size()) ||
        (next_non_equal < left_.size() &&
         left_[next_non_equal]->Min() > right_[next_non_equal]->Max())) {
      // We need to be strict if we are the last in the array, or if
      // the next one is impossible.
      left_[position]->SetMax(right_[position]->Max() - 1);
      right_[position]->SetMin(left_[position]->Min() + 1);
    } else {
      left_[position]->SetMax(right_[position]->Max());
      right_[position]->SetMin(left_[position]->Min());
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("%s([%s], [%s])",
                        strict_ ? "LexicalLess" : "LexicalLessOrEqual",
                        JoinDebugStringPtr(left_, ", ").c_str(),
                        JoinDebugStringPtr(right_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kLexLess, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, strict_);
    visitor->EndVisitConstraint(ModelVisitor::kLexLess, this);
  }

 private:
  int JumpEqualVariables(int start_position) const {
    int position = start_position;
    while (position < left_.size() && left_[position]->Bound() &&
           right_[position]->Bound() &&
           left_[position]->Min() == right_[position]->Min()) {
      position++;
    }
    return position;
  }

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  NumericalRev<int> active_var_;
  const bool strict_;
  Demon* demon_;
};

// ----- Inverse constraint -----

// This constraints maintains: left[i] == j <=> right_[j] == i.
// It assumes array are 0 based.
class Inverse : public Constraint {
 public:
  Inverse(Solver* const s, const std::vector<IntVar*>& left,
          const std::vector<IntVar*>& right)
      : Constraint(s),
        left_(left),
        right_(right),
        left_holes_(left.size()),
        left_iterators_(left_.size()),
        right_holes_(right_.size()),
        right_iterators_(right_.size()) {
    CHECK_EQ(left_.size(), right_.size());
    for (int i = 0; i < left_.size(); ++i) {
      left_holes_[i] = left_[i]->MakeHoleIterator(true);
      left_iterators_[i] = left_[i]->MakeDomainIterator(true);
      right_holes_[i] = right_[i]->MakeHoleIterator(true);
      right_iterators_[i] = right_[i]->MakeDomainIterator(true);
    }
  }

  virtual ~Inverse() {}

  virtual void Post() {
    for (int i = 0; i < left_.size(); ++i) {
      Demon* const left_demon = MakeConstraintDemon2(
          solver(), this, &Inverse::Propagate, "Propagate", i, true);
      left_[i]->WhenDomain(left_demon);
      Demon* const right_demon = MakeConstraintDemon2(
          solver(), this, &Inverse::Propagate, "Propagate", i, false);
      right_[i]->WhenDomain(right_demon);
    }
    solver()->AddConstraint(solver()->MakeAllDifferent(left_, false));
    solver()->AddConstraint(solver()->MakeAllDifferent(right_, false));
  }

  virtual void InitialPropagate() {
    const int size = left_.size();
    for (int i = 0; i < size; ++i) {
      left_[i]->SetRange(0, size - 1);
      right_[i]->SetRange(0, size - 1);
    }
    for (int i = 0; i < size; ++i) {
      PropagateDomain(i, left_[i], left_iterators_[i], right_);
      PropagateDomain(i, right_[i], right_iterators_[i], left_);
    }
  }

  void Propagate(int index, bool left_to_right) {
    if (left_to_right) {
      PropagateHoles(index, left_[index], left_holes_[index], right_);
    } else {
      PropagateHoles(index, right_[index], right_holes_[index], left_);
    }
  }

  virtual std::string DebugString() const {
    return StringPrintf("Inverse([%s], [%s])",
                        JoinDebugStringPtr(left_, ", ").c_str(),
                        JoinDebugStringPtr(right_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kInverse, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->EndVisitConstraint(ModelVisitor::kInverse, this);
  }

 private:
  void PropagateHoles(int index, IntVar* const var, IntVarIterator* const holes,
                      const std::vector<IntVar*>& inverse) {
    const int64 oldmax =
        std::min(var->OldMax(), static_cast<int64>(left_.size() - 1));
    const int64 vmin = var->Min();
    const int64 vmax = var->Max();
    for (int64 value = std::max(var->OldMin(), 0LL); value < vmin; ++value) {
      inverse[value]->RemoveValue(index);
    }
    for (holes->Init(); holes->Ok(); holes->Next()) {
      const int64 hole = holes->Value();
      if (hole >= 0 && hole < left_.size()) {
        inverse[hole]->RemoveValue(index);
      }
    }
    for (int64 value = vmax + 1; value <= oldmax; ++value) {
      inverse[value]->RemoveValue(index);
    }
  }

  void PropagateDomain(int index, IntVar* const var,
                       IntVarIterator* const domain,
                       const std::vector<IntVar*>& inverse) {
    remove_.clear();
    for (domain->Init(); domain->Ok(); domain->Next()) {
      const int64 value = domain->Value();
      if (!inverse[value]->Contains(index)) {
        remove_.push_back(value);
      }
    }
    if (!remove_.empty()) {
      var->RemoveValues(remove_);
    }
  }

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  std::vector<IntVarIterator*> left_holes_;
  std::vector<IntVarIterator*> left_iterators_;
  std::vector<IntVarIterator*> right_holes_;
  std::vector<IntVarIterator*> right_iterators_;
  std::vector<int64> remove_;
};
}  // namespace

// ----- API -----

Demon* Solver::MakeCallbackDemon(Callback1<Solver*>* const callback) {
  return RevAlloc(new Callback1Demon(callback));
}

Demon* Solver::MakeCallbackDemon(Closure* const callback) {
  return RevAlloc(new ClosureDemon(callback));
}

Constraint* Solver::MakeTrueConstraint() {
  DCHECK(true_constraint_ != nullptr);
  return true_constraint_;
}

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

Constraint* Solver::MakeMapDomain(IntVar* const var,
                                  const std::vector<IntVar*>& actives) {
  return RevAlloc(new MapDomain(this, var, actives));
}

Constraint* Solver::MakeLexicalLess(const std::vector<IntVar*>& left,
                                    const std::vector<IntVar*>& right) {
  return RevAlloc(new LexicalLess(this, left, right, true));
}

Constraint* Solver::MakeLexicalLessOrEqual(const std::vector<IntVar*>& left,
                                           const std::vector<IntVar*>& right) {
  return RevAlloc(new LexicalLess(this, left, right, false));
}

Constraint* Solver::MakeInverse(const std::vector<IntVar*>& left,
                                const std::vector<IntVar*>& right) {
  return RevAlloc(new Inverse(this, left, right));
}
}  // namespace operations_research
