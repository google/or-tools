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

#include "base/commandlineflags.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/flatzinc_constraints.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);
DECLARE_bool(use_sat);

namespace operations_research {
namespace {
class BooleanSumOdd : public Constraint {
 public:
  BooleanSumOdd(Solver* const s, IntVar* const* bool_vars, int size)
      : Constraint(s),
        vars_(new IntVar* [size]),
        size_(size),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {
    memcpy(vars_.get(), bool_vars, size_ * sizeof(*bool_vars));
  }

  virtual ~BooleanSumOdd() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumOdd::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    if (num_always_true == num_possible_true && num_possible_true % 2 == 0) {
      solver()->Fail();
    } else if (num_possible_true == num_always_true + 1) {
      DCHECK_NE(-1, possible_true_index);
      if (num_possible_true % 2 == 1) {
        vars_[possible_true_index]->SetMin(1);
      } else {
        vars_[possible_true_index]->SetMax(0);
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
  }

  void Update(int index) {
    DCHECK(vars_[index]->Bound());
    const int64 value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      num_possible_true_vars_.Decr(solver());
    } else {
      DCHECK_EQ(1, value);
      num_always_true_vars_.Incr(solver());
    }
    if (num_always_true_vars_.Value() == num_possible_true_vars_.Value() &&
        num_possible_true_vars_.Value() % 2 == 0) {
      solver()->Fail();
    } else if (num_possible_true_vars_.Value() ==
               num_always_true_vars_.Value() + 1) {
      int possible_true_index = -1;
      for (int i = 0; i < size_; ++i) {
        if (!vars_[i]->Bound()) {
          possible_true_index = i;
          break;
        }
      }
      if (possible_true_index != -1) {
        if (num_possible_true_vars_.Value() % 2 == 1) {
          vars_[possible_true_index]->SetMin(1);
        } else {
          vars_[possible_true_index]->SetMax(0);
        }
      }
    }
  }

  virtual string DebugString() const {
    return StringPrintf("BooleanSumOdd([%s])",
                        DebugStringArray(vars_.get(), size_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(), size_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  const scoped_array<IntVar*> vars_;
  const int size_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
};

class IsBooleanSumInRange : public Constraint {
 public:
  IsBooleanSumInRange(Solver* const s, IntVar* const* bool_vars, int size,
                      int64 range_min, int64 range_max, IntVar* const target)
      : Constraint(s),
        vars_(new IntVar* [size]),
        size_(size),
        range_min_(range_min),
        range_max_(range_max),
        target_(target),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {
    memcpy(vars_.get(), bool_vars, size_ * sizeof(*bool_vars));
  }

  virtual ~IsBooleanSumInRange() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &IsBooleanSumInRange::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
    if (!target_->Bound()) {
      Demon* const u = MakeConstraintDemon0(
          solver(), this, &IsBooleanSumInRange::UpdateTarget, "UpdateTarget");
      target_->WhenBound(u);
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
    UpdateTarget();
  }

  void UpdateTarget() {
    if (num_always_true_vars_.Value() > range_max_ ||
        num_possible_true_vars_.Value() < range_min_) {
      inactive_.Switch(solver());
      target_->SetValue(0);
    } else if (num_always_true_vars_.Value() >= range_min_ &&
               num_possible_true_vars_.Value() <= range_max_) {
      inactive_.Switch(solver());
      target_->SetValue(1);
    } else if (target_->Min() == 1) {
      if (num_possible_true_vars_.Value() == range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_max_) {
        PushAllUnboundToZero();
      }
    } else if (target_->Max() == 0) {
      if (num_possible_true_vars_.Value() == range_max_ + 1 &&
          num_always_true_vars_.Value() >= range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_min_ - 1 &&
                 num_possible_true_vars_.Value() <= range_max_) {
        PushAllUnboundToZero();
      }
    }
  }

  void Update(int index) {
    if (!inactive_.Switched()) {
      DCHECK(vars_[index]->Bound());
      const int64 value = vars_[index]->Min();  // Faster than Value().
      if (value == 0) {
        num_possible_true_vars_.Decr(solver());
      } else {
        DCHECK_EQ(1, value);
        num_always_true_vars_.Incr(solver());
      }
      UpdateTarget();
    }
  }

  virtual string DebugString() const {
    return StringPrintf(
        "Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d] == %s",
        DebugStringArray(vars_.get(), size_, ", ").c_str(), range_min_,
        range_max_, target_->DebugString().c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(), size_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  void PushAllUnboundToZero() {
    inactive_.Switch(solver());
    int true_vars = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min() == 0) {
        vars_[i]->SetValue(0);
      } else {
        true_vars++;
      }
    }
    target_->SetValue(true_vars >= range_min_ && true_vars <= range_max_);
  }

  void PushAllUnboundToOne() {
    inactive_.Switch(solver());
    int true_vars = 0;
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Max() == 1) {
        vars_[i]->SetValue(1);
        true_vars++;
      }
    }
    target_->SetValue(true_vars >= range_min_ && true_vars <= range_max_);
  }

  const scoped_array<IntVar*> vars_;
  const int size_;
  const int64 range_min_;
  const int64 range_max_;
  IntVar* const target_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
  RevSwitch inactive_;
};

class BooleanSumInRange : public Constraint {
 public:
  BooleanSumInRange(Solver* const s, IntVar* const* bool_vars, int size,
                    int64 range_min, int64 range_max)
      : Constraint(s),
        vars_(new IntVar* [size]),
        size_(size),
        range_min_(range_min),
        range_max_(range_max),
        num_possible_true_vars_(0),
        num_always_true_vars_(0) {
    memcpy(vars_.get(), bool_vars, size_ * sizeof(*bool_vars));
  }

  virtual ~BooleanSumInRange() {}

  virtual void Post() {
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        Demon* const u = MakeConstraintDemon1(
            solver(), this, &BooleanSumInRange::Update, "Update", i);
        vars_[i]->WhenBound(u);
      }
    }
  }

  virtual void InitialPropagate() {
    int num_always_true = 0;
    int num_possible_true = 0;
    int possible_true_index = -1;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (var->Min() == 1) {
        num_always_true++;
        num_possible_true++;
      } else if (var->Max() == 1) {
        num_possible_true++;
        possible_true_index = i;
      }
    }
    num_possible_true_vars_.SetValue(solver(), num_possible_true);
    num_always_true_vars_.SetValue(solver(), num_always_true);
    Check();
  }

  void Check() {
    if (num_always_true_vars_.Value() > range_max_ ||
        num_possible_true_vars_.Value() < range_min_) {
      solver()->Fail();
    } else if (num_always_true_vars_.Value() >= range_min_ &&
               num_possible_true_vars_.Value() <= range_max_) {
      // Inhibit.
    } else {
      if (num_possible_true_vars_.Value() == range_min_) {
        PushAllUnboundToOne();
      } else if (num_always_true_vars_.Value() == range_max_) {
        PushAllUnboundToZero();
      }
    }
  }

  void Update(int index) {
    DCHECK(vars_[index]->Bound());
    const int64 value = vars_[index]->Min();  // Faster than Value().
    if (value == 0) {
      num_possible_true_vars_.Decr(solver());
    } else {
      DCHECK_EQ(1, value);
      num_always_true_vars_.Incr(solver());
    }
    Check();
  }

  virtual string DebugString() const {
    return StringPrintf(
        "Sum([%s]) in [%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d]",
        DebugStringArray(vars_.get(), size_, ", ").c_str(), range_min_,
        range_max_);
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kSumEqual, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.get(), size_);
    visitor->EndVisitConstraint(ModelVisitor::kSumEqual, this);
  }

 private:
  void PushAllUnboundToZero() {
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Min() == 0) {
        vars_[i]->SetValue(0);
      } else {
      }
    }
  }

  void PushAllUnboundToOne() {
    for (int i = 0; i < size_; ++i) {
      if (vars_[i]->Max() == 1) {
        vars_[i]->SetValue(1);
      }
    }
  }

  const scoped_array<IntVar*> vars_;
  const int size_;
  const int64 range_min_;
  const int64 range_max_;
  NumericalRev<int> num_possible_true_vars_;
  NumericalRev<int> num_always_true_vars_;
};

// ----- Lex constraint -----

class Lex : public Constraint {
 public:
  Lex(Solver* const s, const std::vector<IntVar*>& left,
      const std::vector<IntVar*>& right, bool strict)
      : Constraint(s), left_(left), right_(right), active_var_(0),
        strict_(strict) {
    CHECK_EQ(left.size(), right.size());
  }

  virtual ~Lex() {}

  virtual void Post() {
    const int position = FindNextValidVar(0);
    active_var_.SetValue(solver(), position);
    if (position < left_.size()) {
      Demon* const demon = MakeConstraintDemon1(
          solver(), this, &Lex::Propagate, "Propagate", position);
      left_[position]->WhenRange(demon);
      right_[position]->WhenRange(demon);
    }
  }

  virtual void InitialPropagate() {
    Propagate(active_var_.Value());
  }

  void Propagate(int var_index) {
    DCHECK_EQ(var_index, active_var_.Value());
    const int position = FindNextValidVar(var_index);
    if (position >= left_.size()) {
      if (strict_) {
        solver()->Fail();
      }
      return;
    }
    if (position != var_index) {
      Demon* const demon = MakeConstraintDemon1(
          solver(), this, &Lex::Propagate, "Propagate", position);
      left_[position]->WhenRange(demon);
      right_[position]->WhenRange(demon);
      active_var_.SetValue(solver(), position);
    }
    if ((strict_ && position == left_.size() - 1) ||
        (position < left_.size() - 1 &&
         left_[position + 1]->Min() > right_[position + 1]->Max())) {
      // We need to be strict if we are the last in the array, or if
      // the next one is impossible.
      left_[position]->SetMax(right_[position]->Max() - 1);
      right_[position]->SetMin(left_[position]->Min() + 1);
    } else {
      left_[position]->SetMax(right_[position]->Max());
      right_[position]->SetMin(left_[position]->Min());
    }
  }

  virtual string DebugString() const {
    return StringPrintf("Lex([%s], [%s]%s)",
                        DebugStringVector(left_, ", ").c_str(),
                        DebugStringVector(right_, ", ").c_str(),
                        strict_ ? ", strict" : "");
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint("Lex", this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->EndVisitConstraint("Lex", this);
  }

 private:
  int FindNextValidVar(int start_position) const {
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
};

// ----- Inverse constraint -----

// This constraints maintains: left[i] == j <=> right_[j] == i.
// It assumes array are 0 based.
class Inverse : public Constraint {
 public:
  Inverse(Solver* const s, const std::vector<IntVar*>& left,
      const std::vector<IntVar*>& right)
      : Constraint(s), left_(left), right_(right), left_holes_(left.size()),
        right_holes_(right_.size()), left_iterators_(left_.size()),
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
    solver()->AddConstraint(solver()->MakeAllDifferent(left_));
    solver()->AddConstraint(solver()->MakeAllDifferent(right_));
  }

  virtual void InitialPropagate() {
    const int size = left_.size();
    for (int i = 0; i < size; ++i) {
      left_[i]->SetRange(0, size - 1);
      right_[i]->SetRange(0, size - 1);
    }
    for (int i = 0; i < size; ++i) {
      PropagateDomain(i, left_[i], left_iterators_[i], right_, &remove_left_);
      PropagateDomain(i, right_[i], right_iterators_[i], left_, &remove_right_);
    }
  }

  void Propagate(int index, bool left_to_right) {
    if (left_to_right) {
      PropagateHoles(index, left_[index], left_holes_[index], right_);
    } else {
      PropagateHoles(index, right_[index], right_holes_[index], left_);
    }
  }

  virtual string DebugString() const {
    return StringPrintf("Inverse([%s], [%s])",
                        DebugStringVector(left_, ", ").c_str(),
                        DebugStringVector(right_, ", ").c_str());
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint("Inverse", this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kLeftArgument,
                                               left_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kRightArgument,
                                               right_);
    visitor->EndVisitConstraint("Inverse", this);
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
      inverse[holes->Value()]->RemoveValue(index);
    }
    for (int64 value = vmax + 1; value <= oldmax; ++value) {
      inverse[value]->RemoveValue(index);
    }
  }

  void PropagateDomain(int index, IntVar* const var,
                       IntVarIterator* const domain,
                       const std::vector<IntVar*>& inverse,
                       std::vector<int64>* const remove) {
    remove->clear();
    for (domain->Init(); domain->Ok(); domain->Next()) {
      const int64 value = domain->Value();
      if (!inverse[value]->Contains(index)) {
        remove->push_back(value);
      }
    }
    if (!remove->empty()) {
      var->RemoveValues(*remove);
    }
  }

  std::vector<IntVar*> left_;
  std::vector<IntVar*> right_;
  std::vector<IntVarIterator*> left_holes_;
  std::vector<IntVarIterator*> left_iterators_;
  std::vector<IntVarIterator*> right_holes_;
  std::vector<IntVarIterator*> right_iterators_;
  std::vector<int64> remove_left_;
  std::vector<int64> remove_right_;
};
}  // namespace

void PostIsBooleanSumInRange(FlatZincModel* const model, CtSpec* const spec,
                             const std::vector<IntVar*>& variables,
                             int64 range_min, int64 range_max,
                             IntVar* const target) {
  Solver* const solver = model->solver();
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  int possible_vars = 0;
  for (int i = 0; i < size; ++i) {
    if (variables[i]->Max() == 1) {
      possible_vars++;
      if (variables[i]->Min() == 1) {
        true_vars++;
      }
    }
  }
  if (true_vars > range_max || possible_vars < range_min) {
    Constraint* const ct = solver->MakeEquality(target, Zero());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else if (true_vars >= range_min && possible_vars <= range_max) {
    Constraint* const ct = solver->MakeEquality(target, 1);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else if (FLAGS_use_sat && range_min == size &&
             AddBoolAndArrayEqVar(model->Sat(), variables, target)) {
    VLOG(2) << "  - posted to sat";
  } else if (FLAGS_use_sat && range_max == 0 &&
             AddBoolOrArrayEqVar(model->Sat(), variables,
                                 solver->MakeDifference(1, target)->Var())) {
    VLOG(2) << "  - posted to sat";
  } else if (FLAGS_use_sat && range_min == 1 && range_max == size &&
             AddBoolOrArrayEqVar(model->Sat(), variables, target)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct = solver->RevAlloc(
        new IsBooleanSumInRange(solver, variables.data(), variables.size(),
                                range_min, range_max, target));
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void PostBooleanSumInRange(FlatZincModel* const model, CtSpec* const spec,
                           const std::vector<IntVar*>& variables,
                           int64 range_min, int64 range_max) {
  Solver* const solver = model->solver();
  const int64 size = variables.size();
  range_min = std::max(0LL, range_min);
  range_max = std::min(size, range_max);
  int true_vars = 0;
  std::vector<IntVar*> alt;
  for (int i = 0; i < size; ++i) {
    if (!variables[i]->Bound()) {
      alt.push_back(variables[i]);
    } else if (variables[i]->Min() == 1) {
      true_vars++;
    }
  }
  const int possible_vars = alt.size();
  range_min -= true_vars;
  range_max -= true_vars;

  if (range_max < 0 || range_min > possible_vars) {
    Constraint* const ct = solver->MakeFalseConstraint();
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else if (range_min <= 0 && range_max >= possible_vars) {
    Constraint* const ct = solver->MakeTrueConstraint();
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else if (FLAGS_use_sat && range_min == 0 && range_max == 1 &&
             AddAtMostOne(model->Sat(), alt)) {
    VLOG(2) << "  - posted to sat";
  } else if (FLAGS_use_sat && range_min == 0 && range_max == size - 1 &&
             AddAtMostNMinusOne(model->Sat(), alt)) {
    VLOG(2) << "  - posted to sat";
  } else if (FLAGS_use_sat && range_min == 1 && range_max == 1 &&
             AddBoolOrArrayEqualTrue(model->Sat(), alt) &&
             AddAtMostOne(model->Sat(), alt)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct = solver->RevAlloc(new BooleanSumInRange(
        solver, alt.data(), alt.size(), range_min, range_max));
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void PostBooleanSumOdd(FlatZincModel* const model, CtSpec* const spec,
                       const std::vector<IntVar*>& variables) {
  Solver* const solver = model->solver();
  Constraint* const ct = solver->RevAlloc(
      new BooleanSumOdd(solver, variables.data(), variables.size()));
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

Constraint* MakeStrongScalProdEquality(Solver* const solver,
                                       const std::vector<IntVar*>& variables,
                                       const std::vector<int64>& coefficients,
                                       int64 rhs) {
  const bool trace = FLAGS_cp_trace_search;
  const bool propag = FLAGS_cp_trace_propagation;
  FLAGS_cp_trace_search = false;
  FLAGS_cp_trace_propagation = false;
  const int size = variables.size();
  IntTupleSet tuples(size);
  Solver s("build");
  std::vector<IntVar*> copy_vars(size);
  for (int i = 0; i < size; ++i) {
    copy_vars[i] = s.MakeIntVar(variables[i]->Min(), variables[i]->Max());
  }
  s.AddConstraint(s.MakeScalProdEquality(copy_vars, coefficients, rhs));
  s.NewSearch(s.MakePhase(copy_vars, Solver::CHOOSE_FIRST_UNBOUND,
                          Solver::ASSIGN_MIN_VALUE));
  while (s.NextSolution()) {
    std::vector<int64> one_tuple(size);
    for (int i = 0; i < size; ++i) {
      one_tuple[i] = copy_vars[i]->Value();
    }
    tuples.Insert(one_tuple);
  }
  s.EndSearch();
  FLAGS_cp_trace_search = trace;
  FLAGS_cp_trace_propagation = propag;
  return solver->MakeAllowedAssignments(variables, tuples);
}

void PostLexLess(FlatZincModel* const model, CtSpec* const spec,
                 const std::vector<IntVar*>& left,
                 const std::vector<IntVar*>& right, bool strict) {
  Solver* const solver = model->solver();
  Constraint* const ct = solver->RevAlloc(new Lex(solver, left, right, strict));
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void PostInverse(FlatZincModel* const model, CtSpec* const spec,
                 const std::vector<IntVar*>& left,
                 const std::vector<IntVar*>& right) {
  Solver* const solver = model->solver();
  Constraint* const ct = solver->RevAlloc(new Inverse(solver, left, right));
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}
}  // namespace operations_research
